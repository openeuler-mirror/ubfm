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
#include <linux/errno.h>
#include "mami_string.h"
#include "ures.h"
#include "mng_kernel.h"
#include "feature_kern.h"
#include "fmdevm.h"

bool feature_kern_is_support_ras_report(void)
{
	int ret;
	u32 is_local_ascend;

	ret = ures_get_attr(MAMI_IS_LOCAL_ASCEND, &is_local_ascend, sizeof(is_local_ascend));
	if (ret == 0 && is_local_ascend != 0) {
		return false;
	}

	return true;
}

bool feature_kern_is_dpu(u32 devid)
{
	int ret;
	struct udev_hw_identify hw = {0};

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}

	if (hw.chip_type == MAMI_KERN_TYPE_1825) {
		return true;
	}

	return false;
}

bool feature_kern_is_ssd(u32 devid)
{
	int ret;
	struct udev_hw_identify hw = {0};

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}

	if (hw.chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		return true;
	}

	return false;
}

bool feature_kern_is_ndie(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}

	if ((hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_2P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_4P)) {
		return true;
	}

	return false;
}

bool feature_kern_ucmi_module_is_ascend(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}

	if ((hw.chip_type == MAMI_KERN_TYPE_ASCEND_V1) ||
		(hw.chip_type == MAMI_KERN_TYPE_ASCEND_V2) ||
		(hw.chip_type == MAMI_KERN_TYPE_UNIONS) ||
		(hw.chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		return true;
	}

	return false;
}

bool feature_kern_ucmi_module_is_kunpeng(u32 devid)
{
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return false;
	}

	if ((hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_1P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_2P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_4P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_2P) ||
		(hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_4P)) {
		return true;
	}

	return false;
}

