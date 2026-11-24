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
#include "ctrlq_fops.h"

#include "feature_loader.h"

#include "ucd.h"
#include "ubfabric.h"
#include "ubfabric.h"
#include "mctrlq_log.h"
#include "ctrlq_kern.h"

#include "ctrlq_trace.h"

#define CTRLQ_FOPS_ADD_CMD(s, p, f) {	\
	.main_cmd		 =   UCD_CMD_CTRLQ, \
	.sub_cmd		  =   s,			 \
	.proc_ctrl_bitmap =   p,			 \
	.func			 =   f			  \
}

struct ctrlq_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

static int ctrlq_fops_chan_switch(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ctrlq_cmd_switch *cmd = (struct ctrlq_cmd_switch *)in;
	int ret;

	if (unlikely((cmd == NULL) || (in_len != sizeof(*cmd)))) {
		mctrlq_err("Invalid param. (devid=%u; in_len=%u; cmd_size=%zu)\n",
			devid, in_len, sizeof(struct ctrlq_cmd_switch));
		return -EINVAL;
	}

	ret = (cmd->on_off == true) ? ctrlq_kern_chan_enable(devid, cmd->chan_id) :
		ctrlq_kern_chan_disable(devid, cmd->chan_id);
	if (unlikely(ret != 0)) {
		mctrlq_err("Ctrlq chan switch fail. (ret=%d; devid=%u; chan_id=%u; on_off=%d)\n",
			ret, devid, cmd->chan_id, cmd->on_off);
		return ret;
	}

	return 0;
}

static int ctrlq_fops_sqe_ts(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ctrlq_sqe_ts *ts = (struct ctrlq_sqe_ts *)in;
	struct ctrlq_chan_id chan = {0};
	struct ctrlq_que que = {0};
	struct ctrlq_msg_head head = {0};

	if (unlikely(in == NULL) || unlikely(in_len != sizeof(struct ctrlq_sqe_ts))) {
		mctrlq_err("Invalid param. (devid=%u;in_is_null=%u;in_len=%u;cmd_size=%zu)\n",
				   devid, (in == NULL), in_len, sizeof(struct ctrlq_sqe_ts));
		return -EINVAL;
	}
	/* trace_sqe() only uses some fields of chan and sqe */
	chan.devid = ts->devid;
	chan.die_id = ts->die_id;
	chan.chan_id = ts->chan_id;
	que.tail = ts->sq_tail;
	que.head = ts->sq_head;
	que.depth = ts->sq_depth;
	head.ver = ts->sqe_ver;
	head.service_type = ts->sqe_type;
	head.bb_num = ts->sqe_bb_num;
	head.op_code = ts->sqe_op_code;
	head.seq = ts->sqe_seq;
	trace_sqe(&chan, &que, &head);
	return 0;
}

static const struct ctrlq_fops g_ctrlq_fops[] = {
	[0] = CTRLQ_FOPS_ADD_CMD(CTRLQ_CHAN_SWITCH, PROC_CTRL_MANAGED, ctrlq_fops_chan_switch),
	[1] = CTRLQ_FOPS_ADD_CMD(CTRLQ_GET_SNAPSHOT, PROC_CTRL_ANY_USER, ctrlq_kern_get_snapshot),
	[2] = CTRLQ_FOPS_ADD_CMD(CTRLQ_SEND_SQE_TS, PROC_CTRL_MANAGED, ctrlq_fops_sqe_ts),
};

int ctrlq_fops_mod_init(void)
{
	const u32 num = ARRAY_SIZE(g_ctrlq_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, MCTRLQ_MODULE_NAME, g_ctrlq_fops[i].main_cmd,
			g_ctrlq_fops[i].sub_cmd, g_ctrlq_fops[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_ctrlq_fops[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (unlikely(ret != 0)) {
			mctrlq_err("Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}
	return 0;
out:
	for (j = 0; j < i; j++) {
		(void)ucd_unregister_dev_cmd(g_ctrlq_fops[j].main_cmd, g_ctrlq_fops[j].sub_cmd);
	}
	return ret;
}

void ctrlq_fops_mod_uninit(void)
{
	const u32 num = ARRAY_SIZE(g_ctrlq_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		(void)ucd_unregister_dev_cmd(g_ctrlq_fops[i].main_cmd, g_ctrlq_fops[i].sub_cmd);
	}
}

