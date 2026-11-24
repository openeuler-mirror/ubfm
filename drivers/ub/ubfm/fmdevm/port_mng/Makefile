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

port_mng_path := port_mng
$(info "port_mng_path" $(port_mng_path))

# include
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/include
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/port_mng
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/fmdevm/port_mng/port_info

# source file
mami_fmdevm-objs += port_mng/port_info/ppp_tbl_cfg.o
mami_fmdevm-objs += port_mng/port_info/pa_tbl_cfg.o
mami_fmdevm-objs += port_mng/port_info/port_info.o
mami_fmdevm-objs += port_mng/port_init.o
