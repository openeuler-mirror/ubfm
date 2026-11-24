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
#include "ubfabric.h"
#include "ubfabric.h"

#include "feature_loader.h"
#include "udm.h"
#include "ucd.h"

#include "ubfabric.h"
#include "ued_core.h"
#include "ued_proc_fs.h"

static int ued_dev_notify(u32 udevid, enum udm_notified_action action, u32 val)
{
	int ret = 0;

	if (action == UDM_INIT) {
		ret = ued_core_add_dev(udevid);
	} else if (action == UDM_UNINIT) {
		ret = ued_core_del_dev(udevid);
	}

	return ret;
}

static int ued_open(pid_t pid)
{
	return ued_core_add_task(MAMI_ANY_DEVID, (int)pid);
}

static int ued_release(pid_t pid)
{
	return ued_core_del_task(MAMI_ANY_DEVID, (int)pid);
}

static int ued_dev_open(u32 udevid, pid_t pid)
{
	return ued_core_add_task(udevid, (int)pid);
}
static int ued_dev_release(u32 udevid, pid_t pid)
{
	return ued_core_del_task(udevid, (int)pid);
}

static int ued_ucd_dev_create_grp(u32 udevid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ued_grp_para *para = (struct ued_grp_para *)in;

	if ((in == NULL) || (out == NULL) || (in_len != sizeof(struct ued_grp_para)) || (out_len != sizeof(u32))) {
		ued_err("Invalid para. (udevid=%u; in_len=%u; out_len=%u)\n", udevid, in_len, out_len);
		return -EINVAL;
	}

	if (strnlen(para->grp_name, EVENT_MAX_GRP_NAME_LEN) == EVENT_MAX_GRP_NAME_LEN) {
		ued_err("Invalid grp_name len\n");
		return -EINVAL;
	}

	return ued_core_create_grp(udevid, current->tgid, para->grp_name, para->thread_num, (u32 *)out);
}

static int ued_ucd_create_grp(void *in, u32 in_len, void *out, u32 out_len)
{
	return ued_ucd_dev_create_grp(MAMI_ANY_DEVID, in, in_len, out, out_len);
}

static int ued_ucd_dev_destroy_grp(u32 udevid, void *in, u32 in_len, void *out, u32 out_len)
{
	if ((in == NULL) || (in_len != sizeof(u32))) {
		ued_err("Invalid para. (udevid=%u; in_len=%u)\n", udevid, in_len);
		return -EINVAL;
	}

	return ued_core_destroy_grp(udevid, current->tgid, *(u32 *)in);
}

static int ued_ucd_destroy_grp(void *in, u32 in_len, void *out, u32 out_len)
{
	return ued_ucd_dev_destroy_grp(MAMI_ANY_DEVID, in, in_len, out, out_len);
}

static int ued_ucd_dev_subscribe_event(u32 udevid, void *in, u32 in_len, void *out, u32 out_len)
{
	struct ued_subscribe_event_para *para = (struct ued_subscribe_event_para *)in;

	if ((in == NULL) || (in_len != sizeof(struct ued_subscribe_event_para))) {
		ued_err("Invalid para. (udevid=%u; in_len=%u)\n", udevid, in_len);
		return -EINVAL;
	}

	return ued_core_subscribe_event(udevid, para->pid, para->gid, para->event_bitmap);
}

static int ued_ucd_subscribe_event(void *in, u32 in_len, void *out, u32 out_len)
{
	return ued_ucd_dev_subscribe_event(MAMI_ANY_DEVID, in, in_len, out, out_len);
}

static int ued_ucd_dev_wait_event(u32 udevid, void *in, u32 in_len, void *out, u32 out_len)
{
	if ((in == NULL) || (in_len != sizeof(struct ued_wait_event_para))) {
		ued_err("Invalid para. (udevid=%u; in_len=%u)\n", udevid, in_len);
		return -EINVAL;
	}

	return ued_core_wait_event(udevid, current->tgid, (struct ued_wait_event_para *)in);
}

static int ued_ucd_wait_event(void *in, u32 in_len, void *out, u32 out_len)
{
	return ued_ucd_dev_wait_event(MAMI_ANY_DEVID, in, in_len, out, out_len);
}

static int ued_ucd_dev_publish_event(u32 udevid, void *in, u32 in_len, void *out, u32 out_len)
{
	if ((in == NULL) || (in_len != sizeof(struct ued_publish_event_para))) {
		ued_err("Invalid para. (udevid=%u; in_len=%u)\n", udevid, in_len);
		return -EINVAL;
	}

	return ued_core_publish_event(udevid, (struct ued_publish_event_para *)in);
}

static int ued_ucd_publish_event(void *in, u32 in_len, void *out, u32 out_len)
{
	return ued_ucd_dev_publish_event(MAMI_ANY_DEVID, in, in_len, out, out_len);
}

static const struct {
	feature_handler handle;
	u32 proc_ctrl_bitmap;
} ued_ucd_cmds[UED_CMD_NUM] = {
	[UED_CREATE_GRP] = { ued_ucd_create_grp, PROC_CTRL_ANY_USER },
	[UED_DESTROY_GRP] = { ued_ucd_destroy_grp, PROC_CTRL_ANY_USER },
	[UED_SUBSCRIBE_EVENT] = { ued_ucd_subscribe_event, PROC_CTRL_MANAGED },
	[UED_WAIT_EVENT] = { ued_ucd_wait_event, PROC_CTRL_ANY_USER },
	[UED_PUBLISH_EVENT] = { ued_ucd_publish_event, PROC_CTRL_MANAGED }
};

