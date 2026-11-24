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
#include <linux/mm.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "mami_string.h"

#include "feature_kern.h"
#include "ures.h"
#include "ued.h"
#include "uop.h"
#include "ulog.h"
#include "idm.h"
#include "mctrlq_log.h"
#include "ubfabric.h"
#include "mctrlq.h"

#define CREATE_TRACE_POINTS
#include "ctrlq_kern.h"
#include "ctrlq_trace.h"

static void ctrlq_chan_destroy(struct ctrlq_chan *chan);

static struct ctrlq_dev *g_ctrlq[DEV_MAX_NUM];
static rwlock_t g_dev_lock[DEV_MAX_NUM];

static int ctrlq_hw_get_die_num(u32 devid, u32 *die_num, enum mami_udie_version *udie_version)
{
	u64 value = 0;
	int ret;

	ret = ures_get_key_value(devid, "die_num", &value);
	if (unlikely((ret != 0) || (value == 0) || (value > DIE_MAX_NUM))) {
		mctrlq_err("Get die num fail. (ret=%d; devid=%u; die_num=%llu)\n", ret, devid, value);
		return -EINVAL;
	}
	*die_num = (u32)value;

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &value);
	if (ret != 0) {
		mctrlq_err("Get udie version fail. (devid=%u; ret=%d)\n", devid, ret);
		return -EINVAL;
	}

	*udie_version = (enum mami_udie_version)value;

	mctrlq_info("Device info. (devid=%u; die_num=%u; version=%u)\n", devid, *die_num, *udie_version);
	return 0;
}

static struct ctrlq_dev *ctrlq_create(u32 devid, u32 chip_type)
{
	u32 valid_chan_num, max_chan_num, die_num, num;
	enum mami_udie_version udie_version;
	struct ctrlq_dev *ctrlq = NULL;
	size_t size = 0;
	u32 i;
	int ret = 0;

	ret = ctrlq_hw_get_die_num(devid, &die_num, &udie_version);
	if (ret != 0) {
		mctrlq_err("Get die num fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	(void)ctrlq_get_max_and_valid_chan_num(udie_version, chip_type, &valid_chan_num, &max_chan_num);
	num = die_num * max_chan_num;
	size = sizeof(struct ctrlq_dev) + sizeof(struct ctrlq_chan *) * num;
	ctrlq = kvzalloc(size, GFP_KERNEL_ACCOUNT);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Ctrlq dev alloc fail. (devid=%u; num=%u; size=%zu)\n", devid, num, size);
		return NULL;
	}

	for (i = 0; i < num; i++) {
		ctrlq->chan[i] = NULL;
	}

	ctrlq->num = num;
	ctrlq->devid = devid;
	ctrlq->die_num = die_num;
	ctrlq->max_chan_num = max_chan_num;
	ctrlq->valid_chan_num = valid_chan_num;
	ctrlq->udie_version = udie_version;
	ctrlq->chip_type = chip_type;

	spin_lock_init(&ctrlq->lock);
	kref_safe_init(&ctrlq->ref);

	return ctrlq;
}

static void ctrlq_destroy(struct ctrlq_dev *ctrlq)
{
	if (likely(ctrlq != NULL)) {
		u32 chan_id;

		for (chan_id = 0; chan_id < ctrlq->num; chan_id++) {
			ctrlq_chan_destroy(ctrlq->chan[chan_id]);
			ctrlq->chan[chan_id] = NULL;
		}
		kvfree(ctrlq);
	}
}

static int ctrlq_add(struct ctrlq_dev *ctrlq)
{
	u32 devid = ctrlq->devid;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}

	write_lock_bh(&g_dev_lock[devid]);
	if (unlikely(g_ctrlq[devid] != NULL)) {
		write_unlock_bh(&g_dev_lock[devid]);
		return -ENODEV;
	}
	g_ctrlq[devid] = ctrlq;
	write_unlock_bh(&g_dev_lock[devid]);

	return 0;
}

static void ctrlq_del(u32 devid)
{
	write_lock_bh(&g_dev_lock[devid]);
	if (likely(g_ctrlq[devid] != NULL)) {
		g_ctrlq[devid] = NULL;
	}
	write_unlock_bh(&g_dev_lock[devid]);
}

static void ctrlq_release(struct kref_safe *kref_s)
{
	struct ctrlq_dev *ctrlq = container_of(kref_s, struct ctrlq_dev, ref);

	ctrlq_destroy(ctrlq);
}

struct ctrlq_dev *ctrlq_get(u32 devid)
{
	struct ctrlq_dev *ctrlq = NULL;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return NULL;
	}

	read_lock_bh(&g_dev_lock[devid]);
	ctrlq = g_ctrlq[devid];
	if (likely(ctrlq != NULL)) {
		if (kref_safe_get_unless_zero(&ctrlq->ref) == 0) {
			read_unlock_bh(&g_dev_lock[devid]);
			return NULL;
		}
	}
	read_unlock_bh(&g_dev_lock[devid]);

	return ctrlq;
}

void ctrlq_put(struct ctrlq_dev *ctrlq)
{
	kref_safe_put(&ctrlq->ref, ctrlq_release);
}

int ctrlq_kern_get_info(u32 devid, struct ctrlq_global_info *info)
{
	int ret = 0;
	struct ctrlq_dev *ctrlq = NULL;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Ctrlq get fail. (devid=%u;)\n", devid);
		return -ENODEV;
	}
	info->devid = ctrlq->devid;
	info->die_num = ctrlq->die_num;
	info->chip_type = ctrlq->chip_type;
	info->udie_version = ctrlq->udie_version;
	info->max_chan_num =  ctrlq->max_chan_num;
	info->valid_chan_num = ctrlq->valid_chan_num;
	ctrlq_put(ctrlq);
	return ret;
}

int ctrlq_kern_init(u32 devid, u32 chip_type)
{
	struct ctrlq_dev *ctrlq = NULL;
	int ret;

	ctrlq = ctrlq_create(devid, chip_type);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Ctrlq create fail. (devid=%u; chip_type=%u)\n", devid, chip_type);
		return -ENOMEM;
	}

	ret = ctrlq_add(ctrlq);
	if (unlikely(ret != 0)) {
		ctrlq_destroy(ctrlq);
		mctrlq_err("Ctrlq add fail. (ret=%d; devid=%u; chip_type=%u)\n", ret, devid, chip_type);
	}

	return ret;
}

void ctrlq_kern_uninit(u32 devid)
{
	struct ctrlq_dev *ctrlq = NULL;

	ctrlq = ctrlq_get(devid);
	if (ctrlq == NULL) {
		return;
	}
	ctrlq_del(devid);
	ctrlq_put(ctrlq);
	ctrlq_put(ctrlq);
}

