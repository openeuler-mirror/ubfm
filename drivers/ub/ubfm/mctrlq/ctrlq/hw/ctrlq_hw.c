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
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include "mami_string.h"
#include "feature_kern.h"
#include "feature_loader.h"
#include "ures.h"
#include "ued.h"
#include "uid.h"
#include "udev_map.h"
#include "mctrlq_log.h"
#include "fmdevm.h"

#include "mctrlq.h"
#include "ctrlq_fops.h"
#include "ctrlq_kern.h"
#include "ctrlq_chip.h"
#include "ctrlq_hw.h"

#define CTRLQ_HW_MAX_CHAN_NUM_ALL   (CTRLQ_HW_MAX_CHAN_NUM * DEV_DIE_MAX_NUM)

struct ctrlq_hw {
	u32 devid;

	unsigned long status[BITS_TO_LONGS(CTRLQ_HW_MAX_CHAN_NUM_ALL)];

	DECLARE_BITMAP(pending_chans, CTRLQ_HW_MAX_CHAN_NUM_ALL);
	u32 last_chan_id;
	struct ctrlq_global_info info;
	const ctrlq_ue_map_t *map_info;
	u32 map_size;
	bool stop;
};

static const struct ctrlq_hw_ops *g_hw_ops;

static struct ctrlq_hw *g_ctrlq_hw[DEV_MAX_NUM];
static DEFINE_SPINLOCK(g_dev_lock);

bool is_kunpeng950(u32 chip_type)
{
	return (chip_type == MAMI_KERN_TYPE_KUNPENG950_1P) || (chip_type == MAMI_KERN_TYPE_KUNPENG950_2P) || (chip_type == MAMI_KERN_TYPE_KUNPENG950_4P);
}
bool feature_is_support_schan(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}
	if (hw.connect_type != UDEV_PCIE_ESPI) {
		return false;
	}
	if (!is_kunpeng950(hw.chip_type)) {
		return false;
	}
	return true;
}

static bool feature_is_schan(u32 devid, u32 chan_id)
{
	return feature_is_support_schan(devid) && (chan_id == CTRLQ_SCHAN_ID);
}

#define UE_RANGE_MASK(s, e)	 ((0x1ULL << ((e) + 1)) - (0x1ULL << (s)))
#define UE_REMAP_TO_CHAN(ue_mask, ue_s, chan_s) \
	((ue_s) < (chan_s) ? (ue_mask << ((chan_s) - (ue_s))) : (ue_mask >> ((ue_s) - (chan_s))))

/*
 * in Ascend_v1
 * Ctrlq chan 0  --> soc int status bit2(ue2) --> fpga int status bit1
 * Ctrlq chan 1  --> soc int status bit3(ue3) --> fpga int status bit2
 * Ctrlq chan 16 --> soc int status bit1(ue1) --> fpga int status bit0
 */
static u64 ctrlq_get_chan_mask(struct ctrlq_hw *ctrlq, u64 ctrlq_status)
{
	const ctrlq_ue_map_t *map = ctrlq->map_info;
	u64 ue_mask = (u64)ctrlq_status << 1;  /* FPGA right-shifts original data by 1 bit */
	u32 i;
	u64 chan_mask = 0;

	if (ctrlq_status == 0) {
		chan_mask = (u64)((0x1ULL << ctrlq->info.valid_chan_num) - 1);
		return chan_mask;
	}

	for (i = 0; i < ctrlq->map_size; ++i) {
		chan_mask |= UE_REMAP_TO_CHAN((ue_mask & UE_RANGE_MASK(map->ue_start, map->ue_end)),
			map->ue_start, map->chan_start);
	}
	return chan_mask;
}
/* in irq */
static void ctrlq_add_pending_chan(struct ctrlq_hw *ctrlq, u32 die_id, u64 chan_mask)
{
	u32 i;

	for (i = 0; i < ctrlq->info.max_chan_num; ++i) {
		if ((0x1ULL << i) & chan_mask) {
			set_bit(i + die_id * ctrlq->info.max_chan_num, ctrlq->pending_chans);
		}
	}
}

