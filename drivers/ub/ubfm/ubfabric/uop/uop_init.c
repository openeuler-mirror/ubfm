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
#include "feature_loader.h"

#include "udm.h"
#include "ures.h"

#include "ubfabric.h"
#include "uop_log.h"
#include "uop_fops.h"
#include "uop_core_init.h"
#include "kmap_init.h"
#include "umap_init.h"
#include "uops_init.h"
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "fpga_adapter_kern.h"
#endif

static inline int uop_ures_get_acc_type(u32 devid, enum uop_acc_type *type)
{
	u64 acc_type = UOP_ACC_MAX;
	int ret;

	ret = ures_get_key_value(devid, MAMI_ACC_TYPE, &acc_type);
	*type = (u32)acc_type; /* Read it from ures later */
	return ret;
}

static int uop_udm_dev_init(u32 devid)
{
	enum uop_acc_type type;
	int ret;

	ret = uop_ures_get_acc_type(devid, &type);
	if (unlikely(ret != 0)) {
		uop_err("Get acc type fail. (devid=%u)\n", devid);
		return ret;
	}

	ret = uops_kern_init(devid, type);
	if (unlikely(ret != 0)) {
		uop_err("Uops kern init fail. (ret=%d; type=%u)\n", ret, type);
		return ret;
	}

#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	ret = ucmi_starry_init(devid);
	if (unlikely(ret != 0)) {
		uops_kern_uninit(devid);
		uop_err("Ucmi starry init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
#endif

	return ret;
}

static void uop_udm_dev_uninit(u32 devid)
{
	uops_kern_uninit(devid);
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	ucmi_starry_uninit(devid);
#endif
}

static int uop_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = uop_udm_dev_init(devid);
	} else if (action == UDM_UNINIT) {
		uop_udm_dev_uninit(devid);
	}

	return ret;
}

static int uop_udm_mod_init(void)
{
	return udm_notifier_register(UOP_MODULE_NAME, UDM_PRI1, uop_udm_notify);
}

static void uop_udm_mod_uninit(void)
{
	(void)udm_notifier_unregister(UOP_MODULE_NAME);
}

typedef int (*uop_mod_init_func)(void);
typedef void (*uop_mod_uninit_func)(void);

static const uop_mod_init_func g_mod_init[] = {
	kmap_mod_init,
	uop_core_mod_init,
	umap_mod_init,
	uops_mod_init,
	uop_udm_mod_init,
	uop_fops_mod_init,
};

static const uop_mod_uninit_func g_mod_uninit[] = {
	kmap_mod_uninit,
	uop_core_mod_uninit,
	umap_mod_uninit,
	uops_mod_uninit,
	uop_udm_mod_uninit,
	uop_fops_mod_uninit,
};

int uop_mod_init(void)
{
	int ret = 0;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(g_mod_init); i++) {
		ret = g_mod_init[i]();
		if (ret != 0) {
			goto out;
		}
	}
	return 0;
out:
	for (j = i - 1; j >= 0; j--) {
		g_mod_uninit[j]();
	}
	return ret;
}
DECLARE_FEATURE_AUTO_INIT(uop_mod_init, FEATURE_LOADER_STAGE_4);

void uop_mod_uninit(void)
{
	int i;

	for (i = ARRAY_SIZE(g_mod_uninit) - 1; i >= 0; i--) {
		g_mod_uninit[i]();
	}
}
DECLARE_FEATURE_AUTO_UNINIT(uop_mod_uninit, FEATURE_LOADER_STAGE_4);

