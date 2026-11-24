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
#include "uop_log.h"
#ifdef CFG_UBFM_SUPPORT_LDST
#include "uops_ldst.h"
#endif	/* CFG_UBFM_SUPPORT_LDST */
#ifdef CFG_UBFM_SUPPORT_CDMA
#include "uops_cdma.h"
#include "uops_cdma_symbol.h"
#endif /* CFG_UBFM_SUPPORT_CDMA */
#include "uops_init.h"
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "uops_pcie_ldst.h"
#endif	/* CFG_UBFM_SUPPORT_PCIE_FPGA */
#ifdef CFG_UBFM_SUPPORT_PCIE_I3C
#include "uops_pcie_i3c.h"
#endif	/* CFG_UBFM_SUPPORT_PCIE_I3C */

struct uops_sub_mod {
	const char *name;
	int (*init)(void);
	void (*uninit)(void);
};

static const struct uops_sub_mod g_uops_sub_mods[] = {
#ifdef CFG_UBFM_SUPPORT_LDST
	{ "uops_ldst",      uops_ldst_init,      uops_ldst_uninit      },
#endif	/* CFG_UBFM_SUPPORT_LDST */
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	{ "uops_pcie_ldst", uops_pcie_ldst_init, uops_pcie_ldst_uninit },
#endif	/* CFG_UBFM_SUPPORT_PCIE_FPGA */
#ifdef CFG_UBFM_SUPPORT_PCIE_I3C
	{ "uops_pcie_i3c",  uops_pcie_i3c_init,  uops_pcie_i3c_uninit  },
#endif	/* CFG_UBFM_SUPPORT_PCIE_I3C */
#ifdef CFG_UBFM_SUPPORT_CDMA
	{ "uops_cdma",      uops_cdma_init,      uops_cdma_uninit      },
#endif /* CFG_UBFM_SUPPORT_CDMA */
	{ NULL,             NULL,                NULL                  },
};

int uops_mod_init(void)
{
	int ret = 0;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(g_uops_sub_mods); i++) {
		if (g_uops_sub_mods[i].init == NULL)
			break;
		ret = g_uops_sub_mods[i].init();
		if (unlikely(ret != 0)) {
			uop_err("%s init fail. (ret=%d)\n", g_uops_sub_mods[i].name, ret);
			for (j = i - 1; j >= 0; j--) {
				g_uops_sub_mods[j].uninit();
			}
			return ret;
		}
		uop_info("%s init successfully.\n", g_uops_sub_mods[i].name);
	}
	return 0;
}

void uops_mod_uninit(void)
{
	int i;

	for (i = ARRAY_SIZE(g_uops_sub_mods) - 1; i >= 0; i--) {
		if (g_uops_sub_mods[i].uninit == NULL)
			continue;
		g_uops_sub_mods[i].uninit();
	}
}
