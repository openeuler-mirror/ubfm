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
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>	 /* kzalloc */

#include "mami_string.h"
#include "ucd.h"
#include "ucd_module.h"
#include "ucd_define.h"

struct ucd_module_cb {
	struct list_head list;
	char module_name[UCD_MODULE_NAME_MAX];
	void *open;
	void *release;
	void *mmap;
};

struct list_head g_ucd_module_chain;
struct list_head g_ucd_module_dev_chain;
struct rw_semaphore g_ucd_module_sem;

static bool check_input_para(const char *module, void *open, void *release, void *mmap)
{
	if (module == NULL) {
		ucd_err("Module name is NULL\n");
		return false;
	}

	if ((open != NULL && release != NULL) || (mmap != NULL)) {
		return true;
	}

	return false;
}

static int _register_module_ops(struct list_head *head, const char *module, void *open, void *release, void *mmap)
{
	struct ucd_module_cb *cb = NULL;
	int ret;

	if (!check_input_para(module, open, release, mmap)) {
		return -EINVAL;
	}

	cb = (struct ucd_module_cb *)kzalloc(
		sizeof(struct ucd_module_cb),
		GFP_KERNEL | __GFP_ACCOUNT);
	if (cb == NULL) {
		ucd_err("kzalloc failed. (module_name=\"%s\"; size=%lu)\n",
			module,
			sizeof(struct ucd_module_cb));
		return -ENOMEM;
	}

	ret = ubfm_strcpy(cb->module_name, UCD_MODULE_NAME_MAX, module);
	if (ret != 0) {
		ucd_err("ubfm_strcpy error. (module_name=\"%s\"; ret=%d)\n", module, ret);
		kfree(cb);
		cb = NULL;
		return ret;
	}

	if (open != NULL && release != NULL) {
		cb->open = open;
		cb->release = release;
	}

	if (mmap != NULL) {
		cb->mmap = mmap;
	}
	down_write(&g_ucd_module_sem);
	list_add(&cb->list, head);
	up_write(&g_ucd_module_sem);
	return 0;
}

int ucd_register_module_ops(const char *module, ucd_open open, ucd_release release)
{
	return _register_module_ops(&g_ucd_module_chain, module, (void *)open, (void *)release, NULL);
}
EXPORT_SYMBOL_GPL(ucd_register_module_ops);

/* Currently mmap is only used by ued, therefore */
int ucd_register_module_dev_ops(const char *module, ucd_dev_open open, ucd_dev_release release, ucd_dev_mmap mmap)
{
	return _register_module_ops(&g_ucd_module_dev_chain, module, (void *)open, (void *)release, (void *)mmap);
}
EXPORT_SYMBOL_GPL(ucd_register_module_dev_ops);

static int _unregister_module_ops(struct list_head *head, const char *module)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;

	if (module == NULL) {
		ucd_err("Input parameter is null.\n");
		return -EINVAL;
	}

	down_write(&g_ucd_module_sem);
	if (list_empty(head)) {
		up_write(&g_ucd_module_sem);
		return 0;
	}

	/* Release module */
	list_for_each_entry_safe(node_info, node_info_next, head, list) {
		if (strncmp(node_info->module_name, module, UCD_MODULE_NAME_MAX) == 0) {
			list_del(&node_info->list);
			up_write(&g_ucd_module_sem);
			kfree(node_info);
			node_info = NULL;
			return 0;
		}
	}
	up_write(&g_ucd_module_sem);
	return  0;
}

int ucd_unregister_module_ops(const char *module)
{
	return _unregister_module_ops(&g_ucd_module_chain, module);
}
EXPORT_SYMBOL_GPL(ucd_unregister_module_ops);

int ucd_unregister_module_dev_ops(const char *module)
{
	return _unregister_module_ops(&g_ucd_module_dev_chain, module);
}
EXPORT_SYMBOL_GPL(ucd_unregister_module_dev_ops);

