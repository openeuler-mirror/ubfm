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
#define pr_fmt(fmt) "[auth_drv] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include "auth_module.h"

#define AUTH_MAX_PROCESS_LIST_SIZE 10
#define AUTH_MAX_TASK_LIST_SIZE 2000 /* AUTH_MAX_PROCESS_LIST_SIZE * AUTH_MAX_TIMESTAMP_QUEUE_SIZE = 10 * 200 = 2000 */
#define AUTH_US_PER_S 1000000
#ifndef unused
#define unused(x)   ((void)(x))
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

static LIST_HEAD(auth_task_list);
static LIST_HEAD(auth_timestamp_list);
static DEFINE_MUTEX(auth_task_mutex);
static DEFINE_MUTEX(auth_timestamp_mutex);
static DECLARE_WAIT_QUEUE_HEAD(poll_wait_queue);

static struct miscdevice auth_dev;

STATIC int auth_open(struct inode *inode, struct file *file)
{
	unused(inode);
	unused(file);
	return 0;
}

STATIC int auth_release(struct inode *inode, struct file *file)
{
	unused(inode);
	unused(file);
	return 0;
}

STATIC bool check_time_diff(struct auth_timestamp_node *node, const ktime_t current_time)
{
	s64 time_diff = 0;
	const ktime_t top_time = node->timestamp_queue[node->queue_head];

	time_diff = ktime_us_delta(current_time, top_time);

	return (time_diff >= AUTH_US_PER_S);
}

STATIC void timestamp_enqueue(struct auth_timestamp_node *node, const ktime_t current_time)
{
	// push in new timestamp if queue is empty
	if (node->queue_size == 0) {
		node->timestamp_queue[node->queue_tail] = current_time;
		node->queue_tail = (node->queue_tail + 1) % AUTH_MAX_TIMESTAMP_QUEUE_SIZE;
		node->queue_size++;
		return;
	}
	// pop out timestamp if queue front is too early
	if (check_time_diff(node, current_time)) {
		node->queue_head = (node->queue_head + 1) % AUTH_MAX_TIMESTAMP_QUEUE_SIZE;
		node->queue_size--;
	}
	// pop out timestamp if queue reaches limit
	if (node->queue_size == AUTH_MAX_TIMESTAMP_QUEUE_SIZE) {
		node->queue_head = (node->queue_head + 1) % AUTH_MAX_TIMESTAMP_QUEUE_SIZE;
		node->queue_size--;
	}

	node->timestamp_queue[node->queue_tail] = current_time;
	node->queue_tail = (node->queue_tail + 1) % AUTH_MAX_TIMESTAMP_QUEUE_SIZE;
	node->queue_size++;
}

STATIC int auth_add_timestamp_node(pid_t tgid, const ktime_t current_time)
{
	struct auth_timestamp_node *node = kzalloc(sizeof(*node), GFP_KERNEL | __GFP_ACCOUNT);

	if (unlikely(node == NULL)) {
		pr_err("IOCTL_GET_PROCNAME: kzalloc auth_timestamp_node failed, size=%zu\n", sizeof(*node));
		return -ENOMEM;
	}

	node->pid = tgid;
	node->queue_head = 0;
	node->queue_tail = 0;
	node->queue_size = 0;

	timestamp_enqueue(node, current_time);
	list_add_tail(&node->list, &auth_timestamp_list);

	return 0;
}

STATIC bool remove_timestamp_node(const ktime_t current_time)
{
	unsigned int index = 0;
	s64 time_diff = 0;
	ktime_t tail_time = 0;
	struct auth_timestamp_node *node = NULL, *tmp = NULL;

	list_for_each_entry_safe(node, tmp, &auth_timestamp_list, list) {
		index = (node->queue_head + node->queue_size - 1U) % AUTH_MAX_TIMESTAMP_QUEUE_SIZE;
		tail_time = node->timestamp_queue[index];
		time_diff = ktime_us_delta(current_time, tail_time);
		if (time_diff >= AUTH_US_PER_S) {
			list_del(&node->list);
			kfree(node);
			node = NULL;
			return true;
		}
	}

	return false;
}

STATIC bool auth_list_is_full(struct list_head *head)
{
	int count = 0;
	struct list_head *pos = NULL;

	list_for_each(pos, head) {
		count++;
		if (count >= AUTH_MAX_PROCESS_LIST_SIZE)
			return true;
	}

	return false;
}

