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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include "mami_string.h"
#include "crc.h"
#include "ubfm_common.h"
#include "fmdevm.h"
#include "ures.h"
#include "ubfabric.h"
#include "ued.h"
#include "fmdevm.h"
#ifdef CFG_UBFM_SUPPORT_CDMA
#include "udev_cdma_pub.h"
#endif /* CFG_UBFM_SUPPORT_CDMA */

#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "fpga_adapter_kern.h"
#endif
#include "udev_map_inner.h"
#include "udev_vsock.h"
#include "udev_ctrl.h"

int udev_set_guid_dev(struct udev_hw_identify *hw)
{
	char guid_str[GUID_STR_LEN];
	struct udev_hw_identify old_hw = {0};
	int ret;

	ret = udev_guid_to_str(hw->guid, guid_str);
	if (ret != 0) {
		hw_err("Trans guid to string failed.\n");
		return ret;
	}
	if (ures_get_attr(guid_str, &old_hw, sizeof(old_hw)) != 0) {
		hw_info("guid add. (chip_type=%u; slot=%u; socket_id=%u; guid=%s)\n",
			hw->chip_type, hw->slot_id, hw->socket_id, guid_str);
	}
	ret = ures_set_attr(guid_str, hw, sizeof(*hw));
	if (ret != 0) {
		hw_err("Set guid attr to ures failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

void show_guid(const char *prefix, const u8 *guid, u32 devid)
{
	char guid_str[GUID_STR_LEN];
	(void)udev_guid_to_str(guid, guid_str);
	hw_debug("%s: %s(devid=%u)\n", prefix, guid_str, devid);
}

int read_file_content(const char *path_str, u32 seek_len, u8 *content, u32 max_len)
{
	ssize_t read_len;
	struct file *fp;

	fp = filp_open(path_str, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		hw_info("file not exist.(path=%s)\n", path_str);
		return -ENOENT;
	}

	fp->f_pos = seek_len;
	read_len = kernel_read(fp, content, max_len - 1, &fp->f_pos);
	if (read_len < 0) {
		hw_err("kernel read fail.(path=%s;ret=%ld)\n", path_str, read_len);
		(void)filp_close(fp, NULL);
		return -EINVAL;
	}
	(void)filp_close(fp, NULL);
	content[read_len] = '\0';

	return 0;
}
struct ncl_mue_info_cfg {
	u32 bitmap : 20,
		vue_num : 8,
		non_main_mue : 1,
		rsvd : 3;
};
#define NCL_MUE_NUM  10
struct ncl_ues_info_cfg {
	u8 mue_num;
	u8 ubus_urma_mue_num;
	u8 rsv0[2];
	struct ncl_mue_info_cfg mue_info[NCL_MUE_NUM];
	u32 ubc_io_port_id_bitmap : 20,
		rsv1 : 12;
};

struct ncl_com_glb_cfg {
	u32 scenario : 2,
		last_nl_work_mode : 2, /* ETH/UB */
		last_nl_dlphy_port_mode : 2,
		controller_plane : 2,
		dcqcn_sw_en : 1,
		rsv0 : 23;
	struct ncl_ues_info_cfg ue_info;
};

/* ncl_com_glb_cfg_v2 and ncl_ues_info_cfg_v2 are only stubbed for use in the esl environment */
#define NCL_MUE_NUM_v2  20
struct ncl_ues_info_cfg_v2 {
	u8 mue_num;
	u8 ubus_urma_mue_num;
	u8 rsv0[2];
	struct ncl_mue_info_cfg mue_info[NCL_MUE_NUM_v2];
	u32 ubc_io_port_id_bitmap : 20,
		rsv1 : 12;
};

struct ncl_com_glb_cfg_v2 {
	u32 scenario : 2,
		last_nl_work_mode : 2, /* ETH/UB */
		last_nl_dlphy_port_mode : 2,
		controller_plane : 2,
		dcqcn_sw_en : 1,
		rsv0 : 23;
	struct ncl_ues_info_cfg_v2 ue_info;
};

#define URMA_MUE_START_INDEX 2
#define URMA_MUE_END_INDEX   17
#define URMA_VUE_START_INDEX 78

#define URMA_MUE_START_INDEX_V2 2
#define URMA_MUE_END_INDEX_V2   21
#define URMA_VUE_START_INDEX_V2 22
#define CDMA_UE_INDEX_V2_ASCEND_V2 1

#define URMA_MUE_START_INDEX_KUNPENG_960 2
#define URMA_MUE_END_INDEX_KUNPENG_960   9
#define URMA_VUE_START_INDEX_KUNPENG_960 18
#define CDMA_UE_INDEX_V2_KUNPENG_960 1

#define URMA_MUE_END_INDEX_SSD   7
#define URMA_VUE_START_INDEX_SSD 8

#define URMA_MUE_NUM_ASCEND_V2 10

STATIC bool is_esl_mode(void)
{
#ifdef MAMI_ESL_MODE
	return true;
#else
	return false;
#endif
}

static int ubcl_to_die_cfg(u32 devid, struct ncl_com_glb_cfg *ubcl, struct udev_die_cfg_from_bios *die_cfg)
{
	u32 global_ue_id;
	u32 global_mue_id_start = URMA_MUE_START_INDEX;
	u32 global_vue_id_start = URMA_VUE_START_INDEX;
	u32 idev_ue_id = 0;
	u32 ncl_mue_id = 0;
	u32 vue_id = 0;
	struct udev_hw_identify hw = {0};
	u32 cdma_ue_index = 1, mue_start_index = URMA_MUE_START_INDEX, mue_end_index = URMA_MUE_END_INDEX;
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw identify failed.(ret=%d, devid=%u)\n", ret, devid);
		return ret;
	}
	die_cfg->ue_info[0].ue_init = 1;
	die_cfg->ue_info[0].ue_type = MAMI_KERN_UE_TYPE_UBC_UE;
	if (hw.chip_type == MAMI_KERN_TYPE_ASCEND_V2) {
		cdma_ue_index = CDMA_UE_INDEX_V2_ASCEND_V2;
		global_mue_id_start = URMA_MUE_START_INDEX_V2;
		global_vue_id_start = URMA_VUE_START_INDEX_V2;
		mue_start_index = URMA_MUE_START_INDEX_V2;
		mue_end_index = URMA_MUE_END_INDEX_V2;
	} else if (hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) {
		cdma_ue_index = CDMA_UE_INDEX_V2_KUNPENG_960;
		global_mue_id_start = URMA_MUE_START_INDEX_KUNPENG_960;
		global_vue_id_start = URMA_VUE_START_INDEX_KUNPENG_960;
		mue_start_index = URMA_MUE_START_INDEX_KUNPENG_960;
		mue_end_index = URMA_MUE_END_INDEX_KUNPENG_960;
	} else if (hw.chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		mue_end_index = URMA_MUE_END_INDEX_SSD;
		global_vue_id_start = URMA_VUE_START_INDEX_SSD;
	}
	die_cfg->ue_info[cdma_ue_index].ue_init = 1;
	die_cfg->ue_info[cdma_ue_index].ue_type = MAMI_KERN_UE_TYPE_CDMA_MUE;
	hw_info("ubcl to die cfg. (mue_num=%u;ubus_mue_num=%u)\n", ubcl->ue_info.mue_num, ubcl->ue_info.ubus_urma_mue_num);
	for (global_ue_id = mue_start_index; global_ue_id <= mue_end_index; global_ue_id++, idev_ue_id++) {
		if (idev_ue_id >= ubcl->ue_info.mue_num) {
			break;
		}
		die_cfg->ue_info[global_ue_id].ue_type = MAMI_KERN_UE_TYPE_URMA_MUE;
		die_cfg->ue_info[global_ue_id].ue_init = 1;
		die_cfg->ue_info[global_ue_id].mue_index = global_ue_id;
		if (idev_ue_id < ubcl->ue_info.ubus_urma_mue_num) {
			die_cfg->ue_info[global_ue_id].ue_enable = 1;
		}
	}
	for (ncl_mue_id = 0; ncl_mue_id < ubcl->ue_info.mue_num; ncl_mue_id++) {
		hw_info("vue info. (idev_mue_id=%u; global_mue_id=%u; vue_num=%u)\n",
			ncl_mue_id, global_mue_id_start, ubcl->ue_info.mue_info[ncl_mue_id].vue_num);
		if (ubcl->ue_info.mue_info[ncl_mue_id].vue_num == 0) {
			global_mue_id_start++;
			continue;
		}
		for (vue_id = 0; (vue_id < ubcl->ue_info.mue_info[ncl_mue_id].vue_num) && (global_vue_id_start < die_cfg->ue_num); vue_id++) {
			die_cfg->ue_info[global_vue_id_start].ue_type = MAMI_KERN_UE_TYPE_URMA_VUE;
			die_cfg->ue_info[global_vue_id_start].ue_init = 1;
			die_cfg->ue_info[global_vue_id_start].mue_index = global_mue_id_start;
			global_vue_id_start++;
		}
		global_mue_id_start++;
	}
	return 0;
}

static int default_ubcl_to_die_cfg(u32 devid, struct udev_die_cfg_from_bios *die_cfg)
{
	u32 global_vue_id_start = URMA_VUE_START_INDEX;
	u32 global_mue_id_start = URMA_MUE_START_INDEX;
	struct ncl_com_glb_cfg_v2 ubcl_obj = {0};
	struct ncl_com_glb_cfg_v2 *ubcl = &ubcl_obj;
	u32 mue_start_index = URMA_MUE_START_INDEX;
	u32 mue_end_index = URMA_MUE_END_INDEX;
	struct udev_hw_identify hw = {0};
	u32 num_mue_with_vue = 0;
	u32 cdma_ue_index = 1;
	u32 idev_ue_id = 0;
	u32 ncl_mue_id = 0;
	u32 global_ue_id;
	u32 vue_id = 0;
	u32 vue_num = 1;
	u32 mue_num = 5;
	int ret;

	num_mue_with_vue = mue_num;
	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw identify failed.(ret=%d, devid=%u)\n", ret, devid);
		return ret;
	}
	if (hw.chip_type == MAMI_KERN_TYPE_ASCEND_V2) {
		cdma_ue_index = CDMA_UE_INDEX_V2_ASCEND_V2;
		global_mue_id_start = URMA_MUE_START_INDEX_V2;
		global_vue_id_start = URMA_VUE_START_INDEX_V2;
		mue_start_index = URMA_MUE_START_INDEX_V2;
		mue_end_index = URMA_MUE_END_INDEX_V2;
		mue_num = URMA_MUE_NUM_ASCEND_V2;
		num_mue_with_vue = 4;   /* only the first 4 mue have vue */
		vue_num = 2;	/* 1 pf maps to 2 vfs */
	} else if (hw.chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) {
		cdma_ue_index = CDMA_UE_INDEX_V2_KUNPENG_960;
		global_mue_id_start = URMA_MUE_START_INDEX_KUNPENG_960;
		global_vue_id_start = URMA_VUE_START_INDEX_KUNPENG_960;
		mue_start_index = URMA_MUE_START_INDEX_KUNPENG_960;
		mue_end_index = URMA_MUE_END_INDEX_KUNPENG_960;
		mue_num = mue_end_index - mue_start_index + 1;
		num_mue_with_vue = mue_num;
	} else if (hw.chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		mue_end_index = URMA_MUE_END_INDEX_SSD;
		global_vue_id_start = URMA_VUE_START_INDEX_SSD;
	}

	hw_info("is esl mode, stub default config by not read stub file\n");
	ubcl->ue_info.ubus_urma_mue_num = 4; /* 4: ubus_urma_mue_num */
	ubcl->ue_info.mue_num = mue_num; /* 5 total urma mue */
	for (ncl_mue_id = 0; ncl_mue_id < num_mue_with_vue; ncl_mue_id++) {
		ubcl->ue_info.mue_info[ncl_mue_id].vue_num = vue_num;
	}

	die_cfg->ue_info[0].ue_init = 1;
	die_cfg->ue_info[0].ue_type = MAMI_KERN_UE_TYPE_UBC_UE;
	die_cfg->ue_info[cdma_ue_index].ue_init = 1;
	die_cfg->ue_info[cdma_ue_index].vport_id = 1;
	die_cfg->ue_info[cdma_ue_index].ue_type = MAMI_KERN_UE_TYPE_CDMA_MUE;

	hw_info("ubcl to die cfg. (mue_num=%u;ubus_mue_num=%u)\n", ubcl->ue_info.mue_num, ubcl->ue_info.ubus_urma_mue_num);
	for (global_ue_id = mue_start_index; global_ue_id <= mue_end_index; global_ue_id++, idev_ue_id++) {
		if (idev_ue_id >= ubcl->ue_info.mue_num) {
			break;
		}

		die_cfg->ue_info[global_ue_id].ue_type = MAMI_KERN_UE_TYPE_URMA_MUE;
		die_cfg->ue_info[global_ue_id].mue_index = global_ue_id;
		die_cfg->ue_info[global_ue_id].vport_id = 0;
		die_cfg->ue_info[global_ue_id].ue_init = 1;
		if (idev_ue_id < ubcl->ue_info.ubus_urma_mue_num) {
			die_cfg->ue_info[global_ue_id].ue_enable = 1;
		}
	}
	for (ncl_mue_id = 0; ncl_mue_id < ubcl->ue_info.mue_num; ncl_mue_id++) {
		hw_info("vue info. (idev_mue_id=%u; global_mue_id=%u; vue_num=%u)\n",
			ncl_mue_id, global_mue_id_start, ubcl->ue_info.mue_info[ncl_mue_id].vue_num);
		if (ubcl->ue_info.mue_info[ncl_mue_id].vue_num == 0) {
			global_mue_id_start++;
			continue;
		}
		for (vue_id = 0; vue_id < ubcl->ue_info.mue_info[ncl_mue_id].vue_num; vue_id++) {
			die_cfg->ue_info[global_vue_id_start].ue_type = MAMI_KERN_UE_TYPE_URMA_VUE;
			die_cfg->ue_info[global_vue_id_start].mue_index = global_mue_id_start;
			die_cfg->ue_info[global_vue_id_start].ue_init = 1;
			global_vue_id_start++;
		}
		global_mue_id_start++;
	}
	return 0;
}

#define UBCL_NCL_OFFSET 1028
/* esl have not imu, so fill the stub values */
static int stub_guid_ue_info(u32 devid, u32 udie_idx, struct udev_die_cfg_from_bios *die_cfg)
{
	char path_str[128]; /**** 128 is path str len ***/
	u8 file_content[256]; /**** 256 is file content len ***/
	int ret;

	(void)UBFM_SPRINTF(path_str, sizeof(path_str), "/home/stub_dev%u_die%u_urma_guid", devid, udie_idx);
	ret = read_file_content(path_str, 0, file_content, sizeof(file_content));
	if (ret == 0) {
		ret = udev_str_to_guid(file_content, die_cfg->urma_ue0_guid);
		hw_info("udev_str_to_guid.(devid=%u;die_idx=%u;file=%s;content=%s;ret=%d)\n",
			devid, udie_idx, path_str, file_content, ret);
	}

	(void)UBFM_SPRINTF(path_str, sizeof(path_str), "/home/stub_dev%u_die%u_cdma_guid", devid, udie_idx);
	ret = read_file_content(path_str, 0, file_content, sizeof(file_content));
	if (ret == 0) {
		ret = udev_str_to_guid(file_content, die_cfg->cdma_ue0_guid);
		hw_info("udev_str_to_guid.(devid=%u;die_idx=%u;file=%s;content=%s;ret=%d)\n",
			devid, udie_idx, path_str, file_content, ret);
	}

	(void)UBFM_SPRINTF(path_str, sizeof(path_str), "/home/stub_dev%u_die%u_ubcl", devid, udie_idx);
	ret = read_file_content(path_str, UBCL_NCL_OFFSET, file_content, sizeof(file_content));
	if (ret == 0) {
		ret = ubcl_to_die_cfg(devid, (struct ncl_com_glb_cfg *)file_content, die_cfg);
		if (ret != 0) {
			hw_err("ubcl to die cfg failed.(ret=%d, devid=%u)\n", ret, devid);
			return ret;
		}
	} else if (is_esl_mode()) {
		ret = default_ubcl_to_die_cfg(devid, die_cfg);
		if (ret != 0) {
			hw_err("default ubcl to die cfg failed.(ret=%d, devid=%u)\n", ret, devid);
			return ret;
		}
	}
	return 0;
}

STATIC int udev_hw_set_dev_info(u32 devid, u32 chip_num_in_dev, u32 die_num_in_dev, u32 die_num_in_chip, u32 ue_num_in_dev)
{
	int ret;

	if ((chip_num_in_dev == 0) || (die_num_in_dev == 0) || (die_num_in_chip == 0) ||
		(chip_num_in_dev * die_num_in_chip != die_num_in_dev)) {
		hw_err("Invalid chip/die num. (devid=%u; chip_num_in_dev=%u; die_num_in_dev=%u; die_num_in_chip=%u)\n", devid,
			chip_num_in_dev, die_num_in_dev, die_num_in_chip);
		return -EINVAL;
	}

	ret = ures_set_key_value(devid, MAMI_CTRL_UE_ID, 1);
	if (ret != 0) {
		hw_err("Failed to set ctrl ue id in device. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	hw_info("set ctrl ue id in device. (devid=%u; ret=%d)\n", devid, ret);
	ret = ures_set_key_value(devid, MAMI_CHIP_NUM, chip_num_in_dev);
	if (ret != 0) {
		hw_err("Failed to set chip_num in device. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_set_key_value(devid, MAMI_DIE_NUM, die_num_in_dev);
	if (ret != 0) {
		hw_err("Failed to set die_num in device. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_set_key_value(devid, MAMI_DIE_NUM_PER_CHIP, die_num_in_chip);
	if (ret != 0) {
		hw_err("Failed to set die_num in chip. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_set_key_value(devid, MAMI_UE_NUM, ue_num_in_dev);
	if (ret != 0) {
		hw_err("Failed to set ue_num in dev. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	return 0;
}

static int udev_hw_set_save_bios_udie_info(u32 devid, const char *udie_cfg_key, struct udev_die_cfg_from_bios *bios_die_cfg)
{
	struct udev_die_cfg_kernel *die_cfg = NULL;
	int ret = -EINVAL;
	u32 die_cfg_size = 0;
	u16 i = 0;

	if (udie_cfg_key == NULL || bios_die_cfg == NULL) {
		return ret;
	}
	if (bios_die_cfg->ue_num > UE_MAX_NUM) {
		hw_err("Err ue num(max ue num = %u, ue num = %u)\n", UE_MAX_NUM, bios_die_cfg->ue_num);
		return -EINVAL;
	}
	die_cfg_size = sizeof(struct udev_die_cfg_kernel) + sizeof(struct udev_ue_info) * bios_die_cfg->ue_num;
	die_cfg = vzalloc(die_cfg_size);
	if (die_cfg == NULL) {
		hw_err("Get hw die cfg failed, alloc mem buf failed(devid=%u)\n", devid);
		return -ENOMEM;
	}
	die_cfg->die_id = bios_die_cfg->die_id;
	(void)ubfm_memcpy(die_cfg->ubc_ue0_guid, MAMI_GUID_LEN, bios_die_cfg->ubc_ue0_guid, MAMI_GUID_LEN);
	(void)ubfm_memcpy(die_cfg->cdma_ue0_guid, MAMI_GUID_LEN, bios_die_cfg->cdma_ue0_guid, MAMI_GUID_LEN);
	(void)ubfm_memcpy(die_cfg->urma_ue0_guid, MAMI_GUID_LEN, bios_die_cfg->urma_ue0_guid, MAMI_GUID_LEN);
	die_cfg->cdma_soft_ue0 = bios_die_cfg->cdma_soft_ue0;
	die_cfg->urma_soft_ue0 = bios_die_cfg->urma_soft_ue0;
	die_cfg->port_num = bios_die_cfg->port_num;
	die_cfg->ub_io_decoder_hpa_base = bios_die_cfg->ub_io_decoder_hpa_base;
	die_cfg->ub_io_decoder_hpa_size = bios_die_cfg->ub_io_decoder_hpa_size;
	die_cfg->ub_memory_version = bios_die_cfg->ub_memory_version;
	die_cfg->body_mask = bios_die_cfg->body_mask;
	(void)ubfm_memcpy(die_cfg->mar_info, sizeof(die_cfg->mar_info), bios_die_cfg->mar_info, sizeof(bios_die_cfg->mar_info));
	die_cfg->decoder_entry_num = bios_die_cfg->decoder_entry_num;
	die_cfg->ub_memory_token_id_max = bios_die_cfg->ub_memory_token_id_max;
	die_cfg->pcie_mask = bios_die_cfg->pcie_mask;
	(void)ubfm_memcpy(die_cfg->rsvd, sizeof(die_cfg->rsvd), bios_die_cfg->rsvd, sizeof(bios_die_cfg->rsvd));
	die_cfg->ue_num = bios_die_cfg->ue_num;
	for (i = 0; i < die_cfg->ue_num; i++) {
		die_cfg->ue_info[i].ue_index = bios_die_cfg->ue_info[i].ue_index;
		die_cfg->ue_info[i].ue_type = bios_die_cfg->ue_info[i].ue_type;
		die_cfg->ue_info[i].ue_init = bios_die_cfg->ue_info[i].ue_init;
		die_cfg->ue_info[i].ue_enable = bios_die_cfg->ue_info[i].ue_enable;
		die_cfg->ue_info[i].port_id = bios_die_cfg->ue_info[i].port_id;
		die_cfg->ue_info[i].mue_index = bios_die_cfg->ue_info[i].mue_index;
		die_cfg->ue_info[i].devue_id = bios_die_cfg->ue_info[i].devue_id;
		die_cfg->ue_info[i].vport_id = bios_die_cfg->ue_info[i].vport_id;
		die_cfg->ue_info[i].vport_enable = bios_die_cfg->ue_info[i].vport_enable;
		(void)ubfm_memcpy(die_cfg->ue_info[i].ue_resv, sizeof(die_cfg->ue_info[i].ue_resv), bios_die_cfg->ue_info[i].ue_resv, sizeof(bios_die_cfg->ue_info[i].ue_resv));
	}
	hw_info("save bios die cfg info, (devid=%u, ue_num=%u %u %u %u)\n", devid, die_cfg->ue_num, (u32)sizeof(struct udev_die_cfg_kernel), (u32)sizeof(struct udev_ue_info), die_cfg_size);
	ret = ures_dev_set_attr(devid, udie_cfg_key, die_cfg, die_cfg_size);
	show_guid("ubc ue0 guid", die_cfg->ubc_ue0_guid, devid);
	show_guid("cdma ue0 guid", die_cfg->cdma_ue0_guid, devid);
	show_guid("urma ue0 guid", die_cfg->urma_ue0_guid, devid);
	hw_info("cdma ue0 %d, urma ue0 %d, port num %d, hpa 0x%llx:0x%llx, ue_num %d, body_mask 0x%x\n",
		die_cfg->cdma_soft_ue0,
		die_cfg->urma_soft_ue0,
		die_cfg->port_num,
		die_cfg->ub_io_decoder_hpa_base,
		die_cfg->ub_io_decoder_hpa_size,
		die_cfg->ue_num,
		die_cfg->body_mask);

	for (i = 0; i < UDEV_MAR_MAX_NUM; i++) {
		struct udev_mar_info *mar_info = &die_cfg->mar_info[i];

		hw_debug("maridx%d, cc 0x%x:0x%x, nc 0x%x:0x%x\n",
			i,
			mar_info->ub_mem_decoder_hpa_cc_base,
			mar_info->ub_mem_decoder_hpa_cc_size,
			mar_info->ub_mem_decoder_hpa_nc_base,
			mar_info->ub_mem_decoder_hpa_nc_size);
	}
	vfree(die_cfg);
	return ret;
}

STATIC int udev_hw_set_pcie_and_body_mask(u32 devid, u32 dieid, u8 body_mask, u32 pcie_mask)
{
	int ret;
	char key_name[URES_NAME_LEN] = {0};

	(void)UBFM_SPRINTF(key_name, sizeof(key_name), MAMI_DIEN_BODY_MASK, dieid);
	ret = ures_set_key_value(devid, key_name, (u64)body_mask);
	if (ret != 0) {
		return ret;
	}

	(void)UBFM_SPRINTF(key_name, sizeof(key_name), MAMI_DIEN_PCIE_MASK, dieid);
	ret = ures_set_key_value(devid, key_name, (u64)pcie_mask);
	if (ret != 0) {
		return ret;
	}

	hw_info("Set pcie_mask and body_mask success. (devid=%u; dieid=%u; pcie_mask=0x%x; body_mask=0x%hhx)\n",
		devid, dieid, pcie_mask, body_mask);
	return 0;
}

static int udev_hw_set_udie_info(u32 devid, struct udev_chip_info *chip_info)
{
	struct udev_die_cfg_from_bios *bios_die_cfg  = NULL;
	struct udev_chip *chip = NULL;
	char udie_cfg_key[URES_NAME_LEN];
	u32 udie_idx = 0, ue_num = 0;
	int ret = -EINVAL;
	u8 i, j, k;

	for (i = 0; i < chip_info->chip_num; ++i) {
		chip = chip_info->chip[i];
		bios_die_cfg = (void *)(chip->die_info);

		for (j = 0; j < chip->udie_num; ++j) {
			show_guid("ori cdma ue0 guid", bios_die_cfg->cdma_ue0_guid, devid);
			show_guid("ori urma ue0 guid", bios_die_cfg->urma_ue0_guid, devid);
			if (is_esl_mode()) {
				ret = stub_guid_ue_info(devid, udie_idx, bios_die_cfg);
				if (ret != 0) {
					hw_err("stub guid ue info failed.(devid=%u; ret=%d)\n", devid, ret);
					return ret;
				}
			}
			(void)UBFM_SPRINTF(udie_cfg_key, sizeof(udie_cfg_key), MAMI_UDEV_DIEN_CFG, udie_idx);
			ret = udev_hw_set_save_bios_udie_info(devid, udie_cfg_key, bios_die_cfg);
			if (ret != 0) {
				hw_err("Set hw cfg to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, udie_cfg_key, ret);
				return ret;
			}
			(void)UBFM_SPRINTF(udie_cfg_key, sizeof(udie_cfg_key), MAMI_DIEN_CTRL_UE_GUID, udie_idx);
			ret = ures_dev_set_attr(devid, udie_cfg_key, bios_die_cfg->cdma_ue0_guid, sizeof(bios_die_cfg->cdma_ue0_guid));
			if (ret != 0) {
				hw_err("Set ctrl ue guid to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, udie_cfg_key, ret);
				return ret;
			}
			show_guid("ubc ue0 guid", bios_die_cfg->ubc_ue0_guid, devid);
			show_guid("cdma ue0 guid", bios_die_cfg->cdma_ue0_guid, devid);
			show_guid("urma ue0 guid", bios_die_cfg->urma_ue0_guid, devid);
			hw_info("chip%d die%d, cdma ue0 %d, urma ue0 %d, port num %d, hpa 0x%llx:0x%llx, ue_num %d\n",
				i, j, bios_die_cfg->cdma_soft_ue0, bios_die_cfg->urma_soft_ue0, bios_die_cfg->port_num,
				bios_die_cfg->ub_io_decoder_hpa_base, bios_die_cfg->ub_io_decoder_hpa_size, bios_die_cfg->ue_num);

			ret = udev_hw_set_pcie_and_body_mask(devid, udie_idx, bios_die_cfg->body_mask, bios_die_cfg->pcie_mask);
			if (ret != 0) {
				return ret;
			}

			for (k = 0; k < UDEV_MAR_MAX_NUM; k++) {
				struct udev_mar_info *mar_info = &bios_die_cfg->mar_info[k];

				hw_debug("chip%d die%d maridx%d, cc 0x%x:0x%x, nc 0x%x:0x%x\n", i, j, k,
					mar_info->ub_mem_decoder_hpa_cc_base, mar_info->ub_mem_decoder_hpa_cc_size,
					mar_info->ub_mem_decoder_hpa_nc_base, mar_info->ub_mem_decoder_hpa_nc_size);
			}
			ue_num += bios_die_cfg->ue_num;
			udie_idx++;

			bios_die_cfg = (void *)bios_die_cfg + (sizeof(struct udev_die_cfg_from_bios) +
				(sizeof(struct udev_ue_info_from_bios) * bios_die_cfg->ue_num));
		}
	}
	hw_debug("hw info.(devid=%u;chip_num=%u;total udie_num=%u)\n", devid, chip_info->chip_num, udie_idx);

	return udev_hw_set_dev_info(devid, chip_info->chip_num, udie_idx, chip_info->chip[0]->udie_num, ue_num);
}

static int get_hw_chip_info(void *mem, u16 cfg_hdr_length, struct udev_chip_info *chip_info)
{
	struct udev_die_cfg_from_bios *bios_die_cfg = NULL;
	struct udev_cfg_info *cfg_info = NULL;
	struct udev_chip *chip = NULL;
	u8 i, j, *chip_num = NULL;
	u16 die_info_size;
	u32 cfg_info_size = 0;
	u16 need_access_length = sizeof(struct udev_cfg_hdr) + sizeof(struct udev_cfg_info) + sizeof(u8);  /* u8: chip number */

	if (need_access_length > cfg_hdr_length) {
		hw_err("Get udev_cfg_hdr and udev_cfg_info failed. (access_length=0x%x; hdr_length=0x%x)\n",
			need_access_length, cfg_hdr_length);
		return -EINVAL;
	}

	cfg_info = (struct udev_cfg_info *)((char *)mem + sizeof(struct udev_cfg_hdr));
	chip_num = (u8 *)cfg_info + sizeof(struct udev_cfg_info);
	if (((*chip_num) == 0) || ((*chip_num) > CHIP_MAX_NUM)) {
		hw_err("chip_num is invalid. (chip_num=%u; CHIP_MAX_NUM=%u)\n", *chip_num, CHIP_MAX_NUM);
		return -EINVAL;
	}
	chip_info->chip_num = *chip_num;
	chip = (struct udev_chip *)(chip_num + sizeof(u8));
	for (i = 0; i < chip_info->chip_num; ++i) {
		need_access_length += sizeof(u8) + sizeof(u8);  /* u8: chip_index, u8: udie_num */
		if (need_access_length > cfg_hdr_length) {
			hw_err("Get chip_index and udie_num failed. (access_length=0x%x; hdr_length=0x%x;"
				"chip_num=%u; chip_index=%u)\n", need_access_length, cfg_hdr_length, chip_info->chip_num, i);
			return -EINVAL;
		}

#if defined(__aarch64__)
		isb();
#else
		mb();  /* Prevent reordering of the udie_num access before the length check. */
#endif

		if (chip->udie_num > DEV_DIE_MAX_NUM) {
			hw_err("invalid udie_num. (udie_num=%u)\n", chip->udie_num);
			return -EINVAL;
		}

		die_info_size = 0;
		bios_die_cfg = (void *)(chip->die_info);
		for (j = 0; j < chip->udie_num; j++) {
			if (bios_die_cfg->ue_num > UE_MAX_NUM) {
				hw_err("Invalid ue_num. (udie_num=%u; ue_num=%u)\n", j, bios_die_cfg->ue_num);
				return -EINVAL;
			}
			die_info_size += (sizeof(struct udev_die_cfg_from_bios) +
				(sizeof(struct udev_ue_info_from_bios) * bios_die_cfg->ue_num));
			bios_die_cfg = (void *)bios_die_cfg + (sizeof(struct udev_die_cfg_from_bios) +
				(sizeof(struct udev_ue_info_from_bios) * bios_die_cfg->ue_num));
		}
		if (USHRT_MAX - need_access_length < die_info_size) {
			hw_err("die_info_size is invalid. (access_length=0x%x; die_info_size=0x%x)\n",
				need_access_length,
				die_info_size);
			return -EINVAL;
		}
		need_access_length += die_info_size;
		if (need_access_length > cfg_hdr_length) {
			hw_err("Get chip_index and udie_num failed. (access_length=0x%x; hdr_length=0x%x;"
				"chip_num=%u; chip_index=%u; udie_num %u)\n", need_access_length, cfg_hdr_length,
				chip_info->chip_num, i, chip->udie_num);
			return -EINVAL;
		}
		chip_info->chip[i] = chip;
		chip = (void *)(chip->die_info) + die_info_size;
	}

	chip_info->reserved = (u8 *)chip;
	cfg_info_size = chip_info->reserved + UDEV_CHIP_RESV_SIZE - (u8 *)mem;
	if (cfg_info_size != cfg_hdr_length) {
		hw_err("Get hw cfg failed. (cfg_size=0x%x;expect=0x%x)\n", cfg_info_size, cfg_hdr_length);
		return -EINVAL;
	}
	return 0;
}

static int udev_cfg_hdr_check(u32 devid, struct udev_cfg_hdr *hdr, u32 max_size)
{
	if (memcmp(&hdr->magic[0], UDEV_CFG_HDR_MAGIC, UDEV_CFG_HDR_MAGIC_LENGTH) != 0) {
		hw_err("Get hw cfg failed, invalid magic. (devid=%u;magic=%u)\n", devid, hdr->magic[0]);
		return -EEXIST;
	}

	if ((hdr->length < sizeof(struct udev_cfg_hdr)) || (hdr->length > max_size)) {
		hw_err("Get hw cfg failed, invalid length 0x%x. (devid=%u)\n", hdr->length, devid);
		return -EINVAL;
	}

	return 0;
}

static int get_resv_mem_base(u64 in_addr, u32 size, u64 *out_addr)
{
	int ret;
	struct addr_seg *seg = NULL;
	u32 cnt = 0;

	if (udev_get_env_mode() == ENV_VSOCK_CLIENT) {
		ret = udev_get_gpa_list(in_addr, size, &seg, &cnt);
		if (ret != 0) {
			return ret;
		}
		*out_addr = seg[0].gpa_start;
		put_gpa_list(seg);
		return 0;
	}

	*out_addr = in_addr;
	return 0;
}

/* Note：The memory pointed to the cfg_info should be freed by the caller by calling vfree */
static int get_hw_info_from_resv_mem(u32 devid, u64 resv_mem_base, u32 size, void **cfg_info,
	struct udev_chip_info *chip_info)
{
	struct udev_cfg_hdr cfg_hdr = {0};
	u16 crc = 0;
	int ret = 0;
	void __iomem *reg_addr = NULL;
	void *cfg_mem = NULL;
	u64 base_addr;

	if (size < sizeof(struct udev_cfg_hdr)) {
		hw_err("Get hw cfg failed, invalid size 0x%x(devid=%u)\n", size, devid);
		return -EINVAL;
	}

	ret = get_resv_mem_base(resv_mem_base, size, &base_addr);
	if (ret != 0) {
		return ret;
	}

	reg_addr = ioremap(base_addr, size);
	if (reg_addr == NULL) {
		hw_err("Get hw cfg failed, ioremap error(devid=%u)\n", devid);
		return -ENOMEM;
	}

	/* Copy configuration information from reserved memory(typically SRAM) to avoid byte alignment
	 * exception during access.
	 */
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	ret = ldst_back_mem_read_kern(devid, (u64)(uintptr_t)reg_addr, &cfg_hdr, sizeof(struct udev_cfg_hdr));
	if (ret != 0) {
		goto fail;
	}
#else
	memcpy_fromio(&cfg_hdr, reg_addr, sizeof(struct udev_cfg_hdr));
#endif
	ret = udev_cfg_hdr_check(devid, &cfg_hdr, size);
	if (ret != 0) {
		goto fail;
	}

	cfg_mem = vzalloc(cfg_hdr.length);
	if (cfg_mem == NULL) {
		hw_err("Get hw cfg failed, alloc mem buf failed(devid=%u)\n", devid);
		ret = -ENOMEM;
		goto fail;
	}

	/* Copy configuration information from reserved memory(typically SRAM) to avoid byte alignment
	 * exception during access.
	 */
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	ret = ldst_back_mem_read_kern(devid, (u64)(uintptr_t)reg_addr, cfg_mem, cfg_hdr.length);
	if (ret != 0) {
		vfree(cfg_mem);
		goto fail;
	}
#else
	memcpy_fromio(cfg_mem, reg_addr, cfg_hdr.length);
#endif
	ret = crc16_cc(cfg_mem + sizeof(u16), cfg_hdr.length - sizeof(u16), &crc);
	if ((ret != 0) || (crc != cfg_hdr.crc)) {
		hw_err("Get hw cfg crc check failed. (crc=0x%x; devid=%u; ret=%d)\n", crc, devid, ret);
		ret = -EACCES;
		vfree(cfg_mem);
		goto fail;
	}

	ret = get_hw_chip_info(cfg_mem, cfg_hdr.length, chip_info);
	if (ret != 0) {
		hw_err("Get hw cfg failed. (expect=0x%x;ret=%d;devid=%u;)\n", cfg_hdr.length, ret, devid);
		vfree(cfg_mem);
		goto fail;
	}

	*cfg_info = cfg_mem;

fail:
	iounmap(reg_addr);
	return ret;
}
#ifdef CFG_UBFM_SUPPORT_CDMA
static int cdma_get_hw_info_from_resv_mem(u32 devid, u64 resv_mem_base, u32 size,
	void **cfg_info, struct udev_chip_info *chip_info)
{
	struct udev_cfg_hdr cfg_hdr = {0};
	void *cfg_mem = NULL;
	u16 crc = 0;
	int ret = 0;

	if (size < sizeof(struct udev_cfg_hdr)) {
		hw_err("Get hw cfg failed, invalid size. (size=0x%x; devid=%u)\n", size, devid);
		return -EINVAL;
	}

	ret = udev_cdma_init_dev(devid);
	if (ret != 0) {
		return ret;
	}

	ret = cdma_dev_read(devid, resv_mem_base, (void *)&cfg_hdr, sizeof(struct udev_cfg_hdr));
	if (ret != 0) {
		hw_err("Read resv mem cfg hdr by cdma failed. (devid=%u; ret=%d)\n", devid, ret);
		goto fail;
	}

	ret = udev_cfg_hdr_check(devid, &cfg_hdr, size);
	if (ret != 0) {
		goto fail;
	}

	cfg_mem = vzalloc(cfg_hdr.length);
	if (cfg_mem == NULL) {
		hw_err("Get hw cfg failed, alloc mem buf failed(devid=%u)\n", devid);
		ret = -ENOMEM;
		goto fail;
	}

	ret = cdma_dev_read(devid, resv_mem_base, (void *)cfg_mem, cfg_hdr.length);
	if (ret != 0) {
		hw_err("Read resv mem by cdma failed. (devid=%u;ret=%d)\n", devid, ret);
		vfree(cfg_mem);
		goto fail;
	}
	ret = crc16_cc(cfg_mem + sizeof(u16), cfg_hdr.length - sizeof(u16), &crc);
	if ((ret != 0) || (crc != cfg_hdr.crc)) {
		hw_err("Get hw cfg crc check failed. (crc=0x%x; devid=%u; ret=%d)\n", crc, devid, ret);
		ret = -EACCES;
		vfree(cfg_mem);
		goto fail;
	}

	ret = get_hw_chip_info(cfg_mem, cfg_hdr.length, chip_info);
	if (ret != 0) {
		hw_err("Get hw cfg failed. (expect=0x%x;ret=%d;devid=%u;)\n", cfg_hdr.length, ret, devid);
		vfree(cfg_mem);
		goto fail;
	}

	*cfg_info = cfg_mem;

fail:
	udev_cdma_uninit_dev(devid);
	return ret;
}
#endif /* CFG_UBFM_SUPPORT_CDMA */

int udev_get_dev_hw_info(u32 devid, enum udev_acc_type type, u64 resv_mem_base, u32 size)
{
	struct udev_cfg_info *cfg_info = NULL;
	struct udev_chip_info chip_info = {0};
	void *cfg_mem = NULL;
	int ret = -EINVAL;
	u8 i;

	if (type == UDEV_CDMA) {
#ifdef CFG_UBFM_SUPPORT_CDMA
		ret = cdma_get_hw_info_from_resv_mem(devid, resv_mem_base, size, &cfg_mem, &chip_info);
#else
		ret = -E_OPNOTSUPP_ON_DEV;
#endif
	} else {
		ret = get_hw_info_from_resv_mem(devid, resv_mem_base, size, &cfg_mem, &chip_info);
	}
	if (ret != 0) {
		hw_err("ubdev get dev hw info from resv mem failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_dev_set_attr(devid, MAMI_UDEV_CFG_HDR, cfg_mem, sizeof(struct udev_cfg_hdr));
	if (ret != 0) {
		hw_err("Set hw dev cfg hdr to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, MAMI_UDEV_CFG_HDR, ret);
		goto failed;
	}

	cfg_info = (struct udev_cfg_info *)(cfg_mem + sizeof(struct udev_cfg_hdr));
	hw_debug("udev cfg info.(devid=%u;type=%d; slot_id=%d; socket_id=%d; rec=%d)\n", devid,
		cfg_info->cfg.device_type, cfg_info->cfg.slot_id, cfg_info->cfg.socket_id, cfg_info->cfg.rec);
	hw_debug("udev cfg info.(devid=%u;ras_err=0x%llx:0x%x; black_box=0x%llx:0x%x)\n", devid,
		cfg_info->cfg.ras_err_status_block_addr, cfg_info->cfg.ras_err_status_block_len,
		cfg_info->cfg.black_box_addr, cfg_info->cfg.black_box_len);
	ret = ures_dev_set_attr(devid, MAMI_UDEV_CFG_INFO, &cfg_info->cfg, sizeof(struct udev_cfg));
	if (ret != 0) {
		hw_err("Set hw dev cfg to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, MAMI_UDEV_CFG_INFO, ret);
		goto failed;
	}

	hw_debug("udev io info.(resv_base=0x%llx;resv_size=0x%x;devid=%u)\n",
		cfg_info->ub_io.resv_mem_base, cfg_info->ub_io.resv_mem_len, devid);
	if (cfg_info->ub_io.reg_count > UB_IO_REG_NUM) {
		hw_err("hw cfg reg count is invalid.(devid=%u; reg_count=%u; max reg_count=%u)\n",
			devid, cfg_info->ub_io.reg_count, UB_IO_REG_NUM);
		ret = -EINVAL;
		goto failed;
	}
	for (i = 0; i < cfg_info->ub_io.reg_count; i++) {
		hw_debug("udev io info reg%d base 0x%llx, size 0x%x(devid=%u)\n", i,
			cfg_info->ub_io.regs[i].start_base, cfg_info->ub_io.regs[i].start_len, devid);
	}
	ret = ures_dev_set_attr(devid, MAMI_UDEV_IO_INFO, &cfg_info->ub_io, sizeof(struct ub_io_info));
	if (ret != 0) {
		hw_err("Set hw cfg to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, MAMI_UDEV_IO_INFO, ret);
		goto failed;
	}

	ret = udev_hw_set_udie_info(devid, &chip_info);
	if (ret != 0) {
		hw_err("set udie info failed.(devid=%u;ret=%d)\n", devid, ret);
	}

failed:
	vfree(cfg_mem);
	return ret;
}

int udev_get_hw_info(u32 devid, u64 resv_mem_base, u32 size, struct udev_hw_identify *hw)
{
	struct udev_cfg_info *cfg_info = NULL;
	struct udev_chip_info chip_info = {0};
	char guid_str[GUID_STR_LEN];
	void *cfg_mem = NULL;
	int ret;

	ret = get_hw_info_from_resv_mem(devid, resv_mem_base, size, &cfg_mem, &chip_info);
	if (ret != 0) {
		hw_err("Get hw info from reserve memory failed(ret=%d)\n", ret);
		return ret;
	}

	cfg_info = (struct udev_cfg_info *)((char *)cfg_mem + sizeof(struct udev_cfg_hdr));
	hw->slot_id = cfg_info->cfg.slot_id;
	hw->socket_id = cfg_info->cfg.socket_id;
	hw->chip_type = (u32)cfg_info->cfg.device_type;
	if (chip_info.chip[0] != NULL) {
		(void)ubfm_memcpy(hw->guid, MAMI_GUID_LEN, chip_info.chip[0]->die_info[0].ubc_ue0_guid, MAMI_GUID_LEN);
	}
	hw->resv_mem_base = resv_mem_base;
	hw->resv_mem_size = size;
	(void)udev_guid_to_str(hw->guid, guid_str);
	hw_info_limit("Get hw cfg info. (devid=%u;type=%u;slot_id=%u;socket_id=%u;guid=%s)\n", devid, hw->chip_type,
			hw->slot_id, hw->socket_id, guid_str);

	vfree(cfg_mem);
	return 0;
}

int udev_ctrl_set_dev_info(u32 devid)
{
	struct udev_hw_identify hw = {0};
	char guid_str[GUID_STR_LEN];
	char cna_eid_key[URES_NAME_LEN];
	struct udm_dev_para para;
	u64 cna_eid;
	int ret;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get dev attr failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	cna_eid = (((u64)para.cna << 32) | (u64)para.eid);  /* cna in high 32 bit */
	(void)UBFM_SPRINTF(cna_eid_key, sizeof(cna_eid_key), MAMI_UDEV_DIEN_CNA_EID, 0);
	ret = ures_set_key_value(devid, cna_eid_key, cna_eid);
	if (ret != 0) {
		hw_err("Set cna eid failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	ret = udev_guid_to_str(para.guid, guid_str);
	if (ret != 0) {
		hw_err("Guid to str failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ures_get_attr(guid_str, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get attr failed.(devid=%u;guid_str=%s;ret=%d)\n", devid, guid_str, ret);
		return ret;
	}

	ret = ures_dev_set_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Set dev attr failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	ret = udev_get_dev_hw_info(devid, hw.access_type, hw.resv_mem_base, hw.resv_mem_size);
	if (ret != 0) {
		hw_err("Get dev hw info failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	return 0;
}
