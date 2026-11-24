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
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/delay.h>

#include "fmdevm.h"
#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"
#include "udm.h"
#include "ures.h"
#include "udev_hw_pub.h"
#include "udev_ctrl.h"
#include "udev_espi_pub.h"
#include "udev_ucmi_ops.h"
#include "mng_devid.h"

#define UDEV_HW_INFO_REG_OFFSET 0x200FF000
#define UDEV_HW_INFO_REG_OFFSET_KUNPENG960 0x8B00000
#define UDEV_HW_INFO_REG_SIZE SIZE_KB(4)
#define UCMI_REGBANK_SIZE SIZE_KB(64)

static u32 g_chip_type;

static inline u64 udev_espi_get_ucmi_base(u32 chip_type)
{
	if (chip_type == MAMI_KERN_TYPE_KUNPENG950_1P || chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) {
		return UCMI_REGBANK_BASE_KUNPENG950;
	}
	if (chip_type == MAMI_KERN_TYPE_KUNPENG950_2P || chip_type == MAMI_KERN_TYPE_KUNPENG960_2P) {
		return UCMI_REGBANK_BASE_KUNPENG950;
	}
	if (chip_type == MAMI_KERN_TYPE_KUNPENG950_4P || chip_type == MAMI_KERN_TYPE_KUNPENG960_4P) {
		return UCMI_REGBANK_BASE_KUNPENG950;
	}
	return UCMI_REGBANK_BASE_ASCEND_V1_UNIONS;
}

static int udev_espi_master_init(struct udev_espi *espi)
{
	espi->space_base = UCMI_BASE;
#ifdef MAMI_SUPPORT_HOST_CTRLCPU
	espi->reg_base = ioremap(UCMI_REGBANK_BASE_KUNPENG950, UCMI_REGBANK_SIZE);
#else
	espi->reg_base = ioremap(UCMI_REGBANK_BASE_ASCEND_V1_UNIONS, UCMI_REGBANK_SIZE);
#endif
	if (espi->reg_base == NULL) {
		return -ENOMEM;
	}

	return ucmi_master_setup(UDEV_ESPI);
}

static void udev_espi_master_uninit(struct udev_espi *espi)
{
	if (espi->reg_base != NULL) {
		iounmap(espi->reg_base);
		espi->reg_base = NULL;
	}
}

static struct udev_espi *udev_alloc_espi(struct device *dev)
{
	struct udev_espi *espi;

	espi = kzalloc(sizeof(struct udev_espi), GFP_KERNEL);
	if (espi == NULL) {
		hw_err("kzalloc espi failed.\n");
		return NULL;
	}
	espi->dev = dev;
	return espi;
}

void udev_free_espi(struct udev_espi *espi)
{
	kfree(espi);
}

STATIC int udev_espi_get_hw_info(struct udev_espi *espi, u32 *chip_type)
{
	int ret;
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct udev_ucmi_win *ucmi_win = NULL;
	struct udev_ucmi_addr addr = {
		.name = DEV_INFO_WIN_NAME,
		.devid = 0,
		.dieid = 0,
		.size = UDEV_HW_INFO_REG_SIZE,
		.chip_addr = UDEV_HW_INFO_REG_OFFSET
	};

	if (*chip_type >= MAMI_KERN_TYPE_KUNPENG960_1P && *chip_type <= MAMI_KERN_TYPE_KUNPENG960_4P) {
		addr.chip_addr = UDEV_HW_INFO_REG_OFFSET_KUNPENG960;
	}

	/* hw_info uses the axi address window 0 by default, configured by bios, no need to configure the slave's axi register */
	ret = ucmi_assign_win_by_axi(UDEV_ESPI, 0, &addr);
	if (ret != 0) {
		hw_err("Set axi failed. (ret=%d)\n", ret);
		return ret;
	}

	addr_info = ucmi_get_addr_info(UDEV_ESPI, addr.devid, addr.chip_addr, addr.name);
	if (addr_info == NULL) {
		hw_err("Get addr info failed. (devid=%u)\n", addr.devid);
		return -EINVAL;
	}
	if (list_empty(&addr_info->win_list)) {
		hw_err("win list is empty. (devid=%u)\n", addr.devid);
		ucmi_put_addr_info(UDEV_ESPI, addr.devid);
		return -EINVAL;
	}
	ucmi_win = list_first_entry(&addr_info->win_list, struct udev_ucmi_win, entry);
	hw_info("Debug: remote addr = %#llx, size=%u\n", ucmi_win->ucmi_off + UCMI_BASE, addr.size);
	ret = udev_get_hw_info(MAMI_ANY_DEVID, ucmi_win->ucmi_off + UCMI_BASE, addr.size, &espi->hw);
	ucmi_put_addr_info(UDEV_ESPI, addr.devid);
	if (ret != 0) {
		hw_err("Get hw info failed. (ret=%d)\n", ret);
		return ret;
	}

	*chip_type = (u32)espi->hw.chip_type;

	if (*chip_type >= MAMI_KERN_TYPE_MAX || *chip_type <= MAMI_KERN_TYPE_INVALID) {
		hw_err("Chip type is invalid. (chip_type=%u)\n", *chip_type);
		return -EINVAL;
	}
	espi->hw.access_type = UDEV_LDST;
	espi->hw.location = UDM_NEAR;
	espi->hw.connect_type = UDEV_ESPI;
	return 0;
}

