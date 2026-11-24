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
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/vm_sockets.h>
#include <net/sock.h>
#include "ulog.h"
#include "udev_vsock.h"
#include "feature_loader.h"

#define MODULE_MNG "vsock"
#define vsock_err(fmt, ...)	  mami_err(MODULE_MNG, fmt, ##__VA_ARGS__)
#define vsock_warn(fmt, ...)	  mami_warn(MODULE_MNG, fmt, ##__VA_ARGS__)
#define vsock_info(fmt, ...)	  mami_info(MODULE_MNG, fmt, ##__VA_ARGS__)
#define vsock_debug(fmt, ...)	  mami_debug(MODULE_MNG, fmt, ##__VA_ARGS__)

#define VSOCK_PORT 12345

static struct socket *g_vsock_client;
static struct task_struct *g_vsock_server_thread;
static vsock_cmd_handler g_vsock_cmd_handler[VSOCK_CMD_MAX] = {0};
static vsock_disconnect_handler g_vsock_disconnect_handler;

int vsock_register_server_cmd_handler(u32 cmd, vsock_cmd_handler handler)
{
	if (cmd >= VSOCK_CMD_MAX || handler == NULL) {
		return -EINVAL;
	}

	g_vsock_cmd_handler[cmd] = handler;
	return 0;
}

int vsock_register_disconnect_handler(vsock_disconnect_handler handler)
{
	if (handler == NULL) {
		return -EINVAL;
	}

	g_vsock_disconnect_handler = handler;
	return 0;
}

STATIC int server_handle_request(struct socket *client)
{
	int ret;
	struct msghdr msg = {0};
	struct kvec vec = {0};
	struct vsock_msg_pkt req = {0}, resp = {0};

	vec.iov_base = &req;
	vec.iov_len  = sizeof(req);
	ret = kernel_recvmsg(client, &msg, &vec, 1, vec.iov_len, 0); /* 1 means use only 1 buffer, 0 means default blocking receive */
	if (ret == 0) { /* after connection is established, server blocks on the recv interface waiting for user-side messages, if the user-side actively closes the connection, the recv interface returns 0 */
		return -ECONNRESET;
	} else if (ret < 0) {
		vsock_err("vsock server recv msg failed. (ret=%d)\n", ret);
		return ret;
	}

	if (req.cmd >= VSOCK_CMD_MAX || g_vsock_cmd_handler[req.cmd] == NULL) {
		vsock_err("vsock server invalid params. (cmd=%d; handle_is_null=%d)\n", req.cmd,
			(g_vsock_cmd_handler[req.cmd] == NULL));
		return -EINVAL;
	}

	ret = g_vsock_cmd_handler[req.cmd](&req, &resp);
	if (ret != 0) {
		vsock_err("vsock server handle msg failed. (cmd=%d; ret=%d)\n", req.cmd, ret);
		return ret;
	}

	vec.iov_base = &resp;
	vec.iov_len  = sizeof(resp);
	ret = kernel_sendmsg(client, &msg, &vec, 1, vec.iov_len);/* 1 means use only 1 buffer */
	if (ret <= 0) {
		vsock_err("vsock server send msg failed. (cmd=%d; ret=%d)\n", resp.cmd, ret);
		return -EPIPE;
	}

	return 0;
}

STATIC int vsock_server_fn(void *data)
{
	int ret;
	struct socket *server = NULL, *client = NULL;
	struct sockaddr_vm sa = {0};

	ret = sock_create_kern(&init_net, AF_VSOCK, SOCK_STREAM, 0, &server);
	if (ret < 0) {
		vsock_err("vsock server create failed. (ret=%d)\n", ret);
		return ret;
	}

	sa.svm_family = AF_VSOCK;
	sa.svm_port   = VSOCK_PORT;
	sa.svm_cid	= VMADDR_CID_ANY;
	ret = kernel_bind(server, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0) {
		vsock_err("vsock server bind failed. (ret=%d)\n", ret);
		goto out;
	}

	ret = kernel_listen(server, SOMAXCONN);
	if (ret < 0) {
		vsock_err("vsock server listen failed. (ret=%d)\n", ret);
		goto out;
	}

	while (!kthread_should_stop()) {
		ret = kernel_accept(server, &client, 0);
		if (ret == -ERESTARTSYS) { /* server thread blocks on listen, at this point unloading ko will stop the thread, accept returns ERESTARTSYS */
			break;
		} else if (ret < 0) {
			vsock_err("vsock server accept failed. (ret=%d)\n", ret);
			continue;
		}

		if (!try_module_get(THIS_MODULE)) { /* after user-side connects, increase reference to prevent unloading ko */
			sock_release(client);
			continue;
		}

	while (server_handle_request(client) == 0) {
	} /* loop to process requests until an error occurs */

		if (g_vsock_disconnect_handler != NULL) { /* user-side disconnect handling, currently used to unregister interrupts */
			g_vsock_disconnect_handler();
		}

		vsock_info("vsock server release connection.\n");
		sock_release(client);
		module_put(THIS_MODULE);
	}

out:
	sock_release(server);
	g_vsock_server_thread = NULL;
	return 0;
}

