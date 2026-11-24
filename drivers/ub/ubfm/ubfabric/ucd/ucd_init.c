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
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include "ucd_define.h"
#include "ucd_feature.h"
#include "ucd_module.h"
#include "feature_loader.h"
#include "mami_string.h"
#include "ucd_acc_ctrl.h"
#include "ucd_proc_fs.h"

struct ucd_file_private_stru {
	u32 devid;
	pid_t owner_pid;
	atomic_t work_count;
};

static int ucd_ops_alloc_priv(u32 devid, struct file *filep)
{
	struct ucd_file_private_stru *priv = NULL;

	priv =
		(struct ucd_file_private_stru *)kzalloc(sizeof(struct ucd_file_private_stru), GFP_KERNEL | __GFP_ACCOUNT);
	if (priv == NULL) {
		ucd_err("kzalloc failed. (size=%lu)\n", sizeof(struct ucd_file_private_stru));
		return -ENOMEM;
	}
	priv->owner_pid = current->tgid;
	priv->devid = devid;
	filep->private_data = (void *)priv;
	atomic_set(&priv->work_count, 0);
	return 0;
}

static int ucd_ops_dev_open(struct inode *inode, struct file *filep)
{
	int ret;
	u32 devid = 0;

	if ((filep == NULL) || (inode == NULL)) {
		return -EINVAL;
	}

	devid = iminor(inode);
	if (devid > DEV_MAX_NUM) {
		return -EINVAL;
	}

	ret = ucd_ops_alloc_priv(devid, filep);
	if (ret != 0) {
		ucd_err("kzalloc failed. (size=%lu)\n", sizeof(struct ucd_file_private_stru));
		return -ENOMEM;
	}
	/* call module open */
	ret = ucd_call_dev_open(devid, current->tgid);
	if (ret != 0) {
		ucd_err("ucd_call_dev_open failed. (ret=%d)\n", ret);
		kfree(filep->private_data);
		filep->private_data = NULL;
		return ret;
	}

	if (!try_module_get(THIS_MODULE)) {
		kfree(filep->private_data);
		filep->private_data = NULL;
		return -EBUSY;
	}
	return 0;
}

static int ucd_ops_open(struct inode *inode, struct file *filep)
{
	int ret;

	if (filep == NULL) {
		return -EINVAL;
	}

	ret = ucd_ops_alloc_priv(MAMI_ANY_DEVID, filep);
	if (ret != 0) {
		ucd_err("kzalloc failed. (size=%lu)\n", sizeof(struct ucd_file_private_stru));
		return -ENOMEM;
	}
	/* call module open */
	ret = ucd_call_open(current->tgid);
	if (ret != 0) {
		ucd_err("ucd_call_dev_open failed. (ret=%d)\n", ret);
		kfree(filep->private_data);
		filep->private_data = NULL;
		return ret;
	}

	if (!try_module_get(THIS_MODULE)) {
		kfree(filep->private_data);
		filep->private_data = NULL;
		return -EBUSY;
	}
	return 0;
}

static int ucd_ops_release(struct inode *inode, struct file *filep)
{
	struct ucd_file_private_stru *priv = NULL;
	int ret = 0;

	if ((filep == NULL) || (filep->private_data == NULL)) {
		return -EINVAL;
	}
	priv = (struct ucd_file_private_stru *)filep->private_data;
	if (priv == NULL) {
		return -EBADFD;
	}

	module_put(THIS_MODULE);
	if (priv->devid == MAMI_ANY_DEVID) {
		ret = ucd_call_release(priv->owner_pid);
	} else {
		ret = ucd_call_dev_release(priv->devid, priv->owner_pid);
	}
	if (ret != 0) {
		ucd_err("ucd_call_dev_release failed. (ret=%d)\n", ret);
	}
	kfree(filep->private_data);
	filep->private_data = NULL;
	return ret;
}

unsigned int ucd_ops_msg_poll(struct file *filep, struct poll_table_struct *wait)
{
	return POLLERR;
}

