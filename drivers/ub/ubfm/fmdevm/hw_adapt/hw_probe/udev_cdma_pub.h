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
#ifndef _UDEV_CDMA_PUB_H
#define _UDEV_CDMA_PUB_H

#include <linux/mutex.h>
#include "ubfm_common.h"
#include "udev_hw_pub.h"
#include "ures.h"
#include "ub/cdma/cdma_api.h"

struct udev_cdma_dev {
	u32 devid;
	struct dma_device *cdev;
	struct dma_device *dev_list;
	u32 cdev_num;
	int ctx;	/* a process has a context */
	struct queue_cfg cfg;
	int queue;
	u32 seid;
	u32 dcna;
	u32 deid;
	u32 seg_size;
	struct dma_seg *seg;
	struct page *local_pages;
	void *local_buf;
	struct mutex lock;
};

int cdma_dev_read(u32 devid, u64 addr, void *data, u32 len);
int cdma_dev_write(u32 devid, u64 addr, void *data, u32 len);
int udev_cdma_init_dev(u32 devid);
void udev_cdma_uninit_dev(u32 devid);

int cdma_read(struct udev_cdma_dev *cdev, u64 addr, void *data, u32 len);
int cdma_write(struct udev_cdma_dev *cdev, u64 addr, void *data, u32 len);
struct udev_cdma_dev *udev_cdma_init(u32 devid, u32 dcna, u32 deid, u32 seid);
void udev_cdma_uninit(struct udev_cdma_dev *cdma_dev);

#endif
