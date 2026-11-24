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
#include <linux/mutex.h>	/* mutex */
#include <linux/slab.h>	 /* kzalloc */
#include <linux/sched.h>

#include "udm_dev.h"
#include "udm_notifier.h"

static struct mutex g_udm_dev_mutex;
static rwlock_t g_udm_dev_rwlock;
static struct udm_dev_inst *g_dev_insts[DEV_MAX_NUM]; /* 8byte * 128 = 1KB. */
static u32 g_local_device_id = UDM_INVALID_UDEVID;
struct udm_dev_inst *udm_dev_inst_get(u32 udevid)
{
	struct udm_dev_inst *dev_inst = NULL;

	if (udevid >= DEV_MAX_NUM) {
		return NULL;
	}

	read_lock_bh(&g_udm_dev_rwlock);
	dev_inst = g_dev_insts[udevid];
	if (dev_inst != NULL) {
		kref_get(&dev_inst->ref);
	}
	read_unlock_bh(&g_udm_dev_rwlock);

	return dev_inst;
}

u32 udm_get_local_devid(void)
{
	return g_local_device_id;
}
EXPORT_SYMBOL_GPL(udm_get_local_devid);

static void udm_destroy_dev_inst(struct udm_dev_inst *dev_inst)
{
	if (dev_inst->para.location == UDM_LOCAL) {
		udm_info("Remove local device. (udevid=%u %u)\n", dev_inst->udevid, g_local_device_id);
		g_local_device_id = UDM_INVALID_UDEVID;
	}
	kfree(dev_inst);
	dev_inst = NULL;
}

static void udm_dev_inst_release(struct kref *kref)
{
	struct udm_dev_inst *dev_inst = container_of(kref, struct udm_dev_inst, ref);

	udm_info("The device removes successfully. (udevid=%u)\n", dev_inst->udevid);
	udm_destroy_dev_inst(dev_inst);
}

void udm_dev_inst_put(struct udm_dev_inst *dev_inst)
{
	kref_put(&dev_inst->ref, udm_dev_inst_release);
}

bool udm_is_udevid_exist(u32 udevid)
{
	struct udm_dev_inst *dev_inst = udm_dev_inst_get(udevid);

	if (dev_inst == NULL) {
		return false;
	}
	udm_dev_inst_put(dev_inst);

	return true;
}
EXPORT_SYMBOL_GPL(udm_is_udevid_exist);

static int udm_alloc_udevid(u32 start, u32 end, u32 *udevid) /* [start, end) */
{
	u32 i;

	for (i = start; i < end; i++) {
		if (g_dev_insts[i] == NULL) {
			*udevid = i;
			return 0;
		}
	}

	return -ENOSPC;
}

static struct udm_dev_inst *udm_create_dev_inst(struct udm_dev_para *para)
{
	static u32 inst_id;
	struct udm_dev_inst *dev_inst = kzalloc(sizeof(struct udm_dev_inst), GFP_KERNEL | __GFP_ACCOUNT);

	if (dev_inst == NULL) {
		udm_err("Out of memory. (udevid=%u)\n", para->udevid);
		return NULL;
	}

	dev_inst->mng_proc_tgid = UDM_INVALID_TGID;
	dev_inst->para = *para;
	dev_inst->inst_id = inst_id++;
	kref_init(&dev_inst->ref);

	return dev_inst;
}

static int _udm_add_dev(struct udm_dev_para *para, u32 *udevid)
{
	int ret;
	struct udm_dev_inst *dev_inst = NULL;

	if (udm_is_udevid_exist(para->udevid)) {
		udm_debug("The udevid already exists. (udevid=%u)\n", para->udevid);
		return -EEXIST;
	}

	*udevid = para->udevid;
	if (para->udevid == UDM_INVALID_UDEVID) {   /* The udevid is assigned by udm. */
		ret = udm_alloc_udevid(0, DEV_MAX_NUM, udevid);
		if (ret != 0) {
			udm_err("No available udevid.\n");
			return ret;
		}
	}

	dev_inst = udm_create_dev_inst(para);
	if (dev_inst == NULL) {
		return -ENOMEM;
	}
	dev_inst->udevid = *udevid;

	ret = udm_create_cdev(dev_inst->udevid, &dev_inst->cdev);
	if (ret != 0) {
		udm_destroy_dev_inst(dev_inst);
		return ret;
	}
	g_dev_insts[*udevid] = dev_inst;
	if (dev_inst->para.location == UDM_LOCAL) {
		udm_info("Add local device. (udevid=%u)\n", dev_inst->udevid);
		g_local_device_id = *udevid;
	}
	udm_info("The device is added successfully. (udevid=%u)\n", *udevid);
	return 0;
}

