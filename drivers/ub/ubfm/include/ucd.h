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
#ifndef UCD_H__
#define UCD_H__
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm_types.h>
#include "ubfabric.h"

typedef int (*feature_handler)(void *in, u32 in_len, void *out, u32 out_len);
typedef int (*dev_feature_handler)(u32 devid, void *in, u32 in_len, void *out, u32 out_len);

struct tag_ucd_feature {
	const char *module;
	u8 main_cmd;
	u8 sub_cmd;
	u8 rsv[2];
	u32 proc_ctrl_bitmap;
	union {
		feature_handler fn;
		dev_feature_handler dev_fn;
		void *call_fn;
	};
};

enum proc_ctrl_flags {
	PROC_CTRL_NONE	  = 0,	  /* Not supported for user-mode invocation */
	PROC_CTRL_ANY_USER  = 1 << 0, /* Supports any user-mode process (requires character device permission) */
	PROC_CTRL_MANAGED   = 1 << 1, /* Only supports management process */
	PROC_CTRL_PROCESS_MGR   = 1 << 2,   /* process-manager process */
	PROC_CTRL_PM_CHILD   = 1 << 3,	  /* pm-child process */
	PROC_CTRL_IAM_MGR = 1 << 4,		 /* iammgr process */
};

static inline void ucd_fill_feature_cmd(struct tag_ucd_feature *feature, const char *module, u8 main_cmd, u8 sub_cmd, void *fn)
{
	feature->module = module;
	feature->main_cmd = main_cmd;
	feature->sub_cmd = sub_cmd;
	feature->call_fn = fn;
}

static inline void ucd_set_proc_ctrl_bitmap(struct tag_ucd_feature *feature, u32 proc_ctrl_bitmap)
{
	feature->proc_ctrl_bitmap = proc_ctrl_bitmap;
}

int ucd_register_cmd(struct tag_ucd_feature *feature);
int ucd_register_dev_cmd(struct tag_ucd_feature *feature);

int ucd_unregister_cmd(u8 main_cmd, u8 sub_cmd);
int ucd_unregister_dev_cmd(u8 main_cmd, u8 sub_cmd);

typedef int (*ucd_open)(pid_t tgid);
typedef int (*ucd_release)(pid_t tgid);
typedef int (*ucd_dev_open)(u32 devid, pid_t tgid);
typedef int (*ucd_dev_release)(u32 devid, pid_t tgid);
typedef int (*ucd_dev_mmap)(u32 devid, struct vm_area_struct *vma);

int ucd_register_module_ops(const char *module, ucd_open open, ucd_release release);
int ucd_register_module_dev_ops(const char *module, ucd_dev_open open, ucd_dev_release release, ucd_dev_mmap mmap);
int ucd_unregister_module_ops(const char *module);
int ucd_unregister_module_dev_ops(const char *module);
/* get ops /dev/ubfabricx or  */
const struct file_operations *ucd_get_module_ops(void);
const struct file_operations *ucd_get_module_dev_ops(void);


#endif

