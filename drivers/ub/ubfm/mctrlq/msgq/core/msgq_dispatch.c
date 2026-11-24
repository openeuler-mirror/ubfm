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
#include <linux/kernel.h>

#include "mctrlq.h"
#include "msgq_cq.h"
#include "mctrlq_log.h"
#include "msgq_dispatch.h"
#include "mami_string.h"

static struct vdm_opcode_to_event g_msgq_vdm_opcode[] = {
	{VDM_VENDER_OPCODE_NODE_TO_FM, VDM_NODE_TO_FM_BUS_CONTROLLER_PORT_RESET, UED_EVENT_PORT_MSGQ_MSG, 0},
	{VDM_VENDER_OPCODE_NODE_TO_FM, VDM_NODE_TO_FM_UBMEM_PORT_RESET, UED_EVENT_UB_MEM_MSGQ_MSG, 0},
};

STATIC int msgq_vdm_opcode_to_event_id(struct msg_pkt *pkt, struct ued_published_event *ued_event)
{
	struct vdm_pkt_header vdm_hdr;
	u32 op_code, sub_opcode;
	u32 i;

	(void)ubfm_memcpy(&vdm_hdr, sizeof(struct vdm_pkt_header), pkt->payload, sizeof(struct vdm_pkt_header));
	op_code = vdm_hdr.opcode;
	sub_opcode = vdm_hdr.sub_opcode;

	for (i = 0; i < (u32)ARRAY_SIZE(g_msgq_vdm_opcode); i++) {
		if (g_msgq_vdm_opcode[i].opcode == op_code && g_msgq_vdm_opcode[i].sub_opcode == sub_opcode) {
			ued_event->event_id = g_msgq_vdm_opcode[i].event_id;
			ued_event->subevent_id = g_msgq_vdm_opcode[i].subevent_id;
			return 0;
		}
	}

	/* Unmatched message, defaults to DRM for processing */
	ued_event->event_id = UED_EVENT_DRM_MSGQ_MSG;
	ued_event->subevent_id = 0;

	return 0;
}

STATIC int msgq_vdm_to_event_id(struct msgq_event *event, struct ued_published_event *ued_event)
{
	struct msg_pkt *pkt = (struct msg_pkt *)(event->data);
	u8 msg_code = pkt->header.msgetah.msg_code;
	u8 sub_msg_code = pkt->header.msgetah.sub_msg_code;

	if (msg_code != UB_MSG_CODE_VDM) {
		mctrlq_err("Invalid msg code. (msg_code=%u; sub_msg_code=%u)\n", msg_code, sub_msg_code);
		return -EINVAL;
	}

	if (sub_msg_code == VDM_SUB_MSG_CODE_VENDER) {
		return msgq_vdm_opcode_to_event_id(pkt, ued_event);
	}

	ued_event->event_id = UED_EVENT_DRM_MSGQ_MSG;
	ued_event->subevent_id = 0;

	return 0;
}

static bool msgq_check_sub_msg_code(u8 sub_msg_code)
{
	if ((sub_msg_code == PORT_SUB_MSG_CODE_LINKUP) || (sub_msg_code == PORT_SUB_MSG_CODE_LINKDOWN)
		|| (sub_msg_code == PORT_SUB_MSG_REMOTE_LINKUP)) {
			return true;
	}

	return false;
}

int msgq_to_event_id(struct msgq_cq *cq, struct ued_published_event *ued_event)
{
	struct msgq_sw_cq *sw_cq = &cq->sw_cq;
	struct msgq_cqe *cqe = &sw_cq->cqe[cq->index];
	struct msgq_cq_opcode *opcode_ = (struct msgq_cq_opcode *)&cqe->opcode;
	u8 msg_code = opcode_->msg_code;
	u8 sub_msg_code = opcode_->sub_msg_code;
	int ret = 0;

	switch (msg_code) {
	case UB_MSG_CODE_LINK:
		ued_event->event_id = UED_EVENT_PORT_MSGQ_MSG;
		if (!msgq_check_sub_msg_code(sub_msg_code)) {
			mctrlq_err("Invalid LINK sub_msg_code. (devid=%u; die_id=%u; sub_msg_code=%u; expected_sub_msg_code={%u, %u, %u})\n",
				cq->devid, cq->die_id, sub_msg_code, PORT_SUB_MSG_CODE_LINKUP, PORT_SUB_MSG_CODE_LINKDOWN, PORT_SUB_MSG_REMOTE_LINKUP);
			return -EINVAL;
		}
		ued_event->subevent_id = sub_msg_code;
		break;

	case UB_MSG_CODE_VDM:
		ret = msgq_vdm_to_event_id(&cq->event, ued_event);
		break;

	default:
		ued_event->event_id = UED_EVENT_MAX_NUM;
		mctrlq_err("MSGQ msg_code neither LINK nor VDM. (devid=%u; die_id=%u; msg_code=%u)\n", cq->devid, cq->die_id, msg_code);
		return -EINVAL;
	}

	return ret;
}