int vsock_server_init(void)
{
	if (udev_get_env_mode() != ENV_VSOCK_SERVER) {
		return 0;
	}

	g_vsock_server_thread = kthread_run(vsock_server_fn, NULL, "vsock_server");
	if (IS_ERR(g_vsock_server_thread)) {
		vsock_err("Failed to start vsock server. (err=%ld)\n", PTR_ERR(g_vsock_server_thread));
		g_vsock_server_thread = NULL;
		return -EAGAIN;
	}

	return 0;
}

void vsock_server_uninit(void)
{
	if (udev_get_env_mode() != ENV_VSOCK_SERVER) {
		return;
	}

	if (g_vsock_server_thread) {
		kthread_stop(g_vsock_server_thread);
	}
}

int vsock_client_init(void)
{
	int ret;
	struct sockaddr_vm sa = {0};

	if (g_vsock_client) {
		return 0;
	}

	ret = sock_create_kern(&init_net, AF_VSOCK, SOCK_STREAM, 0, &g_vsock_client);
	if (ret < 0) {
		vsock_err("vsock create client failed. (ret=%d)\n", ret);
		return ret;
	}

	sa.svm_family = AF_VSOCK;
	sa.svm_port   = VSOCK_PORT;
	sa.svm_cid	= VMADDR_CID_HOST;
	ret = kernel_connect(g_vsock_client, (struct sockaddr *)&sa, sizeof(sa), 0);
	if (ret < 0) {
		vsock_err("vsock client connect failed. (ret=%d)\n", ret);
		sock_release(g_vsock_client);
		g_vsock_client = NULL;
		return ret;
	}

	return 0;
}

void vsock_client_exit(void)
{
	if (g_vsock_client) {
		kernel_sock_shutdown(g_vsock_client, SHUT_RDWR);
		sock_release(g_vsock_client);
		g_vsock_client = NULL;
	}
}

int vsock_client_check_msg(struct vsock_msg_pkt *send_msg, struct vsock_msg_pkt *recv_msg)
{
	if (send_msg == NULL || recv_msg == NULL) {
		return -EINVAL;
	}

	if (g_vsock_client == NULL) {
		return -ENOTCONN;
	}
	return 0;
}

int vsock_client_send_recv(struct vsock_msg_pkt *send_msg, struct vsock_msg_pkt *recv_msg)
{
	int ret;
	struct msghdr msg = {0};
	struct kvec vec = {0};

	ret = vsock_client_check_msg(send_msg, recv_msg);
	if (ret != 0) {
		return ret;
	}
	vec.iov_base = send_msg;
	vec.iov_len  = sizeof(*send_msg);
	ret = kernel_sendmsg(g_vsock_client, &msg, &vec, 1, vec.iov_len); /* 1 means use only 1 buffer */
	if (ret <= 0) {
		vsock_err("vsock client send msg failed. (cmd=%d, ret=%d)\n", send_msg->cmd, ret);
		return -EPIPE;
	}

	vec.iov_base = recv_msg;
	vec.iov_len  = sizeof(*recv_msg);
	ret = kernel_recvmsg(g_vsock_client, &msg, &vec, 1, vec.iov_len, 0); /* 1 means use only 1 buffer, 0 means default blocking receive */
	if (ret == -ERESTARTSYS) { /* when server is waiting for an interrupt, client will block on the recv interface, at this point close will close the client thread, the recv interface returns ERESTARTSYS */
		return ret;
	} else if (ret <= 0) {
		vsock_err("vsock client recv msg failed. (cmd=%d, ret=%d)\n", recv_msg->cmd, ret);
		return -EPIPE;
	}

	return 0;
}

DECLARE_FEATURE_AUTO_INIT(vsock_server_init, FEATURE_LOADER_STAGE_2);
DECLARE_FEATURE_AUTO_UNINIT(vsock_server_uninit, FEATURE_LOADER_STAGE_2);
