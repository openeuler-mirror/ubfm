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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kallsyms.h>
#include "mami_string.h"
#include "uop_log.h"
#include "uops_kern.h"
#include "uop.h"
#include "fmdevm.h"
#include "i3c_kern.h"

int g_i3c_symbol_init;
static i3c_drv_ubnic_probe_func g_i3c_drv_ubnic_probe;
static i3c_drv_ubnic_online_status_func g_i3c_drv_ubnic_online_status;
static i3c_drv_ubnic_xfer_func g_i3c_drv_ubnic_xfer;

unsigned int drv_ubnic_probe_stub(unsigned int inst_id, unsigned int chip_id)
{
	return I3C_STATUS_INVAL;
}

unsigned int drv_ubnic_xfer_stub(unsigned int inst_id, unsigned int chip_id, drv_ubnic_xfer_msg *msg)
{
	return I3C_STATUS_INVAL;
}

unsigned int drv_ubnic_online_status_stub(unsigned int inst_id, unsigned int chip_id, unsigned int *status)
{
	return I3C_STATUS_INVAL;
}

int i3c_get_kern_symbol(void **func, const char *name)
{
	*func = __symbol_get(name);
	if (*func == NULL) {
		return I3C_STATUS_INVAL;
	}
	return 0;
}

static inline int init_symbol(void **var, void *stub, const char *name)
{
	if (i3c_get_kern_symbol(var, name) != 0) {
		*var = stub;
		return I3C_STATUS_INVAL;
	}
	return 0;
}

int i3c_symbol_init(void)
{
	int ret = 0;

	if (g_i3c_symbol_init == 1) {
		return 0;
	}
	ret |= init_symbol((void **)&g_i3c_drv_ubnic_probe, drv_ubnic_probe_stub, "drv_ubnic_probe");
	ret |= init_symbol((void **)&g_i3c_drv_ubnic_online_status, drv_ubnic_online_status_stub, "drv_ubnic_online_status");
	ret |= init_symbol((void **)&g_i3c_drv_ubnic_xfer, drv_ubnic_xfer_stub, "drv_ubnic_xfer");
	uop_info("i3c symbol init ret=%d.\n", ret);
	g_i3c_symbol_init = 1;
	return ret;
}
EXPORT_SYMBOL_GPL(i3c_symbol_init);

void i3c_symbol_uninit(void)
{
	if (g_i3c_symbol_init == 0) {
		return;
	}
	g_i3c_symbol_init = 0;
	(g_i3c_drv_ubnic_probe != drv_ubnic_probe_stub) ? __symbol_put("drv_ubnic_probe") : NULL;
	(g_i3c_drv_ubnic_online_status != drv_ubnic_online_status_stub) ? __symbol_put("drv_ubnic_online_status") : NULL;
	(g_i3c_drv_ubnic_xfer != drv_ubnic_xfer_stub) ? __symbol_put("drv_ubnic_xfer") : NULL;
	g_i3c_drv_ubnic_probe = NULL;
	g_i3c_drv_ubnic_online_status = NULL;
	g_i3c_drv_ubnic_xfer = NULL;
	uop_info("i3c symbol uninit.\n");
}
EXPORT_SYMBOL_GPL(i3c_symbol_uninit);

unsigned int i3c_drv_ubnic_probe(unsigned int inst_id, unsigned int chip_id)
{
	if (g_i3c_drv_ubnic_probe == NULL) {
		return I3C_STATUS_INVAL;
	}
	return g_i3c_drv_ubnic_probe(inst_id, chip_id);
}
EXPORT_SYMBOL_GPL(i3c_drv_ubnic_probe);

unsigned int i3c_drv_ubnic_xfer(unsigned int inst_id, unsigned int chip_id, drv_ubnic_xfer_msg *msg)
{
	if (g_i3c_drv_ubnic_xfer == NULL) {
		return I3C_STATUS_INVAL;
	}
	if (msg == NULL) {
		return -EINVAL;
	}
	return g_i3c_drv_ubnic_xfer(inst_id, chip_id, msg);
}
EXPORT_SYMBOL_GPL(i3c_drv_ubnic_xfer);

unsigned int i3c_drv_ubnic_online_status(unsigned int inst_id, unsigned int chip_id, unsigned int *status)
{
	if (g_i3c_drv_ubnic_online_status == NULL) {
		return I3C_STATUS_INVAL;
	}
	if (status == NULL) {
		return -EINVAL;
	}
	return g_i3c_drv_ubnic_online_status(inst_id, chip_id, status);
}
EXPORT_SYMBOL_GPL(i3c_drv_ubnic_online_status);
