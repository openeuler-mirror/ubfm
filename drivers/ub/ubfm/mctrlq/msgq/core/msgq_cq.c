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
#include <linux/delay.h>

#include "mami_string.h"

#include "ued.h"
#include "uop.h"

#include "mctrlq_log.h"
#include "msgq_trace.h"

#include "msgq_cq.h"
#include "msgq_rq.h"
#include "ubfabric.h"
#include "mctrlq.h"
#include "msgq_dispatch.h"
#include "mctrlq.h"
#include "msgq_kern.h"
#include "msgq_comm.h"

#define MSGQ_RCV_OPCODE 1U
#define MSGQ_CQ_INDEX_INVALID 0xFFFFFFFF

enum msgq_cq_status {
	MSGQ_CQ_STATUS_INIT = 0,	/* Init status must be 0 */
	MSGQ_CQ_STATUS_HANDLE,	  /* Get cqe and start to handle rqe */
	MSGQ_CQ_STATUS_FINISH,	  /* Rqe handle finished */
};

static inline bool msgq_cqe_is_req(u8 opcode)
{
	struct msgq_cq_opcode *opcode_ = (struct msgq_cq_opcode *)&opcode;

	return (opcode_->type == 0);
}

static inline bool msgq_cqe_is_msgcode_valid(u8 opcode, u32 type)
{
	struct msgq_cq_opcode *opcode_ = (struct msgq_cq_opcode *)&opcode;

	return ((type != MSGQ_TYPE_PROTOCOL) || ((opcode_->msg_code >= 0) && (opcode_->msg_code < UB_MSG_CODE_MAX)));
}

static inline bool msgq_cqe_is_msgtype_valid(u32 type)
{
	return (type < MSGQ_TYPE_MAX);
}


static inline void msgq_lock_init(struct msgq_cq *cq)
{
	mutex_init(&cq->lock);
}

static inline void msgq_lock_uninit(struct msgq_cq *cq)
{
	mutex_destroy(&cq->lock);
}

static struct msgq_cq *msgq_die_cq_create(u32 devid, u32 die_id, struct msgq_asic *asic)
{
	struct msgq_cq *cq = NULL;

	cq = (struct msgq_cq *)kvzalloc(sizeof(struct msgq_cq), GFP_KERNEL_ACCOUNT);
	if (unlikely(cq == NULL)) {
		mctrlq_err("Cq alloc fail. (devid=%u; size=%zu)\n", devid, sizeof(struct msgq_cq));
		return NULL;
	}
	cq->devid = devid;
	cq->die_id = die_id;
	cq->index = MSGQ_CQ_INDEX_INVALID;
	cq->event.die_id = die_id;

	return cq;
}

static void msgq_cq_destroy_inner(struct msgq_cq *cq)
{
	kvfree(cq);
}

static int msgq_hw_cq_init(struct msgq_cq *cq, struct msgq_asic *asic)
{
	u32 head = UINT_MAX, tail = UINT_MAX, depth = 0;
	int ret;

	ret = uop_kern_readl(cq->devid, asic->tail, &tail);
	ret |= uop_kern_readl(cq->devid, asic->head, &head);
	ret |= uop_kern_readl(cq->devid, asic->depth, &depth);
	if (unlikely(ret != 0)) {
		mctrlq_err("Cq que read fail. (ret=%d; devid=%u)\n", ret, cq->devid);
		return ret;
	}

	if (unlikely((depth <= 1) || (depth > MSGQ_CQ_DEPTH_MAX) || (head >= depth) || (tail >= depth))) {
		mctrlq_err("Cq que invalid. (devid=%u; depth=%u; head=%u; tail=%u)\n", cq->devid, depth, head, tail);
		return -EINVAL;
	}

	msgq_lock_init(cq);
	cq->hw_cq.asic = *asic;
	cq->hw_cq.que.depth = depth;
	cq->hw_cq.que.head = head;
	cq->hw_cq.que.tail = tail;

	return 0;
}

static void msgq_hw_cq_uninit(struct msgq_cq *cq)
{
	msgq_lock_uninit(cq);
}

STATIC struct msgq_cqe *msgq_sw_get_cqe(struct msgq_cq *cq, u32 index)
{
	if (unlikely(index >= cq->sw_cq.que.depth)) {
		mctrlq_err("Invalid index. (devid=%u; die_id=%u; index=%u; depth=%u)\n",
			cq->devid, cq->die_id, index, cq->sw_cq.que.depth);
		return NULL;
	}
	return &cq->sw_cq.cqe[index];
}

