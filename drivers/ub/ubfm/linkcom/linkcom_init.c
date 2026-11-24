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
#include "feature_loader.h"
#include "ubfm_common.h"
#include "udm.h"
#include "uid.h"
#include "ucd.h"

#include "ubfabric.h"
#include "ubfabric.h"
#include "linkcom_log.h"
#include "linkcom_mng.h"
#include "linkcom_handle_ucd.h"
#include "mgmtq_proc.h"
#include "mgmtq/linkcom_mgmtq_irq.h"
#include "linkcom_core.h"

struct linkcom_ucd_cmd {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

#define LINKCOM_ADD_UCD_CMD(s, p, f) {	 \
	.main_cmd		 = UCD_CMD_UBLINKCOM, \
	.sub_cmd		  = s,				 \
	.proc_ctrl_bitmap = p,				 \
	.func			 = f				  \
}

static const struct linkcom_ucd_cmd g_linkcom_ucd_cmd[] = {
	[UBLINKCOM_OPEN] = LINKCOM_ADD_UCD_CMD(UBLINKCOM_OPEN, PROC_CTRL_MANAGED, linkcom_ucd_open_link_pkt),
	[UBLINKCOM_CLOSE] = LINKCOM_ADD_UCD_CMD(UBLINKCOM_CLOSE, PROC_CTRL_MANAGED, linkcom_ucd_close_link_pkt),
	[UBLINKCOM_SEND] = LINKCOM_ADD_UCD_CMD(UBLINKCOM_SEND, PROC_CTRL_ANY_USER, linkcom_ucd_send_link_pkt),
	[UBLINKCOM_RECV] = LINKCOM_ADD_UCD_CMD(UBLINKCOM_RECV, PROC_CTRL_ANY_USER, linkcom_ucd_recv_link_pkt),
	[UBLINKCOM_EVENTFD] = LINKCOM_ADD_UCD_CMD(UBLINKCOM_EVENTFD, PROC_CTRL_ANY_USER, linkcom_ucd_bond_eventfd),
};

static struct uid_intr_ops g_mgmtq_intr_ops = {
	.owner = NULL,
	.top_half_pre_handle = mgmtq_intr_pre_handle,
	.handle = mgmtq_intr_handle
};

static struct uid_intr_attr g_mgmtq_intr_attr = {
	.block_flag = 1,
	.max_sched_time = 50	 /* periodic task scheduling time, 0 means no periodic task */
};

static const u32 g_linkcom_ucd_cmd_num = ARRAY_SIZE(g_linkcom_ucd_cmd);


static void linkcom_unregister_dev_ucd_num(u32 in_cmd_num)
{
	u32 i = 0, cmd_num = in_cmd_num;
	int ret;

	cmd_num = cmd_num > g_linkcom_ucd_cmd_num ? g_linkcom_ucd_cmd_num : cmd_num;

	for (i = 0; i < cmd_num; i++) {
		ret = ucd_unregister_dev_cmd(UCD_CMD_UBLINKCOM, g_linkcom_ucd_cmd[i].sub_cmd);
		if (ret != 0) {
			linkcom_err("ucd unregister fail. (sub_cmd=%u)\n", g_linkcom_ucd_cmd[i].sub_cmd);
		}
	}
}

void linkcom_unregister_dev_ucd(void)
{
	linkcom_unregister_dev_ucd_num(g_linkcom_ucd_cmd_num);
}

int linkcom_register_dev_ucd(void)
{
	u32 i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(g_linkcom_ucd_cmd); i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, MODULE_LINKCOM_NAME, g_linkcom_ucd_cmd[i].main_cmd,
			g_linkcom_ucd_cmd[i].sub_cmd, g_linkcom_ucd_cmd[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_linkcom_ucd_cmd[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (ret != 0) {
			linkcom_err("ucd register cmd fail, will rollback. (ret=%d; i=%u)\n", ret, i);
			linkcom_unregister_dev_ucd_num(i);
			return ret;
		}
	}

	return 0;
}

int linkcom_dev_mng_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = linkcom_add_dev(devid);
		if (ret != 0) {
			return ret;
		}
		ret = mgmtq_irq_init(devid);
	} else if (action == UDM_UNINIT) {
		mgmtq_irq_uninit(devid);
		ret = linkcom_del_dev(devid);
	}

	return ret;
}


int linkcom_init(void)
{
	int ret;

	mgmtq_proc_init();
	mgmtq_irq_hw_init();

	ret = linkcom_mng_init();
	if (ret != 0) {
		mgmtq_proc_uninit();
		return ret;
	}

	ret = udm_notifier_register(MODULE_LINKCOM_NAME, UDM_PRI3, linkcom_dev_mng_notify);
	if (ret != 0) {
		linkcom_mng_uninit();
		mgmtq_proc_uninit();
		linkcom_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = linkcom_register_dev_ucd();
	if (ret != 0) {
		(void)udm_notifier_unregister(MODULE_LINKCOM_NAME);
		linkcom_mng_uninit();
		mgmtq_proc_uninit();
		linkcom_err("Failed to register ucd. (ret=%d)\n", ret);
		return ret;
	}

	ret = uid_register_intr(UID_TYPE_MGMTQ, &g_mgmtq_intr_attr, &g_mgmtq_intr_ops);
	if (ret != 0) {
		linkcom_unregister_dev_ucd();
		(void)udm_notifier_unregister(MODULE_LINKCOM_NAME);
		linkcom_mng_uninit();
		mgmtq_proc_uninit();
		linkcom_err("Register mgmtq interrupt failed. (ret=%d)\n", ret);
		return ret;
	};

	linkcom_info("Init linkcom success.\n");

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(linkcom_init, FEATURE_LOADER_STAGE_6);


void linkcom_uninit(void)
{
	(void)uid_unregister_intr(UID_TYPE_MGMTQ);
	linkcom_unregister_dev_ucd();
	(void)udm_notifier_unregister(MODULE_LINKCOM_NAME);
	linkcom_mng_uninit();
	mgmtq_proc_uninit();

	linkcom_info("Uninit linkcom success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(linkcom_uninit, FEATURE_LOADER_STAGE_6);
