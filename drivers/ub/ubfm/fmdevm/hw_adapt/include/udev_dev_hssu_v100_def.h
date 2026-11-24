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
#ifndef _UDEV_DEV_HSSU_V100_DEF_H_
#define _UDEV_DEV_HSSU_V100_DEF_H_
#include "udev_udie_version_v1.h"

#define UDIE_HSSU_V100_NL_NUM		 1

#define UDEV_NBAR_FM			0x200000000
#define UDEV_CUSTOM_MEM		 0x2200FC000

#define UDEV_NBAR_BASE_OFFSET (UDEV_NBAR_FM + NBAR_BASE_OFFSET)
#define UDEV_NBAR_MISC_OFFSET (UDEV_NBAR_FM + NBAR_MISC_OFFSET)
#define UDEV_NBAR_TP_OFFSET   (UDEV_NBAR_FM + NBAR_TP_OFFSET)
#define UDEV_NBAR_TA_OFFSET   (UDEV_NBAR_FM + NBAR_TA_OFFSET)
#define UDEV_NBAR_BA0_OFFSET  (UDEV_NBAR_FM + NBAR_BA0_OFFSET)
#define UDEV_NBAR_NL0_OFFSET  (UDEV_NBAR_FM + NBAR_NL0_OFFSET)
#define UDEV_NBAR_UMMU_OFFSET (UDEV_NBAR_FM + NBAR_UMMU_OFFSET)
#define UDEV_NBAR_MSGQ_OFFSET (UDEV_NBAR_FM + NBAR_MSGQ_OFFSET)
#define UDEV_NBAR_MGTQ_OFFSET (UDEV_NBAR_FM + NBAR_MGTQ_OFFSET)
#endif