static void msgq_sw_cq_init(struct msgq_cq *cq)
{
	(void)ubfm_memset(&cq->sw_cq, sizeof(struct msgq_sw_cq), 0, sizeof(struct msgq_sw_cq));

	cq->sw_cq.que.depth = cq->hw_cq.que.depth;
	cq->sw_cq.que.tail = 0;
	cq->sw_cq.que.head = 0;
}

int msgq_cqe_valid_check(struct msgq_cq *cq, struct msgq_cqe *cqe);
static void msgq_sw_cq_update(struct msgq_cq *cq);

static int msgq_sw_cq_enque(struct msgq_cq *cq, struct msgq_cqe *cqe)
{
	struct msgq_sw_cq *sw_cq = &cq->sw_cq;
	bool is_cqe_valid = true;
	int ret;

	ret = msgq_cqe_valid_check(cq, cqe);
	if (ret != 0) {
		is_cqe_valid = false;
		mctrlq_err("Cqe check fail, abandon it. (ret=%d; devid=%u; die_id=%u; hw_cq_head=%u)\n",
			ret, cq->devid, cq->die_id, cq->hw_cq.que.head);
	}

	if (likely(!msgq_que_is_full(&sw_cq->que))) {
		u32 tail = sw_cq->que.tail;

		sw_cq->cqe[tail] = *cqe;
		if (!is_cqe_valid) {
			sw_cq->status[tail] = MSGQ_CQ_STATUS_FINISH;
		}
		trace_msgq_sw_cqe("Sw cqe enque", cq->devid, cq->die_id, &sw_cq->que, cqe);
		sw_cq->que.tail = msgq_que_get_tail(&sw_cq->que, 1);
		if (!is_cqe_valid) {
			msgq_sw_cq_update(cq);
		}
		return 0;
	}

	return -ENOSPC;
}

static inline int msgq_match_cqe_with_msg(struct msgq_cqe *cqe, struct msgq_msg *msg, u32 msn)
{
	struct msgq_cq_opcode *cqe_opcode = (struct msgq_cq_opcode *)&cqe->opcode;
	struct msgq_cq_opcode *msg_opcode = (struct msgq_cq_opcode *)&msg->opcode;

	return ((cqe->task_type == msg->type) && (cqe->msn == msn) &&
		(cqe_opcode->msg_code == msg_opcode->msg_code) &&
		(cqe_opcode->sub_msg_code == msg_opcode->sub_msg_code));
}

static int msgq_sw_cq_scan_rsp(struct msgq_cq *cq, struct msgq_msg *msg, u32 msn, u32 *index)
{
	struct msgq_sw_cq *sw_cq = &cq->sw_cq;
	u32 tail = sw_cq->que.tail, head = sw_cq->que.head;
	u32 depth = sw_cq->que.depth;

	while (tail != head) {
		struct msgq_cqe *cqe = &sw_cq->cqe[head];

		if (!msgq_cqe_is_req(cqe->opcode) && msgq_match_cqe_with_msg(cqe, msg, msn)) {
			if (sw_cq->status[head] == MSGQ_CQ_STATUS_INIT) {
				sw_cq->status[head] = MSGQ_CQ_STATUS_HANDLE;
				*index = head;
				trace_msgq_sw_cqe("Sw cqe deque for type", cq->devid, cq->die_id, &cq->sw_cq.que, cqe);
				return 0;
			}
		}
		head = (head + 1) % depth;
	}

	return -EAGAIN;
}

static int msgq_sw_cq_scan_req(struct msgq_cq *cq, u32 *index)
{
	struct msgq_sw_cq *sw_cq = &cq->sw_cq;
	u32 tail = sw_cq->que.tail, head = sw_cq->que.head;
	u32 depth = sw_cq->que.depth;

	while (tail != head) {
		struct msgq_cqe *cqe = &sw_cq->cqe[head];

		if (msgq_cqe_is_req(cqe->opcode)) {
			if (sw_cq->status[head] == MSGQ_CQ_STATUS_INIT) {
				sw_cq->status[head] = MSGQ_CQ_STATUS_HANDLE;
				*index = head;
				trace_msgq_sw_cqe("Sw cqe deque for opcode", cq->devid, cq->die_id, &cq->sw_cq.que, cqe);
				return 0;
			}
		}
		head = (head + 1) % depth;
	}

	return -EAGAIN;
}

