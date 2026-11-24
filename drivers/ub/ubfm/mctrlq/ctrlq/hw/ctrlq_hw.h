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
#ifndef CTRLQ_HW_H
#define CTRLQ_HW_H

#include <linux/types.h>
#include "ctrlq_kern.h"
#include "mctrlq.h"

#define CTRLQ_HW_MAX_CHAN_NUM	   45U

typedef int (*ctrlq_attr_pack_t)(struct ctrlq_attr *attr, u32 die_id, u32 chan_id, struct ctrlq_global_info *ctrlq);

struct ctrlq_hw_ops {
	ctrlq_attr_pack_t attr_pack;
};

int ctrlq_hw_init(u32 devid, u32 chip_type);
void ctrlq_hw_uninit(u32 devid);

void ctrlq_hw_ops_register(const struct ctrlq_hw_ops *ops);
void ctrlq_irq_proc(u32 devid, u32 flag, u64 para);
void ctrlq_irq_triggered(u32 devid, u64 para);

#endif
