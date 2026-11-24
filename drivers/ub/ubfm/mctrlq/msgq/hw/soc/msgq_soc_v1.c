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
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kref.h>

#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"

#include "fmdevm.h"
#include "ures.h"
#include "uop.h"

#include "mctrlq_log.h"
#include "msgq_kern.h"
#include "msgq_hw.h"
#include "msgq_soc_v1.h"

#define PAGE_SHIFT_256B	8

#define MSGQ_ATTR_SQ_DEPTH	  1024U
#define MSGQ_ATTR_CQ_DEPTH	  1024U
#define MSGQ_ATTR_RQ_DEPTH	  1024U
#define MSGQ_ATTR_SQ_PLD_SIZE   2048U
#define MSGQ_ATTR_RQ_BLK_SIZE   2048U

#define MSGQ_ATTR_SRAM_SQ_DEPTH	  8U
#define MSGQ_ATTR_SRAM_CQ_DEPTH	  16U
#define MSGQ_ATTR_SRAM_RQ_DEPTH	  16U
#define MSGQ_ATTR_SRAM_SQ_PLD_SIZE   256U
#define MSGQ_ATTR_SRAM_RQ_BLK_SIZE   256U

#define MSGQ_BLK_ENTRY	 4
#define MSGQ_SRAM_BLK_ENTRY	 1

struct msgq_attr_mem {
	u64 addr;
	u32 size;
};

struct msgq_pool {
	u64 base;
	u32 size;
	struct gen_pool *pool;
};

struct msgq_attr_dev {
	u32 devid;
	u32 die_num;
	u32 chip_type;
	u64 udie_version;

	u32 sq_depth;
	u32 cq_depth;
	u32 rq_depth;
	u32 sq_pld_size;
	u32 rq_blk_size;

	struct msgq_attr_mem sq[DIE_MAX_NUM];
	struct msgq_attr_mem cq[DIE_MAX_NUM];
	struct msgq_attr_mem rq[DIE_MAX_NUM];

	struct msgq_pool pool_dev;
	struct msgq_pool pool_die[DIE_MAX_NUM];

	struct kref ref;
};

static struct msgq_attr_dev *g_attr_dev[DEV_MAX_NUM];
static DEFINE_MUTEX(g_dev_lock);

static struct msgq_attr_dev *msgq_attr_get(u32 devid);
static void msgq_attr_put(struct msgq_attr_dev *attr_dev);

