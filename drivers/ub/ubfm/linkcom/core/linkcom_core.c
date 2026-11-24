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
#include <linux/delay.h>
#include <linux/pid.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#endif

#include "udm.h"
#include "idm.h"

#include "linkcom.h"
#include "linkcom_log.h"
#include "linkcom_pub.h"
#include "linkcom_mng.h"
#include "linkcom_soc_adapt.h"
#include "linkcom_core.h"


static int linkcom_get_pid_start_time(u32 pid, TASK_TIME_TYPE *time)
{
#ifndef EMU_ST
	struct task_struct *task = NULL;

	rcu_read_lock();
	task = pid_task(find_vpid((pid_t)pid), PIDTYPE_TGID);
	if (task == NULL) {
		rcu_read_unlock();
		linkcom_err("fail to get process start time by pid. (pid=%u)\n", pid);
		return -ESRCH;
	}

	*time = READ_ONCE(task->start_time);
	rcu_read_unlock();
#endif
	return 0;
}

static bool linkcom_is_process_exit(u32 tgid, TASK_TIME_TYPE start_time)
{
	struct task_struct *tsk = NULL;
	struct pid *pid;

#ifndef EMU_ST
	rcu_read_lock();
	pid = get_pid(find_pid_ns(tgid, &init_pid_ns));
	rcu_read_unlock();
	if (pid == NULL) {
		return true;
	}
	tsk = get_pid_task(pid, PIDTYPE_PID);
	put_pid(pid);
	if (tsk == NULL) {
		return true;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	if (start_time == tsk->start_time) {
		put_task_struct(tsk);
		return false;
	}
#else
	if (timespec_equal(&start_time, &tsk->start_time)) {
		put_task_struct(tsk);
		return false;
	}
#endif
	put_task_struct(tsk);
	return true;
#else
	return false;
#endif
}

int linkcom_core_open_link_pkt(u32 dev_id, u32 chan_id, u32 pid, int *handle)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	u32 chan_num = 0;
	int ret = 0;
	TASK_TIME_TYPE pid_start_time = 0;

	ret = linkcom_get_pid_start_time(pid, &pid_start_time);
	if (ret != 0) {
		return ret;
	}

	if (handle == NULL) {
		linkcom_err("handle is NULL. (devid=%u; chan_id=%u)\n", dev_id, chan_id);
		return -EINVAL;
	}

	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		return -EINVAL;
	}
	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("get inst failed. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		*handle = INVALID_LINKCOM_HANDLE;
		return -EINVAL;
	}
	chan_inst = &dev_inst->chan_inst[chan_id];

	mutex_lock(&chan_inst->lock);

	if (chan_inst->occupy_status == FUNC_IDLE_STS) {
		chan_inst->app_pid = pid;
		chan_inst->time = pid_start_time;
		chan_inst->occupy_status = FUNC_OCCUPIED_STS;
		*handle = MASK_LINKCOM_HANDLE(dev_id, chan_id);
		linkcom_info("open link channel success. (dev_id=%u; chan_id=%u; pid=%u)\n", dev_id, chan_id, pid);
	} else if (linkcom_is_process_exit(chan_inst->app_pid, chan_inst->time)) {
		chan_inst->app_pid = pid;
		chan_inst->time = pid_start_time;
		chan_inst->occupy_status = FUNC_OCCUPIED_STS;
		*handle = MASK_LINKCOM_HANDLE(dev_id, chan_id);
		linkcom_info("reopen link channel success. (dev_id=%u; chan_id=%u; pid=%u)\n", dev_id, chan_id, pid);
	} else {
		*handle = INVALID_LINKCOM_HANDLE;
		linkcom_err("chan has been occupied. (dev_id=%u; chan_id=%u; pid=%u)\n", dev_id, chan_id, chan_inst->app_pid);
		ret = -EINVAL;
	}

	mutex_unlock(&chan_inst->lock);
	linkcom_put_dev_inst(dev_id);

	return ret;
}