int udm_add_dev(struct udm_dev_para *para, u32 *udevid)
{
	int ret;

	if ((para == NULL) || (udevid == NULL)) {
		udm_err("Invalid parameter. (is_para_null=%d; is_udevid_null=%d)\n", (para == NULL), (udevid == NULL));
		return -EINVAL;
	}

	ret = udm_dev_para_valid_check(para);
	if (ret != 0) {
		return ret;
	}

	mutex_lock(&g_udm_dev_mutex);
	ret = _udm_add_dev(para, udevid);
	mutex_unlock(&g_udm_dev_mutex);

	if (ret == 0) {
		(void)udm_notifier_call(*udevid, UDM_INIT);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(udm_add_dev);

static void _udm_remove_dev(struct udm_dev_inst *dev_inst)
{
	u32 udevid = dev_inst->udevid;

	udm_cdev_remove_dev(udevid, &dev_inst->cdev);

	write_lock_bh(&g_udm_dev_rwlock);
	g_dev_insts[udevid] = NULL;  /* set inst invalid, so other thread will not get it */
	write_unlock_bh(&g_udm_dev_rwlock);
	kref_put(&dev_inst->ref, udm_dev_inst_release);

	udm_info("The device is deleted successfully. (udevid=%u)\n", udevid);
}

int udm_remove_dev(u32 udevid)
{
	struct udm_dev_inst *dev_inst = NULL;

	dev_inst = udm_dev_inst_get(udevid);
	if (dev_inst == NULL) {
		udm_info("Devid not exist. (udevid=%u)\n", udevid);
		return 0;
	}
	(void)udm_notifier_call(udevid, UDM_UNINIT);

	mutex_lock(&g_udm_dev_mutex);
	_udm_remove_dev(dev_inst);
	mutex_unlock(&g_udm_dev_mutex);

	udm_dev_inst_put(dev_inst);
	return 0;
}
EXPORT_SYMBOL_GPL(udm_remove_dev);

int udm_get_dev_attr(u32 udevid, struct udm_dev_para *dev_para)
{
	struct udm_dev_inst *dev_inst = NULL;

	if (dev_para == NULL) {
		udm_err("The dev_para is NULL.\n");
		return -EINVAL;
	}

	dev_inst = udm_dev_inst_get(udevid);
	if (dev_inst == NULL) {
		udm_err("Invalid udevid. (udevid=%u)\n", udevid);
		return -EINVAL;
	}
	*dev_para = dev_inst->para;

	udm_dev_inst_put(dev_inst);
	return 0;
}
EXPORT_SYMBOL_GPL(udm_get_dev_attr);

int udm_set_dev_attr(u32 udevid, struct udm_dev_para *dev_para)
{
	struct udm_dev_inst *dev_inst = NULL;

	if (dev_para == NULL) {
		udm_err("The dev_para is NULL.\n");
		return -EINVAL;
	}

	dev_inst = udm_dev_inst_get(udevid);
	if (dev_inst == NULL) {
		udm_err("Invalid udevid. (udevid=%u)\n", udevid);
		return -EINVAL;
	}
	dev_inst->para = *dev_para;

	udm_dev_inst_put(dev_inst);
	return 0;
}
EXPORT_SYMBOL_GPL(udm_set_dev_attr);

int udm_set_managed_proc_pid(u32 udevid)
{
	struct udm_dev_inst *dev_inst = NULL;
	int ret = 0;

	dev_inst = udm_dev_inst_get(udevid);
	if (dev_inst == NULL) {
		udm_err("Invalid udevid. (udevid=%u)\n", udevid);
		return -EINVAL;
	}

	mutex_lock(&g_udm_dev_mutex);
	if (dev_inst->mng_proc_tgid == UDM_INVALID_TGID) {
		dev_inst->mng_proc_tgid = current->tgid;
		udm_info("udm set managed proc pid success. (udevid=%u; mng_proc_tgid=%d)\n", udevid, current->tgid);
	} else if (dev_inst->mng_proc_tgid != current->tgid) {
		udm_err("Device is already managed by another process. (udevid=%u; current_tgid=%d; managed_tgid=%d)\n",
			udevid, current->tgid, dev_inst->mng_proc_tgid);
		ret = -EEXIST;
	}
	mutex_unlock(&g_udm_dev_mutex);
	udm_dev_inst_put(dev_inst);

	return ret;
}
EXPORT_SYMBOL_GPL(udm_set_managed_proc_pid);

int udm_dev_release(u32 udevid, pid_t pid)
{
	struct udm_dev_inst *dev_inst = NULL;
	int ret = 0;
	bool is_managed = false;

	dev_inst = udm_dev_inst_get(udevid);
	if (dev_inst == NULL) {
		udm_info("Device already removed. (dev_id=%u)\n", udevid);
		return 0;
	}

	mutex_lock(&g_udm_dev_mutex);
	if (dev_inst->mng_proc_tgid == pid) {
		is_managed = true;
		dev_inst->mng_proc_tgid = UDM_INVALID_TGID;
	}
	mutex_unlock(&g_udm_dev_mutex);

	if (is_managed) {
		udm_info("Start to release the device. (dev_id=%u; pid=%d)\n", udevid, pid);
		ret = udm_remove_dev(udevid);
		if (ret != 0) {
			udm_err("UDM remove dev failed. (dev_id=%u; ret=%d)\n", udevid, ret);
		}
	}

	udm_dev_inst_put(dev_inst);

	return ret;
}
EXPORT_SYMBOL_GPL(udm_dev_release);

void udm_dev_init(void)
{
	mutex_init(&g_udm_dev_mutex);
	rwlock_init(&g_udm_dev_rwlock);
}

void udm_dev_uninit(void)
{
	u32 udevid;

	for (udevid = 0; udevid < DEV_MAX_NUM; udevid++) {
		if (g_dev_insts[udevid] != NULL) {
			udm_destroy_dev_inst(g_dev_insts[udevid]);
		}
	}
	mutex_destroy(&g_udm_dev_mutex);
}