static int _ctrlq_sq_init(struct ctrlq_chan *chan, struct ctrlq_attr *attr)
{
	u32 depth = 0, tail = UINT_MAX, head = UINT_MAX;
	struct ctrlq_asic *asic = &attr->sq_asic;
	struct ctrlq_sq *sq = &chan->sq;
	int ret;

	ret = uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (unlikely((ret != 0) || (depth == 0) || (depth > (UINT_MAX / CTRLQ_DEPTH_STRIDE)))) {
		mctrlq_err("Sq asic read fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u; depth=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id, depth);
		return -EINVAL;
	}
	depth = depth * CTRLQ_DEPTH_STRIDE;
	if (unlikely((tail >= depth) || (head >= depth))) {
		mctrlq_err("Sq que invalid fail. (devid=%u; die_id=%u; chan_id=%u; depth=%u; head=%u; tail=%u)\n",
			chan->devid, chan->die_id, chan->chan_id, depth, head, tail);
		return -EFAULT;
	}
	sq->tx = 0;
	sq->que.tail = tail;
	sq->que.head = head;
	sq->que.depth = depth;
	sq->asic = *asic;
	sq->on_off = CTRLQ_CHAN_ON;
	mutex_init(&sq->lock);

	return 0;
}

static void _ctrlq_sq_uninit(struct ctrlq_chan *chan)
{
	mutex_destroy(&chan->sq.lock);
}

static int _ctrlq_sqe_init(struct ctrlq_chan *chan)
{
	struct ctrlq_sq *sq = &chan->sq;
	struct ctrlq_sqe *sqe = NULL;
	u32 num = CTRLQ_MAX_BB_NUM;

	sqe = kvcalloc(num, sizeof(struct ctrlq_sqe), GFP_KERNEL_ACCOUNT);
	if (unlikely(sqe == NULL)) {
		mctrlq_err("Sqe alloc fail. (devid=%u; die_id=%u; ;chan_id=%u; size=%zu; num=%u)\n",
			chan->devid, chan->die_id, chan->chan_id, sizeof(struct ctrlq_sqe) * num, num);
		return -ENOMEM;
	}
	sq->num = num;
	sq->sqe = sqe;
	return 0;
}

static void _ctrlq_sqe_uninit(struct ctrlq_chan *chan)
{
	struct ctrlq_sq *sq = &chan->sq;

	if (likely(sq->sqe != NULL)) {
		kvfree(sq->sqe);
		sq->sqe = NULL;
	}
	sq->num = 0;
}

static int ctrlq_sq_init(struct ctrlq_chan *chan, struct ctrlq_attr *attr)
{
	int ret;

	ret = _ctrlq_sq_init(chan, attr);
	if (unlikely(ret != 0)) {
		mctrlq_err("Sq init fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id);
		return ret;
	}

	ret = _ctrlq_sqe_init(chan);
	if (unlikely(ret != 0)) {
		_ctrlq_sq_uninit(chan);
		mctrlq_err("Sqe init fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id);
	}

	return ret;
}

static void ctrlq_sq_uninit(struct ctrlq_chan *chan)
{
	_ctrlq_sqe_uninit(chan);
	_ctrlq_sq_uninit(chan);
}

static inline void ctrlq_sq_enable(struct ctrlq_chan *chan)
{
	mutex_lock(&chan->sq.lock);
	chan->sq.on_off = CTRLQ_CHAN_ON;
	mutex_unlock(&chan->sq.lock);
}

static inline void ctrlq_sq_disable(struct ctrlq_chan *chan)
{
	mutex_lock(&chan->sq.lock);
	chan->sq.on_off = CTRLQ_CHAN_OFF;
	mutex_unlock(&chan->sq.lock);
}

static void ctrlq_cq_drop_evt_init(struct ctrlq_chan *chan, struct ctrlq_cq *cq)
{
	u32 plane_id = 0xFFFFFFFF;

	cq->drop_evt.devid = chan->devid;
	(void)idm_kern_get_plane_by_fdie(chan->devid, chan->die_id, &plane_id);
	cq->drop_evt.planeId = plane_id;
	cq->drop_evt.fChanId = chan->chan_id;
	cq->drop_evt.cnt = 0;
}

static int ctrlq_cq_init(struct ctrlq_chan *chan, struct ctrlq_attr *attr)
{
	u32 depth = 0, tail = UINT_MAX, head = UINT_MAX;
	struct ctrlq_asic *asic = &attr->cq_asic;
	struct ctrlq_cq *cq = &chan->cq;
	int ret;

	ret = uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (unlikely((ret != 0) || (depth == 0) || (depth > (UINT_MAX / CTRLQ_DEPTH_STRIDE)))) {
		mctrlq_err("Cq asic read fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u; depth=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id, depth);
		return -EINVAL;
	}

	depth = depth * CTRLQ_DEPTH_STRIDE;
	if (unlikely(tail >= depth)) {
		mctrlq_err("Cq depth read fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u; depth=%u; tail=%u; head=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id, depth, tail, head);
		return -EFAULT;
	}

	if (tail != head) {
		/* Abandon remain cqe when chan init */
		head = tail;
		ret = uop_kern_writel(chan->devid, asic->head, head);
		if (unlikely(ret != 0)) {
			mctrlq_err("Update head fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, chan->devid, chan->die_id, chan->chan_id);
			return ret;
		}
	}

	cq->rx = 0;
	cq->valid = true;
	cq->que.head = head;
	cq->que.tail = tail;
	cq->que.depth = depth;
	cq->asic = *asic;
	cq->cqe = NULL;
	cq->on_off = CTRLQ_CHAN_ON;
	ctrlq_cq_drop_evt_init(chan, cq);
	cq->fail_flag = false;
	cq->fail_time = 0;
	mutex_init(&cq->lock);

	return 0;
}

static void ctrlq_cq_uninit(struct ctrlq_chan *chan)
{
	mutex_destroy(&chan->cq.lock);
}

STATIC int ctrlq_cq_reset(struct ctrlq_chan *chan)
{
	struct ctrlq_asic *asic = &chan->cq.asic;
	struct ctrlq_cq *cq = &chan->cq;
	u32 tail = 0, head = 0;
	int ret;

	ret = uop_kern_readl(chan->devid, asic->tail, &tail);
	if (ret != 0) {
		mctrlq_err("Read ctrlq cq asic tail failed. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id);
		return ret;
	}

	if (tail >= cq->que.depth) {
		mctrlq_err("Ctrlq cq asic tail is invalid. (tail=%u; depth=%u; devid=%u; die_id=%u; chan_id=%u)\n",
			tail, cq->que.depth, chan->devid, chan->die_id, chan->chan_id);
		return -EINVAL;
	}

	head = tail;
	ret = uop_kern_writel(chan->devid, asic->head, head);
	if (unlikely(ret != 0)) {
		mctrlq_err("Update head fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
			ret, chan->devid, chan->die_id, chan->chan_id);
		return ret;
	}

	cq->valid = true;
	cq->que.head = head;
	cq->que.tail = tail;
	cq->cqe = NULL;

	return 0;
}

static int _ctrlq_cq_enable(struct ctrlq_chan *chan)
{
	int ret;

	if (chan->cq.on_off == CTRLQ_CHAN_ON) {
		mctrlq_info("Already on. (devid=%u; chan_id=%u)\n", chan->devid, chan->chan_id);
		return 0;
	}

	ret = ctrlq_cq_reset(chan);
	if (likely(ret == 0)) {
		chan->cq.on_off = CTRLQ_CHAN_ON;
	}

	return ret;
}

static inline int ctrlq_cq_enable(struct ctrlq_chan *chan)
{
	int ret;

	mutex_lock(&chan->cq.lock);
	ret = _ctrlq_cq_enable(chan);
	mutex_unlock(&chan->cq.lock);
	return ret;
}

static int _ctrlq_cq_disable(struct ctrlq_chan *chan)
{
	int ret;

	if (unlikely(chan->cq.on_off == CTRLQ_CHAN_OFF)) {
		mctrlq_info("Already off. (devid=%u; chan_id=%u)\n", chan->devid, chan->chan_id);
		return 0;
	}

	ret = ctrlq_cq_reset(chan);
	if (likely(ret == 0)) {
		chan->cq.on_off = CTRLQ_CHAN_OFF;
	}
	return ret;
}

static int ctrlq_cq_disable(struct ctrlq_chan *chan)
{
	int ret;

	mutex_lock(&chan->cq.lock);
	ret = _ctrlq_cq_disable(chan);
	mutex_unlock(&chan->cq.lock);
	return ret;
}

void mami_ctrlq_schan_handle(u32 devid, u32 die_id, void *rsv) {}

static struct ctrlq_chan *_ctrlq_chan_create(u32 devid, u32 die_id, u32 chan_id)
{
	struct ctrlq_chan *chan = NULL;

	chan = kvzalloc(sizeof(struct ctrlq_chan), GFP_KERNEL_ACCOUNT);
	if (unlikely(chan == NULL)) {
		return NULL;
	}
	mutex_init(&chan->lock);
	idr_init_base(&chan->idr, CTRLQ_SEQ_MIN);
	chan->devid = devid;
	chan->die_id = die_id;
	chan->chan_id = chan_id;
	chan->schan_ops.handle = mami_ctrlq_schan_handle;

	return chan;
}

static void _ctrlq_chan_destroy(struct ctrlq_chan *chan)
{
	idr_destroy(&chan->idr);
	mutex_destroy(&chan->lock);
	kvfree(chan);
}

static struct ctrlq_chan *ctrlq_chan_create(u32 devid, u32 die_id, u32 chan_id, struct ctrlq_attr *attr)
{
	struct ctrlq_chan *chan = NULL;
	int ret;

	chan = _ctrlq_chan_create(devid, die_id, chan_id);
	if (unlikely(chan == NULL)) {
		mctrlq_err("Ctrlq chan alloc fail. (devid=%u; die_id=%u; chan_id=%u; size=%zu)\n",
			devid, die_id, chan_id, sizeof(struct ctrlq_chan));
		return NULL;
	}

	ret = ctrlq_sq_init(chan, attr);
	if (unlikely(ret != 0)) {
		mctrlq_err("Ctrlq sq init fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n", ret, devid, die_id, chan_id);
		goto out;
	}

	ret = ctrlq_cq_init(chan, attr);
	if (unlikely(ret != 0)) {
		ctrlq_sq_uninit(chan);
		mctrlq_err("Ctrlq cq init fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n", ret, devid, die_id, chan_id);
		goto out;
	}
	return chan;
out:
	_ctrlq_chan_destroy(chan);
	return NULL;
}

static void ctrlq_chan_destroy(struct ctrlq_chan *chan)
{
	if (likely(chan != NULL)) {
		ctrlq_cq_uninit(chan);
		ctrlq_sq_uninit(chan);
		_ctrlq_chan_destroy(chan);
	}
}

static int _ctrlq_chan_add(struct ctrlq_dev *ctrlq, struct ctrlq_chan *chan)
{
	u32 chan_id = chan->chan_id;

	if (unlikely(chan_id >= ctrlq->num)) {
		return -EINVAL;
	}
	if (unlikely(ctrlq->chan[chan_id] != NULL)) {
		return -EACCES;
	}
	ctrlq->chan[chan_id] = chan;
	return 0;
}


int ctrlq_kern_get_cq_chan_size(u32 devid, u32 chan_id, u32 *chan_size)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Get ctrlq fail. (devid=%u; chan_id=%u)\n", devid, chan_id);
		return -ENODEV;
	}

	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (unlikely(chan == NULL)) {
		ctrlq_put(ctrlq);
		return -ENODATA;
	}

	*chan_size = chan->cq.que.depth * sizeof(struct ctrlq_cqe);
	ctrlq_put(ctrlq);
	return 0;
}

int ctrlq_kern_get_sq_chan_size(u32 devid, u32 chan_id, u32 *chan_size)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Get ctrlq fail. (devid=%u; chan_id=%u)\n", devid, chan_id);
		return -ENODEV;
	}

	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (unlikely(chan == NULL)) {
		ctrlq_put(ctrlq);
		return -ENODATA;
	}

	*chan_size = chan->sq.que.depth * sizeof(struct ctrlq_sqe);
	ctrlq_put(ctrlq);
	return 0;
}

int ctrlq_kern_get_sq_chan_que_and_asic(u32 devid, u32 chan_id, struct ctrlq_que *que, struct ctrlq_asic *asic)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Get ctrlq fail. (devid=%u; chan_id=%u)\n", devid, chan_id);
		return -ENODEV;
	}

	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (unlikely(chan == NULL)) {
		ctrlq_put(ctrlq);
		return -ENODATA;
	}

	*que = chan->sq.que;
	*asic = chan->sq.asic;
	ctrlq_put(ctrlq);
	return 0;
}

int ctrlq_kern_chan_add(u32 devid, u32 die_id, u32 chan_id, struct ctrlq_attr *attr)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENOMEM;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Get ctrlq fail. (devid=%u; die_id=%u; chan_id=%u)\n", devid, die_id, chan_id);
		return -ENODEV;
	}

	chan = ctrlq_chan_create(devid, die_id, chan_id, attr);
	if (unlikely(chan == NULL)) {
		mctrlq_err("Ctrlq chan create fail. (devid=%u; die_id=%u; chan_id=%u)\n", devid, die_id, chan_id);
		goto out;
	}

	spin_lock_bh(&ctrlq->lock);
	ret = _ctrlq_chan_add(ctrlq, chan);
	spin_unlock_bh(&ctrlq->lock);
	if (unlikely(ret != 0)) {
		ctrlq_chan_destroy(chan);
	}

