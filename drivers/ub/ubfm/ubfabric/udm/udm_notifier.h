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
#ifndef UDM_NOTIFIER_H__
#define UDM_NOTIFIER_H__

#include <linux/rwsem.h>	/* sem */
#include <linux/list.h>	 /* list_head */

#include "udm.h"			/* UDM_PRI_MAX */

#define UDM_NAME_MAX_LEN 32
/* sizeof(struct udm_notifier_node) = (8 * 2 + 4 * 128 + 32 + 8 + 4 + 4) = 576byte.  */
struct udm_notifier_node {
	struct list_head node;
	u32 status[DEV_MAX_NUM];
	char notifier[UDM_NAME_MAX_LEN];
	udm_notify func;
	u32 call_count;
	u32 call_finish;
};

struct udm_notifiers {
	struct list_head pri_head[UDM_PRI_MAX];
	struct rw_semaphore sem;
};

struct udm_notifiers *udm_get_notifiers(void);

void udm_notifier_init(void);
void udm_notifier_uninit(void);

#endif