static int ucd_make_feature_input(struct ucd_ioctl_arg *ctl_arg, struct tag_ucd_feature_arg *arg)
{
	int ret;
	/* means no input data */
	if ((ctl_arg->arg.input_len == 0) && (ctl_arg->arg.input == NULL)) {
		arg->input = NULL;
		arg->input_len = 0;
		return 0;
	}

	/* If the size is smaller than sizeof(u64), the default space is used and does not need to be free. */
	if ((ctl_arg->arg.input_len <= sizeof(u64)) && (ctl_arg->arg.input != NULL)) {
		arg->input_len = ctl_arg->arg.input_len;
		ret = (int)copy_from_user((void *)arg->input, (void *)ctl_arg->arg.input, (unsigned long)ctl_arg->arg.input_len);
		if (ret != 0) {
			ucd_err("copy_func_from failed. (ret=%d; size=%u)\n", ret, ctl_arg->arg.input_len);
			arg->input = NULL;
			return -EFAULT;
		}
		return 0;
	}

	/* input need alloc */
	arg->input = NULL;
	arg->input_len = 0;

	if ((ctl_arg->arg.input_len != 0) && (ctl_arg->arg.input != NULL) && (ctl_arg->arg.input_len <= UCD_MAX_INPUT_LEN)) {
		arg->input = kzalloc(ctl_arg->arg.input_len, GFP_KERNEL | __GFP_ACCOUNT);
		if (arg->input == NULL) {
			ucd_err("kzalloc failed. (size=%u)\n", ctl_arg->arg.input_len);
			return -ENOMEM;
		}
		arg->input_len = ctl_arg->arg.input_len;
		ret = (int)copy_from_user((void *)arg->input, (void *)ctl_arg->arg.input, (unsigned long)ctl_arg->arg.input_len);
		if (ret != 0) {
			ucd_err("copy_func_from failed. (ret=%d; size=%u)\n", ret, ctl_arg->arg.input_len);
			kfree(arg->input);
			arg->input = NULL;
			return -EFAULT;
		}
		return 0;
	}
	ucd_err("Input para error. (input_len=%u)\n", ctl_arg->arg.input_len);
	return -EINVAL;
}

static int ucd_make_feature_output(struct ucd_ioctl_arg *ctl_arg, struct tag_ucd_feature_arg *arg)
{
	/* means no output data */
	if (ctl_arg->arg.output_len == 0) {
		arg->output = NULL;
		arg->output_len = 0;
		return 0;
	}

	/* If the size is smaller than sizeof(u64), the default space is used and does not need to be free. */
	if ((ctl_arg->arg.output_len <= sizeof(u64)) && (ctl_arg->arg.output != NULL)) {
		arg->output_len = ctl_arg->arg.output_len;
		return 0;
	}

	arg->output = NULL;
	arg->output_len = 0;
	if ((ctl_arg->arg.output != NULL) && (ctl_arg->arg.output_len <= UCD_MAX_OUTPUT_LEN)) {
		arg->output = kzalloc(ctl_arg->arg.output_len, GFP_KERNEL | __GFP_ACCOUNT);
		if (arg->output == NULL) {
			ucd_err("kzalloc failed. (size=%u)\n", ctl_arg->arg.output_len);
			return -ENOMEM;
		}
		arg->output_len = ctl_arg->arg.output_len;
		return 0;
	}
	ucd_err("Output len error. (output_len=%u)\n", ctl_arg->arg.output_len);
	return -EINVAL;
}

int ucd_proc_feature_output(struct ucd_ioctl_arg *ctl_arg, struct tag_ucd_feature_arg *arg)
{
	int ret;

	if (ctl_arg->arg.output != NULL) {
		ret = (int)copy_to_user((void *)ctl_arg->arg.output, (void *)arg->output, (unsigned long)ctl_arg->arg.output_len);
		if (ret != 0) {
			ucd_err("copy_to_user failed. (size=%u)\n", ctl_arg->arg.output_len);
			return -EFAULT;
		}
	}

	return 0;
}

static inline void ucd_free_msg(struct tag_ucd_feature_arg *arg)
{
	/* If the size is smaller than sizeof(u64), the default space is used and does not need to be free. */
	if (arg->input != NULL && (arg->input_len > sizeof(u64))) {
		kfree(arg->input);
		arg->input = NULL;
	}
	if (arg->output != NULL && (arg->output_len > sizeof(u64))) {
		kfree(arg->output);
		arg->output = NULL;
	}
}

static inline int ucd_make_up_msg_arg(struct ucd_ioctl_arg *ctl_arg, struct tag_ucd_feature_arg *arg)
{
	int ret;

	arg->key = (ctl_arg->main_cmd << MAINCMD_LEFT) | ctl_arg->sub_cmd;

	ret = ucd_make_feature_input(ctl_arg, arg);
	if (ret != 0) {
		ucd_free_msg(arg);
		return ret;
	}

	ret = ucd_make_feature_output(ctl_arg, arg);
	if (ret != 0) {
		ucd_free_msg(arg);
		return ret;
	}
	return 0;
}

static int ucd_cmd_process(u32 devid, struct ucd_ioctl_arg *ctl_arg)
{
	int ret;
	u64 default_in = 0;
	u64 default_out = 0;
	struct tag_ucd_feature_arg arg = {0};

	arg.devid = devid;

	/* Set default space;
	 * If the size is smaller than sizeof(u64), the default space is used and does not need to be free.
	 */
	arg.input = &default_in;
	arg.output = &default_out;

	/* make up call arg */
	ret = ucd_make_up_msg_arg(ctl_arg, &arg);
	if (ret != 0) {
		ucd_err("Make up message arg failed. (ret=%d)\n", ret);
		return ret;
	}

	/* call feature process */
	if (devid == MAMI_ANY_DEVID) {
		ret = ucd_feature_process(&arg);
	} else {
		ret = ucd_dev_feature_process(&arg);
	}
	if (ret != 0) {
		ucd_free_msg(&arg);
		return ret;
	}
	/* copy out data to user */
	ret = ucd_proc_feature_output(ctl_arg, &arg);
	if (ret != 0) {
		ucd_err("Process output failed. (ret=%d; size=%u)\n", ret, ctl_arg->arg.output_len);
	}
	ucd_free_msg(&arg);
	return ret;
}


