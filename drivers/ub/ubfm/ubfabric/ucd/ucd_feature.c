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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "mami_string.h"
#include "ucd_define.h"
#include "ucd_feature.h"
#include "ucd_kv.h"
#include "ucd_acc_ctrl.h"
#include "ucd_proc_fs.h"

struct ucd_kv_table *g_feature_tbl;
struct ucd_kv_table *g_dev_feature_tbl;

#define STATE_ACTIVE 2U
#define STATE_STOP 3U

static void ucd_feature_register_kv_table_show(struct seq_file *seq, struct ucd_kv_table *target_table)
{
	u32 key;
	struct ucd_kv_node *kv_node = NULL;
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	struct tag_ucd_feature_node **feature_node = NULL;
	struct tag_ucd_feature *feature;

	read_lock_bh(&target_table->lock);
	for (key = 0; key < KV_TABLE_SIZE; key++) {
		head = &(target_table->table[key]);
		hlist_for_each_entry_safe(kv_node, tmp, head, hnode) {
			feature_node = (struct tag_ucd_feature_node **)(kv_node->data);
			feature = &((*feature_node)->feature);
			seq_printf(seq, "module=%s,main_cmd=%u,sub_cmd=%u,proc_ctrl_bitmap=%u\n",
				feature->module, feature->main_cmd, feature->sub_cmd, feature->proc_ctrl_bitmap);
		}
	}
	read_unlock_bh(&target_table->lock);
}

static void ucd_feature_statistic_kv_table_show(struct seq_file *seq, struct ucd_kv_table *target_table)
{
	u32 key;
	struct ucd_kv_node *kv_node = NULL;
	struct hlist_node *tmp = NULL;
	struct hlist_head *head = NULL;
	struct tag_ucd_feature_node **feature_node = NULL;
	struct tag_ucd_feature *feature;
	struct tag_feature_statistic *statistic;
	u64 time_min;

	read_lock_bh(&target_table->lock);
	for (key = 0; key < KV_TABLE_SIZE; key++) {
		head = &(target_table->table[key]);
		hlist_for_each_entry_safe(kv_node, tmp, head, hnode) {
			feature_node = (struct tag_ucd_feature_node **)(kv_node->data);
			feature = &((*feature_node)->feature);
			statistic = &((*feature_node)->s);
			if (statistic->used == 0) {
				time_min = 0;
			} else {
				time_min = statistic->time_min;
			}
			seq_printf(seq, "module=%s, main_cmd=%u, sub_cmd=%u, used=%llu, failed=%llu, "
				"last_ret=%d, time_last=%llums, time_max=%llums, time_min=%llums\n",
				feature->module, feature->main_cmd, feature->sub_cmd, statistic->used, statistic->failed,
				statistic->last_ret, statistic->time_last, statistic->time_max, time_min);
		}
	}
	read_unlock_bh(&target_table->lock);
}

void ucd_feature_register_show(struct seq_file *seq)
{
	ucd_feature_register_kv_table_show(seq, g_feature_tbl);
	ucd_feature_register_kv_table_show(seq, g_dev_feature_tbl);
}

void ucd_feature_statistic_show(struct seq_file *seq)
{
	ucd_feature_statistic_kv_table_show(seq, g_feature_tbl);
	ucd_feature_statistic_kv_table_show(seq, g_dev_feature_tbl);
}

static int ucd_dfx_statistics(struct tag_ucd_feature_node *node, int ret, u64 running_time)
{
	struct tag_feature_statistic *s = &node->s;

	mutex_lock(&(s->statistic_lock));
	s->used++;
	if (ret != 0) {
		s->failed++;
	}
	s->last_ret = ret;
	s->time_last = running_time;
	if (running_time > s->time_max) {
		s->time_max = running_time;
	}
	if (running_time < s->time_min) {
		s->time_min = running_time;
	}
	mutex_unlock(&(s->statistic_lock));
	return 0;
}

static void init_feature_statistics(struct tag_ucd_feature_node *node)
{
	struct tag_feature_statistic *s = &(node->s);

	s->used = 0;
	s->failed = 0;
	s->time_max = 0;
	s->time_min = ((u64)~0ULL);
	s->time_last = 0;
	s->last_ret = 0;
	mutex_init(&(s->statistic_lock));
}

static struct tag_ucd_feature_node *feature_get_key_node(struct ucd_kv_table *tbl, u32 key)
{
	struct tag_ucd_feature_node *node = NULL;
	int ret;

	ret = ucd_kv_get(tbl, key, (void *)&node, sizeof(struct tag_ucd_feature_node *));
	if ((ret != 0) || (node == NULL)) {
		return NULL;
	}
	return node;
}

