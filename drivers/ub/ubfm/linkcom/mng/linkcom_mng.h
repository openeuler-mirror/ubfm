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
#ifndef LINKCOM_MNG_H_
#define LINKCOM_MNG_H_

#include "linkcom_pub.h"

struct linkcom_dev_inst *linkcom_get_dev_inst(u32 devid);
void linkcom_put_dev_inst(u32 devid);

void linkcom_core_release_lst_rsrc(struct linkcom_chan_inst *chan_inst);

void linkcom_sched_recv_pkt_work(struct delayed_work *work, u32 *work_run_flag);
void linkcom_flush_recv_pkt_work(struct delayed_work *work, u32 *work_run_flag);
void linkcom_start_recv_pkt_work(u32 *work_run_flag);
void linkcom_pause_recv_pkt_work(u32 *work_run_flag);

int linkcom_add_list_node(struct linkcom_list_head *head, struct linkcom_rx_cqe cqe, u8 *pkt_buff);
int linkcom_pop_list_node(struct linkcom_list_head *head, u32 chan_id, struct linkcom_rx_cqe *cqe, u8 *pkt_buff, u32 buf_len);
void linkcom_del_all_list_node(struct linkcom_list_head *head, u32 chan_id);

int linkcom_mng_init(void);
void linkcom_mng_uninit(void);

int linkcom_add_dev(u32 devid);
int linkcom_del_dev(u32 devid);

int linkcom_get_ec_status(u32 devid, bool *is_before_ec);
int linkcom_set_ec_status_to_ures(u32 devid, bool is_before_ec);

#endif