int feature_kern_get_udie_version(u32 devid, enum mami_udie_version *udie_version)
{
	int ret = 0;
	u64 value = 0;

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &value);
	if (ret != 0) {
		mng_err("Get udie version failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	*udie_version = (enum mami_udie_version)value;

	return ret;
}

#define TARGET_TBL_ENTRY_NUM_V1 36
#define TARGET_TBL_ENTRY_NUM_V2_UDIE 40
#define TARGET_TBL_ENTRY_NUM_V2_NDIE_8PORT  8
#define TARGET_TBL_ENTRY_NUM_V2_NDIE_16PORT 16
#define TARGET_TBL_ENTRY_NUM_V2_NDIE_32PORT 32
#define TARGET_TBL_ENTRY_NUM_V2_NDIE_64PORT 64

int feature_kern_get_rx_tgtid_table_spec(u32 devid, enum ndie_p2p_port_mode p2p_port_mode, u32 *entry_num)
{
	int ret;
	enum mami_udie_version version;
	u32 port_mode_to_entry_num[PORT_MODE_MAX] = {
		[PORT_MODE_8] =  TARGET_TBL_ENTRY_NUM_V2_NDIE_8PORT,
		[PORT_MODE_16] = TARGET_TBL_ENTRY_NUM_V2_NDIE_16PORT,
		[PORT_MODE_32] = TARGET_TBL_ENTRY_NUM_V2_NDIE_32PORT,
		[PORT_MODE_64] = TARGET_TBL_ENTRY_NUM_V2_NDIE_64PORT
	};

	if (entry_num == NULL || p2p_port_mode >= PORT_MODE_MAX) {
		return -EINVAL;
	}

	if (feature_kern_is_ndie(devid)) {
		*entry_num = port_mode_to_entry_num[p2p_port_mode];
		return 0;
	}

	ret = feature_kern_get_udie_version(devid, &version);
	if (ret != 0) {
		return ret;
	}

	if (version == UDIE_VERSION_V1) {
		*entry_num = TARGET_TBL_ENTRY_NUM_V1;
	} else {
		*entry_num = TARGET_TBL_ENTRY_NUM_V2_UDIE;
	}

	return 0;
}

#define TX_PPP_TGTID_TABLE_ENTRY_NUM_V160_NDIE 8
#define TX_PPP_TGTID_TABLE_ENTRY_NUM_V160_UDIE 40
#define TX_PPP_TGTID_TABLE_ENTRY_NUM_V100 36

int feature_kern_get_tx_tgtid_table_spec(u32 devid, u32 *entry_num)
{
	int ret;
	enum mami_udie_version version;

	ret = feature_kern_get_udie_version(devid, &version);
	if (ret != 0) {
		return ret;
	}

	if (version == UDIE_VERSION_V1) {
		*entry_num = TX_PPP_TGTID_TABLE_ENTRY_NUM_V100;
	} else if (feature_kern_is_ndie(devid)) {
		*entry_num = TX_PPP_TGTID_TABLE_ENTRY_NUM_V160_NDIE;
	} else {
		*entry_num = TX_PPP_TGTID_TABLE_ENTRY_NUM_V160_UDIE;
	}

	return 0;
}

static int get_port_mapping_v1_one_port_last_body(u32 die_num, u32 nl_num, u64 *valid_body_mask, u32 *port_mapping, u32 *port_num)
{
	u32 i, die_id, nl_id;
	u32 max_port_num = die_num * nl_num * NL_PORT_NUM_X4 - die_num;
	u32 die_port_num = nl_num * NL_PORT_NUM_X4 - 1;

	if (*port_num < max_port_num) {
		return -EINVAL;
	}

	for (i = 0; i < max_port_num; i++) {
		die_id = i / die_port_num;
		nl_id = i % die_port_num / NL_PORT_NUM_X4;
		if ((valid_body_mask[die_id] >> nl_id) & 0x1) {
			port_mapping[i] = die_id * nl_num * NL_PORT_MAX_NUM + i % die_port_num * NL_PORT_NUM_X4;
		} else {
			port_mapping[i] = DEV_PORT_MAX_NUM;
		}
	}

	*port_num = max_port_num;
	return 0;
}

static int get_port_mapping_v1_two_port_last_body(u32 die_num, u32 nl_num, u64 *valid_body_mask, u32 *port_mapping, u32 *port_num)
{
	u32 i, die_id, nl_id;
	u32 max_port_num = die_num * nl_num * NL_PORT_NUM_X4;
	u32 die_port_num = nl_num * NL_PORT_NUM_X4;

	if (*port_num < max_port_num) {
		return -EINVAL;
	}

	for (i = 0; i < max_port_num; i++) {
		die_id = i / die_port_num;
		nl_id = i % die_port_num / NL_PORT_NUM_X4;
		if ((valid_body_mask[die_id] >> nl_id) & 0x1) {
			port_mapping[i] = i * NL_PORT_NUM_X4;
		} else {
			port_mapping[i] = DEV_PORT_MAX_NUM;
		}
	}

	*port_num = max_port_num;
	return 0;
}

static void get_port_mapping_v2(u32 die_num, u32 nl_num, u64 *pcie_mask, u64 *body_mask, u32 *port_mapping)
{
	u32 die_id = 0, nl_id = 0, nl_port_id = 0, pg_nl_id = 0, lgport = 0, phyport = 0;
	u32 max_port_num = die_num * nl_num * NL_PORT_MAX_NUM;

	for (lgport = 0; lgport < max_port_num; lgport++) {
		port_mapping[lgport] = DEV_PORT_MAX_NUM;
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		pg_nl_id = 0;
		for (nl_id = 0; nl_id < nl_num; nl_id++) {
			if (!((body_mask[die_id] >> nl_id) & 0x1)) {
				continue;
			}

			if ((pcie_mask[die_id] >> pg_nl_id) & 0x1) {
				pg_nl_id++;
				continue;
			}

			for (nl_port_id = 0; nl_port_id < NL_PORT_MAX_NUM; nl_port_id++) {
				phyport = (die_id * nl_num + nl_id) * NL_PORT_MAX_NUM + nl_port_id;
				lgport = (die_id * nl_num + pg_nl_id) * NL_PORT_MAX_NUM + nl_port_id;
				port_mapping[lgport] = phyport;
			}
			pg_nl_id++;
		}
	}
}

int feature_kern_get_port_mapping(u32 devid, u32 *port_mapping, u32 *port_num)
{
	int ret;
	u64 die_id, die_num, nl_num;
	struct udev_hw_identify hw = {0};
	u64 body_mask[DEV_DIE_MAX_NUM], pcie_mask[DEV_DIE_MAX_NUM], valid_body_mask[DEV_DIE_MAX_NUM];
	char key[URES_NAME_LEN] = {0};

	if (port_mapping == NULL || port_num == NULL) {
		mng_err("Invalid params. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0 || hw.chip_type == MAMI_KERN_TYPE_INVALID || hw.chip_type >= MAMI_KERN_TYPE_MAX) {
		mng_err("Get hw info failed. (devid=%u; ret=%d; chip_type=%u)\n", devid, ret, hw.chip_type);
		return -EINVAL;
	}

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if (ret != 0 || die_num == 0 || die_num > DEV_DIE_MAX_NUM) {
		mng_err("Get die num failed. (devid=%u; ret=%d; die_num=%llu)\n", devid, ret, die_num);
		return -EINVAL;
	}

	ret = ures_get_key_value(devid, MAMI_NL_NUM, &nl_num);
	if (ret != 0 || nl_num == 0 || nl_num > NL_MAX_NUM) {
		mng_err("Get nl num failed. (devid=%u; ret=%d; nl_num=%llu)\n", devid, ret, nl_num);
		return -EINVAL;
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		(void)UBFM_SPRINTF(key, sizeof(key), MAMI_DIEN_BODY_MASK, (u32)die_id);
		ret = ures_get_key_value(devid, key, &body_mask[die_id]);
		if (ret != 0) {
			mng_err("Get body mask failed. (devid=%u; ret=%d; die_id=%llu)\n", devid, ret, die_id);
			return ret;
		}

		(void)UBFM_SPRINTF(key, sizeof(key), MAMI_DIEN_PCIE_MASK, (u32)die_id);
		ret = ures_get_key_value(devid, key, &pcie_mask[die_id]);
		if (ret != 0) {
			mng_err("Get pcie mask failed. (devid=%u; ret=%d; die_id=%llu)\n", devid, ret, die_id);
			return ret;
		}
		valid_body_mask[die_id] = ~pcie_mask[die_id] & body_mask[die_id];
	}

	if (hw.chip_type == MAMI_KERN_TYPE_UNIONS || hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_1P || hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_2P ||
		hw.chip_type == MAMI_KERN_TYPE_KUNPENG950_4P) {
		return get_port_mapping_v1_one_port_last_body((u32)die_num, (u32)nl_num, valid_body_mask, port_mapping, port_num);
	}

	if (hw.chip_type == MAMI_KERN_TYPE_ASCEND_V1 || hw.chip_type == MAMI_KERN_TYPE_1825 || hw.chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		return get_port_mapping_v1_two_port_last_body((u32)die_num, (u32)nl_num, valid_body_mask, port_mapping, port_num);
	}

	if (*port_num < die_num * nl_num * NL_PORT_MAX_NUM) {
		return -EINVAL;
	}

	get_port_mapping_v2((u32)die_num, (u32)nl_num, pcie_mask, body_mask, port_mapping);
	*port_num = die_num * nl_num * NL_PORT_MAX_NUM;
	return 0;
}
