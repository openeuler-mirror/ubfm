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
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/mman.h>

#include "ubfabric.h"
#include "uop_kern.h"
#include "uop_proc.h"
#include "fpga_adapter_kern.h"
#include "ubfm_dfx_fs.h"

#define UOP_PROC_NAME   "mami/uop"

static struct proc_dir_entry *g_uop_entry;

static void uop_proc_print(void *priv, const char *fmt, ...)
{
	struct seq_file *seq = (struct seq_file *)priv;
	va_list args;

	if (likely(seq != NULL)) {
		va_start(args, fmt);
		seq_vprintf(seq, fmt, args);  /* Use seq_vprintf to handle variable arguments */
		va_end(args);
	}
}

static inline void *uop_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

static int uop_proc_show(struct seq_file *seq, void *offset)
{
	u32 devid;

	for (devid = 0; devid < UOP_MAX_DEVNUM; devid++) {
		uop_kern_dev_show(devid, uop_proc_print, seq);
		cond_resched();
	}

	return 0;
}

int uop_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uop_proc_show, uop_proc_get_data(inode));
}

static int uop_back_pressure_show(struct seq_file *seq, void *offset)
{
	u32 devid;

	for (devid = 0; devid < UOP_MAX_DEVNUM; devid++) {
		uop_kern_back_show(devid, uop_proc_print, seq);
		cond_resched();
	}
	return 0;
}

STATIC int uop_back_pressure_open(struct inode *inode, struct file *file)
{
	return single_open(file, uop_back_pressure_show, uop_proc_get_data(inode));
}

static int uop_back_enable_show_probe(struct seq_file *seq, void *offset)
{
	bool state = uop_get_fpga_back_state();

	seq_printf(seq, "%d\n", state);
	return 0;
}

STATIC int uop_back_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, uop_back_enable_show_probe, inode->i_private);
}

#define ECHO_WRITE_LEN 5
#define UOP_BACK_ENABLE 1
#define UOP_BACK_DISABLE 0
STATIC ssize_t uop_back_enable_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char str[ECHO_WRITE_LEN] = {0};
	int val;

	if ((ppos == NULL) || (*ppos != 0) || (count > sizeof(str)) || (ubuf == NULL)) {
		return -EINVAL;
	}
	if (copy_from_user(str, ubuf, count) != 0) {
		return -ENOMEM;
	}
	str[ECHO_WRITE_LEN - 1] = '\0';
	if (kstrtoint(str, 10, &val) != 0) {	/* 10:Decimal */
		return -EFAULT;
	}
	if (val == UOP_BACK_ENABLE || val == UOP_BACK_DISABLE) {
		uop_fpga_back_enable(val);
	}
	return count;
}

#ifndef EMU_ST
static unsigned long uop_get_unmapped_area(struct file *filp, unsigned long addr, unsigned long len,
	unsigned long pgoff, unsigned long flags)
{
	if ((flags & MAP_FIXED) != 0) {
		return -EINVAL;
	}

	if (filp == NULL) {
		return -ENODEV;
	}
	return current->mm->get_unmapped_area(filp, addr, len, pgoff, flags);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *uop_proc_get_fops(void)
{
	static const struct file_operations uop_proc_ops = {
		.owner = THIS_MODULE,
		.open	= uop_proc_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
#ifndef EMU_ST
		.get_unmapped_area = uop_get_unmapped_area,
#endif
	};
	return &uop_proc_ops;
}
static const struct file_operations *uop_back_pressure_get_fops(void)
{
	static const struct file_operations uop_proc_ops = {
		.owner = THIS_MODULE,
		.open	= uop_back_pressure_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
#ifndef EMU_ST
		.get_unmapped_area = uop_get_unmapped_area,
#endif
	};
	return &uop_proc_ops;
}
static const struct file_operations *uop_back_enable_get_fops(void)
{
	static const struct file_operations uop_proc_ops = {
		.owner   = THIS_MODULE,
		.open	= uop_back_enable_open,
		.read	= seq_read,
		.write   = uop_back_enable_write,
		.llseek  = seq_lseek,
		.release = single_release,
#ifndef EMU_ST
		.get_unmapped_area = uop_get_unmapped_area,
#endif
	};
	return &uop_proc_ops;
}
#else
static const struct proc_ops *uop_proc_get_fops(void)
{
	static const struct proc_ops uop_proc_ops = {
		.proc_open	= uop_proc_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
#ifndef EMU_ST
		.proc_get_unmapped_area = uop_get_unmapped_area,
#endif
	};

	return &uop_proc_ops;
}
static const struct proc_ops *uop_back_pressure_get_fops(void)
{
	static const struct proc_ops uop_proc_ops = {
		.proc_open	= uop_back_pressure_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
#ifndef EMU_ST
		.proc_get_unmapped_area = uop_get_unmapped_area,
#endif
	};

	return &uop_proc_ops;
}
static const struct proc_ops *uop_back_enable_get_fops(void)
{
	static const struct proc_ops uop_proc_ops = {
		.proc_open	= uop_back_enable_open,
		.proc_read	= seq_read,
		.proc_write   = uop_back_enable_write,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
#ifndef EMU_ST
		.proc_get_unmapped_area = uop_get_unmapped_area,
#endif
	};

	return &uop_proc_ops;
}
#endif

void uop_proc_mod_init(void)
{
	g_uop_entry = proc_mkdir(UOP_PROC_NAME, NULL);
	if (g_uop_entry != NULL) {
		(void)proc_create_data("uop", UBFM_FS_MODE_OWNER_RO, g_uop_entry, uop_proc_get_fops(), NULL);
		(void)proc_create_data("back_stats", UBFM_FS_MODE_OWNER_RO, g_uop_entry, uop_back_pressure_get_fops(), NULL);
		(void)proc_create_data("back_enable", UBFM_FS_MODE_ALL_RO, g_uop_entry, uop_back_enable_get_fops(), NULL);
	}
}

void uop_proc_mod_uninit(void)
{
	if (g_uop_entry != NULL) {
		remove_proc_subtree(UOP_PROC_NAME, NULL);
		g_uop_entry = NULL;
	}
}

