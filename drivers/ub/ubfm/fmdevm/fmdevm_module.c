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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "feature_loader.h"
#include "ulog.h"
#include "udev_vsock.h"

static int env_mode = ENV_DEFAULT;
module_param(env_mode, int, 0644);

int udev_get_env_mode(void)
{
	return env_mode;
}

static int ubdevm_probe(struct platform_device *pdev)
{
	return module_feature_auto_init_by_scope("sysfs", pdev);
}

static int ubdevm_remove(struct platform_device *pdev)
{
	module_feature_auto_uninit_by_scope("sysfs", pdev);
	return 0;
}

static struct platform_driver g_ubdevm_driver = {
	.probe = ubdevm_probe,
	.remove = ubdevm_remove,
	.driver = {
		.name = "ubdevm",
		.owner = THIS_MODULE
	},
};

static struct platform_device *g_ubdevm_device;

int __init ubdevm_init_module(void)
{
	int ret;
	struct platform_device *pdev;
	/*  3: mng, ras
	 *	5-9: hw_adapt
	 */
	ret = module_feature_auto_init();
	if (ret != 0) {
#ifndef EMU_ST
		mami_err("ubdevm", "module auto init failed. (ret=%d)\n", ret);
		return ret;
#endif
	}

	pdev = platform_device_alloc("ubdevm", -1);
	if (pdev == NULL) {
#ifndef EMU_ST
		module_feature_auto_uninit();
		mami_err("ubdevm", "Alloc device failed.\n");
		return -ENOMEM;
#endif
	}

	ret = platform_device_add(pdev);
	if (ret != 0) {
#ifndef EMU_ST
		platform_device_put(pdev);
		module_feature_auto_uninit();
		mami_err("ubdevm", "Add device failed. (ret=%d)\n", ret);
		return ret;
#endif
	}

	ret = platform_driver_register(&g_ubdevm_driver);
	if (ret != 0) {
#ifndef EMU_ST
		platform_device_unregister(pdev);
		module_feature_auto_uninit();
		mami_err("ubdevm", "Fail to register driver. (ret=%d)\n", ret);
		return ret;
#endif
	}

	g_ubdevm_device = pdev;
	return 0;
}

void __exit ubdevm_exit_module(void)
{
	platform_driver_unregister(&g_ubdevm_driver);
	if (g_ubdevm_device != NULL) {
		platform_device_unregister(g_ubdevm_device);
		g_ubdevm_device = NULL;
	}
	module_feature_auto_uninit();
}

module_init(ubdevm_init_module);
module_exit(ubdevm_exit_module);

MODULE_PARM_DESC(env_mode, "Environment mode parameter");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("UBDEVM");
