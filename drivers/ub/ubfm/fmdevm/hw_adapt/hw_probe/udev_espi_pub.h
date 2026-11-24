/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef _UDEV_ESPI_PUB_H
#define _UDEV_ESPI_PUB_H

#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include "udev_hw_pub.h"
#include "fmdevm.h"
#include "udev_espi_def.h"

#ifdef MAMI_SUPPORT_HOST_CTRLCPU
#define UCMI_BASE 0x3080000000UL
#else
#define UCMI_BASE 0x20080000000UL
#endif

struct udev_espi {
	struct device *dev;
	void __iomem *reg_base;	 /* espi register base addr */
	void __iomem *remote_reg_base;
	u32 remote_ucmi_axi;
	u64 space_base;			 /* espi access space base addr */
	u64 remote_regbank_base;		   /* espi register base addr */
	u32 die_id;
	struct udev_hw_identify hw;
	struct udev_cfg cfg;
	u32 win_val_4m[UCMI_4M_WIN_NUM];
	u32 win_val_16m[UCMI_16M_WIN_NUM];
	u32 aix_remap_val[UCMI_SLAVE_AXI_NUM];
};

static inline u32 udev_espi_readl(struct udev_espi *espi, int offset)
{
	return readl(espi->reg_base + offset);
}

static inline void udev_espi_writel(struct udev_espi *espi, int offset, u32 val)
{
	writel(val, espi->reg_base + offset);
}

void udev_espi_setup_ascend_v1(struct udev_espi *espi);
void udev_espi_setup_unions(struct udev_espi *espi);
void udev_espi_setup_chip_kunpeng950_1p(struct udev_espi *espi);
int udev_espi_probe(struct platform_device *pdev);
int udev_espi_remove(struct platform_device *pdev);
int udev_espi_specify_chip_type(u32 chip_type);
#endif
