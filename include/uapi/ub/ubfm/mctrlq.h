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
#ifndef MCTRLQ_DEF_H
#define MCTRLQ_DEF_H

#include <stdbool.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/time.h>
#endif

#include "ubfm_common.h"
#include "fmdevm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CTRLQ_MAX_BB_NUM		32U
#define CTRLQ_DATA_NUM		  29U

#define CTRLQ_CHAN_MAX_NUM	  18U

#define CTRLQ_CHAN_MAX_NUM_V2_KUNPENG_960	45U /* used by kunpeng960 */
#define CTRLQ_CHAN_MAX_NUM_ASCEND_V2   21U	 /* exclude ub controller、CCU、PMU */
#define CTRLQ_CHAN_MAX_NUM_SSD	  4U

#define CTRLQ_SEQ_MIN		   0x8000
#define CTRLQ_SEQ_MAX		   0xFFFF

#define CTRLQ_DEPTH_STRIDE	  8U

#define CHAN_BASE_DIFF 0x20000

#define CTRLQ_EVENT_LEN (sizeof(struct ctrlq_cqe) * CTRLQ_MAX_BB_NUM)

#define UB_CLAN_LINK_CFG 6

#define NTH_NLP_WITH_TPH 0
#define NTH_NLP_WITHOUT_TPH 1

#define CTPH_NLP_UPI_40BITS_UEID 2
#define CTPH_OPCODE_NOT_CNP 0
#define TAH_OPCODE_MSG 0x10

#define MSGQ_EVENT_LEN (256 - 10) /* sizeof(struct msgq_event) <= UED_MAX_EVENT_LEN */

enum ctrlq_chan_status {
	CTRLQ_CHAN_OFF = 0,
	CTRLQ_CHAN_ON,
};

enum cqe_type {
	CQE_TYPE_TP_MEM = 0x1,
	CQE_TYPE_DEV_REG,
	CQE_TYPE_ADDR_MEM,
	CQE_TYPE_QOS,
	CQE_TYPE_UBMEM,
	CQE_TYPE_UBPORT,
	CQE_TYPE_MAX
};

enum mami_kern_schan_op_type {
	MAMI_KERN_SCHAN_OP_REG = 0,
	MAMI_KERN_SCHAN_OP_UNREG,
	MAMI_KERN_SCHAN_OP_MAX,
};

enum mami_kern_schan_addr_type {
	MAMI_KERN_SCHAN_ADDR_REG = 0,
	MAMI_KERN_SCHAN_ADDR_TX,
	MAMI_KERN_SCHAN_ADDR_RX,
	MAMI_KERN_SCHAN_ADDR_MAX,
};

enum mami_msg_type {
	MAMI_MSG_TYPE_DEV_INFO = 0,
	MAMI_MSG_TYPE_UDIE_INFO = 1,
	MAMI_MSG_TYPE_UE_INFO = 2,
	MAMI_MSG_PASSTHRU = 3,
	MAMI_MSG_CMD_TPL_SCC = 4,
	MAMI_MSG_CMD_TPL_CCC = 5,
	MAMI_MSG_TYPE_UE_TYPE_INFO = 6,
	MAMI_MSG_TYPE_PPP_INFO = 7,
	MAMI_MSG_TYPE_MAX
};

struct mami_ctrlq_drop_cqe_event {
	uint32_t devid;
	uint32_t planeId;
	uint32_t fChanId;
	uint16_t cnt; /* drop cqe cnt */
	uint8_t rsv1[2]; /* 2-byte padding */
	uint64_t dropTime;
	uint32_t cqeType;
	uint32_t bbNum;
};

struct mami_kern_ctrlq_schan_handle_ops {
	void (*handle)(uint32_t devid, uint32_t die_id, void *rsv);
	uint32_t rsv[16];
};

struct mami_port_event {
	uint32_t scna : 24,
		: 8;
	uint32_t port_idx : 16,
		: 16;
	uint32_t die_id : 16,
		: 16;
	uint32_t rsd : 32;
} __attribute__((packed));

