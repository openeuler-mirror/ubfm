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
hw_probe_path := $(hw_adapt_path)/hw_probe
mami_fmdevm-objs += $(hw_probe_path)/udev_ctrl.o
mami_fmdevm-objs += $(hw_probe_path)/remote/udev_remote_msg_probe.o
mami_fmdevm-objs += $(hw_probe_path)/udev_hw_proc_fs.o
mami_fmdevm-objs += $(hw_probe_path)/udev_msg_pub.o

EXTRA_CFLAGS += -I$(fmdevm_path)/$(hw_probe_path)
EXTRA_CFLAGS += -I$(fmdevm_path)/include

ifeq ($(PRODUCT_SIDE), host)
EXTRA_CFLAGS += -DMAMI_SUPPORT_HOST_CTRLCPU
endif

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
		mami_fmdevm-objs += $(hw_probe_path)/near/udev_pcie_probe.o
	endif
else
	EXTRA_CFLAGS += -DMAMI_ESL_MODE
	mami_fmdevm-objs += $(hw_probe_path)/near/udev_espi_probe.o
endif

ifeq ($(CONFIG_UBFM_ACC_LDST), y)
mami_fmdevm-objs += $(hw_probe_path)/local/udev_local_probe.o
endif

ifdef CONFIG_UB_CDMA
	ifeq ($(CONFIG_UBFM_ACC_CDMA), y)
		EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/uops
		mami_fmdevm-objs += $(hw_probe_path)/udev_cdma_pub.o
		mami_fmdevm-objs += $(hw_probe_path)/remote/udev_remote_probe.o
	endif
endif
