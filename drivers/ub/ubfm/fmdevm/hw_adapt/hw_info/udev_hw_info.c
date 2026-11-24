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
#include "fmdevm.h"
#include "ures.h"
#include "ubfabric.h"
#include "fmdevm.h"
#include "udev_hw_pub.h"
#include "udev_hw_info.h"
#include "udev_map.h"
#include "udev_ucmi_ops.h"
#include "../hw_ucmi/udev_ucmi_pcie_espi.h"

struct udev_hw_chip_mng {
	struct udev_hw_info chip_list[MAMI_KERN_TYPE_MAX];
};

static struct udev_hw_chip_mng g_chip_mng;
static bool g_udev_hw_status[DEV_MAX_NUM];

int udev_add_hw_info(u32 chip_type, struct udev_hw_info *info)
{
	if ((chip_type >= MAMI_KERN_TYPE_MAX) || (chip_type <= MAMI_KERN_TYPE_INVALID)) {
		return -EINVAL;
	}
	if (info == NULL) {
		return -EINVAL;
	}
	g_chip_mng.chip_list[chip_type] = *info;
	return 0;
}

static void udev_reset_ures(u32 devid)
{
	struct ures_inst inst;
	u64 die_num = 0;
	int ret;
	u64 i;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num > DIE_MAX_NUM)) {
		hw_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return;
	}
	for (i = 0; i < die_num; ++i) {
		ures_inst_pack(&inst, devid, i);
		ures_die_reset(&inst);
	}
	ures_dev_reset(devid);
}

static int udev_set_ures_reg_info(u32 devid, struct udev_hw_info *hw_info)
{
	struct ures_inst inst;
	struct soc_addr *reg;
	u64 die_num = 0;
	int ret = 0;
	u32 i, j;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num > DIE_MAX_NUM)) {
		hw_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return (ret != 0 ? ret : -EINVAL);
	}

	for (i = 0; i < die_num; ++i) {
		if (test_bit(i, &hw_info->reg_die_bitmap) == 0) {
			continue;
		}
		ures_inst_pack(&inst, devid, i);
		for (j = 0; j < hw_info->addr_reg[i].addr_num; j++) {
			struct udev_soc_addr addr = {0};

			reg = &hw_info->addr_reg[i].addr_list[j];
			udev_soc_addr_pack(&addr, reg->name, reg->phy_addr, (u32)reg->len, reg->perf_mark);
			ret = udev_mem_map(&inst, UOP_ITEM_REG, &addr, UOP_NOT_MAP);
			if (ret != 0) {
				hw_err("Udev reg map failed. (devid=%u; die_id=%u; name=%s; ret=%d)\n",
					   devid, i, reg->name, ret);
				return ret;
			}
		}
	}
	return ret;
}

static int udev_set_ures_mem_info(u32 devid, struct udev_hw_info *hw_info)
{
	struct ures_inst inst;
	struct soc_addr *mem;
	u64 die_num = 0;
	int ret;
	u32 i, j;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num > DIE_MAX_NUM)) {
		hw_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return (ret != 0 ? ret : -EINVAL);
	}

	for (i = 0; i < die_num; ++i) {
		if (test_bit(i, &hw_info->mem_die_bitmap) == 0) {
			continue;
		}
		ures_inst_pack(&inst, devid, i);
		for (j = 0; j < hw_info->addr_mem[i].addr_num; j++) {
			struct udev_soc_addr addr = {0};

			mem = &hw_info->addr_mem[i].addr_list[j];
			udev_soc_addr_pack(&addr, mem->name, mem->phy_addr, (u32)mem->len, mem->perf_mark);
			ret = udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NOT_MAP);
			if (ret != 0) {
				hw_err("Udev mem map failed. (devid=%u; die_id=%u; name=%s; ret=%d)\n",
					   devid, i, mem->name, ret);
				return ret;
			}
		}
	}

	return ret;
}

