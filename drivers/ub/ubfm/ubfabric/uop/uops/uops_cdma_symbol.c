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
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "uop_log.h"
#include "uops_cdma_symbol.h"

enum dma_status dma_read_stub(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id)
{
	return DMA_STATUS_INVAL;
}
enum dma_status dma_write_stub(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id)
{
	return DMA_STATUS_INVAL;
}
struct dma_device *dma_get_device_list_stub(u32 *num_devices)
{
	return NULL;
}
void dma_free_device_list_stub(struct dma_device *dev_list, u32 num_devices) {}
int dma_register_client_stub(struct dma_client *client)
{
	return DMA_STATUS_INVAL;
}
void dma_unregister_client_stub(struct dma_client *client) {}
struct dma_seg *dma_register_seg_stub(struct dma_device *dma_dev, int ctx_id, struct dma_seg_cfg *cfg)
{
	return NULL;
}
struct dma_seg *dma_import_seg_stub(struct dma_seg_cfg *cfg)
{
	return NULL;
}
void dma_unregister_seg_stub(struct dma_device *dma_dev, struct dma_seg *dma_seg) {}
int dma_alloc_queue_stub(struct dma_device *dma_dev, int ctx_id, struct queue_cfg *cfg)
{
	return DMA_STATUS_INVAL;
}
int dma_poll_queue_stub(struct dma_device *dma_dev, int queue_id, u32 cr_cnt, struct dma_cr *cr)
{
	return DMA_STATUS_INVAL;
}
void dma_free_queue_stub(struct dma_device *dma_dev, int queue_id) {}
int dma_create_context_stub(struct dma_device *dma_dev)
{
	return DMA_STATUS_INVAL;
}
void dma_delete_context_stub(struct dma_device *dma_dev, int handle) {}
void dma_unimport_seg_stub(struct dma_seg *dma_seg) {}

static dma_read_kern_f g_dma_read_kern;
static dma_write_kern_f g_dma_write_kern;
static dma_get_device_list_kern_f g_dma_get_device_list_kern;
static dma_free_device_list_kern_f g_dma_free_device_list_kern;
dma_register_client_kern_f g_dma_register_client_kern;
static dma_unregister_client_kern_f g_dma_unregister_client_kern;
static dma_register_seg_kern_f g_dma_register_seg_kern;
static dma_import_seg_kern_f g_dma_import_seg_kern;
static dma_unregister_seg_kern_f g_dma_unregister_seg_kern;
static dma_alloc_queue_kern_f g_dma_alloc_queue_kern;
static dma_poll_queue_kern_f g_dma_poll_queue_kern;
static dma_free_queue_kern_f g_dma_free_queue_kern;
static dma_create_context_kern_f g_dma_create_context_kern;
static dma_delete_context_kern_f g_dma_delete_context_kern;
static dma_unimport_seg_kern_f g_dma_unimport_seg_kern;

struct dma_symbol_entry {
	const char *name;
	void **func;
	void *stub;
};

static const struct dma_symbol_entry g_dma_symbol_table[] = {
	{ "dma_read", (void **)&g_dma_read_kern, dma_read_stub },
	{ "dma_write", (void **)&g_dma_write_kern, dma_write_stub },
	{ "dma_get_device_list", (void **)&g_dma_get_device_list_kern, dma_get_device_list_stub },
	{ "dma_free_device_list", (void **)&g_dma_free_device_list_kern, dma_free_device_list_stub },
	{ "dma_register_client", (void **)&g_dma_register_client_kern, dma_register_client_stub },
	{ "dma_unregister_client", (void **)&g_dma_unregister_client_kern, dma_unregister_client_stub },
	{ "dma_register_seg", (void **)&g_dma_register_seg_kern, dma_register_seg_stub },
	{ "dma_import_seg", (void **)&g_dma_import_seg_kern, dma_import_seg_stub },
	{ "dma_unregister_seg", (void **)&g_dma_unregister_seg_kern, dma_unregister_seg_stub },
	{ "dma_alloc_queue", (void **)&g_dma_alloc_queue_kern, dma_alloc_queue_stub },
	{ "dma_poll_queue", (void **)&g_dma_poll_queue_kern, dma_poll_queue_stub },
	{ "dma_free_queue", (void **)&g_dma_free_queue_kern, dma_free_queue_stub },
	{ "dma_create_context", (void **)&g_dma_create_context_kern, dma_create_context_stub },
	{ "dma_delete_context", (void **)&g_dma_delete_context_kern, dma_delete_context_stub },
	{ "dma_unimport_seg", (void **)&g_dma_unimport_seg_kern, dma_unimport_seg_stub },
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)) && (!defined(AOS_LLVM_BUILD))
int cdma_get_kern_symbol(void **func, const char *name)
{
	*func = __symbol_get(name);
	if (*func == NULL) {
		uop_warn("The cmda symbol cannot get.(symbol=%s;)\n", name);
		return DMA_STATUS_INVAL;
	}
	return 0;
}
#else /* CDMA is only supported in kernel version 6.0+. */
int cdma_get_kern_symbol(void **func, const char *name)
{
	return DMA_STATUS_INVAL;
}
#endif

