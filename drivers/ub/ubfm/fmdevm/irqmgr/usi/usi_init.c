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
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <ub/ubus/ubus.h>
#include <uapi/ub/ubus/ubus_regs.h>
#include "mami_string.h"

#include "feature_loader.h"
#include "feature_kern.h"
#include "ubfm_common.h"
#include "fmdevm.h"
#include "udm.h"
#include "uid.h"
#include "ures.h"
#include "usi_log.h"
#include "usi_ub_def.h"
#include "usi_irq.h"
#include "usi_acpi.h"
#include "usi_ubus_api.h"
#include "ubfabric.h"
#include "ubfabric.h"
#include "ucd.h"
#include "udev_vsock.h"

#define VSOCK_DRV_VERSION 0
#define WAIT_TIMEOUT_MS 1000 /* ms */
#define BIT_CNT_PER_DIE 4

static u32 g_is_register_irq;
static atomic_t g_die_irq[DEV_DIE_MAX_NUM] = {0};
static wait_queue_head_t g_irq_wq;
static struct task_struct *g_client_task;

LIST_HEAD(g_usi_irq_vector_list);
DEFINE_MUTEX(g_usi_irq_vector_lock);
DEFINE_MUTEX(g_client_task_lock);

#define USI_INVALID_LOCAL_EID			   0xFFFFFFFF
STATIC u32 g_usi_local_eid[DEV_DIE_MAX_NUM];
DEFINE_MUTEX(g_usi_local_eid_lock);

static const struct ub_device_id g_usi_device_type_tbl[] = {
	{UB_ENTITY(UB_VENDOR_ID, CTRL_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, CTRLED_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, KUNPENG_960_CTRL_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, KUNPENG_960_CTRLED_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, ASCEND_V1_CTRLED_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, DPU_CTRLED_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, SSD_CTRL_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, SSD_CTRLED_NODE_CTRL_UE_DEVICE_ID)},
	{UB_ENTITY(UB_VENDOR_ID, ASCEND_V1_CTRL_NODE_CTRL_UE_DEVICE_ID)},
	{0},
};

void parse_irq_bitmap(u32 devid, u32 irq_bitmap)
{
	struct uid_irq_info irq_info = {0};
	struct timespec64 ts = {0};
	int die_id;

	(void)ktime_get_real_ts64(&ts);
	irq_info.irq_time = ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);

	for (die_id = 0; die_id < DEV_DIE_MAX_NUM; die_id++) {
		u32 die_irq = (irq_bitmap >> (die_id * BIT_CNT_PER_DIE)) & 0xF;

		if (die_irq & (1U << USI_IRQ_VEC_CTRLQ)) {
			irq_info.irq_para.ctrlq.in_irq = 1;
			irq_info.irq_para.ctrlq.dieid = (die_id & 0xFF);
			uid_trigger_intr(devid, UID_TYPE_CTRLQ, irq_info);
		}

		if (die_irq & (1U << USI_IRQ_VEC_RAS)) {
			uid_trigger_intr(devid, UID_TYPE_RAS, irq_info);
		}

		if (die_irq & (1U << USI_IRQ_VEC_MSGQ)) {
			irq_info.irq_para.mgmtq.in_irq = 1;
			irq_info.irq_para.mgmtq.dieid = (die_id & 0xFF);
			uid_trigger_intr(devid, UID_TYPE_MGMTQ, irq_info);
		}
	}
}

STATIC int vsock_client_start(void *data)
{
	int ret;
	struct vsock_msg_pkt send_msg = {0}, recv_msg = {0};

	ret = vsock_client_init();
	if (ret != 0) {
		goto exit;
	}

	/* get irq */
	while (!kthread_should_stop()) {
		send_msg.cmd = VSOCK_CMD_GET_IRQ;
		ret = vsock_client_send_recv(&send_msg, &recv_msg);
		if (ret != 0 || recv_msg.cmd != VSOCK_CMD_GET_IRQ) {
			goto exit;
		}

		parse_irq_bitmap(udm_get_local_devid(), recv_msg.u.get_irq_rsp.irq_bitmap);
	}

exit:
	usi_info("vsock client thread exit\n");
	vsock_client_exit();
	g_client_task = NULL;
	return 0;
}

