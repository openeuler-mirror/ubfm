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
#include <linux/uidgid.h>
#include <linux/mutex.h>
#include "ucd.h"
#include "mami_string.h"
#include "udm.h"
#include "ures.h"
#include "mng_kernel.h"
#include "fmdevm.h"
#include "fmdevm.h"
#include "mng_devid.h"
#include "ubfm_common.h"
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
#include "udev_ucmi_pcie_espi.h"
#include "udev_ucmi_ops.h"
#include "udev_pcie_pub.h"
#endif

DEFINE_MUTEX(g_mng_ucd_dev_lock);

static int mng_add_dev_to_udm(struct mami_dev_info *dev_info, struct udev_hw_identify *guid_info, u32 devid)
{
	struct udm_dev_para para = {devid, dev_info->cna, dev_info->eid, guid_info->socket_id, guid_info->location, guid_info->chip_type, guid_info->connect_type};

	(void)ubfm_memcpy(para.guid, sizeof(para.guid), dev_info->guid, sizeof(dev_info->guid));
	return udm_add_dev(&para, &devid);
}

static int mng_add_local_dev_to_udm(struct udev_hw_identify *guid_info, u32 devid)
{
	struct udm_dev_para para = {devid, 0, 0, guid_info->socket_id, guid_info->location, guid_info->chip_type, guid_info->connect_type};

	(void)ubfm_memcpy(para.guid, sizeof(para.guid), guid_info->guid, sizeof(guid_info->guid));
	return udm_add_dev(&para, &devid);
}

static int mng_set_dev_create_case(u32 devid, struct mami_dev_info *dev_info)
{
	if (dev_info->createCase >= MAMI_KERN_RECREATE_CASE_MAX) {
#ifndef EMU_ST
		mng_err("create case para invalid. (para val must be in [%u, %u])\n",
			MAMI_KERN_RECREATE_CASE_INITIALIZE, MAMI_KERN_RECREATE_CASE_MAX - 1);
		return -EINVAL;
#endif
	}

	return ures_dev_set_attr(devid, MAMI_UDEV_RECREATE_CASE, &dev_info->createCase, sizeof(dev_info->createCase));
}

static int mng_ucd_create_dev(void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_dev_info *dev_info = NULL;
	struct udev_hw_identify guid_info;
	struct devm_create_dev_out *dev_out = (struct devm_create_dev_out *)out;
	char guid_str[GUID_STR_LEN];
	int ret;
	u32 devid, auto_local_devid;

	if ((in == NULL) || (in_len != sizeof(struct mami_dev_info)) || (out == NULL) ||
		(out_len != sizeof(struct devm_create_dev_out))) {
		mng_err("invalid param. (in=%d; in_len=%u; expect_len=%zu; out=%d; out_len=%u; expect_len=%zu)\n",
			in != NULL, in_len, sizeof(struct mami_dev_info), out != NULL, out_len, sizeof(struct devm_create_dev_out));
		return -EINVAL;
	}

	dev_info = (struct mami_dev_info *)in;
	ret = udev_guid_to_str(dev_info->guid, guid_str);
	if (ret != 0) {
		mng_err("hex to str fail. (ret=%d)\n", ret);
		return ret;
	}
	ret = ures_get_attr(guid_str, &guid_info, sizeof(guid_info));
	if (ret != 0) {
		mng_err("ures_get_attr fail. (ret=%d; guid=%s)\n", ret, guid_str);
		return ret;
	}

	ret = mng_get_devid(guid_info.slot_id, guid_info.socket_id, &devid);
	if (ret != 0) {
		mng_err("udev calc devid fail. (ret=%d)\n", ret);
		return ret;
	}

	mutex_lock(&g_mng_ucd_dev_lock);
	ret = mng_set_dev_create_case(devid, dev_info);
	if (ret != 0) {
		mutex_unlock(&g_mng_ucd_dev_lock);
		mng_err("mng_set_dev_recreate_case fail. (ret=%d; devid=%u; recreate_case=%u)\n",
			ret, devid, dev_info->createCase);
		return ret;
	}

	ret = ures_get_attr(MAMI_AUTO_LOCAL_DEVID, (void *)&auto_local_devid, sizeof(auto_local_devid));
	if ((ret == 0) && (auto_local_devid == devid)) {
		mng_err("Local device has been automatically created, manual creation is not supported.\n");
		mutex_unlock(&g_mng_ucd_dev_lock);
		return E_OPNOTSUPP_ON_DEV;
	}

	ret = mng_add_dev_to_udm(dev_info, &guid_info, devid);
	mutex_unlock(&g_mng_ucd_dev_lock);
	if ((ret != 0) && (ret != -EEXIST)) {
		mng_err("mng_add_dev_to_udm fail. (ret=%d;devid=%u)\n", ret, devid);
		return ret;
	}

	dev_out->ret = (ret == 0) ? 0 : EEXIST;
	dev_out->devid = devid;
	if (dev_out->ret == 0) {
		mng_info("mng_add_dev_to_udm success. (ret=%d; devid=%u)\n", dev_out->ret, devid);
	} else {
		mng_debug("mng_add_dev_to_udm success. (ret=%d; devid=%u)\n", dev_out->ret, devid);
	}
	return 0;
}

