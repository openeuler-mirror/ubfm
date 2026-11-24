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
#ifndef UCD_MODULE_H
#define UCD_MODULE_H

#define UCD_MODULE_NAME_MAX			  256

int ucd_call_open(pid_t owner_pid);
int ucd_call_release(pid_t owner_pid);
int ucd_call_dev_open(u32 devid, pid_t owner_pid);
int ucd_call_dev_release(u32 devid, pid_t owner_pid);
int ucd_call_dev_mmap(u32 devid, struct vm_area_struct *vma);
int ucd_module_list_init(void);
int ucd_module_list_uninit(void);
#endif