STATIC int auth_check_frequency(pid_t tgid, const ktime_t current_time)
{
	int ret = 0;
	bool isFound = false;
	struct auth_timestamp_node *node = NULL;

	mutex_lock(&auth_timestamp_mutex);
	list_for_each_entry(node, &auth_timestamp_list, list) {
		if (node->pid == tgid) {
			isFound = true;
			break;
		}
	}
	if (isFound == true) {
		if (node->queue_size >= AUTH_MAX_TIMESTAMP_QUEUE_SIZE &&
			(!check_time_diff(node, current_time))) {
			mutex_unlock(&auth_timestamp_mutex);
			pr_warn("IOCTL_GET_PROCNAME: pid[%d] accesses frequently, retry later\n", tgid);
			return -EBUSY;
		}
		timestamp_enqueue(node, current_time);
		mutex_unlock(&auth_timestamp_mutex);
		return 0;
	}

	if (auth_list_is_full(&auth_timestamp_list)) {
		if (!remove_timestamp_node(current_time)) {
			mutex_unlock(&auth_timestamp_mutex);
			pr_warn("IOCTL_GET_PROCNAME: cannot handle pid[%d] accesses current, retry later\n", tgid);
			return -EBUSY;
		}
	}

	ret = auth_add_timestamp_node(tgid, current_time);
	if (ret != 0) {
		mutex_unlock(&auth_timestamp_mutex);
		pr_err("IOCTL_GET_PROCNAME: auth_add_timestamp_node failed\n");
		return ret;
	}
	mutex_unlock(&auth_timestamp_mutex);

	return 0;
}

STATIC bool auth_task_list_is_full(void)
{
	int count = 0;
	struct list_head *pos = NULL;

	list_for_each(pos, &auth_task_list) {
		count++;
		if (count >= AUTH_MAX_TASK_LIST_SIZE)
			return true;
	}

	return false;
}

STATIC int auth_add_node(pid_t tgid)
{
	struct auth_task_node *node = kzalloc(sizeof(*node), GFP_KERNEL | __GFP_ACCOUNT);

	if (unlikely(node == NULL)) {
		pr_err("IOCTL_GET_PROCNAME: kzalloc auth_task_node failed, size=%zu\n", sizeof(*node));
		return -ENOMEM;
	}
	node->proc_info.pid = tgid;
	atomic_set(&node->status, 0);
	node->is_used = false;
	init_waitqueue_head(&node->wait_queue);
	mutex_lock(&auth_task_mutex);
	if (auth_task_list_is_full()) {
		mutex_unlock(&auth_task_mutex);
		kfree(node);
		pr_warn("IOCTL_GET_PROCNAME: auth_task_list full, pid[%d] rejected\n", tgid);
		return -EBUSY;
	}
	list_add_tail(&node->list, &auth_task_list);
	mutex_unlock(&auth_task_mutex);
	pr_info("IOCTL_GET_PROCNAME: add pid[%d] into auth_task_list\n", tgid);
	return 0;
}

STATIC void auth_del_node(struct auth_task_node *node)
{
	pr_info("IOCTL_GET_PROCNAME: remove pid[%d] from auth_task_list\n", node->proc_info.pid);
	mutex_lock(&auth_task_mutex);
	list_del(&node->list);
	kfree(node);
	node = NULL;
	mutex_unlock(&auth_task_mutex);
}

STATIC int auth_wait_procname(struct auth_task_node *node, int timeout)
{
	int tmp_timeout = 0;
	long wait_ret = 0;
	unsigned long jf_time_out = 0;

	if (timeout >= 0) {
		tmp_timeout = timeout < AUTH_EVENT_WAIT_TIME_MIN ? AUTH_EVENT_WAIT_TIME_MIN : timeout;
		jf_time_out = msecs_to_jiffies((unsigned int)tmp_timeout);
		wait_ret = wait_event_interruptible_timeout(node->wait_queue, atomic_read(&node->status) == 1,
			(long)jf_time_out);
		if (wait_ret == 0) { /* timeout */
			pr_err("IOCTL_GET_PROCNAME: wait_event timeout, timeout=%d, tmp_timeout=%d\n", timeout, tmp_timeout);
			return -ETIMEDOUT;
		} else if (wait_ret > 0) {
			wait_ret = 0;
		}
	} else if (timeout == AUTH_EVENT_NO_TIMEOUT_FLAG) { /* timeout = -1, always wait, no timeout */
		wait_ret = wait_event_interruptible(node->wait_queue, atomic_read(&node->status) == 1);
	} else {
		pr_err("Invalid parameter.(timeout=%d)\n", timeout);
		return -EINVAL;
	}

	if (wait_ret != 0) {
		pr_err("IOCTL_GET_PROCNAME: auth wait_event func err, err_ret=%ld\n", wait_ret);
		return (wait_ret == -ERESTARTSYS) ? (-ERESTARTSYS) : (-1);
	}

	return 0;
}

