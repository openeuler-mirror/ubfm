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
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include "mami_string.h"

#include "array_ctx.h"

#include "ubfm_common.h"
#include "feature_loader.h"
#include "udev_ucmi_ops.h"
#include "ubfm_dfx_fs.h"
#include "udev_ucmi_pcie_espi.h"
#include "fmdevm.h"
#include "udm.h"
#include "uop.h"
#include "ures.h"
#include "udev_hw_info.h"
#include "fpga_adapter_kern.h"

#define MODULE_UCMI "hw-ucmi"

static struct array_ctx_domain *ucmi_pcie_dev_domain;
static struct uop_win_ops g_ucmi_win_ops = {
	.get_idle_local_win = ucmi_get_idle_local_win,
	.get_local_addr = ucmi_get_local_addr,
	.cfg_win = ucmi_cfg_win,
	.clear_win = ucmi_clear_win,
};

static struct udev_ucmi_info *ucmi_pcie_info_get(u32 devid)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u)\n", devid);
		return NULL;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	return &d_ctx->info;
}

void ucmi_pcie_info_put(u32 devid)
{
	struct array_ctx *ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx != NULL) {
		array_ctx_put(ctx); /* decrement reference count by two */
		array_ctx_put(ctx);
	}
}

static struct udev_ucmi_addr_info *ucmi_pcie_find_addr_info(struct ucmi_pcie_win_cfg *cfg, struct udev_ucmi_addr *addr)
{
	struct udev_ucmi_info *info = &cfg->info;
	struct udev_ucmi_addr_info *addr_info = NULL;

	mutex_lock(&info->mtx);
	list_for_each_entry(addr_info, &info->addr_list, entry) {
		if (strcmp(addr->name, addr_info->name) == 0 && addr->dieid == addr_info->dieid) {
			mutex_unlock(&info->mtx);
			return addr_info;
		}
	}
	mutex_unlock(&info->mtx);
	return NULL;
}

static int ucmi_pcie_master_setup(void)
{
	return 0;
}

static int ucmi_pcie_slave_setup(void)
{
	return 0;
}

/* the 4M space is not necessarily contiguous. So the start address is calculated from the 4M space address info and the 4M window ID */
static int ucmi_pcie_get_acc_addr(struct ucmi_pcie_info *pcie_info, u32 in_win_4m_id, u64 chip_addr, u64 *acc_addr)
{
	int ret = -EINVAL;
	u64 acc_base = 0;
	u64 acc_offset = 0;
	u32 i = 0, tmp_id = 0,  win_4m_id = in_win_4m_id;

	for (i = 0; i < pcie_info->ucmi_map_bar_num; i++) {
		tmp_id = pcie_info->ucmi_bar_size[i] / UCMI_WIN_SIZE_4M;
		if (win_4m_id < tmp_id) {
			acc_base = pcie_info->ucmi_bar_addr[i];
			ret = 0;
			break;
		}
		win_4m_id -= tmp_id;
	}
	acc_offset = ((win_4m_id & UCMI_ACC_ADDR_WIN_MASK) << UCMI_ACC_ADDR_WIN_SHIFT) | (chip_addr & UCMI_ACC_LOW_ADDR_MASK);
	*acc_addr = acc_base + acc_offset;
	return ret;
}

int ucmi_pcie_get_local_addr(u32 devid, u32 win_4m_id, u64 *local_addr)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -EINVAL;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_get_acc_addr(&d_ctx->pcie_info, win_4m_id, 0, local_addr);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);

	return ret;
}

int ucmi_pcie_get_dyn_win_4m_start_id(u32 devid, u32 *start_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -EINVAL;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	*start_id = d_ctx->pcie_info.win_4m_num - UCMI_PCIE_DYN_4M_WIN_NUM;
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return 0;
}

void ucmi_pcie_release_axi_inner(struct ucmi_pcie_win_cfg *cfg, u32 axi_id)
{
	struct ucmi_pcie_axi_node *node = &cfg->axi_node[axi_id];

	if (node->axi_ref > 0) {
		node->axi_ref--;
	}
	hw_debug("release axi. (devid=%u, axi_id=%u, axi_ref=%u)\n", cfg->devid, axi_id, node->axi_ref);
}

void ucmi_pcie_release_axi(u32 devid, u32 axi_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ucmi_pcie_release_axi_inner(d_ctx, axi_id);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
}

static int ucmi_pcie_alloc_axi_inner_dyn(struct ucmi_pcie_win_cfg *cfg, u64 chip_addr, u32 *axi_id)
{
	struct ucmi_pcie_axi_node *node = NULL;
	u64 axi_val = UCMI_PCIE_AXI_VALUE(chip_addr);

	node = &cfg->axi_node[DYN_WIN_AXI_ID];
	if (node->axi_ref != 0 && node->axi_val != axi_val) {
		hw_err("DYN axi conflict. (devid=%u, axi_id=%llu, axi_ref=%u)\n",
			cfg->devid, DYN_WIN_AXI_ID, node->axi_ref);
		return -EEXIST;
	}
	node->axi_addr = ucmi_pcie_get_axi_chip_addr(cfg->pcie_info.ucmi_reg_base, DYN_WIN_AXI_ID);
	node->axi_val = axi_val;
	node->axi_ref++;
	*axi_id = DYN_WIN_AXI_ID;
	return 0;
}

static int ucmi_pcie_alloc_axi_inner(struct ucmi_pcie_win_cfg *cfg, u64 chip_addr, u32 *axi_id)
{
	struct ucmi_pcie_axi_node *node = NULL;
	u64 axi_val = UCMI_PCIE_AXI_VALUE(chip_addr);
	u32 i = 0;

	/* if there is the same high-bit address, reuse the axi window. axi0 cannot be reused */
	for (i = 0x1; i < UCMI_PCIE_AXI_NUM; i++) {
		if (i == DYN_WIN_AXI_ID) {
			continue;
		}
		node = &cfg->axi_node[i];
		if (node->axi_ref > 0 && node->axi_val == axi_val) {
			node->axi_ref++;
			*axi_id = i;
			hw_debug("reuse axi. (devid=%u, axi_id=%u, axi_ref=%u)\n", cfg->devid, i, node->axi_ref);
			return 0;
		}
	}
	/* otherwise allocate an unused axi window. axi0 cannot be reused */
	for (i = 0x1; i < UCMI_PCIE_AXI_NUM; i++) {
		if (i == DYN_WIN_AXI_ID) {
			continue;
		}
		node = &cfg->axi_node[i];
		if (node->axi_ref == 0) {
			node->axi_addr = ucmi_pcie_get_axi_chip_addr(cfg->pcie_info.ucmi_reg_base, i);
			node->axi_val = axi_val;
			node->axi_ref++;
			*axi_id = i;
			hw_debug("alloc axi. (devid=%u, axi_id=%u, axi_ref=%u)\n", cfg->devid, i, node->axi_ref);
			return 0;
		}
	}
	/* use dynamic window 13 when both reuse and new alloc fail */
	return ucmi_pcie_alloc_axi_inner_dyn(cfg, chip_addr, axi_id);
}

