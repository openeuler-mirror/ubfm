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
#include <linux/slab.h> /* kzalloc */
#include <linux/delay.h> /* msleep */
#include <linux/jiffies.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "mami_string.h"
#include "task_ctx.h"
#include "array_ctx.h"
#include "ubfabric.h"

#include "ued.h"
#include "ubfabric.h"
#include "ued_core.h"
#include "ued_proc_fs.h"

#define UED_MAX_TASK 32
#define UED_MAX_GRP 16
#define UED_MAX_THREAD 4
/* The maximum subscribers specified is derived from the maximum number of user processes (10)
 * plus the number of device control processes (1).
 */
#define UED_MAX_SUBSCRIBER 11
#define UED_EVENT_QUE_DEPTH 128

#define SCHED_THREAD_STATUS_IDLE 0
#define SCHED_THREAD_STATUS_READY 1
#define SCHED_THREAD_STATUS_RUN 2

struct ued_event_que {
	u32 depth;
	u32 head;
	u32 tail;
	spinlock_t lock;
	struct ued_event *event[UED_EVENT_QUE_DEPTH];
};

struct ued_thread_ctx {
	atomic_t status;
	u32 valid;
	u32 tid; /* Threads in the group, numbered from 0 */
	u32 pid; /* read thread id */
	u32 sched_event_num[UED_EVENT_MAX_NUM];
	struct mutex mutex;
	wait_queue_head_t wq;
};

struct ued_grp_stat {
	atomic_t publish_event_num;
	atomic_t sched_event_num;
	atomic_t full_drop_event_num;
};

struct ued_grp_ctx {
	int valid;
	u32 gid;
	int pid;
	u32 thread_num;
	u64 event_bitmap;
	struct ued_grp_stat stat[UED_EVENT_MAX_NUM];
	char name[EVENT_MAX_GRP_NAME_LEN];
	struct ued_event_que event_que;
	struct ued_thread_ctx thread_ctx[UED_MAX_THREAD];
};

struct ued_task_ctx {
	int pid;
	u32 devid;
	u32 task_id;
	u32 valid;
	u64 start_timestamp;
	u64 exit_timestamp;
	struct ued_grp_ctx grp_ctx[UED_MAX_GRP];
	struct proc_dir_entry *entry;
};

struct ued_event_subscriber {
	int pid;
	u32 gid;
};

struct ued_dev_ctx {
	u32 devid;
	u32 cur_task_id;
	struct mutex subscriber_mutex;
	struct ued_event_subscriber subscriber[UED_EVENT_MAX_NUM][UED_MAX_SUBSCRIBER];
	struct task_ctx_domain *domain;
	struct proc_dir_entry *entry;
};

static struct array_ctx_domain *ued_dev_domain;

static struct task_ctx *ued_task_ctx_get(u32 devid, int pid)
{
	struct array_ctx *ctx = array_ctx_get(ued_dev_domain, devid);

	if (ctx != NULL) {
		struct task_ctx *tctx = task_ctx_get(((struct ued_dev_ctx *)array_ctx_priv(ctx))->domain, pid);

		array_ctx_put(ctx);
		return tctx;
	}

	return NULL;
}

static void ued_task_ctx_put(struct task_ctx *ctx)
{
	task_ctx_put(ctx);
}

static int ued_event_add_subscriber(struct ued_dev_ctx *d_ctx, int pid, u32 gid, u64 event_bitmap)
{
	int i, j;

	mutex_lock(&d_ctx->subscriber_mutex);

	/* first round check */
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		if (((0x1ULL << i) & event_bitmap) == 0) {
			continue;
		}

		for (j = 0; j < UED_MAX_SUBSCRIBER; j++) {
			struct ued_event_subscriber *subscriber = &d_ctx->subscriber[i][j];

			if (subscriber->pid == 0) {
				break;
			}
		}

		if (j >= UED_MAX_SUBSCRIBER) {
			mutex_unlock(&d_ctx->subscriber_mutex);
			ued_err("Event subscriber overflow. (devid=%u; event=%d)\n", d_ctx->devid, i);
			return -ENOSPC;
		}
	}

	/* second round set */
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		if (((0x1ULL << i) & event_bitmap) == 0) {
			continue;
		}

		for (j = 0; j < UED_MAX_SUBSCRIBER; j++) {
			struct ued_event_subscriber *subscriber = &d_ctx->subscriber[i][j];

			if (subscriber->pid == 0) {
				subscriber->pid = pid;
				subscriber->gid = gid;
				break;
			}
		}
	}

	mutex_unlock(&d_ctx->subscriber_mutex);

	return 0;
}

static void ued_event_del_subscriber(struct ued_dev_ctx *d_ctx, int pid, u32 gid)
{
	int i, j;

	mutex_lock(&d_ctx->subscriber_mutex);

	/* second round set */
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		for (j = 0; j < UED_MAX_SUBSCRIBER; j++) {
			struct ued_event_subscriber *subscriber = &d_ctx->subscriber[i][j];

			if ((subscriber->pid == pid) && (subscriber->gid == gid)) {
				subscriber->pid = 0;
				subscriber->gid = 0;
				break;
			}
		}
	}

	mutex_unlock(&d_ctx->subscriber_mutex);
}

