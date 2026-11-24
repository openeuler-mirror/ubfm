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
#include <linux/compiler.h>

#include "uop.h"
#include "mctrlq_log.h"

#include "msgq_trace.h"
#include "msgq_sq.h"
#include "msgq_comm.h"

static inline void msgq_lock_init(struct msgq_sq *sq)
{
	mutex_init(&sq->lock);
}

static inline void msgq_lock_uninit(struct msgq_sq *sq)
{
	mutex_destroy(&sq->lock);
}

static void msgq_sq_head_update(struct msgq_sq *sq)
{
	u32 head = sq->que.head;
	int ret;

	ret = uop_kern_readl(sq->devid, sq->asic.head, &head);
	if (likely((ret == 0) && (head < sq->que.depth))) {
		rmb(); /* Ensure the sq head is read correctly. */
		sq->que.head = head;
	}
}

static bool msgq_sqe_can_submit(struct msgq_sq *sq)
{
	if (msgq_get_que_space(&sq->que) == 0) {
		msgq_sq_head_update(sq);
	}
	return msgq_get_que_space(&sq->que) > 0;
}

static int msgq_sq_pld_write(struct msgq_sq *sq, struct msgq_sqe *sqe, struct msgq_msg *msg, bool sync)
{
	struct msg_pkt_header_kernel *pkt = (struct msg_pkt_header_kernel *)msg->req_data;
	u64 addr = sq->asic.base + sqe->p_addr;
	int ret;

	if ((msg->type == MSGQ_TYPE_PROTOCOL) && (sync == true)) {
		pkt->src_tassn = sqe->msn;
		pkt->msgq_id = MSGQ_ID_FM;
	}

	ret = uop_kern_mem_write(sq->devid, addr, msg->req_data, msg->req_len);
	if (likely(ret == 0)) {
		wmb(); /* Ensure the uop kern mem is written correctly. */
	}

	return ret;
}

static int _msgq_sqe_submit(struct msgq_sq *sq, struct msgq_sqe *sqe)
{
	u32 tail = sq->que.tail;
	u64 addr;
	int ret;

	addr = sq->asic.base + sizeof(struct msgq_sqe) * tail;
	ret = uop_kern_mem_write(sq->devid, addr, sqe, sizeof(struct msgq_sqe));
	if (unlikely(ret != 0)) {
		mctrlq_err("Sqe write fail. (ret=%d; devid=%u; die_id=%u)\n", ret, sq->devid, sq->die_id);
		return ret;
	}
	wmb(); /* Ensure the uop kern mem is written correctly. */

	tail = msgq_que_get_tail(&sq->que, 1);
	ret = uop_kern_writel(sq->devid, sq->asic.tail, tail);
	if (likely(ret == 0)) {
		wmb(); /* Ensure the uop kern is written correctly. */
		sq->que.tail = tail;
	}

	return ret;
}

static void msgq_sqe_fill(struct msgq_sqe *sqe, struct msgq_sq *sq, struct msgq_msg *msg, u32 msn)
{
	sqe->task_type = msg->type;
	sqe->local = msg->local;
	sqe->opcode = (u8)(msg->opcode);
	sqe->dev_type = msg->dev_type;
	sqe->icrc = (msg->type == MSGQ_TYPE_PROTOCOL) ? 1 : 0;

	sqe->msn = msn;

	sqe->p_len = msg->req_len;
	sqe->p_addr = sizeof(struct msgq_sqe) * sq->que.depth + sq->que.tail * sq->que.pld_size;
}

static int msgq_sqe_submit(struct msgq_sq *sq, struct msgq_msg *msg, struct msgq_sqe *sqe, bool sync)
{
	int ret = -ENOSPC;

	if (likely(msgq_sqe_can_submit(sq))) {
		ret = msgq_sq_pld_write(sq, sqe, msg, sync);
		if (likely(ret == 0)) {
			ret = _msgq_sqe_submit(sq, sqe);
		}
	}
	return ret;
}

void msgq_sq_read_reg(struct msgq_sq *sq, u32 *head, u32 *tail)
{
	msgq_sq_lock(sq);
	msgq_sq_read_head(sq, head);
	msgq_sq_read_tail(sq, tail);
	msgq_sq_unlock(sq);
}