struct ctrlq_msg_head {
	u8 ver;
	u8 service_type;
	u8 bb_num; /* ctrlq drv fill */
	u8 op_code;
	u8 ret;
	u16 seq;
	u8 vue_id; /* 0-n */
	u16 mue_id; /* idev_id */
	u16 rsv;
} __attribute__((packed));

struct ctrlq_msg {
	u32 len;
	void *data;
};

enum ctrlq_chan_type {
	CTRLQ_CHAN_URMA = 0,
	CTRLQ_CHAN_CDMA
};

typedef struct {
	u32 chan_start;
	u32 chan_end;
	u32 ue_start;
	u32 ue_end;
	u32 urma_or_cdma_chan_start;
	u32 ctrlq_cfg_base;
	enum ctrlq_chan_type type;
} ctrlq_ue_map_t;

struct ctrlq_global_info {
	u32 devid;
	u32 die_num;
	u32 chip_type;
	u32 max_chan_num;
	u32 valid_chan_num;
	enum mami_udie_version udie_version;
};

struct ctrlq_sqe {
	u8 ver;
	u8 type;
	u8 bb_num;
	u8 data[CTRLQ_DATA_NUM]; /* Data must be in last of struct */
} __attribute__((packed));

struct ctrlq_cqe {
	u8 ver;
	u8 type;
	u8 bb_num;
	u8 data[CTRLQ_DATA_NUM]; /* Data must be in last of struct */
} __attribute__((packed));


struct ctrlq_event {
	u32 die_id;
	u32 chan_id;
	u32 len;
	u64 timestamp_ns;
	u8 data[CTRLQ_EVENT_LEN]; /* Data must be in last of struct */
} __attribute__((packed));

struct ctrlq_asic {
	u64 base;   /* Uop addr for read base mem */
	u64 depth;  /* Uop addr for read depth reg */
	u64 tail;   /* Uop addr for read tail reg */
	u64 head;   /* Uop addr for read head reg */
	u64 intr;   /* Uop addr for int gen */
	u64 int_mask;
};

struct ctrlq_chan_info {
	u32 chan_id;
	u32 sqe_depth;
	struct ctrlq_asic sq_asic;
};

struct ctrlq_die_chan_info_kernel {
	u32 die_id;
	u32 valid_chan_num;
	struct ctrlq_chan_info chan_info[];
};

enum msgq_type {
	MSGQ_TYPE_PROTOCOL = 0,
	MSGQ_TYPE_ENUM,
	MSGQ_TYPE_PRIV,
	MSGQ_TYPE_MAX
};

struct msgq_msg {
	enum msgq_type type;
	u32 opcode;
	u32 local;
	u32 dev_type;
	u32 disable_log;

	u32 req_len;
	void *req_data;
	u32 rep_len;
	void *rep_data;
};


struct msgq_event {
	u32 die_id;
	u16 msn;
	u32 len;
	u64 timestamp_ns;
	u8 data[MSGQ_EVENT_LEN]; /* Data must be in last of struct */
} __attribute__((packed));

enum msgq_id {
	MSGQ_ID_BUSDRV = 0,
	MSGQ_ID_BIOS,
	MSGQ_ID_FM, /* 0x2: fm msgq exclusively allocated for mami management plane use */
	MSGQ_ID_MCTP,
	MSGQ_ID_MAX
};

/* VDM_SUB_MSG_CODE_VENDER
 * msg opcode
 */
enum vdm_vender_opcode {
	VDM_VENDER_OPCODE_FM_TO_NODE = 0,
	VDM_VENDER_OPCODE_NODE_TO_FM = 1,
	VDM_VENDER_OPCODE_NODE_TO_NODE = 2,
	VDM_VENDER_OPCODE_FIRMWARE = 3,
};

/* UB_MSG_CODE_VDM
 * sub msg code
 */
enum vdm_sub_msg_code {
	VDM_SUB_MSG_CODE_MCTP = 0,
	VDM_SUB_MSG_CODE_UVB = 0x1,
	VDM_SUB_MSG_CODE_VENDER = 0xF,
};

/* VDM_VENDER_OPCODE_NODE_TO_FM
 * sub opcode
 */
