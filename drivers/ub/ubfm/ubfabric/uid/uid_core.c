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
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/err.h>

#include "array_ctx.h"

#include "uid.h"
#include "ubfm_common.h"
#include "uid_core.h"

struct uid_intr {
	bool registered;
	struct uid_intr_attr attr;
	struct uid_intr_ops ops;
};

struct uid_intr_inst {
	enum uid_intr_type type;
	atomic_t init_flag; /* atomic_cmpxchg guards uninit paths to avoid double-free of workqueue */
	struct uid_intr_attr attr;
	struct delayed_work dwork;
	union {
		struct tasklet_struct tasklet;
		struct work_struct work;
	} bottom_half;
	struct workqueue_struct *wq;
	/* cached at init from uid_intrs[type].ops.owner; module_put in uninit
	 * uses this local copy to pair with try_module_get and avoid racing with
	 * uid_register_intr overwriting uid_intrs[type].ops
	 */
	struct module *owner;
	u64 para;
	u64 time;
	atomic_t triggered;
	bool timeout_flag;
};

struct uid_dev_ctx {
	u32 udevid;
	struct uid_intr_inst intr_inst[UID_TYPE_MAX];
};

static struct uid_intr uid_intrs[UID_TYPE_MAX];
static DEFINE_MUTEX(uid_intr_mutex);
static struct array_ctx_domain *uid_dev_domain;