/* Interrupt bottom half */
static u32 ctrlq_get_pending_chan(struct ctrlq_hw *ctrlq)
{
	u32 chan_id;

	chan_id = find_next_bit(ctrlq->pending_chans, CTRLQ_HW_MAX_CHAN_NUM_ALL, ctrlq->last_chan_id + 1);
	if (chan_id < CTRLQ_HW_MAX_CHAN_NUM_ALL) {
		clear_bit(chan_id, ctrlq->pending_chans);
		return chan_id;
	}

	chan_id = find_first_bit(ctrlq->pending_chans, CTRLQ_HW_MAX_CHAN_NUM_ALL);
	if (chan_id < CTRLQ_HW_MAX_CHAN_NUM_ALL) {
		clear_bit(chan_id, ctrlq->pending_chans);
		return chan_id;
	}
	return CTRLQ_HW_MAX_CHAN_NUM_ALL;
}

static void ctrlq_deque_all_chan(struct ctrlq_hw *ctrlq, u32 in_irq)
{
	int max_loop = CTRLQ_HW_MAX_CHAN_NUM_ALL;
	u32 chan_id_;
	int ret;

	if (ctrlq->stop == true) {
		mctrlq_err("Ctrlq init failed, stop scan chan. (devid=%u)\n", ctrlq->devid);
		return;
	}

	/*_ctrlq_kern_deque ret
	 * 1.-ENODEV--ctrlq_dev init fail  stop channel
	 * 2.-ENODEV--chan init fail  stop channel
	 * 2.-EBADR --cqe invalid(bb_num/service_type) recover queue, print 64 bytes before and after the first cqe, report discard event
	 * 3.-ENOSPC-- no space(tail - head < cqe_num)/no new cqe written to chan(tail not incremented)
	 * 4.-EFAULT-- addr err(uop read/write failed) head not updated, no recovery needed, wait for next poll; cqe Retry send
	 * 5. EINVAL--event_id invalid		no recovery needed, wait for next poll
	 * 6. ret!=0 --ued_submit_event fail  no recovery needed, wait for next poll
	 */
	do {
		chan_id_ = ctrlq_get_pending_chan(ctrlq);
		if (chan_id_ >= CTRLQ_HW_MAX_CHAN_NUM_ALL) { /* All chans have been processed */
			break;
		}
		ctrlq->last_chan_id = chan_id_;
		if (test_bit(chan_id_, ctrlq->status)) {
			continue;
		}
		ret = _ctrlq_kern_deque(ctrlq->devid, chan_id_);
		if (ret == -ENODEV) {
			mctrlq_err("Ctrlq dev or chan init failed. (devid=%u;chan_id=%u)\n", ctrlq->devid, chan_id_);
			set_bit(chan_id_, ctrlq->status);
			continue;
		}
	} while (max_loop-- > 0);
}

void ctrlq_irq_proc(u32 devid, u32 flag, u64 para)
{
	uid_irq_para irq_para = {.val = para};
	struct ctrlq_hw *ctrlq;
	u32 die_id;
	u64 chan_mask;

	if (devid >= DEV_MAX_NUM) {
		return;
	}
	ctrlq = g_ctrlq_hw[devid];
	if (ctrlq == NULL) {
		return;
	}
	if (para == 0) {	/* scheduled tasks */
		chan_mask = ctrlq_get_chan_mask(ctrlq, 0);
		for (die_id = 0; die_id < ctrlq->info.die_num; ++die_id) {
			ctrlq_add_pending_chan(ctrlq, die_id, chan_mask);
		}
	}

	ctrlq_deque_all_chan(ctrlq, (u32)irq_para.ctrlq.in_irq);
}

void ctrlq_irq_triggered(u32 devid, u64 para)
{
	uid_irq_para irq_para = {.val = para};
	struct ctrlq_hw *ctrlq;
	u64 chan_mask;

	if (devid >= DEV_MAX_NUM) {
		return;
	}
	ctrlq = g_ctrlq_hw[devid];
	if (ctrlq == NULL) {
		return;
	}
	if (irq_para.ctrlq.dieid >= DIE_MAX_NUM) {
		return;
	}
	chan_mask = ctrlq_get_chan_mask(ctrlq, irq_para.ctrlq.chan_status);
	ctrlq_add_pending_chan(ctrlq, irq_para.ctrlq.dieid, chan_mask);
}

