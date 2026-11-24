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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include "mami_string.h"

#include "ures.h"
#include "array_ctx.h"
#include "ras_log.h"
#include "ras_ctx.h"

static struct array_ctx_domain *ras_dev_domain;

struct ras_dev_ctx *ras_dev_ctx_get(u32 udevid)
{
	struct ras_dev_ctx *d_ctx = NULL;
	struct array_ctx *ctx = NULL;

	ctx = array_ctx_get(ras_dev_domain, udevid);
	if (ctx == NULL) {
		return NULL;
	}

	d_ctx = (struct ras_dev_ctx *)array_ctx_priv(ctx);
	return d_ctx;
}

void ras_dev_ctx_put(u32 udevid)
{
	struct array_ctx *ctx = array_ctx_get(ras_dev_domain, udevid);

	if (ctx != NULL) {
		array_ctx_put(ctx);
		array_ctx_put(ctx);
	}
}

static void ras_dev_ctx_release(struct array_ctx *ctx)
{
	struct ras_dev_ctx *d_ctx = (struct ras_dev_ctx *)array_ctx_priv(ctx);

	ras_info("Ras dev ctx release. (devid=%u)\n", d_ctx->udevid);
	mutex_destroy(&d_ctx->mtx);
	if (d_ctx->payload != NULL) {
		kvfree(d_ctx->payload);
	}
	if (d_ctx->entry != NULL) {
		kvfree(d_ctx->entry);
	}
	kvfree(d_ctx);
}

int ras_alloc_payload_mem(u32 udevid, u32 size)
{
	struct ras_dev_ctx *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	u32 payload_size = size;

	if (size == 0) {
		return -EINVAL;
	}

	ctx = array_ctx_get(ras_dev_domain, udevid);
	if (ctx == NULL) {
		return -ENODEV;
	}

	d_ctx = (struct ras_dev_ctx *)array_ctx_priv(ctx);
	if (d_ctx->payload != NULL && d_ctx->payload_size == size) {
		array_ctx_put(ctx);
		return 0;
	}

	if (d_ctx->payload != NULL) {
		kvfree(d_ctx->payload);
		d_ctx->payload = NULL;
	}

	if (size > RAS_DATA_ENTRY_MAX_SIZE) {
		ras_err("Ras check item_len failed. (devid=%u; item_len=0x%x)\n", udevid, size);
		payload_size = RAS_DATA_ENTRY_MAX_SIZE;
	}

	d_ctx->payload = kvmalloc(payload_size, GFP_KERNEL_ACCOUNT);
	if (d_ctx->payload == NULL) {
		array_ctx_put(ctx);
		return -ENOMEM;
	}

	d_ctx->payload_size = payload_size;
	array_ctx_put(ctx);
	return 0;
}

int ras_alloc_entry_mem(u32 udevid, u32 item_num)
{
	struct ras_dev_ctx *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	u32 entries_size;

	if (item_num == 0) {
		return -EINVAL;
	}

	ctx = array_ctx_get(ras_dev_domain, udevid);
	if (ctx == NULL) {
		return -ENODEV;
	}

	d_ctx = (struct ras_dev_ctx *)array_ctx_priv(ctx);
	if (d_ctx->entry != NULL && d_ctx->entry->item_num == item_num) {
		array_ctx_put(ctx);
		return 0;
	}

	if (d_ctx->entry != NULL) {
		kvfree(d_ctx->entry);
		d_ctx->entry = NULL;
	}

	entries_size = item_num * sizeof(struct ras_data_entry_item);
	d_ctx->entry = kvmalloc(sizeof(struct ras_data_entry) + entries_size, GFP_KERNEL_ACCOUNT);
	if (d_ctx->entry == NULL) {
		array_ctx_put(ctx);
		return -ENOMEM;
	}

	d_ctx->entry->item_num = item_num;
	array_ctx_put(ctx);
	return 0;
}

int ras_add_dev(u32 udevid)
{
	struct ras_dev_ctx *d_ctx = NULL;
	u64 value = 0;
	u32 i;
	int ret;

	d_ctx = kvmalloc(sizeof(*d_ctx), GFP_KERNEL_ACCOUNT);
	if (d_ctx == NULL) {
		ras_err("Vmalloc dev ctx fail. (size=%lu)\n", sizeof(*d_ctx));
		return -ENOMEM;
	}

	(void)ubfm_memset(d_ctx, sizeof(*d_ctx), 0, sizeof(*d_ctx));
	d_ctx->udevid = udevid;

	ret = ures_get_key_value(udevid, MAMI_DIE_NUM, &value);
	if ((ret != 0) || (value == 0) || (value > DIE_MAX_NUM)) {
		ras_err("Get die num failed. (devid=%u; die_num=%llu; ret=%d)\n", udevid, value, ret);
		kvfree(d_ctx);
		return -EINVAL;
	}

	d_ctx->die_num = (u32)value;
	for (i = 0; i < d_ctx->die_num; i++) {
		struct ures_inst inst;
		struct ures_addr_info addr_info = {0};

		ures_inst_pack(&inst, udevid, i);
		ret = ures_get_reg_base(&inst, MAMI_MISC_REG, &addr_info);
		if (ret != 0) {
			ras_err("Get misc reg failed. (devid=%u; dieid=%u; ret=%d)\n", udevid, i, ret);
			kvfree(d_ctx);
			return ret;
		}
		d_ctx->misc_addr[i] = addr_info.addr;
	}

	mutex_init(&d_ctx->mtx);

	ret = array_ctx_create(ras_dev_domain, udevid, d_ctx, ras_dev_ctx_release);
	if (ret != 0) {
		kvfree(d_ctx);
		ras_err("Create fail. (udevid=%u; ret=%d)\n", udevid, ret);
		return ret;
	}

	return 0;
}

int ras_del_dev(u32 udevid)
{
	struct array_ctx *ctx = array_ctx_get(ras_dev_domain, udevid);

	if (ctx == NULL) {
		ras_err("Get ctx failed. (devid=%u)\n", udevid);
		return -EINVAL;
	}

	array_ctx_put(ctx); /* array_ctx_get */
	array_ctx_destroy(ctx); /* array_ctx_create */

	return 0;
}

static void ras_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
	ras_warn("Recycle dev ctx. (devid=%u)\n", ctx->id);
	ras_del_dev(ctx->id);
}

int ras_ctx_init(void)
{
	ras_dev_domain = array_ctx_domain_create("ras_dev", DEV_MAX_NUM);
	if (ras_dev_domain == NULL) {
		ras_err("Create dev domain fail.\n");
		return -ENOMEM;
	}

	return 0;
}

void ras_ctx_uninit(void)
{
	array_ctx_for_each_safe(ras_dev_domain, NULL, ras_dev_ctx_recycle);
	array_ctx_domain_destroy(ras_dev_domain);
	ras_dev_domain = NULL;
}
