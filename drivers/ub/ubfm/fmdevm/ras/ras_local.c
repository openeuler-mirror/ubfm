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
#include <linux/mm.h>
#include "mami_string.h"

#include "ubfm_common.h"
#include "fmdevm.h"
#include "fmdevm.h"
#include "faultmgr_event.h"
#include "udm.h"
#include "idm.h"
#include "ures.h"
#include "feature_kern.h"

#include "ras_log.h"
#include "ras_ctx.h"
#include "apei_tbl_load.h"
#include "local_ras_register.h"
#include "ras_local.h"

static bool is_ucmi(u32 devid, struct local_ras_error_data *errdata)
{
	if (feature_kern_ucmi_module_is_ascend(devid)) {
		if (errdata->module_id == APEI_MODULE_UCMI_ASCEND) {
			return true;
		}
	} else {
		if (errdata->module_id == APEI_MODULE_UCMI_KUNPENG) {
			return true;
		}
	}

	return false;
}

int ras_local_check_errdata(u32 devid, struct ras_dev_ctx *d_ctx)
{
	if (d_ctx == NULL) {
		ras_err("Get ras dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	if (d_ctx->payload == NULL) {
		ras_err("Get ras dev ctx payload failed. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	return 0;
}

static int ras_local_support_check(u32 devid, struct ras_data_entry_item *item)
{
	struct local_ras_error_data *errdata = NULL;
	struct ras_dev_ctx *d_ctx = NULL;
	struct udm_dev_para dev_para;
	u32 array_size;
	int ret;

	if (item->ras_info_len <= offsetof(struct local_ras_error_data, register_array)) {
		ras_err("Invalid ras info len. (devid=%u; ras_info_len=%u; min_len=%zu)\n",
				devid, item->ras_info_len, offsetof(struct local_ras_error_data, register_array));
		return -EINVAL;
	}

	ret = udm_get_dev_attr(devid, &dev_para);
	if (ret != 0) {
		ras_err("Get dev attr failed. (devid=%u)\n", devid);
		return ret;
	}

	if (!(feature_kern_ucmi_module_is_ascend(devid) || feature_kern_ucmi_module_is_kunpeng(devid) || feature_kern_is_ssd(devid))) {
		ras_err("Dev chip_type is not supported. (devid=%u; dev_chip_type=%u)\n", devid, dev_para.chip_type);
		return -EINVAL;
	}

	d_ctx = ras_dev_ctx_get(devid);
	ret = ras_local_check_errdata(devid, d_ctx);
	if (ret != 0) {
		ras_err("Get errdata failed. (devid=%u)\n", devid);
		ras_dev_ctx_put(devid);
		return ret;
	}
	errdata = (struct local_ras_error_data *)d_ctx->payload;
	array_size = item->ras_info_len - offsetof(struct local_ras_error_data, register_array);
	if ((errdata->register_array_size == 0) || (errdata->register_array_size > array_size)) {
		ras_err("Invalid register array size. (devid=%u; reg_arr_size=%u; max_size=%u)\n",
			devid, errdata->register_array_size, array_size);
		ras_dev_ctx_put(devid);
		return -EINVAL;
	}

	if (is_ucmi(devid, errdata)) {
		if ((RAS_DATA_LOCAL_VALID_MODULE_ID & errdata->validation_bits) == 0)  {
			ras_err("Invalid bits. (devid=%u; validation=0x%x)\n", devid, errdata->validation_bits);
			ras_dev_ctx_put(devid);
			return -EINVAL;
		}
	} else {
		if ((RAS_DATA_LOCAL_VALID_MODULE_ID & errdata->validation_bits) == 0 ||
			(RAS_DATA_LOCAL_VALID_SUB_MODULE_ID & errdata->validation_bits) == 0) {
			ras_err("Invalid bits. (devid=%u; validation=0x%x)\n", devid, errdata->validation_bits);
			ras_dev_ctx_put(devid);
			return -EINVAL;
		}
	}

	ras_dev_ctx_put(devid);
	return 0;
}

static u32 ras_local_get_regval(struct local_ras_error_data *errdata, u32 reg_idx)
{
	return *(u32 *)(errdata->register_array + reg_idx * sizeof(u32));
}

static u32 ras_local_get_bit_fields(u32 reg_val, u32 bit_start, u32 bit_len)
{
	return (reg_val >> bit_start) & (~(((u32)(-2) << (bit_start + bit_len - 1)) >> bit_start)); /* -2 is used for mask calculation */
}

static int ras_local_add_additional_reg(struct mami_fault_event *event, u32 reg_val, int *reg_idx)
{
	if ((*reg_idx) * sizeof(u32) >= MAMI_MAX_EVENT_DATA_LENGTH) {
		ras_err("Invalid param. (reg_idx=%d; max_reg_num=%u)\n", (*reg_idx), MAMI_MAX_EVENT_DATA_LENGTH);
		return -EINVAL;
	}

	*(u32 *)(&event->additionalInfo[0] + (*reg_idx) * sizeof(u32)) = reg_val;
	*reg_idx += 1;
	return 0;
}

static void ras_local_fault_event_to_log(struct mami_fault_event *event)
{
	ras_err("Ras occur. (devid=%u; nodeId=%u:%u; eventId=0x%x; severity=%u; detailCode=0x%llx; "
			 "assertion=%u; time=%llums; eventName=%s)\n",
			 event->devid, event->nodeIdType, event->nodeId, event->eventId, event->severity, event->detailCode,
			 event->assertion, event->alarmRaisedTime, event->eventName);
}

static int ras_local_is_report_port(u32 sub_module_id)
{
	u32 module_ids[] = {0x4 /* LQC.NL.PORT */,
						0x6 /* LQC.DLPHY.DLMAC */,
						0x7 /* LQC.DLPHY.MUXPCS */};
	u32 i;

	for (i = 0; i < ARRAY_SIZE(module_ids); i++) {
		if (module_ids[i] == sub_module_id) {
			return 1;
		}
	}

	return 0;
}

static int ras_local_is_report_nl(u32 sub_module_id)
{
	u32 module_ids[] = {0x3 /* LQC.BA */,
						0x5 /* LQC.NL.DEVICE */};
	u32 i;

	for (i = 0; i < ARRAY_SIZE(module_ids); i++) {
		if (module_ids[i] == sub_module_id) {
			return 1;
		}
	}

	return 0;
}

static int ras_local_fill_node_id(u32 devid, struct local_ras_error_data *errdata, struct ras_data_entry_item *item,
								  struct mami_fault_event *event)
{
	struct mami_logical_id_info logic_id = {0};
	u32 chip_id, die_id;
	int ret;

	ret = ras_get_chipid_and_dieid(devid, item, &chip_id, &die_id);
	if (ret != 0) {
		ras_err("Get chipid dieid failed. (devid=%u; chip_id=%u; die_id=%u; ret=%d)\n",
				devid, item->chip_id, item->die_id, ret);
		return ret;
	}

	if (ras_local_is_report_port(errdata->sub_module_id) != 0) {
		ret = ras_get_logicid_by_port_id(devid, chip_id, die_id, errdata->port_id, &logic_id);
	} else if (ras_local_is_report_nl(errdata->sub_module_id) != 0) {
		ret = ras_get_logicid_by_body_id(devid, chip_id, die_id, errdata->core_id, &logic_id);
	} else {
		ret = ras_get_logicid_by_plane_id(devid, chip_id, die_id, &logic_id, event);
	}
	if (ret != 0) {
		ras_err("Get logicid failed. (devid=%u; chip_id=%u; die_id=%u; body_id=%u; port_id=%u; ret=%d)\n",
				devid, chip_id, die_id, errdata->core_id, errdata->port_id, ret);
		return ret;
	}

	event->nodeIdType = logic_id.logicalIdType;
	event->nodeId = logic_id.logicalIdVal;
	return 0;
}

static int ras_local_fault_event_report(u32 devid, struct local_ras_reg_field *reg_field,
										struct ras_data_entry_item *item, u32 need_report)
{
	struct local_ras_submodule *module = reg_field->reg->module;
	struct local_ras_assoc_reg *assoc_reg = NULL;
	struct local_ras_error_data *errdata = NULL;
	struct mami_fault_event *event = NULL;
	struct ras_dev_ctx *d_ctx = NULL;
	local_ras_detailcode detailcode = {0};
	u32 reg_val = 0, reg_count = 0;
	u32 bios_data_max = 0;
	int ret = 0;

	d_ctx = ras_dev_ctx_get(devid);
	ret = ras_local_check_errdata(devid, d_ctx);
	if (ret != 0) {
		ras_err("Get errdata failed. (devid=%u)\n", devid);
		ras_dev_ctx_put(devid);
		return ret;
	}
	errdata = (struct local_ras_error_data *)d_ctx->payload;

	event = &d_ctx->event;
	(void)ubfm_memset(event, sizeof(struct mami_fault_event), 0, sizeof(struct mami_fault_event));
	event->devid = devid;
	reg_val = ras_local_get_regval(errdata, reg_field->reg->reg_idx);
	ret = ras_local_add_additional_reg(event, reg_val, (int *)(void *)&reg_count);
	if (ret != 0) {
		ras_err("Add additional reg failed. (devid=%u; module_id=%u; module_name=%s; reg_idx=%u; ret=%d)\n",
				devid, module->id, module->name, reg_field->reg->reg_idx, ret);
		ras_dev_ctx_put(devid);
		return ret;
	}

	bios_data_max = (errdata->register_array_size / sizeof(u32));
	list_for_each_entry(assoc_reg, &reg_field->associate_regs, entry) {
		if (assoc_reg->reg->reg_idx >= bios_data_max) {
			continue;
		}
		reg_val = ras_local_get_regval(errdata, assoc_reg->reg->reg_idx);
		ret = ras_local_add_additional_reg(event, reg_val, (int *)(void *)&reg_count);
		if (ret != 0) {
			ras_err("Add additional reg failed. (devid=%u; module_id=%u; module_name=%s; assoc_reg_idx=%u; ret=%d)\n",
					devid, module->id, module->name, assoc_reg->reg->reg_idx, ret);
			ras_dev_ctx_put(devid);
			return ret;
		}
	}

	event->eventId = reg_field->event_id;
	ret = UBFM_SPRINTF(event->eventName, MAMI_MAX_EVENT_NAME_LENGTH, "%s.%s[%u:%u]", reg_field->reg->module->name,
					reg_field->reg->name, reg_field->bit_start + reg_field->bit_len - 1, reg_field->bit_start);
	if (ret <= 0) {
		ras_err("Format eventname failed. (devid=%u; module_id=%u; module_name=%s; reg_idx=%u; ret=%d)\n",
				devid, module->id, module->name, reg_field->reg->reg_idx, ret);
		ras_dev_ctx_put(devid);
		return -EINVAL;
	}

	event->severity = reg_field->fault_level;
	event->assertion = MAMI_KERN_FAULT_OCCUR;
	detailcode.bits.start_bit = reg_field->bit_start;
	detailcode.bits.reg_idx = reg_field->reg->reg_idx;
	if (module->id != DEFAULT_MODULE) {
		detailcode.bits.sub_module_id = module->id;
	}
	detailcode.bits.module_id = errdata->module_id;
	event->detailCode = detailcode.val;
	event->alarmRaisedTime = ras_local_get_now();

	ret = ras_local_fill_node_id(devid, errdata, item, event);
	if (ret != 0) {
		ras_dev_ctx_put(devid);
		return ret;
	}

	/* EventId 0 only logs, does not report */
	if (need_report != 0 && event->eventId != 0) {
		ret = ubdevm_kern_report_event(event);
	} else {
		ras_local_fault_event_to_log(event);
	}

	ras_dev_ctx_put(devid);
	return ret;
}

static int ras_local_group_reg_fields_check(struct local_ras_error_data *errdata,
											struct local_ras_reg_field *reg_field)
{
	u32 reg_val, field_val;
	struct local_ras_group_reg_field *grp_reg_field = NULL;

	list_for_each_entry(grp_reg_field, &reg_field->group_regs, entry) {
		reg_val = ras_local_get_regval(errdata, grp_reg_field->reg->reg_idx);
		field_val = ras_local_get_bit_fields(reg_val, grp_reg_field->bit_start, grp_reg_field->bit_len);
		if (field_val != grp_reg_field->default_val) {
			return -1;
		}
	}

	return 0;
}

STATIC int ras_local_reg_fields_check(u32 devid, struct local_ras_reg *reg, struct ras_data_entry_item *item,
									  u32 need_report)
{
	u32 reg_val, field_val;
	int ret;
	struct ras_dev_ctx *d_ctx = NULL;
	struct local_ras_error_data *errdata = NULL;
	struct local_ras_reg_field *reg_field = NULL;

	d_ctx = ras_dev_ctx_get(devid);
	ret = ras_local_check_errdata(devid, d_ctx);
	if (ret != 0) {
		ras_err("Get errdata failed. (devid=%u)\n", devid);
		ras_dev_ctx_put(devid);
		return ret;
	}
	errdata = (struct local_ras_error_data *)d_ctx->payload;

	reg_val = ras_local_get_regval(errdata, reg->reg_idx);
	/* Whether bit_start and bit_len in RegField are valid is checked when the ko is loaded, no redundant check is needed here */
	list_for_each_entry(reg_field, &reg->fields, entry) {
		field_val = ras_local_get_bit_fields(reg_val, reg_field->bit_start, reg_field->bit_len);
		if (field_val == reg_field->default_val) {
			continue;
		}

		if (!list_empty(&reg_field->group_regs) && ras_local_group_reg_fields_check(errdata, reg_field) == 0) {
			continue;
		}

		ret = ras_local_fault_event_report(devid, reg_field, item, need_report);
		if (ret != 0) {
			ras_err("Fault event report failed. (devid=%u; module_id=%u; reg_idx=%u; ret=%d)\n",
					devid, reg->module->id, reg->reg_idx, ret);
		}
	}
	ras_dev_ctx_put(devid);

	return 0;
}

STATIC int ras_local_fault_event_check(u32 devid, struct ras_data_entry_item *item, u32 need_report)
{
	int ret;
	struct local_ras_error_data *errdata = NULL;
	struct local_ras_submodule *module = NULL;
	struct ras_dev_ctx *d_ctx = NULL;
	u32 report_reg_num = 0;
	u32 min_reg_num = 0;
	u32 i;

	d_ctx = ras_dev_ctx_get(devid);
	ret = ras_local_check_errdata(devid, d_ctx);
	if (ret != 0) {
		ras_err("Get errdata failed. (devid=%u)\n", devid);
		ras_dev_ctx_put(devid);
		return ret;
	}
	errdata = (struct local_ras_error_data *)d_ctx->payload;
	/* according to the APEI specification */
	ret = local_ras_submodule_get(devid, errdata->module_id, errdata->sub_module_id, &module);
	if (ret != 0) {
		ras_err("Find apei module failed. (devid=%u; module_id=%u; sub_module_id=%u; ret=%d)\n",
				devid, errdata->module_id, errdata->sub_module_id, ret);
		ras_dev_ctx_put(devid);
		return -ENODATA;
	}

	report_reg_num = errdata->register_array_size / sizeof(u32); /* each error data occupies 4 bytes */
	min_reg_num = (report_reg_num < module->reg_num) ? report_reg_num : module->reg_num;
	for (i = 0; i < min_reg_num; i++) {
		struct local_ras_reg *reg = &module->regs[i];

		if (list_empty(&reg->fields)) {
			continue;
		}

		ret = ras_local_reg_fields_check(devid, reg, item, need_report);
		if (ret != 0) {
			ras_err("Ras field check failed. (devid=%u; module_id=%u; sub_module_id=%u; reg_idx=%u)\n",
					devid, errdata->module_id, errdata->sub_module_id, i);
		}
	}
	ras_dev_ctx_put(devid);
	return 0;
}

int ras_local_process(u32 devid, struct ras_data_entry_item *item, u32 need_report)
{
	int ret;

	ret = ras_local_support_check(devid, item);
	if (ret != 0) {
		return ret;
	}

	ret = ras_local_fault_event_check(devid, item, need_report);
	if (ret != 0) {
		ras_err("Fault event check failed. (devid=%u; ret=%d)\n", devid, ret);
	}
	return ret;
}
