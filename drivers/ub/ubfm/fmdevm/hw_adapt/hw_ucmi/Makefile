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

hw_ucmi_path := $(hw_adapt_path)/hw_ucmi
mami_fmdevm-objs += $(hw_ucmi_path)/udev_ucmi_ops.o
mami_fmdevm-objs += $(hw_ucmi_path)/udev_ucmi_espi.o
EXTRA_CFLAGS += -I$(fmdevm_path)/include

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		mami_fmdevm-objs += $(hw_ucmi_path)/udev_ucmi_pcie_espi.o
		mami_fmdevm-objs += $(hw_ucmi_path)/ucmi_dyn_win_mng.o
	endif
endif
