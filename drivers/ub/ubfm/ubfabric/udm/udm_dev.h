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
#ifndef UDM_DEV_H__
#define UDM_DEV_H__

#include <linux/types.h>

#include "fmdevm.h"
#include "udm.h"
#include "udm_cdev.h"
#include "udm_pub_def.h"

#define UDM_STATUS_INIT_MASK		BIT(0)
#define UDM_STATUS_PREPARE_MASK	 BIT(1)
#define UDM_STATUS_RESET_MASK	   BIT(2)
#define UDM_STATUS_AFTER_RESET_MASK BIT(3)
#define UDM_STATUS_IRT_INIT_MASK	BIT(4)

#define UDM_INVALID_TGID (-1)
struct udm_dev_inst {
	struct kref ref;
	u32 udevid;
	u32 inst_id;
	u32 status;
	pid_t mng_proc_tgid;
	struct udm_dev_para para;
	struct udm_cdev cdev;
};

static inline int udm_dev_para_valid_check(struct udm_dev_para *para)
{
	if ((para->location < UDM_LOCAL) || (para->location >= UDM_LOCATION_MAX) ||
		((para->udevid != UDM_INVALID_UDEVID) && (para->udevid >= DEV_MAX_NUM)) ||
		(para->chip_type == MAMI_KERN_TYPE_INVALID) || (para->chip_type >= MAMI_KERN_TYPE_MAX)) {
		udm_err("Invalid parameter. (udevid=%u; location=%u; chip_type=%u)\n",
			para->udevid, para->location, para->chip_type);
		return -EINVAL;
	}

	return 0;
}

static inline bool udm_is_init_status(u32 status)
{
	return ((status & UDM_STATUS_INIT_MASK) != 0);
}

static inline bool udm_is_prepare_status(u32 status)
{
	return ((status & UDM_STATUS_PREPARE_MASK) != 0);
}

static inline bool udm_is_reset_status(u32 status)
{
	return ((status & UDM_STATUS_RESET_MASK) != 0);
}

static inline bool udm_is_after_reset_status(u32 status)
{
	return ((status & UDM_STATUS_AFTER_RESET_MASK) != 0);
}

static inline bool udm_is_init_status_conflict(u32 status, enum udm_notified_action action)
{
	return ((udm_is_init_status(status) && (action == UDM_INIT)) ||
		(!udm_is_init_status(status) && (action == UDM_UNINIT)));
}

static inline bool udm_is_action_conflict(u32 status, enum udm_notified_action action)
{
	return udm_is_init_status_conflict(status, action);
}

static inline void udm_update_status_by_action(u32 *status, enum udm_notified_action action)
{
	if (action == UDM_INIT) {
		*status = UDM_STATUS_INIT_MASK;
	} else if (action == UDM_UNINIT) {
		*status &= ~UDM_STATUS_INIT_MASK;
	} else if (action == UDM_PREPARE) {
		*status |= UDM_STATUS_PREPARE_MASK;
	} else if (action == UDM_RESET) {
		*status |= UDM_STATUS_RESET_MASK;
	} else if (action == UDM_AFTER_RESET) {
		*status |= UDM_STATUS_AFTER_RESET_MASK;
	} else if (action == UDM_IRQ_INIT) {
		*status |= UDM_STATUS_IRT_INIT_MASK;
	} else if (action == UDM_IRQ_UNINIT) {
		*status &= ~UDM_STATUS_IRT_INIT_MASK;
	}
}

struct udm_dev_inst *udm_dev_inst_get(u32 udevid);
void udm_dev_inst_put(struct udm_dev_inst *dev_inst);

void udm_dev_init(void);
void udm_dev_uninit(void);

#endif
