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
#define pr_fmt(fmt) "XSHMEM_FSC: " fmt

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>

#include "xshmem_framework.h"

#define XSHMEM_BLK_ALIGN	16

struct blk_ctrl {
	unsigned int pool_size;
	unsigned int block_size;

	int total_block;
	int free_block;
	int next_free;
	unsigned long map[];
};

STATIC bool blk_algo_pool_same(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	struct blk_ctrl *ctrl = (struct blk_ctrl *)(xp->private_obj);

	return (ctrl->pool_size == arg->pool_size) && (ctrl->block_size == arg->block_size);
}

STATIC int blk_algo_pool_init(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	int block_cnt;
	struct blk_ctrl *ctrl = NULL;

	if (!IS_ALIGNED(arg->pool_size, XSHMEM_BLK_ALIGN) ||
		!IS_ALIGNED(arg->block_size, XSHMEM_BLK_ALIGN) ||
		(arg->pool_size < arg->block_size) ||
		(arg->pool_size == 0U) || (arg->block_size == 0U) ||
		(arg->pool_size > XSHMEM_MAX_POOL_SIZE) ||
		(arg->block_size > XSHMEM_MAX_BLOCK_SIZE)) {
		pr_err("input size invalid, pool_size: %lu, blk_size: %u\n",
			arg->pool_size, arg->block_size);
		return -EINVAL;
	}

	block_cnt = (int)(arg->pool_size / arg->block_size);
	ctrl = kvmalloc(sizeof(*ctrl) + sizeof(long) * BITS_TO_LONGS(block_cnt),
		GFP_KERNEL | __GFP_ACCOUNT | __GFP_ZERO);
	if (ctrl == NULL) {
		pr_err("kvmalloc for blk_ctrl failed\n");
		return -ENOMEM;
	}

	ctrl->pool_size   = (unsigned int)(arg->pool_size);
	ctrl->block_size  = (unsigned int)(arg->block_size);
	ctrl->total_block = block_cnt;
	ctrl->next_free   = 0;
	ctrl->free_block  = block_cnt;

	xp->private_obj = ctrl;

	return 0;
}

STATIC int blk_algo_pool_free(struct xshm_pool *xp)
{
	kvfree(xp->private_obj);
	return 0;
}

STATIC void blk_algo_pool_show(struct xshm_pool *xp, struct seq_file *seq)
{
	struct blk_ctrl *ctrl = (struct blk_ctrl *)(xp->private_obj);

	seq_printf(seq, "    pool_size:%#x, block_size:%#x, free_block:%d\n",
			   ctrl->pool_size, ctrl->block_size, ctrl->free_block);
}

STATIC int blk_algo_block_alloc(struct xshm_pool *xp, struct xshm_block *blk)
{
	long idx;
	unsigned long size = blk->alloc_size;
	struct blk_ctrl *ctrl = (struct blk_ctrl *)(xp->private_obj);

	if (size > (unsigned long)(ctrl->block_size)) {
		pr_err("the alloc size too big\n");
		return -EINVAL;
	}

	idx = (long)find_next_zero_bit(ctrl->map,
		(unsigned long)(ctrl->total_block), (unsigned long)(ctrl->next_free));
	if (idx == ctrl->total_block) {
		idx = (long)find_next_zero_bit(ctrl->map, (unsigned long)(ctrl->next_free), 0);
		if (idx == ctrl->next_free)
			idx = -ENOSPC;
	}

	if (idx < 0) {
		pr_err("no free BLOCK left in the POOL\n");
		return (int)idx;
	}

	set_bit((unsigned int)idx, ctrl->map);
	ctrl->next_free = ((int)idx + 1) % ctrl->total_block;
	ctrl->free_block--;

	blk->private_obj = (void *)idx;
	blk->offset = (unsigned long)(idx * ctrl->block_size);
	blk->real_size = (unsigned long)(ctrl->block_size);

	return 0;
}

STATIC int blk_algo_block_free(struct xshm_pool *xp, struct xshm_block *blk)
{
	long idx = (long)blk->private_obj;
	struct blk_ctrl *ctrl = (struct blk_ctrl *)(xp->private_obj);

	ctrl->free_block++;
	clear_bit((unsigned int)idx, ctrl->map);
	return 0;
}

struct xshm_pool_algo blk_algo = {
	.num = XSHMEM_ALGO_BLOCK,
	.name = "blk_algo",
	.xshm_pool_same = blk_algo_pool_same,
	.xshm_pool_init = blk_algo_pool_init,
	.xshm_pool_free = blk_algo_pool_free,
	.xshm_pool_show = blk_algo_pool_show,
	.xshm_block_alloc = blk_algo_block_alloc,
	.xshm_block_free = blk_algo_block_free,
};
