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
#ifndef MSGQ_RQ_H
#define MSGQ_RQ_H

#include "msgq_comm.h"

int msgq_rqe_get(struct msgq_rq *rq, struct msgq_cqe *cqe, void *rep_data, u32 rep_len, u32 *real_len);
int msgq_rq_update(struct msgq_rq *rq, struct msgq_cqe *cqe);
struct msgq_rq *msgq_rq_create(u32 devid, u32 die_id, struct msgq_asic *asic);
void msgq_rq_destroy(struct msgq_rq *rq);
int msgq_rq_clean(struct msgq_rq *rq);

#endif