STATIC int auth_get_procname(pid_t tgid, int timeout, unsigned long arg)
{
	int ret = 0, found = 0;
	struct auth_task_node *node = NULL;

	mutex_lock(&auth_task_mutex);
	list_for_each_entry(node, &auth_task_list, list) {
		// is_used: Determine whether it has already been acquired by another thread in the same process.
		if ((!node->is_used) && node->proc_info.pid == tgid) {
			found = 1;
			node->is_used = true;
			break;
		}
	}
	mutex_unlock(&auth_task_mutex);
	if (found == 0) {
		pr_err("IOCTL_GET_PROCNAME: not found cur pid\n");
		return -EFAULT;
	}

	ret = auth_wait_procname(node, timeout);
	if (ret != 0) {
		pr_err("IOCTL_GET_PROCNAME: auth_wait_procname failed\n");
		auth_del_node(node);
		return ret;
	}
	mutex_lock(&auth_task_mutex);
	if (node->proc_info.proc_name[0] == '\0') {
		mutex_unlock(&auth_task_mutex);
		auth_del_node(node);
		pr_err("IOCTL_GET_PROCNAME: proc name is null\n");
		return -EFAULT;
	}
	if (copy_to_user((void __user *)((uintptr_t)arg), (void *)(&node->proc_info), sizeof(struct proc_info)) != 0) {
		mutex_unlock(&auth_task_mutex);
		auth_del_node(node);
		pr_err("IOCTL_GET_PROCNAME: copy_to_user failed\n");
		return -EFAULT;
	}
	pr_info("IOCTL_GET_PROCNAME: pid=%d, procname=%s\n", node->proc_info.pid, node->proc_info.proc_name);
	mutex_unlock(&auth_task_mutex);
	auth_del_node(node);

	return 0;
}

STATIC int ioctl_get_procname(ktime_t current_time, unsigned long arg)
{
	int ret = 0;
	pid_t tgid = current->tgid;
	struct proc_info info = {0};

	ret = auth_check_frequency(tgid, current_time);
	if (ret != 0) {
		pr_err("IOCTL_GET_PROCNAME: auth_check_frequency failed\n");
		return ret;
	}

	if (copy_from_user((void *)&info, (void __user *)(uintptr_t)arg, sizeof(struct proc_info)) != 0) {
		pr_err("IOCTL_GET_PROCNAME: copy_from_user failed\n");
		return -EFAULT;
	}

	ret = auth_add_node(tgid);
	if (ret != 0) {
		pr_err("IOCTL_GET_PROCNAME: add_auth_node failed\n");
		return ret;
	}
	pr_info("IOCTL_GET_PROCNAME: wake up iammgr select\n");
	// notify iammgr process select/poll function
	wake_up_interruptible(&poll_wait_queue);

	ret = auth_get_procname(tgid, info.timeout, arg);
	if (ret != 0) {
		pr_err("IOCTL_GET_PROCNAME: auth_get_procname failed\n");
		return ret;
	}

	return 0;
}

STATIC int ioctl_get_pid(unsigned long arg)
{
	int found = 0;
	struct auth_task_node *node = NULL;

	mutex_lock(&auth_task_mutex);
	list_for_each_entry(node, &auth_task_list, list) {
		if (atomic_read(&node->status) == 0) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		mutex_unlock(&auth_task_mutex);
		pr_err("IOCTL_GET_PID: auth_task_list not pid need auth\n");
		return -EAGAIN;
	}

	if (copy_to_user((void __user *)((uintptr_t)arg), (void *)(&node->proc_info), sizeof(struct proc_info)) != 0) {
		mutex_unlock(&auth_task_mutex);
		pr_err("IOCTL_GET_PID: copy_to_user failed\n");
		return -EFAULT;
	}
	pr_info("IOCTL_GET_PID: pid: %d\n", node->proc_info.pid);
	mutex_unlock(&auth_task_mutex);

	return 0;
}

