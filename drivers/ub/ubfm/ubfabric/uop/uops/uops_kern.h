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
#ifndef UOPS_H
#define UOPS_H

#include <linux/types.h>

#include "ubfabric.h"

int uops_kern_init(u32 devid, enum uop_acc_type type);
void uops_kern_uninit(u32 devid);
int uops_kern_register(enum uop_acc_type type, const struct uop_ops *ops);
void uops_kern_unregister(enum uop_acc_type type);

#endif
