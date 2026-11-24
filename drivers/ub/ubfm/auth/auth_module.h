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
#ifndef AUTH_MODULE_H
#define AUTH_MODULE_H

#include <linux/types.h>
#include <linux/ktime.h>
#include "include/auth_ioctl.h"

#define AUTH_MAX_TIMESTAMP_QUEUE_SIZE 200U

#define AUTH_EVENT_NO_TIMEOUT_FLAG (-1)
#define AUTH_EVENT_WAIT_TIME_MIN 100 /* Minimum waiting time: 100ms */

struct auth_task_node {
	struct proc_info proc_info;
	atomic_t status;
	struct list_head list;
	wait_queue_head_t wait_queue;
	bool is_used; /* Prevent GET_PROCNAME from repeatedly acquiring the same node in multithreading */
};

struct auth_timestamp_node {
	pid_t pid;
	struct list_head list;
	ktime_t timestamp_queue[AUTH_MAX_TIMESTAMP_QUEUE_SIZE];
	unsigned int queue_head;
	unsigned int queue_tail;
	unsigned int queue_size;
};

#endif
