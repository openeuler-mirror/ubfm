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
#include "feature_loader.h"
#include "udm.h"

#include "uid.h"
#include "uid_core.h"

static int uid_dev_notify(u32 udevid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = uid_add_dev(udevid);
	} else if (action == UDM_UNINIT) {
		ret = uid_del_dev(udevid);
	}

	return ret;
}

int uid_init(void)
{
	int ret;

	ret = uid_core_init();
	if (ret != 0) {
		return ret;
	}

	ret = udm_notifier_register(module_uid, UDM_PRI5, uid_dev_notify);
	if (ret != 0) {
		uid_core_uninit();
		uid_err("Failed to register udm notify. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(uid_init, FEATURE_LOADER_STAGE_3);

void uid_uninit(void)
{
	udm_notifier_unregister(module_uid);
	uid_core_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(uid_uninit, FEATURE_LOADER_STAGE_3);