int uid_register_intr(enum uid_intr_type type, struct uid_intr_attr *attr, struct uid_intr_ops *ops)
{
	if ((type >= UID_TYPE_MAX) || (attr == NULL) || (ops == NULL)) {
		uid_err("Invalid para. (type=%u)\n", type);
		return -EINVAL;
	}

	mutex_lock(&uid_intr_mutex);
	if (uid_intrs[type].registered) {
		mutex_unlock(&uid_intr_mutex);
		uid_err("Has been registered. (type=%u)\n", type);
		return -EINVAL;
	}

	uid_intrs[type].registered = true;
	uid_intrs[type].attr = *attr;
	uid_intrs[type].ops = *ops;
	mutex_unlock(&uid_intr_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(uid_register_intr);

static void uid_intr_top_half_pre_handle(struct uid_intr *intr, u32 udevid, enum uid_intr_type type, u64 para)
{
	if (intr->registered) {
		if (intr->ops.top_half_pre_handle) {
			intr->ops.top_half_pre_handle(udevid, type, para);
		}
	}
}

static void uid_intr_handle(struct uid_intr *intr, u32 udevid, enum uid_intr_type type, u32 flag, struct uid_irq_info irq_info)
{
	if (intr->registered) {
		intr->ops.handle(udevid, type, flag, irq_info);
	}
}

static void uid_bottom_half(struct uid_intr_inst *intr_inst)
{
	struct uid_dev_ctx *d_ctx = container_of(intr_inst, struct uid_dev_ctx, intr_inst[intr_inst->type]);
	struct uid_irq_info irq_info = {.irq_para.val = intr_inst->para, .irq_time = intr_inst->time};
	struct uid_intr *intr = &uid_intrs[intr_inst->type];
	u32 flag = intr_inst->timeout_flag ? UID_HANDLE_TIMEOUT_FLAG : 0;

	uid_intr_handle(intr, d_ctx->udevid, intr_inst->type, flag, irq_info);
	atomic_set(&intr_inst->triggered, 0);
}

static void uid_bottom_half_work(struct work_struct *work)
{
	struct uid_intr_inst *intr_inst = container_of(work, struct uid_intr_inst, bottom_half.work);

	uid_bottom_half(intr_inst);
}

static void uid_bottom_half_tasklet(unsigned long data)
{
	struct uid_intr_inst *intr_inst = (struct uid_intr_inst *)(uintptr_t)data;

	uid_bottom_half(intr_inst);
}

static void uid_sched_bottom_half(struct uid_intr_inst *intr_inst, bool timeout_flag, struct uid_irq_info irq_info)
{
	int triggered = atomic_cmpxchg(&intr_inst->triggered, 0, 1);

	intr_inst->timeout_flag = timeout_flag;
	intr_inst->para = irq_info.irq_para.val;
	intr_inst->time = irq_info.irq_time;

	if (intr_inst->attr.max_sched_time > 0) {
		mod_delayed_work(system_wq, &intr_inst->dwork, msecs_to_jiffies(intr_inst->attr.max_sched_time));
	}

	if (triggered == 1) {
		return;
	}

	if (intr_inst->attr.block_flag != 0) {
		queue_work(intr_inst->wq, &intr_inst->bottom_half.work);
	} else {
		tasklet_schedule(&intr_inst->bottom_half.tasklet);
	}
}

static void uid_delayed_work_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct uid_intr_inst *intr_inst = container_of(dwork, struct uid_intr_inst, dwork);
	struct uid_irq_info irq_info = {0};

	uid_sched_bottom_half(intr_inst, true, irq_info);
}

/* in interrupt context, not add log */
void uid_trigger_intr(u32 udevid, enum uid_intr_type type, struct uid_irq_info irq_info)
{
	struct uid_dev_ctx *d_ctx = NULL;
	struct array_ctx *ctx = NULL;
	struct uid_intr_inst *intr_inst = NULL;
	struct uid_intr *intr = NULL;

	if (type >= UID_TYPE_MAX) {
		return;
	}

	ctx = array_ctx_get(uid_dev_domain, udevid);
	if (ctx == NULL) {
		return;
	}

	d_ctx = (struct uid_dev_ctx *)array_ctx_priv(ctx);
	intr_inst = &d_ctx->intr_inst[type];
	intr = &uid_intrs[type];

	if (atomic_read(&intr_inst->init_flag) != 0) {
		uid_intr_top_half_pre_handle(intr, udevid, type, irq_info.irq_para.val);
		uid_sched_bottom_half(intr_inst, false, irq_info);
	}

	array_ctx_put(ctx);
}
EXPORT_SYMBOL_GPL(uid_trigger_intr);

static int uid_alloc_intr_wq(u32 udevid, struct uid_intr_inst *intr_inst)
{
	intr_inst->wq = alloc_workqueue("uid_inst_%u_%d", WQ_UNBOUND | WQ_HIGHPRI, 0, udevid, intr_inst->type);
	if (intr_inst->wq == NULL) {
		uid_err("Alloc workqueue failed. (devid=%u; type=%u)\n", udevid, intr_inst->type);
		return -ENOMEM;
	}
	return 0;
}

static void uid_destroy_intr_wq(struct uid_intr_inst *intr_inst)
{
	if (intr_inst->wq != NULL) {
		flush_workqueue(intr_inst->wq);
		destroy_workqueue(intr_inst->wq);
		intr_inst->wq = NULL;
	}
}

static void uid_uninit_intr_inst_by_type(struct uid_dev_ctx *d_ctx, enum uid_intr_type type)
{
	struct uid_intr_inst *intr_inst = &d_ctx->intr_inst[type];

	/* Atomic claim: only one path changes init_flag from 1 to 0,
	 * the other (rmmod traversal or release callback) returns early,
	 * avoiding double execution of destroy_workqueue/tasklet_kill/module_put.
	 */
	if (atomic_cmpxchg(&intr_inst->init_flag, 1, 0) != 1) {
		return;
	}

	if (intr_inst->attr.max_sched_time > 0) {
		cancel_delayed_work_sync(&intr_inst->dwork);
	}

	if (intr_inst->attr.block_flag != 0) {
		cancel_work_sync(&intr_inst->bottom_half.work);
	} else {
		tasklet_kill(&intr_inst->bottom_half.tasklet);
	}

	uid_destroy_intr_wq(intr_inst);
	/* Use cached owner to pair with try_module_get in uid_init_intr_inst.
	 * Reading uid_intrs[type].ops.owner here would race with uid_register_intr
	 * overwriting ops (same type can be re-registered after unregister).
	 */
	if (intr_inst->owner != NULL) {
		module_put(intr_inst->owner);
	}
}

static void uid_uninit_intr_inst(struct uid_dev_ctx *d_ctx)
{
	enum uid_intr_type type;

	/* No uid_intr_mutex needed: mutual exclusion between uninit paths is
	 * guaranteed by atomic_cmpxchg in uid_uninit_intr_inst_by_type,
	 * preventing recursive lock deadlock when uid_unregister_intr traversal
	 * triggers the release callback.
	 */
	for (type = 0; type < UID_TYPE_MAX; type++) {
		uid_uninit_intr_inst_by_type(d_ctx, type);
	}
}

static int uid_init_intr_inst(struct uid_dev_ctx *d_ctx)
{
	enum uid_intr_type type;
	int ret;

	mutex_lock(&uid_intr_mutex);
	for (type = 0; type < UID_TYPE_MAX; type++) {
		struct uid_intr_inst *intr_inst = &d_ctx->intr_inst[type];
		struct uid_intr *intr = &uid_intrs[type];

		atomic_set(&intr_inst->init_flag, 0);

		if (!intr->registered) {
			continue;
		}

		if (intr->ops.owner != NULL && !try_module_get(intr->ops.owner)) {
			continue;
		}
		intr_inst->owner = intr->ops.owner; /* cache owner for module_put in uninit */

		intr_inst->type = type;
		intr_inst->attr = intr->attr;
		atomic_set(&intr_inst->triggered, 0);
		intr_inst->timeout_flag = false;

		ret = uid_alloc_intr_wq(d_ctx->udevid, intr_inst);
		if (ret != 0) {
			mutex_unlock(&uid_intr_mutex);
			uid_uninit_intr_inst(d_ctx);
			return ret;
		}

		if (intr_inst->attr.block_flag != 0) {
			INIT_WORK(&intr_inst->bottom_half.work, uid_bottom_half_work);
		} else {
			tasklet_init(&intr_inst->bottom_half.tasklet, uid_bottom_half_tasklet, (unsigned long)(uintptr_t)intr_inst);
		}

		if (intr_inst->attr.max_sched_time > 0) {
			INIT_DELAYED_WORK(&intr_inst->dwork, uid_delayed_work_handler);
			queue_delayed_work(system_wq, &intr_inst->dwork, msecs_to_jiffies(intr_inst->attr.max_sched_time));
		}

		/* Set init_flag to 1 only after wq/work/tasklet are fully ready,
		 * so uid_trigger_intr can safely queue_work upon reading 1.
		 */
		atomic_set(&intr_inst->init_flag, 1);
	}
	mutex_unlock(&uid_intr_mutex);
	return 0;
}

static void uid_dev_ctx_release(struct array_ctx *ctx)
{
	struct uid_dev_ctx *d_ctx = (struct uid_dev_ctx *)array_ctx_priv(ctx);

	uid_uninit_intr_inst(d_ctx);
	vfree(d_ctx);
}

int uid_add_dev(u32 udevid)
{
	struct uid_dev_ctx *d_ctx = NULL;
	int ret;

	d_ctx = vzalloc(sizeof(*d_ctx));
	if (d_ctx == NULL) {
		uid_err("Vmalloc dev ctx fail. (size=%lu)\n", sizeof(*d_ctx));
		return -ENOMEM;
	}

	d_ctx->udevid = udevid;

	ret = array_ctx_create(uid_dev_domain, udevid, d_ctx, uid_dev_ctx_release);
	if (ret != 0) {
		vfree(d_ctx);
		uid_err("Create fail. (ret=%d; udevid=%u)\n", ret, udevid);
		return ret;
	}

	return uid_init_intr_inst(d_ctx);
}

int uid_del_dev(u32 udevid)
{
	struct array_ctx *ctx = array_ctx_get(uid_dev_domain, udevid);

	if (ctx == NULL) {
		return -EINVAL;
	}

	array_ctx_put(ctx); /* array_ctx_get */
	array_ctx_destroy(ctx); /* array_ctx_create */

	return 0;
}

static void uid_dev_ctx_recycle_by_type(struct array_ctx *ctx, void *priv)
{
	struct uid_dev_ctx *d_ctx = (struct uid_dev_ctx *)array_ctx_priv(ctx);
	enum uid_intr_type type = *(enum uid_intr_type *)priv;

	uid_warn("Recycle dev ctx intr. (devid=%u; type=%u)\n", ctx->id, type);
	uid_uninit_intr_inst_by_type(d_ctx, type);
}

void uid_uninit_all_intr_inst_by_type(enum uid_intr_type type)
{
	array_ctx_for_each_safe(uid_dev_domain, &type, uid_dev_ctx_recycle_by_type);
}

static void uid_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
	uid_warn("Recycle dev ctx. (devid=%u)\n", ctx->id);
	uid_del_dev(ctx->id);
}

int uid_core_init(void)
{
	uid_dev_domain = array_ctx_domain_create("uid_dev", DEV_MAX_NUM);
	if (uid_dev_domain == NULL) {
		uid_err("Create dev domain fail.\n");
		return -ENOMEM;
	}
	return 0;
}

void uid_core_uninit(void)
{
	array_ctx_for_each_safe(uid_dev_domain, NULL, uid_dev_ctx_recycle);
	array_ctx_domain_destroy(uid_dev_domain);
	uid_dev_domain = NULL;
}

int uid_unregister_intr(enum uid_intr_type type)
{
	if (type >= UID_TYPE_MAX) {
		uid_err("Invalid para. (type=%u)\n", type);
		return -EINVAL;
	}

	mutex_lock(&uid_intr_mutex);
	if (!uid_intrs[type].registered) {
		mutex_unlock(&uid_intr_mutex);
		uid_err("Not register. (type=%u)\n", type);
		return -ENODEV;
	}

	uid_uninit_all_intr_inst_by_type(type);
	uid_intrs[type].registered = false;
	mutex_unlock(&uid_intr_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(uid_unregister_intr);
