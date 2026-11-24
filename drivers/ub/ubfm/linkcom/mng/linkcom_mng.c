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
#include <linux/uaccess.h>

#include "array_ctx.h"

#include "mami_string.h"
#include "ures.h"
#include "ubfabric.h"
#include "linkcom_log.h"
#include "linkcom_core.h"
#include "linkcom_soc_reg.h"
#include "linkcom_mng.h"
#include "fmdevm.h"
#include "uop.h"
#include "udev_map.h"
#include "udev_ucmi_ops.h"
#include "fpga_adapter_kern.h"

#ifdef EMU_ST
#define STATIC
#else
#define STATIC static
#endif

#define DEV_LIST_ID LINKCOM_DIE_MAX_CHAN_NUM
#define FPGA_DMA_TRAS_DEFAULT_STS 0xFFFFFFFF
#define RX_PKT_WORK_DELAY 50 /* ms */

#define LINKCOM_RSV_MEM_BASE       0x20A00000ULL
#define LINKCOM_RSV_MEM_END        0x20B00000ULL
#define CFGSPACE_RSV_NUMBER        15

#define UE_AXI_BASE                0x3FFE0000000ULL
#define UE_AXI_DIE_OFFSET          0x10000000000ULL
#define UE_AXI_UE_OFFSET           0x00020000000ULL
/* UE_AXI_OFFSET = (( UE_AXI_BASE + die_id * UE_AXI_DIE_OFFSET + ue_id * UE_AXI_UE_OFFSET )) */

#define UE_AXI_ADDR_NUM_PER_DIE    3
static const u32 g_ue_axi_id_table[UE_AXI_ADDR_NUM_PER_DIE] = {3, 7, 11}; // per die: 3, 7, 11, 12, 16

#define CTRLQ_NUM_ASCEND_V2        1
#define CTRLQ_NUM_KUNPENG960       2
#define CTRLQ_NUM_DEFAULT          0

#define TX_TYPE_BEFORE_EC          1
#define TX_TYPE_AFTER_EC           0

#define UCMI_DB_WIN_BITS_PER_DIE   16
#define UCMI_DB_WIN_BITS_PER_UE    4

enum WORK_STATUS {
	WORK_STOP = 0,
	WORK_RUN,
	WORK_PAUSE,
};

static struct array_ctx_domain *linkcom_dev_domain;

struct linkcom_dev_inst *linkcom_get_dev_inst(u32 devid)
{
	struct array_ctx *ctx = NULL;
	struct linkcom_dev_inst *d_ctx = NULL;

	ctx = array_ctx_get(linkcom_dev_domain, devid);
	if (ctx == NULL) {
		return NULL;
	}
	d_ctx = (struct linkcom_dev_inst *)array_ctx_priv(ctx);

	return d_ctx;
}

void linkcom_put_dev_inst(u32 devid)
{
	struct array_ctx *ctx = NULL;

	ctx = array_ctx_get(linkcom_dev_domain, devid); /* developer ensures devid is valid (get/put paired), so ctx is not NULL */
	array_ctx_put(ctx);
	array_ctx_put(ctx);
}

void linkcom_chan_recv_pkt_work(struct work_struct *work)
{
#ifndef EMU_ST
	struct linkcom_chan_inst *chan;
	struct linkcom_dev_inst *dev;
	u64 addr, chan_id;
	int ret = 0;

	chan = container_of(work, struct linkcom_chan_inst, work.work);
	chan_id = chan->que_id + (chan->die_id * LINKCOM_DIE_MAX_CHAN_NUM);
	addr = (u64)(uintptr_t)chan - chan_id * sizeof(struct linkcom_chan_inst);
	dev = container_of((void *)(uintptr_t)addr, struct linkcom_dev_inst, chan_inst);

	if (chan->work_run_flag == WORK_STOP) {
		return;
	}

	if (chan->work_run_flag == WORK_RUN) {
		ret = linkcom_core_read_rpkt(dev->dev_id, chan_id);
		if (ret != 0 && ret != -EAGAIN) {
			linkcom_err("Read rpkt fail. (ret=%d)\n", ret);
			return;
		}
	}
	schedule_delayed_work(&chan->work, msecs_to_jiffies(RX_PKT_WORK_DELAY));
#endif
}

