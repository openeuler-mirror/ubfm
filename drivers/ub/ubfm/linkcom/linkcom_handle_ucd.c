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
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "ubfabric.h"
#include "linkcom.h"
#include "linkcom_log.h"
#include "linkcom_mng.h"
#include "linkcom_core.h"
#include "linkcom_pkt_parse.h"
#include "ures.h"
#include "ubfm_common.h"

int linkcom_ucd_open_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct linkcom_open_chan_in *kin = (struct linkcom_open_chan_in *)in;
	int *handle = (int *)out;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		linkcom_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)sizeof(*handle)))) {
		linkcom_err("Invalid out. (out_len=%u; size=%lu)\n", out_len, sizeof(*handle));
		return -EINVAL;
	}

	ret = linkcom_core_open_link_pkt(dev_id, kin->chan_id, kin->app_pid, handle);

	return ret;
}


int linkcom_ucd_close_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct linkcom_close_chan_in *kin = (struct linkcom_close_chan_in *)in;
	u32 chan_id = 0;
	int ret = 0;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		linkcom_err("Invalid in. (in_len=%u; size=%zu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	chan_id = UNMASK_LINKCOM_HANDLE_TO_CHANNEL(kin->handle);

	ret = linkcom_core_close_link_pkt(dev_id, chan_id, kin->app_pid);
	if (ret != 0) {
		linkcom_err("linkcom close link pkt failed. (ret=%d; devid=%u; chan_id=%u)\n", ret, dev_id, chan_id);
		return ret;
	}

	return 0;
}


int linkcom_ucd_send_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct linkcom_send_chan_in *kin = (struct linkcom_send_chan_in *)in;
	struct mami_pkt_send_para tmp_para = {0};
	u32 chan_id = 0;
	u64 ec_status = 0;
	bool is_before_ec = false;
	int ret = 0;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		linkcom_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	ret = ures_get_key_value(dev_id, MAMI_LINKCOM_EC_STATUS, &ec_status);
	if (ret != 0) {
		linkcom_err("Get EC status from ures failed. (ret=%d; devid=%u)\n", ret, dev_id);
		return ret;
	}
	is_before_ec = (ec_status == 1);
	if (is_before_ec) {
		linkcom_err("Send operation is not allowed before EC. (devid=%u)\n", dev_id);
		return -E_OPNOTSUPP_ON_DEV;
	}

	if (unlikely(kin->send_para.pkt == NULL)) {
		linkcom_err("pkt buff is null\n");
		return -EINVAL;
	}

	chan_id = UNMASK_LINKCOM_HANDLE_TO_CHANNEL(kin->handle);

	if ((kin->send_para.pktLen == 0) || (kin->send_para.pktLen > LINK_PKT_BUFF_LEN_MAX)) {
		linkcom_err("Invalid pktLen, valid range: (0, %d]. (devid=%u; pktLen=%u)\n", LINK_PKT_BUFF_LEN_MAX, dev_id,
			kin->send_para.pktLen);
		return -EINVAL;
	}

	tmp_para.pkt = kzalloc(kin->send_para.pktLen, GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp_para.pkt == NULL) {
		linkcom_err("alloc pkt buff fail\n");
		return -ENOMEM;
	}
	ret = (int)copy_from_user((void *)tmp_para.pkt, (void *)kin->send_para.pkt, (u64)kin->send_para.pktLen);
	if (ret != 0) {
		linkcom_err("copy_func_from failed. (ret=%d)\n", ret);
		ret = -EFAULT;
		goto send_pkt_exit;
	}
	tmp_para.lgPortId = kin->send_para.lgPortId;
	tmp_para.cfgType = kin->send_para.cfgType;
	tmp_para.vl = kin->send_para.vl;
	tmp_para.pktLen = kin->send_para.pktLen;

	ret = linkcom_core_send_link_pkt(dev_id, chan_id, tmp_para);
	if (ret != 0) {
		linkcom_err("linkcom send link pkt failed. (ret=%d; devid=%u; chan_id=%u)\n", ret, dev_id, chan_id);
		goto send_pkt_exit;
	}

send_pkt_exit:
	kfree(tmp_para.pkt);
	tmp_para.pkt = NULL;
	return ret;
}


