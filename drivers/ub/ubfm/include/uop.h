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
#ifndef UOP_H
#define UOP_H

#include <linux/module.h>

#include "ubfabric.h"
#include "ubfabric.h"
#include "fmdevm.h"

struct uop_win_ops {
	int (*get_idle_local_win)(u32 devid, u32 *local_win_id_start, u32 *local_win_num, u32 *local_win_size);
	int (*get_local_addr)(u32 devid, u32 local_win_id, u64 *local_addr);
	int (*cfg_win)(u32 devid, u32 local_win_id, u64 aligned_addr, u32 *axi_id_out);
	void (*clear_win)(u32 devid, u32 local_win_id);
};

void uop_kern_register_win_ops(struct uop_win_ops *ops);
void uop_kern_unregister_win_ops(void);

int uop_kern_update_share_local_win(u32 devid);

int uop_kern_add_memory(struct ures_inst *inst, const char *name, enum uop_item_type type,
						struct ures_addr_info *asic_addr, struct ures_addr_info *local_addr);

int uop_kern_writel(u32 devid, u64 addr, u32 val);
int uop_kern_readl(u32 devid, u64 addr, u32 *val);

int uop_kern_writeq(u32 devid, u64 addr, u64 val);
int uop_kern_readq(u32 devid, u64 addr, u64 *val);

int uop_kern_mem_write(u32 devid, u64 addr, void *data, u32 len);
int uop_kern_mem_read(u32 devid, u64 addr, void *data, u32 len);

int uop_kern_io_buffer_write(u32 devid, u64 addr, void *data, u32 len);
int uop_kern_io_buffer_read(u32 devid, u64 addr, void *data, u32 len);

void uop_kern_reset(u32 devid);
int uop_kern_send_recv_msg(u32 dev_id, u32 conn_type, void *ctx);
int uop_kern_register_msg_ops(u32 devid, const uop_msg_send_recv_t ops);
int uop_kern_unregister_msg_ops(u32 devid);
int uop_i3c_send_recv_kern(u32 devid, drv_ubnic_xfer_msg *msg);

#endif
