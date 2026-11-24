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
#include <linux/compiler.h>
#include <linux/uaccess.h>

#include "mami_string.h"
#include "ucd.h"

#include "ures_log.h"
#include "ures.h"
#include "ures_fops.h"

struct ures_dev_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

struct ures_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	feature_handler func;
};

#define URES_FOPS_ADD_CMD(s, p, f) {  \
	.main_cmd		 = UCD_CMD_URES, \
	.sub_cmd		  = s,			\
	.proc_ctrl_bitmap = p,			\
	.func			 = f			 \
}

static int ures_fops_get_mem_base(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_get_mem_base_in *kin = (struct ures_get_mem_base_in *)in;
	struct ures_addr_info *kout = (struct ures_addr_info *)out;
	struct ures_inst inst = {.devid = dev_id};
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		ures_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)sizeof(*kout)))) {
		ures_err("Invalid out. (out_len=%u; size=%lu)\n", out_len, sizeof(*kout));
		return -EINVAL;
	}
	inst.die_id = kin->die_id;
	ret = ures_get_mem_base(&inst, kin->name, kout);
	if (unlikely(ret != 0)) {
		ures_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst.devid, inst.die_id);
	}

	return ret;
}

static int ures_fops_get_reg_base(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_get_reg_base_in *kin = (struct ures_get_reg_base_in *)in;
	struct ures_addr_info *kout = (struct ures_addr_info *)out;
	struct ures_inst inst = {.devid = dev_id};
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)))) {
		ures_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)sizeof(*kout)))) {
		ures_err("Invalid out. (out_len=%u; size=%lu)\n", out_len, sizeof(*kout));
		return -EINVAL;
	}

	inst.die_id = kin->die_id;

	ret = ures_get_reg_base(&inst, kin->name, kout);
	if (unlikely(ret != 0)) {
		ures_err("Get reg base fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n", ret, inst.devid, inst.die_id, kin->name);
	}

	return ret;
}

static int ures_fops_get_mem_list(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_get_mem_list_out *kout = (struct ures_get_mem_list_out *)out;
	struct ures_get_mem_list_in *kin = (struct ures_get_mem_list_in *)in;
	struct ures_inst inst = {.devid = dev_id};
	struct ures_mem *mem = NULL;
	u32 num, min_num;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)) ||
		(kin->num == 0) || (kin->num > URES_MEM_MAX_NUM))) {
		ures_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)(sizeof(*kout))))) {
		ures_err("Invalid out. (out_len=%u; size=%lu; num=%u)\n", out_len, sizeof(*kout), kin->num);
		return -EINVAL;
	}

	kout->num = 0;
	inst.die_id = kin->die_id;
	ret = ures_get_mem_list(&inst, &mem, &num);
	if (unlikely(ret != 0)) {
		return ret;
	}

	if ((mem == NULL) || (num == 0)) {
		return 0;
	}

	min_num = min_t(u32, num, kin->num);
	ret = (int)copy_to_user(kin->mem, mem, sizeof(struct ures_mem) * min_num);
	ures_put_mem_list(mem);
	if (unlikely(ret != 0)) {
		ures_err("Copy to user fail. (ret=%d; devid=%u; die_id=%u; min_num=%u)\n",
			ret, inst.devid, inst.die_id, num);
		return -EFAULT;
	}
	kout->num = min_num;

	return 0;
}

static int ures_fops_get_reg_list(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_get_reg_list_out *kout = (struct ures_get_reg_list_out *)out;
	struct ures_get_reg_list_in *kin = (struct ures_get_reg_list_in *)in;
	struct ures_inst inst = {.devid = dev_id};
	struct ures_reg *reg = NULL;
	u32 num, min_num;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)) ||
		(kin->num == 0) || (kin->num > URES_MEM_MAX_NUM))) {
		ures_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)(sizeof(*kout))))) {
		ures_err("Invalid out. (out_len=%u; size=%lu; num=%u)\n", out_len, sizeof(*kout), kin->num);
		return -EINVAL;
	}

	kout->num = 0;
	inst.die_id = kin->die_id;
	ret = ures_get_reg_list(&inst, &reg, &num);
	if (unlikely(ret != 0)) {
		return ret;
	}

	if ((reg == NULL) || (num == 0)) {
		return 0;
	}

	min_num = min_t(u32, num, kin->num);
	ret = (int)copy_to_user(kin->reg, reg, sizeof(struct ures_reg) * min_num);
	ures_put_reg_list(reg);
	if (unlikely(ret != 0)) {
		ures_err("Copy to user fail. (ret=%d; devid=%u; die_id=%u; min_num=%u)\n",
			ret, inst.devid, inst.die_id, num);
		return -EFAULT;
	}
	kout->num = min_num;

	return 0;
}

