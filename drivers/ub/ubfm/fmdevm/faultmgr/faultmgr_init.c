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
#include "ucd.h"
#include "ubfabric.h"
#include "ubfabric.h"
#include "faultmgr_log.h"
#include "faultmgr_core.h"
#include "faultmgr_queue.h"

struct faultmgr_ucd_cmd {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	feature_handler func;
};

#define FAULTMGR_ADD_UCD_CMD(sub, bitmap, fn) { \
	.main_cmd		 = UCD_CMD_FAULTMGR,	   \
	.sub_cmd		  = sub,					\
	.proc_ctrl_bitmap = bitmap,				 \
	.func			 = fn					  \
}

#define EVENT_ADD_AUTH		  (PROC_CTRL_MANAGED | PROC_CTRL_PROCESS_MGR | PROC_CTRL_PM_CHILD | PROC_CTRL_IAM_MGR)

static const struct faultmgr_ucd_cmd g_faultmgr_ucd_cmd[] = {
	[0] = FAULTMGR_ADD_UCD_CMD(FAULTMGR_EVENT_ADD, EVENT_ADD_AUTH, faultmgr_ucd_event_add),
	[1] = FAULTMGR_ADD_UCD_CMD(FAULTMGR_EVENT_GET, PROC_CTRL_ANY_USER, faultmgr_ucd_event_get),
};

static const u32 g_faultmgr_ucd_cmd_num = ARRAY_SIZE(g_faultmgr_ucd_cmd);

static void faultmgr_unregister_dev_ucd_range(u32 start, u32 cmd_num)
{
	u32 total_count = 0, i = 0;
	int ret = 0;

	total_count = (start + cmd_num) > g_faultmgr_ucd_cmd_num ? g_faultmgr_ucd_cmd_num : (start + cmd_num);

	for (i = start; i < total_count; i++) {
		ret = ucd_unregister_cmd(UCD_CMD_FAULTMGR, g_faultmgr_ucd_cmd[i].sub_cmd);
		if (ret != 0) {
			faultmgr_err("FaultMgr unregister failed. (cmd_idx=%u; total_count=%u; sub_cmd=%u)\n",
						 i, total_count, g_faultmgr_ucd_cmd[i].sub_cmd);
		}
	}
}

void faultmgr_unregister_dev_ucd(void)
{
	faultmgr_unregister_dev_ucd_range(0, g_faultmgr_ucd_cmd_num);
}

int faultmgr_register_dev_ucd(void)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; i < g_faultmgr_ucd_cmd_num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, MODULE_FAULTMGR_NAME, g_faultmgr_ucd_cmd[i].main_cmd,
							 g_faultmgr_ucd_cmd[i].sub_cmd, g_faultmgr_ucd_cmd[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_faultmgr_ucd_cmd[i].proc_ctrl_bitmap);
		ret = ucd_register_cmd(&ucd_cmd);
		if (ret != 0) {
			faultmgr_err("FaultMgr register cmd failed, will rollback. (ret=%d; iter=%u)\n", ret, i);
			faultmgr_unregister_dev_ucd_range(0, i);
			return ret;
		}
	}

	return 0;
}

STATIC int faultmgr_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		faultmgr_queue_online_dev_inc();
		/* Dynamically expand the fault queue buffer based on the maximum number of concurrent devices. */
		ret = faultmgr_queue_reinit(devid);
		if (ret != 0) {
			faultmgr_queue_online_dev_dec();
			return ret;
		}
		return 0;
	}

	if (action == UDM_UNINIT) {
		faultmgr_queue_online_dev_dec();
	}
	return 0;
}

int faultmgr_init(void)
{
	int ret;

	ret = faultmgr_register_dev_ucd();
	if (ret != 0) {
		faultmgr_err("Failed to register ucd. (ret=%d)\n", ret);
		return ret;
	}

	ret = faultmgr_queue_init();
	if (ret != 0) {
		faultmgr_unregister_dev_ucd();
		faultmgr_err("Failed to init faultmgr queue(ret=%d)\n", ret);
		return ret;
	}

	ret = udm_notifier_register(MODULE_FAULTMGR_NAME, UDM_PRI2, faultmgr_udm_notify);
	if (ret != 0) {
		faultmgr_queue_uninit();
		faultmgr_unregister_dev_ucd();
		faultmgr_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	faultmgr_info("Init faultmgr success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(faultmgr_init, FEATURE_LOADER_STAGE_6);

void faultmgr_uninit(void)
{
	(void)udm_notifier_unregister(MODULE_FAULTMGR_NAME);
	faultmgr_queue_uninit();
	faultmgr_unregister_dev_ucd();
	faultmgr_info("Uninit faultmgr success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(faultmgr_uninit, FEATURE_LOADER_STAGE_6);
