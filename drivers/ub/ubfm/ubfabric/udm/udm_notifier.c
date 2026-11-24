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
#include <linux/slab.h>

#include "mami_string.h"
#include "udm_dev.h"
#include "udm_notifier.h"

#define UDM_PRI_DEC_ORDER 0 /* priority is high to low */
#define UDM_PRI_INC_ORDER 1 /* priority is low to high */

static struct udm_notifiers g_udm_notifiers;

struct udm_notifiers *udm_get_notifiers(void)
{
	return &g_udm_notifiers;
}

static int udm_get_action_notifier_order(enum udm_notified_action action)
{
	return ((action == UDM_UNINIT) || (action == UDM_PREPARE)) ? UDM_PRI_DEC_ORDER : UDM_PRI_INC_ORDER;
}

static bool udm_is_ctrl_action(enum udm_notified_action action)
{
	return ((action >= UDM_PREPARE) && (action < UDM_ACTION_MAX));
}

static int udm_single_notifier_call(struct udm_notifier_node *nf, u32 udevid, enum udm_notified_action action)
{
	int ret;

	if (udm_is_action_conflict(nf->status[udevid], action)) {
		udm_info("Conflict action. (notifier=%s; action=%u; udevid=%u; status=0x%x)\n",
			nf->notifier, action, udevid, nf->status[udevid]);
		return 0;
	}
	nf->call_count++;
	ret = nf->func(udevid, action, 0);
	if (ret == 0) {
		udm_update_status_by_action(&nf->status[udevid], action);
	}
	nf->call_finish++;

	return ret;
}

static int udm_single_pri_notifier_call(struct list_head *nf_head, u32 udevid, enum udm_notified_action action)
{
	struct udm_notifier_node *nf = NULL;
	int ret;

	list_for_each_entry(nf, nf_head, node) {
		ret = udm_single_notifier_call(nf, udevid, action);
		if (ret != 0) {
			udm_warn("Notifier call warn. (notifier=%s; action=%u; ret=%d)\n", nf->notifier, action, ret);
			if (udm_is_ctrl_action(action)) {
				return ret;
			}
		}
	}

	return 0;
}

