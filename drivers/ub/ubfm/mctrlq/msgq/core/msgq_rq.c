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

#include "uop.h"
#include "fmdevm.h"
#include "ures.h"
#include "mctrlq_log.h"

#include "msgq_trace.h"
#include "msgq_rq.h"
#define MSGQ_RQ_ENTRY_NUM   16
#define MSGQ_RQ_ENTRY_SIZE  128
#define MSGQ_RQ_ENTRY_SRAM_SIZE  16

static struct msgq_rq *msgq_die_rq_create(u32 devid, u32 die_id, struct msgq_asic *asic)
{
	struct msgq_rq *rq = NULL;

	rq = (struct msgq_rq *)kvzalloc(sizeof(struct msgq_rq), GFP_KERNEL_ACCOUNT);
	if (unlikely(rq == NULL)) {
		mctrlq_err("Rq create fail. (devid=%u; die_id=%u; size=%zu)\n", devid, die_id, sizeof(struct msgq_rq));
		return NULL;
	}
	rq->devid = devid;
	rq->die_id = die_id;
	rq->asic = *asic;

	return rq;
}

static void _msgq_rq_destroy(struct msgq_rq *rq)
{
	kvfree(rq);
}

static int msgq_rq_init(struct msgq_rq *rq)
{
	u32 head = UINT_MAX, tail = UINT_MAX, depth = 0;
	u32 blk_entry;
	u32 ret;

	ret = (u32)uop_kern_readl(rq->devid, rq->asic.tail, &tail);
	ret |= (u32)uop_kern_readl(rq->devid, rq->asic.head, &head);
	ret |= (u32)uop_kern_readl(rq->devid, rq->asic.depth, &depth);
	ret |= (u32)uop_kern_readl(rq->devid, rq->asic.blk_size, &blk_entry);
	if (unlikely(ret != 0)) {
		mctrlq_err("Cq que read fail. (ret=%u; devid=%u; die_id=%u)\n", ret, rq->devid, rq->die_id);
		return (int)ret;
	}

	if (unlikely((depth <= 1) || (depth > MSGQ_RQ_DEPTH_MAX) ||
		(tail >= depth) || (head >= depth) || (blk_entry > MSGQ_RQ_ENTRY_NUM))) {
		mctrlq_err("Invalid que. (devid=%u; die_id=%u; depth=%u; tail=%u; head=%u; blk_entry=%u)\n",
			rq->devid, rq->die_id, depth, tail, head, blk_entry);
		return -EINVAL;
	}

	rq->que.depth = depth;
	rq->que.tail = tail;
	rq->que.head = head;
	rq->blk_size = (1 << blk_entry) * MSGQ_RQ_ENTRY_SIZE;

	return 0;
}

STATIC int _msgq_rqe_get(struct msgq_rq *rq, struct msgq_cqe *cqe, void *rep_data, u32 rep_len, u32 *real_len)
{
	u32 index = cqe->rq_pi, len = ((cqe->p_len > rep_len) ? rep_len : cqe->p_len);
	u32 high_size, low_size, end;
	u64 addr;
	int ret;

	if ((rep_data == NULL) || (len == 0)) {
		return 0;
	}

	if (real_len != NULL) {
		*real_len = len;
	}

	end = index + DIV_ROUND_UP(len, rq->blk_size);
	if (end <= rq->que.depth) {
		addr = rq->asic.base + index * rq->blk_size;
		trace_msgq_rq("Rqe get", rq->devid, rq->die_id, &rq->que, cqe, index, len);
		return uop_kern_mem_read(rq->devid, addr, rep_data, len);
	}

	high_size = (rq->que.depth - index) * rq->blk_size;
	low_size = (len - high_size);

	addr = rq->asic.base + index * rq->blk_size;
	ret = uop_kern_mem_read(rq->devid, addr, rep_data, high_size);
	if (likely(ret == 0)) {
		addr = rq->asic.base;
		trace_msgq_rq("Rqe get", rq->devid, rq->die_id, &rq->que, cqe, index, len);
		ret = uop_kern_mem_read(rq->devid, addr, rep_data + high_size, low_size);
	}

	return ret;
}

static void msgq_rq_tail_update(struct msgq_rq *rq)
{
	u32 tail = rq->que.tail;
	int ret;

	ret = uop_kern_readl(rq->devid, rq->asic.tail, &tail);
	if (likely((ret == 0) && (tail < rq->que.depth))) {
		rmb(); /* Ensure the rq tail is read correctly. */
		rq->que.tail = tail;
	}
}

static bool msgq_rq_pi_valid(struct msgq_rq *rq, u32 pi)
{
	if (rq->que.tail >= rq->que.head) {
		return (pi <= rq->que.tail) && (pi >= rq->que.head);
	}
	return (pi >= rq->que.head) || (pi <= rq->que.tail);
}

