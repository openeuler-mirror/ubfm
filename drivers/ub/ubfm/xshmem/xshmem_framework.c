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
#define pr_fmt(fmt) "XSHMEM: " fmt

#include <linux/types.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/jhash.h>
#include <linux/rcupdate.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "xshmem_framework.h"

#define XSHM_HLIST_TABLE_BIT 13
#define XSHMEM_MAX_ID (1U << 14U)

/* Protect the lifetime for all POOL */
static DEFINE_MUTEX(xshmem_mutex);
static DEFINE_IDR(xshmem_idr);
static DEFINE_HASHTABLE(xshmem_key_list, XSHM_HLIST_TABLE_BIT);
static int next_xshmem_id;

static struct miscdevice xshmem_dev;

static LIST_HEAD(registered_algo);

STATIC struct xshm_pool_algo *xshmem_find_algo(int algo)
{
	struct xshm_pool_algo *tmp = NULL;

	list_for_each_entry(tmp, &registered_algo, algo_node) {
		if (tmp->num == algo)
			return tmp;
	}
	return NULL;
}

STATIC struct hlist_head *hash_bucket(char *key, int len)
{
	u32 bucket = jhash(key, (unsigned int)len, 0);

	return &xshmem_key_list[bucket & ((1U << XSHM_HLIST_TABLE_BIT) - 1U)];
}

struct xshm_task_block_cnt {
	int			refcnt;
	struct list_head	node_list; /* list node in TASK node */
	struct list_head	blk_list;  /* list node in BLOCK */
	struct xshm_block       *blk;
	struct xshm_task_pool_node *node;
};

struct xshm_task_pool_node {
	int			regcnt;    /* register count of the TASK registering to the POOL */
	struct list_head	task_node; /* list node in TASK */
	struct list_head	pool_node; /* list node in POOL */
	struct xshm_task        *task;
	struct xshm_pool        *pool;

	atomic64_t		alloc_size;
	atomic64_t		real_alloc_size;
	/* list_head for all BLOCKs' refcnt in the POOL allocated by the TASK */
	struct list_head	list_head;
};

STATIC void task_alloc_size_update(struct xshm_task_pool_node *node,
				   struct xshm_block *blk, bool inc)
{
	if (inc) {
		atomic64_add((long)(blk->alloc_size), &node->alloc_size);
		atomic64_add((long)(blk->real_size), &node->real_alloc_size);
	} else {
		atomic64_sub((long)(blk->alloc_size), &node->alloc_size);
		atomic64_sub((long)(blk->real_size), &node->real_alloc_size);
	}
}

STATIC void task_link_blk(struct xshm_task_pool_node *node,
			  struct xshm_block *blk,
			  struct xshm_task_block_cnt *cnt)
{
	cnt->refcnt = 1;
	cnt->blk = blk;
	cnt->node = node;
	list_add_tail(&cnt->node_list, &node->list_head);
	list_add_tail(&cnt->blk_list, &blk->list_head);

	task_alloc_size_update(node, blk, true);
}

STATIC void task_unlink_blk(struct xshm_task_block_cnt *cnt)
{
	list_del(&cnt->node_list);
	list_del(&cnt->blk_list);

	task_alloc_size_update(cnt->node, cnt->blk, false);

	kfree(cnt);
}

STATIC DEFINE_MUTEX(task_mutex);
STATIC LIST_HEAD(all_task);

struct xshm_task {
	pid_t			pid;
	int			pool_count;
	struct list_head	list_head;
	struct list_head	list;
	struct mutex		task_mutex;
};

STATIC int xshmem_open(struct inode *inode, struct file *file)
{
	struct xshm_task *ptask = NULL;

	ptask = kmalloc(sizeof(*ptask), GFP_KERNEL | __GFP_ACCOUNT);
	if (unlikely(ptask == NULL))
		return -ENOMEM;

	ptask->pid = current->pid;
	ptask->pool_count = 0;
	INIT_LIST_HEAD(&ptask->list_head);
	mutex_init(&ptask->task_mutex);

	file->private_data = ptask;

	mutex_lock(&task_mutex);
	list_add_tail(&ptask->list, &all_task);
	mutex_unlock(&task_mutex);

	return 0;
}