static void ued_show_event_subscriber(struct ued_dev_ctx *d_ctx, struct seq_file *seq)
{
	int i, j;

	seq_printf(seq, "devid %u event subscriber\n", d_ctx->devid);

	mutex_lock(&d_ctx->subscriber_mutex);

	/* second round set */
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		for (j = 0; j < UED_MAX_SUBSCRIBER; j++) {
			struct ued_event_subscriber *subscriber = &d_ctx->subscriber[i][j];

			if (subscriber->pid != 0) {
				seq_printf(seq, "    event %d subscriber %d pid %d gid %u\n", i, j, subscriber->pid, subscriber->gid);
			}
		}
	}

	mutex_unlock(&d_ctx->subscriber_mutex);
}

void ued_show_subscriber(u32 devid, struct seq_file *seq)
{
	struct array_ctx *ctx = array_ctx_get(ued_dev_domain, devid);

	if (ctx != NULL) {
		ued_show_event_subscriber((struct ued_dev_ctx *)array_ctx_priv(ctx), seq);
		array_ctx_put(ctx);
	}
}

static int ued_event_enque(struct ued_event_que *event_que, struct ued_event *event)
{
	u32 tail;

	spin_lock_bh(&event_que->lock);
	tail = (event_que->tail + 1) % event_que->depth;
	if (tail == event_que->head) {
		spin_unlock_bh(&event_que->lock);
		return -ENOSPC;
	}
	event_que->event[event_que->tail] = event;
	event_que->tail = tail;
	spin_unlock_bh(&event_que->lock);

	return 0;
}

static struct ued_event *ued_event_deque(struct ued_event_que *event_que)
{
	struct ued_event *event = NULL;

	spin_lock_bh(&event_que->lock);
	if (event_que->head != event_que->tail) {
		event = event_que->event[event_que->head];
		event_que->event[event_que->head] = NULL;
		event_que->head = (event_que->head + 1) % event_que->depth;
	}
	spin_unlock_bh(&event_que->lock);

	return event;
}

static void ued_clear_event_que(struct ued_event_que *event_que)
{
	while (1) {
		struct ued_event *event = ued_event_deque(event_que);

		if (event == NULL) {
			break;
		}

		kfree(event);
	};
}

static inline u64 get_event_raise_time(void)
{
	struct timespec64 ts = {0};

	ktime_get_real_ts64(&ts);
	return ((u64)ts.tv_sec * MSEC_PER_SEC) + ((u64)ts.tv_nsec / NSEC_PER_MSEC);
}

static int ued_fill_event_info(struct ued_event *event_t, struct ued_published_event *event)
{
	event_t->event_id = event->event_id;
	event_t->subevent_id = event->subevent_id;
	event_t->msg_len = event->msg_len;
	event_t->timestamp = get_event_raise_time();
	if (event->msg_len > 0) {
		return ubfm_memcpy(event_t->msg, UED_MAX_EVENT_LEN, event->msg, event->msg_len);
	}

	return 0;
}

static void ued_wake_up_grp_thread(struct ued_grp_ctx *grp_ctx)
{
	int old_status;
	u32 i;

	/* find a thread is in idle state to wakeup. */
	for (i = 0; i < grp_ctx->thread_num; i++) {
		struct ued_thread_ctx *thread_ctx = &grp_ctx->thread_ctx[i];

		if (thread_ctx->valid == 1) {
			old_status = atomic_cmpxchg(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE, SCHED_THREAD_STATUS_READY);
			if (old_status == SCHED_THREAD_STATUS_IDLE) {
				wake_up_interruptible(&thread_ctx->wq);
				break;
			}
		}
	}
}

static void ued_wake_up_grp_all_thread(struct ued_grp_ctx *grp_ctx)
{
	u32 i;

	for (i = 0; i < grp_ctx->thread_num; i++) {
		struct ued_thread_ctx *thread_ctx = &grp_ctx->thread_ctx[i];

		if (thread_ctx->valid == 1) {
			wake_up_interruptible(&thread_ctx->wq);
		}
	}
}

static int ued_submit_event_to_grp(struct ued_task_ctx *t_ctx, u32 gid, struct ued_published_event *event)
{
	struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[gid];
	struct ued_event *event_t = NULL;
	int ret;

	if (grp_ctx->valid == 0) {
		ued_err("Invalid gid. (gid=%u)\n", gid);
		return -EINVAL;
	}

	if ((grp_ctx->event_bitmap & (0x1ULL << event->event_id)) == 0) {
		ued_err("Grp not subscribe event. (gid=%u; event_bitmap=%llx; event_id=%u)\n",
			gid, grp_ctx->event_bitmap, event->event_id);
		return -EINVAL;
	}

	event_t = kzalloc(sizeof(*event_t), GFP_KERNEL | GFP_ATOMIC | __GFP_ACCOUNT);
	if (event_t == NULL) {
		ued_err("Malloc event failed. (gid=%u; event_id=%u; msg_len=%u)\n", gid, event->event_id, event->msg_len);
		return -ENOMEM;
	}

	ret = ued_fill_event_info(event_t, event);
	if (ret != 0) {
		ued_err("Failed event failed. (gid=%u; event_id=%u; msg_len=%u)\n", gid, event->event_id, event->msg_len);
		kfree(event_t);
		return -ENOMEM;
	}

	ret = ued_event_enque(&grp_ctx->event_que, event_t);
	if (ret != 0) {
		atomic_inc(&grp_ctx->stat[event->event_id].full_drop_event_num);
		kfree(event_t);
		return -ENOSPC;
	}

	ued_wake_up_grp_thread(grp_ctx);

	atomic_inc(&grp_ctx->stat[event->event_id].publish_event_num);
	return 0;
}

