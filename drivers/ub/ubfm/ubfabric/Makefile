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

# include;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/include/uapi/ub/ubfm                     # kernel include;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/include                  # kernel public;

# inner module;
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/ucd/ucd.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/udm/udm.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/ued/ued.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uid/uid.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/uop/uop.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/ures/ures.mk
include $(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/ulog/ulog.mk

# compile options
EXTRA_CFLAGS += -Werror -Wfloat-equal -Wextra

EXTRA_CFLAGS += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body
# stdarg.h
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

# module name;
obj-$(UBFM_OBJ) += mami_ubfabric.o
# module object;
mami_ubfabric-objs += ubfabric_module.o
