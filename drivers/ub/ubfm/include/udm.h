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
#ifndef UDM_H__
#define UDM_H__

#include <linux/device.h>

#include "ubfm_common.h"
#include "ubfm_common.h"


#define UDM_INVALID_UDEVID 0xffffffffU
struct udm_dev_para {
	/* software info */
	u32 udevid;
	u32 cna;
	u32 eid;
	u32 socket_id;
	enum udm_dev_location location;
	/* hardware info */
	u32 chip_type;
	u32 connect_type;
	u8 guid[MAMI_GUID_LEN];
};

int udm_add_dev(struct udm_dev_para *para, u32 *udevid);
int udm_remove_dev(u32 udevid);

enum udm_dev_ctrl_cmd {
	UDM_CTRL_MAX
};

int udm_dev_ctrl(u32 udevid, enum udm_dev_ctrl_cmd cmd, u32 val);

/* Method of the device notifier */
enum udm_priority { /* high Priority to be notified first */
	UDM_PRI0 = 0, /* hign, init ures */
	UDM_PRI1 = 1, /* ubfabric basic module */
	UDM_PRI2 = 2, /* other module */
	UDM_PRI3 = 3, /* low, manage module */
	UDM_PRI4 = 4,
	UDM_PRI5 = 5,
	UDM_PRI_MAX
};

enum udm_notified_action {
	UDM_INIT,
	UDM_UNINIT,
	UDM_PREPARE,
	UDM_RESET,
	UDM_AFTER_RESET,
	UDM_IRQ_INIT,
	UDM_IRQ_UNINIT,
	UDM_ACTION_MAX
};

typedef int (*udm_notify)(u32 udevid, enum udm_notified_action action, u32 val);
int udm_notifier_register(const char *notifier, enum udm_priority pri, udm_notify func);
int udm_notifier_unregister(const char *notifier);

/* Method of the device probe or notifier */
bool udm_is_udevid_exist(u32 udevid);
int udm_get_dev_attr(u32 udevid, struct udm_dev_para *dev_para);
int udm_set_dev_attr(u32 udevid, struct udm_dev_para *dev_para);
u32 udm_get_local_devid(void);
int udm_notifier_call(u32 udevid, enum udm_notified_action action);

int udm_dev_release(u32 udevid, pid_t pid);
int udm_set_managed_proc_pid(u32 udevid);
#endif
