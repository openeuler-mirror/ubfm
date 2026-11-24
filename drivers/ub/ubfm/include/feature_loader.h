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
#ifndef FEATURE_LOADER_H__
#define FEATURE_LOADER_H__

#include <linux/module.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>

#include "mami_string.h"

/*
 * usage : support feature global/device/task scope auto init and uninit
 *		feature can define init or uninit function use this macros:
 *		global: DECLARE_FEATURE_AUTO_INIT / DECLARE_FEATURE_AUTO_UNINIT
 *		device: DECLARE_FEATURE_AUTO_INIT_DEV / DECLARE_FEATURE_AUTO_UNINIT_DEV
 *		task: DECLARE_FEATURE_AUTO_INIT_TASK / DECLARE_FEATURE_AUTO_UNINIT_TASK
 *		global scope init/uninit call module_feature_auto_init/module_feature_auto_uninit in module_init/module_exit
 *		device scope init/uninit call module_feature_auto_init_dev/module_feature_auto_uninit_dev
 *		when device online or offline
 *		task scope init/uninit call module_feature_auto_init_task/module_feature_auto_uninit_task
 *		when proc add or remove
 *		When there are multiple auto-initialization features in the module and you want to control the sequence,
 *		you can use stage param.
 *		stage : scope 0-9, stage 0 has the highest priority.
 *		init: functions are invoked in the sequence of stages.
 *		uninit: functions are invoked in the oppside sequence of stages.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
struct kernel_symbol {
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	int value_offset;
	int name_offset;
	int namespace_offset;
#else
	unsigned long value;
	const char *name;
	const char *namespace;
#endif
};
#endif

#define PREFIX_MAX_LEN 64

enum {
	FEATURE_SCOPE_GLOBAL = 0,
	FEATURE_SCOPE_DEV,
	FEATURE_SCOPE_TASK,
	FEATURE_SCOPE_SELFDEF,
	FEATURE_SCOPE_MAX
};

enum {
	FEATURE_LOADER_STAGE_0 = 0,
	FEATURE_LOADER_STAGE_1,
	FEATURE_LOADER_STAGE_2,
	FEATURE_LOADER_STAGE_3,
	FEATURE_LOADER_STAGE_4,
	FEATURE_LOADER_STAGE_5,
	FEATURE_LOADER_STAGE_6,
	FEATURE_LOADER_STAGE_7,
	FEATURE_LOADER_STAGE_8,
	FEATURE_LOADER_STAGE_9,
	FEATURE_LOADER_STAGE_MAX,
};

struct feature_task_para {
	int pid;
};

struct feature_dev_para {
	u32 udevid;
};

struct feature_selfdef_para {
	const char *scope;
	void *priv;
};

typedef int (*_fearture_init_func)(void);
typedef void (*_fearture_uninit_func)(void);

typedef int (*_fearture_init_dev_func)(u32 udevid);
typedef void (*_fearture_uninit_dev_func)(u32 udevid);

typedef int (*_fearture_init_task_func)(int pid);
typedef void (*_fearture_uninit_task_func)(int pid);

typedef int (*_fearture_init_selfdef_scope_func)(void *priv);
typedef void (*_fearture_uninit_selfdef_scope_func)(void *priv);

/* the function declared will be auto call in module insert
 * init_fn : should like "int _fearture_init_func(void)"
 */
