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
#ifndef UED_PROC_FS_H__
#define UED_PROC_FS_H__

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

void ued_proc_fs_init(void);
void ued_proc_fs_uninit(void);

struct proc_dir_entry *ued_proc_fs_add_dev(u32 devid, u32 inst);
void ued_proc_fs_del_dev(struct proc_dir_entry *dev_entry);

struct proc_dir_entry *ued_proc_fs_add_task(struct proc_dir_entry *dev_entry, u32 devid, int tgid, u32 task_id);
void ued_proc_fs_del_task(struct proc_dir_entry *task_entry);

#endif