int ucd_call_open(pid_t owner_pid)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;
	int ret;
	ucd_open fn = NULL;

	down_read(&g_ucd_module_sem);
	if (list_empty(&g_ucd_module_chain)) {
		up_read(&g_ucd_module_sem);
		return 0;
	}

	/* Release module */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_chain, list) {
		if (node_info->open == NULL) {
			continue;
		}
		fn = node_info->open;
		ret = fn(owner_pid);
		if (ret != 0) {
			ucd_err("call open failed. (owner_pid=%d)\n", owner_pid);
		}
	}
	up_read(&g_ucd_module_sem);
	return  0;
}

int ucd_call_release(pid_t owner_pid)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;
	int ret;
	ucd_release fn = NULL;

	down_read(&g_ucd_module_sem);
	if (list_empty(&g_ucd_module_chain)) {
		up_read(&g_ucd_module_sem);
		return 0;
	}

	/* release */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_chain, list) {
		if (node_info->release == NULL) {
			continue;
		}
		fn = node_info->release;
		ret = fn(owner_pid);
		if (ret != 0) {
			ucd_err("call release failed. (owner_pid=%d)\n", owner_pid);
		}
	}
	up_read(&g_ucd_module_sem);
	return  0;
}


int ucd_call_dev_open(u32 devid, pid_t owner_pid)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;
	int ret;
	ucd_dev_open fn = NULL;

	down_read(&g_ucd_module_sem);
	if (list_empty(&g_ucd_module_dev_chain)) {
		up_read(&g_ucd_module_sem);
		return 0;
	}

	/* open */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_dev_chain, list) {
		if (node_info->open == NULL) {
			continue;
		}
		fn = node_info->open;
		ret = fn(devid, owner_pid);
		if (ret != 0) {
			ucd_err("call open failed. (owner_pid=%d)\n", owner_pid);
			up_read(&g_ucd_module_sem);
			return ret;
		}
	}
	up_read(&g_ucd_module_sem);
	return  0;
}


int ucd_call_dev_release(u32 devid, pid_t owner_pid)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;
	ucd_dev_release fn = NULL;

	down_read(&g_ucd_module_sem);
	if (list_empty(&g_ucd_module_dev_chain)) {
		up_read(&g_ucd_module_sem);
		return 0;
	}

	/* release */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_dev_chain, list) {
		if (node_info->release == NULL) {
			continue;
		}
		fn = node_info->release;
		(void)fn(devid, owner_pid);
	}
	up_read(&g_ucd_module_sem);
	return  0;
}

int ucd_call_dev_mmap(u32 devid, struct vm_area_struct *vma)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;
	int ret;
	ucd_dev_mmap fn = NULL;

	down_read(&g_ucd_module_sem);
	if (list_empty(&g_ucd_module_dev_chain)) {
		up_read(&g_ucd_module_sem);
		return 0;
	}

	/* mmap */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_dev_chain, list) {
		if (node_info->mmap == NULL) {
			continue;
		}
		fn = node_info->mmap;
		ret = fn(devid, vma);
		if (ret != 0) {
			ucd_err("call mmap failed. (devid=%u)\n", devid);
		}
	}
	up_read(&g_ucd_module_sem);
	return 0;
}

int ucd_module_list_init(void)
{
	INIT_LIST_HEAD(&g_ucd_module_chain);
	INIT_LIST_HEAD(&g_ucd_module_dev_chain);
	init_rwsem(&g_ucd_module_sem);
	return 0;
}

int ucd_module_list_uninit(void)
{
	struct ucd_module_cb *node_info = NULL;
	struct ucd_module_cb *node_info_next = NULL;

	down_write(&g_ucd_module_sem);
	/* Release module */
	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_chain, list) {
		list_del(&node_info->list);
		kfree(node_info);
		node_info = NULL;
	}

	list_for_each_entry_safe(node_info, node_info_next, &g_ucd_module_dev_chain, list) {
		list_del(&node_info->list);
		kfree(node_info);
		node_info = NULL;
	}
	up_write(&g_ucd_module_sem);
	return  0;
}