STATIC int msgq_copy_tail_to_head(u32 devid, u64 head_addr, u64 tail_addr, u32 depth)
{
	u32 tail = 0, head = 0;
	int ret;

	ret = uop_kern_readl(devid, tail_addr, &tail);
	if (ret != 0) {
		mctrlq_err("Msgq Get tail failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (tail >= depth) {
		mctrlq_err("Msgq tail is invalid. (tail=%u; depth=%u)\n", tail, depth);
		return -EINVAL;
	}

	head = tail;
	ret = uop_kern_writel(devid, head_addr, head);
	if (ret != 0) {
		mctrlq_err("Msgq set head failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	return 0;
}

static int msgq_sq_attr_pack(struct msgq_asic *sq, struct ures_inst *inst, struct ures_addr_info *reg)
{
	struct msgq_attr_dev *attr_dev = NULL;
	u64 sq_reg_base_l, sq_reg_base_h;
	u32 sq_mem_l = 0, sq_mem_h = 0;
	size_t size;
	u64 sq_mem;
	int ret;

	attr_dev = msgq_attr_get(inst->devid);
	if (attr_dev == NULL) {
		mctrlq_err("Get attr dev fail. (devid=%u)\n", inst->devid);
		return -ENODEV;
	}

	size = attr_dev->sq[inst->die_id].size;
	sq_mem = attr_dev->sq[inst->die_id].addr;
	sq_reg_base_l = reg->addr + MSGQ_REG_SQ_BASE_L;
	sq_reg_base_h = reg->addr + MSGQ_REG_SQ_BASE_H;

	sq_mem_l = sq_mem & 0xFFFFFFFF;
	sq_mem_h = sq_mem >> 32;	/* Get high 32 bit */

	ret = uop_kern_writel(inst->devid, sq_reg_base_l, sq_mem_l);
	ret |= uop_kern_writel(inst->devid, sq_reg_base_h, sq_mem_h);
	if (ret != 0) {
		mctrlq_err("Get mem base failed. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		goto out;
	}

	sq->base = sq_mem;
	sq->tail = reg->addr + MSGQ_REG_SQ_PI;
	sq->head = reg->addr + MSGQ_REG_SQ_CI;
	sq->depth = reg->addr + MSGQ_REG_SQ_DEP;
	sq->status = reg->addr + MSGQ_REG_SQ_STATUS;
	sq->int_mask = reg->addr + MSGQ_REG_SQ_INT_MASK;
	sq->blk_size = attr_dev->sq_pld_size;

	/* Must set depth first */
	ret = uop_kern_writel(inst->devid, sq->depth, attr_dev->sq_depth);
	ret |= uop_kern_writel(inst->devid, sq->status, 1);
	ret |= uop_kern_writel(inst->devid, sq->int_mask, 0);
	if (ret != 0) {
		mctrlq_err("Write reg fail. (ret=%d; devid=%u)\n", ret, inst->devid);
	}

out:
	msgq_attr_put(attr_dev);
	return ret;
}

static int msgq_cq_attr_pack(struct msgq_asic *cq, struct ures_inst *inst, struct ures_addr_info *reg)
{
	struct msgq_attr_dev *attr_dev = NULL;
	u64 cq_reg_base_l, cq_reg_base_h;
	u32 cq_mem_l = 0, cq_mem_h = 0;
	size_t size;
	u64 cq_mem;
	int ret;

	attr_dev = msgq_attr_get(inst->devid);
	if (attr_dev == NULL) {
		mctrlq_err("Get attr dev fail. (devid=%u)\n", inst->devid);
		return -ENODEV;
	}

	size = attr_dev->cq[inst->die_id].size;
	cq_mem = attr_dev->cq[inst->die_id].addr;

	cq_mem_l = cq_mem & 0xFFFFFFFF;
	cq_mem_h = cq_mem >> 32; /* Get high 32 bit */

	cq_reg_base_l = reg->addr + MSGQ_REG_CQ_BASE_L;
	cq_reg_base_h = reg->addr + MSGQ_REG_CQ_BASE_H;

	ret = uop_kern_writel(inst->devid, cq_reg_base_l, cq_mem_l);
	ret |= uop_kern_writel(inst->devid, cq_reg_base_h, cq_mem_h);
	if (ret != 0) {
		mctrlq_err("Get mem base failed. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		goto out;
	}

	cq->base = cq_mem;
	cq->tail = reg->addr + MSGQ_REG_CQ_PI;
	cq->head = reg->addr + MSGQ_REG_CQ_CI;
	cq->depth = reg->addr + MSGQ_REG_CQ_DEP;
	cq->status = reg->addr + MSGQ_REG_CQ_STATUS;

	ret = uop_kern_writel(inst->devid, cq->depth, attr_dev->cq_depth);
	ret |= msgq_copy_tail_to_head(inst->devid, cq->head, cq->tail, attr_dev->cq_depth);
	ret |= uop_kern_writel(inst->devid, cq->status, 1);
	if (ret != 0) {
		mctrlq_err("Write reg fail. (ret=%d; devid=%u)\n", ret, inst->devid);
	}

out:
	msgq_attr_put(attr_dev);
	return ret;
}

static int msgq_rq_attr_pack(struct msgq_asic *rq, struct ures_inst *inst, struct ures_addr_info *reg)
{
	struct msgq_attr_dev *attr_dev = NULL;
	u64 rq_reg_base_l, rq_reg_base_h;
	u32 rq_mem_l = 0, rq_mem_h = 0;
	struct udev_hw_identify hw = {0};
	u32 blk_entry;  /* 1-256B，4-2KB */
	size_t size;
	u64 rq_mem;
	int ret;

	attr_dev = msgq_attr_get(inst->devid);
	if (attr_dev == NULL) {
		mctrlq_err("Get attr dev fail. (devid=%u)\n", inst->devid);
		return -ENODEV;
	}

	size = attr_dev->rq[inst->die_id].size;
	rq_mem = attr_dev->rq[inst->die_id].addr;

	rq_mem_l = rq_mem & 0xFFFFFFFF;
	rq_mem_h = rq_mem >> 32;	/* Get high 32 bit */

	rq_reg_base_l = reg->addr + MSGQ_REG_RQ_BASE_L;
	rq_reg_base_h = reg->addr + MSGQ_REG_RQ_BASE_H;

	ret = uop_kern_writel(inst->devid, rq_reg_base_l, rq_mem_l);
	ret |= uop_kern_writel(inst->devid, rq_reg_base_h, rq_mem_h);
	if (ret != 0) {
		mctrlq_err("Get mem base failed. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		goto out;
	}

	rq->base = rq_mem;
	rq->tail = reg->addr + MSGQ_REG_RQ_PI;
	rq->head = reg->addr + MSGQ_REG_RQ_CI;
	rq->depth = reg->addr + MSGQ_REG_RQ_DEP;
	rq->blk_size = reg->addr + MSGQ_REG_RQ_SIZE;
	rq->status = reg->addr + MSGQ_REG_RQ_STATUS;

	ret = ures_dev_get_attr(inst->devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, inst->devid);
		goto out;
	}
	blk_entry = ((hw.chip_type == MAMI_KERN_TYPE_UNIONS) ||
		(hw.chip_type == MAMI_KERN_TYPE_UNIONS_210)) ? MSGQ_SRAM_BLK_ENTRY : MSGQ_BLK_ENTRY;

	ret = uop_kern_writel(inst->devid, rq->depth, attr_dev->rq_depth);
	ret |= msgq_copy_tail_to_head(inst->devid, rq->head, rq->tail, attr_dev->rq_depth);
	ret |= uop_kern_writel(inst->devid, rq->status, 1);
	ret |= uop_kern_writel(inst->devid, rq->blk_size, blk_entry);
	if (ret != 0) {
		mctrlq_err("Write reg fail. (ret=%d; devid=%u)\n", ret, inst->devid);
	}

out:
	msgq_attr_put(attr_dev);

	return ret;
}

static int msgq_attr_init_die(u32 devid, u32 die_id, struct msgq_attr_dev *attr_dev)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	struct ures_addr_info sram_mem;
	int ret;

	ret = ures_get_mem_base(&inst, MAMI_MSGQ_SRAM_MEM, &sram_mem);
	if (unlikely(ret != 0)) {
		mctrlq_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	attr_dev->pool_die[die_id].base = sram_mem.addr;
	attr_dev->pool_die[die_id].size = sram_mem.size;

	attr_dev->sq_depth = MSGQ_ATTR_SRAM_SQ_DEPTH;
	attr_dev->cq_depth = MSGQ_ATTR_SRAM_CQ_DEPTH;
	attr_dev->rq_depth = MSGQ_ATTR_SRAM_RQ_DEPTH;
	attr_dev->sq_pld_size = MSGQ_ATTR_SRAM_SQ_PLD_SIZE;
	attr_dev->rq_blk_size = MSGQ_ATTR_SRAM_RQ_BLK_SIZE;

	return 0;
}

static int msgq_attr_init_dev(u32 devid, struct msgq_attr_dev *attr_dev)
{
	struct ub_hsec_mem_entry mem_entry = {0};
	u64 mem_base;
	int ret;

	ret = ures_get_key_value(devid, MSGQ_HSEC_MEM_BASE, &mem_base);
	ret |= ures_dev_get_attr(devid, MSGQ_HSEC_MEM_ENTRY, &mem_entry, sizeof(mem_entry));
	if ((ret != 0) || (mem_entry.entry_num != 1) || (mem_entry.entry_size == 0)) {
		if (ret == 0) {
			mctrlq_err("Get attr error.\n");
			ret = -EINVAL;
		}
		mctrlq_err("Get tag failed. (ret=%d; devid=%u; name=%s; num=%u; size=%u)\n",
			ret, devid, MSGQ_HSEC_MEM_BASE, mem_entry.entry_num, mem_entry.entry_size);
		return ret;
	}
	attr_dev->pool_dev.base = mem_base;
	attr_dev->pool_dev.size = mem_entry.entry_size;

	attr_dev->sq_depth = MSGQ_ATTR_SQ_DEPTH;
	attr_dev->cq_depth = MSGQ_ATTR_CQ_DEPTH;
	attr_dev->rq_depth = MSGQ_ATTR_RQ_DEPTH;
	attr_dev->sq_pld_size = MSGQ_ATTR_SQ_PLD_SIZE;
	attr_dev->rq_blk_size = MSGQ_ATTR_RQ_BLK_SIZE;

	return 0;
}

static struct msgq_attr_dev *_msgq_attr_create(u32 devid, u32 die_num)
{
	struct msgq_attr_dev *attr_dev = NULL;
	struct udev_hw_identify hw = {0};
	u64 udie_version;
	int ret;

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &udie_version);
	if (ret != 0) {
		mctrlq_err("Get udie version fail. (devid=%u; ret=%d)\n", devid, ret);
		return NULL;
	}

	attr_dev = kvzalloc(sizeof(struct msgq_attr_dev), GFP_KERNEL_ACCOUNT);
	if (attr_dev == NULL) {
		mctrlq_err("Attr dev alloc fail. (devid=%u; size=%zu)\n", devid, sizeof(struct msgq_attr_dev));
		return NULL;
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		kvfree(attr_dev);
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}
	attr_dev->chip_type = hw.chip_type;
	attr_dev->udie_version = udie_version;

	if ((attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS) || (attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		u32 die_id;

		for (die_id = 0; die_id < die_num; die_id++) {
			ret = msgq_attr_init_die(devid, die_id, attr_dev);
			if (ret != 0) {
				kvfree(attr_dev);
				return NULL;
			}
		}
	} else {
		ret = msgq_attr_init_dev(devid, attr_dev);
		if (ret != 0) {
			kvfree(attr_dev);
			return NULL;
		}
	}

	attr_dev->devid = devid;
	attr_dev->die_num = die_num;
	kref_init(&attr_dev->ref);

	return attr_dev;
}

static void _msgq_attr_destroy(struct msgq_attr_dev *attr_dev)
{
	kvfree(attr_dev);
}

static void pool_die_rollback(struct msgq_pool *pool_die, u32 creat_pool_die_count)
{
	u32 die_id;

	for (die_id = 0; die_id < creat_pool_die_count; die_id++) {
		gen_pool_destroy(pool_die[die_id].pool);
		pool_die[die_id].pool = NULL;
	}
}

static int _msgq_attr_pool_init(struct msgq_attr_dev *attr_dev)
{
	struct gen_pool *pool = NULL;
	u32 devid = attr_dev->devid;
	u32 die_id;
	int ret;
	u32 creat_pool_die_count = 0;

	if ((attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS) || (attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		for (die_id = 0; die_id < attr_dev->die_num; die_id++) {
			pool = gen_pool_create(PAGE_SHIFT_256B, NUMA_NO_NODE);
			if (pool == NULL) {
				mctrlq_err("Pool creat fail. (devid=%u)\n", devid);
				pool_die_rollback(attr_dev->pool_die, creat_pool_die_count);
				return -ENOMEM;
			}
			ret = gen_pool_add(pool, attr_dev->pool_die[die_id].base, attr_dev->pool_die[die_id].size, NUMA_NO_NODE);
			if (ret != 0) {
				mctrlq_err("Pool add fail. (ret=%d; devid=%u)\n", ret, devid);
				pool_die_rollback(attr_dev->pool_die, creat_pool_die_count);
				goto out;
			}
			attr_dev->pool_die[die_id].pool = pool;
			pool = NULL;
			++creat_pool_die_count;
		}
	} else {
		pool = gen_pool_create(PAGE_SHIFT, NUMA_NO_NODE);
		if (pool == NULL) {
			mctrlq_err("Pool creat fail. (devid=%u)\n", devid);
			return -ENOMEM;
		}
		ret = gen_pool_add(pool, attr_dev->pool_dev.base, attr_dev->pool_dev.size, NUMA_NO_NODE);
		if (ret != 0) {
			mctrlq_err("Pool add fail. (ret=%d; devid=%u)\n", ret, devid);
			goto out;
		}
		attr_dev->pool_dev.pool = pool;
	}

	return 0;
out:
	gen_pool_destroy(pool);
	pool = NULL;
	return ret;
}

static struct msgq_pool *msgq_get_pool(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	if ((attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS) || (attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		return &attr_dev->pool_die[die_id];
	}
	return &attr_dev->pool_dev;
}

static void _msgq_attr_pool_uninit(struct msgq_attr_dev *attr_dev)
{
	struct msgq_pool *pool = NULL;
	u32 die_id;

	if ((attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS) || (attr_dev->chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		for (die_id = 0; die_id < attr_dev->die_num; die_id++) {
			pool = &attr_dev->pool_die[die_id];
			if (pool->pool != NULL) {
				gen_pool_destroy(pool->pool);
				pool->pool = NULL;
			}
		}
	} else {
		pool = &attr_dev->pool_dev;
		if (pool->pool != NULL) {
			gen_pool_destroy(pool->pool);
			pool->pool = NULL;
		}
	}
}

static int msgq_attr_sq_mem_alloc(struct msgq_attr_dev *attr_dev, u32 die_id, u64 *addr, size_t *size)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);

	*size = sizeof(struct msgq_sqe) * attr_dev->sq_depth + attr_dev->sq_pld_size * attr_dev->sq_depth;
	*addr = gen_pool_alloc(pool->pool, *size);
	if (*addr == 0) {
		return -ENOMEM;
	}

	return 0;
}

static int _msgq_attr_sq_mem_init(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	size_t size;
	u64 addr;
	int ret;

	ret = msgq_attr_sq_mem_alloc(attr_dev, die_id, &addr, &size);
	if (ret != 0) {
		return ret;
	}
	attr_dev->sq[die_id].addr = addr;
	attr_dev->sq[die_id].size = (u32)size;

	return 0;
}

static void _msgq_attr_sq_mem_uninit(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);
	size_t size = (size_t)attr_dev->sq[die_id].size;
	u64 addr = attr_dev->sq[die_id].addr;

	if ((addr != 0) && (size != 0)) {
		gen_pool_free(pool->pool, addr, size);
		attr_dev->sq[die_id].addr = 0;
		attr_dev->sq[die_id].size = 0;
	}
}

static int msgq_attr_cq_mem_alloc(struct msgq_attr_dev *attr_dev, u32 die_id, u64 *addr, size_t *size)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);

	*size = sizeof(struct msgq_cqe) * attr_dev->cq_depth;
	*addr = gen_pool_alloc(pool->pool, *size);
	if (*addr == 0) {
		return -ENOMEM;
	}

	return 0;
}

static int _msgq_attr_cq_mem_init(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	size_t size;
	u64 addr;
	int ret;

	ret = msgq_attr_cq_mem_alloc(attr_dev, die_id, &addr, &size);
	if (ret != 0) {
		return ret;
	}
	attr_dev->cq[die_id].addr = addr;
	attr_dev->cq[die_id].size = (u32)size;

	return 0;
}

static void _msgq_attr_cq_mem_uninit(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);
	size_t size = (size_t)attr_dev->cq[die_id].size;
	u64 addr = attr_dev->cq[die_id].addr;

	if ((addr != 0) && (size != 0)) {
		gen_pool_free(pool->pool, addr, size);
		attr_dev->cq[die_id].addr = 0;
		attr_dev->cq[die_id].size = 0;
	}
}

static int msgq_attr_rq_mem_alloc(struct msgq_attr_dev *attr_dev, u32 die_id, u64 *addr, size_t *size)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);

	*size = attr_dev->rq_blk_size * attr_dev->rq_depth;
	*addr = gen_pool_alloc(pool->pool, *size);
	if (*addr == 0) {
		return -ENOMEM;
	}

	return 0;
}