out:
	ctrlq_put(ctrlq);

	return ret;
}

static inline u32 _ctrlq_get_sqe_num(u32 size)
{
	u32 size_ = (size > sizeof(struct ctrlq_sqe)) ? (size - sizeof(struct ctrlq_sqe)) : 0;

	return DIV_ROUND_UP(size_, sizeof(struct ctrlq_sqe)) + 1;
}

static void _ctrlq_sq_head_update(struct ctrlq_sq *sq)
{
	struct ctrlq_chan *chan = container_of(sq, struct ctrlq_chan, sq);
	u32 head = sq->que.head;
	int ret;

	ret = uop_kern_readl(chan->devid, sq->asic.head, &head);
	if (likely((ret == 0) && (head < sq->que.depth))) {
		sq->que.head = head;
	}
}

static void _ctrlq_cq_tail_update(struct ctrlq_cq *cq)
{
	struct ctrlq_chan *chan = container_of(cq, struct ctrlq_chan, cq);
	u32 tail = cq->que.tail;
	int ret;

	ret = uop_kern_readl(chan->devid, cq->asic.tail, &tail);
	if (likely((ret == 0) && (tail < cq->que.depth))) {
		cq->que.tail = tail;
	}
}

static int ctrlq_fill_sqe(struct ctrlq_chan *chan, struct ctrlq_msg *msg)
{
	u32 sqe_num = _ctrlq_get_sqe_num(msg->len);
	size_t sqe_size = sizeof(struct ctrlq_sqe);
	struct ctrlq_sq *sq = &chan->sq;
	struct ctrlq_sqe *sqe = sq->sqe;
	size_t size;
	int ret;

	if (unlikely((sqe_num > sq->num) || (sqe_num == 0) || (sqe == NULL))) {
		return -EINVAL;
	}

	/* If msg size is not divisible by sqe size, last sqe need to be cleared */
	if (msg->len % sqe_size != 0) {
		/* Dst size == Src size. do not mind return value */
		(void)ubfm_memset(&sqe[sqe_num - 1], sqe_size, 0, sqe_size);
	}

	/* Fill data */
	size = sqe_size * sq->num;
	ret = ubfm_memcpy(sqe, size, msg->data, msg->len);
	if (unlikely(ret != 0)) {
		return -ENOMEM;
	}

	/* Update bb_num */
	sqe[0].bb_num = sqe_num;

	return 0;
}

static bool _ctrlq_sqe_can_submit(struct ctrlq_sq *sq, u32 num)
{
	if (_ctrlq_get_que_space(&sq->que) < num) {
		_ctrlq_sq_head_update(sq);
		rmb(); /* Ensure the sq head is written correctly. */
	}
	return _ctrlq_get_que_space(&sq->que) >= num;
}

static bool _ctrlq_cqe_can_recv(struct ctrlq_cq *cq, u32 num)
{
	if (_ctrlq_get_que_used(&cq->que) < num) {
		_ctrlq_cq_tail_update(cq);
		rmb(); /* Ensure the cq tail is written correctly. */
	}
	return _ctrlq_get_que_used(&cq->que) >= num;
}

static int _ctrlq_sqe_submit(struct ctrlq_chan *chan)
{
	struct ctrlq_sq *sq = &chan->sq;
	struct ctrlq_sqe *sqe = sq->sqe;
	u32 tail = sq->que.tail;
	u64 addr;
	int ret;

	addr = sq->asic.base + sizeof(struct ctrlq_sqe) * tail;
	ret = uop_kern_mem_write(chan->devid, addr, sqe, sizeof(struct ctrlq_sqe) * sqe->bb_num);
	if (unlikely(ret != 0)) {
		return ret;
	}
	wmb(); /* Ensure the sqe is written correctly. */

	tail = _ctrlq_que_get_tail(&sq->que, sqe->bb_num);
	ret = uop_kern_writel(chan->devid, sq->asic.tail, tail);
	if (unlikely(ret != 0)) {
		return ret;
	}
	wmb(); /* Ensure the sq tail is written correctly. */

	ret = uop_kern_writel(chan->devid, sq->asic.intr, 1);
	if (unlikely(ret != 0)) {
		return ret;
	}
	wmb(); /* Ensure the sq intr is written correctly. */
	sq->tx++;
	_ctrlq_que_set_tail(&sq->que, tail);

	return 0;
}

