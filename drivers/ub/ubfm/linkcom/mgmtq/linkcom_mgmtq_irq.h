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
#ifndef LINKCOM_MGMTQ_IRQ_H_
#define LINKCOM_MGMTQ_IRQ_H_

#include "uid.h"

void mgmtq_intr_pre_handle(u32 devid, enum uid_intr_type type, u64 para);
void mgmtq_intr_handle(u32 devid, enum uid_intr_type type, u32 flag,  struct uid_irq_info irq_info);
int mgmtq_irq_init(u32 devid);
void mgmtq_irq_uninit(u32 devid);
void mgmtq_irq_hw_init(void);
bool linkcom_get_outband_enable(void);
void linkcom_set_outband_enable(bool enable);
#endif