static int ures_fops_check_name(const void *in, u32 in_len)
{
	if (unlikely((in == NULL) || (in_len == 0) || (in_len > URES_NAME_LEN))) {
		ures_err("Invalid name. (in_is_null=%d; in_len=%u)\n", (in == NULL), in_len);
		return -EINVAL;
	}
	if (unlikely(strnlen((const char *)in, in_len) >= in_len)) {
		ures_err("Invalid name, not null-terminated. (in_len=%u)\n", in_len);
		return -EINVAL;
	}
	return 0;
}

static int ures_fops_get_key(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	int ret = 0;

	if (unlikely(out_len != sizeof(u64))) {
		ures_err("Invalid len. (in_len=%u; out_len=%u)\n", in_len, out_len);
		return -EINVAL;
	}
	ret = ures_fops_check_name(in, in_len);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return ures_get_key_value(dev_id, (const char *)in, out);
}

static int ures_fops_set_key(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_set_dev_key_in *set_key_in = NULL;

	if (unlikely(in_len != sizeof(struct ures_set_dev_key_in))) {
		ures_err("Invalid len. (dev_id=%u, in_len=%u; out_len=%u)\n", dev_id, in_len, out_len);
		return -EINVAL;
	}
	set_key_in = (struct ures_set_dev_key_in *)in;
	return ures_set_key_value(dev_id, set_key_in->name, set_key_in->value);
}

static int ures_fops_get_dev_attr(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	int ret = 0;

	ret = ures_fops_check_name(in, in_len);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return ures_dev_get_attr(dev_id, (const char *)in, out, (size_t)out_len);
}

static int ures_fops_get_attr(void *in, u32 in_len, void *out, u32 out_len)
{
	int ret = 0;

	ret = ures_fops_check_name(in, in_len);
	if (unlikely(ret != 0)) {
		return ret;
	}
	return ures_get_attr((const char *)in, out, (size_t)out_len);
}

static int ures_fops_get_attr_key_list(void *in, u32 in_len, void *out, u32 out_len)
{
	struct ures_get_attr_key_list_out *kout = (struct ures_get_attr_key_list_out *)out;
	struct ures_get_attr_key_list_in *kin = (struct ures_get_attr_key_list_in *)in;
	struct ures_key *key_list = NULL;
	u32 num = 0;
	int ret;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(*kin)) ||
		(kin->num == 0) || (kin->num > URES_ATTR_MAX_NUM))) {
		ures_err("Invalid in. (in_len=%u; size=%lu)\n", in_len, sizeof(*kin));
		return -EINVAL;
	}

	if (unlikely((out == NULL) || (out_len != (u32)(sizeof(*kout))))) {
		ures_err("Invalid out. (out_len=%u)\n", out_len);
		return -EINVAL;
	}

	kout->num = 0;
	num = kin->num;
	ret = ures_get_attr_key_list(&key_list, &num);
	if (unlikely(ret != 0)) {
		ures_err("Get attr tag list fail. (ret=%d; num=%u)\n", ret, num);
		return ret;
	}

	if ((num == 0) || (key_list == NULL)) {
#ifndef EMU_ST
		return 0;
#endif
	}

	ret = (int)copy_to_user(kin->key_list, key_list, sizeof(struct ures_key) * num);
	ures_put_attr_key_list(key_list);
	if (unlikely(ret != 0)) {
		ures_err("Copy to user fail. (ret=%d; num=%u;)\n", ret, num);
		ret = -EFAULT;
	}
	kout->num = num;

	return ret;
}

static int ures_fops_get_win_info(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	enum ures_win_type *win_type = NULL;
	struct ures_win_info *win_info = NULL;

	if (unlikely((in == NULL) || (in_len != (u32)sizeof(enum ures_win_type)))) {
		ures_err("Invalid in. (in_len=%u; expect=%lu)\n", in_len, sizeof(enum ures_win_type));
		return -EINVAL;
	}
	win_type = (enum ures_win_type *)in;
	if (unlikely((out == NULL) || (out_len != (u32)sizeof(struct ures_win_info)))) {
		ures_err("Invalid out. (out_len=%u; expect=%lu)\n", out_len, sizeof(struct ures_win_info));
		return -EINVAL;
	}
	win_info = (struct ures_win_info *)out;
	return ures_get_share_local_win(dev_id, *win_type, win_info);
}

