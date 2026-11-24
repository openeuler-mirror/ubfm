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
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "mami_string.h"

#include "udm.h"
#include "faultmgr_log.h"
#include "faultmgr_queue.h"

static DEFINE_MUTEX(g_faultmgr_lock);
static struct faultmgr_queue *g_faultmgr_queue;
static u32 g_faultmgr_current_serialnum;
static atomic_t device_num = ATOMIC_INIT(0);

static void faultmgr_queue_set_current_serialnum(u32 serial_num)
{
	g_faultmgr_current_serialnum = serial_num;
}

u32 faultmgr_queue_get_current_serialnum(void)
{
	return g_faultmgr_current_serialnum;
}

bool faultmgr_queue_is_newest(u32 serial_num)
{
	u32 current_serialnum = g_faultmgr_current_serialnum;

	return (current_serialnum == serial_num);
}

static void faultmgr_lock(void)
{
	mutex_lock(&g_faultmgr_lock);
}

static void faultmgr_unlock(void)
{
	mutex_unlock(&g_faultmgr_lock);
}

STATIC struct faultmgr_queue *faultmgr_get_queue(void)
{
	return g_faultmgr_queue;
}

static struct faultmgr_queue *faultmgr_set_queue(struct faultmgr_queue *queue)
{
	return g_faultmgr_queue = queue;
}

void faultmgr_queue_online_dev_inc(void)
{
	atomic_inc(&device_num);
}

void faultmgr_queue_online_dev_dec(void)
{
	atomic_dec(&device_num);
}

static int faultmgr_get_devnum(void)
{
	return atomic_read(&device_num);
}

static int faultmgr_realloc_mem(u32 devnum)
{
	struct faultmgr_queue *current_queue = NULL;
	struct faultmgr_queue *queue = NULL;
	u32 capacity, end, count;

	current_queue = faultmgr_get_queue();
	if (current_queue != NULL && devnum <= current_queue->max_devnum) {
		return 0;
	}

	capacity = devnum << FAULTMGR_CAPACITY_SHIFT_PER_DEV;
	queue = kvmalloc(sizeof(struct faultmgr_queue) + capacity * sizeof(struct mami_fault_event), GFP_KERNEL_ACCOUNT);
	if (queue == NULL) {
		faultmgr_err("Realloc queue mem failed. (devnum=%u)\n", devnum);
		return -ENOMEM;
	}

	queue->max_devnum = devnum;
	queue->capacity = capacity;
	queue->mask = capacity - 1;
	if (current_queue == NULL || current_queue->is_empty != 0) {
		queue->is_empty = 1;
		queue->tail = 0;
		queue->head = 0;
		goto out;
	}

	queue->is_empty = current_queue->is_empty;
	if (current_queue->tail <= current_queue->head) {
		(void)ubfm_memcpy(&queue->data[0], capacity * sizeof(struct mami_fault_event),
			&current_queue->data[current_queue->tail],
			(current_queue->head - current_queue->tail + 1) * sizeof(struct mami_fault_event));
		queue->tail = 0;
		queue->head = current_queue->head - current_queue->tail;
		goto out;
	} else {
		end = current_queue->capacity - 1;
		count = end - current_queue->tail + 1;
		(void)ubfm_memcpy(&queue->data[0], capacity * sizeof(struct mami_fault_event),
			&current_queue->data[current_queue->tail], count * sizeof(struct mami_fault_event));
		(void)ubfm_memcpy(&queue->data[count], (capacity - count) * sizeof(struct mami_fault_event),
			&current_queue->data[0], (current_queue->head + 1) * sizeof(struct mami_fault_event));

		queue->tail = 0;
		queue->head = count + current_queue->head;
	}

out:
	faultmgr_set_queue(queue);
	kvfree(current_queue);
	return 0;
}

int faultmgr_queue_init(void)
{
	int ret;

	faultmgr_lock();
	ret = faultmgr_realloc_mem(1);
	if (ret != 0) {
		faultmgr_err("Alloc faultmgr mem failed. (ret=%d)\n", ret);
	}
	faultmgr_unlock();
	return ret;
}

int faultmgr_queue_reinit(u32 devid)
{
	int ret;
	int devnum;

	faultmgr_lock();
	devnum = faultmgr_get_devnum();
	if (devnum <= 0) {
		faultmgr_err("Invalid dev num. (devid=%u; devnum=%d)\n", devid, devnum);
		faultmgr_unlock();
		return -EINVAL;
	}

	ret = faultmgr_realloc_mem(devnum);
	if (ret != 0) {
		faultmgr_err("Realloc faultmgr mem failed. (devid=%u; devnum=%d; ret=%d)\n", devid, devnum, ret);
	}

	faultmgr_unlock();
	return ret;
}

void faultmgr_queue_uninit(void)
{
	struct faultmgr_queue *current_queue = NULL;

	faultmgr_lock();
	current_queue = faultmgr_get_queue();
	if (current_queue != NULL) {
		kvfree(current_queue);
		faultmgr_set_queue(NULL);
	}
	faultmgr_unlock();
}

