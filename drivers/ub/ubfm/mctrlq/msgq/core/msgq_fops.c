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

#include "feature_loader.h"

#include "ucd.h"
#include "ubfabric.h"
#include "ubfabric.h"

#include "msgq_kern.h"
#include "mctrlq_log.h"
#include "msgq_fops.h"

#define MSGQ_FOPS_ADD_CMD(s, p, f) {  \
	.main_cmd		 = UCD_CMD_MSGQ, \
	.sub_cmd		  = s,			\
	.proc_ctrl_bitmap = p,			\
	.func			 = f			 \
}

struct msgq_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

static int _msgq_fops_msg_init(u32 devid, struct msgq_msg *msg, struct msgq_cmd_send *send)
{
	void *req_data = NULL;

	*msg = (struct msgq_msg) {0};
	if (unlikely((send->msg.req_data == NULL) || (send->msg.req_len == 0) || (send->msg.req_len > PAGE_SIZE))) {
		mctrlq_err("Invalid msg. (devid=%u; die_id=%u; req_len=%u)\n",
			devid, send->die_id, send->msg.req_len);
		return -EINVAL;
	}

	req_data = kvzalloc(send->msg.req_len, GFP_KERNEL_ACCOUNT);
	if (unlikely(req_data == NULL)) {
		mctrlq_err("Req data alloc fail. (devid=%u; die_id=%u; req_len=%u)\n",
			devid, send->die_id, send->msg.req_len);
		return -ENOMEM;
	}

	if ((send->msg.rep_data != NULL) && (send->msg.rep_len > 0) && (send->msg.rep_len <= PAGE_SIZE)) {
		void *rep_data = kvzalloc(send->msg.rep_len, GFP_KERNEL_ACCOUNT);

		if (unlikely(rep_data == NULL)) {
			kvfree(req_data);
			mctrlq_err("Rep data alloc fail. (devid=%u; die_id=%u; req_len=%u)\n",
				devid, send->die_id, send->msg.rep_len);
			return -ENOMEM;
		}
		msg->rep_data = rep_data;
		msg->rep_len = send->msg.rep_len;
	}

	msg->type = send->msg.type;
	msg->opcode = send->msg.opcode;
	msg->local = send->msg.local;
	msg->dev_type = send->msg.dev_type;
	msg->disable_log = send->msg.disable_log;
	msg->req_data = req_data;
	msg->req_len = send->msg.req_len;

	return 0;
}

static void _msgq_fops_msg_uninit(struct msgq_msg *msg)
{
	if (msg->req_data != NULL) {
		kvfree(msg->req_data);
		msg->req_data = NULL;
		msg->rep_len = 0;
	}
	if (msg->rep_data != NULL) {
		kvfree(msg->rep_data);
		msg->rep_data = NULL;
		msg->rep_len = 0;
	}
}

static int _msgq_fops_msg_u2k(u32 devid, struct msgq_msg *msg, struct msgq_cmd_send *send)
{
	if (copy_from_user(msg->req_data, (const void __user *)send->msg.req_data, send->msg.req_len)) {
		mctrlq_err("Msg u2k copy fail. (devid=%u; die_id=%u; req_len=%u)\n",
			devid, send->die_id, send->msg.req_len);
		return -EFAULT;
	}
	return 0;
}

static int _msgq_fops_msg_k2u(u32 devid, struct msgq_msg *msg, struct msgq_cmd_send *send)
{
	if ((msg->rep_data != NULL) && (msg->rep_len != 0) && (send->msg.rep_data != NULL) &&
		(send->msg.rep_len != 0)) {
		if (copy_to_user((void __user *)send->msg.rep_data, msg->rep_data, send->msg.rep_len)) {
			mctrlq_err("Msg k2u copy fail. (devid=%u; die_id=%u; rep_len=%u)\n",
				devid, send->die_id, send->msg.rep_len);
			return -EFAULT;
		}
	}
	return 0;
}

static int msgq_fops_send(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct msgq_cmd_send *send = (struct msgq_cmd_send *)in;
	struct msgq_msg msg;
	int ret;

	if (unlikely((in == NULL) || (in_len != sizeof(*send)))) {
		mctrlq_err("Invalid param. (devid=%u; size=%zu; in_len=%u)\n", devid, sizeof(*send), in_len);
		return -EINVAL;
	}

	ret = _msgq_fops_msg_init(devid, &msg, send);
	if (unlikely(ret != 0)) {
		mctrlq_err("Msg init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, send->die_id);
		return ret;
	}
	ret = _msgq_fops_msg_u2k(devid, &msg, send);
	if (unlikely(ret != 0)) {
		mctrlq_err("Msg u2k copy fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, send->die_id);
		goto out;
	}

	if (send->async == true) {
		ret = msgq_kern_send_async(devid, send->die_id, send->msn, &msg);
	} else {
		ret = msgq_kern_send(devid, send->die_id, &msg);
		if (likely(ret == 0)) {
			ret = _msgq_fops_msg_k2u(devid, &msg, send);
		}
	}
out:
	_msgq_fops_msg_uninit(&msg);

	return ret;
}

static const struct msgq_fops g_msgq_fops[] = {
	[0] = MSGQ_FOPS_ADD_CMD(MSGQ_SQ_SEND, PROC_CTRL_MANAGED, msgq_fops_send),
	[1] = MSGQ_FOPS_ADD_CMD(MSGQ_GET_SNAPSHOT, PROC_CTRL_ANY_USER, msgq_kern_get_snapshot),
};

int msgq_fops_mod_init(void)
{
	const u32 num = ARRAY_SIZE(g_msgq_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, MCTRLQ_MODULE_NAME, g_msgq_fops[i].main_cmd,
			g_msgq_fops[i].sub_cmd, g_msgq_fops[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_msgq_fops[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (unlikely(ret != 0)) {
			mctrlq_err("Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}
	return 0;
out:
	for (j = 0; j < i; j++) {
		(void)ucd_unregister_dev_cmd(g_msgq_fops[j].main_cmd, g_msgq_fops[j].sub_cmd);
	}
	return ret;
}
DECLARE_FEATURE_AUTO_INIT(msgq_fops_mod_init, FEATURE_LOADER_STAGE_1);

void msgq_fops_mod_uninit(void)
{
	const u32 num = ARRAY_SIZE(g_msgq_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		(void)ucd_unregister_dev_cmd(g_msgq_fops[i].main_cmd, g_msgq_fops[i].sub_cmd);
	}
}
DECLARE_FEATURE_AUTO_UNINIT(msgq_fops_mod_uninit, FEATURE_LOADER_STAGE_1);

