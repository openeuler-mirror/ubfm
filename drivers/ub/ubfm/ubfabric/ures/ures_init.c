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

#include "ures_log.h"
#include "ures_proc.h"
#include "ures_core.h"
#include "ures_fops.h"

int ures_mod_init(void)
{
	int ret;

	ures_init();
	ures_proc_init();

	ret = ures_fops_init();
	if (ret != 0) {
		ures_proc_uninit();
		ures_uninit();
		ures_err("Ures fops init fail. (ret=%d)\n", ret);
	}

	return ret;
}
DECLARE_FEATURE_AUTO_INIT(ures_mod_init, FEATURE_LOADER_STAGE_1);

void ures_mod_uninit(void)
{
	ures_fops_uninit();
	ures_proc_uninit();
	ures_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(ures_mod_uninit, FEATURE_LOADER_STAGE_1);

