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
#ifndef RAS_LOCAL_H
#define RAS_LOCAL_H

#include "ras_core.h"

#define RAS_DATA_LOCAL_VALID_SOC_ID BIT(0)
#define RAS_DATA_LOCAL_VALID_SOCKET_ID BIT(1)
#define RAS_DATA_LOCAL_VALID_TOTEM_ID BIT(2)
#define RAS_DATA_LOCAL_VALID_NIMBUS_ID BIT(3)
#define RAS_DATA_LOCAL_VALID_SUB_SYSTEM_ID BIT(4)
#define RAS_DATA_LOCAL_VALID_MODULE_ID BIT(5)
#define RAS_DATA_LOCAL_VALID_SUB_MODULE_ID BIT(6)
#define RAS_DATA_LOCAL_VALID_CORE_ID BIT(7)
#define RAS_DATA_LOCAL_VALID_PORT_ID BIT(8)
#define RAS_DATA_LOCAL_VALID_ERR_TYPE BIT(9)
#define RAS_DATA_LOCAL_VALID_ERR_INFO BIT(10)
#define RAS_DATA_LOCAL_VALID_ERR_SEVERITY BIT(11)
#define RAS_DATA_LOCAL_VALID_REG_SIZE BIT(12)

typedef union {
	struct {
		u64 start_bit : 8;
		u64 reg_idx : 8;
		u64 sub_module_id : 8;
		u64 module_id : 8;
		u64 resv : 32;
	} bits;
	u64 val;
} local_ras_detailcode;

int ras_local_process(u32 devid, struct ras_data_entry_item *item, u32 need_report);

#endif  /* End of RAS_LOCAL_H */
