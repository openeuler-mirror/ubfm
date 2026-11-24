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
#include "ubfm_common.h"
#include "feature_loader.h"
#include "mctrlq_log.h"

#include "ures.h"
#include "uop.h"
#include "udev_map.h"
#include "ctrlq_kern.h"
#include "ctrlq_hw.h"
#include "ctrlq_chip.h"
#include "ctrlq.h"
#include "fmdevm.h"
#include "mctrlq.h"

#define CTRLQ_REG_SIZE		  0x1000000

/* The channel in the mapping table must be within the valid_chan_num range */
/* Mapping table for Ascend_v1 */
static const ctrlq_ue_map_t ctrq_ue_map_v100[] = {
	{0,  15, 2, 17,  0, 0x02238, CTRLQ_CHAN_URMA},  /* chan0~15: ue2~17 URMA */
	{16, 16, 1,  1,  16, 0x0222C, CTRLQ_CHAN_CDMA},  /* chan16: ue1 CDMA */
};

/* Mapping table for Ascend_v2 */
static const ctrlq_ue_map_t ctrq_ue_map_ascend_v2[] = {
	{0,  0,  1,  1,  0, 0x0222C, CTRLQ_CHAN_CDMA},  /* chan0: ue1 CDMA */
	{1,  20, 2,  21, 1, 0x02238, CTRLQ_CHAN_URMA},  /* chan1~20: ue2~21 URMA */
};

/* Mapping table for kunpeng960 */
static const ctrlq_ue_map_t ctrq_ue_map_kunpeng960[] = {
	{0,  7,  2,  9,  0, 0x02238, CTRLQ_CHAN_URMA},  /* chan0~7: urma ue2~9 */
	{8, 41, 12, 45,  0, 0x02238, CTRLQ_CHAN_URMA},  /* chan8~41: urma vue12~45 */
	{42, 42, 1,  1, 42, 0x0222C, CTRLQ_CHAN_CDMA},  /* chan42: ue1 */
	{43, 44, 10, 11, 42, 0x0222C, CTRLQ_CHAN_CDMA},  /* chan43~44: cdma vue10~11 */
};

/* Mapping table for HSSU_V100 */
static const ctrlq_ue_map_t ctrq_ue_map_hssu_v100[] = {
	{0, 1, 2, 3, 0, 0x02238, CTRLQ_CHAN_URMA}, /* chan0~1: ue2~3 URMA */
	{2, 2, 1, 1, 2, 0x0222C, CTRLQ_CHAN_CDMA}, /* chan2: ue1 CDMA */
};
/* Determine the chip type and return the corresponding mapping table */
const ctrlq_ue_map_t *ctrlq_kern_get_chip_ue_map(enum mami_udie_version udie_version, u32 chip_type, u32 *map_size)
{
	if (udie_version == UDIE_VERSION_V2) {
		if (chip_is_kunpeng960(chip_type)) {
			*map_size = ARRAY_SIZE(ctrq_ue_map_kunpeng960);
			return ctrq_ue_map_kunpeng960;
		}
		*map_size = ARRAY_SIZE(ctrq_ue_map_ascend_v2);
		return ctrq_ue_map_ascend_v2;
	}
	if (chip_is_ssd(chip_type)) {
		*map_size = ARRAY_SIZE(ctrq_ue_map_hssu_v100);
		return ctrq_ue_map_hssu_v100;
	}
	*map_size = ARRAY_SIZE(ctrq_ue_map_v100);
	return ctrq_ue_map_v100;
}

/* Return the corresponding entry in the mapping table */
const ctrlq_ue_map_t *ctrq_kern_get_field(u32 chan_id, enum mami_udie_version udie_version, u32 chip_type)
{
	u32 map_size, i;
	const ctrlq_ue_map_t *map_table = ctrlq_kern_get_chip_ue_map(udie_version, chip_type, &map_size);

	for (i = 0; i < map_size; i++) {
		const ctrlq_ue_map_t *map_item = &map_table[i];

		if (chan_id >= map_item->chan_start && chan_id <= map_item->chan_end) {
			return map_item;
		}
	}

	return NULL; /* No matching entry found, return NULL */
}

u32 ctrq_kern_get_offset_start(u32 chan_id, enum mami_udie_version udie_version, u32 chip_type)
{
	const ctrlq_ue_map_t *map = ctrq_kern_get_field(chan_id, udie_version, chip_type);

	if (map != NULL) {
		return (chan_id - map->urma_or_cdma_chan_start);
	}
	return 0;
}

u32 ctrq_kern_get_cfg_base(u32 chan_id, enum mami_udie_version udie_version, u32 chip_type)
{
	const ctrlq_ue_map_t *map = ctrq_kern_get_field(chan_id, udie_version, chip_type);

	if (map != NULL) {
		return map->ctrlq_cfg_base;
	}
	return 0;
}

