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
#ifndef UDIE_V2_LINKCOM_SOC_H_
#define UDIE_V2_LINKCOM_SOC_H_

#include "linkcom.h"
#include "linkcom_pub.h"
#include "linkcom_soc_adapt.h"

int udie_v2_inband_read_rpkt(struct linkcom_id_inst *id_inst, struct linkcom_list_head *head, u32 *pkt_num);

#endif
