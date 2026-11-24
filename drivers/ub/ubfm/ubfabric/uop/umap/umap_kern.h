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
#ifndef UMAP_H
#define UMAP_H

#include <linux/types.h>
#include "ubfabric.h"

int umap_kern_mem_map(u32 devid, u32 die_id, const char *name, u64 acc_addr, size_t size);
int umap_kern_reg_map(u32 devid, u32 die_id, const char *name, u64 acc_addr, size_t size);
int umap_kern_unmap(u64 acc_addr, size_t size);
int umap_mem_map(u64 acc_addr, size_t size, struct ures_addr_info *mem, const char *name);

int umap_fpga_reg_map(u32 devid, u64 acc_addr, u64 phy_addr, size_t size);
int umap_fpga_reg_unmap(u64 acc_addr, size_t size);
int umap_mmap(u32 devid, struct vm_area_struct *vma);
#endif