static int ctrlq_sqe_submit(struct ctrlq_chan *chan)
{
	struct ctrlq_sq *sq = &chan->sq;
	struct ctrlq_sqe *sqe = sq->sqe;
	int ret = -ENOSPC;

	trace_prepare_sqe(&chan->id, &chan->sq.que, (struct ctrlq_msg_head *)sqe);
	if (likely(_ctrlq_sqe_can_submit(sq, sqe->bb_num))) {
		ret = _ctrlq_sqe_submit(chan);
		if (likely(ret == 0)) {
			trace_sqe(&chan->id, &chan->sq.que, (struct ctrlq_msg_head *)sqe);
		}
	}
	return ret;
}

static int _ctrlq_enque(struct ctrlq_chan *chan, struct ctrlq_msg *msg)
{
	int ret;

	if (chan->sq.on_off == CTRLQ_CHAN_OFF) {
		return -ENOLINK;
	}

	ret = ctrlq_fill_sqe(chan, msg);
	if (likely(ret == 0)) {
		ret = ctrlq_sqe_submit(chan);
	}

	return ret;
}

static int ctrlq_msg_check(struct ctrlq_msg *msg)
{
	if (unlikely(msg == NULL)) {
		return -EINVAL;
	}

	if (unlikely((msg->len == 0) || (msg->len > sizeof(struct ctrlq_sqe) * CTRLQ_MAX_BB_NUM))) {
		return -EINVAL;
	}

	return 0;
}

int ctrlq_kern_enque(u32 devid, u32 chan_id, struct ctrlq_msg *msg)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	trace_chan("Enque start", devid, chan_id);
	if (unlikely(in_interrupt())) {
		/* Do not support irq, softirq context */
		return -EDEADLK;
	}

	if (ctrlq_msg_check(msg) != 0) {
		return -EINVAL;
	}

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}

	/* There is no delete api. Don't need lock for chan */
	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		mutex_lock(&chan->sq.lock);
		ret = _ctrlq_enque(chan, msg);
		mutex_unlock(&chan->sq.lock);
	}
	ctrlq_put(ctrlq);
	trace_chan("Enque finish", devid, chan_id);

	return ret;
}

static int _ctrlq_kern_seq_alloc(struct ctrlq_chan *chan, u32 *seq)
{
	int id;

	mutex_lock(&chan->lock);
	id = idr_alloc(&chan->idr, NULL, CTRLQ_SEQ_MIN, CTRLQ_SEQ_MAX, GFP_KERNEL_ACCOUNT);
	mutex_unlock(&chan->lock);

	if (unlikely(id < 0)) {
		return -ENOSPC;
	}
	*seq = (u32)id;
	return 0;
}

int ctrlq_kern_seq_alloc(u32 devid, u32 chan_id, u32 *seq)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	if (unlikely(seq == NULL)) {
		return -EINVAL;
	}

	if (unlikely(in_interrupt())) {
		/* Do not support irq, softirq context */
		return -EDEADLK;
	}

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}

	/* There is no delete api. Don't need lock for chan */
	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		ret = _ctrlq_kern_seq_alloc(chan, seq);
	}
	ctrlq_put(ctrlq);

	return ret;
}

static void _ctrlq_kern_seq_free(struct ctrlq_chan *chan, u32 seq)
{
	mutex_lock(&chan->lock);
	idr_remove(&chan->idr, seq);
	mutex_unlock(&chan->lock);
}

void ctrlq_kern_seq_free(u32 devid, u32 chan_id, u32 seq)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return;
	}
	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		_ctrlq_kern_seq_free(chan, seq);
	}
	ctrlq_put(ctrlq);
}

static int _ctrlq_get_head_cqe(struct ctrlq_chan *chan, struct ctrlq_cqe *cqe)
{
	struct ctrlq_cq *cq = &chan->cq;
	u64 addr;

	addr = cq->asic.base + cq->que.head * sizeof(struct ctrlq_cqe);
	return uop_kern_mem_read(chan->devid, addr, cqe, sizeof(struct ctrlq_cqe));
}

static inline bool _ctrlq_cqe_valid(struct ctrlq_cqe *cqe)
{
	struct ctrlq_msg_head *cqe_head = (struct ctrlq_msg_head *)cqe;

	if (!ctrlq_is_mami_seq(cqe_head->seq) && (cqe_head->ret != 0)) {	/* For request messages to mami, the ret field must be 0 */
		return false;
	}
	return ((cqe->type >= CQE_TYPE_TP_MEM) && (cqe->type < CQE_TYPE_MAX)) &&
		(cqe->bb_num > 0) && (cqe->bb_num <= CTRLQ_MAX_BB_NUM);
}

static int _ctrlq_cqe_recv(struct ctrlq_cq *cq, struct ctrlq_cqe *cqe, u32 num)
{
	struct ctrlq_chan *chan = container_of(cq, struct ctrlq_chan, cq);
	u64 src_addr = cq->asic.base + cq->que.head * sizeof(struct ctrlq_cqe);
	u64 dest_addr = (u64)(uintptr_t)cqe;
	u32 new_head = _ctrlq_que_get_head(&cq->que, num);
	u32 end = cq->que.head + num;
	u32 upper_num = 0, lower_num = 0;
	int ret;

	if (end <= cq->que.depth) {
		ret = uop_kern_mem_read(chan->devid, src_addr, (void *)(uintptr_t)dest_addr, sizeof(struct ctrlq_cqe) * num);
		if (unlikely(ret != 0)) {
			mctrlq_err("Ctrlq uop read failed. (devid=%u; ret=%d)\n", chan->devid, ret);
			return ret;
		}
	} else {
		/* Upper read */
		upper_num = cq->que.depth - cq->que.head;
		ret = uop_kern_mem_read(chan->devid, src_addr, (void *)(uintptr_t)dest_addr, sizeof(struct ctrlq_cqe) * upper_num);
		if (unlikely(ret != 0)) {
			mctrlq_err("Ctrlq uop upper read failed. (devid=%u; ret=%d)\n", chan->devid, ret);
			return ret;
		}
		/* Lower read */
		dest_addr += sizeof(struct ctrlq_cqe) * upper_num;
		src_addr = cq->asic.base;
		lower_num = num - upper_num;
		ret = uop_kern_mem_read(chan->devid, src_addr, (void *)(uintptr_t)dest_addr, sizeof(struct ctrlq_cqe) * lower_num);
		if (unlikely(ret != 0)) {
			mctrlq_err("Ctrlq uop lower read failed. (devid=%u; ret=%d)\n", chan->devid, ret);
			return ret;
		}
	}

	rmb(); /* Ensure the ctrlq cqe is read correctly. */
	ret = uop_kern_writel(chan->devid, cq->asic.head, new_head);
	if (likely(ret == 0)) {
		wmb(); /* Ensure the cq head is written correctly. */
		cq->rx++;
		_ctrlq_que_set_head(&cq->que, new_head);
		return 0;
	}
	mctrlq_err("Ctrlq uop write failed. (devid=%u; ret=%d)\n", chan->devid, ret);

	return ret;
}

static int ctrlq_cqe_recv(struct ctrlq_cq *cq, struct ctrlq_cqe *cqe, u32 num)
{
	int ret;

	if (_ctrlq_cqe_can_recv(cq, num)) {
		ret = _ctrlq_cqe_recv(cq, cqe, num);
		if (ret != 0) {
			return -EFAULT;
		}
		return 0;
	}
	return -ENOSPC;
}

