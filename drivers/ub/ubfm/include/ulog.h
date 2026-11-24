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
#ifndef ULOG_H__
#define ULOG_H__

#include "ubfm_common.h"
#include "ubfabric.h"

#ifndef EMU_ST
#include <linux/sched.h>	/* current */
#include <linux/printk.h>   /* printk */

bool mami_kern_log_drop(u32 level);
bool ulog_is_duplicate(const char *fmt, ...);

/* lv: level; m: module name. */
#define mami_printk(lv, lv_str, m, fmt, ...) do {														\
	if (!mami_kern_log_drop(MAMI_KERN_LOG_##lv)) {															\
		(void)printk(KERN_##lv "<%s:%d:%d> [mami %s] ["#lv_str"] [%s %d] " fmt, current->comm, current->tgid, \
			current->pid, m, __func__, __LINE__, ##__VA_ARGS__);											\
	}																										\
} while (0)

#define mami_printk_limit(lv, lv_str, m, fmt, ...) do {													\
	if (!mami_kern_log_drop(MAMI_KERN_LOG_##lv)) {															\
		if (!ulog_is_duplicate(fmt, ##__VA_ARGS__)) {														\
			printk(KERN_##lv "<%s:%d:%d> [mami %s] ["#lv_str"] [%s %d] " fmt, current->comm, current->tgid, \
				current->pid, m, __func__, __LINE__, ##__VA_ARGS__);										\
		}																									\
	}																										\
} while (0)

#define mami_err_limit(m, fmt, ...)   mami_printk_limit(ERR, ERROR, m, fmt, ##__VA_ARGS__)
#define mami_info_limit(m, fmt, ...)   mami_printk_limit(INFO, INFO, m, fmt, ##__VA_ARGS__)

#define mami_err(m, fmt, ...)		 mami_printk_limit(ERR, ERROR, m, fmt, ##__VA_ARGS__)
#define mami_warn(m, fmt, ...)		mami_printk(WARNING, WARN, m, fmt, ##__VA_ARGS__)
#define mami_info(m, fmt, ...)		mami_printk(INFO, INFO, m, fmt, ##__VA_ARGS__)
#define mami_debug(m, fmt, ...)	   mami_printk(DEBUG, DEBUG, m, fmt, ##__VA_ARGS__)
#define mami_event(m, fmt, ...)	   mami_printk(NOTICE, EVENT, m, fmt, ##__VA_ARGS__)

#else
#include "ut_log.h"
#endif  /* EMU_ST */

struct ulog_cfg {
	unsigned int configMask;
	unsigned int devDefaultLogLevel;
	unsigned int kernLogLevel;
	unsigned int devUserLogLevel;
};

int ulog_kern_set_log_cfg(unsigned int devid, struct ulog_cfg *cfg);
int ulog_kern_get_log_cfg(unsigned int devid, struct ulog_cfg *cfg);
int ulog_kern_dump_snapshot(char **buf, uint32_t *buf_len, uint32_t *read_size, const char *format, ...);
static inline bool ulog_level_is_valid(unsigned int level)
{
	return level < MAMI_KERN_LOG_MAX;
}

#endif  /* ULOG_H__ */
