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
#ifndef RAS_CTX_H
#define RAS_CTX_H

#include <linux/mutex.h>
#include "ubfm_common.h"
#include "ras_def.h"

struct ras_dev_ctx {
	u32 udevid;
	u32 die_num;
	bool msi_support;/* out-of-band msi interrupt requires enabling the device-side interrupt and clearing the device-side interrupt status */
	u64 misc_addr[DIE_MAX_NUM];
	struct mutex mtx;
	struct mami_fault_event event;
	u32 payload_size;
	void *payload;	/* Payload starts at NULL; allocated when entry size known. If the memory size mismatches payload_size, free and re-allocate. */
	struct ras_data_entry *entry;	/* entry starts at NULL; allocated when item_num known. If memory has been allocated but memory size mismatches item_num, free and re-allocate. */
};

struct ras_dev_ctx *ras_dev_ctx_get(u32 udevid);
void ras_dev_ctx_put(u32 udevid);

int ras_alloc_payload_mem(u32 udevid, u32 size);
int ras_alloc_entry_mem(u32 udevid, u32 item_num);
int ras_add_dev(u32 udevid);
int ras_del_dev(u32 udevid);
int ras_ctx_init(void);
void ras_ctx_uninit(void);

#endif
