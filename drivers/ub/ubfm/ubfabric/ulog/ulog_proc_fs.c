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
#include <linux/version.h>

#include "feature_loader.h"
#include "ulog_core.h"
#include "ubfm_dfx_fs.h"
#include "ulog_proc_fs.h"

static int ulog_level_show(struct seq_file *seq, void *offset)
{
	ulog_level_info_show(seq);
	return 0;
}

static inline void *ulog_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

int ulog_proc_log_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, ulog_level_show, ulog_proc_get_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *ulog_proc_log_level_get_ops(void)
{
	static const struct file_operations ulog_proc_register_ops = {
		.owner = THIS_MODULE,
		.open	= ulog_proc_log_level_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &ulog_proc_register_ops;
}

#else
static const struct proc_ops *ulog_proc_log_level_get_ops(void)
{
	static const struct proc_ops ulog_proc_register_ops = {
		.proc_open	= ulog_proc_log_level_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};
	return &ulog_proc_register_ops;
}
#endif

int ulog_proc_fs_init(void)
{
	struct proc_dir_entry *top_entry = proc_mkdir(ULOG_PROC_TOP_NAME, NULL);

	if (top_entry != NULL) {
		(void)proc_create_data(ULOG_PROC_LOG_LEVEL, UBFM_FS_MODE_OWNER_RO, top_entry,
			ulog_proc_log_level_get_ops(), NULL);
	}
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ulog_proc_fs_init, FEATURE_LOADER_STAGE_4);

void ulog_proc_fs_uninit(void)
{
	(void)remove_proc_subtree(ULOG_PROC_TOP_NAME, NULL);
}
DECLARE_FEATURE_AUTO_UNINIT(ulog_proc_fs_uninit, FEATURE_LOADER_STAGE_4);
