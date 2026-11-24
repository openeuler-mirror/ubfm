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
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/dmi.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/namei.h>
#include <linux/mm.h>
#include <linux/file.h>
#include "mami_string.h"

#include "ubfm_common.h"
#include "feature_loader.h"
#include "ucd_define.h"
#include "ucd_acc_ctrl.h"
#include "ucd.h"

#define UBE_USER_UID	1201

#define BASE_PLAT_INSTALL_INFO_FILE			  "/etc/baseplat_install.info"
#define DEFAULT_INSTALL_PATH					 "/usr/local/mami"

static struct ucd_proc_ctrl g_ucd_proc_ctrl[] = {
	UCD_PROC_CTRL_DEF(PROC_CTRL_MANAGED, UBE_USER_UID, "mami_dev_ctrl.bin"),
	UCD_PROC_CTRL_DEF(PROC_CTRL_PROCESS_MGR, 0, "bin/base-plat/process-manager/process-manager"),
	UCD_PROC_CTRL_DEF(PROC_CTRL_PM_CHILD, 0, "bin/base-plat/process-manager/pm_child"),
	UCD_PROC_CTRL_DEF(PROC_CTRL_IAM_MGR, 0, "bin/base-plat/iam/iammgr")
};

STATIC int ucd_load_base_plat_install_info(char *buf, u32 buf_size)
{
	ssize_t read_len;
	struct file *fp;

	fp = filp_open(BASE_PLAT_INSTALL_INFO_FILE, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ucd_info("Install baseplat info file not exist.\n");
		return -ENOENT;
	}

	fp->f_pos = 0;
	read_len = kernel_read(fp, buf, buf_size - 1, &fp->f_pos);
	if (read_len < 0) {
		ucd_err("Read install info fail. (ret=%ld)\n", read_len);
		(void)filp_close(fp, NULL);
		return -EINVAL;
	}
	(void)filp_close(fp, NULL);
	buf[read_len] = '\0';

	return 0;
}

#define BASE_PLAT_INSTALL_BUF_SIZE			4096
STATIC int ucd_get_install_path(char *path, u32 size)
{
	const char *tag_context = "InstallPathParam=";
	u32 tag_size;
	char *ppath = path;
	char *ppath_end = path + size;
	char *buf = NULL;
	char *p = NULL;
	char ch;
	int ret;

	buf = kvmalloc(BASE_PLAT_INSTALL_BUF_SIZE, GFP_KERNEL_ACCOUNT);
	if (buf == NULL) {
		return -ENOMEM;
	}

	ret = ucd_load_base_plat_install_info(buf, BASE_PLAT_INSTALL_BUF_SIZE);
	if (ret != 0) {
		goto out;
	}

	p = strstr(buf, tag_context);
	if (p == NULL) {
		ucd_err("Base plat install info not contain tag context.\n");
		ret = -EINVAL;
		goto out;
	}
	tag_size = strlen(tag_context);
	p += tag_size;
	if (p >= buf + BASE_PLAT_INSTALL_BUF_SIZE) {
		ucd_err("Invalid buf.\n");
		ret = -EINVAL;
		goto out;
	}

	while ((ch = *p++) != '\0') {
		if (ppath >= ppath_end) {
			ucd_err("Invalid install path.\n");
			ret = -EINVAL;
			goto out;
		}

		if (ch == '\r' || ch == '\n') {
			break;
		}

		*ppath = ch;
		ppath++;
	}
	*ppath = '\0';

out:
	kvfree(buf);
	return ret;
}

