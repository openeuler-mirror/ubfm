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
#include "udev_map.h"
#include "udev_dev_ascend_v1_def.h"
#include "udev_hw_info.h"

static struct soc_addr ascend_v1_reg_list_die0[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_DIE0_NBAR_TP_OFFSET,   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_DIE0_NBAR_TA_OFFSET,   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_DIE0_NBAR_MISC_OFFSET, SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_NL0_OFFSET,  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_NL1_OFFSET,  SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_NL2_OFFSET,  SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_NL3_OFFSET,  SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_NL4_OFFSET,  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_BA0_OFFSET,  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_BA1_OFFSET,  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_BA2_OFFSET,  SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_BA3_OFFSET,  SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_DIE0_NBAR_BA4_OFFSET,  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_DIE0_NBAR_UMMU_OFFSET, SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_DIE0_NBAR_MSGQ_OFFSET, SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_FM + UDEV_DIE0_NBAR_MGTQ_OFFSET, SIZE_MB(1)},
};

static struct soc_addr ascend_v1_reg_list_die1[] = {
	{MAMI_TP_REG,   UDEV_NBAR_FM + UDEV_DIE1_NBAR_TP_OFFSET,   SIZE_MB(4)},
	{MAMI_TA_REG,   UDEV_NBAR_FM + UDEV_DIE1_NBAR_TA_OFFSET,   SIZE_MB(4)},
	{MAMI_MISC_REG, UDEV_NBAR_FM + UDEV_DIE1_NBAR_MISC_OFFSET, SIZE_MB(1)},
	{MAMI_NL0_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_NL0_OFFSET,  SIZE_MB(8)},
	{MAMI_NL1_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_NL1_OFFSET,  SIZE_MB(8)},
	{MAMI_NL2_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_NL2_OFFSET,  SIZE_MB(8)},
	{MAMI_NL3_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_NL3_OFFSET,  SIZE_MB(8)},
	{MAMI_NL4_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_NL4_OFFSET,  SIZE_MB(8)},
	{MAMI_BA0_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_BA0_OFFSET,  SIZE_MB(4)},
	{MAMI_BA1_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_BA1_OFFSET,  SIZE_MB(4)},
	{MAMI_BA2_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_BA2_OFFSET,  SIZE_MB(4)},
	{MAMI_BA3_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_BA3_OFFSET,  SIZE_MB(4)},
	{MAMI_BA4_REG,  UDEV_NBAR_FM + UDEV_DIE1_NBAR_BA4_OFFSET,  SIZE_MB(4)},
	{MAMI_UMMU_REG, UDEV_NBAR_FM + UDEV_DIE1_NBAR_UMMU_OFFSET, SIZE_MB(1)},
	{MAMI_MSGQ_REG, UDEV_NBAR_FM + UDEV_DIE1_NBAR_MSGQ_OFFSET, SIZE_MB(4)},
	{MAMI_MGTQ_REG, UDEV_NBAR_FM + UDEV_DIE1_NBAR_MGTQ_OFFSET, SIZE_MB(1)},
};


static struct init_win_axi ascend_v1_init_win_axi[] = {
	{ 14, 0x2480 },
	{ 15, 0x2C80 },
};

static struct soc_addr ascend_v1_mem_list_die0[] = {
	{MAMI_CUSTOM_MEM, UDEV_CUSTOM_MEM, SIZE_KB(4)}
};

static struct soc_key_value ascend_v1_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_NL_NUM},
	{MAMI_UDIE_VER, UDIE_VERSION_V1},
};

static struct udev_hsec_mem_info hsec_mem_list_with_mdp[] = HSEC_MEM_WITH_MDP;

