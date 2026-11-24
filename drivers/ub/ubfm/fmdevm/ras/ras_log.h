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
#ifndef RAS_LOG_H
#define RAS_LOG_H

#include "ulog.h"

#define MODULE_RAS_NAME "RAS"

#define ras_err(fmt, ...)	  mami_err_limit(MODULE_RAS_NAME, fmt, ##__VA_ARGS__)
#define ras_warn(fmt, ...)	 mami_warn(MODULE_RAS_NAME, fmt, ##__VA_ARGS__)
#define ras_info(fmt, ...)	 mami_info(MODULE_RAS_NAME, fmt, ##__VA_ARGS__)
#define ras_debug(fmt, ...)	mami_debug(MODULE_RAS_NAME, fmt, ##__VA_ARGS__)
#define ras_err_limit(fmt, ...)	mami_err_limit(MODULE_RAS_NAME, fmt, ##__VA_ARGS__)

#endif