static int ued_submit_event_to_subscriber(struct ued_dev_ctx *d_ctx,
	struct ued_event_subscriber *subscriber, struct ued_published_event *event)
{
	struct task_ctx *ctx = NULL;
	int ret;

	ctx = task_ctx_get(d_ctx->domain, subscriber->pid);
	if (ctx == NULL) {
		ued_err("Invalid pid. (pid=%d)\n", subscriber->pid);
		return -EINVAL;
	}

	ret = ued_submit_event_to_grp((struct ued_task_ctx *)task_ctx_priv(ctx), subscriber->gid, event);

	task_ctx_put(ctx);

	return ret;
}

static int _ued_submit_event_to_dev(struct ued_dev_ctx *d_ctx, struct ued_published_event *event)
{
	int i, ret = -EAGAIN;

	for (i = 0; i < UED_MAX_SUBSCRIBER; i++) {
		struct ued_event_subscriber *subscriber = &d_ctx->subscriber[event->event_id][i];

		if (subscriber->pid != 0) {
			ret = ued_submit_event_to_subscriber(d_ctx, subscriber, event);
			if (ret != 0) {
				break;
			}
		}
	}

	return ret;
}

int ued_submit_event_to_dev(u32 udevid, struct ued_published_event *event)
{
	struct array_ctx *ctx = NULL;
	int ret;

	if (event == NULL) {
		ued_err("Null event ptr. (devid=%u)\n", udevid);
		return -EINVAL;
	}

	if ((event->event_id >= UED_EVENT_MAX_NUM) || (event->msg_len > UED_MAX_EVENT_LEN)
		|| ((event->msg_len > 0) && (event->msg == NULL))) {
		ued_err("Invalid para. (devid=%u;event_id=%u;subevent_id=%u;msg_len=%u)\n", udevid,
			event->event_id, event->subevent_id, event->msg_len);
		return -EINVAL;
	}

	ctx = array_ctx_get(ued_dev_domain, udevid);
	if (ctx == NULL) {
		ued_err("Invalid dev. (devid=%u;event_id=%u;subevent_id=%u)\n", udevid, event->event_id, event->subevent_id);
		return -EINVAL;
	}

	ret = _ued_submit_event_to_dev((struct ued_dev_ctx *)array_ctx_priv(ctx), event);
	array_ctx_put(ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(ued_submit_event_to_dev);

int ued_core_publish_event(u32 udevid, struct ued_publish_event_para *para)
{
	int ret;
	struct ued_published_event event = {0};

	if (para == NULL) {
		ued_err("para is null. (devid=%u)\n", udevid);
		return -EINVAL;
	}
	if ((para->msg_len == 0) || (para->msg == NULL)) {
		ued_err("Invalid msg. (devid=%u; msg_len=%u; msg_is_null=%d)\n", udevid, para->msg_len, (para->msg == NULL));
		return -EINVAL;
	}
	event.event_id = para->event_id;
	event.subevent_id = para->subevent_id;
	event.msg_len = para->msg_len;
	event.msg = kzalloc(para->msg_len, GFP_KERNEL_ACCOUNT);
	if (IS_ERR_OR_NULL(event.msg)) {
		ued_err("kzalloc failed. (size=%u)\n", para->msg_len);
		return -ENOMEM;
	}
	if (copy_from_user(event.msg, (const void __user *)para->msg, para->msg_len)) {
		ued_err("event msg copy fail. (devid=%u; msg_len=%u)\n", udevid, para->msg_len);
		kfree(event.msg);
		event.msg = NULL;
		return -EFAULT;
	}

	ret = ued_submit_event_to_dev(udevid, &event);
	kfree(event.msg);
	event.msg = NULL;
	return ret;
}

static int ued_grp_subscribe_event(struct ued_task_ctx *t_ctx, u32 gid, u64 event_bitmap)
{
	struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[gid];

	if (grp_ctx->valid == 0) {
		ued_err("Invalid gid. (gid=%u)\n", gid);
		return -EINVAL;
	}

	if (grp_ctx->event_bitmap != 0) {
		ued_err("Repeat subscribe. (gid=%u; old_event_bitmap=%llx; new_event_bitmap=%llx)\n",
			gid, grp_ctx->event_bitmap, event_bitmap);
		return -EEXIST;
	}

	grp_ctx->event_bitmap = event_bitmap;

	return 0;
}

static void ued_grp_unsubscribe_event(struct ued_task_ctx *t_ctx, u32 gid)
{
	struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[gid];

	grp_ctx->event_bitmap = 0;
}

static int ued_task_subscribe_event(u32 devid, int pid, u32 gid, u64 event_bitmap)
{
	struct task_ctx *ctx = NULL;
	int ret;

	ctx = ued_task_ctx_get(devid, pid);
	if (ctx == NULL) {
		ued_err("Invalid device pid. (devid=%u; pid=%d)\n", devid, pid);
		return -ENODEV;
	}

	mutex_lock(&ctx->mutex);
	ret = ued_grp_subscribe_event((struct ued_task_ctx *)task_ctx_priv(ctx), gid, event_bitmap);
	mutex_unlock(&ctx->mutex);

	ued_task_ctx_put(ctx);

	return ret;
}

static void ued_task_unsubscribe_event(u32 devid, int pid, u32 gid)
{
	struct task_ctx *ctx = NULL;

	ctx = ued_task_ctx_get(devid, pid);
	if (ctx == NULL) {
		ued_err("Invalid device pid. (devid=%u; pid=%d)\n", devid, pid);
		return;
	}

	mutex_lock(&ctx->mutex);
	ued_grp_unsubscribe_event((struct ued_task_ctx *)task_ctx_priv(ctx), gid);
	mutex_unlock(&ctx->mutex);

	ued_task_ctx_put(ctx);
}

static int ued_dev_event_add_subscriber(u32 devid, int pid, u32 gid, u64 event_bitmap)
{
	struct array_ctx *ctx = NULL;
	int ret;

	ctx = array_ctx_get(ued_dev_domain, devid);
	if (ctx == NULL) {
		ued_err("Invalid dev. (devid=%u; pid=%d)\n", devid, pid);
		return -ENODEV;
	}

	ret = ued_event_add_subscriber((struct ued_dev_ctx *)array_ctx_priv(ctx), pid, gid, event_bitmap);
	array_ctx_put(ctx);

	return ret;
}

static void ued_dev_event_del_subscriber(u32 devid, int pid, u32 gid)
{
	struct array_ctx *ctx = NULL;

	ctx = array_ctx_get(ued_dev_domain, devid);
	if (ctx == NULL) {
		ued_err("Invalid dev. (devid=%u; pid=%d)\n", devid, pid);
		return;
	}

	ued_event_del_subscriber((struct ued_dev_ctx *)array_ctx_priv(ctx), pid, gid);
	array_ctx_put(ctx);
}

int ued_core_subscribe_event(u32 devid, int pid, u32 gid, u64 event_bitmap)
{
	int ret;

	if ((gid >= UED_MAX_GRP) || ((event_bitmap & (~((0x1ULL << UED_EVENT_MAX_NUM) - 1))) != 0) || (event_bitmap == 0)) {
		ued_err("Invalid para. (devid=%u; pid=%d; gid=%u; event_bitmap=%llx)\n", devid, pid, gid, event_bitmap);
		return -EINVAL;
	}

	ret = ued_task_subscribe_event(devid, pid, gid, event_bitmap);
	if (ret != 0) {
		return ret;
	}

	ret = ued_dev_event_add_subscriber(devid, pid, gid, event_bitmap);
	if (ret != 0) {
		ued_task_unsubscribe_event(devid, pid, gid);
		return ret;
	}

	return ret;
}

static int ued_wait_for_publish_event(struct ued_grp_ctx *grp_ctx, struct ued_thread_ctx *thread_ctx, int timeout)
{
	long ret;

	if (timeout == 0) {
		/* no wait or swap out */
		if (grp_ctx->valid == 0) {
			return -EOWNERDEAD;
		} else if (atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) {
			return 0;
		} else {
			return -ENOMSG;
		}
	}

	if (timeout > 0) {
		/* monitor process exit or thread ready, wait function will check the condition firstly */
		ret = wait_event_interruptible_timeout(thread_ctx->wq,
			((atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) || (grp_ctx->valid == 0)),
			(long)msecs_to_jiffies((u32)timeout));
		if (ret == 0) { /* timeout */
			return -ETIMEDOUT;
		} else if (ret > 0) {
			ret = 0;
		}
	} else {
		/* always wait */
		ret = wait_event_interruptible(thread_ctx->wq,
			((atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_READY) || (grp_ctx->valid == 0)));
	}

	if (ret == 0) {
		return (grp_ctx->valid == 0) ? -EOWNERDEAD : 0;
	}
	ret = (ret == -ERESTARTSYS) ? -EINTR : (int)ret;
	if (ret != -EINTR) {
		ued_err("Wait err. (gid=%u; tid=%u; ret=%ld; status=%d; valid=%d, timeout=%dms)\n",
			grp_ctx->gid, thread_ctx->tid, ret, atomic_read(&thread_ctx->status), grp_ctx->valid, timeout);
	}

	return ret;
}

static int ued_thread_obtain_event(struct ued_grp_ctx *grp_ctx, struct ued_thread_ctx *thread_ctx,
	struct ued_event *user_event)
{
	struct ued_event *event = NULL;
	int ret;

	event = ued_event_deque(&grp_ctx->event_que);
	if (event == NULL) {
		return -ENOMSG;
	}

	atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_RUN);
	thread_ctx->sched_event_num[event->event_id]++;
	atomic_inc(&grp_ctx->stat[event->event_id].sched_event_num);
	ret = (int)copy_to_user(user_event, event, sizeof(struct ued_event));
	if (ret != 0) {
		ued_err("Copy to user failed. (gid=%u; tid=%u; event_id=%u)\n", grp_ctx->gid, thread_ctx->tid, event->event_id);
		ret = -EINVAL;
	}

	kfree(event);
	return ret;
}

static int ued_thread_wait_event(struct ued_grp_ctx *grp_ctx, struct ued_thread_ctx *thread_ctx,
	int timeout, struct ued_event *user_event)
{
	int ret;

	if (thread_ctx->valid == 0) {
		thread_ctx->valid = 1; /* set thread valid */
	}

	if (atomic_read(&thread_ctx->status) == SCHED_THREAD_STATUS_RUN) {
		atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
		wmb(); /* Ensure the thread ctx status is written correctly. */
	}

again:
	if (ued_thread_obtain_event(grp_ctx, thread_ctx, user_event) == 0) {
		return 0;
	}

	ret = ued_wait_for_publish_event(grp_ctx, thread_ctx, timeout);
	if ((ret == 0) || (ret == -ETIMEDOUT)) { /* Timeout also attempts to search for more robustness */
		if (ued_thread_obtain_event(grp_ctx, thread_ctx, user_event) == 0) {
			return 0;
		}

		/*
		 * Scenario: The publishing thread first enters the queue and then schedule out. the consumer thread
		 * is already in the run state. The event in the queue is consumed then the thread state is set to idle,
		 * the publishing thread is schedule back, the thread state is set to ready wake up consumer. Then the
		 * consumer thread will find that the queue is empty
		 */
		if (ret == 0) {
			atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
			wmb(); /* Ensure the thread ctx status is written correctly. */
			goto again;
		}
	}

	return ret;
}

static int ued_task_wait_event(struct ued_task_ctx *t_ctx, struct ued_wait_event_para *para)
{
	struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[para->gid];
	struct ued_thread_ctx *thread_ctx = NULL;
	int ret = -EFAULT;

	if (grp_ctx->valid == 0) {
		ued_err("Grp ctx has not been created. (gid=%u)\n", para->gid);
		return E_RES_NOT_INIT;
	}

	if (grp_ctx->pid != current->pid) {
		ued_err("Grp ctx need to reinit. (gid=%u)\n", para->gid);
		return E_RES_NOT_INIT;
	}

	if (para->tid >= grp_ctx->thread_num) {
		ued_err("Invalid para. (gid=%u; tid=%u; thread_num=%u)\n", para->gid, para->tid, grp_ctx->thread_num);
		return -EINVAL;
	}

	thread_ctx = &grp_ctx->thread_ctx[para->tid];

	/* mutex protect multi thread use same tid */
	if (mutex_trylock(&thread_ctx->mutex) != 0) {   /* return 1 if success */
		ret = ued_thread_wait_event(grp_ctx, thread_ctx, para->timeout, para->event);
		mutex_unlock(&thread_ctx->mutex);
	}

	return ret;
}

int ued_core_wait_event(u32 devid, int pid, struct ued_wait_event_para *para)
{
	struct task_ctx *ctx = NULL;
	int ret;

	if ((para->gid >= UED_MAX_GRP) || (para->tid >= UED_MAX_THREAD)) {
		ued_err("Invalid para. (devid=%u; pid=%d; gid=%u; tid=%u)\n", devid, pid, para->gid, para->tid);
		return -EINVAL;
	}

	ctx = ued_task_ctx_get(devid, pid);
	if (ctx == NULL) {
		ued_err("Invalid dev task. (devid=%u; pid=%d)\n", devid, pid);
		return -ENODEV;
	}

	ret = ued_task_wait_event((struct ued_task_ctx *)task_ctx_priv(ctx), para);

	ued_task_ctx_put(ctx);

	return ret;
}

static void ued_event_que_init(struct ued_event_que *event_que)
{
	spin_lock_init(&event_que->lock);
	event_que->head = 0;
	event_que->tail = 0;
	event_que->depth = UED_EVENT_QUE_DEPTH;
}

static void ued_thread_ctx_init(struct ued_thread_ctx *thread_ctx, u32 tid)
{
	thread_ctx->tid = tid;
	mutex_init(&thread_ctx->mutex);
	atomic_set(&thread_ctx->status, SCHED_THREAD_STATUS_IDLE);
	init_waitqueue_head(&thread_ctx->wq);
}

static void ued_grp_ctx_init(struct ued_grp_ctx *grp_ctx, u32 gid)
{
	u32 i;

	grp_ctx->gid = gid;
	ued_event_que_init(&grp_ctx->event_que);
	for (i = 0; i < UED_MAX_THREAD; i++) {
		ued_thread_ctx_init(&grp_ctx->thread_ctx[i], i);
	}
}

static void ued_clear_grp_stat(struct ued_grp_ctx *grp_ctx)
{
	int i, j;

	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		atomic_set(&grp_ctx->stat[i].publish_event_num, 0);
		atomic_set(&grp_ctx->stat[i].sched_event_num, 0);
		atomic_set(&grp_ctx->stat[i].full_drop_event_num, 0);
		for (j = 0; j < UED_MAX_THREAD; j++) {
			grp_ctx->thread_ctx[j].sched_event_num[i] = 0;
		}
	}
}

