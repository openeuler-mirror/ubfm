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
#include <linux/time.h>
#include <linux/mm.h>
#include "mami_string.h"

#include "udm.h"
#include "ued.h"
#include "ures.h"
#include "fmdevm.h"
#include "uop_log.h"
#include "uops_kern.h"
#include "ubfabric.h"

#include "uops_cdma.h"
#include "uops_cdma_symbol.h"

#define LOCAL_BUF_SIZE PAGE_SIZE   /* 1 page */
#define LOCAL_BUF_ORDER 0	   /* 2^0 = 1 page */

#ifdef FPGA_MODE_ENABLE
#define TIMEOUT_CNT 200000
#else
#define TIMEOUT_CNT 10000
#endif

#ifdef MAMI_ESL_MODE
#define CMDA_DELAY_TIME 100
#else
#define CMDA_DELAY_TIME 1
#endif
#define CR_CNT 1
#define LOCAL_SEG_TOKEN 0x1234
#define REMOTE_SEG_TOKEN 0xABCD
#define SEC_PER_MIN 60ULL

enum cdma_jetty_type {
	CDMA_JETTY_ROL = 2,
	CDMA_JETTY_ROI,
	CDMA_JETTY_TYPE_RESERVED,
};

enum cdma_status {
	UOP_CDMA_PRE_RESET = 0,
	UOP_CDMA_INVALID,
	UOP_CDMA_VALID,
};

struct uops_cdma_dev {
	u32 devid;
	struct dma_device *cdev;
	struct dma_device *dev_list;
	u32 cdev_num;
	int ctx;	/* a process has a context */
	struct queue_cfg cfg;
	int queue;
	bool queue_valid;
	u64 queue_alloc_timestamp;
	u32 seid;
	u32 dcna;
	u32 deid;
	u32 seg_size;
	struct dma_seg *seg;
	struct page *local_pages;
	void *local_buf;
	struct mutex lock;
	enum cdma_status status;
};

static struct uops_cdma_dev *g_cdma_dev[DEV_MAX_NUM];

struct mutex g_ctrl_lock;
struct list_head g_dev_ctrl_list = LIST_HEAD_INIT(g_dev_ctrl_list);
struct cdma_dev_ctrl {
	struct list_head list_node;
	u32 seid;
	u32 dcna;
	u32 deid;
	u32 devid;
};

static int cdma_add_dev_ctrl(u32 devid, u32 seid, u32 deid, u32 dcna)
{
	struct cdma_dev_ctrl *dev;

	mutex_lock(&g_ctrl_lock);
	list_for_each_entry(dev, &g_dev_ctrl_list, list_node) {
		if ((dev->seid == seid) && (dev->devid == devid)) {
			uop_err("Repeat add dev ctrl. (seid=%u;devid=%u)\n", seid, devid);
			mutex_unlock(&g_ctrl_lock);
			return -EEXIST;
		}
	}
	dev = kmalloc(sizeof(struct cdma_dev_ctrl), GFP_KERNEL);
	if (dev == NULL) {
		uop_err("Alloc memory failed. (seid=%u;devid=%u)\n", seid, devid);
		mutex_unlock(&g_ctrl_lock);
		return -ENOMEM;
	}
	dev->seid = seid;
	dev->devid = devid;
	dev->deid = deid;
	dev->dcna = dcna;
	list_add_tail(&dev->list_node, &g_dev_ctrl_list);
	mutex_unlock(&g_ctrl_lock);
	return 0;
}

static void cdma_del_dev_ctrl(u32 devid)
{
	struct cdma_dev_ctrl *dev;

	mutex_lock(&g_ctrl_lock);
	list_for_each_entry(dev, &g_dev_ctrl_list, list_node) {
		if (dev->devid == devid) {
			list_del(&dev->list_node);
			kfree(dev);
			break;
		}
	}
	mutex_unlock(&g_ctrl_lock);
}

