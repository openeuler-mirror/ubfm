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
#include <linux/mm.h>

#include "uop_log.h"
#include "kmap_kern.h"
#include "uop_kern.h"

static int kmap_mem_map(u32 devid, struct kmap_addr *mem)
{
	u64 align_local_addr = mem->local_asic_addr & ~(PAGE_SIZE - 1);
	u64 align_offset = mem->local_asic_addr & (PAGE_SIZE - 1);
	size_t align_size = PAGE_ALIGN(mem->size + align_offset);

	void __iomem *va = ioremap(align_local_addr, align_size); /* Reserved memory cacheable? */;

	if (unlikely(va == NULL)) {
		uop_err("Mem ioremap fail. (devid=%u; die_id=%u; name=%s; size=%zu)\n",
			devid, mem->die_id, mem->name, mem->size);
		return -ENOMEM;
	}
	mem->acc_addr = (u64)(uintptr_t)va;
	return 0;
}

static void kmap_mem_unmap(u32 devid, struct kmap_addr *mem)
{
	void __iomem *va = (void __iomem *)(uintptr_t)mem->acc_addr;

	if (likely(va != NULL)) {
		iounmap(va);
		mem->acc_addr = (uintptr_t) NULL;
	}
}

static int kmap_reg_map(u32 devid, struct kmap_addr *reg)
{
	u64 align_local_addr = reg->local_asic_addr & ~(PAGE_SIZE - 1);
	u64 align_offset = reg->local_asic_addr & (PAGE_SIZE - 1);
	size_t align_size = PAGE_ALIGN(reg->size + align_offset);

	void __iomem *va = ioremap(align_local_addr, align_size);

	if (unlikely(va == NULL)) {
		uop_err("Reg ioremap fail. (devid=%u; die_id=%u; name=%s; size=%zu)\n",
			devid, reg->die_id, reg->name, reg->size);
		return -ENOMEM;
	}
	reg->acc_addr = (u64)(uintptr_t)va;
	return 0;
}

static void kmap_reg_unmap(u32 devid, struct kmap_addr *reg)
{
	void __iomem *va = (void __iomem *)(uintptr_t)reg->acc_addr;

	if (likely(va != NULL)) {
		iounmap(va);
		reg->acc_addr = (uintptr_t) NULL;
	}
}

static int kmap_acc_node_map(u32 devid, struct kmap_acc_node *acc_node, u32 acc_node_num)
{
	void __iomem *va = NULL;
	u32 i, j;

	for (i = 0; i < acc_node_num; i++) {
		va = ioremap(acc_node[i].local_asic_addr, acc_node[i].win_size);
		if (unlikely(va == NULL)) {
			uop_err("Acc node ioremap fail. (devid=%u; local_asic_addr=%#llx; size=%u)\n", devid,
				acc_node[i].local_asic_addr, acc_node[i].win_size);
			goto err_exit;
		}
		acc_node[i].acc_addr = (u64)(uintptr_t)va;
	}

	return 0;
err_exit:
	for (j = 0; j < i; j++) {
		va = (void __iomem *)(uintptr_t)(acc_node[j].acc_addr);
		if (likely(va != NULL)) {
			iounmap(va);
			acc_node[j].acc_addr = (u64)(uintptr_t) NULL;
		}
	}
	return -ENOMEM;
}

static void kmap_acc_node_unmap(u32 devid, struct kmap_addr *node)
{
	void __iomem *va = (void __iomem *)(uintptr_t)node->acc_addr;

	if (likely(va != NULL)) {
		iounmap(va);
		node->acc_addr = (uintptr_t) NULL;
	}
}

static const struct kmap_ops g_ldst_ops = {
	.mem_map = kmap_mem_map,
	.mem_unmap = kmap_mem_unmap,
	.reg_map = kmap_reg_map,
	.reg_unmap = kmap_reg_unmap,
	.acc_node_map = kmap_acc_node_map,
	.acc_node_unmap = kmap_acc_node_unmap
};

int kmap_pcie_ldst_init(void)
{
	return kmap_ops_register(UOP_ACC_PCIE_LDST, &g_ldst_ops);
}

void kmap_pcie_ldst_uninit(void)
{
	kmap_ops_unregister(UOP_ACC_PCIE_LDST);
}