STATIC int ioctl_set_procname(unsigned long arg)
{
	struct auth_task_node *node = NULL;
	struct proc_info info = {0};
	int found = 0;

	if (copy_from_user((void *)&info, (void __user *)(uintptr_t)arg, sizeof(struct proc_info)) != 0) {
		pr_err("IOCTL_SET_PROCNAME: copy_from_user failed\n");
		return -EFAULT;
	}

	mutex_lock(&auth_task_mutex);
	list_for_each_entry(node, &auth_task_list, list) {
		if (node->proc_info.pid == info.pid && atomic_read(&node->status) == 0) {
			// proc_name length is the same no verification required
			if (strscpy(node->proc_info.proc_name, info.proc_name, AUTH_PROCNAME_SIZE + 1) < 0) {
				mutex_unlock(&auth_task_mutex);
				pr_err("IOCTL_SET_PROCNAME: strscpy failed\n");
				return -EFAULT;
			}
			found = 1;
			break;
		}
	}

	if (found == 0) {
		mutex_unlock(&auth_task_mutex);
		pr_err("IOCTL_SET_PROCNAME: cannot found pid in list\n");
		return -EFAULT;
	}

	pr_info("IOCTL_SET_PROCNAME: pid=%d, procname=%s\n", node->proc_info.pid, node->proc_info.proc_name);
	atomic_set(&node->status, 1);
	wake_up_interruptible(&node->wait_queue);
	mutex_unlock(&auth_task_mutex);

	return 0;
}

STATIC long auth_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	ktime_t current_time = ktime_get();

	if ((file == NULL) || (arg == 0)) {
		pr_err("invalid param, file or arg is NULL\n");
		return -EINVAL;
	}

	switch (cmd) {
	case AUTH_IOCTL_GET_PROCNAME:
		ret = ioctl_get_procname(current_time, arg);
		break;
	case AUTH_IOCTL_GET_PID:
		ret = ioctl_get_pid(arg);
		break;
	case AUTH_IOCTL_SET_PROCNAME:
		ret = ioctl_set_procname(arg);
		break;
	default:
		pr_err("unsupported command\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

STATIC unsigned int auth_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct auth_task_node *node = NULL;

	poll_wait(file, &poll_wait_queue, wait);
	mutex_lock(&auth_task_mutex);
	list_for_each_entry(node, &auth_task_list, list) {
		if (atomic_read(&node->status) == 0) {
			mask |= POLLIN | POLLRDNORM;
			break;
		}
	}
	mutex_unlock(&auth_task_mutex);
	return mask;
}

STATIC const struct file_operations auth_fops = {
	.owner		  = THIS_MODULE,
	.open		   = auth_open,
	.release		= auth_release,
	.unlocked_ioctl = auth_ioctl,
	.poll		   = auth_poll,
};

STATIC int __init auth_module_driver_init(void)
{
	int ret = 0;

	auth_dev.minor = MISC_DYNAMIC_MINOR;
	auth_dev.name  = "auth_dev";
	auth_dev.fops  = &auth_fops;

	ret = misc_register(&auth_dev);
	if (ret != 0) {
		pr_err("misc_register failed, %d\n", ret);
		return ret;
	}
	pr_info("auth driver install succ.\n");
	return 0;
}

STATIC void __exit auth_module_driver_exit(void)
{
	struct auth_task_node *task_node = NULL, *task_tmp = NULL;
	struct auth_timestamp_node *timestamp_node = NULL, *timestamp_tmp = NULL;

	misc_deregister(&auth_dev);
	// free auth_task_list
	mutex_lock(&auth_task_mutex);
	list_for_each_entry_safe(task_node, task_tmp, &auth_task_list, list) {
		list_del(&task_node->list);
		kfree(task_node);
		task_node = NULL;
	}
	mutex_unlock(&auth_task_mutex);

	// free auth_timestamp_list
	mutex_lock(&auth_timestamp_mutex);
	list_for_each_entry_safe(timestamp_node, timestamp_tmp, &auth_timestamp_list, list) {
		list_del(&timestamp_node->list);
		kfree(timestamp_node);
		timestamp_node = NULL;
	}
	mutex_unlock(&auth_timestamp_mutex);

	pr_info("auth driver remove succ.\n");
}

module_init(auth_module_driver_init);
module_exit(auth_module_driver_exit);

MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("Hisilicon auth driver");
MODULE_LICENSE("GPL");