int ucmi_pcie_alloc_axi(u32 devid, u64 chip_addr, u32 *axi_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_alloc_axi_inner(d_ctx, chip_addr, axi_id);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

int ucmi_pcie_alloc_axi_by_id(u32 devid, u32 axi_id, u64 chip_addr)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct ucmi_pcie_axi_node *node = NULL;
	u64 axi_val = UCMI_PCIE_AXI_VALUE(chip_addr);

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	node = &d_ctx->axi_node[axi_id];
	if (node->axi_ref != 0 && node->axi_val != axi_val) {
		mutex_unlock(&d_ctx->mtx);
		array_ctx_put(ctx);
		return -EEXIST;
	}
	node->axi_addr = ucmi_pcie_get_axi_chip_addr(d_ctx->pcie_info.ucmi_reg_base, axi_id);
	node->axi_val = axi_val;
	node->axi_ref++;
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	hw_debug("alloc axi. (devid=%u, axi_id=%u, axi_ref=%u)\n", devid, axi_id, node->axi_ref);
	return 0;
}

static void ucmi_pcie_release_one_win_4m(struct ucmi_pcie_win_cfg *cfg, u32 win_4m_id)
{
	struct ucmi_pcie_win_4m_node *node = &cfg->win_4m_node[win_4m_id];

	if (node->win_4m_ref > 0) {
		node->win_4m_ref--;
	}
	hw_debug("release win_4m. (devid=%u, win_4m_id=%u, win_4m_ref=%u)\n", cfg->devid, win_4m_id, node->win_4m_ref);
}

static void ucmi_pcie_release_win_4m_inner(struct ucmi_pcie_win_cfg *cfg, u64 *win_4m_bitmap)
{
	u32 max_win_num = cfg->pcie_info.win_4m_num;
	u32 i = 0;

	for (i = 0; i < max_win_num; i++) {
		if (TEST_BIT_U64(i, win_4m_bitmap) != 0) {
			ucmi_pcie_release_one_win_4m(cfg, i);
		}
	}
}

void ucmi_pcie_release_win_4m(u32 devid, u64 *win_4m_bitmap)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ucmi_pcie_release_win_4m_inner(d_ctx, win_4m_bitmap);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
}

static int ucmi_pcie_alloc_one_win_4m(struct ucmi_pcie_win_cfg *cfg, u64 aligned_addr,
									  u32 axi_id, const char *name, u32 *win_4m_id)
{
	struct ucmi_pcie_win_4m_node *node = NULL;
	u32 static_win_num = cfg->pcie_info.win_4m_num - UCMI_PCIE_DYN_4M_WIN_NUM;
	u32 win_4m_fpga_val = ucmi_pcie_get_fpga_4m_win_reg_val(axi_id, aligned_addr);
	u64 win_4m_acc = 0;
	int ret = 0;
	u32 i = 0;

	/* reuse a static window */
	for (i = 0; i < static_win_num; i++) {
		node = &cfg->win_4m_node[i];
		if (node->win_4m_ref > 0 && node->win_4m_fpga_val == win_4m_fpga_val) {
			node->win_4m_ref++;
			*win_4m_id = i;
			hw_debug("reuse win_4m. (devid=%u, win_4m_id=%u, win_4m_ref=%u)\n", cfg->devid, i, node->win_4m_ref);
			return 0;
		}
	}
	/* allocate a new static window */
	for (i = 0; i < static_win_num; i++) {
		node = &cfg->win_4m_node[i];
		if (node->win_4m_ref == 0) {
			if (ucmi_pcie_get_acc_addr(&cfg->pcie_info, i, aligned_addr, &win_4m_acc) != 0) {
				return -EINVAL;
			}
			ret = UBFM_SPRINTF(node->name, sizeof(node->name), "%s", name);
			if (ret <= 0) {
				hw_err("UBFM_SPRINTF failed. (devid=%u; ret=%d)\n", cfg->devid, ret);
				return -EINVAL;
			}
			node->win_4m_id = i;
			node->win_4m_fpga_addr = cfg->pcie_info.fpga_reg_base +
				ucmi_pcie_get_fpga_4m_win_reg_offset(cfg->pcie_info.ucmi_id, i);
			node->win_4m_fpga_val = win_4m_fpga_val;
			node->win_4m_ref++;
			node->win_4m_acc = win_4m_acc;
			node->axi_id = axi_id;
			node->aligned_addr = aligned_addr;
			*win_4m_id = i;
			hw_debug("alloc win_4m. (devid=%u, win_4m_id=%u, win_4m_ref=%u)\n", cfg->devid, i, node->win_4m_ref);
			return 0;
		}
	}
	return -ENOMEM;
}

static int ucmi_pcie_alloc_win_4m_inner(struct ucmi_pcie_win_cfg *cfg, struct udev_ucmi_addr *addr,
										u32 axi_id, u64 *win_4m_bitmap)
{
	u32 remain_size = addr->size;
	u64 cur_chip_addr =  addr->chip_addr;
	u32 cur_size = 0;
	u32 win_4m_id = 0;
	int ret = 0;

	/* exit when remain_size == 0 */
	while (remain_size > 0) {
		cur_size = UCMI_WIN_SIZE_4M - (cur_chip_addr & UCMI_WIN_SIZE_4M_MASK);
		cur_size = remain_size < cur_size ? remain_size : cur_size;  /* remain_size is less than one window */
		cur_chip_addr &= (~UCMI_WIN_SIZE_4M_MASK);
		ret = ucmi_pcie_alloc_one_win_4m(cfg, cur_chip_addr, axi_id, addr->name, &win_4m_id);
		if (ret != 0) {
			ucmi_pcie_release_win_4m_inner(cfg, win_4m_bitmap);
			hw_err("alloc win 4m failed. (devid=%u; name=%s; size=0x%x)\n", addr->devid, addr->name, addr->size);
			return ret;
		}
		SET_BIT_U64(win_4m_id, win_4m_bitmap);
		cur_chip_addr += UCMI_WIN_SIZE_4M;
		remain_size -= cur_size;
	}
	return 0;
}

static int ucmi_pcie_alloc_win_4m(struct udev_ucmi_addr *addr, u32 axi_id, u64 *win_4m_bitmap)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, addr->devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u; name=%s; size=0x%x)\n", addr->devid, addr->name, addr->size);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_alloc_win_4m_inner(d_ctx, addr, axi_id, win_4m_bitmap);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