enum vdm_node_to_fm_sub_opcode {
	VDM_NODE_TO_FM_UE_ENABLE = 1,
	VDM_NODE_TO_FM_BUS_CONTROLLER_PORT_RESET = 2,
	VDM_NODE_TO_FM_UE_RESET_NOTIFY = 3,
	VDM_NODE_TO_FM_UBMEM_PORT_RESET = 4,
	VDM_NODE_TO_FM_CMDQ_ISOLATION = 5,
	VDM_NODE_TO_FM_HCCL_FAULT_EVENT = 7,  /* name to be corrected */
};

/* VDM_VENDER_OPCODE_FM_TO_NODE
 * sub opcode
 */
enum vdm_fm_to_node_sub_opcode {
	VDM_FM_TO_NODE_PDEV_REG = 1,
	VDM_FM_TO_NODE_PDEV_UNREG = 2,
	VDM_FM_TO_NODE_VDEV_REG = 3,
	VDM_FM_TO_NODE_VDEV_UNREG = 4,
	VDM_FM_TO_NODE_BUSINSTANCE_CREATE = 5,
	VDM_FM_TO_NODE_UPGRADE = 6,
	VDM_FM_TO_NODE_PORT_MNG = 7,
	VDM_FM_TO_NODE_URMA_DEV_ATTR_MNG = 8,
	VDM_FM_TO_NODE_UBMEM_PORT_WARN = 12,
	VDM_FM_TO_NODE_UBMEM_PORT_RECOVERY = 13,
	VDM_FM_TO_NODE_GET_DEV_ATTR = 14,
	VDM_FM_TO_NODE_USER_GROUP_CFG = 16,
	VDM_FM_TO_NODE_CMDQ_ISOLATION = 18,
	VDM_FM_TO_NODE_CMDQ_UB_TRACE = 19,
	VDM_FM_TO_NODE_UVB_OVER_UB = 20,
};

enum user_group_sub_cmd {
	UVB_SET_USER_GROUP_HEADER_ADDR = 1,
	UVB_REFRESH_INFO_PREPARE = 2,
	UVB_REFRESH_INFO_DONE = 3,
};

/* PROTOCOL msg code */
enum ub_msg_code {
	UB_MSG_CODE_RAS = 0,
	UB_MSG_CODE_LINK = 1,
	UB_MSG_CODE_CFG = 2,
	UB_MSG_CODE_VDM = 3,
	UB_MSG_CODE_EXCH = 4,
	UB_MSG_CODE_SEC = 5,
	UB_MSG_CODE_POOL = 6,
	UB_MSG_CODE_MAX = 7,
};

/* UB_MSG_CODE_PORT
 * sub msg code
 */
enum port_sub_msg_code {
	PORT_SUB_MSG_CODE_LINKUP = 0,
	PORT_SUB_MSG_CODE_LINKDOWN = 1,
	PORT_SUB_MSG_REMOTE_LINKUP = 4,
};

/* package */
struct ub_link_header {
	u32 plen:14;
	u32 rm:2;
	u32 cfg:4;
	u32 rsvd1:1;
	u32 vl:4;
	u32 rsvd0:1;
	u32 crd_vl:4;
	u32 ack:1;
	u32 crd:1;
};

struct clan_network_header {
	/* DW0 */
	u32 dcna:16;
	u32 scna:16;
	/* DW1 */
	u32 nth_nlp:3;
	u32 mgmt:1;
	u32 sl:4;
	u32 lb:8;
	u32 cc:16;
};

enum msg_extended_header_type {
	MSGETAH_REQ = 0,
	MSGETAH_RSP = 1,
};

struct msg_extended_header {
	u32 plen:12;
	u32 rsvd:4;
	u32 rsp_status:8;
	union {
		struct {
			u8 type:1; /* 0:req 1:rsp */
			u8 msg_code:3;
			u8 sub_msg_code:4;
		};
		u8 code;
	};
} __attribute__((packed));

