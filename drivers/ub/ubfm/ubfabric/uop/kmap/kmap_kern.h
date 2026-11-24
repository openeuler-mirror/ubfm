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
#ifndef KMAP_KERN_H
#define KMAP_KERN_H

#include <linux/types.h>

#include "ures.h"
#include "ubfabric.h"

struct kmap_addr {
	u32 die_id;
	char name[URES_NAME_LEN];
	u64 acc_addr;
	u64 local_asic_addr;
	u64 asic_addr;
	size_t size;
};

struct kmap_bare_addr {
	u32 die_id;
	char name[URES_NAME_LEN];
	u64 asic_addr;
	size_t size;
};

struct kmap_acc_node {
	u64 acc_addr;
	u64 local_asic_addr;
	u32 local_win_id;
	u32 win_size;
};

struct kmap_dev {
	u32 devid;
	u32 die_num;

	enum uop_acc_type acc_type;

	u32 mem_num;
	struct kmap_addr *mem;

	u32 reg_num;
	struct kmap_addr *reg;

	u32 bare_item_num;
	struct kmap_bare_addr *bare_item;

	u32 acc_node_num;
	struct kmap_acc_node *acc_node;
};

typedef int (*kmap_mem_map_t)(u32 devid, struct kmap_addr *mem);
typedef void (*kmap_mem_unmap_t)(u32 devid, struct kmap_addr *mem);
typedef int (*kmap_reg_map_t)(u32 devid, struct kmap_addr *reg);
typedef void (*kmap_reg_unmap_t)(u32 devid, struct kmap_addr *reg);

typedef int (*kmap_acc_node_map_t)(u32 devid, struct kmap_acc_node *acc_node, u32 acc_node_num);
typedef void (*kmap_acc_node_unmap_t)(u32 devid, struct kmap_addr *node);

struct kmap_ops {
	kmap_mem_map_t mem_map;
	kmap_mem_unmap_t mem_unmap;
	kmap_reg_map_t reg_map;
	kmap_reg_unmap_t reg_unmap;
	kmap_acc_node_map_t acc_node_map;
	kmap_acc_node_unmap_t acc_node_unmap;
};

int kmap_dev_init(u32 devid, enum uop_acc_type type, struct kmap_dev *map_dev);
void kmap_dev_uninit(struct kmap_dev *map_dev);

int kmap_ops_register(enum uop_acc_type type, const struct kmap_ops *ops);
void kmap_ops_unregister(enum uop_acc_type type);
const struct kmap_ops *kmap_get_ops(enum uop_acc_type type);

#endif