STATIC int vsock_client_start_open(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	int ret;
	struct vsock_msg_pkt send_msg = {0}, recv_msg = {0};

	mutex_lock(&g_client_task_lock);
	if (udev_get_env_mode() != ENV_VSOCK_CLIENT || g_client_task != NULL) {
		mutex_unlock(&g_client_task_lock);
		return 0;
	}

	ret = vsock_client_init();
	if (ret != 0) {
		mutex_unlock(&g_client_task_lock);
		return ret;
	}

	send_msg.cmd = VSOCK_CMD_REG_IRQ;
	ret = vsock_client_send_recv(&send_msg, &recv_msg);
	if (ret != 0 || recv_msg.cmd != VSOCK_CMD_REG_IRQ) {
		vsock_client_exit();
		mutex_unlock(&g_client_task_lock);
		return (ret == 0) ? -EINVAL : ret;
	}

	/* get version number */
	send_msg.cmd = VSOCK_CMD_GET_VERSION;
	ret = vsock_client_send_recv(&send_msg, &recv_msg);
	if (ret != 0 || recv_msg.cmd != VSOCK_CMD_GET_VERSION || recv_msg.u.get_ver_rsp.version != VSOCK_DRV_VERSION) {
		vsock_client_exit();
		mutex_unlock(&g_client_task_lock);
		return (ret == 0) ? -EINVAL : ret;
	}

	g_client_task = kthread_run(vsock_client_start, NULL, "vsock_client");
	if (IS_ERR(g_client_task)) {
		usi_err("create vsock client thread failed. (err=%ld)\n", PTR_ERR(g_client_task));
		vsock_client_exit();
		g_client_task = NULL;
		mutex_unlock(&g_client_task_lock);
		return -EAGAIN;
	}
	mutex_unlock(&g_client_task_lock);
	return 0;
}

STATIC int vsock_client_end_open(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	if (udev_get_env_mode() != ENV_VSOCK_CLIENT) {
		return 0;
	}

	mutex_lock(&g_client_task_lock);
	if (g_client_task) {
		kthread_stop(g_client_task);
		g_client_task = NULL;
	}
	mutex_unlock(&g_client_task_lock);
	return 0;
}

static const struct {
	u8 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler call_fn;
} g_usi_ucd_cmds[] = {
	{USI_CREATE_CLIENT, PROC_CTRL_MANAGED, vsock_client_start_open},
	{USI_CLOSE_CLIENT, PROC_CTRL_MANAGED, vsock_client_end_open},
};

STATIC bool vsock_server_irq_pending(void)
{
	int i;

	for (i = 0; i < DEV_DIE_MAX_NUM; i++) {
		if (atomic_read(&g_die_irq[i]) != 0) {
			return true;
		}
	}
	return false;
}