int ctrlq_kern_get_ue_id(u32 chan_id, enum mami_udie_version udie_version, u32 chip_type, u32 *ue_id)
{
	const ctrlq_ue_map_t *map = ctrq_kern_get_field(chan_id, udie_version, chip_type);

	if (ue_id == NULL) {
		mctrlq_err("Ue_id is NULL. (chan_id=%u)\n", chan_id);
		return -EINVAL;
	}
	if (map == NULL) {
		mctrlq_err("Ctrlq ue_map NULL. (chan_id=%u)\n", chan_id);
		return -ENOENT;
	}

	*ue_id = map->ue_start + (chan_id - map->chan_start);
	return 0;
}

static int _ctrlq_sq_attr_pack(struct ctrlq_attr *attr, struct ures_inst *inst,
	u32 chan_id, struct ures_addr_info *reg)
{
	u64 tx_reg_base_l, tx_reg_base_h;
	u32 tx_mem_l = 0, tx_mem_h = 0;
	int ret;

	tx_reg_base_l = reg->addr + CTRLQ_REG_TX_BASE_L;
	tx_reg_base_h = reg->addr + CTRLQ_REG_TX_BASE_H;

	ret = uop_kern_readl(inst->devid, tx_reg_base_l, &tx_mem_l);
	ret |= uop_kern_readl(inst->devid, tx_reg_base_h, &tx_mem_h);
	if (ret != 0) {
		mctrlq_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}

	attr->sq_asic.base = ctrlq_addr_combine(tx_mem_h, tx_mem_l);
	attr->sq_asic.tail = reg->addr + CTRLQ_REG_TX_TAIL;
	attr->sq_asic.head = reg->addr + CTRLQ_REG_TX_HEAD;
	attr->sq_asic.depth = reg->addr + CTRLQ_REG_TX_DEPTH;
	attr->sq_asic.intr = reg->addr + CTRLQ_REG_INT_GEN;

	return 0;
}

static int _ctrlq_cq_attr_pack(struct ctrlq_attr *attr, struct ures_inst *inst,
	u32 chan_id, struct ures_addr_info *reg)
{
	u64 rx_reg_base_l, rx_reg_base_h;
	u32 rx_mem_l = 0, rx_mem_h = 0;
	int ret;

	rx_reg_base_l = reg->addr + CTRLQ_REG_RX_BASE_L;
	rx_reg_base_h = reg->addr + CTRLQ_REG_RX_BASE_H;

	ret = uop_kern_readl(inst->devid, rx_reg_base_l, &rx_mem_l);
	ret |= uop_kern_readl(inst->devid, rx_reg_base_h, &rx_mem_h);
	if (ret != 0) {
		mctrlq_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}

	attr->cq_asic.base = ctrlq_addr_combine(rx_mem_h, rx_mem_l);
	attr->cq_asic.tail = reg->addr + CTRLQ_REG_RX_TAIL;
	attr->cq_asic.head = reg->addr + CTRLQ_REG_RX_HEAD;
	attr->cq_asic.depth = reg->addr + CTRLQ_REG_RX_DEPTH;

	return 0;
}

static int ctrlq_get_attr_base(struct ures_inst *inst, struct ctrlq_attr *attr, u32 chan_id,
	struct ures_addr_info *reg, struct ctrlq_global_info *ctrlq)
{
	enum mami_udie_version udie_version = ctrlq->udie_version;
	u32 chip_type = ctrlq->chip_type;
	struct ures_addr_info misc_reg;
	u32 addr_l, addr_h, ue_id = 0;
	u64 base_l, base_h;
	int ret;

