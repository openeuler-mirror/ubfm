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
#include <linux/delay.h>

#include "mami_string.h"

#include "ubfabric.h"
#include "uops_kern.h"
#include "fpga_adapter_kern.h"

STATIC int pcie_ldst_mem_read_kern(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	int ret;

	ret = ldst_back_mem_read_kern(devid, addr, data, len);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

STATIC int pcie_ldst_mem_write_kern(u32 devid, u64 addr, void *data, u32 len, void *user_ctx)
{
	int ret;

	ret = ldst_back_mem_write_kern(devid, addr, data, len);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

STATIC int pcie_ldst_readl_kern(u32 devid, u64 addr, u32 *val, void *user_ctx)
{
	int ret;

	ret = ldst_back_readl_kern(devid, addr, val);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

STATIC int pcie_ldst_writel_kern(u32 devid, u64 addr, u32 val, void *user_ctx)
{
	int ret;

	ret = ldst_back_writel_kern(devid, addr, val);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

STATIC int pcie_ldst_readq_kern(u32 devid, u64 addr, u64 *val, void *user_ctx)
{
	int ret;

	ret = ldst_back_readq_kern(devid, addr, val);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

STATIC int pcie_ldst_writeq_kern(u32 devid, u64 addr, u64 val, void *user_ctx)
{
	int ret;

	ret = ldst_back_writeq_kern(devid, addr, val);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static const struct uop_ops g_ldst_ops = {
	.mem_read = pcie_ldst_mem_read_kern,
	.mem_write = pcie_ldst_mem_write_kern,
	.readl = pcie_ldst_readl_kern,
	.writel = pcie_ldst_writel_kern,
	.readq = pcie_ldst_readq_kern,
	.writeq = pcie_ldst_writeq_kern,
};

int uops_pcie_ldst_init(void)
{
	return uops_kern_register(UOP_ACC_PCIE_LDST, &g_ldst_ops);
}

void uops_pcie_ldst_uninit(void)
{
	uops_kern_unregister(UOP_ACC_PCIE_LDST);
}
