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

#include "uop_log.h"
#include "fpga_adapter_kern.h"

#define FPGA_CTRLQ_INT_SHIFT		0
#define FPGA_CTRLQ_INT_MASK			0x1FFFF
#define FPGA_MSGQ_INT_SHIFT			17
#define FPGA_MSGQ_INT_MASK			0x1
#define FPGA_ABNORMAL_INT_SHIFT		18
#define FPGA_ABNORMAL_INT_MASK		0x1
#define FPGA_RAS_INT_SHIFT			19
#define FPGA_RAS_INT_MASK			0x1

static int read_back_val_by_api(u32 devid, u32 *val)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	union fpga_back_reg back_val = {0};
	rx_proc proc = {0};
	u32 ret = 0;

	ret = starry_drv_fpga_ucmi_get_rx_proc(DRV_INST_ID_SELF, socket_id, &proc);
	if (ret != 0) {
		uop_err("Get rx_proc failed. (devid=%u, socket_id=%u)\n", devid, socket_id);
		return -EIO;
	}

	back_val.ovfl_flag = proc.ind_fifo_ovwl_flag;
	back_val.fifo_cnt = proc.ind_fifo_debug_cnt;
	*val = back_val.val;
	return 0;
}

static int cfg_win_4m_by_api(u32 devid, u32 win_4m_id, u32 value)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	u32 ret = 0;

	ret = starry_drv_ucmi_4m_win_config(DRV_INST_ID_SELF, socket_id, win_4m_id, value);
	if (ret != 0) {
		uop_err("Cfg win 4m failed. (devid=%u, socket_id=%u, win_4m_id=%u)\n", devid, socket_id, win_4m_id);
		return -EIO;
	}
	return 0;
}

static u32 fpga_int_reg_to_u32(const fpga_int_reg *reg)
{
	u32 val = 0;

	val = reg->ctrlq_int_low32 & FPGA_CTRLQ_INT_MASK;
	val |= (reg->int_unions.int_bit.msgq_int & FPGA_MSGQ_INT_MASK) << FPGA_MSGQ_INT_SHIFT;
	val |= (reg->int_unions.int_bit.abnormal_int & FPGA_ABNORMAL_INT_MASK) << FPGA_ABNORMAL_INT_SHIFT;
	val |= (reg->int_unions.int_bit.ras_int & FPGA_RAS_INT_MASK) << FPGA_RAS_INT_SHIFT;
	return val;
}

static void u32_to_fpga_int_reg(u32 val, fpga_int_reg *reg)
{
	reg->ctrlq_int_low32 = val & FPGA_CTRLQ_INT_MASK;
	reg->int_unions.int_bit.msgq_int = (val >> FPGA_MSGQ_INT_SHIFT) & FPGA_MSGQ_INT_MASK;
	reg->int_unions.int_bit.abnormal_int = (val >> FPGA_ABNORMAL_INT_SHIFT) & FPGA_ABNORMAL_INT_MASK;
	reg->int_unions.int_bit.ras_int = (val >> FPGA_RAS_INT_SHIFT) & FPGA_RAS_INT_MASK;
}

static int read_irq_status_by_api(u32 devid, u32 die_id, u32 *status)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	fpga_int_reg reg = {0};
	u32 ret = 0;

	ret = starry_drv_fpga_ucmi_int_status_read(DRV_INST_ID_SELF, socket_id, die_id, &reg);
	if (ret != 0) {
		uop_err("Read int status failed. (devid=%u, die_id=%u)\n", devid, die_id);
		return -EIO;
	}

	*status = fpga_int_reg_to_u32(&reg);
	return 0;
}

static int write_irq_status_by_api(u32 devid, u32 die_id, u32 status)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	fpga_int_reg reg = {0};
	u32 ret = 0;

	u32_to_fpga_int_reg(status, &reg);
	ret = starry_drv_fpga_ucmi_int_status_write(DRV_INST_ID_SELF, socket_id, die_id, reg);
	if (ret != 0) {
		uop_err("Write int status failed. (devid=%u, die_id=%u)\n", devid, die_id);
		return -EIO;
	}
	return 0;
}

static int read_irq_mask_by_api(u32 devid, u32 die_id, u32 *mask)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	fpga_int_reg reg = {0};
	u32 ret = 0;

	ret = starry_drv_fpga_ucmi_int_mask_read(DRV_INST_ID_SELF, socket_id, die_id, &reg);
	if (ret != 0) {
		uop_err("Read int mask failed. (devid=%u, die_id=%u)\n", devid, die_id);
		return -EIO;
	}

	*mask = fpga_int_reg_to_u32(&reg);
	return 0;
}

static int write_irq_mask_by_api(u32 devid, u32 die_id, u32 mask)
{
	u32 socket_id = uop_fpga_get_socket_id(devid);
	fpga_int_reg reg = {0};
	u32 ret = 0;

	u32_to_fpga_int_reg(mask, &reg);
	ret = starry_drv_fpga_ucmi_int_mask_write(DRV_INST_ID_SELF, socket_id, die_id, reg);
	if (ret != 0) {
		uop_err("Write int mask failed. (devid=%u, die_id=%u)\n", devid, die_id);
		return -EIO;
	}
	return 0;
}

const struct fpga_ops g_api_ops = {
	.read_back_val = read_back_val_by_api,
	.cfg_win_4m = cfg_win_4m_by_api,
	.read_irq_status = read_irq_status_by_api,
	.write_irq_status = write_irq_status_by_api,
	.read_irq_mask = read_irq_mask_by_api,
	.write_irq_mask = write_irq_mask_by_api,
};

const struct fpga_ops *fpga_ops_get_g_api_ops(void)
{
	return &g_api_ops;
}