void linkcom_sched_recv_pkt_work(struct delayed_work *work, u32 *work_run_flag)
{
#ifndef EMU_ST
	*work_run_flag = WORK_RUN;
	schedule_delayed_work(work, 0);
#endif
}

void linkcom_flush_recv_pkt_work(struct delayed_work *work, u32 *work_run_flag)
{
	*work_run_flag = WORK_STOP;
	flush_delayed_work(work);
}

void linkcom_pause_recv_pkt_work(u32 *work_run_flag)
{
	*work_run_flag = WORK_PAUSE;
}

void linkcom_start_recv_pkt_work(u32 *work_run_flag)
{
	*work_run_flag = WORK_RUN;
}

int linkcom_add_list_node(struct linkcom_list_head *head, struct linkcom_rx_cqe cqe, u8 *pkt_buff)
{
	struct linkcom_list_node *node = NULL;
	int ret = 0;

	if (head->count >= MAX_BUFF_CNT) {
		linkcom_err("list is overflow\n");
		return -EOVERFLOW;
	}

	if ((cqe.pkt_len == 0) || (cqe.pkt_len > LINK_PKT_BUFF_LEN_MAX)) {
#ifndef EMU_ST
		linkcom_err("Invalid pkt_len. (pkt_len=%u; range=(0,%u])\n", cqe.pkt_len, LINK_PKT_BUFF_LEN_MAX);
		return -EINVAL;
#endif
	}

	node = kzalloc(sizeof(struct linkcom_list_node), GFP_KERNEL | __GFP_ACCOUNT);
	if (node == NULL) {
		linkcom_err("create list node fail\n");
		return -ENOMEM;
	}

	node->pkt_buff = kzalloc(cqe.pkt_len, GFP_KERNEL | __GFP_ACCOUNT);

	if (node->pkt_buff == NULL) {
		linkcom_err("create list node pkt buf fail\n");
		kfree(node);
		return -ENOMEM;
	}

	node->cqe = cqe;
	node->recv_len = cqe.pkt_len;
	ret = ubfm_memcpy(node->pkt_buff, cqe.pkt_len, pkt_buff, cqe.pkt_len);
	if (ret != 0) {
#ifndef EMU_ST
		linkcom_err("Memory copy failed. (ret=%d)\n", ret);
		kfree(node->pkt_buff);
		kfree(node);
		return ret;
#endif
	}

	INIT_LIST_HEAD(&(node->list));

	list_add_tail(&(node->list), &(head->list));

	head->count++;

	return 0;
}

void linkcom_destroy_list_node(struct linkcom_list_node *node)
{
	list_del(&node->list);

	kfree(node->pkt_buff);
	node->pkt_buff = NULL;

	kfree(node);
	node = NULL;
}

void linkcom_del_all_list_node(struct linkcom_list_head *head, u32 chan_id)
{
	struct linkcom_list_node *node = NULL;
	struct linkcom_list_node *node_next = NULL;

	if (list_empty(&(head->list))) {
		return;
	}

	list_for_each_entry_safe(node, node_next, &(head->list), list) {
		if (chan_id + 1 != node->cqe.queue_id && chan_id != DEV_LIST_ID) {
			continue;
		}

		linkcom_destroy_list_node(node);
		head->count--;
	}
}

int linkcom_pop_list_node(struct linkcom_list_head *head, u32 chan_id, struct linkcom_rx_cqe *cqe, u8 *pkt_buff, u32 buf_len)
{
	struct linkcom_list_node *node = NULL;
	struct linkcom_list_node *node_next = NULL;

	if (list_empty(&(head->list))) {
		return -EAGAIN;
	}

	list_for_each_entry_safe(node, node_next, &(head->list), list) {
		int ret;

		if (node->cqe.queue_id != (chan_id % LINKCOM_DIE_MAX_CHAN_NUM) + 1) {
			continue;
		}
		if (node->recv_len > buf_len) {
			linkcom_err("recv buff len is not enough. pkt_len[%u] buf_len[%u]\n", node->recv_len, buf_len);
			return -EINVAL;
		}

		*cqe = node->cqe;

		ret = ubfm_memcpy(pkt_buff, buf_len, (void *)node->pkt_buff, (u64)node->recv_len);
		if (ret != 0) {
#ifndef EMU_ST
			linkcom_err("Memory copy failed. (ret=%d; buf_len=%u; recv_len=%u)\n", ret, buf_len, node->recv_len);
			return ret;
#endif
		}

		linkcom_destroy_list_node(node);
		head->count--;
		return 0;
	}

	return -EAGAIN;
}

