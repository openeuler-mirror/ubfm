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
#include <linux/kernel.h>
#include <linux/slab.h>
#include "mami_string.h"

#include "fmdevm.h"
#include "ras_log.h"
#include "apei_tbl_load.h"

static struct local_ras_reg *local_ras_find_reg(struct local_ras_submodule *module, const char *name)
{
	u32 i;
	struct local_ras_reg *reg = NULL;

	for (i = 0; i < module->reg_num; i++) {
		reg = &module->regs[i];
		if (strcmp(reg->name, name) == 0) {
			return reg;
		}
	}

	return NULL;
}

static void local_ras_group_reg_field_destroy(struct local_ras_reg_field *reg_field)
{
	struct local_ras_group_reg_field *grp_reg_field = NULL;
	struct local_ras_group_reg_field *tmp = NULL;

	list_for_each_entry_safe(grp_reg_field, tmp, &reg_field->group_regs, entry) {
		list_del(&grp_reg_field->entry);
		kfree(grp_reg_field);
	}
}

static void local_ras_assoc_reg_destroy(struct local_ras_reg_field *reg_field)
{
	struct local_ras_assoc_reg *reg = NULL;
	struct local_ras_assoc_reg *tmp = NULL;

	list_for_each_entry_safe(reg, tmp, &reg_field->associate_regs, entry) {
		list_del(&reg->entry);
		kfree(reg);
	}
}

static int load_group_reg_fields(struct apei_fault_reg *fault_reg, struct local_ras_reg_field *reg_field)
{
	struct apei_group_reg *grp_reg = NULL;
	struct local_ras_group_reg_field *grp_reg_field = NULL;
	struct local_ras_reg *reg = NULL;
	u32 i;
	int ret;

	if (fault_reg->group_num == 0 || fault_reg->group_regs == NULL) {
		return 0;
	}

	for (i = 0; i < fault_reg->group_num; i++) {
		grp_reg = &fault_reg->group_regs[i];
		if (grp_reg->bit_len == 0 || grp_reg->bit_start + grp_reg->bit_len > LOCAL_RAS_REG_BITWIDTH) {
			ras_err("Invalid bit len.(module_name=%s; reg_name=%s; grp_idx=%u; bit_start=%u; bit_len=%u)\n",
					fault_reg->module_name, fault_reg->name, i, grp_reg->bit_start, grp_reg->bit_len);
			ret = -EINVAL;
			goto fail;
		}

		reg = local_ras_find_reg(reg_field->reg->module, grp_reg->name);
		if (reg == NULL) {
			ras_err("Cannot find. (module_name=%s; reg_name=%s; grp_reg_name=%s; grp_idx=%u)\n",
					fault_reg->module_name, fault_reg->name, grp_reg->name, i);
			continue;
		}

		grp_reg_field = kzalloc(sizeof(struct local_ras_group_reg_field), GFP_KERNEL | __GFP_ACCOUNT);
		if (grp_reg_field == NULL) {
			ras_err("Alloc grp reg fields failed. (module_name=%s; reg_name=%s; grp_reg_name=%s; grp_idx=%u)\n",
					fault_reg->module_name, fault_reg->name, grp_reg->name, i);
			ret = -ENOMEM;
			goto fail;
		}

		grp_reg_field->bit_start = grp_reg->bit_start;
		grp_reg_field->bit_len = grp_reg->bit_len;
		grp_reg_field->default_val = grp_reg->default_val;
		grp_reg_field->reg = reg;
		list_add_tail(&grp_reg_field->entry, &reg_field->group_regs);
	}

	return 0;
fail:
	local_ras_group_reg_field_destroy(reg_field);
	return ret;
}

static void load_associate_regs(struct apei_fault_reg *fault_reg, struct local_ras_reg *reg,
								struct local_ras_reg_field *reg_field)
{
	struct local_ras_reg *tmp_reg = NULL;
	u32 i;

	for (i = 0; i < fault_reg->associated_num; i++) {
		struct local_ras_assoc_reg *assoc_reg = NULL;

		if (strncmp(fault_reg->module_name, reg->module->name, strlen(fault_reg->module_name)) != 0) {
			continue;
		}

		tmp_reg = local_ras_find_reg(reg->module, fault_reg->associated_regs[i].name);
		if (tmp_reg == NULL) {
			ras_err("Find group reg failed. (module_name=%s; assoc_reg=%s; assoc_idx=%u)\n",
					reg->module->name, fault_reg->associated_regs[i].name, i);
			continue;
		}

		assoc_reg = kzalloc(sizeof(struct local_ras_assoc_reg), GFP_KERNEL | __GFP_ACCOUNT);
		if (assoc_reg == NULL) {
			ras_err("Alloc assoc reg memory failed. (module_name=%s; assoc_reg=%s; assoc_idx=%u)\n",
					reg->module->name, fault_reg->associated_regs[i].name, i);
			continue;
		}

		assoc_reg->reg = tmp_reg;
		list_add_tail(&assoc_reg->entry, &reg_field->associate_regs);
	}
}

static struct local_ras_reg_field *local_ras_reg_field_create(struct apei_fault_reg *fault_reg,
															  struct local_ras_reg *reg)
{
	struct local_ras_reg_field *reg_field = NULL;
	int ret;

	if (fault_reg->bit_len == 0 || fault_reg->bit_start + fault_reg->bit_len > LOCAL_RAS_REG_BITWIDTH) {
		ras_err("Invalid bit. (module_name=%s; fault_reg_name=%s; bit_start=%u; bit_len=%u)\n",
				fault_reg->module_name, fault_reg->name, fault_reg->bit_start, fault_reg->bit_len);
		return NULL;
	}