static const struct {
	dev_feature_handler handle;
	u32 proc_ctrl_bitmap;
} ued_dev_ucd_cmds[UED_CMD_NUM] = {
	[UED_CREATE_GRP] = { ued_ucd_dev_create_grp, PROC_CTRL_ANY_USER },
	[UED_DESTROY_GRP] = { ued_ucd_dev_destroy_grp, PROC_CTRL_ANY_USER },
	[UED_SUBSCRIBE_EVENT] = { ued_ucd_dev_subscribe_event, PROC_CTRL_MANAGED },
	[UED_WAIT_EVENT] = { ued_ucd_dev_wait_event, PROC_CTRL_ANY_USER },
	[UED_PUBLISH_EVENT] = { ued_ucd_dev_publish_event, PROC_CTRL_MANAGED }
};

static void ued_uninit_user_sub_cmd(u8 max_sub_cmd)
{
	u8 sub_cmd;

	for (sub_cmd = 0; sub_cmd < max_sub_cmd; sub_cmd++) {
		(void)ucd_unregister_cmd(UCD_CMD_UED, sub_cmd);
		(void)ucd_unregister_dev_cmd(UCD_CMD_UED, sub_cmd);
	}
}

static int ued_init_user_sub_cmd(u8 max_sub_cmd)
{
	u8 sub_cmd;

	for (sub_cmd = 0; sub_cmd < max_sub_cmd; sub_cmd++) {
		struct tag_ucd_feature feature;
		int ret;

		ucd_fill_feature_cmd(&feature, module_ued, UCD_CMD_UED, sub_cmd, (void *)ued_ucd_cmds[sub_cmd].handle);

		/* Setting the Process Trustlist */
		ucd_set_proc_ctrl_bitmap(&feature, ued_ucd_cmds[sub_cmd].proc_ctrl_bitmap);

		ret = ucd_register_cmd(&feature);
		if (ret != 0) {
			ued_uninit_user_sub_cmd(sub_cmd);
			return ret;
		}

		feature.dev_fn = (void *)ued_dev_ucd_cmds[sub_cmd].handle;
		ucd_set_proc_ctrl_bitmap(&feature, ued_dev_ucd_cmds[sub_cmd].proc_ctrl_bitmap);

		ret = ucd_register_dev_cmd(&feature);
		if (ret != 0) {
			(void)ucd_unregister_cmd(UCD_CMD_UED, sub_cmd);
			ued_uninit_user_sub_cmd(sub_cmd);
			return ret;
		}
	}

	return 0;
}

static int ued_init_user_cmd(void)
{
	int ret;

	ret = ucd_register_module_ops(module_ued, ued_open, ued_release);
	if (ret != 0) {
		ued_err("Failed to register ucd ops. (ret=%d)\n", ret);
		return ret;
	}

	ret = ucd_register_module_dev_ops(module_ued, ued_dev_open, ued_dev_release, NULL);
	if (ret != 0) {
		(void)ucd_unregister_module_ops(module_ued);
		ued_err("Failed to register ucd dev ops. (ret=%d)\n", ret);
		return ret;
	}

	ret = ued_init_user_sub_cmd(UED_CMD_NUM);
	if (ret != 0) {
		(void)ucd_unregister_module_dev_ops(module_ued);
		(void)ucd_unregister_module_ops(module_ued);
		return ret;
	}

	return 0;
}

static void ued_uninit_user_cmd(void)
{
	ued_uninit_user_sub_cmd(UED_CMD_NUM);
	(void)ucd_unregister_module_dev_ops(module_ued);
	(void)ucd_unregister_module_ops(module_ued);
}

int ued_init(void)
{
	int ret;

	ued_proc_fs_init();

	ret = ued_core_init();
	if (ret != 0) {
		return ret;
	}

	ret = udm_notifier_register(module_ued, UDM_PRI1, ued_dev_notify);
	if (ret != 0) {
		ued_core_uninit();
		ued_err("Failed to register udm notify. (ret=%d)\n", ret);
		return ret;
	}

	ret = ued_core_add_dev(MAMI_ANY_DEVID);
	if (ret != 0) {
		udm_notifier_unregister(module_ued);
		ued_core_uninit();
		return ret;
	}

	ret = ued_init_user_cmd();
	if (ret != 0) {
		ued_core_del_dev(MAMI_ANY_DEVID);
		udm_notifier_unregister(module_ued);
		ued_core_uninit();
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ued_init, FEATURE_LOADER_STAGE_3);

void ued_uninit(void)
{
	ued_uninit_user_cmd();
	ued_core_del_dev(MAMI_ANY_DEVID);
	udm_notifier_unregister(module_ued);
	ued_core_uninit();
	ued_proc_fs_uninit();
}
DECLARE_FEATURE_AUTO_UNINIT(ued_uninit, FEATURE_LOADER_STAGE_3);