static int msgq_sw_cq_finish(struct msgq_sw_cq *sw_cq, u32 index)
{
	if (likely((index < sw_cq->que.depth) && (sw_cq->status[index] == MSGQ_CQ_STATUS_HANDLE))) {
		sw_cq->status[index] = MSGQ_CQ_STATUS_FINISH;
		return 0;
	}
	return -EINVAL;
}

static u32 _msgq_sw_cq_update(struct msgq_cq *cq)
{
	u32 tail = cq->sw_cq.que.tail, head = cq->sw_cq.que.head;
	u32 depth = cq->sw_cq.que.depth;
	u32 num = 0;

	while (tail != head) {
		int ret;

		if (cq->sw_cq.status[head] != MSGQ_CQ_STATUS_FINISH) {
			break;
		}

		ret = msgq_rq_update(cq->rq, msgq_sw_get_cqe(cq, head));
		if (unlikely(ret != 0)) {
			break;
		}

		cq->sw_cq.status[head] = MSGQ_CQ_STATUS_INIT;
		head = (head + 1) % depth;
		num++;
	}
	return num;
}

static void msgq_sw_que_update(struct msgq_cq *cq, u32 num)
{
	cq->sw_cq.que.head = msgq_que_get_head(&cq->sw_cq.que, num);
}

static void msgq_sw_cq_update(struct msgq_cq *cq)
{
	u32 num;

	num = _msgq_sw_cq_update(cq);
	if (num != 0) {
		msgq_sw_que_update(cq, num);
	}
}

static int msgq_hw_cqe_read(struct msgq_cq *cq, struct msgq_cqe *cqe)
{
	u32 head = cq->hw_cq.que.head;
	u64 addr;
	int ret;

	addr = cq->hw_cq.asic.base + sizeof(struct msgq_cqe) * head;
	ret = uop_kern_mem_read(cq->devid, addr, cqe, sizeof(struct msgq_cqe));
	if (unlikely(ret != 0)) {
		mctrlq_err("Cqe read fail. (ret=%d; devid=%u; head=%u)\n", ret, cq->devid, cq->hw_cq.que.head);
		return ret;
	}
	rmb(); /* Ensure the cqe is read correctly. */
	trace_msgq_hw_cqe("Hw cqe recv", cq->devid, cq->die_id, &cq->hw_cq.que, cqe);
	head = msgq_que_get_head(&cq->hw_cq.que, 1);
	ret = uop_kern_writel(cq->devid, cq->hw_cq.asic.head, head);
	if (unlikely(ret != 0)) {
		mctrlq_err("Head write fail. (ret=%d; devid=%u; head=%u)\n", ret, cq->devid, head);
		return ret;
	}
	wmb(); /* Ensure the cq head is written correctly. */

	cq->hw_cq.que.head = head;

	return 0;
}

static int msgq_hw_cq_tail_update(struct msgq_cq *cq)
{
	u32 tail = cq->hw_cq.que.tail;
	int ret;

	ret = uop_kern_readl(cq->devid, cq->hw_cq.asic.tail, &tail);
	if (likely((ret == 0) && (tail < cq->hw_cq.que.depth))) {
		rmb(); /* Ensure the cq tail is read correctly. */
		cq->hw_cq.que.tail = tail;
	}
	return ret;
}

static int msgq_hw_cq_can_recv(struct msgq_cq *cq, u32 *cqe_num)
{
	int ret = 0;
	*cqe_num = msgq_get_que_used(&cq->hw_cq.que);
	if (*cqe_num > 0) {
		return 0;
	}
	ret = msgq_hw_cq_tail_update(cq);
	if (ret != 0) {
		return ret;
	}
	*cqe_num = msgq_get_que_used(&cq->hw_cq.que);
	return 0;
}

static inline int msgq_cqe_status_check(struct msgq_cqe *cqe)
{
	return (cqe->status == ENOTRECOVERABLE) ? -EINVAL : 0;
}

static void msgq_cq_print(struct msgq_cq *cq, u32 head, struct msgq_cqe *cqe)
{
	mctrlq_err("Cqe[dev=%u:die=%u]:head=%u;type=%d;opcode=%d;p_len=%d;msn=%d;status=%d\n",
		cq->devid, cq->die_id, head, cqe->task_type, cqe->opcode, cqe->p_len, cqe->msn, cqe->status);
}

