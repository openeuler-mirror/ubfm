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
#ifndef _UDEV_FEATURE_H
#define _UDEV_FEATURE_H

#include "ubfm_common.h"
#include "fmdevm.h"

enum mamiDevChnlType {
	MAMI_CHNL_TYPE = 0,
	DPU_CHNL_TYPE = 1,
	CHNL_TYPE_MAX
};

struct dev_mng_intf_group {
	int (*hw_probe)(struct mami_dev_info *dev_info);
	int (*hw_init)(u32 devid, int chip_type, int conn_type, int acc_type);
};
u32 udev_kern_get_mng_chnl_type(u32 chip_type);
int udev_register_intf(u32 type, struct dev_mng_intf_group *intf);
struct dev_mng_intf_group *udev_get_mng_intf(u32 type);

u32 get_udie_mue_max_num(u32 chip_type);
#endif
