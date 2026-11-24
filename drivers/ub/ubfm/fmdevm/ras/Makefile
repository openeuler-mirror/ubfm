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

ras_path := ras
$(info "ras_path" $(ras_path))

EXTRA_CFLAGS += -I$(fmdevm_path)/$(ras_path)
EXTRA_CFLAGS += -I$(fmdevm_path)/$(ras_path)/table
$(info "ras extra cflags: $(EXTRA_CFLAGS)")
mami_fmdevm-objs += $(ras_path)/ras_init.o
mami_fmdevm-objs += $(ras_path)/ras_core.o
mami_fmdevm-objs += $(ras_path)/ras_ctx.o
mami_fmdevm-objs += $(ras_path)/ras_local.o
mami_fmdevm-objs += $(ras_path)/ras_protoc.o
mami_fmdevm-objs += $(ras_path)/ras_sysfs.o
mami_fmdevm-objs += $(ras_path)/table/apei_tbl_load.o
mami_fmdevm-objs += $(ras_path)/table/apei_ub_v100.o
mami_fmdevm-objs += $(ras_path)/table/apei_ub_v160.o
mami_fmdevm-objs += $(ras_path)/table/apei_ucmi_tbl.o
mami_fmdevm-objs += $(ras_path)/table/local_ras_register.o
