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
EXTRA_CFLAGS += -I$(MAMI_ROOT)/include/uapi/ub/ubfm
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/mng				# modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/hw_adapt/hw_ucmi	            # modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/include			# modules common headers;

# source file
mami_fmdevm-objs += mng/mng_cdev.o mng/mng_handle_ucd.o mng/mng_init.o mng/idm.o mng/feature.o

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
	endif
endif
