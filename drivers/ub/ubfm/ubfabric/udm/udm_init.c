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
#include "udm_dev.h"
#include "udm_notifier.h"
#include "udm_proc_fs.h"

int udm_init(void)
{
	int ret;

	udm_dev_init();
	udm_notifier_init();

	ret = udm_cdev_init();
	if (ret != 0) {
		udm_notifier_uninit();
		udm_dev_uninit();
		return ret;
	}

	udm_proc_fs_init();
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(udm_init, FEATURE_LOADER_STAGE_2);

void udm_uninit(void)
{
	udm_proc_fs_uninit();
	udm_cdev_uninit();
	udm_notifier_uninit();
	udm_dev_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(udm_uninit, FEATURE_LOADER_STAGE_2);
