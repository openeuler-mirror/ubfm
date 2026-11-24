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

#include "ubfm_dfx_fs.h"
#include "ucd_proc_fs.h"
#include "ucd_feature.h"

static int ucd_register_show(struct seq_file *seq, void *offset)
{
	ucd_feature_register_show(seq);
	return 0;
}

static int ucd_statistic_show(struct seq_file *seq, void *offset)
{
	ucd_feature_statistic_show(seq);
	return 0;
}

static inline void *ucd_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}


int ucd_proc_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, ucd_register_show, ucd_proc_get_data(inode));
}

int ucd_proc_statistic_open(struct inode *inode, struct file *file)
{
	return single_open(file, ucd_statistic_show, ucd_proc_get_data(inode));
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *ucd_proc_statistic_get_ops(void)
{
	static const struct file_operations ucd_proc_statistic_ops = {
		.owner = THIS_MODULE,
		.open	= ucd_proc_statistic_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &ucd_proc_statistic_ops;
}

static const struct file_operations *ucd_proc_register_get_ops(void)
{
	static const struct file_operations ucd_proc_register_ops = {
		.owner = THIS_MODULE,
		.open	= ucd_proc_register_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &ucd_proc_register_ops;
}

#else

static const struct proc_ops *ucd_proc_statistic_get_ops(void)
{
	static const struct proc_ops ucd_proc_statistic_ops = {
		.proc_open	= ucd_proc_statistic_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};
	return &ucd_proc_statistic_ops;
}

static const struct proc_ops *ucd_proc_register_get_ops(void)
{
	static const struct proc_ops ucd_proc_register_ops = {
		.proc_open	= ucd_proc_register_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};
	return &ucd_proc_register_ops;
}
#endif

void ucd_proc_fs_init(void)
{
	struct proc_dir_entry *top_entry = proc_mkdir(UCD_PROC_TOP_NAME, NULL);

	if (top_entry != NULL) {
		(void)proc_create_data(UCD_PROC_REGISTER_DATA, UBFM_FS_MODE_OWNER_RO, top_entry, ucd_proc_register_get_ops(), NULL);
		(void)proc_create_data(UCD_PROC_STATISTIC_DATA, UBFM_FS_MODE_OWNER_RO, top_entry, ucd_proc_statistic_get_ops(), NULL);
	}
}

void ucd_proc_fs_uninit(void)
{
	(void)remove_proc_subtree(UCD_PROC_TOP_NAME, NULL);
}