static int mng_ucd_get_local_devid(void *in, u32 in_len, void *out, u32 out_len)
{
	u32 i, dev_cnt;
	int ret;
	struct udm_dev_para para = {0};

	if ((out == NULL) || (out_len != sizeof(u32))) {
		mng_err("invalid param. (out=%d; out_len=%u; expect_len=%zu)\n", out != NULL, out_len, sizeof(u32));
		return -EINVAL;
	}

	dev_cnt = 0;
	for (i = 0; i < DEV_MAX_NUM; i++) {
		if (!udm_is_udevid_exist(i)) {
			continue;
		}
		dev_cnt++;

		ret = udm_get_dev_attr(i, &para);
		if (ret != 0) {
			return ret;
		}

		if (para.location == UDM_LOCAL) {
			*((u32 *)out) = para.udevid;
			return 0;
		}
	}

	mng_err("no %s found, failed to get local devid.\n", (dev_cnt == 0) ? "device" : "local device");
	return -ENODEV;
}

static int mng_find_local_guid_info(struct udev_hw_identify *guid_info)
{
	int ret;
	u32 i, num;
	struct ures_key *key = NULL;

	ret = ures_get_attr_key_list(&key, &num);
	if (ret != 0) {
		mng_err("failed to get ures attr tag list. (ret=%d)\n", ret);
		return ret;
	}

	for (i = 0; i < num; i++) {
		if (strncmp(key[i].name, GUID_STR_PREFIX, GUID_STR_PREFIX_LEN) != 0) {
			continue;
		}

		ret = ures_get_attr(key[i].name, guid_info, sizeof(*guid_info));
		if (ret != 0) {
			ures_put_attr_key_list(key);
			return ret;
		}

		if (guid_info->location == UDM_LOCAL) {
			ures_put_attr_key_list(key);
			return 0;
		}
	}

	ures_put_attr_key_list(key);
	return -EFAULT;
}

static int mng_ucd_create_local_dev(void *in, u32 in_len, void *out, u32 out_len)
{
	struct udev_hw_identify guid_info = {0};
	int ret;
	u32 devid;

	if ((out == NULL) || (out_len != sizeof(devid))) {
		mng_err("invalid param. (out=%d; out_len=%u; expect_len=%zu)\n", out != NULL, out_len, sizeof(devid));
		return -EINVAL;
	}

	ret = mng_find_local_guid_info(&guid_info);
	if (ret != 0) {
		mng_err("failed to find local guid info. (ret=%d)\n", ret);
		return ret;
	}

	ret = mng_get_devid(guid_info.slot_id, guid_info.socket_id, &devid);
	if (ret != 0) {
		mng_err("failed to calc devid. (slot_id=%u; socket_id=%u; ret=%d)\n", guid_info.slot_id, guid_info.socket_id, ret);
		return ret;
	}

	mutex_lock(&g_mng_ucd_dev_lock);
	ret = mng_add_local_dev_to_udm(&guid_info, devid);
	if ((ret != 0) && (ret != -EEXIST)) {
		mng_err("failed to add local dev to udm. (ret=%d; devid=%u)\n", ret, devid);
		mutex_unlock(&g_mng_ucd_dev_lock);
		return ret;
	}

	ret = ures_set_attr(MAMI_AUTO_LOCAL_DEVID, (void *)&devid, sizeof(devid));
	if (ret != 0) {
		mng_err("failed to set auto local devid to ures. (ret=%d; devid=%u)\n", ret, devid);
		mutex_unlock(&g_mng_ucd_dev_lock);
		return ret;
	}

	*((u32 *)out) = devid;
	mutex_unlock(&g_mng_ucd_dev_lock);
	return 0;
}

