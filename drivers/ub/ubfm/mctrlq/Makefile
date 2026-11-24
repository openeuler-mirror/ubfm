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

ifeq ($(strip $(MAMI_ROOT)),)
  ifneq ($(strip $(TOP_DIR)),)
    MAMI_ROOT := $(TOP_DIR)/ubengine/mami/kernel
  else ifneq ($(KBUILD_EXTMOD),)
    MAMI_ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../../../..)
  else
    MAMI_ROOT := $(srctree)
  endif
endif

UBFM_OBJ := $(if $(KBUILD_EXTMOD),m,$(CONFIG_UB_UBFM))

ifneq ($(KBUILD_EXTMOD),)
KBUILD_EXTRA_SYMBOLS := $(wildcard $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/Module.symvers)
KBUILD_EXTRA_SYMBOLS += $(wildcard $(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/Module.symvers)
endif

# include;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/include/uapi/ub/ubfm                      # kernel include;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/include/                  # kernel public;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq

# inner module;
include $(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq/ctrlq.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/msgq/msgq.mk

ifeq ($(CONFIG_UBFM_DEBUG), y)
EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
	endif
endif

ifeq ($(ESL_MODE), true)
	EXTRA_CFLAGS += -DMAMI_ESL_MODE
endif

ifeq ($(FPGA_MODE), true)
	EXTRA_CFLAGS += -DFPGA_MODE_ENABLE
endif

EXTRA_CFLAGS += -Werror -Wfloat-equal -Wextra

EXTRA_CFLAGS += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
# stdarg.h
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

# module name;
obj-$(UBFM_OBJ) += mami_mctrlq.o
mami_mctrlq-objs += mctrlq_init.o
