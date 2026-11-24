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
#define TRACE_SYSTEM ctrlq

#include <linux/types.h>
#include <linux/time.h>
#include <linux/tracepoint.h>

#if !defined(CTRLQ_TRACE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define CTRLQ_TRACE_EVENT_H

TRACE_EVENT(chan,
	TP_PROTO(const char *str, u32 devid, u32 chan_id),
	TP_ARGS(str, devid, chan_id),
	TP_STRUCT__entry(
		__string(str, str)
		__field(u32, devid)
		__field(u32, chan_id)
	),
	TP_fast_assign(
		__assign_str(str, str);
		__entry->devid = devid;
		__entry->chan_id = chan_id;
	),
	TP_printk("%s:\t devid=%u; chan_id=%u", __get_str(str), __entry->devid, __entry->chan_id)
);

TRACE_EVENT(irq_top_half,
	TP_PROTO(u32 devid, u64 para),
	TP_ARGS(devid, para),
	TP_STRUCT__entry(
		__field(u32, devid)
		__field(u64, para)
	),
	TP_fast_assign(
		__entry->devid = devid;
		__entry->para = para;
	),
	TP_printk("devid=%u; para=0x%llx", __entry->devid, __entry->para)
);

DECLARE_EVENT_CLASS(ctrlq_qe_template,
	TP_PROTO(struct ctrlq_chan_id *chan, struct ctrlq_que *que, struct ctrlq_msg_head *ctrlq_head),
	TP_ARGS(chan, que, ctrlq_head),
	TP_STRUCT__entry(
		__field(u32, devid)
		__field(u32, dieid)
		__field(u32, chan_id)
		__field(u32, pi)
		__field(u32, ci)
		__field(u32, depth)
		__field_struct(struct ctrlq_msg_head, ctrlq_head)
	),
	TP_fast_assign(
		__entry->devid = chan->devid;
		__entry->dieid = chan->die_id;
		__entry->chan_id = chan->chan_id;
		__entry->pi = que->tail;
		__entry->ci = que->head;
		__entry->depth = que->depth;
		__entry->ctrlq_head = *ctrlq_head
	),
	TP_printk(
		"chan: %u-%u-%u que: %u-%u-%u ctrlq_head: %s",
		__entry->devid, __entry->dieid, __entry->chan_id,
		__entry->pi, __entry->ci, __entry->depth,
		__print_array((u8 *)&__entry->ctrlq_head, sizeof(struct ctrlq_msg_head), sizeof(__u8))
	)
);

#define DEFINE_CTRLQ_QE_EVENT(name) \
	DEFINE_EVENT(ctrlq_qe_template, name,	\
		TP_PROTO(struct ctrlq_chan_id *chan, struct ctrlq_que *que, struct ctrlq_msg_head *ctrlq_head), \
		TP_ARGS(chan, que, ctrlq_head))

DEFINE_CTRLQ_QE_EVENT(prepare_sqe);
DEFINE_CTRLQ_QE_EVENT(sqe);
DEFINE_CTRLQ_QE_EVENT(cqe);
DEFINE_CTRLQ_QE_EVENT(prepare_cqe_event);
DEFINE_CTRLQ_QE_EVENT(send_cqe_event);

#endif /* CTRLQ_TRACE_EVENT_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE ctrlq_trace
#include <trace/define_trace.h>

