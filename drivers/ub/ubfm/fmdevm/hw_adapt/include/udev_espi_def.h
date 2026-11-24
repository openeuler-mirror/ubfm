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
#ifndef _UDEV_ESPI_DEF_H
#define _UDEV_ESPI_DEF_H

#include <linux/types.h>

#define UCMI_4M_WIN_NUM 64
#define UCMI_16M_WIN_NUM 48
#define UCMI_SLAVE_AXI_NUM 16

#define UCMI_4M_WIN_OFFSET(id)	  (0x100 + 0x4 * (id))
#define UCMI_16M_WIN_OFFSET(id)	 (0x200 + 0x4 * (id))
#define UCMI_AXI_WIN_OFFSET(id)	 (0x400 + 0x4 * (id))

#define REMAP_4M_ADDR(id) ((((id) & 0x30) << 24) + (((id) & 0xf) << 22) + 0xc000000)
#define REMAP_16M_ADDR(id) ((((id) / 12) << 28) + (((id) % 12) << 24))

#define REMAP_4M_VAL(axi_id, addr) (((axi_id) << 6) + (((addr) >> 22) & 0x3F))
#define REMAP_16M_VAL(axi_id, addr) (((axi_id) << 4) + (((addr) >> 24) & 0xF))

#define UCMI_ACC_ADDR_4M(id, die_id, off) ((REMAP_4M_ADDR(id)) | ((off) & 0x3FFFFF))
#define UCMI_ACC_ADDR_16M(id, die_id, off) ((REMAP_16M_ADDR(id)) | ((off) & 0xFFFFFF))

#define UCMI_INVALID_AXI_VALUE 0xFFFFFFFF
#define UCMI_AXI_VALUE(asic_addr) (((asic_addr) >> 28) & 0xFFFFF)

#define REMAP_INFO_4M(off, si, id, a_id, d_id)			  \
	.chip_addr = (off),									 \
	.ucmi_off = UCMI_ACC_ADDR_4M((id), (die_id), (off)),	\
	.size = (si),										   \
	.die_id = (d_id),									   \
	.axi_id = (a_id),									   \
	.win_id = (id),										 \
	.win_type = UCMI_WIN_TYPE_4M,						   \
	.win_val = REMAP_4M_VAL((a_id), (off))

#define REMAP_INFO_16M(off, si, id, a_id, d_id)			 \
	.chip_addr = (off),									 \
	.ucmi_off = UCMI_ACC_ADDR_16M((id), (die_id), (off)),   \
	.size = (si),										   \
	.die_id = (d_id),									   \
	.axi_id = (a_id),									   \
	.win_id = (id),										 \
	.win_type = UCMI_WIN_TYPE_16M,						  \
	.win_val = REMAP_16M_VAL((a_id), (off))

#endif
