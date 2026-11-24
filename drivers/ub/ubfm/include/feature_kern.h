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
#ifndef FEATURE_KERN_H
#define FEATURE_KERN_H

#include "ubfm_common.h"

bool feature_kern_is_support_ras_report(void);
bool feature_kern_is_dpu(u32 devid);
bool feature_kern_is_ndie(u32 devid);
bool feature_kern_is_ssd(u32 devid);
bool feature_is_support_usi_va(u32 devid);
bool feature_is_support_schan(u32 devid);
bool feature_kern_ucmi_module_is_ascend(u32 devid);
bool feature_kern_ucmi_module_is_kunpeng(u32 devid);
int feature_kern_get_udie_version(u32 devid, enum mami_udie_version *udie_version);
int feature_kern_get_rx_tgtid_table_spec(u32 devid, enum ndie_p2p_port_mode p2p_port_mode, u32 *entry_num);
int feature_kern_get_tx_tgtid_table_spec(u32 devid, u32 *entry_num);
int feature_kern_get_port_mapping(u32 devid, u32 *port_mapping, u32 *port_num);

#endif
