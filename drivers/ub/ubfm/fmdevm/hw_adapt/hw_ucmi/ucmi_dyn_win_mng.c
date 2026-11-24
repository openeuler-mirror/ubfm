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
#include <linux/export.h>
#include "pcie_to_ucmi.h"
#include "udev_ucmi_pcie_espi.h"

static int ucmi_dyn_check_param(u32 devid, u32 dyn_4m_id, u64 aligned_addr, u64 *local_addr)
{
	if (dyn_4m_id >= UCMI_PCIE_DYN_4M_WIN_NUM) {
		hw_err("invalid dyn_4m_id. (devid=%u, dyn_4m_id=%u, max id=%llu)\n", devid, dyn_4m_id, UCMI_PCIE_DYN_4M_WIN_NUM);
		return -EINVAL;
	}
	if ((aligned_addr & UCMI_WIN_SIZE_4M_MASK) != 0) {
		hw_err("invalid addr. (devid=%u)\n", devid);
		return -EINVAL;
	}
	if (local_addr == NULL) {
		hw_err("local_addr is null. (devid=%u)\n", devid);
		return -EINVAL;
	}
	return 0;
}

int ucmi_cfg_dyn_win(u32 devid, u32 dyn_4m_id, u64 aligned_addr, u64 *local_addr)
{
	char name[UCMI_WIN_NAME_LEN];
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};
	u32 start_id = 0;
	int ret = 0;

	if (ucmi_dyn_check_param(devid, dyn_4m_id, aligned_addr, local_addr) != 0) {
		return -EINVAL;
	}
	if (UBFM_SPRINTF(name, sizeof(name), "dyn_win_%u", dyn_4m_id) <= 0) {
		return -EINVAL;
	}
	ret = ucmi_pcie_alloc_axi_by_id(devid, DYN_WIN_AXI_ID, aligned_addr);
	if (ret != 0) {
		hw_err("alloc dyn win_256m failed. (devid=%u, ret=%d)\n", devid, ret);
		return ret;
	}
	ret = ucmi_pcie_get_dyn_win_4m_start_id(devid, &start_id);
	if (ret != 0) {
		hw_err("get dyn start_id failed. (devid=%u, ret=%d)\n", devid, ret);
		goto end1;
	}
	ret = ucmi_pcie_alloc_win_4m_by_id(devid, DYN_WIN_AXI_ID, name, start_id + dyn_4m_id, aligned_addr, local_addr);
	if (ret != 0) {
		hw_err("alloc dyn win_4m failed. (devid=%u, ret=%d, win_4m_id=%u)\n", devid, ret, start_id + dyn_4m_id);
		goto end1;
	}
	SET_BIT_U64(start_id + dyn_4m_id, win_4m_bitmap);
	ret = ucmi_pcie_cfg_axi(devid, DYN_WIN_AXI_ID);
	if (ret != 0) {
		hw_err("cfg dyn win_256m failed. (devid=%u, ret=%d)\n", devid, ret);
		goto end0;
	}
	ret = ucmi_pcie_cfg_win_4m(devid, win_4m_bitmap);
	if (ret != 0) {
		hw_err("cfg dyn win_4m failed. (devid=%u, ret=%d)\n", devid, ret);
		goto end0;
	}
	return 0;
end0:
	ucmi_pcie_release_win_4m(devid, win_4m_bitmap);
end1:
	ucmi_pcie_release_axi(devid, DYN_WIN_AXI_ID);
	return ret;
}
EXPORT_SYMBOL_GPL(ucmi_cfg_dyn_win);

