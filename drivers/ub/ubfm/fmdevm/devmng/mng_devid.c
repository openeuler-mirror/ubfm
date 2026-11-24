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
#include "mng_devid.h"
#include <linux/errno.h>
#include <linux/export.h>

/*
 *	valid range of slot_id and socket_id: [0, 0xffff)
 *	since the socid default value 0xffffffff means unused, slot_id and socket_id cannot both be 0xffff. Therefore 0xffff is excluded from the valid range
 */
#define SOCID_SLOT_ID_MASK	0xffff0000U
#define SOCID_SLOT_ID_SHIFT   16
#define SOCID_SLOT_ID_MAX	 0xffffU
#define SOCID_SOCKET_ID_MASK  0x0000ffffU
#define SOCID_SOCKET_ID_MAX   0xffffU

static struct mng_devid {
	u32 devid_num;
	u32 devid2Socid[DEV_MAX_NUM];
} g_mng_devid = {
	.devid_num = 0,
	.devid2Socid = { [0 ... DEV_MAX_NUM - 1] = 0xffffffffU }
};
static DEFINE_MUTEX(g_devid_mutex);

bool is_slot_id_valid(u32 slot_id)
{
	return slot_id < SOCID_SLOT_ID_MAX;
}

bool is_socket_id_valid(u32 socket_id)
{
	return socket_id < SOCID_SOCKET_ID_MAX;
}

int mng_get_slot_id_socket_id(u32 devid, u32 *slot_id, u32 *socket_id)
{
	u32 socid = 0;

	mutex_lock(&g_devid_mutex);
	if (devid >= g_mng_devid.devid_num) {
		mutex_unlock(&g_devid_mutex);
		return -EINVAL;
	}
	socid = g_mng_devid.devid2Socid[devid];
	mutex_unlock(&g_devid_mutex);
	*slot_id =  (socid & SOCID_SLOT_ID_MASK) >> SOCID_SLOT_ID_SHIFT;
	*socket_id = socid & SOCID_SOCKET_ID_MASK;
	return 0;
}
EXPORT_SYMBOL_GPL(mng_get_slot_id_socket_id);

int mng_get_devid(u32 slot_id, u32 socket_id, u32 *devid)
{
	u32 socid = 0;
	u32 i = 0;

	if ((slot_id >= SOCID_SLOT_ID_MAX) || (socket_id >= SOCID_SOCKET_ID_MAX)) {
		return -EINVAL;
	}
	socid = ((slot_id << SOCID_SLOT_ID_SHIFT) & SOCID_SLOT_ID_MASK) | (socket_id & SOCID_SOCKET_ID_MASK);
	mutex_lock(&g_devid_mutex);
	for (i = 0; i < g_mng_devid.devid_num; i++) {
		if (socid == g_mng_devid.devid2Socid[i]) {
			mutex_unlock(&g_devid_mutex);
			*devid = i;
			return 0;
		}
	}
	mutex_unlock(&g_devid_mutex);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(mng_get_devid);

int mng_alloc_devid(u32 slot_id, u32 socket_id, u32 *devid)
{
	u32 socid = 0;
	u32 i = 0;

	if ((slot_id >= SOCID_SLOT_ID_MAX) || (socket_id >= SOCID_SOCKET_ID_MAX)) {
		return -EINVAL;
	}
	socid = ((slot_id << SOCID_SLOT_ID_SHIFT) & SOCID_SLOT_ID_MASK) | (socket_id & SOCID_SOCKET_ID_MASK);
	mutex_lock(&g_devid_mutex);
	for (i = 0; i < g_mng_devid.devid_num; i++) {
		if (socid == g_mng_devid.devid2Socid[i]) {
			mutex_unlock(&g_devid_mutex);
			*devid = i;
			return -EEXIST;
		}
	}
	if (g_mng_devid.devid_num >= DEV_MAX_NUM) {
		mutex_unlock(&g_devid_mutex);
		return -ENOMEM;
	}
	*devid = g_mng_devid.devid_num;
	g_mng_devid.devid2Socid[g_mng_devid.devid_num] = socid;
	g_mng_devid.devid_num++;
	mutex_unlock(&g_devid_mutex);
	return 0;
}

void mng_devid_2_socid_info_show(struct seq_file *seq)
{
	u32 i = 0;
	u32 socid, slot_id, socket_id;

	mutex_lock(&g_devid_mutex);
	for (i = 0; i < g_mng_devid.devid_num; i++) {
		socid = g_mng_devid.devid2Socid[i];
		slot_id =  (socid & SOCID_SLOT_ID_MASK) >> SOCID_SLOT_ID_SHIFT;
		socket_id = socid & SOCID_SOCKET_ID_MASK;
		seq_printf(seq, "devid=%-3u  slot_id=%-3u  socket_id=%-3u\n", i, slot_id, socket_id);
	}
	mutex_unlock(&g_devid_mutex);
}
