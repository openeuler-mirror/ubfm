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
#ifndef MSI_IRQ_H
#define MSI_IRQ_H

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/pci.h>

#include "ubfm_common.h"
#include "fpga_adapter_kern.h"

#define UDEV_MSI_IRQ_VECTOR_NUM			 16
#define MSI_IRQ_NAME_LEN					32

struct pci_msi_data;
struct msi_irq_data {
	char name[MSI_IRQ_NAME_LEN];
	int irq;
	u32 devid;
	u32 vector_idx;
	u32 irq_idx;	/* one interrupt number per DIE */
	struct pci_msi_data *msi_data;
};

struct pci_msi_data {
	u32 devnum;
	u32 irqnum;
	const struct fpga_ops *fpga_ops;
	struct pci_dev *dev;
	struct msi_irq_data irq_data[UDEV_MSI_IRQ_VECTOR_NUM];
	struct mutex mtx_lock;
	struct list_head entry;
};

#endif /* End of MSI_IRQ_H */
