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
#ifdef CFG_BUILD_DEBUG
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include "mami_string.h"

#include "ubfm_common.h"
#include "feature_loader.h"
#include "ubfm_dfx_fs.h"
#include "faultmgr_log.h"
#include "faultmgr_event.h"
#include "faultmgr_queue.h"

STATIC ssize_t faultmgr_inject_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mami_fault_event event = {.eventId = 0x10011111, .eventName = "event_test"};

	ubdevm_kern_report_event(&event);
	return (ssize_t)count;
}

static DEVICE_ATTR(inject, UBFM_FS_MODE_OWNER_RW, NULL, faultmgr_inject_store);
static struct attribute *g_faultmgr_sysfs_attrs[] = {
	&dev_attr_inject.attr,
	NULL
};

static struct attribute_group g_faultmgr_attr_grp = {
	.name = "faultmgr",
	.attrs = g_faultmgr_sysfs_attrs,
};

int faultmgr_sysfs_init(void *priv)
{
	struct platform_device *pdev = NULL;
	int ret;

	if (unlikely(priv == NULL)) {
		faultmgr_err("Invalid param\n");
		return -EINVAL;
	}
	pdev = (struct platform_device *)priv;
	ret = sysfs_create_group(&pdev->dev.kobj, &g_faultmgr_attr_grp);
	if (unlikely(ret != 0)) {
		faultmgr_err("Create group failed. (ret=%d)\n", ret);
	}
	return ret;
}
DECLARE_FEATURE_AUTO_INIT_BY_SCOPE(sysfs, faultmgr_sysfs_init, FEATURE_LOADER_STAGE_6);

void faultmgr_sysfs_uninit(void *priv)
{
	struct platform_device *pdev = NULL;

	if (unlikely(priv == NULL)) {
		faultmgr_err("Invalid param.\n");
		return;
	}
	pdev = (struct platform_device *)priv;
	sysfs_remove_group(&pdev->dev.kobj, &g_faultmgr_attr_grp);
}
DECLARE_FEATURE_AUTO_UNINIT_BY_SCOPE(sysfs, faultmgr_sysfs_uninit, FEATURE_LOADER_STAGE_6);
#endif
