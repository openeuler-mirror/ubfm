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
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/msgq				# modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/msgq/core
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/msgq/hw
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/msgq/hw/soc

# source file
mami_mctrlq-objs += msgq/msgq_init.o
mami_mctrlq-objs += msgq/core/msgq_kern.o msgq/core/msgq_cq.o msgq/core/msgq_fops.o msgq/core/msgq_dispatch.o
mami_mctrlq-objs += msgq/core/msgq_proc.o msgq/core/msgq_rq.o msgq/core/msgq_sq.o
mami_mctrlq-objs += msgq/hw/msgq_hw.o
mami_mctrlq-objs += msgq/hw/soc/msgq_soc_v1.o
