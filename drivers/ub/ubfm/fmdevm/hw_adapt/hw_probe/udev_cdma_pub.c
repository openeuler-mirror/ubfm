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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include "mami_string.h"
#include "feature_loader.h"
#include "udm.h"
#include "udev_cdma_pub.h"
#include "uops_cdma_symbol.h"

#define LOCAL_BUF_SIZE PAGE_SIZE   /* 1 page */
#define LOCAL_BUF_ORDER 0	   /* 2^0 = 1 page */
#define TIMEOUT_CNT 10000
#define CR_CNT 1
#define LOCAL_SEG_TOKEN 0x1234
#define REMOTE_SEG_TOKEN 0xABCD

static struct udev_cdma_dev *g_hw_cdma_dev[DEV_MAX_NUM];

enum cdma_optype {
	UDEV_CDMA_READ = 0,
	UDEV_CDMA_WRITE,
};

static int cdma_poll_queue(struct udev_cdma_dev *cdma_dev)
{
	u32 loop = TIMEOUT_CNT + 1;
	struct dma_cr cr = {0};
	int cnt = 0;

	while (--loop) {
		cnt = dma_poll_queue_symbol(cdma_dev->cdev, cdma_dev->queue, CR_CNT, &cr);
		if (cnt == CR_CNT) {
			break;
		}
		udelay(100);   /* wait 100us for each poll */
	}

	if ((cnt != CR_CNT) || (cr.status != DMA_CR_SUCCESS)) {
		hw_err("Dma poll failed.(cnt=%d;status=%u;loop=%u)\n", cnt, cr.status, loop);
		return -EINVAL;
	}
	return 0;
}

static int cdma_rw_request(struct udev_cdma_dev *cdma_dev, u64 addr, void *data,
	u32 len, enum cdma_optype type)
{
	struct dma_seg *remote_seg = NULL;
	struct dma_seg_cfg seg_cfg = {0};
	int ret = -ENXIO;

	if (len > cdma_dev->seg_size) {
		hw_err("Invalid rw size. (len=%u;max_len=%u)\n", len, cdma_dev->seg_size);
		return -EINVAL;
	}

	cdma_dev->seg->len = len;
	seg_cfg.sva = (uint64_t)addr;
	seg_cfg.len = len;
	remote_seg = dma_import_seg_symbol(&seg_cfg);
	if (remote_seg == NULL) {
		hw_err("Import remote segment failed. (devid=%u)\n", cdma_dev->devid);
		return -ENOMEM;
	}

	if (type == UDEV_CDMA_READ) {
		ret = dma_read_symbol(cdma_dev->cdev, remote_seg, cdma_dev->seg, cdma_dev->queue);
	} else if (type == UDEV_CDMA_WRITE) {
		/* copy from user Avoid frequent segment creation */
		(void)ubfm_memcpy(cdma_dev->local_buf, cdma_dev->seg_size, data, len);
		ret = dma_write_symbol(cdma_dev->cdev, remote_seg, cdma_dev->seg, cdma_dev->queue);
	}
	if (ret != 0) {
		hw_err("Dma op failed. (devid=%u;op=%u;ret=%d)\n", cdma_dev->devid, type, ret);
		ret = -EIO;
		goto failed;
	}

	ret = cdma_poll_queue(cdma_dev);
	if (ret != 0) {
		hw_err("Poll queue failed. (devid=%u; ret=%d)\n", cdma_dev->devid, ret);
		ret = -EIO;
		goto failed;
	}
	/* copy to user */
	if (type == UDEV_CDMA_READ) {
		(void)ubfm_memcpy(data, len, cdma_dev->local_buf, len);
	}

	hw_debug("Hw rw success. (devid=%u; type=%u; tid=%u; len=%u)\n",
		cdma_dev->devid, type, cdma_dev->seg->tid, len);

failed:
	dma_unimport_seg_symbol(remote_seg);
	return ret;
}

static int cdma_rw_request_proc(struct udev_cdma_dev *cdma_dev, u64 addr, void *data, u32 len, enum cdma_optype type)
{
	int ret;

	mutex_lock(&cdma_dev->lock);
	ret = cdma_rw_request(cdma_dev, addr, data, len, type);
	mutex_unlock(&cdma_dev->lock);
	return ret;
}

int cdma_read(struct udev_cdma_dev *cdev, u64 addr, void *data, u32 len)
{
	int ret = 0;
	u32 i = 0;

	if (cdev == NULL) {
		hw_err("Cdev is NULL.\n");
		return -EINVAL;
	}

	for (i = 0; i < len / MAMI_CDMA_RW_ONCE; i++) {
		ret = cdma_rw_request_proc(cdev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			MAMI_CDMA_RW_ONCE, UDEV_CDMA_READ);
		if (ret != 0) {
			return ret;
		}
	}
	if ((len % MAMI_CDMA_RW_ONCE) != 0) {
		ret = cdma_rw_request_proc(cdev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			(len % MAMI_CDMA_RW_ONCE), UDEV_CDMA_READ);
	}

	return ret;
}

