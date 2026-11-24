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
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include "mami_string.h"
#include "uop_log.h"
#include "uop.h"
#include "udm.h"
#include "ures.h"
#include "feature_loader.h"
#include "fpga_adapter_kern.h"

#define PCIE_FPGA_DELAY() udelay(1)

struct fpga_back_info {
	u64 phy_back_addr;
	u64 total_fifo_cnt;
	u32 fifo_ovfl_cnt;
	u32 ucmi_unavailable_cnt;
	bool ucmi_available;
	const struct fpga_ops *fpga_ops;
};

#ifndef EMU_ST
	static bool g_kern_back_enable = true;
#else
	static bool g_kern_back_enable;
#endif

static struct fpga_back_info *g_fpga_back_info[DEV_MAX_NUM] = {0};
static struct rw_semaphore g_fpga_back_lock[DEV_MAX_NUM] = {0};
static u32 g_devid_socket_id[DEV_MAX_NUM] = {0};
static void __iomem *g_devid_back_vaddr[DEV_MAX_NUM] = {0};
static void __iomem *g_devid_msi_int_vaddr[DEV_MAX_NUM] = {0};

enum starry_api_func {
	STARRY_GET_CHIP_NUM,
	STARRY_GET_SLOT_ID,
	STARRY_GET_UCMI_BSP_STATE,
	STARRY_GET_DEVICE_BAR,
	STARRY_GET_DEVICE_BAR_64BIT,
	STARRY_GET_CHIP_MNG,
	STARRY_REGISTER_INT,
	STARRY_UNREGISTER_INT,
	STARRY_READ_INT_STATUS,
	STARRY_WRITE_INT_STATUS,
	STARRY_READ_INT_MASK,
	STARRY_WRITE_INT_MASK,
	STARRY_CONFIG_4M_WIN,
	STARRY_GET_RX_PROC,
	STARRY_WRITE_UCMI_GLOBAL_CFG,
	STARRY_WRITE_CHIP_ACC_WIN_DOORBELL,
	STARRY_WRITE_CHIP_SQE_WIN,
	STARRY_WRITE_CFG_DONE,
	STARRY_API_MAX
};

struct starry_api_entry {
	const char *name;
	void *func;
	bool must_support; /* "false" indicates that the FPGA registration operation is used when the Starry interface is not supported. */
};

typedef u32 (*starry_drv_board_get_ucmi_chip_num_fn)(u32 inst_id, u32 *num);
typedef u32 (*starry_drv_board_get_tor_slotid_fn)(u32 *slot_id);
typedef u32 (*starry_drv_get_ucmi_bsp_state_fn)(u32 inst_id, u32 chip_id, u32 *value);
typedef u32 (*starry_drv_pci_get_device_bar_fn)(u32 dev_num, u32 uiBarNum, u64 *base, u32 *size);
typedef u32 (*starry_drv_pci_get_device_bar_64bit_fn)(u32 dev_num, u32 uiBarNum, u64 *base, u64 *size);
typedef u32 (*starry_drv_board_get_chip_mng_fn)(u32 inst_id, u32 chip_id, chip_ub_mng *chip_mng);
typedef u32 (*starry_drv_intr_set_regist_fn)(u32 index_id, u32 type, drv_interrupt_callback_fn fn, void *p);
typedef u32 (*starry_drv_intr_set_unregist_fn)(u32 index_id, u32 type);

