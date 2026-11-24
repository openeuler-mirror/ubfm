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
#ifndef MNG_HANDLE_UCD_H__
#define MNG_HANDLE_UCD_H__


#include <linux/types.h>

void mng_unregister_ucd_cmd(void);
int mng_register_ucd_cmd(void);

int mng_dev_release(u32 devid, pid_t pid);
int mng_dev_open(u32 devid, pid_t pid);
#endif

