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
#ifndef MNG_KERNEL_H__
#define MNG_KERNEL_H__


#include "ulog.h"

#define MODULE_MNG "mng"

#define mng_err(fmt, ...)	  mami_err(MODULE_MNG, fmt, ##__VA_ARGS__)
#define mng_warn(fmt, ...)	  mami_warn(MODULE_MNG, fmt, ##__VA_ARGS__)
#define mng_info(fmt, ...)	  mami_info(MODULE_MNG, fmt, ##__VA_ARGS__)
#define mng_debug(fmt, ...)	  mami_debug(MODULE_MNG, fmt, ##__VA_ARGS__)


#endif