STATIC struct xshm_task_pool_node *find_task_pool_node(struct xshm_task *task, struct xshm_pool *xp)
{
	struct xshm_task_pool_node *node = NULL;

	list_for_each_entry(node, &task->list_head, task_node) {
		if (node->pool == xp)
			return node;
	}
	return NULL;
}

/*
 * The refcnt of the xp should be increased by one on success.
 * We don't require the xshmem_mutex here since the xp->refcnt has been increased already.
 */
STATIC int xshmem_pool_attach(struct xshm_task *task, struct xshm_pool *xp)
{
	struct xshm_task_pool_node *node = NULL;

	node = find_task_pool_node(task, xp);
	if (node == NULL) {
		node = kmalloc(sizeof(*node), GFP_KERNEL | __GFP_ACCOUNT);
		if (unlikely(node == NULL)) {
			pr_err("alloc xshm_task_pool_node failed\n");
			return -ENOMEM;
		}
		node->regcnt = 1;
		node->task = task;
		node->pool = xp;
		atomic64_set(&node->alloc_size, 0);
		atomic64_set(&node->real_alloc_size, 0);
		list_add_tail(&node->task_node, &task->list_head);
		INIT_LIST_HEAD(&node->list_head);

		/* Do not add node to xp->list_head until all its elements initialized */
		spin_lock(&xp->xp_task_spinlock);
		list_add_tail(&node->pool_node, &xp->list_head);
		spin_unlock(&xp->xp_task_spinlock);

		atomic_inc(&xp->refcnt);
		task->pool_count++;
	} else {
		if (node->regcnt == INT_MAX) {
			pr_err("xshm_task_pool_node reg count overflow\n");
			return -EOVERFLOW;
		}
		node->regcnt++;
	}
	return 0;
}

STATIC void xshmem_block_destroy(struct xshm_pool *xp, struct xshm_block *blk)
{
	xp->algo->xshm_block_free(xp, blk); // should not fail
	list_del(&blk->block_node);
	kfree(blk);
}

STATIC void xshmem_task_clear_block(struct xshm_task_pool_node *node)
{
	struct xshm_pool *xp = node->pool;
	struct xshm_task_block_cnt *cnt = NULL, *tmp = NULL;

	mutex_lock(&xp->xp_block_mutex);
	list_for_each_entry_safe(cnt, tmp, &node->list_head, node_list) {
		struct xshm_block *blk = cnt->blk;

		blk->refcnt -= cnt->refcnt;

		task_unlink_blk(cnt);

		if (blk->refcnt == 0)
			xshmem_block_destroy(xp, blk);
	}
	mutex_unlock(&xp->xp_block_mutex);
}

/* The caller should increase xp->refcnt to protect the POOL from being destroyed. */
STATIC int xshmem_pool_detach(struct xshm_task *task, struct xshm_pool *xp)
{
	struct xshm_task_pool_node *node = NULL;

	node = find_task_pool_node(task, xp);
	if (node != NULL) {
		node->regcnt--;
		if (node->regcnt != 0)
			return 0;

		spin_lock(&xp->xp_task_spinlock);
		list_del(&node->pool_node);
		spin_unlock(&xp->xp_task_spinlock);

		xshmem_task_clear_block(node);

		list_del(&node->task_node);
		atomic_dec(&xp->refcnt);
		task->pool_count--;
		kfree(node);
		return 0;
	}

	pr_err("the POOL has already been detached\n");
	return -ESRCH;
}

/*
 * The caller must hold xshmem_mutex and this function would unlock xshmem_mutex on failure.
 */
STATIC struct xshm_pool *xshmem_pool_create(struct xshm_pool_algo *algo, struct xshm_reg_arg *arg,
						struct hlist_head *bucket)
{
	int id, ret;
	struct xshm_pool *xp = NULL;
	char *key = arg->key;
	unsigned int key_len = arg->key_len;

