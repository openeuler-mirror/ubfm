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
#ifndef UDM_PUB_DEF_H__
#define UDM_PUB_DEF_H__

#include "ulog.h"

#define UB_FABRIC_UDM "udm"
#define udm_err(fmt, ...)   mami_err(UB_FABRIC_UDM, fmt, ##__VA_ARGS__)
#define udm_warn(fmt, ...)  mami_warn(UB_FABRIC_UDM, fmt, ##__VA_ARGS__)
#define udm_info(fmt, ...)  mami_info(UB_FABRIC_UDM, fmt, ##__VA_ARGS__)
#define udm_debug(fmt, ...) mami_debug(UB_FABRIC_UDM, fmt, ##__VA_ARGS__)
#define udm_event(fmt, ...) mami_event(UB_FABRIC_UDM, fmt, ##__VA_ARGS__)

#endif
