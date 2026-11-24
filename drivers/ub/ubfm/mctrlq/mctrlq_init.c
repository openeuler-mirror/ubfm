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
#include <linux/types.h>
#include <linux/module.h>

#include "feature_loader.h"
#include "udm.h"

#include "msgq_init.h"
#include "ctrlq_init.h"

#include "mctrlq_log.h"

static int mctrlq_udm_dev_init(u32 devid)
{
	int ret;

	ret = ctrlq_kern_dev_init(devid);
	if (ret != 0) {
		mctrlq_err("Ctrlq init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = msgq_kern_dev_init(devid);
	if (ret != 0) {
		ctrlq_kern_dev_uninit(devid);
		mctrlq_err("Msgq init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	mctrlq_debug("Mctrlq dev init. (devid=%u)\n", devid);

	return 0;
}

static void mctrlq_udm_dev_uninit(u32 devid)
{
	mctrlq_debug("Mctrlq dev uninit. (devid=%u)\n", devid);
	msgq_kern_dev_uninit(devid);
	ctrlq_kern_dev_uninit(devid);
}

static int mctrlq_udm_dev_irq_init(u32 devid)
{
	return ctrlq_kern_dev_irq_init(devid);
}

static void mctrlq_udm_dev_irq_uninit(u32 devid)
{
	ctrlq_kern_dev_irq_uninit(devid);
}

static int mctrlq_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = mctrlq_udm_dev_init(devid);
	} else if (action == UDM_UNINIT) {
		mctrlq_udm_dev_uninit(devid);
	} else if (action == UDM_IRQ_INIT) {
		ret = mctrlq_udm_dev_irq_init(devid);
	} else if (action == UDM_IRQ_UNINIT) {
		mctrlq_udm_dev_irq_uninit(devid);
	}

	return ret;
}

int __init mctrlq_mod_init(void)
{
	int ret;

	ret = module_feature_auto_init();
	if (ret != 0) {
		mctrlq_err("Feature load fail. (ret=%d)\n", ret);
		return ret;
	}

	ret = udm_notifier_register(MCTRLQ_MODULE_NAME, UDM_PRI2, mctrlq_udm_notify);
	if (ret != 0) {
		module_feature_auto_uninit();
		mctrlq_err("Udm notifier register fail. (ret=%d)\n", ret);
	}

	return ret;
}

void __exit mctrlq_mod_exit(void)
{
	(void)udm_notifier_unregister(MCTRLQ_MODULE_NAME);
	/* Clear the callbacks registered into uop before this module's text goes away. */
	msgq_kern_unregister_all_intf();
	module_feature_auto_uninit();
}

module_init(mctrlq_mod_init);
module_exit(mctrlq_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("MCTRLQ");