/* If tx or rx by cdma, the packet length is 256 bytes at most each time, which is limited by A's sram. */
int cdma_write(struct udev_cdma_dev *cdev, u64 addr, void *data, u32 len)
{
	int ret = 0;
	u32 i = 0;

	if (cdev == NULL) {
		hw_err("Cdev is NULL.\n");
		return -EINVAL;
	}

	for (i = 0; i < len / MAMI_CDMA_RW_ONCE; i++) {
		ret = cdma_rw_request_proc(cdev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			MAMI_CDMA_RW_ONCE, UDEV_CDMA_WRITE);
		if (ret != 0) {
			return ret;
		}
	}
	if ((len % MAMI_CDMA_RW_ONCE) != 0) {
		ret = cdma_rw_request_proc(cdev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			(len % MAMI_CDMA_RW_ONCE), UDEV_CDMA_WRITE);
	}

	return ret;
}

/* If tx or rx by cdma, the packet length is 256 bytes at most each time, which is limited by A's sram. */
int cdma_dev_read(u32 devid, u64 addr, void *data, u32 len)
{
	struct udev_cdma_dev *cdma_dev = NULL;

	if ((devid >= DEV_MAX_NUM) || (g_hw_cdma_dev[devid] == NULL)) {
		hw_err("Invalid devid. (devid=%u)\n", devid);
		return -EINVAL;
	}
	cdma_dev = g_hw_cdma_dev[devid];

	return cdma_read(cdma_dev, addr, data, len);
}

/* If tx or rx by cdma, the packet length is 256 bytes at most each time, which is limited by A's sram. */
int cdma_dev_write(u32 devid, u64 addr, void *data, u32 len)
{
	struct udev_cdma_dev *cdma_dev = NULL;

	if ((devid >= DEV_MAX_NUM) || (g_hw_cdma_dev[devid] == NULL)) {
		hw_err("Invalid devid. (devid=%u)\n", devid);
		return -EINVAL;
	}
	cdma_dev = g_hw_cdma_dev[devid];

	return cdma_write(cdma_dev, addr, data, len);
}

static void cdma_set_queue_cfg(struct udev_cdma_dev *cdma_dev, struct queue_cfg *cfg)
{
	cfg->queue_depth = 256;	/* default depth is 256 */
	cfg->priority = 0;		 /* default priority is 0 */
	cfg->dcna = cdma_dev->dcna;
	cfg->rmt_eid.dw0 = cdma_dev->deid;
}

static int cdma_alloc_dma_queue(u32 devid, struct udev_cdma_dev *cdma_dev)
{
	struct queue_cfg cfg = {0};

	cdma_set_queue_cfg(cdma_dev, &cfg);
	cdma_dev->queue = dma_alloc_queue_symbol(cdma_dev->cdev, cdma_dev->ctx, &cfg);
	if (cdma_dev->queue < 0) {
		hw_err("Alloc queue failed. (devid=%u;queue_id=%d)\n", devid, cdma_dev->queue);
		return -ENOMEM;
	}

	return 0;
}

static void cdma_free_dma_queue(struct udev_cdma_dev *cdma_dev)
{
	if (cdma_dev->queue >= 0) {
		dma_free_queue_symbol(cdma_dev->cdev, cdma_dev->queue);
		cdma_dev->queue = -1; /* -1 is invalid queue id */
	}
}