	reg_field = kzalloc(sizeof(struct local_ras_reg_field), GFP_KERNEL | __GFP_ACCOUNT);
	if (reg_field == NULL) {
		ras_err("Alloc reg field failed.\n");
		return NULL;
	}

	reg_field->bit_start = fault_reg->bit_start;
	reg_field->bit_len = fault_reg->bit_len;
	reg_field->default_val = fault_reg->default_val;
	reg_field->event_id = fault_reg->event_id;
	reg_field->fault_level = fault_reg->fault_level;
	reg_field->reg = reg;
	INIT_LIST_HEAD(&reg_field->group_regs);
	INIT_LIST_HEAD(&reg_field->associate_regs);

	ret = load_group_reg_fields(fault_reg, reg_field);
	if (ret != 0) {
		ras_err("Load group reg fields failed. (module_name=%s; reg_name=%s; ret=%d)\n",
				reg->module->name, reg->name, ret);
	}

	if (fault_reg->associated_regs != NULL) {
		load_associate_regs(fault_reg, reg, reg_field);
	}

	return reg_field;
}

static void local_ras_reg_destroy(struct local_ras_reg *reg)
{
	struct local_ras_reg_field *reg_field = NULL;
	struct local_ras_reg_field *tmp = NULL;

	list_for_each_entry_safe(reg_field, tmp, &reg->fields, entry) {
		list_del(&reg_field->entry);
		local_ras_group_reg_field_destroy(reg_field);
		local_ras_assoc_reg_destroy(reg_field);
		kfree(reg_field);
	}
}

static int load_fault_reg_fields(struct apei_fault_reg *fault_reg, struct local_ras_submodule *module)
{
	u32 i;
	struct local_ras_reg_field *reg_field = NULL;
	struct local_ras_reg *reg = NULL;
	int ret;

	for (i = 0; i < module->reg_num; i++) {
		reg = &module->regs[i];
		if (strcmp(reg->name, fault_reg->name) != 0) {
			continue;
		}
		if (strcmp(module->name, fault_reg->module_name) != 0) {
			continue;
		}

		reg_field = local_ras_reg_field_create(fault_reg, reg);
		if (reg_field == NULL) {
			ras_err("Create reg failed. (reg_name=%s; fault_reg_name=%s)\n", reg->name, fault_reg->name);
			ret = -ENOMEM;
			goto fail;
		}

		list_add_tail(&reg_field->entry, &reg->fields);
	}

	return 0;
fail:
	local_ras_reg_destroy(reg);
	return ret;
}

static struct local_ras_submodule *local_ras_submodule_create(struct apei_submodule *module)
{
	struct local_ras_submodule *tmp_module = NULL;
	struct local_ras_reg *reg = NULL;
	u32 i;

	tmp_module = kzalloc(sizeof(struct local_ras_submodule), GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp_module == NULL) {
		ras_err("Alloc module failed.\n");
		return NULL;
	}

	tmp_module->regs = kcalloc(module->reg_num, sizeof(struct local_ras_reg), GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp_module->regs == NULL) {
		ras_err("Alloc module reg failed.\n");
		kfree(tmp_module);
		return NULL;
	}

	(void)ubfm_strncpy(tmp_module->name, LOCAL_RAS_MODULE_NAME_LEN, module->name, LOCAL_RAS_MODULE_NAME_LEN - 1);
	tmp_module->id = module->id;
	tmp_module->reg_num = module->reg_num;
	for (i = 0; i < tmp_module->reg_num; i++) {
		reg = &tmp_module->regs[i];
		(void)ubfm_strncpy(reg->name, LOCAL_RAS_REG_NAME_LEN, module->regs[i].name, LOCAL_RAS_REG_NAME_LEN - 1);
		reg->reg_idx = i;
		reg->module = tmp_module;

		INIT_LIST_HEAD(&reg->fields);
	}

	return tmp_module;
}

static void local_ras_submodule_destroy(struct local_ras_submodule *module)
{
	u32 i;
	struct local_ras_reg *reg = NULL;

	for (i = 0; i < module->reg_num; i++) {
		reg = &module->regs[i];
		local_ras_reg_destroy(reg);
	}

	kfree(module->regs);
	module->regs = NULL;
	kfree(module);
}

void local_ras_module_destroy(struct local_ras_module *module)
{
	struct local_ras_submodule *pos = NULL;
	struct local_ras_submodule *tmp = NULL;

	if (module == NULL) {
		return;
	}

	list_for_each_entry_safe(pos, tmp, &module->sub_modules, entry) {
		list_del(&pos->entry);
		local_ras_submodule_destroy(pos);
	}
}

int local_ras_module_from_config(struct apei_submodule_table *submod_tbl, struct apei_fault_reg_table *reg_tbl,
								 struct local_ras_module *module)
{
	u32 i, j;
	int ret;

	for (i = 0; i < submod_tbl->num; i++) {
		struct local_ras_submodule *submodule = NULL;

		submodule = local_ras_submodule_create(&submod_tbl->modules[i]);
		if (submodule == NULL) {
			ras_err("Create local ras submodule failed. (idx=%u)\n", i);
			ret = -ENOMEM;
			goto fail;
		}

		list_add_tail(&submodule->entry, &module->sub_modules);

		for (j = 0; j < reg_tbl->num; j++) {
			ret = load_fault_reg_fields(&reg_tbl->regs[j], submodule);
			if (ret != 0) {
				ras_err("Load fault reg failed. (module_name=%s; fault_reg_name=%s; fault_reg_idx=%u)\n",
						submodule->name, reg_tbl->regs[j].name, j);
				goto fail;
			}
		}
	}

	return 0;

fail:
	local_ras_module_destroy(module);
	return ret;
}