STATIC int udev_espi_map_remote_reg(struct udev_espi *espi)
{
	struct udev_ucmi_addr addr = {
		.name = UCMI_WIN_NAME,
		.devid = 0,
		.dieid = 0,
		.size = UCMI_REGBANK_SIZE,
	};
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct udev_ucmi_win *ucmi_win = NULL;
	void __iomem *base;
	int ret;

	espi->remote_regbank_base = udev_espi_get_ucmi_base(espi->hw.chip_type);
	addr.chip_addr = espi->remote_regbank_base;

	/* ucmi uses the axi address window 1 by default, configured by bios, no need to configure the slave's axi register */
	ret = ucmi_assign_win_by_axi(UDEV_ESPI, 1, &addr);
	if (ret != 0) {
		hw_err("Assign win by axi failed. (ret=%d)\n", ret);
		return ret;
	}

	addr_info = ucmi_get_addr_info(UDEV_ESPI, addr.devid, addr.chip_addr, addr.name);
	if (addr_info == NULL) {
		hw_err("Get addr info failed. (devid=%u)\n", addr.devid);
		return -EINVAL;
	}
	if (list_empty(&addr_info->win_list)) {
		hw_err("win list is empty. (devid=%u)\n", addr.devid);
		ucmi_put_addr_info(UDEV_ESPI, addr.devid);
		return -EINVAL;
	}
	ucmi_win = list_first_entry(&addr_info->win_list, struct udev_ucmi_win, entry);
	base = ioremap(ucmi_win->ucmi_off + UCMI_BASE, addr.size);
	ucmi_put_addr_info(UDEV_ESPI, addr.devid);
	if (base == NULL) {
		hw_err("Ioremap remote espi failed.\n");
		return -ENOMEM;
	}
	espi->remote_reg_base = base;
	/* agreed with BIOS: ucmi uses the axi address window 1 by default */
	espi->remote_ucmi_axi = 1;
	return 0;
}

static void udev_espi_unmap_remote_reg(struct udev_espi *espi)
{
	if (espi->remote_reg_base != NULL) {
		iounmap(espi->remote_reg_base);
		espi->remote_reg_base = NULL;
	}
}

static int udev_espi_init(struct udev_espi *espi)
{
	int ret;

	ret = udev_espi_map_remote_reg(espi);
	if (ret != 0) {
		hw_err("Map remote espi reg failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = ucmi_slave_setup(UDEV_ESPI);
	if (ret != 0) {
		hw_err("Slave setup failed. (ret=%d)\n", ret);
		udev_espi_unmap_remote_reg(espi);
		return ret;
	}

	return 0;
}

int udev_espi_specify_chip_type(u32 chip_type)
{
	if (chip_type <= MAMI_KERN_TYPE_INVALID || chip_type >= MAMI_KERN_TYPE_MAX) {
		hw_err("Chip type is invalid. (chip_type=%u)\n", chip_type);
		return -EINVAL;
	}
	g_chip_type = chip_type;

	return 0;
}

int udev_espi_probe(struct platform_device *pdev)
{
	struct udev_espi *espi;
	u32 chip_type = g_chip_type, devid;
	int ret;

	espi = udev_alloc_espi(&pdev->dev);
	if (espi == NULL) {
		hw_err("Alloc espi failed.\n");
		return -ENOMEM;
	}

	ret = ucmi_init(UDEV_ESPI, espi);
	if (ret != 0) {
		hw_err("UCMI init failed. (ret=%d)\n", ret);
		goto free_espi;
	}

	ret = udev_espi_master_init(espi);
	if (ret != 0) {
		goto uninit_ucmi;
	}

	ret = udev_espi_get_hw_info(espi, &chip_type);
	if (ret != 0) {
		hw_err("Get chip_type failed. (ret=%d)\n", ret);
		goto uninit_espi;
	}

	ret = udev_espi_init(espi);
	if (ret != 0) {
		hw_err("Espi init failed. (chip_type=%u;ret=%d)\n", chip_type, ret);
		goto uninit_espi;
	}

	ret = udev_set_guid_dev(&espi->hw);
	if (ret != 0) {
		hw_err("Set guid dev failed. (ret=%d)\n", ret);
		goto uninit_espi;
	}

	ret = mng_alloc_devid(espi->hw.slot_id, espi->hw.socket_id, &devid);
	if (ret != 0 && ret != -EEXIST) {
		hw_err("Alloc devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", espi->hw.slot_id, espi->hw.socket_id, ret);
		goto uninit_espi;
	}
	hw_info("Alloc devid. (slot_id=%u, socket_id=%u, devid=%u)\n", espi->hw.slot_id, espi->hw.socket_id, devid);

	platform_set_drvdata(pdev, espi);

	return 0;
uninit_espi:
	udev_espi_master_uninit(espi);
uninit_ucmi:
	ucmi_uninit(UDEV_ESPI, espi);
free_espi:
	udev_free_espi(espi);
	espi = NULL;
	return ret;
}

int udev_espi_remove(struct platform_device *pdev)
{
	struct udev_espi *espi = (struct udev_espi *)platform_get_drvdata(pdev);

	if (espi != NULL) {
		platform_set_drvdata(pdev, NULL);
		udev_espi_master_uninit(espi);
		ucmi_uninit(UDEV_ESPI, espi);
		udev_free_espi(espi);
		espi = NULL;
	}
	ures_reset();
	return 0;
}
