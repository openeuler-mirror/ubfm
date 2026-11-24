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
#include <linux/kernel.h>
#include "fmdevm.h"
#include "ubfm_common.h"
#include "feature_loader.h"
#include "udev_dev_hssu_v100_def.h"
#include "udev_hw_info.h"

static struct soc_addr chip_hssu_v100_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_TP_OFFSET,	SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_TA_OFFSET,	SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_MISC_OFFSET,  SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_NL0_OFFSET,   SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_BA0_OFFSET,   SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_UMMU_OFFSET,  SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_MSGQ_OFFSET,  SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_MGTQ_OFFSET,  SIZE_MB(1)},
};

static struct soc_addr chip_hssu_v100_chip0_mem_list[] = {
	{MAMI_CUSTOM_MEM, UDEV_CUSTOM_MEM, SIZE_KB(4)}
};

static struct soc_key_value chip_hssu_v100_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_HSSU_V100_NL_NUM},
	{MAMI_UDIE_VER, UDIE_VERSION_V1},
};

static struct udev_hsec_mem_info hsec_mem_list[] = HSEC_MEM_LIST;

int udev_add_chip_hssu_v100_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_HSSU_V100;

	hw_info.mem_die_bitmap = (0x1 << UDEV_DIE0);
	hw_info.reg_die_bitmap = (0x1 << UDEV_DIE0);

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_hssu_v100_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_hssu_v100_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_hssu_v100_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_hssu_v100_reg_list);

	hw_info.key_value_list = chip_hssu_v100_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_hssu_v100_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list);
	return udev_add_hw_info(MAMI_KERN_TYPE_HSSU_V100, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_hssu_v100_info, FEATURE_LOADER_STAGE_5);
