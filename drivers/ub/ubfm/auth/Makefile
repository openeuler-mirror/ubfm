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

drv_auth-$(UBFM_OBJ) += auth_module.o

obj-$(UBFM_OBJ) += drv_auth.o