static int feature_add_key_node(struct ucd_kv_table *tbl, u32 key, struct tag_ucd_feature_node *node)
{
	return ucd_kv_set(tbl, key, (void *)&node, sizeof(struct tag_ucd_feature_node *));
}

static int feature_del_key_node(struct ucd_kv_table *tbl, u32 key)
{
	return ucd_kv_del(tbl, key);
}

static struct tag_ucd_feature_node *feature_alloc_feature_node(struct tag_ucd_feature *feature)
{
	struct tag_ucd_feature_node *node = NULL;

	node = (struct tag_ucd_feature_node *)kzalloc(sizeof(struct tag_ucd_feature_node), GFP_KERNEL | __GFP_ACCOUNT);
	if (node == NULL) {
		ucd_err("kzalloc failed.\n");
		return NULL;
	}

	node->feature = *feature;
	init_feature_statistics(node);
	return node;
}

static void feature_free_feature_node(struct tag_ucd_feature_node *node)
{
	if (node->proc_ctrl != NULL) {
		kfree(node->proc_ctrl);
		node->proc_ctrl = NULL;
	}
	if (node->proc_buf != NULL) {
		kfree(node->proc_buf);
		node->proc_buf = NULL;
	}

	kfree(node);
}

static inline void feature_set_state(struct tag_ucd_feature_node *node, u32 state)
{
	node->state = state;
}

int feature_inc_work(struct tag_ucd_feature_node *node)
{
	/* If state was not "STATE_ACTIVE",no more work are allowed. */
	if (node->state != STATE_ACTIVE) {
		ucd_err("Not support in this status. (state=%u)\n", node->state);
		return -EBUSY;
	}
	atomic_inc(&node->count);
	return 0;
}

static inline void feature_dec_work(struct tag_ucd_feature_node *node)
{
	atomic_dec(&node->count);
}

static int feature_wait_work_finish(struct tag_ucd_feature_node *node)
{
	int wait_count = 0;
	/* Check file not in used */
	while ((atomic_read(&node->count) != 0) && (wait_count < FEATURE_WAIT_MAX_TIME)) {
		wait_count++;
		if ((wait_count % FEATURE_CONFIRM_WARN_MASK) == 0) {
			ucd_warn("Wait process finish. (main_cmd=%u; sub_cmd=%u; "
					 "work_count=%d)\n",
				node->feature.main_cmd,
				node->feature.sub_cmd,
				atomic_read(&node->count));
		}
		msleep(FEATURE_WAIT_EACH_TIME);
	}

	if (wait_count >= FEATURE_WAIT_MAX_TIME) {
		ucd_err("Wait process time out. (main_cmd=%u; sub_cmd=%u; work_count=%d)\n",
			node->feature.main_cmd,
			node->feature.sub_cmd,
			atomic_read(&node->count));
		return -EBUSY;
	}
	return 0;
}

static int ucd_add_cmd(struct ucd_kv_table *kv_tbl, struct tag_ucd_feature *feature)
{
	struct tag_ucd_feature_node *node = NULL;
	u32 key = 0;
	int ret;

	if ((kv_tbl == NULL) || (feature == NULL)) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}
	if ((feature->call_fn == NULL) || (feature->module == NULL)) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}
	key = (feature->main_cmd << MAINCMD_LEFT) | feature->sub_cmd;

	node = feature_alloc_feature_node(feature);
	if (node == NULL) {
		ucd_err("Alloc node failed. (main_cmd=%u; sub_cmd=%u)\n", feature->main_cmd, feature->sub_cmd);
		return -ENOMEM;
	}
	ret = feature_add_key_node(kv_tbl, key, node);
	if (ret != 0) {
		ucd_err("Add to table failed. (tag=%08x)\n", key);
		feature_free_feature_node(node);
		node = NULL;
		return ret;
	}
	feature_set_state(node, STATE_ACTIVE);

	ucd_info("register OK. (owner_name=%s; main_cmd=%u; sub_cmd=%u; proc_ctrl_bitmap=%u)\n",
		feature->module, feature->main_cmd, feature->sub_cmd, feature->proc_ctrl_bitmap);
	return 0;
}

int ucd_register_cmd(struct tag_ucd_feature *feature)
{
	return ucd_add_cmd(g_feature_tbl, feature);
}
EXPORT_SYMBOL_GPL(ucd_register_cmd);

int ucd_register_dev_cmd(struct tag_ucd_feature *feature)
{
	return ucd_add_cmd(g_dev_feature_tbl, feature);
}
EXPORT_SYMBOL_GPL(ucd_register_dev_cmd);

