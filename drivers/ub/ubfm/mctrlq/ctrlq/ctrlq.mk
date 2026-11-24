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
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq				# modules inner headers;
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq/core
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq/hw
EXTRA_CFLAGS += -I$(MAMI_ROOT)/drivers/ub/ubfm/mctrlq/ctrlq/hw/chip

# source file
mami_mctrlq-objs += ctrlq/ctrlq_init.o
mami_mctrlq-objs += ctrlq/core/ctrlq_kern.o ctrlq/core/ctrlq_proc.o ctrlq/core/ctrlq_fops.o ctrlq/core/ctrlq_debug.o
mami_mctrlq-objs += ctrlq/hw/ctrlq_hw.o ctrlq/hw/ctrlq_irq.o
mami_mctrlq-objs += ctrlq/hw/chip/ctrlq_chip.o
