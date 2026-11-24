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
#include <linux/proc_fs.h>

#include "feature_loader.h"

#define MAMI_PROC_TOP_NAME	"mami"

static struct proc_dir_entry *mami_top_entry;

static void mami_proc_fs_init(void)
{
	mami_top_entry = proc_mkdir(MAMI_PROC_TOP_NAME, NULL);
}

static void mami_proc_fs_uninit(void)
{
	if (mami_top_entry != NULL) {
		remove_proc_subtree(MAMI_PROC_TOP_NAME, NULL);
		mami_top_entry = NULL;
	}
}

int __init ubfabric_init_module(void)
{
	int ret;

	mami_proc_fs_init();

	/*  0: ucd
	 *	1: ures
	 *	2: udm
	 *	3: ued
	 *	3: uop
	 */
	ret = module_feature_auto_init();
	if (ret != 0) {
		mami_proc_fs_uninit();
		return ret;
	}

	return 0;
}

void __exit ubfabric_exit_module(void)
{
	module_feature_auto_uninit();
	mami_proc_fs_uninit();
}

module_init(ubfabric_init_module);
module_exit(ubfabric_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("UBFABRIC");

