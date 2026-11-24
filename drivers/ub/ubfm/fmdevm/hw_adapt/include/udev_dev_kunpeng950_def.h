/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef _UDEV_DEV_KUNPENG950_DEF_H_
#define _UDEV_DEV_KUNPENG950_DEF_H_
#include "udev_udie_version_v1.h"

#define UDEV_NBAR_FM			0x3300000000
#define UDEV_SFC_HISS_BASE	  0x3004000000
/* kunpeng950 has only one udie, but has 4 chip max */
#define UDEV_CHIP_OFFSET		0x4000000000

/* kunpeng960 has 4 chips, each with 2 dies. */
#define UDEV_DIE_OFFSET			0x1000000000

#define MAMI_SRAM_DIE0_BASE(chip_id)	 (0x3020000000 + (UDEV_CHIP_OFFSET * (chip_id)))

#define UDEV_CUSTOM_MEM		 0x30200FC000
#define KUNPENG_960_UDEV_CUSTOM_MEM	  0x9D31000

/* CHIP chip_id=0, 1, 2, 3, ... */
#define UDEV_NBAR_BASE_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BASE_OFFSET)
#define UDEV_NBAR_MISC_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_MISC_OFFSET)
#define UDEV_NBAR_TP_OFFSET(chip_id, die_id)	 ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_TP_OFFSET)
#define UDEV_NBAR_BA0_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BA0_OFFSET)
#define UDEV_NBAR_TA_OFFSET(chip_id, die_id)	 ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_TA_OFFSET)
#define UDEV_NBAR_NL0_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_NL0_OFFSET)
#define UDEV_NBAR_BA1_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BA1_OFFSET)
#define UDEV_NBAR_NL1_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_NL1_OFFSET)
#define UDEV_NBAR_BA2_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BA2_OFFSET)
#define UDEV_NBAR_NL2_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_NL2_OFFSET)
#define UDEV_NBAR_BA3_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BA3_OFFSET)
#define UDEV_NBAR_NL3_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_NL3_OFFSET)
#define UDEV_NBAR_BA4_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_BA4_OFFSET)
#define UDEV_NBAR_NL4_OFFSET(chip_id, die_id)	((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_NL4_OFFSET)
#define UDEV_NBAR_UMMU_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_UMMU_OFFSET)
#define UDEV_NBAR_CCUM_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_CCUM_OFFSET)
#define UDEV_NBAR_MSGQ_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_MSGQ_OFFSET)
#define UDEV_NBAR_MGTQ_OFFSET(chip_id, die_id)   ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + NBAR_MGTQ_OFFSET)
#define UDEV_UERES2_OFFSET(chip_id, die_id)	  ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + UDEV_UERES2_BASE)
#define UDEV_CTRLQ_REG_OFFSET(chip_id, die_id)   (UDEV_UERES2_OFFSET(chip_id, die_id) + UDEV_UERES2_CTRLQ_OFFSET)

#define UDEV_SFC_HISS0_OFFSET (UDEV_SFC_HISS_BASE + SFC_HISS0_OFFSET)
#define UDEV_SFC_HISS1_OFFSET (UDEV_SFC_HISS_BASE + SFC_HISS1_OFFSET)
#define UDEV_SFC_HISS2_OFFSET (UDEV_SFC_HISS_BASE + SFC_HISS2_OFFSET)
#define UDEV_SFC_HISS3_OFFSET (UDEV_SFC_HISS_BASE + SFC_HISS3_OFFSET)

/* kunpeng960 */
#define UDEV_NBAR_KUNPENG_960_FM			 0x2f00000000
#define UDEV_UPA0_OFFSET			0x1240000
#define UDEV_UPA1_OFFSET			0x1280000
#define UDEV_UAMU_OFFSET			0x12c0000

#define UDEV_UPA0_KUNPENG_960_BASE(chip_id, die_id)  ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + UDEV_NBAR_KUNPENG_960_FM + UDEV_UPA0_OFFSET)
#define UDEV_UPA1_KUNPENG_960_BASE(chip_id, die_id)  ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + UDEV_NBAR_KUNPENG_960_FM + UDEV_UPA1_OFFSET)
#define UDEV_UAMU_KUNPENG_960_BASE(chip_id, die_id)  ((UDEV_CHIP_OFFSET * (chip_id)) + (UDEV_DIE_OFFSET * (die_id)) + UDEV_NBAR_KUNPENG_960_FM + UDEV_UAMU_OFFSET)
#endif
