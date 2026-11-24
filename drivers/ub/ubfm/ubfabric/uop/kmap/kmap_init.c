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
#include <linux/compiler.h>
#ifdef CFG_UBFM_SUPPORT_LDST
#include "kmap_ldst.h"
#endif	/* CFG_UBFM_SUPPORT_LDST */
#ifdef CFG_UBFM_SUPPORT_CDMA
#include "kmap_cdma.h"
#endif	/* CFG_UBFM_SUPPORT_CDMA */
#include "kmap_kern.h"
#include "kmap_init.h"
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "kmap_pcie_ldst.h"
#endif	/* CFG_UBFM_SUPPORT_PCIE_FPGA */
#ifdef CFG_UBFM_SUPPORT_PCIE_I3C
#include "kmap_pcie_i3c.h"
#endif	/* CFG_UBFM_SUPPORT_PCIE_I3C */
#include "uop_log.h"

struct kmap_sub_mod {
	const char *name;
	int (*init)(void);
	void (*uninit)(void);
};

static const struct kmap_sub_mod g_kmap_sub_mods[] = {
#ifdef CFG_UBFM_SUPPORT_LDST
	{ "kmap_ldst",      kmap_ldst_init,      kmap_ldst_uninit      },
#endif	/* CFG_UBFM_SUPPORT_LDST */
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	{ "kmap_pcie_ldst", kmap_pcie_ldst_init, kmap_pcie_ldst_uninit },
#endif	/* CFG_UBFM_SUPPORT_PCIE_FPGA */
#ifdef CFG_UBFM_SUPPORT_PCIE_I3C
	{ "kmap_pcie_i3c",  kmap_pcie_i3c_init,  kmap_pcie_i3c_uninit  },
#endif	/* CFG_UBFM_SUPPORT_PCIE_I3C */
#ifdef CFG_UBFM_SUPPORT_CDMA
	{ "kmap_cdma",      kmap_cdma_init,      kmap_cdma_uninit      },
#endif	/* CFG_UBFM_SUPPORT_CDMA */
	{ NULL,             NULL,                NULL                  },
};

int kmap_mod_init(void)
{
	int ret = 0;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(g_kmap_sub_mods); i++) {
		if (g_kmap_sub_mods[i].init == NULL)
			break;
		ret = g_kmap_sub_mods[i].init();
		if (unlikely(ret != 0)) {
			uop_err("%s init fail. (ret=%d)\n", g_kmap_sub_mods[i].name, ret);
			for (j = i - 1; j >= 0; j--) {
				g_kmap_sub_mods[j].uninit();
			}
			return ret;
		}
		uop_info("%s init successfully.\n", g_kmap_sub_mods[i].name);
	}
	return 0;
}

void kmap_mod_uninit(void)
{
	int i;

	for (i = ARRAY_SIZE(g_kmap_sub_mods) - 1; i >= 0; i--) {
		if (g_kmap_sub_mods[i].uninit == NULL)
			continue;
		g_kmap_sub_mods[i].uninit();
	}
}
