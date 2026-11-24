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
#ifndef MSGQ_DISPATCH_H
#define MSGQ_DISPATCH_H

#include <linux/types.h>

#include "mctrlq.h"
#include "msgq_pub.h"
#include "ued.h"

struct vdm_opcode_to_event {
	u32 opcode;
	u32 sub_opcode;
	u32 event_id;
	u32 subevent_id;
};

int msgq_to_event_id(struct msgq_cq *cq, struct ued_published_event *ued_event);

#endif
