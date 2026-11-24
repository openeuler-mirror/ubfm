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
#ifndef CTRLQ_DEBUG_H
#define CTRLQ_DEBUG_H

#ifdef CFG_BUILD_DEBUG
#include "ubfm_common.h"
#include "mctrlq.h"

int ctrlq_kern_enque_cq(u32 devid, u32 chan_id, struct ctrlq_msg *msg);
#endif

#endif