static int ued_task_add_grp(struct ued_task_ctx *t_ctx, char *grp_name, u32 thread_num, u32 *gid)
{
	int i;

	if (strnlen(grp_name, EVENT_MAX_GRP_NAME_LEN) >= EVENT_MAX_GRP_NAME_LEN) {
		ued_err("Invalid too long grp_name");
		return -EINVAL;
	}

	for (i = 0; i < UED_MAX_GRP; i++) {
		struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[i];

		if (grp_ctx->valid == 0) {
			/* grp reuse should clear the previous stat */
			ued_clear_grp_stat(grp_ctx);

			/* grp name just use to debug, so not care return value */
			(void)ubfm_strcpy(grp_ctx->name, EVENT_MAX_GRP_NAME_LEN, grp_name);

			grp_ctx->thread_num = thread_num;
			grp_ctx->pid = current->pid;
			wmb();/* Ensure the grp ctx pid is written correctly. */
			grp_ctx->valid = 1;
			*gid = i;
			return 0;
		}
	}

	return -ENOSPC;
}

static int ued_task_del_grp(struct ued_task_ctx *t_ctx, u32 gid)
{
	struct ued_grp_ctx *grp_ctx = &t_ctx->grp_ctx[gid];

	if (grp_ctx->valid == 0) {
		ued_err("Invalid gid. (gid=%u)\n", gid);
		return -EINVAL;
	}

	grp_ctx->valid = 0;
	wmb(); /* Ensure the grp_ctx valid is written correctly. */
	ued_grp_unsubscribe_event(t_ctx, gid);
	ued_wake_up_grp_all_thread(grp_ctx);
	ued_clear_event_que(&grp_ctx->event_que);

	return 0;
}

