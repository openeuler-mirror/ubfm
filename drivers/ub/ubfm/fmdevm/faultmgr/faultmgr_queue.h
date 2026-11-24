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
#ifndef FAULTMGR_QUEUE_H
#define FAULTMGR_QUEUE_H

#include "ubfm_common.h"

#define FAULTMGR_CAPACITY_SHIFT_PER_DEV			 5	   /* 1 << 5 = 32, i.e. 32 buffers per concurrently online device */
#define FAULTMGR_SERIALNUM_MASK					 0xFFFFFFF   /* event serial number uses the low 28 bits for storage */

struct faultmgr_queue {
	u32 head;
	u32 tail;
	u32 max_devnum;
	u32 capacity;		   /* capacity is max_devnum << FAULTMGR_CAPACITY_SHIFT_PER_DEV */
	u32 mask;			   /* capacity mask */
	u32 is_empty;
	struct mami_fault_event data[];
};

int faultmgr_queue_init(void);
int faultmgr_queue_reinit(u32 devid);
void faultmgr_queue_uninit(void);
int faultmgr_queue_add_event(struct mami_fault_event *event);
int faultmgr_queue_get_event(struct mami_read_event_param *para);
bool faultmgr_queue_is_newest(u32 serial_num);
void faultmgr_queue_online_dev_inc(void);
void faultmgr_queue_online_dev_dec(void);
u32 faultmgr_queue_get_current_serialnum(void);

#endif