static int _msgq_attr_rq_mem_init(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	size_t size;
	u64 addr;
	int ret;

	ret = msgq_attr_rq_mem_alloc(attr_dev, die_id, &addr, &size);
	if (ret != 0) {
		return ret;
	}
	attr_dev->rq[die_id].addr = addr;
	attr_dev->rq[die_id].size = (u32)size;

	return 0;
}

static void _msgq_attr_rq_mem_uninit(struct msgq_attr_dev *attr_dev, u32 die_id)
{
	struct msgq_pool *pool = msgq_get_pool(attr_dev, die_id);
	size_t size = (size_t)attr_dev->rq[die_id].size;
	u64 addr = attr_dev->rq[die_id].addr;

	if ((addr != 0) && (size != 0)) {
		gen_pool_free(pool->pool, addr, size);
		attr_dev->rq[die_id].addr = 0;
		attr_dev->rq[die_id].size = 0;
	}
}

static int _msgq_attr_mem_init(struct msgq_attr_dev *attr_dev)
{
	u32 die_num = attr_dev->die_num;
	u32 die_id, i;
	int ret;

	for (die_id = 0; die_id < die_num; die_id++) {
		ret = _msgq_attr_sq_mem_init(attr_dev, die_id);
		if (ret != 0) {
			goto out;
		}
		ret = _msgq_attr_cq_mem_init(attr_dev, die_id);
		if (ret != 0) {
			_msgq_attr_sq_mem_uninit(attr_dev, die_id);
			goto out;
		}
		ret = _msgq_attr_rq_mem_init(attr_dev, die_id);
		if (ret != 0) {
			_msgq_attr_cq_mem_uninit(attr_dev, die_id);
			_msgq_attr_sq_mem_uninit(attr_dev, die_id);
			goto out;
		}
	}
	return 0;
out:

	for (i = 0; i < die_id; i++) {
		_msgq_attr_rq_mem_uninit(attr_dev, i);
		_msgq_attr_cq_mem_uninit(attr_dev, i);
		_msgq_attr_sq_mem_uninit(attr_dev, i);
	}

	return -ENOMEM;
}

