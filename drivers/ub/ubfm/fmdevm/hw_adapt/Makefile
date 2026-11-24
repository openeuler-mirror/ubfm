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

hw_adapt_path := hw_adapt
$(info "hw_adapt_path" $(hw_adapt_path))

ifeq ($(FPGA_MODE), true)
EXTRA_CFLAGS += -DFPGA_MODE_ENABLE
endif

mami_fmdevm-objs += $(hw_adapt_path)/udev_main.o $(hw_adapt_path)/udev_feature.o
EXTRA_CFLAGS += -I$(fmdevm_path)/$(hw_adapt_path)/include

include $(fmdevm_path)/$(hw_adapt_path)/hw_probe/Makefile
include $(fmdevm_path)/$(hw_adapt_path)/hw_info/Makefile
include $(fmdevm_path)/$(hw_adapt_path)/hw_map/Makefile
include $(fmdevm_path)/$(hw_adapt_path)/hw_ucmi/Makefile

ifdef CONFIG_UB_CDMA
	ifeq ($(CONFIG_UBFM_ACC_CDMA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_CDMA
	endif
endif
