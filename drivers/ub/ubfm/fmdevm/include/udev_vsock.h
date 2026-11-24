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
#ifndef UDEV_VSOCK_H
#define UDEV_VSOCK_H

#include "ubfm_common.h"

enum {
	ENV_DEFAULT = 0,
	ENV_VSOCK_SERVER = 1,
	ENV_VSOCK_CLIENT = 2,
};

enum {
	VSOCK_CMD_REG_IRQ = 0,
	VSOCK_CMD_GET_VERSION,
	VSOCK_CMD_GET_IRQ,
	VSOCK_CMD_GET_HPA,
	VSOCK_CMD_GET_UBFABRIC_ADDR,
	VSOCK_CMD_MAX
};

struct vsock_msg_pkt {
	u32 cmd;
	union {
		u32 data[7];
		struct {
			u32 version;
		} get_ver_rsp;
		struct {
			u32 index;  /* [0, die_num) */
		} get_eid_req;
		struct {
			u32 id;
		} get_eid_rsp;
		struct {
			u32 irq_bitmap;  /* 4bit per die */
		} get_irq_rsp;
		struct {
			u32 seg_index;
		} get_hpa_req;
		struct {
			u64 hpa;
			u32 size;
			u32 is_share;
		} get_hpa_rsp;
		struct {
			u64 ubfabric_addr;
		} get_ubfabric_addr_rsp;
	} u;
};

int udev_get_env_mode(void);

typedef int (*vsock_cmd_handler)(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp);
int vsock_register_server_cmd_handler(u32 cmd, vsock_cmd_handler handler);

typedef void (*vsock_disconnect_handler)(void);
int vsock_register_disconnect_handler(vsock_disconnect_handler handler);

int vsock_client_init(void);
void vsock_client_exit(void);
int vsock_client_send_recv(struct vsock_msg_pkt *send_msg, struct vsock_msg_pkt *recv_msg);

#endif
