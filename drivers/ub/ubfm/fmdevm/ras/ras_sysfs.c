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
#ifdef CFG_BUILD_DEBUG
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include "mami_string.h"

#include "ubfm_common.h"
#include "feature_loader.h"
#include "uop.h"
#include "uid.h"
#include "ubfm_dfx_fs.h"
#include "crc.h"
#include "ras_core.h"
#include "ras_local.h"
#include "ras_ctx.h"
#include "ras_log.h"
#include "ras_sysfs.h"

struct ras_sysfs_data_payload {
	u32 length;
	u32 type;
	char payload[RAS_DATA_ENTRY_MIN_SIZE];
};

STATIC int ras_sysfs_generate_data_entry_header(u32 devid, struct ras_data_header *hdr,
												struct ures_addr_info *ras_hdr_addr)
{
	u32 entry_size = hdr->item_num * sizeof(struct ras_data_entry_item);
	struct ras_data_entry *entry = NULL;
	struct ras_data_entry_item *item = NULL;
	int ret;
	u32 i;

	entry = kvmalloc(sizeof(struct ras_data_entry) + entry_size, GFP_KERNEL_ACCOUNT);
	if (entry == NULL) {
		ras_err("Alloc entry failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	entry->item_num = hdr->item_num;
	for (i = 0; i < entry->item_num; i++) {
		item = &entry->entry[i];
		item->ras_info_addr = (u64)((i + 1) * hdr->item_len) + ras_hdr_addr->addr;
		item->ras_info_len = hdr->item_len;
		item->used = 0;
		item->type = 1;
		item->crc = 0;
	}

	ret = uop_kern_mem_write(devid, ras_hdr_addr->addr + sizeof(struct ras_data_header), entry, entry_size);
	kvfree(entry);
	if (ret != 0) {
		ras_err("Save ras entry data failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	return ret;
}

static void ras_sysfs_pack_data_header(struct ras_data_header *data_hdr)
{
	(void)ubfm_memcpy(&data_hdr->magic[0], RAS_INFO_HEADER_MAGIC_LEN, RAS_INFO_HEADER_MAGIC,
				   RAS_INFO_HEADER_MAGIC_LEN);
	data_hdr->version = 0;
	data_hdr->valid = 1;
	data_hdr->item_num = 15;	/* initialize 15 entries */
	data_hdr->item_len = 0x400;	 /* 1K space for a single entry */
	data_hdr->valid_submodule = 0;
}

STATIC int ras_sysfs_generate_data_header(u32 devid, struct ures_addr_info *ras_hdr_addr,
										  struct ras_data_header *data_hdr)
{
	int ret;
	u16 crc = 0;

	ret = ras_get_data_header(devid, ras_hdr_addr, data_hdr);
	if (ret != 0) {
		return ret;
	}

	if (ras_hdr_addr->size < RAS_DATA_MEM_SIZE) {
		ras_err("Invalid ras mem size. (devid=%u; size=0x%x)\n", devid, ras_hdr_addr->size);
		return -EINVAL;
	}

	/* already initialized, do not re-initialize */
	if (memcmp(data_hdr, RAS_INFO_HEADER_MAGIC, RAS_INFO_HEADER_MAGIC_LEN) == 0) {
		ret = crc16_cc((const unsigned char *)data_hdr, sizeof(struct ras_data_header) - sizeof(u16), &crc);
		if (ret == 0 && (crc == data_hdr->header_crc)) {
			return 0;
		}
	}

	ras_sysfs_pack_data_header(data_hdr);
	ret = crc16_cc((const unsigned char *)data_hdr, sizeof(struct ras_data_header) - sizeof(u16), &crc);
	if (ret != 0) {
		ras_err("Calc crc failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	data_hdr->header_crc = crc;
	ret = uop_kern_mem_write(devid, ras_hdr_addr->addr, data_hdr, sizeof(struct ras_data_header));
	if (ret != 0) {
		ras_err("Save ras header failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	return ras_sysfs_generate_data_entry_header(devid, data_hdr, ras_hdr_addr);
}

STATIC int ras_sysfs_inject_errdata(u32 devid, u32 chip_id, u32 die_id, struct ras_sysfs_data_payload *data)
{
	struct ras_data_entry_item *item = NULL;
	struct ras_data_entry *entry = NULL;
	struct ras_dev_ctx *d_ctx = NULL;
	struct ures_addr_info ras_hdr_addr;
	struct ras_data_header data_hdr;
	u64 entry_offset = 0;
	u32 i = 0;
	int ret = 0;

	ret = ras_sysfs_generate_data_header(devid, &ras_hdr_addr, &data_hdr);
	if (ret != 0) {
		ras_err("Generate data header failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	ret = ras_data_entry_load(devid, &ras_hdr_addr, &data_hdr);
	if (ret != 0) {
		ras_err("Ras data load failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL || d_ctx->entry == NULL) {
		ras_err("Ras get dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	entry = d_ctx->entry;
	for (i = 0; i < entry->item_num; i++) {
		item = &entry->entry[i];
		if (item->used == 0) {
			item->ras_info_addr = (u64)((i + 1) * data_hdr.item_len) + ras_hdr_addr.addr;
			item->ras_info_len = data_hdr.item_len;
			item->chip_id = (u8)chip_id;
			item->die_id = (u8)die_id;
			item->type = data->type & 0x7;
			item->crc = 0;
			break;
		}
	}

	if (i == entry->item_num) {
		ras_err("No data entry can be used. (devid=%u; idx=%u)\n", devid, i);
		ras_dev_ctx_put(devid);
		return -ENOSPC;
	}

	ret = uop_kern_mem_write(devid, item->ras_info_addr, data->payload, data->length);
	if (ret != 0) {
		ras_err("Store inject data failed. (devid=%u; idx=%u; ret=%d)\n", devid, i, ret);
		ras_dev_ctx_put(devid);
		return ret;
	}

	item->used = 1;
	entry_offset = ras_hdr_addr.addr + sizeof(struct ras_data_header) + i * sizeof(struct ras_data_entry_item);
	ret = uop_kern_mem_write(devid, entry_offset, item, sizeof(struct ras_data_entry_item));
	if (ret != 0) {
		ras_err("Save data entry failed. (devid=%u; i=%u; ret=%d)\n", devid, i, ret);
	}

	ras_dev_ctx_put(devid);
	return ret;
}

STATIC int ras_sysfs_inject_local_ras_error(u32 devid, const char *buf, struct ras_sysfs_data_payload *data)
{
	struct local_ras_error_data *local_data = (struct local_ras_error_data *)data->payload;
	const char *p = buf;
	u32 reg_num = 0;
	u32 reg_val = 0;
	u32 chip_id = 0;
	u32 die_id = 0;
	u32 i = 0;

	data->length = sizeof(struct local_ras_error_data);
	if (data->length >= RAS_DATA_ENTRY_MIN_SIZE) {
		ras_err("Invalid buffer size. (devid=%u; size=%uB)\n", devid, data->length);
		return -EINVAL;
	}

	SIMPLE_STRTOUL_SAFE(p, chip_id, 0);
	SIMPLE_STRTOUL_SAFE(p, die_id, 0);
	local_data->socket_id = chip_id;
	SIMPLE_STRTOUL_SAFE(p, local_data->core_id, 0);
	SIMPLE_STRTOUL_SAFE(p, local_data->port_id, 0);
	SIMPLE_STRTOUL_SAFE(p, local_data->module_id, 0);
	SIMPLE_STRTOUL_SAFE(p, local_data->sub_module_id, 0);
	SIMPLE_STRTOUL_SAFE(p, reg_num, 0);
	ras_info("Inject LocalRAS. (devid=%u; chip_id=%u; dieid=%u; core_id=%u; port_id=%u; "
			 "module_id=0x%x; sub_module_id=0x%x; reg_num=%u)\n",
			 devid, chip_id, die_id, local_data->core_id, local_data->port_id,
			 local_data->module_id, local_data->sub_module_id, reg_num);

	if ((reg_num == 0) || (reg_num >= (RAS_DATA_ENTRY_MIN_SIZE - sizeof(struct local_ras_error_data)) / sizeof(u32))) {
		ras_err("Invalid register num. (devid=%u; reg_num=%u)\n", devid, reg_num);
		return -EINVAL;
	}

	local_data->register_array_size = reg_num * sizeof(u32);
	data->length += local_data->register_array_size;

	for (i = 0; i < reg_num; i++) {
		SIMPLE_STRTOUL_SAFE(p, reg_val, 0);
		*(u32 *)(&local_data->register_array[i * sizeof(u32)]) = reg_val;
	}
	local_data->validation_bits = 0x11EA;
	data->type = RAS_ERRDATA_TYPE_LOCAL;

	return ras_sysfs_inject_errdata(devid, chip_id, die_id, data);
}

#define RAS_ABNORMAL_LANE_DOWNGRADE_BIT				 7
STATIC int ras_sysfs_inject_protoc_ras_error(u32 devid, const char *buf, struct ras_sysfs_data_payload *data)
{
	struct protoc_ras_error_data *protoc_data = (struct protoc_ras_error_data *)data->payload;
	const char *p = buf;
	u32 chip_id = 0;
	u32 die_id = 0;

	data->length = sizeof(struct protoc_ras_error_data);
	if (data->length >= RAS_DATA_ENTRY_MIN_SIZE) {
		ras_err("Invalid buffer size. (devid=%u; size=%uB)\n", devid, data->length);
		return -EINVAL;
	}

	SIMPLE_STRTOUL_SAFE(p, chip_id, 0);
	SIMPLE_STRTOUL_SAFE(p, die_id, 0);
	SIMPLE_STRTOUL_SAFE(p, protoc_data->eid, 0);
	SIMPLE_STRTOUL_SAFE(p, protoc_data->cna, 0);
	SIMPLE_STRTOUL_SAFE(p, protoc_data->port, 0);

	ras_info("Inject protocRAS. (devid=%u; chip_id=%u; dieid=%u; eid=%u; cna=%u; port_id=%u)\n",
			 devid, chip_id, die_id, protoc_data->eid, protoc_data->cna, protoc_data->port);

	protoc_data->field.val = 0x2F;
	protoc_data->base_info.dev_type = 0x9F;	 /* for test use, the real devtype corresponds to the devtype of the ubc controller of the current die */
	protoc_data->base_info.class_code = 0x2;	/* for test use, the real devtype corresponds to the class code of the ubc controller of the current die */
	protoc_data->err_info.cor_mask = ~(1 << RAS_ABNORMAL_LANE_DOWNGRADE_BIT);
	protoc_data->err_info.cor_status = 1 << RAS_ABNORMAL_LANE_DOWNGRADE_BIT;
	data->type = RAS_ERRDATA_TYPE_PROTOC;

	return ras_sysfs_inject_errdata(devid, chip_id, die_id, data);
}

STATIC ssize_t ras_sysfs_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u32 devid, type;
	const char *p = buf;
	int ret;
	struct ras_sysfs_data_payload *data = NULL;

	data = kvmalloc(sizeof(struct ras_sysfs_data_payload), GFP_KERNEL_ACCOUNT);
	if (data == NULL) {
		ras_err("Alloc ras sysfs data failed.\n");
		return count;
	}

	(void)ubfm_memset(data, sizeof(struct ras_sysfs_data_payload), 0, sizeof(struct ras_sysfs_data_payload));
	SIMPLE_STRTOUL_SAFE(p, devid, 0);
	SIMPLE_STRTOUL_SAFE(p, type, 0);
	if (type == RAS_ERRDATA_TYPE_PROTOC) {
		ret = ras_sysfs_inject_protoc_ras_error(devid, p, data);
	} else {
		ret = ras_sysfs_inject_local_ras_error(devid, p, data);
	}
	if (ret != 0) {
		ras_err("Inject ras data failed. (devid=%u; ret=%d)\n", devid, ret);
	}

	kvfree(data);
	return (ssize_t)count;
}

STATIC ssize_t ras_sysfs_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	int ret;

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "Usage: echo <devid> <RAS_TYPE> <Option>\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "RAS_TYPE: 1: LocalRAS(default)   2: Protocol RAS\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "Option:\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "    LocalRAS: <chipId> <dieId> <bodyId> <portId> "
					"<moduleId> <subModuleId> <regNum> <regVal0> <regVal1> ...\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "    Protocol RAS: <chipId> <dieId> <eid> <cna> <portId>\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	return count;
}

STATIC ssize_t ras_sysfs_notify_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u32 devid, die_id;
	const char *p = buf;
	struct uid_irq_info irq_info = {0};

	SIMPLE_STRTOUL_SAFE(p, devid, 0);
	SIMPLE_STRTOUL_SAFE(p, die_id, 0);

	ras_info("Trigger RAS. (devid=%u; die_id=%u)\n", devid, die_id);
	irq_info.irq_para.val = (u64)die_id;
	uid_trigger_intr(devid, UID_TYPE_RAS, irq_info);
	return count;
}

static DEVICE_ATTR(ras_cmd, UBFM_FS_MODE_OWNER_RW, ras_sysfs_cmd_show, ras_sysfs_cmd_store);
static DEVICE_ATTR(notify, UBFM_FS_MODE_OWNER_RW, NULL, ras_sysfs_notify_store);

static struct attribute *g_ras_sysfs_attrs[] = {
	&dev_attr_ras_cmd.attr,
	&dev_attr_notify.attr,
	NULL
};

static struct attribute_group g_ras_attr_grp = {
	.name = "ras",
	.attrs = g_ras_sysfs_attrs,
};

int ras_sysfs_init(void *priv)
{
	struct platform_device *pdev = NULL;
	int ret;

	if (unlikely(priv == NULL)) {
		ras_err("Invalid param\n");
		return -EINVAL;
	}
	pdev = (struct platform_device *)priv;
	ret = sysfs_create_group(&pdev->dev.kobj, &g_ras_attr_grp);
	if (unlikely(ret != 0)) {
		ras_err("Create group failed. (ret=%d)\n", ret);
	}
	return ret;
}
DECLARE_FEATURE_AUTO_INIT_BY_SCOPE(sysfs, ras_sysfs_init, FEATURE_LOADER_STAGE_6);

void ras_sysfs_uninit(void *priv)
{
	struct platform_device *pdev = NULL;

	if (unlikely(priv == NULL)) {
		ras_err("Invalid param\n");
		return;
	}
	pdev = (struct platform_device *)priv;
	sysfs_remove_group(&pdev->dev.kobj, &g_ras_attr_grp);
}
DECLARE_FEATURE_AUTO_UNINIT_BY_SCOPE(sysfs, ras_sysfs_uninit, FEATURE_LOADER_STAGE_6);
#endif
