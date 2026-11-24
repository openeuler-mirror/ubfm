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
#ifndef LINKCOM_PUB_H_
#define LINKCOM_PUB_H_

#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/eventfd.h>
#include "ubfm_common.h"
#include "linkcom.h"
#include "linkcom.h"
#include "fmdevm.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
typedef u64 TASK_TIME_TYPE;
#else
typedef struct timespec TASK_TIME_TYPE;
#endif

#define LINK_PKT_FUNC_IDLE		LINK_PKT_FUNC_MAX
#define MAX_BUFF_CNT			  1024
#define LINK_PKT_BUFF_LEN_MAX	 (32 * 1024)
#define RECV_MAX_TIMEOUT		  30000 /* ms */

#define LINKCOM_FUNC_VALID	0
#define FUNC_OCCUPIED_STS  1
#define FUNC_IDLE_STS	  0

struct linkcom_id_inst {
	u32 dev_id;
	u32 die_id;
	u32 que_id;
	enum mami_udie_version udie_version;
	enum mami_dev_type chip_type;
};

struct linkcom_tx_wqe {
	u32 wqe_err_type : 8;
	u32 opcode : 8;
	u32 vl : 4;
	u32 rsv0 : 1;
	u32 ctrl_cpu_ind : 1;
	u32 soft_assign_ind : 1;
	u32 icrc_cs : 1;
	u32 dst_nl_id : 5;
	u32 dst_port_id : 3;
	u32 src_nl_id : 3;
	u32 src_port_id : 3;
	u32 ee_bits : 2;
	u32 pkt_buf_entry_id : 8;
	u32 pkt_len : 16;
	u32 cfg : 4;
	u32 cpu_qid : 4;
	u32 rsv1 : 24;
	u32 dip0 : 32;
	u32 dip1 : 32;
	u32 dip2 : 32;
	u32 dip3 : 32;
};

struct linkcom_rx_cqe {
	u32 rsv0 : 16;
	u32 reason : 4;
	u32 rsv1 : 4;
	u32 nl_id : 5;
	u32 port_id : 3;
	u32 pkt_len : 16;
	u32 pkt_buf_entry_id : 8;
	u32 queue_id : 3;
	u32 rsv2 : 5;
};

struct linkcom_list_node {
	u32 idx;
	struct linkcom_rx_cqe cqe;
	u8 *pkt_buff;
	u32 recv_len;
	struct list_head list;
};

struct linkcom_list_head {
	u32 count;
	struct list_head list;
};

struct linkcom_wait_queue {
	u32 flag;
	struct wait_queue_head head;
};

struct linkcom_chan_inst {
	u32 chan_id;
	u32 die_id;
	u32 que_id; /* 4 que per die, 0~3 */
	u32 app_pid;
	u32 occupy_status;
	u64 imp_base_addr;
	int eventfd;
	struct eventfd_ctx *eventctx;
	TASK_TIME_TYPE time;
	struct mutex lock;
	struct linkcom_wait_queue wq;
	struct linkcom_list_head head;
	struct delayed_work work;
	u32 work_run_flag;
};

struct linkcom_dev_inst {
	u32 dev_id;   /* destination device id */
	u32 die_num;
	u32 chan_num;
	u32 chip_id;
	enum mami_udie_version udie_version;
	enum mami_dev_type chip_type;
	enum udm_dev_location location;
	enum udev_connect_type connect_type;
	struct linkcom_chan_inst chan_inst[DIE_MAX_NUM * LINKCOM_DIE_MAX_CHAN_NUM];
};

#endif
