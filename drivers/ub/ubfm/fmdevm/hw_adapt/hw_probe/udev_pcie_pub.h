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
#ifndef _UDEV_PCIE_PUB_H
#define _UDEV_PCIE_PUB_H

#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include "udev_hw_pub.h"
#include "fpga_adapter_kern.h"
#include "ubfabric.h"

struct udev_pcie {
	struct udev_hw_identify hw;
	bool chip_valid;
	u32 devid;
	u32 slot_id;
	u32 socket_id;
	u64 fpga_reg_base;
	u32 chip_type;
	u64 ucmi_reg_base; /* base address of the ucmi register table */
	chip_ub_mng chip_mng;
};

int udev_pcie_probe_all(struct ures_key *key, u32 num);
int udev_pcie_init(void);
int udev_pcie_uninit(void);
#endif