void linkcom_core_get_chan_imp_base(u32 devid, struct linkcom_chan_inst *chan_inst)
{
	struct linkcom_id_inst id_inst = {.dev_id = devid, .die_id = chan_inst->die_id};
	u64 imp_base = 0;

	imp_base = linkcom_get_lqc_imp_base_addr(&id_inst);
	chan_inst->imp_base_addr = imp_base;
}

void linkcom_init_chan_inst(struct linkcom_dev_inst *dev_inst)
{
	u32 idx;

	for (idx = 0; idx < dev_inst->chan_num; idx++) {
		dev_inst->chan_inst[idx].chan_id = idx;
		dev_inst->chan_inst[idx].die_id = idx / LINKCOM_DIE_MAX_CHAN_NUM;
		dev_inst->chan_inst[idx].que_id = idx % LINKCOM_DIE_MAX_CHAN_NUM;
		dev_inst->chan_inst[idx].app_pid = 0;
		dev_inst->chan_inst[idx].time = 0;
		dev_inst->chan_inst[idx].imp_base_addr = 0;
		dev_inst->chan_inst[idx].occupy_status = FUNC_IDLE_STS;
		mutex_init(&dev_inst->chan_inst[idx].lock);
		dev_inst->chan_inst[idx].wq.flag = 0;
		init_waitqueue_head(&dev_inst->chan_inst[idx].wq.head);
		dev_inst->chan_inst[idx].work_run_flag = WORK_STOP;
		INIT_DELAYED_WORK(&(dev_inst->chan_inst[idx].work), linkcom_chan_recv_pkt_work);
		INIT_LIST_HEAD(&(dev_inst->chan_inst[idx].head.list));
		dev_inst->chan_inst[idx].head.count = 0;
		dev_inst->chan_inst[idx].eventfd = -1;
		dev_inst->chan_inst[idx].eventctx = NULL;
		(void)linkcom_core_get_chan_imp_base(dev_inst->dev_id, &dev_inst->chan_inst[idx]);
	}
}

void linkcom_core_release_lst_rsrc(struct linkcom_chan_inst *chan_inst)
{
	linkcom_flush_recv_pkt_work(&chan_inst->work, &chan_inst->work_run_flag);
	mutex_lock(&chan_inst->lock);
	linkcom_del_all_list_node(&chan_inst->head, chan_inst->que_id);
	chan_inst->occupy_status = FUNC_IDLE_STS;
	mutex_unlock(&chan_inst->lock);
}

static void linkcom_uninit_chan_inst(struct linkcom_dev_inst *dev_inst)
{
	struct linkcom_chan_inst *chan_inst = NULL;
	u32 chan_num = dev_inst->chan_num;
	u32 i = 0;

	for (i = 0; i < chan_num; i++) {
		chan_inst = &dev_inst->chan_inst[i];
		if (chan_inst->occupy_status == FUNC_IDLE_STS) {
			continue;
		}

		mutex_lock(&chan_inst->lock);
		chan_inst->app_pid = 0;
		chan_inst->time = 0;
		linkcom_core_chan_eventctx_recycle(chan_inst);
		mutex_unlock(&chan_inst->lock);

		linkcom_core_release_lst_rsrc(chan_inst);
		linkcom_info("uninit dev[%u] chan[%u] mng success.\n", dev_inst->dev_id, i);
	}
}