	xp = kmalloc(sizeof(*xp) + key_len + 1U, GFP_KERNEL | __GFP_ACCOUNT);
	if (unlikely(xp == NULL)) {
		mutex_unlock(&xshmem_mutex);
		pr_err("alloc pool memory failed\n");
		return (struct xshm_pool *)ERR_PTR(-ENOMEM);
	}
	xp->key_len = (int)key_len;
	ret = strscpy(xp->key, key, key_len + 1U);
	if (ret < 0) {
		mutex_unlock(&xshmem_mutex);
		kfree(xp);
		return (struct xshm_pool *)ERR_PTR(ret);
	}
	xp->algo = algo;
	ret = algo->xshm_pool_init(xp, arg);
	if (ret < 0) {
		mutex_unlock(&xshmem_mutex);
		kfree(xp);
		pr_err("init hook failed\n");
		return (struct xshm_pool *)ERR_PTR(ret);
	}

	id = idr_alloc(&xshmem_idr, xp, next_xshmem_id, XSHMEM_MAX_ID, GFP_KERNEL | __GFP_ACCOUNT);
	if (id < 0) {
		if (next_xshmem_id != 0)
			id = idr_alloc(&xshmem_idr, xp, 0, next_xshmem_id, GFP_KERNEL | __GFP_ACCOUNT);
		if (id < 0) {
			mutex_unlock(&xshmem_mutex);
			algo->xshm_pool_free(xp);
			kfree(xp);
			pr_err("idr alloc failed\n");
			return (struct xshm_pool *)ERR_PTR(id);
		}
	}

	next_xshmem_id = (int)(((unsigned int)id + 1U) & (XSHMEM_MAX_ID - 1U));

	xp->pool_id = id;
	atomic_set(&xp->refcnt, 0);
	spin_lock_init(&xp->xp_task_spinlock);
	INIT_LIST_HEAD(&xp->list_head);

	mutex_init(&xp->xp_block_mutex);
	INIT_LIST_HEAD(&xp->block_head);
	hlist_add_head(&xp->hnode, bucket);
	return xp;
}

/*
 * get the POOL specified by key, create one if it not exist.
 */
STATIC struct xshm_pool *xshmem_pool_register(struct xshm_pool_algo *algo, struct xshm_reg_arg *arg)
{
	struct xshm_pool *xp = NULL;
	struct hlist_head *bucket = NULL;

	bucket = hash_bucket(arg->key, (int)(arg->key_len));

	mutex_lock(&xshmem_mutex);
	hlist_for_each_entry(xp, bucket, hnode) {
		if ((arg->key_len == (unsigned int)(xp->key_len)) && (strncmp(arg->key, xp->key, arg->key_len) == 0)) {
			if ((xp->algo != algo) || (algo->xshm_pool_same(xp, arg) == 0)) {
				mutex_unlock(&xshmem_mutex);
				pr_err("the pool reg arg check failed\n");
				return (struct xshm_pool *)ERR_PTR(-EINVAL);
			}
			goto exist;
		}
	}
	xp = xshmem_pool_create(algo, arg, bucket);
	if (IS_ERR(xp))
		return xp;

exist:
	/*
	 * Here increase the POOL's refcnt by one, one for the convenience of fallback in
	 * error branch in ioctl_xshmem_pool_register(), another to promise that the POOL
	 * cannot be delete in the later attach routine.
	 */
	atomic_inc(&xp->refcnt);
	mutex_unlock(&xshmem_mutex);

	return xp;
}

STATIC struct xshm_pool *xshmem_pool_get(int pool_id)
{
	struct xshm_pool *xp = NULL;

	mutex_lock(&xshmem_mutex);
	xp = (struct xshm_pool *)idr_find(&xshmem_idr, (unsigned long)pool_id);
	if (xp != NULL)
		atomic_inc(&xp->refcnt);
	mutex_unlock(&xshmem_mutex);

	return xp;
}

STATIC int xshmem_pool_put(struct xshm_pool *xp)
{
	mutex_lock(&xshmem_mutex);
	if (!atomic_dec_and_test(&xp->refcnt)) {
		mutex_unlock(&xshmem_mutex);
		return XSHMEM_POOL_STATUS_EXIST;
	}

	hlist_del(&xp->hnode);
	(void)idr_remove(&xshmem_idr, (unsigned long)(xp->pool_id));
	mutex_unlock(&xshmem_mutex);
	WARN(!list_empty(&xp->block_head), "POOL deleted with referenced BLOCK\n");
	xp->algo->xshm_pool_free(xp);
	kfree(xp);

	return XSHMEM_POOL_STATUS_DELETE;
}

