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
#ifndef UDEV_MEM_H
#define UDEV_MEM_H

#include <linux/types.h>
#include "uop.h"

#define HW_PERF_SENSITIVE 0
#define HW_PERF_INSENSITIVE 1

struct udev_soc_addr {
	const char *name;
	u64 phy_addr;
	u32 len;
	u32 perf_mark;
};

static inline void udev_soc_addr_pack(struct udev_soc_addr *soc_addr, const char *name, u64 addr, u32 size, u32 perf_mark)
{
	soc_addr->name = name;
	soc_addr->phy_addr = addr;
	soc_addr->len = size;
	soc_addr->perf_mark = perf_mark;
}

#define UOP_NOT_MAP			 0
#define UOP_NEED_MAP			1

int udev_mem_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem, u32 is_uop_map);
int udev_bbox_mem_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem, u32 is_uop_map);
#endif /* end of HW_MEM_H */
