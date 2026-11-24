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
#include <linux/mm.h>
#include <linux/limits.h>

#include "mami_string.h"
#include "udm.h"
#include "udev_hw_pub.h"
#include "fmdevm.h"
#include "ubfabric.h"
#include "mctrlq.h"
#include "ures.h"
#include "ubfabric.h"
#include "udev_msg_pub.h"
#include "udev_ctrl.h"
#include "udev_feature.h"

/* This function is used only by the DPU. */
int udev_cfg_magic_check(struct udev_cfg_hdr *hdr)
{
	if (memcmp(&hdr->magic[0], UDEV_CFG_HDR_MAGIC, UDEV_CFG_HDR_MAGIC_LENGTH) != 0) {
		hw_err("Get hw cfg failed, invalid magic. magic=%.*s\n", UDEV_CFG_HDR_MAGIC_LENGTH, hdr->magic);
		return -EINVAL;
	}

	return 0;
}

/* This function is used only by the DPU. */
STATIC int udev_set_hw_dev_info(u32 devid, u32 chip_num_in_dev, u32 die_num_in_dev, u32 die_num_in_chip, u32 ue_num_in_dev)
{
	int ret;

	if ((chip_num_in_dev == 0) || (die_num_in_dev == 0) || (die_num_in_chip == 0) ||
		(chip_num_in_dev * die_num_in_chip != die_num_in_dev)) {
		hw_err("Invalid chip/die num. (devid=%u; chip_num_in_dev=%u; die_num_in_dev=%u; die_num_in_chip=%u)\n", devid,
			chip_num_in_dev, die_num_in_dev, die_num_in_chip);
		return -EINVAL;
	}
	hw_info("chip/die num. (devid=%u; chip_num_in_dev=%u; die_num_in_dev=%u; die_num_in_chip=%u; ue_num_in_dev=%u)\n", devid,
		chip_num_in_dev, die_num_in_dev, die_num_in_chip, ue_num_in_dev);
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

STATIC int msg_generate_ue_type_info(u32 devid, struct udev_msg_die_cfg *msg_die_cfg, struct udev_die_cfg_kernel *die_cfg)
{
	struct udev_msg_ue_type_rsp *ue_type_rsp = NULL;
	int ret = -EINVAL;
	u32 seg_idx, ue_idx;
	struct msg_cmd cmd = {0};
	struct dev_ue_type_info_cmd ue_type_cmd = {0};
	u32 ue_type_info_size = sizeof(struct udev_msg_ue_type_rsp) + msg_die_cfg->ue_segment_num * sizeof(struct udev_ue_type_seg_rsp);
	struct udm_dev_para para;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get dev attr failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	ue_type_rsp = (struct udev_msg_ue_type_rsp *)kvmalloc(ue_type_info_size, GFP_KERNEL_ACCOUNT);
	if (ue_type_rsp == NULL) {
		hw_err("Get ue_info failed, (devid=%u,size=%u)\n", devid, ue_type_info_size);
		return -ENOMEM;
	}
	ue_type_cmd.ue_type_seg_num = msg_die_cfg->ue_segment_num;
	cmd.cmd_type = MAMI_MSG_TYPE_UE_TYPE_INFO;
	cmd.in_buf = (void *)&ue_type_cmd;
	cmd.in_len = sizeof(struct dev_ue_type_info_cmd);
	cmd.out_buf = (void *)ue_type_rsp;
	cmd.out_len = ue_type_info_size;
	ret = msgq_get_cfg(devid, para.connect_type, &cmd);
	if (ret != 0) {
		hw_err("msg read ue type failed. (ret=%d, devid=%u)\n", ret, devid);
		kvfree(ue_type_rsp);
		return ret;
	}
	for (seg_idx = 0; seg_idx < ue_type_rsp->type_seg_num; seg_idx++) {
		if (ue_type_rsp->type_info[seg_idx].ue_start >= die_cfg->ue_num || ue_type_rsp->type_info[seg_idx].ue_end > die_cfg->ue_num) {
			hw_err("dev %u err seg %u start %u end %u\n", devid, seg_idx, ue_type_rsp->type_info[seg_idx].ue_start, ue_type_rsp->type_info[seg_idx].ue_end);
			continue;
		}
		hw_info("dev %u seg %u start %u end %u type %u sub type %u\n", devid, seg_idx, ue_type_rsp->type_info[seg_idx].ue_start,
			ue_type_rsp->type_info[seg_idx].ue_end, ue_type_rsp->type_info[seg_idx].ue_type, ue_type_rsp->type_info[seg_idx].ue_sub_type);
		for (ue_idx = ue_type_rsp->type_info[seg_idx].ue_start; ue_idx < ue_type_rsp->type_info[seg_idx].ue_end && ue_idx < die_cfg->ue_num; ue_idx++) {
			die_cfg->ue_info[ue_idx].ue_type = ue_type_rsp->type_info[seg_idx].ue_type;
			die_cfg->ue_info[ue_idx].ue_sub_type = ue_type_rsp->type_info[seg_idx].ue_sub_type;
		}
	}
	kvfree(ue_type_rsp);
	return 0;
}
/* This function is used only by the DPU. */
static int msg_generate_ue_info(u32 devid, struct udev_msg_die_cfg *msg_die_cfg, struct udev_die_cfg_kernel *die_cfg, struct udev_ue_res_info *ue_res_info, struct udev_msg_ue_info *ue_info)
{
	u32 ue_idx = 0;
	u32 i, j;
	u64 ctrl_ue_id;
	int ret;

	ue_res_info->mue_num = ue_info->mue_num;
	for (i = 0; i < ue_info->mue_num && ue_idx < die_cfg->ue_num; i++) {
		die_cfg->ue_info[ue_idx].ue_index = ue_idx;
		die_cfg->ue_info[ue_idx].ue_type = ue_info->mue_info[i].mue_type;
		die_cfg->ue_info[ue_idx].ue_sub_type = ue_info->mue_info[i].mue_sub_type;
		die_cfg->ue_info[ue_idx].ue_init = ue_info->mue_info[i].ue_init;
		die_cfg->ue_info[ue_idx].ue_enable = ue_info->mue_info[i].ue_enable;
		die_cfg->ue_info[ue_idx].port_id = ue_info->mue_info[i].port_id;
		die_cfg->ue_info[ue_idx].mue_index = i;
		hw_debug("dev %u mue %u sa0 is 0x%x sa1 is 0x%x sa2 is 0x%x, vue sa0 is 0x%x sa1 is 0x%x sa2 is 0x%x\n",
			devid, i, ue_info->mue_info[i].mue_res_info.mue_uers0_sa_l, ue_info->mue_info[i].mue_res_info.mue_uers1_sa_l, ue_info->mue_info[i].mue_res_info.mue_uers2_sa_l,
			ue_info->mue_info[i].mue_res_info.vue_uers0_sa_l, ue_info->mue_info[i].mue_res_info.vue_uers1_sa_l, ue_info->mue_info[i].mue_res_info.vue_uers2_sa_l);
		(void)ubfm_memcpy(&(ue_res_info->res_info[i]), sizeof(struct udev_mue_res_info),
			&(ue_info->mue_info[i].mue_res_info), sizeof(struct udev_mue_res_info));
		ctrl_ue_id = 1;
		ret = ures_set_key_value(devid, MAMI_CTRL_UE_ID, 1);
		if (ret != 0) {
			hw_err("Set ctrl ue id to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, MAMI_CTRL_UE_ID, ret);
			return ret;
		}
		ue_idx++;
	}
	for (i = 0; i < ue_info->mue_num && ue_idx < die_cfg->ue_num; i++) {
		hw_debug("dev %u ue_num %u mue_idx %u init %u enable %u port_id %u vf start %u vf end %u\n", devid, die_cfg->ue_num, i, ue_info->mue_info[i].ue_init,
			ue_info->mue_info[i].ue_enable, ue_info->mue_info[i].port_id, ue_info->mue_info[i].mue_res_info.vue_start, ue_info->mue_info[i].mue_res_info.vue_end);
		if (ue_info->mue_info[i].ue_init == 0) {
			continue;
		}
		for (j = ue_info->mue_info[i].mue_res_info.vue_start; j <= ue_info->mue_info[i].mue_res_info.vue_end && ue_idx < die_cfg->ue_num; j++) {
			die_cfg->ue_info[ue_idx].ue_index = ue_idx;
			die_cfg->ue_info[ue_idx].ue_type = ue_info->mue_info[i].vue_type;
			die_cfg->ue_info[ue_idx].ue_sub_type = ue_info->mue_info[i].vue_sub_type;
			die_cfg->ue_info[ue_idx].ue_init = ue_info->mue_info[i].vue_init;
			die_cfg->ue_info[ue_idx].ue_enable = ue_info->mue_info[i].vue_enable;
			die_cfg->ue_info[ue_idx].port_id = ue_info->mue_info[i].port_id;
			die_cfg->ue_info[ue_idx].mue_index = i;
			ue_idx++;
		}
	}

	hw_info("ue_segment_num.(devid=%u; seg_num=%u)\n", devid, msg_die_cfg->ue_segment_num);
	if (msg_die_cfg->ue_segment_num > 0) {
		return msg_generate_ue_type_info(devid, msg_die_cfg, die_cfg);
	}
	return 0;
}

/* This function is used only by the DPU. */
STATIC int msgq_get_hw_ue_info(u32 devid, struct udev_msg_cfg_info *cfg_info, struct udev_die_cfg_kernel *die_cfg, struct udev_msg_die_cfg *msg_die_cfg)
{
	struct udev_msg_ue_info *ue_info = NULL;
	struct udev_ue_res_info *res_info = NULL;
	int ret = -EINVAL;
	struct msg_cmd cmd = {0};
	struct dev_ue_info_cmd ue_cmd = {0};
	char udie_res_info_key[URES_NAME_LEN];
	u32 mue_num = msg_die_cfg->mue_num;
	u32 ue_info_size = sizeof(struct udev_msg_ue_info) + mue_num * sizeof(struct udev_msg_mue_info);
	u32 res_info_size = sizeof(struct udev_ue_res_info) + mue_num * sizeof(struct udev_mue_res_info);
	struct udm_dev_para para;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get dev attr failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	if (mue_num > (u32)U8_MAX) {
		hw_err("Invalid mue num. (devid=%u; mue_num=%u; max=%u)\n", devid, mue_num, (u32)U8_MAX);
		return -EINVAL;
	}
	ue_info = (struct udev_msg_ue_info *)kvzalloc(ue_info_size, GFP_KERNEL_ACCOUNT);
	if (ue_info == NULL) {
		hw_err("Get ue_info failed, (devid=%u,size=%u)\n", devid, ue_info_size);
		return -ENOMEM;
	}
	ue_cmd.ue_num = mue_num;
	cmd.cmd_type = MAMI_MSG_TYPE_UE_INFO;
	cmd.in_buf = (void *)&ue_cmd;
	cmd.in_len = sizeof(struct dev_ue_info_cmd);
	cmd.out_buf = (void *)ue_info;
	cmd.out_len = ue_info_size;
	ret = msgq_get_cfg(devid, para.connect_type, &cmd);
	if (ret != 0) {
		hw_err("msg read ue info failed. (ret=%d, devid=%u)\n", ret, devid);
		kvfree(ue_info);
		return ret;
	}
	/* msg_generate_ue_info() walks ue_info[] by this count. */
	if (ue_info->mue_num != mue_num) {
		hw_err("Ue info num mismatch. (devid=%u; requested=%u; received=%u)\n", devid, mue_num, ue_info->mue_num);
		kvfree(ue_info);
		return -EINVAL;
	}
	res_info = (struct udev_ue_res_info *)kvzalloc(res_info_size, GFP_KERNEL_ACCOUNT);
	if (res_info == NULL) {
		hw_err("Get res_info failed, (devid=%u,size=%u)\n", devid, res_info_size);
		kvfree(ue_info);
		return -ENOMEM;
	}

	ret = msg_generate_ue_info(devid, msg_die_cfg, die_cfg, res_info, ue_info);
	if (ret != 0) {
		kvfree(ue_info);
		kvfree(res_info);
		return ret;
	}
	kvfree(ue_info);
	(void)UBFM_SPRINTF(udie_res_info_key, sizeof(udie_res_info_key), MAMI_UDEV_DIEN_RES_CFG, die_cfg->die_id);
	ret = ures_dev_set_attr(devid, udie_res_info_key, res_info, res_info_size);
	if (ret != 0) {
		hw_err("Set res_info to ures failed. (devid=%u;name=%s;ret=%d)\n", devid, udie_res_info_key, ret);
		kvfree(res_info);
		return ret;
	}
	kvfree(res_info);
	return 0;
}

/* This function is used only by the DPU. */
STATIC int msg_generate_die_info(u32 devid, struct udev_die_cfg_kernel *die_cfg, struct udev_msg_die_cfg *msg_die_cfg)
{
	int ret;
	char ctrl_ue_param_key[URES_NAME_LEN];

	die_cfg->die_id = msg_die_cfg->die_id;
	(void)ubfm_memcpy(die_cfg->ubc_ue0_guid, MAMI_GUID_LEN, msg_die_cfg->ubc_ue0_guid, MAMI_GUID_LEN);
	(void)ubfm_memcpy(die_cfg->cdma_ue0_guid, MAMI_GUID_LEN, msg_die_cfg->cdma_ue0_guid, MAMI_GUID_LEN);
	(void)UBFM_SPRINTF(ctrl_ue_param_key, sizeof(ctrl_ue_param_key), MAMI_DIEN_CTRL_UE_GUID, die_cfg->die_id);
	ret = ures_dev_set_attr(devid, ctrl_ue_param_key, msg_die_cfg->cdma_ue0_guid, sizeof(msg_die_cfg->cdma_ue0_guid));
	if (ret != 0) {
		hw_err("Set ctrl ue guid to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, ctrl_ue_param_key, ret);
		return ret;
	}
	die_cfg->port_num = (u8)msg_die_cfg->port_num;
	die_cfg->ue_num = msg_die_cfg->total_ue_num;
	die_cfg->body_mask = (u8)msg_die_cfg->body_mask;
	die_cfg->urma_soft_ue0 = msg_die_cfg->sup_ppp_msg;
	return 0;
}

STATIC int udev_set_hw_pcie_and_body_mask(u32 devid, u32 dieid, u8 body_mask, u32 pcie_mask)
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

/* This function is used only by the DPU. */
int msgq_get_hw_die_info(u32 devid, struct udev_msg_cfg_info *cfg_info)
{
	struct udev_msg_die_cfg msg_die_cfg = {0};
	struct udev_die_cfg_kernel *die_cfg = NULL;
	u32 die_cfg_size = 0;
	char udie_cfg_key[URES_NAME_LEN];
	u32 udie_idx = 0, ue_num = 0;
	int ret = -EINVAL;
	u32 i, j;
	struct msg_cmd cmd = {0};
	struct dev_udie_info_cmd udie_cmd = {0};
	struct udm_dev_para para;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get dev attr failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	if (cfg_info->chip_num > CHIP_MAX_NUM) {
		hw_err("Invalid chip_num. (devid=%u; chip_num=%u; max_val=%u; ret=%d)\n",
									devid, cfg_info->chip_num, CHIP_MAX_NUM, ret);
		return -EINVAL;
	}

	if (cfg_info->chip_udie_num > DEV_DIE_MAX_NUM) {
		hw_err("Invalid chip_udie_num. (devid=%u; chip_udie_num=%u; max_val=%u; ret=%d)\n",
									devid, cfg_info->chip_udie_num, DEV_DIE_MAX_NUM, ret);
		return -EINVAL;
	}

	for (i = 0; i < cfg_info->chip_num; ++i) {
		for (j = 0; j < cfg_info->chip_udie_num; ++j) {
			udie_cmd.udie_idx = j;
			cmd.cmd_type = MAMI_MSG_TYPE_UDIE_INFO;
			cmd.in_buf = (void *)&udie_cmd;
			cmd.in_len = sizeof(struct dev_udie_info_cmd);
			cmd.out_buf = (void *)&msg_die_cfg;
			cmd.out_len = sizeof(struct udev_msg_die_cfg);
			ret = msgq_get_cfg(devid, para.connect_type, &cmd);
			if (ret != 0) {
				hw_err("msg read die info failed. (ret=%d, devid=%u)\n", ret, devid);
				return ret;
			}
			die_cfg_size = sizeof(struct udev_die_cfg_kernel) + msg_die_cfg.total_ue_num * sizeof(struct udev_ue_info);
			die_cfg = kvmalloc(die_cfg_size, GFP_KERNEL_ACCOUNT);
			if (die_cfg == NULL) {
				hw_err("Get die cfg failed, (devid=%u,size=%u)\n", devid, die_cfg_size);
				return -ENOMEM;
			}
			(void)ubfm_memset(die_cfg, die_cfg_size, 0, die_cfg_size);
			ret = msg_generate_die_info(devid, die_cfg, &msg_die_cfg);
			if (ret != 0) {
				kvfree(die_cfg);
				return ret;
			}
			ret = msgq_get_hw_ue_info(devid, cfg_info, die_cfg, &msg_die_cfg);
			if (ret != 0) {
				hw_err("msg read ue info failed. (ret=%d, devid=%u)\n", ret, devid);
				kvfree(die_cfg);
				return ret;
			}
			(void)UBFM_SPRINTF(udie_cfg_key, sizeof(udie_cfg_key), MAMI_UDEV_DIEN_CFG, udie_idx);
			ret = ures_dev_set_attr(devid, udie_cfg_key, die_cfg, die_cfg_size);
			if (ret != 0) {
				hw_err("Set hw cfg to ures failed. (devid=%u;name=%s;ret=%d)\n", devid, udie_cfg_key, ret);
				kvfree(die_cfg);
				return ret;
			}
			ret = udev_set_hw_pcie_and_body_mask(devid, udie_idx, die_cfg->body_mask, die_cfg->pcie_mask);
			if (ret != 0) {
				kvfree(die_cfg);
				return ret;
			}

			hw_info("chip%d die%d, port num %d, ue_num %d, body_mask 0x%x\n",
				i, j, die_cfg->port_num, die_cfg->ue_num, die_cfg->body_mask);
			udie_idx++;
			ue_num += die_cfg->ue_num;
			kvfree(die_cfg);
		}
	}
	hw_info("devid %u chip_num %u, total udie_num %u\n", devid, cfg_info->chip_num, udie_idx);

	return udev_set_hw_dev_info(devid, cfg_info->chip_num, udie_idx, cfg_info->chip_udie_num, ue_num);
}

/* This function is used only by the DPU. */
int udev_get_dev_hw_info_by_msg(u32 devid)
{
	struct udev_msg_cfg_info cfg_info = {0};
	struct udm_dev_para para = {0};
	struct msg_cmd cmd = {0};
	struct dev_cfg_info_cmd dev_cmd = {0};
	struct udev_cfg dev_cfg = {0};
	int ret = 0;

	ret = udm_get_dev_attr(devid, &para);
	if (ret != 0) {
		hw_err("Get dev attr failed. (devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}

	dev_cmd.cna = para.cna;
	dev_cmd.eid = para.eid;
	cmd.cmd_type = MAMI_MSG_TYPE_DEV_INFO;
	cmd.in_buf = (void *)&dev_cmd;
	cmd.in_len = sizeof(struct dev_cfg_info_cmd);
	cmd.out_buf = (void *)&cfg_info;
	cmd.out_len = sizeof(struct udev_msg_cfg_info);
	ret = msgq_get_cfg(devid, para.connect_type, &cmd);
	if (ret != 0) {
		hw_err("msg read dev cfg failed. (ret=%d, dev_id=%u)\n", ret, devid);
		return -EINVAL;
	}
	ret = udev_cfg_magic_check(&cfg_info.cfg_hdr);
	if (ret != 0) {
		return -EINVAL;
	}
	hw_info("udev cfg info. (devid=%u;type=%d;slot_id=%d;socket_id=%d;chip_num %u;total udie_num %u;rec=%d)\n", devid,
		cfg_info.cfg.device_type, cfg_info.cfg.slot_id, cfg_info.cfg.socket_id, cfg_info.chip_num, cfg_info.chip_udie_num, cfg_info.cfg.rec);
	dev_cfg.device_type = (u16)cfg_info.cfg.device_type;
	dev_cfg.slot_id = (u8)cfg_info.cfg.slot_id;
	dev_cfg.socket_id = (u8)cfg_info.cfg.socket_id;
	dev_cfg.rec = (u8)cfg_info.cfg.rec;
	dev_cfg.ras_err_status_block_addr = cfg_info.cfg.ras_err_status_block_addr;
	dev_cfg.ras_err_status_block_len = cfg_info.cfg.ras_err_status_block_len;
	dev_cfg.black_box_addr = cfg_info.cfg.black_box_addr;
	dev_cfg.black_box_len = cfg_info.cfg.black_box_len;
	ret = ures_dev_set_attr(devid, MAMI_UDEV_CFG_INFO, &dev_cfg, sizeof(struct udev_cfg));
	if (ret != 0) {
		hw_err("Set hw dev cfg to ures failed.(devid=%u;name=%s;ret=%d)\n", devid, MAMI_UDEV_CFG_INFO, ret);
		return -EINVAL;
	}

	ret = msgq_get_hw_die_info(devid, &cfg_info);
	if (ret != 0) {
		hw_err("msg read die cfg failed. (ret=%d, dev_id=%u)\n", ret, devid);
		return -EINVAL;
	}
	return 0;
}

/* This function is used only by the DPU. */
int udev_set_dev_info_by_msg(u32 devid)
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
	ret = udev_get_dev_hw_info_by_msg(devid);
	if (ret != 0) {
		hw_err("Get dev hw info failed.(devid=%u;ret=%d)\n", devid, ret);
		return ret;
	}
	return 0;
}

/* This function is used only by the DPU. */
int udev_hw_dev_init_by_msg(u32 devid, int chip_type, int conn_type, int acc_type)
{
	int ret;

	ret = ures_set_key_value(devid, MAMI_ACC_TYPE, (u64)acc_type);
	if (ret != 0) {
		return ret;
	}
	ret = udev_set_dev_info_by_msg(devid);
	if (ret != 0) {
		return ret;
	}
	ret = udev_init_hw_info(devid, chip_type);
	if (ret != 0) {
		return ret;
	}
	return 0;
}
