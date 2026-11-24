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
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>

#include "feature_loader.h"
#include "ubfm_common.h"

#include "ures.h"
#include "mctrlq_log.h"
#include "msgq_kern.h"
#include "msgq_hw.h"

#define MSGQ_HW_WORK_TIME		  50

struct msgq_hw {
	u32 devid;
	u32 die_num;

	enum mami_udie_version ver;
	unsigned long status[BITS_TO_LONGS(DIE_MAX_NUM)];
	struct delayed_work work;

	bool stop;
};

static struct msgq_hw *g_msgq_hw[DEV_MAX_NUM];
static DEFINE_SPINLOCK(g_dev_lock);

static const struct msgq_hw_ops *g_msgq_ops[UDIE_VERSION_MAX];

static void msgq_hw_work(struct work_struct *work_)
{
	struct msgq_hw *msgq_dev = container_of(work_, struct msgq_hw, work.work);
	u32 die_num = msgq_dev->die_num;
	u32 devid = msgq_dev->devid;
	bool stop = false;
	u32 die_id;
	int ret;

	stop = msgq_dev->stop;
	rmb(); /* Ensure the stop flag is read correctly. */
	if (stop == true) {
		return;
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		if (!test_bit(die_id, msgq_dev->status)) {
			ret = msgq_kern_deque(devid, die_id);
			if (likely((ret == 0) || (ret == -EAGAIN) || (ret == -ENOSPC))) {
				continue;
			}
			if (msgq_kern_chan_check(devid, die_id) != 0) { /* When the cdma path is abnormal, enter abnormal state and prepare to discard pending msgq packets */
				set_bit(die_id, msgq_dev->status);
				mctrlq_err_limit("Msgq deque fail, the link is broken. (devid=%u; die_id=%u; ret=%d)\n", devid, die_id, ret);
			} else {
				mctrlq_err_limit("Msgq deque fail (devid=%u; die_id=%u; ret=%d)\n", devid, die_id, ret);
			}
		} else { /* Abnormal state detected, need to discard pending msgq packets */
			ret = msgq_kern_chan_check(devid, die_id);
			if (ret != 0) {
				continue;
			}
			ret = msgq_update_when_recover(devid, die_id);
			if (unlikely(ret != 0)) {
				mctrlq_err("Msgq recover chan fail. (devid=%u; die_id=%u; ret=%d)\n", devid, die_id, ret);
			} else {
				clear_bit(die_id, msgq_dev->status);
				mctrlq_info("Msgq recover chan success. (devid=%u; die_id=%u)\n", devid, die_id);
			}
		}
	}

	schedule_delayed_work(&msgq_dev->work, msecs_to_jiffies(MSGQ_HW_WORK_TIME));
}

static int mctrlq_get_die_num(u32 devid, u32 *die_num)
{
	u64 value = 0;
	int ret;

	ret = ures_get_key_value(devid, "die_num", &value);
	if ((ret != 0) || (value == 0) || (value > DIE_MAX_NUM)) {
		mctrlq_err("Get die num fail. (ret=%d; devid=%u; die_num=%llu)\n", ret, devid, value);
		return -EINVAL;
	}
	*die_num = (u32)value;

	return 0;
}

static int mctrlq_get_udie_ver(u32 devid, enum mami_udie_version *ver)
{
	u64 value;
	int ret;

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &value);
	if ((ret != 0) || (value >= (u64)UDIE_VERSION_MAX)) {
		mctrlq_err("Git udie ver fail. (ret=%d; devid=%u; value=%llu)\n", ret, devid, value);
		return -EINVAL;
	}
	*ver = (enum mami_udie_version)value;
	return 0;
}

