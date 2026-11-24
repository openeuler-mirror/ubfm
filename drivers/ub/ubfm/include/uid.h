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
#ifndef UID_H__
#define UID_H__

#include <linux/types.h>
#include <linux/module.h>

#define UID_HANDLE_TIMEOUT_FLAG (0x1 << 0)

enum uid_intr_type {
	UID_TYPE_MSGQ = 0,
	UID_TYPE_CTRLQ = 1,
	UID_TYPE_VNIC = 2,
	UID_TYPE_RAS = 3,
	UID_TYPE_MGMTQ = 4,
	UID_TYPE_MAX
};

typedef union {
	struct {
		u64 dieid : 8;
		u64 rsv : 55;
		u64 in_irq : 1;
	} ras;
	struct {
		u64 dieid : 8;
		u64 chan_status : 17;
		u64 rsv : 38;
		u64 in_irq : 1;
	} ctrlq;
	struct {
		u64 dieid : 8;
		u64 chan_status : 4;
		u64 rsv : 51;
		u64 in_irq : 1;
	} mgmtq;
	u64 val;
} uid_irq_para;

struct uid_irq_info {
	uid_irq_para irq_para;
	u64 irq_time; /* ms. Record the timing of MSI/USI interrupt handling */
};

struct uid_intr_attr {
	u32 block_flag; /* 0: no block, uid will callback func in tasklet; else block in work */
	/* When interrupts are not enabled or lost, UID ensures that it is scheduled callback_func once according to the
	 * max_sched_time. set to 0 not enable this.
	 */
	u32 max_sched_time; /* ms. set 0 not use this feature */
};

struct uid_intr_ops {
	struct module *owner;
	/* not must, call in the top half of the interruption */
	void (*top_half_pre_handle)(u32 udevid, enum uid_intr_type type, u64 para);
	/* must, call in the tasklet(when block_flag is 0) or work(when block_flag is not 0) */
	void (*handle)(u32 udevid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info);
};

int uid_register_intr(enum uid_intr_type type, struct uid_intr_attr *attr, struct uid_intr_ops *ops);
int uid_unregister_intr(enum uid_intr_type type);

/* called by the interrupt service routine in top half of the interruption */
void uid_trigger_intr(u32 udevid, enum uid_intr_type type, struct uid_irq_info irq_info);

#endif