#define MAX_IDEV_ENTIRY_NUM 256
#define MAMI_ENTITY_DWORD_NUM (MAX_IDEV_ENTIRY_NUM / 32)
#define FUNCTION_ENTITY_ENABLE(index) (0x12034 + 0x4 * (index))
static int _ctrlq_enable_ue_res(struct ctrlq_hw *ctrlq, u32 die_id)
{
	struct ures_inst inst = {.devid = ctrlq->devid, .die_id = die_id};
	struct ures_addr_info misc_reg = {0};
	u32 enable_val = 0xFFFFFFFF;	/* enable all entity */
	u32 after = 0;
	int i;
	int ret;

	ret = ures_get_reg_base(&inst, MAMI_MISC_REG, &misc_reg);
	if (ret != 0) {
		mctrlq_err("Get misc reg fail. (ret=%d; devid=%u; die_id=%u)\n",
			ret, inst.devid, inst.die_id);
		return ret;
	}
	for (i = 0; i < MAMI_ENTITY_DWORD_NUM; ++i) {
		ret = uop_kern_writel(inst.devid, misc_reg.addr + FUNCTION_ENTITY_ENABLE(i), enable_val);
		if (ret != 0) {
			mctrlq_err("Set ub entity enable fail. (ret=%d; devid=%u; die_id=%u)\n",
				ret, inst.devid, inst.die_id);
			return ret;
		}
		ret = uop_kern_readl(inst.devid, misc_reg.addr + FUNCTION_ENTITY_ENABLE(i), &after);
		if (ret != 0) {
			mctrlq_err("Read ub entity enable after write fail. (ret=%d; devid=%u; die_id=%u; index=%u)\n",
				ret, inst.devid, inst.die_id, i);
			return ret;
		}
		mctrlq_info("Ub entity enable. (devid=%u; die_id=%u; index=%u; after=0x%x)\n",
			inst.devid, inst.die_id, i, after);
	}

	return 0;
}

static void ctrlq_get_asic_addr_range(struct ctrlq_attr *attr, u64 *base_addr, u64 *end_addr,
	u32 cq_chan_size, u32 sq_chan_size)
{
	u64 tmp_addr;

	tmp_addr = min(attr->sq_asic.base, attr->cq_asic.base);
	if (*base_addr == 0 || tmp_addr < *base_addr) {
		*base_addr = tmp_addr;
	}

	*end_addr = max3(*end_addr, attr->cq_asic.base + cq_chan_size, attr->sq_asic.base + sq_chan_size);
}

static int ctrlq_kern_die_chan_info_init(struct ctrlq_die_chan_info_kernel **die_chan_info, u32 die_id, u32 valid_chan_num)
{
	size_t size;

	size = sizeof(struct ctrlq_die_chan_info_kernel) + sizeof(struct ctrlq_chan_info) * valid_chan_num;
	*die_chan_info = kvzalloc(size, GFP_KERNEL_ACCOUNT);
	if (*die_chan_info == NULL) {
		mctrlq_err("Ctrlq chan_info alloc fail. (die_id=%u; valid_chan_num=%u; size=%zu)\n", die_id, valid_chan_num, size);
		return -ENOMEM;
	}
	(*die_chan_info)->die_id = die_id;
	(*die_chan_info)->valid_chan_num = valid_chan_num;
	return 0;
}

static int ctrlq_kern_chan_info_init(u32 devid, struct ctrlq_chan_info *chan_info, u32 chan_id, u32 global_chan_id)
{
	struct ctrlq_que que = {0};
	struct ctrlq_asic asic = {0};
	int ret;

	ret = ctrlq_kern_get_sq_chan_que_and_asic(devid, global_chan_id, &que, &asic);
	if (ret != 0) {
		mctrlq_err("Get sq chan que and asic fail. (ret=%d; devid=%u; chan_id=%u; global_chan_id=%u)\n",
			ret, devid, chan_id, global_chan_id);
		return ret;
	}
	chan_info->chan_id = chan_id;
	chan_info->sq_asic = asic;
	chan_info->sqe_depth = que.depth;

	return 0;
}

static void ctrlq_kern_die_chan_info_uninit(struct ctrlq_die_chan_info_kernel *die_chan_info)
{
	if (die_chan_info != NULL) {
		kvfree(die_chan_info);
	}
}

