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
#undef TRACE_SYSTEM
#define TRACE_SYSTEM msgq

#include <linux/types.h>
#include <linux/tracepoint.h>

#include "msgq_comm.h"

#if !defined(MSGQ_TRACE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define MSGQ_TRACE_EVENT_H

TRACE_EVENT(msgq_sqe,
	TP_PROTO(const char *str, u32 devid, u32 die_id, struct msgq_que *que, struct msgq_sqe *sqe),
	TP_ARGS(str, devid, die_id, que, sqe),
	TP_STRUCT__entry(
		__string(str, str)
		__field(u32, devid)
		__field(u32, die_id)
		__field(u32, tail)
		__field(u32, head)
		__field(u32, depth)
		__field_struct(struct msgq_sqe, sqe)
	),
	TP_fast_assign(
		__assign_str(str, str);
		__entry->devid = devid;
		__entry->die_id = die_id;
		__entry->tail = que->tail;
		__entry->head = que->head;
		__entry->depth = que->depth;
		__entry->sqe = *sqe;
	),
	TP_printk(
		"%s:\tdevid=%u; die_id=%u; tail=%u; head=%u; depth=%u; "
		"task_type=%u; dev_type=%u; p_len=%u; msn=%u; p_addr=0x%x",
		__get_str(str),
		__entry->devid, __entry->die_id,
		__entry->tail, __entry->head, __entry->depth,
		(u32)__entry->sqe.task_type, (u32)__entry->sqe.dev_type,
		(u32)__entry->sqe.p_len, (u32)__entry->sqe.msn, (u32)__entry->sqe.p_addr
	)
);

DECLARE_EVENT_CLASS(msgq_cqe_template,
	TP_PROTO(const char *str, u32 devid, u32 die_id, struct msgq_que *que, struct msgq_cqe *cqe),
	TP_ARGS(str, devid, die_id, que, cqe),
	TP_STRUCT__entry(
		__string(str, str)
		__field(u32, devid)
		__field(u32, die_id)
		__field(u32, tail)
		__field(u32, head)
		__field(u32, depth)
		__field_struct(struct msgq_cqe, cqe)
	),
	TP_fast_assign(
		__assign_str(str, str);
		__entry->devid = devid;
		__entry->die_id = die_id;
		__entry->tail = que->tail;
		__entry->head = que->head;
		__entry->depth = que->depth;
		__entry->cqe = *cqe;
	),
	TP_printk(
		"%s:\tdevid=%u; die_id=%u; tail=%u; head=%u; depth=%u; "
		"task_type=%u; opcode=%u; p_len=%u; msn=%u; status=%u; rq_pi=0x%x",
		__get_str(str),
		__entry->devid, __entry->die_id,
		__entry->tail, __entry->head, __entry->depth,
		(u32)__entry->cqe.task_type, (u32)__entry->cqe.opcode,
		(u32)__entry->cqe.p_len, (u32)__entry->cqe.msn,
		(u32)__entry->cqe.status, (u32)__entry->cqe.rq_pi
	)
);

#define DEFINE_MSGQ_CQE_EVENT(name) \
	DEFINE_EVENT(msgq_cqe_template, name, \
		TP_PROTO(const char *str, u32 devid, u32 die_id, struct msgq_que *que, struct msgq_cqe *cqe), \
		TP_ARGS(str, devid, die_id, que, cqe))

DEFINE_MSGQ_CQE_EVENT(msgq_hw_cqe);
DEFINE_MSGQ_CQE_EVENT(msgq_sw_cqe);
DEFINE_MSGQ_CQE_EVENT(msgq_rq_cqe);

TRACE_EVENT(msgq_rq,
	TP_PROTO(const char *str, u32 devid, u32 die_id, struct msgq_que *que,
		struct msgq_cqe *cqe, u32 index, u32 len),
	TP_ARGS(str, devid, die_id, que, cqe, index, len),
	TP_STRUCT__entry(
		__string(str, str)
		__field(u32, devid)
		__field(u32, die_id)
		__field(u32, tail)
		__field(u32, head)
		__field(u32, depth)
		__field(u32, index)
		__field(u32, len)
		__field_struct(struct msgq_cqe, cqe)
	),
	TP_fast_assign(
		__assign_str(str, str);
		__entry->devid = devid;
		__entry->die_id = die_id;
		__entry->tail = que->tail;
		__entry->head = que->head;
		__entry->depth = que->depth;
		__entry->index = index;
		__entry->len = len;
		__entry->cqe = *cqe;
	),
	TP_printk(
		"%s:\tdevid=%u; die_id=%u; tail=%u; head=%u; depth=%u; "
		"index=%u; len=%u; msn=%u; status=%u; rq_pi=%u; p_len=%u",
		__get_str(str),
		__entry->devid, __entry->die_id,
		__entry->tail, __entry->head, __entry->depth,
		__entry->index, __entry->len,
		(u32)__entry->cqe.msn, (u32)__entry->cqe.status,
		(u32)__entry->cqe.rq_pi, (u32)__entry->cqe.p_len
	)
);

#endif /* MSGQ_TRACE_EVENT_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE msgq_trace
#include <trace/define_trace.h>
