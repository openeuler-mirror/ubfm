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
#ifndef LINKCOM_SOC_REG_H_
#define LINKCOM_SOC_REG_H_

#define TP_PPP_GLB_BASE	   (0x80000)
#define PPP_TP_REPORT_QID_CFG (0x340C)

#define TP_TPP_BASE			   (0xE0000)
#define TPP_CTRL_CPU_WQE_CI(qid)  (0x0C00 + 0x4 * (qid))
#define TPP_CTRL_CPU_WQE_PI(qid)  (0x0D00 + 0x4 * (qid))
#define TPP_CTRL_CPU_DROP_WQE_CNT(qid)  (0x2388 + 0x4 * (qid))
#define TPP_CTRL_CPU_DROP_CNT_MAX	   (0x10000)

#define MISC_LQC_IMP_BASE		  (0x30000)
#define IMP_CTRL_CPU_TWQE_CI(qid)  (0x5070 + 0x100 * (qid))
#define IMP_CTRL_CPU_TWQE_PI(qid)  (0x506C + 0x100 * (qid))
#define IMP_CTRL_CPU_RCQE_BUF(qid) (0x5060 + 0x100 * (qid))
#define IMP_CTRL_CPU_RPKT_BUF(qid) (0x505C + 0x100 * (qid))

#define IMP_CTRL_CPU_ESPI_INT_MASK(qid)  (0x503C + 0x100 * (qid))
#define IMP_CTRL_CPU_ESPI_INT_STS(qid)   (0x5040 + 0x100 * (qid))

#define LQC_TA_DM				  (0xC0000)
#define IMP_RPKT_DROP_CNT(qid)	 (0x5068 + 0x100 * (qid))
#define IMP_RPKT_BLOCK_CFG(qid)	(0x5064 + 0x100 * (qid))
#define DM_CFG_RSV_REG_0(qid)	(0x2000 + 0x4 * (qid))
#define DM_CFG_RSV_REG_4(qid)	(0x2010 + 0x4 * (qid))
#define DM_CFG_RSV_REG_8(qid)	(0x2020 + 0x4 * (qid))

#define IMP_DBG_RSV_REG_24_MASK	(0x1FFFFFFF)
#define IMP_DROP_INCREASE_MASK	 (0x60000000) /* bit30 & bit29 */

#define CTRL_CPU_MGTQ_OFFSET(qid)  (0x40000 * (qid))
#define CTRL_CPU_MGTQ_TPKT_BUF	 (0x00000)
#define CTRL_CPU_MGTQ_WQE		  (0x10000)
#define CTRL_CPU_MGTQ_RPKT_BUF	 (0x20000)
#define CTRL_CPU_MGTQ_CQE		  (0x30000)

#define CTRL_CPU_MGTQ_WQE_CI_MSK		(0xFFFF)
#define CTRL_CPU_MGTQ_WQE_ENTRY_OFFSET  (0x20)
#define CTRL_CPU_MGTQ_WQE_DEPTH		 (2)
#define CTRL_CPU_MGTQ_WQE_ENREY_LEN	 (28) /* byte */
#define CTRL_CPU_MGTQ_WQE_SIZE		  (CTRL_CPU_MGTQ_WQE_DEPTH * CTRL_CPU_MGTQ_WQE_ENREY_LEN)

#define CTRL_CPU_MGTQ_CQE_DEPTH_MASK  (0x7F)
#define CTRL_CPU_MGTQ_CQE_DEPTH	   (128)
#define CTRL_CPU_MGTQ_CQE_ENREY_LEN   (8) /* byte */
#define CTRL_CPU_MGTQ_CQE_SIZE		(CTRL_CPU_MGTQ_CQE_DEPTH * CTRL_CPU_MGTQ_CQE_ENREY_LEN)

#define CTRL_CPU_MGTQ_TPKT_CREDIT_NUM           (28)
#define CTRL_CPU_MGTQ_TPKT_PER_CREDIT_SIZE      (160)
#define CTRL_CPU_MGTQ_TPKT_CREDIT_TOTAL_SIZE    (CTRL_CPU_MGTQ_TPKT_CREDIT_NUM * CTRL_CPU_MGTQ_TPKT_PER_CREDIT_SIZE)