static int mng_ucd_destroy_dev(void *in, u32 in_len, void *out, u32 out_len)
{
	int ret;
	u32 *udevid = NULL;

	if ((in == NULL) || (in_len != sizeof(u32))) {
		mng_err("invalid param. (in=%d; in_len=%u; expect_len=%zu)\n", in != NULL, in_len, sizeof(u32));
		return -EINVAL;
	}
	udevid = (u32 *)in;
	mutex_lock(&g_mng_ucd_dev_lock);
	ret = udm_remove_dev(*udevid);
	mutex_unlock(&g_mng_ucd_dev_lock);
	if (ret != 0) {
		mng_err("udm remove dev fail. (devid=%u; ret=%d)\n", *udevid, ret);
	}
	return ret;
}

#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
static void mng_clear_attr_list(struct ures_key *key_list, u32 num)
{
	u32 i;

	for (i = 0; i < num; i++) {
		if (key_list[i].name[0] != '\0') {
			mng_info("guid del. (name:%s)\n", key_list[i].name);
			ures_del_attr(key_list[i].name);
		}
	}
}

static int mng_ucd_device_probe(void *in, u32 in_len, void *out, u32 out_len)
{
	int ret;
	struct ures_key *key_list = NULL;
	u32 num = 0;

	ret = ures_get_attr_key_list(&key_list, &num);
	if (unlikely(ret != 0)) {
		mng_err("Get attr tag list failed. (ret=%d; num=%u)\n", ret, num);
		return ret;
	}

	/* serialize probe with create/destroy to avoid concurrent ucmi get/remove on the same devid */
	mutex_lock(&g_mng_ucd_dev_lock);
	ret = udev_pcie_probe_all(key_list, num);
	mutex_unlock(&g_mng_ucd_dev_lock);

	mng_clear_attr_list(key_list, num);
	ures_put_attr_key_list(key_list);

	if (unlikely(ret != 0)) {
		mng_err("Detect device failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

static int mng_ucd_get_ucmi_win_lost(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	u32 *mode = in;
	u32 win_lost = 0;
	int ret = 0;

	if ((in == NULL) || (in_len != sizeof(u32))) {
		mng_err("invalid param. (in=%d; in_len=%u; expect_len=%zu)\n",
			in != NULL, in_len, sizeof(u32));
		return -EINVAL;
	}
	if ((out == NULL) || (out_len != sizeof(u32))) {
		mng_err("invalid param. (out=%d; out_len=%u; expect_len=%zu)\n", out != NULL, out_len, sizeof(u32));
		return -EINVAL;
	}

	switch (*mode) {
	case UCMI_WIN_CHECK_ALL:
		ret = ucmi_pcie_check_all_win_lost(devid, &win_lost);
		break;
	case UCMI_WIN_CHECK_AXI2:
		ret = ucmi_pcie_check_axi_win_lost(devid, 2, &win_lost);	/* check axi2 */
		break;
	default:
		return -EINVAL;
	}

	if (ret != 0) {
		return ret;
	}

	*(u32 *)out = win_lost;
	return 0;
}

#else
static int mng_ucd_device_probe(void *in, u32 in_len, void *out, u32 out_len)
{
	return 0;
}

static int mng_ucd_get_ucmi_win_lost(u32 devid, void *in, u32 in_len, void *out, u32 out_len)
{
	return E_OPNOTSUPP_ON_DEV;
}

#endif

STATIC int mng_ucd_set_managed_pid(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	int ret = 0;

	mutex_lock(&g_mng_ucd_dev_lock);
	ret = udm_set_managed_proc_pid(dev_id);
	mutex_unlock(&g_mng_ucd_dev_lock);
	if (ret != 0) {
		mng_err("udm set managed proc pid failed. (devid=%u; ret=%d)\n", dev_id, ret);
		return ret;
	}

	return 0;
}

static const struct {
	u8 sub_cmd;
	u32 proc_ctrl_bitmap;
	u8 is_dev_handler;
	void *call_fn;
} g_mng_ucd_cmds[] = {
	{MNG_CREATE_DEV, PROC_CTRL_ANY_USER, 0, mng_ucd_create_dev},
	{MNG_DESTROY_DEV, PROC_CTRL_ANY_USER, 0, mng_ucd_destroy_dev},
	{MNG_DEVICE_PROBE, PROC_CTRL_ANY_USER, 0, mng_ucd_device_probe},
	{MNG_CREATE_LOCAL_DEV, PROC_CTRL_ANY_USER, 0, mng_ucd_create_local_dev},
	{MNG_GET_LOCAL_DEVID, PROC_CTRL_ANY_USER, 0, mng_ucd_get_local_devid},
	{MNG_GET_UCMI_WIN_LOST, PROC_CTRL_MANAGED, 1, mng_ucd_get_ucmi_win_lost},
	{MNG_SET_MANAGED_PID, PROC_CTRL_MANAGED, 1, mng_ucd_set_managed_pid},
};

static const u32 g_mng_ucd_cmd_num = ARRAY_SIZE(g_mng_ucd_cmds);

static void mng_unregister_ucd_cmd_num(u32 in_cmd_num)
{
	u32 i = 0, cmd_num = in_cmd_num;
	int ret;

	cmd_num = cmd_num > g_mng_ucd_cmd_num ? g_mng_ucd_cmd_num : cmd_num;

	for (i = 0; i < cmd_num; i++) {
		if (g_mng_ucd_cmds[i].is_dev_handler) {
			ret = ucd_unregister_dev_cmd(UCD_CMD_MNG, g_mng_ucd_cmds[i].sub_cmd);
		} else {
			ret = ucd_unregister_cmd(UCD_CMD_MNG, g_mng_ucd_cmds[i].sub_cmd);
		}
		if (ret != 0) {
			mng_err("ucd unregister fail. (sub_cmd=%d)\n", g_mng_ucd_cmds[i].sub_cmd);
		}
	}
}

void mng_unregister_ucd_cmd(void)
{
	mng_unregister_ucd_cmd_num(g_mng_ucd_cmd_num);
}

int mng_register_ucd_cmd(void)
{
	u32 i;
	int ret;
	struct tag_ucd_feature feature;

	for (i = 0; i < g_mng_ucd_cmd_num; i++) {
		ucd_fill_feature_cmd(&feature, MODULE_MNG, UCD_CMD_MNG, g_mng_ucd_cmds[i].sub_cmd, g_mng_ucd_cmds[i].call_fn);
		ucd_set_proc_ctrl_bitmap(&feature, g_mng_ucd_cmds[i].proc_ctrl_bitmap);
		if (g_mng_ucd_cmds[i].is_dev_handler) {
			ret = ucd_register_dev_cmd(&feature);
		} else {
			ret = ucd_register_cmd(&feature);
		}
		if (ret != 0) {
			mng_err("ucd register cmd fail, will rollback. (ret=%d)\n", ret);
			mng_unregister_ucd_cmd_num(i);
			return ret;
		}
	}
	return 0;
}

int mng_dev_release(u32 devid, pid_t pid)
{
	int ret = 0;

	mutex_lock(&g_mng_ucd_dev_lock);
	ret = udm_dev_release(devid, pid);
	mutex_unlock(&g_mng_ucd_dev_lock);
	if (ret != 0) {
		mng_err("Failed to release device. (devid=%u, ret=%d)\n", devid, ret);
		return ret;
	}

	return 0;
}

/* The empty function is designed to meet the requirements of the ucd_register_module_dev_ops interface. */
int mng_dev_open(u32 devid, pid_t pid)
{
	return 0;
}

