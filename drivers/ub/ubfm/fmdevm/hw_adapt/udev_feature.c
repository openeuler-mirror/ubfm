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
#include "udev_hw_pub.h"
#include "fmdevm.h"
#include "ubfabric.h"
#include "idm.h"
#include "udev_feature.h"

#define MAX_MUE_NUM_8  8
#define MAX_MUE_NUM_16  16
#define MAX_MUE_NUM_32  32

struct dev_mng_intf_group g_intf_group[CHNL_TYPE_MAX];

/* When a new chip type is added, the value needs to be changed accordingly. */
u32 g_udie_mue_num[MAMI_KERN_TYPE_MAX] = {MAX_MUE_NUM_16, MAX_MUE_NUM_16, MAX_MUE_NUM_16, MAX_MUE_NUM_16, MAX_MUE_NUM_16, MAX_MUE_NUM_16,
								MAX_MUE_NUM_8, MAX_MUE_NUM_8, MAX_MUE_NUM_8, MAX_MUE_NUM_8, MAX_MUE_NUM_8, MAX_MUE_NUM_32};
u32 g_mng_chnl_type[MAMI_KERN_TYPE_MAX] = {MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE,
								MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, MAMI_CHNL_TYPE, DPU_CHNL_TYPE};

u32 get_udie_mue_max_num(u32 chip_type)
{
	if (chip_type >= MAMI_KERN_TYPE_MAX) {
		return MAX_MUE_NUM_16;
	}
	return g_udie_mue_num[chip_type];
}

struct dev_mng_intf_group *udev_get_mng_intf(u32 type)
{
	return &(g_intf_group[type]);
}

u32 udev_kern_get_mng_chnl_type(u32 chip_type)
{
	if (chip_type >= MAMI_KERN_TYPE_MAX) {
		return MAMI_CHNL_TYPE;
	}
	return g_mng_chnl_type[chip_type];
}

int udev_register_intf(u32 type, struct dev_mng_intf_group *intf)
{
	struct dev_mng_intf_group *intf_group = NULL;
	int ret;

	if (type >= CHNL_TYPE_MAX) {
		return -EINVAL;
	}
	intf_group = udev_get_mng_intf(type);
	ret = ubfm_memcpy(intf_group, sizeof(struct dev_mng_intf_group), intf, sizeof(struct dev_mng_intf_group));
	if (ret != 0) {
		hw_err("reg intf fail %u.\n", type);
		return -EFAULT;
	}
	return 0;
}
