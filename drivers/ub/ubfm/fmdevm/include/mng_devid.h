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
#ifndef MNG_DEVID_H
#define MNG_DEVID_H

#include "ubfm_common.h"
#include <linux/seq_file.h>

bool is_slot_id_valid(u32 slot_id);
bool is_socket_id_valid(u32 socket_id);
int mng_get_slot_id_socket_id(u32 devid, u32 *slot_id, u32 *socket_id);
int mng_get_devid(u32 slot_id, u32 socket_id, u32 *devid);
int mng_alloc_devid(u32 slot_id, u32 socket_id, u32 *devid);
void mng_devid_2_socid_info_show(struct seq_file *seq);

#endif  /* end of MNG_DEVID_H */
