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
#include <linux/module.h>

#include "ubfm_common.h"
#include "usi_ubus_api.h"
#include "usi_log.h"

typedef int (*ub_cfg_write_dword_fn)(struct ub_entity *, u64, u32);
typedef void (*ub_disable_intr_fn)(struct ub_entity *);
typedef int (*ub_irq_vector_fn)(struct ub_entity *, unsigned int);
typedef void (*ub_unset_user_info_fn)(struct ub_entity *);
typedef int (*ub_set_user_info_fn)(struct ub_entity *);
typedef void (*ub_entity_enable_fn)(struct ub_entity *, u8);
typedef int (*ub_alloc_irq_vectors_affinity_fn)(struct ub_entity *, unsigned int, unsigned int, unsigned int,
										  struct irq_affinity *);
typedef struct ub_entity *(*ub_get_ent_by_eid_fn)(unsigned int);
typedef int (*ub_register_driver_fn)(struct ub_driver *, struct module *, const char *);
typedef void (*ub_unregister_driver_fn)(struct ub_driver *);

enum ubus_api_func {
	UBUS_WRITE_DWORD,
	UBUS_DISABLE_INTR,
	UBUS_IRQ_VECTOR,
	UBUS_UNSET_USER_INFO,
	UBUS_SET_USER_INFO,
	UBUS_ENTITY_ENABLE,
	UBUS_ALLOC_IRQ_VECTOR,
	UBUS_GET_ENT_BY_EID,
	UBUS_REGISTER_DRV,
	UBUS_UNREGISTER_DRV,
	UBUS_API_MAX
};

struct ubus_api_entry {
	const char *name;
	void *func;
};

static struct ubus_api_entry g_ubus_api_table[UBUS_API_MAX] = {
	[UBUS_WRITE_DWORD] = { "ub_cfg_write_dword", NULL },
	[UBUS_DISABLE_INTR] = { "ub_disable_intr", NULL },
	[UBUS_IRQ_VECTOR] = { "ub_irq_vector", NULL },
	[UBUS_UNSET_USER_INFO] = { "ub_unset_user_info", NULL },
	[UBUS_SET_USER_INFO] = { "ub_set_user_info", NULL },
	[UBUS_ENTITY_ENABLE] = { "ub_entity_enable", NULL },
	[UBUS_ALLOC_IRQ_VECTOR] = { "ub_alloc_irq_vectors_affinity", NULL },
	[UBUS_GET_ENT_BY_EID] = { "ub_get_ent_by_eid", NULL },
	[UBUS_REGISTER_DRV] = { "__ub_register_driver", NULL },
	[UBUS_UNREGISTER_DRV] = { "ub_unregister_driver", NULL },
};

int usi_ub_cfg_write_dword(struct ub_entity *uent, u64 pos, u32 val)
{
	if (!g_ubus_api_table[UBUS_WRITE_DWORD].func) {
		return -EPERM;
	}

	return ((ub_cfg_write_dword_fn)g_ubus_api_table[UBUS_WRITE_DWORD].func)(uent, pos, val);
}

void usi_ub_disable_intr(struct ub_entity *uent)
{
	if (!g_ubus_api_table[UBUS_DISABLE_INTR].func) {
		return;
	}

	return ((ub_disable_intr_fn)g_ubus_api_table[UBUS_DISABLE_INTR].func)(uent);
}

int usi_ub_irq_vector(struct ub_entity *uent, unsigned int nr)
{
	if (!g_ubus_api_table[UBUS_IRQ_VECTOR].func) {
		return -EPERM;
	}

	return ((ub_irq_vector_fn)g_ubus_api_table[UBUS_IRQ_VECTOR].func)(uent, nr);
}

void usi_ub_unset_user_info(struct ub_entity *uent)
{
	if (!g_ubus_api_table[UBUS_UNSET_USER_INFO].func) {
		return;
	}

	return ((ub_unset_user_info_fn)g_ubus_api_table[UBUS_UNSET_USER_INFO].func)(uent);
}

int usi_ub_set_user_info(struct ub_entity *uent)
{
	if (!g_ubus_api_table[UBUS_SET_USER_INFO].func) {
		return -EPERM;
	}

	return ((ub_set_user_info_fn)g_ubus_api_table[UBUS_SET_USER_INFO].func)(uent);
}

void usi_ub_entity_enable(struct ub_entity *uent, u8 enable)
{
	if (!g_ubus_api_table[UBUS_ENTITY_ENABLE].func) {
		return;
	}

	return ((ub_entity_enable_fn)g_ubus_api_table[UBUS_ENTITY_ENABLE].func)(uent, enable);
}

int usi_ub_alloc_irq_vectors(struct ub_entity *uent, unsigned int min, unsigned int max)
{
	if (!g_ubus_api_table[UBUS_ALLOC_IRQ_VECTOR].func) {
		return -EPERM;
	}

	return ((ub_alloc_irq_vectors_affinity_fn)g_ubus_api_table[UBUS_ALLOC_IRQ_VECTOR].func)(uent, min, max, 0, NULL);
}

struct ub_entity *usi_ub_get_ent_by_eid(unsigned int eid)
{
	if (!g_ubus_api_table[UBUS_GET_ENT_BY_EID].func) {
		return NULL;
	}

	return ((ub_get_ent_by_eid_fn)g_ubus_api_table[UBUS_GET_ENT_BY_EID].func)(eid);
}

int usi_ub_register_driver(struct ub_driver *drv)
{
	if (!g_ubus_api_table[UBUS_REGISTER_DRV].func) {
		return -EPERM;
	}

	return ((ub_register_driver_fn)g_ubus_api_table[UBUS_REGISTER_DRV].func)(drv, THIS_MODULE, KBUILD_MODNAME);
}

void usi_ub_unregister_driver(struct ub_driver *drv)
{
	if (!g_ubus_api_table[UBUS_UNREGISTER_DRV].func) {
		return;
	}

	return ((ub_unregister_driver_fn)g_ubus_api_table[UBUS_UNREGISTER_DRV].func)(drv);
}

int usi_ubus_api_init(void)
{
	int i;

	for (i = 0; i < UBUS_API_MAX; i++) {
		if (!g_ubus_api_table[i].name) {
			continue;
		}

		g_ubus_api_table[i].func = __symbol_get(g_ubus_api_table[i].name);
		if (!g_ubus_api_table[i].func) {
			usi_err("Failed to get ubus symbol: %s.\n", g_ubus_api_table[i].name);
			usi_ubus_api_uninit();
			return -ENOENT;
		}
	}

	usi_info("Usi ubus api init successfully.\n");
	return 0;
}

void usi_ubus_api_uninit(void)
{
	int i;

	for (i = 0; i < UBUS_API_MAX; i++) {
		if (g_ubus_api_table[i].func) {
			g_ubus_api_table[i].func = NULL;
			__symbol_put(g_ubus_api_table[i].name);
		}
	}
}