int ucmi_pcie_alloc_win_4m_by_id(u32 devid, u32 axi_id, const char *name, u32 win_4m_id, u64 aligned_addr, u64 *local_addr)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct ucmi_pcie_win_4m_node *node = NULL;
	u32 win_4m_fpga_val = ucmi_pcie_get_fpga_4m_win_reg_val(axi_id, aligned_addr);
	u64 win_4m_acc = 0;
	int ret = 0;

	hw_debug("cfg dyn 4m. (devid=%u, axi_id=%u, name=%s, 4m_id=%u, addr=%#llx\n",
		devid, axi_id, name, win_4m_id, aligned_addr);

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("cfg dyn 4m. (devid=%u\n", devid);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	node = &d_ctx->win_4m_node[win_4m_id];
	if (node->win_4m_ref != 0) {
		hw_err("4m win already allocated. (devid=%u; win_4m_id=%u)\n", devid, win_4m_id);
		ret = -EINVAL;
		goto end;
	}
	if (ucmi_pcie_get_acc_addr(&d_ctx->pcie_info, win_4m_id, aligned_addr, &win_4m_acc) != 0) {
		ret = -EINVAL;
		hw_err("cfg dyn 4m. (devid=%u\n", devid);
		goto end;
	}
	ret = UBFM_SPRINTF(node->name, sizeof(node->name), "%s", name);
	if (ret <= 0) {
		hw_err("UBFM_SPRINTF failed. (devid=%u; ret=%d)\n", devid, ret);
		ret = -EINVAL;
		goto end;
	}
	ret = 0;
	node->win_4m_id = win_4m_id;
	node->win_4m_fpga_addr = d_ctx->pcie_info.fpga_reg_base +
		ucmi_pcie_get_fpga_4m_win_reg_offset(d_ctx->pcie_info.ucmi_id, win_4m_id);
	node->win_4m_fpga_val = win_4m_fpga_val;
	node->win_4m_ref++;
	node->win_4m_acc = win_4m_acc;
	node->axi_id = axi_id;
	node->aligned_addr = aligned_addr;
	*local_addr = win_4m_acc;

end:
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

static void ucmi_pcie_free_addr_info_inner(struct ucmi_pcie_win_cfg *cfg, struct udev_ucmi_addr_info *addr_info)
{
	struct udev_ucmi_win *pos = NULL;
	struct udev_ucmi_win *tmp = NULL;

	list_for_each_entry_safe(pos, tmp, &addr_info->win_list, entry) {
		list_del(&pos->entry);
		vfree(pos);
	}
	vfree(addr_info);
}

static void ucmi_pcie_free_addr_info(struct udev_ucmi_addr *addr)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, addr->devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u; name=%s; size=0x%x)\n", addr->devid, addr->name, addr->size);
		return;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	info = &d_ctx->info;
	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		if (strcmp(addr->name, pos->name) == 0 && addr->dieid == pos->dieid) {
			list_del(&pos->entry);
			ucmi_pcie_free_addr_info_inner(d_ctx, pos);
			break;
		}
	}
	mutex_unlock(&info->mtx);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
}

static int ucmi_pcie_alloc_addr_info_inner(struct ucmi_pcie_win_cfg *cfg, struct udev_ucmi_addr *addr,
										   u32 axi_id, u64 *win_4m_bitmap)
{
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct ucmi_pcie_win_4m_node *node = NULL;
	struct udev_ucmi_win *win = NULL;
	struct udev_ucmi_win *pos = NULL;
	struct udev_ucmi_win *tmp = NULL;
	u32 remain_size = addr->size;
	u64 cur_chip_addr = addr->chip_addr;
	u32 cur_size = 0;
	u32 static_win_num = cfg->pcie_info.win_4m_num - UCMI_PCIE_DYN_4M_WIN_NUM;
	u32 i = 0, idx = 0;
	int ret = 0;

	addr_info = ucmi_pcie_find_addr_info(cfg, addr);
	if (addr_info != NULL) {
		hw_err("Addr info already exists. (devid=%u, name=%s, dieid=%u)\n", addr->devid, addr->name, addr->dieid);
		return -EEXIST;
	}
	addr_info = vzalloc(sizeof(struct udev_ucmi_addr_info));
	if (addr_info == NULL) {
		hw_err("Alloc addr info failed. (devid=%u, name=%s, dieid=%u)\n", addr->devid, addr->name, addr->dieid);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&addr_info->win_list);
	ret = UBFM_SPRINTF(&addr_info->name[0], sizeof(addr_info->name), "%s", addr->name);
	if (ret <= 0) {
		hw_err("Format addr info name failed. (devid=%u, name=%s, dieid=%u)\n", addr->devid, addr->name, addr->dieid);
		vfree(addr_info);
		return -EINVAL;
	}

	addr_info->chip_addr = addr->chip_addr;
	addr_info->size = addr->size;
	addr_info->dieid = addr->dieid;

	for (i = 0; i < static_win_num; i++) {
		if (TEST_BIT_U64(i, win_4m_bitmap) == 0) {
			continue;
		}
		node = &cfg->win_4m_node[i];
		win = vzalloc(sizeof(struct udev_ucmi_win));
		if (win == NULL) {
			hw_err("Alloc win failed. (devid=%u, name=%s, dieid=%u)\n", addr->devid, addr->name, addr->dieid);
			ret = -ENOMEM;
			goto err;
		}
		ret = UBFM_SPRINTF(&win->name[0], sizeof(win->name), "%s_win%u", addr->name, idx);
		if (ret <= 0) {
			hw_err("Format failed. (devid=%u, idx=%u, name=%s, dieid=%u)\n", addr->devid, idx, addr->name, addr->dieid);
			vfree(win);
			ret = -EINVAL;
			goto err;
		}
		idx++;
		cur_size = UCMI_WIN_SIZE_4M - (cur_chip_addr & UCMI_WIN_SIZE_4M_MASK);
		cur_size = remain_size < cur_size ? remain_size : cur_size;  /* remain_size is less than one window */
		win->chip_addr = cur_chip_addr;
		win->size = cur_size;
		win->ucmi_base = node->win_4m_acc;
		win->ucmi_off = cur_chip_addr & UCMI_WIN_SIZE_4M_MASK;
		cur_chip_addr = (cur_chip_addr & (~UCMI_WIN_SIZE_4M_MASK)) + UCMI_WIN_SIZE_4M;
		list_add_tail(&win->entry, &addr_info->win_list);
	}
	addr_info->axi_id = axi_id;
	(void)ubfm_memcpy(addr_info->win_4m_bitmap, sizeof(addr_info->win_4m_bitmap), win_4m_bitmap, sizeof(addr_info->win_4m_bitmap));
	mutex_lock(&cfg->info.mtx);
	list_add_tail(&addr_info->entry, &cfg->info.addr_list);
	mutex_unlock(&cfg->info.mtx);
	return 0;

err:
	list_for_each_entry_safe(pos, tmp, &addr_info->win_list, entry) {
		list_del(&pos->entry);
		vfree(pos);
	}
	vfree(addr_info);
	return ret;
}

static int ucmi_pcie_alloc_addr_info(struct udev_ucmi_addr *addr, u32 axi_id, u64 *win_4m_bitmap)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	u32 devid = addr->devid;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u; name=%s; size=0x%x)\n", devid, addr->name, addr->size);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_alloc_addr_info_inner(d_ctx, addr, axi_id, win_4m_bitmap);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);

	return ret;
}

void ucmi_pcie_clear_axi(u32 axi_id)
{
}