static void cdma_set_queue_cfg(struct uops_cdma_dev *cdma_dev, struct queue_cfg *cfg)
{
	cfg->queue_depth = 256;	  /* default depth is 256 */
	cfg->priority = 0;		 /* default priority is 0 */
	cfg->dcna = cdma_dev->dcna;
	cfg->rmt_eid.dw0 = cdma_dev->deid;
	cfg->trans_mode = cdma_dev->cfg.trans_mode;
}

static int cdma_alloc_dma_queue(u32 devid, struct uops_cdma_dev *cdma_dev)
{
	struct queue_cfg cfg = {0};

	cdma_set_queue_cfg(cdma_dev, &cfg);
	cdma_dev->queue = dma_alloc_queue_symbol(cdma_dev->cdev, cdma_dev->ctx, &cfg);
	if (cdma_dev->queue < 0) {
		uop_err("Alloc queue failed. (devid=%u;queue_id=%d)\n", devid, cdma_dev->queue);
		return -ENOMEM;
	}
	cdma_dev->queue_alloc_timestamp = (u64)ktime_get_seconds();
	cdma_dev->queue_valid = true;
	return 0;
}

static void cdma_free_dma_queue(struct uops_cdma_dev *cdma_dev)
{
	cdma_dev->queue_valid = false;
	if (cdma_dev->queue >= 0) {
		dma_free_queue_symbol(cdma_dev->cdev, cdma_dev->queue);
		cdma_dev->queue = -1; /* -1 is invalid queue id */
	}
}

static void cdma_flush_queue(struct uops_cdma_dev *cdma_dev, u64 timeout)
{
	if ((u64)ktime_get_seconds() - cdma_dev->queue_alloc_timestamp < timeout) {
		return;
	}
	cdma_free_dma_queue(cdma_dev);
	(void)cdma_alloc_dma_queue(cdma_dev->devid, cdma_dev);
}

#define CDMA_READ 0
#define CDMA_WRITE 1

static int cdma_poll_queue(struct uops_cdma_dev *cdma_dev)
{
	u32 loop = TIMEOUT_CNT + 1;
	struct dma_cr cr = {0};
	int cnt = 0;

	while (--loop) {
		if (cdma_dev->status != UOP_CDMA_VALID) {
			return -ENODEV;
		}
		cnt = dma_poll_queue_symbol(cdma_dev->cdev, cdma_dev->queue, CR_CNT, &cr);
		if (cnt == CR_CNT) {
			break;
		}
		udelay(CMDA_DELAY_TIME);   /* wait 1us for each poll, esl wait 100us */
	}

	if ((cnt != CR_CNT) || (cr.status != DMA_CR_SUCCESS)) {
		uop_err_limit("Dma poll failed.(cnt=%d;status=%u;loop=%u)\n", cnt, cr.status, loop);
		cdma_flush_queue(cdma_dev, SEC_PER_MIN);
		return -EINVAL;
	}
	return 0;
}