int linkcom_core_close_link_pkt(u32 dev_id, u32 chan_id, u32 pid)
{
	int ret;
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	u32 chan_num = 0;
	TASK_TIME_TYPE time = 0;

	ret = linkcom_get_pid_start_time(pid, &time);
	if (ret != 0) {
		return ret;
	}
	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		return -EINVAL;
	}
	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("get inst failed. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}
	chan_inst = &dev_inst->chan_inst[chan_id];

	mutex_lock(&chan_inst->lock);
	if (chan_inst->app_pid != pid || chan_inst->time != time) {
		mutex_unlock(&chan_inst->lock);
		linkcom_put_dev_inst(dev_id);
		linkcom_err("pid=%u time=%lluns is not owner pid(pid=%u, time=%lluns)\n", pid, time, chan_inst->app_pid, chan_inst->time);
		return -EINVAL;
	}
	linkcom_core_chan_eventctx_recycle(chan_inst);

	chan_inst->app_pid = 0;
	chan_inst->time = 0;
	mutex_unlock(&chan_inst->lock);

	linkcom_core_release_lst_rsrc(chan_inst);
	linkcom_put_dev_inst(dev_id);

	linkcom_info("close link channel success, dev_id[%u] chan_id[%u]\n", dev_id, chan_id);

	return 0;
}
int linkcom_check_channel_owner(struct linkcom_chan_inst *chan_inst)
{
	TASK_TIME_TYPE time = 0;
	u32 pid = 0;

	pid = (u32)task_tgid_nr(current);
	time = (TASK_TIME_TYPE)READ_ONCE(current->group_leader->start_time);
	if ((chan_inst->app_pid != pid) || (chan_inst->time != time)) {
		linkcom_err("Process is not owner of channel. (app_pid=%u; app_time=%lluns; chan_pid=%u; chan_time=%lluns)\n",
			pid, time, chan_inst->app_pid, chan_inst->time);
		return -EINVAL;
	}

	return 0;
}

int linkcom_core_send_link_pkt_inner(struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst, struct mami_pkt_send_para para)
{
	struct linkcom_id_inst id_inst = {0};
	int ret = 0;

	linkcom_fill_id_inst(&id_inst, dev_inst, chan_inst);

	mutex_lock(&chan_inst->lock);
	ret = linkcom_check_channel_owner(chan_inst);
	if (ret != 0) {
		linkcom_err("Check channel owner failed. (devid=%u; chanid=%u)\n", dev_inst->dev_id, chan_inst->chan_id);
		mutex_unlock(&chan_inst->lock);
		return ret;
	}
	ret = linkcom_inband_send_link_pkt(&id_inst, para);
	mutex_unlock(&chan_inst->lock);

	return ret;
}

int linkcom_core_bond_eventfd_inner(u32 dev_id, struct linkcom_open_chan_in *kin, struct linkcom_chan_inst *chan_inst)
{
	struct eventfd_ctx *ctx = NULL;
	u32 chan_id = kin->chan_id;
	int ret = 0;

	/* supports eventfd reporting */
	if (kin->eventfd >= 0) {
		/* get eventfd kernel context */
		ctx = eventfd_ctx_fdget(kin->eventfd);
		if (IS_ERR(ctx)) {
			chan_inst->eventfd = -1;
			chan_inst->eventctx = NULL;
			ret = PTR_ERR(ctx);
			linkcom_err("Failed to get eventfd context. (devid=%u; chan_id=%u; eventfd=%d; ret=%d)\n",
				dev_id, chan_id, kin->eventfd, ret);
			return ret;
		}
		chan_inst->eventfd = kin->eventfd;
		chan_inst->eventctx = ctx;
		linkcom_info("Bond eventfd context success. (devid=%u; chan_id=%u; eventfd=%d; ret=%d)\n",
			dev_id, chan_id, kin->eventfd, ret);
		if (chan_inst->head.count > 0) {
			linkcom_event_signal(chan_inst, chan_inst->head.count);
			linkcom_info("Linkcom eventfd force wakeup. (devid=%u; chan_id=%u; eventfd=%d; pkt_num=%u)\n",
				dev_id, chan_id, kin->eventfd, chan_inst->head.count);
		}
	} else {
		if (kin->eventfd != -1) {
			ret = -EINVAL;
			linkcom_err("Invalid eventfd. (devid=%u; chan_id=%u; eventfd=%d; ret=%d)\n",
				dev_id, chan_id, kin->eventfd, ret);
			return ret;
		}
		/* scenario without using eventfd */
		chan_inst->eventfd = -1;
		chan_inst->eventctx = NULL;
	}

	return 0;
}

