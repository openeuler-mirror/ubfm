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
#ifndef UDEV_UCMI_PCIE_ESPI_H
#define UDEV_UCMI_PCIE_ESPI_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "udev_pcie_pub.h"
#include "udev_ucmi_ops.h"
#include "pcie_to_ucmi.h"
#include "fpga_adapter_kern.h"


#define BITS_PER_U64 64ULL
#define TEST_BIT_U64(nr, addr) \
	(!!((1ULL << ((nr) % BITS_PER_U64)) & (addr)[(nr) / BITS_PER_U64]))

#define SET_BIT_U64(nr, addr) \
	(((u64 *)(addr))[(nr) / BITS_PER_U64] |= (1ULL << ((nr) % BITS_PER_U64)))


#define UCMI_PCIE_NODE_NAME		 "ucmi"

#define FPGA_4M_WIN_REG_BASE		0x5C000UL
#define FPGA_4M_WIN_REG_OFFSET	  0x4

#define FPGA_CHIP_ACC_WIN_REG_BASE   0x40008UL
#define FPGA_CHIP_ACC_WIN_REG_OFFSET 0x2000UL

#define FPGA_REG_VAL_AXI_SHIFT		  6
#define FPGA_REG_VAL_AXI_MAST		   0xF
#define FPGA_REG_VAL_MIDDLE_ADDR_SHIFT  22
#define FPGA_REG_VAL_MIDDLE_ADDR_MAST   0x3F

#define UCMI_AXI_REG_BASE		   0x400
#define UCMI_AXI_REG_OFFSET		 0x4

/* PCIE-ESPI only supports 4M windows */
#define UCMI_PCIE_MAX_WIN_4M_NUM		128ULL

/* if 4M windows are less than 48, use window sharing */
#define UCMI_PCIE_MIN_WIN_4M_NUM		48ULL

#define UCMI_ACC_ADDR_WIN_SHIFT	 22
#define UCMI_ACC_ADDR_WIN_MASK	  0x7F
#define UCMI_ACC_LOW_ADDR_MASK	  0x3FFFFF

#define UCMI_AXI_ERROR_VAL1 0xFFFFFFFF
#define UCMI_AXI_ERROR_VAL2 0xDEADBEEF

#define UCMI_AXI_RESET_DEFAULT_VAL 0x8F

#define UCMI_PCIE_AXI_VALUE(asic_addr)		  (((asic_addr) >> 28) & 0xFFFFF)
#define DYN_WIN_AXI_ID 13ULL

struct ucmi_pcie_axi_node {
	u64 axi_addr; /* ucmi axi address */
	u64 axi_val; /* value of the ucmi axi register */
	u32 axi_ref;
};

struct ucmi_pcie_win_4m_node {
	char name[UCMI_WIN_NAME_LEN];
	u32 win_4m_id;
	u64 win_4m_fpga_addr; /* 4m window register address on the fpga */
	u32 win_4m_fpga_val; /* 4m window register value on the fpga */
	u32 win_4m_ref;
	u64 win_4m_acc; /* access address of the 4m window mapped on the master side */
	u32 axi_id;
	u64 aligned_addr; /* 4m aligned address */
};

struct ucmi_pcie_info {
	u64 fpga_reg_base; /* fpga register bar space base address obtained from the starry interface */
	u64 ucmi_reg_base; /* base address of the ucmi register table */
	void __iomem *virt_ucmi_addr; /* virtual address after ucmi register mapping */
	u32 ucmi_map_bar_num;
	u64 ucmi_bar_addr[UCMI_MAP_BAR_NUM_MAX]; /* bar space base address obtained from the starry interface */
	u64 ucmi_bar_size[UCMI_MAP_BAR_NUM_MAX]; /* bar space size obtained from the starry interface */
	u32 ucmi_id;
	u32 win_4m_num;
	u32 win_4m_mode; /* 4m window mode, 0: exclusive 1: shared */
};

