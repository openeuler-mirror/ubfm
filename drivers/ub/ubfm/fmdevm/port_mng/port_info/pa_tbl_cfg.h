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
#ifndef _PA_TBL_CFG_H_
#define _PA_TBL_CFG_H_

#include "fmdevm.h"

u64 port_info_get_nl_base_addr(u32 devid, u32 die_id, u32 nl_id);
int get_ndie_p2p_port_mode(u32 devid, u32 die_id, u32 nl_id, enum ndie_p2p_port_mode *p2p_port_mode);

int pa_table_read(struct nl_id_inst *inst, enum route_table_cfg_type type, u32 entry, u8 *data, u32 size);

#endif
