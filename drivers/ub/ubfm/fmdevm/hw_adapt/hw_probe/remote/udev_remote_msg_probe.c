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
/* This file is used only by the DPU. */
#include "mami_string.h"
#include <linux/slab.h>
#include <linux/limits.h>
#include "ubfm_common.h"
#include "fmdevm.h"
#include "ures.h"
#include "uop.h"
#include "udev_ctrl.h"
#include "udev_hw_info.h"
#include "udev_msg_pub.h"
#include "mctrlq.h"
#include "fmdevm.h"
#include "mng_devid.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b))?(a):(b))
#endif
#define VDM_HEAD_LEN (sizeof(struct msg_pkt_header_kernel) + sizeof(struct vdm_pkt_header))

#define VDM_DEV_INFO_REQ_LEN    (VDM_HEAD_LEN + sizeof(struct vdm_get_dev_info_pld))
#define VDM_DEV_INFO_RSP_LEN    (VDM_HEAD_LEN + sizeof(struct vdm_get_dev_info_rsp_pld))
#define VDM_UDIE_INFO_REQ_LEN   (VDM_HEAD_LEN + sizeof(struct vdm_get_die_info_pld))
#define VDM_UDIE_INFO_RSP_LEN   (VDM_HEAD_LEN + sizeof(struct vdm_get_die_info_rsp_pld))
#define VDM_UE_INFO_REQ_LEN     (VDM_HEAD_LEN + sizeof(struct vdm_get_ue_info_pld))
#define VDM_UE_TYPE_REQ_LEN	    (VDM_HEAD_LEN + sizeof(struct vdm_get_ue_type_req_pld))

#define MSG_PKT_HEAD_INITIALIZER(_dcna, _deid) \
	.ulh.cfg = UB_CLAN_LINK_CFG, \
	.nth.mgmt = 1, \
	.nth.nth_nlp = NTH_NLP_WITH_TPH, \
	.nth.dcna = (_dcna), \
	.ctph_nlp = CTPH_NLP_UPI_40BITS_UEID, \
	.tp_opcode = CTPH_OPCODE_NOT_CNP, \
	.upi = 0x7FFF, \
	.deid = (_deid), \
	.ta_opcode = TAH_OPCODE_MSG,

#define MSG_PKT_HEAD_VDM_INITIALIZER(_dcna, _deid) \
	MSG_PKT_HEAD_INITIALIZER(_dcna, _deid) \
	.msgetah.msg_code = UB_MSG_CODE_VDM, \
	.msgetah.sub_msg_code = VDM_SUB_MSG_CODE_VENDER,

#define MSG_SQE_INITIALIZER(_req_data, _rsp_data, _rsp_len) \
	.type = MSGQ_TYPE_PROTOCOL, \
	.opcode = (_req_data).header.msgetah.code, \
	.local = 0, /* 1: local; use 1 when combining local send/receive packets remotely via CDMA */ \
	.dev_type = 0, \
	.req_data = &(_req_data), \
	.rep_len = (_rsp_len), \
	.rep_data = &(_rsp_data),

/* This function is used only by the DPU. */
static int udev_get_cna_eid_by_devid(u32 dev_id, u32 *cna, u32 *eid)
{
	u64 cna_eid = 0;
	char cna_eid_key[URES_NAME_LEN];
	int ret = 0;

	(void)UBFM_SPRINTF(cna_eid_key, sizeof(cna_eid_key), MAMI_UDEV_DIEN_CNA_EID, 0);
	ret = ures_get_key_value(dev_id, cna_eid_key, &cna_eid);
	if (ret != 0) {
		hw_err("Can not get cna_eid. (devid=%u; ret=%d)\n", dev_id, ret);
		return ret;
	}
	*cna = (u32)(cna_eid >> 32);  /* 32: High 32bit for cna */
	*eid = (u32)(cna_eid & 0xffffffff); /* 0xffffffff: Low 32bit for eid */
	return 0;
}

/* This function is used only by the DPU. */
static void debug_print_data(void *data, u32 len)
{
	u32 *buf = NULL;
	u8 *char_buf = NULL;
	u32 i = 0;
	u32 size = 4;

	buf = (u32 *)data;
	char_buf = (u8 *)data;
	hw_debug("Data info(msg len=%u)\n", len);
	for (i = 0; i < len / size; i++) {
		hw_debug("[%03d]: 0x%08x\n", i, buf[i]);
	}
	for (i = i * size; i < len; i++) {
		hw_debug("Last: 0x%x\n", char_buf[i]);
	}
}

