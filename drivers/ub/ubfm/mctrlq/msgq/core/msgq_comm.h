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
#ifndef MSGQ_COMM_H
#define MSGQ_COMM_H

#include <linux/types.h>
#include <linux/mutex.h>

#include "mctrlq.h"
#include "msgq_pub.h"
#include "uop.h"

#define MSGQ_SQ_DEPTH_MAX	   2048U
#define MSGQ_SQ_PLD_SIZE		2048U
#define MSGQ_SRAM_SQ_PLD_SIZE   256U

struct msgq_sq {
	u32 devid;
	u32 die_id;
	struct msgq_que que;
	struct msgq_asic asic;
	struct mutex lock;
};

struct msgq_record {
	u32 devid;
	u32 die_id;
	enum msgq_type type;
	u32 msn;
	u32 sq_head;
	u32 sq_tail;
	u32 cq_head;
	u32 cq_tail;
	u32 rq_head;
	u32 rq_tail;
};

#define MSGQ_CQ_DEPTH_MAX	  2048U

struct msgq_sw_cq {
	struct msgq_que que;
	u8 status[MSGQ_CQ_DEPTH_MAX];
	struct msgq_cqe cqe[MSGQ_CQ_DEPTH_MAX];
};

struct msgq_hw_cq {
	struct msgq_que que;
	struct msgq_asic asic;
};

struct msgq_cq {
	u32 devid;
	u32 die_id;
	struct msgq_sw_cq sw_cq; /* Cache cqe */
	struct msgq_hw_cq hw_cq;

	struct msgq_rq *rq;
	struct mutex lock;

	u32 index; /* Recv index */
	struct msgq_event event;
};

#define MSGQ_RQ_DEPTH_MAX	   2048U

struct msgq_rq {
	u32 devid;
	u32 die_id;
	struct msgq_que que;
	struct msgq_asic asic;
	u32 blk_size;
};

#define MSN_SIZE 65535

struct msn_bitmap {
	unsigned long bitmap[BITS_TO_LONGS(MSN_SIZE)];
};

struct msgq_chan {
	u32 devid;
	u32 die_id;

	struct mutex lock[MSGQ_TYPE_MAX];
	u16 msn[MSGQ_TYPE_MAX];
	struct msn_bitmap bitmaps[MSGQ_TYPE_MAX];

	struct msgq_sq *sq;
	struct msgq_cq *cq;
	struct msgq_asic sq_asic;
	struct msgq_asic cq_asic;
	struct msgq_asic rq_asic;
};

static inline void msgq_cq_lock(struct msgq_cq *cq)
{
	mutex_lock(&cq->lock);
}

static inline void msgq_cq_unlock(struct msgq_cq *cq)
{
	mutex_unlock(&cq->lock);
}

static inline void msgq_sq_lock(struct msgq_sq *sq)
{
	mutex_lock(&sq->lock);
}

static inline void msgq_sq_unlock(struct msgq_sq *sq)
{
	mutex_unlock(&sq->lock);
}

static inline void msgq_sq_read_head(struct msgq_sq *sq, u32 *head)
{
	(void)uop_kern_readl(sq->devid, sq->asic.head, head);
}

static inline void msgq_sq_read_tail(struct msgq_sq *sq, u32 *tail)
{
	(void)uop_kern_readl(sq->devid, sq->asic.tail, tail);
}

static inline void msgq_cq_read_head(struct msgq_cq *cq, u32 *head)
{
	(void)uop_kern_readl(cq->devid, cq->hw_cq.asic.head, head);
}

static inline void msgq_cq_read_tail(struct msgq_cq *cq, u32 *tail)
{
	(void)uop_kern_readl(cq->devid, cq->hw_cq.asic.tail, tail);
}

static inline void msgq_rq_read_head(struct msgq_rq *rq, u32 *head)
{
	(void)uop_kern_readl(rq->devid, rq->asic.head, head);
}

static inline void msgq_rq_read_tail(struct msgq_rq *rq, u32 *tail)
{
	(void)uop_kern_readl(rq->devid, rq->asic.tail, tail);
}

#endif