static int cdma_rw_request(struct uops_cdma_dev *cdma_dev, u64 addr, void *data,
	u32 len, u32 type)
{
	struct dma_seg *remote_seg = NULL;
	struct dma_seg_cfg seg_cfg = {0};
	int ret = -ENXIO;

	if ((len > cdma_dev->seg_size) || (cdma_dev->seg == NULL)) {
		uop_err("Invalid rw size or seg is null. (len=%u;max_len=%u)\n", len, cdma_dev->seg_size);
		return -EINVAL;
	}

	if (!cdma_dev->queue_valid) {
		uop_err_limit("cdma_dev queue is invalid. (devid=%u)\n", cdma_dev->devid);
		return -EFAULT;
	}

	cdma_dev->seg->len = len;
	seg_cfg.sva = addr;
	seg_cfg.len = len;
	remote_seg = dma_import_seg_symbol(&seg_cfg);
	if (remote_seg == NULL) {
		uop_err("Import remote segment failed. (devid=%u)\n", cdma_dev->devid);
		return -ENOMEM;
	}

	if (type == CDMA_READ) {
		ret = dma_read_symbol(cdma_dev->cdev, remote_seg, cdma_dev->seg, cdma_dev->queue);
	} else if (type == CDMA_WRITE) {
		/* copy from user Avoid frequent segment creation */
		(void)ubfm_memcpy(cdma_dev->local_buf, cdma_dev->seg_size, data, len);
		ret = dma_write_symbol(cdma_dev->cdev, remote_seg, cdma_dev->seg, cdma_dev->queue);
	}
	if (ret != 0) {
		uop_err_limit("Dma op failed. (devid=%u; op=%u; len=%u; ret=%d)\n",
			cdma_dev->devid, type, len, ret);
		cdma_flush_queue(cdma_dev, 0);
		ret = -EIO;
		goto failed;
	}

	ret = cdma_poll_queue(cdma_dev);
	if (ret != 0) {
		uop_err_limit("Poll queue failed. (devid=%u; type=%u; len=%u; ret=%d)\n",
			cdma_dev->devid, type, len, ret);
		ret = -EIO;
		goto failed;
	}
	/* copy to user */
	if (type == CDMA_READ) {
		(void)ubfm_memcpy(data, len, cdma_dev->local_buf, len);
	}

failed:
	dma_unimport_seg_symbol(remote_seg);
	return ret;
}

static int cdma_rw_request_proc(struct uops_cdma_dev *cdma_dev, u64 addr, void *data, u32 len, u32 type)
{
	int ret;

	if ((cdma_dev == NULL) || (cdma_dev->status != UOP_CDMA_VALID)) {
		return -ENODEV;
	}
	mutex_lock(&cdma_dev->lock);
	ret = cdma_rw_request(cdma_dev, addr, data, len, type);
	mutex_unlock(&cdma_dev->lock);
	return ret;
}

