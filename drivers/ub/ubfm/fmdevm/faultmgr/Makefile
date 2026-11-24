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

faultmgr_path := faultmgr
$(info "faultmgr_path" $(faultmgr_path))

mami_fmdevm-objs += $(faultmgr_path)/faultmgr_init.o
mami_fmdevm-objs += $(faultmgr_path)/faultmgr_core.o
mami_fmdevm-objs += $(faultmgr_path)/faultmgr_queue.o
mami_fmdevm-objs += $(faultmgr_path)/faultmgr_sysfs.o
EXTRA_CFLAGS += -I$(fmdevm_path)/$(faultmgr_path)/include
