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
#ifndef CTRLQ_CHIP_H
#define CTRLQ_CHIP_H

#include <linux/types.h>
#include "mctrlq.h"

#define CTRLQ_REG_BASE						  0x00018800
#define CTRLQ_REG_CHAN_OFF					  0x20000

#define CTRLQ_REG_RX_BASE_L			(CTRLQ_REG_BASE)
#define CTRLQ_REG_RX_BASE_H			(0x4 + CTRLQ_REG_BASE)
#define CTRLQ_REG_RX_DEPTH			 (0x8 + CTRLQ_REG_BASE)
#define CTRLQ_REG_RX_TAIL			  (0x10 + CTRLQ_REG_BASE)
#define CTRLQ_REG_RX_HEAD			  (0x14 + CTRLQ_REG_BASE)

#define CTRLQ_REG_TX_BASE_L			(0x18 + CTRLQ_REG_BASE)
#define CTRLQ_REG_TX_BASE_H			(0x1C + CTRLQ_REG_BASE)
#define CTRLQ_REG_TX_DEPTH			 (0x20 + CTRLQ_REG_BASE)
#define CTRLQ_REG_TX_TAIL			  (0x24 + CTRLQ_REG_BASE)
#define CTRLQ_REG_TX_HEAD			  (0x28 + CTRLQ_REG_BASE)

#define CTRLQ_REG_INT_BASE					  0x00018000
#define CTRLQ_REG_INT_GEN			  (0x10 + CTRLQ_REG_INT_BASE)

#define IMP_CTRLQ_TX_INT_MASK		   0x30200

int ctrlq_kern_get_ue_id(u32 chan_id, enum mami_udie_version udie_version, u32 chip_type, u32 *ue_id);

/* Get the address of the specified type of the CtrlQ s-channel mapped to the bar space */
int mami_ctrlq_schan_get_addr(u32 devid, u32 die_id, enum mami_kern_schan_addr_type type, u64 *addr);
const ctrlq_ue_map_t *ctrlq_kern_get_chip_ue_map(enum mami_udie_version udie_version, u32 chip_type, u32 *map_size);

#endif
