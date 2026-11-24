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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include "mami_string.h"

#ifdef CFG_BUILD_DEBUG
#include <linux/sysfs.h>
#include <linux/platform_device.h>

#include "ubfm_dfx_fs.h"
#endif

#include "ubfm_common.h"
#include "msi_log.h"
#include "udm.h"
#include "uid.h"
#include "ures.h"
#include "feature_loader.h"
#include "fmdevm.h"
#include "fpga_int_def.h"
#include "msi_irq.h"
#include "fpga_adapter_kern.h"

LIST_HEAD(g_msi_pci_data_list);
DEFINE_MUTEX(g_msi_pci_data_lock);

static struct pci_msi_data *msi_pci_data_get(u32 devnum)
{
	struct pci_msi_data *pos = NULL;

	mutex_lock(&g_msi_pci_data_lock);
	list_for_each_entry(pos, &g_msi_pci_data_list, entry) {
		if (pos->devnum == devnum) {
			mutex_unlock(&g_msi_pci_data_lock);
			return pos;
		}
	}

	mutex_unlock(&g_msi_pci_data_lock);
	return NULL;
}

STATIC int msi_pci_data_add(struct pci_msi_data *data)
{
	u32 i;
	struct pci_msi_data *tmp = msi_pci_data_get(data->devnum);

	if (tmp != NULL) {
		return -EEXIST;
	}

	tmp = kzalloc(sizeof(struct pci_msi_data), GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp == NULL) {
		msi_err("Alloc msi data mem failed. (devnum=%u)\n", data->devnum);
		return -ENOMEM;
	}

	mutex_init(&tmp->mtx_lock);
	tmp->devnum = data->devnum;
	tmp->irqnum = data->irqnum;
	tmp->dev = data->dev;
	tmp->fpga_ops = data->fpga_ops;

	for (i = 0; i < UDEV_MSI_IRQ_VECTOR_NUM; i++) {
		tmp->irq_data[i].irq = -1;
	}

	mutex_lock(&g_msi_pci_data_lock);
	list_add_tail(&tmp->entry, &g_msi_pci_data_list);
	mutex_unlock(&g_msi_pci_data_lock);
	return 0;
}

STATIC struct pci_dev *msi_get_pci_dev(u32 devid, u32 devnum)
{
	u32 domain, bus_id, dev_id, func_id;
	struct pci_dev *pdev = NULL;
	struct pci_bus *pbus = NULL;

	domain = (devnum >> 16) & 0xFFFF;	   /* the high 16 bits are domain */
	bus_id = (devnum >> 8) & 0xFF;		  /* bits 8 ~ 15 are busid */
	dev_id = (devnum >> 3) & 0x1F;		  /* bits 3 ~ 7 are pcie device id */
	func_id = devnum & 0x7;

	msi_info("Get pcie info. (devid=%u; domain=%u; bus_id=%u; dev_id=%u; func_id=%u)\n",
			 devid, domain, bus_id, dev_id, func_id);
	pbus = (struct pci_bus *)pci_find_bus((int)domain, (int)bus_id);
	if (pbus == NULL) {
		msi_err("Pci find bus failed. (devid=%u; devnum=%u)\n", devid, devnum);
		return NULL;
	}

	pdev = (struct pci_dev *)pci_get_slot(pbus, PCI_DEVFN(dev_id, func_id));
	if (pdev == NULL) {
		msi_err("Pci get slot failed. (devid=%u; devnum=%u)\n", devid, devnum);
		return NULL;
	}

	return pdev;
}

