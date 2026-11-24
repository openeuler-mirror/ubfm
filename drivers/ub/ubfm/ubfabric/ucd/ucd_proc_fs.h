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
#ifndef UCD_PROC_FS_H__
#define UCD_PROC_FS_H__

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define UCD_PROC_TOP_NAME	"mami/ucd"
#define UCD_PROC_REGISTER_DATA  "register"
#define UCD_PROC_STATISTIC_DATA "statistic"

void ucd_proc_fs_init(void);
void ucd_proc_fs_uninit(void);

#endif  /* __UCD_PORC_FS_H__ */