#define CTRLQ_PRINT_BUFFER_LEN 32 /* sizeof(struct ctrlq_cqe) *2 */
#define CTRLQ_PRINT_DATA_LEN 256 /* Each buffer holds at most 32 bytes, each byte occupies 5 characters (0x00 + space) */
static void ctrlq_print_cqe_mem(struct ctrlq_chan *chan)
{
	struct ctrlq_cq *cq = &chan->cq;
	struct ctrlq_que *que = &cq->que;
	u64 base = cq->asic.base;
	u64 offset1, offset2;
	u32 cqe_size = sizeof(struct ctrlq_cqe);
	u8 buffer[CTRLQ_PRINT_BUFFER_LEN];
	u32 depth_max = que->depth - 1, i; /* depth_idx_max 2047 */
	u32 head_idx = que->head > 0 ? que->head - 1 : depth_max; /* cur_cqe mem offset */
	char pre_data[CTRLQ_PRINT_DATA_LEN] = {0};
	char cur_data[CTRLQ_PRINT_DATA_LEN] = {0};
	int ret;

	/* Read pre_data into buffer */
	offset1 = head_idx > 0 ? base + (head_idx - 1) * cqe_size : base + depth_max * cqe_size;
	ret = uop_kern_mem_read(chan->devid, offset1, buffer, cqe_size);
	if (ret != 0) {
		mctrlq_err("Uop kern mem read addr(0x%llx) failed.\n", offset1);
		return;
	}
	for (i = 0; i < cqe_size; i++) {
		(void)UBFM_SPRINTF(pre_data + strlen(pre_data), sizeof(pre_data) - strlen(pre_data), "0x%02x ", buffer[i]);
	}
	/* Read cur_data into buffer */
	offset2 = base + head_idx * cqe_size;
	ret = uop_kern_mem_read(chan->devid, offset2, buffer, cqe_size);
	if (ret != 0) {
		mctrlq_err("Uop kern mem read addr(0x%llx) failed.\n", offset2);
		return;
	}
	for (i = 0; i < cqe_size; i++) {
		(void)UBFM_SPRINTF(cur_data + strlen(cur_data), sizeof(cur_data) - strlen(cur_data), "0x%02x ", buffer[i]);
	}
	/* struct ctrlq_cqe, struct ctrlq_msg_head */
	mctrlq_err("Ctrlq chan[%u] memory around head[%u]:\n pre_cqe:%s,\n cur_cqe:%s\n",
	chan->chan_id, que->head, pre_data, cur_data);
}

void ctrlq_send_cqe_drop_event(u32 devid, struct mami_ctrlq_drop_cqe_event *drop_evt)
{
	struct ued_published_event event = {0};
	u32 fchan_id;
	int ret;

	fchan_id = drop_evt->fChanId;
	event.event_id = UED_EVENT_CTRLQ_EVENT_MSG;
	event.subevent_id = CTRLQ_CQE_DROP_SUB_EVENT;

	event.msg_len = sizeof(struct mami_ctrlq_drop_cqe_event);
	event.msg = (u8 *)drop_evt;

	ret = ued_submit_event_to_dev(devid, &event);
	if (likely(ret != 0)) {
		mctrlq_err("Ctrlq cqe drop send event failed.(devid=%u;fchanid=%u;ret=%d)\n", devid, fchan_id, ret);
		return;
	}
	mctrlq_info("Ctrlq cqe drop send event success.(devid=%u;fchanid=%u)\n", devid, fchan_id);
}

static u64 get_now_time(void)
{
	struct timespec64 ts = {0};

	ktime_get_real_ts64(&ts);
	return ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);
}

static void ctrlq_pack_drop_event(struct ctrlq_chan *chan, struct ctrlq_cqe *cqe)
{
	struct mami_ctrlq_drop_cqe_event drop_evt = chan->cq.drop_evt;
	u32 cnt = _ctrlq_get_que_used(&chan->cq.que) + 1;

	drop_evt.cnt = (u16)cnt;
	drop_evt.dropTime = get_now_time();
	drop_evt.cqeType = cqe->type;
	drop_evt.bbNum = cqe->bb_num;
	mctrlq_info("Ctrlq cqe will be dropped.(devid=%u;dieId=%u;chanId=%u;dropCqeCnt=%u;type=%d;bbNum=%u;head=%u;tail=%u)\n",
		chan->devid, chan->die_id, chan->chan_id, drop_evt.cnt, cqe->type, cqe->bb_num, chan->cq.que.head,
		chan->cq.que.tail);
	ctrlq_send_cqe_drop_event(chan->devid, &drop_evt);
}

static void ctrlq_cqe_fault_notify_recovery(struct ctrlq_chan *chan, struct ctrlq_cqe *cqe)
{
	int ret;

	ctrlq_pack_drop_event(chan, cqe);
	ctrlq_print_cqe_mem(chan);
	ret = ctrlq_cq_reset(chan);
	if (ret != 0) {
		mctrlq_err("Reset cq failed.(devid=%u;dieId=%u;chanId=%u;)\n", chan->devid, chan->die_id, chan->chan_id);
		return;
	}
	mctrlq_info("Reset cq success.(devid=%u;dieId=%u;chanId=%u;)\n", chan->devid, chan->die_id, chan->chan_id);
}

static u64 get_timestamp_ns(void)
{
	return ktime_get_ns();
}

static int _ctrlq_event_pack(struct ctrlq_chan *chan)
{
	size_t cqe_size = sizeof(struct ctrlq_cqe);
	struct ctrlq_msg_head *cqe_head = NULL;
	struct ctrlq_cq *cq = &chan->cq;
	struct ctrlq_cqe *cqe = cq->cqe;
	int ret;

	if (cqe != NULL) {
		/* Last event is not sent. Retry it */
		return 0;
	}
	cqe = (struct ctrlq_cqe *)cq->event.data;
	/* Get first cqe */
	ret = ctrlq_cqe_recv(cq, cqe, 1);
	if (unlikely(ret != 0)) {
		return ret;
	}

	cqe_head = (struct ctrlq_msg_head *)cqe;
	if (unlikely(!_ctrlq_cqe_valid(cqe))) {
		mctrlq_err("Invalid cqe, ser_type must in [%d, %d), bb_num must in =(0, %d], ret must be 0 when seq < 32768."
			"(devid=%u; die_id=%u; chan_id=%u; type=%d; bb_num=%u; head=%u; tail=%u; depth=%u; ret=%u; seq=%u)\n",
			CQE_TYPE_TP_MEM, CQE_TYPE_MAX, CTRLQ_MAX_BB_NUM,
			chan->devid, chan->die_id, chan->chan_id, cqe->type, cqe->bb_num,
			cq->que.head, cq->que.tail, cq->que.depth, cqe_head->ret, cqe_head->seq);
		ctrlq_cqe_fault_notify_recovery(chan, cqe);
		return -EBADR;
	}

	trace_cqe(&chan->id, &chan->cq.que, (struct ctrlq_msg_head *)cqe);
	cq->event.die_id = chan->die_id;
	cq->event.chan_id = chan->chan_id;
	cq->event.len = cqe_size * cqe->bb_num;
	/* If there's more than 1 cqe, read remain cqe */
	if (cqe->bb_num > 1) {
		ret = ctrlq_cqe_recv(cq, cqe + 1, cqe->bb_num - 1);
		if (unlikely(ret != 0)) {
			return ret;
		}
	}
	cq->cqe = cqe;
	cq->event.timestamp_ns = get_timestamp_ns();
	return 0;
}

static inline u32 ctrlq_type_to_event(u8 type)
{
	enum ued_event_id event_id = UED_EVENT_MAX_NUM;

	switch (type) {
	case 0x1:
		event_id = UED_EVENT_TPL_CTRL_MSG;
		break;
	case 0x2:
		event_id = UED_EVENT_DRM_CTRL_MSG;
		break;
	case 0x3:
		event_id = UED_EVENT_IRT_CTRL_MSG;
		break;
	case 0x4:
		event_id = UED_EVENT_UBQOS_CTRL_MSG;
		break;
	case 0x5:
		event_id = UED_EVENT_UB_MEM_CTRL_MSG;
		break;
	case 0x6:
		event_id = UED_EVENT_PORT_CTRLQ_MSG;
		break;
	default:
		break;
	}
	return event_id;
}