int ued_core_create_grp(u32 devid, int pid, char *grp_name, u32 thread_num, u32 *gid)
{
	struct task_ctx *ctx = NULL;
	int ret;

	if (thread_num >= UED_MAX_THREAD) {
		ued_err("Invalid para. (devid=%u; grp_name=%s; thread_num=%u)\n", devid, grp_name, thread_num);
		return -EINVAL;
	}

	ctx = ued_task_ctx_get(devid, pid);
	if (ctx == NULL) {
		ued_err("Invalid dev task. (devid=%u; pid=%d)\n", devid, pid);
		return -ENODEV;
	}

	mutex_lock(&ctx->mutex);
	ret = ued_task_add_grp((struct ued_task_ctx *)task_ctx_priv(ctx), grp_name, thread_num, gid);
	mutex_unlock(&ctx->mutex);

	ued_task_ctx_put(ctx);

	return ret;
}

int ued_core_destroy_grp(u32 devid, int pid, u32 gid)
{
	struct task_ctx *ctx = NULL;
	int ret;

	if (gid >= UED_MAX_GRP) {
		ued_err("Invalid gid. (devid=%u; pid=%d; gid=%u)\n", devid, pid, gid);
		return -EINVAL;
	}

	ctx = ued_task_ctx_get(devid, pid);
	if (ctx == NULL) {
		ued_err("Invalid dev task. (devid=%u; pid=%d)\n", devid, pid);
		return -ENODEV;
	}

	ued_dev_event_del_subscriber(devid, pid, gid);

	mutex_lock(&ctx->mutex);
	ret = ued_task_del_grp((struct ued_task_ctx *)task_ctx_priv(ctx), gid);
	mutex_unlock(&ctx->mutex);

	ued_task_ctx_put(ctx);

	return ret;
}