int linkcom_core_check_channel_send_ability(const struct linkcom_dev_inst *dev_inst, u32 chan_id)
{
	/* Every die has 4 queues and queue0 is borrowed by IMP in Ascend_v1 */
	if ((dev_inst->udie_version == UDIE_VERSION_V1) && (chan_id % LINKCOM_DIE_MAX_CHAN_NUM == 0)) {
		linkcom_err("Channel couldn't be used to send. (devid=%u; chan_id=%u)\n", dev_inst->dev_id, chan_id);
		return -EINVAL;
	}
	return 0;
}

int linkcom_core_send_link_pkt(u32 dev_id, u32 chan_id, struct mami_pkt_send_para para)
{
	struct linkcom_chan_inst *chan_inst = NULL;
	struct linkcom_dev_inst *dev_inst = NULL;
	u32 chan_num = 0;
	int ret = 0;

	if (para.pkt == NULL) {
		linkcom_err("Input parameter is error. (dev_id=%u; chan_id=%u)\n", dev_id, chan_id);
		return -EINVAL;
	}

	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		return -EINVAL;
	}

	ret = linkcom_core_check_channel_send_ability(dev_inst, chan_id);
	if (ret != 0) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("Check channel failed. (dev_id[%u]; chan_id[%u])\n", dev_id, chan_id);
		return -EINVAL;
	}

	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("get inst failed. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}
	chan_inst = &dev_inst->chan_inst[chan_id];

	ret = linkcom_core_send_link_pkt_inner(dev_inst, chan_inst, para);
	linkcom_put_dev_inst(dev_id);
	return ret;
}

int linkcom_wait_for_pkt(u32 devid, struct linkcom_chan_inst *chan_inst, int timeout, struct mami_pkt_recv_para *para)
{
	struct linkcom_list_head *head = &chan_inst->head;
	struct linkcom_wait_queue *wq = &chan_inst->wq;
	u32 *work_flag = &chan_inst->work_run_flag;
	struct mutex *lock = &chan_inst->lock;
	struct linkcom_rx_cqe cqe = {0};
	u32 chan_id = chan_inst->chan_id;
	int ret = 0;

	if (timeout >= 0) {
		wait_event_interruptible_timeout(wq->head, wq->flag, (long)msecs_to_jiffies(timeout));
	} else {
		wait_event_interruptible(wq->head, wq->flag);
	}
	wq->flag = 0;

	linkcom_pause_recv_pkt_work(work_flag);

	mutex_lock(lock);
	ret = linkcom_check_channel_owner(chan_inst);
	if (ret != 0) {
		linkcom_err("Check channel owner failed. (devid=%u; chanid=%u)\n", devid, chan_inst->chan_id);
		mutex_unlock(&chan_inst->lock);
		return ret;
	}

	ret = linkcom_pop_list_node(head, chan_id, &cqe, para->pktBuff, para->buffLen);
	if (ret == 0) {
		para->reason = cqe.reason;
		para->recvLen = cqe.pkt_len;
		para->lgPortId = 0;
		(void)idm_kern_get_lgport_by_fbody_bport(devid, cqe.nl_id, cqe.port_id, &para->lgPortId);
	}

	mutex_unlock(lock);

	return ret;
}

int linkcom_core_inband_recv_link_pkt(struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst,
										int timeout, struct mami_pkt_recv_para *para)
{
	int ret;

	ret = linkcom_wait_for_pkt(dev_inst->dev_id, chan_inst, 0, para);
	if (ret != -EAGAIN) {
		return ret;
	}

	ret = linkcom_wait_for_pkt(dev_inst->dev_id, chan_inst, timeout, para);

	return ret;
}