STATIC int ioctl_xshmem_pool_register(struct xshm_task *task, unsigned long arg)
{
	int ret;
	struct xshm_pool *xp = NULL;
	unsigned int key_len;
	char key[XSHM_KEY_SIZE + 1U];
	struct xshm_reg_arg reg_arg;
	struct xshm_pool_algo *algo = NULL;

	if (copy_from_user(&reg_arg, (struct xshmem_reg_arg __user *)arg,
		sizeof(reg_arg)) != 0) {
		pr_err("register: copy_from_user failed\n");
		return -EFAULT;
	}

	algo = xshmem_find_algo(reg_arg.algo);
	if (unlikely(algo == NULL)) {
		pr_err("unsupported algorithm\n");
		return -ENODEV;
	}

	key_len = reg_arg.key_len;
	if ((key_len == 0U) || (key_len > XSHM_KEY_SIZE)) {
		pr_err("tag_len invalid\n");
		return -EINVAL;
	}

	if (copy_from_user(key, (char __user *)reg_arg.key, key_len) != 0) {
		pr_err("copy tag from user failed\n");
		return -EFAULT;
	}
	key[key_len] = '\0';
	reg_arg.key = key;
	xp = xshmem_pool_register(algo, &reg_arg);
	if (IS_ERR(xp))
		return (int)PTR_ERR(xp);
	ret = xshmem_pool_attach(task, xp);
	(void)xshmem_pool_put(xp);

	return (ret < 0) ? ret : xp->pool_id;
}

STATIC int ioctl_xshmem_pool_unregister(struct xshm_task *task, unsigned long arg)
{
	int ret, state;
	struct xshm_pool *xp = NULL;

	xp = xshmem_pool_get((int)arg);
	if (xp == NULL) {
		pr_err("couldn't find the pool\n");
		return -ENODEV;
	}

	ret = xshmem_pool_detach(task, xp);

	state = xshmem_pool_put(xp);

	return (ret < 0) ? ret : state;
}

/* The caller must hold xp->xp_block_mutex */
STATIC struct xshm_block *xshmem_find_block(struct xshm_pool *xp, u32 offset)
{
	struct xshm_block *blk = NULL;

	list_for_each_entry(blk, &xp->block_head, block_node) {
		if (blk->offset == offset)
			return blk;
	}
	return NULL;
}

STATIC int ioctl_xshmem_block_alloc(struct xshm_pool *xp, struct xshm_task_pool_node *node,
					struct xshm_block_arg *arg)
{
	int ret;
	struct xshm_block *blk = NULL;
	struct xshm_task_block_cnt *cnt = NULL;

	blk = kmalloc(sizeof(*blk), GFP_KERNEL | __GFP_ACCOUNT);
	if (unlikely(blk == NULL)) {
		pr_err("alloc xshm_block memory failed\n");
		return -ENOMEM;
	}
	cnt = kmalloc(sizeof(*cnt), GFP_KERNEL | __GFP_ACCOUNT);
	if (unlikely(cnt == NULL)) {
		pr_err("alloc xshm_task_block_cnt memory failed\n");
		kfree(blk);
		return -ENOMEM;
	}

	blk->alloc_size = arg->size;
	mutex_lock(&xp->xp_block_mutex);
	ret = xp->algo->xshm_block_alloc(xp, blk);
	if (ret < 0) {
		pr_err("block_alloc failed, alloc size: %lu, ret: %d\n", arg->size, ret);
		kfree(blk);
		kfree(cnt);
	} else {
		blk->refcnt = 1;
		arg->offset = blk->offset;
		INIT_LIST_HEAD(&blk->list_head);
		list_add_tail(&blk->block_node, &xp->block_head);

		task_link_blk(node, blk, cnt);
	}
	mutex_unlock(&xp->xp_block_mutex);

	return ret;
}

STATIC struct xshm_task_block_cnt *
find_task_block_cnt(struct xshm_task_pool_node *node, struct xshm_block *blk)
{
	struct xshm_task_block_cnt *cnt = NULL;

	list_for_each_entry(cnt, &blk->list_head, blk_list) {
		if (cnt->node == node)
			return cnt;
	}
	return NULL;
}

