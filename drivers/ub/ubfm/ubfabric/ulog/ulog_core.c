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
#include <linux/mutex.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/jhash.h>
#include <linux/string.h>

#include "mami_string.h"
#include "feature_loader.h"
#include "ubfm_common.h"
#include "udm.h"
#include "ulog.h"
#include "ulog_core.h"

#define LOG_PRINT_MAX_SIZE 256
#define LOG_CACHE_SIZE 128
struct ulog_item {
	u32 log_hash;
	u32 timestamp;
};
struct ulog_cache {
	struct ulog_item items[LOG_CACHE_SIZE];
};
static struct ulog_cache g_ulog_cache;
static DEFINE_SPINLOCK(cache_lock);

#define ULOG_MODULE_NAME "ulog"
struct ulog_dev {
	u32 devUserLogLevel;
	u32 devDefaultLogLevel;
};

static struct mutex g_ulog_lock;
static struct ulog_dev g_dev_ulog[DEV_MAX_NUM];
static u32 g_kernLogLevel = MAMI_KERN_LOG_INFO;

static const char *g_log_level_str[MAMI_KERN_LOG_MAX] = {
	"MAMI_KERN_LOG_EMERG",
	"MAMI_KERN_LOG_ALERT",
	"MAMI_KERN_LOG_CRIT",
	"MAMI_KERN_LOG_ERR",
	"MAMI_KERN_LOG_WARNING",
	"MAMI_KERN_LOG_NOTICE",
	"MAMI_KERN_LOG_INFO",
	"MAMI_KERN_LOG_DEBUG",
};

static const char *get_log_str(u32 level)
{
	return level < MAMI_KERN_LOG_MAX ? g_log_level_str[level] : "UNKNOWN";
}

bool mami_kern_log_drop(unsigned int level)
{
	return level > g_kernLogLevel;
}
EXPORT_SYMBOL_GPL(mami_kern_log_drop);

static inline struct ulog_cache *get_log_cache(void)
{
	return &g_ulog_cache;
}

static inline bool ulog_check_and_update(u32 hash)
{
	struct ulog_cache *cache = get_log_cache();
	struct ulog_item *item = NULL;
	u64 oldest = ULLONG_MAX;
	int i, lru_index = 0;
	unsigned long flags;
	int empty = INT_MAX;

	if (!spin_trylock_irqsave(&cache_lock, flags)) {
		return false;
	}

	for (i = 0; i < LOG_CACHE_SIZE; i++) {
		item = &cache->items[i];
		if (item->timestamp == 0) {
			empty = (empty == INT_MAX) ? i : empty;
			continue;
		}
		if (item->log_hash == hash) {
			item->timestamp = jiffies_64;
			spin_unlock_irqrestore(&cache_lock, flags);
			return true;
		}
		if (item->timestamp < oldest) {
			oldest = item->timestamp;
			lru_index = i;
		}
	}
	item = (empty != INT_MAX) ? &cache->items[empty] : &cache->items[lru_index];
	item->log_hash = hash;
	item->timestamp = jiffies_64;

	spin_unlock_irqrestore(&cache_lock, flags);
	return false;
}

bool ulog_is_duplicate(const char *fmt, ...)
{
	char log_buf[LOG_PRINT_MAX_SIZE + 1] = {0};
	va_list args;
	u32 len;
	u32 hash;

	va_start(args, fmt);
	len = (u32)vsnprintf(log_buf, sizeof(log_buf), fmt, args);
	va_end(args);

	len = len > LOG_PRINT_MAX_SIZE ? LOG_PRINT_MAX_SIZE : len;
	hash = jhash(log_buf, len, 0);

	return ulog_check_and_update(hash);
}
EXPORT_SYMBOL_GPL(ulog_is_duplicate);