static const struct ures_dev_fops g_ures_dev_fops[] = {
	[0] = URES_FOPS_ADD_CMD(URES_GET_MEM_BASE, PROC_CTRL_MANAGED, ures_fops_get_mem_base),
	[1] = URES_FOPS_ADD_CMD(URES_GET_REG_BASE, PROC_CTRL_MANAGED, ures_fops_get_reg_base),
	[2] = URES_FOPS_ADD_CMD(URES_GET_MEM_LIST, PROC_CTRL_MANAGED, ures_fops_get_mem_list),
	[3] = URES_FOPS_ADD_CMD(URES_GET_REG_LIST, PROC_CTRL_MANAGED, ures_fops_get_reg_list),
	[4] = URES_FOPS_ADD_CMD(URES_GET_KEY_VALUE, PROC_CTRL_ANY_USER, ures_fops_get_key),
	[5] = URES_FOPS_ADD_CMD(URES_SET_KEY_VALUE, PROC_CTRL_MANAGED, ures_fops_set_key),
	[6] = URES_FOPS_ADD_CMD(URES_GET_DEV_ATTR, PROC_CTRL_ANY_USER, ures_fops_get_dev_attr),
	[7] = URES_FOPS_ADD_CMD(URES_GET_WIN_INFO, PROC_CTRL_MANAGED, ures_fops_get_win_info),
};

static const struct ures_fops g_ures_fops[] = {
	[0] = URES_FOPS_ADD_CMD(URES_GET_ATTR, PROC_CTRL_ANY_USER, ures_fops_get_attr),
	[1] = URES_FOPS_ADD_CMD(URES_GET_ATTR_KEY_LIST, PROC_CTRL_ANY_USER, ures_fops_get_attr_key_list),
};

static int ures_ucd_register(void)
{
	const u32 num = ARRAY_SIZE(g_ures_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, URES_MODULE_NAME, g_ures_fops[i].main_cmd,
			g_ures_fops[i].sub_cmd, g_ures_fops[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_ures_fops[i].proc_ctrl_bitmap);
		ret = ucd_register_cmd(&ucd_cmd);
		if (ret != 0) {
			ures_err("Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}

	return 0;
out:
	for (j = 0; j < i; j++) {
		ucd_unregister_cmd(g_ures_fops[j].main_cmd, g_ures_fops[j].sub_cmd);
	}
	return ret;
}

static void ures_ucd_unregister(void)
{
	const u32 num = ARRAY_SIZE(g_ures_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		ucd_unregister_cmd(g_ures_fops[i].main_cmd, g_ures_fops[i].sub_cmd);
	}
}

static int ures_ucd_dev_register(void)
{
	const u32 num = ARRAY_SIZE(g_ures_dev_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;

		ucd_fill_feature_cmd(&ucd_cmd, URES_MODULE_NAME, g_ures_dev_fops[i].main_cmd,
			g_ures_dev_fops[i].sub_cmd, g_ures_dev_fops[i].func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, g_ures_dev_fops[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (ret != 0) {
			ures_err("Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}
	return 0;
out:
	for (j = 0; j < i; j++) {
		(void)ucd_unregister_dev_cmd(g_ures_dev_fops[j].main_cmd, g_ures_dev_fops[j].sub_cmd);
	}
	return ret;
}

static void ures_ucd_dev_unregister(void)
{
	const u32 num = ARRAY_SIZE(g_ures_dev_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		(void)ucd_unregister_dev_cmd(g_ures_dev_fops[i].main_cmd, g_ures_dev_fops[i].sub_cmd);
	}
}

int ures_fops_init(void)
{
	int ret;

	ret = ures_ucd_register();
	if (ret != 0) {
		ures_err("Ucd register fail. (ret=%d)\n", ret);
		return ret;
	}

	ret = ures_ucd_dev_register();
	if (ret != 0) {
		ures_ucd_unregister();
		ures_err("Ucd dev register fail. (ret=%d)\n", ret);
	}

	return ret;
}

void ures_fops_uninit(void)
{
	ures_ucd_dev_unregister();
	ures_ucd_unregister();
}