int linkcom_core_recv_link_pkt(u32 dev_id, u32 chan_id, int timeout, struct mami_pkt_recv_para *para)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	TASK_TIME_TYPE time = 0;
	u32 chan_num = 0;
	u32 pid = 0;
	int ret = 0;

	if ((timeout < -1) || (timeout > RECV_MAX_TIMEOUT)) {
		linkcom_err("Invalid timeout, valid range: [-1, %d]ms. (devid=%u; timeout=%dms)\n", RECV_MAX_TIMEOUT, dev_id,
			timeout);
		return -EINVAL;
	}

	if (para == NULL || para->pktBuff == NULL) {
		linkcom_err("Input parameter is error. (dev_id=%u; chan_id=%u)\n", dev_id, chan_id);
		return -EINVAL;
	}

	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		return -EINVAL;
	}
	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("get inst failed. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}
	chan_inst = &dev_inst->chan_inst[chan_id];

	pid = (u32)task_tgid_nr(current);
	time = (TASK_TIME_TYPE)READ_ONCE(current->group_leader->start_time);
	if (chan_inst->app_pid != pid || chan_inst->time != time) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("pid=%u time=%lluns is not owner pid(pid=%u, time=%lluns)\n", pid, time, chan_inst->app_pid, chan_inst->time);
		return -EINVAL;
	}
	ret = linkcom_core_inband_recv_link_pkt(dev_inst, chan_inst, timeout, para);
	linkcom_put_dev_inst(dev_id);
	return ret;
}

void linkcom_event_signal(struct linkcom_chan_inst *chan_inst, u32 pkt_num)
{
	/* basic mode */
	if (chan_inst->eventctx == NULL) {
		return;
	}
	/* multiplexed mode */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 7, 12)
	for (int i = 0; i < pkt_num; i++) {
		eventfd_signal(chan_inst->eventctx);
	}
#else
	if (pkt_num != 0) {
		eventfd_signal(chan_inst->eventctx, pkt_num);
	}
#endif
}

int linkcom_core_inband_read_rpkt(struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst)
{
	struct linkcom_id_inst id_inst = {0};
	u32 pkt_num = 0;
	int ret = 0;

	linkcom_fill_id_inst(&id_inst, dev_inst, chan_inst);

	mutex_lock(&chan_inst->lock);
	if (chan_inst->occupy_status != FUNC_OCCUPIED_STS) {
		linkcom_err("chan status is idle. (devid=%u; chanid=%u)\n", dev_inst->dev_id, chan_inst->chan_id);
		mutex_unlock(&chan_inst->lock);
		return -EINVAL;
	}
	ret = linkcom_inband_read_rpkt(&id_inst, &(chan_inst->head), &pkt_num);
	/* consider multi-packet scenario returning error midway */
	if ((ret == 0 || pkt_num > 0)) {
		linkcom_event_signal(chan_inst, pkt_num);
	}
	mutex_unlock(&chan_inst->lock);

	if (ret == 0) {
		chan_inst->wq.flag = 1;
		wake_up(&(chan_inst->wq.head));
	}

	return ret;
}

int linkcom_core_read_rpkt(u32 dev_id, u32 chan_id)
{
	struct linkcom_dev_inst *dev_inst = NULL;
	struct linkcom_chan_inst *chan_inst = NULL;
	u32 chan_num = 0;
	int ret = 0;

	dev_inst = linkcom_get_dev_inst(dev_id);
	if (dev_inst == NULL) {
		return -EINVAL;
	}
	chan_num = dev_inst->chan_num;
	if (chan_id >= chan_num) {
		linkcom_put_dev_inst(dev_id);
		linkcom_err("get inst failed. (devid=%u; chan_id=%u; chan_num=%u)\n", dev_id, chan_id, chan_num);
		return -EINVAL;
	}

	chan_inst = &dev_inst->chan_inst[chan_id];
	ret = linkcom_core_inband_read_rpkt(dev_inst, chan_inst);

	linkcom_put_dev_inst(dev_id);
	return ret;
}

bool linkcom_core_check_channel_status(const struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst)
{
	if ((chan_inst != NULL) && (chan_inst->occupy_status == FUNC_OCCUPIED_STS)) {
		return true;
	}

	return false;
}

void linkcom_core_chan_eventctx_recycle(struct linkcom_chan_inst *chan_inst)
{
	if (chan_inst->eventctx == NULL) {
		return;
	}

	eventfd_ctx_put(chan_inst->eventctx);
	chan_inst->eventctx = NULL;
	chan_inst->eventfd = -1;
}
