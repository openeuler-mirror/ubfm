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
#ifndef UID_CORE_H__
#define UID_CORE_H__

#include "ulog.h"

#define module_uid "uid"

#define uid_err(fmt, ...)	  mami_err(module_uid, fmt, ##__VA_ARGS__)
#define uid_warn(fmt, ...)	  mami_warn(module_uid, fmt, ##__VA_ARGS__)
#define uid_info(fmt, ...)	  mami_info(module_uid, fmt, ##__VA_ARGS__)
#define uid_debug(fmt, ...)	  mami_debug(module_uid, fmt, ##__VA_ARGS__)

int uid_core_init(void);
void uid_core_uninit(void);

int uid_add_dev(u32 devid);
int uid_del_dev(u32 devid);

#endif

