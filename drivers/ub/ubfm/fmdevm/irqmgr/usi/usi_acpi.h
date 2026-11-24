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
#ifndef USI_ACPI_H
#define USI_ACPI_H

#include <ub/ubus/ubus.h>

struct usi_iort_map_id {
	u32 map_id;
	u32 out_ref;
	u8 type;
};

int usi_cfgspace_to_its_pa_by_acpi(struct ub_entity *uent);

#endif
