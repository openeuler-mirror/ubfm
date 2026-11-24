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
#ifndef _UDEV_MSG_PUB_H
#define _UDEV_MSG_PUB_H

#include "ubfm_common.h"
#include "fmdevm.h"
#include "udev_msg_pub.h"
#include "mctrlq.h"
#include "mctrlq.h"

struct msg_cmd {
	u32 cmd_type;
	void *in_buf;
	u32 in_len;
	void *out_buf;
	u32 out_len;
};

struct dev_cfg_info_cmd {
	u32 cna;
	u32 eid;
};

struct dev_udie_info_cmd {
	u8 udie_idx;
};

struct dev_ue_info_cmd {
	u32 ue_num;
};

struct dev_ue_type_info_cmd {
	u32 ue_type_seg_num;
};
#pragma pack(1)
struct vdm_get_dev_info_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
};

struct vdm_get_die_info_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	u32 rsv1 : 16;
	u32 die_id : 16;
};

struct vdm_get_ue_info_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	u32 offset : 16; /* from which ue to start getting */
	u32 req_ue_num : 16; /* how many ues to get */
};

struct vdm_get_ue_type_req_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	u32 offset : 16; /* from which segment to start getting */
	u32 req_seg_num : 16; /* how many segments to get */
};

/* msgq pkt */
struct msg_pkt {
	struct msg_pkt_header_kernel header;
	char payload[];
};

struct udev_cfg_msg {
	u32 device_type;
	u32 slot_id;	 /* Slot number of the current board in the rack */
	u32 socket_id;   /* Number of the current soc on the current board */
	u32 rec;		 /* 8byte alignment is reserved for future use */
	u64 ras_err_status_block_addr;
	u32 ras_err_status_block_len;
	u64 black_box_addr;
	u32 black_box_len;
};

struct udev_msg_cfg_info {
	struct udev_cfg_hdr cfg_hdr;
	struct udev_cfg_msg cfg;
	u8 ubc_ue0_guid[MAMI_GUID_LEN];
	struct mami_eid ubc_ue0_eid;
	u32 primary_cna;
	u32 chip_num;
	u32 chip_udie_num;
};

struct vdm_get_dev_info_rsp_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	struct udev_msg_cfg_info data;
};

struct udev_msg_die_cfg {
	u32 die_id;
	u8 ubc_ue0_guid[MAMI_GUID_LEN];
	u8 cdma_ue0_guid[MAMI_GUID_LEN];
	u8 urma_ue0_guid[MAMI_GUID_LEN];
	u32 cdma_soft_ue0;
	u32 ue_segment_num : 8;
	u32 sup_ppp_msg : 1;
	u32 urma_soft_ue0 : 23;
	u32 port_num;
	u64 ub_io_decoder_hpa_base;
	u64 ub_io_decoder_hpa_size;
	u32 body_mask;
	u32 mar_num;
	struct udev_mar_info mar_info[UDEV_MAR_MAX_NUM];
	u32 mue_num : 16;
	u32 total_ue_num : 16;
};

struct udev_msg_ue_res_info {
	u32 mue_idx;
	u32 mue_uers0_sa_l;
	u32 mue_uers0_sa_h;
	u32 mue_uers0_ss;
	u32 mue_uers1_ss;
	u32 mue_uers2_ss;
	u32 vue_uers0_sa_l;
	u32 vue_uers0_sa_h;
	u32 vue_uers0_ss;
	u32 vue_uers1_ss;
	u32 vue_uers2_ss;
};

struct vdm_get_die_info_rsp_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	struct udev_msg_die_cfg data;
};

struct udev_mue_res_info {
	u16 vue_start;
	u16 vue_end;
	u16 mue_sys_page;
	u16 vue_sys_page;
	u32 mue_uers0_sa_l;
	u32 mue_uers0_sa_h;
	u32 mue_uers1_sa_l;
	u32 mue_uers1_sa_h;
	u32 mue_uers2_sa_l;
	u32 mue_uers2_sa_h;
	u32 mue_uers0_ss;
	u32 mue_uers1_ss;
	u32 mue_uers2_ss;
	u32 vue_uers0_sa_l;
	u32 vue_uers0_sa_h;
	u32 vue_uers1_sa_l;
	u32 vue_uers1_sa_h;
	u32 vue_uers2_sa_l;
	u32 vue_uers2_sa_h;
	u32 vue_uers0_ss;
	u32 vue_uers1_ss;
	u32 vue_uers2_ss;
};

struct udev_msg_mue_info {
	u8  mue_index;
	u8  mue_type;
	u8  vue_type;
	u8  ue_init  :1;
	u8  ue_enable:1;
	u8  port_id  :6;
	u8 mue_sub_type;
	u8 vue_sub_type;
	u16 vue_init : 1;
	u16 vue_enable : 1;
	u16 rsv :14;
	struct udev_mue_res_info mue_res_info;
	u32 rsv1;
};

struct udev_msg_ue_info {
	u8 mue_num;
	struct udev_msg_mue_info mue_info[];
};

struct udev_ue_res_info {
	u32 mue_num;
	struct udev_mue_res_info res_info[];
};

struct vdm_get_ue_info_rsp_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	u32 rsv1 : 16;
	u32 ue_num: 16;
	struct udev_msg_mue_info data[];
};

struct udev_ue_type_seg_rsp {
	u8 ue_type;
	u8 ue_sub_type;
	u16 ue_start;
	u16 ue_end;
	u16 rsv;
};

struct udev_msg_ue_type_rsp {
	u32 type_seg_num;
	struct udev_ue_type_seg_rsp type_info[];
};

struct vdm_get_ue_type_rsp_pld {
	u32 rsv : 16;
	u32 sub_cmd : 16;
	u32 rsv1 : 16;
	u32 segment_num : 16;
	struct udev_ue_type_seg_rsp data[];
};

struct msg_pkt_rsp {
	struct msg_pkt_header_kernel header;
	char payload[];
};
#pragma pack()

int udev_cfg_magic_check(struct udev_cfg_hdr *hdr);
int msgq_get_cfg(u32 dev_id, u32 conn_type, struct msg_cmd *cmd);
int udev_hw_dev_init_by_msg(u32 devid, int chip_type, int conn_type, int acc_type);
#endif