static void _msgq_attr_mem_uninit(struct msgq_attr_dev *attr_dev)
{
	u32 die_num = attr_dev->die_num;
	u32 die_id;

	for (die_id = 0; die_id < die_num; die_id++) {
		_msgq_attr_rq_mem_uninit(attr_dev, die_id);
		_msgq_attr_cq_mem_uninit(attr_dev, die_id);
		_msgq_attr_sq_mem_uninit(attr_dev, die_id);
	}
}

static int _msgq_attr_init(struct msgq_attr_dev *attr_dev)
{
	int ret;

	ret = _msgq_attr_pool_init(attr_dev);
	if (ret != 0) {
		mctrlq_err("Attr pool init fail. (ret=%d; devid=%u)\n", ret, attr_dev->devid);
		return ret;
	}

	ret = _msgq_attr_mem_init(attr_dev);
	if (ret != 0) {
		_msgq_attr_pool_uninit(attr_dev);
		mctrlq_err("Attr mem init fail. (ret=%d; devid=%u)\n", ret, attr_dev->devid);
		return ret;
	}

	return 0;
}

static void _msgq_attr_uninit(struct msgq_attr_dev *attr_dev)
{
	_msgq_attr_mem_uninit(attr_dev);
	_msgq_attr_pool_uninit(attr_dev);
}

