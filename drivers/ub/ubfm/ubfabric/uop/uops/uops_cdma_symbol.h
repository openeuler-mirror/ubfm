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
#ifndef UOPS_CDMA_SYMBOL_H
#define UOPS_CDMA_SYMBOL_H

#include "ub/cdma/cdma_api.h"

typedef enum dma_status (*dma_read_kern_f)(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id);
typedef enum dma_status (*dma_write_kern_f)(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id);
typedef struct dma_device *(*dma_get_device_list_kern_f)(u32 *num_devices);
typedef void (*dma_free_device_list_kern_f)(struct dma_device *dev_list, u32 num_devices);
typedef int (*dma_register_client_kern_f)(struct dma_client *client);
typedef void (*dma_unregister_client_kern_f)(struct dma_client *client);
typedef struct dma_seg *(*dma_register_seg_kern_f)(struct dma_device *dma_dev, int ctx_id, struct dma_seg_cfg *cfg);
typedef struct dma_seg *(*dma_import_seg_kern_f)(struct dma_seg_cfg *cfg);
typedef void (*dma_unregister_seg_kern_f)(struct dma_device *dma_dev, struct dma_seg *dma_seg);
typedef int (*dma_alloc_queue_kern_f)(struct dma_device *dma_dev, int ctx_id, struct queue_cfg *cfg);
typedef int (*dma_poll_queue_kern_f)(struct dma_device *dma_dev, int queue_id, u32 cr_cnt, struct dma_cr *cr);
typedef void (*dma_free_queue_kern_f)(struct dma_device *dma_dev, int queue_id);
typedef int (*dma_create_context_kern_f)(struct dma_device *dma_dev);
typedef void (*dma_delete_context_kern_f)(struct dma_device *dma_dev, int handle);
typedef void (*dma_unimport_seg_kern_f)(struct dma_seg *dma_seg);

enum dma_status dma_read_symbol(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id);
enum dma_status dma_write_symbol(struct dma_device *dma_dev, struct dma_seg *rmt_seg,
			struct dma_seg *local_seg, int queue_id);
struct dma_device *dma_get_device_list_symbol(u32 *num_devices);
void dma_free_device_list_symbol(struct dma_device *dev_list, u32 num_devices);
int dma_register_client_symbol(struct dma_client *client);
void dma_unregister_client_symbol(struct dma_client *client);
struct dma_seg *dma_register_seg_symbol(struct dma_device *dma_dev, int ctx_id, struct dma_seg_cfg *cfg);
struct dma_seg *dma_import_seg_symbol(struct dma_seg_cfg *cfg);
void dma_unregister_seg_symbol(struct dma_device *dma_dev, struct dma_seg *dma_seg);
int dma_alloc_queue_symbol(struct dma_device *dma_dev, int ctx_id, struct queue_cfg *cfg);
int dma_poll_queue_symbol(struct dma_device *dma_dev, int queue_id, u32 cr_cnt, struct dma_cr *cr);
void dma_free_queue_symbol(struct dma_device *dma_dev, int queue_id);
int dma_create_context_symbol(struct dma_device *dma_dev);
void dma_delete_context_symbol(struct dma_device *dma_dev, int handle);
void dma_unimport_seg_symbol(struct dma_seg *dma_seg);

extern dma_register_client_kern_f g_dma_register_client_kern;
int dma_register_client_stub(struct dma_client *client);

int cdma_symbol_init(void);
void cdma_symbol_uninit(void);
#endif

