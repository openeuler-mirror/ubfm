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
#include <linux/mm.h>
#include <asm/pgtable.h>

#include "ubfabric.h"
#include "ubfabric.h"

#include "ucd.h"
#include "uop_log.h"
#include "umap_kern.h"
#include "uop_kern.h"
#include "uop_fops.h"
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "fpga_adapter_kern.h"
#endif

#define UOP_FOPS_ADD_CMD(s, p, f) {  \
	.main_cmd		 = UCD_CMD_UOP, \
	.sub_cmd		  = s,		   \
	.proc_ctrl_bitmap = p,		   \
	.func			 = f			\
}

struct uop_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

static int uop_fops_mem_map(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_map_in *kin = (struct uop_map_in *)in;

	if (unlikely((kin == NULL) || (in_len != (u32)sizeof(*kin)))) {
		uop_err("Invalid param. (in_len=%u; size=%u)\n", in_len, (u32)sizeof(*kin));
		return -EINVAL;
	}
	if (strnlen(kin->name, URES_NAME_LEN) >= URES_NAME_LEN) {
		uop_err("Invalid name, not null-terminated. (devid=%u; die_id=%u)\n", devid, kin->die_id);
		return -EINVAL;
	}
	return umap_kern_mem_map(devid, kin->die_id, kin->name, kin->acc_addr, kin->size);
}

static int uop_fops_reg_map(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_map_in *kin = (struct uop_map_in *)in;

	if (unlikely((kin == NULL) || (in_len != (u32)sizeof(*kin)))) {
		uop_err("Invalid param. (in_len=%u; size=%u)\n", in_len, (u32)sizeof(*kin));
		return -EINVAL;
	}
	return umap_kern_reg_map(devid, kin->die_id, kin->name, kin->acc_addr, kin->size);
}

static int uop_fops_unmap(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_unmap_in *kin = (struct uop_unmap_in *)in;

	if (unlikely((kin == NULL) || (in_len != (u32)sizeof(*kin)))) {
		uop_err("Invalid param. (in_len=%u; size=%u)\n", in_len, (u32)sizeof(*kin));
		return -EINVAL;
	}

	return umap_kern_unmap(kin->acc_addr, kin->size);
}

#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
static int uop_fops_fpga_back_reg_map(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	u64 *kin = (u64 *)in;
	u64 *kout = (u64 *)out;
	u64 phy_addr = 0;
	int ret = 0;

	if (unlikely((kin == NULL) || (in_len != (u32)sizeof(*kin)))) {
		uop_err("Invalid param. (in_len=%u; size=%zu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}
	if (unlikely((kout == NULL) || (out_len != (u32)sizeof(*kout)))) {
		uop_err("Invalid param. (out_len=%u; size=%zu)\n", out_len, sizeof(*kout));
		return -EINVAL;
	}
	ret = uop_fpga_get_back_paddr(devid, &phy_addr);
	if (unlikely(ret != 0 || phy_addr == 0x0)) {
		uop_err("Get fpga back reg addr failed. (devid=%u)\n", devid);
		return ret;
	}
	ret = umap_fpga_reg_map(devid, *kin, phy_addr & ~(PAGE_SIZE - 1), PAGE_SIZE);
	if (unlikely(ret != 0)) {
		uop_err("Map fpga back reg addr failed. (devid=%u)\n", devid);
		return ret;
	}
	*kout = *kin + (phy_addr & (PAGE_SIZE - 1));
	return 0;
}

static int uop_fops_fpga_back_reg_unmap(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	u64 *kin = (u64 *)in;
	int ret = 0;

	if (unlikely((kin == NULL) || (in_len != (u32)sizeof(*kin)))) {
		uop_err("Invalid param. (in_len=%u; size=%zu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}
	ret = umap_fpga_reg_unmap(*kin, PAGE_SIZE);
	if (ret != 0) {
		return ret;
	}
	return 0;
}
#else
static int uop_fops_fpga_back_reg_map(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	return 0;
}

static int uop_fops_fpga_back_reg_unmap(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	return 0;
}
#endif

static int uop_fops_acc_node_map(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_acc_node *acc_node = NULL;
	struct ures_addr_info mem = {0};
	u64 local_addr = 0;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(struct uop_acc_node)))) {
		uop_err("Invalid param. (in_len=%u; expect=%u)\n", in_len, (u32)sizeof(struct uop_acc_node));
		return -EINVAL;
	}
	acc_node = (struct uop_acc_node *)in;

	if (unlikely((acc_node->len == 0) || !PAGE_ALIGNED(acc_node->len) || (acc_node->len > UCMI_PCIE_WIN_4M_SIZE))) {
		uop_err("Invalid acc node len. (devid=%u; len=%u; len_max=%u)\n", devid, acc_node->len, UCMI_PCIE_WIN_4M_SIZE);
		return -EINVAL;
	}

	ret = uop_kern_get_local_addr(devid, acc_node->local_win_id, &local_addr);
	if (unlikely(ret != 0)) {
		uop_err("Get local addr fail. (devid=%u; local_win_id=%u)\n", devid, acc_node->local_win_id);
		return ret;
	}

	mem.addr = local_addr;
	mem.size = acc_node->len;

	ret = umap_mem_map(acc_node->acc_addr, acc_node->len, &mem, NULL);
	if (unlikely(ret != 0)) {
		uop_err("Mem map fail.(ret=%d; devid=%u)\n", ret, devid);
	}
	return ret;
}

static int uop_fops_acc_node_unmap(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_acc_node *acc_node = NULL;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(struct uop_acc_node)))) {
		uop_err("Invalid param. (in_len=%u; expect=%u)\n", in_len, (u32)sizeof(struct uop_acc_node));
		return -EINVAL;
	}
	acc_node = (struct uop_acc_node *)in;

	return umap_kern_unmap(acc_node->acc_addr, acc_node->len);
}