static void msgq_hw_cq_show(struct msgq_cq *cq)
{
	u32 tail = cq->hw_cq.que.tail, head = cq->hw_cq.que.head;
	u32 depth = cq->hw_cq.que.depth;
	u64 addr;

	mctrlq_err("Hw cq show[dev=%u:die=%u:depth=%u:tail=%u:head=%u]:\n", cq->devid, cq->die_id, depth, tail, head);
	while (tail != head) {
		struct msgq_cqe cqe;
		int ret;

		addr = cq->hw_cq.asic.base + sizeof(struct msgq_cqe) * head;
		ret = uop_kern_mem_read(cq->devid, addr, &cqe, sizeof(struct msgq_cqe));
		if (unlikely(ret != 0)) {
			mctrlq_err("Read hw cqe fail. (ret=%d; devid=%u; die_id=%u; head=%u; depth=%u)\n",
				ret, cq->devid, cq->die_id, head, depth);
			return;
		}
		msgq_cq_print(cq, head, &cqe);
		head = (head + 1) % depth;
	}
}

static void msgq_sw_cq_show(struct msgq_cq *cq)
{
	u32 tail = cq->sw_cq.que.tail, head = cq->sw_cq.que.head;
	u32 depth = cq->sw_cq.que.depth;

	mctrlq_err("Sw cq show[dev=%u:die=%u:depth=%u:tail=%u:head=%u]:\n", cq->devid, cq->die_id, depth, tail, head);
	while (tail != head) {
		struct msgq_cqe *cqe = &cq->sw_cq.cqe[head];

		msgq_cq_print(cq, head, cqe);
		head = (head + 1) % depth;
	}
}

static void msgq_cq_show(struct msgq_cq *cq, enum msgq_type type, u32 msn)
{
	msgq_cq_lock(cq);
	mctrlq_err("Cq show[dev=%u:die=%u:type=%u:msn=%u]:\n", cq->devid, cq->die_id, type, msn);
	msgq_hw_cq_show(cq);
	msgq_sw_cq_show(cq);
	msgq_cq_unlock(cq);
}

static int msgq_check_cqe_rsp_valid(struct msgq_cq *cq, struct msgq_cqe *cqe)
{
	unsigned long *addr;
	u32 find;
	struct msgq_dev *mdev = NULL;
	struct msgq_chan *chan = NULL;

	mdev = msgq_get(cq->devid);
	if (unlikely(mdev == NULL)) {
		mctrlq_err("Msgq get mdev fail. (devid=%u; die_id=%u)\n", cq->devid, cq->die_id);
		return -ENODEV;
	}
	chan = msgq_get_chan(mdev, cq->die_id);
	if (unlikely(chan == NULL)) {
		mctrlq_err("Msgq get chan fail. (devid=%u; die_id=%u)\n", cq->devid, cq->die_id);
		msgq_put(mdev);
		return -EINVAL;
	}
	addr = chan->bitmaps[cqe->task_type].bitmap;
	find = (cqe->msn + MSN_SIZE - 1) % MSN_SIZE;
	if (test_bit(find, addr) == 0) {
		msgq_put(mdev);
		return -EINVAL;
	}
	msgq_put(mdev);
	return 0;
}

int msgq_cqe_valid_check(struct msgq_cq *cq, struct msgq_cqe *cqe)
{
	struct msgq_cq_opcode *op = (struct msgq_cq_opcode *)&cqe->opcode;
	int ret;

	ret = msgq_cqe_status_check(cqe);
	if (ret != 0) {
		mctrlq_err("MSGQ msgq_cqe_status_check fail, abandon it. (ret=%d; cqe_status=%d; cqe_task_type=%u; cqe_msn=%u; "
				"cqe_type=%u; cqe_msg_code=%u; cqe_sub_msg_code=%u)\n",
				ret, cqe->status, cqe->task_type, cqe->msn, op->type, op->msg_code, op->sub_msg_code);
		return -EINVAL;
	}
	if (!msgq_cqe_is_msgcode_valid(cqe->opcode, cqe->task_type) || !msgq_cqe_is_msgtype_valid(cqe->task_type)) {
		mctrlq_err("MSGQ cqe check fail, abandon it. (cqe_status=%d; cqe_task_type=%u; cqe_msn=%u; cqe_type=%u; "
				"cqe_msg_code=%u; cqe_sub_msg_code=%u)\n",
				cqe->status, cqe->task_type, cqe->msn, op->type, op->msg_code, op->sub_msg_code);
		return -EINVAL;
	}
	if (!msgq_cqe_is_req(cqe->opcode)) {
		ret = msgq_check_cqe_rsp_valid(cq, cqe);
		if (ret != 0) {
			mctrlq_err("MSGQ msgq_check_cqe_rsp_valid fail, abandon it. (ret=%d; cqe_status=%d; cqe_task_type=%u; "
					"cqe_msn=%u; cqe_type=%u; cqe_msg_code=%u; cqe_sub_msg_code=%u)\n",
					ret, cqe->status, cqe->task_type, cqe->msn, op->type, op->msg_code, op->sub_msg_code);
			return -EINVAL;
		}
	}

	return 0;
}