struct ucmi_pcie_win_cfg {
	u32 devid;
	struct mutex mtx;
	struct ucmi_pcie_info pcie_info;
	struct ucmi_pcie_axi_node axi_node[UCMI_PCIE_AXI_NUM];
	struct ucmi_pcie_win_4m_node win_4m_node[UCMI_PCIE_MAX_WIN_4M_NUM];
	struct udev_ucmi_info info;
	const struct fpga_ops *fpga_ops;
};

union fpga_chip_acc_win_reg {
	struct {
		u32 ucmi_die0_reg_acc_win:4;
		u32 ucmi_die1_reg_acc_win:4;
		u32 ucmi_die0_mngtq_win:4;
		u32 ucmi_die1_mngtq_win:4;
		u32 ucmi_die0_ctrlq_msgq_win:4;
		u32 ucmi_die1_ctrlq_msgq_win:4;
		u32 rsv:8;
	};
	u32 val;
};

static inline u64 ucmi_pcie_get_axi_chip_addr(u64 ucmi_reg_base, u32 axi_id)
{
	return ucmi_reg_base + (UCMI_AXI_REG_BASE + UCMI_AXI_REG_OFFSET * (axi_id));
}

static inline u32 ucmi_pcie_get_fpga_4m_win_reg_offset(u32 ucmi_id, u64 win_4m_id)
{
	return FPGA_4M_WIN_REG_BASE + (ucmi_id * UCMI_PCIE_MAX_WIN_4M_NUM + win_4m_id) * FPGA_4M_WIN_REG_OFFSET;
}

static inline u32 ucmi_pcie_get_fpga_4m_win_reg_val(u32 axi_id, u64 chip_addr)
{
	return ((axi_id & FPGA_REG_VAL_AXI_MAST) << FPGA_REG_VAL_AXI_SHIFT) |
		   ((chip_addr >> FPGA_REG_VAL_MIDDLE_ADDR_SHIFT) & FPGA_REG_VAL_MIDDLE_ADDR_MAST);
}

static inline u32 ucmi_pcie_get_fpga_chip_acc_win_reg_offset(u32 ucmi_id)
{
	return FPGA_CHIP_ACC_WIN_REG_BASE + ucmi_id * FPGA_CHIP_ACC_WIN_REG_OFFSET;
}

int ucmi_pcie_get_dyn_win_4m_start_id(u32 devid, u32 *start_id);
int ucmi_pcie_alloc_axi_by_id(u32 devid, u32 axi_id, u64 chip_addr);
int ucmi_pcie_cfg_axi(u32 devid, u32 axi_id);
int ucmi_pcie_alloc_axi(u32 devid, u64 chip_addr, u32 *axi_id);
void ucmi_pcie_release_axi(u32 devid, u32 axi_id);
int ucmi_pcie_alloc_win_4m_by_id(u32 devid, u32 axi_id, const char *name, u32 win_4m_id, u64 chip_addr, u64 *local_addr);
int ucmi_pcie_cfg_win_4m(u32 devid, u64 *win_4m_bitmap);
void ucmi_pcie_release_win_4m(u32 devid, u64 *win_4m_bitmap);
int ucmi_pcie_get_win_4m_mode(u32 devid, u32 *win_4m_mode);
int ucmi_pcie_get_idle_win_4m(u32 devid, u32 *win_4m_id_start, u32 *win_4m_num, u32 *win_4m_size);
int ucmi_pcie_get_local_addr(u32 devid, u32 win_4m_id, u64 *local_addr);
int ucmi_pcie_get_axi_id_by_win_4m(u32 devid, u32 win_4m_id, u32 *axi_id);
int ucmi_pcie_check_all_win_lost(u32 devid, u32 *win_lost);
int ucmi_pcie_check_axi_win_lost(u32 devid, u32 axi_id, u32 *win_lost);

#endif