static struct msgq_attr_dev *msgq_attr_create(u32 devid, u32 die_num)
{
	struct msgq_attr_dev *attr_dev = NULL;
	int ret;

	attr_dev = _msgq_attr_create(devid, die_num);
	if (attr_dev == NULL) {
		mctrlq_err("Attr create fail. (devid=%u)\n", devid);
		return NULL;
	}

	ret = _msgq_attr_init(attr_dev);
	if (ret != 0) {
		_msgq_attr_destroy(attr_dev);
		mctrlq_err("Attr init fail. (ret=%d; devid=%u)\n", ret, devid);
		return NULL;
	}

	return attr_dev;
}

static void msgq_attr_destroy(struct msgq_attr_dev *attr_dev)
{
	if (attr_dev != NULL) {
		_msgq_attr_uninit(attr_dev);
		_msgq_attr_destroy(attr_dev);
	}
}

static int msgq_attr_add(struct msgq_attr_dev *attr_dev)
{
	u32 devid = attr_dev->devid;

	if (devid >= DEV_MAX_NUM) {
		return -EINVAL;
	}
	mutex_lock(&g_dev_lock);
	if (g_attr_dev[devid] != NULL) {
		mutex_unlock(&g_dev_lock);
		return -ENODEV;
	}
	g_attr_dev[devid] = attr_dev;
	mutex_unlock(&g_dev_lock);

	return 0;
}

