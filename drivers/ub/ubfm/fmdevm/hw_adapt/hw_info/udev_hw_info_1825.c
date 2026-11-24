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
#include "udev_dev_1825_def.h"
#include "udev_hw_info.h"

static struct soc_addr chip_1825_reg_list[] = {
	{MAMI_MISC_REG, UDEV_NBAR_MISC_OFFSET, SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_NL0_OFFSET,  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_NL1_OFFSET,  SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_NL2_OFFSET,  SIZE_MB(8)},
	{MAMI_D2H_REG,  UDEV_NBAR_D2H_OFFSET,  SIZE_MB(1)},
};

static struct soc_key_value chip_1825_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_1825NL_NUM},
	{MAMI_UDIE_VER, UDIE_VERSION_V1},
};

int udev_add_chip_1825_info(void)
{
	/* mami of 1825 cannot access memory, high-security memory, bbox, related addresses default to 0 so they will not be initialized into the resource repository */
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_1825;

	hw_info.reg_die_bitmap = (0x1 << UDEV_DIE0);

	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_1825_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_1825_reg_list);

	hw_info.key_value_list = chip_1825_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_1825_key_value_list);

	return udev_add_hw_info(MAMI_KERN_TYPE_1825, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_1825_info, FEATURE_LOADER_STAGE_5);