static int ulog_check_log_level(struct ulog_cfg *cfg)
{
	if ((cfg->configMask & DEV_DEFAULT_LOG_BIT) != 0) {
		if (!ulog_level_is_valid(cfg->devDefaultLogLevel)) {
			mami_err(ULOG_MODULE_NAME, "Default log level is invalid. (level=%u;max_level=%d)\n",
				cfg->devDefaultLogLevel, MAMI_KERN_LOG_MAX);
			return -EINVAL;
		}
	}
	if ((cfg->configMask & KERN_LOG_BIT) != 0) {
		if (!ulog_level_is_valid(cfg->kernLogLevel)) {
			mami_err(ULOG_MODULE_NAME, "Kernel log level is invalid. (level=%u;max_level=%d)\n",
				cfg->kernLogLevel, MAMI_KERN_LOG_MAX);
			return -EINVAL;
		}
	}
	return 0;
}

int ulog_kern_set_log_cfg(u32 devid, struct ulog_cfg *cfg)
{
	if (devid >= DEV_MAX_NUM) {
		mami_err(ULOG_MODULE_NAME, "Devid is invalid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (ulog_check_log_level(cfg) != 0) {
		return -EINVAL;
	}

	mutex_lock(&g_ulog_lock);
	if ((cfg->configMask & DEV_DEFAULT_LOG_BIT) != 0) {
		g_dev_ulog[devid].devDefaultLogLevel = cfg->devDefaultLogLevel;
	}
	if ((cfg->configMask & CUR_LOG_BIT) != 0) {
		g_dev_ulog[devid].devUserLogLevel = cfg->devUserLogLevel;
	}
	if ((cfg->configMask & KERN_LOG_BIT) != 0) {
		g_kernLogLevel = cfg->kernLogLevel;
	}
	mutex_unlock(&g_ulog_lock);

	return 0;
}

int ulog_kern_get_log_cfg(u32 devid, struct ulog_cfg *cfg)
{
	if (devid >= DEV_MAX_NUM) {
		mami_err(ULOG_MODULE_NAME, "Devid is invalid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	mutex_lock(&g_ulog_lock);
	if ((cfg->configMask & DEV_DEFAULT_LOG_BIT) != 0) {
		cfg->devDefaultLogLevel = g_dev_ulog[devid].devDefaultLogLevel;
	}
	if ((cfg->configMask & KERN_LOG_BIT) != 0) {
		cfg->kernLogLevel = g_kernLogLevel;
	}
	mutex_unlock(&g_ulog_lock);

	return 0;
}

int ulog_kern_dump_snapshot(char **buf, uint32_t *buf_len, uint32_t *read_size, const char *format, ...)
{
	va_list args;
	int written;

	if (*buf_len <= 0) {
		return -ENOSPC;
	}

	va_start(args, format);
	written = vsnprintf(*buf, *buf_len, format, args);
	va_end(args);

	if (written <= 0) {
		return -ENOSPC;
	}

	if (*buf_len < written) {
		return -ENOSPC;
	}

	*buf_len -= written;
	*buf += (u32)written;
	*read_size += (u32)written;

	return 0;
}
EXPORT_SYMBOL_GPL(ulog_kern_dump_snapshot);

void ulog_level_info_show(struct seq_file *seq)
{
	u32 level;
	u32 i;

	seq_printf(seq, "global kern log level: %s\n", get_log_str(g_kernLogLevel));
	for (i = 0; i < DEV_MAX_NUM; ++i) {
		if (!udm_is_udevid_exist(i)) {
			continue;
		}

		level = g_dev_ulog[i].devDefaultLogLevel;
		seq_printf(seq, "dev[%4u] default log level: %s, current log level: %s\n", i,
			get_log_str(g_dev_ulog[i].devDefaultLogLevel), get_log_str(g_dev_ulog[i].devUserLogLevel));
	}
}

int ulog_mod_init(void)
{
	int i;

	mutex_init(&g_ulog_lock);
	g_kernLogLevel = MAMI_KERN_LOG_INFO;  /* default kern log level */
	for (i = 0; i < DEV_MAX_NUM; i++) {
		g_dev_ulog[i].devDefaultLogLevel = MAMI_KERN_LOG_INFO;
		g_dev_ulog[i].devUserLogLevel = MAMI_KERN_LOG_INFO;
	}
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ulog_mod_init, FEATURE_LOADER_STAGE_3);

void ulog_mod_uninit(void)
{
	mutex_destroy(&g_ulog_lock);
}
DECLARE_FEATURE_AUTO_UNINIT(ulog_mod_uninit, FEATURE_LOADER_STAGE_3);