STATIC irqreturn_t msi_irq_handler(int irq, void *dev_id)
{
	struct msi_irq_data *irq_data = (struct msi_irq_data *)dev_id;
	struct pci_msi_data *msi_data = irq_data->msi_data;
	const struct fpga_ops *fpga_ops = msi_data->fpga_ops;
	struct uid_irq_info irq_info = {0};
	struct timespec64 ts = {0};
	u32 status = 0;

	(void)ktime_get_real_ts64(&ts);
	irq_info.irq_time = ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);

	if (fpga_ops == NULL) {
		msi_err("Irq ops not initialized.\n");
		return IRQ_NONE;
	}

	(void)fpga_ops->read_irq_status(irq_data->devid, irq_data->irq_idx, &status);
	if ((status & FPGA_CTRLQ_INT_MASK) != 0) {
		irq_info.irq_para.ctrlq.in_irq = 1;
		irq_info.irq_para.ctrlq.dieid = (irq_data->irq_idx & 0xFF);
		irq_info.irq_para.ctrlq.chan_status = status;
		uid_trigger_intr(irq_data->devid, UID_TYPE_CTRLQ, irq_info);
	}

	if ((status & (FPGA_RAS_INT_MASK << FPGA_RAS_INT_SHIFT)) != 0) {
		irq_info.irq_para.ras.in_irq = 1;
		/* one interrupt per DIE, here irq_idx is equivalent to passing the die id to the RAS module */
		irq_info.irq_para.ras.dieid = (irq_data->irq_idx & 0xFF);
		uid_trigger_intr(irq_data->devid, UID_TYPE_RAS, irq_info);
	}

	/* Clear int status */
	(void)fpga_ops->write_irq_status(irq_data->devid, irq_data->irq_idx, status);
	return IRQ_HANDLED;
}

STATIC void msi_irq_enable(struct msi_irq_data *irq_data)
{
	struct pci_msi_data *msi_data = irq_data->msi_data;
	(void)msi_data->fpga_ops->write_irq_mask(irq_data->devid, irq_data->irq_idx, 0xBFFFF);
}

STATIC void msi_irq_disable(struct msi_irq_data *irq_data)
{
	struct pci_msi_data *msi_data = irq_data->msi_data;
	(void)msi_data->fpga_ops->write_irq_mask(irq_data->devid, irq_data->irq_idx, 0x0);
}