STATIC irqreturn_t usi_irq_handler(int irq, void *data)
{
	struct usi_irq_data *irq_data = (struct usi_irq_data *)data;
	struct uid_irq_info irq_info = {0};
	struct timespec64 ts = {0};

	(void)ktime_get_real_ts64(&ts);
	irq_info.irq_time = ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);

	if (irq_data->irq_idx == USI_IRQ_VEC_CTRLQ) {
		irq_info.irq_para.ctrlq.in_irq = 1;
		irq_info.irq_para.ctrlq.dieid = (irq_data->dieid & 0xFF);
		uid_trigger_intr(irq_data->devid, UID_TYPE_CTRLQ, irq_info);
		return IRQ_HANDLED;
	}

	if (irq_data->irq_idx == USI_IRQ_VEC_RAS) {
		uid_trigger_intr(irq_data->devid, UID_TYPE_RAS, irq_info);
		return IRQ_HANDLED;
	}

	/* MGMTQ and MSGQ share one interrupt vector */
	if (irq_data->irq_idx == USI_IRQ_VEC_MSGQ) {
		irq_info.irq_para.mgmtq.in_irq = 1;
		irq_info.irq_para.mgmtq.dieid = (irq_data->dieid & 0xFF);
		uid_trigger_intr(irq_data->devid, UID_TYPE_MGMTQ, irq_info);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

STATIC irqreturn_t usi_server_irq_handler(int irq, void *data)
{
	struct usi_irq_data *irq_data = (struct usi_irq_data *)data;
	u32 die_id =  irq_data->dieid;
	u32 irq_bits = 1U << (irq_data->irq_idx);
	/* merge the interrupts of this die */
	atomic_or(irq_bits, &g_die_irq[die_id]);
	/* wake up the service thread */
	wake_up(&g_irq_wq);
	return IRQ_HANDLED;
}

STATIC bool usi_is_local_dev(struct ub_entity *uent)
{
	enum usi_ub_guid_device_id local_device_id[] = {
		CTRL_NODE_CTRL_UE_DEVICE_ID,
		SSD_CTRL_NODE_CTRL_UE_DEVICE_ID,
		KUNPENG_960_CTRL_NODE_CTRL_UE_DEVICE_ID,
		ASCEND_V1_CTRL_NODE_CTRL_UE_DEVICE_ID,
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(local_device_id); ++i) {
		if (uent_device(uent) == local_device_id[i]) {
			return true;
		}
	}
	return false;
}

static int usi_get_udm_dev_attr_by_udev(struct ub_entity *uent, struct udm_dev_para *dev_para, u32 *die_id)
{
	char ctrl_ue_param_key[URES_NAME_LEN];
	u64 eid, die_num, i;
	u32 devid;
	int ret;

	for (devid = 0; devid < DEV_MAX_NUM; devid++) {
		if (!udm_is_udevid_exist(devid)) {
			continue;
		}

		ret = udm_get_dev_attr(devid, dev_para);
		if (ret != 0) {
			usi_err("Get dev attr failed. (devid=%u)\n", devid);
			continue;
		}

		ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
		if ((ret != 0) || (die_num > DEV_DIE_MAX_NUM)) {
			usi_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
			continue;
		}

		for (i = 0; i < die_num; i++) {
			(void)UBFM_SPRINTF(ctrl_ue_param_key, sizeof(ctrl_ue_param_key), MAMI_DIEN_UE1_EID, (u32)i, 0);
			ret = ures_get_key_value(devid, ctrl_ue_param_key, &eid);
			if (ret != 0) {
				continue;
			}

			if ((u32)eid == uent->eid) {
				*die_id = (u32)i;
				return 0;
			}
		}
	}

	return -ENODEV;
}

static void usi_fill_irq_data(struct usi_irq_data *irq_data, u32 devid, u32 die_id,
							  struct ub_entity *uent, u32 irq_idx)
{
	irq_data->cna = uent->cna;
	irq_data->eid = uent->eid;
	irq_data->irq_idx = irq_idx;
	irq_data->devid = devid;
	irq_data->dieid = die_id;
}

static int usi_dev_request_irqs(struct ub_entity *uent, u32 devid, u32 die_id)
{
	int ret, i, j;
	struct usi_irq_data_vector *data_vector = NULL;
	struct usi_irq_data *irq_data = NULL;
	u64 ver;

	ret = usi_ub_alloc_irq_vectors(uent, USI_IRQ_VEC_NUM, USI_IRQ_VEC_NUM);
	if (ret < USI_IRQ_VEC_NUM) {
		/* if allocation fails, also actively release, to avoid the next allocation possibly failing due to resources not being properly released */
		usi_ub_disable_intr(uent);
		usi_err("Alloc irq vector failed. (devid=%u; dieid=%u; eid=%u; no_intr=%u; int_type=%u; ret=%d)\n",
				devid, die_id, uent->eid, uent->no_intr, uent->intr_type1, ret);
		return (ret == 0) ? -ENODATA : ret;
	}

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &ver);
	if (ret != 0) {
		usi_ub_disable_intr(uent);
		usi_err("Get udie version failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	if ((enum mami_udie_version)ver == UDIE_VERSION_V1) {
		/* USI interrupt scheme, needs to configure the ITS address as PA to match the root window of the management node. Otherwise USI is not connected */
		if (!usi_is_local_dev(uent) && !feature_kern_is_ssd(devid)) {
			ret = usi_cfgspace_to_its_pa_by_acpi(uent);
			if (ret != 0) {
				usi_ub_disable_intr(uent);
				return ret;
			}
		}
	}

	data_vector = kzalloc(sizeof(struct usi_irq_data_vector), GFP_KERNEL | ___GFP_ACCOUNT);
	if (data_vector == NULL) {
		usi_ub_disable_intr(uent);
		usi_err("Alloc irq data vector failed. (devid=%u; dieid=%u; eid=%u)\n", devid, die_id, uent->eid);
		return -ENOMEM;
	}

	for (i = 0; i < USI_IRQ_VEC_NUM; i++) {
		irq_data = &data_vector->vector[i];
		usi_fill_irq_data(irq_data, devid, die_id, uent, (u32)i);
		irq_data->irq = usi_ub_irq_vector(uent, (u32)i);
		if (irq_data->irq < 0) {
			usi_err("Get irq failed. (devid=%u; dieid=%u; eid=%u; irq_idx=%d; ret=%d)\n", devid, die_id,
					uent->eid, i, irq_data->irq);
			ret = irq_data->irq;
			goto fail;
		}

		(void)UBFM_SPRINTF(irq_data->name, USI_IRQ_NAME_LEN, "usi_%u_%u_%d", devid, die_id, i);
		if (udev_get_env_mode() == ENV_VSOCK_SERVER) {
			ret = request_irq(irq_data->irq, usi_server_irq_handler, IRQF_SHARED, irq_data->name, irq_data);
		} else {
			ret = request_irq(irq_data->irq, usi_irq_handler, IRQF_SHARED, irq_data->name, irq_data);
		}
		if (ret != 0) {
			usi_err("Request irq failed. (devid=%u; dieid=%u; eid=%u; irq_idx=%d; ret=%d)\n", devid,
					die_id, uent->eid, i, ret);
			goto fail;
		}
		usi_info("Register irq success. (devid=%u; dieid=%u; eid=%u; irq_idx=%d; irq_name=%s)\n",
				 devid, die_id, uent->eid, i, irq_data->name);
	}

	mutex_lock(&g_usi_irq_vector_lock);
	list_add_tail(&data_vector->entry, &g_usi_irq_vector_list);
	mutex_unlock(&g_usi_irq_vector_lock);

	return 0;
fail:
	for (j = 0; j < i; j++) {
		irq_data = &data_vector->vector[j];
		(void)free_irq(irq_data->irq, irq_data);
	}
	kfree(data_vector);
	usi_ub_disable_intr(uent);
	return ret;
}


static int usi_server_dev_irq_init(struct ub_entity *uent, u32 die_id)
{
	int ret;

	ret = usi_ub_set_user_info(uent);
	if (ret != 0) {
		usi_err("Set user info failed. (die_id=%u; eid=%u; ret=%d)\n", die_id, uent->eid, ret);
		return ret;
	}

	usi_ub_entity_enable(uent, 1); /* 1 means enable */
	ret = usi_dev_request_irqs(uent, DEV_MAX_NUM, die_id);
	if (ret != 0) {
		usi_ub_entity_enable(uent, 0); /* 1 means disable */
		usi_ub_unset_user_info(uent);
	}

	return ret;
}

static void usi_dev_free_irqs(struct ub_entity *uent)
{
	struct usi_irq_data_vector *pos = NULL;
	struct usi_irq_data_vector *tmp = NULL;
	struct usi_irq_data *irq_data = NULL;
	int i;

	mutex_lock(&g_usi_irq_vector_lock);
	list_for_each_entry_safe(pos, tmp, &g_usi_irq_vector_list, entry) {
		if (pos->vector[0].eid != uent->eid) {
			continue;
		}
		list_del(&pos->entry);

		for (i = 0; i < USI_IRQ_VEC_NUM; i++) {
			irq_data = &pos->vector[i];
			(void)free_irq(irq_data->irq, irq_data);
		}
		kfree(pos);
	}
	mutex_unlock(&g_usi_irq_vector_lock);
	usi_ub_disable_intr(uent);
	usi_ub_unset_user_info(uent);
}

static int usi_dev_irq_init(struct ub_entity *uent)
{
	int ret;
	struct udm_dev_para dev_para;
	u32 die_id = 0;

	ret = usi_get_udm_dev_attr_by_udev(uent, &dev_para, &die_id);
	if (ret != 0) {
		usi_err("Get dev attr by udev failed. (eid=%u; ret=%d)\n", uent->eid, ret);
		return ret;
	}

	ret = usi_ub_set_user_info(uent);
	if (ret != 0) {
		usi_err("Set user info failed. (devid=%u; die_id=%u; eid=%u; ret=%d)\n", dev_para.udevid, die_id,
				uent->eid, ret);
		return ret;
	}

	usi_ub_entity_enable(uent, 1);
	ret = usi_dev_request_irqs(uent, dev_para.udevid, die_id);
	if (ret != 0) {
		usi_ub_entity_enable(uent, 0);
		usi_ub_unset_user_info(uent);
	}

	return ret;
}

static void usi_dev_irq_uninit(struct ub_entity *uent)
{
	usi_dev_free_irqs(uent);
}

STATIC int usi_ctrl_ue_probe(struct ub_entity *uent, const struct ub_device_id *id)
{
	char guid_str[256] = {0};
	u32 die_id = 0;

	if (uent == NULL || id == NULL || uent->ubc == NULL) {
		usi_err("Invalid para.\n");
		return -EINVAL;
	}

	(void)ub_show_guid(&uent->guid, guid_str);
	usi_info("ctrl ue probe. (eid=%u; cna=%u; entity_idx=%u; ent_type=%d; no_intr=%u;"
			 "intr_type1=%u; intr_en=%u)\n",
			 uent->eid, uent->cna, uent->entity_idx, uent_type(uent),
			 uent->no_intr, uent->intr_type1, uent->intr_enabled);
	usi_info("ctrl ue probe. (eid=%u; local_dev=%d; guid=%s)\n", uent->eid, usi_is_local_dev(uent), guid_str);
	/* local devices postpone interrupt registration until device creation */
	if (usi_is_local_dev(uent)) {
		mutex_lock(&g_usi_local_eid_lock);
		die_id = uent->ubc->ctl_no;

		if (die_id >= DEV_DIE_MAX_NUM) {
			usi_err("Too many CtrlUe. (eid=%u; dieid=%u)\n", uent->eid, die_id);
			mutex_unlock(&g_usi_local_eid_lock);
			return -EINVAL;
		}

		g_usi_local_eid[die_id] = uent->eid;
		mutex_unlock(&g_usi_local_eid_lock);
		return 0;
	}

	return usi_dev_irq_init(uent);
}

STATIC void usi_ctrl_ue_remove(struct ub_entity *uent)
{
	if (uent == NULL) {
		usi_err("Invalid para.\n");
		return;
	}

	usi_info("USI ctrl ue removed. (eid=%u;  ent_type=%d)\n", uent->eid, uent_type(uent));
	usi_dev_irq_uninit(uent);
}

static void usi_local_dev_uninit(u32 devid)
{
	struct ub_entity *uent = NULL;
	u64 die_num = 0;
	u32 die_id = 0;
	u32 eid = 0;
	int ret = 0;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if ((ret != 0) || (die_num > DEV_DIE_MAX_NUM)) {
		usi_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return;
	}

	for (die_id = 0; die_id < (u32)die_num; die_id++) {
		eid = g_usi_local_eid[die_id];
		if (eid == USI_INVALID_LOCAL_EID) {
			usi_err("CtrlUe has not probed. (devid=%u; die_id=%u)\n", devid, die_id);
			continue;
		}

		uent = usi_ub_get_ent_by_eid(eid);
		if (uent == NULL) {
			usi_err("Get entity failed. (devid=%u; die_id=%u; eid=%u)\n", devid, die_id, eid);
			continue;
		}

		usi_dev_irq_uninit(uent);
	}
	usi_info("USI local dev uninit success. (devid=%u)\n", devid);
}

static void usi_server_local_dev_uninit(void)
{
	struct ub_entity *uent = NULL;
	u32 die_id, eid;

	for (die_id = 0; die_id < DEV_DIE_MAX_NUM; die_id++) {
		eid = g_usi_local_eid[die_id];
		if (eid == USI_INVALID_LOCAL_EID) {
			continue;
		}

		uent = usi_ub_get_ent_by_eid(eid);
		if (uent == NULL) {
			usi_err("Get entity failed. (die_id=%u; eid=%u)\n", die_id, eid);
			continue;
		}

		usi_dev_irq_uninit(uent);
	}
	usi_info("USI local dev uninit success.\n");
}

static int usi_local_dev_init(u32 devid)
{
	struct ub_entity *uent = NULL;
	char ctrl_ue_param_key[URES_NAME_LEN];
	u64 die_num = 0;
	u32 die_id = 0;
	u32 eid = 0;
	int ret = 0;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num);
	if (ret != 0) {
		usi_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	if (die_num > DEV_DIE_MAX_NUM) {
		usi_err("Invalid die_num. (devid=%u; die_num=%llu; max_val=%u; ret=%d)\n", devid, die_num, DEV_DIE_MAX_NUM, ret);
		return -EINVAL;
	}

	for (die_id = 0; die_id < (u32)die_num; die_id++) {
		eid = g_usi_local_eid[die_id];
		if (eid == USI_INVALID_LOCAL_EID) {
			usi_err("CtrlUe has not probed. (devid=%u; die_id=%u)\n", devid, die_id);
			return -EINVAL;
		}

		(void)UBFM_SPRINTF(ctrl_ue_param_key, sizeof(ctrl_ue_param_key), MAMI_DIEN_UE1_EID, die_id, 0);
		ret = ures_set_key_value(devid, ctrl_ue_param_key, (u64)eid);
		if (ret != 0) {
			usi_err("Set eid failed. (devid=%u; die_id=%u; eid=%u)\n", devid, die_id, eid);
			return ret;
		}
	}

	for (die_id = 0; die_id < (u32)die_num; die_id++) {
		eid = g_usi_local_eid[die_id];

		uent = usi_ub_get_ent_by_eid(eid);
		if (uent == NULL) {
			usi_err("Get entity failed. (devid=%u; die_id=%u; eid=%u)\n", devid, die_id, eid);
			ret = -ENODEV;
			goto fail;
		}

		ret = usi_dev_irq_init(uent);
		if (ret != 0) {
			usi_err("USI local dev init failed. (devid=%u; die_id=%u; eid=%u)\n", devid, die_id, eid);
			goto fail;
		}
	}

	usi_info("USI local dev init success. (devid=%u)\n", devid);
	return 0;
fail:
	usi_local_dev_uninit(devid);
	return ret;
}

static int usi_server_local_dev_init(void)
{
	struct ub_entity *uent = NULL;
	u32 die_id, eid;
	int ret;

	for (die_id = 0; die_id < DEV_DIE_MAX_NUM; die_id++) {
		eid = g_usi_local_eid[die_id];
		if (g_usi_local_eid[die_id] == USI_INVALID_LOCAL_EID) {
			continue;
		}
		uent = usi_ub_get_ent_by_eid(eid);
		if (uent == NULL) {
			usi_err("Get entity failed. (die_id=%u; eid=%u)\n", die_id, eid);
			ret = -ENODEV;
			goto fail;
		}
		ret = usi_server_dev_irq_init(uent, die_id);
		if (ret != 0) {
			usi_err("USI local dev init failed. (die_id=%u; eid=%u)\n", die_id, eid);
			goto fail;
		}
	}

	usi_info("USI local dev init success.\n");
	return 0;
fail:
	usi_server_local_dev_uninit();
	return ret;
}

STATIC void usi_irq_vector_destroy(void)
{
	struct usi_irq_data_vector *pos = NULL;
	struct usi_irq_data_vector *tmp = NULL;
	struct usi_irq_data *irq_data = NULL;
	struct ub_entity *uent = NULL;
	int i;

	mutex_lock(&g_usi_irq_vector_lock);
	list_for_each_entry_safe(pos, tmp, &g_usi_irq_vector_list, entry) {
		list_del(&pos->entry);

		for (i = 0; i < USI_IRQ_VEC_NUM; i++) {
			irq_data = &pos->vector[i];
			(void)free_irq(irq_data->irq, irq_data);
		}

		uent = usi_ub_get_ent_by_eid(pos->vector[0].eid);
		if (uent == NULL) {
			usi_err("Get entity failed. (devid=%u; die_id=%u; eid=%u)\n", pos->vector[0].devid,
					pos->vector[0].dieid, pos->vector[0].eid);
		} else {
			usi_ub_disable_intr(uent);
		}

		kfree(pos);
	}
	mutex_unlock(&g_usi_irq_vector_lock);
}

STATIC int usi_udm_notify(u32 devid, enum udm_notified_action action, u32 val)
{
	int ret;
	struct udm_dev_para dev_para;

	ret = udm_get_dev_attr(devid, &dev_para);
	if (ret != 0) {
		usi_err("Get dev attr failed. (action=%u; devid=%u)\n", action, devid);
		return ret;
	}

	if (dev_para.location != UDM_LOCAL) {
		return 0;
	}

	if (action == UDM_INIT) {
		ret = usi_local_dev_init(devid);
	} else if (action == UDM_UNINIT) {
		usi_local_dev_uninit(devid);
	}
	return ret;
}

static struct ub_driver g_ctrl_ue_driver = {
	.name = "usi_irq_drv",
	.id_table = g_usi_device_type_tbl,
	.probe = usi_ctrl_ue_probe,
	.remove = usi_ctrl_ue_remove,
};

void usi_unregister_ucd_cmd(void)
{
	int ret;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(g_usi_ucd_cmds); i++) {
		ret = ucd_unregister_dev_cmd(UCD_CMD_USI, g_usi_ucd_cmds[i].sub_cmd);
		if (ret != 0) {
			usi_err("ucd unregister fail. (sub_cmd=%d)\n", g_usi_ucd_cmds[i].sub_cmd);
		}
	}
}