/* Added starry interface. */
typedef u32 (*starry_drv_fpga_ucmi_int_status_read_fn)(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *status);
typedef u32 (*starry_drv_fpga_ucmi_int_status_write_fn)(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg status);
typedef u32 (*starry_drv_fpga_ucmi_int_mask_read_fn)(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *mask);
typedef u32 (*starry_drv_fpga_ucmi_int_mask_write_fn)(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg mask);
typedef u32 (*starry_drv_ucmi_4m_win_config_fn)(u32 inst_id, u32 chip_id, u32 win_id, u32 win_point);
typedef u32 (*starry_drv_fpga_ucmi_get_rx_proc_fn)(u32 inst_id, u32 chip_id, rx_proc *proc);
typedef u32 (*starry_drv_fpga_ucmi_write_ucmi_global_cfg_fn)(u32 inst_id, u32 chip_id, ucmi_global_cfg_s global_cfg);
typedef u32 (*starry_drv_fpga_ucmi_write_chip_acc_win_doorbell_fn)(u32 inst_id, u32 chip_id, u32 die_id, u32 ucmi_db_win);
typedef u32 (*starry_drv_fpga_ucmi_write_chip_acc_win_sqe_fn)(u32 inst_id, u32 chip_id, u32 sqe_win);
typedef u32 (*starry_drv_fpga_ucmi_write_cfg_done_flag_fn)(u32 inst_id, u32 chip_id, u32 cfg_done);

static struct starry_api_entry g_starry_api_table[STARRY_API_MAX] = {
	[STARRY_GET_CHIP_NUM] = { "drv_board_get_ucmi_chip_num", NULL, true },
	[STARRY_GET_SLOT_ID] = { "drv_board_get_tor_slotid", NULL, true },
	[STARRY_GET_UCMI_BSP_STATE] = { "drv_get_ucmi_bsp_state", NULL, true },
	[STARRY_GET_DEVICE_BAR] = { "drv_pci_get_device_bar", NULL, true }, /* used in 950 */
	[STARRY_GET_DEVICE_BAR_64BIT] = { "drv_pci_get_device_bar_64bit", NULL, false }, /* used in 960 */
	[STARRY_GET_CHIP_MNG] = { "drv_board_get_chip_mng", NULL, true },
	[STARRY_REGISTER_INT] = { "drv_intr_set_regist", NULL, true },
	[STARRY_UNREGISTER_INT] = { "drv_intr_set_unregist", NULL, true },
	[STARRY_READ_INT_STATUS] = { "drv_fpga_ucmi_int_status_read", NULL, false },
	[STARRY_WRITE_INT_STATUS] = { "drv_fpga_ucmi_int_status_write", NULL, false },
	[STARRY_READ_INT_MASK] = { "drv_fpga_ucmi_int_mask_read", NULL, false },
	[STARRY_WRITE_INT_MASK] = { "drv_fpga_ucmi_int_mask_write", NULL, false },
	[STARRY_CONFIG_4M_WIN] = { "drv_ucmi_4m_win_config", NULL, false },
	[STARRY_GET_RX_PROC] = { "drv_fpga_ucmi_get_rx_proc", NULL, false },
	[STARRY_WRITE_UCMI_GLOBAL_CFG] = { "drv_fpga_ucmi_write_ucmi_global_cfg", NULL, false },
	[STARRY_WRITE_CHIP_ACC_WIN_DOORBELL] = { "drv_fpga_ucmi_write_chip_acc_win_doorbell", NULL, false },
	[STARRY_WRITE_CHIP_SQE_WIN] = { "drv_fpga_ucmi_write_chip_acc_win_sqe", NULL, false },
	[STARRY_WRITE_CFG_DONE] = { "drv_fpga_ucmi_write_cfg_done_flag", NULL, false },
};

u32 starry_drv_board_get_ucmi_chip_num(u32 inst_id, u32 *num)
{
	if (!g_starry_api_table[STARRY_GET_CHIP_NUM].func) {
		return -EPERM;
	}

	return ((starry_drv_board_get_ucmi_chip_num_fn)g_starry_api_table[STARRY_GET_CHIP_NUM].func)(inst_id, num);
}
EXPORT_SYMBOL_GPL(starry_drv_board_get_ucmi_chip_num);

u32 starry_drv_board_get_tor_slotid(u32 *slot_id)
{
	if (!g_starry_api_table[STARRY_GET_SLOT_ID].func) {
		return -EPERM;
	}

	return ((starry_drv_board_get_tor_slotid_fn)g_starry_api_table[STARRY_GET_SLOT_ID].func)(slot_id);
}
EXPORT_SYMBOL_GPL(starry_drv_board_get_tor_slotid);

