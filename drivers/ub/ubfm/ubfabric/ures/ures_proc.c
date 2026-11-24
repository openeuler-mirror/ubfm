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
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "ures_core.h"
#include "ures_proc.h"
#include "ubfm_dfx_fs.h"

#define URES_PROC_NAME  "mami/ures"

static struct proc_dir_entry *g_ures_entry;

static void _ures_proc_print(void *priv, const char *fmt, ...)
{
	struct seq_file *seq = (struct seq_file *)priv;
	va_list args;

	if (likely(seq != NULL)) {
		va_start(args, fmt);
		seq_vprintf(seq, fmt, args);  /* use seq_vprintf to handle variable arguments */
		va_end(args);
	}
}

static int _ures_proc_show(struct seq_file *seq, void *offset)
{
	u32 devid, die_id;

	ures_show(_ures_proc_print, seq);
	for (devid = 0; devid < URES_MAX_DEVNUM; devid++) {
		ures_dev_res_show(devid, _ures_proc_print, seq);
		for (die_id = 0; die_id < URES_MAX_DIE_NUM; die_id++) {
			struct ures_inst inst;

			ures_inst_pack(&inst, devid, die_id);
			ures_die_res_show(&inst, _ures_proc_print, seq);
		}
	}

	return 0;
}

static inline void *ures_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

int ures_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _ures_proc_show, ures_proc_get_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *ures_proc_get_fops(void)
{
	static const struct file_operations ures_proc_ops = {
		.owner = THIS_MODULE,
		.open	= ures_proc_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};
	return &ures_proc_ops;
}
#else
static const struct proc_ops *ures_proc_get_fops(void)
{
	static const struct proc_ops ures_proc_ops = {
		.proc_open	= ures_proc_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};
	return &ures_proc_ops;
}
#endif

void ures_proc_init(void)
{
	g_ures_entry = proc_mkdir(URES_PROC_NAME, NULL);
	if (g_ures_entry != NULL) {
		(void)proc_create_data("ures", UBFM_FS_MODE_OWNER_RO, g_ures_entry, ures_proc_get_fops(), NULL);
	}
}

void ures_proc_uninit(void)
{
	if (g_ures_entry != NULL) {
		(void)remove_proc_subtree(URES_PROC_NAME, NULL);
	}
}