static long ucd_ops_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct ucd_ioctl_arg ctl_arg = {0};

	if ((filep == NULL) || (arg == 0)) {
		ucd_err("Invalid file_private_data.\n");
		return -EINVAL;
	}

	/* copy ioctl arg from user */
	ret = (int)copy_from_user(&ctl_arg, (void *)((uintptr_t)arg), sizeof(struct ucd_ioctl_arg));
	if (ret != 0) {
		ucd_err("copy_from_user failed.\n");
		return -EFAULT;
	}
	if (cmd == UCD_IOCTL_CMD) {
		ret = ucd_cmd_process(MAMI_ANY_DEVID, &ctl_arg);
	} else {
		ucd_err("Invalid command. (command=%u; main_cmd=%u; sub_cmd=%u)\n", cmd, ctl_arg.main_cmd, ctl_arg.sub_cmd);
		ret = -ENODEV;
	}
	return ret;
}

static long ucd_ops_dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct ucd_ioctl_arg ctl_arg = {0};
	struct ucd_file_private_stru *priv = NULL;

	if ((filep == NULL) || (arg == 0) || (filep->private_data == NULL)) {
		ucd_err("Invalid file_private_data.\n");
		return -EINVAL;
	}
	/* get priv from filep */
	priv = (struct ucd_file_private_stru *)filep->private_data;
	/* copy ioctl arg from user */
	ret = (int)copy_from_user(&ctl_arg, (void *)((uintptr_t)arg), sizeof(struct ucd_ioctl_arg));
	if (ret != 0) {
		ucd_err("copy_from_user failed.\n");
		return -EFAULT;
	}
	if (cmd == UCD_IOCTL_DEV_CMD) {
		ret = ucd_cmd_process(priv->devid, &ctl_arg);
	} else {
		ucd_err("Invalid command. (command=%u; main_cmd=%u; sub_cmd=%u)\n", cmd, ctl_arg.main_cmd, ctl_arg.sub_cmd);
		ret = -ENODEV;
	}
	return ret;
}


static int ucd_ops_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ucd_file_private_stru *priv = NULL;
	int ret;

	if ((file == NULL) || (vma == NULL)) {
		ucd_err("Invalid para. (file is null=%d; vma is null=%d)\n", (file == NULL), (vma == NULL));
		return -EINVAL;
	}

	if (file->private_data == NULL) {
		ucd_err("private_data is NULL.\n");
		return -EINVAL;
	}

	priv = (struct ucd_file_private_stru *)file->private_data;
	ret = ucd_call_dev_mmap(priv->devid, vma);
	if (ret != 0) {
		ucd_err("call dev mmap failed. (ret=%d)\n", ret);
		return ret;
	}
	return 0;
}

/* use for /dev/manager */
const struct file_operations g_ucd_file_operations = {
	.owner = THIS_MODULE,
	.open = ucd_ops_open,
	.release = ucd_ops_release,
	.poll = ucd_ops_msg_poll,
	.unlocked_ioctl = ucd_ops_ioctl,
};

/* use for /dev/ubfabricX */
const struct file_operations g_ucd_dev_file_operations = {
	.owner = THIS_MODULE,
	.open = ucd_ops_dev_open,
	.release = ucd_ops_release,
	.poll = ucd_ops_msg_poll,
	.unlocked_ioctl = ucd_ops_dev_ioctl,
	.mmap = ucd_ops_dev_mmap,
};

const struct file_operations *ucd_get_module_ops(void)
{
	return &g_ucd_file_operations;
}
EXPORT_SYMBOL_GPL(ucd_get_module_ops);

const struct file_operations *ucd_get_module_dev_ops(void)
{
	return &g_ucd_dev_file_operations;
}
EXPORT_SYMBOL_GPL(ucd_get_module_dev_ops);

int ucd_init(void)
{
	int ret;

#ifndef EMU_ST
	ret = ucd_proc_ctrl_init();
	if (ret != 0) {
		ucd_err("Failed to init proc ctrl. (ret=%d)\n", ret);
		return ret;
	}
#endif

	ret = ucd_module_list_init();
	if (ret) {
		ucd_err("Failed to init ucd kv. (ret=%d)\n", ret);
		return ret;
	}

	ret = ucd_feature_init();
	if (ret) {
		ucd_err("Failed to init feature. (ret=%d)\n", ret);
		ucd_module_list_uninit();
		return ret;
	}
	ucd_proc_fs_init();

	ucd_info("ucd init success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ucd_init, FEATURE_LOADER_STAGE_0);

void ucd_uninit(void)
{
	ucd_proc_fs_uninit();
	ucd_feature_uninit();
	ucd_module_list_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(ucd_uninit, FEATURE_LOADER_STAGE_0);