int ucmi_pcie_cfg_axi(u32 devid, u32 axi_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct ucmi_pcie_info *pcie_info = NULL;
	struct ucmi_pcie_axi_node *node = NULL;
	u64 virt_lock_acc = 0;
	u64 virt_axi_acc = 0;
	int ret = 0;
	u32 read_back_data = 0;
	u32 i = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	pcie_info = &d_ctx->pcie_info;
	node = &d_ctx->axi_node[axi_id];
	virt_lock_acc = (u64)(uintptr_t)pcie_info->virt_ucmi_addr + UCMI_LOCK_OFFSET; /* within 4kb range */
	virt_axi_acc = (u64)(uintptr_t)pcie_info->virt_ucmi_addr + node->axi_addr - pcie_info->ucmi_reg_base; /* within 4kb range */
	ret = ldst_back_writel_kern(d_ctx->devid, virt_lock_acc, UCMI_UNLOCK_VAL); /* unlock the axi register */
	if (ret != 0) {
		goto end;
	}
	/* configure the axi register. the high 20 bits of the address. bit[28:47] */
	ret = ldst_back_writel_kern(d_ctx->devid, virt_axi_acc, node->axi_val);
#ifndef EMU_ST
	for (i = 0; i < 3; i++) { /* 3 : read back 3 times */
		(void)ldst_back_readl_kern(d_ctx->devid, virt_axi_acc, &read_back_data);
		if (read_back_data == node->axi_val) {
			break;
		}
	}

	if (read_back_data != node->axi_val) {
		hw_warn("Axi cfg read back check failed. (devid=%u; axi_id=%u; read_back_data=%#x; expect=%#llx)\n", devid,
				axi_id, read_back_data, node->axi_val);
	}
#endif
end:
	(void)ldst_back_writel_kern(d_ctx->devid, virt_lock_acc, UCMI_LOCK_VAL); /* axi register is locked by default */
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

static int ucmi_pcie_check_axi(struct udev_ucmi_addr *addr, u32 axi_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct ucmi_pcie_info *pcie_info = NULL;
	struct ucmi_pcie_axi_node *node = NULL;
	u64 virt_axi_acc = 0;
	u32 axi_val = 0;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, addr->devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u; name=%s; size=0x%x)\n", addr->devid, addr->name, addr->size);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	pcie_info = &d_ctx->pcie_info;
	node = &d_ctx->axi_node[axi_id];
	virt_axi_acc = (u64)(uintptr_t)pcie_info->virt_ucmi_addr + node->axi_addr - pcie_info->ucmi_reg_base; /* within 4kb range */
	ret = ldst_back_readl_kern(d_ctx->devid, virt_axi_acc, &axi_val);
	if (ret != 0) {
		goto end;
	}
	if (axi_val == UCMI_AXI_ERROR_VAL1 || axi_val == UCMI_AXI_ERROR_VAL2) { /* axi_val may be udie0 or udie1 high addr */
		hw_err("Slave UCMI check nodev. (devid=%u; axi_val=%#x)\n", d_ctx->devid, axi_val);
		ret = -ENODEV;
	} else {
		node->axi_val = axi_val; /* save the correct axi value */
	}
end:
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

void ucmi_pcie_clear_win_4m(u64 *win_4m_bitmap)
{
}

int ucmi_pcie_cfg_win_4m(u32 devid, u64 *win_4m_bitmap)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	u32 max_win_num = 0;
	u32 i = 0;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	max_win_num = d_ctx->pcie_info.win_4m_num;
	for (i = 0; i < max_win_num; i++) {
		if (TEST_BIT_U64(i, win_4m_bitmap) == 0) {
			continue;
		}
		ret = d_ctx->fpga_ops->cfg_win_4m(d_ctx->devid, i, d_ctx->win_4m_node[i].win_4m_fpga_val);
		if (ret != 0) {
			hw_err("cfg_win_4m failed. (devid=%u; win_4m_id=%u; ret=%d)\n", d_ctx->devid, i, ret);
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

static int ucmi_pcie_assign_win(struct udev_ucmi_addr *addr)
{
	u32 axi_id = 0;
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};
	int ret = 0;

	ret = ucmi_pcie_alloc_axi(addr->devid, addr->chip_addr, &axi_id);
	if (ret != 0) {
		hw_err("alloc axi failed. (devid=%u; name=%s; size=0x%x)\n", addr->devid, addr->name, addr->size);
		return ret;
	}
	ret = ucmi_pcie_alloc_win_4m(addr, axi_id, win_4m_bitmap);
	if (ret != 0) {
		goto end2;
	}
	ret = ucmi_pcie_alloc_addr_info(addr, axi_id, win_4m_bitmap);
	if (ret != 0) {
		goto end1;
	}
	ret = ucmi_pcie_cfg_win_4m(addr->devid, win_4m_bitmap);
	if (ret != 0) {
		goto end0;
	}
	ret = ucmi_pcie_cfg_axi(addr->devid, axi_id);
	if (ret != 0) {
		goto end0;
	}

	hw_debug("assign win succ. (ret=%d, devid=%u, name=%s)\n", ret, addr->devid, addr->name);
	return 0;

end0:
	ucmi_pcie_free_addr_info(addr);
end1:
	ucmi_pcie_release_win_4m(addr->devid, win_4m_bitmap);
end2:
	ucmi_pcie_release_axi(addr->devid, axi_id);
	hw_err("assign win failed. (ret=%d, devid=%u, name=%s)\n", ret, addr->devid, addr->name);
	return ret;
}

static int ucmi_pcie_assign_256m_win(u32 devid, u64 chip_addr, u32 *axi_id)
{
	u32 alloc_axi_id = 0;
	int ret;

	ret = ucmi_pcie_alloc_axi(devid, chip_addr, &alloc_axi_id);
	if (ret != 0) {
		hw_err("alloc axi failed. (devid=%u)\n", devid);
		return ret;
	}

	ret = ucmi_pcie_cfg_axi(devid, alloc_axi_id);
	if (ret != 0) {
		ucmi_pcie_release_axi(devid, alloc_axi_id);
		hw_err("cfg axi failed. (ret=%d; devid=%u; axi_id=%u)\n", ret, devid, alloc_axi_id);
		return ret;
	}

	*axi_id = alloc_axi_id;
	hw_debug("assign win axi succ. (devid=%u; axi_id=%u)\n", devid, alloc_axi_id);
	return 0;
}

static int ucmi_pcie_assign_win_by_axi(u32 axi_id, struct udev_ucmi_addr *addr)
{
	int ret = 0;
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM] = {0};

	if (axi_id >= UCMI_PCIE_AXI_NUM) {
		hw_err("Invalid axi_id. (devid=%u; name=%s; axi_id=%u)\n", addr->devid, addr->name, axi_id);
		return -EINVAL;
	}

	ret = ucmi_pcie_alloc_axi_by_id(addr->devid, axi_id, addr->chip_addr);
	if (ret != 0) {
		return ret;
	}
	ret = ucmi_pcie_alloc_win_4m(addr, axi_id, win_4m_bitmap);
	if (ret != 0) {
		goto end2;
	}
	ret = ucmi_pcie_alloc_addr_info(addr, axi_id, win_4m_bitmap);
	if (ret != 0) {
		goto end1;
	}
	ret = ucmi_pcie_cfg_win_4m(addr->devid, win_4m_bitmap);
	if (ret != 0) {
		goto end0;
	}
	ret = ucmi_pcie_check_axi(addr, axi_id);
	if (ret != 0) {
		goto end0;
	}
	return 0;

end0:
	ucmi_pcie_free_addr_info(addr);
end1:
	ucmi_pcie_release_win_4m(addr->devid, win_4m_bitmap);
end2:
	ucmi_pcie_release_axi(addr->devid, axi_id);
	return ret;
}

/* clean up addr_info resources and window counts */
static void ucmi_pcie_remove_addr_info(struct ucmi_pcie_win_cfg *cfg, struct udev_ucmi_addr_info *addr_info)
{
	ucmi_pcie_release_win_4m_inner(cfg, addr_info->win_4m_bitmap);
	ucmi_pcie_release_axi_inner(cfg, addr_info->axi_id);
	ucmi_pcie_free_addr_info_inner(cfg, addr_info);
}

static void ucmi_pcie_remove_win(struct udev_ucmi_addr *addr)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;
	u32 devid = addr->devid;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u)\n", devid);
		return;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	info = &d_ctx->info;
	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		if (strcmp(addr->name, pos->name) == 0 && addr->dieid == pos->dieid) {
			list_del(&pos->entry);
			ucmi_pcie_remove_addr_info(d_ctx, pos);
			break;
		}
	}
	mutex_unlock(&info->mtx);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
}

