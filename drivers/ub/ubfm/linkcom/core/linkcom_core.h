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
#ifndef LINKCOM_CORE_H_
#define LINKCOM_CORE_H_

#include "ubfabric.h"
#include "linkcom_pub.h"


static inline void linkcom_fill_id_inst(struct linkcom_id_inst *id_inst, struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst)
{
	id_inst->dev_id = dev_inst->dev_id;
	id_inst->die_id = chan_inst->chan_id / LINKCOM_DIE_MAX_CHAN_NUM;
	id_inst->que_id = chan_inst->chan_id % LINKCOM_DIE_MAX_CHAN_NUM;
	id_inst->udie_version = dev_inst->udie_version;
	id_inst->chip_type = dev_inst->chip_type;
}

int linkcom_core_open_link_pkt(u32 dev_id, u32 chan_id, u32 pid, int *handle);
int linkcom_core_close_link_pkt(u32 dev_id, u32 chan_id, u32 pid);
int linkcom_core_send_link_pkt(u32 dev_id, u32 chan_id, struct mami_pkt_send_para para);
int linkcom_core_recv_link_pkt(u32 dev_id, u32 chan_id, int timeout, struct mami_pkt_recv_para *para);

int linkcom_core_read_rpkt(u32 dev_id, u32 chan_id);
bool linkcom_core_check_channel_status(const struct linkcom_dev_inst *dev_inst, struct linkcom_chan_inst *chan_inst);
void linkcom_event_signal(struct linkcom_chan_inst *chan_inst, u32 pkt_num);
void linkcom_core_chan_eventctx_recycle(struct linkcom_chan_inst *chan_inst);
int linkcom_core_bond_eventfd_inner(u32 dev_id, struct linkcom_open_chan_in *kin, struct linkcom_chan_inst *chan_inst);
#endif
