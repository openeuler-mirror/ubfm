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
#include <linux/module.h>

#include "xshmem_framework.h"

#define XSHMEM_FSC_MAX_SHIFT		31U
#define XSHMEM_FSC_MIN_SHIFT		4U
/* exclude */
#define XSHMEM_FSC_MAX_TYPE		(XSHMEM_FSC_MAX_SHIFT - XSHMEM_FSC_MIN_SHIFT + 1U)
#define XSHMEM_FSC_ALIGN		(1U << XSHMEM_FSC_MIN_SHIFT)
/* exclude */
#define XSHMEM_FSC_MAX_ALLOC_SIZE	(1U << XSHMEM_FSC_MAX_SHIFT)
/*
 * The valid input size is above 0b10000.
 * valid return is 0,1,...,MAX_FSC_MAX_TYPE - 1
 */
STATIC int fsc_size2type(u32 size)
{
	return fls((int)size) - (int)XSHMEM_FSC_MIN_SHIFT - 1;
}

/* the block reagion is [start, start + size) */
struct fsc_block {
	bool is_free;
	u32 start;
	u32 size;
	struct list_head block_node;
	struct list_head free_node;
};

struct fsc_ctrl {
	u32 total_size;
	u32 free_size;
	DECLARE_BITMAP(free_map, XSHMEM_FSC_MAX_TYPE); /* bit0~bit27 represent type0~type27 */
	struct list_head block_head;
	struct list_head free_list[XSHMEM_FSC_MAX_TYPE];
};

/* insert the block to proper free_list */
STATIC void fsc_insert_block(struct fsc_ctrl *ctrl, struct fsc_block *block)
{
	int type = fsc_size2type(block->size);

	block->is_free = true;
	list_add_tail(&block->free_node, &ctrl->free_list[type]);
	ctrl->free_size += block->size;
	set_bit((u32)type, ctrl->free_map);
}

/* delete the block from free_list */
STATIC void fsc_delete_block(struct fsc_ctrl *ctrl, struct fsc_block *block)
{
	int type = fsc_size2type(block->size);

	block->is_free = false;
	list_del(&block->free_node);
	ctrl->free_size -= block->size;
	if (list_empty(&ctrl->free_list[type]) != 0)
		clear_bit((u32)type, ctrl->free_map);
}

STATIC bool fsc_algo_pool_same(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	struct fsc_ctrl *ctrl = (struct fsc_ctrl *)(xp->private_obj);

	return ctrl->total_size == arg->pool_size;
}

STATIC int fsc_algo_pool_init(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	int i;
	struct fsc_ctrl *ctrl = NULL;
	struct fsc_block *block = NULL;

	if ((!IS_ALIGNED(arg->pool_size, XSHMEM_FSC_ALIGN)) || (arg->pool_size == 0U) ||
		(arg->pool_size > XSHMEM_MAX_POOL_SIZE)) {
		pr_err("input xshm_pool size invalid, size: %ld\n", arg->pool_size);
		return -EINVAL;
	}

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL | __GFP_ACCOUNT);
	block = kmalloc(sizeof(*block), GFP_KERNEL | __GFP_ACCOUNT);
	if ((ctrl == NULL) || (block == NULL)) {
		kfree(ctrl);
		kfree(block);
		return -ENOMEM;
	}

	ctrl->total_size = (u32)(arg->pool_size);
	ctrl->free_size = 0;
	INIT_LIST_HEAD(&ctrl->block_head);
	for (i = 0; i < (int)ARRAY_SIZE(ctrl->free_list); i++)
		INIT_LIST_HEAD(&ctrl->free_list[i]);

	block->start = 0;
	block->size = (u32)(arg->pool_size);
	list_add_tail(&block->block_node, &ctrl->block_head);
	fsc_insert_block(ctrl, block);

	xp->private_obj = ctrl;

	return 0;
}

STATIC int fsc_algo_pool_free(struct xshm_pool *xp)
{
	struct fsc_block *block = NULL, *tmp = NULL;
	struct fsc_ctrl *ctrl = (struct fsc_ctrl *)(xp->private_obj);

	WARN(ctrl->total_size != ctrl->free_size, "The alloced block are not all free\n");
	list_for_each_entry_safe(block, tmp, &ctrl->block_head, block_node) {
		list_del(&block->block_node);
		kfree(block);
	}

	kfree(ctrl);

	return 0;
}

STATIC void fsc_algo_pool_show(struct xshm_pool *xp, struct seq_file *seq)
{
	struct fsc_ctrl *ctrl = (struct fsc_ctrl *)(xp->private_obj);

	seq_printf(seq, "    total_size: %#x, free_size: %#x\n", ctrl->total_size, ctrl->free_size);
}