#define DECLARE_FEATURE_AUTO_INIT(init_fn, stage) \
	int stage##_init_##init_fn(void) \
	{ \
		return init_fn(); \
	} \
	EXPORT_SYMBOL_GPL(stage##_init_##init_fn)

/* the function declared will be auto call in module remove
 * init_fn : should like "void _fearture_uninit_func(void)"
 */
#define DECLARE_FEATURE_AUTO_UNINIT(uninit_fn, stage) \
	void stage##_uninit_##uninit_fn(void) \
	{ \
		uninit_fn(); \
	} \
	EXPORT_SYMBOL_GPL(stage##_uninit_##uninit_fn)

/* the function declared will be auto call when dev online
 * init_fn : should like "int _fearture_init_dev_func(u32 udevid)"
 */
#define DECLARE_FEATURE_AUTO_INIT_DEV_NAME(init_fn, stage, ko_name) \
	int stage##_dev_init_##init_fn##_##ko_name(u32 udevid) \
	{ \
		return init_fn(udevid); \
	} \
	EXPORT_SYMBOL_GPL(stage##_dev_init_##init_fn##_##ko_name)
#define DECLARE_FEATURE_AUTO_INIT_DEV(init_fn, stage) \
	DECLARE_FEATURE_AUTO_INIT_DEV_NAME(init_fn, stage, KO_NAME)

/* the function declared will be auto call when dev offline
 * init_fn : should like "void _fearture_uninit_dev_func(u32 udevid)"
 */
#define DECLARE_FEATURE_AUTO_UNINIT_DEV_NAME(uninit_fn, stage, ko_name) \
	void stage##_dev_uninit_##uninit_fn##_##ko_name(u32 udevid) \
	{ \
		uninit_fn(udevid); \
	} \
	EXPORT_SYMBOL_GPL(stage##_dev_uninit_##uninit_fn##_##ko_name)
#define DECLARE_FEATURE_AUTO_UNINIT_DEV(uninit_fn, stage) \
	DECLARE_FEATURE_AUTO_UNINIT_DEV_NAME(uninit_fn, stage, KO_NAME)

/* the function declared will be auto call when task init
 * init_fn : should like "int _fearture_init_task_func(int pid)"
 */
#define DECLARE_FEATURE_AUTO_INIT_TASK(init_fn, stage) \
	int stage##_task_init_##init_fn(int pid) \
	{ \
		return init_fn(pid); \
	} \
	EXPORT_SYMBOL_GPL(stage##_task_init_##init_fn)

/* the function declared will be auto call when task uninit
 * init_fn : should like "void _fearture_uninit_task_func(int pid)"
 */
#define DECLARE_FEATURE_AUTO_UNINIT_TASK(uninit_fn, stage) \
	void stage##_task_uninit_##uninit_fn(int pid) \
	{ \
		uninit_fn(pid); \
	} \
	EXPORT_SYMBOL_GPL(stage##_task_uninit_##uninit_fn)

/* the function declared will be auto call when module_feature_auto_init_selfdef called
 * init_fn : should like "int _fearture_init_selfdef_func(void *priv)"
 */
#define DECLARE_FEATURE_AUTO_INIT_BY_SCOPE(scope, init_fn, stage) \
	int stage##_##scope##_init_##init_fn(void *priv) \
	{ \
		return init_fn(priv); \
	} \
	EXPORT_SYMBOL_GPL(stage##_##scope##_init_##init_fn)

/* the function declared will be auto call when module_feature_auto_uninit_selfdef called
 * init_fn : should like "void _fearture_uninit_selfdef_func(void *priv)"
 */
#define DECLARE_FEATURE_AUTO_UNINIT_BY_SCOPE(scope, uninit_fn, stage) \
	void stage##_##scope##_uninit_##uninit_fn(void *priv) \
	{ \
		uninit_fn(priv); \
	} \
	EXPORT_SYMBOL_GPL(stage##_##scope##_uninit_##uninit_fn)

static inline void *get_symbol_fun(const struct kernel_symbol *sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	return (void *)(uintptr_t)offset_to_ptr(&sym->value_offset);
#else
	return (void *)(uintptr_t)sym->value;
#endif
}

static inline const char *get_symbol_name(const struct kernel_symbol *sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	return offset_to_ptr(&sym->name_offset);
#else
	return sym->name;
#endif
}

static inline void *get_symbol_prefix_func(const struct kernel_symbol *sym, char *prefix)
{
	const char *sym_name = get_symbol_name(sym);

	if (strstr(sym_name, prefix) != sym_name) {
		return NULL;
	}

	return get_symbol_fun(sym);
}

static inline int module_feature_init_call(int feature_scope, void *fn, void *param)
{
	if (feature_scope == FEATURE_SCOPE_GLOBAL) {
		return ((_fearture_init_func)fn)();
	} else if (feature_scope == FEATURE_SCOPE_DEV) {
		struct feature_dev_para *dev_para = (struct feature_dev_para *)param;

		return ((_fearture_init_dev_func)fn)(dev_para->udevid);
	} else if (feature_scope == FEATURE_SCOPE_TASK) {
		struct feature_task_para *task_para = (struct feature_task_para *)param;

		return ((_fearture_init_task_func)fn)(task_para->pid);
	} else if (feature_scope == FEATURE_SCOPE_SELFDEF) {
		return ((_fearture_init_selfdef_scope_func)fn)(((struct feature_selfdef_para *)param)->priv);
	}

	return 0;
}

static inline int module_feature_try_to_init(int feature_scope, void *param,
	const struct kernel_symbol *sym, char *prefix)
{
	void *fn = get_symbol_prefix_func(sym, prefix);

	return (fn != NULL) ? module_feature_init_call(feature_scope, fn, param) : 0;
}

static inline void module_feature_uninit_call(int feature_scope, void *fn, void *param)
{
	if (feature_scope == FEATURE_SCOPE_GLOBAL) {
		((_fearture_uninit_func)fn)();
	} else if (feature_scope == FEATURE_SCOPE_DEV) {
		struct feature_dev_para *dev_para = (struct feature_dev_para *)param;
		((_fearture_uninit_dev_func)fn)(dev_para->udevid);
	} else if (feature_scope == FEATURE_SCOPE_TASK) {
		struct feature_task_para *task_para = (struct feature_task_para *)param;
		((_fearture_uninit_task_func)fn)(task_para->pid);
	} else if (feature_scope == FEATURE_SCOPE_SELFDEF) {
		((_fearture_uninit_selfdef_scope_func)fn)(((struct feature_selfdef_para *)param)->priv);
	}
}

static inline void module_feature_try_to_uninit(int feature_scope, void *param,
	const struct kernel_symbol *sym, char *prefix)
{
	void *fn = get_symbol_prefix_func(sym, prefix);

	if (fn != NULL) {
		module_feature_uninit_call(feature_scope, fn, param);
	}
}

static inline void module_feature_uninit_prefix_range(int feature_scope, void *param,
	struct module *module, char *prefix, u32 num_gpl_syms)
{
	u32 i;

	for (i = 0; i < num_gpl_syms; i++) {
		module_feature_try_to_uninit(feature_scope, param, &module->gpl_syms[i], prefix);
	}
}

static inline void _module_feature_uninit_by_full_name(int feature_scope, void *param,
	struct module *module, char *full_name)
{
	u32 i;

	for (i = 0; i < module->num_gpl_syms; i++) {
		const char *sym_name = get_symbol_name(&module->gpl_syms[i]);

		if (strcmp(sym_name, full_name) == 0) {
			void *fn = get_symbol_fun(&module->gpl_syms[i]);

			module_feature_uninit_call(feature_scope, fn, param);
			break;
		}
	}
}

static inline void module_feature_rollback_format_uninit_func_name(const char *init_fn_name,
	const char *init_label, char *uninit_func_name, int len)
{
	char *init_label_start = strstr(init_fn_name, init_label);

	if (ubfm_strncpy(uninit_func_name, len, init_fn_name, init_label_start - init_fn_name) == 0) {
		if (ubfm_strcat(uninit_func_name, len, "un") == 0) {
			if (ubfm_strcat(uninit_func_name, len, init_label_start) == 0) {
				return;
			}
		}
	}
	uninit_func_name[0] = '\0';
}

static inline void module_feature_rollback_format_uninit_full_func_name(const struct kernel_symbol *sym,
	char *init_prefix, char *uninit_prefix, char *uninit_full_func_name, int in_len)
{
	const char *init_fn_name = get_symbol_name(sym) + strlen(init_prefix);
	const char *init_label = "init";
	int len = in_len;

	uninit_full_func_name[0] = '\0';
	if (strlen(init_fn_name) > strlen(init_label)) {
		/* rollback regular: xxx_init, xxx_uninit, xxx_init_dev, xxx_uninit_dev, xxx_init_task, xxx_uninit_task */
		if (strstr(init_fn_name, init_label) != NULL) {
			if (ubfm_strcpy(uninit_full_func_name, len, uninit_prefix) == 0) {
				module_feature_rollback_format_uninit_func_name(init_fn_name, init_label,
					uninit_full_func_name + strlen(uninit_prefix), len -= strlen(uninit_prefix));
			}
		}
	}
}

static inline void module_feature_init_prefix_rollback_one_fn(int feature_scope, void *param,
	struct module *module, const struct kernel_symbol *sym, char *init_prefix, char *uninit_prefix)
{
	char uninit_full_func_name[PREFIX_MAX_LEN];

	module_feature_rollback_format_uninit_full_func_name(sym, init_prefix, uninit_prefix,
		uninit_full_func_name, PREFIX_MAX_LEN);
	if (strlen(uninit_full_func_name) > 0) {
		_module_feature_uninit_by_full_name(feature_scope, param, module, uninit_full_func_name);
	}
}

static inline void module_feature_init_prefix_rollback(int feature_scope, void *param,
	char *init_prefix, char *uninit_prefix, u32 rollback_num)
{
	struct module *module = THIS_MODULE;
	u32 i;

	for (i = 0; i < rollback_num; i++) {
		void *fn = get_symbol_prefix_func(&module->gpl_syms[i], init_prefix);

		if (fn != NULL) {
			module_feature_init_prefix_rollback_one_fn(feature_scope, param, module,
				&module->gpl_syms[i], init_prefix, uninit_prefix);
		}
	}
}

static inline int module_feature_init_prefix(int feature_scope, void *param, char *init_prefix, char *uninit_prefix)
{
	struct module *module = THIS_MODULE;
	u32 i;
	int ret;

	for (i = 0; i < module->num_gpl_syms; i++) {
		ret = module_feature_try_to_init(feature_scope, param, &module->gpl_syms[i], init_prefix);
		if (ret != 0) {
			module_feature_init_prefix_rollback(feature_scope, param, init_prefix, uninit_prefix, i);
			return ret;
		}
	}

	return 0;
}

static inline void module_feature_uninit_prefix(int feature_scope, void *param, char *prefix)
{
	struct module *module = THIS_MODULE;

	module_feature_uninit_prefix_range(feature_scope, param, module, prefix, module->num_gpl_syms);
}

static inline void module_feature_format_init_prefix(int feature_scope, void *param, char *prefix, int len, int stage)
{
	if (feature_scope == FEATURE_SCOPE_GLOBAL) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_init_", stage);
	} else if (feature_scope == FEATURE_SCOPE_DEV) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_dev_init_", stage);
	} else if (feature_scope == FEATURE_SCOPE_TASK) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_init_", stage);
	} else if (feature_scope == FEATURE_SCOPE_SELFDEF) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_%s_init_", stage,
			((struct feature_selfdef_para *)param)->scope);
	}
}

static inline void module_feature_format_uninit_prefix(int feature_scope, void *param, char *prefix, int len, int stage)
{
	if (feature_scope == FEATURE_SCOPE_GLOBAL) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_uninit_", stage);
	} else if (feature_scope == FEATURE_SCOPE_DEV) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_dev_uninit_", stage);
	} else if (feature_scope == FEATURE_SCOPE_TASK) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_uninit_", stage);
	} else if (feature_scope == FEATURE_SCOPE_SELFDEF) {
		(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_%s_uninit_", stage,
			((struct feature_selfdef_para *)param)->scope);
	}
}

