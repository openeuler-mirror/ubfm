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
#include "fmdevm.h"
#include "ubfm_common.h"
#include "ures.h"
#include "mctrlq_log.h"
#include "ctrlq_hw.h"
#include "ctrlq_init.h"

static bool ctrlq_is_support(u32 devid, u32 chip_type)
{
	if ((chip_type == MAMI_KERN_TYPE_UNIONS) || (chip_type == MAMI_KERN_TYPE_1825) ||
		(chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		/* For unions, ctrlq reg is not inited */
		mctrlq_info("Ctrlq shouldn't be init. (devid=%u; chip_type=%u)\n", devid, chip_type);
		return false;
	}
	return true;
}

int ctrlq_kern_dev_init(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (!ctrlq_is_support(devid, hw.chip_type)) {
		return 0;
	}

	ret = ctrlq_hw_init(devid, hw.chip_type);
	if (ret != 0) {
		mctrlq_err("Ctrlq hw init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	return 0;
}

void ctrlq_kern_dev_uninit(u32 devid)
{
	ctrlq_hw_uninit(devid);
}

int ctrlq_kern_dev_irq_init(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (!ctrlq_is_support(devid, hw.chip_type)) {
		return 0;
	}
	return ctrlq_kern_irq_unmask(devid);
}
void ctrlq_kern_dev_irq_uninit(u32 devid)
{
	return ctrlq_kern_irq_mask(devid);
}
