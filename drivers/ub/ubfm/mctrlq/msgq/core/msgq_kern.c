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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include "mami_string.h"

#include "kref_safe.h"

#include "uop.h"
#include "mctrlq_log.h"
#include "msgq_sq.h"
#include "msgq_cq.h"
#include "msgq.h"
#include "fmdevm.h"
#include "ures.h"
#include "udm.h"
#include "ulog.h"
#include "ubfabric.h"
#define CREATE_TRACE_POINTS
#include "msgq_trace.h"
#include "msgq_kern.h"
#include "mctrlq.h"

static struct msgq_dev *g_msgq[DEV_MAX_NUM];
static rwlock_t g_dev_lock[DEV_MAX_NUM];

static void msn_bitmap_init(struct msgq_chan *chan, enum msgq_type type)
{
	(void)ubfm_memset(&chan->bitmaps[type], sizeof(chan->bitmaps[type]), 0, sizeof(chan->bitmaps[type]));
}

static struct msgq_chan *_msgq_chan_create(u32 devid, u32 die_id)
{
	struct msgq_chan *chan = NULL;
	enum msgq_type type;

	chan = kvzalloc(sizeof(struct msgq_chan), GFP_KERNEL_ACCOUNT);
	if (chan == NULL) {
		mctrlq_err("Chan alloc fail. (devid=%u; die_id=%u; size=%zu)\n", devid, die_id, sizeof(struct msgq_chan));
		return NULL;
	}

	for (type = MSGQ_TYPE_PROTOCOL; type < MSGQ_TYPE_MAX; type++) {
		msn_bitmap_init(chan, type);
		mutex_init(&chan->lock[type]);
	}
	chan->devid = devid;
	chan->die_id = die_id;

	return chan;
}

static void _msgq_chan_destroy(struct msgq_chan *chan)
{
	enum msgq_type type;

	for (type = MSGQ_TYPE_PROTOCOL; type < MSGQ_TYPE_MAX; type++) {
		mutex_destroy(&chan->lock[type]);
	}
	kvfree(chan);
}

