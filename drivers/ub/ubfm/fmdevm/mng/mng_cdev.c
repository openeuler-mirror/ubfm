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
#include <linux/uidgid.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include "mng_kernel.h"
#include "ucd.h"


#define MNG_CDEV_NUM 1

#define CDEV_MODE_PERMISSION	0660

static kuid_t g_cdev_uid = {1201}; /* mamiUser's uid=1201 */
static kgid_t g_cdev_gid = {1201}; /* mamiUser's gid=1201 */

struct mng_cdev_info {
	dev_t devno;
	struct class *dev_class;
	struct cdev *cdev;
	struct device *dev;
};

static int mng_init_devno_class(dev_t *pdevno, struct class **pdev_class)
{
	int ret;
	dev_t devno;
	struct class *dev_class = NULL;

	ret = alloc_chrdev_region(&devno, 0, MNG_CDEV_NUM, "mami_ubdevm_cdev");
	if (ret) {
		mng_err("Alloc chrdev region failed. (ret=%d)\n", ret);
		return ret;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	dev_class = class_create("mami_ubdevm_class");
#else
	dev_class = class_create(THIS_MODULE, "mami_ubdevm_class");
#endif
	if (IS_ERR(dev_class)) {
		unregister_chrdev_region(devno, MNG_CDEV_NUM);
		mng_err("Create class failed.\n");
		return -EFAULT;
	}
	*pdevno = devno;
	*pdev_class = dev_class;
	return 0;
}

static void mng_uninit_devno_class(dev_t *pdevno, struct class **pdev_class)
{
	class_destroy(*pdev_class);
	*pdev_class = NULL;

	unregister_chrdev_region(*pdevno, MNG_CDEV_NUM);
	*pdevno = 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static char *mng_devnode(const struct device *dev, umode_t *mode, kuid_t *uid, kgid_t *gid)
#else
static char *mng_devnode(struct device *dev, umode_t *mode, kuid_t *uid, kgid_t *gid)
#endif
{
	if (mode != NULL) {
		*mode = CDEV_MODE_PERMISSION;
	}
	if ((uid != NULL) && (gid != NULL)) {
		*uid = g_cdev_uid;
		*gid = g_cdev_gid;
	}
	return NULL;
}

static void mng_device_release(struct device *dev)
{
	mng_debug("Free dev mem.\n");
	kfree(dev);
}

static const struct device_type g_mng_dev_type = {
	.devnode = mng_devnode,
};

static int mng_init_cdev_device(dev_t devno, struct class *dev_class, struct cdev **pcdev, struct device **pdev)
{
	int ret;
	struct cdev *cdev = NULL;
	struct device *dev = NULL;

	/* init cdev */
	cdev = cdev_alloc();
	if (cdev == NULL) {
		mng_err("Alloc cdev failed.\n");
		return -ENOMEM;
	}
	cdev->owner = THIS_MODULE;
	cdev->ops = ucd_get_module_ops();

	/* init dev */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		cdev_del(cdev);
		mng_err("Alloc dev failed.\n");
		return -ENOMEM;
	}
	device_initialize(dev);
	dev->devt = devno;
	dev->class = dev_class;
	dev->type = &g_mng_dev_type;
	dev->release = mng_device_release;
	(void)kobject_set_name(&dev->kobj, "ubdevm");

	/* attach device to cdev */
	cdev->kobj.parent = &dev->kobj;
	ret = cdev_add(cdev, dev->devt, 1);
	if (ret != 0) {
		mng_err("Cdev add failed. (ret=%d)\n", ret);
		goto err_cdev;
	}
	ret = device_add(dev); /* cdev_add and cdev_alloc are Rollback by cdev_del */
	if (ret != 0) {
		mng_err("Cdev add device failed. (ret=%d)\n", ret);
		goto err_dev;
	}
	*pdev = dev;
	*pcdev = cdev;
	return 0;

err_dev:
	cdev_del(cdev);
err_cdev:
	put_device(dev);
	return ret;
}

static void mng_uninit_cdev_device(struct cdev **pcdev, struct device **pdev)
{
	device_del(*pdev);
	cdev_del(*pcdev);
	put_device(*pdev);
	*pcdev = NULL;
	*pdev = NULL;
}

static struct mng_cdev_info g_mng_cdev_info;

int mng_create_cdev(void)
{
	int ret;

	ret = mng_init_devno_class(&g_mng_cdev_info.devno, &g_mng_cdev_info.dev_class);
	if (ret != 0) {
		return ret;
	}
	ret = mng_init_cdev_device(g_mng_cdev_info.devno, g_mng_cdev_info.dev_class,
		&g_mng_cdev_info.cdev, &g_mng_cdev_info.dev);
	if (ret != 0) {
		mng_err("init cdev device fail, will uninit devno class.(ret=%d)\n", ret);
		mng_uninit_devno_class(&g_mng_cdev_info.devno, &g_mng_cdev_info.dev_class);
		return ret;
	}
	return 0;
}

void mng_destroy_cdev(void)
{
	mng_uninit_cdev_device(&g_mng_cdev_info.cdev, &g_mng_cdev_info.dev);
	mng_uninit_devno_class(&g_mng_cdev_info.devno, &g_mng_cdev_info.dev_class);
}

