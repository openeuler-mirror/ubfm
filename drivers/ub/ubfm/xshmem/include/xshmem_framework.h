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
#ifndef __USR_XSHMEM_FRAMEWORK_H
#define __USR_XSHMEM_FRAMEWORK_H

#define XSHM_KEY_SIZE 253U

struct xshm_reg_arg {
	int algo;
	unsigned long pool_size;
	unsigned int block_size; /* used for blk algorithm */
	unsigned int key_len;
	char *key;
};

struct xshm_block_arg {
	int pool_id;
	union {
		unsigned long size; /* for alloc input */
		unsigned long offset; /* for get, put, delete and alloc output */
	};
};

#ifndef LLT_ENABLE
#define XSHMEM_POOL_REGISTER		(_IOW('X', 1, struct xshm_reg_arg))
#define XSHMEM_POOL_UNREGISTER		(_IO('X', 2))
#define XSHMEM_BLOCK_ALLOC		(_IOWR('X', 3, struct xshm_block_arg))
#define XSHMEM_BLOCK_GET		(_IOW('X', 4, struct xshm_block_arg))
#define XSHMEM_BLOCK_PUT		(_IOW('X', 5, struct xshm_block_arg))
#else
#define XSHMEM_POOL_REGISTER		0
#define XSHMEM_POOL_UNREGISTER		1
#define XSHMEM_BLOCK_ALLOC		    2
#define XSHMEM_BLOCK_GET		    3
#define XSHMEM_BLOCK_PUT		    4
#endif

#define XSHMEM_ALGO_EMPTY	-1
#define XSHMEM_ALGO_BLOCK	0
#define XSHMEM_ALGO_FSC		1

#define XSHMEM_POOL_STATUS_DELETE 0
#define XSHMEM_POOL_STATUS_EXIST  1

#endif