static int msgq_chan_init(struct msgq_chan *chan, struct msgq_attr *attr)
{
	u32 devid = chan->devid, die_id = chan->die_id;
	struct msgq_sq *sq = NULL;
	struct msgq_cq *cq = NULL;

	sq = msgq_sq_create(devid, die_id, &attr->sq);
	if (sq == NULL) {
		mctrlq_err("Msgq sq create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -ENOMEM;
	}

	cq = msgq_cq_create(devid, die_id, &attr->cq, &attr->rq);
	if (cq == NULL) {
		msgq_sq_destroy(sq);
		mctrlq_err("Msgq cq create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -ENOMEM;
	}
	chan->sq = sq;
	chan->cq = cq;
	chan->sq_asic = attr->sq;
	chan->cq_asic = attr->cq;
	chan->rq_asic = attr->rq;

	return 0;
}

static void msgq_chan_uninit(struct msgq_chan *chan)
{
	if (chan->sq != NULL) {
		msgq_sq_destroy(chan->sq);
		chan->sq = NULL;
	}
	if (chan->cq != NULL) {
		msgq_cq_destroy(chan->cq);
		chan->cq = NULL;
	}
}

static struct msgq_chan *msgq_chan_create(u32 devid, u32 die_id, struct msgq_attr *attr)
{
	struct msgq_chan *chan = NULL;
	int ret;

	chan = _msgq_chan_create(devid, die_id);
	if (chan == NULL) {
		mctrlq_err("Msgq chan create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return NULL;
	}

	ret = msgq_chan_init(chan, attr);
	if (ret != 0) {
		_msgq_chan_destroy(chan);
		mctrlq_err("Msgq chan init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return NULL;
	}

	return chan;
}

static void msgq_chan_destroy(struct msgq_chan *chan)
{
	if (chan != NULL) {
		msgq_chan_uninit(chan);
		_msgq_chan_destroy(chan);
	}
}

static int msgq_chan_add(struct msgq_dev *msgq, struct msgq_chan *chan)
{
	mutex_lock(&msgq->lock);
	if (msgq->chan[chan->die_id] != NULL) {
		mutex_unlock(&msgq->lock);
		return -ENODEV;
	}
	msgq->chan[chan->die_id] = chan;
	mutex_unlock(&msgq->lock);

	return 0;
}

static struct msgq_dev *msgq_create(u32 devid)
{
	struct msgq_dev *msgq = NULL;
	u32 die_id;

	msgq = kvmalloc(sizeof(struct msgq_dev), GFP_KERNEL_ACCOUNT);
	if (msgq == NULL) {
		mctrlq_err("Msgq alloc fail. (devid=%u; size=%zu)\n", devid, sizeof(struct msgq_dev));
		return NULL;
	}

	for (die_id = 0; die_id < DIE_MAX_NUM; die_id++) {
		msgq->chan[die_id] = NULL;
	}

	msgq->devid = devid;
	mutex_init(&msgq->lock);
	kref_safe_init(&msgq->ref);

	return msgq;
}

static void msgq_destroy(struct msgq_dev *msgq)
{
	u32 die_id;

	for (die_id = 0; die_id < DIE_MAX_NUM; die_id++) {
		if (msgq->chan[die_id] != NULL) {
			msgq_chan_destroy(msgq->chan[die_id]);
			msgq->chan[die_id] = NULL;
		}
	}
	kvfree(msgq);
}

static void msgq_release(struct kref_safe *kref_s)
{
	struct msgq_dev *mdev = container_of(kref_s, struct msgq_dev, ref);

	msgq_destroy(mdev);
}

struct msgq_dev *msgq_get(u32 devid)
{
	struct msgq_dev *msgq = NULL;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return NULL;
	}

	read_lock_bh(&g_dev_lock[devid]);
	msgq = g_msgq[devid];
	if (likely(msgq != NULL)) {
		if (kref_safe_get_unless_zero(&msgq->ref) == 0) {
			read_unlock_bh(&g_dev_lock[devid]);
			return NULL;
		}
	}
	read_unlock_bh(&g_dev_lock[devid]);

	return msgq;
}

void msgq_put(struct msgq_dev *msgq)
{
	kref_safe_put(&msgq->ref, msgq_release);
}

int msgq_kern_chan_add(u32 devid, u32 die_id, struct msgq_attr *attr)
{
	struct msgq_dev *msgq = NULL;
	struct msgq_chan *chan = NULL;
	int ret = -ENOMEM;

	msgq = msgq_get(devid);
	if (msgq == NULL) {
		return -ENODEV;
	}

	chan = msgq_chan_create(devid, die_id, attr);
	if (chan == NULL) {
		mctrlq_err("Chan create fail. (devid=%u; die_id=%u)\n", devid, die_id);
		goto out;
	}

	ret = msgq_chan_add(msgq, chan);
	if (unlikely(ret != 0)) {
		msgq_chan_destroy(chan);
		mctrlq_err("Chan add fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
	}
out:
	msgq_put(msgq);
	return ret;
}

static int msgq_add(struct msgq_dev *msgq)
{
	u32 devid = msgq->devid;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}

	write_lock_bh(&g_dev_lock[devid]);
	if (g_msgq[devid] != NULL) {
		write_unlock_bh(&g_dev_lock[devid]);
		return -ENODEV;
	}
	g_msgq[devid] = msgq;
	write_unlock_bh(&g_dev_lock[devid]);

	return 0;
}

static void msgq_del(u32 devid)
{
	write_lock_bh(&g_dev_lock[devid]);
	if (likely(g_msgq[devid] != NULL)) {
		g_msgq[devid] = NULL;
	}
	write_unlock_bh(&g_dev_lock[devid]);
}

struct msgq_chan *msgq_get_chan(struct msgq_dev *mdev, u32 die_id)
{
	if (unlikely(die_id >= DIE_MAX_NUM)) {
		return NULL;
	}
	return mdev->chan[die_id];
}

static int msgq_msn_get(struct msgq_chan *chan, enum msgq_type type, u32 *msn)
{
	unsigned long find, tmp, size, *addr;
	int ret = 0;

	if (unlikely(type >= MSGQ_TYPE_MAX)) {
		return -EINVAL;
	}

	find = chan->msn[type];
	addr = chan->bitmaps[type].bitmap;

	mutex_lock(&chan->lock[type]);
	if (unlikely(test_bit(find, addr))) {
		size = MSN_SIZE;
		tmp = find;
		find = find_next_zero_bit(addr, size, find);
#ifndef EMU_ST
		if (unlikely(find == size)) {
			size = tmp;
			find = find_next_zero_bit(addr, size, 0);
			if (unlikely(find == size)) {
				mctrlq_err("Msn alloc fail. (devid=%u; die_id=%u)\n", chan->devid, chan->die_id);
				ret = -EPERM;
				goto out;
			}
		}
#endif
	}

	set_bit(find, addr);
	chan->msn[type] = (find + 1) % MSN_SIZE;
	*msn = find + 1; /* find bit is 0~65534, ret use 1~65535 */
out:
	mutex_unlock(&chan->lock[type]);
	return ret;
}

void msgq_msn_put(struct msgq_chan *chan, enum msgq_type type, u32 msn)
{
	unsigned long *addr;

	mutex_lock(&chan->lock[type]);
	addr = chan->bitmaps[type].bitmap;
	clear_bit(msn - 1, addr);
	mutex_unlock(&chan->lock[type]);
}

static int msgq_chan_send(struct msgq_chan *chan, struct msgq_msg *msg)
{
	enum msgq_type type = msg->type;
	int ret;
	u32 msn = 0;
	u32 prv_head = 0, prv_tail = 0, cur_head = 0, cur_tail = 0;
	struct msgq_que prv_que = chan->sq->que;
	struct msgq_record record = {0};

	ret = msgq_msn_get(chan, type, &msn);
	if (unlikely(ret != 0)) {
		mctrlq_err("Msn alloc failed. (ret=%d; devid=%u; die_id=%u)\n", ret, chan->devid, chan->die_id);
		return ret;
	}

	msgq_sq_read_reg(chan->sq, &prv_head, &prv_tail);

	ret = msgq_sq_send(chan, msg, msn, true, &record);
	if (unlikely(ret != 0)) {
		if (likely(msg->disable_log == 0)) {
			mctrlq_err("Msgq send failed. (ret=%d; devid=%u; die_id=%u; msn=%u)\n", ret, chan->devid,
				chan->die_id, msn);
		}
		msgq_msn_put(chan, type, msn);
		goto out;
	}

	ret = msgq_cq_wait(chan, msg, msn, &record);
	if (ret != -EAGAIN) {
		msgq_msn_put(chan, type, msn);	/* When the CQE wait times out, release the MSN directly within _msgq_cq_wait */
	}

out:
	if (unlikely(ret != 0) && likely(msg->disable_log == 0)) {
		msgq_sq_read_reg(chan->sq, &cur_head, &cur_tail);
		mctrlq_err("Msgq chan send/wait failed. (ret=%d;devid=%u;die_id=%u;"
			"prv_reg=[%u:%u];prv_que=[%u:%u];cur_reg=[%u:%u];cur_que=[%u:%u];type=%u;msn=%u;opcode=%u)\n",
			ret, chan->devid, chan->die_id, prv_head, prv_tail, prv_que.head, prv_que.tail,
			cur_head, cur_tail, chan->sq->que.head, chan->sq->que.tail, msg->type, msn, msg->opcode);
	}

	return ret;
}

static int msgq_chan_send_async(struct msgq_chan *chan, u16 msn, struct msgq_msg *msg)
{
	struct msgq_record record = {0};
	int ret;

	ret = msgq_sq_send(chan, msg, msn, false, &record);
	if (unlikely(ret != 0)) {
		mctrlq_err("Msgq send fail. (ret=%d; devid=%u; die_id=%u; msn=%u)\n", ret, chan->devid, chan->die_id, msn);
	}
	return ret;
}

static inline int msgq_chan_deque(struct msgq_chan *chan)
{
	return msgq_cq_deque(chan->cq);
}

int msgq_msg_check(u32 devid, u32 die_id, struct msgq_msg *msg)
{
	if (unlikely(msg == NULL)) {
		mctrlq_err("Msg is NULL. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	if (unlikely(msg->type >= MSGQ_TYPE_MAX)) {
		mctrlq_err("Invalid type. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	if (unlikely((msg->req_data == NULL) || (msg->type == MSGQ_TYPE_PROTOCOL && msg->req_len < sizeof(struct msg_pkt_header_kernel)))) {
		mctrlq_err("Req data invalid. (devid=%u; die_id=%u; req_is_null=%d; cur_req_len=%u; exp_req_len=%zu)\n",
			devid, die_id, msg->req_data == NULL, msg->req_len, sizeof(struct msg_pkt_header_kernel));
		return -EINVAL;
	}

	return 0;
}

int msgq_kern_send(u32 devid, u32 die_id, struct msgq_msg *msg)
{
	struct msgq_chan *chan = NULL;
	struct msgq_dev *mdev = NULL;
	int ret = -ENODEV;
	struct udm_dev_para para = {0};

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		mctrlq_err("Udm_get_dev_attr fail. (ret=%d; devid=%u)\n", ret, devid);
	}

	if (ret == 0 && para.chip_type == MAMI_KERN_TYPE_1825) {
		return uop_kern_send_recv_msg(devid, para.connect_type, (void *)msg);
	}

	if (msgq_msg_check(devid, die_id, msg)) {
		mctrlq_err("Msg check fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	mdev = msgq_get(devid);
	if (unlikely(mdev == NULL)) {
		return -ENODEV;
	}

	chan = msgq_get_chan(mdev, die_id);
	if (unlikely(chan == NULL)) {
		mctrlq_err("Msgq get chan fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		msgq_put(mdev);
		return -EINVAL;
	}
	ret = msgq_chan_send(chan, msg);
	msgq_put(mdev);

	if (unlikely(ret != 0)) {
		mctrlq_err("Msgq send fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(msgq_kern_send);

int msgq_kern_i3c_send(u32 dev_id, void *user_ctx)
{
	struct msgq_msg *msg = (struct msgq_msg *)user_ctx;
	struct msg_pkt_header_kernel *req_data = NULL;
	drv_ubnic_xfer_msg i3c_msg = { 0 };
	int ret = 0;
	u32 die_id = 0;

	if (msgq_msg_check(dev_id, die_id, msg)) {
		mctrlq_err("Msg check fail. (devid=%u; die_id=%u)\n", dev_id, die_id);
		return -EINVAL;
	}
	if (msg->rep_data == NULL || msg->rep_len <= 0) {
		mctrlq_err("Resp data invalid. (devid=%u; die_id=%u; resp_is_null=%d; cur_resp_len=%u;)\n",
			dev_id, die_id, msg->rep_data == NULL, msg->rep_len);
		return -EINVAL;
	}
	req_data = (struct msg_pkt_header_kernel *)(msg->req_data);
	mctrlq_debug("Send msg by i3c. (dev_id=%u, sdie_id=%u, local=%u, scna=%u, dcna=%u)\n", dev_id, die_id, msg->local, req_data->nth.scna, req_data->nth.dcna);
	i3c_msg.op = PCIE_I3C_UB_MESSAGE;
	i3c_msg.write_len = msg->req_len;
	i3c_msg.read_len = msg->rep_len;
	i3c_msg.write_buf = msg->req_data;
	i3c_msg.read_buf = msg->rep_data;
	ret = uop_i3c_send_recv_kern(dev_id, &i3c_msg);
	if (ret != 0) {
		mctrlq_err("i3c Msg send fail. (ret=%d; devid=%u; die_id=%u)\n", ret, dev_id, die_id);
		return -EINVAL;
	}
	return 0;
}

int msgq_dpu_register_intf(u32 devid, struct udev_hw_identify *hw)
{
	int ret;

	if (hw->connect_type == UDEV_PCIE_I3C) {
		mctrlq_info("reg i3c intf. (devid=%u; chip_type=%u)\n", devid, hw->chip_type);
		ret = uop_kern_register_msg_ops(devid, msgq_kern_i3c_send);
		if (ret != 0) {
			mctrlq_err("Reg msg ops fail. (ret=%d; devid=%u)\n", ret, devid);
			return ret;
		}
	} else {
		/* For 1825 ub, no msgq */
		mctrlq_info("Msgq shouldn't be init. (devid=%u; chip_type=%u)\n", devid, hw->chip_type);
	}

	return 0;
}

int mgmtq_kern_send(u32 sdev_id, void *user_ctx)
{
	struct msgq_msg *msg = (struct msgq_msg *)user_ctx;
	struct msg_pkt_header_kernel *req_data = NULL;
	int ret = 0;
	u32 sdie_id = 0;
	u64 cna_eid = 0;
	char cna_eid_key[URES_NAME_LEN];
	/* send msg to local device */
	if (msgq_msg_check(sdev_id, sdie_id, msg)) {
		mctrlq_err("Msg check fail. (devid=%u; die_id=%u)\n", sdev_id, sdie_id);
		return -EINVAL;
	}
	(void)UBFM_SPRINTF(cna_eid_key, sizeof(cna_eid_key), MAMI_UDEV_DIEN_CNA_EID, 0);
	ret = ures_get_key_value(sdev_id, cna_eid_key, &cna_eid);
	if (ret != 0) {
		mctrlq_info("Can not get cna_eid. (devid=%u; ret=%d)\n", sdev_id, ret);
		return ret;
	}
	req_data = (struct msg_pkt_header_kernel *)msg->req_data;
	req_data->nth.scna = (u32)(cna_eid >> 32);  /* 32: High 32bit for cna; */
	msg->local = 0;
	mctrlq_debug("Send msg by mgmtq. (sdev_id=%u, sdie_id=%u, local=%u, scna=%u, dcna=%u)\n", sdev_id, sdie_id, msg->local, req_data->nth.scna, req_data->nth.dcna);
	return msgq_kern_send(sdev_id, sdie_id, msg);
}

int msgq_kern_send_async(u32 devid, u32 die_id, u16 msn, struct msgq_msg *msg)
{
	struct msgq_chan *chan = NULL;
	struct msgq_dev *mdev = NULL;
	int ret = -ENODEV;

	if (msgq_msg_check(devid, die_id, msg)) {
		mctrlq_err("Msg check fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	mdev = msgq_get(devid);
	if (unlikely(mdev == NULL)) {
		return -ENODEV;
	}

	chan = msgq_get_chan(mdev, die_id);
	if (likely(chan != NULL)) {
		ret = msgq_chan_send_async(chan, msn, msg);
	}
	msgq_put(mdev);

	if (unlikely(ret != 0)) {
		mctrlq_err("Msgq send fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
	}

	return ret;
}

/* Read the cq.depth register and memory via the kernel uop, and determine whether the current data path (e.g., cdma) is normal based on the return value */
int msgq_kern_chan_check(u32 devid, u32 die_id)
{
	struct msgq_chan *chan = NULL;
	struct msgq_dev *mdev = NULL;
	u32 hw_cq_depth = 0;
	int ret = 0;

	mdev = msgq_get(devid);
	if (unlikely(mdev == NULL)) {
		mctrlq_err("Invalid msgq devid. (devid=%u; die_id=%u)\n", devid, die_id);
		return -ENODEV;
	}
	chan = msgq_get_chan(mdev, die_id);
	if (unlikely(chan == NULL)) {
		mctrlq_err("Invalid msgq channel. (devid=%u; die_id=%u)\n", devid, die_id);
		ret = -EINVAL;
		goto out;
	}
	ret = uop_kern_readl(devid, chan->cq->hw_cq.asic.depth, &hw_cq_depth);
	if (unlikely((ret != 0))) {
		ret = -EIO;
		goto out;
	}

out:
	msgq_put(mdev);
	return ret;
}

int msgq_kern_deque(u32 devid, u32 die_id)
{
	struct msgq_chan *chan = NULL;
	struct msgq_dev *mdev = NULL;
	int ret = -ENODEV;

	mdev = msgq_get(devid);
	if (unlikely(mdev == NULL)) {
		return -ENODEV;
	}

	chan = msgq_get_chan(mdev, die_id);
	if (likely(chan != NULL)) {
		ret = msgq_chan_deque(chan);
	}
	msgq_put(mdev);

	return ret;
}

int msgq_update_when_recover(u32 devid, u32 die_id)
{
	int ret = 0;
	struct msgq_dev *mdev = NULL;
	struct msgq_chan *chan = NULL;

	mdev = msgq_get(devid);
	if (mdev == NULL) {
		mctrlq_err("Msgq get mdev fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -ENODEV;
	}
	chan = msgq_get_chan(mdev, die_id);
	if (chan == NULL) {
		msgq_put(mdev);
		mctrlq_err("Msgq get chan fail. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}
	ret = msgq_sq_clean(chan->sq);
	if (unlikely(ret != 0)) {
		goto out;
	}
	ret = msgq_cq_clean(chan->cq); /* rq is also cleaned up via msgq_cq_clean */

out:
	msgq_put(mdev);
	return ret;
}

int msgq_kern_init(u32 devid)
{
	struct msgq_dev *msgq = NULL;
	int ret;
	struct udev_hw_identify hw = {0};

	msgq = msgq_create(devid);
	if (msgq == NULL) {
		mctrlq_err("Msgq create fail. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	ret = msgq_add(msgq);
	if (ret != 0) {
		msgq_destroy(msgq);
		mctrlq_err("Msgq add fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	/* Register msg ops to uop */
	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		msgq_del(msgq->devid);
		msgq_put(msgq);
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (hw.location == UDM_LOCAL) {
		ret = uop_kern_register_msg_ops(devid, mgmtq_kern_send);
		if (ret != 0) {
			msgq_del(msgq->devid);
			msgq_put(msgq);
			mctrlq_err("Reg msg ops fail. (ret=%d; devid=%u)\n", ret, devid);
			return ret;
		}
	}
	return ret;
}

void msgq_kern_uninit(u32 devid)
{
	struct msgq_dev *msgq = NULL;

	/* msgq_kern_init() registered mgmtq_kern_send() into uop, undo it. */
	(void)uop_kern_unregister_msg_ops(devid);

	msgq = msgq_get(devid);
	if (msgq == NULL) {
		return;
	}
	msgq_del(devid);
	msgq_put(msgq);
	msgq_put(msgq);
}

static void _msgq_sq_show(struct msgq_chan *chan, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	struct msgq_asic *asic = &chan->sq_asic;
	u32 head = 0, tail = 0, depth = 0;
	struct msgq_sqe sqe;
	u64 addr;
	int ret;

	show(priv, "  Msgq Sq show:\n");
	show(priv, "    Sq asic: base=0x%llx;head=0x%llx;tail=0x%llx;depth=0x%llx\n",
		asic->base, asic->head, asic->tail, asic->depth);
	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (ret == 0) {
		show(priv, "    Sq reg: head=%u;tail=%u;depth=%u\n", head, tail, depth);
	}
	show(priv, "    Sq que: head=%u;tail=%u;depth=%u;pld_size=%u\n",
		chan->sq->que.head, chan->sq->que.tail, chan->sq->que.depth, chan->sq->que.pld_size);

	addr = asic->base + sizeof(struct msgq_sqe) * tail;
	ret = uop_kern_mem_read(chan->devid, addr, &sqe, sizeof(struct msgq_sqe));
	if (ret == 0) {
		show(priv, "    Sq sqe: task_type=%u;local=%u;dev_type=%u;opcode=%u;msn=%u;p_addr=0x%x;",
			sqe.task_type, sqe.local, sqe.dev_type, sqe.opcode, sqe.msn, sqe.p_addr);
		show(priv, "p_len=%u\n", sqe.p_len);
	}
}

static void _msgq_cq_show(struct msgq_chan *chan, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	struct msgq_asic *asic = &chan->cq_asic;
	u32 head = 0, tail = 0, depth = 0;
	struct msgq_cqe cqe;
	u64 addr;
	int ret;

	show(priv, "  Msgq Cq show:\n");
	show(priv, "    Cq asic: base=0x%llx;head=0x%llx;tail=0x%llx;depth=0x%llx\n",
		asic->base, asic->head, asic->tail, asic->depth);
	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (ret == 0) {
		show(priv, "    Cq reg: head=%u;tail=%u;depth=%u\n", head, tail, depth);
	}
	show(priv, "    Cq hw_cq: head=%u;tail=%u;depth=%u\n",
			chan->cq->hw_cq.que.head, chan->cq->hw_cq.que.tail, chan->cq->hw_cq.que.depth);
	show(priv, "    Cq sw_cq: head=%u;tail=%u;depth=%u\n",
		chan->cq->sw_cq.que.head, chan->cq->sw_cq.que.tail, chan->cq->sw_cq.que.depth);

	addr = asic->base + sizeof(struct msgq_cqe) * head;
	ret = uop_kern_mem_read(chan->devid, addr, &cqe, sizeof(struct msgq_cqe));
	if (ret == 0) {
		show(priv, "    Cq cqe: task_type=%u;opcode=%u;p_len=%u;msn=%u;rq_pi=%u;status=%u\n",
			cqe.task_type, cqe.opcode, cqe.p_len, cqe.msn, cqe.rq_pi, cqe.status);
	}
}

static void _msgq_rq_show(struct msgq_chan *chan, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 head = 0, tail = 0, depth = 0, blk_size = 0;
	struct msgq_asic *asic = &chan->rq_asic;
	int ret;

	show(priv, "  Msgq Rq show:\n");
	show(priv, "    Rq asic: base=0x%llx;head=0x%llx;tail=0x%llx;depth=0x%llx;blk_size=0x%llx\n",
		asic->base, asic->head, asic->tail, asic->depth, asic->blk_size);
	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	ret |= uop_kern_readl(chan->devid, asic->blk_size, &blk_size);
	if (ret == 0) {
		show(priv, "    Rq reg: head=%u;tail=%u;depth=%u;blk_size=%u\n", head, tail, depth, blk_size);
	}
	show(priv, "    Rq que: head=%u;tail=%u;depth=%u\n", chan->cq->rq->que.head, chan->cq->rq->que.tail, chan->cq->rq->que.depth);
}

static void _msgq_show(struct msgq_chan *chan, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	show(priv, "Msgq show:[dev-%u;die-%u]\n", chan->devid, chan->die_id);
	_msgq_sq_show(chan, show, priv);
	_msgq_cq_show(chan, show, priv);
	_msgq_rq_show(chan, show, priv);
}

void msgq_kern_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	struct msgq_dev *msgq = NULL;

	msgq = msgq_get(devid);
	if (msgq != NULL) {
		u32 die_id;

		for (die_id = 0; die_id < DIE_MAX_NUM; die_id++) {
			struct msgq_chan *chan = msgq->chan[die_id];

			if (chan != NULL) {
				_msgq_show(chan, show, priv);
			}
		}
		msgq_put(msgq);
	}
}

static int msgq_sq_get_snapshot(struct msgq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	struct msgq_asic *asic = &chan->sq_asic;
	u32 head = 0, tail = 0, depth = 0;
	struct msgq_sqe sqe;
	u64 addr;
	int ret, ret_snap;

	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, " Msgq Sq show:\n");

	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (ret == 0) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Sq reg: head=%u;tail=%u;depth=%u\n", head, tail, depth);
	}
	ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Sq que: head=%u;tail=%u;depth=%u;pld_size=%u\n",
		chan->sq->que.head, chan->sq->que.tail, chan->sq->que.depth, chan->sq->que.pld_size);

	addr = asic->base + sizeof(struct msgq_sqe) * tail;
	ret = uop_kern_mem_read(chan->devid, addr, &sqe, sizeof(struct msgq_sqe));
	if (ret == 0) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Sq sqe: task_type=%u;local=%u;dev_type=%u;opcode=%u;msn=%u;p_addr=0x%x;",
			sqe.task_type, sqe.local, sqe.dev_type, sqe.opcode, sqe.msn, sqe.p_addr);
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "p_len=%u\n", sqe.p_len);
	}

	if (ret_snap != 0) {
		mctrlq_warn("Msgq snapshot buffer lack of space. (devid=%u;ret=%d)\n", chan->devid, ret_snap);
		return ret_snap;
	}

	return 0;
}

static int msgq_cq_get_snapshot(struct msgq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	struct msgq_asic *asic = &chan->cq_asic;
	u32 head = 0, tail = 0, depth = 0;
	struct msgq_cqe cqe;
	u64 addr;
	int ret, ret_snap;

	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, " Msgq Cq show:\n");

	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	if (ret == 0) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq reg: head=%u;tail=%u;depth=%u\n", head, tail, depth);
	}
	ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq hw_cq: head=%u;tail=%u;depth=%u\n",
			chan->cq->hw_cq.que.head, chan->cq->hw_cq.que.tail, chan->cq->hw_cq.que.depth);
	ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq sw_cq: head=%u;tail=%u;depth=%u\n",
		chan->cq->sw_cq.que.head, chan->cq->sw_cq.que.tail, chan->cq->sw_cq.que.depth);

	addr = asic->base + sizeof(struct msgq_cqe) * head;
	ret = uop_kern_mem_read(chan->devid, addr, &cqe, sizeof(struct msgq_cqe));
	if (ret == 0) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Cq cqe: task_type=%u;opcode=%u;p_len=%u;msn=%u;rq_pi=%u;status=%u\n",
			cqe.task_type, cqe.opcode, cqe.p_len, cqe.msn, cqe.rq_pi, cqe.status);
	}

	if (ret_snap != 0) {
		mctrlq_warn("Msgq snapshot buffer lack of space. (devid=%u;ret=%d)\n", chan->devid, ret_snap);
		return ret_snap;
	}

	return 0;
}

static int msgq_rq_get_snapshot(struct msgq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	u32 head = 0, tail = 0, depth = 0, blk_size = 0;
	struct msgq_asic *asic = &chan->rq_asic;
	int ret, ret_snap;

	ret_snap = ulog_kern_dump_snapshot(buf, buf_len, read_size, " Msgq Rq show:\n");

	ret = uop_kern_readl(chan->devid, asic->head, &head);
	ret |= uop_kern_readl(chan->devid, asic->tail, &tail);
	ret |= uop_kern_readl(chan->devid, asic->depth, &depth);
	ret |= uop_kern_readl(chan->devid, asic->blk_size, &blk_size);
	if (ret == 0) {
		ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Rq reg: head=%u;tail=%u;depth=%u;blk_size=%u\n", head, tail, depth, blk_size);
	}

	ret_snap |= ulog_kern_dump_snapshot(buf, buf_len, read_size, "  Rq que: head=%u;tail=%u;depth=%u\n", chan->cq->rq->que.head, chan->cq->rq->que.tail, chan->cq->rq->que.depth);
	if (ret_snap != 0) {
		mctrlq_warn("Msgq snapshot buffer lack of space. (devid=%u;ret=%d)\n", chan->devid, ret_snap);
		return ret_snap;
	}

	return 0;
}

static int msgq_chan_get_snapshot(struct msgq_chan *chan, char **buf, u32 *buf_len, u32 *read_size)
{
	int ret;

	ret = ulog_kern_dump_snapshot(buf, buf_len, read_size, "\nMsgq show:[dev-%u;die-%u]\n", chan->devid, chan->die_id);
	if (ret != 0) {
		mctrlq_warn("Msgq snapshot buffer lack of space. (devid=%u)\n", chan->devid);
		return ret;
	}

	ret = msgq_sq_get_snapshot(chan, buf, buf_len, read_size);
	ret |= msgq_cq_get_snapshot(chan, buf, buf_len, read_size);
	ret |= msgq_rq_get_snapshot(chan, buf, buf_len, read_size);
	if (ret != 0) {
		mctrlq_warn("Msgq chan get snapshot failed. (devid=%u;die_id=%d;ret=%d)\n", chan->devid, chan->die_id, ret);
		return ret;
	}

	return 0;
}

#define SNAPSHOT_MSGQ_MAX_OUTPUT_LEN (64 * 1024)
static int msgq_kern_snapshot_para_check(u32 devid, struct msgq_get_snapshot_in *para)
{
	if (devid >= DEV_MAX_NUM) {
		mctrlq_warn("Devid is invalid. (devid=%u; max_devid=%u)\n", devid, DEV_MAX_NUM - 1);
		return -EINVAL;
	}

	if (para == NULL) {
		mctrlq_warn("Msgq_get_snapshot_in para is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (para->buf == NULL) {
		mctrlq_warn("Msgq snapshot para invalid: buff is NULL. (devid=%u;buf_len=%u)\n", devid, para->buf_len);
		return -EINVAL;
	}

	if (para->read_size == NULL) {
		mctrlq_warn("Msgq snapshot para invalid: read_size is NULL. (devid=%u;buf_len=%u)\n", devid, para->buf_len);
		return -EINVAL;
	}

	if ((para->buf_len == 0) || (para->buf_len > SNAPSHOT_MSGQ_MAX_OUTPUT_LEN)) {
		mctrlq_warn("Msgq snapshot para invalid: buf_len invalid. (devid=%u;buf_len=%u;max_buf_len=%u)\n",
			devid, para->buf_len, SNAPSHOT_MSGQ_MAX_OUTPUT_LEN);
		return -EINVAL;
	}

	return 0;
}

static int msgq_copy_snapshot_to_user(u32 devid, void *buf_user, void *buf_kern, void *read_size_addr, u32 read_size)
{
	int ret;

	ret = (int)copy_to_user(buf_user, buf_kern, read_size);
	if (ret != 0) {
		mctrlq_warn("Msgq copy buf to user failed. (devid=%u,ret=%d)\n", devid, ret);
		return ret;
	}

	ret = (int)copy_to_user(read_size_addr, (void *)&read_size, sizeof(u32));
	if (ret != 0) {
		mctrlq_warn("Msgq copy read_size to user failed. (devid=%u,ret=%d)\n", devid, ret);
		return ret;
	}

	return 0;
}

int msgq_kern_get_snapshot(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct msgq_get_snapshot_in *para = (struct msgq_get_snapshot_in *)in;
	struct msgq_dev *msgq = NULL;
	u32 buf_len = 0, read_size = 0;
	char *buf = NULL, *buf_start;
	u32 die_id;
	int ret;

	if (in_len != sizeof(struct msgq_get_snapshot_in)) {
		mctrlq_warn("Msgq snapshot para in_len is invalid. (devid=%u; in_len=%u; valid_in_len=%u)\n", devid, in_len,
			(u32)sizeof(struct msgq_get_snapshot_in));
		return -EINVAL;
	}

	ret = msgq_kern_snapshot_para_check(devid, para);
	if (ret != 0) {
		mctrlq_warn("Msgq snapshot para invalid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	buf_len = para->buf_len;
	buf = (char *)kzalloc(buf_len, GFP_KERNEL | __GFP_ACCOUNT);
	if (buf == NULL) {
		mctrlq_warn("Msgq kzalloc snapshot buf failed. (devid=%u)\n", devid);
		return -ENOSPC;
	}
	buf_start = buf;

	msgq = msgq_get(devid);
	if (unlikely(msgq == NULL)) {
		mctrlq_warn("Msgq is NULL. (devid=%u)\n", devid);
		(void)ulog_kern_dump_snapshot(&buf, &buf_len, &read_size, "Msgq is NULL. (devid=%u)\n", devid);
		goto out;
	}

	for (die_id = 0; die_id < DIE_MAX_NUM; die_id++) {
		struct msgq_chan *chan = msgq->chan[die_id];

		if (chan == NULL) {
			continue;
		}
		ret = msgq_chan_get_snapshot(chan, &buf, &buf_len, &read_size);
		if (ret != 0) {
			mctrlq_warn("Msgq chan get snapshot failed. (devid=%u, die_id=%u)\n", chan->devid, die_id);
			break;
		}
	}
	msgq_put(msgq);

out:
	ret = msgq_copy_snapshot_to_user(devid, (void *)para->buf, (void *)buf_start, (void *)para->read_size, read_size);
	kfree(buf_start);

	return ret;
}

int msgq_kern_mod_init(void)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		rwlock_init(&g_dev_lock[devid]);
	}
	return 0;
}