int linkcom_ucd_recv_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct linkcom_recv_chan_in *kin = (struct linkcom_recv_chan_in *)in;
	struct linkcom_recv_chan_out *kout = (struct linkcom_recv_chan_out *)out;
	struct mami_pkt_recv_para tmp_para = {0};
	u32 chan_id;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		linkcom_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)sizeof(*kout)))) {
		linkcom_err("Invalid out. (out_len=%u; size=%lu)\n", out_len, sizeof(*kout));
		return -EINVAL;
	}

	chan_id = UNMASK_LINKCOM_HANDLE_TO_CHANNEL(kin->handle);

	if ((kin->buf_len == 0) || (kin->buf_len > LINK_PKT_BUFF_LEN_MAX)) {
		linkcom_err("Invalid buf_len, valid range: (0, %d]. (devid=%u; buf_len=%u)\n", LINK_PKT_BUFF_LEN_MAX, dev_id,
			kin->buf_len);
		return -EINVAL;
	}

	tmp_para.pktBuff = kzalloc(kin->buf_len, GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp_para.pktBuff == NULL) {
		linkcom_err("alloc pkt buff fail\n");
		return -ENOMEM;
	}
	tmp_para.buffLen = kin->buf_len;

	if (!try_module_get(THIS_MODULE)) {
		ret = -EBUSY;
		goto recv_pkt_exit;
	}
	ret = linkcom_core_recv_link_pkt(dev_id, chan_id, kin->timeout, &tmp_para);
	module_put(THIS_MODULE);
	if (ret != 0) {
		if (ret != -EAGAIN) {
			linkcom_err("linkcom recv link pkt failed. (ret=%d, devid=%u, chanid=%u)\n", ret, dev_id, chan_id);
		}
		goto recv_pkt_exit;
	}
	linkcom_recv_pkt_mask(&tmp_para);
	ret = (int)copy_to_user((void *)kin->pkt_buff, tmp_para.pktBuff, tmp_para.recvLen);
	if (ret != 0) {
		linkcom_err("copy_func_from failed. (ret=%d)\n", ret);
		goto recv_pkt_exit;
	}
	kout->reason = tmp_para.reason;
	kout->port_index = tmp_para.lgPortId;
	kout->recv_len = tmp_para.recvLen;

recv_pkt_exit:
	kfree(tmp_para.pktBuff);
	tmp_para.pktBuff = NULL;
	return ret;
}

int linkcom_ucd_bond_eventfd(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct linkcom_open_chan_in *kin = (struct linkcom_open_chan_in *)in;
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	u32 chan_id = 0;
	u32 chan_num = 0;
	TASK_TIME_TYPE time = 0;
	u32 pid = 0;
	int ret = 0;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		linkcom_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	chan_id = kin->chan_id;
	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		linkcom_err("Dev_id is Invalid. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}
	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("Chan_id is Invalid. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}
	chan_inst = &dev_inst->chan_inst[chan_id];
	mutex_lock(&chan_inst->lock);
	pid = (u32)task_tgid_nr(current);
	time = (TASK_TIME_TYPE)READ_ONCE(current->group_leader->start_time);
	if (chan_inst->app_pid != pid || chan_inst->time != time) {
		linkcom_err("get app_pid failed. (devid=%u; kin.app_pid=%u; chan_inst.app_pid=%u; time:%llu; chan_inst.time:%llu)\n",
			dev_id, kin->app_pid, chan_inst->app_pid, time, chan_inst->time);
		ret = -EINVAL;
		goto bond_eventfd_exit;
	}

	/* check for resource abnormal leakage */
	linkcom_core_chan_eventctx_recycle(chan_inst);
	ret = linkcom_core_bond_eventfd_inner(dev_id, kin, chan_inst);
	if (ret != 0) {
		linkcom_err("Bond eventfd inner failed. (devid=%u; chan_id=%u; eventfd=%d; ret=%d)\n",
			dev_id, chan_id, kin->eventfd, ret);
		goto bond_eventfd_exit;
	}

bond_eventfd_exit:
	mutex_unlock(&chan_inst->lock);
	linkcom_put_dev_inst(dev_id);
	return ret;
}
