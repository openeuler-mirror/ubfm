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
#ifndef MSGQ_PUB_H
#define MSGQ_PUB_H

#include <linux/types.h>

#include "ubfm_common.h"

struct msgq_que {
	u32 tail;
	u32 head;
	u32 depth;
	u32 pld_size; /* Only used for SQ */
};

struct msgq_asic {
	u64 base;
	u64 tail;
	u64 head;
	u64 depth;
	u64 status;
	u64 int_mask;
	u64 blk_size; /* Rq: used as addr. Sq: used as value */
};

static inline u32 msgq_get_que_used(struct msgq_que *que)
{
	return ((que->tail + que->depth - que->head) % que->depth);
}

static inline u32 msgq_get_que_space(struct msgq_que *que)
{
	u32 used = msgq_get_que_used(que);

	return (que->depth - used - 1);
}

static inline u32 msgq_que_get_tail(struct msgq_que *que, u32 num)
{
	return (que->tail + num) % que->depth;
}

static inline u32 msgq_que_get_head(struct msgq_que *que, u32 num)
{
	return (que->head + num) % que->depth;
}

static inline bool msgq_que_is_full(struct msgq_que *que)
{
	return ((que->tail + 1) % que->depth) == que->head;
}

static inline bool msgq_que_is_empty(struct msgq_que *que)
{
	return (que->tail == que->head);
}

#endif