int ucd_proc_ctrl_init(void)
{
	char base_path[UCD_PROC_PATH_MAX] = {0};
	struct path path;
	u32 i;
	int ret;

	ret = ucd_get_install_path(base_path, UCD_PROC_PATH_MAX);
	if (ret == -ENOENT) {
		/* If no baseplat_install.info file indicates the install path, use the default path; standard card auto-deployment belongs to this scenario */
		(void)ubfm_strcpy(base_path, sizeof(base_path), DEFAULT_INSTALL_PATH);
	} else if (ret != 0) {
		ucd_err("Get install path failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = kern_path(base_path, LOOKUP_FOLLOW, &path);
	if (ret != 0) {
		ucd_err("Install path not exist. (path=%s; ret=%d)\n", base_path, ret);
		return ret;
	}

	path_put(&path);
	ucd_info("Base install path: %s.\n", base_path);
	for (i = 0; i < (u32)ARRAY_SIZE(g_ucd_proc_ctrl); i++) {
		struct ucd_proc_ctrl *ctrl = &g_ucd_proc_ctrl[i];

		ret = UBFM_SPRINTF(ctrl->absolute_path, UCD_PROC_PATH_MAX, "%s/%s", base_path, ctrl->relative_path);
		if (ret <= 0) {
			ucd_err("Format absolute path failed. (idx=%u; ret=%d)\n", i, ret);
			return ret;
		}
	}

	return 0;
}

STATIC struct ucd_proc_ctrl *proc_ctrl_path_get(u32 flag)
{
	u32 i;

	for (i = 0; i < (u32)ARRAY_SIZE(g_ucd_proc_ctrl); i++) {
		struct ucd_proc_ctrl *ctrl = &g_ucd_proc_ctrl[i];

		if (flag == ctrl->flag) {
			return ctrl;
		}
	}
	return NULL;
}

STATIC bool proc_ctrl_access(u32 flag)
{
	char *tmp, *pathname;
	struct mm_struct *active_mm = current->active_mm;
	struct ucd_proc_ctrl *ctrl = NULL;
	kuid_t uid;

	ctrl = proc_ctrl_path_get(flag);
	if (ctrl == NULL) {
		return false;
	}

#ifndef EMU_ST
	if (active_mm == NULL || active_mm->exe_file == NULL) {
		return false;
	}

	tmp = (char *)(void *)(uintptr_t)__get_free_page(GFP_KERNEL);
	if (tmp == NULL) {
		return false;
	}

	pathname = d_path(&active_mm->exe_file->f_path, tmp, PAGE_SIZE);
	if (IS_ERR(pathname)) {
		free_page((uintptr_t)tmp);
		return false;
	}

	if (strcmp((const char *)pathname, ctrl->absolute_path) != 0) {
		ucd_debug("Path is not matched. (current_path=%s; required_path=%s)\n", pathname, ctrl->absolute_path);
		free_page((uintptr_t)tmp);
		return false;
	}

	free_page((uintptr_t)tmp);
	uid.val = ctrl->uid;
	if (!uid_eq(current_uid(), uid)) {
		ucd_err("current uid has no permission. (current_uid=%u; required_uid=%u)\n",
				from_kuid(&init_user_ns, current_uid()), from_kuid(&init_user_ns, uid));
		return false;
	}
#endif
	return true;
}

int ucd_check_proc_permission(u32 proc_ctrl_bitmap)
{
	if (proc_ctrl_bitmap == 0) {
		return -EPERM;
	}

	if (proc_ctrl_bitmap & (u32)PROC_CTRL_ANY_USER) {
		return 0;
	}

	if (proc_ctrl_bitmap & (u32)PROC_CTRL_MANAGED) {
		if (proc_ctrl_access(PROC_CTRL_MANAGED)) {
			return 0;
		}
	}

	if (proc_ctrl_bitmap & (u32)PROC_CTRL_PROCESS_MGR) {
		if (proc_ctrl_access(PROC_CTRL_PROCESS_MGR)) {
			return 0;
		}
	}

	if (proc_ctrl_bitmap & (u32)PROC_CTRL_PM_CHILD) {
		if (proc_ctrl_access(PROC_CTRL_PM_CHILD)) {
			return 0;
		}
	}

	if (proc_ctrl_bitmap & (u32)PROC_CTRL_IAM_MGR) {
		if (proc_ctrl_access(PROC_CTRL_IAM_MGR)) {
			return 0;
		}
	}

	return -EPERM;
}

static inline s32 runtime_env_check(u32 user_acc, u32 profile)
{
	/* run env not set, para error */
	if ((user_acc & UCD_RUN_ENV_MASK) == 0) {
		return -EPERM;
	}

	if ((((profile) & (UCD_ENV_PHYSICAL)) == 0) && (((user_acc) & (UCD_ENV_PHYSICAL)) != 0)) {
		return -EPERM;
	}

	if ((((profile) & (UCD_ENV_VIRTUAL)) == 0) && (((user_acc) & (UCD_ENV_VIRTUAL)) != 0)) {
		return -EPERM;
	}
	return 0;
}

static inline s32 user_group_identify(u32 user_acc, u32 profile)
{
	/* user right not set, para error */
	if ((user_acc & UCD_ACC_MASK) == 0) {
		return -EPERM;
	}

	if ((((profile) & (UCD_ACC_USER)) == 0) && (((user_acc) & (UCD_ACC_USER)) != 0)) {
		return -EPERM;
	}

	if ((((profile) & (UCD_ACC_OPERATE)) == 0) && (((user_acc) & (UCD_ACC_OPERATE)) != 0)) {
		return -EPERM;
	}
	if ((((profile) & (UCD_ACC_ROOT)) == 0) && (((user_acc) & (UCD_ACC_ROOT)) != 0)) {
		return -EPERM;
	}
	return 0;
}

static inline bool ucd_check_in_virtual_machine(void)
{
	if (dmi_match(DMI_SYS_VENDOR, DMI_VIRTUAL_SYSVENDOR))  {
		return true;
	}
	return false;
}

static inline u32 ucd_get_run_env(void)
{
	u32 env;

	if (ucd_check_in_virtual_machine()) {
		env = UCD_ENV_VIRTUAL;
	} else {
		env = UCD_ENV_PHYSICAL;
	}
	return env;
}

static inline u32 ucd_get_user_role(void)
{
	const struct cred *cred = current_cred();
	u32 acc;

	/* check user role */
	if ((cred != NULL) && (cred->euid.val == 0)) {
		acc = UCD_ACC_ROOT;
	} else {
		acc = UCD_ACC_USER;
	}
	return acc;
}

static inline u32 ucd_feature_access_get_acc(void)
{
	u32 acc;

	acc = ucd_get_user_role();
	acc |= ucd_get_run_env();
	return acc;
}

int ucd_feature_access_identify(u32 profile)
{
	int ret;
	u32 user_acc;
	/* no need check */
	if (profile == 0) {
		return 0;
	}
	user_acc = ucd_feature_access_get_acc();
	ret = runtime_env_check(user_acc, profile);
	if (ret != 0) {
		return ret;
	}

	ret = user_group_identify(user_acc, profile);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

/* parse proc string, "devm,ubfabric,..." to {"devm","ubfabric",".","."} */
static int ucd_parse_string(char **str, const char *delim, char **out,
	u32 *out_len)
{
	const char *sep = NULL;
	char *s = NULL;
	char *tok = NULL;
	char sc, c;
	u32 max_len, count;

	max_len = *out_len;
	*out_len = 0;
	count = 0;
	s = *str;
	for (tok = s; ((tok != NULL) && (count < max_len));) {
		c = *s++;
		sep = delim;
		do {
			sc = *sep++;
			if (sc != c) {
				continue;
			}

			if (c == '\0') {
				s = NULL;
			} else {
				s[-1] = 0;
			}

			*out++ = tok;
			count++;
			tok = s;
			break;
		} while (sc != 0);
	}
	*out_len = count;
	return 0;
}

static int ucd_feature_alloc_and_save_proc_name(char **process_name, int process_num,  char ***proc_ctl)
{
	int i;
	char **tmp_proc = NULL;

	tmp_proc = kcalloc(process_num, sizeof(char *), GFP_KERNEL | __GFP_ACCOUNT);
	if (tmp_proc == NULL) {
		ucd_err("kcalloc failed. (len=%lu)\n", process_num * sizeof(char *));
		return -ENOMEM;
	}
	*proc_ctl = tmp_proc;
	for (i = 0; i < process_num; i++) {
		tmp_proc[i] = process_name[i];
	}

	return 0;
}

int ucd_feature_parse_proc_ctrl(const char *proc_str, char **buf, char ***proc_ctl, u32 *proc_num)
{
	char *process_name[PROC_NAME_NUM_MAX + 1] = {NULL};
	int ret;
	unsigned int process_num;
	size_t len;

	/* if proc_ctrl was NULL,means not use whitelist control */
	if (proc_str == NULL) {
		return 0;
	}
	if ((buf == NULL) || (proc_ctl == NULL) || (proc_num == NULL)) {
		return -EINVAL;
	}
	len = strnlen(proc_str, UCD_ACCESS_PROC_STRING_MAX);
	if (len >= UCD_ACCESS_PROC_STRING_MAX) {
		ucd_err("Parse proc failed. (len=%lu)\n", len);
		return -EINVAL;
	}
	*buf = (char *)kzalloc(len + 1, GFP_KERNEL | __GFP_ACCOUNT);
	if (*buf == NULL) {
		return -ENOMEM;
	}
	ret = ubfm_strcpy(*buf, len + 1, proc_str);
	if (ret != 0) {
		ucd_err("ubfm_strcpy failed. (len=%lu)\n", len);
		goto ERR_OUT;
	}
	process_num = PROC_NAME_NUM_MAX;
	ret = ucd_parse_string(buf, ",", process_name, &process_num);
	if ((ret != 0) || (process_num == 0)) {
		ucd_err("ucd_parse_string failed. (ret=%d)\n", ret);
		ret = -ENOEXEC;
		goto ERR_OUT;
	}

	/* malloc proc_ctl, save the  process_name */
	ret = ucd_feature_alloc_and_save_proc_name(process_name, process_num, proc_ctl);
	if (ret != 0) {
		ucd_err("ucd_feature_alloc_and_save_proc_name failed. (len=%lu)\n", process_num * sizeof(char *));
		goto ERR_OUT;
	}

	*proc_num = process_num;
	return 0;

ERR_OUT:
	kfree(*buf);
	*buf = NULL;
	return ret;
}

s32 ucd_feature_whitelist_check(const char **proc_ctrl, u32 proc_num)
{
	return 0;
}
