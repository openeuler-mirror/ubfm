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
#include <linux/mm.h>
#include "mami_string.h"
#include "uop.h"

#include "ctrlq_kern.h"
#include "mctrlq_log.h"
#include "ctrlq_trace.h"

#ifdef CFG_BUILD_DEBUG  /* Delete the debug macro when testing */
/* begin:Debug for dt test:ctrlq_enque_cqe */
static inline u32 _ctrlq_get_cqe_num(u32 size)
{
	u32 size_ = (size > sizeof(struct ctrlq_cqe)) ? (size - sizeof(struct ctrlq_cqe)) : 0;

	return DIV_ROUND_UP(size_, sizeof(struct ctrlq_cqe)) + 1;
}

void _ctrlq_cq_head_update(struct ctrlq_cq *cq)
{
	struct ctrlq_chan *chan = container_of(cq, struct ctrlq_chan, cq);
	u32 head = cq->que.head;
	int ret;

	ret = uop_kern_readl(chan->devid, cq->asic.head, &head);
	if (likely((ret == 0) && (head < cq->que.depth))) {
		cq->que.head = head;
	}
}

static int ctrlq_fill_cqe(struct ctrlq_msg *msg, struct ctrlq_cqe *cqe)
{
	u32 cqe_num = _ctrlq_get_cqe_num(msg->len);
	size_t cqe_size = sizeof(struct ctrlq_cqe);
	size_t size;
	int ret;
	u32 cq_num = CTRLQ_MAX_BB_NUM;

	if (unlikely((cqe_num > cq_num) || (cqe_num == 0) || (cqe == NULL))) {
		return -EINVAL;
	}

	/* If msg size is not divisible by sqe size, last sqe need to be cleared */
	if (msg->len % cqe_size != 0) {
		/* Dst size == Src size. do not mind return value */
		(void)ubfm_memset(&cqe[cqe_num - 1], cqe_size, 0, cqe_size);
	}

	/* Fill data */
	size = cqe_size * cq_num;
	ret = ubfm_memcpy(cqe, size, msg->data, msg->len);
	if (unlikely(ret != 0)) {
		return -ret;
	}

	return 0;
}

static bool _ctrlq_cqe_can_submit(struct ctrlq_cq *cq, u32 num)
{
	if (_ctrlq_get_que_space(&cq->que) < num) {
		_ctrlq_cq_head_update(cq);
		rmb(); /* Ensure the cq head is written correctly. */
	}
	return _ctrlq_get_que_space(&cq->que) >= num;
}
static int _ctrlq_cqe_submit(struct ctrlq_chan *chan, struct ctrlq_cqe *cqe)
{
	struct ctrlq_cq *cq = &chan->cq;
	u32 tail = cq->que.tail;
	u64 addr;
	int ret;

	addr = cq->asic.base + sizeof(struct ctrlq_cqe) * tail;
	mctrlq_debug("[ctrlq_inject], cq_base_addr:0x%llx, dst_addr:0x%llx, tail=%u\n", cq->asic.base, addr, tail);
	ret = uop_kern_mem_write(chan->devid, addr, cqe, sizeof(struct ctrlq_cqe) * cqe->bb_num);
	if (unlikely(ret != 0)) {
		return ret;
	}
	wmb(); /* Ensure the cqe is written correctly. */

	tail = _ctrlq_que_get_tail(&cq->que, cqe->bb_num);
	ret = uop_kern_writel(chan->devid, cq->asic.tail, tail);
	if (unlikely(ret != 0)) {
		return ret;
	}
	wmb(); /* Ensure the cq tail is written correctly. */
	_ctrlq_que_set_tail(&cq->que, tail);
	return 0;
}

static int ctrlq_cqe_submit(struct ctrlq_chan *chan, struct ctrlq_cqe *cqe)
{
	struct ctrlq_cq *cq = &chan->cq;
	int ret = -ENOSPC;

	if (likely(_ctrlq_cqe_can_submit(cq, cqe->bb_num))) {
		ret = _ctrlq_cqe_submit(chan, cqe);
	}
	return ret;
}

static int _ctrlq_enque_cq(struct ctrlq_chan *chan, struct ctrlq_msg *msg)
{
	int ret;
	struct ctrlq_cqe *cqe = NULL;

	if (chan->cq.on_off == CTRLQ_CHAN_OFF) {
		return -ENOLINK;
	}

	cqe = kvcalloc(CTRLQ_MAX_BB_NUM, sizeof(struct ctrlq_cqe), GFP_KERNEL_ACCOUNT);
	if (unlikely(cqe == NULL)) {
		return -ENOMEM;
	}
	ret = ctrlq_fill_cqe(msg, cqe);
	mctrlq_debug("[hlt_kern_test] ctrlq_fill_cqe finish, ret=%d\n", ret);
	if (likely(ret == 0)) {
		ret = ctrlq_cqe_submit(chan, cqe);
		mctrlq_debug("[hlt_kern_test] ctrlq_cqe_submit finish, ret=%d\n", ret);
	}

	kvfree(cqe);
	cqe = NULL;
	return ret;
}

static int ctrlq_msg_check_cq(struct ctrlq_msg *msg)
{
	if (unlikely(msg == NULL)) {
		return -EINVAL;
	}

	if (unlikely((msg->len == 0) || (msg->len > sizeof(struct ctrlq_cqe) * CTRLQ_MAX_BB_NUM))) {
		return -EINVAL;
	}

	return 0;
}

int ctrlq_kern_enque_cq(u32 devid, u32 chan_id, struct ctrlq_msg *msg)
{
	struct ctrlq_dev *ctrlq = NULL;
	struct ctrlq_chan *chan = NULL;
	int ret = -ENODEV;

	trace_chan("Enque start", devid, chan_id);
	if (unlikely(in_interrupt())) {
		/* Do not support irq, softirq context */
		return -EDEADLK;
	}

	if (ctrlq_msg_check_cq(msg) != 0) {
		return -EINVAL;
	}

	ctrlq = ctrlq_get(devid);
	if (unlikely(ctrlq == NULL)) {
		return -ENODEV;
	}

	/* There is no delete api. Don't need lock for chan */
	chan = ctrlq_get_chan(ctrlq, chan_id);
	if (likely(chan != NULL)) {
		mutex_lock(&chan->cq.lock);
		ret = _ctrlq_enque_cq(chan, msg);
		mctrlq_debug("[hlt_kern_test] _ctrlq_enque_cq finish\n");
		mutex_unlock(&chan->cq.lock);
	}
	ctrlq_put(ctrlq);
	mctrlq_debug("[hlt_kern_test] enque finish\n");
	trace_chan("Enque finish", devid, chan_id);

	return ret;
}
EXPORT_SYMBOL_GPL(ctrlq_kern_enque_cq);
/* end:add for dt test ctrlq enque cqe */
#endif
