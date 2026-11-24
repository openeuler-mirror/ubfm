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

#include "mami_string.h"
#include "ued_core.h"
#include "ued_proc_fs.h"
#include "ubfm_dfx_fs.h"

#define UED_PROC_TOP_NAME	"mami/ued"
#define UED_PROC_NAME_LEN   64

static struct proc_dir_entry *ued_top_entry;

static int ued_proc_show(struct seq_file *seq, void *offset)
{
	u64 priv = (u64)(uintptr_t)(seq->private);
	int tgid = (int)priv;
	u32 devid = (u32)(priv >> 32); /* devid high 32 bit */

	ued_show_task(devid, tgid, seq);
	return 0;
}

int ued_proc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	return single_open(file, ued_proc_show, pde_data(inode));
#else
	return single_open(file, ued_proc_show, PDE_DATA(inode));
#endif
}

static int ued_subscriber_show(struct seq_file *seq, void *offset)
{
	u32 devid = (u32)(uintptr_t)(seq->private);

	ued_show_subscriber(devid, seq);
	return 0;
}

int ued_subscriber_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	return single_open(file, ued_subscriber_show, pde_data(inode));
#else
	return single_open(file, ued_subscriber_show, PDE_DATA(inode));
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations ued_proc = {
	.owner = THIS_MODULE,
	.open	= ued_proc_open,
	.read	= seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
static const struct file_operations ued_subscriber = {
	.owner = THIS_MODULE,
	.open	= ued_subscriber_open,
	.read	= seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
#else
static const struct proc_ops ued_proc = {
	.proc_open	= ued_proc_open,
	.proc_read	= seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
static const struct proc_ops ued_subscriber = {
	.proc_open	= ued_subscriber_open,
	.proc_read	= seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
#endif

struct proc_dir_entry *ued_proc_fs_add_task(struct proc_dir_entry *dev_entry, u32 devid, int tgid, u32 task_id)
{
	char name[UED_PROC_NAME_LEN];
	u64 priv = (u32)tgid | ((u64)(devid) << 32); /* devid high 32 bit */

	if (dev_entry == NULL) {
		return NULL;
	}

	(void)UBFM_SPRINTF(name, UED_PROC_NAME_LEN, "task_%d(id_%u)", tgid, task_id);
	return proc_create_data((const char *)name, UBFM_FS_MODE_OG_RO, dev_entry, &ued_proc, (void *)(uintptr_t)priv);
}

void ued_proc_fs_del_task(struct proc_dir_entry *task_entry)
{
	if (task_entry != NULL) {
		proc_remove(task_entry);
	}
}

struct proc_dir_entry *ued_proc_fs_add_dev(u32 devid, u32 inst)
{
	struct proc_dir_entry *entry = NULL;
	char name[UED_PROC_NAME_LEN];

	if (ued_top_entry == NULL) {
		return NULL;
	}

	(void)UBFM_SPRINTF(name, UED_PROC_NAME_LEN, "dev-%u(inst-%u)", devid, inst);
	entry =  proc_mkdir(name, ued_top_entry);
	if (entry != NULL) {
		(void)proc_create_data("event_subscriber", UBFM_FS_MODE_OG_RO, entry, &ued_subscriber, (void *)(uintptr_t)devid);
	}

	return entry;
}

void ued_proc_fs_del_dev(struct proc_dir_entry *dev_entry)
{
	if (dev_entry != NULL) {
		proc_remove(dev_entry);
	}
}

void ued_proc_fs_init(void)
{
	ued_top_entry = proc_mkdir(UED_PROC_TOP_NAME, NULL);
}

void ued_proc_fs_uninit(void)
{
	if (ued_top_entry != NULL) {
		remove_proc_subtree(UED_PROC_TOP_NAME, NULL);
		ued_top_entry = NULL;
	}
}

