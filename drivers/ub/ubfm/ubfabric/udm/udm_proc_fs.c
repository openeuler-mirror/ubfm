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
#include <linux/version.h>

#include "fmdevm.h"
#include "udm_dev.h"
#include "udm_notifier.h"
#include "udm_cdev.h"
#include "ubfm_dfx_fs.h"

static inline const char *udm_get_chip_name(u32 chip_type)
{
	static const char *chip_name[MAMI_KERN_TYPE_MAX] = {
		[MAMI_KERN_TYPE_INVALID] = "unknown",
		[MAMI_KERN_TYPE_ASCEND_V1] = "ascend_v1",
		[MAMI_KERN_TYPE_KUNPENG950_1P] = "kunpeng950_1P",
		[MAMI_KERN_TYPE_UNIONS] = "unions",
		[MAMI_KERN_TYPE_KUNPENG950_2P] = "kunpeng950_2P",
		[MAMI_KERN_TYPE_KUNPENG950_4P] = "kunpeng950_4P",
		[MAMI_KERN_TYPE_ASCEND_V2] = "ascend_v2",
		[MAMI_KERN_TYPE_UNIONS_210] = "unions_210",
		[MAMI_KERN_TYPE_KUNPENG960_1P] = "kunpeng960_1P",
		[MAMI_KERN_TYPE_KUNPENG960_2P] = "kunpeng960_2P",
		[MAMI_KERN_TYPE_KUNPENG960_4P] = "kunpeng960_4P",
		[MAMI_KERN_TYPE_1825] = "1825",
		[MAMI_KERN_TYPE_HSSU_V100] = "HSSU_V100",
	};

	return ((chip_type > MAMI_KERN_TYPE_INVALID) && (chip_type < MAMI_KERN_TYPE_MAX)) ? chip_name[chip_type] : chip_name[0];
}

static void udm_udev_single_show(struct seq_file *seq, struct udm_dev_inst *dev_inst)
{
	char guid_str[GUID_STR_LEN] = {0};

	(void)udev_guid_to_str(dev_inst->para.guid, guid_str);
	seq_printf(seq, "udevid[%u] cna[%u] eid[%u] location[%u] chip_type[%s]\n",
		dev_inst->udevid, dev_inst->para.cna, dev_inst->para.eid, dev_inst->para.location,
		udm_get_chip_name(dev_inst->para.chip_type));
	seq_printf(seq, "            guid[%s]\n", guid_str);
	seq_puts(seq, "\n");
}

static int udm_udev_show(struct seq_file *seq, void *offset)
{
	int i;
	struct udm_dev_inst *dev_inst = NULL;

	for (i = 0; i < DEV_MAX_NUM; i++) {
		dev_inst = udm_dev_inst_get(i);
		if (dev_inst != NULL) {
			udm_udev_single_show(seq, dev_inst);
			udm_dev_inst_put(dev_inst);
		}
	}

	return 0;
}

int udm_udev_open(struct inode *inode, struct file *file)
{
	return single_open(file, udm_udev_show, inode->i_private);
}

static void _udm_notifier_show(struct seq_file *seq, struct udm_notifiers *notifiers)
{
	int i;
	struct list_head *nf_head = NULL;
	struct udm_notifier_node *nf = NULL;

	down_read(&notifiers->sem);
	for (i = 0; i < UDM_PRI_MAX; i++) {
		nf_head = &notifiers->pri_head[i];

		seq_printf(seq, "    pri %d\n", i);
		if (!list_empty(nf_head)) {
			list_for_each_entry(nf, nf_head, node) {
				seq_printf(seq, "        notifier: %s %u %u\n", nf->notifier, nf->call_count, nf->call_finish);
			}
		}
	}
	up_read(&notifiers->sem);
	seq_puts(seq, "\n");
}

static int udm_notifier_show(struct seq_file *seq, void *offset)
{
	_udm_notifier_show(seq, udm_get_notifiers());
	return 0;
}

int udm_notifier_open(struct inode *inode, struct file *file)
{
	return single_open(file, udm_notifier_show, inode->i_private);
}

static int udm_create_delay_flag_show_probe(struct seq_file *seq, void *offset)
{
	u32 i = 0;
	bool state = true;

	for (i = 0; i < DEV_MAX_NUM; i++) {
		state = udm_get_create_delay_flag(i);
		seq_printf(seq, "%u:%d\n", i, state);
	}
	return 0;
}

int udm_create_delay_flag_open(struct inode *inode, struct file *file)
{
	return single_open(file, udm_create_delay_flag_show_probe, inode->i_private);
}

#define ECHO_WRITE_LEN 5
ssize_t udm_create_delay_flag_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos)
{
	char str[ECHO_WRITE_LEN] = {0};
	char *token1 = NULL, *token2 = str;
	int devid, state;

	if ((ppos == NULL) || (*ppos != 0) || (count >= sizeof(str)) || (ubuf == NULL)) {
		return -EINVAL;
	}
	if (copy_from_user(str, ubuf, count) != 0) {
		return -ENOMEM;
	}
	str[count] = '\0';
	token1 = strsep(&token2, ":");
	if (token1 == NULL || *token1 == '\0') {
		return -EINVAL;
	}
	if (token2 == NULL || *token2 == '\0') {
		return -EINVAL;
	}
	if (kstrtoint(token1, 10, &devid) != 0) {
		return -EINVAL;
	}
	if (kstrtoint(token2, 10, &state)) {
		return -EINVAL;
	}
	if (((u32)devid < DEV_MAX_NUM) && (state == CREATE_FLAG_TRUE || state == CREATE_FLAG_FALSE)) {
		udm_set_create_delay_flag((u32)devid, state);
	}
	return count;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
static const struct file_operations g_udm_udevice_ops = {
	.owner = THIS_MODULE,
	.open	= udm_udev_open,
	.read	= seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations g_udm_notifier_ops = {
	.owner = THIS_MODULE,
	.open	= udm_notifier_open,
	.read	= seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations g_udm_dev_state_ops = {
	.owner = THIS_MODULE,
	.open	= udm_create_delay_flag_open,
	.read	= seq_read,
	.write   = udm_create_delay_flag_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

#else

static const struct proc_ops g_udm_udevice_ops = {
	.proc_open	= udm_udev_open,
	.proc_read	= seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops g_udm_notifier_ops = {
	.proc_open	= udm_notifier_open,
	.proc_read	= seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops g_udm_dev_state_ops = {
	.proc_open	= udm_create_delay_flag_open,
	.proc_read    = seq_read,
	.proc_write   = udm_create_delay_flag_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
#endif

void udm_proc_fs_init(void)
{
	struct proc_dir_entry *top_entry = proc_mkdir("mami/udm", NULL);

	if (top_entry != NULL) {
		(void)proc_create_data("udevice", UBFM_FS_MODE_OWNER_RO, top_entry, &g_udm_udevice_ops, NULL);
		(void)proc_create_data("notifiers", UBFM_FS_MODE_OWNER_RO, top_entry, &g_udm_notifier_ops, NULL);
		(void)proc_create_data("create_delay", UBFM_FS_MODE_OWNER_RW, top_entry, &g_udm_dev_state_ops, NULL);
	}
}

void udm_proc_fs_uninit(void)
{
	(void)remove_proc_subtree("mami/udm", NULL);
}
