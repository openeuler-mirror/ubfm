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
#ifndef UBDEVM_PORT_PUB_H__
#define UBDEVM_PORT_PUB_H__

#include "ulog.h"

#define MODULE_PORT_MNG_NAME "udev_port_mng"

#define udev_port_err(fmt, ...)	  mami_err(MODULE_PORT_MNG_NAME, fmt, ##__VA_ARGS__)
#define udev_port_warn(fmt, ...)	 mami_warn(MODULE_PORT_MNG_NAME, fmt, ##__VA_ARGS__)
#define udev_port_info(fmt, ...)	 mami_info(MODULE_PORT_MNG_NAME, fmt, ##__VA_ARGS__)
#define udev_port_debug(fmt, ...)	mami_debug(MODULE_PORT_MNG_NAME, fmt, ##__VA_ARGS__)

#endif