static int msi_alloc_irq_vectors(u32 devid, chip_ub_mng *chip_mng)
{
	struct pci_msi_data msi_data = {0};
	int ret;

	msi_data.dev = msi_get_pci_dev(devid, chip_mng->devnum);
	if (msi_data.dev == NULL) {
		msi_err("Get pci dev failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	msi_data.devnum = chip_mng->devnum;
	ret = pci_alloc_irq_vectors(msi_data.dev, UDEV_MSI_IRQ_VECTOR_NUM, UDEV_MSI_IRQ_VECTOR_NUM,
								PCI_IRQ_MSI | PCI_IRQ_MSIX);
	if (ret != UDEV_MSI_IRQ_VECTOR_NUM) {
		msi_err("Alloc msi irq failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	msi_data.irqnum = (u32)ret;
	msi_info("Alloc msi irq num %u. (devid=%u; devnum=%u)\n", msi_data.irqnum, devid, chip_mng->devnum);

	msi_data.fpga_ops = fpga_ops_get();
	if (msi_data.fpga_ops == NULL) {
		msi_err("Get fpga ops failed. (devid=%u; devnum=%u)\n", devid, chip_mng->devnum);
		pci_free_irq_vectors(msi_data.dev);
		return -ENOMEM;
	}

	ret = msi_pci_data_add(&msi_data);
	if (ret != 0) {
		msi_err("Add msi data failed. (devid=%u; devnum=%u; ret=%d)\n", devid, chip_mng->devnum, ret);
		pci_free_irq_vectors(msi_data.dev);
		return ret;
	}

	msi_info("Add msi data success. (devid=%u; devnum=%u)\n", devid, chip_mng->devnum);
	return 0;
}

static int msi_dev_request_irqs(u32 devid, chip_ub_mng *chip_mng, struct pci_msi_data *msi_data)
{
	struct msi_irq_data *irq_data = NULL;
	u64 die_num = 0;
	u32 i, j, vector_idx;
	int ret = 0, irq;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if (ret != 0) {
		msi_err("Get die num failed. (devid=%u; die_num=%llu; ret=%d)\n", devid, die_num, ret);
		return ret;
	}

	if (chip_mng->msi_len < (u32)die_num) {
		msi_err("Invalid msi len. (devid=%u; msi_len=%u; die_num=%llu)\n", devid, chip_mng->msi_len, die_num);
		return -EINVAL;
	}

	mutex_lock(&msi_data->mtx_lock);
	for (i = 0; i < (u32)die_num; i++) {
		vector_idx = chip_mng->msi_start + i;
		if (vector_idx >= UDEV_MSI_IRQ_VECTOR_NUM) {
			msi_err("Invalid msi vector idx. (devid=%u; idx=%u; msi_start=%u)\n", devid, i, chip_mng->msi_start);
			ret = -EINVAL;
			goto fail;
		}

		irq_data = &msi_data->irq_data[vector_idx];
		if (irq_data->irq != -1) {
			msi_err("Irq already register. (devid=%u; idx=%u; vector_idx=%u)\n", devid, i, vector_idx);
			ret = -EINVAL;
			goto fail;
		}

		irq_data->vector_idx = vector_idx;
		irq_data->devid = devid;
		irq_data->irq_idx = i;
		irq_data->msi_data = msi_data;

		irq = pci_irq_vector(msi_data->dev, vector_idx);
		if (irq < 0) {
			msi_err("Irq vector get failed. (devid=%u; vector_idx=%u; ret=%d)\n", devid, vector_idx, irq);
			ret = irq;
			goto fail;
		}

		msi_irq_disable(irq_data);
		(void)UBFM_SPRINTF(irq_data->name, MSI_IRQ_NAME_LEN, "msi_%u_%u", devid, i);
		ret = request_irq(irq, msi_irq_handler, IRQF_SHARED, irq_data->name, irq_data);
		if (ret != 0) {
			msi_err("Request irq failed. (devid=%u; irq=%d; vector_idx=%u; ret=%d)\n", devid, irq,
					vector_idx, ret);
			goto fail;
		}
		irq_data->irq = irq;
		msi_irq_enable(irq_data);
	}

	mutex_unlock(&msi_data->mtx_lock);
	udm_notifier_call(devid, UDM_IRQ_INIT);
	msi_info("Request irq success. (devid=%u; die_num=%llu; msi_start=%u; msi_len=%u)\n", devid, die_num,
			 chip_mng->msi_start, chip_mng->msi_len);
	return 0;
fail:
	for (j = 0; j < i; j++) {
		vector_idx = chip_mng->msi_start + j;
		irq_data = &msi_data->irq_data[vector_idx];
		(void)free_irq(irq_data->irq, irq_data);
		irq_data->irq = -1;
	}

	mutex_unlock(&msi_data->mtx_lock);
	return ret;
}

static void msi_dev_free_irqs(u32 devid, chip_ub_mng *chip_mng, struct pci_msi_data *msi_data)
{
	struct msi_irq_data *irq_data = NULL;
	u32 i, vector_idx;
	u64 die_num = 0;
	int ret;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if (ret != 0) {
		msi_err("Get die num failed. (devid=%u; die_num=%llu; ret=%d)\n", devid, die_num, ret);
		return;
	}

	if (chip_mng->msi_len < (u32)die_num) {
		msi_err("Invalid msi len. (devid=%u; msi_len=%u; die_num=%llu)\n", devid, chip_mng->msi_len, die_num);
		return;
	}

	mutex_lock(&msi_data->mtx_lock);
	for (i = 0; i < (u32)die_num; i++) {
		vector_idx = chip_mng->msi_start + i;
		if (vector_idx >= UDEV_MSI_IRQ_VECTOR_NUM) {
			msi_err("Invalid msi vector idx. (devid=%u; idx=%u; msi_start=%u)\n", devid, i, chip_mng->msi_start);
			break;
		}

		irq_data = &msi_data->irq_data[vector_idx];
		if (irq_data->irq < 0) {
			continue;
		}

		(void)free_irq(irq_data->irq, irq_data);
		irq_data->irq = -1;

		msi_irq_disable(irq_data);
	}
	mutex_unlock(&msi_data->mtx_lock);
}

static int msi_dev_init(u32 devid, struct udev_hw_identify *hw)
{
	chip_ub_mng chip_mng = {0};
	struct pci_msi_data *msi_data = NULL;
	int ret;

	ret = (int)starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, hw->socket_id, &chip_mng);
	if (ret != 0) {
		msi_err("Get chip mng failed. (devid=%u; socket_id=%u)\n", devid, hw->socket_id);
		return ret;
	}

	msi_data = msi_pci_data_get(chip_mng.devnum);
	if (msi_data == NULL) {
		/* If the interrupt vector has not been allocated yet, allocate it first. */
		ret = msi_alloc_irq_vectors(devid, &chip_mng);
		if (ret != 0) {
			msi_err("Alloc irq vectors failed. (devid=%u; socket_id=%u; devnum=%u; ret=%d)\n",
					devid, hw->socket_id, chip_mng.devnum, ret);
			return ret;
		}

		msi_data = msi_pci_data_get(chip_mng.devnum);
		if (msi_data == NULL) {
			msi_err("Cannot get msi data. (devid=%u; socket_id=%u; devnum=%u)\n", devid,
					hw->socket_id, chip_mng.devnum);
			return -EINVAL;
		}
	}

	return msi_dev_request_irqs(devid, &chip_mng, msi_data);
}

static void msi_dev_uninit(u32 devid, struct udev_hw_identify *hw)
{
	chip_ub_mng chip_mng = {0};
	struct pci_msi_data *msi_data = NULL;
	int ret;

	udm_notifier_call(devid, UDM_IRQ_UNINIT);

	ret = (int)starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, hw->socket_id, &chip_mng);
	if (ret != 0) {
		msi_err("Get chip mng failed. (devid=%u; socket_id=%u)\n", devid, hw->socket_id);
		return;
	}

	msi_data = msi_pci_data_get(chip_mng.devnum);
	if (msi_data == NULL) {
		msi_err("Get msi data failed. (devid=%u; socket_id=%u; devnum=%u)\n", devid, hw->socket_id,
				chip_mng.devnum);
		return;
	}

	msi_dev_free_irqs(devid, &chip_mng, msi_data);
}

static void msi_pci_data_destroy(void)
{
	struct pci_msi_data *pos = NULL;
	struct pci_msi_data *tmp = NULL;
	u32 i;

	mutex_lock(&g_msi_pci_data_lock);
	list_for_each_entry_safe(pos, tmp, &g_msi_pci_data_list, entry) {
		list_del(&pos->entry);
		for (i = 0; i < UDEV_MSI_IRQ_VECTOR_NUM; i++) {
			if (pos->irq_data[i].irq == -1) {
				continue;
			}

			free_irq(pos->irq_data[i].irq, &pos->irq_data[i]);
		}

		pci_free_irq_vectors(pos->dev);
		pos->fpga_ops = NULL;
		mutex_destroy(&pos->mtx_lock);
		kfree(pos);
	}

	mutex_unlock(&g_msi_pci_data_lock);
}

STATIC int msi_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	struct udev_hw_identify hw_attr;
	struct udm_dev_para para;
	int ret;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		msi_err("Get dev attr failed. (devid=%u)\n", devid);
		return ret;
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw_attr, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		msi_err("Get hw dev attr failed. (devid=%u)\n", devid);
		return ret;
	}

	if (hw_attr.connect_type != UDEV_PCIE_ESPI) {
		return 0;
	}

	if (action == UDM_INIT) {
		ret = msi_dev_init(devid, &hw_attr);
	} else if (action == UDM_UNINIT) {
		msi_dev_uninit(devid, &hw_attr);
	}
	return ret;
}

int msi_init(void)
{
	int ret;

	ret = udm_notifier_register(MODULE_MSI_NAME, UDM_PRI4, msi_udm_notify);
	if (ret != 0) {
		msi_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(msi_init, FEATURE_LOADER_STAGE_6);

void msi_uninit(void)
{
	msi_pci_data_destroy();
	(void)udm_notifier_unregister(MODULE_MSI_NAME);
}
DECLARE_FEATURE_AUTO_UNINIT(msi_uninit, FEATURE_LOADER_STAGE_6);