static void msgq_attr_del(struct msgq_attr_dev *attr_dev)
{
	u32 devid = attr_dev->devid;

	if (devid >= DEV_MAX_NUM) {
		return;
	}
	mutex_lock(&g_dev_lock);
	if (g_attr_dev[devid] != NULL) {
		g_attr_dev[devid] = NULL;
	}
	mutex_unlock(&g_dev_lock);
}

static void msgq_attr_release(struct kref *kref)
{
	struct msgq_attr_dev *attr_dev = container_of(kref, struct msgq_attr_dev, ref);

	msgq_attr_destroy(attr_dev);
}

static struct msgq_attr_dev *msgq_attr_get(u32 devid)
{
	struct msgq_attr_dev *attr_dev = NULL;

	if (devid >= DEV_MAX_NUM) {
		return NULL;
	}
	mutex_lock(&g_dev_lock);
	attr_dev = g_attr_dev[devid];
	if (attr_dev != NULL) {
		if (kref_get_unless_zero(&attr_dev->ref) == 0) {
			mutex_unlock(&g_dev_lock);
			return NULL;
		}
	}
	mutex_unlock(&g_dev_lock);

	return attr_dev;
}

static void msgq_attr_put(struct msgq_attr_dev *attr_dev)
{
	kref_put(&attr_dev->ref, msgq_attr_release);
}

