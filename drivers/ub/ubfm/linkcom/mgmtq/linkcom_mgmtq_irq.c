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
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "ubfabric.h"
#include "udm.h"
#include "uid.h"
#include "uop.h"
#include "idm.h"

#include "linkcom.h"
#include "linkcom_log.h"
#include "linkcom_mng.h"
#include "linkcom_pub.h"
#include "linkcom_core.h"
#include "linkcom_soc_reg.h"

#include "linkcom_mgmtq_irq.h"

#define INVALID_DEV_ID	DEV_MAX_NUM
#define CTRL_CPU_MGTQ_NUM_MAX	  (LINKCOM_DIE_MAX_CHAN_NUM * DIE_MAX_NUM)
#define CTRL_CPU_MGTQ_IRQ_INT_EN	1
#define CTRL_CPU_MGTQ_ESPI_INT_EN   2
#define IMP_CTRL_CPU_MGTQ_RCQE_INT_EN(qid)	(0x5038 + 0x100 * (qid))
#define IMP_CTRL_CPU_MGTQ_IRQ_INT_MASK(qid)   (0x504C + 0x100 * (qid))
#define IMP_CTRL_CPU_MGTQ_IRQ_INT_STS(qid)	(0x5050 + 0x100 * (qid))
#define IMP_CTRL_CPU_MGTQ_IRQ_INT_RO(qid)	 (0x5054 + 0x100 * (qid))
#define IMP_CTRL_CPU_MGTQ_IRQ_INT_SET(qid)	(0x5058 + 0x100 * (qid))

struct mgmtq_hw {
	u32 devid;
	u32 last_chan_id;
	DECLARE_BITMAP(pending_channels, CTRL_CPU_MGTQ_NUM_MAX);
};

static struct mgmtq_hw g_mgmtq_hw[DEV_MAX_NUM];
static DEFINE_SPINLOCK(g_dev_lock);
static atomic_t g_outband_enable = ATOMIC_INIT(0);

int mgmtq_hw_init(u32 dev_id)
{
	spin_lock(&g_dev_lock);

	if (g_mgmtq_hw[dev_id].devid != INVALID_DEV_ID) {
		spin_unlock(&g_dev_lock);
		return -ENODEV;
	}
	g_mgmtq_hw[dev_id].devid = dev_id;
	g_mgmtq_hw[dev_id].last_chan_id = 0;
	bitmap_zero(g_mgmtq_hw[dev_id].pending_channels, CTRL_CPU_MGTQ_NUM_MAX);

	spin_unlock(&g_dev_lock);
	return 0;
}

void mgmtq_hw_uninit(u32 devid)
{
	spin_lock(&g_dev_lock);
	g_mgmtq_hw[devid].devid = INVALID_DEV_ID;
	g_mgmtq_hw[devid].last_chan_id = 0;
	spin_unlock(&g_dev_lock);
}

static int linkcom_set_mgmtq_rcqe_int_en(struct linkcom_dev_inst *dev_inst, u64 imp_base, u32 die_id)
{
	u64 reg_addr = 0;
	u32 chan_id = 0;
	int ret = 0;

	for (chan_id = 0; chan_id < LINKCOM_DIE_MAX_CHAN_NUM; chan_id++) {
		reg_addr = imp_base + IMP_CTRL_CPU_MGTQ_RCQE_INT_EN(chan_id);
		ret = uop_kern_writel(dev_inst->dev_id, reg_addr, CTRL_CPU_MGTQ_IRQ_INT_EN);
		if (ret != 0) {
			linkcom_err("Write mgmtq_int_en reg failed. (devid=%u; die_id=%u; chan_id=%u; ret=%d)\n",
				dev_inst->dev_id, die_id, chan_id, ret);
			return ret;
		}
	}

	return ret;
}

static int linkcom_clear_mgmtq_die_mask(struct linkcom_dev_inst *dev_inst, u64 imp_base, u32 die_id)
{
	u64 reg_addr = 0;
	u32 chan_id = 0;
	int ret = 0;

	for (chan_id = 0; chan_id < LINKCOM_DIE_MAX_CHAN_NUM; chan_id++) {
		reg_addr = imp_base + IMP_CTRL_CPU_MGTQ_IRQ_INT_MASK(chan_id);
		ret = uop_kern_writel(dev_inst->dev_id, reg_addr, 0);
		if (ret != 0) {
			linkcom_err("Write mgmtq_irq_int_mask reg failed. (devid=%u; die_id=%u; chan_id=%u; ret=%d)\n",
				dev_inst->dev_id, die_id, chan_id, ret);
			return ret;
		}
	}

	return ret;
}

