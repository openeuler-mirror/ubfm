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
#ifndef URES_CORE_H
#define URES_CORE_H

#include <linux/compiler.h>
#include <linux/errno.h>

#include "ubfabric.h"

void ures_die_res_show(struct ures_inst *inst, void (*show)(void *priv, const char *fmt, ...), void *priv);
void ures_dev_res_show(u32 devid, void (*show)(void *priv, const char *fmt, ...), void *priv);
void ures_show(void (*show)(void *priv, const char *fmt, ...), void *priv);

void ures_init(void);
void ures_uninit(void);

#endif
