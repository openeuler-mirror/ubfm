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
#ifndef _UDEV_DEV_1825_DEF_H_
#define _UDEV_DEV_1825_DEF_H_
#include "udev_udie_version_v1.h"

#define UDIE_1825NL_NUM		 3

#define UDEV_NBAR_FM			0xD0000000

#define UDEV_NBAR_BASE_OFFSET (UDEV_NBAR_FM + NBAR_BASE_OFFSET)
#define UDEV_NBAR_MISC_OFFSET (UDEV_NBAR_FM + NBAR_MISC_OFFSET)
#define UDEV_NBAR_NL0_OFFSET  (UDEV_NBAR_FM + NBAR_NL0_OFFSET)
#define UDEV_NBAR_NL1_OFFSET  (UDEV_NBAR_FM + NBAR_NL1_OFFSET)
#define UDEV_NBAR_NL2_OFFSET  (UDEV_NBAR_FM + NBAR_NL2_OFFSET)
#define UDEV_NBAR_D2H_OFFSET  (UDEV_NBAR_FM + NBAR_D2H_OFFSET)

#endif
