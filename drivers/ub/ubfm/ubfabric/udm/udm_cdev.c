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
#include <linux/version.h>  /* KERNEL_VERSION */
#include <linux/slab.h>	 /* kzalloc/kfree */
#include <linux/delay.h>	/* msleep */

#include "mami_string.h"
#include "ucd.h"
#include "udm_dev.h"
#include "udm_cdev.h"
#include "udm_pub_def.h"

#define DEV_MODE_PERMISSION 0660
#define CREATE_FLAG_MAX_CNT 100
#define CREATE_FLAG_TIME_MS 10

static dev_t g_udm_dev;
static struct class *g_udm_class;

static kuid_t g_dev_uid = {1201}; /* mamiUser's uid=1201 */
static kgid_t g_dev_gid = {1201}; /* mamiUser's gid=1201 */
static umode_t g_dev_mode = DEV_MODE_PERMISSION;

static bool g_create_delay[DEV_MAX_NUM] = { [0 ... DEV_MAX_NUM - 1] = false };

void udm_set_create_delay_flag(u32 devid, bool flag)
{
	g_create_delay[devid] = flag;
}

bool udm_get_create_delay_flag(u32 devid)
{
	return g_create_delay[devid];
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static char *udm_devnode(const struct device *dev, umode_t *mode, kuid_t *uid, kgid_t *gid)
#else
static char *udm_devnode(struct device *dev, umode_t *mode, kuid_t *uid, kgid_t *gid)
#endif
{
	if (dev != NULL) {
		if (mode != NULL) {
			*mode = g_dev_mode;
		}

		if ((uid != NULL) && (gid != NULL)) {
			*uid = g_dev_uid;
			*gid = g_dev_gid;
		}
	}

	return NULL;
}

const struct device_type g_udm_type = {
	.devnode = udm_devnode,
};

static void udm_device_release(struct device *dev)
{
	udm_debug("To free dev memory. (udevid=%u)\n", MINOR(dev->devt));
	kfree(dev);
	dev = NULL;
}

static void udm_cdev_set_parent(struct cdev *cdev, struct kobject *kobj)
{
	cdev->kobj.parent = kobj;
}

static int udm_cdev_device_add(u32 devid, struct cdev *cdev, struct device *dev)
{
	int ret;
	u32 i = 0;

/* Outband scenario: Ensure the character device has been created successfully */
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	for (i = 0; i < CREATE_FLAG_MAX_CNT; i++) {
		if (g_create_delay[devid] != 0) {
			msleep(CREATE_FLAG_TIME_MS);
		} else {
			break;
		}
	}
	if (i >= CREATE_FLAG_MAX_CNT) {
		udm_warn("create cdev timeout! (devid=%u, i=%u)\n", devid, i);
	} else {
		udm_debug("create cdev success. (devid=%u, i=%u)\n", devid, i);
	}
#else
	(void)devid;
	(void)i;
#endif

	udm_cdev_set_parent(cdev, &dev->kobj);

	ret = cdev_add(cdev, dev->devt, 1);
	if (ret != 0) {
		udm_err("Failed to add cdev. (ret=%d)\n", ret);
		return ret;
	}

	return device_add(dev); /* cdev_add and cdev_alloc are Rollback by cdev_del */
}

int udm_create_cdev(u32 udevid, struct udm_cdev *cdev)
{
	int ret;
	const char *brief_name = "ubfabric";
	struct device *dev = NULL;

	(void)UBFM_SPRINTF(cdev->name, UDM_DEV_NAME_LEN, "/dev/%s%u", brief_name, udevid);
	cdev->devno = MKDEV(MAJOR(g_udm_dev), udevid);
	mutex_init(&cdev->mutex);

	/* init cdev */
	cdev->cdev = cdev_alloc();
	if (cdev->cdev == NULL) {
		udm_err("Failed to invoke cdev_alloc. (udevid=%u)\n", udevid);
		return -ENOMEM;
	}
	cdev->cdev->owner = THIS_MODULE;
	cdev->cdev->ops = ucd_get_module_dev_ops();

	/* init dev */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL | __GFP_ACCOUNT);
	if (dev == NULL) {
		cdev_del(cdev->cdev);
		cdev->cdev = NULL;
		udm_err("Failed to alloc memory for dev. (udevid=%u)\n", udevid);
		return -ENOMEM;
	}
	device_initialize(dev);
	dev->devt = cdev->devno;
	dev->class = g_udm_class;
	dev->type = &g_udm_type;
	dev->release = udm_device_release;
	(void)kobject_set_name(&dev->kobj, "%s%u", brief_name, udevid);
	cdev->dev = dev;

	/* attach device to cdev */
	ret = udm_cdev_device_add(udevid, cdev->cdev, cdev->dev);
	if (ret != 0) {
		cdev_del(cdev->cdev);
		kfree(cdev->dev);
		cdev->dev = NULL;
		udm_err("Failed to add device to cdev. (udevid=%u; ret=%d)\n", udevid, ret);
		return ret;
	}

	return 0;
}

static int udm_cdev_class_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&g_udm_dev, 0, DEV_MAX_NUM, "mami-cdev");
	if (ret) {
		udm_err("Failed to alloc memory for char dev region. (ret=%d)\n", ret);
		return ret;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	g_udm_class = class_create("mami-class");
#else
	g_udm_class = class_create(THIS_MODULE, "mami-class");
#endif
	if (IS_ERR(g_udm_class)) {
		unregister_chrdev_region(g_udm_dev, DEV_MAX_NUM);
		udm_err("Failed to create class for dev.\n");
		return -EFAULT;
	}

	return 0;
}

static void udm_cdev_class_uninit(void)
{
	class_destroy(g_udm_class);
	unregister_chrdev_region(g_udm_dev, DEV_MAX_NUM);
}

static void udm_cdev_device_del(u32 devid, struct cdev *cdev, struct device *dev)
{
	g_create_delay[devid] = CREATE_FLAG_TRUE;
	device_del(dev);
	cdev_del(cdev);
}

static void udm_remove_cdev(u32 udevid, struct udm_cdev *cdev)
{
	udm_cdev_device_del(udevid, cdev->cdev, cdev->dev);
	put_device(cdev->dev);
}

void udm_cdev_remove_dev(u32 udevid, struct udm_cdev *cdev)
{
	mutex_lock(&cdev->mutex);
	udm_remove_cdev(udevid, cdev);
	mutex_unlock(&cdev->mutex);
}

static void udm_recycle_all_cdev(void)
{
	u32 udevid;

	for (udevid = 0; udevid < DEV_MAX_NUM; udevid++) {
		struct udm_dev_inst *dev_inst = udm_dev_inst_get(udevid);

		if (dev_inst != NULL) {
			udm_cdev_remove_dev(udevid, &dev_inst->cdev);
			udm_dev_inst_put(dev_inst);
		}
	}
}

int udm_cdev_init(void)
{
	int ret;

	ret = udm_cdev_class_init();
	if (ret != 0) {
		return ret;
	}

	return 0;
}

void udm_cdev_uninit(void)
{
	udm_recycle_all_cdev();
	udm_cdev_class_uninit();
}
