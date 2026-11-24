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
#include <linux/delay.h>
#include "mami_string.h"
#include "udm.h"
#include "ubfabric.h"
#include "uop_log.h"
#include "uops_kern.h"
#include "fmdevm.h"
#include "uop.h"
#include "fmdevm.h"

STATIC int pcie_kern_i3c_copy_guid(u32 devid, u8 *guid)
{
	struct udm_dev_para para = {0};
	int ret, i;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		uop_err("udm_get_dev_attr fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	for (i = 0; i < MAMI_GUID_LEN; i++) {
		guid[i] = para.guid[i];
	}

	return 0;
}

static int pcie_i3c_readl_kern(u32 devid, u64 addr, u32 *val, void *user_ctx)
{
	int ret;
	drv_ubnic_xfer_msg readl_msg = {0};
	i3c_reg_rd_req rd_req = {0};
	i3c_reg_rd_rsp rd_rsp = {0};

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	UNUSED(user_ctx);
	ret = pcie_kern_i3c_copy_guid(devid, rd_req.guid);
	if (ret != 0) {
		uop_err("pcie_kern_i3c_copy_guid fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	rd_req.read_addr_h = I3C_ADDR_HI(addr);
	rd_req.read_addr_l = I3C_ADDR_LW(addr);
	readl_msg.op = PCIE_I3C_REG_RD;
	readl_msg.write_len = sizeof(i3c_reg_rd_req);
	readl_msg.read_len = sizeof(i3c_reg_rd_rsp);
	readl_msg.write_buf = (void *)&rd_req;
	readl_msg.read_buf = (void *)&rd_rsp;
	ret = uop_i3c_send_recv_kern(devid, &readl_msg);
	if (ret != 0) {
		uop_err("read reg fail. (ret=%d; devid=%u; addr=0x%llx)\n", ret, devid, addr);
		return ret;
	}
	if (rd_rsp.status != 0) {
		uop_err("read reg fail. (ret=%d; devid=%u; addr=0x%llx; status=%u)\n", ret, devid, addr, rd_rsp.status);
		return -EBUSY;
	}
	*val = rd_rsp.read_data;
	return 0;
}

STATIC int pcie_i3c_writel_kern(u32 devid, u64 addr, u32 val, void *user_ctx)
{
	int ret;
	drv_ubnic_xfer_msg writel_msg = {0};
	i3c_reg_wr_req wr_req = {0};
	i3c_reg_wr_rsp wr_rsp = {0};

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	UNUSED(user_ctx);
	ret = pcie_kern_i3c_copy_guid(devid, wr_req.guid);
	if (ret != 0) {
		uop_err("pcie_kern_i3c_copy_guid fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	wr_req.write_addr_h = I3C_ADDR_HI(addr);
	wr_req.write_addr_l = I3C_ADDR_LW(addr);
	wr_req.write_data = val;
	writel_msg.op = PCIE_I3C_REG_WR;
	writel_msg.write_len = sizeof(i3c_reg_wr_req);
	writel_msg.read_len = sizeof(i3c_reg_wr_rsp);
	writel_msg.write_buf = (void *)&wr_req;
	writel_msg.read_buf = (void *)&wr_rsp;
	ret = uop_i3c_send_recv_kern(devid, &writel_msg);
	if (ret != 0) {
		uop_err("i3c write reg fail. (ret=%d; devid=%u; addr=0x%llx)\n", ret, devid, addr);
		return ret;
	}
	if (wr_rsp.status != 0) {
		uop_err("i3c write reg fail. (ret=%d; devid=%u; addr=0x%llx; status=%u)\n", ret, devid, addr, wr_rsp.status);
		return -EBUSY;
	}
	return 0;
}

STATIC int pcie_i3c_readq_kern(u32 devid, u64 addr, u64 *val, void *user_ctx)
{
	u64 src = addr;
	u32 *dst = (u32 *)(void *)val;
	int ret;

	ret = pcie_i3c_readl_kern(devid, src, dst, user_ctx);
	if (unlikely(ret != 0)) {
		return ret;
	}
	dst++;
	src += UOP_U32_BYTE_NUM;
	ret = pcie_i3c_readl_kern(devid, src, dst, user_ctx);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return 0;
}

static int pcie_i3c_writeq_kern(u32 devid, u64 addr, u64 val, void *user_ctx)
{
	u64 dst = addr;
	u32 *src = (u32 *)(void *)&val;
	int ret;

	ret = pcie_i3c_writel_kern(devid, dst, *src, user_ctx);
	if (unlikely(ret != 0)) {
		return ret;
	}
	src++;
	dst += UOP_U32_BYTE_NUM;

	ret = pcie_i3c_writel_kern(devid, dst, *src, user_ctx);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return 0;
}

static const struct uop_ops g_i3c_ops = {
	.readl = pcie_i3c_readl_kern,
	.writel = pcie_i3c_writel_kern,
	.readq = pcie_i3c_readq_kern,
	.writeq = pcie_i3c_writeq_kern,
};

int uops_pcie_i3c_init(void)
{
	return uops_kern_register(UOP_ACC_PCIE_I3C, &g_i3c_ops);
}

void uops_pcie_i3c_uninit(void)
{
	uops_kern_unregister(UOP_ACC_PCIE_I3C);
}
