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
#ifndef UOP_CORE_H
#define UOP_CORE_H

#include <linux/types.h>
#include "ubfabric.h"
#include "kmap_kern.h"

void uop_kern_dev_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv);

void uop_kern_mod_init(void);
void uop_kern_mod_uninit(void);

int uop_kern_register_ops(u32 devid, const struct uop_ops *ops, enum uop_acc_type type);
int uop_kern_add_item_with_op_dev(struct kmap_dev *map_dev, struct uop_item *item, enum uop_item_type type);
int uop_kern_get_items(struct kmap_dev *map_dev, enum uop_item_type type, struct uop_item *item, u32 num, u32 *real_num);

int uop_kern_add_acc_node(struct kmap_dev *map_dev);
int uop_kern_get_acc_node(struct kmap_dev *map_dev, struct uop_acc_node *acc_node, u32 num, u32 *real_num);

int uop_kern_get_idle_local_win(u32 devid, u32 *local_win_id_start, u32 *local_win_num, u32 *local_win_size);
int uop_kern_get_local_addr(u32 devid, u32 local_win_id, u64 *local_addr);
int uop_kern_cfg_win(u32 devid, u32 local_win_id, u64 aligned_addr, u32 *axi_id_out);
void uop_kern_clear_win(u32 devid, u32 local_win_id);

#endif