static int ctrlq_cq_recovery(struct ctrlq_chan *chan, struct ctrlq_cq *cq, int in_ret)
{
	int ret = in_ret;

	cq->cqe = NULL;
	if (cq->que.head != cq->que.tail) {
		ret = ctrlq_cq_reset(chan);
		if (ret != 0) {
			mctrlq_err("Reset cq failed.(devid=%u;dieId=%u;chanId=%u)\n", chan->devid, chan->die_id, chan->chan_id);
			return ret;
		}
		mctrlq_info("Reset cq success.(devid=%u;dieId=%u;chanId=%u;head=%u;tail=%u)\n",
					 chan->devid, chan->die_id, chan->chan_id, cq->que.head, cq->que.tail);
	}
	cq->fail_flag = false;
	return ret;
}

#define UED_ERR_TIMEOUT	4000ULL /* 4 * MSEC_PER_SEC; */
bool ued_invalid_timeout(u64 fail_time)
{
	return (get_now_time() - fail_time) > UED_ERR_TIMEOUT;
}

static int _ctrlq_send_event(struct ctrlq_chan *chan)
{
	struct ued_published_event event = {0};
	struct ctrlq_cq *cq = &chan->cq;
	struct ctrlq_cqe *cqe = cq->cqe;
	int ret;

	trace_prepare_cqe_event(&chan->id, &chan->cq.que, (struct ctrlq_msg_head *)cqe);
	event.event_id = ctrlq_type_to_event(cqe->type);
	if (unlikely(event.event_id == UED_EVENT_MAX_NUM)) {
		mctrlq_err("Ctrlq eventId invalid. (devid=%u;dieId=%u;chanId=%u;evtId=%d)\n",
			chan->devid, chan->die_id, chan->chan_id, event.event_id);
		return -EINVAL;
	}

	event.msg_len = cq->event.len + offsetof(struct ctrlq_event, data);
	event.msg = (u8 *)&cq->event;
	ret = ued_submit_event_to_dev(chan->devid, &event);
	if (unlikely(ret != 0)) {
		/* first fail */
		if (cq->fail_flag == false) {
			cq->fail_time = get_now_time();
			cq->fail_flag = true;
			return ret;
		}
		/* ued fail timeout 4s, cqe drop but ued cannot send event. */
		if (ued_invalid_timeout(cq->fail_time)) {
			mctrlq_err("Ctrlq ued send event failed. (devid=%u;dieId=%u;chanId=%u;evtId=%d;head=%u)\n",
					chan->devid, chan->die_id, chan->chan_id, event.event_id, cq->que.head);
			return ctrlq_cq_recovery(chan, cq, ret);
		}
		return ret;
	}

	cq->fail_flag = false;
	cq->cqe = NULL; /* Event sent. Prepare next event */
	trace_send_cqe_event(&chan->id, &chan->cq.que, (struct ctrlq_msg_head *)cqe);
	return 0;
}

int ctrlq_schan_check_pub(u32 devid, u32 die_id)
{
	struct ctrlq_global_info info = {0};
	int ret;

	if (devid >= DEV_MAX_NUM) {
		mctrlq_err("Invalid devid, must in [0, %d) but devid=%u.\n", DEV_MAX_NUM, devid);
		return -EINVAL;
	}
	if (!feature_is_support_schan(devid)) {
		mctrlq_err("Not support on this chipType.(devid=%u)\n", devid);
		return -E_OPNOTSUPP_ON_DEV;
	}

	ret = ctrlq_kern_get_info(devid, &info);
	if (ret != 0) {
		return ret;
	}
	if (die_id >= info.die_num) {
		mctrlq_err("Invalid die_id, must in [0, %u) but die_id=%u.(devid=%u)\n", info.die_num, die_id, devid);
		return -EINVAL;
	}

	return 0;
}

/* CtrlQ schan oper */
void ctrlq_chan_max_op(struct ctrlq_chan *chan, enum mami_kern_schan_op_type type, struct mami_kern_ctrlq_schan_handle_ops *ops)
{
	if (type == MAMI_KERN_SCHAN_OP_REG) {
		chan->schan_ops.handle = ops->handle;
		return;
	}
	chan->schan_ops.handle = mami_ctrlq_schan_handle;
}

static int ctrlq_schan_check_para(u32 devid, u32 die_id, enum mami_kern_schan_op_type type, struct mami_kern_ctrlq_schan_handle_ops *ops)
{
	uint32_t i = 0;

	if (type >= MAMI_KERN_SCHAN_OP_MAX) {
		mctrlq_err("Invalid type. (devid=%u; dieId=%u; type=%u; max_type=%u)\n",
			devid, die_id, type, MAMI_KERN_SCHAN_OP_MAX);
		return -EINVAL;
	}
	if (ops == NULL) {
		mctrlq_err("The ops is null. (devid=%u; die_id=%u; type=%d)\n", devid, die_id, type);
		return -EINVAL;
	}
	if ((type == MAMI_KERN_SCHAN_OP_REG) && (ops->handle == NULL)) {
		mctrlq_err("The ops handle is null. (devid=%u; die_id=%u; type=%d)\n", devid, die_id, type);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(ops->rsv); i++) {
		if (ops->rsv[i] != 0) {
			mctrlq_err("The ops rsv should all be 0. (devid=%u; die_id=%u; type=%d)\n", devid, die_id, type);
			return -EOPNOTSUPP;
		}
	}

	return 0;
}
int mami_ctrlq_schan_op(u32 devid, u32 die_id, enum mami_kern_schan_op_type type, struct mami_kern_ctrlq_schan_handle_ops *ops)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret;

	ret = ctrlq_schan_check_pub(devid, die_id);
	if (ret != 0) {
		return ret;
	}
	ret = ctrlq_schan_check_para(devid, die_id, type, ops);
	if (ret != 0) {
		mctrlq_err("Ctrlq_schan_check_para failed.(devid=%u; die_id=%u; type=%d; ret=%d)\n", devid, die_id, type, ret);
		return ret;
	}

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Ctrlq get fail.(devid=%u;die_id=%u;type=%d)\n", devid, die_id, type);
		return -ENODEV;
	}
	chan = ctrlq_get_chan(ctrlq, CTRLQ_SCHAN_ID + die_id * CTRLQ_CHAN_MAX_NUM);
	if (likely(chan != NULL)) {
		mutex_lock(&chan->lock);
		ctrlq_chan_max_op(chan, type, ops);
		mutex_unlock(&chan->lock);
	}
	ctrlq_put(ctrlq);
	mctrlq_info("Set schan op.(devid=%u;die_id=%u;type=%u)\n", devid, die_id, type);
	return 0;
}
EXPORT_SYMBOL_GPL(mami_ctrlq_schan_op);

static int _ctrlq_deque(struct ctrlq_chan *chan)
{
	struct ctrlq_cq *cq = &chan->cq;
	int loop_count = 128; /* Maximum of 128 CtrlQ entries at once. */
	int ret;

	if (cq->on_off == CTRLQ_CHAN_OFF) {
		/* Cq is off. do not change return value */
		return 0;
	}

	do {
		loop_count--;
		ret = _ctrlq_event_pack(chan);
		if (likely(ret == 0)) {
			ret = _ctrlq_send_event(chan);
		}
	} while ((ret == 0) && (loop_count >= 0));

	return ret;
}

bool schan_cqe_exist(struct ctrlq_chan *chan)
{
	struct ctrlq_cq *cq = &chan->cq;
	u32 head, tail;
	int ret;

	if (chan->chan_id != CTRLQ_SCHAN_ID) {
		return false;
	}
	ret = uop_kern_readl(chan->devid, cq->asic.tail, &tail);
	if (ret != 0) {
		mctrlq_err("Schan cq tail read fail. (ret=%d; devid=%u; die_id=%u)\n",
			ret, chan->devid, chan->die_id);
		return false;
	}
	ret = uop_kern_readl(chan->devid, cq->asic.head, &head);
	if (ret != 0) {
		mctrlq_err("Schan cq head read fail.(ret=%d;devid=%u;die_id=%u)\n", ret, chan->devid, chan->die_id);
		return false;
	}

	return tail != head;
}