static int msgq_cq_cache(struct msgq_cq *cq, u32 *enque_num)
{
	int ret = 0;
	u32 cqe_num = 0; /* Number of pending cqes in the current cq hw */
	*enque_num = 0;  /* Number of cqes enqueued into cq sw */
	while ((ret = msgq_hw_cq_can_recv(cq, &cqe_num)) == 0) {
		struct msgq_cqe cqe;

		if (cqe_num == 0) {
			break;
		}
		ret = msgq_hw_cqe_read(cq, &cqe);
		if (unlikely(ret != 0)) {
			mctrlq_err_limit("Hw cqe read fail. (ret=%d; devid=%u; die_id=%u)\n", ret, cq->devid, cq->die_id);
			break;
		}
		ret = msgq_sw_cq_enque(cq, &cqe);
		if (unlikely(ret != 0)) {
			mctrlq_err("Sw enque fail. (ret=%d; devid=%u; die_id=%u; sw_cq_depth=%u; sw_cq_head=%u; sw_cq_tail=%u; enque_num=%u)\n",
				ret, cq->devid, cq->die_id, cq->sw_cq.que.depth, cq->sw_cq.que.head, cq->sw_cq.que.tail, *enque_num);
			break;
		}
		*enque_num = *enque_num + 1;
	}
	return ret;
}

STATIC int msgq_cq_poll_opcode(struct msgq_cq *cq, u32 *index)
{
	int ret;
	u32 enque_num = 0;

	/* Scan sw cq */
	ret = msgq_sw_cq_scan_req(cq, index);
	if (ret == 0) {
		return ret;
	}

	/* No proper cqe in sw cq. Read cqe from hw cq and cache it to sw cq */
	ret = msgq_cq_cache(cq, &enque_num);
	if (ret != 0) {
		return ret;
	}
	if (enque_num == 0) {
		return -ENOSPC;
	}
	/* Scan sw cq again */
	return msgq_sw_cq_scan_req(cq, index);
}

static int msgq_cq_poll_type(struct msgq_cq *cq, struct msgq_msg *msg, u32 msn, u32 *index)
{
	int ret;
	u32 enque_num = 0;
	/* Scan sw cq */
	ret = msgq_sw_cq_scan_rsp(cq, msg, msn, index);
	if (ret == 0) {
		return ret;
	}

	/* No proper cqe in sw cq. Read cqe from hw cq and cache it to sw cq */
	ret = msgq_cq_cache(cq, &enque_num);
	if (ret != 0) {
		return ret;
	}
	if (enque_num == 0) {
		return -ENOSPC;
	}
	/* Scan sw cq again */
	return msgq_sw_cq_scan_rsp(cq, msg, msn, index);
}

STATIC int _msgq_cq_get_timeout(struct msgq_msg *msg)
{
	int timeout = MSGQ_CQ_TIMEOUT;
	struct upgrade_req_msg_header *req_msg_header = NULL;
	struct vdm_pkt_header *vdm_hdr = NULL;
	u32 opcode, sub_opcode;
	bool is_upgrade_msg;
	struct msg_pkt_header_kernel *pkt_header = NULL;

	/* Only PROTOCOL messages carry a msg_pkt_header_kernel; PRIV messages use a private payload format. */
	if ((msg->type != MSGQ_TYPE_PROTOCOL) || (msg->req_len < sizeof(struct msg_pkt_header_kernel))) {
		return timeout;
	}

	pkt_header = (struct msg_pkt_header_kernel *)msg->req_data;
	if (pkt_header->msgetah.msg_code == UB_MSG_CODE_VDM) {
		if (msg->req_len < sizeof(struct upgrade_req_msg_header) + sizeof(struct vdm_pkt_header)) {
			return timeout;
		}
		req_msg_header = (struct upgrade_req_msg_header *)msg->req_data;
		vdm_hdr = (struct vdm_pkt_header *)req_msg_header->payload;
		opcode = vdm_hdr->opcode;
		sub_opcode = vdm_hdr->sub_opcode;
		is_upgrade_msg = ((opcode == VDM_VENDER_OPCODE_FM_TO_NODE) && (sub_opcode == VDM_FM_TO_NODE_UPGRADE));
		timeout = is_upgrade_msg ? MSGQ_CQ_TIMEOUT_UPGRADE : MSGQ_CQ_TIMEOUT;
	}

	return timeout;
}

