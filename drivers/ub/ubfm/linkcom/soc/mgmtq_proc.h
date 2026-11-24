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
#ifndef MGMTQ_PROC_H
#define MGMTQ_PROC_H

#include "ubfm_common.h"

int mgmtq_proc_init(void);
void mgmtq_proc_uninit(void);
int mgmtq_proc_open(struct inode *inode, struct file *file);
int mgmtq_outband_enable_open(struct inode *inode, struct file *file);
ssize_t mgmtq_outband_enable_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *ppos);

#endif
