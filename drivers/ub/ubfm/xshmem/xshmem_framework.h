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
#ifndef __XSHMEM_FRAMEWORK_H
#define __XSHMEM_FRAMEWORK_H

#include <linux/types.h>
#include <linux/seq_file.h>
#include "include/xshmem_framework.h"

#define XSHMEM_MAX_POOL_SIZE	(0x1UL << 31U) // max size definition, consistent with libxshmem.
#define XSHMEM_MAX_BLOCK_SIZE	(0x1UL << 31U)

#ifndef LLT_ENABLE
#define STATIC static
#else
#define STATIC
#endif

struct xshm_pool {
	int			pool_id;
	atomic_t		refcnt;

	/* Used to protect the list of TASK attached */
	spinlock_t		xp_task_spinlock;
	struct list_head	list_head;
	struct hlist_node	hnode;

	struct xshm_pool_algo   *algo;

	/* Used to serialize the alloc and free operation on the POOL */
	struct mutex		xp_block_mutex;
	struct list_head	block_head; /* for all alloced block */
	void                    *private_obj;

	int			key_len;
	/* MUST be the last element */
	char			key[];
};

struct xshm_block {
	int			refcnt;
	unsigned long		offset;
	unsigned long		alloc_size;
	unsigned long		real_size;

	struct list_head	block_node;
	void                    *private_obj;

	struct list_head	list_head;
};

#define ALGO_NAME_MAX 20
struct xshm_pool_algo {
	int num;
	char name[ALGO_NAME_MAX];
	struct list_head algo_node;
	bool (*xshm_pool_same)(struct xshm_pool *xp, struct xshm_reg_arg *arg);
	int (*xshm_pool_init)(struct xshm_pool *xp, struct xshm_reg_arg *arg);
	int (*xshm_pool_free)(struct xshm_pool *xp);
	void (*xshm_pool_show)(struct xshm_pool *xp, struct seq_file *seq);
	int (*xshm_block_alloc)(struct xshm_pool *xp, struct xshm_block *blk);
	int (*xshm_block_free)(struct xshm_pool *xp, struct xshm_block *blk);
};

extern struct xshm_pool_algo fsc_algo;
extern struct xshm_pool_algo blk_algo;

#endif
