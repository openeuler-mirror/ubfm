// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/time64.h>

#include "mami_string.h"
#include "ures.h"
#include "uop.h"
#include "ubfm_common.h"
#include "port_pub.h"
#include "feature_kern.h"
#include "idm.h"
#include "pa_tbl_cfg.h"

#define TBL_CFG_TIMEOUT 1000000 /* us */

#define TABLE_WRITE 0
#define TABLE_READ  1

#define PA_ROUTE_TBL_CFG 0x4C0
#define PA_ROUTE_TBL_DATA(index) (0x4c4 + (0x4 * (index)))

#define NL_RXPA_GLB_OFFSET (0x8000 + 0x4000)
#define PA_ROUTE_MODE_CFG 0x400

#define CFG_PA_ROUTE_P_NUM_OFFSET 25
#define CFG_PA_ROUTE_P_NUM_MASK 3

#define NDIE_P2P_8PORT_MODE  3 /* bit 11 */
#define NDIE_P2P_16PORT_MODE 0 /* bit 00 */
#define NDIE_P2P_32PORT_MODE 1 /* bit 01 */
#define NDIE_P2P_64PORT_MODE 2 /* bit 10 */

struct pa_route_tbl_cfg {
	u32 cmd : 1; /* 0 write, 1 read */
	u32 rsv1 : 2;
	u32 status : 1;
	u32 cmd_en : 1;
	u32 rsv2 : 3;
	u32 table_type : 4;
	u32 rsv3 : 4;
	u32 entry : 12;
	u32 rsv4 : 4;
};

u64 port_info_get_nl_base_addr(u32 devid, u32 die_id, u32 nl_id)
{
	const char *nl_reg_name[NL_MAX_NUM] = {MAMI_NL0_REG, MAMI_NL1_REG, MAMI_NL2_REG, MAMI_NL3_REG, MAMI_NL4_REG};
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	struct ures_addr_info nl_reg = {0};
	int ret;

	if ((die_id >= DIE_MAX_NUM) || (nl_id >= NL_MAX_NUM)) {
		udev_port_err("Invalid para. (devid=%u; die_id=%u; nl_id=%u)\n", devid, die_id, nl_id);
		return 0ULL;
	}

	ret = ures_get_reg_base(&inst, nl_reg_name[nl_id], &nl_reg);
	if (ret != 0) {
		udev_port_err("Get nl reg base failed. (devid=%u; die_id=%u; nl_id=%u)\n", devid, die_id, nl_id);
		return 0ULL;
	}

	return nl_reg.addr;
}

static u64 get_nl_rxpa_glb(u32 devid, u32 die_id, u32 nl_id)
{
	u64 base_addr = 0;

	base_addr = port_info_get_nl_base_addr(devid, die_id, nl_id);
	if (base_addr == 0) {
		return 0;
	}
	return (base_addr + NL_RXPA_GLB_OFFSET);
}

int get_ndie_p2p_port_mode(u32 devid, u32 die_id, u32 nl_id, enum ndie_p2p_port_mode *p2p_port_mode)
{
	u64 base_addr = 0;
	u32 value = 0, cfg_pa_route_p_num = 0;
	int ret = 0;

	u32 port_mode_to_p2p_enum[] = {
		[NDIE_P2P_8PORT_MODE] =  PORT_MODE_8,
		[NDIE_P2P_16PORT_MODE] = PORT_MODE_16,
		[NDIE_P2P_32PORT_MODE] = PORT_MODE_32,
		[NDIE_P2P_64PORT_MODE] = PORT_MODE_64
	};

	base_addr = get_nl_rxpa_glb(devid, die_id, nl_id);
	if (base_addr == 0) {
		udev_port_err("nl base addr is zero. (devid=%u)\n", devid);
		return -EFAULT;
	}

	ret = uop_kern_readl(devid, base_addr + PA_ROUTE_MODE_CFG, &value);
	if (ret != 0) {
		udev_port_err("nl base addr is zero. (devid=%u)\n", devid);
		return ret;
	}

	cfg_pa_route_p_num = (value >> CFG_PA_ROUTE_P_NUM_OFFSET) & CFG_PA_ROUTE_P_NUM_MASK;
	udev_port_info("die_id %u, nl_id %u, value %u, cfg_pa_route_p_num %u\n", die_id, nl_id, value, cfg_pa_route_p_num);
	*p2p_port_mode = port_mode_to_p2p_enum[cfg_pa_route_p_num];

	return 0;
}

static u64 get_timestamp_us(void)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);

	return (u64)((ts.tv_sec * USEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_USEC));
}

static int wait_table_cfg_idle(u32 devid, u64 nl_base)
{
	u64 time_start, time_cur;
	struct pa_route_tbl_cfg cfg = {0};
	int ret;

#ifndef EMU_ST
	rmb(); /* ensure previous writes are finished before waiting for the status */

	time_start = get_timestamp_us();
	do {
		ret = uop_kern_readl(devid, nl_base + PA_ROUTE_TBL_CFG, (u32 *)(void *)&cfg);
		if (ret != 0) {
			udev_port_err("Read tbl cfg failed. (devid=%u; ret=%d\n)", devid, ret);
			return ret;
		}

		time_cur = get_timestamp_us();
		if ((time_cur - time_start) > TBL_CFG_TIMEOUT) {
			udev_port_err("Wait idle timeout. (devid=%u; ret=%d\n)", devid, ret);
			return -ETIME;
		}
	} while (cfg.status != 0);
#endif

	return 0;
}

int pa_table_read(struct nl_id_inst *inst, enum route_table_cfg_type type, u32 entry, u8 *data, u32 size)
{
	struct pa_route_tbl_cfg cfg = {0};
	u64 base_addr;
	int ret;
	u32 i;

	base_addr = get_nl_rxpa_glb(inst->devid, inst->fdieid, inst->dbodyid);
	if (base_addr == 0) {
		return -EFAULT;
	}

	ret = wait_table_cfg_idle(inst->devid, base_addr);
	if (ret != 0) {
		return ret;
	}

	cfg.cmd = TABLE_READ;
	cfg.table_type = (u32)type;
	cfg.entry = entry;

	/*
	 * Only in esl, "cfg.cmd_en" should be set 0 and then set 1.
	 * Other scenarios, SOC will set "cfg.cmd_en" to 0, if opt is completed.
	 **/
	cfg.cmd_en = 0;
	ret = uop_kern_writel(inst->devid, base_addr + PA_ROUTE_TBL_CFG, *(u32 *)(void *)&cfg);
	if (ret != 0) {
		udev_port_err("Write tbl cfg failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
			inst->devid, inst->fdieid, type, ret);
		return ret;
	}
	wmb(); /* Ensure the uop kern is written correctly. */

	cfg.cmd_en = 1;
	ret = uop_kern_writel(inst->devid, base_addr + PA_ROUTE_TBL_CFG, *(u32 *)(void *)&cfg);
	if (ret != 0) {
		udev_port_err("Write tbl cfg failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
			inst->devid, inst->fdieid, type, ret);
		return ret;
	}
	wmb(); /* Ensure the uop kern is written correctly. */

	ret = wait_table_cfg_idle(inst->devid, base_addr);
	if (ret != 0) {
		return ret;
	}

	for (i = 0; i < (size / sizeof(u32)); i++) {
		ret = uop_kern_readl(inst->devid, base_addr + PA_ROUTE_TBL_DATA(i), (u32 *)(data + i * sizeof(u32)));
		if (ret != 0) {
			udev_port_err("Read tbl data failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
				inst->devid, inst->fdieid, type, ret);
			return ret;
		}
	}

	return 0;
}
