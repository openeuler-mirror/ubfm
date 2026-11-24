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
#include "feature_loader.h"
#include "fmdevm.h"
#include "ucd.h"
#include "ubfabric.h"
#include "ubfabric.h"
#include "ulog.h"
#include "ulog_handle.h"

static int ulog_ucd_set_log_cfg(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_log_cfg *cfg = (struct mami_log_cfg *)in;
	struct ulog_cfg ulog_cfg = {0};

	if ((in == NULL) || (in_len != sizeof(struct mami_log_cfg))) {
		mami_err("ulog", "in is null or len is invalid. (inlen=%u;valid_len=%zu)\n", in_len, sizeof(struct mami_log_cfg));
		return -EINVAL;
	}
	if ((dev_id != cfg->devid) || (cfg->devid >= DEV_MAX_NUM)) {
		mami_err("ulog", "Devid is invalid. (dev_id=%u;cfg_dev_id=%u)\n", dev_id, cfg->devid);
		return -EINVAL;
	}
	ulog_cfg.configMask = cfg->configMask;
	ulog_cfg.kernLogLevel = cfg->kernLogLevel;
	ulog_cfg.devDefaultLogLevel = cfg->devDefaultLogLevel;
	ulog_cfg.devUserLogLevel = cfg->devCurLogLevel;
	return ulog_kern_set_log_cfg(dev_id, &ulog_cfg);
}

static int ulog_ucd_get_log_cfg(u32 dev_id, void *in, u32 in_len, void *out, u32 out_len)
{
	struct mami_log_cfg *cfg_in = (struct mami_log_cfg *)in;
	struct mami_log_cfg *cfg_out = (struct mami_log_cfg *)out;
	struct ulog_cfg ulog_cfg = {0};
	int ret;

	if ((in == NULL) || (in_len != sizeof(struct mami_log_cfg))) {
		mami_err("ulog", "in is null or len is invalid. (inlen=%u;valid_len=%zu)\n",
			in_len, sizeof(struct mami_log_cfg));
		return -EINVAL;
	}
	if ((out == NULL) || (out_len != sizeof(struct mami_log_cfg))) {
		mami_err("ulog", "out is null or len is invalid. (outlen=%u;valid_len=%zu)\n",
			out_len, sizeof(struct mami_log_cfg));
		return -EINVAL;
	}
	if (dev_id >= DEV_MAX_NUM) {
		mami_err("ulog", "Devid is invalid. (dev_id=%u)\n", dev_id);
		return -EINVAL;
	}
	ulog_cfg.configMask = cfg_in->configMask;
	ret = ulog_kern_get_log_cfg(dev_id, &ulog_cfg);
	if (ret == 0) {
		cfg_out->kernLogLevel = ulog_cfg.kernLogLevel;
		cfg_out->devDefaultLogLevel = ulog_cfg.devDefaultLogLevel;
	}
	return ret;
}

struct ulog_dev_fops {
	u32 main_cmd;
	u32 sub_cmd;
	u32 proc_ctrl_bitmap;
	dev_feature_handler func;
};

#define ULOG_FOPS_ADD_CMD(s, p, f) {	 \
	.main_cmd		 = UCD_CMD_ULOG, \
	.sub_cmd		  = s,			\
	.proc_ctrl_bitmap = p,			\
	.func			 = f			 \
}

static const struct ulog_dev_fops g_ulog_dev_fops[] = {
	[0] = ULOG_FOPS_ADD_CMD(ULOG_SET_LOG_CFG, PROC_CTRL_MANAGED, ulog_ucd_set_log_cfg),
	[1] = ULOG_FOPS_ADD_CMD(ULOG_GET_LOG_CFG, PROC_CTRL_MANAGED, ulog_ucd_get_log_cfg),
};

static int ulog_ucd_register(void)
{
	const u32 num = ARRAY_SIZE(g_ulog_dev_fops);
	u32 i, j;
	int ret;

	for (i = 0; i < num; i++) {
		struct tag_ucd_feature ucd_cmd;
		const struct ulog_dev_fops *fops = &g_ulog_dev_fops[i];

		ucd_fill_feature_cmd(&ucd_cmd, "ulog", fops->main_cmd, fops->sub_cmd, fops->func);
		ucd_set_proc_ctrl_bitmap(&ucd_cmd, fops->proc_ctrl_bitmap);
		ret = ucd_register_dev_cmd(&ucd_cmd);
		if (ret != 0) {
			mami_err("ulog", "Ucd register fail. (ret=%d; i=%u)\n", ret, i);
			goto out;
		}
	}

	return 0;
out:
	for (j = 0; j < i; j++) {
		ucd_unregister_dev_cmd(g_ulog_dev_fops[j].main_cmd, g_ulog_dev_fops[j].sub_cmd);
	}
	return ret;
}

static void ulog_ucd_unregister(void)
{
	const u32 num = ARRAY_SIZE(g_ulog_dev_fops);
	u32 i;

	for (i = 0; i < num; i++) {
		ucd_unregister_dev_cmd(g_ulog_dev_fops[i].main_cmd, g_ulog_dev_fops[i].sub_cmd);
	}
}

int ulog_fops_init(void)
{
	int ret;

	ret = ulog_ucd_register();
	if (ret != 0) {
		mami_err("ulog", "Ucd register fail. (ret=%d)\n", ret);
		return ret;
	}

	return ret;
}
DECLARE_FEATURE_AUTO_INIT(ulog_fops_init, FEATURE_LOADER_STAGE_4);

void ulog_fops_uninit(void)
{
	ulog_ucd_unregister();
}
DECLARE_FEATURE_AUTO_UNINIT(ulog_fops_uninit, FEATURE_LOADER_STAGE_4);