void ucmi_clear_dyn_win(u32 devid, u32 dyn_4m_id)
{
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};
	u32 start_id = 0;
	int ret = 0;

	if (dyn_4m_id >= UCMI_PCIE_DYN_4M_WIN_NUM) {
		hw_err("invalid para. (devid=%u, dyn_4m_id=%u, max_id=%llu)\n", devid, dyn_4m_id, UCMI_PCIE_DYN_4M_WIN_NUM - 1);
		return;
	}
	ret = ucmi_pcie_get_dyn_win_4m_start_id(devid, &start_id);
	if (ret != 0) {
		hw_err("get dyn start 4m id failed. (devid=%u, ret=%d)\n", devid, ret);
		return;
	}
	SET_BIT_U64(start_id + dyn_4m_id, win_4m_bitmap);
	ucmi_pcie_release_win_4m(devid, win_4m_bitmap);
	ucmi_pcie_release_axi(devid, DYN_WIN_AXI_ID);
}
EXPORT_SYMBOL_GPL(ucmi_clear_dyn_win);

int ucmi_cfg_win(u32 devid, u32 win_4m_id, u64 aligned_addr, u32 *axi_id_out)
{
	char name[UCMI_WIN_NAME_LEN];
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};
	int ret = 0;
	u32 axi_id = 0;
	u64 local_addr = 0;

	if (axi_id_out != NULL) {
		*axi_id_out = 0;
	}
	if (UBFM_SPRINTF(name, sizeof(name), "share_win_%u", win_4m_id) <= 0) {
		return -EINVAL;
	}
	if (win_4m_id >= UCMI_PCIE_MAX_WIN_4M_NUM) {
		return -EINVAL;
	}
	ret = ucmi_pcie_alloc_axi(devid, aligned_addr, &axi_id);
	if (ret != 0) {
		hw_err("Alloc axi failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ucmi_pcie_alloc_win_4m_by_id(devid, axi_id, name, win_4m_id, aligned_addr, &local_addr);
	if (ret != 0) {
		hw_err("Alloc win 4m failed. (devid=%u; win_4m_id=%u; ret=%d)\n", devid, win_4m_id, ret);
		goto end1;
	}
	SET_BIT_U64(win_4m_id, win_4m_bitmap);
	ret = ucmi_pcie_cfg_axi(devid, axi_id);
	if (ret != 0) {
		hw_err("Cfg axi failed. (devid=%u; ret=%d)\n", devid, ret);
		goto end0;
	}
	ret = ucmi_pcie_cfg_win_4m(devid, win_4m_bitmap);
	if (ret != 0) {
		hw_err("Cfg win 4m failed. (devid=%u; ret=%d)\n", devid, ret);
		goto end0;
	}
	if (axi_id_out != NULL) {
		*axi_id_out = axi_id;
	}
	return 0;
end0:
	ucmi_pcie_release_win_4m(devid, win_4m_bitmap);
end1:
	ucmi_pcie_release_axi(devid, axi_id);
	return ret;
}
EXPORT_SYMBOL_GPL(ucmi_cfg_win);

void ucmi_clear_win(u32 devid, u32 win_4m_id)
{
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};
	u32 axi_id = 0;
	int ret;

	ret = ucmi_pcie_get_axi_id_by_win_4m(devid, win_4m_id, &axi_id);
	if (ret != 0) {
		return;
	}

	SET_BIT_U64(win_4m_id, win_4m_bitmap);
	ucmi_pcie_release_win_4m(devid, win_4m_bitmap);
	ucmi_pcie_release_axi(devid, axi_id);
}
EXPORT_SYMBOL_GPL(ucmi_clear_win);

int ucmi_get_idle_local_win(u32 devid, u32 *win_4m_id_start, u32 *win_4m_num, u32 *win_4m_size)
{
	if (win_4m_id_start == NULL || win_4m_num == NULL || win_4m_size == NULL) {
		return -EINVAL;
	}
	return ucmi_pcie_get_idle_win_4m(devid, win_4m_id_start, win_4m_num, win_4m_size);
}
EXPORT_SYMBOL_GPL(ucmi_get_idle_local_win);

int ucmi_get_local_addr(u32 devid, u32 win_4m_id, u64 *local_addr)
{
	if (local_addr == NULL) {
		return -EINVAL;
	}
	return ucmi_pcie_get_local_addr(devid, win_4m_id, local_addr);
}
EXPORT_SYMBOL_GPL(ucmi_get_local_addr);
