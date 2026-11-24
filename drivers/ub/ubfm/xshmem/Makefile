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

UBFM_OBJ := $(if $(KBUILD_EXTMOD),m,$(CONFIG_UB_UBFM))

ccflags-y += -funsigned-char
ifneq ($(strip $(TOP_DIR)),)
ifneq ($(TARGET_PRODUCT),UB)
ccflags-y += -Wno-error=date-time
ccflags-y += -Wshadow
ccflags-y += -Wformat=2
ccflags-y += -Wextra
ccflags-y += -Wcast-qual
ccflags-y += -Wconversion
ccflags-y += -Wdelete-non-virtual-dtor
ccflags-y += -Wdate-time
ccflags-y += -Wvla
ccflags-y += -Wfloat-equal
ccflags-y += -Wunused
ccflags-y += -Wnon-virtual-dtor
ccflags-y += -Wcast-align
ccflags-y += -Wswitch-default
ccflags-y += -Wstrict-prototypes
ccflags-y += -Wundef
ccflags-y += -Woverloaded-virtual
ccflags-y += -Wall
ccflags-y += -Wtrampolines
ccflags-y += -fstrong-eval-order
ccflags-y += -freg-struct-return
ccflags-y += -fvisibility=hidden
ccflags-y += -pipe
endif
endif

drv_xshmem-$(UBFM_OBJ) := xshmem_fsc.o xshmem_framework.o xshmem_blk.o

obj-$(UBFM_OBJ) += drv_xshmem.o
