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

irqmgr_path := irqmgr
$(info "irqmgr_path" $(irqmgr_path))

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -I$(fmdevm_path)/$(irqmgr_path)/msi
		mami_fmdevm-objs += $(irqmgr_path)/msi/msi_init.o
	endif
endif

ifneq ($(filter y,$(CONFIG_UBFM_ACC_LDST) $(CONFIG_UBFM_ACC_CDMA)),)
EXTRA_CFLAGS += -I$(fmdevm_path)/$(irqmgr_path)/usi
mami_fmdevm-objs += $(irqmgr_path)/usi/usi_init.o
mami_fmdevm-objs += $(irqmgr_path)/usi/usi_acpi.o
mami_fmdevm-objs += $(irqmgr_path)/usi/usi_ubus_api.o
endif
