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
#include "udev_dev_kunpeng950_def.h"
#include "udev_hw_info.h"

enum chipID {
	CHIP0 = 0,
	CHIP1 = 1,
	CHIP2,
	CHIP3
};

/* kunpeng950 has only one udie, but has 4 chip max */
static struct soc_addr chip_kunpeng950_chip0_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_NBAR_TP_OFFSET(CHIP0, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_NBAR_TA_OFFSET(CHIP0, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_NBAR_MISC_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL0_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL1_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL2_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL3_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL4_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA0_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA1_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA2_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA3_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA4_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_NBAR_UMMU_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MGTQ_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(1)},
};

static struct soc_addr chip_kunpeng950_chip0_mem_list[] = {
	{MAMI_CUSTOM_MEM, UDEV_CUSTOM_MEM, SIZE_KB(4)}
};

static struct soc_addr chip_kunpeng950_chip1_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_NBAR_TP_OFFSET(CHIP1, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_NBAR_TA_OFFSET(CHIP1, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_NBAR_MISC_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL0_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL1_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL2_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL3_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL4_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA0_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA1_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA2_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA3_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA4_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_NBAR_UMMU_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MGTQ_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(1)},
};

static struct soc_addr chip_kunpeng950_chip2_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_NBAR_TP_OFFSET(CHIP2, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_NBAR_TA_OFFSET(CHIP2, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_NBAR_MISC_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL0_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL1_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL2_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL3_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL4_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA0_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA1_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA2_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA3_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA4_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_NBAR_UMMU_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(4)},
};

static struct soc_addr chip_kunpeng950_chip3_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_NBAR_TP_OFFSET(CHIP3, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_NBAR_TA_OFFSET(CHIP3, UDEV_DIE0),	SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_NBAR_MISC_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL0_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL1_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL2_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL3_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_NBAR_NL4_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA0_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA1_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA2_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA3_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_NBAR_BA4_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_NBAR_UMMU_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(4)},
};

static struct soc_key_value chip_kunpeng950_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_NL_NUM},
	{MAMI_UDIE_VER, UDIE_VERSION_V1},
};

static struct init_win_axi chip_kunpeng950_1p_init_win_axi[] = {
	{ 14, 0x330 },
	{ 15, 0x730 },
};

static struct udev_hsec_mem_info hsec_mem_list_with_mdp[] = HSEC_MEM_WITH_MDP;

int udev_add_chip_kunpeng950_1p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG950_1P;

	hw_info.mem_die_bitmap = (0x1 << UDEV_DIE0);
	hw_info.reg_die_bitmap = (0x1 << UDEV_DIE0);

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_reg_list);

	hw_info.key_value_list = chip_kunpeng950_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng950_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);
	hw_info.init_win_axi = chip_kunpeng950_1p_init_win_axi;
	hw_info.init_win_axi_num = ARRAY_SIZE(chip_kunpeng950_1p_init_win_axi);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG950_1P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng950_1p_info, FEATURE_LOADER_STAGE_5);

static struct init_win_axi chip_kunpeng950_2p_init_win_axi[] = {
	{ 14, 0x330 },
	{ 15, 0x730 },
};

int udev_add_chip_kunpeng950_2p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG950_2P;

	hw_info.mem_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_reg_list);
	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE1].addr_list = chip_kunpeng950_chip1_reg_list;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(chip_kunpeng950_chip1_reg_list);

	hw_info.key_value_list = chip_kunpeng950_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng950_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);
	hw_info.init_win_axi = chip_kunpeng950_2p_init_win_axi;
	hw_info.init_win_axi_num = ARRAY_SIZE(chip_kunpeng950_2p_init_win_axi);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG950_2P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng950_2p_info, FEATURE_LOADER_STAGE_5);

int udev_add_chip_kunpeng950_4p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG950_4P;

	hw_info.mem_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3));
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3));

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng950_chip0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng950_chip0_reg_list);

	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE1].addr_list = chip_kunpeng950_chip1_reg_list;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(chip_kunpeng950_chip1_reg_list);

	hw_info.addr_mem[UDEV_DIE2].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE2].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE2].addr_list = chip_kunpeng950_chip2_reg_list;
	hw_info.addr_reg[UDEV_DIE2].addr_num = ARRAY_SIZE(chip_kunpeng950_chip2_reg_list);

	hw_info.addr_mem[UDEV_DIE3].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE3].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE3].addr_list = chip_kunpeng950_chip3_reg_list;
	hw_info.addr_reg[UDEV_DIE3].addr_num = ARRAY_SIZE(chip_kunpeng950_chip3_reg_list);

	hw_info.key_value_list = chip_kunpeng950_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng950_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG950_4P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng950_4p_info, FEATURE_LOADER_STAGE_5);