int udev_add_ascend_v1_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_ASCEND_V1;

	hw_info.mem_die_bitmap = (0x1 << UDEV_DIE0);
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));

	hw_info.addr_mem[UDEV_DIE0].addr_list = ascend_v1_mem_list_die0;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(ascend_v1_mem_list_die0);
	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE0].addr_list = ascend_v1_reg_list_die0;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(ascend_v1_reg_list_die0);
	hw_info.addr_reg[UDEV_DIE1].addr_list = ascend_v1_reg_list_die1;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(ascend_v1_reg_list_die1);

	hw_info.key_value_list = ascend_v1_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(ascend_v1_key_value_list);
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);
	hw_info.init_win_axi = ascend_v1_init_win_axi;
	hw_info.init_win_axi_num = ARRAY_SIZE(ascend_v1_init_win_axi);
	return udev_add_hw_info(MAMI_KERN_TYPE_ASCEND_V1, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_ascend_v1_info, FEATURE_LOADER_STAGE_5);

/* Ascend_v2 */
#ifdef MAMI_ESL_MODE
static struct soc_addr ascend_v2_reg_list_die0[] = {
	{MAMI_TP_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_TP_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_TA_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_TA_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MISC_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MISC_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_NL0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL0_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL1_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL2_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL3_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL4_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_BA0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA0_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA1_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA2_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA3_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA4_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_UMMU_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_UMMU_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_MSGQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MSGQ_OFFSET,  SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MGTQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MGTQ_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_SLLC0_REG,             UDEV_DIE0_SLLC0_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_SLLC1_REG,             UDEV_DIE0_SLLC1_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_SLLC2_REG,             UDEV_DIE0_SLLC2_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA0_REG,               UDEV_DIE0_AA0_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA1_REG,               UDEV_DIE0_AA1_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA2_REG,               UDEV_DIE0_AA2_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA3_REG,               UDEV_DIE0_AA3_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA4_REG,               UDEV_DIE0_AA4_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_BROADCAST_HA_REG,      UDEV_DIE0_BROADCAST_HA_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_BROADCAST_ACCESS_REG,  UDEV_DIE0_BROADCAST_ACCESS_ASCEND_V2_BASE,            SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_ACCESS_AGENT_REG,      UDEV_DIE0_ACCESS_AGENT_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_L3T0_REG,              UDEV_DIE0_L3T0_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_L3T1_REG,              UDEV_DIE0_L3T1_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_HA_TOP0_REG,           UDEV_DIE0_HA_TOP0_ASCEND_V2_BASE,                     SIZE_MB(2),    HW_PERF_SENSITIVE},
	{MAMI_ACCESS_TOP0_REG,       UDEV_DIE0_ACCESS_TOP0_ASCEND_V2_BASE,                 SIZE_KB(2240), HW_PERF_SENSITIVE},
};
static struct soc_addr ascend_v2_reg_list_die1[] = {
	{MAMI_TP_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_TP_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_TA_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_TA_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MISC_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MISC_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_NL0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL0_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL1_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL2_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL3_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL4_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_BA0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA0_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA1_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA2_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA3_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA4_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_UMMU_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_UMMU_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_MSGQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MSGQ_OFFSET,  SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MGTQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MGTQ_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_SLLC0_REG,             UDEV_DIE1_SLLC0_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_SLLC1_REG,             UDEV_DIE1_SLLC1_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_SLLC2_REG,             UDEV_DIE1_SLLC2_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA0_REG,               UDEV_DIE1_AA0_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA1_REG,               UDEV_DIE1_AA1_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA2_REG,               UDEV_DIE1_AA2_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA3_REG,               UDEV_DIE1_AA3_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_AA4_REG,               UDEV_DIE1_AA4_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_BROADCAST_HA_REG,      UDEV_DIE1_BROADCAST_HA_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_BROADCAST_ACCESS_REG,  UDEV_DIE1_BROADCAST_ACCESS_ASCEND_V2_BASE,            SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_ACCESS_AGENT_REG,      UDEV_DIE1_ACCESS_AGENT_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_L3T0_REG,              UDEV_DIE1_L3T0_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_L3T1_REG,              UDEV_DIE1_L3T1_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_SENSITIVE},
	{MAMI_HA_TOP0_REG,           UDEV_DIE1_HA_TOP0_ASCEND_V2_BASE,                     SIZE_MB(2),    HW_PERF_SENSITIVE},
	{MAMI_ACCESS_TOP0_REG,       UDEV_DIE1_ACCESS_TOP0_ASCEND_V2_BASE,                 SIZE_KB(2240), HW_PERF_SENSITIVE},
};
#else
static struct soc_addr ascend_v2_reg_list_die0[] = {
	{MAMI_TP_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_TP_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_TA_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_TA_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MISC_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MISC_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_NL0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL0_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL1_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL2_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL3_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_NL4_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_BA0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA0_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA1_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA2_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA3_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_BA4_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_UMMU_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_UMMU_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_MSGQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MSGQ_OFFSET,  SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MGTQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE0_NBAR_MGTQ_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_SLLC0_REG,             UDEV_DIE0_SLLC0_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_SLLC1_REG,             UDEV_DIE0_SLLC1_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_SLLC2_REG,             UDEV_DIE0_SLLC2_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA0_REG,               UDEV_DIE0_AA0_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA1_REG,               UDEV_DIE0_AA1_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA2_REG,               UDEV_DIE0_AA2_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA3_REG,               UDEV_DIE0_AA3_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA4_REG,               UDEV_DIE0_AA4_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_BROADCAST_HA_REG,      UDEV_DIE0_BROADCAST_HA_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_BROADCAST_ACCESS_REG,  UDEV_DIE0_BROADCAST_ACCESS_ASCEND_V2_BASE,            SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_ACCESS_AGENT_REG,      UDEV_DIE0_ACCESS_AGENT_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_L3T0_REG,              UDEV_DIE0_L3T0_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_L3T1_REG,              UDEV_DIE0_L3T1_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_HA_TOP0_REG,           UDEV_DIE0_HA_TOP0_ASCEND_V2_BASE,                     SIZE_MB(2),    HW_PERF_INSENSITIVE},
	{MAMI_ACCESS_TOP0_REG,       UDEV_DIE0_ACCESS_TOP0_ASCEND_V2_BASE,                 SIZE_KB(2240), HW_PERF_INSENSITIVE},
};
static struct soc_addr ascend_v2_reg_list_die1[] = {
	{MAMI_TP_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_TP_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_TA_REG,                UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_TA_OFFSET,    SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MISC_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MISC_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_NL0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL0_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL1_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL2_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL3_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_NL4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_NL4_OFFSET,   SIZE_MB(8),    HW_PERF_SENSITIVE},
	{MAMI_BA0_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA0_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA1_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA1_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA2_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA2_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA3_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA3_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_BA4_REG,               UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_BA4_OFFSET,   SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_UMMU_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_UMMU_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_MSGQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MSGQ_OFFSET,  SIZE_MB(4),    HW_PERF_SENSITIVE},
	{MAMI_MGTQ_REG,              UDEV_NBAR_ASCEND_V2_FM + UDEV_DIE1_NBAR_MGTQ_OFFSET,  SIZE_MB(1),    HW_PERF_SENSITIVE},
	{MAMI_SLLC0_REG,             UDEV_DIE1_SLLC0_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_SLLC1_REG,             UDEV_DIE1_SLLC1_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_SLLC2_REG,             UDEV_DIE1_SLLC2_ASCEND_V2_BASE,                       SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA0_REG,               UDEV_DIE1_AA0_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA1_REG,               UDEV_DIE1_AA1_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA2_REG,               UDEV_DIE1_AA2_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA3_REG,               UDEV_DIE1_AA3_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_AA4_REG,               UDEV_DIE1_AA4_ASCEND_V2_BASE,                         SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_BROADCAST_HA_REG,      UDEV_DIE1_BROADCAST_HA_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_BROADCAST_ACCESS_REG,  UDEV_DIE1_BROADCAST_ACCESS_ASCEND_V2_BASE,            SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_ACCESS_AGENT_REG,      UDEV_DIE1_ACCESS_AGENT_ASCEND_V2_BASE,                SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_L3T0_REG,              UDEV_DIE1_L3T0_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_L3T1_REG,              UDEV_DIE1_L3T1_ASCEND_V2_BASE,                        SIZE_KB(64),   HW_PERF_INSENSITIVE},
	{MAMI_HA_TOP0_REG,           UDEV_DIE1_HA_TOP0_ASCEND_V2_BASE,                     SIZE_MB(2),    HW_PERF_INSENSITIVE},
	{MAMI_ACCESS_TOP0_REG,       UDEV_DIE1_ACCESS_TOP0_ASCEND_V2_BASE,                 SIZE_KB(2240), HW_PERF_INSENSITIVE},
};
#endif