int linkcom_set_irq_cfg(struct linkcom_dev_inst *dev_inst)
{
	struct linkcom_id_inst id_inst = {.dev_id = dev_inst->dev_id};
	u64 imp_base = 0;
	u32 dieid = 0;
	int ret = 0;

	for (dieid = 0; dieid < dev_inst->die_num; dieid++) {
		id_inst.die_id = dieid;
		imp_base = linkcom_get_lqc_imp_base_addr(&id_inst);
		if (imp_base == 0) {
			linkcom_err("Base addr is 0.\n");
			return -EFAULT;
		}

		ret = linkcom_set_mgmtq_rcqe_int_en(dev_inst, imp_base, dieid);
		if (ret != 0) {
			linkcom_err("Set mgtq_int_en reg failed. (devid=%u; ret=%d)\n", dev_inst->dev_id, ret);
			return ret;
		}

		ret = linkcom_clear_mgmtq_die_mask(dev_inst, imp_base, dieid);
		if (ret != 0) {
			linkcom_err("Clear mgmtq_int_mask reg failed. (devid=%u; ret=%d)\n", dev_inst->dev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int linkcom_set_mgmtq_rcqe_espi_en(struct linkcom_dev_inst *dev_inst, u64 imp_base, u32 die_id)
{
	u64 reg_addr = 0;
	u32 chan_id = 0;
	int ret = 0;

	for (chan_id = 0; chan_id < LINKCOM_DIE_MAX_CHAN_NUM; chan_id++) {
		reg_addr = imp_base + IMP_CTRL_CPU_MGTQ_RCQE_INT_EN(chan_id);
		ret = uop_kern_writel(dev_inst->dev_id, reg_addr, CTRL_CPU_MGTQ_ESPI_INT_EN);
		if (ret != 0) {
			linkcom_err("Write mgtq_int_en reg failed. (devid=%u; die_id=%u; chan_id=%u; ret=%d)\n",
				dev_inst->dev_id, die_id, chan_id, ret);
			return ret;
		}
	}

	return ret;
}

int linkcom_clear_irq_cfg(struct linkcom_dev_inst *dev_inst)
{
	struct linkcom_id_inst id_inst = {.dev_id = dev_inst->dev_id};
	u64 imp_base = 0;
	u32 die_id;
	int ret;

	for (die_id = 0; die_id < dev_inst->die_num; die_id++) {
		id_inst.die_id = die_id;
		imp_base = linkcom_get_lqc_imp_base_addr(&id_inst);
		if (imp_base == 0) {
			linkcom_err("Base addr is 0.\n");
			return -EFAULT;
		}

		ret = linkcom_set_mgmtq_rcqe_espi_en(dev_inst, imp_base, die_id);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

bool linkcom_core_check_channel_intr_status(struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst)
{
	u64 reg_addr = 0;
	u32 status = 0;
	int ret = 0;

	/* skip outband check */
	if (dev_inst->location == UDM_NEAR && linkcom_get_outband_enable()) {
		return true;
	}

	/* inband intr status */
	if (chan_inst->imp_base_addr == 0) {
		return false;
	}

	reg_addr = chan_inst->imp_base_addr + IMP_CTRL_CPU_MGTQ_IRQ_INT_STS(chan_inst->que_id);
	ret = uop_kern_readl(dev_inst->dev_id, reg_addr, &status);
	if (ret != 0) {
		linkcom_err("Read int_status reg failed. (devid=%u; die_id=%u; ret=%d)\n",
			dev_inst->dev_id, chan_inst->die_id, ret);
		return false;
	}

	if (status != 0) {
		return true;
	}
	return false;
}

int linkcom_clear_channel_int_status(struct linkcom_id_inst *id_inst, u64 imp_base, u32 chan_id)
{
	u64 reg_addr = 0;
	int ret = 0;

	/* status register is INT_WC type, write 1 to clear */
	reg_addr = imp_base + IMP_CTRL_CPU_MGTQ_IRQ_INT_STS(chan_id % LINKCOM_DIE_MAX_CHAN_NUM);
	ret = uop_kern_writel(id_inst->dev_id, reg_addr, 1);
	if (ret != 0) {
		linkcom_err("Write int_status reg failed. (devid=%u; die_id=%u; ret=%d)\n",
			id_inst->dev_id, id_inst->die_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_clear_channel_int_mask(struct linkcom_id_inst *id_inst, u64 imp_base, u32 chan_id)
{
	u64 reg_addr = 0;
	int ret = 0;

	reg_addr = imp_base + IMP_CTRL_CPU_MGTQ_IRQ_INT_MASK(chan_id % LINKCOM_DIE_MAX_CHAN_NUM);
	ret = uop_kern_writel(id_inst->dev_id, reg_addr, 0);
	if (ret != 0) {
		linkcom_err("Write int_mask reg failed. (devid=%u; die_id=%u; ret=%d)\n", id_inst->dev_id, id_inst->die_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_clear_channel_intr_status_and_unmask(struct linkcom_dev_inst *d_ctx, u64 imp_base, u32 die_id, u32 chan_id)
{
	struct linkcom_id_inst id_inst = {.dev_id = d_ctx->dev_id, .die_id = die_id};
	int ret = 0;

	ret = linkcom_clear_channel_int_status(&id_inst, imp_base, chan_id);
	if (ret != 0) {
		linkcom_err("Clear linkcom mgmtq irq intr status failed. (devid=%u; dieid=%u; chanid=%u; ret=%d)\n",
			d_ctx->dev_id, die_id, chan_id, ret);
		return ret;
	}

	ret = linkcom_clear_channel_int_mask(&id_inst, imp_base, chan_id);
	if (ret != 0) {
		linkcom_err("Unmask linkcom mgmtq irq intr mask failed. (devid=%u; dieid=%u; chanid=%u; ret=%d)\n",
			d_ctx->dev_id, die_id, chan_id, ret);
		return ret;
	}

	return ret;
}

static void mgmtq_add_pending_chan(struct mgmtq_hw *mgmtq, u32 die_id, u32 chan_mask)
{
	u32 i;

	for (i = 0; i < LINKCOM_DIE_MAX_CHAN_NUM; ++i) {
		if ((0x1U << i) & chan_mask) {
			set_bit(i + die_id * LINKCOM_DIE_MAX_CHAN_NUM, mgmtq->pending_channels);
		}
	}
}

void mgmtq_irq_triggered(u32 devid, u64 para)
{
	uid_irq_para irq_para = {.val = para};
	struct mgmtq_hw *mgmtq;
	u32 chan_mask;

	if (devid >= DEV_MAX_NUM) {
		return;
	}

	mgmtq = &g_mgmtq_hw[devid];
	if (mgmtq == NULL) {
		return;
	}

	if (irq_para.mgmtq.dieid >= DIE_MAX_NUM) {
		return;
	}

	chan_mask = 0xF;
	mgmtq_add_pending_chan(mgmtq, irq_para.mgmtq.dieid, chan_mask);
}

static u32 mgmtq_get_pending_chan(struct mgmtq_hw *mgmtq)
{
	u32 chan_id;

	chan_id = find_next_bit(mgmtq->pending_channels, CTRL_CPU_MGTQ_NUM_MAX, mgmtq->last_chan_id);
	if (chan_id < CTRL_CPU_MGTQ_NUM_MAX) {
		clear_bit(chan_id, mgmtq->pending_channels);
		return chan_id;
	}

	chan_id = find_first_bit(mgmtq->pending_channels, CTRL_CPU_MGTQ_NUM_MAX);
	if (chan_id < CTRL_CPU_MGTQ_NUM_MAX) {
		clear_bit(chan_id, mgmtq->pending_channels);
		return chan_id;
	}

	return CTRL_CPU_MGTQ_NUM_MAX;
}

void mgmtq_irq_proc_intr(struct linkcom_dev_inst *dev_inst)
{
	struct linkcom_chan_inst *chan_inst = NULL;
	struct mgmtq_hw *mgmtq;
	u32 chan_id = 0;
	int ret = 0, i = 0;

	mgmtq = &g_mgmtq_hw[dev_inst->dev_id];
	if (mgmtq == NULL) {
		return;
	}
	mgmtq->last_chan_id = 0;

	for (i = 0; i < LINKCOM_DIE_MAX_CHAN_NUM * DIE_MAX_NUM; i++) {
		chan_id = mgmtq_get_pending_chan(mgmtq);
		if (chan_id >= CTRL_CPU_MGTQ_NUM_MAX) { /* All chans have been processed */
			break;
		}
		chan_inst = &dev_inst->chan_inst[chan_id];
		mgmtq->last_chan_id = chan_id + 1;

		if (!linkcom_core_check_channel_status(dev_inst, chan_inst)) {
			continue;
		}

		if (!linkcom_core_check_channel_intr_status(dev_inst, chan_inst)) {
			continue;
		}

		ret = linkcom_core_read_rpkt(dev_inst->dev_id, chan_id);
		/* queue ci equals pi -> abnormal -> -EAGAIN */
		/* queue list is full -> abnormal -> -EOVERFLOW */
		if (ret != 0 && ret != -EAGAIN) {
			linkcom_err("Read rpkt in intr failed. (devid=%u; chanid=%u; ret=%d)\n", dev_inst->dev_id, chan_id, ret);
		} else {
			ret = linkcom_clear_channel_intr_status_and_unmask(dev_inst, chan_inst->imp_base_addr, chan_inst->die_id, chan_id);
			if (ret != 0) {
				linkcom_err("Clear rpkt in intr failed. (devid=%u; chanid=%u; ret=%d)\n", dev_inst->dev_id, chan_id, ret);
			}
		}
	}
}

void mgmtq_irq_proc(u32 devid, u64 para)
{
	uid_irq_para irq_para = {.val = para};
	struct linkcom_dev_inst *dev_inst;
	struct mgmtq_hw *mgmtq;
	u32 chan_mask = 0x0F;
	u32 die_id = 0;

	if (devid >= DEV_MAX_NUM) {
		linkcom_err("Device id is invalid. (devid=%u)\n", devid);
		return;
	}

	mgmtq = &g_mgmtq_hw[devid];
	if (mgmtq == NULL) {
		return;
	}

	dev_inst = linkcom_get_dev_inst(devid);
	if (dev_inst == NULL) {
		linkcom_err("Get linkcom dev ctx failed. (devid=%u)\n", devid);
		return;
	}

	die_id = (u32)irq_para.mgmtq.dieid;

	if ((u32)irq_para.mgmtq.in_irq == 0) {
		chan_mask = 0xF;
		for (die_id = 0; die_id < dev_inst->die_num; die_id++) {
			mgmtq_add_pending_chan(mgmtq, die_id, chan_mask);
		}
	}

	mgmtq_irq_proc_intr(dev_inst);
	linkcom_put_dev_inst(devid);
}


void mgmtq_intr_pre_handle(u32 devid, enum uid_intr_type type, u64 para)
{
	mgmtq_irq_triggered(devid, para);
}

void mgmtq_intr_handle(u32 devid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info)
{
	mgmtq_irq_proc(devid, irq_info.irq_para.val);
}

void mgmtq_irq_hw_init(void)
{
	int devid = 0;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		g_mgmtq_hw[devid].devid = INVALID_DEV_ID;
		g_mgmtq_hw[devid].last_chan_id = 0;
	}
}

int mgmtq_irq_init(u32 devid)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct udm_dev_para dev_para;
	int ret = 0;

	ret = udm_get_dev_attr(devid, &dev_para);
	if (ret != 0) {
		linkcom_err("Get dev attr failed. (devid=%u)\n", devid);
		return ret;
	}

	/* initialize mgmtq intr bitmap info */
	ret = mgmtq_hw_init(devid);
	if (ret != 0) {
		linkcom_err("Mgmtq hw init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (dev_para.location == UDM_NEAR) {
		return 0;
	}

	dev_inst = linkcom_get_dev_inst(devid);
	if (dev_inst == NULL) {
		linkcom_err("Get linkcom dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	/* enable inband interrupt registration */
	ret = linkcom_set_irq_cfg(dev_inst);
	if (ret != 0) {
		linkcom_err("Set irq cfg failed. (devid=%u, ret=%d)\n", devid, ret);
	}

	linkcom_put_dev_inst(devid);
	return ret;
}

void mgmtq_irq_uninit(u32 devid)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct udm_dev_para dev_para;
	int ret;

	ret = udm_get_dev_attr(devid, &dev_para);
	if (ret != 0) {
		linkcom_err("Get dev attr failed. (devid=%u)\n", devid);
		return;
	}

	dev_inst = linkcom_get_dev_inst(devid);
	if (dev_inst == NULL) {
		linkcom_err("Get linkcom dev ctx failed. (devid=%u)\n", devid);
		return;
	}

	/* disable inband interrupt */
	ret = linkcom_clear_irq_cfg(dev_inst);
	if (ret != 0) {
		linkcom_err("Clear irq cfg failed. (devid=%u, ret=%d)\n", devid, ret);
	}

	mgmtq_hw_uninit(devid);

	linkcom_put_dev_inst(devid);
}

bool linkcom_get_outband_enable(void)
{
	return atomic_read(&g_outband_enable) != 0;
}

void linkcom_set_outband_enable(bool enable)
{
	atomic_set(&g_outband_enable, enable ? 1 : 0);
}
