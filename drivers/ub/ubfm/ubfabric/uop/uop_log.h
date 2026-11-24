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
#ifndef UOP_LOG_H
#define UOP_LOG_H

#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/smp.h>

#include "ulog.h"

#define UOP_MODULE_NAME "uop"

#define uop_err_limit(fmt, ...) mami_err_limit(UOP_MODULE_NAME, fmt, ##__VA_ARGS__)
#define uop_err(fmt, ...)	   mami_err_limit(UOP_MODULE_NAME, fmt, ##__VA_ARGS__)
#define uop_warn(fmt, ...)	  mami_warn(UOP_MODULE_NAME, fmt, ##__VA_ARGS__)
#define uop_info(fmt, ...)	  mami_info(UOP_MODULE_NAME, fmt, ##__VA_ARGS__)
#define uop_debug(fmt, ...)	 mami_debug(UOP_MODULE_NAME, fmt, ##__VA_ARGS__)

#endif
