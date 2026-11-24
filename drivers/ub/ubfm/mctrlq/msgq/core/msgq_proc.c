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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/sched.h>

#include "ubfm_common.h"
#include "feature_loader.h"

#include "msgq_kern.h"
#include "ubfm_dfx_fs.h"

#define MSGQ_PROC_NAME   "mami/msgq"

static struct proc_dir_entry *g_msgq_entry;

static void msgq_proc_print(void *priv, const char *fmt, ...)
{
	struct seq_file *seq = (struct seq_file *)priv;
	va_list args;

	if (likely(seq != NULL)) {
		va_start(args, fmt);
		seq_vprintf(seq, fmt, args);  /* Use seq_vprintf to handle variable arguments */
		va_end(args);
	}
}

static inline void *msgq_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

static int msgq_proc_show(struct seq_file *seq, void *offset)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		msgq_kern_show(devid, msgq_proc_print, seq);
	}

	return 0;
}

int msgq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, msgq_proc_show, msgq_proc_get_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *msgq_proc_get_fops(void)
{
	static const struct file_operations msgq_proc_ops = {
		.owner = THIS_MODULE,
		.open	= msgq_proc_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &msgq_proc_ops;
}
#else
static const struct proc_ops *msgq_proc_get_fops(void)
{
	static const struct proc_ops msgq_proc_ops = {
		.proc_open	= msgq_proc_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};

	return &msgq_proc_ops;
}
#endif

int msgq_proc_mod_init(void)
{
	g_msgq_entry = proc_mkdir(MSGQ_PROC_NAME, NULL);
	if (g_msgq_entry != NULL) {
		(void)proc_create_data("msgq", UBFM_FS_MODE_OWNER_RO, g_msgq_entry, msgq_proc_get_fops(), NULL);
	}
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(msgq_proc_mod_init, FEATURE_LOADER_STAGE_1);

void msgq_proc_mod_uninit(void)
{
	if (g_msgq_entry != NULL) {
		remove_proc_subtree(MSGQ_PROC_NAME, NULL);
		g_msgq_entry = NULL;
	}
}
DECLARE_FEATURE_AUTO_UNINIT(msgq_proc_mod_uninit, FEATURE_LOADER_STAGE_1);