int _ctrlq_kern_deque(u32 devid, u32 chan_id)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_err("Ctrlq dev init failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (chan == NULL) {
		ctrlq_put(ctrlq);
		return ret;
	}

	if (chan->schan_ops.handle == mami_ctrlq_schan_handle) {
		mutex_lock(&chan->cq.lock);
		ret = _ctrlq_deque(chan);
		mutex_unlock(&chan->cq.lock);
	} else {
		ret = 0;
		if (schan_cqe_exist(chan)) {
			chan->schan_ops.handle(devid, chan->die_id, NULL);
			mctrlq_debug("Ctrlq schan lcne. (devid=%u; chanId=%u)\n", devid, chan->chan_id);
		}
	}
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	if (ctrlq->irq_enable != 0) {
		(void)uop_kern_writel(devid, chan->cq.asic.int_mask, 0);
	}
#endif
	ctrlq_put(ctrlq);

	return ret;
}

static void _ctrlq_sq_show(struct ctrlq_chan *chan,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ctrlq_sq *sq = &chan->sq;
	u32 head, tail, depth;
	int ret;

	show(priv, "  Ctrlq Sq show:\n");
	show(priv, "    Sq asic: tail=0x%llx;head=0x%llx;depth=0x%llx;base=0x%llx\n",
		sq->asic.tail, sq->asic.head, sq->asic.depth, sq->asic.base);

	ret = uop_kern_readl(chan->devid, sq->asic.head, &head);
	ret |= uop_kern_readl(chan->devid, sq->asic.tail, &tail);
	ret |= uop_kern_readl(chan->devid, sq->asic.depth, &depth);
	if (likely(ret == 0)) {
		show(priv, "    Sq reg: tail=%u;head=%u;depth=%u\n", tail, head, depth);
	}
}

static inline void ctrlq_sq_show(struct ctrlq_chan *chan,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	mutex_lock(&chan->sq.lock);
	_ctrlq_sq_show(chan, show, priv);
	mutex_unlock(&chan->sq.lock);
}

static void _ctrlq_cq_show(struct ctrlq_chan *chan,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ctrlq_cq *cq = &chan->cq;
	struct ctrlq_cqe cqe = {0};
	u32 head, tail, depth;
	int ret;

	show(priv, "  Ctrlq Cq show:\n");
	show(priv, "    Cq que:  tail=%u;head=%u;depth=%u;rx=%llu;valid=%d;on_off=%d\n",
		cq->que.tail, cq->que.head, cq->que.depth, cq->rx, cq->valid, cq->on_off);
	show(priv, "    Cq asic: tail=0x%llx;head=0x%llx;depth=0x%llx;base=0x%llx\n",
		cq->asic.tail, cq->asic.head, cq->asic.depth, cq->asic.base);
	ret = _ctrlq_get_head_cqe(chan, &cqe);
	if (likely(ret == 0)) {
		show(priv, "    Cqe head: ver=%u;type=%u;bb_num=%u\n",
			cqe.ver, cqe.type, cqe.bb_num);
	}
	ret = uop_kern_readl(chan->devid, cq->asic.head, &head);
	ret |= uop_kern_readl(chan->devid, cq->asic.tail, &tail);
	ret |= uop_kern_readl(chan->devid, cq->asic.depth, &depth);
	if (likely(ret == 0)) {
		show(priv, "    Cq reg:  tail=%u;head=%u;depth=%u\n", tail, head, depth);
	}

	if (cq->cqe != NULL) {
		show(priv, "    Cq cache: ver=%u; type=%u; bb_num=%u\n", cq->cqe->ver, cq->cqe->type, cq->cqe->bb_num);
	}
}

static inline void ctrlq_cq_show(struct ctrlq_chan *chan,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	mutex_lock(&chan->cq.lock);
	_ctrlq_cq_show(chan, show, priv);
	mutex_unlock(&chan->cq.lock);
}

static inline void ctrlq_chan_show(struct ctrlq_chan *chan,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	show(priv, "Ctrlq show[dev-%u;die-%u;chan-%u]:\n",
		chan->devid, chan->die_id, chan->chan_id);
	ctrlq_sq_show(chan, show, priv);
	ctrlq_cq_show(chan, show, priv);
}

static int _ctrlq_sq_get_snapshot(struct ctrlq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	struct ctrlq_sq *sq = &chan->sq;
	u32 head, tail, depth;
	int ret, ret_snap;

	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, " Ctrlq Sq show:\n");

	ret = uop_kern_readl(chan->devid, sq->asic.head, &head);
	ret |= uop_kern_readl(chan->devid, sq->asic.tail, &tail);
	ret |= uop_kern_readl(chan->devid, sq->asic.depth, &depth);
	if (likely(ret == 0)) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Sq reg: tail=%u;head=%u;depth=%u\n", tail, head, depth);
	}

	if (ret_snap != 0) {
		mctrlq_warn("Ctrlq snapshot buffer lack of space. (devid=%u;ret=%d)", chan->devid, ret_snap);
		return ret_snap;
	}

	return 0;
}

static inline int ctrlq_sq_get_snapshot(struct ctrlq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	int ret;

	mutex_lock(&chan->sq.lock);
	ret = _ctrlq_sq_get_snapshot(chan, buf, buf_len, read_size);
	if (ret != 0) {
		mctrlq_warn("Ctrlq sq get snapshot failed. (devid=%u;chan_id=%u;ret=%d)\n", chan->devid, chan->chan_id, ret);
		goto out;
	}

out:
	mutex_unlock(&chan->sq.lock);
	return ret;
}

static int _ctrlq_cq_get_snapshot(struct ctrlq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	struct ctrlq_cq *cq = &chan->cq;
	struct ctrlq_cqe cqe = {0};
	u32 head, tail, depth;
	int ret, ret_snap;

	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, " Ctrlq Cq show:\n");
	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq que: tail=%u;head=%u;depth=%u;rx=%llu;valid=%d;on_off=%d\n",
		cq->que.tail, cq->que.head, cq->que.depth, cq->rx, cq->valid, cq->on_off);

	ret = _ctrlq_get_head_cqe(chan, &cqe);
	if (likely(ret == 0)) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cqe head: ver=%u;type=%u;bb_num=%u\n",
			cqe.ver, cqe.type, cqe.bb_num);
	}
	ret = uop_kern_readl(chan->devid, cq->asic.head, &head);
	ret |= uop_kern_readl(chan->devid, cq->asic.tail, &tail);
	ret |= uop_kern_readl(chan->devid, cq->asic.depth, &depth);
	if (likely(ret == 0)) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq reg: tail=%u;head=%u;depth=%u\n", tail, head, depth);
	}

	if (cq->cqe != NULL) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq cache: ver=%u; type=%u; bb_num=%u\n",
			cq->cqe->ver, cq->cqe->type, cq->cqe->bb_num);
	}

	if (ret_snap != 0) {
		mctrlq_warn("Ctrlq snapshot buffer lack of space. (devid=%u;ret=%d)\n", chan->devid, ret_snap);
		return ret_snap;
	}

	return 0;
}

static int ctrlq_cq_get_snapshot(struct ctrlq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	int ret;

	mutex_lock(&chan->cq.lock);
	ret = _ctrlq_cq_get_snapshot(chan, buf, buf_len, read_size);
	if (ret != 0) {
		mctrlq_warn("Ctrlq cq get snapshot failed. (devid=%u;chan_id=%u;ret=%d)\n", chan->devid, chan->chan_id, ret);
		goto out;
	}

out:
	mutex_unlock(&chan->cq.lock);
	return ret;
}

