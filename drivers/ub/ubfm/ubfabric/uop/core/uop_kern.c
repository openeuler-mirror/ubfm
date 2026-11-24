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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/compiler.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rwsem.h>
#include <linux/kernel.h>
#include <linux/sort.h>
#include <linux/atomic.h>
#include <linux/preempt.h>
#include <linux/jiffies.h>
#include <stdarg.h>

#include "mami_string.h"
#include "kref_safe.h"
#include "ubfabric.h"
#include "uop.h"
#include "udm.h"
#include "ures.h"
#include "fmdevm.h"

#include "uop_log.h"
#include "uop_kern.h"
#include "ubfm_common.h"
#include "i3c_kern.h"

struct uop_items {
	enum uop_item_type type;
	struct rw_semaphore sem;
	atomic_t index;		 /* Cached item */
	u32 num;				/* Item num */
	struct uop_item item[UOP_MAX_ITEM];
};

struct uop_acc_nodes {
	u32 bare_item_num;
	struct uop_bare_item bare_items[UOP_MAX_BARE_ITEM];
	u32 acc_node_num;
	struct uop_acc_node nodes[UOP_MAX_ACC_NODE];
	u32 last_acc_idx;
	u32 acc_share_win_mode; /* ubx share 4m; 121 inline share 256m */
	struct rw_semaphore sem;
};

struct uop_dev {
	u32 devid;
	struct uop_ops ops;
	struct uop_extend_ops ext_ops;
	struct kref_safe ref;
	struct kmap_dev map_dev;

	struct uop_items items[UOP_ITEM_TYPE_MAX];
	struct uop_acc_nodes *acc_nodes;
};

static struct uop_dev *g_uop_dev[UOP_MAX_DEVNUM];
static rwlock_t g_dev_lock[UOP_MAX_DEVNUM];
static struct uop_win_ops *g_uop_win_ops;

void uop_kern_register_win_ops(struct uop_win_ops *ops)
{
	g_uop_win_ops = ops;
}
EXPORT_SYMBOL_GPL(uop_kern_register_win_ops);

void uop_kern_unregister_win_ops(void)
{
	g_uop_win_ops = NULL;
}
EXPORT_SYMBOL_GPL(uop_kern_unregister_win_ops);

int uop_kern_get_idle_local_win(u32 devid, u32 *local_win_id_start, u32 *local_win_num, u32 *local_win_size)
{
	if (g_uop_win_ops == NULL || g_uop_win_ops->get_idle_local_win == NULL) {
		uop_err("Uop win ops NULL\n");
		return -EINVAL;
	}
	return g_uop_win_ops->get_idle_local_win(devid, local_win_id_start, local_win_num, local_win_size);
}

int uop_kern_get_local_addr(u32 devid, u32 local_win_id, u64 *local_addr)
{
	if (g_uop_win_ops == NULL || g_uop_win_ops->get_local_addr == NULL) {
		uop_err("Uop win ops NULL\n");
		return -EINVAL;
	}
	return g_uop_win_ops->get_local_addr(devid, local_win_id, local_addr);
}

int uop_kern_cfg_win(u32 devid, u32 local_win_id, u64 aligned_addr, u32 *axi_id_out)
{
	if (g_uop_win_ops == NULL || g_uop_win_ops->cfg_win == NULL) {
		uop_err("Uop win ops NULL\n");
		return -EINVAL;
	}
	return g_uop_win_ops->cfg_win(devid, local_win_id, aligned_addr, axi_id_out);
}

void uop_kern_clear_win(u32 devid, u32 local_win_id)
{
	if (g_uop_win_ops == NULL || g_uop_win_ops->clear_win == NULL) {
		uop_err("Uop win ops NULL\n");
		return;
	}
	g_uop_win_ops->clear_win(devid, local_win_id);
}

/* Re-query idle local win and set USER share_local_win, avoiding collision
 * with ctrlq windows that were allocated after uop's PRI1 init.
 */
