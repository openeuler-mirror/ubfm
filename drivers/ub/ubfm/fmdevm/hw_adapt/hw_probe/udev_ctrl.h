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
#ifndef _UDEV_CTRL_H_
#define _UDEV_CTRL_H_
#include <linux/types.h>
#include "udev_hw_pub.h"

int udev_set_guid_dev(struct udev_hw_identify *hw);
int udev_get_hw_info(u32 devid, u64 resv_mem_base, u32 size, struct udev_hw_identify *hw);
int udev_get_dev_hw_info(u32 devid, enum udev_acc_type type, u64 resv_mem_base, u32 size);
int read_file_content(const char *path_str, u32 seek_len, u8 *content, u32 max_len);

#endif
