# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

# include
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop				# modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/core
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/kmap
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/umap
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/uops

# source file
mami_ubfabric-objs += uop/uop_init.o uop/uop_fops.o
mami_ubfabric-objs += uop/core/uop_kern.o uop/core/uop_proc.o uop/core/uop_core_init.o
mami_ubfabric-objs += uop/kmap/kmap_kern.o uop/kmap/kmap_ldst.o uop/kmap/kmap_init.o uop/kmap/kmap_pcie_ldst.o uop/kmap/kmap_pcie_i3c.o
mami_ubfabric-objs += uop/umap/umap_kern.o uop/umap/umap_init.o
mami_ubfabric-objs += uop/uops/uops_kern.o uop/uops/uops_ldst.o uop/uops/uops_init.o uop/uops/uops_pcie_ldst.o uop/uops/uops_pcie_i3c.o
mami_ubfabric-objs += uop/fpga_adapter/fpga_back_kern.o
mami_ubfabric-objs += uop/fpga_adapter/fpga_ops.o
mami_ubfabric-objs += uop/fpga_adapter/fpga_reg_ops.o
mami_ubfabric-objs += uop/fpga_adapter/fpga_api_ops.o
mami_ubfabric-objs += uop/i3c_adapter/i3c_kern.o

ifeq ($(CONFIG_UBFM_ACC_LDST), y)
EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_LDST
endif

ifdef CONFIG_UB_CDMA
	ifeq ($(CONFIG_UBFM_ACC_CDMA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_CDMA
		mami_ubfabric-objs += uop/uops/uops_cdma.o uop/uops/uops_cdma_symbol.o
		mami_ubfabric-objs += uop/kmap/kmap_cdma.o
	endif
endif

ifeq ($(FPGA_MODE), true)
EXTRA_CFLAGS += -DFPGA_MODE_ENABLE
endif

ifeq ($(ESL_MODE), true)
	EXTRA_CFLAGS += -DMAMI_ESL_MODE
else
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
		ifeq ($(CONFIG_UBFM_ACC_PCIE_I3C), y)
			EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_I3C
		endif
	endif
endif