static int cdma_alloc_seg_buf(u32 devid, struct udev_cdma_dev *cdma_dev)
{
	struct dma_seg_cfg seg_cfg = {0};
	struct page *local_pages = NULL;
	void *local_buf = NULL;

	cdma_dev->seg_size = LOCAL_BUF_SIZE;
	local_pages = alloc_pages(GFP_KERNEL | __GFP_ACCOUNT, LOCAL_BUF_ORDER);
	if (local_pages == NULL) {
		hw_err("Alloc local buf failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	/* local_pages is non-NULL, so the local_buf obtained by offset must be non-NULL */
	local_buf = page_address(local_pages);

	seg_cfg.sva = (uint64_t)(uintptr_t)local_buf;
	seg_cfg.len = LOCAL_BUF_SIZE;
	seg_cfg.token_value_valid = false;
	cdma_dev->seg = dma_register_seg_symbol(cdma_dev->cdev, cdma_dev->ctx, &seg_cfg);
	if (cdma_dev->seg == NULL) {
		hw_err("Register seg failed. (devid=%u)\n", devid);
		__free_pages(local_pages, LOCAL_BUF_ORDER);
		local_pages = NULL;
		return -ENOMEM;
	}
	cdma_dev->local_pages = local_pages;
	cdma_dev->local_buf = local_buf;
	return 0;
}

static void cdma_free_seg_buf(struct udev_cdma_dev *cdma_dev)
{
	if (cdma_dev->seg != NULL) {
		cdma_dev->seg->len = LOCAL_BUF_SIZE;
		dma_unregister_seg_symbol(cdma_dev->cdev, cdma_dev->seg);
		cdma_dev->seg = NULL;
	}
	if (cdma_dev->local_pages != NULL) {
		cdma_dev->local_buf = NULL;
		__free_pages(cdma_dev->local_pages, LOCAL_BUF_ORDER);
		cdma_dev->local_pages = NULL;
	}
}

static void cdma_dev_free(struct udev_cdma_dev *cdma_dev)
{
	if (cdma_dev == NULL) {
		return;
	}
	cdma_free_seg_buf(cdma_dev);
	cdma_free_dma_queue(cdma_dev);
	if (cdma_dev->ctx >= 0) {
		dma_delete_context_symbol(cdma_dev->cdev, cdma_dev->ctx);
		cdma_dev->ctx = -1; /* -1 is invalid ctx id */
	}
	if (cdma_dev->dev_list != NULL) {
		dma_free_device_list_symbol(cdma_dev->dev_list, cdma_dev->cdev_num);
		cdma_dev->dev_list = NULL;
	}
	kfree(cdma_dev);
}

struct dma_device *udev_get_cdma_dev(struct udev_cdma_dev *cdma_dev, u32 seid)
{
	struct dma_device *dev_list = NULL;
	u32 num = 0, i;

	dev_list = dma_get_device_list_symbol(&num);
	if (dev_list == NULL) {
		hw_err("Failed to get dev list.\n");
		return NULL;
	}
	if (num == 0) {
		hw_err("Invalid cdma device num. (devid=%u; num=%u)\n", cdma_dev->devid, num);
		(void)dma_free_device_list_symbol(dev_list, num);
		return NULL;
	}
	for (i = 0; i < num; i++) {
		hw_info("The dev_list[%u] eid: %u\n", i, dev_list[i].attr.eid.dw0);
	}
	cdma_dev->dev_list = dev_list;
	cdma_dev->cdev_num = num;
	return &dev_list[0];
}

struct udev_cdma_dev *udev_cdma_init(u32 devid, u32 dcna, u32 deid, u32 seid)
{
	struct udev_cdma_dev *cdma_dev = NULL;
	int ret = 0;

	hw_debug("Cdma dev init start. (devid=%u;cna=%u;eid=%u)\n", devid, dcna, deid);
	cdma_dev = kzalloc(sizeof(struct udev_cdma_dev), GFP_KERNEL | __GFP_ACCOUNT);
	if (cdma_dev == NULL) {
		return NULL;
	}

	cdma_dev->devid = devid;
	cdma_dev->seid = seid;
	cdma_dev->dcna = dcna;
	cdma_dev->deid = deid;
	cdma_dev->cdev = udev_get_cdma_dev(cdma_dev, seid);
	if (cdma_dev->cdev == NULL) {
		kfree(cdma_dev);
		hw_err("Get cdma dev failed. (devid=%u;seid=%u)\n", devid, seid);
		cdma_dev = NULL;
		return NULL;
	}
	cdma_dev->ctx = dma_create_context_symbol(cdma_dev->cdev);
	if (cdma_dev->ctx < 0) {
		hw_err("Alloc context failed. (devid=%u;ctx=%d)\n", devid, cdma_dev->ctx);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	ret = cdma_alloc_dma_queue(devid, cdma_dev);
	if (ret != 0) {
		hw_err("Alloc queue failed. (devid=%u)\n", devid);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	ret = cdma_alloc_seg_buf(devid, cdma_dev);
	if (ret != 0) {
		hw_err("Alloc local buf failed. (devid=%u)\n", devid);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	mutex_init(&cdma_dev->lock);

	hw_debug("Cdma dev init success. (devid=%u;cna=%u;eid=%u)\n", devid, dcna, deid);
	return cdma_dev;
}

void udev_cdma_uninit(struct udev_cdma_dev *cdma_dev)
{
	u32 devid;

	if (cdma_dev == NULL) {
		hw_err("Cdma dev is NULL\n");
		return;
	}
	devid = cdma_dev->devid;
	mutex_destroy(&cdma_dev->lock);
	udelay(1000);	 /* sleep 1000 us waitfor cdma poll done */
	cdma_dev_free(cdma_dev);
	hw_debug("Cdma dev uninit success. (devid=%u)\n", devid);
}

int udev_cdma_init_dev(u32 devid)
{
	struct udev_cdma_dev *cdma_dev = NULL;
	struct udm_dev_para para = {0};
	int ret;

	if ((devid >= DEV_MAX_NUM) || (g_hw_cdma_dev[devid] != NULL)) {
		hw_err("Cdma dev has been initialized. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get udm para failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	cdma_dev = udev_cdma_init(devid, para.cna, para.eid, 0x1);
	if (cdma_dev == NULL) {
		hw_err("Create cdma dev failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}
	g_hw_cdma_dev[devid] = cdma_dev;
	return 0;
}

void udev_cdma_uninit_dev(u32 devid)
{
	if ((devid >= DEV_MAX_NUM) || (g_hw_cdma_dev[devid] == NULL)) {
		hw_err("Invalid devid or device has been uninit. (devid=%u)\n", devid);
		return;
	}
	udev_cdma_uninit(g_hw_cdma_dev[devid]);
	g_hw_cdma_dev[devid] = NULL;
}