static int _msgq_cq_wait(struct msgq_chan *chan, struct msgq_cq *cq, struct msgq_msg *msg, u32 msn, u32 *index)
{
	int ret;
	u8 rsp_status = 0;
	int timeout = _msgq_cq_get_timeout(msg);
	enum msgq_type type = msg->type;
	struct msg_pkt_header_kernel *pkt_header = NULL;

	/* Only PROTOCOL messages carry a msg_pkt_header_kernel; PRIV messages use a private payload format. */
	if ((type == MSGQ_TYPE_PROTOCOL) && (msg->req_len >= sizeof(struct msg_pkt_header_kernel))) {
		pkt_header = (struct msg_pkt_header_kernel *)msg->req_data;
		rsp_status = pkt_header->msgetah.rsp_status;
	}

	if (rsp_status == EOPNOTSUPP) {
		mctrlq_err("Msgq msg ack not_support. (devid=%u; dieId=%u; msn=%u)\n", cq->devid, cq->die_id, msn);
		return -EOPNOTSUPP;
	}

	do {
		msgq_cq_lock(cq);
		ret = msgq_cq_poll_type(cq, msg, msn, index);
		msgq_cq_unlock(cq);
		if (ret == 0) {
			return 0;
		}
		usleep_range(MSGQ_CQ_SLEEP_TIME, MSGQ_CQ_SLEEP_TIME);
		timeout -= MSGQ_CQ_SLEEP_TIME;
	} while (timeout > 0);

	msgq_cq_lock(cq);
	ret = msgq_cq_poll_type(cq, msg, msn, index);
	if (ret != 0) {
		msgq_msn_put(chan, type, msn);
	}
	msgq_cq_unlock(cq);

	return (ret == 0) ? 0 : -EAGAIN;
}

static int msgq_cq_done(struct msgq_cq *cq, u32 index)
{
	int ret;

	ret = msgq_sw_cq_finish(&cq->sw_cq, index);
	if (likely(ret == 0)) {
		msgq_sw_cq_update(cq);
	}

	return ret;
}

static void msgq_cq_read_reg(struct msgq_cq *cq, u32 *head, u32 *tail)
{
	msgq_cq_lock(cq);
	msgq_cq_read_head(cq, head);
	msgq_cq_read_tail(cq, tail);
	msgq_cq_unlock(cq);
}

static void print_timeout_record(struct msgq_chan *chan, u32 msn, struct msgq_record *record)
{
	u32 sq_head = 0, sq_tail = 0, cq_head = 0, cq_tail = 0;
	u32 rq_head = 0, rq_tail = 0;

	msgq_sq_lock(chan->sq);
	msgq_cq_lock(chan->cq);

	msgq_sq_read_head(chan->sq, &sq_head);
	msgq_sq_read_tail(chan->sq, &sq_tail);
	msgq_cq_read_head(chan->cq, &cq_head);
	msgq_cq_read_tail(chan->cq, &cq_tail);
	msgq_rq_read_head(chan->cq->rq, &rq_head);
	msgq_rq_read_tail(chan->cq->rq, &rq_tail);

	msgq_cq_unlock(chan->cq);
	msgq_sq_unlock(chan->sq);

	mctrlq_err("Msgq wait timeout. (devid=%u;die_id=%u;msn=%u;"
		"before sq_head=%u; after sq_head=%u;"
		"before sq_tail=%u; after sq_tail=%u;"
		"before cq_head=%u; after cq_head=%u;"
		"before cq_tail=%u; after cq_tail=%u;"
		"before rq_head=%u; after rq_head=%u;"
		"before rq_tail=%u; after rq_tail=%u;)\n", chan->devid, chan->die_id, msn,
		record->sq_head, sq_head, /* sq head unchanged, indicating imp did not read data */
		record->sq_tail, sq_tail,
		record->cq_head, cq_head,
		record->cq_tail, cq_tail, /* cq tail unchanged, indicating imp did not send data */
		record->rq_head, rq_head,
		record->rq_tail, rq_tail  /* rq tail unchanged, indicating imp did not write rsp data */
	);
}

