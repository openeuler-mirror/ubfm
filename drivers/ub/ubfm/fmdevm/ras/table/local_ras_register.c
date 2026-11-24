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
#include <linux/mm.h>

#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"
#include "udm.h"
#include "ras_log.h"
#include "local_ras_register.h"

static struct list_head g_local_ras_module_tbl[MAMI_KERN_TYPE_MAX] = {0};

STATIC int local_ras_module_register_inner(u32 chip_type, u32 module_id, struct local_ras_module *module)
{
	struct local_ras_module_table *pos = NULL;

	if (chip_type >= MAMI_KERN_TYPE_MAX) {
		ras_err("Invalid para. (chip_type=%u; module_id=%u)\n", chip_type, module_id);
		return -EINVAL;
	}

	list_for_each_entry(pos, &g_local_ras_module_tbl[chip_type], entry) {
		if (pos->module_id == module_id) {
			return -EEXIST;
		}
	}

	pos = kvmalloc(sizeof(struct local_ras_module_table), GFP_KERNEL_ACCOUNT);
	if (pos == NULL) {
		ras_err("Alloc ras module table failed. (chip_type=%u; module_id=%u)\n", chip_type, module_id);
		return -ENOMEM;
	}

	pos->module_id = module_id;
	pos->module = module;
	list_add_tail(&pos->entry, &g_local_ras_module_tbl[chip_type]);
	return 0;
}

STATIC void local_ras_module_unregister_inner(u32 chip_type, u32 module_id)
{
	struct local_ras_module_table *pos = NULL;
	struct local_ras_module_table *tmp = NULL;

	if (chip_type >= MAMI_KERN_TYPE_MAX) {
		ras_err("Invalid para. (chip_type=%u; module_id=%u)\n", chip_type, module_id);
		return;
	}

	list_for_each_entry_safe(pos, tmp, &g_local_ras_module_tbl[chip_type], entry) {
		if (pos->module_id == module_id) {
			list_del(&pos->entry);
			kvfree(pos);
			break;
		}
	}
}

int local_ras_module_register(struct apei_module_id *id_map, u32 map_num, struct local_ras_module *module)
{
	u32 i, j;
	int ret;

	for (i = 0; i < map_num; i++) {
		ret = local_ras_module_register_inner(id_map[i].chip_type, id_map[i].id, module);
		if (ret != 0) {
			ras_err("Register module failed. (idx=%u; chip_type=%u; id=%u; ret=%d)\n",
					i, id_map[i].chip_type, id_map[i].id, ret);
			goto fail;
		}
	}

	return 0;

fail:
	for (j = 0; j < i; j++) {
		local_ras_module_unregister_inner(id_map[j].chip_type, id_map[j].id);
	}
	return ret;
}

void local_ras_module_unregister(struct apei_module_id *id_map, u32 map_num)
{
	u32 i;

	for (i = 0; i < map_num; i++) {
		local_ras_module_unregister_inner(id_map[i].chip_type, id_map[i].id);
	}
}

int local_ras_submodule_get(u32 devid, u32 module_id, u32 submod_id, struct local_ras_submodule **submodule)
{
	struct local_ras_module_table *pos = NULL;
	struct local_ras_submodule *submod = NULL;
	struct udm_dev_para dev_para;
	int ret;

	ret = udm_get_dev_attr(devid, &dev_para);
	if (ret != 0) {
		ras_err("Get dev attr failed. (devid=%u)\n", devid);
		return ret;
	}

	if (dev_para.chip_type >= MAMI_KERN_TYPE_MAX) {
		ras_err("Invalid para. (devid=%u; chip_type=%u; module_id=%u; submodule_id=%u)\n",
				devid, dev_para.chip_type, module_id, submod_id);
		return -EINVAL;
	}

	list_for_each_entry(pos, &g_local_ras_module_tbl[dev_para.chip_type], entry) {
		if (pos->module_id != module_id) {
			continue;
		}

		list_for_each_entry(submod, &pos->module->sub_modules, entry) {
			if (submod->id == submod_id || submod->id == DEFAULT_MODULE) {
				*submodule = submod;
				return 0;
			}
		}

		return -ENODATA;
	}

	return -ENODATA;
}

static void local_ras_module_table_destroy(struct list_head *tbl)
{
	struct local_ras_module_table *pos = NULL;
	struct local_ras_module_table *tmp = NULL;

	list_for_each_entry_safe(pos, tmp, tbl, entry) {
		list_del(&pos->entry);
		kvfree(pos);
	}
}

int local_ras_register_init(void)
{
	u32 i;

	for (i = 0; i < MAMI_KERN_TYPE_MAX; i++) {
		INIT_LIST_HEAD(&g_local_ras_module_tbl[i]);
	}

	ras_info("Local ras module table init success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(local_ras_register_init, FEATURE_LOADER_STAGE_1);

void local_ras_register_uninit(void)
{
	u32 i;

	for (i = 0; i < MAMI_KERN_TYPE_MAX; i++) {
		local_ras_module_table_destroy(&g_local_ras_module_tbl[i]);
	}
	ras_info("Local ras module table uninit success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(local_ras_register_uninit, FEATURE_LOADER_STAGE_1);