int linkcom_get_ec_status(u32 devid, bool *is_before_ec)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_id_inst id_inst = {0};
	u64 dm_base_addr;
	u64 reg_addr;
	u32 reg_val;
	int ret;

	dev_inst = linkcom_get_dev_inst(devid);
	if (dev_inst == NULL) {
		linkcom_err("Get dev inst failed. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (dev_inst->chip_type != MAMI_KERN_TYPE_ASCEND_V2) {
		linkcom_put_dev_inst(devid);
		return 0;
	}

	id_inst.dev_id = devid;
	id_inst.die_id = 0;
	dm_base_addr = linkcom_get_lqc_ta_dm_base_addr(&id_inst);
	if (dm_base_addr == 0) {
		linkcom_err("Get TA DM base addr failed. (devid=%u)\n", devid);
		linkcom_put_dev_inst(devid);
		return -EINVAL;
	}

	reg_addr = dm_base_addr + DM_CFG_RSV_REG_0(CFGSPACE_RSV_NUMBER); // DFX_DM_CFGSPACE_RSV_REG
	ret = uop_kern_readl(devid, reg_addr, &reg_val);
	if (ret != 0) {
		linkcom_err("Read EC status reg failed. (ret=%d; devid=%u)\n", ret, devid);
		linkcom_put_dev_inst(devid);
		return ret;
	}

	*is_before_ec = (reg_val == 1);
	linkcom_put_dev_inst(devid);
	return 0;
}