/* Cq wait with type & msn */
int msgq_cq_wait(struct msgq_chan *chan, struct msgq_msg *msg, u32 msn, struct msgq_record *record)
{
	u32 index;
	int ret;
	struct msgq_cqe *cqe = NULL;
	struct msgq_cq *cq = chan->cq;
	u32 prv_head = 0, prv_tail = 0, cur_head = 0, cur_tail = 0;
	struct msgq_que prv_sw_que = cq->sw_cq.que, prv_hw_que = cq->hw_cq.que;

	msgq_cq_read_reg(cq, &prv_head, &prv_tail);

	ret = _msgq_cq_wait(chan, cq, msg, msn, &index);
	if (ret == -EAGAIN) {
		print_timeout_record(chan, msn, record);
	}
	if (unlikely(ret != 0)) {
		msgq_cq_show(cq, msg->type, msn);
		return ret;
	}

	msgq_cq_lock(cq);
	cqe = msgq_sw_get_cqe(cq, index);
#ifndef EMU_ST
	if (cqe == NULL) {
		mctrlq_err("Get cqe is null. index=%u\n", index);
		msgq_cq_unlock(cq);
		return -ENODEV;
	}
#endif

	ret = msgq_rqe_get(cq->rq, cqe, msg->rep_data, msg->rep_len, NULL);
	if (unlikely(ret != 0)) {
		mctrlq_err("Get rqe failed. (devid=%d; die_id=%d; index=%u; ret=%d)\n", cq->devid, cq->die_id, index, ret);
	}
	(void)msgq_cq_done(cq, index);
	msgq_cq_unlock(cq);
	if (unlikely(cqe->status != 0) && likely(msg->disable_log == 0)) {
		msgq_cq_read_reg(cq, &cur_head, &cur_tail);
		mctrlq_err("Get err cqe.(devid=%u;die_id=%u;"
			"prv_reg=[%u:%u];prv_sw_que=[%u:%u];prv_hw_que=[%u:%u];cur_reg=[%u:%u];cur_sw_que=[%u:%u];cur_hw_que=[%u:%u]"
			"task_type=%u;msn=%u;cqe_opcode=%u;status=%u;rq_pi=%u;p_len=%u;msg_opcod=%u;msg_type=%u)",
			cq->devid, cq->die_id, prv_head, prv_tail, prv_sw_que.head, prv_sw_que.tail, prv_hw_que.head, prv_hw_que.tail,
			cur_head, cur_tail, cq->sw_cq.que.head, cq->sw_cq.que.tail, cq->hw_cq.que.head, cq->hw_cq.que.tail,
			cqe->task_type, cqe->msn, cqe->opcode, cqe->status, cqe->rq_pi, cqe->p_len, msg->opcode, msg->type);
	}
	return (ret == 0) ? cqe->status : ret;
}

static u64 get_timestamp_ns(void)
{
	return ktime_get_ns();
}

STATIC int msgq_cq_event_pack(struct msgq_cq *cq)
{
	struct msgq_event *event = &cq->event; /* Wq is single thread, No need to add lock here */
	struct msgq_cqe *cqe = NULL;
	u32 index;
	u32 event_len = 0; /* Use aligned local var: event->len is misaligned due to __packed */
	int ret;

	if (cq->index != MSGQ_CQ_INDEX_INVALID) {
		return 0;
	}

	ret = msgq_cq_poll_opcode(cq, &index);
	if (ret != 0) {
		return ret;
	}
	cqe = msgq_sw_get_cqe(cq, index);
	if (cqe == NULL) {
		return -EACCES;
	}
	event->msn = cqe->msn;
	ret = msgq_rqe_get(cq->rq, cqe, event->data, MSGQ_EVENT_LEN, &event_len);
	if (unlikely(ret != 0)) {
		mctrlq_err("Get rqe failed. (devid=%d; die_id=%d; index=%u; ret=%d)\n", cq->devid, cq->die_id, index, ret);
	}
	(void)msgq_cq_done(cq, index);

	cq->index = MSGQ_CQ_INDEX_INVALID;
	if (ret != 0) {
		return ret;
	}

	/* Skip sending event to userspace when there's no rqe data */
	if (event_len == 0) {
		return -EAGAIN;
	}

	event->len = event_len;
	event->timestamp_ns = get_timestamp_ns();
	cq->index = index;

	return 0;
}

