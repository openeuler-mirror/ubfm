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
#include <linux/kernel.h>

#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"
#include "ras_log.h"
#include "apei_tbl_load.h"
#include "local_ras_register.h"

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"TIMEOUT_TH"},
	{"TIMEOUT_ADDR"}, {"CFG_BUS_AXI_AWFIFO_DFX"},
	{"CFG_BUS_AXI_WFIFO_DFX"}, {"CFG_BUS_AXI_BFIFO_DFX"},
	{"CFG_BUS_AXI_ARFIFO_DFX"}, {"CFG_BUS_AXI_RFIFO_DFX"},
	{"CFG_REQ_FIFO_DFX"}, {"CFG_RSP_FIFO_DFX"},
	{"UMMU_REQ_FIFO_DFX"}, {"UMMU_RSP_FIFO_DFX"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"DFX_IMP_CMT_PC_0"},
	{"DFX_IMP_CMT_PC_1"}, {"DFX_IMP_MCU"},
	{"DFX_IMP_MCU_AW_STATUS"}, {"DFX_IMP_MCU_AR_STATUS"},
	{"DFX_IMP_MCU_LAST_AWADDR"}, {"DFX_IMP_MCU_LAST_ARADDR"},
	{"DFX_IMP_MST_LAST_AWADDR_LOW"}, {"DFX_IMP_MST_LAST_AWADDR_HIGH"},
	{"DFX_IMP_MST_LAST_ARADDR_LOW"}, {"DFX_IMP_MST_LAST_ARADDR_HIGH"},
	{"IMP_DBG_RSV_REG_0"}, {"IMP_DBG_RSV_REG_1"},
	{"IMP_DBG_RSV_REG_2"}, {"IMP_DBG_RSV_REG_3"},
	{"IMP_DBG_RSV_REG_4"}, {"IMP_DBG_RSV_REG_5"},
	{"IMP_DBG_RSV_REG_6"}, {"IMP_DBG_RSV_REG_7"},
	{"IMP_DBG_RSV_REG_8"}, {"IMP_DBG_RSV_REG_9"},
	{"IMP_DBG_RSV_REG_10"}, {"IMP_DBG_RSV_REG_11"},
	{"IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP_DTCM1_1BIT_ERR_CNT"},
	{"IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP_DTCM0_1BIT_ERR_CNT"},
	{"MGTQ_MEM_ECC_ERR_ADDR"}, {"MGTQ_MEM_1BIT_ERR_CNT"},
	{"CMDQ_MEM_ECC_ERR_ADDR"}, {"CMDQ_MEM_1BIT_ERR_CNT"},
	{"IMP_CTRL_CPU_MGTQ_TPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_TPKT_1BIT_ERR_CNT"},
	{"IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RPKT_1BIT_ERR_CNT"},
	{"IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RCQE_1BIT_ERR_CNT"},
	{"IMP_DTCM1_2BIT_ERR_CNT"}, {"IMP_DTCM0_2BIT_ERR_CNT"},
	{"MGTQ_MEM_2BIT_ERR_CNT"}, {"CMDQ_MEM_2BIT_ERR_CNT"},
	{"IMP_CTRL_CPU_MGTQ_TPKT_2BIT_ERR_CNT"}, {"IMP_CTRL_CPU_MGTQ_RPKT_2BIT_ERR_CNT"},
	{"IMP_CTRL_CPU_MGTQ_RCQE_2BIT_ERR_CNT"}, {"IMP_AHB_SLV_ERR_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_ba_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"DEVICE_RAS_STATUS_1"},
	{"DEVICE_RAS_STATUS_2"}, {"DEVICE_RAS_STATUS_3"},
	{"DEVICE_RAS_STATUS_4"}, {"DEVICE_RAS_STATUS_5"},
	{"LSAD_MEM_ECC_1B_INFO"}, {"LSAD_MEM_ECC_2B_INFO"},
	{"DFX_TP_LSA_REQ_STATUS3"}, {"LSAD_CCUA_SEQ_NS_CHECK_FAIL_CNT"},
	{"LSAD_CCUA_SEQ_RXDATA_POISON_CNT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_reg[] = {
	{"RAS_STATUS_0"}, {"RAS_STATUS_1"},
	{"RAS_STATUS_2"}, {"RAS_STATUS_3"},
	{"SSU_EG_P2P_HOS_BYPASS_FIFO_INT"}, {"SSU_EG_P2P_BYPASS_FIFO_INT_EN"},
	{"SSU_EG_MAC_FIFO_INT_EN"}, {"SSU_EG_MAC_FIFO_INT"},
	{"SSU_EG_MAC_FIFO_INT_STICKY"}, {"SSU_EG_BYPASS_FIFO_INT_STICKY"},
	{"SSU_EG_MEM_ECC_SERR_INT"}, {"SSU_EG_MEM_ECC_SERR_INT_EN"},
	{"SSU_EG_RX_HOS_BPS_MEM_ECC_ERR_ADDR"}, {"SSU_EG_RX_P2P_BPS_MEM_ECC_ERR_ADDR"},
	{"SSU_EG_MEM_ECC_MERR_INT"}, {"SSU_EG_MEM_ECC_MERR_INT_EN"},
	{"SSU_PORT_BASED_ERR_INT"}, {"SSU_PORT_BASED_ERR_INT_EN"},
	{"SSU_PORT_BASED_ERR_INT_STICKY"}, {"SSU_PORT_BASED_ERR_INT"},
	{"SSU_PORT_BASED_ERR_INT_EN"}, {"SSU_PORT_BASED_ERR_INT_STICKY"},
	{"LINK_CREDIT_PRIVATE_CNT_INT"}, {"LINK_CREDIT_PRIVATE_CNT_INT_EN"},
	{"LINK_CREDIT_PRIVATE_CNT_INT_STICKY"}, {"LINK_CREDIT_SHARE_TC_CNT_INT"},
	{"LINK_CREDIT_SHARE_TC_CNT_INT_EN"}, {"LINK_CREDIT_SHARE_TC_CNT_INT_STICKY"},
	{"LINK_CREDIT_SHARE_TCG_CNT_INT"}, {"LINK_CREDIT_SHARE_TCG_CNT_INT_EN"},
	{"LINK_CREDIT_SHARE_TCG_CNT_STICKY"}, {"UB_PKT_CHK_CFG"},
	{"PA_UB_LEN_ERR_STS"}, {"PA_UB_COM_ERR_STS"},
	{"PA_UB_FLD_ERR_STS"}, {"UB_PKT_CHK_CFG"},
	{"PA_UB_LEN_ERR_STS"}, {"PA_UB_COM_ERR_STS"},
	{"PA_UB_FLD_ERR_STS"}, {"PFA_INT_SRC_PKT"},
	{"PFA_INT_SRC_PKT"}, {"PFA_INT_SRC_FIFO"},
	{"PFA_INT_SRC_FIFO_DL"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"DEVICE_RAS_STATUS_1"},
	{"DEVICE_RAS_STATUS_2"}, {"DEVICE_RAS_STATUS_3"},
	{"DEVICE_RAS_STATUS_4"}, {"SSU_EG_HOS_FIFO_INT_STICKY"},
	{"SSU_IGEG_INT"}, {"SSU_IGEG_ERR_INT_STICKY"},
	{"TXDMA_OVERSHOOT_INT"}, {"OQ_OQM_ECC_1BIT_INT"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_0"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_1"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_2"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_3"},
	{"OQ_OQM_ECC_2BIT_INT"}, {"QM_FIFO_OVERFLOW_INT_STICKY"},
	{"QM_FIFO_UNDERFLOW_INT_STICKY"}, {"OQ_OQM_TX_QUEUE_OVERFLOW_INT"},
	{"OQ_OQM_RX_QUEUE_OVERFLOW_INT"}, {"OQ_OQM_TX_QUEUE_EMPTY_DEQ_INT"},
	{"OQ_OQM_RX_QUEUE_EMPTY_DEQ_INT"}, {"OQSCHBMCREDITCNTINT_STICKY"},
	{"SSU_MEM_ECC_SERR"}, {"SSU_MEM_ECC_INFO"},
	{"SSU_MEM_ECC_MERR"}, {"SSU_BM_INT"},
	{"NL_RXPA_GLBPA_P2P_PATH_CREDIT_CFG"}, {"PA_SWITCH_MODE_CFG"},
	{"NL_RXPA_GLBPA_EXCP_STATUS"}, {"PA_MEM_INIT"},
	{"PA_MEM_ECC_SERR"}, {"PA_MEM_ECC_INFO"},
	{"PA_MEM_ECC_MERR"}, {"PA_LINEAR_TBL_CFG"},
	{"PA_ROUTING_TBL_CFG"}, {"SEQ_CTRL_CRDT_OVERFLOW_0"},
	{"SEQ_CTRL_CRDT_OVERFLOW_1"}, {"SEQ_CTRL_CRDT_OVERFLOW_2"},
	{"SEQ_CTRL_CRDT_OVERFLOW_3"}, {"SEQ_CTRL_CRDT_OVERFLOW_4"},
	{"SEQ_CTRL_CRDT_UNDERFLOW_0"}, {"SEQ_CTRL_CRDT_UNDERFLOW_1"},
	{"SEQ_CTRL_CRDT_UNDERFLOW_2"}, {"SEQ_CTRL_CRDT_UNDERFLOW_3"},
	{"SEQ_CTRL_CRDT_UNDERFLOW_4"}, {"SEQ_CTRL_BP_WATERLINE"},
	{"SEQ_CTRL_BP_BYPS_WATERLINE"}, {"INT_STATUS"},
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_reg[] = {
	{"LQC_DLMAC_RAS_INT_DL_SRC"}, {"LQC_DLMAC_RAS_INT_PFA_SRC"},
	{"CFG_ACK_TIMEOUT"}, {"CFG_RECV_ACK_RETURN"},
	{"CFG_RETRY_THRESHOLD"}, {"CFG_REINIT_CNT_PERIOD"},
	{"CFG_RETRAIN_CNT_PERIOD"}, {"CFG_TX_BP_TIMEOUT"},
	{"DL_RETRY_MEM_ECC_INFO"}, {"DL_PMU_MEM_ECC_INFO"},
	{"DL_RX_DBUF_MEM_ECC_INFO"}, {"CFG_BA_DUF_ECC_INJECT_CTRL"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_reg[] = {
	{"LQC_MUXPCS_RAS_INT_PCS_SRC"}, {"LQC_MUXPCS_RAS_INT_MACMUX_SRC"},
	{"LQC_MUXPCS_RAS_INT_FEC_SRC"}, {"LQC_MUXPCS_RAS_INT_MD_SRC"},
	{"ST_SDS_PW_STATUS2"}, {"ST_PCS_TX_FIFO_STATUS1"},
	{"ST_UB_STATUS4"}, {"ST_MD_RX_DESKEW_1BIT_ECC_ADD"},
	{"ST_CH_TRAINING"}, {"PHY_LINK_STA_1"},
	{"PHY_LINK_STA_2"}, {"PORT_LINK_STA"},
	{"ST_UB_DYNAMIC_LANE_INFO1"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_reg[] = {
	{"TP_DAM_ECC1B"}, {"TP_DAM_ECC2B"},
	{"TP_DAM_AXI_ALARM"}, {"TP_DAM_ECC1B_INFO"},
	{"TP_DAM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_reg[] = {
	{"TP_EUM_ECC1B"}, {"TP_EUM_ECC2B"},
	{"TP_EUM_AXI_ALARM"}, {"TP_EUM_ECC1B_INFO"},
	{"TP_EUM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_reg[] = {
	{"LRB_ECC_1B_INT"}, {"LRB_ECC_2B_INT"},
	{"LRB_ECC_1B_ERR_INFO"}, {"LRB_ECC_2B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_reg[] = {
	{"PPP_TP_MEM_ECC_ERR_1B_INT"}, {"PPP_TP_MEM_ECC_ERR_2B_INT"},
	{"PPP_TP_MEM_ECC_INFO_BANK0"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_reg[] = {
	{"RQM_ECC1B"}, {"RQM_ECC2B"},
	{"RQM_ALM"}, {"RQM_ECC_1B_INFO"},
	{"RQM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_head_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_ERR_1B_INT"}, {"RXDMA_HEAD_BUFF_ECC_ERR_2B_INT"},
	{"RXDMA_HEAD_BUFF_ECC_ERR_1B_INFO"}, {"RXDMA_HEAD_BUFF_ECC_ERR_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_reg[] = {
	{"TPRXP_ECC1B"}, {"TPRXP_ECC2B"},
	{"TPRXP_ECC_1B_INFO"}, {"TPRXP_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_sxc_reg[] = {
	{"SCC_MEM_ECC_ERR_1B_INT"}, {"SCC_MEM_ECC_ERR_2B_INT"},
	{"SCC_INT_SRC"}, {"SCC_MEM_ECC_1B_INFO"},
	{"SCC_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_tqs_reg[] = {
	{"TQS_ECC1B"}, {"TQS_ECC2B"},
	{"TAI_ECC1B"}, {"TAI_ECC2B"},
	{"TQS_ECC_1B_INFO"}, {"TQS_ECC_2B_INFO"},
	{"TAI_ECC_1B_INFO"}, {"TAI_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_ERR_1B_INT"}, {"LQC_TP_TIMER_MEM_ECC_ERR_2B_INT"},
	{"LQC_TP_TIMER_RESP_ERR_INFO"}, {"LQC_TP_TIMER_MEM_ECC_1B_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_reg[] = {
	{"TP_TPCM_ECC1B"}, {"TP_TPCM_ECC2B"},
	{"TP_TPCM_AXI_ALARM"}, {"TP_TPCM_ECC1B_INFO"},
	{"TP_TPCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_reg[] = {
	{"TP_TPGCM_ECC1B"}, {"TP_TPGCM_ECC2B"},
	{"TP_TPGCM_CACHE_ALARM_RC"}, {"TP_TPGCM_ECC1B_INFO"},
	{"TP_TPGCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_reg[] = {
	{"TP_TPMM_ECC1B"}, {"TP_TPMM_ECC2B"},
	{"TP_TPMM_AXI_ALARM"}, {"TP_TPMM_ECC1B_INFO"},
	{"TP_TPMM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_reg[] = {
	{"TPP_ECC1B"}, {"TPP_ECC2B"},
	{"TPP_ECC1B_1"}, {"TPP_ECC2B_1"},
	{"TPP_ECC_1B_ADDR"}, {"TPP_ECC_2B_ADDR"},
	{"TPP_ECC_1B_ADDR_1"}, {"TPP_ECC_2B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_tp_wqem_reg[] = {
	{"LQC_WQEM_MEM_ECC_ERR_1B_INT"}, {"LQC_WQEM_MEM_ECC_ERR_2B_INT"},
	{"LQC_WQEM_INT_SRC"}, {"LQC_WQEM_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_extq_reg[] = {
	{"EXTQ_ECC1B"}, {"EXTQ_ECC2B"},
	{"EXTQ_ALM"}, {"EXTQ_ECC_1B_INFO"},
	{"EXTQ_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_reg[] = {
	{"UBOMMU_INT_SRC"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_CQM_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_CQM_INT_SRC"}, {"LQC_TA_CQM_MEM_ECC_1B_INFO"},
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}, {"LQC_TA_CQM_AMI_RAS_1"},
	{"LQC_TA_CQM_AMI_RAS_2"}, {"LQC_TA_CQM_AMI_RAS_3"},
	{"LQC_TA_CQM_AMI_RAS_4"}, {"LQC_TA_CQM_AMI_RAS_5"},
	{"LQC_TA_CQM_AMI_RAS_6"}, {"LQC_TA_CQM_AMI_RAS_7"},
	{"LQC_TA_CQM_AMI_RAS_8"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_ERR_1B"}, {"LQC_TA_MRD_MEM_ECC_ERR_2B"},
	{"LQC_TA_MRD_INT_SRC"}, {"LQC_TA_MRD_MEM_ECC_1B_INFO"},
	{"LQC_TA_MRD_MEM_ECC_2B_INFO"}, {"LQC_TA_MRD_FUNC_RAS_INFO_0"},
	{"LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_rsp_reg[] = {
	{"LQC_TA_RSP.RSP_MEM_ECC_ERR_1B"}, {"LQC_TA_RSP.RSP_MEM_ECC_ERR_2B"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_reg[] = {
	{"SCH_RINT"}, {"CRDT_TAB_WDAT_RINT"},
	{"RQ2QS_LINK_TAB_RINT"}, {"QS2PRI_LINK_TAB_RINT"},
	{"SCH_RQ_TO_QS_LINK"}, {"SCH_QS_TO_PRI_LINK"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_reg[] = {
	{"LQC_TA_TOM_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_TOM_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_TOM_INT_SRC"}, {"LQC_TA_TOM_PF_ERR_BM0"},
	{"PAGE_OVER_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_reg[] = {
	{"LQC_TA_TQC_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_TQC_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_TQC_INT_SRC"}, {"LQC_TA_TQC_FE_ERR_BM0"},
	{"LQC_TA_TQC_FE_ERR_BM1"}, {"LQC_TA_TQC_FE_ERR_BM2"},
	{"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_wqem_reg[] = {
	{"LQC_WQEM_MEM_ECC_ERR_1B_INT"}, {"LQC_WQEM_MEM_ECC_ERR_2B_INT"},
	{"LQC_WQEM_INT_SRC"}, {"LQC_WQEM_MEM_ECC_ERR_1B_INT"},
	{"LQC_WQEM_MEM_ECC_ERR_2B_INT"}, {"LQC_WQEM_FUNC_ERR_BM"},
	{"LQC_WQEM_FUNC_ERR_BM1"}, {"LQC_WQEM_FUNC_ERR_BM2"},
	{"LQC_WQEM_FUNC_ERR_BM3"}, {"LQC_WQEM_FUNC_ERR_BM4"},
	{"LQC_WQEM_FUNC_ERR_BM5"}, {"LQC_WQEM_FUNC_ERR_BM6"},
	{"LQC_WQEM_FUNC_ERR_BM7"}, {"LQC_WQEM_LOCK_BUF_REQ_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqms_reg[] = {
	{"LQC_TA_TQMS_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_TQMS_MEM_ECC_ERR_2B_INT"},
	{"TQMS_INT_SRC"}
};

static struct apei_reg g_apei_ub_lqc_ta_usi_reg[] = {
	{"LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_USI.USI_INT_SRC"}, {"LQC_TA_USI.USI_WR_BUS_ERR_BMP"},
	{"LQC_TA_USI.USI_INT_ERR_DFX3"}
};

static struct apei_reg g_apei_ub_lqc_ta_rig_reg[] = {
	{"LQC_TA_RIG.USI_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_RIG.USI_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_RIG.USI_INT_SRC"}, {"LQC_TA_USI.USI_WR_BUS_ERR_BMP"},
	{"LQC_TA_USI.USI_INT_ERR_DFX3"}
};

static struct apei_reg g_apei_ub_lqc_ta_dm_reg[] = {
	{"LQC_TA_DM.DM_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_DM.DM_MEM_ECC_ERR_2B_INT"}
};

static struct apei_reg g_apei_ub_lqc_ta_idev_reg[] = {
	{"LQC_TA_IDEV.DM_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_IDEV.DM_MEM_ECC_ERR_2B_INT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqep_reg[] = {
	{"LQC_TA_TQEP_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_TQEP_MEM_ECC_ERR_2B_INT"}
};

static struct apei_reg g_apei_ub_lqc_ta_eip_reg[] = {
	{"EIP_MEM_ECC_ERR_1B_INT"}, {"EIP_MEM_ECC_ERR_2B_INT"},
	{"EIP_BRESP_INT_SRC0"}, {"EIP_BRESP_INT_SRC1"},
	{"EIP_BRESP_INT_SRC2"}, {"EIP_BRESP_INT_SRC3"},
	{"EIP_BRESP_INT_SRC4"}, {"EIP_BRESP_INT_SRC5"},
	{"EIP_BRESP_INT_SRC6"}, {"EIP_BRESP_INT_SRC7"},
	{"EIP_INT_SRC0"}, {"EIP_INT_SRC1"},
	{"EIP_INT_SRC2"}, {"EIP_INT_SRC3"},
	{"EIP_INT_SRC4"}, {"EIP_INT_SRC5"},
	{"EIP_INT_SRC6"}, {"EIP_INT_SRC7"},
	{"EIP_INT_MEM_ECC_INFO"}, {"EIP_DB_MEM_ECC_INFO"},
	{"EIP_BA_MEM_ECC_INFO"}
};

static struct apei_submodule g_apei_ub_v160_module[] = {
	APEI_MODULE_DEF("LQC.MISC.MISC_SLV", 0x1, g_apei_ub_lqc_misc_misc_slv_reg),
	APEI_MODULE_DEF("LQC.MISC.IMP", 0x2, g_apei_ub_lqc_misc_imp_reg),
	APEI_MODULE_DEF("LQC.BA", 0x3, g_apei_ub_lqc_ba_reg),
	APEI_MODULE_DEF("LQC.NL.PORT", 0x4, g_apei_ub_lqc_nl_port_reg),
	APEI_MODULE_DEF("LQC.NL.DEVICE", 0x5, g_apei_ub_lqc_nl_device_reg),
	APEI_MODULE_DEF("LQC.DLPHY.DLMAC", 0x6, g_apei_ub_lqc_dlphy_dlmac_reg),
	APEI_MODULE_DEF("LQC.DLPHY.MUXPCS", 0x7, g_apei_ub_lqc_dlphy_muxpcs_reg),
	APEI_MODULE_DEF("LQC.TP.DAM", 0x20, g_apei_ub_lqc_tp_dam_reg),
	APEI_MODULE_DEF("LQC.TP.EUM", 0x21, g_apei_ub_lqc_tp_eum_reg),
	APEI_MODULE_DEF("LQC.TP.LRB", 0x22, g_apei_ub_lqc_tp_lrb_reg),
	APEI_MODULE_DEF("LQC.TP.PPP", 0x23, g_apei_ub_lqc_tp_ppp_reg),
	APEI_MODULE_DEF("LQC.TP.RQM", 0x24, g_apei_ub_lqc_tp_rqm_reg),
	APEI_MODULE_DEF("LQC.TP.HEAD", 0x25, g_apei_ub_lqc_tp_head_reg),
	APEI_MODULE_DEF("LQC.TP.RXP", 0x26, g_apei_ub_lqc_tp_rxp_reg),
	APEI_MODULE_DEF("LQC.TP.SCC_SXC", 0x27, g_apei_ub_lqc_tp_scc_sxc_reg),
	APEI_MODULE_DEF("LQC.TP.TAI_TQS", 0x28, g_apei_ub_lqc_tp_tai_tqs_reg),
	APEI_MODULE_DEF("LQC.TP.TIMER", 0x29, g_apei_ub_lqc_tp_timer_reg),
	APEI_MODULE_DEF("LQC.TP.TPCM", 0x2a, g_apei_ub_lqc_tp_tpcm_reg),
	APEI_MODULE_DEF("LQC.TP.TPGCM", 0x2b, g_apei_ub_lqc_tp_tpgcm_reg),
	APEI_MODULE_DEF("LQC.TP.TPMM", 0x2c, g_apei_ub_lqc_tp_tpmm_reg),
	APEI_MODULE_DEF("LQC.TP.TPP", 0x2d, g_apei_ub_lqc_tp_tpp_reg),
	APEI_MODULE_DEF("LQC.TP.WQEM", 0x2e, g_apei_ub_lqc_tp_wqem_reg),
	APEI_MODULE_DEF("LQC.TP.TQS_EXTQ", 0x2f, g_apei_ub_lqc_tp_tqs_extq_reg),
	APEI_MODULE_DEF("LQC.TP.UBOMMU", 0x30, g_apei_ub_lqc_tp_ubommu_reg),
	APEI_MODULE_DEF("LQC.TA.CQM", 0x40, g_apei_ub_lqc_ta_cqm_reg),
	APEI_MODULE_DEF("LQC.TA.MRD", 0x41, g_apei_ub_lqc_ta_mrd_reg),
	APEI_MODULE_DEF("LQC.TA.RSP", 0x42, g_apei_ub_lqc_ta_rsp_reg),
	APEI_MODULE_DEF("LQC.TA.TM", 0x43, g_apei_ub_lqc_ta_tm_reg),
	APEI_MODULE_DEF("LQC.TA.TOM", 0x44, g_apei_ub_lqc_ta_tom_reg),
	APEI_MODULE_DEF("LQC.TA.TQC", 0x45, g_apei_ub_lqc_ta_tqc_reg),
	APEI_MODULE_DEF("LQC.TA.WQEM", 0x46, g_apei_ub_lqc_ta_wqem_reg),
	APEI_MODULE_DEF("LQC.TA.TQMS", 0x47, g_apei_ub_lqc_ta_tqms_reg),
	APEI_MODULE_DEF("LQC.TA.USI", 0x48, g_apei_ub_lqc_ta_usi_reg),
	APEI_MODULE_DEF("LQC.TA.RIG", 0x49, g_apei_ub_lqc_ta_rig_reg),
	APEI_MODULE_DEF("LQC.TA.DM", 0x4a, g_apei_ub_lqc_ta_dm_reg),
	APEI_MODULE_DEF("LQC.TA.IDEV", 0x4b, g_apei_ub_lqc_ta_idev_reg),
	APEI_MODULE_DEF("LQC.TA.TQEP", 0x4c, g_apei_ub_lqc_ta_tqep_reg),
	APEI_MODULE_DEF("LQC.TA.EIP", 0x4d, g_apei_ub_lqc_ta_eip_reg)
};

static struct apei_reg g_apei_ub_lqc_ba_assoc0_reg[] = {
	{"LSAD_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc1_reg[] = {
	{"LSAD_CCUA_SEQ_NS_CHECK_FAIL_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc2_reg[] = {
	{"LSAD_CCUA_SEQ_RXDATA_POISON_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc3_reg[] = {
	{"LSAD_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc4_reg[] = {
	{"LSAD_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc5_reg[] = {
	{"DFX_TP_LSA_REQ_STATUS3"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc6_reg[] = {
	{"CFG_ACK_TIMEOUT"}, {"CFG_RECV_ACK_RETURN"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc7_reg[] = {
	{"CFG_RETRY_THRESHOLD"}, {"CFG_REINIT_CNT_PERIOD"},
	{"CFG_RETRAIN_CNT_PERIOD"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc8_reg[] = {
	{"CFG_TX_BP_TIMEOUT"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc9_reg[] = {
	{"DL_RETRY_MEM_ECC_INFO"}, {"DL_PMU_MEM_ECC_INFO"},
	{"DL_RX_DBUF_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc10_reg[] = {
	{"DL_RETRY_MEM_ECC_INFO"}, {"DL_PMU_MEM_ECC_INFO"},
	{"DL_RX_DBUF_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc11_reg[] = {
	{"DL_RETRY_MEM_ECC_INFO"}, {"DL_PMU_MEM_ECC_INFO"},
	{"DL_RX_DBUF_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc16_reg[] = {
	{"ST_SDS_PW_STATUS2"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc17_reg[] = {
	{"ST_PCS_TX_FIFO_STATUS1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc18_reg[] = {
	{"ST_CH_TRAINING"}, {"ST_UB_STATUS4"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc19_reg[] = {
	{"ST_UB_STATUS4"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc20_reg[] = {
	{"PHY_LINK_STA_1"}, {"PHY_LINK_STA_2"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc21_reg[] = {
	{"ST_CH_TRAINING"}, {"ST_CH_TRAINING"},
	{"PORT_LINK_STA"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc22_reg[] = {
	{"ST_UB_DYNAMIC_LANE_INFO1"}, {"ST_UB_DYNAMIC_LANE_INFO1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc23_reg[] = {
	{"ST_MD_RX_DESKEW_1BIT_ECC_ADD"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc24_reg[] = {
	{"ST_MD_RX_DESKEW_1BIT_ECC_ADD"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_assoc25_reg[] = {
	{"TPRXP_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_assoc26_reg[] = {
	{"TPRXP_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_wqem_assoc27_reg[] = {
	{"LQC_WQEM_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_wqem_assoc28_reg[] = {
	{"LQC_WQEM_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc29_reg[] = {
	{"TP_DAM_ECC1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc30_reg[] = {
	{"TP_DAM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc31_reg[] = {
	{"TP_EUM_ECC1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc32_reg[] = {
	{"TP_EUM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc33_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc34_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc35_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc36_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc37_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc38_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc39_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc40_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_sxc_assoc41_reg[] = {
	{"SCC_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_sxc_assoc42_reg[] = {
	{"SCC_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc43_reg[] = {
	{"PPP_TP_MEM_ECC_INFO_BANK0"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc44_reg[] = {
	{"PPP_TP_MEM_ECC_INFO_BANK0"}
};

static struct apei_reg g_apei_ub_lqc_tp_head_assoc45_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_ERR_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_head_assoc46_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_ERR_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_tqs_assoc47_reg[] = {
	{"TQS_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_tqs_assoc48_reg[] = {
	{"TQS_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_extq_assoc49_reg[] = {
	{"EXTQ_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_extq_assoc50_reg[] = {
	{"EXTQ_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc51_reg[] = {
	{"TP_TPCM_ECC1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc52_reg[] = {
	{"TP_TPCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc53_reg[] = {
	{"TP_TPGCM_ECC1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc54_reg[] = {
	{"TP_TPGCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc55_reg[] = {
	{"RQM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc56_reg[] = {
	{"RQM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_assoc57_reg[] = {
	{"LRB_ECC_1B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_assoc58_reg[] = {
	{"LRB_ECC_2B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc59_reg[] = {
	{"TP_TPMM_ECC1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc60_reg[] = {
	{"TP_TPMM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_tqs_assoc61_reg[] = {
	{"TAI_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_tqs_assoc62_reg[] = {
	{"TAI_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc63_reg[] = {
	{"TPP_ECC_1B_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc64_reg[] = {
	{"TPP_ECC_2B_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc65_reg[] = {
	{"TPP_ECC_1B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc66_reg[] = {
	{"TPP_ECC_2B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc67_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc68_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc69_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc70_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc71_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc74_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc75_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc76_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc77_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc78_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc79_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc80_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc81_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc82_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc83_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc84_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc85_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc86_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc87_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc88_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc89_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc90_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc91_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc92_reg[] = {
	{"LQC_TA_CQM_AMI_RAS_1"}, {"LQC_TA_CQM_AMI_RAS_2"},
	{"LQC_TA_CQM_AMI_RAS_3"}, {"LQC_TA_CQM_AMI_RAS_4"},
	{"LQC_TA_CQM_AMI_RAS_5"}, {"LQC_TA_CQM_AMI_RAS_6"},
	{"LQC_TA_CQM_AMI_RAS_7"}, {"LQC_TA_CQM_AMI_RAS_8"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc93_reg[] = {
	{"LQC_TA_CQM_AMI_RAS_1"}, {"LQC_TA_CQM_AMI_RAS_2"},
	{"LQC_TA_CQM_AMI_RAS_3"}, {"LQC_TA_CQM_AMI_RAS_4"},
	{"LQC_TA_CQM_AMI_RAS_5"}, {"LQC_TA_CQM_AMI_RAS_6"},
	{"LQC_TA_CQM_AMI_RAS_7"}, {"LQC_TA_CQM_AMI_RAS_8"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc94_reg[] = {
	{"LQC_TA_CQM_AMI_RAS_1"}, {"LQC_TA_CQM_AMI_RAS_2"},
	{"LQC_TA_CQM_AMI_RAS_3"}, {"LQC_TA_CQM_AMI_RAS_4"},
	{"LQC_TA_CQM_AMI_RAS_5"}, {"LQC_TA_CQM_AMI_RAS_6"},
	{"LQC_TA_CQM_AMI_RAS_7"}, {"LQC_TA_CQM_AMI_RAS_8"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_assoc95_reg[] = {
	{"LQC_TA_CQM_AMI_RAS_1"}, {"LQC_TA_CQM_AMI_RAS_2"},
	{"LQC_TA_CQM_AMI_RAS_3"}, {"LQC_TA_CQM_AMI_RAS_4"},
	{"LQC_TA_CQM_AMI_RAS_5"}, {"LQC_TA_CQM_AMI_RAS_6"},
	{"LQC_TA_CQM_AMI_RAS_7"}, {"LQC_TA_CQM_AMI_RAS_8"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc96_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc97_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc98_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc99_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc100_reg[] = {
	{"LQC_TA_MRD_FUNC_RAS_INFO_0"}, {"LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc101_reg[] = {
	{"LQC_TA_MRD_FUNC_RAS_INFO_0"}, {"LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_wqem_assoc144_reg[] = {
	{"LQC_WQEM_FUNC_ERR_BM"}, {"LQC_WQEM_FUNC_ERR_BM1"},
	{"LQC_WQEM_FUNC_ERR_BM2"}, {"LQC_WQEM_FUNC_ERR_BM3"},
	{"LQC_WQEM_FUNC_ERR_BM4"}, {"LQC_WQEM_FUNC_ERR_BM5"},
	{"LQC_WQEM_FUNC_ERR_BM6"}, {"LQC_WQEM_FUNC_ERR_BM7"}
};

static struct apei_reg g_apei_ub_lqc_ta_wqem_assoc145_reg[] = {
	{"LQC_WQEM_FUNC_ERR_BM"}, {"LQC_WQEM_FUNC_ERR_BM1"},
	{"LQC_WQEM_FUNC_ERR_BM2"}, {"LQC_WQEM_FUNC_ERR_BM3"},
	{"LQC_WQEM_FUNC_ERR_BM4"}, {"LQC_WQEM_FUNC_ERR_BM5"},
	{"LQC_WQEM_FUNC_ERR_BM6"}, {"LQC_WQEM_FUNC_ERR_BM7"}
};

static struct apei_reg g_apei_ub_lqc_ta_wqem_assoc146_reg[] = {
	{"LQC_WQEM_LOCK_BUF_REQ_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc147_reg[] = {
	{"CRDT_TAB_WDAT_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc148_reg[] = {
	{"RQ2QS_LINK_TAB_RINT"}, {"QS2PRI_LINK_TAB_RINT"},
	{"SCH_RQ_TO_QS_LINK"}, {"SCH_QS_TO_PRI_LINK"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc149_reg[] = {
	{"RQ2QS_LINK_TAB_RINT"}, {"QS2PRI_LINK_TAB_RINT"},
	{"SCH_RQ_TO_QS_LINK"}, {"SCH_QS_TO_PRI_LINK"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc150_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc151_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc152_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc153_reg[] = {
	{"LQC_TA_TOM_PF_ERR_BM0"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc154_reg[] = {
	{"LQC_TA_TOM_PF_ERR_BM0"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc155_reg[] = {
	{"PAGE_OVER_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_eip_assoc156_reg[] = {
	{"EIP_DB_MEM_ECC_INFO"}, {"EIP_BA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_eip_assoc157_reg[] = {
	{"EIP_DB_MEM_ECC_INFO"}, {"EIP_BA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc168_reg[] = {
	{"SSU_EG_RX_HOS_BPS_MEM_ECC_ERR_ADDR"}, {"SSU_EG_RX_P2P_BPS_MEM_ECC_ERR_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc169_reg[] = {
	{"SSU_EG_RX_HOS_BPS_MEM_ECC_ERR_ADDR"}, {"SSU_EG_RX_P2P_BPS_MEM_ECC_ERR_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc171_reg[] = {
	{"OQ_OQM_ECC_ERR_LAST_ADDR_0"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_1"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_2"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_3"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc172_reg[] = {
	{"OQ_OQM_ECC_ERR_LAST_ADDR_0"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_1"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_2"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_3"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc175_reg[] = {
	{"OQSCHBMCREDITCNTINT_STICKY"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc177_reg[] = {
	{"LINK_CREDIT_SHARE_TCG_CNT_STICKY"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc178_reg[] = {
	{"LINK_CREDIT_SHARE_TCG_CNT_STICKY"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc179_reg[] = {
	{"SSU_MEM_ECC_SERR"}, {"SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc180_reg[] = {
	{"SSU_MEM_ECC_MERR"}, {"SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc188_reg[] = {
	{"OQ_OQM_ECC_ERR_LAST_ADDR_0"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_1"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_2"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_3"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc189_reg[] = {
	{"OQ_OQM_ECC_ERR_LAST_ADDR_0"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_1"},
	{"OQ_OQM_ECC_ERR_LAST_ADDR_2"}, {"OQ_OQM_ECC_ERR_LAST_ADDR_3"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc192_reg[] = {
	{"OQSCHBMCREDITCNTINT_STICKY"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc196_reg[] = {
	{"SSU_MEM_ECC_SERR"}, {"SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc197_reg[] = {
	{"SSU_MEM_ECC_MERR"}, {"SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc205_reg[] = {
	{"NL_RXPA_GLBPA_P2P_PATH_CREDIT_CFG"}, {"NL_RXPA_GLBPA_EXCP_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc206_reg[] = {
	{"PA_MEM_INIT"}, {"PA_MEM_ECC_SERR"},
	{"PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc207_reg[] = {
	{"PA_MEM_INIT"}, {"PA_MEM_ECC_MERR"},
	{"PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc208_reg[] = {
	{"PA_LINEAR_TBL_CFG"}, {"PA_ROUTING_TBL_CFG"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc209_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_LEN_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc210_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"},
	{"PA_UB_FLD_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc211_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc212_reg[] = {
	{"NL_RXPA_GLBPA_P2P_PATH_CREDIT_CFG"}, {"NL_RXPA_GLBPA_EXCP_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc213_reg[] = {
	{"PA_MEM_INIT"}, {"PA_MEM_ECC_SERR"},
	{"PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc214_reg[] = {
	{"PA_MEM_INIT"}, {"PA_MEM_ECC_MERR"},
	{"PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc215_reg[] = {
	{"PA_LINEAR_TBL_CFG"}, {"PA_ROUTING_TBL_CFG"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc216_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_LEN_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc217_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"},
	{"PA_UB_FLD_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc218_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc219_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_LEN_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc220_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"},
	{"PA_UB_FLD_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc221_reg[] = {
	{"UB_PKT_CHK_CFG"}, {"PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc222_reg[] = {
	{"PFA_INT_SRC_FIFO_DL"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc223_reg[] = {
	{"PFA_INT_SRC_FIFO_DL"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc224_reg[] = {
	{"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc225_reg[] = {
	{"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc226_reg[] = {
	{"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc227_reg[] = {
	{"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc228_reg[] = {
	{"PFA_INT_SRC_FIFO"}, {"PFA_INT_SRC_FIFO_DL"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc229_reg[] = {
	{"PFA_INT_SRC_PKT"}, {"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc230_reg[] = {
	{"PFA_INT_SRC_PKT"}, {"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc231_reg[] = {
	{"PFA_INT_SRC_PKT"}, {"PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc232_reg[] = {
	{"SEQ_CTRL_CRDT_OVERFLOW_0"}, {"SEQ_CTRL_CRDT_OVERFLOW_1"},
	{"SEQ_CTRL_CRDT_OVERFLOW_2"}, {"SEQ_CTRL_CRDT_OVERFLOW_3"},
	{"SEQ_CTRL_CRDT_OVERFLOW_4"}, {"SEQ_CTRL_CRDT_UNDERFLOW_0"},
	{"SEQ_CTRL_CRDT_UNDERFLOW_1"}, {"SEQ_CTRL_CRDT_UNDERFLOW_2"},
	{"SEQ_CTRL_CRDT_UNDERFLOW_3"}, {"SEQ_CTRL_CRDT_UNDERFLOW_4"},
	{"SEQ_CTRL_BP_WATERLINE"}, {"SEQ_CTRL_BP_BYPS_WATERLINE"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc233_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc234_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc235_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc236_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc237_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc238_reg[] = {
	{"INT_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc239_reg[] = {
	{"DFX_IMP_CMT_PC_0"}, {"DFX_IMP_MCU"},
	{"DFX_IMP_MCU_AW_STATUS"}, {"DFX_IMP_MCU_AR_STATUS"},
	{"DFX_IMP_MCU_LAST_ARADDR"}, {"DFX_IMP_MST_LAST_AWADDR_LOW"},
	{"DFX_IMP_MST_LAST_AWADDR_HIGH"}, {"DFX_IMP_MST_LAST_ARADDR_LOW"},
	{"DFX_IMP_MST_LAST_ARADDR_HIGH"}, {"IMP_DBG_RSV_REG_0"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc240_reg[] = {
	{"IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP_DTCM1_1BIT_ERR_CNT"},
	{"IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP_DTCM0_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc241_reg[] = {
	{"IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP_DTCM1_1BIT_ERR_CNT"},
	{"IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP_DTCM0_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc242_reg[] = {
	{"MGTQ_MEM_ECC_ERR_ADDR"}, {"MGTQ_MEM_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc243_reg[] = {
	{"CMDQ_MEM_ECC_ERR_ADDR"}, {"CMDQ_MEM_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc244_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_TPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_TPKT_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc245_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RPKT_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc246_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RCQE_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc247_reg[] = {
	{"IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP_DTCM1_2BIT_ERR_CNT"},
	{"IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP_DTCM0_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc248_reg[] = {
	{"IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP_DTCM1_2BIT_ERR_CNT"},
	{"IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP_DTCM0_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc249_reg[] = {
	{"MGTQ_MEM_ECC_ERR_ADDR"}, {"MGTQ_MEM_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc250_reg[] = {
	{"CMDQ_MEM_ECC_ERR_ADDR"}, {"CMDQ_MEM_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc251_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_TPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_TPKT_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc252_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RPKT_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc253_reg[] = {
	{"IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"}, {"IMP_CTRL_CPU_MGTQ_RCQE_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc254_reg[] = {
	{"IMP_AHB_SLV_ERR_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_assoc255_reg[] = {
	{"TIMEOUT_TH"}, {"TIMEOUT_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_assoc256_reg[] = {
	{"CFG_BUS_AXI_AWFIFO_DFX"}, {"CFG_BUS_AXI_WFIFO_DFX"},
	{"CFG_BUS_AXI_BFIFO_DFX"}, {"CFG_BUS_AXI_ARFIFO_DFX"},
	{"CFG_BUS_AXI_RFIFO_DFX"}, {"CFG_REQ_FIFO_DFX"},
	{"CFG_RSP_FIFO_DFX"}, {"UMMU_REQ_FIFO_DFX"},
	{"UMMU_RSP_FIFO_DFX"}
};

static struct apei_fault_reg g_apei_ub_v160_fault_reg[] = {
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 31, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 30, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 29, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 28, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 27, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 26, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 25, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 24, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 23, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 22, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc0_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 21, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 20, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 15, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 14, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 13, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 12, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 11, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 10, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 9, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 8, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 7, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 3, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc1_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 2, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc2_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc3_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_5", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc4_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 30, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 29, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 28, 1, 0, 0x81afaa00, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 23, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 22, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 21, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 20, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 19, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 18, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 17, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 16, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 15, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 14, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 13, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 12, 1, 0, 0x81afaa05, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 11, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 10, 1, 0, 0x81afaa05, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 9, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 8, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 7, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc5_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 28, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 19, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 18, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 17, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 7, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 2, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 30, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 29, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 28, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 27, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 26, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 25, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 24, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 23, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 22, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 21, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 20, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 19, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 18, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 17, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 16, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 15, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 14, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 13, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 12, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 11, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 10, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 9, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 7, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 6, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 29, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 27, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 26, 1, 0, 0x81afaa01, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 25, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 24, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 23, 1, 0, 0x81afaa02, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 22, 1, 0, 0x81afaa03, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 21, 1, 0, 0x81afaa04, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 15, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 14, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 13, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 12, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 11, 1, 0, 0x81afaa00, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 18, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 17, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 9, 8, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 1, 8, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 4, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 23, 3, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 2, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 0, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 12, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 8, 4, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 20, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 6, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 22, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 26, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc6_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 27, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc7_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 29, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc8_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 30, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 14, 3, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc9_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 17, 2, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc10_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 31, 1, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc11_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 0, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 1, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 2, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 3, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 4, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 5, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 6, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 7, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 8, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 9, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 10, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 11, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 12, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 13, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 14, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 15, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc16_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc17_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc18_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc19_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc20_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc21_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc22_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 28, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_FEC_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MD_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc23_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MD_SRC", 1, 1, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc24_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MD_SRC", 2, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.RXP", "TPRXP_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxp_assoc25_reg)},
	{"LQC.TP.RXP", "TPRXP_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxp_assoc26_reg)},
	{"LQC.TP.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_wqem_assoc27_reg)},
	{"LQC.TP.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_wqem_assoc28_reg)},
	{"LQC.TP.WQEM", "LQC_WQEM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.WQEM", "LQC_WQEM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.DAM", "TP_DAM_ECC1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc29_reg)},
	{"LQC.TP.DAM", "TP_DAM_ECC2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc30_reg)},
	{"LQC.TP.EUM", "TP_EUM_ECC1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc31_reg)},
	{"LQC.TP.EUM", "TP_EUM_ECC2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc32_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc33_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc34_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc35_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc36_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc37_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc38_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc39_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc40_reg)},
	{"LQC.TP.SCC_SXC", "SCC_MEM_ECC_ERR_1B_INT", 0, 4, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_sxc_assoc41_reg)},
	{"LQC.TP.SCC_SXC", "SCC_MEM_ECC_ERR_2B_INT", 0, 4, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_sxc_assoc42_reg)},
	{"LQC.TP.SCC_SXC", "SCC_MEM_ECC_ERR_2B_INT", 14, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_1B_INT", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc43_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_2B_INT", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc44_reg)},
	{"LQC.TP.HEAD", "RXDMA_HEAD_BUFF_ECC_ERR_1B_INT", 0, 5, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_head_assoc45_reg)},
	{"LQC.TP.HEAD", "RXDMA_HEAD_BUFF_ECC_ERR_2B_INT", 0, 5, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_head_assoc46_reg)},
	{"LQC.TP.TAI_TQS", "TQS_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_tqs_assoc47_reg)},
	{"LQC.TP.TAI_TQS", "TQS_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_tqs_assoc48_reg)},
	{"LQC.TP.TQS_EXTQ", "EXTQ_ECC1B", 0, 20, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_extq_assoc49_reg)},
	{"LQC.TP.TQS_EXTQ", "EXTQ_ECC2B", 0, 20, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_extq_assoc50_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_ECC1B", 0, 8, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc51_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_ECC2B", 0, 8, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc52_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_ECC1B", 0, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc53_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_ECC2B", 0, 2, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc54_reg)},
	{"LQC.TP.RQM", "RQM_ECC1B", 0, 4, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc55_reg)},
	{"LQC.TP.RQM", "RQM_ECC2B", 0, 4, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc56_reg)},
	{"LQC.TP.LRB", "LRB_ECC_1B_INT", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_lrb_assoc57_reg)},
	{"LQC.TP.LRB", "LRB_ECC_2B_INT", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_lrb_assoc58_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_ECC1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc59_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_ECC2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc60_reg)},
	{"LQC.TP.TAI_TQS", "TAI_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_tqs_assoc61_reg)},
	{"LQC.TP.TAI_TQS", "TAI_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_tqs_assoc62_reg)},
	{"LQC.TP.TPP", "TPP_ECC1B", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc63_reg)},
	{"LQC.TP.TPP", "TPP_ECC2B", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc64_reg)},
	{"LQC.TP.TPP", "TPP_ECC1B_1", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc65_reg)},
	{"LQC.TP.TPP", "TPP_ECC2B_1", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc66_reg)},
	{"LQC.TP.DAM", "TP_DAM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.DAM", "TP_DAM_AXI_ALARM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.DAM", "TP_DAM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.EUM", "TP_EUM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.EUM", "TP_EUM_AXI_ALARM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.EUM", "TP_EUM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.SCC_SXC", "SCC_INT_SRC", 4, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.SCC_SXC", "SCC_INT_SRC", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.SCC_SXC", "SCC_INT_SRC", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.SCC_SXC", "SCC_INT_SRC", 1, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.SCC_SXC", "SCC_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TQS_EXTQ", "EXTQ_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.TQS_EXTQ", "EXTQ_ALM", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPGCM", "TP_TPGCM_CACHE_ALARM_RC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPGCM", "TP_TPGCM_CACHE_ALARM_RC", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPGCM", "TP_TPGCM_CACHE_ALARM_RC", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.RQM", "RQM_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.RQM", "RQM_ALM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.UBOMMU", "UBOMMU_INT_SRC", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.UBOMMU", "UBOMMU_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.UBOMMU", "UBOMMU_INT_SRC", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.UBOMMU", "UBOMMU_INT_SRC", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TP.UBOMMU", "UBOMMU_INT_SRC", 4, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 8, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 9, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 7, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 8, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 9, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc67_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc68_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 2, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc69_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 3, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc70_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 4, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc71_reg)},
	{"LQC.TA.RSP", "LQC_TA_RSP.RSP_MEM_ECC_ERR_1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.RSP_MEM_ECC_ERR_1B", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.RSP_MEM_ECC_ERR_2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.RSP_MEM_ECC_ERR_2B", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_INT_SRC", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_INT_SRC", 0, 7, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_INT_SRC", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RIG", "LQC_TA_RIG.USI_INT_SRC", 0, 7, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.DM", "LQC_TA_DM.DM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.DM", "LQC_TA_DM.DM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.IDEV", "LQC_TA_IDEV.DM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.IDEV", "LQC_TA_IDEV.DM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc74_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc75_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc76_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc77_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc78_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc79_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc80_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc81_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 8, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc82_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc83_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc84_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc85_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc86_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc87_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc88_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc89_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 7, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc90_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 8, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc91_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc92_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc93_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc94_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_cqm_assoc95_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc96_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_1B", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc97_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc98_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_2B", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc99_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc100_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc101_reg)},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 14, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 18, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 19, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 20, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 21, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 22, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 23, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 24, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 25, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 26, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 27, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 28, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 29, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 30, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_1B_INT", 31, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 14, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 16, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 17, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 18, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 19, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 20, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 21, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 22, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 23, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 24, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 25, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 26, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 27, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 28, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 29, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 30, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_MEM_ECC_ERR_2B_INT", 31, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.WQEM", "LQC_WQEM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_wqem_assoc144_reg)},
	{"LQC.TA.WQEM", "LQC_WQEM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_wqem_assoc145_reg)},
	{"LQC.TA.WQEM", "LQC_WQEM_INT_SRC", 2, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_wqem_assoc146_reg)},
	{"LQC.TA.TQMS", "LQC_TA_TQMS_MEM_ECC_ERR_1B_INT", 0, 10, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "LQC_TA_TQMS_MEM_ECC_ERR_2B_INT", 0, 10, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "TQMS_INT_SRC", 2, 2, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "TQMS_INT_SRC", 4, 2, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TM", "CRDT_TAB_WDAT_RINT", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc147_reg)},
	{"LQC.TA.TM", "RQ2QS_LINK_TAB_RINT", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc148_reg)},
	{"LQC.TA.TM", "QS2PRI_LINK_TAB_RINT", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc149_reg)},
	{"LQC.TA.TM", "SCH_RINT", 2, 28, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc150_reg)},
	{"LQC.TA.TM", "SCH_RINT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc151_reg)},
	{"LQC.TA.TM", "SCH_RINT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc152_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc153_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc154_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc155_reg)},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEP", "LQC_TA_TQEP_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_MEM_ECC_ERR_1B_INT", 0, 3, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_eip_assoc156_reg)},
	{"LQC.TA.EIP", "EIP_MEM_ECC_ERR_2B_INT", 0, 3, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_eip_assoc157_reg)},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC0", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC1", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC2", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC3", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC4", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC5", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC6", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_BRESP_INT_SRC7", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC0", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC1", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC2", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC3", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC4", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC5", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC6", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.EIP", "EIP_INT_SRC7", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 4, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 8, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "RAS_STATUS_1", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "RAS_STATUS_1", 9, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc168_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_1", 10, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc169_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_1", 11, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc171_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 17, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc172_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 18, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 19, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc175_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_3", 18, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "RAS_STATUS_3", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc177_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_3", 22, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc178_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc179_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc180_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_2", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc188_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 17, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc189_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 18, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 19, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc192_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc196_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc197_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_3", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc205_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc206_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc207_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc208_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc209_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc210_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc211_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 23, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc212_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 20, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc213_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 19, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc214_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_0", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc215_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc216_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 18, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc217_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_0", 22, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc218_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_1", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "RAS_STATUS_1", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc219_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_1", 18, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc220_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_1", 22, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc221_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 10, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc222_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc223_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 8, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc224_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 7, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc225_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 6, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc226_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 5, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc227_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 4, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc228_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc229_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 1, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc230_reg)},
	{"LQC.NL.PORT", "RAS_STATUS_2", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc231_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 16, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc232_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc233_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 4, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc234_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc235_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc236_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc237_reg)},
	{"LQC.NL.DEVICE", "DEVICE_RAS_STATUS_4", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc238_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 12, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc239_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc240_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc241_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 10, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc242_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc243_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 15, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc244_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc245_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 19, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc246_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc247_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc248_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 11, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc249_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc250_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 16, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc251_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 18, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc252_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 20, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc253_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc254_reg)},
	{"LQC.MISC.MISC_SLV", "DEVICE_RAS_STATUS_0", 14, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_misc_slv_assoc255_reg)},
	{"LQC.MISC.MISC_SLV", "DEVICE_RAS_STATUS_0", 13, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_misc_slv_assoc256_reg)},
};

static struct local_ras_module g_apei_ub_v160_module_list;
static struct apei_submodule_table g_apei_ub_v160_submodule_tbl = {
	.num = ARRAY_SIZE(g_apei_ub_v160_module),
	.modules = g_apei_ub_v160_module
};
static struct apei_fault_reg_table g_apei_ub_v160_fault_reg_table = {
	.num = ARRAY_SIZE(g_apei_ub_v160_fault_reg),
	.regs = g_apei_ub_v160_fault_reg
};

static struct apei_module_id g_apei_ub_v160_id_map[] = {
	{MAMI_KERN_TYPE_ASCEND_V2, APEI_MODULE_UB_ASCEND},
	{MAMI_KERN_TYPE_UNIONS_210, APEI_MODULE_UB_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG960_1P, APEI_MODULE_UB_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG960_2P, APEI_MODULE_UB_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG960_4P, APEI_MODULE_UB_KUNPENG}
};

int apei_ub_v160_init(void)
{
	int ret;

	INIT_LIST_HEAD(&g_apei_ub_v160_module_list.sub_modules);
	ret = local_ras_module_from_config(&g_apei_ub_v160_submodule_tbl, &g_apei_ub_v160_fault_reg_table,
									   &g_apei_ub_v160_module_list);
	if (ret != 0) {
		ras_err("Load ub v160 ras table from config failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = local_ras_module_register(g_apei_ub_v160_id_map, ARRAY_SIZE(g_apei_ub_v160_id_map),
									&g_apei_ub_v160_module_list);
	if (ret != 0) {
		ras_err("Register ub v160 ras table failed. (ret=%d)\n", ret);
		local_ras_module_destroy(&g_apei_ub_v160_module_list);
		return ret;
	}

	ras_info("APEI ub v160 init success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(apei_ub_v160_init, FEATURE_LOADER_STAGE_3);

void apei_ub_v160_uninit(void)
{
	local_ras_module_unregister(g_apei_ub_v160_id_map, ARRAY_SIZE(g_apei_ub_v160_id_map));
	local_ras_module_destroy(&g_apei_ub_v160_module_list);
	ras_info("APEI ub v160 deinit success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(apei_ub_v160_uninit, FEATURE_LOADER_STAGE_3);
