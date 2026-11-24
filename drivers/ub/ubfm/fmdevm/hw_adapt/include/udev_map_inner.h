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
#ifndef _UDEV_MAP_INNER_H_
#define _UDEV_MAP_INNER_H_

#include "ubfm_common.h"

struct addr_seg {
	u64 hpa_start; /* Host Physical Address */
	u64 gpa_start; /* Guest Physical Address */
	u32 size;
	u32 is_share;
};

typedef int (*addr_map_func)(u64, u32, struct addr_seg **, u32 *);

int udev_register_map_func(addr_map_func func);
int udev_get_gpa_list(u64 hpa, u32 size, struct addr_seg **segs, u32 *cnt);
void put_gpa_list(struct addr_seg *segs);

#endif
