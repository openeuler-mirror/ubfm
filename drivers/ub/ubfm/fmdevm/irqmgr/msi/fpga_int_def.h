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
#ifndef FPGA_INT_DEF_H
#define FPGA_INT_DEF_H

#define FPGA_INT_REG_OFFSET			 0x51000
#define FPGA_INT_REG_SIZE			   0x1000

#define FPGA_INT_MSI_STATUS(vec_idx)		(0x40 + (vec_idx) * 4)
#define FPGA_INT_MSI_MASK(vec_idx)		  (0x80 + (vec_idx) * 4)

#define FPGA_CTRLQ_INT_SHIFT			0
#define FPGA_CTRLQ_INT_MASK			 0x1FFFF
#define FPGA_MSGQ_INT_SHIFT			 17
#define FPGA_MSGQ_INT_MASK			  0x1
#define FPGA_ABNORMAL_INT_SHIFT		 18
#define FPGA_ABNORMAL_INT_MASK		  0x1
#define FPGA_RAS_INT_SHIFT			  19
#define FPGA_RAS_INT_MASK			   0x1

#endif