static void ued_show_thread_ctx(struct ued_thread_ctx *thread_ctx, struct seq_file *seq)
{
	int i;

	seq_printf(seq, "            tid %u status %d\n", thread_ctx->tid, atomic_read(&thread_ctx->status));
	seq_puts(seq, "                stat: eventid  sched\n");
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		if (thread_ctx->sched_event_num[i] != 0) {
			seq_printf(seq, "                   %8d  %8u\n", i, thread_ctx->sched_event_num[i]);
		}
	}
}

static void ued_show_grp_ctx(struct ued_grp_ctx *grp_ctx, struct seq_file *seq)
{
	u32 i;

	seq_printf(seq, "        gid %u valid %d name %s thread_num %u event_bitmap %llx\n",
		grp_ctx->gid, grp_ctx->valid, grp_ctx->name, grp_ctx->thread_num, grp_ctx->event_bitmap);
	seq_printf(seq, "            event_que: depth %u head %u tail %u\n",
		grp_ctx->event_que.depth, grp_ctx->event_que.head, grp_ctx->event_que.tail);

	seq_puts(seq, "            stat: eventid  publish  sched  full_drop\n");
	for (i = 0; i < UED_EVENT_MAX_NUM; i++) {
		seq_printf(seq, "                 %8u  %8d  %8d  %8d\n",
			i, atomic_read(&grp_ctx->stat[i].publish_event_num), atomic_read(&grp_ctx->stat[i].sched_event_num),
			atomic_read(&grp_ctx->stat[i].full_drop_event_num));
	}

	for (i = 0; i < grp_ctx->thread_num; i++) {
		ued_show_thread_ctx(&grp_ctx->thread_ctx[i], seq);
	}
}

