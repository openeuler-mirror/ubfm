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
#include "faultmgr_event.h"
#include "ras_log.h"
#include "ras_ctx.h"
#include "ras_protoc.h"

#define RAS_ABNORMAL_LANE_DOWNGRADE_BIT				 7

int ras_protoc_process(u32 devid, struct ras_data_entry_item *item, u32 need_report)
{
	struct mami_fault_event *event = NULL;
	struct mami_logical_id_info logic_id;
	struct ras_dev_ctx *d_ctx = NULL;
	struct protoc_ras_error_data *errdata = NULL;
	u32 chip_id, die_id;
	u64 val, alarm_time;
	int ret;

	if (item->ras_info_len < sizeof(struct protoc_ras_error_data)) {
		ras_err("Invalid ras info len. (devid=%u; ras_info_len=%u; min_len=%zu)\n",
				devid, item->ras_info_len, sizeof(struct protoc_ras_error_data));
		return -EINVAL;
	}

	d_ctx = ras_dev_ctx_get(devid);
	if (d_ctx == NULL || d_ctx->payload == NULL) {
		ras_err("Get ras dev ctx failed. (devid=%u)\n", devid);
		return -ENODEV;
	}

	errdata = (struct protoc_ras_error_data *)d_ctx->payload;
	ras_info("Protoc RAS. (devid=%u; valid_bits=0x%llx; cor_mask=0x%llx; cor_status=0x%llx)\n",
			 devid, errdata->field.val, errdata->err_info.cor_mask, errdata->err_info.cor_status);
	if (errdata->field.bits.eid_valid == 0 || errdata->field.bits.cna_valid == 0 ||
		errdata->field.bits.base_info_valid == 0 || errdata->field.bits.aer_info_valid == 0) {
		ras_err("Invalid field info. (devid=%u)\n", devid);
		ras_dev_ctx_put(devid);
		return -EINVAL;
	}

	/* no lane downgrade abnormal */
	val = errdata->err_info.cor_status & (~errdata->err_info.cor_mask);
	if ((val & (1 << RAS_ABNORMAL_LANE_DOWNGRADE_BIT)) == 0) {
		ras_dev_ctx_put(devid);
		return 0;
	}

	alarm_time = ras_local_get_now();
	if (errdata->field.bits.port_valid == 0) {
		ras_err("RAS device lane downgrade occur. (devid=%u; time=%llums)\n", devid, alarm_time);
		ras_dev_ctx_put(devid);
		return 0;
	}

	event = &d_ctx->event;
	(void)ubfm_memset(event, sizeof(struct mami_fault_event), 0, sizeof(struct mami_fault_event));
	event->eventId = 0x81B18605;		/* protocol RAS lane downgrade fault, eventId is 0x81B18605 */
	event->devid = devid;
	event->detailCode = 0;
	event->alarmRaisedTime = alarm_time;
	event->severity = MAMI_KERN_EVENT_MINOR;
	event->assertion = MAMI_KERN_FAULT_OCCUR;
	*(u32 *)&event->additionalInfo[0x0] = errdata->eid;
	*(u32 *)&event->additionalInfo[0x4] = errdata->cna;
	*(u32 *)&event->additionalInfo[0x8] = errdata->port;
	*(u32 *)&event->additionalInfo[0xC] = errdata->err_info.cor_status;

	ret = ras_get_chipid_and_dieid(devid, item, &chip_id, &die_id);
	if (ret != 0) {
		ras_err("Get chipid dieid failed. (devid=%u; chip_id=%u; die_id=%u; ret=%d)\n",
				devid, item->chip_id, item->die_id, ret);
		ras_dev_ctx_put(devid);
		return ret;
	}

	ret = ras_get_logicid_by_port_id(devid, chip_id, die_id, errdata->port, &logic_id);
	if (ret != 0) {
		ras_err("Get logicid failed. (devid=%u; chip_id=%u; die_id=%u; port_id=%u; ret=%d)\n",
				devid, chip_id, die_id, errdata->port, ret);
		ras_dev_ctx_put(devid);
		return ret;
	}

	ret = UBFM_SPRINTF(event->eventName, MAMI_MAX_EVENT_NAME_LENGTH, "ABNORMAL.CHIP%u.DIE%u.LANE%u.DOWNGRADE",
					chip_id, die_id, errdata->port);
	if (ret <= 0) {
		ras_err("Format name failed. (devid=%u; ret=%d)\n", devid, ret);
		ras_dev_ctx_put(devid);
		return -EINVAL;
	}

	event->nodeIdType = logic_id.logicalIdType;
	event->nodeId = logic_id.logicalIdVal;

	if (need_report != 0) {
		ret = ubdevm_kern_report_event(event);
	} else {
		ras_err("RAS lane downgrade occur. (devid=%u; nodeId=%u:%u; eventId=0x%x; severity=%u; "
			"detailCode=0x%llx; assertion=%u; time=%llums; eventName=%s)\n",
			event->devid, event->nodeIdType, event->nodeId, event->eventId, event->severity, event->detailCode,
			event->assertion, event->alarmRaisedTime, event->eventName);
	}

	ras_dev_ctx_put(devid);
	return ret;
}
