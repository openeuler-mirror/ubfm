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
#ifndef FAULTMGR_CORE_H
#define FAULTMGR_CORE_H

#include <linux/kernel.h>

int faultmgr_ucd_event_add(void *in, u32 in_len, void *out, u32 out_len);
int faultmgr_ucd_event_get(void *in, u32 in_len, void *out, u32 out_len);
int faultmgr_ucd_get_next_event_index(void *in, u32 in_len, void *out, u32 out_len);

#endif
