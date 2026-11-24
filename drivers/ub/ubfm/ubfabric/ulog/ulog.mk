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

# include
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/ubfabric/ulog

# source file
mami_ubfabric-objs += ulog/ulog_core.o
mami_ubfabric-objs += ulog/ulog_proc_fs.o
mami_ubfabric-objs += ulog/ulog_handle.o