static int msgq_soc_attr_pack(struct msgq_attr *attr, u32 devid, u32 die_id)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	struct ures_addr_info reg = {0};

	int ret;

	ret = ures_get_reg_base(&inst, MAMI_MSGQ_REG, &reg);
	if (ret != 0) {
		mctrlq_err("Get MSIC base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst.devid, inst.die_id);
		return ret;
	}

	ret = msgq_sq_attr_pack(&attr->sq, &inst, &reg);
	ret |= msgq_cq_attr_pack(&attr->cq, &inst, &reg);
	ret |= msgq_rq_attr_pack(&attr->rq, &inst, &reg);
	if (ret != 0) {
		mctrlq_err("Sq, Cq, Rq init fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	return 0;
}

static int msgq_attr_dev_init(u32 devid, u32 die_num)
{
	struct msgq_attr_dev *attr_dev = NULL;
	int ret;

	attr_dev = msgq_attr_create(devid, die_num);
	if (attr_dev == NULL) {
		mctrlq_err("Attr create fail. (devid=%u; die_num=%u)\n", devid, die_num);
		return -ENOMEM;
	}

	ret = msgq_attr_add(attr_dev);
	if (ret != 0) {
		msgq_attr_destroy(attr_dev);
		mctrlq_err("Attr add fail. (ret=%d; devid=%u;; die_num=%u)\n", ret, devid, die_num);
		return ret;
	}

	return 0;
}

static void msgq_attr_dev_uninit(u32 devid)
{
	struct msgq_attr_dev *attr_dev = NULL;

	attr_dev = msgq_attr_get(devid);
	if (attr_dev == NULL) {
		return;
	}
	msgq_attr_del(attr_dev);
	msgq_attr_put(attr_dev);
	msgq_attr_put(attr_dev);
}

static const struct msgq_hw_ops g_soc_ops = {
	.init = msgq_attr_dev_init,
	.get_attr = msgq_soc_attr_pack,
	.uninit = msgq_attr_dev_uninit
};

int msgq_soc_v1_mod_init(void)
{
	msgq_kern_hw_register(UDIE_VERSION_V1, &g_soc_ops);
	msgq_kern_hw_register(UDIE_VERSION_V2, &g_soc_ops);
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(msgq_soc_v1_mod_init, FEATURE_LOADER_STAGE_0);
