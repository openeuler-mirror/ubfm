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
#ifndef MSGQ_SQ_H
#define MSGQ_SQ_H

#include "msgq_comm.h"

int msgq_sq_send(struct msgq_chan *chan, struct msgq_msg *msg, u32 msn, bool sync, struct msgq_record *record);
struct msgq_sq *msgq_sq_create(u32 devid, u32 die_id, struct msgq_asic *asic);
void msgq_sq_destroy(struct msgq_sq *sq);
int msgq_sq_clean(struct msgq_sq *sq);

void msgq_sq_read_reg(struct msgq_sq *sq, u32 *head, u32 *tail);

#endif