int mgmtq_msg_check(u32 devid, u32 die_id, struct msgq_msg *msg)
{
	if (unlikely(msg == NULL)) {
		hw_err("Msg is NULL. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	if (unlikely(msg->type >= MSGQ_TYPE_MAX)) {
		hw_err("Invalid type. (devid=%u; die_id=%u)\n", devid, die_id);
		return -EINVAL;
	}

	if (unlikely((msg->req_data == NULL) || (msg->req_len < sizeof(struct msg_pkt_header_kernel)))) {
		hw_err("Req data invalid. (devid=%u; die_id=%u; req_is_null=%d; cur_req_len=%u; exp_req_len=%zu)\n",
			devid, die_id, msg->req_data == NULL, msg->req_len, sizeof(struct msg_pkt_header_kernel));
		return -EINVAL;
	}

	return 0;
}

int udev_mgmtq_kern_i3c_send(u32 dev_id, void *user_ctx)
{
	struct msgq_msg *msg = (struct msgq_msg *)user_ctx;
	drv_ubnic_xfer_msg i3c_msg = { 0 };
	int ret = 0;
	u32 die_id = 0;

	if (mgmtq_msg_check(dev_id, die_id, msg)) {
		hw_err("Msg check fail. (devid=%u; die_id=%u)\n", dev_id, die_id);
		return -EINVAL;
	}
	if (msg->rep_data == NULL || msg->rep_len <= 0) {
		hw_err("Resp data invalid. (devid=%u; die_id=%u; resp_is_null=%d; cur_resp_len=%u;)\n",
			dev_id, die_id, msg->rep_data == NULL, msg->rep_len);
		return -EINVAL;
	}

	hw_debug("Send msg by i3c. (dev_id=%u, sdie_id=%u)\n", dev_id, die_id);
	i3c_msg.op = PCIE_I3C_UB_MESSAGE;
	i3c_msg.write_len = msg->req_len;
	i3c_msg.read_len = msg->rep_len;
	i3c_msg.write_buf = msg->req_data;
	i3c_msg.read_buf = msg->rep_data;
	ret = uop_i3c_send_recv_kern(dev_id, &i3c_msg);
	if (ret != 0) {
		hw_err("i3c Msg send fail. (ret=%d; devid=%u; die_id=%u)\n", ret, dev_id, die_id);
		return -EINVAL;
	}
	return 0;
}

int mgmtq_kern_send_recv_msg(u32 dev_id,  u32 conn_type, void *ctx)
{
	if (conn_type == UDEV_PCIE_I3C) {
		return udev_mgmtq_kern_i3c_send(dev_id, ctx);
	} else {
		return uop_kern_send_recv_msg(dev_id, conn_type, ctx);
	}
}
/* This function is used only by the DPU. */
static int udev_mgmtq_get_dev_info(u32 dev_id, u32 conn_type, struct msg_cmd *cmd)
{
	int ret = 0;
	struct dev_cfg_info_cmd *in_param = (struct dev_cfg_info_cmd *)cmd->in_buf;
	struct msg_pkt *req_data = NULL;
	struct msg_pkt_rsp *rsp_data = NULL;
	struct vdm_pkt_header *req_vdm_hdr = NULL;
	struct vdm_pkt_header *rsq_vdm_hdr = NULL;
	struct vdm_get_dev_info_pld *vdm_pld = NULL;
	struct vdm_get_dev_info_rsp_pld *rsp_pld = NULL;

	req_data = (struct msg_pkt *)kzalloc(VDM_DEV_INFO_REQ_LEN, GFP_KERNEL);
	if (req_data == NULL) {
		return -ENOMEM;
	}
	rsp_data = (struct msg_pkt_rsp *)kzalloc(VDM_DEV_INFO_RSP_LEN, GFP_KERNEL);
	if (rsp_data == NULL) {
		kfree(req_data);
		return -ENOMEM;
	}

	req_vdm_hdr = (struct vdm_pkt_header *)req_data->payload;
	vdm_pld = (struct vdm_get_dev_info_pld *)((void *)req_vdm_hdr + sizeof(struct vdm_pkt_header));

	req_data->header = (struct msg_pkt_header_kernel) {
		MSG_PKT_HEAD_VDM_INITIALIZER(in_param->cna, in_param->eid)
		.msgetah.plen = sizeof(struct vdm_pkt_header) + sizeof(struct vdm_get_dev_info_pld),
	};
	req_vdm_hdr->opcode = VDM_VENDER_OPCODE_FM_TO_NODE;
	req_vdm_hdr->sub_opcode = VDM_FM_TO_NODE_GET_DEV_ATTR;
	vdm_pld->sub_cmd = MAMI_MSG_TYPE_DEV_INFO;

	struct msgq_msg msg = {
		MSG_SQE_INITIALIZER(*req_data, *rsp_data, VDM_DEV_INFO_RSP_LEN)
		.req_len = VDM_DEV_INFO_REQ_LEN,
	};

	ret = mgmtq_kern_send_recv_msg(dev_id, conn_type, &msg);
	if (ret != 0) {
		hw_err("Failed to get udev info by msg. (dcna=%u; ret=%d)\n", in_param->cna, ret);
		kfree(req_data);
		kfree(rsp_data);
		return ret;
	}
	hw_debug("Rsp dev info(msg len=%u; pld len=%u)\n", msg.rep_len, rsp_data->header.msgetah.plen);
	debug_print_data(rsp_data, VDM_DEV_INFO_RSP_LEN);
	rsq_vdm_hdr = (struct vdm_pkt_header *)rsp_data->payload;
	rsp_pld = (struct vdm_get_dev_info_rsp_pld *)((void *)rsq_vdm_hdr + sizeof(struct vdm_pkt_header));
	ret = ubfm_memcpy(cmd->out_buf, (size_t)cmd->out_len, &rsp_pld->data, sizeof(rsp_pld->data));
	if (ret != 0) {
		hw_err("Memcpy Failed (ret=%d)\n", ret);
		kfree(req_data);
		kfree(rsp_data);
		return ret;
	}

	kfree(req_data);
	kfree(rsp_data);
	return ret;
}

/* This function is used only by the DPU. */
static int udev_mgmtq_get_die_info(u32 dev_id, u32 conn_type, struct msg_cmd *cmd)
{
	int ret = 0;
	u32 dcna = 0, deid = 0;
	struct dev_udie_info_cmd *in_param = (struct dev_udie_info_cmd *)cmd->in_buf;
	struct msg_pkt *req_data = NULL;
	struct msg_pkt_rsp *rsp_data = NULL;
	struct vdm_pkt_header *req_vdm_hdr = NULL;
	struct vdm_pkt_header *rsq_vdm_hdr = NULL;
	struct vdm_get_die_info_pld *vdm_pld = NULL;
	struct vdm_get_die_info_rsp_pld *rsp_pld = NULL;

	req_data = (struct msg_pkt *)kzalloc(VDM_UDIE_INFO_REQ_LEN, GFP_KERNEL);
	if (req_data == NULL) {
		return -ENOMEM;
	}
	rsp_data = (struct msg_pkt_rsp *)kzalloc(VDM_UDIE_INFO_RSP_LEN, GFP_KERNEL);
	if (rsp_data == NULL) {
		kfree(req_data);
		return -ENOMEM;
	}

	req_vdm_hdr = (struct vdm_pkt_header *)req_data->payload;
	vdm_pld = (struct vdm_get_die_info_pld *)((void *)req_vdm_hdr + sizeof(struct vdm_pkt_header));

	req_data->header = (struct msg_pkt_header_kernel) {
		MSG_PKT_HEAD_VDM_INITIALIZER(dcna, deid)
		.msgetah.plen = sizeof(struct vdm_pkt_header) + sizeof(struct vdm_get_die_info_pld),
	};
	req_vdm_hdr->opcode = VDM_VENDER_OPCODE_FM_TO_NODE;
	req_vdm_hdr->sub_opcode = VDM_FM_TO_NODE_GET_DEV_ATTR;
	vdm_pld->sub_cmd = MAMI_MSG_TYPE_UDIE_INFO;
	vdm_pld->die_id = in_param->udie_idx;

	struct msgq_msg msg = {
		MSG_SQE_INITIALIZER(*req_data, *rsp_data, VDM_UDIE_INFO_RSP_LEN)
		.req_len = VDM_UDIE_INFO_REQ_LEN,
	};

	ret = udev_get_cna_eid_by_devid(dev_id, &dcna, &deid);
	if (ret != 0) {
		hw_err("Failed to get cna eid by devid. (devid=%u; ret=%d)\n", dev_id, ret);
		kfree(req_data);
		kfree(rsp_data);
		return ret;
	}
	req_data->header.nth.dcna = dcna;
	req_data->header.deid = deid;
	ret = mgmtq_kern_send_recv_msg(dev_id, conn_type, &msg);
	if (ret != 0) {
		hw_err("Failed to get udev info by msg. (dcna=%u; ret=%d)\n", dcna, ret);
		kfree(req_data);
		kfree(rsp_data);
		return ret;
	}
	hw_debug("Rsp die info(msg len=%u; pld len=%u)\n", msg.rep_len, rsp_data->header.msgetah.plen);
	debug_print_data(rsp_data, VDM_UDIE_INFO_RSP_LEN);
	rsq_vdm_hdr = (struct vdm_pkt_header *)rsp_data->payload;
	rsp_pld = (struct vdm_get_die_info_rsp_pld *)((void *)rsq_vdm_hdr + sizeof(struct vdm_pkt_header));
	ret = ubfm_memcpy(cmd->out_buf, (size_t)cmd->out_len, &rsp_pld->data, sizeof(rsp_pld->data));
	if (ret != 0) {
		hw_err("Memcpy Failed (ret=%d)\n", ret);
		kfree(req_data);
		kfree(rsp_data);
		return ret;
	}

	kfree(req_data);
	kfree(rsp_data);
	return ret;
}

/* This function is used only by the DPU. */
static int _udev_mgmtq_send_ue_info_msg(u32 dev_id, u32 conn_type, struct msg_cmd *cmd, u16 start_idx, u16 num)
{
	int ret = 0;
	u32 dcna = 0, deid = 0, resp_len;
	struct msg_pkt_rsp *rsp_data = NULL;
	struct udev_msg_ue_info *out_data = (struct udev_msg_ue_info *)cmd->out_buf;
	u32 real_num = 0;
	u32 recv_num;
	struct msg_pkt *req_data = NULL;
	struct vdm_pkt_header *req_vdm_hdr = NULL;
	struct vdm_pkt_header *rsq_vdm_hdr = NULL;
	struct vdm_get_ue_info_pld *vdm_pld = NULL;
	struct vdm_get_ue_info_rsp_pld *rsp_pld = NULL;

	req_data = (struct msg_pkt *)kzalloc(VDM_UE_INFO_REQ_LEN, GFP_KERNEL);
	if (req_data == NULL) {
		return -ENOMEM;
	}

	req_vdm_hdr = (struct vdm_pkt_header *)req_data->payload;
	vdm_pld = (struct vdm_get_ue_info_pld *)((void *)req_vdm_hdr + sizeof(struct vdm_pkt_header));

	req_data->header = (struct msg_pkt_header_kernel) {
		MSG_PKT_HEAD_VDM_INITIALIZER(dcna, deid)
		.msgetah.plen = sizeof(struct vdm_pkt_header) + sizeof(struct vdm_get_ue_info_pld),
	};
	req_vdm_hdr->opcode = VDM_VENDER_OPCODE_FM_TO_NODE;
	req_vdm_hdr->sub_opcode = VDM_FM_TO_NODE_GET_DEV_ATTR;
	vdm_pld->sub_cmd = MAMI_MSG_TYPE_UE_INFO;
	vdm_pld->offset = start_idx;
	vdm_pld->req_ue_num = num;

	struct msgq_msg msg = {
		.type = MSGQ_TYPE_PROTOCOL,
		.opcode = req_data->header.msgetah.code,
		.local = 0,
		.req_data = req_data,
		.req_len = VDM_UE_INFO_REQ_LEN,
	};

	ret = udev_get_cna_eid_by_devid(dev_id, &dcna, &deid);
	if (ret != 0) {
		hw_err("Failed to get cna eid by devid. (devid=%u; ret=%d)\n", dev_id, ret);
		kfree(req_data);
		return ret;
	}
	req_data->header.nth.dcna = dcna;
	req_data->header.deid = deid;
	resp_len = VDM_HEAD_LEN + sizeof(struct vdm_get_ue_info_rsp_pld) + sizeof(struct udev_msg_mue_info) * num;
	rsp_data = (struct msg_pkt_rsp *)kzalloc(resp_len, GFP_KERNEL);
	if (rsp_data == NULL) {
		hw_err("Get ue info failed, alloc mem buf failed(devid=%u, len=%u)\n", dev_id, resp_len);
		kfree(req_data);
		return -ENOMEM;
	}
	msg.rep_len = resp_len;
	msg.rep_data = rsp_data;
	ret = mgmtq_kern_send_recv_msg(dev_id, conn_type, &msg);
	if (ret != 0) {
		hw_err("Failed to get udev info by msg. (dcna=%u; ret=%d)\n", dcna, ret);
		kfree(rsp_data);
		kfree(req_data);
		return ret;
	}
	rsq_vdm_hdr = (struct vdm_pkt_header *)rsp_data->payload;
	rsp_pld = (struct vdm_get_ue_info_rsp_pld *)((void *)rsq_vdm_hdr + sizeof(struct vdm_pkt_header));
	/* A short response leaves a hole in out_data, the caller still advances by num. */
	if (rsp_pld->ue_num < num) {
		hw_err("Ue num too small. (devid=%u; start_idx=%u; req_num=%u; rsp_num=%u)\n", dev_id, start_idx, num, rsp_pld->ue_num);
		kfree(rsp_data);
		kfree(req_data);
		return -EINVAL;
	}
	if (rsp_pld->ue_num > num) {
		hw_warn("Ue num not correct. (req ue num=%u; resp ue num=%u)\n", num, rsp_pld->ue_num);
	}
	real_num = MIN(rsp_pld->ue_num, num);
	ret = ubfm_memcpy(&out_data->mue_info[start_idx], (size_t)num * sizeof(struct udev_msg_mue_info), rsp_pld->data, sizeof(struct udev_msg_mue_info) * real_num);
	if (ret != 0) {
		hw_err("Memcpy Failed (ret=%d)\n", ret);
		kfree(rsp_data);
		kfree(req_data);
		return ret;
	}
	recv_num = (u32)out_data->mue_num + real_num;
	if (unlikely(recv_num > (u32)U8_MAX)) {
		hw_err("Ue num overflow. (devid=%u; start_idx=%u; recv_num=%u; max=%u)\n", dev_id, start_idx, recv_num, (u32)U8_MAX);
		kfree(rsp_data);
		kfree(req_data);
		return -EOVERFLOW;
	}
	out_data->mue_num = (u8)recv_num;

	kfree(rsp_data);
	kfree(req_data);
	return ret;
}

/* This function is used only by the DPU. */
static int udev_mgmtq_get_ue_info(u32 dev_id, u32 conn_type, struct msg_cmd *cmd)
{
	int ret = 0;
	u16 start_idx = 0, num = 0;
	u32 loop = 0, loop_num = 0;
	struct dev_ue_info_cmd *in_param = (struct dev_ue_info_cmd *)cmd->in_buf;
	struct udev_msg_ue_info *out_data = (struct udev_msg_ue_info *)cmd->out_buf;

	if (in_param->ue_num > U8_MAX) {
		hw_err("Error param. (devid=%u, ue_num=%u, max_ue_num=%u)\n", dev_id, in_param->ue_num, (u32)U8_MAX);
		return -EINVAL;
	}
	if (cmd->in_len != sizeof(struct dev_ue_info_cmd) || cmd->out_len != (sizeof(struct udev_msg_ue_info) + sizeof(struct udev_msg_mue_info) * in_param->ue_num)) {
		hw_err("Error param. (devid=%u, in_len=%u, correct_in_len=%u, out_len=%u, correct_out_len=%u)\n", dev_id, cmd->in_len, (u32)sizeof(struct dev_ue_info_cmd), cmd->out_len, (u32)(sizeof(struct udev_msg_ue_info) + sizeof(struct udev_msg_mue_info) * in_param->ue_num));
		return -EINVAL;
	}
	out_data->mue_num = 0;
	if (in_param->ue_num == 0) {
		return 0;
	}
	num = 0x2;  /* get 2 MUE info each time */
	loop_num = in_param->ue_num / num;
	for (loop = 0; loop < loop_num; loop++) {
		/* send msg */
		ret = _udev_mgmtq_send_ue_info_msg(dev_id, conn_type, cmd, loop * num, num);
		if (ret != 0) {
			hw_err("Failed to get udev info by msg. (devid=%u, start_idx=%u, num=%u, ret=%d)\n", dev_id, start_idx, num, ret);
			return ret;
		}
		/* update */
		start_idx = start_idx + num;
	}
	if (start_idx < in_param->ue_num) {
		num = in_param->ue_num % num;
		ret = _udev_mgmtq_send_ue_info_msg(dev_id, conn_type, cmd, start_idx, num);
	}
	return ret;
}

/* This function is used only by the DPU. */
static int _udev_mgmtq_send_ue_type_info_msg(u32 dev_id, u32 conn_type, struct msg_cmd *cmd, u16 start_idx, u16 num)
{
	int ret = 0;
	u32 dcna = 0, deid = 0, resp_len;
	struct msg_pkt_rsp *rsp_data = NULL;
	struct udev_msg_ue_type_rsp *out_data = (struct udev_msg_ue_type_rsp *)cmd->out_buf;
	u32 real_num = 0;
	struct msg_pkt *req_data = NULL;
	struct vdm_pkt_header *req_vdm_hdr = NULL;
	struct vdm_pkt_header *rsq_vdm_hdr = NULL;
	struct vdm_get_ue_type_req_pld *vdm_pld = NULL;
	struct vdm_get_ue_type_rsp_pld *rsp_pld = NULL;

	req_data = (struct msg_pkt *)kzalloc(VDM_UE_TYPE_REQ_LEN, GFP_KERNEL);
	if (req_data == NULL) {
		return -ENOMEM;
	}

	req_vdm_hdr = (struct vdm_pkt_header *)req_data->payload;
	vdm_pld = (struct vdm_get_ue_type_req_pld *)((void *)req_vdm_hdr + sizeof(struct vdm_pkt_header));

	req_data->header = (struct msg_pkt_header_kernel) {
		MSG_PKT_HEAD_VDM_INITIALIZER(dcna, deid)
		.msgetah.plen = sizeof(struct vdm_pkt_header) + sizeof(struct vdm_get_ue_type_req_pld),
	};
	req_vdm_hdr->opcode = VDM_VENDER_OPCODE_FM_TO_NODE;
	req_vdm_hdr->sub_opcode = VDM_FM_TO_NODE_GET_DEV_ATTR;
	vdm_pld->sub_cmd = MAMI_MSG_TYPE_UE_TYPE_INFO;
	vdm_pld->offset = start_idx;
	vdm_pld->req_seg_num = num;

	struct msgq_msg msg = {
		.type = MSGQ_TYPE_PROTOCOL,
		.opcode = req_data->header.msgetah.code,
		.local = 0,
		.req_data = req_data,
		.req_len = VDM_UE_TYPE_REQ_LEN,
	};

	ret = udev_get_cna_eid_by_devid(dev_id, &dcna, &deid);
	if (ret != 0) {
		hw_err("Failed to get cna eid by devid. (devid=%u; ret=%d)\n", dev_id, ret);
		kfree(req_data);
		return ret;
	}
	req_data->header.nth.dcna = dcna;
	req_data->header.deid = deid;
	resp_len = VDM_HEAD_LEN + sizeof(struct vdm_get_ue_type_rsp_pld) + sizeof(struct udev_ue_type_seg_rsp) * num;
	rsp_data = (struct msg_pkt_rsp *)kzalloc(resp_len, GFP_KERNEL);
	if (rsp_data == NULL) {
		hw_err("Get ue info failed, alloc mem buf failed(devid=%u, len=%u)\n", dev_id, resp_len);
		kfree(req_data);
		return -ENOMEM;
	}
	msg.rep_len = resp_len;
	msg.rep_data = rsp_data;
	ret = mgmtq_kern_send_recv_msg(dev_id, conn_type, &msg);
	if (ret != 0) {
		hw_err("Failed to get udev info by msg. (dcna=%u; ret=%d)\n", dcna, ret);
		kfree(rsp_data);
		kfree(req_data);
		return ret;
	}
	rsq_vdm_hdr = (struct vdm_pkt_header *)rsp_data->payload;
	rsp_pld = (struct vdm_get_ue_type_rsp_pld *)((void *)rsq_vdm_hdr + sizeof(struct vdm_pkt_header));
	if (rsp_pld->segment_num > num) {
		hw_warn("segment_num err. (req seg num=%u; resp seg num=%u)\n", num, rsp_pld->segment_num);
	}
	real_num = MIN(rsp_pld->segment_num, num);
	ret = ubfm_memcpy(&out_data->type_info[start_idx], (size_t)num * sizeof(struct udev_ue_type_seg_rsp), rsp_pld->data, sizeof(struct udev_ue_type_seg_rsp) * real_num);
	if (ret != 0) {
		hw_err("dev %u Memcpy Failed (ret=%d)\n", dev_id, ret);
	}
	out_data->type_seg_num += real_num;
	hw_debug("Rsp dev info(msg len=%u; pld len=%u; rsp num=%u; type_seg_num=%u; real_num=%u)\n", msg.rep_len,
		rsp_data->header.msgetah.plen, rsp_pld->segment_num, out_data->type_seg_num, real_num);
	debug_print_data(rsp_data, resp_len);

	kfree(rsp_data);
	kfree(req_data);
	return ret;
}
/* This function is used only by the DPU. */
static int udev_mgmtq_get_ue_type_info(u32 dev_id, u32 conn_type, struct msg_cmd *cmd)
{
	int ret = 0;
	u16 start_idx = 0, num = 0;
	u32 loop = 0, loop_num = 0;
	struct dev_ue_type_info_cmd *in_param = (struct dev_ue_type_info_cmd *)cmd->in_buf;
	struct udev_msg_ue_type_rsp *out_data = (struct udev_msg_ue_type_rsp *)cmd->out_buf;

	if (cmd->in_len != sizeof(struct dev_ue_type_info_cmd) || cmd->out_len != (sizeof(struct udev_msg_ue_type_rsp) + sizeof(struct udev_ue_type_seg_rsp) * in_param->ue_type_seg_num)) {
		hw_err("Error param. (devid=%u, in_len=%u, correct_in_len=%u, out_len=%u, correct_in_len=%u)\n", dev_id, cmd->in_len, (u32)sizeof(struct dev_ue_type_info_cmd),
			cmd->out_len, (u32)(sizeof(struct udev_msg_ue_type_rsp) + sizeof(struct udev_ue_type_seg_rsp) * in_param->ue_type_seg_num));
		return -EINVAL;
	}
	out_data->type_seg_num = 0;
	if (in_param->ue_type_seg_num == 0) {
		return 0;
	}
	num = 0x10;  /* get 16 segment info each time */
	loop_num = in_param->ue_type_seg_num / num;
	for (loop = 0; loop < loop_num; loop++) {
		/* send msg */
		hw_info("devid=%u, send seg offset %u num %u\n", dev_id, loop * num, num);
		ret = _udev_mgmtq_send_ue_type_info_msg(dev_id, conn_type, cmd, loop * num, num);
		if (ret != 0) {
			hw_err("Failed to get udev info by msg. (devid=%u, start_idx=%u, num=%u, ret=%d)\n", dev_id, start_idx, num, ret);
			return ret;
		}
		/* update */
		start_idx = start_idx + num;
	}
	if (start_idx < in_param->ue_type_seg_num) {
		num = in_param->ue_type_seg_num % num;
		hw_info("devid=%u, send seg offset %u num %u\n", dev_id, start_idx, num);
		ret = _udev_mgmtq_send_ue_type_info_msg(dev_id, conn_type, cmd, start_idx, num);
	}
	return ret;
}

/* This function is used only by the DPU. */
int msgq_get_cfg(u32 dev_id, u32 conn_type, struct msg_cmd *cmd)
{
	if (cmd == NULL || cmd->in_buf == NULL || cmd->out_buf == NULL) {
		hw_err("NULL param. (devid=%u)\n", dev_id);
		return -EINVAL;
	}

	switch (cmd->cmd_type) {
	case MAMI_MSG_TYPE_DEV_INFO:
		if (cmd->in_len != sizeof(struct dev_cfg_info_cmd) || cmd->out_len != sizeof(struct udev_msg_cfg_info)) {
			hw_err("Error param. (devid=%u, in_len=%u, correct_in_len=%u, out_len=%u, correct_in_len=%u)\n", dev_id, cmd->in_len,
				(u32)sizeof(struct dev_cfg_info_cmd), cmd->out_len, (u32)sizeof(struct udev_msg_cfg_info));
			return -EINVAL;
		}
		return udev_mgmtq_get_dev_info(dev_id, conn_type, cmd);
	case MAMI_MSG_TYPE_UDIE_INFO:
		if (cmd->in_len != sizeof(struct dev_udie_info_cmd) || cmd->out_len != sizeof(struct udev_msg_die_cfg)) {
			hw_err("Error param. (devid=%u, in_len=%u, correct_in_len=%u, out_len=%u, correct_in_len=%u)\n", dev_id, cmd->in_len,
				(u32)sizeof(struct dev_udie_info_cmd), cmd->out_len, (u32)sizeof(struct udev_msg_die_cfg));
			return -EINVAL;
		}
		return udev_mgmtq_get_die_info(dev_id, conn_type, cmd);
	case MAMI_MSG_TYPE_UE_INFO: return udev_mgmtq_get_ue_info(dev_id, conn_type, cmd);
	case MAMI_MSG_TYPE_UE_TYPE_INFO:
		return udev_mgmtq_get_ue_type_info(dev_id, conn_type, cmd);
	default: return -EINVAL;
	}
	return 0;
}

static bool is_well_known_guid(const u8 guid[])
{
	int i;
	int well_known_size = 14; /* the high 112 bits of a well-known GUID are all 1 */

	for (i = 0; i < well_known_size; i++) {
		if (guid[i] != 0xff) {
			return false;
		}
	}
	return true;
}

static int udev_check_para(struct udev_msg_cfg_info *cfg_info, struct mami_dev_info *dev_info)
{
	int ret;
	char guid_str[GUID_STR_LEN];
	char guid_in_str[GUID_STR_LEN];

	ret = udev_guid_to_str(cfg_info->ubc_ue0_guid, guid_str);
	if (ret == 0) {
		hw_info("udev_guid_to_str.(chip_type=%u;ubc_ue0_guid=%s;ret=%d)\n", dev_info->chipType, guid_str, ret);
	}
	ret = udev_guid_to_str(dev_info->guid, guid_in_str);
	if (ret == 0) {
		hw_info("udev_guid_to_str.(chip_type=%u;input guid=%s;ret=%d)\n", dev_info->chipType, guid_in_str, ret);
	}
	hw_info("input eid %u; dev eid (chip_type=%u;compressedFlag=%u;compressedEid=%u),input cna %u; primary_cna=%u\n", dev_info->eid,
		dev_info->chipType, cfg_info->ubc_ue0_eid.compressedFlag, cfg_info->ubc_ue0_eid.compressedEid, dev_info->cna, cfg_info->primary_cna);
	if (is_well_known_guid(dev_info->guid)) {
		hw_err("(chip_type=%u input guid is well-known guid)\n", dev_info->chipType);
		return -EINVAL;
	}
	if (udev_guid_compare(cfg_info->ubc_ue0_guid, dev_info->guid) != 0) {
		hw_err("(chip_type=%u;input guid err)\n", dev_info->chipType);
		return -EINVAL;
	}
	if (dev_info->eid != cfg_info->ubc_ue0_eid.compressedEid) {
		hw_err("(chip_type=%u;input eid err)\n", dev_info->chipType);
		return -EINVAL;
	}
	return 0;
}

/* This function is used only by the DPU. */
static int udev_get_hw_info_by_msg(struct mami_dev_info *dev_info, struct udev_hw_identify *hw)
{
	struct udev_msg_cfg_info cfg_info = {0};
	struct udev_cfg_msg *cfg = NULL;
	int ret;
	struct msg_cmd cmd = {0};
	struct dev_cfg_info_cmd dev_cmd = {0};

	dev_cmd.cna = dev_info->cna;
	dev_cmd.eid = dev_info->eid;
	cmd.cmd_type = MAMI_MSG_TYPE_DEV_INFO;
	cmd.in_buf = (void *)&dev_cmd;
	cmd.in_len = sizeof(struct dev_cfg_info_cmd);
	cmd.out_buf = (void *)&cfg_info;
	cmd.out_len = sizeof(struct udev_msg_cfg_info);
	ret = msgq_get_cfg(0, hw->connect_type, &cmd);
	if (ret != 0) {
		hw_err("Msg read failed. (ret=%d, chip_type=%u)\n", ret, dev_info->chipType);
		return -EINVAL;
	}
	ret = udev_check_para(&cfg_info, dev_info);
	if (ret != 0) {
		return ret;
	}
	ret = udev_cfg_magic_check(&cfg_info.cfg_hdr);
	if (ret != 0) {
		return -EINVAL;
	}
	cfg = &cfg_info.cfg;
	if (cfg->device_type != dev_info->chipType) {
		hw_err("Invalid msg chip_type. (cfg_chip_type=%u;in_chip_type=%u)\n",
			cfg->device_type, dev_info->chipType);
		return -EINVAL;
	}
	hw->chip_type = cfg->device_type;
	hw->slot_id = cfg->slot_id;
	hw->socket_id = cfg->socket_id;
	hw->resv_mem_base = 0;
	hw_debug("Get msg cfg info. (type=%u; slot_id=%u; socket_id=%u)\n",
		cfg->device_type, cfg->slot_id, cfg->socket_id);
	return 0;
}

/* This function is used only by the DPU. */
static int udev_get_remote_hw_info(struct mami_dev_info *dev_info)
{
	u32 chip_type = dev_info->chipType, devid;
	struct udev_hw_identify hw = {0};
	int ret;

	if (chip_type >= MAMI_KERN_TYPE_MAX || chip_type <= MAMI_KERN_TYPE_INVALID) {
		hw_err("Chip type is invalid. (chip_type=%u)\n", chip_type);
		return -EINVAL;
	}
	/* only in-band devices trigger the msg probe flow */
	hw.access_type = UDEV_CDMA;
	hw.location = UDM_REMOTE;
	hw.connect_type = UDEV_UBUS;
	hw.resv_mem_size = 0;
	hw.chip_type = chip_type;
	ret = udev_get_hw_info_by_msg(dev_info, &hw);
	if (ret != 0) {
		hw_err("Get hw info by msg failed. (ret=%d)\n", ret);
		return ret;
	}
	udev_guid_copy(hw.guid, dev_info->guid);
	ret = udev_set_guid_dev(&hw);
	if (ret != 0) {
		hw_err("Set guid info failed.(ret=%d)\n", ret);
		return ret;
	}
	ret = mng_alloc_devid(hw.slot_id, hw.socket_id, &devid);
	if (ret != 0 && ret != -EEXIST) {
		hw_err("Alloc devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", hw.slot_id, hw.socket_id, ret);
		return ret;
	}
	hw_info("Alloc devid. (slot_id=%u, socket_id=%u, devid=%u)\n", hw.slot_id, hw.socket_id, devid);
	return 0;
}

/* This function is used only by the DPU. */
int hw_adapt_msg_probe(struct mami_dev_info *dev_info)
{
	int ret;

	ret = udev_get_remote_hw_info(dev_info);
	if (ret != 0) {
		hw_err("Remote msg get hw info failed.(ret=%d; chip_type=%u)\n", ret, dev_info->chipType);
		return ret;
	}
	hw_info("Trigger dev msg probe success. (ret=%d; chip_type=%u)\n", ret, dev_info->chipType);
	return 0;
}
