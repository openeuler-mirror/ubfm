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
#ifndef UDEV_UCMI_ESPI_H
#define UDEV_UCMI_ESPI_H

#include <linux/types.h>
#include <linux/mutex.h>

#include "udev_espi_pub.h"
#include "udev_ucmi_ops.h"

#define UCMI_CTRL_OFFSET			0x000
#define UCMI_MST_IDLE_CNT_OFFSET	0x4C
#define UCMI_SCLK_OUT_DIV_OFFSET	0x4
#define UCMI_MST_PIPE_EN_OFFSET	 0x304
#define UCMI_OT_GATE_OFFSET		 0x1C
#define UCMI_MST_TX_CFG_OFFSET	  0x560
#define UCMI_MST_RX_CFG_OFFSET	  0x568
#define UCMI_FAULT_CLR_OFFSET	   0x28
#define UCMI_RAS_CE_EN_OFFSET	   0x5B0
#define UCMI_RAS_NFE_EN_OFFSET	  0x5B4
#define UCMI_RAS_FE_EN_OFFSET	   0x5B8
#define UCMI_ABNOR_IMSK_OFFSET	  0x10

#define UCMI_SRC_4M_OFFSET		  0x14
#define UCMI_SRC_14M_OFFSET		 0x18

struct ucmi_axi_node {
	u64 base_addr;
	u32 axi_id;
	u32 count;
};

struct ucmi_win_node {
	struct udev_ucmi_win win;
	u32 die_id;
	u32 win_id;
	u32 win_type;
	u32 win_val;
	struct ucmi_axi_node *axi;
};

struct ucmi_win_cfg {
	struct mutex mtx;
	struct ucmi_axi_node axi[UCMI_SLAVE_AXI_NUM];
	struct ucmi_win_node win_4m[UCMI_4M_WIN_NUM];
	struct ucmi_win_node win_16m[UCMI_16M_WIN_NUM];
	struct udev_ucmi_info info;

	struct udev_espi *espi;
};

#endif /* end of UDEV_UCMI_ESPI_H */
