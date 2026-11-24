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
#ifndef APEI_DEF_H
#define APEI_DEF_H

#include <linux/kernel.h>
#include "ubfm_common.h"

/* default module, when no sub module id is specified, returns upon encountering the Default module */
#define DEFAULT_MODULE				  0xFFFFFFFF
/* Module IDs of different products are not unified, but it must be guaranteed that they are unified within a product, and will evolve toward unification in the future */
#define APEI_MODULE_UB_KUNPENG		  0x2d
#define APEI_MODULE_UCMI_KUNPENG		0x30
#define APEI_MODULE_UB_ASCEND		   0x31
#define APEI_MODULE_UCMI_ASCEND		 0x38

#define APEI_MODULE_DEF(module_name_, id_, reg_table_) \
	{ .name = (module_name_), .id = (id_), .reg_num = ARRAY_SIZE(reg_table_), .regs = (reg_table_) }

#define APEI_GROUP_REGS(reg_table_) \
	.group_num = ARRAY_SIZE(reg_table_), .group_regs = (reg_table_)
#define APEI_GROUP_NULL() \
	.group_num = 0, .group_regs = NULL

#define APEI_ASSOC_REGS(reg_table_) \
	.associated_num = ARRAY_SIZE(reg_table_), .associated_regs = (reg_table_)
#define APEI_ASSOC_NULL() \
	.associated_num = 0, .associated_regs = NULL

struct apei_reg {
	const char *name;
};

struct apei_group_reg {
	const char *name;
	u32 bit_start;
	u32 bit_len;
	u32 default_val;
};

struct apei_submodule {
	const char *name;
	u32 id;
	u32 reg_num;
	struct apei_reg *regs;
};

struct apei_fault_reg {
	const char *module_name;
	const char *name;
	u32 bit_start;
	u32 bit_len;
	u32 default_val;
	u32 event_id;
	u32 fault_level;
	u32 group_num;
	struct apei_group_reg *group_regs;
	u32 associated_num;
	struct apei_reg *associated_regs;
};

struct apei_submodule_table {
	u32 num;
	struct apei_submodule *modules;
};

struct apei_fault_reg_table {
	u32 num;
	struct apei_fault_reg *regs;
};

struct apei_module_id {
	u32 chip_type;
	u32 id;
};

#endif