static inline int module_feature_init_stage(int feature_scope, void *param, int stage)
{
	char init_prefix[PREFIX_MAX_LEN];
	char uninit_prefix[PREFIX_MAX_LEN];

	module_feature_format_init_prefix(feature_scope, param, init_prefix, PREFIX_MAX_LEN, stage);
	module_feature_format_uninit_prefix(feature_scope, param, uninit_prefix, PREFIX_MAX_LEN, stage);
	return module_feature_init_prefix(feature_scope, param, init_prefix, uninit_prefix);
}

static inline void module_feature_uninit_stage(int feature_scope, void *param, int stage)
{
	char uninit_prefix[PREFIX_MAX_LEN];

	module_feature_format_uninit_prefix(feature_scope, param, uninit_prefix, PREFIX_MAX_LEN, stage);
	module_feature_uninit_prefix(feature_scope, param, uninit_prefix);
}

static inline void module_feature_uninit_stage_range(int feature_scope, void *param, int min_stage, int max_stage)
{
	int stage;

	for (stage = max_stage; stage >= min_stage; stage--) {
		module_feature_uninit_stage(feature_scope, param, stage);
	}
}

static inline int module_feature_unified_init(int feature_scope, void *param)
{
	int stage;
	int ret;

	for (stage = FEATURE_LOADER_STAGE_0; stage <= FEATURE_LOADER_STAGE_9; stage++) {
		ret = module_feature_init_stage(feature_scope, param, stage);
		if (ret != 0) {
			module_feature_uninit_stage_range(feature_scope, param, FEATURE_LOADER_STAGE_0, stage - 1);
			return ret;
		}
	}

	return 0;
}