int linkcom_set_ec_status_to_ures(u32 devid, bool is_before_ec)
{
	int ret;

	ret = ures_set_key_value(devid, MAMI_LINKCOM_EC_STATUS, (u64)is_before_ec);
	if (ret != 0) {
		linkcom_err("Save EC status to ures failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	return 0;
}

static int linkcom_rsv_addr_map(u32 devid, u32 connect_type, u32 chip_id)
{
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};
	struct udev_ucmi_addr_info *addr_info = NULL;
	u32 axi_id;
	int ret;

	udev_soc_addr_pack(&addr, "linkcom_rsv_mem", LINKCOM_RSV_MEM_BASE,
			   (u64)(LINKCOM_RSV_MEM_END - LINKCOM_RSV_MEM_BASE), HW_PERF_SENSITIVE);
	ures_inst_pack(&inst, devid, 0);
	ret = udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NEED_MAP);
	if (ret != 0) {
		linkcom_err("Rsv addr map failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	addr_info = ucmi_get_addr_info(connect_type, devid, LINKCOM_RSV_MEM_BASE, "linkcom_rsv_mem");
	if (addr_info == NULL) {
		linkcom_err("Get addr info failed. (devid=%u)\n", devid);
		return -EINVAL;
	}
	axi_id = addr_info->axi_id;
	ucmi_put_addr_info(connect_type, devid);

	ret = (int)starry_drv_fpga_ucmi_write_chip_acc_win_sqe(DRV_INST_ID_SELF, chip_id, axi_id);
	if (ret == -EPERM) {
		linkcom_warn("Starry write chip SQE win API not available, skip. (devid=%u)\n", devid);
	} else if (ret != 0) {
		linkcom_err("Write chip SQE win failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	linkcom_info("Rsv addr map ok. (devid=%u; axi_id=%u)\n", devid, axi_id);
	return 0;
}

static int linkcom_assign_ue_axi_wins(u32 connect_type, u32 devid, u32 die_num, u32 chip_id)
{
	u32 die_id, ue_idx, axi_id;
	u64 chip_addr;
	u32 ucmi_db_win;
	int ret;

	for (die_id = 0; die_id < die_num; die_id++) {
		ucmi_db_win = 0;
		for (ue_idx = 0; ue_idx < UE_AXI_ADDR_NUM_PER_DIE; ue_idx++) {
			chip_addr = UE_AXI_BASE + (u64)die_id * UE_AXI_DIE_OFFSET +
				    (u64)g_ue_axi_id_table[ue_idx] * UE_AXI_UE_OFFSET;
			ret = ucmi_assign_256m_win(connect_type, devid, chip_addr, &axi_id);
			if (ret != 0) {
				linkcom_err("Assign UE AXI win failed. (ret=%d; devid=%u; die=%u; ue=%u)\n",
					    ret, devid, die_id, g_ue_axi_id_table[ue_idx]);
				return ret;
			}
			ucmi_db_win |= (axi_id << (ue_idx * UCMI_DB_WIN_BITS_PER_UE));
			linkcom_info("Assign UE AXI win ok. (die=%u; ue=%u; axi_id=%u)\n",
				     die_id, g_ue_axi_id_table[ue_idx], axi_id);
		}
		ret = (int)starry_drv_fpga_ucmi_write_chip_acc_win_doorbell(DRV_INST_ID_SELF, chip_id, die_id, ucmi_db_win);
		if (ret == -EPERM) {
			linkcom_warn("Starry write chip acc win doorbell API not available, skip. (devid=%u; die_id=%u)\n", devid, die_id);
		} else if (ret != 0) {
			linkcom_err("Write chip acc win doorbell failed. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
			return ret;
		}
	}
	return 0;
}

STATIC int linkcom_dev_mng_init(struct linkcom_dev_inst *dev_inst)
{
	u32 devid = dev_inst->dev_id;
	u64 die_num = 0;
	u64 udie_version = 0;
	struct udev_hw_identify hw = {0};
	int ret = 0;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num > DIE_MAX_NUM)) {
		linkcom_err("get die num failed. (devid=%u; die_num=%llu; ret=%d)\n", devid, die_num, ret);
		return -EINVAL;
	}

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &udie_version);
	if ((ret != 0) || (udie_version >= UDIE_VERSION_MAX)) {
		linkcom_err("get udie version failed. (devid[%u]; version[%llu]; ret[%d])\n", devid, udie_version, ret);
		return -EINVAL;
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		linkcom_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	dev_inst->die_num = die_num;
	dev_inst->chan_num = die_num * LINKCOM_DIE_MAX_CHAN_NUM;  /* 4 channels per die */
	dev_inst->chip_id = hw.socket_id;
	dev_inst->udie_version = (enum mami_udie_version)udie_version;
	dev_inst->chip_type = (enum mami_dev_type)hw.chip_type;
	dev_inst->location = (enum udm_dev_location)hw.location;
	dev_inst->connect_type = hw.connect_type;

	linkcom_init_chan_inst(dev_inst);

	linkcom_info("init linkcom devid[%u] mng success.\n", devid);

	return 0;
}

static void linkcom_dev_mng_uninit(struct linkcom_dev_inst *dev_inst)
{
	linkcom_uninit_chan_inst(dev_inst);
	linkcom_info("uninit linkcom devid[%u] mng success.\n", dev_inst->dev_id);
}

static void linkcom_dev_ctx_release(struct array_ctx *ctx)
{
	struct linkcom_dev_inst *d_ctx = (struct linkcom_dev_inst *)array_ctx_priv(ctx);

	linkcom_dev_mng_uninit(d_ctx);
	vfree(d_ctx);
}

static int linkcom_write_ucmi_global_cfg(struct linkcom_dev_inst *d_ctx, bool is_before_ec, u32 devid)
{
	int ret;
	ucmi_global_cfg_s global_cfg = {0};

	if (d_ctx->chip_type == MAMI_KERN_TYPE_ASCEND_V2) {
		global_cfg.int_bit.ctrlq_num = CTRLQ_NUM_ASCEND_V2;
	} else if (d_ctx->chip_type == MAMI_KERN_TYPE_KUNPENG960_1P || d_ctx->chip_type == MAMI_KERN_TYPE_KUNPENG960_2P || d_ctx->chip_type == MAMI_KERN_TYPE_KUNPENG960_4P) {
		global_cfg.int_bit.ctrlq_num = CTRLQ_NUM_KUNPENG960;
	} else {
		global_cfg.int_bit.ctrlq_num = CTRLQ_NUM_DEFAULT;
	}
	global_cfg.int_bit.tx_type = is_before_ec ? TX_TYPE_BEFORE_EC : TX_TYPE_AFTER_EC;

	ret = (int)starry_drv_fpga_ucmi_write_ucmi_global_cfg(DRV_INST_ID_SELF, d_ctx->chip_id, global_cfg);
	if (ret == -EPERM) {
		linkcom_warn("Starry write ucmi global cfg API not available, skip. (devid=%u)\n", devid);
	} else if (ret != 0) {
		linkcom_err("Write UCMI global cfg failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	return 0;
}

static int linkcom_dev_init_outofband_cfg(struct linkcom_dev_inst *d_ctx, u32 devid)
{
	int ret;
	bool is_before_ec = false;

	ret = linkcom_get_ec_status(devid, &is_before_ec);
	if (ret != 0) {
		linkcom_err("Check EC status failed, skip rsv map and UE AXI win. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = linkcom_set_ec_status_to_ures(devid, is_before_ec);
	if (ret != 0) {
		linkcom_err("Set EC status to ures failed, skip rsv map and UE AXI win. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (d_ctx->connect_type != UDEV_PCIE_ESPI) {
		return 0;
	}

	ret = linkcom_write_ucmi_global_cfg(d_ctx, is_before_ec, devid);
	if (ret != 0) {
		return ret;
	}

	if (is_before_ec) { /* MAMI_KERN_TYPE_ASCEND_V2，outband，before EC*/
		ret = linkcom_rsv_addr_map(devid, d_ctx->connect_type, d_ctx->chip_id);
		if (ret != 0) {
			linkcom_err("Rsv addr map failed. (ret=%d; devid=%u)\n", ret, devid);
			return ret;
		}

		ret = linkcom_assign_ue_axi_wins(d_ctx->connect_type, devid, d_ctx->die_num, d_ctx->chip_id);
		if (ret != 0) {
			linkcom_err("Assign UE AXI wins failed. (ret=%d; devid=%u)\n", ret, devid);
			return ret;
		}

		ret = (int)starry_drv_fpga_ucmi_write_cfg_done_flag(DRV_INST_ID_SELF, d_ctx->chip_id, 1);
		if (ret == -EPERM) {
			linkcom_warn("Starry write cfg done flag API not available, skip. (devid=%u)\n", devid);
		} else if (ret != 0) {
			linkcom_err("Write cfg done failed. (ret=%d; devid=%u)\n", ret, devid);
			return ret;
		}
	}

	return 0;
}

int linkcom_add_dev(u32 devid)
{
	struct linkcom_dev_inst *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	int ret;

	d_ctx = vzalloc(sizeof(*d_ctx));
	if (d_ctx == NULL) {
		linkcom_err("Vmalloc dev ctx fail. (size=%lu)\n", sizeof(*d_ctx));
		return -ENOMEM;
	}

	d_ctx->dev_id = devid;

	ret = array_ctx_create(linkcom_dev_domain, devid, d_ctx, linkcom_dev_ctx_release);
	if (ret != 0) {
		vfree(d_ctx);
		linkcom_err("Create fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = linkcom_dev_mng_init(d_ctx);
	if (ret != 0) {
		ctx = array_ctx_get(linkcom_dev_domain, devid);
		array_ctx_put(ctx);
		array_ctx_destroy(ctx); /* d_ctx is freed in linkcom_dev_ctx_release */
		return ret;
	}

	ret = linkcom_dev_init_outofband_cfg(d_ctx, devid);
	if (ret != 0) {
		ctx = array_ctx_get(linkcom_dev_domain, devid);
		array_ctx_put(ctx);
		array_ctx_destroy(ctx);
		return ret;
	}

	return 0;
}

int linkcom_del_dev(u32 devid)
{
	struct array_ctx *ctx = array_ctx_get(linkcom_dev_domain, devid);

	if (ctx == NULL) {
		return -EINVAL;
	}

	array_ctx_put(ctx); /* array_ctx_get */
	array_ctx_destroy(ctx); /* array_ctx_create */

	return 0;
}

static void linkcom_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
	linkcom_warn("Recycle dev ctx. (devid=%u)\n", ctx->id);
	linkcom_del_dev(ctx->id);
}

int linkcom_mng_init(void)
{
	linkcom_dev_domain = array_ctx_domain_create("linkcom_dev", DEV_MAX_NUM);
	if (linkcom_dev_domain == NULL) {
		linkcom_err("Create dev domain fail.\n");
		return -ENOMEM;
	}
	return 0;
}

void linkcom_mng_uninit(void)
{
	array_ctx_for_each_safe(linkcom_dev_domain, NULL, linkcom_dev_ctx_recycle);
	array_ctx_domain_destroy(linkcom_dev_domain);
	linkcom_dev_domain = NULL;
}