static int ctrlq_kern_die_chan_info_set(u32 devid, struct ctrlq_die_chan_info_kernel *die_chan_info)
{
	char die_chan_info_key[URES_NAME_LEN] = {0};
	size_t size = sizeof(struct ctrlq_die_chan_info_kernel) + sizeof(struct ctrlq_chan_info) * die_chan_info->valid_chan_num;
	int ret;

	ret = UBFM_SPRINTF(die_chan_info_key, sizeof(die_chan_info_key), MAMI_DIEN_CTRLQ_CHAN_INFO, die_chan_info->die_id);
	if (ret < 0) {
		return -ERANGE;
	}
	ret = ures_dev_set_attr(devid, die_chan_info_key, die_chan_info, size);
	if (ret != 0) {
		mctrlq_err("Set die_chan_info to ures failed. (devid=%u; die_id=%u; name=%s; size=%zu; ret=%d)\n",
			devid, die_chan_info->die_id, die_chan_info_key, size, ret);
	}
	return ret;
}

static int ctrlq_chan_init(struct ctrlq_hw *ctrlq, u32 die_id, struct udev_die_cfg_kernel *die_cfg,
	u64 *base_addr, u64 *end_addr)
{
	struct ctrlq_die_chan_info_kernel *die_chan_info = NULL;
	u32 chan_id;
	int ret;

	ret = ctrlq_kern_die_chan_info_init(&die_chan_info, die_id, ctrlq->info.valid_chan_num);
	if (ret != 0) {
		return ret;
	}
	for (chan_id = 0; chan_id < ctrlq->info.valid_chan_num; chan_id++) {
		struct ctrlq_attr attr;
		u32 cq_chan_size, sq_chan_size, tmp_chan_id, ue_id = 0;

		ret = ctrlq_kern_get_ue_id(chan_id, ctrlq->info.udie_version, ctrlq->info.chip_type, &ue_id);
		if (ret != 0) {
			mctrlq_err("Ctrlq kern get ue_id fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, ctrlq->devid, die_id, chan_id);
			return ret;
		}
		ret = g_hw_ops->attr_pack(&attr, die_id, chan_id, &ctrlq->info);
		if (ret != 0) {
			mctrlq_err("Ctrlq attr pack fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, ctrlq->devid, die_id, chan_id);
			ctrlq_kern_die_chan_info_uninit(die_chan_info);
			return ret;
		}
		tmp_chan_id = chan_id + die_id * ctrlq->info.max_chan_num;
		if ((die_cfg->ue_info[ue_id].ue_init == 0) && (!feature_is_schan(ctrlq->devid, chan_id))) {
			set_bit(tmp_chan_id, ctrlq->status);
			continue;
		}
		ret = ctrlq_kern_chan_add(ctrlq->devid, die_id, tmp_chan_id, &attr);
		if (ret != 0) {
			mctrlq_err("Ctrlq chan add fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, ctrlq->devid, die_id, chan_id);
			ctrlq_kern_die_chan_info_uninit(die_chan_info);
			return ret;
		}

		ret = ctrlq_kern_get_cq_chan_size(ctrlq->devid, tmp_chan_id, &cq_chan_size);
		if (ret != 0) {
			mctrlq_err("Get cq chan size fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, ctrlq->devid, die_id, chan_id);
			ctrlq_kern_die_chan_info_uninit(die_chan_info);
			return ret;
		}
		ret = ctrlq_kern_get_sq_chan_size(ctrlq->devid, tmp_chan_id, &sq_chan_size);
		if (ret != 0) {
			mctrlq_err("Get sq chan size fail. (ret=%d; devid=%u; die_id=%u; chan_id=%u)\n",
				ret, ctrlq->devid, die_id, chan_id);
			ctrlq_kern_die_chan_info_uninit(die_chan_info);
			return ret;
		}
		ret = ctrlq_kern_chan_info_init(ctrlq->devid, &die_chan_info->chan_info[chan_id], chan_id, tmp_chan_id);
		if (ret != 0) {
			ctrlq_kern_die_chan_info_uninit(die_chan_info);
			return ret;
		}
		ctrlq_get_asic_addr_range(&attr, base_addr, end_addr, cq_chan_size, sq_chan_size);
	}

	ret = ctrlq_kern_die_chan_info_set(ctrlq->devid, die_chan_info);
	ctrlq_kern_die_chan_info_uninit(die_chan_info);
	return ret;
}

static int ctrlq_hw_info_init(struct ctrlq_hw *ctrlq)
{
	int ret;

	bitmap_zero(ctrlq->pending_chans, CTRLQ_HW_MAX_CHAN_NUM_ALL);
	ret = ctrlq_kern_get_info(ctrlq->devid, &ctrlq->info);
	if (ret != 0) {
		return ret;
	}
	ctrlq->map_info = ctrlq_kern_get_chip_ue_map(ctrlq->info.udie_version, ctrlq->info.chip_type, &ctrlq->map_size);
	return 0;
}

static int ctrlq_asic_addr_map(struct ctrlq_hw *ctrlq, u64 base_addr, u64 end_addr)
{
	struct udev_soc_addr addr = {0};
	struct ures_inst inst = {0};
	int ret;

	if (end_addr - base_addr == 0) {
		mctrlq_err("Ctrlq hw init failed. (devid=%u)\n", ctrlq->devid);
		return -EINVAL;
	}
	udev_soc_addr_pack(&addr, "ctrlq_mem_base", base_addr, (u32)(end_addr - base_addr), HW_PERF_SENSITIVE);

	/* The arrangement order of cq and sq memory distribution filled by bios is uncertain, so the memory space of all dies is merged here and attached under die 0. There is a constraint on the bios memory distribution here: i.e., the cq and sq memory space of all dies must be contiguous, with no holes.
	 */
	ures_inst_pack(&inst, ctrlq->devid, 0);
	ret = udev_mem_map(&inst, UOP_ITEM_MEM, &addr, UOP_NEED_MAP);
	if (ret != 0) {
		mctrlq_err("Ctrlq add mem failed. (ret=%d; devid=%u; mem_name=ctrlq_mem_base; size=0x%x)\n",
			ret, ctrlq->devid, addr.len);
		return ret;
	}
	return 0;
}

static int _ctrlq_hw_init(struct ctrlq_hw *ctrlq)
{
	u32 die_id, ue_num_per_die, die_cfg_size;
	struct udev_die_cfg_kernel *die_cfg = NULL;
	char udie_cfg_key[URES_NAME_LEN];
	u64 base_addr = 0, end_addr = 0;
	u64 ue_num_in_dev = 0;
	int ret = -ENODEV;

	ret = ctrlq_hw_info_init(ctrlq);
	if (ret != 0) {
		return ret;
	}
	ret = ures_get_key_value(ctrlq->devid, MAMI_UE_NUM, &ue_num_in_dev);
	if (ret != 0) {
		mctrlq_err("Get dev ue num fail. (devid=%u; ret=%d)\n", ctrlq->devid, ret);
		return ret;
	}
	ue_num_per_die = (u32)(ue_num_in_dev / ctrlq->info.die_num);
	die_cfg_size = sizeof(struct udev_die_cfg_kernel) + sizeof(struct udev_ue_info) * ue_num_per_die;
	die_cfg = (struct udev_die_cfg_kernel *)kvmalloc(die_cfg_size, GFP_KERNEL_ACCOUNT);
	if (die_cfg == NULL) {
		return -ENOMEM;
	}
	for (die_id = 0; die_id < ctrlq->info.die_num; die_id++) {
		ret = _ctrlq_enable_ue_res(ctrlq, die_id);
		if (ret != 0) {
			kvfree(die_cfg);
			mctrlq_err("Enable ue res fail. (ret=%d; devid=%u; die_id=%u)\n", ret, ctrlq->devid, die_id);
			return ret;
		}
		(void)ubfm_memset(die_cfg, die_cfg_size, 0, die_cfg_size);
		ret = UBFM_SPRINTF(udie_cfg_key, sizeof(udie_cfg_key), MAMI_UDEV_DIEN_CFG, die_id);
		if (ret < 0) {
			kvfree(die_cfg);
			return -EINVAL;
		}
		ret = ures_dev_get_attr(ctrlq->devid, udie_cfg_key, die_cfg, die_cfg_size);
		if (ret != 0) {
			kvfree(die_cfg);
			mctrlq_err("Get die info failed. (devid=%u; die_id=%u; ret=%d)\n", ctrlq->devid, die_id, ret);
			return ret;
		}
		ret = ctrlq_chan_init(ctrlq, die_id, die_cfg, &base_addr, &end_addr);
		if (ret != 0) {
			kvfree(die_cfg);
			return ret;
		}
	}
	kvfree(die_cfg);
	ret = ctrlq_asic_addr_map(ctrlq, base_addr, end_addr);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static void _ctrlq_hw_uninit(struct ctrlq_hw *ctrlq)
{
	ctrlq->stop = true;
	wmb(); /* Ensure the ctrlq stop flag is written correctly. */
}

static struct ctrlq_hw *_ctrlq_hw_create(u32 devid)
{
	struct ctrlq_hw *ctrlq = NULL;

	if ((g_hw_ops == NULL) || (g_hw_ops->attr_pack == NULL)) {
		mctrlq_err("Hw ops is not registered. (devid=%u)\n", devid);
		return NULL;
	}

	ctrlq = kvzalloc(sizeof(struct ctrlq_hw), GFP_KERNEL_ACCOUNT);
	if (ctrlq == NULL) {
		mctrlq_err("Ctrlq alloc fail. (devid=%u; size=%zu)\n", devid, sizeof(struct ctrlq_hw));
		return NULL;
	}

	ctrlq->devid = devid;
	ctrlq->stop = false;

	mctrlq_info("Ctrlq hw create. (devid=%u;)\n", devid);

	return ctrlq;
}

static void _ctrlq_hw_destroy(struct ctrlq_hw *ctrlq)
{
	kvfree(ctrlq);
}

static struct ctrlq_hw *ctrlq_hw_create(u32 devid, u32 chip_type)
{
	struct ctrlq_hw *ctrlq = NULL;
	int ret;

	ctrlq = _ctrlq_hw_create(devid);
	if (ctrlq == NULL) {
		return NULL;
	}

	ret = ctrlq_kern_init(devid, chip_type);
	if (ret != 0) {
		_ctrlq_hw_destroy(ctrlq);
		mctrlq_err("ctrlq_kern_init fail. devid=%u\n", devid);
		return NULL;
	}

	ret = _ctrlq_hw_init(ctrlq);
	if (ret != 0) {
		ctrlq_kern_uninit(devid);
		_ctrlq_hw_destroy(ctrlq);
		mctrlq_err("_ctrlq_hw_init fail. devid=%u\n", devid);
		return NULL;
	}

	return ctrlq;
}

static void ctrlq_hw_destroy(struct ctrlq_hw *ctrlq)
{
	_ctrlq_hw_uninit(ctrlq);
	ctrlq_kern_uninit(ctrlq->devid);
	_ctrlq_hw_destroy(ctrlq);
}

static int ctrlq_hw_add(struct ctrlq_hw *ctrlq)
{
	u32 devid = ctrlq->devid;

	spin_lock(&g_dev_lock);
	if (g_ctrlq_hw[devid] != NULL) {
		spin_unlock(&g_dev_lock);
		return -ENODEV;
	}
	g_ctrlq_hw[devid] = ctrlq;
	spin_unlock(&g_dev_lock);
	return 0;
}

static struct ctrlq_hw *_ctrlq_hw_get_del(u32 devid)
{
	struct ctrlq_hw *ctrlq = NULL;

	spin_lock(&g_dev_lock);
	ctrlq = g_ctrlq_hw[devid];
	g_ctrlq_hw[devid] = NULL;
	spin_unlock(&g_dev_lock);
	return ctrlq;
}

int ctrlq_hw_init(u32 devid, u32 chip_type)
{
	struct ctrlq_hw *ctrlq = NULL;
	int ret;

	ctrlq = ctrlq_hw_create(devid, chip_type);
	if (ctrlq == NULL) {
		return -ENOMEM;
	}

	ret = ctrlq_hw_add(ctrlq);
	if (ret != 0) {
		ctrlq_hw_destroy(ctrlq);
		return ret;
	}

	return 0;
}

void ctrlq_hw_uninit(u32 devid)
{
	struct ctrlq_hw *ctrlq = NULL;

	ctrlq = _ctrlq_hw_get_del(devid);
	if (ctrlq != NULL) {
		ctrlq_hw_destroy(ctrlq);
	}
}

void ctrlq_hw_ops_register(const struct ctrlq_hw_ops *ops)
{
	g_hw_ops = ops;
}

int ctrlq_hw_mod_init(void)
{
	int ret;

	ret = ctrlq_kern_mod_init();
	if (ret != 0) {
		return ret;
	}
	return ctrlq_fops_mod_init();
}
DECLARE_FEATURE_AUTO_INIT(ctrlq_hw_mod_init, FEATURE_LOADER_STAGE_1);

void ctrlq_hw_mod_uninit(void)
{
	u32 devid;

	ctrlq_fops_mod_uninit();
	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		ctrlq_hw_uninit(devid);
		cond_resched();
	}
}
DECLARE_FEATURE_AUTO_UNINIT(ctrlq_hw_mod_uninit, FEATURE_LOADER_STAGE_1);