static int udev_set_ures_key_value(u32 devid, struct udev_hw_info *hw_info)
{
	struct soc_key_value *key_val;
	int ret;
	u32 i;

	for (i = 0; i < hw_info->key_value_num; ++i) {
		key_val = &hw_info->key_value_list[i];
		ret = ures_set_key_value(devid, key_val->key_name, key_val->value);
		if (ret != 0) {
			hw_err("Set die irq failed. (devid=%u; die_id=%u; ret=%d)\n",
				devid, i, ret);
			return ret;
		}
	}
	return 0;
}

static inline void udev_set_hsec_mem_align(u64 *addr, u64 align_size)
{
	*addr = (*addr + align_size - 1) & (~(align_size - 1));
}

static int udev_hsec_map(u32 devid, u64 hsec_base, u32 hsec_size)
{
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};

	ures_inst_pack(&inst, devid, 0);
	udev_soc_addr_pack(&addr, MAMI_HSEC_MEM, hsec_base, hsec_size, HW_PERF_SENSITIVE);
	return udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NOT_MAP);
}

static int udev_set_ures_hsec_mem(u32 devid, struct udev_hw_info *hw_info)
{
	struct ub_io_info ub_io = {0};
	struct udev_cfg cfg = {0};
	struct ub_hsec_mem_entry entry = {0};
	struct udev_hsec_mem_info *hmem;
	u64 hmem_base = 0;
	u32 hmem_len = 0, last_item_len = 0, map_mem_len = 0;
	int ret;
	u32 i;

	if (hw_info->hsec_mem_list == NULL) {   /* no need to set hsec mem info */
		return 0;
	}
	ret = ures_dev_get_attr(devid, MAMI_UDEV_IO_INFO, &ub_io, sizeof(struct ub_io_info));
	if (ret != 0) {
		hw_err("Get hsec mem base failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	hmem_base = ub_io.resv_mem_base;
	hmem_len = ub_io.resv_mem_len;
	hw_info("Get hsec mem.(devid=%u; len=%u)\n", devid, hmem_len);

	ret = ures_dev_get_attr(devid, MAMI_UDEV_CFG_INFO, &cfg, sizeof(struct udev_cfg));
	if (ret != 0) {
		hw_err("Get chip type failed. (ret=%d;devid=%u)", ret, devid);
		return ret;
	}

	/* uions has no high-security memory, return success directly */
	if (hmem_base == 0 && hmem_len == 0) {
		return 0;
	}
	if ((cfg.device_type == MAMI_KERN_TYPE_UNIONS) || (cfg.device_type == MAMI_KERN_TYPE_UNIONS_210)) {
		return 0;
	}

	for (i = 0; i < hw_info->hsec_mem_num; ++i) {
		hmem = &hw_info->hsec_mem_list[i];
		ret = ures_set_key_value(devid, hmem->base_name, hmem_base);
		if (ret != 0) {
			hw_err("Set hsec mem base failed. (devid=%u;name=%s;ret=%d)\n", devid, hmem->base_name, ret);
			return ret;
		}
		/* the last one is dynamically calculated, occupying all remaining high-security memory */
		if (i == hw_info->hsec_mem_num - 1) {
			last_item_len = ub_io.resv_mem_len > (hmem_base - ub_io.resv_mem_base) ?
				ub_io.resv_mem_len - (hmem_base - ub_io.resv_mem_base) : 0;
		}
		hmem_base += (hmem->entry_num * hmem->entry_size);
		udev_set_hsec_mem_align(&hmem_base, UDEV_HSEC_MEM_ALIGN_SIZE);
		entry.entry_num = hmem->entry_num;
		entry.entry_size = (i == hw_info->hsec_mem_num - 1) ? last_item_len : hmem->entry_size;
		ret = ures_dev_set_attr(devid, hmem->entry_name, &entry, sizeof(entry));
		if (ret != 0) {
			hw_err("Set hsec entry failed. (devid=%u;name=%s;ret=%d)\n", devid, hmem->entry_name, ret);
			return ret;
		}
	}

	map_mem_len = ub_io.resv_mem_len - last_item_len; /* the last block is used as a TPL dynamic memory pool, ctrlcpu does not need to access it, not mapped */
	ret = udev_hsec_map(devid, ub_io.resv_mem_base, map_mem_len);
	if (ret != 0) {
		hw_err("Udev hsec mem map failed. (devid=%u; base=0x%llx; size=0x%x; ret=%d)\n",
			   devid, ub_io.resv_mem_base, map_mem_len, ret);
		return ret;
	}
	hw_debug("Map hsec mem.(devid=%u; len=%u)\n", devid, map_mem_len);
	return 0;
}

static int udev_bbox_map(u32 devid, u64 black_box_addr, u32 black_box_len)
{
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};

	ures_inst_pack(&inst, devid, 0);
	udev_soc_addr_pack(&addr, MAMI_BBOX_MEM, black_box_addr, black_box_len, HW_PERF_SENSITIVE);
	return udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NOT_MAP);
}

