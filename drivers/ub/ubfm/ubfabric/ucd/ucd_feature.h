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
#ifndef UCD_FEATURE_H
#define UCD_FEATURE_H

#include "ucd.h"
#include <linux/mutex.h>

#include "ucd_proc_fs.h"

struct tag_ucd_feature_arg {
	u32 devid;
	u32 key;
	void *input;
	u32 input_len;
	void *output;
	u32 output_len;
};

struct tag_feature_statistic {
	u64 used;
	u64 failed;
	u64 time_max;
	u64 time_min;
	u64 time_last;
	int last_ret;
	struct mutex statistic_lock;
};

struct tag_ucd_feature_node {
	struct tag_ucd_feature feature;
	char **proc_ctrl;
	char *proc_buf;
	atomic_t count;
	u32 proc_num;
	u32 state;
	struct tag_feature_statistic s;
};

/* max time 60s */
#define FEATURE_WAIT_MAX_TIME	(6000)

#define FEATURE_WAIT_EACH_TIME	(10)

#define FEATURE_CONFIRM_WARN_MASK (1000)

#define MAINCMD_LEFT  8

void ucd_feature_register_show(struct seq_file *seq);
void ucd_feature_statistic_show(struct seq_file *seq);
int ucd_feature_init(void);
void ucd_feature_uninit(void);
int ucd_feature_process(struct tag_ucd_feature_arg *arg);
int ucd_dev_feature_process(struct tag_ucd_feature_arg *arg);
int feature_inc_work(struct tag_ucd_feature_node *node);

#endif