u32 starry_drv_get_ucmi_bsp_state(u32 inst_id, u32 chip_id, u32 *value)
{
	if (!g_starry_api_table[STARRY_GET_UCMI_BSP_STATE].func) {
		return -EPERM;
	}

	return ((starry_drv_get_ucmi_bsp_state_fn)g_starry_api_table[STARRY_GET_UCMI_BSP_STATE].func)(inst_id, chip_id, value);
}
EXPORT_SYMBOL_GPL(starry_drv_get_ucmi_bsp_state);

u32 starry_drv_pci_get_device_bar(u32 dev_num, u32 uiBarNum, u64 *base, u64 *size)
{
	u32 size32 = 0;
	u32 ret = 0;

	if (!g_starry_api_table[STARRY_GET_DEVICE_BAR].func && !g_starry_api_table[STARRY_GET_DEVICE_BAR_64BIT].func) {
		return -EPERM;
	}

	if (g_starry_api_table[STARRY_GET_DEVICE_BAR_64BIT].func) {
		return ((starry_drv_pci_get_device_bar_64bit_fn)g_starry_api_table[STARRY_GET_DEVICE_BAR_64BIT].func)(dev_num, uiBarNum, base, size);
	}

	ret = ((starry_drv_pci_get_device_bar_fn)g_starry_api_table[STARRY_GET_DEVICE_BAR].func)(dev_num, uiBarNum, base, &size32);
	*size = size32;
	return ret;
}
EXPORT_SYMBOL_GPL(starry_drv_pci_get_device_bar);

u32 starry_drv_board_get_chip_mng(u32 inst_id, u32 chip_id, chip_ub_mng *chip_mng)
{
	if (!g_starry_api_table[STARRY_GET_CHIP_MNG].func) {
		return -EPERM;
	}

	return ((starry_drv_board_get_chip_mng_fn)g_starry_api_table[STARRY_GET_CHIP_MNG].func)(inst_id, chip_id, chip_mng);
}
EXPORT_SYMBOL_GPL(starry_drv_board_get_chip_mng);

u32 starry_drv_intr_set_regist(u32 index_id, u32 type, drv_interrupt_callback_fn fn, void *p)
{
	if (!g_starry_api_table[STARRY_REGISTER_INT].func) {
		return -EPERM;
	}

	return ((starry_drv_intr_set_regist_fn)g_starry_api_table[STARRY_REGISTER_INT].func)(index_id, type, fn, p);
}
EXPORT_SYMBOL_GPL(starry_drv_intr_set_regist);

u32 starry_drv_intr_set_unregist(u32 index_id, u32 type)
{
	if (!g_starry_api_table[STARRY_UNREGISTER_INT].func) {
		return -EPERM;
	}

	return ((starry_drv_intr_set_unregist_fn)g_starry_api_table[STARRY_UNREGISTER_INT].func)(index_id, type);
}
EXPORT_SYMBOL_GPL(starry_drv_intr_set_unregist);

u32 starry_drv_fpga_ucmi_int_status_read(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *status)
{
	if (!g_starry_api_table[STARRY_READ_INT_STATUS].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_int_status_read_fn)g_starry_api_table[STARRY_READ_INT_STATUS].func)(inst_id, chip_id, die_id, status);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_int_status_read);

u32 starry_drv_fpga_ucmi_int_status_write(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg status)
{
	if (!g_starry_api_table[STARRY_WRITE_INT_STATUS].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_int_status_write_fn)g_starry_api_table[STARRY_WRITE_INT_STATUS].func)(inst_id, chip_id, die_id, status);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_int_status_write);

u32 starry_drv_fpga_ucmi_int_mask_read(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *mask)
{
	if (!g_starry_api_table[STARRY_READ_INT_MASK].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_int_mask_read_fn)g_starry_api_table[STARRY_READ_INT_MASK].func)(inst_id, chip_id, die_id, mask);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_int_mask_read);

