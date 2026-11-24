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
#ifndef MSGQ_CQ_H
#define MSGQ_CQ_H

#include "msgq_comm.h"
#include "msgq_rq.h"

#ifdef FPGA_MODE_ENABLE
#define MSGQ_CQ_TIMEOUT		 100000000	/* us */
#define MSGQ_CQ_TIMEOUT_UPGRADE 600000000	/* us */
#elif defined(EMU_ST)
#define MSGQ_CQ_TIMEOUT		 50000	   /* us */
#define MSGQ_CQ_TIMEOUT_UPGRADE 600000	  /* us */
#else
#define MSGQ_CQ_TIMEOUT		 8000000	/* us */
#define MSGQ_CQ_TIMEOUT_UPGRADE 60000000	/* us */
#endif

#define MSGQ_CQ_SLEEP_TIME	  50000	   /* us */

struct upgrade_req_msg_header {
	struct msg_pkt_header_kernel header;
	char payload[];
};

struct msgq_cq_opcode {
	u8 type:1; /* 0:req; 1:rsp */
	u8 msg_code:3;
	u8 sub_msg_code:4;
} __attribute__((packed));

/* msgq pkt */
struct msg_pkt {
	struct msg_pkt_header_kernel header;
	char payload[];
} __attribute__((packed));

int msgq_cq_wait(struct msgq_chan *chan, struct msgq_msg *msg, u32 msn, struct msgq_record *record);
int msgq_cq_deque(struct msgq_cq *cq);
int msgq_cq_clean(struct msgq_cq *cq);

struct msgq_cq *msgq_cq_create(u32 devid, u32 die_id, struct msgq_asic *cq_asic, struct msgq_asic *rq_asic);
void msgq_cq_destroy(struct msgq_cq *cq);

#endif