int msgq_sq_send(struct msgq_chan *chan, struct msgq_msg *msg, u32 msn, bool sync, struct msgq_record *record)
{
	struct msgq_sq *sq = chan->sq;
	struct msgq_cq *cq = chan->cq;
	struct msgq_sqe sqe = {0};
	int ret;

	if (unlikely(msg->req_len > sq->que.pld_size)) {
		mctrlq_err("Invalid req len. (devid=%u; die_id=%u; req_len=%u; pld_size=%u)\n",
			sq->devid, sq->die_id, msg->req_len, sq->que.pld_size);
		return -EINVAL;
	}

	msgq_sq_lock(sq);

	msgq_cq_lock(cq);
	msgq_sq_read_head(sq, &record->sq_head);
	msgq_cq_read_tail(cq, &record->cq_tail);
	msgq_sq_read_tail(sq, &record->sq_tail);
	msgq_cq_read_head(cq, &record->cq_head);
	msgq_rq_read_head(cq->rq, &record->rq_head);
	msgq_rq_read_tail(cq->rq, &record->rq_tail);
	msgq_cq_unlock(cq);

	msgq_sqe_fill(&sqe, sq, msg, msn);
	trace_msgq_sqe("Sqe submit start", sq->devid, sq->die_id, &sq->que, &sqe);
	ret = msgq_sqe_submit(sq, msg, &sqe, sync);
	if (ret == 0) {
		trace_msgq_sqe("Sqe submit end", sq->devid, sq->die_id, &sq->que, &sqe);
	}
	msgq_sq_unlock(sq);

	if (unlikely(ret != 0)) {
		mctrlq_err("Sq send fail. (ret=%d; devid=%u; die_id=%u; msn=%u)\n", ret, sq->devid, sq->die_id, msn);
	}

	return ret;
}

static int msgq_sq_init(struct msgq_sq *sq)
{
	u32 head = UINT_MAX, tail = UINT_MAX, depth = 0;
	int ret;

	ret = uop_kern_readl(sq->devid, sq->asic.tail, &tail);
	ret |= uop_kern_readl(sq->devid, sq->asic.head, &head);
	ret |= uop_kern_readl(sq->devid, sq->asic.depth, &depth);
	if (unlikely(ret != 0)) {
		mctrlq_err("Sq que read fail. (ret=%d; devid=%u; die_id=%u)\n", ret, sq->devid, sq->die_id);
		return ret;
	}

	if (unlikely((depth <= 1) || (depth > MSGQ_SQ_DEPTH_MAX) || (head >= depth) || (tail >= depth))) {
		mctrlq_err("Sq que invalid. (devid=%u; die_id=%u; depth=%u; head=%u; tail=%u)\n",
			sq->devid, sq->die_id, depth, head, tail);
		return -EINVAL;
	}

	msgq_lock_init(sq);
	sq->que.depth = depth;
	sq->que.tail = tail;
	sq->que.head = head;

	return 0;
}

static void msgq_sq_uninit(struct msgq_sq *sq)
{
	msgq_lock_uninit(sq);
}

static struct msgq_sq *msgq_die_sq_create(u32 devid, u32 die_id, struct msgq_asic *asic)
{
	struct msgq_sq *sq = NULL;

	sq = (struct msgq_sq *)kvzalloc(sizeof(struct msgq_sq), GFP_KERNEL_ACCOUNT);
	if (unlikely(sq == NULL)) {
		mctrlq_err("Sq alloc fail. (devid=%u; die_id=%u; size=%zu)\n", devid, die_id, sizeof(struct msgq_sq));
		return NULL;
	}
	sq->devid = devid;
	sq->die_id = die_id;
	sq->asic = *asic;
	sq->que.pld_size = asic->blk_size;

	return sq;
}

static inline void msgq_sq_destroy_inner(struct msgq_sq *sq)
{
	kvfree(sq);
}

struct msgq_sq *msgq_sq_create(u32 devid, u32 die_id, struct msgq_asic *asic)
{
	struct msgq_sq *sq = NULL;
	int ret;

	sq = msgq_die_sq_create(devid, die_id, asic);
	if (unlikely(sq == NULL)) {
		mctrlq_err("Sq create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return NULL;
	}

	ret = msgq_sq_init(sq);
	if (unlikely(ret != 0)) {
		msgq_sq_destroy_inner(sq);
		mctrlq_err("Sq init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return NULL;
	}

	return sq;
}

void msgq_sq_destroy(struct msgq_sq *sq)
{
	if (likely(sq != NULL)) {
		msgq_sq_uninit(sq);
		msgq_sq_destroy_inner(sq);
	}
}

int msgq_sq_clean(struct msgq_sq *sq)
{
	int ret = 0;
	u32 reg_tail = UINT_MAX;

	msgq_sq_lock(sq);
	ret = uop_kern_readl(sq->devid, sq->asic.tail, &reg_tail);
	rmb(); /* Ensure the uop kern is read correctly. */
	if (unlikely(ret != 0)) {
		goto out;
	}
	if (unlikely(reg_tail >= sq->que.depth)) {
		mctrlq_err("Invalid tail. (devid=%u; die_id=%u; reg_tail=%u; depth=%u)\n",
			sq->devid, sq->die_id, reg_tail, sq->que.depth);
		ret = -EINVAL;
		goto out;
	}
	sq->que.tail = reg_tail;

out:
	msgq_sq_unlock(sq);
	return ret;
}
