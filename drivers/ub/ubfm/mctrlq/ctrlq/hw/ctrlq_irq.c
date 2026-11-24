// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ktime.h>

#include "feature_loader.h"
#include "ubfm_common.h"
#include "uid.h"
#include "ures.h"
#include "uop.h"
#include "udm.h"
#include "mctrlq_log.h"
#include "ctrlq_hw.h"
#include "ctrlq_irq.h"
#include "ctrlq_trace.h"

void ctrlq_intr_pre_handle(u32 devid, enum uid_intr_type type, u64 para)
{
	trace_irq_top_half(devid, para);
	ctrlq_irq_triggered(devid, para);
}

void ctrlq_intr_handle(u32 devid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info)
{
	ctrlq_irq_proc(devid, flag, irq_info.irq_para.val);
}

static struct uid_intr_ops g_ctrlq_intr_ops = {
	/* Since the ctrlq module and the mng dev are the same module, owner must be passed as NULL here */
	.owner = NULL,
	.top_half_pre_handle = ctrlq_intr_pre_handle,
	.handle = ctrlq_intr_handle
};

static struct uid_intr_attr g_ctrlq_intr_attr = {
	.block_flag = 1,
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	.max_sched_time = 1000     /* Periodic task scheduling time, 0 means no periodic task */
#else
	.max_sched_time = 50
#endif
};

int ctrlq_irq_init(void)
{
	int ret;

	ret = uid_register_intr(UID_TYPE_CTRLQ, &g_ctrlq_intr_attr, &g_ctrlq_intr_ops);
	if (ret != 0) {
		mctrlq_err("Register ctrlq failed. (ret=%d)\n", ret);
	}
	mctrlq_info("Register ctrlq uid.\n");
	return ret;
}
DECLARE_FEATURE_AUTO_INIT(ctrlq_irq_init, FEATURE_LOADER_STAGE_9);

void ctrlq_irq_uninit(void)
{
	(void)uid_unregister_intr(UID_TYPE_CTRLQ);
}
DECLARE_FEATURE_AUTO_UNINIT(ctrlq_irq_uninit, FEATURE_LOADER_STAGE_9);
