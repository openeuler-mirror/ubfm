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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "mami_string.h"

#include "ubfabric.h"
#include "uops_kern.h"

static int ldst_mem_read(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	return (ubfm_memcpy(data, len, (void *)(uintptr_t)addr, len) != 0) ? -ENOMEM : 0;
}

static int ldst_mem_write(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	return (ubfm_memcpy((void *)(uintptr_t)addr, len, data, len) != 0) ? -ENOMEM : 0;
}

static int ldst_readl(u32 devid, u64 addr, u32 *val, void *user_ctx)
{
	*val = readl((const volatile void __iomem *)(void *)(uintptr_t)addr);
	return 0;
}

static int ldst_writel(u32 devid, u64 addr, u32 val, void *user_ctx)
{
	writel(val, (volatile void __iomem *)(void *)(uintptr_t)addr);
	return 0;
}

static int ldst_readq(u32 devid, u64 addr, u64 *val, void *user_ctx)
{
	*val = readq((const volatile void __iomem *)(void *)(uintptr_t)addr);
	return 0;
}

static int ldst_writeq(u32 devid, u64 addr, u64 val, void *user_ctx)
{
	writeq(val, (volatile void __iomem *)(void *)(uintptr_t)addr);
	return 0;
}

static const struct uop_ops g_ldst_ops = {
	.mem_read = ldst_mem_read,
	.mem_write = ldst_mem_write,
	.readl = ldst_readl,
	.writel = ldst_writel,
	.readq = ldst_readq,
	.writeq = ldst_writeq
};

int uops_ldst_init(void)
{
	return uops_kern_register(UOP_ACC_LDST, &g_ldst_ops);
}

void uops_ldst_uninit(void)
{
	uops_kern_unregister(UOP_ACC_LDST);
}

