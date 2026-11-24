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
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/io.h>

#include "mgmtq_proc.h"
#include "linkcom_pub.h"
#include "linkcom_log.h"
#include "linkcom_mng.h"
#include "udie_v1_linkcom_soc.h"
#include "uop.h"
#include "linkcom_mgmtq_irq.h"
#include "ubfm_dfx_fs.h"

#define MGMTQ_PROC_NAME   "mami/mgmtq"

static struct proc_dir_entry *g_mgmtq_entry;

void mgmtq_dev_show(u32 devid, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	struct linkcom_id_inst id_inst;
	u32 die_id, chan, chan_id, cpu_qid;

	dev_inst = linkcom_get_dev_inst(devid);
	if (dev_inst == NULL) {
		return;
	}

	for (die_id = 0; die_id < dev_inst->die_num; die_id++) {
		for (chan = 0; chan < LINKCOM_DIE_MAX_CHAN_NUM; chan++) {
			chan_id = die_id * LINKCOM_DIE_MAX_CHAN_NUM + chan;
			if (chan_id >= dev_inst->chan_num) {
				linkcom_put_dev_inst(devid);
				return;
			}
			chan_inst = &dev_inst->chan_inst[chan_id];

			id_inst.dev_id = devid;
			id_inst.die_id = die_id;
			id_inst.que_id = chan_inst->que_id;
			cpu_qid = (chan_id % LINKCOM_DIE_MAX_CHAN_NUM) + 1;
			id_inst.udie_version = dev_inst->udie_version;
			show(priv, "Mgmtq show:[dev-%u;die-%u;chan-%u;inst_qid-%u;cpu_qid=%u;cnt=%u]:\n",
					devid, die_id, chan_id, id_inst.que_id, cpu_qid, chan_inst->head.count);
			mgmtq_show(&id_inst, show, priv);
			show(priv, "\n");
		}
	}
	linkcom_put_dev_inst(devid);
}

static inline void *mgmtq_proc_get_data(struct inode *inode)
{
	void *data = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
	data = pde_data(inode);
#else
	data = PDE_DATA(inode);
#endif
	return data;
}

static void mgmtq_proc_print(void *priv, const char *fmt, ...)
{
	struct seq_file *seq = (struct seq_file *)priv;
	va_list args;

	if (likely(seq != NULL)) {
		va_start(args, fmt);
		seq_vprintf(seq, fmt, args);  /* use seq_vprintf to handle variable arguments */
		va_end(args);
	}
}

static int mgmtq_proc_show(struct seq_file *seq, void *offset)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		mgmtq_dev_show(devid, mgmtq_proc_print, seq);
	}

	return 0;
}

static int mgmtq_outband_enable_show(struct seq_file *seq, void *offset)
{
	seq_printf(seq, "%d\n", linkcom_get_outband_enable());
	return 0;
}

int mgmtq_outband_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmtq_outband_enable_show, mgmtq_proc_get_data(inode));
}

#define OUTBAND_ENABLE 1
#define OUTBAND_DISABLE 0

ssize_t mgmtq_outband_enable_write(struct file *filp, const char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char str[5] = {0};
	int val;

	if (ppos == NULL || *ppos != 0 || count > sizeof(str) || ubuf == NULL) {
		return -EINVAL;
	}
	if (copy_from_user(str, ubuf, count) != 0) {
		return -ENOMEM;
	}
	str[4] = '\0';
	if (kstrtoint(str, 10, &val) != 0) {
		return -EINVAL;
	}
	if (val == OUTBAND_ENABLE || val == OUTBAND_DISABLE) {
		linkcom_set_outband_enable(val);
	} else {
		return -EINVAL;
	}
	return count;
}

int mgmtq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgmtq_proc_show, mgmtq_proc_get_data(inode));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *mgmtq_proc_get_fops(void)
{
	static const struct file_operations mgmtq_proc_ops = {
		.owner = THIS_MODULE,
		.open	= mgmtq_proc_open,
		.read	= seq_read,
		.llseek  = seq_lseek,
		.release = single_release,
	};

	return &mgmtq_proc_ops;
}
#else
static const struct proc_ops *mgmtq_proc_get_fops(void)
{
	static const struct proc_ops mgmtq_proc_ops = {
		.proc_open	= mgmtq_proc_open,
		.proc_read	= seq_read,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};

	return &mgmtq_proc_ops;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations *mgmtq_outband_enable_get_fops(void)
{
	static const struct file_operations mgmtq_proc_ops = {
		.owner   = THIS_MODULE,
		.open	= mgmtq_outband_enable_open,
		.read	= seq_read,
		.write   = mgmtq_outband_enable_write,
		.llseek  = seq_lseek,
		.release = single_release,
	};

	return &mgmtq_proc_ops;
}
#else
static const struct proc_ops *mgmtq_outband_enable_get_fops(void)
{
	static const struct proc_ops mgmtq_proc_ops = {
		.proc_open	= mgmtq_outband_enable_open,
		.proc_read	= seq_read,
		.proc_write   = mgmtq_outband_enable_write,
		.proc_lseek   = seq_lseek,
		.proc_release = single_release,
	};

	return &mgmtq_proc_ops;
}
#endif

int mgmtq_proc_init(void)
{
	g_mgmtq_entry = proc_mkdir(MGMTQ_PROC_NAME, NULL);
	if (g_mgmtq_entry != NULL) {
		(void)proc_create_data("mgmtq", UBFM_FS_MODE_OWNER_RO, g_mgmtq_entry, mgmtq_proc_get_fops(), NULL);
		(void)proc_create_data("outband_enable", UBFM_FS_MODE_OWNER_RO, g_mgmtq_entry,
				   mgmtq_outband_enable_get_fops(), NULL);
	}
	return 0;
}

void mgmtq_proc_uninit(void)
{
	if (g_mgmtq_entry != NULL) {
		remove_proc_subtree(MGMTQ_PROC_NAME, NULL);
		g_mgmtq_entry = NULL;
	}
}
