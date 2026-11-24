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
#ifndef LINKCOM_PKT_PARSE_H_
#define LINKCOM_PKT_PARSE_H_
#include "ubfm_common.h"
#include "linkcom.h"

#define LINKCOM_UDP_DST_PORT_UB 4792
#pragma pack(1)
typedef struct _linkcom_ub_tah {
	u16 udf_flg : 1;
	u16 rsv : 1;
	u16 poison : 1;
	u16 tk_vld : 1;
	u16 ee : 2;
	u16 taver : 2;
	u16 ta_opcode : 8;
	u16 src_tassn;
	u32 sjetty : 20;
	u32 sjt_type : 2;
	u32 excl : 1;
	u32 rsv_1 : 1;
	u32 alloc : 1;
	u32 retry : 1;
	u32 se : 1;
	u32 jetty_en : 1;
	u32 odr : 4;
} linkcom_ub_tah;

typedef struct _linkcom_ub_mem_tah {
	u16 udf_flg : 1;
	u16 rsv : 1;
	u16 poison : 1;
	u16 tk_vld : 1;
	u16 ee : 2;
	u16 taver : 2;
	u16 ta_opcode : 8;
	u16 src_tassn;
} linkcom_ub_mem_tah;

typedef struct _linkcom_ub_tph {
	u16 nlp : 4;
	u16 pad : 2;
	u16 tp_ver : 2;
	u16 tp_opcode : 8;
	u16 src_tpn_0; /* [23:8] */
	u16 dst_tpn_0 : 8; /* [23:16] */
	u16 src_tpn_1 : 8; /* [7:0] */
	u16 dst_tpn_1; /* [15:0] */
	u16 psn_0 : 8; /* [23:16] */
	u16 mn : 4;
	u16 reserved : 2;
	u16 ef : 1;
	u16 a : 1;
	u16 psn_1; /* [15:0] */
	u16 tpmsn_0 : 8; /* [23:8] */
	u16 rspinfo : 5;
	u16 rspst : 3;
	u16 tpmsn_1; /* [15:0] */
} linkcom_ub_tph;

typedef struct _linkcom_ub_stph {
	u16 nlp : 4;
	u16 tp_ver : 2;
	u16 pad : 2;
	u16 tp_opcode : 8;
	u16 msg_len;
} linkcom_ub_stph;

typedef struct _linkcom_ub_ctph {
	u8 nlp : 4;
	u8 pad : 2;
	u8 tp_opcode : 2;
} linkcom_ub_ctph;

typedef struct _linkcom_ub_upi_eid {
	u32 upi;
	u32 seid[4];
	u32 deid[4];
} linkcom_ub_upi_eid;

typedef struct _linkcom_ub_compact_upi_eid {
	u16 upi;
	u32 deid_0 : 12; /* [19 : 8] */
	u32 seid : 20;
	u8 deid_1; /* [7 : 0] */
} linkcom_ub_compact_upi_eid;

typedef struct _linkcom_ub_cip {
	u16 sn_0 : 8; /* [15 : 8] */
	u16 cip_id : 8;
	u16 rsv : 4;
	u16 nlp : 4;
	u16 sn_1 : 8; /* [7 : 0] */
} linkcom_ub_cip;

typedef struct _linkcom_ub_cfg6_nth {
	u16 plength : 14;
	u16 rm : 2;
	u16 scna;
	u16 dcna;
	u16 cc;
	u8 nlp : 3;
	u8 mgnt_bit : 1;
	u8 sl : 4;
	u8 lb;
} linkcom_ub_cfg6_nth;

typedef struct _linkcom_ub_cfg7_nth {
	u16 plength : 14;
	u16 rm : 2;
	u16 scna_0; /* [23 : 8] */
	u16 dcna_0 : 8; /* [23:16] */
	u16 scna_1 : 8; /* [0 : 7] */
	u16 dcna_1; /* [15:0] */
	u16 cc;
	u8 nlp : 3;
	u8 rsvd2 : 1;
	u8 sl : 4;
	u8 lb;
} linkcom_ub_cfg7_nth;

typedef struct _linkcom_ub_cfg9_nth {
	u16 plength : 14;
	u16 rm : 2;
	u16 scna;
	u16 dcna;
	u16 cc;
	u8 nlp : 3;
	u8 rsvd2 : 1;
	u8 sl : 4;
	u8 lb;
} linkcom_ub_cfg9_nth;

