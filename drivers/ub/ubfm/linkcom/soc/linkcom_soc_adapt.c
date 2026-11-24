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
#include "linkcom_log.h"
#include "udie_v1_linkcom_soc.h"
#include "udie_v2_linkcom_soc.h"
#include "linkcom_soc_adapt.h"


static int (*g_inband_send_link_pkt[UDIE_VERSION_MAX])(struct linkcom_id_inst *id_inst, struct mami_pkt_send_para send_para) = {
	[UDIE_VERSION_V1] = udie_v1_inband_send_link_pkt,
	[UDIE_VERSION_V2] = udie_v1_inband_send_link_pkt,
};

int linkcom_inband_send_link_pkt(struct linkcom_id_inst *id_inst, struct mami_pkt_send_para send_para)
{
	if (id_inst == NULL) {
		linkcom_err("send_link_pkt id_inst is NULL\n");
		return -EINVAL;
	}

	if (id_inst->udie_version >= UDIE_VERSION_MAX) {
		linkcom_err("udie version is not supported.(udie_version=%u)\n", id_inst->udie_version);
		return -EINVAL;
	}

	return g_inband_send_link_pkt[id_inst->udie_version](id_inst, send_para);
}

static int (*g_inband_read_rpkt[UDIE_VERSION_MAX])(struct linkcom_id_inst *id_inst, struct linkcom_list_head *head, u32 *pkt_num) = {
	[UDIE_VERSION_V1] = udie_v1_inband_read_rpkt,
	[UDIE_VERSION_V2] = udie_v2_inband_read_rpkt,
};

int linkcom_inband_read_rpkt(struct linkcom_id_inst *id_inst, struct linkcom_list_head *head, u32 *pkt_num)
{
	if (head == NULL) {
		linkcom_err("head is NULL\n");
		return -EINVAL;
	}

	if (id_inst == NULL) {
		linkcom_err("read_rpkt id_inst is NULL\n");
		return -EINVAL;
	}

	if (pkt_num == NULL) {
		linkcom_err("pkt_num is NULL\n");
		return -EINVAL;
	}

	if (id_inst->udie_version >= UDIE_VERSION_MAX) {
		linkcom_err("udie version is not supported.(udie_version=%u)\n", id_inst->udie_version);
		return -EINVAL;
	}

	return g_inband_read_rpkt[id_inst->udie_version](id_inst, head, pkt_num);
}

static void (*g_mgmtq_show[UDIE_VERSION_MAX])(struct linkcom_id_inst *id_inst,
			void (*show)(void *priv, const char *fmt, ...), void *priv) = {
	[UDIE_VERSION_V1] = udie_v1_mgmtq_show,
	[UDIE_VERSION_V2] = udie_v1_mgmtq_show,
};

void mgmtq_show(struct linkcom_id_inst *id_inst, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	if (id_inst == NULL) {
		linkcom_err("mgmtq show id_inst is NULL\n");
		return;
	}
	if (id_inst->udie_version >= UDIE_VERSION_MAX) {
		linkcom_err("udie version is not supported.(udie_version=%u)\n", id_inst->udie_version);
		return;
	}

	return g_mgmtq_show[id_inst->udie_version](id_inst, show, priv);
}