#define CTRL_CPU_MGTQ_RPKT_BUFF_ID_MASK	 (0x7F)
#define CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH	   (128)
#define CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN   (128) /* byte */
#define CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_SIZE  (CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH * CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN)

#define RD_CQE_LEN	   4 /* byte */
#define RD_RPKT_BUFF_LEN 4 /* byte */
#define WR_TPKT_BUFF_LEN 4 /* byte */

#define DMA_STS_LOW_ADDR_OFFSET  (0x24)
#define DMA_STS_HIGH_ADDR_OFFSET (0x28)
#define SQ_DW0_OFFSET			(0x0)
#define SQ_DW1_OFFSET			(0x4)
#define SQ_DW2_OFFSET			(0x8)
#define LOW_ADDR_MASK			(0x00000000FFFFFFFF)
#define LOW_ADDR_OFFSET		  (0)
#define HIGH_ADDR_MASK		   (0xFFFFFFFF00000000)
#define HIGH_ADDR_OFFSET		 (32)
#define PCIE_BDF_NUM		(0xB0100UL)
#define PCIE_WR_UCMI_BASE   (0x400000UL)
#define UCMI_WR_UCMI_OFFSET (0x1000UL)
#define PCIE_RD_UCMI_BASE   (0x408000)
#define UCMI_RD_UCMI_OFFSET (0x1000UL)

#define LINKCOM_MAX_CONFIG_TYPE (16U)
#define LINKCOM_MAX_VL_NUM	  (16U)

#define TX_PKT_WAIT_CNT	 (300)
#define TX_PKT_WAIT_TIME	(100) /* us */

/* only used for v2 */
#define CTRL_CPU_UDIE_MGTQ_CQE_DEPTH	   (128)
#define CTRL_CPU_NDIE_MGTQ_CQE_DEPTH	   (36)
#define CTRL_CPU_UDIE_MGTQ_RPKT_BUFF_DEPTH (128)
#define CTRL_CPU_NDIE_MGTQ_RPKT_BUFF_DEPTH (36)

#define LINKCOM_RPKT_NEXT_CI_INIT 0xFF /* initialized to an invalid value, used to determine whether the value has changed */
#define LINKCOM_BLOCK_CFG_INIT_VAL 0xFF /* initialized to an invalid value, used to determine whether the value has changed */

struct report_qid_cfg {
	u32 cfg_npi_err_qid : 3;
	u32 rsv0 : 1;
	u32 cfg_route_miss_qid : 3;
	u32 rsv1 : 1;
	u32 cfg_err_pkt_qid : 3;
	u32 rsv2 : 1;
	u32 cfg_loc_pkt_qid : 3;
	u32 rsv3 : 1;
	u32 cfg_enum_pkt_qid : 3;
	u32 rsv4 : 1;
	u32 cfg_msg_pkt_qid : 3;
	u32 rsv5 : 9;
};

struct tpp_ctrl_cpu_wqe_ci {
	u32 ctrl_cpu_qid_wqe_ci : 16;
	u32 rsv : 16;
};

struct tpp_ctrl_cpu_wqe_pi {
	u32 ctrl_cpu_qid_wqe_pi : 16;
	u32 rsv : 16;
};

struct tpp_ctrl_cpu_drop_wqe_cnt {
	u32 ctrl_cpu_qid_drop_wqe_cnt : 16;
	u32 rsv : 16;
};

struct imp_ctrl_cpu_twqe_ci {
	u32 ctrl_cpu_mgtq_twqe_ci : 1;
	u32 rsv : 31;
};

struct imp_ctrl_cpu_twqe_pi {
	u32 ctrl_cpu_mgtq_twqe_pi : 1;
	u32 rsv : 31;
};

struct imp_ctrl_cpu_rcqe_buf {
	u32 rcqe_pi : 10;
	u32 rsv1 : 6;
	u32 rcqe_ci : 10;
	u32 rsv2 : 5;
	u32 rcqe_empty : 1;
};