u32 starry_drv_fpga_ucmi_int_mask_write(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg mask)
{
	if (!g_starry_api_table[STARRY_WRITE_INT_MASK].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_int_mask_write_fn)g_starry_api_table[STARRY_WRITE_INT_MASK].func)(inst_id, chip_id, die_id, mask);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_int_mask_write);

u32 starry_drv_ucmi_4m_win_config(u32 inst_id, u32 chip_id, u32 win_id, u32 win_point)
{
	if (!g_starry_api_table[STARRY_CONFIG_4M_WIN].func) {
		return -EPERM;
	}

	return ((starry_drv_ucmi_4m_win_config_fn)g_starry_api_table[STARRY_CONFIG_4M_WIN].func)(inst_id, chip_id, win_id, win_point);
}
EXPORT_SYMBOL_GPL(starry_drv_ucmi_4m_win_config);

u32 starry_drv_fpga_ucmi_get_rx_proc(u32 inst_id, u32 chip_id, rx_proc *proc)
{
	if (!g_starry_api_table[STARRY_GET_RX_PROC].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_get_rx_proc_fn)g_starry_api_table[STARRY_GET_RX_PROC].func)(inst_id, chip_id, proc);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_get_rx_proc);

/* Determine whether to use the newly added interfaces or the register read/write method. */
static bool use_starry_api(void)
{
	int i;

	for (i = 0; i < STARRY_API_MAX; i++) {
		if (i == STARRY_GET_DEVICE_BAR_64BIT) {
			continue;
		}

		if (!g_starry_api_table[i].func) {
			uop_warn("Failed to get starry symbol: %s.\n", g_starry_api_table[i].name);
			return false;
		}
	}

	return true;
}

const struct fpga_ops *fpga_ops_get(void)
{
	if (use_starry_api()) {
		return fpga_ops_get_by_type(FPGA_API_OPS_TYPE);
	}
	return fpga_ops_get_by_type(FPGA_REG_OPS_TYPE);
}
EXPORT_SYMBOL_GPL(fpga_ops_get);

u32 starry_drv_fpga_ucmi_write_ucmi_global_cfg(u32 inst_id, u32 chip_id, ucmi_global_cfg_s global_cfg)
{
	if (!g_starry_api_table[STARRY_WRITE_UCMI_GLOBAL_CFG].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_write_ucmi_global_cfg_fn)g_starry_api_table[STARRY_WRITE_UCMI_GLOBAL_CFG].func)(inst_id, chip_id, global_cfg);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_write_ucmi_global_cfg);

u32 starry_drv_fpga_ucmi_write_chip_acc_win_doorbell(u32 inst_id, u32 chip_id, u32 die_id, u32 ucmi_db_win)
{
	if (!g_starry_api_table[STARRY_WRITE_CHIP_ACC_WIN_DOORBELL].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_write_chip_acc_win_doorbell_fn)g_starry_api_table[STARRY_WRITE_CHIP_ACC_WIN_DOORBELL].func)(inst_id, chip_id, die_id, ucmi_db_win);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_write_chip_acc_win_doorbell);

u32 starry_drv_fpga_ucmi_write_chip_acc_win_sqe(u32 inst_id, u32 chip_id, u32 sqe_win)
{
	if (!g_starry_api_table[STARRY_WRITE_CHIP_SQE_WIN].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_write_chip_acc_win_sqe_fn)g_starry_api_table[STARRY_WRITE_CHIP_SQE_WIN].func)(inst_id, chip_id, sqe_win);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_write_chip_acc_win_sqe);

u32 starry_drv_fpga_ucmi_write_cfg_done_flag(u32 inst_id, u32 chip_id, u32 cfg_done)
{
	if (!g_starry_api_table[STARRY_WRITE_CFG_DONE].func) {
		return -EPERM;
	}

	return ((starry_drv_fpga_ucmi_write_cfg_done_flag_fn)g_starry_api_table[STARRY_WRITE_CFG_DONE].func)(inst_id, chip_id, cfg_done);
}
EXPORT_SYMBOL_GPL(starry_drv_fpga_ucmi_write_cfg_done_flag);

