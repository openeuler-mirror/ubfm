/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef UDM_CDEV_H__
#define UDM_CDEV_H__

#include <linux/cdev.h> /* struct cdev */

#define CREATE_FLAG_TRUE 1
#define CREATE_FLAG_FALSE 0
#define UDM_DEV_NAME_LEN 32
struct udm_cdev {
	char name[UDM_DEV_NAME_LEN];
	struct device *dev;
	struct cdev *cdev;
	dev_t devno;
	struct mutex mutex;
};

int udm_create_cdev(u32 udevid, struct udm_cdev *cdev);
void udm_cdev_remove_dev(u32 udevid, struct udm_cdev *cdev);

int udm_cdev_init(void);
void udm_cdev_uninit(void);

void udm_set_create_delay_flag(u32 devid, bool flag);
bool udm_get_create_delay_flag(u32 devid);
#endif
