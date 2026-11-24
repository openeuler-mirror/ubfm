// SPDX-License-Identifier: GPL-2.0-only
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
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>

#include "ucd_define.h"
#include "ucd_kv.h"
#include "ucd_feature.h"
#include "mami_string.h"

static inline void free_kv(struct ucd_kv_node *kv)
{
	if (kv != NULL) {
		kfree(kv->data);
		kv->data = NULL;

		kfree(kv);
	}
}

static inline u32 ucd_kv_hash_33(u32 tag)
{
	u32 hash, i;
	char *p = (char *)&tag;

	for (i = 0, hash = 0; i < sizeof(int); i++) {
		hash = (hash << 5) + hash + *p++; /* 5 hash left offset */
	}
	return hash;
}

static inline struct hlist_head *ucd_kv_get_table_head(struct ucd_kv_table *cb, u32 key)
{
	return &cb->table[ucd_kv_hash_33(key) % KV_TABLE_SIZE];
}

static struct ucd_kv_node *ucd_kv_new_node(u32 key, void *value, int len)
{
	struct ucd_kv_node *node = NULL;
	int ret;

	node = kzalloc(sizeof(struct ucd_kv_node), GFP_ATOMIC | __GFP_ACCOUNT);
	if (node == NULL) {
		return NULL;
	}
	node->key = key;
	node->data = kzalloc(len, GFP_ATOMIC | __GFP_ACCOUNT);
	if (node->data == NULL) {
		goto free_node;
	}
	ret = ubfm_memcpy(node->data, len, value, len);
	if (ret != 0) {
		goto free_data;
	}
	node->data_len = len;
	return node;
free_data:
	kfree(node->data);
	node->data = NULL;
free_node:
	kfree(node);
	node = NULL;
	return NULL;
}

static struct ucd_kv_node *ucd_kv_search(struct ucd_kv_table *cb, u32 key)
{
	struct hlist_head *table_head = NULL;
	struct ucd_kv_node *node = NULL;
	struct hlist_node *tmp = NULL;

	table_head = ucd_kv_get_table_head(cb, key);
	hlist_for_each_entry_safe(node, tmp, table_head, hnode) {
		if (node->key == key) {
			break;
		}
	}
	return node;
}

static int ucd_kv_add(struct ucd_kv_table *cb, u32 key, void *value, int len)
{
	struct hlist_head *table_head = NULL;
	struct ucd_kv_node *node = NULL;

	node = ucd_kv_new_node(key, value, len);
	if (node == NULL) {
		return -ENOMEM;
	}
	table_head = ucd_kv_get_table_head(cb, key);
	hlist_add_head(&node->hnode, table_head);
	return 0;
}

int ucd_kv_del(struct ucd_kv_table *cb, u32 key)
{
	struct ucd_kv_node *node = NULL;

	if (cb == NULL) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}
	write_lock_bh(&cb->lock);
	node = ucd_kv_search(cb, key);
	if (node != NULL) {
		hlist_del(&node->hnode);
		write_unlock_bh(&cb->lock);
		free_kv(node);
		return 0;
	}
	write_unlock_bh(&cb->lock);
	ucd_err("No such device node. (tag=%08x)\n", key);
	return -ENODEV;
}
int ucd_kv_set(struct ucd_kv_table *cb, u32 key, void *value, int len)
{
	struct hlist_head *table_head = NULL;
	struct ucd_kv_node *node = NULL;
	struct hlist_node *tmp = NULL;
	int ret;

	if (((cb) == NULL) || ((value) == NULL)
		|| ((len) > KV_VALUE_MAX_LEN) || ((len) <= 0)) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}

	write_lock_bh(&cb->lock);
	table_head = ucd_kv_get_table_head(cb, key);
	hlist_for_each_entry_safe(node, tmp, table_head, hnode) {
		if (node->key == key) {
			write_unlock_bh(&cb->lock);
			ucd_err("Repeated registration of the same node, (tag=%08x)\n", key);
			return -EINVAL;
		}
	}
	ret = ucd_kv_add(cb, key, value, len);
	write_unlock_bh(&cb->lock);
	return ret;
}

int ucd_kv_get(struct ucd_kv_table *cb, u32 key, void *value, int len)
{
	struct ucd_kv_node *node = NULL;
	int ret;

	if (((cb) == NULL) || ((value) == NULL)
		|| ((len) > KV_VALUE_MAX_LEN) || ((len) <= 0)) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}

	write_lock_bh(&cb->lock);
	node = ucd_kv_search(cb, key);
	if (node == NULL) {
		write_unlock_bh(&cb->lock);
		ucd_debug("No such device node. (tag=%08x)\n", key);
		return -ENODEV;
	}

	ret = ubfm_memcpy(value, len, node->data, node->data_len);
	if (ret != 0) {
		write_unlock_bh(&cb->lock);
		ucd_err("Failed to memcpy node data. (tag=%08x; ret=%d)\n", key, ret);
		return -EINVAL;
	}

	ret = feature_inc_work(*(struct tag_ucd_feature_node **)value);
	if (ret != 0) {
		write_unlock_bh(&cb->lock);
		return ret;
	}

	write_unlock_bh(&cb->lock);
	return ret;
}

void ucd_kv_dump(struct ucd_kv_table *cb, char *buf, ssize_t *offset, key_dump_handle handle)
{
	struct hlist_head *table_head = NULL;
	struct hlist_node *tmp = NULL;
	struct ucd_kv_node *node = NULL;
	int i;
	ssize_t ret;

	if (cb == NULL) {
		ucd_err("Invalid Parameter.\n");
		return;
	}
	read_lock_bh(&cb->lock);
	for (i = 0; i < KV_TABLE_SIZE; i++) {
		table_head = &cb->table[i];
		hlist_for_each_entry_safe(node, tmp, table_head, hnode) {
			ret = handle(node->data, buf, offset);
			if (ret < 0) {
				read_unlock_bh(&cb->lock);
				return;
			}
		}
	}
	read_unlock_bh(&cb->lock);
}

struct ucd_kv_table *ucd_kv_init(void)
{
	struct ucd_kv_table *p;
	int i;

	p = kzalloc(sizeof(struct ucd_kv_table), GFP_KERNEL | __GFP_ACCOUNT);
	if (p == NULL) {
		ucd_err("kzalloc failed. (size=%lu)\n", sizeof(struct ucd_kv_table));
		return NULL;
	}
	for (i = 0; i < KV_TABLE_SIZE; i++) {
		INIT_HLIST_HEAD(&p->table[i]);
	}
	rwlock_init(&p->lock);
	return p;
}

void ucd_kv_free(struct ucd_kv_table *cb)
{
	struct hlist_head *table_head = NULL;
	struct ucd_kv_node *node = NULL;
	struct hlist_node *tmp = NULL;
	int i;

	if (cb == NULL) {
		ucd_err("Invalid Parameter.\n");
		return;
	}
	write_lock_bh(&cb->lock);
	for (i = 0; i < KV_TABLE_SIZE; i++) {
		table_head = &cb->table[i];
		hlist_for_each_entry_safe(node, tmp, table_head, hnode) {
			hlist_del_init(&node->hnode);
			free_kv(node);
			node = NULL;
		}
	}
	write_unlock_bh(&cb->lock);
	kfree(cb);
	cb = NULL;
}