int uop_kern_update_share_local_win(u32 devid)
{
	struct ures_win_info win_info = {0};
	u32 win_id_start = 0;
	u32 win_num = 0;
	u32 win_size = 0;
	int ret;

	ret = uop_kern_get_idle_local_win(devid, &win_id_start, &win_num, &win_size);
	if (unlikely(ret != 0)) {
		uop_err("Get idle local win fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (unlikely(win_num == 0)) {
		uop_err("Idle local win not enough. (devid=%u; win_num=%u)\n", devid, win_num);
		return -ENOSPC;
	}

	win_info.win_id_start = win_id_start;
	win_info.win_num = win_num;
	win_info.win_size = win_size;

	ret = ures_set_share_local_win(devid, URES_WIN_TYPE_USER, &win_info);
	if (unlikely(ret != 0)) {
		uop_err("Set user share local win fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_update_share_local_win);

static inline int uop_dev_check(u32 devid)
{
	return (devid >= UOP_MAX_DEVNUM) ? -EINVAL : 0;
}

static int uop_item_check(struct uop_item *item, enum uop_item_type type)
{
	size_t len;

	if (unlikely(item == NULL)) {
		uop_err("Item is NULL\n");
		return -EINVAL;
	}

	len = strnlen(item->name, UOP_NAME_LEN);
	if (unlikely((len == 0) || (len >= UOP_NAME_LEN))) {
		uop_err("Invalid item name. (len=%lu)\n", len);
		return -EINVAL;
	}

	if (unlikely(item->asic_addr >= (item->asic_addr + (u64)item->len))) {
		uop_err("Invalid item len. (len=%u)\n", item->len);
		return -EINVAL;
	}

	if (unlikely(type >= UOP_ITEM_TYPE_MAX)) {
		uop_err("Invalid type. (type=%u)\n", type);
		return -EINVAL;
	}

	return 0;
}

static u32 uop_get_item_total_num(struct uop_dev *op_dev)
{
	enum uop_item_type type;
	u32 num = 0;

	for (type = UOP_ITEM_MEM; type  < UOP_ITEM_TYPE_MAX; type++) {
		struct uop_items *items = &op_dev->items[type];

		down_read(&items->sem);
		num += items->num;
		up_read(&items->sem);
	}
	return num;
}

static const char *uop_get_item_name(enum uop_item_type type)
{
	if (type == UOP_ITEM_MEM) {
		return "mem";
	} else if (type == UOP_ITEM_REG) {
		return "reg";
	}
	return "unknown";
}

static inline bool uop_item_range_valid(struct uop_item *item, u64 addr, u32 len)
{
	return ((item != NULL) &&
			(item->asic_addr != 0) &&
			(addr >= item->asic_addr) &&
			((addr + len) > addr) &&
			((addr + len) <= (item->asic_addr + item->len)));
}

static int uop_dev_add_inner(struct uop_dev *op_dev)
{
	if (unlikely(g_uop_dev[op_dev->devid] != NULL)) {
		return -EBUSY;
	}
	g_uop_dev[op_dev->devid] = op_dev;
	return 0;
}

static inline void uop_dev_del_inner(u32 devid)
{
	g_uop_dev[devid] = NULL;
}

static inline int uop_dev_add(struct uop_dev *op_dev)
{
	int ret;

	write_lock_bh(&g_dev_lock[op_dev->devid]);
	ret = uop_dev_add_inner(op_dev);
	write_unlock_bh(&g_dev_lock[op_dev->devid]);
	return ret;
}

static inline void uop_dev_del(u32 devid)
{
	write_lock_bh(&g_dev_lock[devid]);
	uop_dev_del_inner(devid);
	write_unlock_bh(&g_dev_lock[devid]);
}

static struct uop_dev *uop_dev_create(u32 devid, const struct uop_ops *ops)
{
	struct uop_dev *op_dev = NULL;
	enum uop_item_type type;

	op_dev = kvzalloc(sizeof(struct uop_dev), GFP_KERNEL_ACCOUNT);
	if (unlikely(op_dev == NULL)) {
		return NULL;
	}
	for (type = UOP_ITEM_MEM; type < UOP_ITEM_TYPE_MAX; type++) {
		struct uop_items *items = &op_dev->items[type];

		items->type = type;
		atomic_set(&items->index, 0);
		init_rwsem(&items->sem);
	}

	op_dev->devid = devid;
	op_dev->ops = *ops;
	kref_safe_init(&op_dev->ref);

	return op_dev;
}

static void uop_dev_destroy(struct uop_dev *op_dev)
{
	if (likely(op_dev != NULL)) {
		if (op_dev->acc_nodes != NULL) {
			kvfree(op_dev->acc_nodes);
			op_dev->acc_nodes = NULL;
		}
		kvfree(op_dev);
	}
}

static int uop_dev_init(struct uop_dev *op_dev)
{
	int ret = 0;

	if (op_dev->ops.init_dev != NULL) {
		ret = op_dev->ops.init_dev(op_dev->devid);
	}
	return ret;
}

static void uop_dev_uninit(struct uop_dev *op_dev)
{
	if (op_dev->ops.uninit_dev != NULL) {
		op_dev->ops.uninit_dev(op_dev->devid);
	}
}

static void uop_dev_release(struct kref_safe *kref_s)
{
	struct uop_dev *op_dev = container_of(kref_s, struct uop_dev, ref);

	kmap_dev_uninit(&op_dev->map_dev);
	uop_dev_uninit(op_dev);
	uop_dev_destroy(op_dev);
}

static struct uop_dev *uop_dev_get(u32 devid)
{
	struct uop_dev *op_dev = NULL;

	if (unlikely(devid >= UOP_MAX_DEVNUM)) {
		return NULL;
	}

	read_lock_bh(&g_dev_lock[devid]);
	op_dev = g_uop_dev[devid];
	if (likely(op_dev != NULL)) {
		if (kref_safe_get_unless_zero(&op_dev->ref) == 0) {
			read_unlock_bh(&g_dev_lock[devid]);
			return NULL;
		}
	}
	read_unlock_bh(&g_dev_lock[devid]);

	return op_dev;
}

static inline void uop_dev_put(struct uop_dev *op_dev)
{
	kref_safe_put(&op_dev->ref, uop_dev_release);
}

int uop_kern_register_ops(u32 devid, const struct uop_ops *ops, enum uop_acc_type type)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	ret = uop_dev_check(devid);
	if (unlikely(ret != 0)) {
		uop_err("Invalid devid. (devid=%u; max_devid=%u)\n", devid, (u32)UOP_MAX_DEVNUM);
		return ret;
	}

	if (unlikely(ops == NULL)) {
		uop_err("Ops is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	op_dev = uop_dev_create(devid, ops);
	if (unlikely(op_dev == NULL)) {
		uop_err("Uop dev create fail. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	ret = uop_dev_init(op_dev);
	if (unlikely(ret != 0)) {
		uop_dev_destroy(op_dev);
		uop_err("Uop dev init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = kmap_dev_init(devid, type, &op_dev->map_dev);
	if (unlikely(ret != 0)) {
		uop_dev_uninit(op_dev);
		uop_dev_destroy(op_dev);
		uop_err("Kmap dev init fail. (ret=%d; devid=%u; type=%u)\n", ret, devid, type);
		return ret;
	}

	ret = uop_dev_add(op_dev);
	if (unlikely(ret != 0)) {
		kmap_dev_uninit(&op_dev->map_dev);
		uop_dev_uninit(op_dev);
		uop_dev_destroy(op_dev);
		uop_err("Uop dev add fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	return ret;
}

int uop_kern_register_msg_ops(u32 devid, const uop_msg_send_recv_t ops)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	ret = uop_dev_check(devid);
	if (unlikely(ret != 0)) {
		uop_err("Invalid devid. (devid=%u; max_devid=%u)\n", devid, (u32)UOP_MAX_DEVNUM);
		return ret;
	}

	if (unlikely(ops == NULL)) {
		uop_err("Ops is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		uop_err("dev not create. (devid=%u)\n", devid);
		return -ENOMEM;
	}
	op_dev->ext_ops.send_recv_msg = ops;
	uop_dev_put(op_dev);
	uop_info("Uop msg ops reg success. (devid=%u)\n", devid);
	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_register_msg_ops);

int uop_kern_unregister_msg_ops(u32 devid)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	ret = uop_dev_check(devid);
	if (unlikely(ret != 0)) {
		uop_err("Invalid devid. (devid=%u; max_devid=%u)\n", devid, (u32)UOP_MAX_DEVNUM);
		return ret;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	if (op_dev->ext_ops.send_recv_msg == NULL) {
		uop_dev_put(op_dev);
		return 0;
	}
	op_dev->ext_ops.send_recv_msg = NULL;
	uop_dev_put(op_dev);
	uop_info("Uop msg ops unreg success. (devid=%u)\n", devid);
	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_unregister_msg_ops);

static int uop_item_cmp(const void *a, const void *b)
{
	const struct uop_item *item_a = (const struct uop_item *)a;
	const struct uop_item *item_b = (const struct uop_item *)b;

	if (item_a->asic_addr < item_b->asic_addr) {
		return -1;
	}

	if (item_a->asic_addr > item_b->asic_addr) {
		return 1;
	}

	return 0;
}

static inline void uop_item_sort(struct uop_items *items)
{
	sort(items->item, items->num, sizeof(struct uop_item), uop_item_cmp, NULL);
}

static int uop_item_append(struct uop_items *items, struct uop_item *item)
{
	struct uop_item *item_ = NULL;
	u32 index = items->num;

	if (unlikely(index >= UOP_MAX_ITEM)) {
		return -ENOSPC;
	}

	item_ = &items->item[index];
	/* Src size == Dest size, Don't mind return value */
	(void)ubfm_memcpy(item_, sizeof(struct uop_item), item, sizeof(struct uop_item));
	items->num++;

	return 0;
}

static struct uop_item *uop_find_item(struct uop_items *items, u64 addr, u32 len)
{
	int index = atomic_read(&items->index); /* Get cache index */
	int right = (int)items->num - 1;
	int left = 0;

	if ((addr + len) < addr) {
		return NULL;
	}

	while (left <= right) {
		struct uop_item *item = &items->item[index];

		if (addr >= (item->asic_addr + item->len)) {
			left = index + 1;
		} else if ((addr + len) <= item->asic_addr) {
			right = index - 1;
		} else {
			/* Item found, set to cache index */
			atomic_set(&items->index, index);
			return item;
		}
		index = (left + right) >> 1;
	}

	return NULL;
}

static int uop_add_item(struct uop_items *items, struct uop_item *item)
{
	struct uop_item *item_ = uop_find_item(items, item->asic_addr, item->len);

	if (unlikely(item_ != NULL)) {
		return -EEXIST;
	}
	return uop_item_append(items, item);
}

int uop_kern_add_item_with_op_dev(struct kmap_dev *map_dev, struct uop_item *item, enum uop_item_type type)
{
	struct uop_dev *op_dev = container_of(map_dev, struct uop_dev, map_dev);
	struct uop_items *items = NULL;
	int ret;

	ret = uop_item_check(item, type);
	if (unlikely(ret != 0)) {
		uop_err("Item check fail. (ret=%d; devid=%u)\n", ret, op_dev->devid);
		return ret;
	}

	items = &op_dev->items[type];
	down_write(&items->sem);
	ret = uop_add_item(items, item);
	if (likely(ret == 0)) {
		uop_item_sort(items);
	}
	up_write(&items->sem);

	if (unlikely(ret != 0)) {
		uop_err("Uop item add fail. (ret=%d; devid=%u; name=%s)\n", ret, op_dev->devid, item->name);
	}

	return ret;
}

STATIC int uop_kern_add_item(u32 devid, struct uop_item *item, enum uop_item_type type)
{
	struct uop_items *items = NULL;
	struct uop_dev *op_dev = NULL;
	int ret;

	ret = uop_dev_check(devid);
	if (unlikely(ret != 0)) {
		uop_err("Invalid devid. (ret=%d; devid=%u; max_devid=%u)\n", ret, devid, (u32)UOP_MAX_DEVNUM);
		return ret;
	}

	ret = uop_item_check(item, type);
	if (unlikely(ret != 0)) {
		uop_err("Item check fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		uop_err("Uop dev get fail. (devid=%u; name=%s)\n", devid, item->name);
		return -ENODEV;
	}
	items = &op_dev->items[type];
	down_write(&items->sem);
	ret = uop_add_item(items, item);
	if (likely(ret == 0)) {
		uop_item_sort(items);
	}
	up_write(&items->sem);
	uop_dev_put(op_dev);

	if (unlikely(ret != 0)) {
		uop_err("Uop item add fail. (ret=%d; devid=%u; name=%s)\n", ret, devid, item->name);
	}

	return ret;
}

int uop_kern_add_acc_node(struct kmap_dev *map_dev)
{
	struct uop_dev *op_dev = container_of(map_dev, struct uop_dev, map_dev);
	u32 i;

	if (unlikely(map_dev->bare_item_num >= UOP_MAX_BARE_ITEM)) {
		uop_err("Add acc node para invalid. (devid=%u; bare_item_num=%u; max=%u)\n",
			map_dev->devid, map_dev->bare_item_num, UOP_MAX_BARE_ITEM);
		return -EINVAL;
	}

	if (unlikely(map_dev->acc_node_num >= UOP_MAX_ACC_NODE)) {
		uop_err("Add acc node para invalid. (devid=%u; acc_node_num=%u; max=%u)\n",
			map_dev->devid, map_dev->acc_node_num, UOP_MAX_ACC_NODE);
		return -EINVAL;
	}

	op_dev->acc_nodes = kvzalloc(sizeof(struct uop_acc_nodes), GFP_KERNEL_ACCOUNT);
	if (unlikely(op_dev->acc_nodes == NULL)) {
		uop_err("Acc node alloc fail. (devid=%u; size=%zu)\n",
			map_dev->devid, sizeof(struct uop_acc_nodes));
		return -ENOMEM;
	}
	op_dev->acc_nodes->bare_item_num = map_dev->bare_item_num;
	op_dev->acc_nodes->acc_node_num = map_dev->acc_node_num;
	for (i = 0; i < map_dev->bare_item_num; i++) {
		(void)uop_bare_item_pack(&op_dev->acc_nodes->bare_items[i], map_dev->bare_item[i].name,
			map_dev->bare_item[i].asic_addr, map_dev->bare_item[i].size);
	}

	for (i = 0; i < map_dev->acc_node_num; i++) {
		if (unlikely(map_dev->acc_node[i].acc_addr == 0 || map_dev->acc_node[i].win_size == 0)) {
			uop_err("Add acc node para invalid. (devid=%u; acc_node_id=%u)\n", map_dev->devid, i);
			kvfree(op_dev->acc_nodes);
			op_dev->acc_nodes = NULL;
			return -EINVAL;
		}
		op_dev->acc_nodes->nodes[i].acc_addr = map_dev->acc_node[i].acc_addr;
		op_dev->acc_nodes->nodes[i].asic_addr = 0;
		op_dev->acc_nodes->nodes[i].len = map_dev->acc_node[i].win_size;
		op_dev->acc_nodes->nodes[i].local_win_id = map_dev->acc_node[i].local_win_id;
	}

	init_rwsem(&op_dev->acc_nodes->sem);

	return 0;
}

static struct uop_bare_item *uop_find_bare_item(struct uop_acc_nodes *acc_nodes, u64 addr, u32 len)
{
	struct uop_bare_item *bare_item = NULL;
	u32 i = 0;

	if ((addr + len) < addr) {
		return NULL;
	}

	for (i = 0; i < acc_nodes->bare_item_num; i++) {
		bare_item = &acc_nodes->bare_items[i];
		if ((bare_item->asic_addr + bare_item->len) < bare_item->asic_addr) {
			continue;
		}
		if ((addr >= bare_item->asic_addr) && ((addr + len) <= (bare_item->asic_addr + bare_item->len))) {
			return bare_item;
		}
	}
	return NULL;
}

static struct uop_acc_node *uop_find_acc_node(struct uop_acc_nodes *acc_nodes, u64 addr, u32 len)
{
	struct uop_acc_node *acc_node = NULL;
	u32 i = 0;

	if ((addr + len) < addr) {
		return NULL;
	}

	for (i = 0; i < acc_nodes->acc_node_num; i++) {
		acc_node = &acc_nodes->nodes[i];
		if ((acc_node->asic_addr == 0) || ((acc_node->asic_addr + acc_node->len) < acc_node->asic_addr)) {
			continue;
		}
		if ((addr >= acc_node->asic_addr) && ((addr + len) <= (acc_node->asic_addr + acc_node->len))) {
			return acc_node;
		}
	}
	return NULL;
}

static void _uop_kern_get_acc_node(struct uop_acc_nodes *acc_nodes, struct uop_acc_node *acc_node, u32 num, u32 *real_num)
{
	u32 copy_num;

	*real_num = 0;
	down_read(&acc_nodes->sem);
	copy_num = min_t(u32, acc_nodes->acc_node_num, num);
	if (copy_num == 0) {
		up_read(&acc_nodes->sem);
		return;
	}

	(void)ubfm_memcpy(acc_node, copy_num * sizeof(struct uop_acc_node), acc_nodes->nodes,
		copy_num * sizeof(struct uop_acc_node));
	up_read(&acc_nodes->sem);
	*real_num = copy_num;
}

int uop_kern_get_acc_node(struct kmap_dev *map_dev, struct uop_acc_node *acc_node, u32 num, u32 *real_num)
{
	struct uop_dev *op_dev = container_of(map_dev, struct uop_dev, map_dev);
	u32 devid = map_dev->devid;

	if (unlikely((acc_node == NULL) || (num == 0) || (real_num == NULL))) {
		uop_err("Invalid param. (devid=%u; num=%u)\n", devid, num);
		return -EINVAL;
	}

	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}

	if (op_dev->acc_nodes == NULL) {
		*real_num = 0;
		return 0;
	}

	_uop_kern_get_acc_node(op_dev->acc_nodes, acc_node, num, real_num);

	return 0;
}

static const struct kmap_ops *uop_get_kmap_ops(u32 devid)
{
	u64 acc_type = UOP_ACC_MAX;
	const struct kmap_ops *ops = NULL;
	int ret;

	ret = ures_get_key_value(devid, MAMI_ACC_TYPE, &acc_type);
	if (ret != 0) {
		uop_err("Get acc type failed. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	if ((u32)acc_type >= UOP_ACC_MAX) {
		uop_err("Invalid acc type. (acc_type=%llu; devid=%u)\n", acc_type, devid);
		return NULL;
	}

	ops = kmap_get_ops((enum uop_acc_type)acc_type);
	if (ops == NULL) {
		uop_err("Get kmap ops failed. (acc_type=%llu; devid=%u)\n", acc_type, devid);
		return NULL;
	}

	return ops;
}

static int uop_kern_memory_map(u32 devid, enum uop_item_type type, struct kmap_addr *addr)
{
	const struct kmap_ops *ops = NULL;
	int ret;

	ops = uop_get_kmap_ops(devid);
	if (ops == NULL) {
		uop_err("Get kmap ops failed. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (type == UOP_ITEM_MEM) {
		ret = ops->mem_map(devid, addr);
	} else {
		ret = ops->reg_map(devid, addr);
	}

	return ret;
}

static void uop_kern_memory_unmap(u32 devid, enum uop_item_type type, struct kmap_addr *addr)
{
	const struct kmap_ops *ops = NULL;

	if (addr->acc_addr == 0) {
		uop_err("Acc addr is null. (devid=%u; type=%u)\n", devid, (u32)type);
		return;
	}

	ops = uop_get_kmap_ops(devid);
	if (ops == NULL) {
		uop_err("Get kmap ops failed. (devid=%u; type=%u)\n", devid, (u32)type);
		return;
	}

	if (type == UOP_ITEM_MEM) {
		ops->mem_unmap(devid, addr);
	} else {
		ops->reg_unmap(devid, addr);
	}
}

static int uop_kern_kmap_pack(struct kmap_addr *addr, const char *name, const struct ures_inst *inst,
							  const struct ures_addr_info *asic_addr, const struct ures_addr_info *local_addr)
{
	int ret;

	addr->die_id = inst->die_id;
	addr->local_asic_addr = local_addr->addr;
	addr->asic_addr = asic_addr->addr;
	addr->size = asic_addr->size;

	ret = ubfm_strcpy(addr->name, URES_NAME_LEN, name);
	return (ret != 0) ? -ENOMEM : 0;
}

int uop_kern_add_memory(struct ures_inst *inst, const char *name, enum uop_item_type type,
						struct ures_addr_info *asic_addr, struct ures_addr_info *local_addr)
{
	struct kmap_addr addr = {0};
	struct uop_item item = {0};
	int ret;

	if (inst == NULL || name == NULL || asic_addr == NULL || local_addr == NULL) {
		uop_err("Invalid para.\n");
		return -EINVAL;
	}

	ret = uop_kern_kmap_pack(&addr, name, inst, asic_addr, local_addr);
	if (ret != 0) {
		uop_err("Kmap pack by asic addr failed. (devid=%u; dieid=%u; type=%u; name=%s; ret=%d)\n",
				inst->devid, inst->die_id, (u32)type, name, ret);
		return ret;
	}

	ret = uop_kern_memory_map(inst->devid, type, &addr);
	if (ret != 0) {
		uop_err("Map asic addr failed. (devid=%u; dieid=%u; type=%u; name=%s; ret=%d)\n",
				inst->devid, inst->die_id, (u32)type, name, ret);
		return ret;
	}

	ret = uop_item_pack(&item, name, addr.asic_addr, addr.acc_addr, (u32)addr.size);
	if (ret != 0) {
		uop_err("Item pack by kmap failed. (devid=%u; dieid=%u; type=%u; name=%s; ret=%d)\n",
				inst->devid, inst->die_id, (u32)type, name, ret);
		uop_kern_memory_unmap(inst->devid, type, &addr);
		return ret;
	}

	ret = uop_kern_add_item(inst->devid, &item, type);
	if (ret != 0) {
		uop_err("Add item failed. (devid=%u; dieid=%u; type=%u; name=%s; ret=%d)\n",
				inst->devid, inst->die_id, (u32)type, name, ret);
		uop_kern_memory_unmap(inst->devid, type, &addr);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_add_memory);

#ifdef EMU_ST
typedef void (*uop_post_writel)(u32 devid, u64 addr, u32 val);
typedef void (*uop_post_writeq)(u32 devid, u64 addr, u64 val);

static uop_post_writel post_writel;
static uop_post_writel post_writeq;

void uop_kern_register_post_write(uop_post_writel writel, uop_post_writel writeq)
{
	post_writel = writel;
	post_writeq = writeq;
}
#endif

#define UOP_KERN_DEBUG	  1

static inline u64 uop_addr_align_down(u64 addr, u64 len)
{
	return (addr & (~(len - 1)));
}

static int uop_writel_by_acc_node(struct uop_dev *op_dev, u64 addr, u32 val)
{
	struct uop_acc_nodes *acc_nodes = op_dev->acc_nodes;
	struct rw_semaphore *sem = &acc_nodes->sem;
	u32 acc_idx = 0;
	struct uop_bare_item *bare_item = NULL;
	struct uop_acc_node *acc_node = NULL;
	int ret = 0;

	down_read(sem);
	bare_item = uop_find_bare_item(acc_nodes, addr, sizeof(val));
	if (unlikely(bare_item == NULL)) {
		up_read(sem);
		return -ENOENT;
	}

	acc_node = uop_find_acc_node(acc_nodes, addr, sizeof(val));
	if (acc_node != NULL) {
		ret = op_dev->ops.writel(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
		up_read(sem);
		return ret;
	}
	up_read(sem);
	down_write(sem);
	/* Re-validate under write lock: another thread may have mapped addr */
	acc_node = uop_find_acc_node(acc_nodes, addr, sizeof(val));
	if (acc_node != NULL) {
		ret = op_dev->ops.writel(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
		up_write(sem);
		return ret;
	}
	bare_item = uop_find_bare_item(acc_nodes, addr, sizeof(val));
	if (unlikely(bare_item == NULL)) {
		up_write(sem);
		return -ENOENT;
	}
	acc_idx = (acc_nodes->last_acc_idx + 1) % acc_nodes->acc_node_num;
	acc_node = &acc_nodes->nodes[acc_idx];
	if (acc_node->asic_addr != 0) {
		uop_kern_clear_win(op_dev->devid, acc_node->local_win_id);
	}
	acc_node->asic_addr = bare_item->asic_addr + ((addr - bare_item->asic_addr) / acc_node->len * acc_node->len);
	acc_node->asic_addr = uop_addr_align_down(acc_node->asic_addr, (u64)acc_node->len);
	ret = uop_kern_cfg_win(op_dev->devid, acc_node->local_win_id, acc_node->asic_addr, &acc_node->axi_id);
	if (unlikely(ret != 0)) {
		acc_node->asic_addr = 0;
		up_write(sem);
		return ret;
	}
	acc_nodes->last_acc_idx = acc_idx;
	ret = op_dev->ops.writel(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
	up_write(sem);
	return ret;
}

static inline int uop_writel(struct uop_dev *op_dev, u64 addr, u32 val)
{
	struct uop_items *items = &op_dev->items[UOP_ITEM_REG];
	struct uop_item *item = NULL;
	int ret;

	down_read(&items->sem);
	item = uop_find_item(items, addr, sizeof(val));
	if (uop_item_range_valid(item, addr, sizeof(val))) {
		ret = op_dev->ops.writel(op_dev->devid, item->acc_addr + (addr - item->asic_addr), val, item->user_ctx);
		up_read(&items->sem);
		return ret;
	}
	up_read(&items->sem);
	return (op_dev->acc_nodes != NULL) ? uop_writel_by_acc_node(op_dev, addr, val) : -EEXIST;
}

int uop_kern_writel(u32 devid, u64 addr, u32 val)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_writel(op_dev, addr, val);
	uop_dev_put(op_dev);

#ifdef EMU_ST
	if ((ret == 0) && (post_writel != NULL)) {
		post_writel(devid, addr, val);
	}
#endif

#ifdef UOP_KERN_DEBUG
	if (ret != 0) {
		uop_err_limit("Uop writel failed. (ret=%d; devid=%u; val=0x%x)\n", ret, devid, val);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_writel);

static int uop_readl_by_acc_node(struct uop_dev *op_dev, u64 addr, u32 *val)
{
	struct uop_acc_nodes *acc_nodes = op_dev->acc_nodes;
	struct rw_semaphore *sem = &acc_nodes->sem;
	struct uop_bare_item *bare_item = NULL;
	struct uop_acc_node *acc_node = NULL;
	u32 acc_idx = 0;
	int ret = 0;

	down_read(sem);
	bare_item = uop_find_bare_item(acc_nodes, addr, sizeof(*val));
	if (unlikely(bare_item == NULL)) {
		up_read(sem);
		return -ENOENT;
	}

	acc_node = uop_find_acc_node(acc_nodes, addr, sizeof(*val));
	if (acc_node != NULL) {
		ret = op_dev->ops.readl(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
		up_read(sem);
		return ret;
	}
	up_read(sem);
	down_write(sem);
	/* Re-validate under write lock: another thread may have mapped addr */
	acc_node = uop_find_acc_node(acc_nodes, addr, sizeof(*val));
	if (acc_node != NULL) {
		ret = op_dev->ops.readl(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
		up_write(sem);
		return ret;
	}
	bare_item = uop_find_bare_item(acc_nodes, addr, sizeof(*val));
	if (unlikely(bare_item == NULL)) {
		up_write(sem);
		return -ENOENT;
	}
	acc_idx = (acc_nodes->last_acc_idx + 1) % acc_nodes->acc_node_num;
	acc_node = &acc_nodes->nodes[acc_idx];
	if (acc_node->asic_addr != 0) {
		uop_kern_clear_win(op_dev->devid, acc_node->local_win_id);
	}
	acc_node->asic_addr = bare_item->asic_addr + ((addr - bare_item->asic_addr) / acc_node->len * acc_node->len);
	acc_node->asic_addr = uop_addr_align_down(acc_node->asic_addr, (u64)acc_node->len);
	ret = uop_kern_cfg_win(op_dev->devid, acc_node->local_win_id, acc_node->asic_addr, &acc_node->axi_id);
	if (unlikely(ret != 0)) {
		acc_node->asic_addr = 0;
		up_write(sem);
		return ret;
	}
	acc_nodes->last_acc_idx = acc_idx;
	ret = op_dev->ops.readl(op_dev->devid, acc_node->acc_addr + (addr - acc_node->asic_addr), val, NULL);
	up_write(sem);
	return ret;
}

static inline int uop_readl(struct uop_dev *op_dev, u64 addr, u32 *val)
{
	struct uop_items *items = &op_dev->items[UOP_ITEM_REG];
	struct uop_item *item = NULL;
	int ret;

	down_read(&items->sem);
	item = uop_find_item(items, addr, sizeof(*val));
	if (uop_item_range_valid(item, addr, sizeof(*val))) {
		ret = op_dev->ops.readl(op_dev->devid, item->acc_addr + (addr - item->asic_addr), val, item->user_ctx);
		up_read(&items->sem);
		return ret;
	}
	up_read(&items->sem);
	return (op_dev->acc_nodes != NULL) ? uop_readl_by_acc_node(op_dev, addr, val) : -EEXIST;
}

int uop_kern_readl(u32 devid, u64 addr, u32 *val)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	if (unlikely(val == NULL)) {
		return -EINVAL;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_readl(op_dev, addr, val);
	uop_dev_put(op_dev);

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_readl);

int uop_i3c_send_recv_kern(u32 devid, drv_ubnic_xfer_msg *msg)
{
	struct udm_dev_para para;
	int ret;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		return ret;
	}
#ifdef CFG_UBFM_SUPPORT_PCIE_I3C
	return i3c_drv_ubnic_xfer(I3C_DRV_INST_ID_SELF, para.socket_id, msg);
#else
	return -ENOTSUPP;
#endif
}
EXPORT_SYMBOL_GPL(uop_i3c_send_recv_kern);

/* This function is used only by the DPU. */
int uop_kern_send_recv_msg(u32 dev_id,  u32 conn_type, void *ctx)
{
	struct uop_dev *op_dev = NULL;
	int ret;
	u32 send_dev_id;

	if (in_irq()) {
		return -EDEADLK;
	}

	if (unlikely(ctx == NULL)) {
		return -EINVAL;
	}
	if (conn_type == UDEV_PCIE_I3C) {
		send_dev_id = dev_id;
	} else {
		send_dev_id = udm_get_local_devid();
	}

	op_dev = uop_dev_get(send_dev_id);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	if (op_dev->ext_ops.send_recv_msg == NULL) {
		uop_err("Uop msg opt not reg. (devid=%u)", send_dev_id);
		uop_dev_put(op_dev);
		return -EINVAL;
	}
	ret = op_dev->ext_ops.send_recv_msg(send_dev_id, ctx);
	uop_dev_put(op_dev);
	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_send_recv_msg);

static int uop_writeq_by_acc_node(struct uop_dev *op_dev, u64 addr, u64 val)
{
	u32 *src = (u32 *)(void *)&val;
	u64 dst = addr;
	int ret;

	ret = uop_writel_by_acc_node(op_dev, dst, *src);
	if (unlikely(ret != 0)) {
		return ret;
	}
	src++;
	dst += UOP_U32_BYTE_NUM;
	ret = uop_writel_by_acc_node(op_dev, dst, *src);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return 0;
}

static inline int uop_writeq(struct uop_dev *op_dev, u64 addr, u64 val)
{
	struct uop_items *items = &op_dev->items[UOP_ITEM_REG];
	struct uop_item *item = NULL;
	int ret;

	down_read(&items->sem);
	item = uop_find_item(items, addr, sizeof(val));
	if (uop_item_range_valid(item, addr, sizeof(val))) {
		ret = op_dev->ops.writeq(op_dev->devid, item->acc_addr + (addr - item->asic_addr), val, item->user_ctx);
		up_read(&items->sem);
		return ret;
	}
	up_read(&items->sem);
	return (op_dev->acc_nodes != NULL) ? uop_writeq_by_acc_node(op_dev, addr, val) : -EEXIST;
}

int uop_kern_writeq(u32 devid, u64 addr, u64 val)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_writeq(op_dev, addr, val);
	uop_dev_put(op_dev);

#ifdef EMU_ST
	if ((ret == 0) && (post_writeq != NULL)) {
		post_writeq(devid, addr, val);
	}
#endif

#ifdef UOP_KERN_DEBUG
	if (ret != 0) {
		uop_err_limit("Uop writeq failed. (ret=%d; devid=%u; val=0x%llx)\n", ret, devid, val);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_writeq);

#define U32_WIDTH 32
int uop_kern_readq(u32 devid, u64 addr, u64 *val)
{
	struct uop_dev *op_dev = NULL;
	u32 low = 0, high = 0;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	if (unlikely(val == NULL)) {
		return -EINVAL;
	}
	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_readl(op_dev, addr, &low);
	if (ret != 0) {
		goto exit;
	}
	ret = uop_readl(op_dev, addr + sizeof(u32), &high);
	if (ret != 0) {
		goto exit;
	}
	*val = ((u64)high << U32_WIDTH) | (u64)low;
exit:
	uop_dev_put(op_dev);

#ifdef UOP_KERN_DEBUG
	if (ret != 0) {
		uop_err_limit("Uop readq failed. (ret=%d; devid=%u)\n", ret, devid);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_readq);

static inline int uop_mem_write(struct uop_dev *op_dev, u64 addr, void *data, u32 len)
{
	struct uop_items *items = &op_dev->items[UOP_ITEM_MEM];
	struct uop_item *item = NULL;
	int ret;

	down_read(&items->sem);
	item = uop_find_item(items, addr, len);
	ret = uop_item_range_valid(item, addr, len) ?
		op_dev->ops.mem_write(op_dev->devid, item->acc_addr + (addr - item->asic_addr),
			data, len, item->user_ctx) : (-EEXIST);
	up_read(&items->sem);

	return ret;
}

int uop_kern_mem_write(u32 devid, u64 addr, void *data, u32 len)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	if (unlikely((data == NULL) || (len == 0))) {
		return -EINVAL;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_mem_write(op_dev, addr, data, len);
	uop_dev_put(op_dev);

#ifdef UOP_KERN_DEBUG
	if (ret != 0) {
		uop_err_limit("Uop mem write failed. (ret=%d; devid=%u)\n", ret, devid);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_mem_write);

static inline int uop_mem_read(struct uop_dev *op_dev, u64 addr, void *data, u32 len)
{
	struct uop_items *items = &op_dev->items[UOP_ITEM_MEM];
	struct uop_item *item = NULL;
	int ret;

	down_read(&items->sem);
	item = uop_find_item(items, addr, len);
	ret = uop_item_range_valid(item, addr, len) ?
		op_dev->ops.mem_read(op_dev->devid, item->acc_addr + (addr - item->asic_addr),
			data, len, item->user_ctx) : (-EEXIST);
	up_read(&items->sem);

	return ret;
}

int uop_kern_mem_read(u32 devid, u64 addr, void *data, u32 len)
{
	struct uop_dev *op_dev = NULL;
	int ret;

	if (in_irq()) {
		return -EDEADLK;
	}

	if (unlikely((data == NULL) || (len == 0))) {
		return -EINVAL;
	}

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	ret = uop_mem_read(op_dev, addr, data, len);
	uop_dev_put(op_dev);

#ifdef UOP_KERN_DEBUG
	if (ret != 0) {
		uop_err_limit("Uop mem read failed. (ret=%d; devid=%u)\n", ret, devid);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(uop_kern_mem_read);

int uop_kern_io_buffer_write(u32 devid, u64 addr, void *data, u32 len)
{
	struct uop_dev *op_dev = NULL;
	int ret;
	u32 remainder = len % sizeof(u32);
	u32 reg_num = remainder == 0 ? (len / sizeof(u32)) : (len / sizeof(u32) + 1);
	u64 reg_addr;
	u32 reg_val;
	u32 i, j;

	if (in_irq()) {
		return -EDEADLK;
	}
	if (unlikely(data == NULL)) {
		return -EINVAL;
	}
	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}

	for (i = 0; i < reg_num; i++) {
		reg_addr = addr + (u64)(i * sizeof(u32));
		/* The last data is not 4-byte aligned, read byte by byte */
		if ((i == reg_num - 1) && (remainder != 0)) {
			reg_val = 0;
			for (j = 0; j < remainder; j++) {
				reg_val |= *(u8 *)(data + i * sizeof(u32) + j) << (8 * j); /* 8 : shift left by j bytes */
			}
		} else {
			reg_val = *(u32 *)(data + i * sizeof(u32));
		}

		ret = uop_writel(op_dev, reg_addr, reg_val);
#ifdef EMU_ST
		if ((ret == 0) && (post_writel != NULL)) {
			post_writel(devid, reg_addr, reg_val);
		}
#endif
		if (ret != 0) {
#ifdef UOP_KERN_DEBUG
			uop_err_limit("Uop writel failed. (ret=%d; devid=%u; val=0x%x)\n", ret, devid, reg_val);
#endif
			uop_dev_put(op_dev);
			return ret;
		}
	}
	uop_dev_put(op_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_io_buffer_write);

int uop_kern_io_buffer_read(u32 devid, u64 addr, void *data, u32 len)
{
	struct uop_dev *op_dev = NULL;
	int ret;
	u32 remainder = len % sizeof(u32);
	u32 reg_num = remainder == 0 ? (len / sizeof(u32)) : (len / sizeof(u32) + 1);
	u64 reg_addr;
	u32 reg_val;
	u32 i, j;

	if (in_irq()) {
		return -EDEADLK;
	}
	if (unlikely(data == NULL)) {
		return -EINVAL;
	}
	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	for (i = 0; i < reg_num; i++) {
		reg_val = 0;
		reg_addr = addr + (u64)(i * sizeof(u32));
		ret = uop_readl(op_dev, reg_addr, &reg_val);
		if (ret != 0) {
#ifdef UOP_KERN_DEBUG
			uop_err_limit("Uop readl failed. (ret=%d; devid=%u)\n", ret, devid);
#endif
			uop_dev_put(op_dev);
			return ret;
		}
		if ((i == reg_num - 1) && remainder != 0) {
			for (j = 0; j < remainder; j++) {
				*(u8 *)(data + i * sizeof(u32) + j) = (u8)(reg_val >> (8 * j)); /* 8 : shift right by j bytes */
			}
		} else {
			*(u32 *)(data + i * sizeof(u32)) = reg_val;
		}
	}
	uop_dev_put(op_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(uop_kern_io_buffer_read);

static void uop_items_show(struct uop_items *items, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 num = items->num;
	u32 i;

	if (num == 0) {
		return;
	}
	show(priv, "  %s:\n", uop_get_item_name(items->type));
	for (i = 0; i < num; i++) {
		struct uop_item *item = &items->item[i];

		show(priv, "    [%u]:name=%s; len=0x%x\n", i, item->name, item->len);
	}
}

static u64 get_timestamp_s(void)
{
	return jiffies_to_msecs(jiffies) / 1000; /* 1000ms = 1s */
}

static void uop_dev_show(struct uop_dev *op_dev, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 total_num = uop_get_item_total_num(op_dev);
	enum uop_item_type type;
	static u32 timestamp;

	if (total_num == 0) {
		return;
	}
	if (get_timestamp_s() - timestamp < 30) {   /* 30s */
		return;
	}
	timestamp = get_timestamp_s();
	show(priv, "Uop show:[dev-%u]\n", op_dev->devid);
	for (type = UOP_ITEM_MEM; type < UOP_ITEM_TYPE_MAX; type++) {
		struct uop_items *items = &op_dev->items[type];

		down_read(&items->sem);
		uop_items_show(items, show, priv);
		up_read(&items->sem);
	}
}

void uop_kern_dev_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	struct uop_dev *op_dev = uop_dev_get(devid);

	if (op_dev == NULL) {
		return;
	}
	uop_dev_show(op_dev, show, priv);
	uop_dev_put(op_dev);
}

static void _uop_kern_get_items(struct uop_dev *op_dev, enum uop_item_type type,
	struct uop_item *item, u32 num, u32 *real_num)
{
	struct uop_items *items = &op_dev->items[type];
	u32 copy_num;

	*real_num = 0;
	down_read(&items->sem);
	copy_num = min_t(u32, items->num, num);
	if (copy_num == 0) {
		/* If there is no items, still return 0 */
		up_read(&items->sem);
		return;
	}
	/* Src size == Dest size, Don't mind return value */
	(void)ubfm_memcpy(item, copy_num * sizeof(struct uop_item), items->item, copy_num * sizeof(struct uop_item));
	up_read(&items->sem);
	*real_num = copy_num;
}

int uop_kern_get_items(struct kmap_dev *map_dev, enum uop_item_type type, struct uop_item *item, u32 num, u32 *real_num)
{
	struct uop_dev *op_dev = container_of(map_dev, struct uop_dev, map_dev);
	u32 devid = map_dev->devid;

	if (unlikely(type >= UOP_ITEM_TYPE_MAX)) {
		uop_err("Invalid type. (devid=%u; type=%u)\n", devid, type);
		return -EINVAL;
	}

	if (unlikely((item == NULL) || (num == 0) || (real_num == NULL))) {
		uop_err("Invalid param. (devid=%u; num=%u)\n", devid, num);
		return -EINVAL;
	}

	if (unlikely(op_dev == NULL)) {
		return -ENODEV;
	}
	_uop_kern_get_items(op_dev, type, item, num, real_num);

	return 0;
}

void uop_kern_reset(u32 devid)
{
	struct uop_dev *op_dev = NULL;

	op_dev = uop_dev_get(devid);
	if (unlikely(op_dev == NULL)) {
		return;
	}
	uop_dev_del(op_dev->devid);
	uop_dev_put(op_dev); /* Put when op_dev created */
	uop_dev_put(op_dev);
}

void uop_kern_mod_init(void)
{
	u32 devid;

	for (devid = 0; devid < UOP_MAX_DEVNUM; devid++) {
		rwlock_init(&g_dev_lock[devid]);
	}
}

void uop_kern_mod_uninit(void)
{
	u32 devid;

	for (devid = 0; devid < UOP_MAX_DEVNUM; devid++) {
		uop_kern_reset(devid);
		cond_resched();
	}
}

