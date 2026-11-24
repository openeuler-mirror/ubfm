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
#ifndef UCD_KV_H
#define UCD_KV_H

#include <linux/types.h>
#include <linux/spinlock_types.h>

#define KV_TABLE_BIT 8
#define KV_TABLE_SIZE (0x1 << KV_TABLE_BIT) /* 256 */

#define KV_VALUE_MAX_LEN 256

struct ucd_kv_node {
	struct hlist_node hnode;
	u32 key;
	int data_len;
	void *data;
};

struct ucd_kv_table {
	rwlock_t lock;
	struct hlist_head table[KV_TABLE_SIZE];
};

typedef ssize_t (*key_dump_handle)(void *node, char *buf, ssize_t *offset);

struct ucd_kv_table *ucd_kv_init(void);
int ucd_kv_get(struct ucd_kv_table *cb, u32 key, void *value, int len);
int ucd_kv_set(struct ucd_kv_table *cb, u32 key, void *value, int len);
int ucd_kv_del(struct ucd_kv_table *cb, u32 key);
void ucd_kv_uninit(struct ucd_kv_table *cb);

void ucd_kv_free(struct ucd_kv_table *cb);
#endif