int msgq_cq_event_send(struct msgq_cq *cq)
{
	struct ued_published_event ued_event = {0};
	struct msgq_event *event = &cq->event;
	struct msg_pkt *pkt = NULL;
	struct mami_port_event *lk_msg = NULL;
	int ret;

	ret = msgq_to_event_id(cq, &ued_event);
	if (unlikely(ret != 0)) {
		mctrlq_err("Msgq to event id failed. (ret=%d)\n", ret);
		return -EINVAL;
	}

	if (ued_event.event_id == UED_EVENT_PORT_MSGQ_MSG) {
		pkt = (struct msg_pkt *)(event->data);
		lk_msg = (struct mami_port_event *)pkt->payload;
		lk_msg->die_id = event->die_id;
	}

	ued_event.msg = (u8 *)event;
	ued_event.msg_len = event->len + offsetof(struct msgq_event, data);
	ret = ued_submit_event_to_dev(cq->devid, &ued_event);

	return ret;
}

/* Cq wait with opcode */
int msgq_cq_deque(struct msgq_cq *cq)
{
	int ret;

	msgq_cq_lock(cq);
	ret = msgq_cq_event_pack(cq);
	if (ret == 0) {
		ret = msgq_cq_event_send(cq);
		cq->index = MSGQ_CQ_INDEX_INVALID;
	}
	msgq_cq_unlock(cq);
	return ret;
}

int msgq_cq_clean(struct msgq_cq *cq)
{
	u32 i = 0;
	int ret = 0;
	u32 reg_tail = UINT_MAX;

	msgq_cq_lock(cq);
	/* Clean hw_cq */
	ret = uop_kern_readl(cq->devid, cq->hw_cq.asic.tail, &reg_tail);
	rmb(); /* Ensure thecq tail is read correctly. */
	if (unlikely(ret != 0)) {
		goto out;
	}
	if (reg_tail >= cq->sw_cq.que.depth) {
		mctrlq_err("Msgq cq tail is invalid. (devid=%u; die_id=%u; tail=%u; depth=%u)\n", cq->devid, cq->die_id, reg_tail, cq->sw_cq.que.depth);
		ret = -EINVAL;
		goto out;
	}
	ret = uop_kern_writel(cq->devid, cq->hw_cq.asic.head, reg_tail);
	wmb(); /* Ensure the cq head is written correctly. */
	if (unlikely(ret != 0)) {
		goto out;
	}
	cq->hw_cq.que.head = reg_tail;
	cq->hw_cq.que.tail = reg_tail;
	/* Clean sw_cq */
	cq->sw_cq.que.head = 0;
	cq->sw_cq.que.tail = 0;
	for (i = 0; i < cq->sw_cq.que.depth; i++) {
		cq->sw_cq.status[i] = MSGQ_CQ_STATUS_INIT;
	}
	/* Clean rq */
	ret = msgq_rq_clean(cq->rq);
out:
	msgq_cq_unlock(cq);
	return ret;
}

static int msgq_cq_init(struct msgq_cq *cq, struct msgq_asic *asic)
{
	int ret;

	ret = msgq_hw_cq_init(cq, asic);
	if (unlikely(ret != 0)) {
		mctrlq_err("Hw cq init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, cq->devid, cq->die_id);
		return ret;
	}

	msgq_sw_cq_init(cq);
	return ret;
}

static void msgq_cq_uninit(struct msgq_cq *cq)
{
	msgq_hw_cq_uninit(cq);
}

struct msgq_cq *msgq_cq_create(u32 devid, u32 die_id, struct msgq_asic *cq_asic, struct msgq_asic *rq_asic)
{
	struct msgq_cq *cq = NULL;
	int ret;

	cq = msgq_die_cq_create(devid, die_id, cq_asic);
	if (unlikely(cq == NULL)) {
		mctrlq_err("Cq create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return NULL;
	}

	ret = msgq_cq_init(cq, cq_asic);
	if (unlikely(ret != 0)) {
		mctrlq_err("Cq init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		goto err_cq_init;
	}

	cq->rq = msgq_rq_create(devid, die_id, rq_asic);
	if (unlikely(cq->rq == NULL)) {
		mctrlq_err("Rq create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		goto err_rq_init;
	}

	return cq;

err_rq_init:
	msgq_cq_uninit(cq);
err_cq_init:
	msgq_cq_destroy_inner(cq);
	return NULL;
}

void msgq_cq_destroy(struct msgq_cq *cq)
{
	if (likely(cq != NULL)) {
		if (likely(cq->rq != NULL)) {
			msgq_rq_destroy(cq->rq);
			cq->rq = NULL;
		}
		msgq_cq_uninit(cq);
		msgq_cq_destroy_inner(cq);
	}
}

