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
#ifndef APEI_TBL_LOAD_H
#define APEI_TBL_LOAD_H

#include <linux/kernel.h>
#include <linux/list.h>

#include "apei_def.h"
#include "local_ras_def.h"

int local_ras_module_from_config(struct apei_submodule_table *submod_tbl, struct apei_fault_reg_table *reg_tbl,
								 struct local_ras_module *module);
void local_ras_module_destroy(struct local_ras_module *module);

#endif
