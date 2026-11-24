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
#ifndef _UDEV_HW_PUB_H_
#define _UDEV_HW_PUB_H_

#include <linux/types.h>
#include "mami_string.h"
#include "ulog.h"
#include "ubfm_common.h"
#include "fmdevm.h"

#define MODULE_HW "hw-adapt"

#define hw_err(fmt, ...)	mami_err(MODULE_HW, fmt, ##__VA_ARGS__)
#define hw_warn(fmt, ...)   mami_warn(MODULE_HW, fmt, ##__VA_ARGS__)
#define hw_info(fmt, ...)   mami_info(MODULE_HW, fmt, ##__VA_ARGS__)
#define hw_debug(fmt, ...)  mami_debug(MODULE_HW, fmt, ##__VA_ARGS__)

#define hw_info_limit(fmt, ...)   mami_info_limit(MODULE_HW, fmt, ##__VA_ARGS__)

enum udev_acc_type {
	UDEV_LDST = 0,
	UDEV_CDMA,
	UDEV_PCIE_LDST,
	UDEV_PCIE_I3C_LDST,
	UDEV_ACC_TYPE_NUM
};

#define MAMI_UDEV_CFG_HDR	   "udev_cfg_hdr"
#define MAMI_UDEV_CFG_INFO	  "udev_cfg_info"
#define MAMI_UDEV_IO_INFO	   "udev_io_info"

#define SIZE_KB(size) ((size) * 0x400)
#define SIZE_MB(size) ((size) * 0x100000)

#define UCMI_REGBANK_BASE_KUNPENG950	0x3002620000UL
#define UCMI_REGBANK_BASE_ASCEND_V1_UNIONS  0x20002620000UL

#define UCMI_LOCK_OFFSET			0x38
#define UCMI_UNLOCK_VAL			 0x5A5A5A5A
#define UCMI_LOCK_VAL			   0xFFFF0000

struct ucmi_pcie_axi_node;

int udev_init_hw_info(u32 devid, u32 chip_type);
void udev_uninit_hw_info(u32 devid, u32 chip_type);
int udev_init_win_axi(u32 devid, u32 chip_type, u64 ucmi_reg_base, struct ucmi_pcie_axi_node *node);

int udev_ctrl_set_dev_info(u32 devid);

#endif