static void ucmi_pcie_remove_all_share_win(u32 devid, struct ucmi_pcie_win_cfg *cfg)
{
	struct ucmi_pcie_win_4m_node *win_4m_node = NULL;
	u32 win_4m_id;
	struct ures_win_info win_info[URES_WIN_TYPE_MAX] = { 0 };
	u32 i, j;
	int ret;

	for (i = 0; i < URES_WIN_TYPE_MAX; i++) {
		ret = ures_get_share_local_win(devid, i, &win_info[i]);
		if (ret != 0) {
			hw_err("Cannot get share win info. (devid=%u)\n", devid);
			return;
		}
		for (j = 0; j < win_info[i].win_num; j++) {
			win_4m_id = j + win_info[i].win_id_start;
			if (win_4m_id >= UCMI_PCIE_MAX_WIN_4M_NUM) {
				break;
			}
			win_4m_node = &cfg->win_4m_node[win_4m_id];
			if (win_4m_node->win_4m_ref == 0) {
				continue;
			}
			if (win_4m_node->axi_id < UCMI_PCIE_AXI_NUM) {
				ucmi_pcie_release_axi_inner(cfg, win_4m_node->axi_id);
			}
			win_4m_node->win_4m_ref = 0;
		}
	}
}

static void ucmi_pcie_remove_all_dynamic_win(u32 devid)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Cannot find win cfg. (devid=%u)\n", devid);
		return;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	info = &d_ctx->info;
	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		if (strcmp(pos->name, UCMI_WIN_NAME) == 0 || strcmp(pos->name, DEV_INFO_WIN_NAME) == 0) {
			continue;
		}
		list_del(&pos->entry);
		ucmi_pcie_remove_addr_info(d_ctx, pos);
	}
	mutex_unlock(&info->mtx);
	ucmi_pcie_remove_all_share_win(devid, d_ctx);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
}

static void ucmi_pcie_remove_all_win(struct ucmi_pcie_win_cfg *cfg)
{
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	mutex_lock(&cfg->mtx);
	info = &cfg->info;
	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		list_del(&pos->entry);
		ucmi_pcie_remove_addr_info(cfg, pos);
	}
	mutex_unlock(&info->mtx);
	mutex_unlock(&cfg->mtx);
}

static int ucmi_pcie_dev_init(struct ucmi_pcie_win_cfg *win_cfg, struct udev_pcie *pcie)
{
	u64 bar_size = 0;
	u64 ucmi_acc = 0;
	u32 win_num = 0;
	u32 i = 0;
	int ret = 0;

	win_cfg->pcie_info.fpga_reg_base	= pcie->fpga_reg_base;
	win_cfg->pcie_info.ucmi_reg_base	= pcie->ucmi_reg_base;
	win_cfg->pcie_info.ucmi_id		  = pcie->chip_mng.ucmi_id;
	win_cfg->pcie_info.ucmi_map_bar_num = pcie->chip_mng.ucmi_map_bar_num;
	for (i = 0; i < win_cfg->pcie_info.ucmi_map_bar_num && i < UCMI_MAP_BAR_NUM_MAX; i++) {
		win_cfg->pcie_info.ucmi_bar_addr[i] = pcie->chip_mng.base_addr[i];
		win_cfg->pcie_info.ucmi_bar_size[i] = pcie->chip_mng.size[i];
		bar_size += pcie->chip_mng.size[i];
	}

	win_num = bar_size / UCMI_WIN_SIZE_4M;
	if (win_num <= UCMI_PCIE_DYN_4M_WIN_NUM || win_num > UCMI_PCIE_MAX_WIN_4M_NUM) {
		hw_err("Ucmi win_4m num error. (devid=%u; ret=%d; win_num=%u])\n", pcie->devid, ret, win_num);
		return -EINVAL;
	}

	win_cfg->pcie_info.win_4m_num = win_num;
	win_cfg->pcie_info.win_4m_mode = win_num <= UCMI_PCIE_MIN_WIN_4M_NUM ? UOP_MODE_SHARE : UOP_MODE_EXCLUSIVE;

	if (ucmi_pcie_get_acc_addr(&win_cfg->pcie_info, 0, pcie->ucmi_reg_base, &ucmi_acc) != 0) {
		return -EINVAL;
	}

	win_cfg->fpga_ops = fpga_ops_get();
	if (win_cfg->fpga_ops == NULL) {
		hw_err("Get fpga ops failed. (devid=%u)\n", pcie->devid);
		return -EINVAL;
	}

	win_cfg->pcie_info.virt_ucmi_addr = ioremap(ucmi_acc, 0x1000); /* allocate 4kb space */
	if (win_cfg->pcie_info.virt_ucmi_addr == NULL) {
		return -ENOMEM;
	}

	mutex_init(&win_cfg->mtx);
	mutex_init(&win_cfg->info.mtx);

	win_cfg->devid = pcie->devid;
	win_cfg->info.devid = pcie->devid;
	INIT_LIST_HEAD(&win_cfg->info.addr_list);

	ret = udev_init_win_axi(pcie->devid, pcie->chip_type, pcie->ucmi_reg_base, win_cfg->axi_node);
	if (ret != 0) {
		hw_err("Init udev win axi failed. (devid=%u; ret=%d)\n", pcie->devid, ret);
		mutex_destroy(&win_cfg->info.mtx);
		mutex_destroy(&win_cfg->mtx);
		iounmap(win_cfg->pcie_info.virt_ucmi_addr);
		return ret;
	}

	return 0;
}

static void ucmi_pcie_dev_uninit(struct ucmi_pcie_win_cfg *cfg)
{
	ucmi_pcie_remove_all_win(cfg); /* fallback measure, delete all window resources */
	iounmap(cfg->pcie_info.virt_ucmi_addr);
	cfg->fpga_ops = NULL;
	mutex_destroy(&cfg->info.mtx);
	mutex_destroy(&cfg->mtx);
}

static void ucmi_pcie_dev_ctx_release(struct array_ctx *ctx)
{
	struct ucmi_pcie_win_cfg *d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);

	ucmi_pcie_dev_uninit(d_ctx);
	vfree(d_ctx);
}