int usi_register_ucd_cmd(void)
{
	int ret;
	u32 i;
	struct tag_ucd_feature feature = {0};

	for (i = 0; i < ARRAY_SIZE(g_usi_ucd_cmds); i++) {
		ucd_fill_feature_cmd(&feature, MODULE_USI_NAME, UCD_CMD_USI, g_usi_ucd_cmds[i].sub_cmd,
			g_usi_ucd_cmds[i].call_fn);
		ucd_set_proc_ctrl_bitmap(&feature, g_usi_ucd_cmds[i].proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&feature);
		if (ret != 0) {
			usi_err("ucd register cmd fail, will rollback. (ret=%d)\n", ret);
			usi_unregister_ucd_cmd();
			return ret;
		}
	}

	return 0;
}

STATIC int vsock_server_handler_reg_irq(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp)
{
	int ret;

	if (req->cmd != VSOCK_CMD_REG_IRQ) {
		return -EINVAL;
	}

	if (g_is_register_irq != 1) {
		ret = usi_server_local_dev_init();
		if (ret != 0) {
			return ret;
		}
	}
	g_is_register_irq = 1;

	resp->cmd = VSOCK_CMD_REG_IRQ;
	return 0;
}

STATIC int vsock_server_handler_get_version(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp)
{
	if (req->cmd != VSOCK_CMD_GET_VERSION) {
		return -EINVAL;
	}

	resp->cmd = VSOCK_CMD_GET_VERSION;
	resp->u.get_ver_rsp.version = VSOCK_DRV_VERSION;
	return 0;
}