static inline void module_feature_unified_uninit(int feature_scope, void *param)
{
	module_feature_uninit_stage_range(feature_scope, param, FEATURE_LOADER_STAGE_0, FEATURE_LOADER_STAGE_9);
}

static inline int module_feature_auto_init(void)
{
	return module_feature_unified_init(FEATURE_SCOPE_GLOBAL, NULL);
}

static inline void module_feature_auto_uninit(void)
{
	module_feature_unified_uninit(FEATURE_SCOPE_GLOBAL, NULL);
}

static inline int module_feature_auto_init_dev(u32 udevid)
{
	struct feature_dev_para dev_para = {.udevid = udevid};

	return module_feature_unified_init(FEATURE_SCOPE_DEV, (void *)&dev_para);
}

static inline void module_feature_auto_uninit_dev(u32 udevid)
{
	struct feature_dev_para dev_para = {.udevid = udevid};

	module_feature_unified_uninit(FEATURE_SCOPE_DEV, (void *)&dev_para);
}

static inline int module_feature_auto_init_task(int pid)
{
	struct feature_task_para task_para = {.pid = pid};

	return module_feature_unified_init(FEATURE_SCOPE_TASK, (void *)&task_para);
}

static inline void module_feature_auto_uninit_task(int pid)
{
	struct feature_task_para task_para = {.pid = pid};

	module_feature_unified_uninit(FEATURE_SCOPE_TASK, (void *)&task_para);
}

