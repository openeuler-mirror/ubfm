/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef UCD_ACC_CTRL_H
#define UCD_ACC_CTRL_H

#include "ubfm_common.h"

#define UCD_ACC_MASK 0X000FU
/* allow running as root */
#define UCD_ACC_ROOT 0X0001U
/* allow running as HwHiAiUser */
#define UCD_ACC_OPERATE 0X0002U
/* allow running as guest */
#define UCD_ACC_USER 0X0004U


#define UCD_ACC_ALL (UCD_ACC_ROOT | UCD_ACC_OPERATE | UCD_ACC_USER)
#define UCD_ACC_NOT_USER (UCD_ACC_ROOT | UCD_ACC_OPERATE |)
#define UCD_ACC_MANAGE_USER (UCD_ACC_ROOT)
#define UCD_ACC_ROOT_ONLY (UCD_ACC_ROOT)


#define UCD_RUN_ENV_MASK 0X00F0U
/* Runtime environment */
/* Support physical */
#define UCD_ENV_PHYSICAL 0X0010U
/* Support virtual */
#define UCD_ENV_VIRTUAL 0X0020U


#define UCD_ENV_ALL (UCD_ENV_PHYSICAL | UCD_ENV_VIRTUAL)
#define UCD_PHYSICAL_ONLY (UCD_ENV_PHYSICAL)
#define UCD_VIRTUAL_ONLY (UCD_ENV_VIRTUAL)

#define UCD_VDEV_MASK 0X0700U

#define UCD_SUPPORT_ALL (UCD_ACC_ALL | UCD_ENV_ALL)

#define UCD_SUPPORT_ROOT_ONLY (UCD_ACC_ROOT_ONLY | UCD_ENV_ALL)

#define UCD_SUPPORT_ROOT_PHY (UCD_ACC_ROOT_ONLY | UCD_PHYSICAL_ONLY)

#define PROC_NAME_NUM_MAX				 50
#define UCD_ACCESS_PROC_STRING_MAX 4096U

#define UCD_PROC_PATH_MAX		   256

struct ucd_proc_ctrl {
	u32 flag;
	u32 uid;
	const char *relative_path;
	char absolute_path[UCD_PROC_PATH_MAX];
};

#define UCD_PROC_CTRL_DEF(flag_, uid_, relative_path_) { \
	.flag = (flag_), \
	.uid = (uid_), \
	.relative_path = (relative_path_),\
	.absolute_path = {0} }

int ucd_proc_ctrl_init(void);
int ucd_feature_access_identify(u32 feature_prof);

int ucd_feature_whitelist_check(const char **proc_ctrl, u32 proc_num);
int ucd_feature_parse_proc_ctrl(const char *proc_str, char **buf,
	char ***proc_ctl, u32 *proc_num);

#define DMI_VIRTUAL_SYSVENDOR "QEMU"
#define DMI_VIRTUAL_PRODUCT_NAME "KVM Virtual Machine"

/**
 * @brief Check whether the current process has permission to invoke the specified command
 *
 * @param proc_ctrl_bitmap  Process control bitmap configured during command registration
 * @return 0 indicates invocation is allowed; negative value indicates not allowed
 */
int ucd_check_proc_permission(unsigned int proc_ctrl_bitmap);


#endif