static int ucd_del_cmd(struct ucd_kv_table *kv_tbl, u8 main_cmd, u8 sub_cmd)
{
	int ret;
	u32 key = 0;
	struct tag_ucd_feature_node *node = NULL;

	if (kv_tbl == NULL) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}
	key = (main_cmd << MAINCMD_LEFT) | sub_cmd;

	node = feature_get_key_node(kv_tbl, key);
	if (node == NULL) {
		ucd_err("Not support feature. (tag=%08x)\n", key);
		return -EINVAL;
	}

	feature_set_state(node, STATE_STOP);
	feature_dec_work(node);
	ret = feature_wait_work_finish(node);
	if (ret != 0) {
		ucd_err("Wait work finish time out.(main_cmd=%u; sub_cmd=%u)\n", main_cmd, sub_cmd);
		return ret;
	}

	ret = feature_del_key_node(kv_tbl, key);
	if (ret != 0) {
		ucd_err("Delete kv node failed. (tag=%08x)\n", key);
		return ret;
	}

	feature_free_feature_node(node);
	node = NULL;
	ucd_info("unregister OK. (main_cmd=%u; sub_cmd=%u)\n", main_cmd, sub_cmd);
	return 0;
}

int ucd_unregister_cmd(u8 main_cmd, u8 sub_cmd)
{
	return ucd_del_cmd(g_feature_tbl, main_cmd, sub_cmd);
}
EXPORT_SYMBOL_GPL(ucd_unregister_cmd);

int ucd_unregister_dev_cmd(u8 main_cmd, u8 sub_cmd)
{
	return ucd_del_cmd(g_dev_feature_tbl, main_cmd, sub_cmd);
}
EXPORT_SYMBOL_GPL(ucd_unregister_dev_cmd);

static int _ucd_feature_process(struct ucd_kv_table *kv_tbl, struct tag_ucd_feature_arg *arg)
{
	struct tag_ucd_feature *feature = NULL;
	struct tag_ucd_feature_node *node = NULL;
	int ret;
	ktime_t start_time, end_time;
	u64 running_time;

	if ((arg == NULL) || (kv_tbl == NULL)) {
		ucd_err("Invalid Parameter.\n");
		return -EINVAL;
	}
	node = feature_get_key_node(kv_tbl, arg->key);
	if (node == NULL || (node->feature.call_fn == NULL)) {
		ucd_debug("Not support feature. (tag=%08x)\n", arg->key);
		return -E_OPNOTSUPP_ON_DEV;
	}

	feature = &node->feature;
	/* whitelist check */
	ret = ucd_check_proc_permission(feature->proc_ctrl_bitmap);
	if (ret != 0) {
		feature_dec_work(node);
		ucd_err("Operation not permitted. (tag=%08x; proc_ctrl_bitmap=%08x; main_cmd=%u; sub_cmd=%u)\n",
			arg->key, feature->proc_ctrl_bitmap, feature->main_cmd, feature->sub_cmd);
		return -EPERM;
	}

	start_time = ktime_get();
	if (kv_tbl == g_dev_feature_tbl) {
		ret = feature->dev_fn(arg->devid, arg->input, arg->input_len, arg->output, arg->output_len);
	} else {
		ret = feature->fn(arg->input, arg->input_len, arg->output, arg->output_len);
	}
	end_time = ktime_get();

	running_time = (u64)ktime_to_ns(end_time - start_time);
	ucd_dfx_statistics(node, ret, running_time);
	feature_dec_work(node);
	return ret;
}

int ucd_feature_process(struct tag_ucd_feature_arg *arg)
{
	return _ucd_feature_process(g_feature_tbl, arg);
}

int ucd_dev_feature_process(struct tag_ucd_feature_arg *arg)
{
	return _ucd_feature_process(g_dev_feature_tbl, arg);
}

int ucd_feature_init(void)
{
	g_feature_tbl = ucd_kv_init();
	if (g_feature_tbl == NULL) {
		return -ENOMEM;
	}

	g_dev_feature_tbl = ucd_kv_init();
	if (g_dev_feature_tbl == NULL) {
		ucd_kv_free(g_feature_tbl);
		g_feature_tbl = NULL;
		return -ENOMEM;
	}
	return 0;
}

void ucd_feature_uninit(void)
{
	ucd_kv_free(g_dev_feature_tbl);
	g_dev_feature_tbl = NULL;
	ucd_kv_free(g_feature_tbl);
	g_feature_tbl = NULL;
}

