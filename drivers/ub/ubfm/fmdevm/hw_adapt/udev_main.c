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
#include "feature_loader.h"
#include "udm.h"
#include "ures.h"
#include "udev_hw_pub.h"
#include "idm.h"
#include "udev_feature.h"
#ifdef CFG_UBFM_SUPPORT_CDMA
#include "udev_remote_probe.h"
#endif
#include "udev_remote_msg_probe.h"
#include "udev_msg_pub.h"

static int udev_hw_get_dev_attr(u32 devid, struct udev_hw_identify *hw)
{
	char guid_str[GUID_STR_LEN];
	struct udm_dev_para para;
	int ret;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		return ret;
	}

	ret = udev_guid_to_str(para.guid, guid_str);
	if (ret != 0) {
		return ret;
	}

	ret = ures_get_attr(guid_str, hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int udev_hw_dev_init(u32 devid, int chip_type, int conn_type, int acc_type)
{
	u32 chnl_type;

	chnl_type = udev_kern_get_mng_chnl_type(chip_type);
	if (chnl_type >= CHNL_TYPE_MAX) {
		hw_err("dev %u invalid chip_type %d conn_type %d acc_type %d\n", devid, chip_type, conn_type, acc_type);
		return -EINVAL;
	}
	if (udev_get_mng_intf(chnl_type)->hw_init == NULL) {
		hw_err("dev %u chnl_type %u hw init is NULL\n", devid, chnl_type);
		return -EINVAL;
	}
	return udev_get_mng_intf(chnl_type)->hw_init(devid, chip_type, conn_type, acc_type);
}

static void udev_hw_dev_uninit(u32 devid, int chip_type, int conn_type, int acc_type)
{
	udev_uninit_hw_info(devid, chip_type);
}

static int udev_hw_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	struct udev_hw_identify hw_attr;
	int ret = 0;

	/* get chip_type and acc_type from runtime_data */
	if (action == UDM_INIT) {
		ret = udev_hw_get_dev_attr(devid, &hw_attr);
		if (ret != 0) {
			hw_err("ures get attr failed. (devid=%u; ret=%d)\n", devid, ret);
			return ret;
		}

		ret = udev_hw_dev_init(devid, hw_attr.chip_type,
			hw_attr.connect_type, hw_attr.access_type);
		if (ret != 0) {
			return ret;
		}
	} else if (action == UDM_UNINIT) {
		ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw_attr, sizeof(struct udev_hw_identify));
		if (ret != 0) {
			hw_err("ures get dev attr failed. (devid=%u; ret=%d)\n", devid, ret);
			return ret;
		}

		udev_hw_dev_uninit(devid, hw_attr.chip_type,
			hw_attr.connect_type, hw_attr.access_type);
	}
	return ret;
}


static int udev_hw_dev_init_mami(u32 devid, int chip_type, int conn_type, int acc_type)
{
	int ret;

	ret = ures_set_key_value(devid, MAMI_ACC_TYPE, (u64)acc_type);
	if (ret != 0) {
		return ret;
	}
	ret = udev_ctrl_set_dev_info(devid);
	if (ret != 0) {
		return ret;
	}
	ret = udev_init_hw_info(devid, chip_type);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

int udev_register_mami_intf(void)
{
	int ret;
	struct dev_mng_intf_group intf_group = {
#ifdef CFG_UBFM_SUPPORT_CDMA
		.hw_probe = hw_adapt_cdma_probe,
#endif
		.hw_init = udev_hw_dev_init_mami
	};

	ret = udev_register_intf(MAMI_CHNL_TYPE, &intf_group);
	if (ret != 0) {
		hw_err("Register cdma mng intf failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

int udev_register_dpu_intf(void)
{
	int ret;
	struct dev_mng_intf_group intf_group = {
		.hw_probe = hw_adapt_msg_probe,
		.hw_init = udev_hw_dev_init_by_msg
	};

	ret = udev_register_intf(DPU_CHNL_TYPE, &intf_group);
	if (ret != 0) {
		hw_err("Register cdma mng intf failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

int udev_hw_adapt_init(void)
{
	int ret;

	ret = udev_register_mami_intf();
	if (ret != 0) {
		return ret;
	}

	ret = udev_register_dpu_intf();
	if (ret != 0) {
		return ret;
	}

	ret = udm_notifier_register(MODULE_HW, UDM_PRI0, udev_hw_udm_notify);
	if (ret != 0) {
		hw_err("Register udm notifier failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(udev_hw_adapt_init, FEATURE_LOADER_STAGE_6);

void udev_hw_adapt_uninit(void)
{
	(void)udm_notifier_unregister(MODULE_HW);
}
DECLARE_FEATURE_AUTO_UNINIT(udev_hw_adapt_uninit, FEATURE_LOADER_STAGE_6);
