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
#ifndef USI_UB_DEF_H
#define USI_UB_DEF_H

#define UB_VENDOR_ID 0xcc08

enum usi_ub_guid_device_id {
	DPU_CTRLED_NODE_CTRL_UE_DEVICE_ID = 0x8202,
	SSD_CTRLED_NODE_CTRL_UE_DEVICE_ID = 0x8014,
	SSD_CTRL_NODE_CTRL_UE_DEVICE_ID = 0x8016,
	CONTROLLER_DEVICE_ID = 0xA000,
	IDEV_URMA_MUE_DEVICE_ID = 0xA001,
	IDEV_URMA_VUE_DEVICE_ID = 0xA002,
	IDEV_CDMA_MUE_DEVICE_ID = 0xA003,
	IDEV_CDMA_VUE_DEVICE_ID = 0xA004,
	IDEV_PMU_MUE_DEVICE_ID = 0xA005,
	IDEV_PMU_VUE_DEVICE_ID = 0xA006,
	CTRL_NODE_CTRL_UE_DEVICE_ID = 0xA007,
	CTRLED_NODE_CTRL_UE_DEVICE_ID = 0xA009,
	KUNPENG_960_CTRL_NODE_CTRL_UE_DEVICE_ID = 0xA011,
	KUNPENG_960_CTRLED_NODE_CTRL_UE_DEVICE_ID = 0xA013,
	ASCEND_V1_CTRLED_NODE_CTRL_UE_DEVICE_ID = 0xD80E,
	ASCEND_V1_CTRL_NODE_CTRL_UE_DEVICE_ID = 0xD900,
};

#endif /* End of USI_UB_DEF_H */