static void ued_show_task_ctx(struct ued_task_ctx *t_ctx, struct seq_file *seq)
{
	int i;

	seq_printf(seq, "    pid %d task_id %u valid %u\n", t_ctx->pid, t_ctx->task_id, t_ctx->valid);

	for (i = 0; i < UED_MAX_GRP; i++) {
		if (t_ctx->grp_ctx[i].valid == 1) {
			ued_show_grp_ctx(&t_ctx->grp_ctx[i], seq);
		}
	}
}

void ued_show_task(u32 devid, int pid, struct seq_file *seq)
{
	struct task_ctx *ctx = ued_task_ctx_get(devid, pid);

	if (ctx != NULL) {
		task_ctx_item_show(ctx, seq);
		ued_show_task_ctx((struct ued_task_ctx *)task_ctx_priv(ctx), seq);
		ued_task_ctx_put(ctx);
	}
}

static void ued_task_ctx_init(struct ued_task_ctx *t_ctx)
{
	u32 i;

	for (i = 0; i < UED_MAX_GRP; i++) {
		ued_grp_ctx_init(&t_ctx->grp_ctx[i], i);
	}

	wmb(); /* Ensure the grp ctx status is written correctly. */
	t_ctx->valid = 1;
}

static void ued_task_ctx_uninit(struct ued_task_ctx *t_ctx)
{
	int i;

	t_ctx->valid = 0;
	wmb(); /* Ensure the t_ctx valid is written correctly. */

	for (i = 0; i < UED_MAX_GRP; i++) {
		if (t_ctx->grp_ctx[i].valid == 1) {
			ued_dev_event_del_subscriber(t_ctx->devid, t_ctx->pid, i);
			ued_task_del_grp(t_ctx, i);
		}
	}
}

static void ued_task_ctx_release(struct task_ctx *ctx)
{
	struct ued_task_ctx *t_ctx = (struct ued_task_ctx *)task_ctx_priv(ctx);
	struct array_ctx *d_ctx;

	ued_info("Release success. (devid=%u; pid=%d)\n", t_ctx->devid, t_ctx->pid);

	/* If dev_ctx still exists, this is the normal del_task path: task_entry
	 * hasn't been freed, so proc_remove(task_entry) is safe.
	 * If dev_ctx is gone (del_dev completed), proc_remove(dev_entry) has
	 * already recursively freed task_entry — calling proc_remove again
	 * would be use-after-free.
	 */
	d_ctx = array_ctx_get(ued_dev_domain, t_ctx->devid);
	if (d_ctx != NULL) {
		ued_proc_fs_del_task(t_ctx->entry);
		array_ctx_put(d_ctx);
	}

	ued_task_ctx_uninit(t_ctx);
	vfree(t_ctx);
}

static int ued_task_ctx_create(struct ued_dev_ctx *d_ctx, int pid)
{
	struct ued_task_ctx *t_ctx = NULL;
	int ret;

	t_ctx = vzalloc(sizeof(*t_ctx));
	if (t_ctx == NULL) {
		ued_err("Vmalloc ctx fail. (size=%zu)\n", sizeof(*t_ctx));
		return -ENOMEM;
	}

	t_ctx->pid = pid;
	t_ctx->task_id = d_ctx->cur_task_id++;
	t_ctx->devid = d_ctx->devid;
	ued_task_ctx_init(t_ctx);

	ret = task_ctx_create(d_ctx->domain, pid, t_ctx, ued_task_ctx_release);
	if (ret != 0) {
		ued_err("Create fail. (ret=%d; pid=%d)\n", ret, pid);
		vfree(t_ctx);
		return ret;
	}

	t_ctx->entry = ued_proc_fs_add_task(d_ctx->entry, t_ctx->devid, t_ctx->pid, t_ctx->task_id);

	ued_info("Create success. (devid=%u; pid=%d)\n", d_ctx->devid, pid);

	return 0;
}

static int ued_dev_add_task(struct ued_dev_ctx *d_ctx, int pid)
{
	struct task_ctx_domain *domain = d_ctx->domain;
	struct task_ctx *ctx = NULL;
	int ret;

	mutex_lock(&domain->mutex);
	ctx = task_ctx_get(domain, pid);
	if (ctx == NULL) {
		ret = ued_task_ctx_create(d_ctx, pid);
		if (ret != 0) {
			mutex_unlock(&domain->mutex);
			return ret;
		}
	} else {
		task_ctx_put(ctx);
	}
	mutex_unlock(&domain->mutex);

	return 0;
}

