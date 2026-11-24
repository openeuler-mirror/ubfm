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
#ifndef UDEV_UCMI_OPS_H
#define UDEV_UCMI_OPS_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "udev_hw_pub.h"
#include "udev_ucmi_def.h"
#include "ures.h"

#define WIN_4M_BITMAP_NUM 2U
#define WIN_4M_BITS_NUM 64U
struct udev_ucmi_addr {
	const char *name;
	u32 devid;
	u64 chip_addr;
	u32 dieid;
	u32 size;
};

struct udev_ucmi_addr_info {
	/* original window name */
	char name[UCMI_WIN_NAME_LEN];
	u64 chip_addr;
	u32 size;
	u32 dieid;
	u32 axi_id; /* used for window release */
	u64 win_4m_bitmap[WIN_4M_BITMAP_NUM]; /* used for window release */

	/* stores the udev_ucmi_win window information allocated by ucmi */
	struct list_head win_list;
	struct list_head entry;
};

struct udev_ucmi_win {
	/* the name here will be named with _{index} suffix, not the original window name */
	char name[UCMI_WIN_NAME_LEN];
	u64 ucmi_base;
	u64 ucmi_off;
	u64 chip_addr;
	u32 size;

	/* points to the parent addr info, for easy access to parent node information */
	struct udev_ucmi_addr_info *info;
	struct list_head entry;
};

struct udev_ucmi_info {
	u32 devid;
	struct mutex mtx;
	/* stores all address information */
	struct list_head addr_list;
};

typedef int (*ucmi_init_fn)(void *priv_data);
typedef void (*ucmi_uninit_fn)(void *priv_data);
typedef int (*ucmi_master_setup_fn)(void);
typedef int (*ucmi_slave_setup_fn)(void);

typedef int (*ucmi_assign_win_fn)(struct udev_ucmi_addr *addr);
typedef int (*ucmi_assign_win_by_axi_fn)(u32 axi_id, struct udev_ucmi_addr *addr);
typedef int (*ucmi_assign_256m_win_fn)(u32 devid, u64 chip_addr, u32 *axi_id);
typedef void (*ucmi_remove_win_fn)(struct udev_ucmi_addr *addr);
typedef void (*ucmi_remove_all_dynamic_win_fn)(u32 devid);
typedef struct udev_ucmi_info *(*ucmi_info_get_fn)(u32 devid);
typedef void (*ucmi_info_put_fn)(u32 devid);
typedef int (*ucmi_get_win_mode_fn)(u32 devid, u32 *win_mode);

struct udev_ucmi_ops {
	ucmi_init_fn init;
	ucmi_uninit_fn uninit;
	ucmi_master_setup_fn master_setup;
	ucmi_slave_setup_fn slave_setup;
	ucmi_assign_win_fn assign_win;
	ucmi_assign_win_by_axi_fn assign_win_by_axi;
	ucmi_assign_256m_win_fn assign_256m_win;
	ucmi_remove_win_fn remove_win;
	ucmi_remove_all_dynamic_win_fn remove_all_dynamic_win;
	ucmi_info_get_fn info_get;
	ucmi_info_put_fn info_put;
	ucmi_get_win_mode_fn get_win_mode;
};

int ucmi_init(u32 connect_type, void *priv_data);
void ucmi_uninit(u32 connect_type, void *priv_data);
int ucmi_master_setup(u32 connect_type);
int ucmi_slave_setup(u32 connect_type);

int ucmi_assign_win(u32 connect_type, struct udev_ucmi_addr *addr);
int ucmi_assign_win_by_axi(u32 connect_type, u32 axi_id, struct udev_ucmi_addr *addr);
int ucmi_assign_256m_win(u32 connect_type, u32 devid, u64 chip_addr, u32 *axi_id);
void ucmi_remove_win(u32 connect_type, struct udev_ucmi_addr *addr);
/* during probe, two windows ucmi and dev_info are allocated, this interface removes all windows except ucmi and dev_info */
void ucmi_remove_all_dynamic_win(u32 connect_type, u32 devid);

struct udev_ucmi_addr_info *ucmi_get_addr_info(u32 connect_type, u32 devid, u64 chip_addr, const char *name);
void ucmi_put_addr_info(u32 connect_type, u32 devid);

typedef int (*ucmi_addr_fn)(struct ures_inst *inst, struct udev_ucmi_addr_info *info);
int ucmi_foreach_addr(u32 connect_type, u32 devid, struct ures_inst *inst, ucmi_addr_fn fn);

int udev_ucmi_ops_register(enum udev_connect_type type, const struct udev_ucmi_ops *ops);
void udev_ucmi_ops_unregister(enum udev_connect_type type);
const struct udev_ucmi_ops *udev_ucmi_ops_get(enum udev_connect_type type);
int ucmi_get_win_mode(u32 connect_type, u32 devid, u32 *win_mode);

#endif  /* end of UDEV_UCMI_OPS_H */
