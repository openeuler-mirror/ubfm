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
#ifndef LOCAL_RAS_REGISTER_H
#define LOCAL_RAS_REGISTER_H

#include <linux/kernel.h>
#include <linux/list.h>

#include "apei_def.h"
#include "local_ras_def.h"

struct local_ras_module_table {
	u32 module_id;
	struct local_ras_module *module;
	struct list_head entry;
};

int local_ras_module_register(struct apei_module_id *id_map, u32 map_num, struct local_ras_module *module);
void local_ras_module_unregister(struct apei_module_id *id_map, u32 map_num);
int local_ras_submodule_get(u32 devid, u32 module_id, u32 submod_id, struct local_ras_submodule **submodule);

#endif