static int ucmi_pcie_win_cfg_add(struct udev_pcie *pcie)
{
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	u32 devid = pcie->devid;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx != NULL) {
		array_ctx_put(ctx);
		return 0;
	}
	d_ctx = vzalloc(sizeof(*d_ctx));
	if (d_ctx == NULL) {
		hw_err("Vmalloc dev ctx fail. (size=%lu)\n", sizeof(*d_ctx));
		return -ENOMEM;
	}

	ret = ucmi_pcie_dev_init(d_ctx, pcie);
	if (ret != 0) {
		vfree(d_ctx);
		hw_err("Pcie dev init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	ret = array_ctx_create(ucmi_pcie_dev_domain, devid, d_ctx, ucmi_pcie_dev_ctx_release);
	if (ret != 0) {
		ucmi_pcie_dev_uninit(d_ctx);
		vfree(d_ctx);
		hw_err("Create fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	return 0;
}

STATIC void ucmi_pcie_win_cfg_remove(u32 devid)
{
	struct array_ctx *ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);

	if (ctx == NULL) {
		return;
	}

	array_ctx_put(ctx); /* array_ctx_get */
	array_ctx_destroy(ctx); /* array_ctx_create */
}

static int ucmi_pcie_init(void *priv_data)
{
	int ret = 0;
	struct udev_pcie *pcie = (struct udev_pcie *)priv_data;

	if (pcie == NULL) {
		hw_err("Invalid para.\n");
		return -EINVAL;
	}
	ret = ucmi_pcie_win_cfg_add(pcie);
	if (ret != 0) {
		hw_err("Add win cfg failed. (devid=%u, ret=%d)\n", pcie->devid, ret);
		return ret;
	}

	return 0;
}

static void ucmi_pcie_uninit(void *priv_data)
{
	struct udev_pcie *pcie = (struct udev_pcie *)priv_data;

	if (pcie == NULL) {
		hw_warn("Invalid para.\n");
		return;
	}

	ucmi_pcie_win_cfg_remove(pcie->devid);
}

#ifdef CFG_BUILD_DEBUG
DEFINE_MUTEX(g_ucmi_pcie_lock); /* lock protecting the two global variables below */
static u32 g_ucmi_pcie_debug_devid;
static int g_ucmi_pcie_last_item_idx;

STATIC ssize_t ucmi_pcie_info_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const char *p = buf;

	mutex_lock(&g_ucmi_pcie_lock);
	SIMPLE_STRTOUL_SAFE(p, g_ucmi_pcie_debug_devid, 0);
	g_ucmi_pcie_last_item_idx = 0;

	hw_info("devid=%u\n", g_ucmi_pcie_debug_devid);
	mutex_unlock(&g_ucmi_pcie_lock);
	return (ssize_t)count;
}

STATIC ssize_t ucmi_pcie_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const int max_items_print_once = 32;	 /* display up to 32 items at a time */
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct ucmi_pcie_win_4m_node *node_4m = NULL;
	struct ucmi_pcie_axi_node *axi = NULL;
	ssize_t count = 0;
	int ret, print_count = 0;
	u32 devid;
	int last_item_idx;

	mutex_lock(&g_ucmi_pcie_lock);
	devid = g_ucmi_pcie_debug_devid;
	last_item_idx = g_ucmi_pcie_last_item_idx;
	mutex_unlock(&g_ucmi_pcie_lock);

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		ret = UBFM_SPRINTF(buf, PAGE_SIZE, "Cannot find ucmi device. (devid=%u)\n", devid);
		if (ret > 0) {
			count = (ssize_t)ret;
		}
		return count;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "win_id  reg_addr       reg_val  "
		"name                    chip_addr      axi_id  axi_val  ucmi_offset\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	mutex_lock(&d_ctx->mtx);
	while (print_count < max_items_print_once) {
		if (last_item_idx >= UCMI_PCIE_MAX_WIN_4M_NUM) {
			last_item_idx = 0;
			break;
		}

		node_4m = &d_ctx->win_4m_node[last_item_idx++];
		if (node_4m->win_4m_ref == 0) {
			continue;
		}
		axi = &d_ctx->axi_node[node_4m->axi_id];

		print_count++;
		ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "%-8u0x%-13llx0x%-7x%-24s0x%-13llx%-8u0x%-7llx0x%-13llx\n",
			node_4m->win_4m_id, node_4m->win_4m_fpga_addr, node_4m->win_4m_fpga_val, node_4m->name,
			node_4m->aligned_addr, node_4m->axi_id, axi->axi_val, node_4m->win_4m_acc);
		if (ret > 0) {
			count += (ssize_t)ret;
		}
	}
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);

	if (last_item_idx != 0 && PAGE_SIZE > count) {
		ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "more ...\n");
		if (ret > 0) {
			count += (ssize_t)ret;
		}
	}

	mutex_lock(&g_ucmi_pcie_lock);
	if (devid == g_ucmi_pcie_debug_devid) {
		g_ucmi_pcie_last_item_idx = last_item_idx;
	}
	mutex_unlock(&g_ucmi_pcie_lock);

	return count;
}

STATIC ssize_t ucmi_pcie_nodes_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct array_ctx *ctx = NULL;
	u32 i = 0;
	ssize_t count = 0;
	int ret;

	ret = UBFM_SPRINTF(buf, PAGE_SIZE, "Show all ucmi nodes(devid):\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	for (i = 0; i < DEV_MAX_NUM; i++) {
		ctx = array_ctx_get(ucmi_pcie_dev_domain, i);
		if (ctx == NULL) {
			continue;
		}
		array_ctx_put(ctx);
		ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "%u\n", i);
		if (ret > 0) {
			count += (ssize_t)ret;
		}
	}
	return count;
}
#endif

static struct udev_ucmi_ops g_ucmi_pcie_ops = {
	.init = ucmi_pcie_init,
	.uninit = ucmi_pcie_uninit,
	.master_setup = ucmi_pcie_master_setup,
	.slave_setup = ucmi_pcie_slave_setup,
	.assign_win = ucmi_pcie_assign_win,
	.assign_win_by_axi = ucmi_pcie_assign_win_by_axi,
	.assign_256m_win = ucmi_pcie_assign_256m_win,
	.remove_win = ucmi_pcie_remove_win,
	.info_get = ucmi_pcie_info_get,
	.info_put = ucmi_pcie_info_put,
	.remove_all_dynamic_win = ucmi_pcie_remove_all_dynamic_win,
	.get_win_mode = ucmi_pcie_get_win_4m_mode,
};

static int get_axiid_by_high_addr(struct ucmi_pcie_win_cfg *cfg, u64 high_addr, u32 *axiid)
{
	u32 i = 0;

	for (i = 0; i < UCMI_PCIE_AXI_NUM; i++) {
		if (cfg->axi_node[i].axi_val == high_addr) {
			*axiid = i;
			return 0;
		}
	}
	return -EINVAL;
}

STATIC int ucmi_get_nbar_addr(u32 devid, u64 *nbar_addr, u64 *die_num)
{
	struct ures_inst inst = {0};
	struct ures_addr_info reg = {0};
	u64 die_number = 0;
	u64 i;
	int ret = 0;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_number);
	if (ret != 0 || die_number == 0 || die_number > DIE_MAX_NUM) {
		hw_err("Get die num fail. (ret=%d; devid=%u; die_num=%llu)\n", ret, devid, die_number);
		return -EINVAL;
	}

	inst.devid = devid;
	for (i = 0; i < die_number; i++) {
		inst.die_id = i;
		ret = ures_get_reg_base(&inst, MAMI_MGTQ_REG, &reg);
		if (ret != 0) {
			hw_err("Get MGTQ base failed. (ret=%d; devid=%u; die_id=%u)\n", ret, inst.devid, inst.die_id);
			return ret;
		}
		nbar_addr[i] = UCMI_PCIE_AXI_VALUE(reg.addr);
	}
	*die_num = die_number;
	return 0;
}

