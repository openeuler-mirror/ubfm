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
#ifndef LINKCOM_UCD_H_
#define LINKCOM_UCD_H_

int linkcom_ucd_open_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len);
int linkcom_ucd_close_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len);
int linkcom_ucd_send_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len);
int linkcom_ucd_recv_link_pkt(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len);
int linkcom_ucd_bond_eventfd(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len);

#endif