static int uop_fops_cfg_win(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_acc_node *acc_node = NULL;
	u32 axi_id = 0;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(struct uop_acc_node)))) {
		uop_err("Invalid param. (in_len=%u; expect=%u)\n", in_len, (u32)sizeof(struct uop_acc_node));
		return -EINVAL;
	}
	acc_node = (struct uop_acc_node *)in;

	ret = uop_kern_cfg_win(devid, acc_node->local_win_id, acc_node->asic_addr, &axi_id);
	if (ret == 0 && out != NULL && out_len >= (u32)sizeof(u32)) {
		acc_node->axi_id = axi_id;
		*(u32 *)out = axi_id;
	}
	return ret;
}

static int uop_fops_clear_win(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct uop_acc_node *acc_node = NULL;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(struct uop_acc_node)))) {
		uop_err("Invalid param. (in_len=%u; expect=%u)\n", in_len, (u32)sizeof(struct uop_acc_node));
		return -EINVAL;
	}
	acc_node = (struct uop_acc_node *)in;
	uop_kern_clear_win(devid, acc_node->local_win_id);
	return 0;
}

static const struct uop_fops g_uop_fops[] = {
	[0] = UOP_FOPS_ADD_CMD(UOP_MEM_MAP, PROC_CTRL_MANAGED, uop_fops_mem_map),
	[1] = UOP_FOPS_ADD_CMD(UOP_MEM_UNMAP, PROC_CTRL_MANAGED, uop_fops_unmap),
	[2] = UOP_FOPS_ADD_CMD(UOP_REG_MAP, PROC_CTRL_MANAGED, uop_fops_reg_map),
	[3] = UOP_FOPS_ADD_CMD(UOP_REG_UNMAP, PROC_CTRL_MANAGED, uop_fops_unmap),
	[4] = UOP_FOPS_ADD_CMD(UOP_FPGA_BACK_REG_MAP, PROC_CTRL_MANAGED, uop_fops_fpga_back_reg_map),
	[5] = UOP_FOPS_ADD_CMD(UOP_FPGA_BACK_REG_UNMAP, PROC_CTRL_MANAGED, uop_fops_fpga_back_reg_unmap),
	[6] = UOP_FOPS_ADD_CMD(UOP_ACC_NODE_MAP, PROC_CTRL_MANAGED, uop_fops_acc_node_map),
	[7] = UOP_FOPS_ADD_CMD(UOP_ACC_NODE_UNMAP, PROC_CTRL_MANAGED, uop_fops_acc_node_unmap),
	[8] = UOP_FOPS_ADD_CMD(UOP_CFG_WIN, PROC_CTRL_MANAGED, uop_fops_cfg_win),
	[9] = UOP_FOPS_ADD_CMD(UOP_CLR_WIN, PROC_CTRL_MANAGED, uop_fops_clear_win),
};

static int uop_ucd_register(void)
{
	const u32 num = ARRAY_SIZE(g_uop_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, UOP_MODULE_NAME, g_uop_fops[i].main_cmd,
			g_uop_fops[i].sub_cmd, g_uop_fops[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_uop_fops[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (unlikely(ret != 0)) {
			uop_err("Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}
	return 0;
out:
	for (j = 0; j < i; j++) {
		(void)ucd_unregister_dev_cmd(g_uop_fops[j].main_cmd, g_uop_fops[j].sub_cmd);
	}
	return ret;
}

static void uop_ucd_unregister(void)
{
	const u32 num = ARRAY_SIZE(g_uop_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		(void)ucd_unregister_dev_cmd(g_uop_fops[i].main_cmd, g_uop_fops[i].sub_cmd);
	}
}

int uop_fops_mod_init(void)
{
	int ret;

	ret = uop_ucd_register();
	if (unlikely(ret != 0)) {
		uop_err("Uop ucd register fail. (ret=%d)\n", ret);
	}

	return ret;
}

void uop_fops_mod_uninit(void)
{
	uop_ucd_unregister();
}