static struct init_win_axi ascend_v2_init_win_axi[] = {
	{ 14, 0x2020 },
	{ 15, 0x2820 },
};

static struct soc_key_value ascend_v2_key_value_list[] = {
	{MAMI_NL_NUM, UDIE_NL_NUM},
	{MAMI_UDIE_VER, UDIE_VERSION_V2}
};

#ifdef FPGA_MODE_ENABLE
static struct udev_hsec_mem_info hsec_ascend_v2_mem_list[] = HSEC_MEM_LIST;
#endif

int udev_add_ascend_v2_info(void)
{
	static struct udev_hw_info hw_info = {0};

	hw_info.valid = HW_VALID;
	hw_info.chip_type = MAMI_KERN_TYPE_ASCEND_V2;

	hw_info.mem_die_bitmap = (0x1 << UDEV_DIE0);
	hw_info.reg_die_bitmap = ((0x1 << UDEV_DIE0) | ((0x1 << UDEV_DIE1)));

	hw_info.addr_mem[UDEV_DIE0].addr_list = ascend_v1_mem_list_die0;
	hw_info.addr_mem[UDEV_DIE0].addr_num = ARRAY_SIZE(ascend_v1_mem_list_die0);
	hw_info.addr_mem[UDEV_DIE1].addr_list = NULL;
	hw_info.addr_mem[UDEV_DIE1].addr_num = 0;
	hw_info.addr_reg[UDEV_DIE0].addr_list = ascend_v2_reg_list_die0;
	hw_info.addr_reg[UDEV_DIE0].addr_num = ARRAY_SIZE(ascend_v2_reg_list_die0);
	hw_info.addr_reg[UDEV_DIE1].addr_list = ascend_v2_reg_list_die1;
	hw_info.addr_reg[UDEV_DIE1].addr_num = ARRAY_SIZE(ascend_v2_reg_list_die1);

	hw_info.key_value_list = ascend_v2_key_value_list;
	hw_info.key_value_num = ARRAY_SIZE(ascend_v2_key_value_list);
#ifdef FPGA_MODE_ENABLE
	hw_info.hsec_mem_list = hsec_ascend_v2_mem_list;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_ascend_v2_mem_list);
	hw_info("hsec memory initialized without MDP.\n");
#else
	hw_info.hsec_mem_list = hsec_mem_list_with_mdp;
	hw_info.hsec_mem_num = ARRAY_SIZE(hsec_mem_list_with_mdp);
#endif
	hw_info.init_win_axi = ascend_v2_init_win_axi;
	hw_info.init_win_axi_num = ARRAY_SIZE(ascend_v2_init_win_axi);
	return udev_add_hw_info(MAMI_KERN_TYPE_ASCEND_V2, &hw_info);
}
DECLARE_FEATURE_AUTO_INIT(udev_add_ascend_v2_info, FEATURE_LOADER_STAGE_5);