STATIC int ioctl_xshmem_block_get(struct xshm_pool *xp, struct xshm_task_pool_node *node, u32 offset)
{
	int ret = 0;
	struct xshm_block *blk = NULL;

	mutex_lock(&xp->xp_block_mutex);
	blk = xshmem_find_block(xp, offset);
	if (blk != NULL) {
		struct xshm_task_block_cnt *cnt = NULL;

		cnt = find_task_block_cnt(node, blk);
		if (cnt == NULL) {
			cnt = kmalloc(sizeof(*cnt), GFP_KERNEL | __GFP_ACCOUNT);
			if (cnt == NULL) {
				mutex_unlock(&xp->xp_block_mutex);
				pr_err("get:alloc xshm_task_block_cnt memory failed\n");
				return -ENOMEM;
			}

			if (blk->refcnt == INT_MAX) {
				pr_err("xshm_block ref count overflow, current ref: %d\n", blk->refcnt);
				kfree(cnt);
				ret = -EOVERFLOW;
			} else {
				task_link_blk(node, blk, cnt);
				blk->refcnt++;
			}
		} else {
			if ((cnt->refcnt == INT_MAX) || (blk->refcnt == INT_MAX)) {
				pr_err("ref overflow, xshm_task_block_cnt ref: %d, xshm_block ref: %d\n",
					cnt->refcnt, blk->refcnt);
				ret = -EOVERFLOW;
			} else {
				cnt->refcnt++;
				blk->refcnt++;
			}
		}
	} else {
		pr_err("get unalloced block\n");
		ret = -ENODEV;
	}
	mutex_unlock(&xp->xp_block_mutex);

	return ret;
}

STATIC int ioctl_xshmem_block_put(struct xshm_pool *xp, struct xshm_task_pool_node *node,
				u32 offset)
{
	int ret = 0;
	struct xshm_block *blk = NULL;

	mutex_lock(&xp->xp_block_mutex);
	blk = xshmem_find_block(xp, offset);
	if (blk != NULL) {
		struct xshm_task_block_cnt *cnt = NULL;

		cnt = find_task_block_cnt(node, blk);
		if (cnt == NULL) {
			mutex_unlock(&xp->xp_block_mutex);
			pr_err("the TASK cannot put the block\n");
			return -EPERM;
		}
		cnt->refcnt--;
		if (cnt->refcnt == 0)
			task_unlink_blk(cnt);

		blk->refcnt--;
		if (blk->refcnt == 0)
			xshmem_block_destroy(xp, blk);
	} else {
		pr_err("free unalloced block\n");
		ret = -ENODEV;
	}
	mutex_unlock(&xp->xp_block_mutex);

	return ret;
}

STATIC int ioctl_xshmem_block_common(struct xshm_task *task, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct xshm_pool *xp = NULL;
	struct xshm_block_arg block_arg;
	struct xshm_task_pool_node *node = NULL;
	struct xshm_block_arg *usr_arg = (struct xshm_block_arg __user *)arg;

	if (copy_from_user(&block_arg, usr_arg, sizeof(block_arg)) != 0) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	xp = xshmem_pool_get(block_arg.pool_id);
	if (xp == NULL) {
		pr_err("invalid pool_id\n");
		return -EINVAL;
	}
	node = find_task_pool_node(task, xp);
	if (node == NULL) {
		xshmem_pool_put(xp);
		pr_err("TASK is not attached to POOL\n");
		return -EINVAL;
	}

	switch (cmd) {
	case XSHMEM_BLOCK_ALLOC:
		ret = ioctl_xshmem_block_alloc(xp, node, &block_arg);
		if (ret == 0) {
			ret = put_user(block_arg.offset, &usr_arg->offset);
			if (ret != 0)
				(void)ioctl_xshmem_block_put(xp, node, (unsigned int)(block_arg.offset));
		}
		break;
	case XSHMEM_BLOCK_GET:
		ret = ioctl_xshmem_block_get(xp, node, (unsigned int)(block_arg.offset));
		break;
	case XSHMEM_BLOCK_PUT:
		ret = ioctl_xshmem_block_put(xp, node, (unsigned int)(block_arg.offset));
		break;
	default:
		pr_err("%s pid %d unknown cmd\n", __func__, current->pid);
		break;
	}

	(void)xshmem_pool_put(xp);

	return ret;
}

