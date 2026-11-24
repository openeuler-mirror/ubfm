// SPDX-License-Identifier: GPL-2.0-only
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
#include "port_pub.h"
#include "feature_loader.h"
#include "udm.h"
#include "port_info.h"

int udev_dev_port_mng_init(u32 devid)
{
	int ret;

	ret = port_info_init(devid);
	if (ret != 0) {
		udev_port_err("Port info init failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	return 0;
}

int udev_dev_port_mng_uninit(u32 devid)
{
	(void)devid;
	return 0;
}

static int udev_port_mng_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = udev_dev_port_mng_init(devid);
	} else if (action == UDM_UNINIT) {
		ret = udev_dev_port_mng_uninit(devid);
	}

	return ret;
}

int udev_port_mng_init(void)
{
	int ret;

	ret = udm_notifier_register(MODULE_PORT_MNG_NAME, UDM_PRI2, udev_port_mng_notify);
	if (ret != 0) {
		udev_port_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(udev_port_mng_init, FEATURE_LOADER_STAGE_6);

void udev_port_mng_uninit(void)
{
	(void)udm_notifier_unregister(MODULE_PORT_MNG_NAME);
}
DECLARE_FEATURE_AUTO_UNINIT(udev_port_mng_uninit, FEATURE_LOADER_STAGE_6);
