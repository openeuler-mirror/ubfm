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
#include <linux/mm.h>
#include <linux/ktime.h>

#include "ubfm_common.h"
#include "fmdevm.h"
#include "fmdevm.h"
#include "uop.h"
#include "idm.h"
#include "crc.h"
#include "ras_def.h"
#include "ras_log.h"
#include "ras_ctx.h"
#include "ras_local.h"
#include "uid.h"
#include "ras_protoc.h"
#include "ras_core.h"
#include "mami_string.h"

STATIC int ras_get_mem_addr(u32 devid, struct ures_addr_info *addr)
{
	struct ures_inst inst;
	int ret;

	ures_inst_pack(&inst, devid, 0);
	ret = ures_get_mem_base(&inst, MAMI_RAS_MEM, addr);
	if (ret != 0) {
		return -ENOTSUPP;
	}

	return 0;
}

int ras_get_data_header(u32 devid, struct ures_addr_info *ras_hdr_addr, struct ras_data_header *data_hdr)
{
	int ret;

	ret = ras_get_mem_addr(devid, ras_hdr_addr);
	if (ret != 0) {
		return ret;
	}

	if (ras_hdr_addr->size < sizeof(struct ras_data_header)) {
		ras_err("Invalid ras mem size. (devid=%u; mem_size=0x%x)\n", devid, ras_hdr_addr->size);
		return -EINVAL;
	}

	ret = uop_kern_mem_read(devid, ras_hdr_addr->addr, data_hdr, sizeof(struct ras_data_header));
	if (ret != 0) {
		ras_err_limit("Read ras data header failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	return ret;
}

int ras_data_entry_load(u32 devid, struct ures_addr_info *ras_hdr_addr, struct ras_data_header *data_hdr)
{
	struct ras_dev_ctx *d_ctx = NULL;
	int ret;

	ret = ras_alloc_entry_mem(devid, data_hdr->item_num);
	if (ret != 0) {
		ras_err("Alloc entries mem failed. (devid=%u; item_num=0x%x; ret=%d)\n", devid, data_hdr->item_num, ret);
		return ret;
	}

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL) {
		ras_err("Get ras dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	if (d_ctx->entry == NULL) {
		ras_dev_ctx_put(devid);
		ras_err("Ras dev ctx entry is null. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	ret = uop_kern_mem_read(devid, ras_hdr_addr->addr + sizeof(struct ras_data_header), d_ctx->entry->entry,
							d_ctx->entry->item_num * sizeof(struct ras_data_entry_item));
	if (ret != 0) {
		ras_err("Read ras data entry failed. (devid=%u; ret=%d)\n", devid, ret);
		ras_dev_ctx_put(devid);
		return ret;
	}

	ras_dev_ctx_put(devid);
	return 0;
}

static int ras_get_data_header_and_check(u32 devid, struct ures_addr_info *ras_hdr_addr,
										 struct ras_data_header *data_hdr)
{
	u16 crc = 0;
	u32 entries_size, total_size;
	int ret;

	ret = ras_get_data_header(devid, ras_hdr_addr, data_hdr);
	if (ret != 0) {
		return ret;
	}

	if (memcmp(&data_hdr->magic[0], RAS_INFO_HEADER_MAGIC, RAS_INFO_HEADER_MAGIC_LEN) != 0) {
		ras_err_limit("Head magic check failed. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = crc16_cc((const unsigned char *)data_hdr, sizeof(struct ras_data_header) - sizeof(u16), &crc);
	if (ret != 0) {
		ras_err_limit("Crc calculate failed. (devid=%u; calc_crc=0x%x; expect_crc=0x%x; ret=%d)\n",
				devid, crc, data_hdr->header_crc, ret);
		return -EINVAL;
	}

	if (crc != data_hdr->header_crc) {
		ras_err_limit("Crc check failed. (devid=%u; calc_crc=0x%x; expect_crc=0x%x; ret=%d)\n",
				devid, crc, data_hdr->header_crc, ret);
		return -EINVAL;
	}

	if ((data_hdr->valid & 0x1) == 0 || (data_hdr->item_num == 0)) {
		ras_err_limit("Ras data is invalid. (devid=%u; valid_bit=0x%x; item_num=%u)\n",
				devid, data_hdr->valid, data_hdr->item_num);
		return -EINVAL;
	}
	if ((data_hdr->item_len < RAS_DATA_ENTRY_MIN_SIZE) || (data_hdr->item_len > RAS_DATA_ENTRY_MAX_SIZE)) {
		ras_err_limit("Ras item len is invalid. (devid=%u; item_len=%uB; item_len range=[%u, %u])\n",
				devid, data_hdr->item_len, RAS_DATA_ENTRY_MIN_SIZE, RAS_DATA_ENTRY_MAX_SIZE);
		return -EINVAL;
	}

	entries_size = data_hdr->item_num * sizeof(struct ras_data_entry_item);
	total_size = sizeof(struct ras_data_header) + entries_size + data_hdr->item_num * data_hdr->item_len;
	if (total_size > ras_hdr_addr->size) {
		ras_err_limit("Invalid ras mem size. (devid=%u; total_size=0x%x; mem_size=0x%x)\n", devid,
				total_size, ras_hdr_addr->size);
		return -EINVAL;
	}

	return 0;
}

static int ras_payload_process_by_ras_type(u32 devid, struct ras_data_entry_item *item, u32 need_report)
{
	u32 ras_type = item->type;

	if (ras_type == RAS_ERRDATA_TYPE_LOCAL) {
		return ras_local_process(devid, item, need_report);
	} else if (ras_type == RAS_ERRDATA_TYPE_PROTOC) {
		return ras_protoc_process(devid, item, need_report);
	}

	ras_err("Ras err data type not support. (devid=%u; type=%u)\n", devid, ras_type);
	return -EINVAL;
}

static void ras_err_data_entry_to_unused(u32 devid, u32 item_idx, struct ures_addr_info *ras_hdr_addr,
										 struct ras_data_entry_item *item)
{
	u64 offset;
	int ret;
	u16 value;

	item->used = 0;
	value = *(u16 *)((uintptr_t)(&item->ras_info_len) + sizeof(u32));
	offset = sizeof(struct ras_data_header) + item_idx * sizeof(struct ras_data_entry_item) +
		offsetof(struct ras_data_entry_item, ras_info_len);
	ret = uop_kern_mem_write(devid, ras_hdr_addr->addr + offset + sizeof(u32), &value, sizeof(u16));
	if (ret != 0) {
		ras_err("To unused failed. (devid=%u; item_idx=%u; ret=%d)\n", devid, item_idx, ret);
	}
}

static bool ras_item_info_addr_check(struct ras_data_entry_item *item, struct ures_addr_info *ras_hdr_addr,
									 struct ras_data_header *data_hdr, struct ras_dev_ctx *d_ctx)
{
	u64 min_item_addr, end_addr;

	min_item_addr = ras_hdr_addr->addr + sizeof(struct ras_data_header) + data_hdr->item_num *
		sizeof(struct ras_data_entry_item);
	end_addr = ras_hdr_addr->addr + (u64)ras_hdr_addr->size;

	if (end_addr <= ras_hdr_addr->addr) {   /* prevent end_addr overflow wrap-around */
		return false;
	}
	if ((item->ras_info_len <= 0) || (item->ras_info_len > data_hdr->item_len) ||
		(item->ras_info_len > d_ctx->payload_size)) {
		return false;
	}
	if ((item->ras_info_addr < min_item_addr) || (item->ras_info_addr > end_addr) ||
		(item->ras_info_addr + item->ras_info_len > end_addr)) {
		return false;
	}
	return true;
}

STATIC int ras_payload_process(u32 devid, struct ures_addr_info *ras_hdr_addr, struct ras_data_header *data_hdr,
							   u32 need_report)
{
	u32 i;
	int ret;
	struct ras_data_entry_item *item = NULL;
	struct ras_data_entry *entry = NULL;
	struct ras_dev_ctx *d_ctx = NULL;

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL) {
		ras_err("Get ras dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	if ((d_ctx->entry == NULL) || (d_ctx->payload == NULL)) {
		ras_dev_ctx_put(devid);
		ras_err("Get ras dev ctx entry or payload failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	entry = d_ctx->entry;
	for (i = 0; i < entry->item_num; i++) {
		item = &entry->entry[i];
		if (item->used == 0) {  /* No data */
			continue;
		}

		if (!ras_item_info_addr_check(item, ras_hdr_addr, data_hdr, d_ctx)) {
			ras_err("Invalid ras info addr. (devid=%u; item_idx=%u)\n", devid, i);
			ras_err_data_entry_to_unused(devid, i, ras_hdr_addr, item);
			continue;
		}

		ret = uop_kern_mem_read(devid, item->ras_info_addr, d_ctx->payload, item->ras_info_len);
		if (ret != 0) {
			ras_err("Read ras info failed. (devid=%u; item_idx=%u)\n", devid, i);
			continue;
		}

		ret = ras_payload_process_by_ras_type(devid, item, need_report);
		if (ret != 0) {
			ras_err("Ras data process failed. (devid=%u; type=%u; ret=%d)\n", devid, item->type, ret);
		}
		ras_err_data_entry_to_unused(devid, i, ras_hdr_addr, item);
	}
	ras_dev_ctx_put(devid);

	return 0;
}

#define MISC_IMP_GPIO_INT_MASK					0x30450ULL
#define MISC_IMP_GPIO_INT_STATUS				  0x30454ULL

STATIC void ras_clear_int_status_and_unmask(struct ras_dev_ctx *d_ctx, u32 dieid)
{
	int ret;

	if (dieid >= d_ctx->die_num) {
		ras_err("Invalid dieid. (devid=%u; dieid=%u; die_num=%u)\n", d_ctx->udevid, dieid, d_ctx->die_num);
		return;
	}

	ret = uop_kern_writel(d_ctx->udevid, d_ctx->misc_addr[dieid] + MISC_IMP_GPIO_INT_STATUS, 1);
	if (ret != 0) {
		ras_err("Clear ras int status failed. (devid=%u; dieid=%u; ret=%d)\n", d_ctx->udevid, dieid, ret);
	}

	ret = uop_kern_writel(d_ctx->udevid, d_ctx->misc_addr[dieid] + MISC_IMP_GPIO_INT_MASK, 0);
	if (ret != 0) {
		ras_err("Unmask ras int failed. (devid=%u; dieid=%u; ret=%d)\n", d_ctx->udevid, dieid, ret);
	}
}

STATIC void ras_int_mask(struct ras_dev_ctx *d_ctx, u32 dieid)
{
	int ret;

	ret = uop_kern_writel(d_ctx->udevid, d_ctx->misc_addr[dieid] + MISC_IMP_GPIO_INT_MASK, 1);
	if (ret != 0) {
		ras_err("Mask ras int failed. (devid=%u; dieid=%u; ret=%d)\n", d_ctx->udevid, dieid, ret);
	}
}

int ras_irq_init(u32 devid)
{
	struct ras_dev_ctx *d_ctx = NULL;
	u32 i;

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL) {
		ras_err("Get RAS dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	for (i = 0; i < d_ctx->die_num; i++) {
		ras_clear_int_status_and_unmask(d_ctx, i);
	}
	d_ctx->msi_support = true;
	ras_dev_ctx_put(devid);
	return 0;
}

int ras_irq_uninit(u32 devid)
{
	struct ras_dev_ctx *d_ctx = NULL;
	u32 i;

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL) {
		ras_err("Get RAS dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	for (i = 0; i < d_ctx->die_num; i++) {
		ras_int_mask(d_ctx, i);
	}
	d_ctx->msi_support = false;
	ras_dev_ctx_put(devid);
	return 0;
}

int ras_data_parser_and_report(u32 devid, u32 need_report)
{
	struct ras_data_header data_hdr;
	struct ures_addr_info ras_hdr_addr;
	int ret;

	ret = ras_get_data_header_and_check(devid, &ras_hdr_addr, &data_hdr);
	if (ret != 0) {
		return ret;
	}

	ret = ras_data_entry_load(devid, &ras_hdr_addr, &data_hdr);
	if (ret != 0) {
		ras_err("Ras data entry load failed. (devid=%u)\n", devid);
		return ret;
	}

	ret = ras_alloc_payload_mem(devid, data_hdr.item_len);
	if (ret != 0) {
		ras_err("Alloc payload mem failed. (devid=%u; size=%u)\n", devid, data_hdr.item_len);
		return ret;
	}

	ret = ras_payload_process(devid, &ras_hdr_addr, &data_hdr, need_report);
	if (ret != 0) {
		ras_err("Ras errdata entry process failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	return ret;
}

void ras_intr_handle(u32 devid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info)
{
	int ret;
	struct ras_dev_ctx *d_ctx = NULL;
	uid_irq_para irq_para = {.val = irq_info.irq_para.val};
	u64 time = irq_info.irq_time;
	u64 start_time = 0;
	u64 end_time;

	start_time = ras_local_get_now();

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL) {
		return;
	}

	mutex_lock(&d_ctx->mtx);
	ret = ras_data_parser_and_report(devid, 1);
	if (ret != 0 && ret != -ENOTSUPP) {
		ras_err_limit("Parser and report failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	if (d_ctx->msi_support) {
		if (irq_para.ras.in_irq != 0) {
			ras_clear_int_status_and_unmask(d_ctx, irq_para.ras.dieid);
		} else {
			u32 i;

			for (i = 0; i < d_ctx->die_num; i++) {
				ras_clear_int_status_and_unmask(d_ctx, i);
			}
		}
	}

	end_time = ras_local_get_now();

	mutex_unlock(&d_ctx->mtx);
	ras_dev_ctx_put(devid);
	if (time != 0) {
		ras_info("Ras interrupt processing time info. "
				 "(devid=%u; trigger_time=%llums; start_time=%llums; end_time=%llums; cost=%llums)\n",
				 devid, time, start_time, end_time, (end_time - time));
	}
}

u64 ras_local_get_now(void)
{
	struct timespec64 ts = {0};

	ktime_get_real_ts64(&ts);
	return ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);
}

int ras_get_chipid_and_dieid(u32 devid, struct ras_data_entry_item *item, u32 *chip_id, u32 *dieid)
{
	int ret;
	u32 i, die_num_per_chip, die_num_in_dev, chip_num_in_dev, ue_num_per_die;
	u64 die_num = 0, ue_num_in_dev = 0;
	char udie_cfg_key[URES_NAME_LEN];
	struct udev_die_cfg_kernel *die_cfg = NULL;
	u32 die_cfg_size = 0;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num == 0) || (die_num > DIE_MAX_NUM)) {
		ras_err("Get dev die num fail. (devid=%u; die_num=%llu; ret=%d)\n", devid, die_num, ret);
		return -EINVAL;
	}

	ret = idm_kern_get_chip_die_num(devid, &die_num_per_chip, &die_num_in_dev, &chip_num_in_dev);
	if (ret != 0 || die_num_per_chip == 0 || die_num_in_dev == 0 || chip_num_in_dev == 0) {
		ras_err("Get chip die num failed. (devid=%u; die_num_per_chip=%u; die_num_in_dev=%u; "
				"chip_num_in_dev=%u; ret=%d)\n",
				devid, die_num_per_chip, die_num_in_dev, chip_num_in_dev, ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_UE_NUM, &ue_num_in_dev);
	if (ret != 0) {
		ras_err("Get dev ue num fail. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	ue_num_per_die = (u32)(ue_num_in_dev / die_num);
	die_cfg_size = sizeof(struct udev_die_cfg_kernel) + sizeof(struct udev_ue_info) * ue_num_per_die;
	die_cfg = (struct udev_die_cfg_kernel *)kvmalloc(die_cfg_size, GFP_KERNEL_ACCOUNT);
	if (die_cfg == NULL) {
		ras_err("Malloc failed. (devid=%u; size=%u)\n", devid, die_cfg_size);
		return -ENOMEM;
	}
	for (i = die_num_per_chip * item->chip_id; i < (u32)die_num; i++) {
		(void)UBFM_SPRINTF(udie_cfg_key, sizeof(udie_cfg_key), MAMI_UDEV_DIEN_CFG, i);
		ret = ures_dev_get_attr(devid, udie_cfg_key, die_cfg, die_cfg_size);
		if (ret != 0) {
			ras_err("Get die info failed. (devid=%u; die_id=%u; ret=%d)\n", devid, i, ret);
			kvfree(die_cfg);
			return ret;
		}

		if (die_cfg->die_id != item->die_id) {
			continue;
		}

		*chip_id = i / die_num_per_chip;
		*dieid = i % die_num_per_chip;
		kvfree(die_cfg);
		return 0;
	}
	kvfree(die_cfg);
	return -ENODATA;
}

/* the port id here is the logic id within the die */
int ras_get_logicid_by_port_id(u32 devid, u32 chip_id, u32 die_id, u32 port_id, struct mami_logical_id_info *logic_id)
{
	struct mami_physical_id_info phy_id = {0};
	u32 in_port;
	int ret;

	ret = idm_kern_get_inport_by_chip_die_port(devid, chip_id, die_id, port_id, &in_port);
	if (ret != 0) {
		ras_err("Get fport id failed. (devid=%u; chip_id=%u; die_id=%u; port_id=%u; ret=%d)\n",
				devid, chip_id, die_id, port_id, ret);
		return ret;
	}

	phy_id.physicalIdVal = MAMI_MAKE_PHY_ID(chip_id, 0, 0, 0, in_port);
	phy_id.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_PORT;
	return idm_kern_phyid2lgid(devid, phy_id, logic_id);
}

int ras_get_logicid_by_body_id(u32 devid, u32 chip_id, u32 die_id, u32 body_id, struct mami_logical_id_info *logic_id)
{
	int ret;
	struct mami_physical_id_info phy_id = {0};
	u32 portset;

	ret = idm_kern_get_portset_by_chip_die_body(devid, chip_id, die_id, body_id, &portset);
	if (ret != 0) {
		return ret;
	}

	phy_id.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_BODY;
	phy_id.physicalIdVal = MAMI_MAKE_PHY_ID(chip_id, 0, die_id, portset, 0);
	return idm_kern_phyid2lgid(devid, phy_id, logic_id);
}

int ras_get_logicid_by_plane_id(u32 devid, u32 chip_id, u32 die_id, struct mami_logical_id_info *logic_id, struct mami_fault_event *event)
{
	struct mami_physical_id_info phy_id = {0};
	struct udev_hw_identify hw = {0};
	int ret = 0;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		ras_err("Get hw identity failed. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if ((hw.chip_type == MAMI_KERN_TYPE_UNIONS) || (hw.chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		phy_id.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_CHIP;
		phy_id.physicalIdVal = MAMI_MAKE_PHY_ID(chip_id, 0, 0, 0, 0);
		ras_err("TYPE_CHIP ras occurred. (devid=%u; chip_id=%u; die_id=%u; detailCode=0x%llx; time=%llums)\n",
			devid, chip_id, die_id, event->detailCode, event->alarmRaisedTime);
	} else {
		phy_id.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_DIE;
		phy_id.physicalIdVal = MAMI_MAKE_PHY_ID(chip_id, 0, die_id, 0, 0);
	}

	return idm_kern_phyid2lgid(devid, phy_id, logic_id);
}