STATIC long xshmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct xshm_task *task = (struct xshm_task *)(file->private_data);

	mutex_lock(&task->task_mutex);
	switch (cmd) {
	case XSHMEM_POOL_REGISTER:
		ret = ioctl_xshmem_pool_register(task, arg);
		break;
	case XSHMEM_POOL_UNREGISTER:
		ret = ioctl_xshmem_pool_unregister(task, arg);
		break;
	case XSHMEM_BLOCK_ALLOC:
	case XSHMEM_BLOCK_PUT:
	case XSHMEM_BLOCK_GET:
		ret = ioctl_xshmem_block_common(task, cmd, arg);
		break;
	default:
		pr_err("unsupported command\n");
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&task->task_mutex);

	return ret;
}

STATIC int xshmem_release(struct inode *inode, struct file *file)
{
	struct xshm_task_pool_node *node = NULL, *tmp = NULL;
	struct xshm_task *task = (struct xshm_task *)(file->private_data);

	mutex_lock(&task_mutex);
	list_del(&task->list);
	mutex_unlock(&task_mutex);

	list_for_each_entry_safe(node, tmp, &task->list_head, task_node) {
		if (node->regcnt > 1)
			pr_warn("Not all user unregister the POOL, id:%d\n", node->pool->pool_id);

		spin_lock(&node->pool->xp_task_spinlock);
		list_del(&node->pool_node);
		spin_unlock(&node->pool->xp_task_spinlock);

		xshmem_task_clear_block(node);
		list_del(&node->task_node);
		(void)xshmem_pool_put(node->pool);

		kfree(node);
	}

	kfree(task);

	return 0;
}

STATIC const struct file_operations xshmem_fops = {
	.owner          = THIS_MODULE,
	.open           = xshmem_open,
	.release        = xshmem_release,
	.unlocked_ioctl = xshmem_ioctl,
};

STATIC void per_task_info_show(struct seq_file *seq, struct xshm_task *task)
{
	struct xshm_task_pool_node *node = NULL;

	mutex_lock(&task->task_mutex);

	seq_printf(seq, "TASK:%d\n", task->pid);
	list_for_each_entry(node, &task->list_head, task_node)
		seq_printf(seq, "    POOL:%-4d %#-8llx %#-8llx\n", node->pool->pool_id,
				(u64)atomic64_read(&node->alloc_size),
				(u64)atomic64_read(&node->real_alloc_size));

	mutex_unlock(&task->task_mutex);
}

STATIC int task_info_show(struct seq_file *seq, void *offset)
{
	struct xshm_task *task = NULL;

	mutex_lock(&task_mutex);
	list_for_each_entry(task, &all_task, list)
		per_task_info_show(seq, task);
	mutex_unlock(&task_mutex);

	return 0;
}

STATIC int per_pool_info_show(int id, void *p, void *data)
{
	struct xshm_pool *xp = (struct xshm_pool *)(p);
	struct seq_file *seq = (struct seq_file *)(data);

	seq_printf(seq, "POOL:%d\n    tag:%s, refcnt:%d, algo:%s\n",
			id, xp->key, atomic_read(&xp->refcnt), xp->algo->name);
	if (xp->algo->xshm_pool_show != NULL) {
		mutex_lock(&xp->xp_block_mutex);
		xp->algo->xshm_pool_show(xp, seq);
		mutex_unlock(&xp->xp_block_mutex);
	}

	return 0;
}

STATIC int pool_info_show(struct seq_file *seq, void *offset)
{
	int ret;

	mutex_lock(&xshmem_mutex);
	ret = idr_for_each(&xshmem_idr, per_pool_info_show, seq);
	mutex_unlock(&xshmem_mutex);

	return ret;
}

