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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "udev_hw_pub.h"
#include "feature_loader.h"
#include "udev_espi_pub.h"
#include "ubfm_dfx_fs.h"

#ifndef EMU_ST
#ifdef MAMI_ESL_MODE
static struct platform_device g_pdev;
#endif

#define HW_PROBE_PROC_NAME "mami/hw_probe"
static struct proc_dir_entry *g_hw_probe_entry;
static DEFINE_MUTEX(g_probe_lock);
static int g_probe_already;
#define UDEV_ESPI_DEV_PROBE 1
#define udev_local_dev_init 2

static int hw_procfs_show_probe(struct seq_file *seq, void *offset)
{
	seq_puts(seq, "echo 1 > probe\n");
	return 0;
}

static int hw_probe_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hw_procfs_show_probe, inode->i_private);
}

#define ECHO_WRITE_LEN 5
static ssize_t hw_adapt_write_probe(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char str[ECHO_WRITE_LEN] = {0};
	int val;

	if ((ppos == NULL) || (*ppos != 0) || (count > sizeof(str)) || (ubuf == NULL)) {
		hw_err("Invalid parameters.\n");
		return -EINVAL;
	}
	if (copy_from_user(str, ubuf, count) != 0) {
		hw_err("Copy from user error.\n");
		return -ENOMEM;
	}
	str[ECHO_WRITE_LEN - 1] = '\0';
	mutex_lock(&g_probe_lock);
	if (g_probe_already) {
		hw_err("Has already probe.\n");
		mutex_unlock(&g_probe_lock);
		return -EAGAIN;
	}
	if (kstrtoint(str, 10, &val) != 0) {  /* 10:Decimal */
		hw_err("String to int failed. (str=%s)\n", str);
		mutex_unlock(&g_probe_lock);
		return -EFAULT;
	}
	if (val == UDEV_ESPI_DEV_PROBE) {
#ifdef MAMI_ESL_MODE
		if (udev_espi_probe(&g_pdev) != 0) {
			hw_err("Espi probe failed.\n");
			mutex_unlock(&g_probe_lock);
			return -EFAULT;
		}
		g_probe_already = val;
#endif
	}
	mutex_unlock(&g_probe_lock);
	return count;
}

static ssize_t hw_adapt_write_chip_type(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char str[ECHO_WRITE_LEN] = {0};
	u32 chip_type;

	if ((ppos == NULL) || (*ppos != 0) || (count > sizeof(str)) || (ubuf == NULL)) {
		hw_err("Invalid parameters.\n");
		return -EINVAL;
	}
	if (copy_from_user(str, ubuf, count) != 0) {
		hw_err("Copy from user error.\n");
		return -ENOMEM;
	}
	str[ECHO_WRITE_LEN - 1] = '\0';
	mutex_lock(&g_probe_lock);
	if (g_probe_already) {
		hw_err("Has already probe, cannot specify chip type again.\n");
		mutex_unlock(&g_probe_lock);
		return -EAGAIN;
	}
	mutex_unlock(&g_probe_lock);
	if (kstrtou32(str, 10, &chip_type) != 0) {  /* 10:Decimal */
		hw_err("String to int failed. (str=%s)\n", str);
		return -EFAULT;
	}
#ifdef MAMI_ESL_MODE
	if (udev_espi_specify_chip_type(chip_type) != 0) {
		hw_err("Specifying failed, chip type is invalid.\n");
		return -EINVAL;
	}
#endif

	return count;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations probe = {
	.owner = THIS_MODULE,
	.open	= hw_probe_proc_open,
	.read	= seq_read,
	.write = hw_adapt_write_probe,
	.llseek  = seq_lseek,
	.release = single_release,
};
static const struct file_operations chip_type = {
	.owner = THIS_MODULE,
	.open	= hw_probe_proc_open,
	.read	= seq_read,
	.write = hw_adapt_write_chip_type,
	.llseek  = seq_lseek,
	.release = single_release,
};
#else
static const struct proc_ops probe = {
	.proc_open	= hw_probe_proc_open,
	.proc_read	= seq_read,
	.proc_write = hw_adapt_write_probe,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
static const struct proc_ops chip_type = {
	.proc_open	= hw_probe_proc_open,
	.proc_read	= seq_read,
	.proc_write = hw_adapt_write_chip_type,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
#endif

int hw_probe_proc_init(void)
{
	int ret = 0;

	mutex_lock(&g_probe_lock);
	g_hw_probe_entry = proc_mkdir(HW_PROBE_PROC_NAME, NULL);
	if (g_hw_probe_entry != NULL) {
		proc_create_data("probe", UBFM_FS_MODE_OWNER_RW, g_hw_probe_entry, &probe, NULL);
		proc_create_data("chip_type", UBFM_FS_MODE_OWNER_RW, g_hw_probe_entry, &chip_type, NULL);
	}

	g_probe_already = 0;
	mutex_unlock(&g_probe_lock);
	return ret;
}

void hw_probe_proc_uninit(void)
{
	if (g_hw_probe_entry != NULL) {
		(void)remove_proc_subtree(HW_PROBE_PROC_NAME, NULL);
	}
	if (g_probe_already == UDEV_ESPI_DEV_PROBE) {
#ifdef MAMI_ESL_MODE
		udev_espi_remove(&g_pdev);
		g_probe_already = 0;
#endif
	}
}

int udev_debug_procfs_init(void)
{
	return hw_probe_proc_init();
}
DECLARE_FEATURE_AUTO_INIT(udev_debug_procfs_init, FEATURE_LOADER_STAGE_9);

void udev_debug_procfs_uninit(void)
{
	hw_probe_proc_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(udev_debug_procfs_uninit, FEATURE_LOADER_STAGE_9);
#else
int udev_debug_procfs_init(void)
{
	return 0;
}
#endif