struct msg_pkt_header_kernel { /* little-endian */
	/* DW0 */
	struct ub_link_header ulh;
	/* DW1-DW2 */
	struct clan_network_header nth;
	/* DW3 */
	u32 seid_h:8;
	u32 upi:16;
	u32 ctph_nlp:4; /* tp header */
	u32 pad:2;
	u32 tp_opcode:2;
	/* DW4 */
	u32 deid:20;
	u32 seid_l:12;
	/* DW5 */
	u32 src_tassn:16; /* msgq */
	u32 taver:3;
	u32 tk_vld:1;
	u32 udf:4;
	u32 ta_opcode:8;
	/* DW6 */
	u32 msgq_id:8; /* msgq */
	u32 rsvd0:24; /* Just don't care */
	/* DW7 */
	struct msg_extended_header msgetah;
} __attribute__((packed));

struct msgq_sqe {
	/* DW0 */
	u32 task_type : 2;
	u32 rsvd0 : 2;
	u32 local : 1;
	u32 dev_type : 2;
	u32 icrc : 1;
	u8 opcode;
	u32 p_len : 12;
	u32 rsvd1 : 4;

	/* DW1 */
	u32 msn : 16;
	u32 rsvd3 : 16;

	/* DW2 */

	u32 p_addr;
	/* DW3 */
	u32 rsvd2;
} __attribute__((packed));

struct msgq_cqe {
	/* DW0 */
	u32 task_type : 2;
	u32 rsvd0 : 6;
	u8 opcode;
	u32 p_len : 12;
	u32 rsvd1 : 4;

	/* DW1 */
	u32 msn : 16;
	u32 rsvd5 : 16;

	/* DW2 */
	u32 rq_pi : 10;
	u32 rsvd2 : 6;
	u32 status : 8;
	u32 rsvd3 : 8;

	/* DW3 */
	u32 rsvd4;
} __attribute__((packed));

/* vdm payload */
struct vdm_guid {
	/* DW0 */
	u32 class_code : 16;
	u32 device_id : 16;
	/* DW1 */
	u32 version : 4;
	u32 type : 4;
	u32 oui : 24;
};

struct vdm_pkt_header {
	/* DW0~1 */
	struct vdm_guid guid;
	/* DW2 */
	u32 rsv:12;
	u32 msg_version:4;
	u32 opcode:6;
	u32 sub_opcode:10;
};

static inline bool ctrlq_is_mami_seq(u32 seq)
{
	if (seq >= CTRLQ_SEQ_MIN && seq <= CTRLQ_SEQ_MAX) {
		return true;
	}
	return false;
}

static inline bool chip_is_kunpeng960(u32 chip_type)
{
	if ((chip_type == MAMI_KERN_TYPE_KUNPENG960_1P) || (chip_type == MAMI_KERN_TYPE_KUNPENG960_2P) ||
		(chip_type == MAMI_KERN_TYPE_KUNPENG960_4P)) {
		return true;
	}
	return false;
}

static inline bool chip_is_ssd(u32 chip_type)
{
	if (chip_type == MAMI_KERN_TYPE_HSSU_V100) {
		return true;
	}
	return false;
}

static inline void ctrlq_get_max_and_valid_chan_num(enum mami_udie_version version, u32 chip_type, u32 *valid_chan_num, u32 *max_chan_num)
{
	if (version == UDIE_VERSION_V2) {
		if (chip_is_kunpeng960(chip_type)) {
			*max_chan_num = CTRLQ_CHAN_MAX_NUM_V2_KUNPENG_960;
			*valid_chan_num = *max_chan_num;
		} else {
			*max_chan_num = CTRLQ_CHAN_MAX_NUM_ASCEND_V2;
			*valid_chan_num = *max_chan_num;
		}
	} else {
		*max_chan_num = (chip_type != MAMI_KERN_TYPE_HSSU_V100) ? CTRLQ_CHAN_MAX_NUM : CTRLQ_CHAN_MAX_NUM_SSD;
		*valid_chan_num = *max_chan_num - 1;
	}
}

static inline u64 ctrlq_addr_combine(u32 high32bit, u32 low32bit)
{
	/* Combine high 32 bit and low bit */
	return (((u64)high32bit << 32) | low32bit);
}

#ifdef __cplusplus
}
#endif

#endif
