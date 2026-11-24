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
#ifndef CTRLQ_KERN_H
#define CTRLQ_KERN_H

#include <linux/types.h>
#include <linux/idr.h>

#include "kref_safe.h"

#include "mctrlq.h"
#include "ctrlq.h"
#include "mctrlq.h"
#include "mctrlq.h"
#include "mctrlq.h"
struct ctrlq_que {
	u32 tail;
	u32 head;
	u32 depth;
};

struct ctrlq_attr {
	struct ctrlq_asic sq_asic;
	struct ctrlq_asic cq_asic;
};

struct ctrlq_sq {
	struct ctrlq_que que;
	struct ctrlq_asic asic;
	u64 tx;

	u32 num;				/* number of sqe array */
	struct ctrlq_sqe *sqe;  /* Cached sqe array */
	struct mutex lock;

	int on_off;
};

struct ctrlq_cq {
	struct ctrlq_que que;
	struct ctrlq_asic asic;

	bool valid;

	u64 rx;
	struct mutex lock;

	struct ctrlq_cqe *cqe; /* Store first cqe here */
	struct ctrlq_event event;
	struct mami_ctrlq_drop_cqe_event drop_evt; /* drop event publish */
	int on_off;
	u64 fail_time;
	bool fail_flag;
};

struct ctrlq_chan_id {
	u32 devid;
	u32 die_id;
	u32 chan_id;
};

struct ctrlq_chan {
	union {
		struct {
			u32 devid;
			u32 die_id;
			u32 chan_id;
		};
		struct ctrlq_chan_id id;
	};
	struct mutex lock;
	struct idr idr;

	struct ctrlq_sq sq;
	struct ctrlq_cq cq;
	struct mami_kern_ctrlq_schan_handle_ops schan_ops;
};

static inline u32 _ctrlq_get_que_used(struct ctrlq_que *que)
{
	return (que->tail + que->depth - que->head) % que->depth;
}

static inline u32 _ctrlq_get_que_space(struct ctrlq_que *que)
{
	u32 used = _ctrlq_get_que_used(que);

	return que->depth - used - 1;
}

static inline u32 _ctrlq_que_get_tail(struct ctrlq_que *que, u32 num)
{
	return (que->tail + num) % que->depth;
}

static inline void _ctrlq_que_set_tail(struct ctrlq_que *que, u32 tail)
{
	que->tail = tail % que->depth;
}

static inline void _ctrlq_que_set_head(struct ctrlq_que *que, u32 head)
{
	que->head = head % que->depth;
}

static inline u32 _ctrlq_que_get_head(struct ctrlq_que *que, u32 num)
{
	return (que->head + num) % que->depth;
}

struct ctrlq_dev {
	u32 devid;
	struct kref_safe ref;
	spinlock_t lock;
	u32 num; /* number of max chan */
	u32 die_num;
	u32 chip_type;
	u32 max_chan_num;
	u32 valid_chan_num;
	enum mami_udie_version udie_version;
	u32 irq_enable;
	struct ctrlq_chan *chan[];
};
static inline struct ctrlq_chan *ctrlq_get_chan(struct ctrlq_dev *ctrlq, u32 chan_id)
{
	return (chan_id < ctrlq->num) ? ctrlq->chan[chan_id] : NULL;
}

int _ctrlq_kern_deque(u32 devid, u32 chan_id);
struct ctrlq_dev *ctrlq_get(u32 devid);
void ctrlq_put(struct ctrlq_dev *ctrlq);

void ctrlq_kern_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv);
int ctrlq_kern_chan_add(u32 devid, u32 die_id, u32 chan_id, struct ctrlq_attr *attr);
int ctrlq_kern_init(u32 devid, u32 chip_type);
void ctrlq_kern_uninit(u32 devid);
int ctrlq_kern_get_info(u32 devid, struct ctrlq_global_info *info);

int ctrlq_kern_get_cq_chan_size(u32 devid, u32 chan_id, u32 *chan_size);
int ctrlq_kern_get_sq_chan_size(u32 devid, u32 chan_id, u32 *chan_size);
int ctrlq_kern_get_sq_chan_que_and_asic(u32 devid, u32 chan_id, struct ctrlq_que *que, struct ctrlq_asic *asic);

int ctrlq_kern_chan_enable(u32 devid, u32 chan_id);
int ctrlq_kern_chan_disable(u32 devid, u32 chan_id);

int ctrlq_kern_irq_unmask(u32 devid);
void ctrlq_kern_irq_mask(u32 devid);

int ctrlq_kern_mod_init(void);

#ifndef EMU_ST
#define CTRLQ_SCHAN_ID	  15U  /* ue17 use chan, mami or lcne, each die */
#else
#define CTRLQ_SCHAN_ID	  0U
#endif
void mami_ctrlq_schan_handle(u32 devid, u32 die_id, void *rsv);
/* CtrlQ s-channel handler function */
int mami_ctrlq_schan_op(u32 devid, u32 die_id, enum mami_kern_schan_op_type type, struct mami_kern_ctrlq_schan_handle_ops *ops);
int ctrlq_schan_check_pub(u32 devid, u32 die_id);
int ctrlq_kern_get_snapshot(u32 devid, void *in, u32 in_len, void *out, u32 out_len);
#endif
