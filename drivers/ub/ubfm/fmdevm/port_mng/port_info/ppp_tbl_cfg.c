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
#include "ppp_tbl_cfg.h"

#define TBL_CFG_TIMEOUT 1000000 /* us */

#define TABLE_ITEM_SIZE 24 /* 6 * sizeof(u32) */
#define TABLE_WRITE 0
#define TABLE_READ  1

#define TP_PPP_GLB_OFFSET 0x80000
#define TP_PPP_TX_GLB_OFFSET 0x1b0000

#define PPP_TP_ROUTE_TBL_CFG 0x34C0
#define PPP_TP_ROUTE_TBL_DATA(index) (0x34c4 + (0x4 * (index)))

#define PA_TBL_CFG_DIE_MAX_NUM  DEV_DIE_MAX_NUM
#define PA_TBL_CFG_NL_MAX_NUM  5

struct ppp_tp_route_tbl_cfg {
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

static int get_tp_base_addr(struct ures_inst *inst, u64 *base_addr)
{
	enum mami_udie_version udie_version = UDIE_VERSION_MAX;
	struct ures_addr_info addr_info = {0};
	u64 value;
	int ret;

	ret = ures_get_key_value(inst->devid, MAMI_UDIE_VER, &value);
	if (ret != 0) {
		udev_port_err("Get udie version failed. (devid=%u; ret=%d)\n", inst->devid, ret);
		return ret;
	}
	udie_version = (enum mami_udie_version)value;

	ret = ures_get_reg_base(inst, MAMI_TP_REG, &addr_info);
	if (ret != 0) {
		udev_port_err("Get tp reg base failed. (devid=%u; die_id=%u; ret=%d)\n", inst->devid, inst->die_id, ret);
		return -ENXIO;
	}

	if (udie_version == UDIE_VERSION_V2) {
		*base_addr = addr_info.addr + TP_PPP_TX_GLB_OFFSET;
	} else {
		*base_addr = addr_info.addr + TP_PPP_GLB_OFFSET;
	}

	return 0;
}

static u64 get_timestamp_us(void)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);

	return (u64)((ts.tv_sec * USEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_USEC));
}

static bool is_table_cfg_idle(struct ppp_tp_route_tbl_cfg *tcfg)
{
	return (tcfg->status == 0);
}

static int wait_table_cfg_idle(struct ures_inst *inst)
{
	u64 base_addr, time_start, time_cur;
	struct ppp_tp_route_tbl_cfg cfg = {0};
	int ret;

#ifndef EMU_ST
	ret = get_tp_base_addr(inst, &base_addr);
	if (ret != 0) {
		return ret;
	}

	rmb(); /* ensure previous writes are finished before waiting for the status */

	time_start = get_timestamp_us();
	do {
		ret = uop_kern_readl(inst->devid, base_addr + PPP_TP_ROUTE_TBL_CFG, (u32 *)(void *)&cfg);
		if (ret != 0) {
			udev_port_err("Read tbl cfg failed. (devid=%u; die_id=%u; ret=%d\n)", inst->devid, inst->die_id, ret);
			return ret;
		}

		time_cur = get_timestamp_us();
		if ((time_cur - time_start) > TBL_CFG_TIMEOUT) {
			udev_port_err("Wait idle timeout. (devid=%u; die_id=%u; ret=%d\n)", inst->devid, inst->die_id, ret);
			return -ETIME;
		}
	} while (!is_table_cfg_idle(&cfg));
#endif

	return 0;
}

static int tp_ppp_table_read(struct ures_inst *inst, int type, u32 entry, u8 *data, u32 size)
{
	struct ppp_tp_route_tbl_cfg cfg;
	u64 base_addr;
	int ret;
	u32 i;

	ret = get_tp_base_addr(inst, &base_addr);
	if (ret != 0) {
		return ret;
	}
#ifndef EMU_ST
	ret = wait_table_cfg_idle(inst);
	if (ret != 0) {
		return ret;
	}

	(void)ubfm_memset(&cfg, sizeof(cfg), 0, sizeof(struct ppp_tp_route_tbl_cfg));

	cfg.cmd = TABLE_READ;
	cfg.table_type = (u32)type;
	cfg.entry = entry;

	/*
	 * Only in esl, "cfg.cmd_en" should be set 0 and then set 1.
	 * Other scenarios, SOC will set "cfg.cmd_en" to 0, if opt is completed.
	 **/
	cfg.cmd_en = 0;
	ret = uop_kern_writel(inst->devid, base_addr + PPP_TP_ROUTE_TBL_CFG, *(u32 *)(void *)&cfg);
	if (ret != 0) {
		udev_port_err("Write tbl cfg failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
			inst->devid, inst->die_id, type, ret);
		return ret;
	}
	wmb(); /* Ensure the uop kern is written correctly. */

	cfg.cmd_en = 1;
	ret = uop_kern_writel(inst->devid, base_addr + PPP_TP_ROUTE_TBL_CFG, *(u32 *)(void *)&cfg);
	if (ret != 0) {
		udev_port_err("Write tbl cfg failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
			inst->devid, inst->die_id, type, ret);
		return ret;
	}
	wmb(); /* Ensure the uop kern is written correctly. */

	ret = wait_table_cfg_idle(inst);
	if (ret != 0) {
		return ret;
	}

	for (i = 0; i < (size / sizeof(u32)); i++) {
		ret = uop_kern_readl(inst->devid, base_addr + PPP_TP_ROUTE_TBL_DATA(i), (u32 *)(data + i * sizeof(u32)));
		if (ret != 0) {
			udev_port_err("Read tbl data failed. (devid=%u; die_id=%u; type=%d; ret=%d\n)",
				inst->devid, inst->die_id, type, ret);
			return ret;
		}
	}
#endif

	return 0;
}

static int ppp_table_read_para_check(struct ures_inst *inst, enum route_table_cfg_type type,
	u32 entry, u8 *data, u32 size)
{
	if (inst == NULL) {
		udev_port_err("Invalid inst.\n");
		return -EINVAL;
	}

	if (inst->die_id >= PA_TBL_CFG_DIE_MAX_NUM) {
		udev_port_err("Invalid param. (die_id=%u)\n", inst->die_id);
		return -EINVAL;
	}

	if (type >= ROUTE_TABLE_CFG_TYPE_MAX) {
		udev_port_err("Invalid param. (type=%u)\n", type);
		return -EINVAL;
	}

	if (data == NULL) {
		udev_port_err("Invalid data.\n");
		return -EINVAL;
	}

	if ((size > TABLE_ITEM_SIZE) || (size == 0) || ((size % sizeof(u32)) != 0)) {
		udev_port_err("Item size is invalid. (size=%u)\n", size);
		return -EINVAL;
	}

	return 0;
}

int ppp_table_read(struct ures_inst *inst, enum route_table_cfg_type type, u32 entry, u8 *data, u32 size)
{
	int ret;

	ret = ppp_table_read_para_check(inst, type, entry, data, size);
	if (ret != 0) {
		return ret;
	}

	return tp_ppp_table_read(inst, type, entry, data, size);
}
