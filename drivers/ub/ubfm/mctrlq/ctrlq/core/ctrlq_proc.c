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

#include "ctrlq_kern.h"
#include "ubfm_dfx_fs.h"

#define CTRLQ_PROC_NAME   "mami/ctrlq"

static struct proc_dir_entry *g_ctrlq_entry;

static void ctrlq_proc_print(void *priv, const char *fmt, ...)
{
	struct seq_file *seq = (struct seq_file *)priv;
	va_list args;

	if (likely(seq != NULL)) {
		va_start(args, fmt);
		seq_vprintf(seq, fmt, args);  /* Use seq_vprintf to handle variable arguments */
		va_end(args);
	}
}

static inline void *ctrlq_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

static int ctrlq_proc_show(struct seq_file *seq, void *offset)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		ctrlq_kern_show(devid, ctrlq_proc_print, seq);
		cond_resched();
	}

	return 0;
}

int ctrlq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ctrlq_proc_show, ctrlq_proc_get_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *ctrlq_proc_get_fops(void)
{
	static const struct file_operations ctrlq_proc_ops = {
		.owner = THIS_MODULE,
		.open	= ctrlq_proc_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &ctrlq_proc_ops;
}
#else
static const struct proc_ops *ctrlq_proc_get_fops(void)
{
	static const struct proc_ops ctrlq_proc_ops = {
		.proc_open	= ctrlq_proc_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};

	return &ctrlq_proc_ops;
}
#endif

int ctrlq_proc_mod_init(void)
{
	g_ctrlq_entry = proc_mkdir(CTRLQ_PROC_NAME, NULL);
	if (g_ctrlq_entry != NULL) {
		(void)proc_create_data("ctrlq", UBFM_FS_MODE_OWNER_RO, g_ctrlq_entry, ctrlq_proc_get_fops(), NULL);
	}
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ctrlq_proc_mod_init, FEATURE_LOADER_STAGE_1);

void ctrlq_proc_mod_uninit(void)
{
	if (g_ctrlq_entry != NULL) {
		remove_proc_subtree(CTRLQ_PROC_NAME, NULL);
		g_ctrlq_entry = NULL;
	}
}
DECLARE_FEATURE_AUTO_UNINIT(ctrlq_proc_mod_uninit, FEATURE_LOADER_STAGE_1);