static int ucmi_store_axi_in_ures(u32 devid, struct ucmi_pcie_win_cfg *cfg)
{
	u64 nbar_addr[DIE_MAX_NUM], die_num;
	struct udev_ucmi_axi_id_info axi_id_info = {0};
	struct udev_hw_identify hw = {0};
	u32 axiid = 0;
	u32 dynamic_axi_id = (u32)DYN_WIN_AXI_ID;
	u32 i = 0;
	int ret = 0;

	ret = ucmi_get_nbar_addr(devid, nbar_addr, &die_num);
	if (ret != 0) {
		return ret;
	}

	for (i = 0; i < die_num; i++) {
		ret = get_axiid_by_high_addr(cfg, nbar_addr[i], &axiid);
		if (ret != 0) {
			hw_warn("get axiid failed. (devid=%u; ret=%d; dieid=%u)\n", devid, ret, i);
			continue;
		}
		axi_id_info.ucmi_axi_id_val[i] = axiid;
	}

	ret = ures_dev_set_attr(devid, MAMI_UDEV_AXI_ID_INFO, &axi_id_info, sizeof(struct udev_ucmi_axi_id_info));
	if (ret != 0) {
		hw_err("Set udev_axi_id_info failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw attr for dynamic_axi_id failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	if (hw.chip_type == MAMI_KERN_TYPE_UNIONS) {
		dynamic_axi_id = 0xFFFFFFFF;
	}

	ret = ures_dev_set_attr(devid, MAMI_UDEV_DYNAMIC_AXI_ID, &dynamic_axi_id, sizeof(u32));
	if (ret != 0) {
		hw_err("Set udev_dynamic_axi_id failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	return 0;
}

int ucmi_pcie_get_win_4m_mode(u32 devid, u32 *win_4m_mode)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Ctx is null. (devid=%u)\n", devid);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	*win_4m_mode = d_ctx->pcie_info.win_4m_mode;
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return 0;
}

static int ucmi_pcie_get_idle_win_4m_inner(struct ucmi_pcie_win_cfg *cfg, u32 *win_4m_id_start, u32 *win_4m_num)
{
	u32 static_win_num = cfg->pcie_info.win_4m_num - UCMI_PCIE_DYN_4M_WIN_NUM;
	struct ucmi_pcie_win_4m_node *node = NULL;
	u32 win_id_start = 1;	/* win 0 is used by ucmi by default */
	u32 win_num = 0;
	u32 i;

	for (i = 1; i < static_win_num; i++) {
		node = &cfg->win_4m_node[i];
		if (node->win_4m_ref == 0) {
			win_id_start = i;
			win_num = static_win_num - i;
			break;
		}
	}

	for (i = win_id_start; i < static_win_num; i++) {
		node = &cfg->win_4m_node[i];
		if (node->win_4m_ref != 0) {
			hw_err("Win 4m not idle. (devid=%u; win_4m_id=%u; ref=%u)\n", cfg->devid, i, node->win_4m_ref);
			return -EINVAL;
		}
	}
	*win_4m_id_start = win_id_start;
	*win_4m_num = win_num;
	hw_debug("Idle win 4m info. (devid=%u; win_4m_id_start=%u; win_4m_num=%u)\n", cfg->devid, *win_4m_id_start,
		*win_4m_num);
	return 0;
}

int ucmi_pcie_get_idle_win_4m(u32 devid, u32 *win_4m_id_start, u32 *win_4m_num, u32 *win_4m_size)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Ctx is null. (devid=%u)\n", devid);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_get_idle_win_4m_inner(d_ctx, win_4m_id_start, win_4m_num);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	if (ret != 0) {
		hw_err("get idle win 4m failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	*win_4m_size = (*win_4m_num == 0) ? 0 : UCMI_PCIE_WIN_4M_SIZE;
	return ret;
}

int ucmi_pcie_get_axi_id_by_win_4m_inner(struct ucmi_pcie_win_cfg *cfg, u32 win_4m_id, u32 *axi_id)
{
	u32 max_win_num = cfg->pcie_info.win_4m_num;
	struct ucmi_pcie_win_4m_node *node = NULL;

	if (win_4m_id >= max_win_num) {
		return -EINVAL;
	}
	node = &cfg->win_4m_node[win_4m_id];
	if (node->win_4m_ref == 0) {
		return -EINVAL;
	}
	*axi_id = node->axi_id;
	return 0;
}

int ucmi_pcie_get_axi_id_by_win_4m(u32 devid, u32 win_4m_id, u32 *axi_id)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Ctx is null. (devid=%u)\n", devid);
		return -ENODATA;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);
	ret = ucmi_pcie_get_axi_id_by_win_4m_inner(d_ctx, win_4m_id, axi_id);
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	return ret;
}

static int ucmi_pcie_read_axi_inner(u32 devid, struct ucmi_pcie_win_cfg *cfg, u32 axi_id, u32 *axi_val)
{
#ifndef EMU_ST
	struct ucmi_pcie_info *pcie_info = NULL;
	struct ucmi_pcie_axi_node *node = NULL;
	u64 virt_axi_acc = 0;
	int ret = 0;

	pcie_info = &cfg->pcie_info;
	node = &cfg->axi_node[axi_id];
	virt_axi_acc = (u64)(uintptr_t)pcie_info->virt_ucmi_addr + node->axi_addr - pcie_info->ucmi_reg_base;
	ret = ldst_back_readl_kern(devid, virt_axi_acc, axi_val);
	if (ret != 0) {
		hw_warn("Ldst back read failed. (devid=%u; axi_id=%u)\n", devid, axi_id);
		return ret;
	}

	return ret;
#endif
}

int ucmi_pcie_check_all_win_lost(u32 devid, u32 *win_lost)
{
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	u32 cfg_num = 0;
	u32 axi_val = 0;
	u32 axi_id = 0;
	u32 lost = 0;
	int ret = 0;

	if (win_lost == NULL) {
		return -EINVAL;
	}
	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Ctx is null. (devid=%u)\n", devid);
		return -ENODATA;
	}
#ifndef EMU_ST
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);

	for (axi_id = 0x1; axi_id < UCMI_PCIE_AXI_NUM; axi_id++) {
		if (axi_id == DYN_WIN_AXI_ID) {
			continue;
		}

		if (d_ctx->axi_node[axi_id].axi_ref == 0) {
			continue;
		}

		cfg_num++;

		ret = ucmi_pcie_read_axi_inner(devid, d_ctx, axi_id, &axi_val);
		if (ret != 0) {
			continue;
		}

		if (axi_val == UCMI_AXI_ERROR_VAL1 || axi_val == UCMI_AXI_ERROR_VAL2) {
			hw_err("Slave UCMI check nodev. (devid=%u; axi_val=%#x)\n", d_ctx->devid, axi_val);
			ret = -ENODEV;
			break;
		}

		if (axi_val != d_ctx->axi_node[axi_id].axi_val) {
			hw_info_limit("UCMI win lost. (devid=%u; axi_id=%u; axi_val=%#x; expect_axi_value=%#llx)\n", devid, axi_id,
				axi_val, d_ctx->axi_node[axi_id].axi_val);
			lost = 1;
			break;
		}
	}

	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);

	if (cfg_num == 0) {
		hw_err("No axi win is in use. (devid=%u)\n", devid);
		return -ENODATA;
	}

	if (ret != 0) {
		return ret;
	}
	*win_lost = lost;
