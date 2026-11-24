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

ifeq ($(FPGA_MODE), true)
EXTRA_CFLAGS += -DFPGA_MODE_ENABLE
endif

hw_info_path := $(hw_adapt_path)/hw_info
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info_ascend_v1.o
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info_unions.o
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info_kunpeng950.o
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info_1825.o
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info_hssu_v100.o
mami_fmdevm-objs += $(hw_info_path)/udev_hw_info.o

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
	endif
else
	EXTRA_CFLAGS += -DMAMI_ESL_MODE
endif

EXTRA_CFLAGS += -I$(fmdevm_path)/$(hw_info_path)
