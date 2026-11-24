// SPDX-License-Identifier: GPL-2.0-only
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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "fpga_adapter_kern.h"
#include "uop_log.h"

#define UCMI_PCIE_MAX_WIN_4M_NUM		128ULL
#define FPGA_4M_WIN_REG_BASE        0x5C000UL
#define FPGA_4M_WIN_REG_OFFSET      0x4

#define FPGA_INT_MSI_STATUS_BASE            0x40
#define FPGA_INT_MSI_MASK_BASE              0x80

static int read_back_val_by_reg(u32 devid, u32 *val)
{
	void __iomem *vaddr = uop_fpga_get_back_vaddr(devid);

	if (unlikely(vaddr == NULL)) {
		uop_err("back vaddr not initialized. (devid=%u)\n", devid);
		return -EIO;
	}

	*val = readl(vaddr);
	return 0;
}

static inline u32 ucmi_pcie_get_fpga_4m_win_reg_offset(u32 ucmi_id, u64 win_4m_id)
{
	return FPGA_4M_WIN_REG_BASE + (ucmi_id * UCMI_PCIE_MAX_WIN_4M_NUM + win_4m_id) * FPGA_4M_WIN_REG_OFFSET;
}

static int cfg_win_4m_by_reg(u32 devid, u32 win_4m_id, u32 value)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	chip_ub_mng chip_mng = {0};
	u64 fpga_reg_base = 0;
	u64 bar_size = 0;
	u64 reg_addr = 0;
	void __iomem *virt_addr = NULL;
	u32 read_back_data = 0;
	u32 ret = 0;

	ret = starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, socket_id, &chip_mng);
	if (ret != 0) {
		uop_err("Get chip_mng failed. (devid=%u, socket_id=%u)\n", devid, socket_id);
		return -EIO;
	}

	ret = starry_drv_pci_get_device_bar(chip_mng.devnum, 0, &fpga_reg_base, &bar_size);
	if (ret != 0) {
		uop_err("Get device bar failed. (devnum=%u)\n", chip_mng.devnum);
		return -EIO;
	}

	reg_addr = fpga_reg_base + ucmi_pcie_get_fpga_4m_win_reg_offset(chip_mng.ucmi_id, win_4m_id);
	virt_addr = ioremap(reg_addr, PAGE_SIZE);
	if (virt_addr == NULL) {
		uop_err("ioremap win 4m reg failed.\n");
		return -EIO;
	}

	writel(value, virt_addr);
	wmb(); /* Ensure the value is written correctly. */
	read_back_data = readl(virt_addr);
	iounmap(virt_addr);
	if (read_back_data != value) {
		uop_err("Win 4m back read check failed. (win_4m_id=%u; read_back=%#x; expect=%#x)\n", win_4m_id, read_back_data, value);
		return -EIO;
	}

	return 0;
}

static void __iomem *msi_get_reg_vaddr(u32 devid, u32 die_id, u64 reg_offset)
{
	void __iomem *vaddr = uop_fpga_get_msi_int_vaddr(devid);

	if (unlikely(vaddr == NULL)) {
		uop_err("msi int vaddr not initialized. (devid=%u)\n", devid);
		return NULL;
	}

	return vaddr + reg_offset + die_id * 4;
}

static int read_irq_status_by_reg(u32 devid, u32 die_id, u32 *status)
{
	void __iomem *vaddr = msi_get_reg_vaddr(devid, die_id, FPGA_INT_MSI_STATUS_BASE);

	if (unlikely(vaddr == NULL)) {
		return -EIO;
	}

	*status = readl(vaddr);
	return 0;
}

static int write_irq_status_by_reg(u32 devid, u32 die_id, u32 status)
{
	void __iomem *vaddr = msi_get_reg_vaddr(devid, die_id, FPGA_INT_MSI_STATUS_BASE);

	if (unlikely(vaddr == NULL)) {
		return -EIO;
	}

	writel(status, vaddr);
	return 0;
}

static int read_irq_mask_by_reg(u32 devid, u32 die_id, u32 *mask)
{
	void __iomem *vaddr = msi_get_reg_vaddr(devid, die_id, FPGA_INT_MSI_MASK_BASE);

	if (unlikely(vaddr == NULL)) {
		return -EIO;
	}

	*mask = readl(vaddr);
	return 0;
}

static int write_irq_mask_by_reg(u32 devid, u32 die_id, u32 mask)
{
	void __iomem *vaddr = msi_get_reg_vaddr(devid, die_id, FPGA_INT_MSI_MASK_BASE);

	if (unlikely(vaddr == NULL)) {
		return -EIO;
	}

	writel(mask, vaddr);
	return 0;
}

const struct fpga_ops g_reg_ops = {
	.read_back_val = read_back_val_by_reg,
	.cfg_win_4m = cfg_win_4m_by_reg,
	.read_irq_status = read_irq_status_by_reg,
	.write_irq_status = write_irq_status_by_reg,
	.read_irq_mask = read_irq_mask_by_reg,
	.write_irq_mask = write_irq_mask_by_reg,
};

const struct fpga_ops *fpga_ops_get_g_reg_ops(void)
{
	return &g_reg_ops;
}
