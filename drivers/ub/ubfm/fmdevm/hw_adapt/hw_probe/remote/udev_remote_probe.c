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
#include "ucd.h"
#include "mami_string.h"
#include "feature_loader.h"

#include "ubfm_common.h"
#include "fmdevm.h"
#include "ures.h"
#include "udev_ctrl.h"
#include "udev_hw_info.h"
#include "fmdevm.h"
#include "udev_cdma_pub.h"
#include "udev_feature.h"
#include "mng_devid.h"

#include "udev_msg_pub.h"
#include "msgq.h"
#define UDEV_HW_INFO_REG_OFFSET 0xFF000

#define UDEV_KUNPENG950_SRAM_BASE 0x3020000000
#define UDEV_ASCEND_V1_SRAM_BASE 0x20020000000
#define UDEV_UNIONS_SRAM_BASE 0x20020000000
/* used by the kunpeng960 version */
#define UDEV_KUNPENG960_SRAM_BASE 0x9D30000
#define UDEV_HSSU_V100_SRAM_BASE 0x808D80C000

struct udev_cfg_head {
	struct udev_cfg_hdr cfg_hdr;
	struct udev_cfg cfg;
};

static int udev_get_hw_info_by_cdma(struct mami_dev_info *dev_info, struct udev_hw_identify *hw)
{
	struct udev_cdma_dev *cdma_dev = NULL;
	u32 chip_type = dev_info->chipType;
	struct udev_cfg_head cfg_head = {0};
	struct udev_cfg *cfg;
	u64 resv_mem_base;
	int ret;

	if (chip_type == MAMI_KERN_TYPE_ASCEND_V1 || chip_type == MAMI_KERN_TYPE_ASCEND_V2) {
		resv_mem_base = UDEV_ASCEND_V1_SRAM_BASE + UDEV_HW_INFO_REG_OFFSET;
	} else if ((chip_type == MAMI_KERN_TYPE_UNIONS) || (chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		resv_mem_base = UDEV_UNIONS_SRAM_BASE + UDEV_HW_INFO_REG_OFFSET;
	} else if (chip_type == MAMI_KERN_TYPE_KUNPENG960_1P || chip_type == MAMI_KERN_TYPE_KUNPENG960_2P || chip_type == MAMI_KERN_TYPE_KUNPENG960_4P) {
		resv_mem_base = UDEV_KUNPENG960_SRAM_BASE;
	} else if (chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		resv_mem_base = UDEV_HSSU_V100_SRAM_BASE;
	} else {
		resv_mem_base = UDEV_KUNPENG950_SRAM_BASE + UDEV_HW_INFO_REG_OFFSET;
	}

	cdma_dev = udev_cdma_init(0, dev_info->cna, dev_info->eid, 0x2);
	if (cdma_dev == NULL) {
		hw_err("Init cdma dev failed.\n");
		return -ENOMEM;
	}
	ret = cdma_read(cdma_dev, resv_mem_base, (void *)&cfg_head, sizeof(cfg_head));
	if (ret != 0) {
		hw_err("Cdma read failed. (ret=%d;chip_type=%u;mem=%#llx)\n", ret, chip_type, resv_mem_base);
		udev_cdma_uninit(cdma_dev);
		return ret;
	}
	udev_cdma_uninit(cdma_dev);

	cfg = &cfg_head.cfg;
	if (cfg->device_type != dev_info->chipType) {
		hw_err("Invalid chip_type. (cfg_chip_type=%u;in_chip_type=%u)\n",
			cfg->device_type, dev_info->chipType);
		return -EINVAL;
	}
	hw->chip_type = cfg->device_type;
	hw->slot_id = cfg->slot_id;
	hw->socket_id = cfg->socket_id;
	hw->resv_mem_base = resv_mem_base;
	hw_debug("Get cfg info. (type=%u; slot_id=%u; socket_id=%u)\n",
		cfg->device_type, cfg->slot_id, cfg->socket_id);
	return 0;
}

static int udev_remote_get_hw_info(struct mami_dev_info *dev_info)
{
	u32 chip_type = dev_info->chipType, devid;
	struct udev_hw_identify hw = {0};
	int ret;

	if (chip_type >= MAMI_KERN_TYPE_MAX || chip_type <= MAMI_KERN_TYPE_INVALID) {
		hw_err("Chip type is invalid. (chip_type=%u)\n", chip_type);
		return -EINVAL;
	}

	ret = udev_get_hw_info_by_cdma(dev_info, &hw);
	if (ret != 0) {
		hw_err("Get hw info by cdma failed. (ret=%d)\n", ret);
		return ret;
	}
	udev_guid_copy(hw.guid, dev_info->guid);
	hw.chip_type = chip_type;
	hw.access_type = UDEV_CDMA;
	hw.location = UDM_REMOTE;
	hw.connect_type = UDEV_UBUS;
	hw.resv_mem_size = SIZE_KB(4); /* resv sram size is 4KB */

	ret = udev_set_guid_dev(&hw);
	if (ret != 0) {
		hw_err("Set guid info failed.(ret=%d)\n", ret);
		return ret;
	}
	ret = mng_alloc_devid(hw.slot_id, hw.socket_id, &devid);
	if (ret != 0 && ret != -EEXIST) {
		hw_err("Get devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", hw.slot_id, hw.socket_id, ret);
		return ret;
	}
	hw_info("Alloc devid. (slot_id=%u, socket_id=%u, devid=%u)\n", hw.slot_id, hw.socket_id, devid);
	return 0;
}

int hw_adapt_cdma_probe(struct mami_dev_info *dev_info)
{
	int ret;

	ret = udev_remote_get_hw_info(dev_info);
	if (ret != 0) {
		hw_err("Remote get hw info failed.(ret=%d; chip_type=%u)\n", ret, dev_info->chipType);
		return ret;
	}
	hw_info("Trigger dev probe success. (ret=%d; chip_type=%u)\n", ret, dev_info->chipType);
	return 0;
}

int hw_adapt_ucd_trigger_probe(void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_dev_info *dev_info = NULL;
	u32 chnl_type;

	if ((in == NULL) || (in_len != sizeof(struct mami_dev_info))) {
		hw_err("invalid param. (in=%d; in_len=%u; expect_len=%zu)\n",
			in != NULL, in_len, sizeof(struct mami_dev_info));
		return -EINVAL;
	}

	dev_info = (struct mami_dev_info *)in;
	chnl_type = udev_kern_get_mng_chnl_type(dev_info->chipType);
	if (chnl_type >= CHNL_TYPE_MAX) {
		hw_err("invalid chip_type %u\n", dev_info->chipType);
		return -EINVAL;
	}
	if (udev_get_mng_intf(chnl_type)->hw_probe == NULL) {
		hw_err("chnl_type %u hw probe is NULL\n", chnl_type);
		return -EINVAL;
	}
	return udev_get_mng_intf(chnl_type)->hw_probe(dev_info);
}

static const struct {
	u8 sub_cmd;
	u32 proc_ctrl_bitmap;
	u8 is_dev_handler;
	void *call_fn;
} g_hw_ucd_cmds[] = {
	{HWA_TRIGGER_DEV_PROBE, PROC_CTRL_ANY_USER, 0, hw_adapt_ucd_trigger_probe},
};

static const u32 g_hw_ucd_cmd_num = ARRAY_SIZE(g_hw_ucd_cmds);

static void hw_adapt_unregister_ucd_cmd_num(u32 cmd_num)
{
	u32 i;
	int ret;

	cmd_num = cmd_num > g_hw_ucd_cmd_num ? g_hw_ucd_cmd_num : cmd_num;

	for (i = 0; i < cmd_num; i++) {
		if (g_hw_ucd_cmds[i].is_dev_handler) {
			ret = ucd_unregister_dev_cmd(UCD_CMD_HW_ADAPT, g_hw_ucd_cmds[i].sub_cmd);
		} else {
			ret = ucd_unregister_cmd(UCD_CMD_HW_ADAPT, g_hw_ucd_cmds[i].sub_cmd);
		}
		if (ret != 0) {
			hw_err("ucd unregister fail. (sub_cmd=%d)\n", g_hw_ucd_cmds[i].sub_cmd);
		}
	}
}

void hw_adapt_ucd_cmd_uninit(void)
{
	hw_adapt_unregister_ucd_cmd_num(g_hw_ucd_cmd_num);
}
DECLARE_FEATURE_AUTO_UNINIT(hw_adapt_ucd_cmd_uninit, FEATURE_LOADER_STAGE_9);

int hw_adapt_ucd_cmd_init(void)
{
	u32 i;
	int ret;
	struct tag_ucd_feature feature;

	for (i = 0; i < g_hw_ucd_cmd_num; i++) {
		ucd_fill_feature_cmd(&feature, MODULE_HW, UCD_CMD_HW_ADAPT, g_hw_ucd_cmds[i].sub_cmd, g_hw_ucd_cmds[i].call_fn);
		ucd_set_proc_ctrl_bitmap(&feature, g_hw_ucd_cmds[i].proc_ctrl_bitmap);
		if (g_hw_ucd_cmds[i].is_dev_handler) {
			ret = ucd_register_dev_cmd(&feature);
		} else {
			ret = ucd_register_cmd(&feature);
		}
		if (ret != 0) {
			hw_err("ucd register cmd fail, will rollback. (ret=%d)\n", ret);
			hw_adapt_unregister_ucd_cmd_num(i);
			return ret;
		}
	}
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(hw_adapt_ucd_cmd_init, FEATURE_LOADER_STAGE_9);
