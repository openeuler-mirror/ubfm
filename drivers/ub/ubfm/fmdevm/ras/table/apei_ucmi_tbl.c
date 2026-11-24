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

#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"
#include "ras_log.h"
#include "apei_tbl_load.h"
#include "local_ras_register.h"

static struct apei_reg g_apei_ucmi_reg[] = {
	{"UCMI_ABNOR_INTR_RAW"}
};

static struct apei_submodule g_apei_ucmi_module[] = {
	APEI_MODULE_DEF("UCMI", DEFAULT_MODULE, g_apei_ucmi_reg)
};

static struct apei_fault_reg g_apei_ucmi_fault_reg[] = {
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 15, 1, 0, 0x81C38009, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 14, 1, 0, 0x81C38004, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 13, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 12, 1, 0, 0x81C38000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 11, 1, 0, 0x81C38000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 10, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 9, 1, 0, 0x81C38000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 8, 1, 0, 0x81C38009, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"UCMI", "UCMI_ABNOR_INTR_RAW", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()}
};

static struct local_ras_module g_apei_ucmi_module_list;
static struct apei_submodule_table g_apei_ucmi_submodule_tbl = {
	.num = ARRAY_SIZE(g_apei_ucmi_module),
	.modules = g_apei_ucmi_module
};
static struct apei_fault_reg_table g_apei_ucmi_fault_reg_table = {
	.num = ARRAY_SIZE(g_apei_ucmi_fault_reg),
	.regs = g_apei_ucmi_fault_reg
};

static struct apei_module_id g_apei_ucmi_id_map[] = {
	{MAMI_KERN_TYPE_ASCEND_V1, APEI_MODULE_UCMI_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG950_1P, APEI_MODULE_UCMI_KUNPENG},
	{MAMI_KERN_TYPE_UNIONS, APEI_MODULE_UCMI_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG950_2P, APEI_MODULE_UCMI_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG950_4P, APEI_MODULE_UCMI_KUNPENG},
	{MAMI_KERN_TYPE_ASCEND_V2, APEI_MODULE_UCMI_ASCEND},
	{MAMI_KERN_TYPE_UNIONS_210, APEI_MODULE_UCMI_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG960_1P, APEI_MODULE_UCMI_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG960_2P, APEI_MODULE_UCMI_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG960_4P, APEI_MODULE_UCMI_KUNPENG}
};

int apei_ucmi_init(void)
{
	int ret;

	INIT_LIST_HEAD(&g_apei_ucmi_module_list.sub_modules);

	ret = local_ras_module_from_config(&g_apei_ucmi_submodule_tbl, &g_apei_ucmi_fault_reg_table,
									   &g_apei_ucmi_module_list);
	if (ret != 0) {
		ras_err("Load ucmi ras table from config failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = local_ras_module_register(g_apei_ucmi_id_map, ARRAY_SIZE(g_apei_ucmi_id_map), &g_apei_ucmi_module_list);
	if (ret != 0) {
		ras_err("Register ucmi ras module failed. (ret=%d)\n", ret);
		local_ras_module_destroy(&g_apei_ucmi_module_list);
		return ret;
	}

	ras_info("APEI ucmi ras table init success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(apei_ucmi_init, FEATURE_LOADER_STAGE_3);

void apei_ucmi_uninit(void)
{
	local_ras_module_unregister(g_apei_ucmi_id_map, ARRAY_SIZE(g_apei_ucmi_id_map));
	local_ras_module_destroy(&g_apei_ucmi_module_list);
	ras_info("APEI ucmi ras table deinit success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(apei_ucmi_uninit, FEATURE_LOADER_STAGE_3);
