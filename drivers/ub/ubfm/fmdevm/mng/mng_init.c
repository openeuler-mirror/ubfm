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
#include <linux/uidgid.h>
#include <linux/types.h>
#include "feature_loader.h"
#include "mng_kernel.h"
#include "mng_cdev.h"
#include "mng_handle_ucd.h"
#include "ucd.h"

#define MNG_MODULE_NAME "mng"

int mng_init_cmd_dev_register(void)
{
	int ret = 0;

	ret = ucd_register_module_dev_ops(MNG_MODULE_NAME, mng_dev_open, mng_dev_release, NULL);
	if (ret != 0) {
		mng_err("ucd_register_module_dev_ops failed. (ret=%d)\n", ret);

		return ret;
	}

	return 0;
}

int mng_init_cmd_dev_unregister(void)
{
	int ret = 0;

	ret = ucd_unregister_module_dev_ops(MNG_MODULE_NAME);
	if (ret != 0) {
		mng_err("ucd_unregister_module_dev_ops failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

int mng_init(void)
{
	int ret;

	ret = mng_create_cdev();
	if (ret != 0) {
		return ret;
	}

	ret = mng_register_ucd_cmd();
	if (ret != 0) {
		mng_err("Failed to register ucd, will destroy dev. (ret=%d)\n", ret);
		mng_destroy_cdev();
		return ret;
	}

	ret = mng_init_cmd_dev_register();
	if (ret != 0) {
		mng_err("Failed to register ucd, will destroy dev and unregister ucd cmd. (ret=%d)\n", ret);
		mng_unregister_ucd_cmd();
		mng_destroy_cdev();
		return ret;
	}

	mng_info("Init ubdevm mng success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(mng_init, FEATURE_LOADER_STAGE_3);

void mng_uninit(void)
{
	(void)mng_init_cmd_dev_unregister();
	mng_unregister_ucd_cmd();
	mng_destroy_cdev();
	mng_info("Uninit ubdevm mng success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(mng_uninit, FEATURE_LOADER_STAGE_3);
