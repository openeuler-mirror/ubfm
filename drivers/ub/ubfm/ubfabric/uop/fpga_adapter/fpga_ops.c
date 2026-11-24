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

#include "uop_log.h"
#include "fpga_adapter_kern.h"

const struct fpga_ops *fpga_ops_get_by_type(enum fpga_ops_type type)
{
	if (unlikely(type >= FPGA_MAX_OPS_TYPE)) {
		uop_err("Invalid param. (type=%u)\n", type);
		return NULL;
	}

	if (type == FPGA_REG_OPS_TYPE) {
		return fpga_ops_get_g_reg_ops();
	}

	return fpga_ops_get_g_api_ops();
}