STATIC void __init xshmem_proc_fs_init(void)
{
	struct proc_dir_entry *entry = NULL;

	entry = (struct proc_dir_entry *)proc_mkdir("xshmem", NULL);
	if (entry == NULL) {
		pr_err("create proc dir failed\n");
		return;
	}

	if (proc_create_single("task_info", 0400, entry, task_info_show) == NULL)
		pr_warn("task_info file create failed\n");

	if (proc_create_single("pool_info", 0400, entry, pool_info_show) == NULL)
		pr_warn("pool_info file create failed\n");
}

STATIC int xshmem_register_algo(struct xshm_pool_algo *algo)
{
	struct xshm_pool_algo *tmp = NULL;

	if ((algo == NULL) || (algo->xshm_pool_init == NULL) || (algo->xshm_pool_free == NULL) ||
		(algo->xshm_block_alloc == NULL) || (algo->xshm_block_free == NULL) ||
		(algo->xshm_pool_same == NULL))
		return -EINVAL;

	list_for_each_entry(tmp, &registered_algo, algo_node) {
		if (algo->num == tmp->num) {
			pr_err("algorithm with the same id[%d,%s] has been registered\n",
					tmp->num, tmp->name);
			return -EEXIST;
		}
	}
	list_add_tail(&algo->algo_node, &registered_algo);

	pr_info("algo: no: %d, name: %s, registered\n", algo->num, algo->name);

	return 0;
}

#ifdef XSHMEM_MODULE_DEBUG
STATIC bool empty_algo_pool_same(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	pr_info("pool_same_hook: algo:%s, pool_size: %ld\n", xp->algo->name, arg->pool_size);
	return (unsigned long)xp->private_obj == arg->pool_size;
}

STATIC int empty_algo_pool_init(struct xshm_pool *xp, struct xshm_reg_arg *arg)
{
	pr_info("pool_init_hook: algo:%s, pool_size: %ld\n", xp->algo->name, arg->pool_size);
	xp->private_obj = (void *)arg->pool_size;
	return 0;
}

STATIC int empty_algo_pool_free(struct xshm_pool *xp)
{
	pr_info("pool_free_hook: algo:%s, pool_size: %ld, pool_id:%d\n",
		xp->algo->name, (long)xp->private_obj, xp->pool_id);
	return 0;
}

STATIC int empty_algo_block_alloc(struct xshm_pool *xp, struct xshm_block *blk)
{
	pr_info("block_alloc_hook:pool_id:%d, alloc_size:%lu\n", xp->pool_id, blk->alloc_size);
	blk->real_size = blk->alloc_size;
	blk->offset = 0;
	return 0;
}

STATIC int empty_algo_block_free(struct xshm_pool *xp, struct xshm_block *blk)
{
	pr_info("block_free_hook:pool_id:%d, offset:%ld\n", xp->pool_id, blk->offset);
	return 0;
}

/* Use just for test */
STATIC struct xshm_pool_algo empty_algo = {
	.num = XSHMEM_ALGO_EMPTY,
	.name = "empty_algo",
	.xshm_pool_same = empty_algo_pool_same,
	.xshm_pool_init = empty_algo_pool_init,
	.xshm_pool_free = empty_algo_pool_free,
	.xshm_block_alloc = empty_algo_block_alloc,
	.xshm_block_free = empty_algo_block_free,
};
#endif

STATIC int __init xshmem_init(void)
{
	int ret;

	xshmem_dev.minor = MISC_DYNAMIC_MINOR;
	xshmem_dev.name  = "xshmem_dev";
	xshmem_dev.fops  = &xshmem_fops;

	ret = misc_register(&xshmem_dev);
	if (ret != 0) {
		pr_err("misc_register failed, %d\n", ret);
		return ret;
	}

	(void)xshmem_register_algo(&fsc_algo);
	(void)xshmem_register_algo(&blk_algo);
#ifdef XSHMEM_MODULE_DEBUG
	(void)xshmem_register_algo(&empty_algo);
#endif
	xshmem_proc_fs_init();

	return 0;
}
module_init(xshmem_init);

STATIC void __exit xshmem_exit(void)
{
	(void)remove_proc_subtree("xshmem", NULL);
	misc_deregister(&xshmem_dev);
	pr_info("module exit\n");
}
module_exit(xshmem_exit);

MODULE_LICENSE("GPL v2");
