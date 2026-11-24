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
#ifndef UED_CORE_H__
#define UED_CORE_H__

#include <linux/seq_file.h>

#include "ulog.h"
#include "ubfabric.h"

#define module_ued "ued"

#define ued_err(fmt, ...)	  mami_err(module_ued, fmt, ##__VA_ARGS__)
#define ued_warn(fmt, ...)	  mami_warn(module_ued, fmt, ##__VA_ARGS__)
#define ued_info(fmt, ...)	  mami_info(module_ued, fmt, ##__VA_ARGS__)
#define ued_debug(fmt, ...)	  mami_debug(module_ued, fmt, ##__VA_ARGS__)

int ued_core_init(void);
void ued_core_uninit(void);

int ued_core_add_dev(u32 devid);
int ued_core_del_dev(u32 devid);

int ued_core_add_task(u32 devid, int pid);
int ued_core_del_task(u32 devid, int pid);

int ued_core_create_grp(u32 devid, int pid, char *grp_name, u32 threadNum, u32 *gid);
int ued_core_destroy_grp(u32 devid, int pid, u32 gid);

int ued_core_subscribe_event(u32 devid, int pid, u32 gid, u64 event_bitmap);
int ued_core_wait_event(u32 devid, int pid, struct ued_wait_event_para *para);
int ued_core_publish_event(u32 udevid, struct ued_publish_event_para *para);

void ued_show_subscriber(u32 devid, struct seq_file *seq);
void ued_show_task(u32 devid, int pid, struct seq_file *seq);

#endif

