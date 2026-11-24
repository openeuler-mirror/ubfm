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

linkcom_path := linkcom
$(info "linkcom_path" $(linkcom_path))

# include
EXTRA_CFLAGS += -I$(MAMI_ROOT)/include/uapi/ub/ubfm                     # kernel include;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/include                  # kernel public;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/hw_adapt/include

EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom # modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/mgmtq
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/mng
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/core
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/soc
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/soc/udie_v1
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/linkcom/soc/udie_v2

# source file
mami_linkcom-objs += linkcom_init.o
mami_linkcom-objs += linkcom_handle_ucd.o
mami_linkcom-objs += linkcom_pkt_parse.o
mami_linkcom-objs += mgmtq/linkcom_mgmtq_irq.o
mami_linkcom-objs += mng/linkcom_mng.o
mami_linkcom-objs += core/linkcom_core.o
mami_linkcom-objs += soc/linkcom_soc_adapt.o
mami_linkcom-objs += soc/linkcom_soc_reg.o
mami_linkcom-objs += soc/udie_v1/udie_v1_linkcom_soc.o
mami_linkcom-objs += soc/udie_v2/udie_v2_linkcom_soc.o
mami_linkcom-objs += soc/mgmtq_proc.o

# compile options
EXTRA_CFLAGS += -Werror -Wfloat-equal -Wextra

EXTRA_CFLAGS += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
# stdarg.h
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

ifneq ($(ESL_MODE), true)
	ifeq ($(CONFIG_UBFM_ACC_PCIE_FPGA), y)
		EXTRA_CFLAGS += -DCFG_UBFM_SUPPORT_PCIE_FPGA
	endif
endif

# module name;
obj-$(UBFM_OBJ) += mami_linkcom.o
# module object;
mami_linkcom-objs += linkcom_module.o
