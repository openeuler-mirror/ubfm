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

#ifndef __UBFM_DFX_FS_H__
#define __UBFM_DFX_FS_H__

#include <linux/kernel.h>

/** Owner read-only (used for proc/sysfs read-only nodes) */
#define UBFM_FS_MODE_OWNER_RO  0400

/** Owner and group read-only (used for proc/sysfs owner+group read-only nodes) */
#define UBFM_FS_MODE_OG_RO     0440

/** All read-only (used for proc/sysfs global read-only nodes) */
#define UBFM_FS_MODE_ALL_RO    0444

/** Owner read-write (used for proc/sysfs read-write nodes) */
#define UBFM_FS_MODE_OWNER_RW  0600

#define SIMPLE_STRTOUL_SAFE(p, v, base)	\
	do { \
		if (*(p) == '\0') { \
			(v) = 0; \
		} else { \
			(v) = (typeof(v))simple_strtoul((p), (char **)&(p), (base)); \
			(p) = (*(p)) == '\0' ? (p) : (p) + 1; \
		} \
	} while (0) \

#endif /* __UBFM_DFX_FS_H__ */