/* not suggest call this in high performance situation */
static inline int module_feature_auto_init_by_scope(const char *scope, void *priv)
{
	struct feature_selfdef_para para = {scope, priv};

	return module_feature_unified_init(FEATURE_SCOPE_SELFDEF, (void *)&para);
}

/* not suggest call this in high performance situation */
static inline int module_feature_auto_call_by_scope(const char *scope, void *priv)
{
	return module_feature_auto_init_by_scope(scope, priv);
}

/* not suggest call this in high performance situation */
static inline void module_feature_auto_uninit_by_scope(const char *scope, void *priv)
{
	struct feature_selfdef_para para = {scope, priv};

	module_feature_unified_uninit(FEATURE_SCOPE_SELFDEF, (void *)&para);
}

typedef void (*_fearture_show_task_func)(int pid, struct seq_file *seq);

/* the function declared will be auto call when show task
 * init_fn : should like "void _fearture_uninit_task_func(int pid)"
 */
#define DECLARE_FEATURE_AUTO_SHOW_TASK(show_task_fn, stage) \
	void stage##_task_show_##show_task_fn(int pid, struct seq_file *seq) \
	{ \
		show_task_fn(pid, seq); \
	} \
	EXPORT_SYMBOL_GPL(stage##_task_show_##show_task_fn)

static inline void module_feature_show_task_prefix(char *prefix, int pid, struct seq_file *seq)
{
	struct module *module = THIS_MODULE;
	u32 i;

	for (i = 0; i < module->num_gpl_syms; i++) {
		void *fn = get_symbol_prefix_func(&module->gpl_syms[i], prefix);

		if (fn != NULL) {
			((_fearture_show_task_func)fn)(pid, seq);
		}
	}
}

static inline void module_feature_format_show_task_prefix(char *prefix, int len, int stage)
{
	(void)UBFM_SPRINTF(prefix, PREFIX_MAX_LEN, "FEATURE_LOADER_STAGE_%d_task_show_", stage);
}

static inline void module_feature_show_task_stage(int stage, int pid, struct seq_file *seq)
{
	char show_prefix[PREFIX_MAX_LEN];

	module_feature_format_show_task_prefix(show_prefix, PREFIX_MAX_LEN, stage);
	module_feature_show_task_prefix(show_prefix, pid, seq);
}

/* pid=0, show global info */
static inline void module_feature_auto_show_task(int pid, struct seq_file *seq)
{
	int stage;

	for (stage = FEATURE_LOADER_STAGE_0; stage <= FEATURE_LOADER_STAGE_9; stage++) {
		module_feature_show_task_stage(stage, pid, seq);
	}
}
#endif