STATIC int vsock_server_wait_for_irq(struct vsock_msg_pkt *resp)
{
	int ret = 0;

	ret = wait_event_interruptible_timeout(g_irq_wq, vsock_server_irq_pending(), (long)msecs_to_jiffies(WAIT_TIMEOUT_MS));
	if (ret <= 0) { /* no interrupt reported */
		resp->u.get_irq_rsp.irq_bitmap = 0;
		return -ETIMEDOUT;
	}
	return 0; /* interrupt reported */
}

STATIC int vsock_server_handler_get_irq(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp)
{
	int ret, new, i, old;
	u32 irq_bitmap = 0;

	if (req->cmd != VSOCK_CMD_GET_IRQ) {
		return -EINVAL;
	}

	resp->cmd = VSOCK_CMD_GET_IRQ;

	/* wait for irq to be reported, if no report for more than 1s return 0 directly; if there is, report the interrupt directly */
	ret = vsock_server_wait_for_irq(resp);
	if (ret != 0) {
		return 0;
	}

	for (i = 0; i < DEV_DIE_MAX_NUM; i++) {
		do {
			old = atomic_read(&g_die_irq[i]);
			if (old == 0) {
				break;
			}
			new = atomic_cmpxchg(&g_die_irq[i], old, 0);
		} while (new != old); /* loop CAS to ensure atomic update, when old and new are equal the original interrupt flag has been cleared (set to 0) */
		irq_bitmap |= (old << (i * BIT_CNT_PER_DIE));
	}

	resp->u.get_irq_rsp.irq_bitmap = irq_bitmap;
	return 0;
}