static int cdma_alloc_seg_buf(u32 devid, struct uops_cdma_dev *cdma_dev)
{
	struct dma_seg_cfg seg_cfg = {0};
	struct page *local_pages = NULL;
	void *local_buf = NULL;

	cdma_dev->seg_size = LOCAL_BUF_SIZE;
	local_pages = alloc_pages(GFP_KERNEL | __GFP_ACCOUNT, LOCAL_BUF_ORDER);
	if (local_pages == NULL) {
		uop_err("Alloc local buf failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}
	local_buf = page_address(local_pages);
	if (local_buf == NULL) {
		uop_err("Map pages failed. (devid=%u)\n", devid);
		__free_pages(local_pages, LOCAL_BUF_ORDER);
		local_pages = NULL;
		return -ENOMEM;
	}

	seg_cfg.sva = (uint64_t)(uintptr_t)local_buf;
	seg_cfg.len = LOCAL_BUF_SIZE;
	seg_cfg.token_value_valid = false;
	cdma_dev->seg = dma_register_seg_symbol(cdma_dev->cdev, cdma_dev->ctx, &seg_cfg);
	if (cdma_dev->seg == NULL) {
		uop_err("Register seg failed. (devid=%u)\n", devid);
		__free_pages(local_pages, LOCAL_BUF_ORDER);
		local_pages = NULL;
		return -ENOMEM;
	}
	cdma_dev->local_pages = local_pages;
	cdma_dev->local_buf = local_buf;
	return 0;
}

static void cdma_free_seg_buf(struct uops_cdma_dev *cdma_dev)
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

static void cdma_dev_free(struct uops_cdma_dev *cdma_dev)
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

struct dma_device *uops_get_cdma_dev(struct uops_cdma_dev *cdma_dev, u32 seid)
{
	struct dma_device *dev_list = NULL;
	u32 num = 0, i;

	dev_list = dma_get_device_list_symbol(&num);
	if (dev_list == NULL) {
		uop_err("Failed to get dev list. (devid=%u)\n", cdma_dev->devid);
		return NULL;
	}
	if (num == 0) {
		uop_err("Invalid cdma device num. (devid=%u; num=%u)\n", cdma_dev->devid, num);
		(void)dma_free_device_list_symbol(dev_list, num);
		return NULL;
	}
	for (i = 0; i < num; i++) {
		uop_info("dev_list[%u] eid: %u\n", i, dev_list[i].attr.eid.dw0);
	}
	cdma_dev->dev_list = dev_list;
	cdma_dev->cdev_num = num;
	return &dev_list[0];
}

static struct uops_cdma_dev *cdma_init(u32 devid, u32 dcna, u32 deid, u32 seid)
{
	struct uops_cdma_dev *cdma_dev = NULL;
	struct udev_hw_identify hw = {0};
	int ret = 0;

	uop_info("Cdma dev init start. (devid=%u)\n", devid);
	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		uop_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	cdma_dev = kzalloc(sizeof(struct uops_cdma_dev), GFP_KERNEL | __GFP_ACCOUNT);
	if (cdma_dev == NULL) {
		return NULL;
	}

	cdma_dev->devid = devid;
	cdma_dev->dcna = dcna;
	cdma_dev->deid = deid;
	cdma_dev->cdev = uops_get_cdma_dev(cdma_dev, seid);
	cdma_dev->cfg.trans_mode = (hw.chip_type == MAMI_KERN_TYPE_1825 ? CDMA_JETTY_ROL : CDMA_JETTY_ROI);
	if (cdma_dev->cdev == NULL) {
		kfree(cdma_dev);
		uop_err("Failed to get cdma dev. (devid=%u; seid=%u)\n", devid, seid);
		cdma_dev = NULL;
		return NULL;
	}
	cdma_dev->seid = cdma_dev->cdev->attr.eid.dw0;
	cdma_dev->ctx = dma_create_context_symbol(cdma_dev->cdev);
	if (cdma_dev->ctx < 0) {
		uop_err("Alloc context failed. (devid=%u;ctx=%d)\n", devid, cdma_dev->ctx);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	uop_info("cdma alloc queue. (devid=%u; dcna=%u; deid=0x%x)\n", devid, dcna, deid);
	ret = cdma_alloc_dma_queue(devid, cdma_dev);
	if (ret != 0) {
		uop_err("Alloc queue failed. (devid=%u)\n", devid);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	ret = cdma_alloc_seg_buf(devid, cdma_dev);
	if (ret != 0) {
		uop_err("Alloc local buf failed. (devid=%u)\n", devid);
		cdma_dev_free(cdma_dev);
		return NULL;
	}

	mutex_init(&cdma_dev->lock);
	cdma_dev->status = UOP_CDMA_VALID;
	g_cdma_dev[devid] = cdma_dev;

	uop_info("Cdma dev init success. (devid=%u;cna=%u;eid=%u)\n", devid, dcna, deid);
	return cdma_dev;
}

static void cdma_uninit(u32 devid)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	if (cdma_dev == NULL) {
		uop_err("Cdma dev is NULL\n");
		return;
	}
	cdma_dev->status = UOP_CDMA_INVALID;
	udelay(1000);	 /* sleep 1000 us waitfor cdma poll done */
	devid = cdma_dev->devid;
	mutex_destroy(&cdma_dev->lock);
	cdma_dev_free(cdma_dev);
	g_cdma_dev[devid] = NULL;
	uop_info("Cdma dev uninit success. (devid=%u)\n", devid);
}

static inline void cdma_submit_event_to_user(u32 devid, u32 sub_event)
{
	struct ued_published_event event = {0};
	int ret;

	event.event_id = UED_EVENT_DEVM_CTRL_MSG;
	event.subevent_id = sub_event;
	event.msg_len = 0;
	event.msg = NULL;
	ret = ued_submit_event_to_dev(devid, &event);
	if (ret != 0) {
		uop_err("submit event failed. (devid=%u;event=%u;sub_event=%u)\n", devid, event.event_id, event.subevent_id);
	}
}

static void cdma_stop(u32 devid)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	if (cdma_dev == NULL) {
		uop_err("Cdma dev is NULL\n");
		return;
	}
	cdma_dev->status = UOP_CDMA_PRE_RESET;
	cdma_submit_event_to_user(devid, UBDEVM_CDMA_REMOVE_SUB_EVENT);
	uop_info("Cdma dev pre reset. (devid=%u)\n", devid);
}

static int uops_cdma_init_dev(u32 devid)
{
	struct uops_cdma_dev *cdma_dev = NULL;
	struct udm_dev_para para = {0};
	int ret;

	if ((devid >= DEV_MAX_NUM) || (g_cdma_dev[devid] != NULL)) {
		uop_err("Cdma dev has been initialized. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		uop_err("Get udm para failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	cdma_dev = cdma_init(devid, para.cna, para.eid, 0x1);
	if (cdma_dev == NULL) {
		uop_err("Failed to create cdma dev. (devid=%u)\n", devid);
		return -ENOMEM;
	}
	ret = cdma_add_dev_ctrl(cdma_dev->devid, cdma_dev->seid, cdma_dev->deid, cdma_dev->dcna);
	if (ret != 0) {
		cdma_uninit(devid);
		cdma_dev = NULL;
		return ret;
	}
	return 0;
}

static void uops_cdma_uninit_dev(u32 devid)
{
	if ((devid >= DEV_MAX_NUM) || (g_cdma_dev[devid] == NULL)) {
		uop_err("Invalid devid or device has been uninit. (devid=%u)\n", devid);
		return;
	}
	cdma_del_dev_ctrl(devid);
	cdma_uninit(devid);
	g_cdma_dev[devid] = NULL;
}

static int cdma_mem_read(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	int ret;
	u32 i = 0;
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	for (i = 0; i < len / MAMI_CDMA_RW_ONCE; i++) {
		ret = cdma_rw_request_proc(cdma_dev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			MAMI_CDMA_RW_ONCE, CDMA_READ);
		if (ret != 0) {
			return ret;
		}
	}
	if ((len % MAMI_CDMA_RW_ONCE) != 0) {
		ret = cdma_rw_request_proc(cdma_dev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			(len % MAMI_CDMA_RW_ONCE), CDMA_READ);
	}

	return ret;
}

static int cdma_mem_write(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	int ret;
	u32 i = 0;
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	for (i = 0; i < len / MAMI_CDMA_RW_ONCE; i++) {
		ret = cdma_rw_request_proc(cdma_dev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			MAMI_CDMA_RW_ONCE, CDMA_WRITE);
		if (ret != 0) {
			return ret;
		}
	}
	if ((len % MAMI_CDMA_RW_ONCE) != 0) {
		ret = cdma_rw_request_proc(cdma_dev, (addr + i * MAMI_CDMA_RW_ONCE), (data + i * MAMI_CDMA_RW_ONCE),
			(len % MAMI_CDMA_RW_ONCE), CDMA_WRITE);
	}

	return ret;
}

static int cdma_readl(u32 devid, u64 addr, u32 *val, void *user_ctx)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	return cdma_rw_request_proc(cdma_dev, addr, val, sizeof(u32), CDMA_READ);
}

static int cdma_writel(u32 devid, u64 addr, u32 val, void *user_ctx)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	return cdma_rw_request_proc(cdma_dev, addr, &val, sizeof(u32), CDMA_WRITE);
}

static int cdma_readq(u32 devid, u64 addr, u64 *val, void *user_ctx)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	return cdma_rw_request_proc(cdma_dev, addr, val, sizeof(u64), CDMA_READ);
}

static int cdma_writeq(u32 devid, u64 addr, u64 val, void *user_ctx)
{
	struct uops_cdma_dev *cdma_dev = g_cdma_dev[devid];

	UNUSED(user_ctx);
	return cdma_rw_request_proc(cdma_dev, addr, &val, sizeof(u64), CDMA_WRITE);
}

static const struct uop_ops g_cdma_ops = {
	.mem_read = cdma_mem_read,
	.mem_write = cdma_mem_write,
	.readl = cdma_readl,
	.writel = cdma_writel,
	.readq = cdma_readq,
	.writeq = cdma_writeq,
	.init_dev = uops_cdma_init_dev,
	.uninit_dev = uops_cdma_uninit_dev
};

static int cdma_dev_probe(u32 eid)
{
	struct cdma_dev_ctrl *dev;
	struct cdma_dev_ctrl *tmp = NULL;

	mutex_lock(&g_ctrl_lock);
	list_for_each_entry_safe(dev, tmp, &g_dev_ctrl_list, list_node) {
		if (dev->seid == eid) {
			if (cdma_init(dev->devid, dev->dcna, dev->deid, dev->seid) == NULL) {
				uop_err("Cdma init dev failed. (devid=%u;seid=%u)\n", dev->devid, dev->seid);
				list_del(&dev->list_node);
				kfree(dev);
			} else {
				cdma_submit_event_to_user(dev->devid, UBDEVM_CDMA_PROBE_SUB_EVENT);
			}
		}
	}
	mutex_unlock(&g_ctrl_lock);
	uop_info("Uops cdma dev probe. (eid=%u)\n", eid);
	return 0;
}

static void cdma_dev_remove(u32 eid)
{
	struct cdma_dev_ctrl *dev;

	mutex_lock(&g_ctrl_lock);
	list_for_each_entry(dev, &g_dev_ctrl_list, list_node) {
		if (dev->seid == eid) {
			cdma_uninit(dev->devid);
		}
	}
	mutex_unlock(&g_ctrl_lock);
	uop_info("Uops cdma dev remove. (eid=%u)\n", eid);
}

static void cdma_dev_stop(u32 eid)
{
	/* Must also ensure no new link is established */
	struct cdma_dev_ctrl *dev;

	mutex_lock(&g_ctrl_lock);
	list_for_each_entry(dev, &g_dev_ctrl_list, list_node) {
		if (dev->seid == eid) {
			cdma_stop(dev->devid);
		}
	}
	mutex_unlock(&g_ctrl_lock);
	uop_info("Uops cdma dev stop. (eid=%u)\n", eid);
}

static struct dma_client ubfabric_client = {
	.list_node = LIST_HEAD_INIT(ubfabric_client.list_node),
	.client_name = "ubfabric",
	.add = cdma_dev_probe,
	.remove = cdma_dev_remove,
	.stop = cdma_dev_stop,
};

int uops_cdma_init(void)
{
	int ret;

	ret = cdma_symbol_init();
	if (ret != 0) {
		cdma_symbol_uninit();
		uop_warn("Cdma symbol not found. (ret=%d)\n", ret);
		return 0;
	}

	mutex_init(&g_ctrl_lock);
	ret = dma_register_client_symbol(&ubfabric_client);
	if (ret != 0) {
		uop_err("Register dma client failed. (ret=%d)\n", ret);
		cdma_symbol_uninit();
		return ret;
	}
	ret = uops_kern_register(UOP_ACC_CDMA, &g_cdma_ops);
	if (ret != 0) {
		uop_err("Register cdma ops failed. (ret=%d)\n", ret);
		dma_unregister_client_symbol(&ubfabric_client);
		cdma_symbol_uninit();
		return ret;
	}
	uop_info("Uops cdma init success.\n");
	return 0;
}

void uops_cdma_uninit(void)
{
	if (g_dma_register_client_kern == dma_register_client_stub) {
		return;
	}
	uops_kern_unregister(UOP_ACC_CDMA);
	dma_unregister_client_symbol(&ubfabric_client);
	cdma_symbol_uninit();
	uop_info("Uops cdma uninit.\n");
}

