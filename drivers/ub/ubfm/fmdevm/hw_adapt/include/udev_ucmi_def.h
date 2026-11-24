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
#ifndef UDEV_UCMI_DEF_H
#define UDEV_UCMI_DEF_H

#define UCMI_WIN_SIZE_4M			0x400000ULL
#define UCMI_WIN_SIZE_4M_MASK	   0x3FFFFFULL
#define UCMI_WIN_SIZE_16M		   0x1000000ULL
#define UCMI_WIN_SIZE_16M_MASK	  0xFFFFFFULL

#define UCMI_WIN_NAME_LEN		   64

#define UCMI_WIN_NAME			   "ucmi"
#define DEV_INFO_WIN_NAME		   "dev_info"

enum win_type_e {
	UCMI_WIN_TYPE_4M = 0,
	UCMI_WIN_TYPE_16M,
	UCMI_WIN_TYPE_MAX
};

#endif  /* end of UDEV_UCMI_DEF_H */