int ued_core_add_task(u32 devid, int pid)
{
	struct array_ctx *ctx = NULL;
	int ret;

	ctx = array_ctx_get(ued_dev_domain, devid);
	if (ctx == NULL) {
		ued_err("Invalid dev. (devid=%u; pid=%d)\n", devid, pid);
		return -EINVAL;
	}

	ret = ued_dev_add_task((struct ued_dev_ctx *)array_ctx_priv(ctx), pid);
	array_ctx_put(ctx);

	return ret;
}

static int ued_dev_del_task(struct ued_dev_ctx *d_ctx, int pid)
{
	struct task_ctx_domain *domain = d_ctx->domain;
	struct task_ctx *ctx = NULL;

	mutex_lock(&domain->mutex);
	ctx = ued_task_ctx_get(d_ctx->devid, pid);
	if (ctx == NULL) {
		mutex_unlock(&domain->mutex);
		ued_info("Ctx already release. (devid=%u; pid=%d)\n", d_ctx->devid, pid);
		return 0;
	}

	ued_task_ctx_uninit((struct ued_task_ctx *)task_ctx_priv(ctx));

	ued_task_ctx_put(ctx); /* ued_task_ctx_get */
	task_ctx_destroy(ctx); /* task_ctx_create */
	mutex_unlock(&domain->mutex);

	return 0;
}

int ued_core_del_task(u32 devid, int pid)
{
	struct array_ctx *ctx = NULL;
	int ret;

	ctx = array_ctx_get(ued_dev_domain, devid);
	if (ctx == NULL) {
		return -EINVAL;
	}

	ret = ued_dev_del_task((struct ued_dev_ctx *)array_ctx_priv(ctx), pid);
	array_ctx_put(ctx);

	return ret;
}

static void ued_task_ctx_recycle(struct task_ctx *ctx, void *priv)
{
	ued_warn("Recycle task. (name=%s; pid=%d)\n", ctx->name, ctx->tgid);
	ued_task_ctx_uninit((struct ued_task_ctx *)task_ctx_priv(ctx));
	task_ctx_destroy(ctx); /* task_ctx_create */
}

static void ued_dev_ctx_release(struct array_ctx *ctx)
{
	struct ued_dev_ctx *d_ctx = (struct ued_dev_ctx *)array_ctx_priv(ctx);

	ued_proc_fs_del_dev(d_ctx->entry);
	task_ctx_domain_destroy(d_ctx->domain);
	vfree(d_ctx);
}

int ued_core_add_dev(u32 devid)
{
	struct ued_dev_ctx *d_ctx = NULL;
	static u32 inst;
	int ret;

	d_ctx = vzalloc(sizeof(*d_ctx));
	if (d_ctx == NULL) {
		ued_err("Vmalloc dev ctx fail. (size=%lu)\n", sizeof(*d_ctx));
		return -ENOMEM;
	}

	d_ctx->devid = devid;
	mutex_init(&d_ctx->subscriber_mutex);
	d_ctx->domain = task_ctx_domain_create("ued_dev", UED_MAX_TASK);
	if (d_ctx->domain == NULL) {
		mutex_destroy(&d_ctx->subscriber_mutex);
		vfree(d_ctx);
		ued_err("Create task domain fail. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	ret = array_ctx_create(ued_dev_domain, devid, d_ctx, ued_dev_ctx_release);
	if (ret != 0) {
		task_ctx_domain_destroy(d_ctx->domain);
		mutex_destroy(&d_ctx->subscriber_mutex);
		vfree(d_ctx);
		ued_err("Create fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	d_ctx->entry = ued_proc_fs_add_dev(devid, inst++);

	return 0;
}

int ued_core_del_dev(u32 devid)
{
	struct array_ctx *ctx = array_ctx_get(ued_dev_domain, devid);
	struct ued_dev_ctx *d_ctx;
	struct task_ctx_domain *domain;

	if (ctx == NULL) {
		return -EINVAL;
	}

	d_ctx = array_ctx_priv(ctx);
	domain = d_ctx->domain;
	mutex_lock(&domain->mutex);
	task_ctx_for_each_safe(domain, NULL, ued_task_ctx_recycle);
	mutex_unlock(&domain->mutex);

	array_ctx_put(ctx); /* array_ctx_get */
	array_ctx_destroy(ctx); /* array_ctx_create */

	return 0;
}

static void ued_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
	ued_warn("Recycle dev ctx. (devid=%u)\n", ctx->id);
	task_ctx_for_each_safe(((struct ued_dev_ctx *)array_ctx_priv(ctx))->domain, NULL, ued_task_ctx_recycle);
	array_ctx_destroy(ctx); /* array_ctx_create */
}

int ued_core_init(void)
{
	ued_dev_domain = array_ctx_domain_create("ued_dev", DEV_MAX_NUM + 1); /* +1 for UB any devid */
	if (ued_dev_domain == NULL) {
		ued_err("Create dev domain fail.\n");
		return -ENOMEM;
	}

	return 0;
}

void ued_core_uninit(void)
{
	array_ctx_for_each_safe(ued_dev_domain, NULL, ued_dev_ctx_recycle);
	array_ctx_domain_destroy(ued_dev_domain);
	ued_dev_domain = NULL;
}

