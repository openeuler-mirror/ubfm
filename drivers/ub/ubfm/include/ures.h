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
#ifndef URES_H
#define URES_H

#include <linux/types.h>

#include "ubfabric.h"

int ures_set_mem_base(struct ures_inst *inst, const char *name, struct ures_addr_info *mem);
int ures_get_mem_base(struct ures_inst *inst, const char *name, struct ures_addr_info *mem);

int ures_get_mem_list(struct ures_inst *inst, struct ures_mem **mem, u32 *num);
void ures_put_mem_list(struct ures_mem *mem);

int ures_set_reg_base(struct ures_inst *inst, const char *name, struct ures_addr_info *reg);
int ures_get_reg_base(struct ures_inst *inst, const char *name, struct ures_addr_info *reg);

int ures_get_reg_list(struct ures_inst *inst, struct ures_reg **reg, u32 *num);
void ures_put_reg_list(struct ures_reg *reg);

int ures_set_irq_num(struct ures_inst *inst, u32 irq_type, u32 irq_num);
int ures_get_irq_num(struct ures_inst *inst, u32 irq_type, u32 *irq_num);

int ures_set_irq_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 irq);
int ures_get_irq_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 *irq);

int ures_set_irq_flag_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 flag);
int ures_get_irq_flag_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 *flag);

int ures_set_share_local_win(u32 devid, enum ures_win_type win_type, struct ures_win_info *win_info);
int ures_get_share_local_win(u32 devid, enum ures_win_type win_type, struct ures_win_info *win_info);

int ures_set_key_value(u32 devid, const char *name, u64 value);
int ures_get_key_value(u32 devid, const char *name, u64 *value);

int ures_dev_set_attr(u32 devid, const char *name, void *attr, size_t size);
int ures_dev_get_attr(u32 devid, const char *name, void *attr, size_t size);

int ures_set_attr(const char *name, void *attr, size_t size);
int ures_get_attr(const char *name, void *attr, size_t size);
int ures_del_attr(const char *name);

int ures_get_attr_key_list(struct ures_key **key, u32 *num);
void ures_put_attr_key_list(struct ures_key *key);

void ures_die_reset(struct ures_inst *inst);
void ures_dev_reset(u32 devid);
void ures_reset(void);

#endif