static int udev_set_ures_bbox_mem(u32 devid)
{
	u64 black_box_addr = 0;
	u32 black_box_len = 0;
	struct udev_hw_identify hw = {0};
	struct udev_cfg cfg = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw attr failed. (ret=%d; devid=%u;)\n", ret, devid);
		return ret;
	}

	hw_info("The hw connect type is : %d\n", hw.connect_type);
	if (hw.connect_type == UDEV_UBUS || hw.connect_type == UDEV_LOCAL) {
		ret = ures_dev_get_attr(devid, MAMI_UDEV_CFG_INFO, &cfg, sizeof(struct udev_cfg));
		if (ret != 0) {
			hw_err("Get blackbox information failed. (ret=%d;devid=%u)", ret, devid);
			return ret;
		}

		black_box_addr = cfg.black_box_addr;
		black_box_len = cfg.black_box_len;
		if (black_box_addr == 0 || black_box_len == 0 || black_box_len > 0xF00000) {
			hw_warn("The chip doesn't support blackbox. (devid=%u, len=%u, addr=valid %d)\n",
				devid, black_box_len, (black_box_addr != 0));
			return 0;
		}

		ret = udev_bbox_map(devid, black_box_addr, black_box_len);
		if (ret != 0) {
			hw_err("Udev blackbox mem map failed. (devid=%u; size=0x%x; ret=%d)\n",
				devid, black_box_len, ret);
			return ret;
		}
	}
	return ret;
}

