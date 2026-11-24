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
#ifndef RAS_CORE_H
#define RAS_CORE_H

#include "ures.h"
#include "uid.h"
#include "ras_def.h"

int ras_get_data_header(u32 devid, struct ures_addr_info *ras_hdr_addr, struct ras_data_header *data_hdr);
/* caller needs to free the memory */
int ras_data_entry_load(u32 devid, struct ures_addr_info *ras_hdr_addr, struct ras_data_header *data_hdr);
void ras_intr_handle(u32 devid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info);
int ras_data_parser_and_report(u32 devid, u32 need_report);

int ras_irq_init(u32 devid);
int ras_irq_uninit(u32 devid);

u64 ras_local_get_now(void);
int ras_get_chipid_and_dieid(u32 devid, struct ras_data_entry_item *item, u32 *chip_id, u32 *dieid);
int ras_get_logicid_by_port_id(u32 devid, u32 chip_id, u32 die_id, u32 port_id, struct mami_logical_id_info *logic_id);
int ras_get_logicid_by_body_id(u32 devid, u32 chip_id, u32 die_id, u32 body_id, struct mami_logical_id_info *logic_id);
int ras_get_logicid_by_plane_id(u32 devid, u32 chip_id, u32 die_id, struct mami_logical_id_info *logic_id, struct mami_fault_event *event);
#endif
