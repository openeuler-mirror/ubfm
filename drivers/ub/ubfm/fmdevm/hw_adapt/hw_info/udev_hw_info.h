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
#ifndef _UDEV_HW_INFO_H_
#define _UDEV_HW_INFO_H_

#include <linux/bitmap.h>
#include <linux/interrupt.h>
#include "udev_hw_pub.h"
#include "ubfm_common.h"
#include "ures.h"

#define HW_INVALID 0
#define HW_VALID 1

struct soc_addr {
	const char *name;
	u64 phy_addr;
	size_t len;
	u32 perf_mark;
};

#define SOC_ADDR_ITEM(name, addr, len)  \
	.name = (name), .phy_addr = (addr), .len = (len)

struct soc_addr_info {
	u32 addr_num;
	struct soc_addr *addr_list;
};

struct soc_key_value {
	const char *key_name;
	u64 value;
};

struct init_win_axi {
	u32 axi_id;
	u32 axi_val;
};

struct udev_hw_info {
	u32 valid;
	u32 chip_type;
	unsigned long mem_die_bitmap;
	struct soc_addr_info addr_mem[DEV_DIE_MAX_NUM];
	unsigned long reg_die_bitmap;
	struct soc_addr_info addr_reg[DEV_DIE_MAX_NUM];
	u32 key_value_num;
	struct soc_key_value *key_value_list;
	u32 hsec_mem_num;
	struct udev_hsec_mem_info *hsec_mem_list;
	u32 init_win_axi_num;
	struct init_win_axi *init_win_axi;
};

enum dieID {
	UDEV_DIE0 = 0,
	UDEV_DIE1 = 1,
	UDEV_DIE2,
	UDEV_DIE3,
	UDEV_DIE4,
	UDEV_DIE5,
	UDEV_DIE6,
	UDEV_DIE7
};

int udev_add_hw_info(u32 chip_type, struct udev_hw_info *info);

struct udev_hsec_mem_info {
	const char *base_name;
	const char *entry_name;
	unsigned int entry_num;
	unsigned int entry_size;
};

#define UDEV_HSEC_MEM_ALIGN_SIZE 0x1000 /* 4KB */

#endif
