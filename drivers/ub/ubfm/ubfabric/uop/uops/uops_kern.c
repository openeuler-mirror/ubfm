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
#include "mami_string.h"

#include "uop_log.h"
#include "ubfabric.h"
#include "uop.h"
#include "uop_kern.h"
#include "uops_kern.h"

static const struct uop_ops *g_uops[UOP_ACC_MAX];

int uops_kern_init(u32 devid, enum uop_acc_type type)
{
	int ret;

	if (unlikely((devid >= DEV_MAX_NUM) || (type >= UOP_ACC_MAX))) {
		uop_err("Invalid param. (devid=%u; type=%u)\n", devid, type);
		return -EINVAL;
	}

	if (unlikely(g_uops[type] == NULL)) {
		uop_err("Acc type not supported. (devid=%u; type=%u)\n", devid, type);
		return -EINVAL;
	}

	ret = uop_kern_register_ops(devid, g_uops[type], type);
	if (unlikely(ret != 0)) {
		uop_err("Uop register fail. (ret=%d; devid=%u)\n", ret, devid);
	}

	return ret;
}

void uops_kern_uninit(u32 devid)
{
	if (likely(devid < DEV_MAX_NUM)) {
		uop_kern_reset(devid);
	}
}

int uops_kern_register(enum uop_acc_type type, const struct uop_ops *ops)
{
	if (unlikely((type >= UOP_ACC_MAX) || (ops == NULL))) {
		uop_err("Invalid param. (type=%u)\n", type);
		return -EINVAL;
	}

	if (unlikely(g_uops[type] != NULL)) {
		uop_err("Uops already registered. (type=%u)\n", type);
		return -EINVAL;
	}
	g_uops[type] = ops;

	return 0;
}

void uops_kern_unregister(enum uop_acc_type type)
{
	if (likely(type < UOP_ACC_MAX)) {
		g_uops[type] = NULL;
	}
}

