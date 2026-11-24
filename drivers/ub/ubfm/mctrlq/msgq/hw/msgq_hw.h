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
#ifndef MSGQ_HW_H
#define MSGQ_HW_H

#include <linux/types.h>

#include "msgq_kern.h"

typedef int (*msgq_attr_pack_t)(struct msgq_attr *attr, u32 devid, u32 die_id);
typedef int (*msgq_attr_init_t)(u32 devid, u32 die_num);
typedef void (*msgq_attr_uninit_t)(u32 devid);

struct msgq_hw_ops {
	msgq_attr_init_t init;
	msgq_attr_pack_t get_attr;
	msgq_attr_uninit_t uninit;
};

void msgq_kern_hw_register(enum mami_udie_version ver, const struct msgq_hw_ops *ops);

int msgq_kern_hw_init(u32 devid);
void msgq_kern_hw_uninit(u32 devid);

#endif
