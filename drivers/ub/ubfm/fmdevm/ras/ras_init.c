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
#include <linux/slab.h>

#include "feature_loader.h"
#include "ubfm_common.h"
#include "udm.h"
#include "ras_ctx.h"
#include "ras_log.h"
#include "ras_core.h"
#include "feature_kern.h"

static struct uid_intr_ops g_ras_intr_ops = {
	/* since the ras module and the mng dev are the same module, owner must be passed as NULL here */
	.owner = NULL,
	.top_half_pre_handle = NULL,
	.handle = ras_intr_handle
};

static struct uid_intr_attr g_ras_intr_attr = {
	.block_flag = 1,		/* RAS parsing needs to use uop/ures related interfaces, must be in process context, use work_queue */
	.max_sched_time = 1000	/* ms */
};

STATIC int ras_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	struct udev_hw_identify hw = {0};
	int ret = 0;
	u64 ver = 0;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		ras_err("Get hw identity fail. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &ver);
	if (ret != 0) {
		ras_err("Get udie version failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	if (((enum mami_udie_version)ver == UDIE_VERSION_V2) && (hw.chip_type != MAMI_KERN_TYPE_ASCEND_V2)) {
		ras_warn("Ras is not support, it needs to adapt.\n");
		return 0;
	}

	if (action == UDM_INIT) {
		ret = ras_add_dev(devid);
		if (ret != 0) {
			return ret;
		}
	} else if (action == UDM_UNINIT) {
		ret = ras_del_dev(devid);
	} else if (action == UDM_IRQ_INIT) {
		ret = ras_irq_init(devid);
	} else if (action == UDM_IRQ_UNINIT) {
		ret = ras_irq_uninit(devid);
	}

	return ret;
}

int ras_module_init(void)
{
	int ret;

	if (!feature_kern_is_support_ras_report()) {
		return 0;
	}

	ret = ras_ctx_init();
	if (ret != 0) {
		ras_err("Ras ctx init failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = udm_notifier_register(MODULE_RAS_NAME, UDM_PRI3, ras_udm_notify);
	if (ret != 0) {
		ras_err("Register udm notifier failed. (ret=%d)\n", ret);
		ras_ctx_uninit();
		return ret;
	}

	ret = uid_register_intr(UID_TYPE_RAS, &g_ras_intr_attr, &g_ras_intr_ops);
	if (ret != 0) {
		ras_err("Register ras failed. (ret=%d)\n", ret);
		(void)udm_notifier_unregister(MODULE_RAS_NAME);
		ras_ctx_uninit();
	}

	return ret;
}
DECLARE_FEATURE_AUTO_INIT(ras_module_init, FEATURE_LOADER_STAGE_9);

void ras_module_uninit(void)
{
	if (!feature_kern_is_support_ras_report()) {
		return;
	}

	(void)uid_unregister_intr(UID_TYPE_RAS);
	(void)udm_notifier_unregister(MODULE_RAS_NAME);
	ras_ctx_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(ras_module_uninit, FEATURE_LOADER_STAGE_9);