int starry_api_init(void)
{
	int i;

	for (i = 0; i < STARRY_API_MAX; i++) {
		if (!g_starry_api_table[i].name) {
			continue;
		}

		g_starry_api_table[i].func = __symbol_get(g_starry_api_table[i].name);
		if ((g_starry_api_table[i].func == NULL) && g_starry_api_table[i].must_support) {
			uop_warn("Failed to get starry symbol: %s.\n", g_starry_api_table[i].name);
			starry_api_uninit();
			return 0;
		}
	}

	uop_info("Starry api init successfully.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(starry_api_init, FEATURE_LOADER_STAGE_0);

void starry_api_uninit(void)
{
	int i;

	for (i = 0; i < STARRY_API_MAX; i++) {
		if (g_starry_api_table[i].func) {
			g_starry_api_table[i].func = NULL;
			__symbol_put(g_starry_api_table[i].name);
		}
	}
}
DECLARE_FEATURE_AUTO_UNINIT(starry_api_uninit, FEATURE_LOADER_STAGE_0);

int uop_fpga_info_init_kern(u32 devid, u32 socket_id, u64 back_reg_addr, u64 msi_int_base)
{
	void __iomem *back_vaddr = NULL;
	void __iomem *msi_vaddr = NULL;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	if (unlikely(g_fpga_back_info[devid] != NULL)) {
		return 0;
	}

	g_fpga_back_info[devid] = kzalloc(sizeof(struct fpga_back_info), GFP_KERNEL | __GFP_ACCOUNT);
	if (unlikely(g_fpga_back_info[devid] == NULL)) {
		return -ENOMEM;
	}

	back_vaddr = ioremap(back_reg_addr, PAGE_SIZE);
	if (unlikely(back_vaddr == NULL)) {
		uop_err("ioremap back reg failed. (devid=%u)\n", devid);
		kfree(g_fpga_back_info[devid]);
		g_fpga_back_info[devid] = NULL;
		return -EIO;
	}

	msi_vaddr = ioremap(msi_int_base, PAGE_SIZE);
	if (unlikely(msi_vaddr == NULL)) {
		uop_err("ioremap msi int reg failed. (devid=%u)\n", devid);
		iounmap(back_vaddr);
		kfree(g_fpga_back_info[devid]);
		g_fpga_back_info[devid] = NULL;
		return -EIO;
	}

	g_fpga_back_info[devid]->fpga_ops = fpga_ops_get();
	g_fpga_back_info[devid]->phy_back_addr = back_reg_addr;
	init_rwsem(&g_fpga_back_lock[devid]);
	g_fpga_back_info[devid]->ucmi_available = 1;

	WRITE_ONCE(g_devid_socket_id[devid], socket_id);
	WRITE_ONCE(g_devid_back_vaddr[devid], back_vaddr);
	WRITE_ONCE(g_devid_msi_int_vaddr[devid], msi_vaddr);

	return 0;
}
EXPORT_SYMBOL_GPL(uop_fpga_info_init_kern);

u32 uop_fpga_get_socket_id(u32 devid)
{
	if (unlikely(devid >= DEV_MAX_NUM)) {
		return 0;
	}
	return READ_ONCE(g_devid_socket_id[devid]);
}
EXPORT_SYMBOL_GPL(uop_fpga_get_socket_id);

void __iomem *uop_fpga_get_back_vaddr(u32 devid)
{
	if (unlikely(devid >= DEV_MAX_NUM)) {
		return NULL;
	}
	return READ_ONCE(g_devid_back_vaddr[devid]);
}
EXPORT_SYMBOL_GPL(uop_fpga_get_back_vaddr);

void __iomem *uop_fpga_get_msi_int_vaddr(u32 devid)
{
	if (unlikely(devid >= DEV_MAX_NUM)) {
		return NULL;
	}
	return READ_ONCE(g_devid_msi_int_vaddr[devid]);
}
EXPORT_SYMBOL_GPL(uop_fpga_get_msi_int_vaddr);

static void uop_fpga_back_info_uninit_kern(u32 devid)
{
	void __iomem *back_vaddr = NULL;
	void __iomem *msi_vaddr = NULL;

	if (g_fpga_back_info[devid] == NULL) {
		return;
	}

	g_fpga_back_info[devid]->fpga_ops = NULL;
	kfree(g_fpga_back_info[devid]);
	g_fpga_back_info[devid] = NULL;

	WRITE_ONCE(g_devid_socket_id[devid], 0);

	back_vaddr = READ_ONCE(g_devid_back_vaddr[devid]);
	WRITE_ONCE(g_devid_back_vaddr[devid], NULL);
	if (back_vaddr != NULL) {
		iounmap(back_vaddr);
	}

	msi_vaddr = READ_ONCE(g_devid_msi_int_vaddr[devid]);
	WRITE_ONCE(g_devid_msi_int_vaddr[devid], NULL);
	if (msi_vaddr != NULL) {
		iounmap(msi_vaddr);
	}
}

void uop_fpga_info_uninit_all_kern(void)
{
	u32 devid = 0;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		uop_fpga_back_info_uninit_kern(devid);
	}
}
EXPORT_SYMBOL_GPL(uop_fpga_info_uninit_all_kern);

int uop_fpga_get_back_paddr(u32 devid, u64 *phy_addr)
{
	if (unlikely(devid >= DEV_MAX_NUM || phy_addr == NULL)) {
		return -EINVAL;
	}
	down_read(&g_fpga_back_lock[devid]);
	if (unlikely(g_fpga_back_info[devid] == NULL)) {
		up_read(&g_fpga_back_lock[devid]);
		return -EINVAL;
	}
	*phy_addr = g_fpga_back_info[devid]->phy_back_addr;
	up_read(&g_fpga_back_lock[devid]);
	return 0;
}

STATIC int uop_kern_fpga_back_check_inner(u32 devid)
{
	struct fpga_back_info *back_info = g_fpga_back_info[devid];
	union fpga_back_reg back_val = {0};
	int count = 400;

	do {
#ifndef EMU_ST
		if (back_info->fpga_ops->read_back_val(devid, &back_val.val) != 0) {
			break;
		}
#else
		back_val.val = 0;
#endif
		back_info->total_fifo_cnt += back_val.fifo_cnt;
		if (back_val.ovfl_flag == 0) {
			return 0;
		}
#ifndef EMU_ST
		if (back_val.val == PCIE_BAR_ERROR_VAL || back_info->ucmi_available == 0) {
			back_info->ucmi_unavailable_cnt++;
			break;
		}
		back_info->fifo_ovfl_cnt++;
		udelay(1); /* sleep 1us each time */
		count--;
#endif
	} while (count > 0);
	return -EIO;
}

STATIC int uop_kern_fpga_back_check(u32 devid)
{
	int ret = 0;

	if (!g_kern_back_enable) {
		PCIE_FPGA_DELAY();
		return 0;
	}
	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	if (unlikely(g_fpga_back_info[devid] == NULL || g_fpga_back_info[devid]->fpga_ops == NULL)) {
		return -EINVAL;
	}
	ret = uop_kern_fpga_back_check_inner(devid);
	if (unlikely(ret != 0)) {
		uop_err_limit("ucmi back check failed. (devid:%u, ret:%d)\n", devid, ret);
	}
	return ret;
}

int ldst_back_readl_kern(u32 devid, u64 addr, u32 *data)
{
	int ret = 0;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	down_write(&g_fpga_back_lock[devid]);
	ret = uop_kern_fpga_back_check(devid);
	if (unlikely(ret != 0)) {
		up_write(&g_fpga_back_lock[devid]);
		return ret;
	}
	up_write(&g_fpga_back_lock[devid]);
#ifndef EMU_ST
	*data = readl((const volatile void __iomem *)(void *)(uintptr_t)addr);
#else
	*data = 0;
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_readl_kern);

int ldst_back_writel_kern(u32 devid, u64 addr, u32 data)
{
	int ret = 0;

	if (unlikely(devid >= DEV_MAX_NUM)) {
		return -EINVAL;
	}
	down_write(&g_fpga_back_lock[devid]);
	ret = uop_kern_fpga_back_check(devid);
	if (unlikely(ret != 0)) {
		up_write(&g_fpga_back_lock[devid]);
		return ret;
	}
	up_write(&g_fpga_back_lock[devid]);
#ifndef EMU_ST
	writel(data, (volatile void __iomem *)(void *)(uintptr_t)addr);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_writel_kern);

int ldst_back_readq_kern(u32 devid, u64 addr, u64 *val)
{
	u64 src = addr;
	u32 *dst = (u32 *)(void *)val;
	int ret;

	ret = ldst_back_readl_kern(devid, src, dst);
	if (unlikely(ret != 0)) {
		return ret;
	}
	dst++;
	src += UOP_U32_BYTE_NUM;
	ret = ldst_back_readl_kern(devid, src, dst);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_readq_kern);

int ldst_back_writeq_kern(u32 devid, u64 addr, u64 val)
{
	u64 dst = addr;
	u32 *src = (u32 *)(void *)&val;
	int ret;

	ret = ldst_back_writel_kern(devid, dst, *src);
	if (unlikely(ret != 0)) {
		return ret;
	}
	src++;
	dst += UOP_U32_BYTE_NUM;

	ret = ldst_back_writel_kern(devid, dst, *src);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_writeq_kern);

int ldst_back_mem_read_kern(u32 devid, u64 addr, void *data, u32 len)
{
	u64 src = addr;
	void *dst = data;
	u32 cnt = len, data_read_tmp = 0;
	int ret;

	while (cnt >= UOP_U32_BYTE_NUM) {
		ret = ldst_back_readl_kern(devid, src, (u32 *)dst);
		if (unlikely(ret != 0)) {
			return ret;
		}
		src += UOP_U32_BYTE_NUM;
		dst += UOP_U32_BYTE_NUM;
		cnt -= UOP_U32_BYTE_NUM;
	}

	if (cnt != 0) {
		ret = ldst_back_readl_kern(devid, src, &data_read_tmp);
		if (unlikely(ret != 0)) {
			return ret;
		}
		while (cnt--) {
			*(u8 *)dst = data_read_tmp & 0xff;
			dst++;
			data_read_tmp >>= BITS_PER_BYTE;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_mem_read_kern);

int ldst_back_mem_write_kern(u32 devid, u64 addr, void *data, u32 len)
{
	u64 dst = addr;
	void *src = data;
	u32 cnt = len, data_read_tmp = 0, data_write_tmp = 0, data_read_mask, data_write_mask;
	int ret;

	while (cnt >= UOP_U32_BYTE_NUM) {
		ret = ldst_back_writel_kern(devid, dst, *(u32 *)src);
		if (unlikely(ret != 0)) {
			return ret;
		}
		dst += UOP_U32_BYTE_NUM;
		src += UOP_U32_BYTE_NUM;
		cnt -= UOP_U32_BYTE_NUM;
	}

	if (cnt != 0) {
		ret = ldst_back_readl_kern(devid, dst, &data_read_tmp);
		if (unlikely(ret != 0)) {
			return ret;
		}
		data_write_mask = (1 << (cnt * BITS_PER_BYTE)) - 1;
		data_read_mask = ~data_write_mask;
		data_write_tmp = (data_read_tmp & data_read_mask) | (*(u32 *)src & data_write_mask);
		ret = ldst_back_writel_kern(devid, dst, data_write_tmp);
		if (ret != 0) {
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ldst_back_mem_write_kern);

void uop_kern_back_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv)
{
	down_read(&g_fpga_back_lock[devid]);
	if (likely(g_fpga_back_info[devid] == NULL)) {
		up_read(&g_fpga_back_lock[devid]);
		return;
	}
	show(priv, "back pressure info: devid[%u] total_fifo_cnt[%llu] fifo_ovfl_cnt[%u] ucmi_unavailable_cnt[%u]\n",
		devid, g_fpga_back_info[devid]->total_fifo_cnt, g_fpga_back_info[devid]->fifo_ovfl_cnt, g_fpga_back_info[devid]->ucmi_unavailable_cnt);
	up_read(&g_fpga_back_lock[devid]);
}

void uop_fpga_back_enable(bool enable)
{
	g_kern_back_enable = enable;
}

bool uop_get_fpga_back_state(void)
{
	return g_kern_back_enable;
}

u32 drv_int_cb(u32 index_id, u32 type, void *p)
{
	u32 devid = (u32)(uintptr_t)p;

	uop_info("drv int cb handle. (devid=%u, index_id=%u, type=%u)\n", devid, index_id, type);
	if (devid < DEV_MAX_NUM) {
		down_write(&g_fpga_back_lock[devid]);
		if (g_fpga_back_info[devid] != NULL) {
			g_fpga_back_info[devid]->ucmi_available = 0;
		}
		up_write(&g_fpga_back_lock[devid]);
	}
	return 0;
}

int ucmi_starry_init(u32 devid)
{
	struct udev_hw_identify hw_attr = {0};
	chip_ub_mng mng = {0};
	u32 index_id = 0;
	int ret = 0;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw_attr, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		uop_err("Get hw dev attr failed. (ret=%d, devid=%u)\n", ret, devid);
		return ret;
	}
	if (hw_attr.chip_type != MAMI_KERN_TYPE_ASCEND_V1 && hw_attr.chip_type != MAMI_KERN_TYPE_KUNPENG950_2P) {
		return 0;
	}
	ret = (int)starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, hw_attr.socket_id, &mng);
	if (ret != 0) {
		uop_err("Get chip mng failed. (ret=%d, devid=%u, socket_id=%u)\n", ret, devid, hw_attr.socket_id);
		return ret;
	}
	index_id = mng.udie_irq_index_id;

	ret = (int)starry_drv_intr_set_regist(index_id, DRV_INTR_TYPE_IODIE_POWER_DOWN, drv_int_cb, (void *)(uintptr_t)devid);
	uop_info("Register power down intr. (devid=%u, index_id=%u, ret=%d)\n", devid, index_id, ret);

	ret = (int)starry_drv_intr_set_regist(index_id, DRV_INTR_TYPE_IODIE_RESET, drv_int_cb, (void *)(uintptr_t)devid);
	uop_info("Register reset intr. (devid=%u, index_id=%u, ret=%d)\n", devid, index_id, ret);
	return 0;
}

void ucmi_starry_uninit(u32 devid)
{
	struct udev_hw_identify hw_attr = {0};
	chip_ub_mng mng = {0};
	u32 index_id = 0;
	int ret = 0;

	if (devid < DEV_MAX_NUM) {
		down_write(&g_fpga_back_lock[devid]);
		if (g_fpga_back_info[devid] != NULL) {
			g_fpga_back_info[devid]->ucmi_available = 1;
		}
		up_write(&g_fpga_back_lock[devid]);
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw_attr, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		uop_err("Get hw dev attr failed. (ret=%d, devid=%u)\n", ret, devid);
		return;
	}
	if (hw_attr.chip_type != MAMI_KERN_TYPE_ASCEND_V1 && hw_attr.chip_type != MAMI_KERN_TYPE_KUNPENG950_2P) {
		return;
	}
	ret = (int)starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, hw_attr.socket_id, &mng);
	if (ret != 0) {
		uop_err("Get chip mng failed. (ret=%d, devid=%u, socket_id=%u)\n", ret, devid, hw_attr.socket_id);
		return;
	}
	index_id = mng.udie_irq_index_id;
	ret = (int)starry_drv_intr_set_unregist(index_id, DRV_INTR_TYPE_IODIE_RESET);
	uop_info("Unregister reset intr. (devid=%u, index_id=%u, ret=%d)\n", devid, index_id, ret);

	ret = (int)starry_drv_intr_set_unregist(index_id, DRV_INTR_TYPE_IODIE_POWER_DOWN);
	uop_info("Unregister power down intr. (devid=%u, index_id=%u, ret=%d)\n", devid, index_id, ret);
}
