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
#ifndef LOCAL_RAS_DEF_H
#define LOCAL_RAS_DEF_H

#include <linux/kernel.h>
#include <linux/list.h>

#include "ubfm_common.h"

#define LOCAL_RAS_REG_BITWIDTH				  32
#define LOCAL_RAS_MODULE_NAME_LEN			   32
#define LOCAL_RAS_REG_NAME_LEN				  128
struct local_ras_submodule;
struct local_ras_reg;

struct local_ras_group_reg_field {
	u32 bit_start;
	u32 bit_len;
	u32 default_val;
	struct local_ras_reg *reg;
	struct list_head entry;
};

struct local_ras_reg_field {
	u32 bit_start;
	u32 bit_len;
	u32 default_val;
	u32 event_id;
	u32 fault_level;
	struct local_ras_reg *reg;
	struct list_head entry;
	struct list_head group_regs;
	struct list_head associate_regs;
};

struct local_ras_reg {
	char name[LOCAL_RAS_REG_NAME_LEN];
	u32 reg_idx;
	struct local_ras_submodule *module;
	struct list_head fields;
};

struct local_ras_assoc_reg {
	struct local_ras_reg *reg;
	struct list_head entry;
};

struct local_ras_submodule {
	char name[LOCAL_RAS_MODULE_NAME_LEN];
	u32 id;
	u32 reg_num;
	struct local_ras_reg *regs;
	struct list_head entry;
};

struct local_ras_module {
	struct list_head sub_modules;
};

#endif