static int _udm_notifier_call(struct udm_notifiers *notifiers, u32 udevid, enum udm_notified_action action)
{
	int i, ret, pri;

	for (i = 0; i < UDM_PRI_MAX; i++) {
		pri = (udm_get_action_notifier_order(action) == UDM_PRI_DEC_ORDER) ? (UDM_PRI_MAX - 1 - i) : i;
		ret = udm_single_pri_notifier_call(&notifiers->pri_head[pri], udevid, action);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int udm_notifier_call(u32 udevid, enum udm_notified_action action)
{
	struct udm_notifiers *notifiers = udm_get_notifiers();
	int ret;

	down_read(&notifiers->sem);
	ret = _udm_notifier_call(notifiers, udevid, action);
	up_read(&notifiers->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(udm_notifier_call);

static struct udm_notifier_node *udm_find_notifier_node(struct udm_notifiers *notifiers, const char *notifier)
{
	int i;

	for (i = UDM_PRI0; i < UDM_PRI_MAX; i++) {
		struct list_head *nf_head = &notifiers->pri_head[i];
		struct udm_notifier_node *nf = NULL;

		list_for_each_entry(nf, nf_head, node) {
			if (strncmp(nf->notifier, notifier, UDM_NAME_MAX_LEN) == 0) {
				return nf;
			}
		}
	}

	return NULL;
}

static struct udm_notifier_node *udm_create_notifier(const char *notifier, udm_notify func)
{
	struct udm_notifier_node *nf = kzalloc(sizeof(struct udm_notifier_node), GFP_KERNEL | __GFP_ACCOUNT);
	int ret;

	if (nf == NULL) {
		udm_err("Out of memory. (notifier=\"%s\")\n", notifier);
		return NULL;
	}
	ret = ubfm_strcpy(nf->notifier, UDM_NAME_MAX_LEN, notifier);
	if (ret != 0) {
		kfree(nf);
		nf = NULL;
		return NULL;
	}
	nf->func = func;
	return nf;
}

static void udm_destroy_notifier(struct udm_notifier_node *nf)
{
	kfree(nf);
	nf = NULL;
}

static int _udm_notifier_register(struct udm_notifiers *notifiers, const char *notifier, enum udm_priority pri,
	udm_notify func)
{
	struct udm_notifier_node *nf = udm_find_notifier_node(notifiers, notifier);

	if (nf != NULL) {
		udm_err("Repeat register. (notifier=\"%s\")\n", notifier);
		return -EEXIST;
	}

	nf = udm_create_notifier(notifier, func);
	if (nf == NULL) {
		udm_err("Failed to create notifier node. (notifier=\"%s\")\n", notifier);
		return -ENOMEM;
	}
	list_add_tail(&nf->node, &notifiers->pri_head[pri]);

	udm_info("The notifier registers successfully. (notifier=\"%s\"; pri=%u)\n", notifier, pri);
	return 0;
}

int udm_notifier_register(const char *notifier, enum udm_priority pri, udm_notify func)
{
	int ret;
	struct udm_notifiers *notifiers = NULL;

	if ((notifier == NULL) || (func == NULL)) {
		udm_err("Invalid parameter. (is_notifier_null=%d; is_func_null=%d)\n", (notifier == NULL), (func == NULL));
		return -EINVAL;
	}

	if ((pri < UDM_PRI0) || (pri >= UDM_PRI_MAX)) {
		udm_err("Invalid pri. (notifier=\"%s\"; pri=%u)\n", notifier, pri);
		return -EINVAL;
	}

	notifiers = udm_get_notifiers();
	down_write(&notifiers->sem);
	ret = _udm_notifier_register(notifiers, notifier, pri, func);
	up_write(&notifiers->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(udm_notifier_register);

static int _udm_notifier_unregister(struct udm_notifiers *notifiers, const char *notifier)
{
	struct udm_notifier_node *nf = NULL;

	nf = udm_find_notifier_node(notifiers, notifier);
	if (nf == NULL) {
		udm_err("Repeat unregister. (notifier=\"%s\")\n", notifier);
		return -EINVAL;
	}

	list_del(&nf->node);
	udm_destroy_notifier(nf);

	udm_info("The notifier unregisters successfully. (notifier=\"%s\")\n", notifier);
	return 0;
}

int udm_notifier_unregister(const char *notifier)
{
	int ret;
	struct udm_notifiers *notifiers = NULL;

	if (notifier == NULL) {
		udm_err("The notifier is NULL.\n");
		return -EINVAL;
	}

	notifiers = udm_get_notifiers();
	down_write(&notifiers->sem);
	ret = _udm_notifier_unregister(notifiers, notifier);
	up_write(&notifiers->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(udm_notifier_unregister);

static void _udm_notifiers_init(struct udm_notifiers *notifiers)
{
	int i;

	for (i = UDM_PRI0; i < UDM_PRI_MAX; i++) {
		INIT_LIST_HEAD(&notifiers->pri_head[i]);
	}
	init_rwsem(&notifiers->sem);
}

void udm_notifier_init(void)
{
	_udm_notifiers_init(udm_get_notifiers());
}

static void _udm_notifiers_uninit(struct udm_notifiers *notifiers)
{
	int i;

	for (i = 0; i < UDM_PRI_MAX; i++) {
		struct list_head *nf_head = &notifiers->pri_head[i];
		struct udm_notifier_node *nf = NULL, *n = NULL;

		list_for_each_entry_safe(nf, n, nf_head, node) {
			list_del(&nf->node);
			udm_destroy_notifier(nf);
		}
	}
}

void udm_notifier_uninit(void)
{
	_udm_notifiers_uninit(udm_get_notifiers());
}