static int faultmgr_queue_do_add_event(struct mami_fault_event *event)
{
	struct faultmgr_queue *current_queue = NULL;
	u32 current_serialnum;

	current_queue = faultmgr_get_queue();
	if (current_queue == NULL) {
		faultmgr_err("Faultmgr queue is not initialized.\n");
		return -EINVAL;
	}

	if (current_queue->is_empty != 0) {
		current_queue->tail = 0;
		current_queue->head = 0;
		current_queue->is_empty = 0;
		(void)ubfm_memcpy(&current_queue->data[0], sizeof(struct mami_fault_event), event,
			sizeof(struct mami_fault_event));
		/* the first event serial number is 1, the user will initially pass serial number 0, so that this event can be obtained */
		current_queue->data[0].eventSerialNum = 1;
		faultmgr_queue_set_current_serialnum(current_queue->data[0].eventSerialNum);
		return 0;
	}

	current_serialnum = current_queue->data[current_queue->head].eventSerialNum;
	current_queue->head = (current_queue->head + 1) % current_queue->capacity;
	if (current_queue->head == current_queue->tail) {
		current_queue->tail = (current_queue->tail + 1) % current_queue->capacity;
	}

	(void)ubfm_memcpy(&current_queue->data[current_queue->head], sizeof(struct mami_fault_event), event,
		sizeof(struct mami_fault_event));
	current_queue->data[current_queue->head].eventSerialNum = (current_serialnum + 1) & FAULTMGR_SERIALNUM_MASK;
	faultmgr_queue_set_current_serialnum(current_queue->data[current_queue->head].eventSerialNum);
	return 0;
}

int faultmgr_queue_add_event(struct mami_fault_event *event)
{
	int ret;

	if (event == NULL) {
		faultmgr_err("Invalid fault event.\n");
		return -EINVAL;
	}

	faultmgr_lock();
	ret = faultmgr_queue_do_add_event(event);
	faultmgr_unlock();
	return ret;
}

static int faultmgr_queue_get_latest_event(struct faultmgr_queue *current_queue, struct mami_read_event_param *para)
{
	u32 i, queue_idx, serialnum;
	int ret;

	for (i = 0; i < para->inEventNum; i++) {
		queue_idx = (current_queue->tail + i) % current_queue->capacity;
		/* para->event is userspace memory */
		ret = (int)copy_to_user((void *)(para->event + i), &current_queue->data[queue_idx],
			sizeof(struct mami_fault_event));
		if (ret != 0) {
			faultmgr_err("Copy to user failed. (ret=%d)\n", ret);
			return ret;
		}
		serialnum = current_queue->data[queue_idx].eventSerialNum;

		para->outEventNum++;
		if (queue_idx == current_queue->head) {
			break;
		}
	}

	if (para->outEventNum != 0) {
		para->eventSerialNum = serialnum;
	}

	return 0;
}

static int faultmgr_queue_get_event_(struct faultmgr_queue *current_queue, struct mami_read_event_param *para)
{
	u32 i = 0, queue_idx, serialnum, serialnumt;
	int ret;

	serialnumt = current_queue->data[current_queue->tail].eventSerialNum;
	if (para->eventSerialNum >= serialnumt) {
		queue_idx = ((para->eventSerialNum - serialnumt) + current_queue->tail) % current_queue->capacity;
	} else {
		queue_idx = ((para->eventSerialNum + FAULTMGR_SERIALNUM_MASK - serialnumt) + current_queue->tail) % current_queue->capacity;
	}

	for (i = 0; i < para->inEventNum; i++) {
		if (queue_idx == current_queue->head) {
			break;
		}

		queue_idx = (queue_idx + 1) % current_queue->capacity;
		/* para->event is userspace memory */
		ret = (int)copy_to_user((void *)(para->event + i), &current_queue->data[queue_idx],
			sizeof(struct mami_fault_event));
		if (ret != 0) {
			faultmgr_err("Copy to user failed. (ret=%d)\n", ret);
			return ret;
		}
		serialnum = current_queue->data[queue_idx].eventSerialNum;

		para->outEventNum++;
	}

	if (para->outEventNum != 0) {
		para->eventSerialNum = serialnum;
	}

	return 0;
}

int faultmgr_queue_get_event(struct mami_read_event_param *para)
{
	struct faultmgr_queue *current_queue = NULL;
	u32 serialnumh, serialnumt;
	int ret;

	if (para == NULL) {
		faultmgr_err("Invalid fault para.\n");
		return -EINVAL;
	}

	faultmgr_lock();
	current_queue = faultmgr_get_queue();
	if (current_queue == NULL) {
		faultmgr_unlock();
		faultmgr_err("Faultmgr queue is not initialized.\n");
		return -EINVAL;
	}

	if (current_queue->is_empty != 0) {
		faultmgr_unlock();
		para->outEventNum = 0;
		return 0;
	}

	if (para->eventSerialNum == MAMI_EVENT_SERIALNUM_LATEST) {
		ret = faultmgr_queue_get_latest_event(current_queue, para);
		faultmgr_unlock();
		return ret;
	} else if (para->eventSerialNum > FAULTMGR_SERIALNUM_MASK) {
		faultmgr_unlock();
		faultmgr_err("Invalid param. (serialNum=0x%x)\n", para->eventSerialNum);
		return -EINVAL;
	}

	serialnumh = current_queue->data[current_queue->head].eventSerialNum;
	serialnumt = current_queue->data[current_queue->tail].eventSerialNum;
	if ((serialnumt <= serialnumh && para->eventSerialNum >= serialnumt && para->eventSerialNum <= serialnumh) ||
		(serialnumt > serialnumh && (para->eventSerialNum >= serialnumt || para->eventSerialNum <= serialnumh))) {
		ret = faultmgr_queue_get_event_(current_queue, para);
		faultmgr_unlock();
		return ret;
	}

	ret = faultmgr_queue_get_latest_event(current_queue, para);
	faultmgr_unlock();
	return ret;
}
