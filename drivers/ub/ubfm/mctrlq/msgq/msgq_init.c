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
#include "mami_string.h"
#include "ures.h"
#include "mctrlq_log.h"
#include "msgq_hw.h"
#include "msgq_init.h"
#include "mctrlq.h"
#include "mctrlq.h"
#include "fmdevm.h"
#include "uop.h"

int msgq_kern_dev_init(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mctrlq_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (hw.chip_type == MAMI_KERN_TYPE_1825) {
		return msgq_dpu_register_intf(devid, &hw);
	}
	ret = msgq_kern_hw_init(devid);
	if (ret != 0) {
		mctrlq_err("Msgq hw init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	return 0;
}

void msgq_kern_dev_uninit(u32 devid)
{
	/* Undo the registration done by msgq_kern_dev_init(), the 1825 hw uninit is a no-op. */
	(void)uop_kern_unregister_msg_ops(devid);
	msgq_kern_hw_uninit(devid);
}

void msgq_kern_unregister_all_intf(void)
{
	u32 devid;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		(void)uop_kern_unregister_msg_ops(devid);
	}
}