/* kunpeng960 */
/* kunpeng960 has 4 chips, each with 2 dies. */
static struct soc_addr chip_kunpeng960_chip0_die0_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP0, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP0, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP0, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP0, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP0, UDEV_DIE0), SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MGTQ_OFFSET(CHIP0, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP0, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP0, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP0, UDEV_DIE0),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip0_die1_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP0, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP0, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP0, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP0, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP0, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP0, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP0, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP0, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP0, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP0, UDEV_DIE1), SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MGTQ_OFFSET(CHIP0, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP0, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP0, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP0, UDEV_DIE1),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip1_die0_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP1, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP1, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP1, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP1, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP1, UDEV_DIE0), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP1, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP1, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP1, UDEV_DIE0),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip1_die1_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP1, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP1, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP1, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP1, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP1, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP1, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP1, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP1, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP1, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP1, UDEV_DIE1), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP1, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP1, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP1, UDEV_DIE1),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip2_die0_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP2, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP2, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP2, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP2, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP2, UDEV_DIE0), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP2, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP2, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP2, UDEV_DIE0),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip2_die1_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP2, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP2, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP2, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP2, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP2, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP2, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP2, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP2, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP2, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP2, UDEV_DIE1), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP2, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP2, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP2, UDEV_DIE1),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip3_die0_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP3, UDEV_DIE0),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP3, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP3, UDEV_DIE0),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP3, UDEV_DIE0), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP3, UDEV_DIE0), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP3, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP3, UDEV_DIE0),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP3, UDEV_DIE0),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip3_die1_reg_list[] = {
	{MAMI_TP_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TP_OFFSET(CHIP3, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_TA_OFFSET(CHIP3, UDEV_DIE1),   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MISC_OFFSET(CHIP3, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL0_OFFSET(CHIP3, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_NL1_OFFSET(CHIP3, UDEV_DIE1),  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA0_OFFSET(CHIP3, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA1_OFFSET(CHIP3, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_BA2_OFFSET(CHIP3, UDEV_DIE1),  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_UMMU_OFFSET(CHIP3, UDEV_DIE1), SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_KUNPENG_960_FM + UDEV_NBAR_MSGQ_OFFSET(CHIP3, UDEV_DIE1), SIZE_MB(4)},
	{MAMI_UPA0_REG, UDEV_UPA0_KUNPENG_960_BASE(CHIP3, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UPA1_REG, UDEV_UPA1_KUNPENG_960_BASE(CHIP3, UDEV_DIE1),					   SIZE_KB(128)},
	{MAMI_UAMU_REG, UDEV_UAMU_KUNPENG_960_BASE(CHIP3, UDEV_DIE1),					   SIZE_KB(12)},
};

static struct soc_addr chip_kunpeng960_chip0_mem_list[] = {
	{MAMI_CUSTOM_MEM, KUNPENG_960_UDEV_CUSTOM_MEM, SIZE_KB(4)}
};

static struct soc_key_value chip_kunpeng960_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_NL_NUM_KUNPENG_960},
	{MAMI_UDIE_VER, UDIE_VERSION_V2},
};

int udev_add_chip_kunpeng960_1p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG960_1P;

	hw_info.mem_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die0_reg_list);
	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE1].addr_list = chip_kunpeng960_chip0_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die1_reg_list);

	hw_info.key_value_list = chip_kunpeng960_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng960_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG960_1P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng960_1p_info, FEATURE_LOADER_STAGE_5);

int udev_add_chip_kunpeng960_2p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG960_2P;

	hw_info.mem_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3));
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3));

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE1].addr_list = chip_kunpeng960_chip0_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die1_reg_list);

	hw_info.addr_mem[UDEV_DIE2].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE2].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE2].addr_list = chip_kunpeng960_chip1_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE2].addr_num = ARRAY_SIZE(chip_kunpeng960_chip1_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE3].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE3].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE3].addr_list = chip_kunpeng960_chip1_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE3].addr_num = ARRAY_SIZE(chip_kunpeng960_chip1_die1_reg_list);

	hw_info.key_value_list = chip_kunpeng960_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng960_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG960_2P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng960_2p_info, FEATURE_LOADER_STAGE_5);

int udev_add_chip_kunpeng960_4p_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_KUNPENG960_4P;

	hw_info.mem_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3) |
							  (0x1 << UDEV_DIE4) | (0x1 << UDEV_DIE5) | (0x1 << UDEV_DIE6) | (0x1 << UDEV_DIE7));
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | (0x1 << UDEV_DIE1) | (0x1 << UDEV_DIE2) | (0x1 << UDEV_DIE3) |
							  (0x1 << UDEV_DIE4) | (0x1 << UDEV_DIE5) | (0x1 << UDEV_DIE6) | (0x1 << UDEV_DIE7));

	hw_info.addr_mem[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_mem_list;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_mem_list);
	hw_info.addr_reg[UDEV_DIE0].addr_list = chip_kunpeng960_chip0_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE1].addr_list = chip_kunpeng960_chip0_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(chip_kunpeng960_chip0_die1_reg_list);

	hw_info.addr_mem[UDEV_DIE2].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE2].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE2].addr_list = chip_kunpeng960_chip1_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE2].addr_num = ARRAY_SIZE(chip_kunpeng960_chip1_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE3].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE3].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE3].addr_list = chip_kunpeng960_chip1_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE3].addr_num = ARRAY_SIZE(chip_kunpeng960_chip1_die1_reg_list);

	hw_info.addr_mem[UDEV_DIE4].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE4].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE4].addr_list = chip_kunpeng960_chip2_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE4].addr_num = ARRAY_SIZE(chip_kunpeng960_chip2_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE5].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE5].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE5].addr_list = chip_kunpeng960_chip2_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE5].addr_num = ARRAY_SIZE(chip_kunpeng960_chip2_die1_reg_list);

	hw_info.addr_mem[UDEV_DIE6].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE6].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE6].addr_list = chip_kunpeng960_chip3_die0_reg_list;
	hw_info.addr_reg[UDEV_DIE6].addr_num = ARRAY_SIZE(chip_kunpeng960_chip3_die0_reg_list);

	hw_info.addr_mem[UDEV_DIE7].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE7].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE7].addr_list = chip_kunpeng960_chip3_die1_reg_list;
	hw_info.addr_reg[UDEV_DIE7].addr_num = ARRAY_SIZE(chip_kunpeng960_chip3_die1_reg_list);

	hw_info.key_value_list = chip_kunpeng960_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(chip_kunpeng960_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);

	return udev_add_hw_info(MAMI_KERN_TYPE_KUNPENG960_4P, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_chip_kunpeng960_4p_info, FEATURE_LOADER_STAGE_5);