static int ctrlq_chan_get_snapshot(struct ctrlq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	int ret;

	ret = ulog_kern_dump_snapshot(buf, buf_len, read_size, "\nCtrlq show[dev-%u;die-%u;chan-%u]:\n",
		chan->devid, chan->die_id, chan->chan_id);
	if (ret != 0) {
		mctrlq_warn("Ctrlq snapshot buffer lack of space. (devid=%u;ret=%d)\n", chan->devid, ret);
		return ret;
	}

	ret = ctrlq_sq_get_snapshot(chan, buf, buf_len, read_size);
	ret |= ctrlq_cq_get_snapshot(chan, buf, buf_len, read_size);
	if (ret != 0) {
		mctrlq_warn("Ctrlq chan get snapshot failed. (devid=%u;chan_id=%u;ret=%d)\n", chan->devid, chan->chan_id, ret);
		return ret;
	}

	return 0;
}

#define SNAPSHOT_CTRLQ_MAX_OUTPUT_LEN (64 * 1024)
static int ctrlq_kern_snapshot_para_check(u32 devid, struct ctrlq_get_snapshot_in *para)
{
	if (devid >= DEV_MAX_NUM) {
		mctrlq_warn("Devid is invalid. (devid=%u; max_devid=%u)\n", devid, DEV_MAX_NUM - 1);
		return -EINVAL;
	}

	if (para == NULL) {
		mctrlq_warn("Ctrlq_get_snapshot_in para is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (para->buf == NULL) {
		mctrlq_warn("Ctrlq snapshot para invalid: buff is NULL. (devid=%u;buf_len=%u)\n", devid, para->buf_len);
		return -EINVAL;
	}

	if (para->read_size == NULL) {
		mctrlq_warn("Ctrlq snapshot para invalid: read_size is NULL. (devid=%u;buf_len=%u)\n", devid, para->buf_len);
		return -EINVAL;
	}

	if ((para->buf_len == 0) || (para->buf_len > SNAPSHOT_CTRLQ_MAX_OUTPUT_LEN)) {
		mctrlq_warn("Ctrlq snapshot para invalid: buf_len invalid. (devid=%u;buf_len=%u;max_buf_len=%u)\n",
			devid, para->buf_len, SNAPSHOT_CTRLQ_MAX_OUTPUT_LEN);
		return -EINVAL;
	}

	return 0;
}

static int ctrlq_copy_snapshot_to_user(u32 devid, void *buf_user, void *buf_kern, void *read_size_addr, u32 read_size)
{
	int ret;

	ret = (int)copy_to_user(buf_user, buf_kern, read_size);
	if (ret != 0) {
		mctrlq_warn("Ctrlq copy buf to user failed. (devid=%u,ret=%d)\n", devid, ret);
		return -EFAULT;
	}

	ret = (int)copy_to_user(read_size_addr, (void *)&read_size, sizeof(u32));
	if (ret != 0) {
		mctrlq_warn("Ctrlq copy read_size to user failed. (devid=%u,ret=%d)\n", devid, ret);
		return -EFAULT;
	}

	return 0;
}

int ctrlq_kern_get_snapshot(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ctrlq_get_snapshot_in *para = (struct ctrlq_get_snapshot_in *)in;
	struct ctrlq_dev *ctrlq = NULL;
	u32 buf_len = 0, read_size = 0;
	u32 chan_id;
	char *buf = NULL, *buf_start;
	int ret;

	if (in_len != sizeof(struct ctrlq_get_snapshot_in)) {
		mctrlq_warn("Ctrlq snapshot para in_len is invalid. (devid=%u; in_len=%u; valid_in_len=%u)\n", devid, in_len,
			(u32)sizeof(struct ctrlq_get_snapshot_in));
		return -EINVAL;
	}

	ret = ctrlq_kern_snapshot_para_check(devid, para);
	if (ret != 0) {
		mctrlq_warn("Ctrlq snapshot para invalid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	buf_len = para->buf_len;
	buf = (char *)kzalloc(buf_len, GFP_KERNEL | __GFP_ACCOUNT);
	if (buf == NULL) {
		mctrlq_warn("Ctrlq kzalloc snapshot buf failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}
	buf_start = buf;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		mctrlq_warn("Ctrlq is NULL. (devid=%u)\n", devid);
		(void)ulog_kern_dump_snapshot(&buf, &buf_len, &read_size, "Ctrlq is NULL. (devid=%u)\n", devid);
		goto out;
	}

	for (chan_id = 0; chan_id < ctrlq->num; chan_id++) {
		struct ctrlq_chan *chan = ctrlq_get_chan(ctrlq, chan_id);

		if (likely(chan != NULL)) {
			ret = ctrlq_chan_get_snapshot(chan, &buf, &buf_len, &read_size);
			if (ret != 0) {
				mctrlq_warn("Ctrlq get snapshot failed. (devid=%u;ret=%d)\n", devid, ret);
				break;
			}
		}
		cond_resched();
	}

	ctrlq_put(ctrlq);

out:
	ret = ctrlq_copy_snapshot_to_user(devid, (void *)para->buf, (void *)buf_start, (void *)para->read_size, read_size);
	kfree(buf_start);

	return ret;
}

static inline int _ctrlq_chan_enable(struct ctrlq_chan *chan)
{
	int ret;

	ret = ctrlq_cq_enable(chan);
	if (likely(ret == 0)) {
		ctrlq_sq_enable(chan);
	}
	return ret;
}

int ctrlq_kern_chan_enable(u32 devid, u32 chan_id)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}

	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		mutex_lock(&chan->lock);
		ret = _ctrlq_chan_enable(chan);
		mutex_unlock(&chan->lock);
	}
	ctrlq_put(ctrlq);

	if (unlikely(ret != 0)) {
		mctrlq_err("Ctrlq chan enable fail. (ret=%d; devid=%u; chan_id=%u)\n", ret, devid, chan_id);
	}

	return ret;
}

static int ctrlq_chan_disable(struct ctrlq_chan *chan)
{
	int ret;

	ret = ctrlq_cq_disable(chan);
	if (likely(ret == 0)) {
		ctrlq_sq_disable(chan);
	}
	return ret;
}

int ctrlq_kern_chan_disable(u32 devid, u32 chan_id)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}

	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		mutex_lock(&chan->lock);
		ret = ctrlq_chan_disable(chan);
		mutex_unlock(&chan->lock);
	}
	ctrlq_put(ctrlq);

	if (unlikely(ret != 0)) {
		mctrlq_err("Ctrlq chan disable fail. (ret=%d; devid=%u; chan_id=%u)\n", ret, devid, chan_id);
	}

	return ret;
}

int ctrlq_kern_irq_unmask(u32 devid)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	u32 chan_id;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}
	for (chan_id = 0; chan_id < ctrlq->num; chan_id++) {
		chan = ctrlq_get_chan(ctrlq, chan_id);
		if (likely(chan != NULL)) {
	#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
			(void)uop_kern_writel(devid, chan->cq.asic.int_mask, 0);
	#endif
		}
	}
	ctrlq->irq_enable = 1;
	ctrlq_put(ctrlq);
	return 0;
}

void ctrlq_kern_irq_mask(u32 devid)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	u32 chan_id;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return;
	}
	ctrlq->irq_enable = 0;
	for (chan_id = 0; chan_id < ctrlq->num; chan_id++) {
		chan = ctrlq_get_chan(ctrlq, chan_id);
		if (likely(chan != NULL)) {
	#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
			(void)uop_kern_writel(devid, chan->cq.asic.int_mask, 1);
	#endif
		}
	}
	ctrlq_put(ctrlq);
}

void ctrlq_kern_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ctrlq_dev *ctrlq = NULL;
	u32 chan_id;

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return;
	}
	for (chan_id = 0; chan_id < ctrlq->num; chan_id++) {
		struct ctrlq_chan *chan = ctrlq_get_chan(ctrlq, chan_id);

		if (likely(chan != NULL)) {
			ctrlq_chan_show(chan, show, priv);
		}
		cond_resched();
	}
	ctrlq_put(ctrlq);
}

int ctrlq_kern_mod_init(void)
{
	u32 devid;

	BUILD_BUG_ON(sizeof(struct ctrlq_sqe) != 32);   /* Sqe size must be 32Byte */
	BUILD_BUG_ON(sizeof(struct ctrlq_cqe) != 32);   /* Cqe size must be 32Byte */
	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		rwlock_init(&g_dev_lock[devid]);
	}
	return 0;
}