	ret = ures_get_reg_base(inst, MAMI_MISC_REG, &misc_reg);
	if (ret != 0) {
		mctrlq_err("Get misc reg fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}
#ifndef EMU_ST
	base_l = base_h = misc_reg.addr;
	base_l += 0x10000 + ctrq_kern_get_cfg_base(chan_id, udie_version, chip_type);

	ret = uop_kern_readl(inst->devid, base_l, &addr_l);
	if (ret != 0) {
		mctrlq_err("Get ctrlq low cfg base fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}

	base_h += 0x10000 + 0x4 + ctrq_kern_get_cfg_base(chan_id, udie_version, chip_type);
	ret = uop_kern_readl(inst->devid, base_h, &addr_h);
	if (ret != 0) {
		mctrlq_err("Get ctrlq high cfg base fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}
	reg->addr = ctrlq_addr_combine(addr_h, addr_l);
	reg->addr += ctrq_kern_get_offset_start(chan_id, udie_version, chip_type) * CHAN_BASE_DIFF;
	reg->size = CTRLQ_REG_SIZE;
#else
	reg->addr = misc_reg.addr;
	reg->size = CTRLQ_REG_SIZE;
#endif
	if (reg->addr == 0) {
		mctrlq_err("Invalid ctrlq reg. (devid=%u; die_id=%u; chan_id=%u)\n", inst->devid, inst->die_id,
				   chan_id);
		return -EINVAL;
	}
	ret = ctrlq_kern_get_ue_id(chan_id, udie_version, chip_type, &ue_id);
	if (ret != 0) {
		mctrlq_err("Ctrlq kern get ue_id fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, inst->devid, inst->die_id, chan_id);
		return ret;
	}

	attr->cq_asic.int_mask = misc_reg.addr + IMP_CTRLQ_TX_INT_MASK + 0x4 * ue_id;

	return 0;
}

static int ctrlq_attr_pack(struct ctrlq_attr *attr, u32 die_id, u32 chan_id, struct ctrlq_global_info *ctrlq)
{
	struct ures_inst inst = {.devid = ctrlq->devid, .die_id = die_id};
	struct udev_soc_addr addr = {.name = "ctrlq_reg_base"};
	struct ures_addr_info reg = {0};
	int ret;

	ret = ctrlq_get_attr_base(&inst, attr, chan_id, &reg, ctrlq);
	if (ret != 0) {
		mctrlq_err("Get Attr base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst.devid, inst.die_id);
		return ret;
	}

	udev_soc_addr_pack(&addr, "ctrlq_reg_base", reg.addr, reg.size, HW_PERF_SENSITIVE);
	ret = udev_mem_map(&inst, UOP_ITEM_REG, &addr, UOP_NEED_MAP);
	if (ret != 0) {
		mctrlq_err("Add cfg base failed. (ret=%d; devid=%u; dieid=%u; chan_id=%u)\n",
				   ret, inst.devid, inst.die_id, chan_id);
		return ret;
	}

	ret = _ctrlq_sq_attr_pack(attr, &inst, chan_id, &reg);
	ret |= _ctrlq_cq_attr_pack(attr, &inst, chan_id, &reg);
	if (ret != 0) {
		mctrlq_err("Add cfg base failed. (ret=%d; devid=%u; dieid=%u; chan_id=%u;)\n",
			ret, inst.devid, inst.die_id, chan_id);
	}

	return ret;
}

static const struct ctrlq_hw_ops g_chip_ops = {
	.attr_pack = ctrlq_attr_pack
};

int ctrlq_chip_mod_init(void)
{
	ctrlq_hw_ops_register(&g_chip_ops);
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ctrlq_chip_mod_init, FEATURE_LOADER_STAGE_0);

#define CHAN_BASE_TX_RX_DIFF	0x10000
static int ctrlq_schan_get_mem_addr(u32 devid, u32 die_id, enum mami_kern_schan_addr_type type, u64 *addr)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	struct ures_addr_info mem = {0};
	char name[64];
	int ret;

	(void)UBFM_SPRINTF(name, sizeof(name), "ctrlq_mem_base_win%u_local", die_id);
	ret = ures_get_mem_base(&inst, name, &mem);
	if (ret != 0) {
		mctrlq_err("Get ctrlq mem_local fail. (ret=%d;devid=%u;die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	if (type == MAMI_KERN_SCHAN_ADDR_RX) {
		*addr = mem.addr + CTRLQ_SCHAN_ID * CHAN_BASE_DIFF;
		return 0;
	}

	*addr = mem.addr + CTRLQ_SCHAN_ID * CHAN_BASE_DIFF + CHAN_BASE_TX_RX_DIFF;
	return 0;
}

static int ctrlq_schan_get_reg_addr(u32 devid, u32 die_id, u64 *addr)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	struct ures_addr_info reg = {0};
	char name[64];
	int ret;

	(void)UBFM_SPRINTF(name, sizeof(name), "ctrlq_reg_base_win%u_local", die_id);
	ret = ures_get_reg_base(&inst, name, &reg);
	if (ret != 0) {
		mctrlq_err("Get ctrlq reg_local fail. (ret=%d;devid=%u;die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	*addr = reg.addr + CTRLQ_REG_BASE + CTRLQ_SCHAN_ID * CHAN_BASE_DIFF;
	return 0;
}

/* Get the address of the specified type of the CtrlQ s-channel mapped to the bar space */
int mami_ctrlq_schan_get_addr(u32 devid, u32 die_id, enum mami_kern_schan_addr_type type, u64 *addr)
{
	int ret;

	ret = ctrlq_schan_check_pub(devid, die_id);
	if (ret != 0) {
		return ret;
	}
	if (type >= MAMI_KERN_SCHAN_ADDR_MAX) {
		mctrlq_err("Invalid type, must in [0, %d) but type=%u.(devid=%u;dieId=%u)\n",
				   MAMI_KERN_SCHAN_ADDR_MAX, type, devid, die_id);
		return -EINVAL;
	}
	if (addr == NULL) {
		mctrlq_err("Addr is NULL.(devid=%u; dieId=%u)\n", devid, die_id);
		return -EINVAL;
	}

	if (type == MAMI_KERN_SCHAN_ADDR_REG) {
		ret = ctrlq_schan_get_reg_addr(devid, die_id, addr);
		if (ret != 0) {
			mctrlq_err("Get schan ctrlq reg addr fail.(ret=%d;devid=%u;dieId=%u;type=%d)\n", ret, devid, die_id, type);
			return ret;
		}
	} else {
		ret = ctrlq_schan_get_mem_addr(devid, die_id, type, addr);
		if (ret != 0) {
			mctrlq_err("Get schan addr fail.(ret=%d;devid=%u;dieId=%u;type=%d)\n", ret, devid, die_id, type);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mami_ctrlq_schan_get_addr);
