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
#include "udev_ucmi_ops.h"
#include "uop.h"

static const struct udev_ucmi_ops *g_udev_ucmi_ops[UDEV_CONNECT_TYPE_NUM];

int udev_ucmi_ops_register(enum udev_connect_type type, const struct udev_ucmi_ops *ops)
{
	if (unlikely((type >= UDEV_CONNECT_TYPE_NUM) || (ops == NULL))) {
		hw_err("Invalid param. (connect_type=%u)\n", type);
		return -EINVAL;
	}

	if (unlikely(g_udev_ucmi_ops[type] != NULL)) {
		hw_err("Ucmi ops already registered. (connect_type=%u)\n", type);
		return -EEXIST;
	}
	g_udev_ucmi_ops[type] = ops;

	return 0;
}

void udev_ucmi_ops_unregister(enum udev_connect_type type)
{
	if (likely(type < UDEV_CONNECT_TYPE_NUM)) {
		g_udev_ucmi_ops[type] = NULL;
	}
}

const struct udev_ucmi_ops *udev_ucmi_ops_get(enum udev_connect_type type)
{
	if (unlikely(type >= UDEV_CONNECT_TYPE_NUM)) {
		hw_err("Invalid param. (connect_type=%u)\n", type);
		return NULL;
	}

	return g_udev_ucmi_ops[type];
}

int ucmi_init(u32 connect_type, void *priv_data)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->init == NULL) {
		hw_err("Init fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->init(priv_data);
}

void ucmi_uninit(u32 connect_type, void *priv_data)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return;
	}

	if (ops->uninit == NULL) {
		hw_err("Uninit fn is NULL. (connect_type=%u)\n", connect_type);
		return;
	}

	ops->uninit(priv_data);
}

int ucmi_master_setup(u32 connect_type)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->master_setup == NULL) {
		hw_err("Master setup fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->master_setup();
}

int ucmi_slave_setup(u32 connect_type)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->slave_setup == NULL) {
		hw_err("Slave setup fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->slave_setup();
}

int ucmi_assign_win(u32 connect_type, struct udev_ucmi_addr *addr)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->assign_win == NULL) {
		hw_err("Assign win fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->assign_win(addr);
}

int ucmi_assign_win_by_axi(u32 connect_type, u32 axi_id, struct udev_ucmi_addr *addr)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->assign_win_by_axi == NULL) {
		hw_err("Assign win by axi fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->assign_win_by_axi(axi_id, addr);
}

int ucmi_assign_256m_win(u32 connect_type, u32 devid, u64 chip_addr, u32 *axi_id)
{
	const struct udev_ucmi_ops *ops = NULL;

	if (axi_id == NULL) {
		hw_err("axi_id is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->assign_256m_win == NULL) {
		hw_err("Assign 256m win fn is NULL. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	return ops->assign_256m_win(devid, chip_addr, axi_id);
}

void ucmi_remove_win(u32 connect_type, struct udev_ucmi_addr *addr)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return;
	}

	if (ops->remove_win == NULL) {
		hw_err("Remove win fn is NULL. (connect_type=%u)\n", connect_type);
		return;
	}

	return ops->remove_win(addr);
}

static struct udev_ucmi_info *ucmi_info_get(u32 connect_type, u32 devid)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return NULL;
	}

	if (ops->info_get == NULL) {
		hw_err("Get addr info fn is NULL. (connect_type=%u)\n", connect_type);
		return NULL;
	}

	return ops->info_get(devid);
}

static void ucmi_info_put(u32 connect_type, u32 devid)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return;
	}

	if (ops->info_put == NULL) {
		hw_err("Get addr info fn is NULL. (connect_type=%u)\n", connect_type);
		return;
	}

	ops->info_put(devid);
}

struct udev_ucmi_addr_info *ucmi_get_addr_info(u32 connect_type, u32 devid, u64 chip_addr, const char *name)
{
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *addr_info = NULL;

	info = ucmi_info_get(connect_type, devid);
	if (info == NULL) {
		hw_err("Get ucmi info failed. (connect_type=%u; devid=%u)\n", connect_type, devid);
		return NULL;
	}

	mutex_lock(&info->mtx);
	list_for_each_entry(addr_info, &info->addr_list, entry) {
		if ((chip_addr >= addr_info->chip_addr) && (chip_addr < addr_info->chip_addr + addr_info->size) &&
			strcmp(addr_info->name, name) == 0) {
			mutex_unlock(&info->mtx);
			return addr_info;
		}
	}
	mutex_unlock(&info->mtx);
	ucmi_info_put(connect_type, devid); /* ensure that when the interface returns NULL, the upper-layer user does not need to put */
	return NULL;
}

void ucmi_put_addr_info(u32 connect_type, u32 devid)
{
	ucmi_info_put(connect_type, devid);
}

int ucmi_foreach_addr(u32 connect_type, u32 devid, struct ures_inst *inst, ucmi_addr_fn fn)
{
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct udev_ucmi_info *info = NULL;
	int ret;

	if (fn == NULL) {
		hw_err("Addr fn is NULL. (connect_type=%u; devid=%u)\n", connect_type, devid);
		return -EINVAL;
	}

	info = ucmi_info_get(connect_type, devid);
	if (info == NULL) {
		hw_err("Get ucmi info failed. (connect_type=%u; devid=%u)\n", connect_type, devid);
		return -ENODATA;
	}

	mutex_lock(&info->mtx);
	list_for_each_entry(addr_info, &info->addr_list, entry) {
		ret = fn(inst, addr_info);
		if (ret != 0) {
			mutex_unlock(&info->mtx);
			ucmi_info_put(connect_type, devid);
			hw_err("UCMI addr fn failed. (connect_type=%u; devid=%u; ret=%d)\n", connect_type, devid, ret);
			return ret;
		}
	}

	mutex_unlock(&info->mtx);
	ucmi_info_put(connect_type, devid);
	return 0;
}

void ucmi_remove_all_dynamic_win(u32 connect_type, u32 devid)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return;
	}

	if (ops->remove_all_dynamic_win == NULL) {
		hw_err("Get remove_all_dynamic_win fn is NULL. (connect_type=%u)\n", connect_type);
		return;
	}

	return ops->remove_all_dynamic_win(devid);
}

int ucmi_get_win_mode(u32 connect_type, u32 devid, u32 *win_mode)
{
	const struct udev_ucmi_ops *ops = NULL;

	ops = udev_ucmi_ops_get(connect_type);
	if (ops == NULL) {
		hw_err("Get ops failed. (connect_type=%u)\n", connect_type);
		return -EINVAL;
	}

	if (ops->get_win_mode == NULL) {
		*win_mode = UOP_MODE_EXCLUSIVE; /* default exclusive */
		return 0;
	}

	return ops->get_win_mode(devid, win_mode);
}

EXPORT_SYMBOL_GPL(ucmi_assign_256m_win);
EXPORT_SYMBOL_GPL(ucmi_get_addr_info);
EXPORT_SYMBOL_GPL(ucmi_put_addr_info);