static int udev_set_ures_ras_mem(u32 devid)
{
	struct udev_cfg cfg = {0};
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_CFG_INFO, &cfg, sizeof(struct udev_cfg));
	if (ret != 0) {
		hw_err("Get dev attr failed. (ret=%d)\n", ret);
		return ret;
	}

	/* currently Ascend_v1 and Unions BIOS do not configure RAS memory, if RAS mem is not allocated here, return success as well */
	if (cfg.ras_err_status_block_addr == 0 || cfg.ras_err_status_block_len == 0) {
		hw_info("Ras mem not support. (devid=%u)\n", devid);
		return 0;
	}

	ures_inst_pack(&inst, devid, 0);
	udev_soc_addr_pack(&addr, MAMI_RAS_MEM, cfg.ras_err_status_block_addr, cfg.ras_err_status_block_len, HW_PERF_SENSITIVE);
	ret = udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NOT_MAP);
	if (ret != 0) {
		hw_err("Map ras mem failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	return ret;
}

static int udev_set_ures_dfx_mem(u32 devid, struct udev_hw_info *hw_info)
{
	struct ub_io_info ub_io = {0};
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};
	int ret;

	if ((hw_info->chip_type != MAMI_KERN_TYPE_ASCEND_V1) && (hw_info->chip_type != MAMI_KERN_TYPE_ASCEND_V2)) { /* only supported by Ascend_v1 / Ascend_v2 */
		return 0;
	}
	ret = ures_dev_get_attr(devid, MAMI_UDEV_IO_INFO, &ub_io, sizeof(struct ub_io_info));
	if (ret != 0) {
		hw_err("Get dfx mem base failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	if ((ub_io.regs[0].start_base == 0) || (ub_io.regs[0].start_len == 0)) {
		hw_info("Dfx mem not support. (devid=%u)\n", devid);
		return 0;
	}
	ures_inst_pack(&inst, devid, 0);
	udev_soc_addr_pack(&addr, "dfx_mem_base", ub_io.regs[0].start_base, ub_io.regs[0].start_len * UB_IO_REG_LEN_UNIT, HW_PERF_SENSITIVE);
	ret = udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NOT_MAP);
	if (ret != 0) {
		hw_err("Map dfx mem failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	return 0;
}

static int udev_set_ures_hw_info(u32 devid, struct udev_hw_info *hw_info)
{
	u32 ret = 0;

	ret |= (u32)udev_set_ures_key_value(devid, hw_info);
	ret |= (u32)udev_set_ures_reg_info(devid, hw_info);
	ret |= (u32)udev_set_ures_mem_info(devid, hw_info);
	ret |= (u32)udev_set_ures_hsec_mem(devid, hw_info);
	ret |= (u32)udev_set_ures_bbox_mem(devid);
	ret |= (u32)udev_set_ures_ras_mem(devid);
	ret |= (u32)udev_set_ures_dfx_mem(devid, hw_info);
	if (ret != 0) {
		hw_err("Set ures hw info failed. (devid=%u)\n", devid);
		/* ures reset devid ures */
		udev_reset_ures(devid);
		return (int)ret;
	}
	return (int)ret;
}

int udev_init_hw_info(u32 devid, u32 chip_type)
{
	struct udev_hw_info *hw_info;
	int ret;

	if ((chip_type >= MAMI_KERN_TYPE_MAX) || (chip_type <= MAMI_KERN_TYPE_INVALID)) {
		hw_err("Invalid chip_type. (chip=%u)\n", chip_type);
		return -EINVAL;
	}

	hw_info = &g_chip_mng.chip_list[chip_type];
	if (hw_info->valid != HW_VALID) {
		hw_err("Not support chip_type, not hw info. (chip=%u)\n", chip_type);
		return -EINVAL;
	}
	if (g_udev_hw_status[devid] == HW_VALID) {
		hw_err("device has already init. (devid=%u;chip=%u)\n", devid, chip_type);
		return -EEXIST;
	}
	ret = udev_set_ures_hw_info(devid, hw_info);
	if (ret != 0) {
		hw_err("set hw info failed. (devid=%u; chip=%u)\n", devid, chip_type);
		return ret;
	}
	g_udev_hw_status[devid] = HW_VALID;
	return 0;
}

int udev_init_win_axi(u32 devid, u32 chip_type, u64 ucmi_reg_base, struct ucmi_pcie_axi_node *node)
{
	struct udev_hw_info *hw_info = NULL;
	u32 i = 0;

	if ((chip_type >= MAMI_KERN_TYPE_MAX) || (chip_type <= MAMI_KERN_TYPE_INVALID)) {
		hw_err("Invalid chip_type. (chip=%u)\n", chip_type);
		return -EINVAL;
	}

	hw_info = &g_chip_mng.chip_list[chip_type];
	if (hw_info->valid != HW_VALID) {
		hw_err("Not support chip_type, not hw info. (chip=%u)\n", chip_type);
		return -EINVAL;
	}
	for (i = 0; i < hw_info->init_win_axi_num; i++) {
		node[hw_info->init_win_axi[i].axi_id].axi_addr = ucmi_reg_base + (UCMI_AXI_REG_BASE + UCMI_AXI_REG_OFFSET * (hw_info->init_win_axi[i].axi_id));
		node[hw_info->init_win_axi[i].axi_id].axi_ref++;
		node[hw_info->init_win_axi[i].axi_id].axi_val = hw_info->init_win_axi[i].axi_val;
	}
	return 0;
}

static void udev_remove_all_win(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw attr failed. (ret=%d; devid=%u)\n", ret, devid);
		return;
	}

	if ((hw.connect_type != UDEV_PCIE_ESPI) && (hw.connect_type != UDEV_ESPI)) {
		return;
	}

	ucmi_remove_all_dynamic_win(hw.connect_type, devid);
}

void udev_uninit_hw_info(u32 devid, u32 chip_type)
{
	hw_info("Dev hw info uninit. (devid=%u)\n", devid);
	udev_remove_all_win(devid);
	g_udev_hw_status[devid] = HW_INVALID;
	udev_reset_ures(devid);
}