STATIC void vsock_server_handler_unreg_irq(void)
{
	if (g_is_register_irq != 0) {
		usi_server_local_dev_uninit();
		g_is_register_irq = 0;
	}
}

int usi_server_reg_handler(void)
{
	int ret;

	ret = vsock_register_server_cmd_handler(VSOCK_CMD_REG_IRQ, vsock_server_handler_reg_irq);
	ret |= vsock_register_server_cmd_handler(VSOCK_CMD_GET_VERSION, vsock_server_handler_get_version);
	ret |= vsock_register_server_cmd_handler(VSOCK_CMD_GET_IRQ, vsock_server_handler_get_irq);
	ret |= vsock_register_disconnect_handler(vsock_server_handler_unreg_irq);
	if (ret != 0) {
		usi_err("Usi server reg handler failed. (ret=%d)\n", ret);
	}

	return ret;
}

int usi_init(void)
{
	int ret;
	u32 i;

	if (udev_get_env_mode() == ENV_VSOCK_CLIENT) {
		return usi_register_ucd_cmd();
	}

	ret = usi_ubus_api_init();
	if (ret != 0) {
		usi_err("Ubus api init failed. (ret=%d)\n", ret);
		return ret;
	}

	for (i = 0; i < DEV_DIE_MAX_NUM; i++) {
		g_usi_local_eid[i] = USI_INVALID_LOCAL_EID;
	}

	ret = usi_ub_register_driver(&g_ctrl_ue_driver);
	if (ret != 0) {
		usi_ubus_api_uninit();
		usi_err("Register ub driver failed. (ret=%d)\n", ret);
		return ret;
	}

	if (udev_get_env_mode() == ENV_VSOCK_SERVER) {
		init_waitqueue_head(&g_irq_wq);
		ret = usi_server_reg_handler();
	} else {
		ret = udm_notifier_register(MODULE_USI_NAME, UDM_PRI4, usi_udm_notify);
	}

	if (ret != 0) {
		usi_ub_unregister_driver(&g_ctrl_ue_driver);
		usi_ubus_api_uninit();
		return ret;
	}

	usi_info("Register usi success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(usi_init, FEATURE_LOADER_STAGE_6);

void usi_uninit(void)
{
	if (udev_get_env_mode() == ENV_VSOCK_CLIENT) {
		usi_unregister_ucd_cmd();
		mutex_lock(&g_client_task_lock);
		if (g_client_task) {
			kthread_stop(g_client_task);
			g_client_task = NULL;
		}
		mutex_unlock(&g_client_task_lock);
		return;
	}

	usi_irq_vector_destroy();
	usi_ub_unregister_driver(&g_ctrl_ue_driver);
	if (udev_get_env_mode() != ENV_VSOCK_SERVER) {
		(void)udm_notifier_unregister(MODULE_USI_NAME);
	}
	usi_ubus_api_uninit();
	usi_info("Unregister usi success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(usi_uninit, FEATURE_LOADER_STAGE_6);
