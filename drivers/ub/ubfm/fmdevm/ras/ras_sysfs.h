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
#ifndef RAS_SYSFS_H
#define RAS_SYSFS_H

#include "ubfm_common.h"

struct ras_apei_inject_data {
	u32 sub_module_id;
	u32 core_id;
	u32 port_id;
	u32 reg_num;
	u32 *reg;
};

#endif /* End of RAS_SYSFS_H */