STATIC struct fsc_block *find_free_block(struct fsc_ctrl *ctrl, u32 size)
{
	u32 type;
	struct fsc_block *block = NULL;

	type = (u32)fsc_size2type(size);
	type = (u32)find_next_bit(ctrl->free_map, XSHMEM_FSC_MAX_TYPE, (unsigned long)(type + 1U));
	if (type == XSHMEM_FSC_MAX_TYPE) {
		/*
		 * The list of which the significant bits is more than required,
		 * contains no more free block. We need to traverse the same level list.
		 */
		type = (u32)fsc_size2type(size);
		list_for_each_entry(block, &ctrl->free_list[type], free_node) {
			if (block->size >= size)
				return block;
		}
		pr_err("cannot find a xshmem_block\n");
		return NULL;
	} else {
		return list_first_entry(&ctrl->free_list[type], struct fsc_block, free_node);
	}
}

STATIC int split_new_block(struct fsc_ctrl *ctrl, struct fsc_block *block, u32 size)
{
	fsc_delete_block(ctrl, block);

	if (block->size < (size + XSHMEM_FSC_ALIGN))
		return (int)(block->start); /* Remaining space is not enough for new blk */

	struct fsc_block *new_obj = NULL;

	new_obj = kmalloc(sizeof(*new_obj), GFP_KERNEL | __GFP_ACCOUNT);
	if (new_obj == NULL) {
		fsc_insert_block(ctrl, block);
		pr_err("split: alloc new_obj block memory failed\n");
		return -ENOMEM;
	}
	new_obj->start = block->start + size;
	new_obj->size = block->size - size;
	block->size = size;

	list_add(&new_obj->block_node, &block->block_node);
	fsc_insert_block(ctrl, new_obj);

	return (int)(block->start);
}

STATIC int fsc_algo_block_alloc(struct xshm_pool *xp, struct xshm_block *blk)
{
	int ret;
	u32 aligned_size, size;
	struct fsc_block *block = NULL;
	struct fsc_ctrl *ctrl = (struct fsc_ctrl *)(xp->private_obj);

	if (blk->alloc_size >= XSHMEM_FSC_MAX_ALLOC_SIZE) {
		pr_err("alloc size(%lu) not support\n", blk->alloc_size);
		return -EOVERFLOW;
	}
	size = (u32)(blk->alloc_size);
	if (size == 0U) {
		pr_err("alloc size invalid\n");
		return -EINVAL;
	}

	aligned_size = ALIGN(size, XSHMEM_FSC_ALIGN);
	if ((aligned_size < size) || (aligned_size >= XSHMEM_FSC_MAX_ALLOC_SIZE)) {
		pr_err("alloc size too big\n");
		return -EOVERFLOW;
	}

	block = find_free_block(ctrl, aligned_size);
	if (block == NULL)
		return -ENOSPC;
	ret = split_new_block(ctrl, block, aligned_size);
	if (ret >= 0) {
		/* split success */
		blk->private_obj = block;
		blk->offset = (unsigned long)ret;
		blk->real_size = block->size;
		return 0;
	}

	return ret;
}

STATIC void check_and_combine_next_block(struct fsc_ctrl *ctrl, struct fsc_block *block)
{
	struct fsc_block *next = NULL;

	if (list_is_last(&block->block_node, &ctrl->block_head) != 0)
		return;

	next = (struct fsc_block *)list_next_entry(block, block_node);
	if (!next->is_free)
		return;

	fsc_delete_block(ctrl, next);
	block->size += next->size;

	list_del(&next->block_node);
	kfree(next);
}

STATIC void check_and_combine_prev_block(struct fsc_ctrl *ctrl, struct fsc_block *block)
{
	struct fsc_block *prev = NULL;

	if (block->block_node.prev == &ctrl->block_head)
		return;

	prev = (struct fsc_block *)list_prev_entry(block, block_node);
	if (!prev->is_free)
		return;

	fsc_delete_block(ctrl, prev);
	block->size += prev->size;
	block->start = prev->start;

	list_del(&prev->block_node);
	kfree(prev);
}

STATIC int fsc_algo_block_free(struct xshm_pool *xp, struct xshm_block *blk)
{
	struct fsc_ctrl *ctrl = (struct fsc_ctrl *)(xp->private_obj);
	struct fsc_block *block = (struct fsc_block *)blk->private_obj;

	if (block->is_free) {
		pr_err("free unalloced block\n");
		return -EFAULT;
	}

	check_and_combine_next_block(ctrl, block);
	check_and_combine_prev_block(ctrl, block);

	fsc_insert_block(ctrl, block);

	return 0;
}

struct xshm_pool_algo fsc_algo = {
	.num = XSHMEM_ALGO_FSC,
	.name = "fsc_algo",
	.xshm_pool_same = fsc_algo_pool_same,
	.xshm_pool_init = fsc_algo_pool_init,
	.xshm_pool_free = fsc_algo_pool_free,
	.xshm_pool_show = fsc_algo_pool_show,
	.xshm_block_alloc = fsc_algo_block_alloc,
	.xshm_block_free = fsc_algo_block_free,
};