static bool msgq_rq_can_recv(struct msgq_rq *rq, u32 pi, u32 num)
{
	u32 pi_begin = pi, pi_end = ((pi + num) % rq->que.depth);

	if (!msgq_rq_pi_valid(rq, pi_begin) || !msgq_rq_pi_valid(rq, pi_end)) {
		msgq_rq_tail_update(rq);
	}
	return msgq_rq_pi_valid(rq, pi_begin) && msgq_rq_pi_valid(rq, pi_end);
}

int msgq_rqe_get(struct msgq_rq *rq, struct msgq_cqe *cqe, void *rep_data, u32 rep_len, u32 *real_len)
{
	u32 num;

	if (unlikely(cqe == NULL)) {
		mctrlq_err("Cqe is NULL. (devid=%u; die_id=%u)\n", rq->devid, rq->die_id);
		return -EINVAL;
	}

	if (unlikely(cqe->p_len > rq->blk_size * rq->que.depth)) {
		mctrlq_err("Invalid p_len. (devid=%u; die_id=%u; p_len=%d; max_plen=%u; blk_size=%u)\n",
			rq->devid, rq->die_id, cqe->p_len, rq->blk_size * rq->que.depth, rq->blk_size);
		return -EINVAL;
	}

	num = DIV_ROUND_UP(cqe->p_len, rq->blk_size);
	if (num == 0) {
		/* There's no rqe data */
		if (real_len != NULL) {
			*real_len = 0;
		}
		return 0;
	}

	if (unlikely(!msgq_rq_can_recv(rq, cqe->rq_pi, num))) {
		mctrlq_err("Rq can't receive. (devid=%u; die_id=%u; num=%u; depth=%u; head=%u; tail=%u; rq_pi=%d)\n",
			rq->devid, rq->die_id, num, rq->que.depth, rq->que.head, rq->que.tail, cqe->rq_pi);
		return -ENOSPC;
	}

	return _msgq_rqe_get(rq, cqe, rep_data, rep_len, real_len);
}

static int _msgq_rq_update(struct msgq_rq *rq, struct msgq_cqe *cqe, u32 num)
{
	u32 head = (cqe->rq_pi + num) % rq->que.depth;
	int ret;

	ret = uop_kern_writel(rq->devid, rq->asic.head, head);
	if (likely(ret == 0)) {
		wmb(); /* Ensure the rq head is written correctly. */
		rq->que.head = head;
		trace_msgq_rq("Rqe update", rq->devid, rq->die_id, &rq->que, cqe, 0, 0);
	}
	return ret;
}

int msgq_rq_update(struct msgq_rq *rq, struct msgq_cqe *cqe)
{
	u32 num;

	if (unlikely((rq == NULL) || (cqe == NULL))) {
		return -EINVAL;
	}
	num = DIV_ROUND_UP(cqe->p_len, rq->blk_size);
	if (num == 0) {
		return 0;
	}

	if (cqe->rq_pi != rq->que.head) {
		mctrlq_err("Invalid pid. (devid=%u; die_id=%u; rq_pid=%d; head=%u; tail=%u; depth=%u)\n",
			rq->devid, rq->die_id, cqe->rq_pi, rq->que.head, rq->que.tail, rq->que.depth);
		return -EINVAL;
	}

	return _msgq_rq_update(rq, cqe, num);
}

struct msgq_rq *msgq_rq_create(u32 devid, u32 die_id, struct msgq_asic *asic)
{
	struct msgq_rq *rq = NULL;
	int ret;

	rq = msgq_die_rq_create(devid, die_id, asic);
	if (unlikely(rq == NULL)) {
		return NULL;
	}

	ret = msgq_rq_init(rq);
	if (unlikely(ret != 0)) {
		_msgq_rq_destroy(rq);
		return NULL;
	}

	return rq;
}

void msgq_rq_destroy(struct msgq_rq *rq)
{
	if (likely(rq != NULL)) {
		_msgq_rq_destroy(rq);
	}
}

int msgq_rq_clean(struct msgq_rq *rq)
{
	int ret = 0;
	u32 reg_tail = UINT_MAX;

	ret = uop_kern_readl(rq->devid, rq->asic.tail, &reg_tail);
	rmb(); /* Ensure the rq tail is read correctly. */
	if (unlikely(ret != 0)) {
		goto out;
	}
	if (unlikely(reg_tail >= rq->que.depth)) {
		mctrlq_err("Invalid reg_tail. (devid=%u; die_id=%u; reg_tail=%u; depth=%u)\n",
			rq->devid, rq->die_id, reg_tail, rq->que.depth);
		ret = -EINVAL;
		goto out;
	}
	ret = uop_kern_writel(rq->devid, rq->asic.head, reg_tail);
	wmb(); /* Ensure the rq head is written correctly. */
	if (unlikely(ret != 0)) {
		goto out;
	}
	rq->que.head = reg_tail;
	rq->que.tail = reg_tail;

out:
	return ret;
}
