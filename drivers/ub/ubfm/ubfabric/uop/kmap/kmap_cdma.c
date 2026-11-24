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
#include <linux/io.h>
#include <asm/io.h>
#include <linux/compiler.h>

#include "uop_log.h"
#include "kmap_kern.h"

static int kmap_mem_map(u32 devid, struct kmap_addr *mem)
{
	u64 align_local_addr = mem->local_asic_addr & ~(PAGE_SIZE - 1);

	UNUSED(devid);
	mem->acc_addr = (u64)(uintptr_t)align_local_addr;
	return 0;
}

static void kmap_mem_unmap(u32 devid, struct kmap_addr *mem)
{
	UNUSED(devid);
	mem->acc_addr = 0;
}

static int kmap_reg_map(u32 devid, struct kmap_addr *reg)
{
	u64 align_local_addr = reg->local_asic_addr & ~(PAGE_SIZE - 1);

	UNUSED(devid);
	reg->acc_addr = (u64)(uintptr_t)align_local_addr;
	return 0;
}

static void kmap_reg_unmap(u32 devid, struct kmap_addr *reg)
{
	UNUSED(devid);
	reg->acc_addr = 0;
}

static const struct kmap_ops g_cdma_ops = {
	.mem_map = kmap_mem_map,
	.mem_unmap = kmap_mem_unmap,
	.reg_map = kmap_reg_map,
	.reg_unmap = kmap_reg_unmap
};

int kmap_cdma_init(void)
{
	return kmap_ops_register(UOP_ACC_CDMA, &g_cdma_ops);
}

void kmap_cdma_uninit(void)
{
	kmap_ops_unregister(UOP_ACC_CDMA);
}

