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
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/wait.h>

#include "faultmgr_log.h"
#include "faultmgr_queue.h"
#include "feature_kern.h"
#include "ubfm_common.h"

static DECLARE_WAIT_QUEUE_HEAD(faultmgr_wq);
static atomic_t parallel_req_num = ATOMIC_INIT(0);

static const char * const faultmgr_assertion_table[] = {
	"Fault event recover",   /* 0 */
	"Fault event occur",	 /* 1 */
	"One-Time event"		 /* 2 */
};

#define MAX_PARALLEL_REG_NUM			8		   /* to avoid too many simultaneous userspace requests, limit to a maximum of 8 concurrent users getting fault events */

void faultmgr_wq_wakeup(void)
{
	wake_up_interruptible(&faultmgr_wq);
}

static void faultmgr_fault_event_to_log(struct mami_fault_event *event)
{
	if (event->assertion == MAMI_KERN_FAULT_RECOVERY) {
		faultmgr_warn("%s. (eventId=0x%x; devid=%u; nodeId=%u:%u; detailCode=0x%llx; severity=%u; "
			"assertion=%u; time=%llums; eventName=%s)\n",
			faultmgr_assertion_table[event->assertion], event->eventId, event->devid, event->nodeIdType, event->nodeId,
			event->detailCode, event->severity, event->assertion, event->alarmRaisedTime, event->eventName);
	} else {
		faultmgr_err("%s. (eventId=0x%x; devid=%u; nodeId=%u:%u; detailCode=0x%llx; severity=%u; "
			"assertion=%u; time=%llums; eventName=%s)\n",
			faultmgr_assertion_table[event->assertion], event->eventId, event->devid, event->nodeIdType, event->nodeId,
			event->detailCode, event->severity, event->assertion, event->alarmRaisedTime, event->eventName);
	}
}

int ubdevm_kern_report_event(struct mami_fault_event *event)
{
	int assertion_table_size = 0;
	int ret = 0;

	if (!feature_kern_is_support_ras_report()) {
		return E_OPNOTSUPP_ON_DEV;
	}

	/* Force the last character of the user-provided eventName to '\0'
	 * to prevent out-of-bounds string access caused by an invalid string.
	 */
	event->eventName[MAMI_MAX_EVENT_NAME_LENGTH - 1] = '\0';
	assertion_table_size = ARRAY_SIZE(faultmgr_assertion_table);
	if (event->assertion >= assertion_table_size) {
		faultmgr_err("Event assertion check failed. (event_id=0x%x; event_assertion=%d)\n",
			 event->eventId, event->assertion);
		return -EINVAL;
	}

	ret = faultmgr_queue_add_event(event);
	if (ret != 0) {
		faultmgr_err("Add event to queue failed. (event_id=0x%x; event_name=%s; ret=%d)\n",
					 event->eventId, event->eventName, ret);
		return ret;
	}
	faultmgr_fault_event_to_log(event);

	faultmgr_wq_wakeup();
	return 0;
}

int faultmgr_ucd_event_add(void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_fault_event *event = (struct mami_fault_event *)in;
	int ret;

	if ((in == NULL) || (in_len != sizeof(struct mami_fault_event))) {
		faultmgr_err("Invalid para. (in_len=%u)\n", in_len);
		return -EINVAL;
	}

	ret = ubdevm_kern_report_event(event);
	if (ret != 0 && ret != E_OPNOTSUPP_ON_DEV) {
		faultmgr_err("Add event failed. (event_id=0x%x; event_name=%s; ret=%d)\n",
					 event->eventId, event->eventName, ret);
	}

	return ret;
}

#define GET_EVENT_MAX_TIMEOUT 30000
#define GET_EVENT_MIN_TIMEOUT (-1)
static DEFINE_MUTEX(g_faultmgr_parallel_lock);
int faultmgr_ucd_event_check(void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_read_event_param *in_param = NULL;

	if ((in == NULL) || (in_len != sizeof(struct mami_read_event_param)) || (out == NULL) ||
		(out_len != sizeof(struct mami_read_event_param))) {
		faultmgr_err("Invalid para. (in=%s; in_len=%u; out=%s; out_len=%u)\n",
					 in == NULL ? "NULL" : "NOT NULL", in_len, out == NULL ? "NULL" : "NOT NULL", out_len);
		return -EINVAL;
	}

	in_param = (struct mami_read_event_param *)in;
	if (in_param->inEventNum == 0 || in_param->event == NULL) {
		faultmgr_err("Invalid input event num. (inEventNum=%u; event=%s)\n", in_param->inEventNum,
			in_param->event == NULL ? "NULL" : "not NULL");
		return -EINVAL;
	}
	if (in_param->timeout < GET_EVENT_MIN_TIMEOUT || in_param->timeout > GET_EVENT_MAX_TIMEOUT) {
		faultmgr_err("Invalid timeout para. (timeout=%dms, min=%dms, max=%dms)\n",
			in_param->timeout, GET_EVENT_MIN_TIMEOUT, GET_EVENT_MAX_TIMEOUT);
		return -EINVAL;
	}

	return 0;
}

int faultmgr_ucd_event_get(void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_read_event_param *out_param = NULL;
	struct mami_read_event_param *in_param = NULL;
	struct mami_read_event_param param = {0};
	int ret = 0;

	ret = faultmgr_ucd_event_check(in, in_len, out, out_len);
	if (ret != 0) {
		faultmgr_err("Faultmgr ucd para check failed.\n");
		return ret;
	}

	out_param = (struct mami_read_event_param *)out;
	in_param = (struct mami_read_event_param *)in;

	param.eventSerialNum = in_param->eventSerialNum;
	param.inEventNum = in_param->inEventNum;
	param.event = in_param->event;	  /* event is userspace memory */

	if (param.eventSerialNum == MAMI_EVENT_SERIALNUM_NEWEST) {
		param.eventSerialNum = faultmgr_queue_get_current_serialnum();
	}

	mutex_lock(&g_faultmgr_parallel_lock);
	if (atomic_read(&parallel_req_num) >= MAX_PARALLEL_REG_NUM) {
		mutex_unlock(&g_faultmgr_parallel_lock);
		faultmgr_err("Parallel req num exceed max. (max=%u)\n", MAX_PARALLEL_REG_NUM);
		return -EBUSY;
	}

	atomic_inc(&parallel_req_num);
	mutex_unlock(&g_faultmgr_parallel_lock);
	if (in_param->timeout > 0) {
		ret = wait_event_interruptible_timeout(faultmgr_wq, !faultmgr_queue_is_newest(param.eventSerialNum),
			(long)msecs_to_jiffies((u32)in_param->timeout));
		if (ret == 0) { /* timeout */
			out_param->eventSerialNum = param.eventSerialNum;
			out_param->outEventNum = 0;
			atomic_dec(&parallel_req_num);
			return 0;
		}
	} else if (in_param->timeout < 0) {
		wait_event_interruptible(faultmgr_wq, !faultmgr_queue_is_newest(param.eventSerialNum));
	}

	ret = faultmgr_queue_get_event(&param);
	if (ret != 0) {
		goto fail;
	}

	out_param->eventSerialNum = param.eventSerialNum;
	out_param->outEventNum = param.outEventNum;

fail:
	atomic_dec(&parallel_req_num);
	return ret;
}
