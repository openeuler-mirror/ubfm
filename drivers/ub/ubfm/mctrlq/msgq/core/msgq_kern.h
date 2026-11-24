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
#ifndef MSGQ_KERN_H
#define MSGQ_KERN_H

#include <linux/types.h>

#include "msgq_pub.h"
#include "mctrlq.h"
#include "kref_safe.h"

struct msgq_dev {
	u32 devid;
	struct mutex lock;
	struct kref_safe ref;

	struct msgq_chan *chan[DIE_MAX_NUM];
};

struct msgq_attr {
	u32 sq_pld_size;
	struct msgq_asic sq;
	struct msgq_asic cq;
	struct msgq_asic rq;
};

void msgq_msn_put(struct msgq_chan *chan, enum msgq_type type, u32 msn);
int msgq_msg_check(u32 devid, u32 die_id, struct msgq_msg *msg);
int msgq_kern_send(u32 devid, u32 die_id, struct msgq_msg *msg);
int msgq_kern_send_async(u32 devid, u32 die_id, u16 msn, struct msgq_msg *msg);

int msgq_dpu_register_intf(u32 devid, struct udev_hw_identify *hw);
int msgq_kern_deque(u32 devid, u32 die_id);
void msgq_kern_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv);
int msgq_kern_chan_add(u32 devid, u32 die_id, struct msgq_attr *attr);
int msgq_update_when_recover(u32 devid, u32 die_id);
int msgq_kern_chan_check(u32 devid, u32 die_id);

struct msgq_chan *msgq_get_chan(struct msgq_dev *mdev, u32 die_id);
struct msgq_dev *msgq_get(u32 devid);
void msgq_put(struct msgq_dev *msgq);

int msgq_kern_init(u32 devid);
void msgq_kern_uninit(u32 devid);

int msgq_kern_mod_init(void);
int msgq_kern_get_snapshot(u32 devid, void *in, u32 in_len, void *out, u32 out_len);

#endif