struct imp_ctrl_cpu_rpkt_buf {
	u32 rpkt_pi : 10;
	u32 rsv0 : 6;
	u32 rpkt_ci : 10;
	u32 rsv1 : 5;
	u32 rpkt_empty : 1;
};

struct imp_ctrl_cpu_block_cfg {
	u32 block_cfg : 10;
	u32 rsv0 : 6;
	u32 drop_en : 1;
	u32 drop_cfg : 10;
	u32 rsv1 : 5;
};

struct linkcom_debug_reg {
	u32 rsv_reg_0; /* records the packet entry offset caused by drop abnormality, used to calculate the real entry_id of the packet */
	u32 rsv_reg_4; /* records the number of drop abnormalities, and marks whether there is a drop abnormality in this round for processing in the next round */
	u32 rsv_reg_8; /* records the drop count, compared with the drop register in the next round to determine whether a drop occurred */
	u32 drop_cnt;  /* records the initial drop count of each round of receive processing, used to determine whether a drop occurred during receive */
};

struct fpga_sq_dw2 {
	u32 data_len : 16;
	u32 sq_idx : 2;
	u32 rsv1 : 6;
	u32 que_id : 5;
	u32 die_id : 1;
	u32 rsv2 : 2;
};

struct fpga_wr_data {
	struct linkcom_tx_wqe wqe;
	u64 pkt_addr;
};

struct fpga_rd_data {
	struct linkcom_rx_cqe cqe;
	u64 pkt_addr;
};

enum recv_flag {
	NORMAL_STS = 0, /* no drop abnormality occurred */
	ABNORMAL_NO_DROP_STS, /* drop abnormality occurred, judged no drop at the start of this round */
	ABNORMAL_WITH_DROP_STS, /* drop abnormality occurred, judged drop at the start of this round */
	MAX_RECV_STS,
};

struct read_pkt_ctx {
	u32 status;
	u32 next_pkt_ci;
	u64 imp_base;
	u64 rsv_base;
	u64 rpkt_base;
	struct linkcom_id_inst inst;
	struct linkcom_debug_reg dbg_reg;
	struct imp_ctrl_cpu_block_cfg cfg;
	struct imp_ctrl_cpu_rcqe_buf rcqe_reg;
	struct imp_ctrl_cpu_rpkt_buf rpkt_reg;
	struct linkcom_list_head *head;
};

u32 linkcom_get_mgtq_cqe_depth(struct linkcom_id_inst *id_inst);
u32 linkcom_get_mgtq_rpkt_buff_depth(struct linkcom_id_inst *id_inst);

u64 linkcom_get_lqc_imp_base_addr(struct linkcom_id_inst *id_inst);
u64 linkcom_get_lqc_ta_dm_base_addr(struct linkcom_id_inst *id_inst);
u64 linkcom_get_tp_base_addr(struct linkcom_id_inst *id_inst);
u64 linkcom_get_tp_tpp_base_addr(struct linkcom_id_inst *id_inst);
u64 linkcom_get_ctrl_cpu_mgtq_tx_buf_base(struct linkcom_id_inst *id_inst);
u64 linkcom_get_ctrl_cpu_mgtq_wqe_base(struct linkcom_id_inst *id_inst);
u64 linkcom_get_mgtq_cqe_base(struct linkcom_id_inst *id_inst);
u64 linkcom_get_mgtq_rpkt_buf_base(struct linkcom_id_inst *id_inst);

int linkcom_get_cqe_ptr(struct linkcom_id_inst *id_inst, u64 reg_base,
						struct imp_ctrl_cpu_rcqe_buf *rcqe_ptr, struct imp_ctrl_cpu_rpkt_buf *rpkt_ptr);
int linkcom_read_cqe(struct linkcom_id_inst *id_inst, u32 cqe_ci, struct linkcom_rx_cqe *cqe);
int linkcom_read_rpkt_buf(struct read_pkt_ctx *ctx, u32 entry_id, u32 pkt_len, u8 *recv_buff);
int linkcom_write_rpkt_ci(struct read_pkt_ctx *ctx);

#endif
