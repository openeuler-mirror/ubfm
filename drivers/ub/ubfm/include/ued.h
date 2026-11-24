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
#ifndef UED_H__
#define UED_H__

#include <linux/types.h>

#include "ubfm_common.h"
#include "udm.h"
#include "ubfabric.h"
#include "udm.h"

struct ued_published_event {
	u32 event_id;
	u32 subevent_id;
	u32 msg_len;
	u8 *msg;
};

/* ret = -EAGAIN, no subscriber, drv should try publish again */
int ued_submit_event_to_dev(u32 udevid, struct ued_published_event *event);

static inline int ued_submit_event(struct ued_published_event *event)
{
	return ued_submit_event_to_dev(MAMI_ANY_DEVID, event);
}

static inline int ued_submit_event_to_all(struct ued_published_event *event)
{
	int i, ret;

	ret = ued_submit_event_to_dev(MAMI_ANY_DEVID, event);
	for (i = 0; i < DEV_MAX_NUM; i++) {
		if (udm_is_udevid_exist(i)) {
			ret |= ued_submit_event_to_dev(i, event);
		}
	}

	return ret;
}

#endif