typedef struct _linkcom_ub_lh {
	u16 cfg : 4;
	u16 rsvd1 : 1;
	u16 vl : 4;
	u16 rsvd0 : 1;
	u16 crd_vl : 4;
	u16 ack : 1;
	u16 crd : 1;
} linkcom_ub_lh;

typedef struct _linkcom_udp {
	u16 src_port;
	u16 dst_port;
	u16 length;
	u16 check_sum;
} linkcom_udp;

typedef struct _linkcom_ipv4 {
	u8 version_ihl;
	u8 type_of_service;
	u16 total_length;
	u16 identification;
	u16 flags_frag;
	u8 ttl;
	u8 protocol;
	u16 header_checksum;
	u32 src_ip;
	u32 dst_ip;
} linkcom_ipv4;

typedef struct _linkcom_ipv6 {
	u32 version : 4;
	u32 traffic_class : 8;
	u32 flow_label : 20;
	u16 payload_length;
	u8 next_header;
	u8 hop_limit;
	u8 src_ip[16];
	u8 dst_ip[16];
} linkcom_ipv6;

typedef struct _linkcom_ub_cfg3_nth {
	u16 plength : 14;
	u16 rm : 2;
	u16 cc;
	u16 npi_0 : 9; /* npi[24 : 16] */
	u16 rsvd2 : 7;
	u16 npi_1; /* npi[15 : 0] */
} linkcom_ub_cfg3_nth;

typedef struct _linkcom_ub_cfg4_nth {
	u16 plength : 14;
	u16 rm : 2;
	u16 cc;
	u16 npi_0 : 9; /* [24 : 16] */
	u16 rsvd2 : 7;
	u16 npi_1; /* [15 : 0] */
} linkcom_ub_cfg4_nth;
#pragma pack()

enum linkcom_ub_tp_opcode {
	LINKCOM_UB_TP_OP_0 = 0,  /* unreliable transaction pkt */
	LINKCOM_UB_TP_OP_1 = 1,  /* reliable transaction pkt */
	LINKCOM_UB_TP_OP_2 = 2,  /* transport ack(TPACK) without CETPH */
	LINKCOM_UB_TP_OP_3 = 3,  /* transport ack with CETPH */
	LINKCOM_UB_TP_OP_4 = 4,  /* reserved */
	LINKCOM_UB_TP_OP_5 = 5,  /* transport sack without CETPH */
	LINKCOM_UB_TP_OP_6 = 6,  /* transport sack with CETPH */
	LINKCOM_UB_TP_OP_7 = 7,  /* reserved */
	LINKCOM_UB_TP_OP_8 = 8,  /* CNP */
	LINKCOM_UB_TP_OP_MAX,
};

enum linkcom_ub_cfg {
	LINKCOM_UB_CFG_0 = 0,  /* data link layer control packet */
	LINKCOM_UB_CFG_1 = 1,  /* reserved */
	LINKCOM_UB_CFG_2 = 2,  /* reserved */
	LINKCOM_UB_CFG_3 = 3,  /* IPV4 packet */
	LINKCOM_UB_CFG_4 = 4,  /* IPV6 packet */
	LINKCOM_UB_CFG_5 = 5,  /* network control packet */
	LINKCOM_UB_CFG_6 = 6,  /* inband control packet */
	LINKCOM_UB_CFG_7 = 7,  /* UB Domain network packet */
	LINKCOM_UB_CFG_8 = 8,  /* reserved */
	LINKCOM_UB_CFG_9 = 9,  /* UB Memory packet */
	LINKCOM_UB_CFG_MAX,
};

enum linkcom_reason {
	LINKCOM_CAM_HIT = 0,
	LINKCOM_PROTOCOL_CHECK_FAIL = 1,
	LINKCOM_ROUTE_MISS = 2,
	LINKCOM_NPI_CHECK_FAIL = 3,
	LINKCOM_ENUM_OR_HIT_LOCAL_MSG = 4,
	LINKCOM_MIRROR_TO_CTRLCPU = 5,
	LINKCOM_NCSI,
	LINKCOM_REASON_MAX,
};

void linkcom_recv_pkt_mask(struct mami_pkt_recv_para *para);

#endif
