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
#ifndef KREF_SAFE_H
#define KREF_SAFE_H

#include <linux/kref.h>
#include <linux/preempt.h>
#include <linux/workqueue.h>

struct kref_safe {
	struct kref kref;
	void (*release)(struct kref_safe *kref_s);
	struct work_struct work;
};

static void kref_safe_release_work(struct work_struct *work)
{
	struct kref_safe *kref_s = container_of(work, struct kref_safe, work);

	kref_s->release(kref_s);
}

static inline void kref_safe_init(struct kref_safe *kref_s)
{
	INIT_WORK(&kref_s->work, kref_safe_release_work);
	kref_init(&kref_s->kref);
}

static inline unsigned int kref_safe_read(const struct kref_safe *kref_s)
{
	return kref_read((const struct kref *)&kref_s->kref);
}

static inline void kref_safe_get(struct kref_safe *kref_s)
{
	kref_get(&kref_s->kref);
}

static inline int kref_safe_put(struct kref_safe *kref_s, void (*release)(struct kref_safe *kref_s))
{
	struct kref *kref = &kref_s->kref;

	if (refcount_dec_and_test(&kref->refcount)) {
		if (in_interrupt()) {
			/* The blocking function may be invoked in release.
			 * If the blocking function is invoked during the
			 * interruption, an exception occurs. Therefore,
			 * the release work is started.
			 */
			kref_s->release = release;
			schedule_work(&kref_s->work);
		} else {
			release(kref_s);
		}
		return 1;
	}
	return 0;
}

static inline int __must_check kref_safe_get_unless_zero(struct kref_safe *kref_s)
{
	return kref_get_unless_zero(&kref_s->kref);
}

#endif

