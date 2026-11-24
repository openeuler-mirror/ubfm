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
#include "ucd.h"
#include "umap_kern.h"
#include "uop_log.h"


#define module_umap "umap"

int umap_mod_init(void)
{
	int ret;

	ret = ucd_register_module_dev_ops(module_umap, NULL, NULL, umap_mmap);
	if (ret != 0) {
		uop_err("ucd_register_module_dev_ops. (ret=%d)\n", ret);
	}
	return ret;
}

void umap_mod_uninit(void)
{
	ucd_unregister_module_dev_ops(module_umap);
}