static inline int init_symbol(void **var, void *stub, const char *name)
{
	if (cdma_get_kern_symbol(var, name) != 0) {
		*var = stub;
		return DMA_STATUS_INVAL;
	}
	return 0;
}

int cdma_symbol_init(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(g_dma_symbol_table); ++i) {
		ret |= init_symbol(g_dma_symbol_table[i].func, g_dma_symbol_table[i].stub, g_dma_symbol_table[i].name);
	}
	return ret;
}

void cdma_symbol_uninit(void)
{
	unsigned int i;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)) && (!defined(AOS_LLVM_BUILD))
	for (i = 0; i < ARRAY_SIZE(g_dma_symbol_table); ++i) {
		if (*(g_dma_symbol_table[i].func) != g_dma_symbol_table[i].stub) {
			__symbol_put(g_dma_symbol_table[i].name);
			*(g_dma_symbol_table[i].func) = g_dma_symbol_table[i].stub;
		}
	}
#endif
}

enum dma_status dma_read_symbol(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id)
{
	return g_dma_read_kern(dma_dev, rmt_seg, local_seg, queue_id);
}
EXPORT_SYMBOL_GPL(dma_read_symbol);

enum dma_status dma_write_symbol(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id)
{
	return g_dma_write_kern(dma_dev, rmt_seg, local_seg, queue_id);
}
EXPORT_SYMBOL_GPL(dma_write_symbol);

struct dma_device *dma_get_device_list_symbol(u32 *num_devices)
{
	return g_dma_get_device_list_kern(num_devices);
}
EXPORT_SYMBOL_GPL(dma_get_device_list_symbol);

void dma_free_device_list_symbol(struct dma_device *dev_list, u32 num_devices)
{
	return g_dma_free_device_list_kern(dev_list, num_devices);
}
EXPORT_SYMBOL_GPL(dma_free_device_list_symbol);

int dma_register_client_symbol(struct dma_client *client)
{
	return g_dma_register_client_kern(client);
}
EXPORT_SYMBOL_GPL(dma_register_client_symbol);

void dma_unregister_client_symbol(struct dma_client *client)
{
	return g_dma_unregister_client_kern(client);
}
EXPORT_SYMBOL_GPL(dma_unregister_client_symbol);

struct dma_seg *dma_register_seg_symbol(struct dma_device *dma_dev, int ctx_id, struct dma_seg_cfg *cfg)
{
	return g_dma_register_seg_kern(dma_dev, ctx_id, cfg);
}
EXPORT_SYMBOL_GPL(dma_register_seg_symbol);

struct dma_seg *dma_import_seg_symbol(struct dma_seg_cfg *cfg)
{
	return g_dma_import_seg_kern(cfg);
}
EXPORT_SYMBOL_GPL(dma_import_seg_symbol);

void dma_unregister_seg_symbol(struct dma_device *dma_dev, struct dma_seg *dma_seg)
{
	return g_dma_unregister_seg_kern(dma_dev, dma_seg);
}
EXPORT_SYMBOL_GPL(dma_unregister_seg_symbol);

int dma_alloc_queue_symbol(struct dma_device *dma_dev, int ctx_id,
			struct queue_cfg *cfg)
{
	return g_dma_alloc_queue_kern(dma_dev, ctx_id, cfg);
}
EXPORT_SYMBOL_GPL(dma_alloc_queue_symbol);

int dma_poll_queue_symbol(struct dma_device *dma_dev, int queue_id, u32 cr_cnt, struct dma_cr *cr)
{
	return g_dma_poll_queue_kern(dma_dev, queue_id, cr_cnt, cr);
}
EXPORT_SYMBOL_GPL(dma_poll_queue_symbol);

void dma_free_queue_symbol(struct dma_device *dma_dev, int queue_id)
{
	return g_dma_free_queue_kern(dma_dev, queue_id);
}
EXPORT_SYMBOL_GPL(dma_free_queue_symbol);

int dma_create_context_symbol(struct dma_device *dma_dev)
{
	return g_dma_create_context_kern(dma_dev);
}
EXPORT_SYMBOL_GPL(dma_create_context_symbol);

void dma_delete_context_symbol(struct dma_device *dma_dev, int handle)
{
	return g_dma_delete_context_kern(dma_dev, handle);
}
EXPORT_SYMBOL_GPL(dma_delete_context_symbol);

void dma_unimport_seg_symbol(struct dma_seg *dma_seg)
{
	return g_dma_unimport_seg_kern(dma_seg);
}
EXPORT_SYMBOL_GPL(dma_unimport_seg_symbol);