static int _msgq_hw_init(u32 devid, u32 die_id, enum mami_udie_version ver)
{
	struct msgq_attr attr;
	int ret;

	if ((g_msgq_ops[ver] == NULL) || (g_msgq_ops[ver]->get_attr == NULL)) {
		mctrlq_err("Msgq ops is NULL. (devid=%u; die_id=%u; ver=%u)\n", devid, die_id, ver);
		return -EINVAL;
	}

	ret = g_msgq_ops[ver]->get_attr(&attr, devid, die_id);
	if (ret != 0) {
		mctrlq_err("Get attr fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	return msgq_kern_chan_add(devid, die_id, &attr);
}

static int msgq_hw_attr_dev_init(u32 devid, u32 die_num, enum mami_udie_version ver)
{
	int ret = 0;

	if ((g_msgq_ops[ver] != NULL) && (g_msgq_ops[ver]->init != NULL)) {
		ret = g_msgq_ops[ver]->init(devid, die_num);
	}
	return ret;
}

static void msgq_hw_attr_dev_uninit(u32 devid, enum mami_udie_version ver)
{
	if ((g_msgq_ops[ver] != NULL) && (g_msgq_ops[ver]->uninit != NULL)) {
		g_msgq_ops[ver]->uninit(devid);
	}
}

static int _msgq_kern_hw_init(struct msgq_hw *msgq_dev)
{
	u32 devid = msgq_dev->devid, die_num = msgq_dev->die_num;
	enum mami_udie_version ver = msgq_dev->ver;
	u32 die_id;
	int ret;

	ret = msgq_kern_init(devid);
	if (ret != 0) {
		mctrlq_err("Msgq init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = msgq_hw_attr_dev_init(devid, die_num, ver);
	if (ret != 0) {
		msgq_kern_uninit(devid);
		mctrlq_err("Attr dev init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		ret = _msgq_hw_init(devid, die_id, ver);
		if (ret != 0) {
			msgq_hw_attr_dev_uninit(devid, ver);
			msgq_kern_uninit(devid);
			mctrlq_err("Msgq init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
			break;
		}
	}

	if (ret == 0) {
		INIT_DELAYED_WORK(&msgq_dev->work, msgq_hw_work);
		schedule_delayed_work(&msgq_dev->work, 0);
	}

	return ret;
}

static void _msgq_kern_hw_uninit(struct msgq_hw *msgq_dev)
{
	enum mami_udie_version ver = msgq_dev->ver;
	u32 devid = msgq_dev->devid;

	msgq_dev->stop = true;
	wmb(); /* Ensure the stop flag is written correctly. */

	cancel_delayed_work_sync(&msgq_dev->work);
	msgq_hw_attr_dev_uninit(devid, ver);
	msgq_kern_uninit(devid);
}

static int _msgq_hw_add(struct msgq_hw *msgq_dev)
{
	u32 devid = msgq_dev->devid;

	spin_lock(&g_dev_lock);
	if (g_msgq_hw[devid] != NULL) {
		spin_unlock(&g_dev_lock);
		mctrlq_err("Msgq dev add fail. (devid=%u)\n", devid);
		return -ENODEV;
	}
	g_msgq_hw[devid] = msgq_dev;
	spin_unlock(&g_dev_lock);
	return 0;
}

static struct msgq_hw *_msgq_hw_get_del(u32 devid)
{
	struct msgq_hw *msgq_dev = NULL;

	spin_lock(&g_dev_lock);
	msgq_dev = g_msgq_hw[devid];
	g_msgq_hw[devid] = NULL;
	spin_unlock(&g_dev_lock);

	return msgq_dev;
}

static struct msgq_hw *msgq_hw_create(u32 devid)
{
	struct msgq_hw *msgq_dev = NULL;
	enum mami_udie_version ver;
	u32 die_num;
	int ret;

	ret = mctrlq_get_die_num(devid, &die_num);
	if (ret != 0) {
		mctrlq_err("Get die num fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	ret = mctrlq_get_udie_ver(devid, &ver);
	if (ret != 0) {
		mctrlq_err("Get udie ver fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	msgq_dev = (struct msgq_hw *)kvzalloc(sizeof(struct msgq_hw), GFP_KERNEL_ACCOUNT);
	if (msgq_dev == NULL) {
		mctrlq_err("Msgq dev alloc fail. (devid=%u; size=%zu)\n", devid, sizeof(struct msgq_hw));
		return NULL;
	}
	msgq_dev->devid = devid;
	msgq_dev->die_num = die_num;
	msgq_dev->ver = ver;
	msgq_dev->stop = false;

	return msgq_dev;
}

static void msgq_hw_destroy(struct msgq_hw *msgq_dev)
{
	if (msgq_dev != NULL) {
		kvfree(msgq_dev);
	}
}

int msgq_kern_hw_init(u32 devid)
{
	struct msgq_hw *msgq_dev = NULL;
	int ret;

	msgq_dev = msgq_hw_create(devid);
	if (msgq_dev == NULL) {
		mctrlq_err("Msgq hw create fail. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	ret = _msgq_kern_hw_init(msgq_dev);
	if (ret != 0) {
		mctrlq_err("Msgq hw init fail. (ret=%d; devid=%u)\n", ret, devid);
		goto err_init;
	}

	ret = _msgq_hw_add(msgq_dev);
	if (ret != 0) {
		mctrlq_err("Msgq hw add fail. (ret=%d; devid=%u)\n", ret, devid);
		goto err_add;
	}

	return 0;
err_add:
	_msgq_kern_hw_uninit(msgq_dev);
err_init:
	msgq_hw_destroy(msgq_dev);
	return ret;
}

void msgq_kern_hw_uninit(u32 devid)
{
	struct msgq_hw *msgq_dev = NULL;

	msgq_dev = _msgq_hw_get_del(devid);
	if (msgq_dev != NULL) {
		_msgq_kern_hw_uninit(msgq_dev);
		msgq_hw_destroy(msgq_dev);
	}
}

void msgq_kern_hw_register(enum mami_udie_version ver, const struct msgq_hw_ops *ops)
{
	if (ver < UDIE_VERSION_MAX) {
		g_msgq_ops[ver] = ops;
	}
}

int msgq_hw_mod_init(void)
{
	return msgq_kern_mod_init();
}
DECLARE_FEATURE_AUTO_INIT(msgq_hw_mod_init, FEATURE_LOADER_STAGE_1);

void msgq_hw_mod_uninit(void)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		msgq_kern_hw_uninit(devid);
		cond_resched();
	}
}
DECLARE_FEATURE_AUTO_UNINIT(msgq_hw_mod_uninit, FEATURE_LOADER_STAGE_1);