#endif
	return 0;
}

int ucmi_pcie_check_axi_win_lost(u32 devid, u32 axi_id, u32 *win_lost)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	u32 axi_val;
	int ret = 0;

	if (win_lost == NULL || axi_id >= UCMI_PCIE_AXI_NUM) {
		return -EINVAL;
	}
	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		hw_err("Ctx is null. (devid=%u)\n", devid);
		return -ENODATA;
	}
#ifndef EMU_ST
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);
	mutex_lock(&d_ctx->mtx);

	if (d_ctx->axi_node[axi_id].axi_ref == 0) {
		ret = -ENODATA;
		hw_err("Axi ref is zero. (devid=%u; axi_id=%u)\n", devid, axi_id);
		goto out;
	}

	ret = ucmi_pcie_read_axi_inner(devid, d_ctx, axi_id, &axi_val);
	if (ret != 0) {
		hw_err("Read axi inner failed. (devid=%u; axi_id=%u)\n", devid, axi_id);
		goto out;
	}

	if (axi_val == UCMI_AXI_ERROR_VAL1 || axi_val == UCMI_AXI_ERROR_VAL2) {
		hw_err("Slave UCMI check nodev. (devid=%u; axi_val=%#x)\n", d_ctx->devid, axi_val);
		ret = -ENODEV;
		goto out;
	}

	*win_lost = (axi_val != d_ctx->axi_node[axi_id].axi_val) ? 1U : 0U;

out:
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
#endif
	return ret;
}

STATIC int udev_ucmi_dev_init(u32 devid)
{
	struct array_ctx *ctx = NULL;
	struct ucmi_pcie_win_cfg *d_ctx = NULL;
	int ret = 0;

	ctx = array_ctx_get(ucmi_pcie_dev_domain, devid);
	if (ctx == NULL) {
		return -EINVAL;
	}
	d_ctx = (struct ucmi_pcie_win_cfg *)array_ctx_priv(ctx);

	mutex_lock(&d_ctx->mtx);
	ret = ucmi_store_axi_in_ures(devid, d_ctx);
	if (ret != 0) {
		goto end;
	}

end:
	mutex_unlock(&d_ctx->mtx);
	array_ctx_put(ctx);
	if (ret != 0) {
		return ret;
	}

	/* After probe stage window allocation (including ctrlq), refresh USER share_local_win */
	ret = uop_kern_update_share_local_win(devid);
	if (ret != 0) {
		hw_err("Update share local win fail. (ret=%d; devid=%u)\n", ret, devid);
	}
	return ret;
}

static void udev_ucmi_dev_uninit(u32 devid)
{
}

static int udev_hw_ucmi_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = udev_ucmi_dev_init(devid);
	} else if (action == UDM_UNINIT) {
		udev_ucmi_dev_uninit(devid);
	}
	return ret;
}

static void ucmi_pcie_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
	hw_warn("Recycle dev ctx. (devid=%u)\n", ctx->id);
	ucmi_pcie_win_cfg_remove(ctx->id);
}

int ucmi_pcie_core_init(void)
{
	ucmi_pcie_dev_domain = array_ctx_domain_create("ucmi_dev", DEV_MAX_NUM);
	if (ucmi_pcie_dev_domain == NULL) {
		hw_err("Create dev domain fail.\n");
		return -ENOMEM;
	}
	return 0;
}

void ucmi_pcie_core_uninit(void)
{
	array_ctx_for_each_safe(ucmi_pcie_dev_domain, NULL, ucmi_pcie_dev_ctx_recycle);
	array_ctx_domain_destroy(ucmi_pcie_dev_domain);
	ucmi_pcie_dev_domain = NULL;
}

int ucmi_pcie_ops_init(void)
{
	int ret;

	ret = udm_notifier_register(MODULE_UCMI, UDM_PRI3, udev_hw_ucmi_notify); /* at this point mctrlq is initialized ok, address window configuration is complete */
	if (ret != 0) {
		hw_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = udev_ucmi_ops_register(UDEV_PCIE_ESPI, &g_ucmi_pcie_ops);
	if (ret != 0) {
		(void)udm_notifier_unregister(MODULE_UCMI);
		hw_err("Register PCIE espi ucmi ops failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = ucmi_pcie_core_init();
	if (ret != 0) {
		udev_ucmi_ops_unregister(UDEV_PCIE_ESPI);
		(void)udm_notifier_unregister(MODULE_UCMI);
		return ret;
	}
	uop_kern_register_win_ops(&g_ucmi_win_ops);
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ucmi_pcie_ops_init, FEATURE_LOADER_STAGE_2);

void ucmi_pcie_ops_uninit(void)
{
	uop_kern_unregister_win_ops();
	ucmi_pcie_core_uninit();
	udev_ucmi_ops_unregister(UDEV_PCIE_ESPI);
	(void)udm_notifier_unregister(MODULE_UCMI);
}
DECLARE_FEATURE_AUTO_UNINIT(ucmi_pcie_ops_uninit, FEATURE_LOADER_STAGE_2);

#ifdef CFG_BUILD_DEBUG
static DEVICE_ATTR(info, UBFM_FS_MODE_OWNER_RW, ucmi_pcie_info_show, ucmi_pcie_info_store);
static DEVICE_ATTR(nodes, UBFM_FS_MODE_OWNER_RW, ucmi_pcie_nodes_show, NULL);

static struct attribute *g_ucmi_pcie_sysfs_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_nodes.attr,
	NULL
};

static struct attribute_group g_ucmi_pcie_attr_grp = {
	.name = "ucmi",
	.attrs = g_ucmi_pcie_sysfs_attrs,
};
#endif

STATIC int ucmi_pcie_sysfs_init(void *priv)
{
	int ret = 0;
	struct platform_device *pdev = NULL;

	pdev = (struct platform_device *)priv;
#ifdef CFG_BUILD_DEBUG
	ret = sysfs_create_group(&pdev->dev.kobj, &g_ucmi_pcie_attr_grp);
	if (ret != 0) {
		hw_err("Create group failed. (ret=%d)\n", ret);
	}
#endif
	return ret;
}
DECLARE_FEATURE_AUTO_INIT_BY_SCOPE(sysfs, ucmi_pcie_sysfs_init, FEATURE_LOADER_STAGE_9);

STATIC void ucmi_pcie_sysfs_uninit(void *priv)
{
	struct platform_device *pdev = NULL;

	pdev = (struct platform_device *)priv;
#ifdef CFG_BUILD_DEBUG
	sysfs_remove_group(&pdev->dev.kobj, &g_ucmi_pcie_attr_grp);
#endif
}
DECLARE_FEATURE_AUTO_UNINIT_BY_SCOPE(sysfs, ucmi_pcie_sysfs_uninit, FEATURE_LOADER_STAGE_9);
