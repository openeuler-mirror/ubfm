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

static struct apei_reg g_apei_ub_lqc_ba_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"DEVICE_RAS_STATUS_1"},
	{"DEVICE_RAS_STATUS_2"}, {"DEVICE_RAS_STATUS_3"},
	{"DEVICE_RAS_STATUS_4"}, {"MAR_AUTH_NEAR_ADDR_LOW"},
	{"MAR_AUTH_NEAR_ADDR_HIGH"}, {"MAR_AUTH_FAR_ADDR_LOW"},
	{"MAR_AUTH_FAR_ADDR_HIGH"}, {"MAR_TIMEOUT_ADDR_LOW"},
	{"MAR_TIMEOUT_ADDR_HIGH"}, {"MAR_ILLEGAL_ADDR_LOW"},
	{"MAR_ILLEGAL_ADDR_HIGH"}, {"MAR_RDATA_POISON_LOW"},
	{"MAR_RDATA_POISON_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"LQC_SUBCTRL.TIMEOUT_TH"},
	{"LQC_SUBCTRL.TIMEOUT_ADDR"}, {"CFG_BUS_AXI_AWFIFO_DFX"},
	{"CFG_BUS_AXI_WFIFO_DFX"}, {"CFG_BUS_AXI_BFIFO_DFX"},
	{"CFG_BUS_AXI_ARFIFO_DFX"}, {"CFG_BUS_AXI_RFIFO_DFX"},
	{"UMMU_REQ_FIFO_DFX"}, {"UMMU_RSP_FIFO_DFX"},
	{"CFG_REQ_FIFO_DFX"}, {"CFG_RSP_FIFO_DFX"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"IMP.DFX_IMP_CMT_PC_0"},
	{"IMP.DFX_IMP_CMT_PC_1"}, {"IMP.DFX_IMP_MCU"},
	{"IMP.DFX_IMP_MCU_AW_STATUS"}, {"IMP.DFX_IMP_MCU_AR_STATUS"},
	{"IMP.DFX_IMP_MCU_LAST_AWADDR"}, {"IMP.DFX_IMP_MCU_LAST_ARADDR"},
	{"IMP.DFX_IMP_MST_LAST_AWADDR_LOW"}, {"IMP.DFX_IMP_MST_LAST_AWADDR_HIGH"},
	{"IMP.DFX_IMP_MST_LAST_ARADDR_LOW"}, {"IMP.DFX_IMP_MST_LAST_ARADDR_HIGH"},
	{"IMP.IMP_DBG_RSV_REG_0"}, {"IMP.IMP_DBG_RSV_REG_1"},
	{"IMP.IMP_DBG_RSV_REG_2"}, {"IMP.IMP_DBG_RSV_REG_3"},
	{"IMP.IMP_DBG_RSV_REG_4"}, {"IMP.IMP_DBG_RSV_REG_5"},
	{"IMP.IMP_DBG_RSV_REG_6"}, {"IMP.IMP_DBG_RSV_REG_7"},
	{"IMP.IMP_DBG_RSV_REG_8"}, {"IMP.IMP_DBG_RSV_REG_9"},
	{"IMP.IMP_DBG_RSV_REG_10"}, {"IMP.IMP_DBG_RSV_REG_11"},
	{"IMP.IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP.IMP_DTCM1_1BIT_ERR_CNT"},
	{"IMP.IMP_DTCM1_2BIT_ERR_CNT"}, {"IMP.IMP_DTCM0_ECC_ERR_ADDR"},
	{"IMP.IMP_DTCM0_1BIT_ERR_CNT"}, {"IMP.IMP_DTCM0_2BIT_ERR_CNT"},
	{"IMP.MGTQ_MEM_ECC_ERR_ADDR"}, {"IMP.MGTQ_MEM_1BIT_ERR_CNT"},
	{"IMP.CMDQ_MEM_ECC_ERR_ADDR"}, {"IMP.CMDQ_MEM_1BIT_ERR_CNT"},
	{"IMP.MGTQ_MEM_2BIT_ERR_CNT"}, {"IMP.CMDQ_MEM_2BIT_ERR_CNT"},
	{"IMP.IMP_AHB_SLV_ERR_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_TPKT_ERR_CNT_ADDR"},
	{"IMP.IMP_CTRL_CPU_MGTQ_TPKT_1BIT_ERR_CNT"}, {"IMP.IMP_CTRL_CPU_MGTQ_TPKT_2BIT_ERR_CNT"},
	{"IMP.IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_RPKT_1BIT_ERR_CNT"},
	{"IMP.IMP_CTRL_CPU_MGTQ_RPKT_2BIT_ERR_CNT"}, {"IMP.IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"},
	{"IMP.IMP_CTRL_CPU_MGTQ_RCQE_1BIT_ERR_CNT"}, {"IMP.IMP_CTRL_CPU_MGTQ_RCQE_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_reg[] = {
	{"TP_DAM_ECC1B"}, {"TP_DAM_ECC2B"},
	{"TP_DAM_AXI_ALARM"}, {"TP_DAM_VFT_BT_ALARM"},
	{"TP_DAM_ECC1B_INFO"}, {"TP_DAM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_reg[] = {
	{"TP_EUM_ECC1B"}, {"TP_EUM_ECC2B"},
	{"TP_EUM_AXI_ALARM"}, {"TP_EUM_VFT_BT_ALARM"},
	{"TP_EUM_ECC1B_INFO"}, {"TP_EUM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_reg[] = {
	{"LRB_ECC_1B_INT"}, {"LRB_ECC_2B_INT"},
	{"LRB_ECC_1B_ERR_INFO"}, {"LRB_ECC_2B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_reg[] = {
	{"PPP_TP_MEM_ECC_ERR_1B_INT_BANK0"}, {"PPP_TP_MEM_ECC_ERR_1B_INT_BANK1"},
	{"PPP_TP_MEM_ECC_ERR_1B_INT_BANK2"}, {"PPP_TP_MEM_ECC_ERR_2B_INT_BANK0"},
	{"PPP_TP_MEM_ECC_ERR_2B_INT_BANK1"}, {"PPP_TP_MEM_ECC_ERR_2B_INT_BANK2"},
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK0"},
	{"PPP_TP_MEM_ECC_INFO_BANK1"}, {"PPP_TP_MEM_ECC_INFO_BANK2"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_reg[] = {
	{"RQM_ECC1B"}, {"RQM_ECC2B"},
	{"RQM_ALM"}, {"TP_RQM_DFX_JTG_NOR_ERR"},
	{"TP_RQM_DFX_JFR_NOR_ERR"}, {"RQM_ECC_1B_INFO"},
	{"RQM_ECC_2B_INFO"}, {"TP_RQM_DFX_JTG_NOR_FUNCTION"},
	{"TP_RQM_DFX_JFR_NOR_FUNCTION"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxdma_head_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_ERR_1B_INT"}, {"RXDMA_HEAD_BUFF_ECC_ERR_2B_INT"},
	{"RXDMA_HEAD_BUFF_ECC_INSERT"}, {"RXDMA_HEAD_BUFF_ECC_ERR_1B_INFO"},
	{"RXDMA_HEAD_BUFF_ECC_ERR_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_reg[] = {
	{"TP_RD_WQE_PRC_ALM"}, {"TP_RECM_ALM"},
	{"TPRXP_ECC1B"}, {"TPRXP_ECC2B"},
	{"TPRXP_ECC_1B_INFO"}, {"TPRXP_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_reg[] = {
	{"SCC_INT_SRC"}, {"SCC_MEM_ECC_ERR_1B_INT"},
	{"SCC_MEM_ECC_ERR_2B_INT"}, {"SCC_MEM_MODE"},
	{"SCC_MEM_ECC_1B_INFO"}, {"SCC_MEM_ECC_2B_INFO"},
	{"SCC_INT_EN"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_reg[] = {
	{"TAI_ECC1B"}, {"TAI_ECC2B"},
	{"TAI_ECC_1B_INFO"}, {"TAI_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_reg[] = {
	{"LQC_TP_TIMER_MEM_ECC_ERR_1B_INT"}, {"LQC_TP_TIMER_MEM_ECC_ERR_2B_INT"},
	{"LQC_TP_TIMER_RESP_ERR_INFO"}, {"LQC_TP_TIMER_MEM_MODE"},
	{"TIMER_ECC_ERR_INFO"}, {"LQC_TP_TIMER_MEM_ECC_1B_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_reg[] = {
	{"TP_TPCM_ECC1B"}, {"TP_TPCM_ECC2B"},
	{"TP_TPCM_AXI_ALARM"}, {"TP_TPCM_VFT_BT_ALARM"},
	{"TP_TPCM_ECC1B_INFO"}, {"TP_TPCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_reg[] = {
	{"TP_TPGCM_ECC1B"}, {"TP_TPGCM_ECC2B"},
	{"TP_TPGCM_AXI_ALARM"}, {"TP_TPGCM_VFT_BT_ALARM"},
	{"TP_TPGCM_ECC1B_INFO"}, {"TP_TPGCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_reg[] = {
	{"TP_TPMM_AXI_ALARM"}, {"TP_TPMM_VFT_BT_ALARM"},
	{"TP_TPMM_ECC1B"}, {"TP_TPMM_ECC2B"},
	{"TP_TPMM_ECC1B_INFO"}, {"TP_TPMM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_reg[] = {
	{"TPP_ECC1B"}, {"TPP_ECC2B"},
	{"TPP_ECC1B_1"}, {"TPP_ECC2B_1"},
	{"TPP_ECC_1B_RAM_ID"}, {"TPP_ECC_1B_ADDR"},
	{"TPP_ECC_1B_RAM_ID_1"}, {"TPP_ECC_1B_ADDR_1"},
	{"TPP_ECC_2B_RAM_ID"}, {"TPP_ECC_2B_ADDR"},
	{"TPP_ECC_2B_RAM_ID_1"}, {"TPP_ECC_2B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_reg[] = {
	{"TQEM_ECC1B_0"}, {"TQEM_ECC1B_1"},
	{"TQEM_ECC1B_2"}, {"TQEM_ECC1B_3"},
	{"TQEM_ECC2B_0"}, {"TQEM_ECC2B_1"},
	{"TQEM_ECC2B_2"}, {"TQEM_ECC2B_3"},
	{"TQEM_ALM"}, {"TQEM_ECC_1B_INFO"},
	{"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_reg[] = {
	{"TQS_ECC1B"}, {"TQS_ECC2B"},
	{"EXTQ_ECC1B"}, {"EXTQ_ECC2B"},
	{"EXTQ_ALM"}, {"TQS_ECC_1B_INFO"},
	{"TQS_ECC_2B_INFO"}, {"EXTQ_ECC_1B_INFO"},
	{"EXTQ_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_reg[] = {
	{"UBOMMU_MEM_ECC_ERR_1B_INT"}, {"UBOMMU_MEM_ECC_ERR_2B_INT"},
	{"UBOMMU_MEM_MODE"}, {"UBOMMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_ta_cqm_reg[] = {
	{"LQC_TA_CQM_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_CQM_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_CQM_INT_SRC"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_reg[] = {
	{"LQC_TA_MRD_MEM_ECC_ERR_1B"}, {"LQC_TA_MRD_MEM_ECC_ERR_2B"},
	{"LQC_TA_MRD_INT_SRC"}, {"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_0"},
	{"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_rsp_reg[] = {
	{"LQC_TA_RSP.MEM_ECC_ERR_1B"}, {"LQC_TA_RSP.MEM_ECC_ERR_2B"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_reg[] = {
	{"SCH_RINT"}
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

static struct apei_reg g_apei_ub_lqc_ta_tqeb_reg[] = {
	{"LQC_TA_TQEB_MEM_ECC_ERR_1B_INT"}, {"LQC_TA_TQEB_MEM_ECC_ERR_2B_INT"},
	{"LQC_TA_TQEB_INT_SRC"}, {"LQC_TA_TQEB_FUNC_ERR_BM"},
	{"LQC_TA_TQEB_FUNC_ERR_BM1"}, {"LQC_TA_TQEB_FUNC_ERR_BM2"},
	{"LQC_TA_TQEB_FUNC_ERR_BM3"}, {"LQC_TA_TQEB_FUNC_ERR_BM4"},
	{"LQC_TA_TQEB_FUNC_ERR_BM5"}, {"LQC_TA_TQEB_FUNC_ERR_BM6"},
	{"LQC_TA_TQEB_FUNC_ERR_BM7"}, {"LQC_TA_TQEB_LOCK_BUF_REQ_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqms_reg[] = {
	{"LQC_TA_TQMS_INT_SRC"}, {"LQC_TA_TQMS_MEM_ECC_ERR_1B_INT"},
	{"LQC_TA_TQMS_MEM_ECC_ERR_2B_INT"}, {"LQC_TA_TQMS_DFX_TQMS_FIFO_EMPTY"}
};

static struct apei_reg g_apei_ub_lqc_ta_usi_reg[] = {
	{"LQC_TA_USI.USI_INT_SRC"}, {"LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT"},
	{"LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT"}, {"LQC_TA_USI.USI_WR_BUS_ERR_BMP"},
	{"LQC_TA_USI.USI_INT_ERR_DFX0"}, {"LQC_TA_USI.USI_INT_ERR_DFX1"},
	{"LQC_TA_USI.USI_INT_ERR_DFX2"}, {"LQC_TA_USI.USI_INT_ERR_DFX3"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_reg[] = {
	{"NL_CFG_GLB.RAS_STATUS_0"}, {"NL_CFG_GLB.RAS_STATUS_1"},
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_LEN_ERR_STS"},
	{"NL_RXPA_PORT.PA_UB_COM_ERR_STS"}, {"NL_RXPA_PORT.PA_UB_FLD_ERR_STS"},
	{"NL_TXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_TXPA_PORT.PA_UB_LEN_ERR_STS"},
	{"NL_TXPA_PORT.PA_UB_COM_ERR_STS"}, {"NL_TXPA_PORT.PA_UB_FLD_ERR_STS"},
	{"NL_SSU_GLB.SSU_MEM_ECC_SERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_MERR"},
	{"NL_SSU_GLB.SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_reg[] = {
	{"NL_CFG_GLB.DEVICE_RAS_STATUS_0"}, {"NL_CFG_GLB.DEVICE_RAS_STATUS_1"},
	{"INT_INJECT_CONCTRL"}, {"INT_INJ_SRC_EN_0"},
	{"NL_SSU_GLB.SSU_MEM_ECC_SERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_MERR"},
	{"NL_SSU_GLB.SSU_MEM_ECC_INFO"}, {"PPP_NL_GLB.PPP_NL_MEM_MODE"},
	{"PPP_NL_GLB.PPP_NL_MEM_INIT"}, {"PPP_NL_GLB.PPP_NL_MEM_ECC_ERR_1B_INT"},
	{"PPP_NL_GLB.PPP_NL_MEM_ECC_ERR_2B_INT"}, {"PPP_NL_GLB.PPP_NL_MEM_ECC_INFO"},
	{"NL_RXPA_GLB.PA_MEM_MODE"}, {"NL_RXPA_GLB.PA_MEM_INIT"},
	{"NL_RXPA_GLB.PA_MEM_ECC_SERR"}, {"NL_RXPA_GLB.PA_MEM_ECC_MERR"},
	{"NL_RXPA_GLB.PA_MEM_ECC_INFO"}, {"NL_RXPA_GLB.PA_ROUTE_MODE_CFG"},
	{"NL_RXPA_GLB.PA_EXCP_CHK_CFG"}, {"NL_RXPA_GLB.PA_P2P_BYPASS_STATUS"},
	{"NL_RXPA_GLB.PA_EXCP_STATUS"}, {"NL_RXPA_PORT.UB_PKT_CHK_CFG"},
	{"NL_RXPA_PORT.PA_UB_LEN_ERR_STS"}, {"NL_RXPA_PORT.IGU_OUTER_ERR_STS"},
	{"NL_RXPA_PORT.PA_UB_COM_ERR_STS"}, {"NL_RXPA_PORT.PA_UB_FLD_ERR_STS"},
	{"NL_RXPA_PORT.IGU_CFG"}, {"NL_TXPA_PORT.UB_PKT_CHK_CFG"},
	{"NL_TXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_reg[] = {
	{"LQC_DLMAC_RAS_INT_DL_SRC"}, {"LQC_DLMAC_RAS_INT_MAC_SRC"},
	{"LQC_DLMAC_RAS_INT_PFA_SRC"}, {"LQC_UB_DL_PORT_REG.ST_CCA_ERR_LSM"},
	{"LQC_UB_DL_PORT_REG.DL_RETRY_MEM_ECC_INFO"}, {"LQC_UB_DL_PORT_REG.DL_PMU_MEM_ECC_INFO"},
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_PKT"}, {"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_FIFO"},
	{"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_1BIT_ECC_ADDR_0"}, {"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_1BIT_ECC_ADDR_1"},
	{"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_2BIT_ECC_ADDR_0"}, {"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_2BIT_ECC_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_reg[] = {
	{"LQC_MUXPCS_RAS_INT_MACMUX_SRC"}, {"LQC_MUXPCS_RAS_INT_PCS_SRC"},
	{"UB_PCS_COM_REG.ST_SDS_PW_STATUS2"}, {"UB_PCS_LANE_REG.ST_PCS_TX_FIFO_STATUS1"},
	{"UB_PCS_LANE_REG.ST_PCS_RX_FIFO_STATUS1"}, {"UB_PCS_LANE_REG.CFG_PCS_LANE_FIFO_MODE"},
	{"UB_PCS_COM_REG.ST_COMMON_PCS_STATUS0"}, {"UB_PCS_LANE_REG.CFG_PCS_TX_FIFO_THR"},
	{"UB_PCS_LANE_REG.ST_PCS_RX_AM_DEC_INF0"}, {"UB_PCS_LANE_REG.ST_PCS_RX_AM_DEC_INF3"},
	{"UB_MAC_MUX_PORT_REG.ST_CH_TRAINING"}, {"UB_MAC_MUX_PORT_REG.ST_UB_STATUS4"},
	{"UB_CFGSPACE_PHY.PHY_LINK_STA_1"}, {"UB_CFGSPACE_PHY.PHY_LINK_STA_2"},
	{"UB_CFGSPACE_PHY.PORT_LINK_STA"}, {"UB_MAC_MUX_PORT_REG.ST_UB_DYNAMIC_LANE_INFO1"}
};

static struct apei_reg g_apei_ub_lqc_eth_reg[] = {
	{"DEVICE_RAS_STATUS_0"}, {"NETWORK.ETH_MAC_COMMON.UBOE_REG_TIMOUT_ADDR"},
	{"NETWORK.ETH_MAC_COMMON.MAG_FIFO_ERR_DFX"}, {"NETWORK.ETH_MAC_COMMON.MAG_TX_FIFO_DFX"}
};

static struct apei_submodule g_apei_ub_module[] = {
	APEI_MODULE_DEF("LQC.BA", 0x3, g_apei_ub_lqc_ba_reg),
	APEI_MODULE_DEF("LQC.MISC.MISC_SLV", 0x1, g_apei_ub_lqc_misc_misc_slv_reg),
	APEI_MODULE_DEF("LQC.MISC.IMP", 0x2, g_apei_ub_lqc_misc_imp_reg),
	APEI_MODULE_DEF("LQC.TP.DAM", 0x20, g_apei_ub_lqc_tp_dam_reg),
	APEI_MODULE_DEF("LQC.TP.EUM", 0x21, g_apei_ub_lqc_tp_eum_reg),
	APEI_MODULE_DEF("LQC.TP.LRB", 0x22, g_apei_ub_lqc_tp_lrb_reg),
	APEI_MODULE_DEF("LQC.TP.PPP", 0x23, g_apei_ub_lqc_tp_ppp_reg),
	APEI_MODULE_DEF("LQC.TP.RQM", 0x24, g_apei_ub_lqc_tp_rqm_reg),
	APEI_MODULE_DEF("LQC.TP.RXDMA_HEAD", 0x25, g_apei_ub_lqc_tp_rxdma_head_reg),
	APEI_MODULE_DEF("LQC.TP.RXP", 0x26, g_apei_ub_lqc_tp_rxp_reg),
	APEI_MODULE_DEF("LQC.TP.SCC", 0x27, g_apei_ub_lqc_tp_scc_reg),
	APEI_MODULE_DEF("LQC.TP.TAI", 0x28, g_apei_ub_lqc_tp_tai_reg),
	APEI_MODULE_DEF("LQC.TP.TIMER", 0x29, g_apei_ub_lqc_tp_timer_reg),
	APEI_MODULE_DEF("LQC.TP.TPCM", 0x2a, g_apei_ub_lqc_tp_tpcm_reg),
	APEI_MODULE_DEF("LQC.TP.TPGCM", 0x2b, g_apei_ub_lqc_tp_tpgcm_reg),
	APEI_MODULE_DEF("LQC.TP.TPMM", 0x2c, g_apei_ub_lqc_tp_tpmm_reg),
	APEI_MODULE_DEF("LQC.TP.TPP", 0x2d, g_apei_ub_lqc_tp_tpp_reg),
	APEI_MODULE_DEF("LQC.TP.TQEM", 0x2e, g_apei_ub_lqc_tp_tqem_reg),
	APEI_MODULE_DEF("LQC.TP.TQS", 0x2f, g_apei_ub_lqc_tp_tqs_reg),
	APEI_MODULE_DEF("LQC.TP.UBOMMU", 0x30, g_apei_ub_lqc_tp_ubommu_reg),
	APEI_MODULE_DEF("LQC.TA.CQM", 0x40, g_apei_ub_lqc_ta_cqm_reg),
	APEI_MODULE_DEF("LQC.TA.MRD", 0x41, g_apei_ub_lqc_ta_mrd_reg),
	APEI_MODULE_DEF("LQC.TA.RSP", 0x42, g_apei_ub_lqc_ta_rsp_reg),
	APEI_MODULE_DEF("LQC.TA.TM", 0x43, g_apei_ub_lqc_ta_tm_reg),
	APEI_MODULE_DEF("LQC.TA.TOM", 0x44, g_apei_ub_lqc_ta_tom_reg),
	APEI_MODULE_DEF("LQC.TA.TQC", 0x45, g_apei_ub_lqc_ta_tqc_reg),
	APEI_MODULE_DEF("LQC.TA.TQEB", 0x46, g_apei_ub_lqc_ta_tqeb_reg),
	APEI_MODULE_DEF("LQC.TA.TQMS", 0x47, g_apei_ub_lqc_ta_tqms_reg),
	APEI_MODULE_DEF("LQC.TA.USI", 0x48, g_apei_ub_lqc_ta_usi_reg),
	APEI_MODULE_DEF("LQC.NL.PORT", 0x4, g_apei_ub_lqc_nl_port_reg),
	APEI_MODULE_DEF("LQC.NL.DEVICE", 0x5, g_apei_ub_lqc_nl_device_reg),
	APEI_MODULE_DEF("LQC.DLPHY.DLMAC", 0x6, g_apei_ub_lqc_dlphy_dlmac_reg),
	APEI_MODULE_DEF("LQC.DLPHY.MUXPCS", 0x7, g_apei_ub_lqc_dlphy_muxpcs_reg),
	APEI_MODULE_DEF("LQC.ETH", 0x1d, g_apei_ub_lqc_eth_reg),
};

static struct apei_reg g_apei_ub_lqc_ba_assoc0_reg[] = {
	{"MAR_TIMEOUT_ADDR_LOW"}, {"MAR_TIMEOUT_ADDR_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc1_reg[] = {
	{"MAR_AUTH_FAR_ADDR_LOW"}, {"MAR_AUTH_FAR_ADDR_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc2_reg[] = {
	{"MAR_AUTH_NEAR_ADDR_LOW"}, {"MAR_AUTH_NEAR_ADDR_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc3_reg[] = {
	{"MAR_RDATA_POISON_LOW"}, {"MAR_RDATA_POISON_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_ba_assoc4_reg[] = {
	{"MAR_ILLEGAL_ADDR_LOW"}, {"MAR_ILLEGAL_ADDR_HIGH"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc5_reg[] = {
	{"IMP.DFX_IMP_CMT_PC_0"}, {"IMP.DFX_IMP_CMT_PC_1"},
	{"IMP.DFX_IMP_MCU"}, {"IMP.DFX_IMP_MCU_AW_STATUS"},
	{"IMP.DFX_IMP_MCU_AR_STATUS"}, {"IMP.DFX_IMP_MCU_LAST_AWADDR"},
	{"IMP.DFX_IMP_MCU_LAST_ARADDR"}, {"IMP.DFX_IMP_MST_LAST_AWADDR_LOW"},
	{"IMP.DFX_IMP_MST_LAST_AWADDR_HIGH"}, {"IMP.DFX_IMP_MST_LAST_ARADDR_LOW"},
	{"IMP.DFX_IMP_MST_LAST_ARADDR_HIGH"}, {"IMP.IMP_DBG_RSV_REG_0"},
	{"IMP.IMP_DBG_RSV_REG_1"}, {"IMP.IMP_DBG_RSV_REG_2"},
	{"IMP.IMP_DBG_RSV_REG_3"}, {"IMP.IMP_DBG_RSV_REG_4"},
	{"IMP.IMP_DBG_RSV_REG_5"}, {"IMP.IMP_DBG_RSV_REG_6"},
	{"IMP.IMP_DBG_RSV_REG_7"}, {"IMP.IMP_DBG_RSV_REG_8"},
	{"IMP.IMP_DBG_RSV_REG_9"}, {"IMP.IMP_DBG_RSV_REG_10"},
	{"IMP.IMP_DBG_RSV_REG_11"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc6_reg[] = {
	{"IMP.IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP.IMP_DTCM1_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc7_reg[] = {
	{"IMP.IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP.IMP_DTCM0_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc8_reg[] = {
	{"IMP.MGTQ_MEM_ECC_ERR_ADDR"}, {"IMP.MGTQ_MEM_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc9_reg[] = {
	{"IMP.CMDQ_MEM_ECC_ERR_ADDR"}, {"IMP.CMDQ_MEM_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc10_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_TPKT_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_TPKT_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc11_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_RPKT_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc12_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_RCQE_1BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc13_reg[] = {
	{"IMP.IMP_DTCM1_ECC_ERR_ADDR"}, {"IMP.IMP_DTCM1_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc14_reg[] = {
	{"IMP.IMP_DTCM0_ECC_ERR_ADDR"}, {"IMP.IMP_DTCM0_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc15_reg[] = {
	{"IMP.MGTQ_MEM_ECC_ERR_ADDR"}, {"IMP.MGTQ_MEM_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc16_reg[] = {
	{"IMP.CMDQ_MEM_ECC_ERR_ADDR"}, {"IMP.CMDQ_MEM_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc17_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_TPKT_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc18_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_RPKT_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_RPKT_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc19_reg[] = {
	{"IMP.IMP_CTRL_CPU_MGTQ_RCQE_ERR_CNT_ADDR"}, {"IMP.IMP_CTRL_CPU_MGTQ_RCQE_2BIT_ERR_CNT"}
};

static struct apei_reg g_apei_ub_lqc_misc_imp_assoc20_reg[] = {
	{"IMP.IMP_AHB_SLV_ERR_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_assoc21_reg[] = {
	{"LQC_SUBCTRL.TIMEOUT_TH"}, {"LQC_SUBCTRL.TIMEOUT_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_misc_misc_slv_assoc22_reg[] = {
	{"CFG_BUS_AXI_AWFIFO_DFX"}, {"CFG_BUS_AXI_WFIFO_DFX"},
	{"CFG_BUS_AXI_BFIFO_DFX"}, {"CFG_BUS_AXI_ARFIFO_DFX"},
	{"CFG_BUS_AXI_RFIFO_DFX"}, {"UMMU_REQ_FIFO_DFX"},
	{"UMMU_RSP_FIFO_DFX"}, {"CFG_REQ_FIFO_DFX"},
	{"CFG_RSP_FIFO_DFX"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_assoc23_reg[] = {
	{"TPRXP_ECC_1B_INFO"}, {"TPRXP_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxp_assoc24_reg[] = {
	{"TPRXP_ECC_1B_INFO"}, {"TPRXP_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc25_reg[] = {
	{"TP_DAM_ECC1B_INFO"}, {"TP_DAM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc26_reg[] = {
	{"TP_DAM_ECC1B_INFO"}, {"TP_DAM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc27_reg[] = {
	{"TP_EUM_ECC1B_INFO"}, {"TP_EUM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc28_reg[] = {
	{"TP_EUM_ECC1B_INFO"}, {"TP_EUM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc29_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc30_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc31_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc32_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc33_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc34_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc35_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc36_reg[] = {
	{"LQC_TP_TIMER_MEM_MODE"}, {"TIMER_ECC_ERR_INFO"},
	{"LQC_TP_TIMER_MEM_ECC_1B_INFO"}, {"LQC_TP_TIMER_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_assoc37_reg[] = {
	{"SCC_MEM_MODE"}, {"SCC_MEM_ECC_1B_INFO"},
	{"SCC_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_assoc38_reg[] = {
	{"SCC_MEM_MODE"}, {"SCC_MEM_ECC_1B_INFO"},
	{"SCC_MEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc39_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc40_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc41_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc42_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc43_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc44_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc45_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc46_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc47_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc48_reg[] = {
	{"TQEM_ECC_1B_INFO"}, {"TQEM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc49_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK0"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc50_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK1"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc51_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK2"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc52_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK0"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc53_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK1"}
};

static struct apei_reg g_apei_ub_lqc_tp_ppp_assoc54_reg[] = {
	{"PPP_TP_MEM_MODE"}, {"PPP_TP_MEM_ECC_INFO_BANK2"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxdma_head_assoc55_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_INSERT"}, {"RXDMA_HEAD_BUFF_ECC_ERR_1B_INFO"},
	{"RXDMA_HEAD_BUFF_ECC_ERR_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rxdma_head_assoc56_reg[] = {
	{"RXDMA_HEAD_BUFF_ECC_INSERT"}, {"RXDMA_HEAD_BUFF_ECC_ERR_1B_INFO"},
	{"RXDMA_HEAD_BUFF_ECC_ERR_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_assoc57_reg[] = {
	{"TAI_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tai_assoc58_reg[] = {
	{"TAI_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc59_reg[] = {
	{"TQS_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc60_reg[] = {
	{"TQS_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc61_reg[] = {
	{"EXTQ_ECC_1B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc62_reg[] = {
	{"EXTQ_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc63_reg[] = {
	{"TPP_ECC_1B_RAM_ID"}, {"TPP_ECC_1B_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc64_reg[] = {
	{"TPP_ECC_1B_RAM_ID_1"}, {"TPP_ECC_1B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc65_reg[] = {
	{"TPP_ECC_2B_RAM_ID"}, {"TPP_ECC_2B_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpp_assoc66_reg[] = {
	{"TPP_ECC_2B_RAM_ID_1"}, {"TPP_ECC_2B_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc67_reg[] = {
	{"TP_TPCM_ECC1B_INFO"}, {"TP_TPCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc68_reg[] = {
	{"TP_TPCM_ECC1B_INFO"}, {"TP_TPCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc69_reg[] = {
	{"TP_TPGCM_ECC1B_INFO"}, {"TP_TPGCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc70_reg[] = {
	{"TP_TPGCM_ECC1B_INFO"}, {"TP_TPGCM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc71_reg[] = {
	{"RQM_ECC_1B_INFO"}, {"RQM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc72_reg[] = {
	{"RQM_ECC_1B_INFO"}, {"RQM_ECC_2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_assoc73_reg[] = {
	{"LRB_ECC_1B_ERR_INFO"}, {"LRB_ECC_2B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_lrb_assoc74_reg[] = {
	{"LRB_ECC_1B_ERR_INFO"}, {"LRB_ECC_2B_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc75_reg[] = {
	{"TP_TPMM_ECC1B_INFO"}, {"TP_TPMM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc76_reg[] = {
	{"TP_TPMM_ECC1B_INFO"}, {"TP_TPMM_ECC2B_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_assoc77_reg[] = {
	{"UBOMMU_MEM_MODE"}, {"UBOMMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_assoc78_reg[] = {
	{"UBOMMU_MEM_MODE"}, {"UBOMMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_assoc79_reg[] = {
	{"UBOMMU_MEM_MODE"}, {"UBOMMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_ubommu_assoc80_reg[] = {
	{"UBOMMU_MEM_MODE"}, {"UBOMMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc81_reg[] = {
	{"TP_DAM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc82_reg[] = {
	{"TP_DAM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc83_reg[] = {
	{"TP_EUM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc84_reg[] = {
	{"TP_EUM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc85_reg[] = {
	{"LQC_TP_TIMER_RESP_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc86_reg[] = {
	{"LQC_TP_TIMER_RESP_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_timer_assoc87_reg[] = {
	{"LQC_TP_TIMER_RESP_ERR_INFO"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_assoc88_reg[] = {
	{"SCC_INT_EN"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_assoc89_reg[] = {
	{"SCC_INT_EN"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc90_reg[] = {
	{"TQEM_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqem_assoc91_reg[] = {
	{"TQEM_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc92_reg[] = {
	{"EXTQ_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc93_reg[] = {
	{"EXTQ_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tqs_assoc94_reg[] = {
	{"EXTQ_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc95_reg[] = {
	{"TP_TPCM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc96_reg[] = {
	{"TP_TPCM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc97_reg[] = {
	{"TP_TPCM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc98_reg[] = {
	{"TP_TPGCM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc99_reg[] = {
	{"TP_TPGCM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc100_reg[] = {
	{"RQM_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc101_reg[] = {
	{"RQM_ALM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc102_reg[] = {
	{"TP_TPMM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc103_reg[] = {
	{"TP_TPMM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc104_reg[] = {
	{"TP_TPMM_AXI_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc105_reg[] = {
	{"TP_DAM_VFT_BT_ALARM"}, {"TP_DAM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_dam_assoc106_reg[] = {
	{"TP_DAM_VFT_BT_ALARM"}, {"TP_DAM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc107_reg[] = {
	{"TP_EUM_VFT_BT_ALARM"}, {"TP_EUM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_eum_assoc108_reg[] = {
	{"TP_EUM_VFT_BT_ALARM"}, {"TP_EUM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc109_reg[] = {
	{"TP_TPCM_VFT_BT_ALARM"}, {"TP_TPCM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpcm_assoc110_reg[] = {
	{"TP_TPCM_VFT_BT_ALARM"}, {"TP_TPCM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc111_reg[] = {
	{"TP_TPGCM_VFT_BT_ALARM"}, {"TP_TPGCM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpgcm_assoc112_reg[] = {
	{"TP_TPGCM_VFT_BT_ALARM"}, {"TP_TPGCM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc113_reg[] = {
	{"TP_TPMM_VFT_BT_ALARM"}, {"TP_TPMM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_tpmm_assoc114_reg[] = {
	{"TP_TPMM_VFT_BT_ALARM"}, {"TP_TPMM_VFT_BT_ALARM"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc115_reg[] = {
	{"TP_RQM_DFX_JTG_NOR_FUNCTION"}, {"TP_RQM_DFX_JTG_NOR_FUNCTION"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc116_reg[] = {
	{"TP_RQM_DFX_JTG_NOR_FUNCTION"}, {"TP_RQM_DFX_JTG_NOR_FUNCTION"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc117_reg[] = {
	{"TP_RQM_DFX_JFR_NOR_FUNCTION"}, {"TP_RQM_DFX_JFR_NOR_FUNCTION"}
};

static struct apei_reg g_apei_ub_lqc_tp_rqm_assoc118_reg[] = {
	{"TP_RQM_DFX_JFR_NOR_FUNCTION"}, {"TP_RQM_DFX_JFR_NOR_FUNCTION"}
};

static struct apei_reg g_apei_ub_lqc_tp_scc_assoc119_reg[] = {
	{"SCC_INT_EN"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc120_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc121_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc122_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc123_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqc_assoc124_reg[] = {
	{"LQC_TA_TQC_FE_ERR_BM0"}, {"LQC_TA_TQC_FE_ERR_BM1"},
	{"LQC_TA_TQC_FE_ERR_BM2"}, {"LQC_TA_TQC_FE_ERR_BM3"}
};

static struct apei_reg g_apei_ub_lqc_ta_usi_assoc125_reg[] = {
	{"LQC_TA_USI.USI_WR_BUS_ERR_BMP"}, {"LQC_TA_USI.USI_INT_ERR_DFX0"},
	{"LQC_TA_USI.USI_INT_ERR_DFX1"}, {"LQC_TA_USI.USI_INT_ERR_DFX2"},
	{"LQC_TA_USI.USI_INT_ERR_DFX3"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc126_reg[] = {
	{"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_0"}, {"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_mrd_assoc127_reg[] = {
	{"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_0"}, {"LQC_TA_MRD.LQC_TA_MRD_FUNC_RAS_INFO_1"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqeb_assoc128_reg[] = {
	{"LQC_TA_TQEB_FUNC_ERR_BM"}, {"LQC_TA_TQEB_FUNC_ERR_BM1"},
	{"LQC_TA_TQEB_FUNC_ERR_BM2"}, {"LQC_TA_TQEB_FUNC_ERR_BM3"},
	{"LQC_TA_TQEB_FUNC_ERR_BM4"}, {"LQC_TA_TQEB_FUNC_ERR_BM5"},
	{"LQC_TA_TQEB_FUNC_ERR_BM6"}, {"LQC_TA_TQEB_FUNC_ERR_BM7"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqeb_assoc129_reg[] = {
	{"LQC_TA_TQEB_FUNC_ERR_BM"}, {"LQC_TA_TQEB_FUNC_ERR_BM1"},
	{"LQC_TA_TQEB_FUNC_ERR_BM2"}, {"LQC_TA_TQEB_FUNC_ERR_BM3"},
	{"LQC_TA_TQEB_FUNC_ERR_BM4"}, {"LQC_TA_TQEB_FUNC_ERR_BM5"},
	{"LQC_TA_TQEB_FUNC_ERR_BM6"}, {"LQC_TA_TQEB_FUNC_ERR_BM7"}
};

static struct apei_reg g_apei_ub_lqc_ta_tqeb_assoc130_reg[] = {
	{"LQC_TA_TQEB_LOCK_BUF_REQ_CNT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc131_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc132_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tm_assoc133_reg[] = {
	{"SCH_RINT"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc134_reg[] = {
	{"LQC_TA_TOM_PF_ERR_BM0"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc135_reg[] = {
	{"LQC_TA_TOM_PF_ERR_BM0"}
};

static struct apei_reg g_apei_ub_lqc_ta_tom_assoc136_reg[] = {
	{"PAGE_OVER_CNT"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc137_reg[] = {
	{"NL_SSU_GLB.SSU_MEM_ECC_SERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc138_reg[] = {
	{"NL_SSU_GLB.SSU_MEM_ECC_MERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc145_reg[] = {
	{"NL_SSU_GLB.SSU_MEM_ECC_SERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc146_reg[] = {
	{"NL_SSU_GLB.SSU_MEM_ECC_MERR"}, {"NL_SSU_GLB.SSU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc153_reg[] = {
	{"PPP_NL_GLB.PPP_NL_MEM_MODE"}, {"PPP_NL_GLB.PPP_NL_MEM_INIT"},
	{"PPP_NL_GLB.PPP_NL_MEM_ECC_ERR_1B_INT"}, {"PPP_NL_GLB.PPP_NL_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc154_reg[] = {
	{"PPP_NL_GLB.PPP_NL_MEM_MODE"}, {"PPP_NL_GLB.PPP_NL_MEM_INIT"},
	{"PPP_NL_GLB.PPP_NL_MEM_ECC_ERR_2B_INT"}, {"PPP_NL_GLB.PPP_NL_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc157_reg[] = {
	{"NL_RXPA_GLB.PA_MEM_MODE"}, {"NL_RXPA_GLB.PA_MEM_INIT"},
	{"NL_RXPA_GLB.PA_MEM_ECC_SERR"}, {"NL_RXPA_GLB.PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc158_reg[] = {
	{"NL_RXPA_GLB.PA_MEM_MODE"}, {"NL_RXPA_GLB.PA_MEM_INIT"},
	{"NL_RXPA_GLB.PA_MEM_ECC_MERR"}, {"NL_RXPA_GLB.PA_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc159_reg[] = {
	{"NL_RXPA_GLB.PA_ROUTE_MODE_CFG"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc160_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_LEN_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc161_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_LEN_ERR_STS"},
	{"NL_RXPA_PORT.IGU_OUTER_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc162_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_COM_ERR_STS"},
	{"NL_RXPA_PORT.PA_UB_FLD_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc163_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_COM_ERR_STS"},
	{"NL_RXPA_PORT.PA_UB_FLD_ERR_STS"}, {"NL_RXPA_PORT.IGU_CFG"},
	{"NL_RXPA_PORT.IGU_OUTER_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc164_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc165_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc166_reg[] = {
	{"NL_RXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_RXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc167_reg[] = {
	{"NL_RXPA_GLB.PA_EXCP_CHK_CFG"}, {"NL_RXPA_GLB.PA_P2P_BYPASS_STATUS"},
	{"NL_RXPA_GLB.PA_EXCP_STATUS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc168_reg[] = {
	{"NL_TXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_TXPA_PORT.PA_UB_LEN_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc169_reg[] = {
	{"NL_TXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_TXPA_PORT.PA_UB_COM_ERR_STS"},
	{"NL_TXPA_PORT.PA_UB_FLD_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_device_assoc170_reg[] = {
	{"NL_TXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_TXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_nl_port_assoc171_reg[] = {
	{"NL_TXPA_PORT.UB_PKT_CHK_CFG"}, {"NL_TXPA_PORT.PA_UB_COM_ERR_STS"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc172_reg[] = {
	{"LQC_UB_DL_PORT_REG.ST_CCA_ERR_LSM"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc173_reg[] = {
	{"LQC_UB_DL_PORT_REG.DL_RETRY_MEM_ECC_INFO"}, {"LQC_UB_DL_PORT_REG.DL_PMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc174_reg[] = {
	{"LQC_UB_DL_PORT_REG.DL_RETRY_MEM_ECC_INFO"}, {"LQC_UB_DL_PORT_REG.DL_PMU_MEM_ECC_INFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc175_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc176_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc177_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc178_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_PKT"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc179_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_FIFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc180_reg[] = {
	{"LQC_UB_PFA_PORT_REG.PFA_INT_SRC_FIFO"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc181_reg[] = {
	{"UB_PCS_COM_REG.ST_SDS_PW_STATUS2"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc182_reg[] = {
	{"UB_PCS_LANE_REG.ST_PCS_TX_FIFO_STATUS1"}, {"UB_PCS_LANE_REG.CFG_PCS_LANE_FIFO_MODE"},
	{"UB_PCS_LANE_REG.CFG_PCS_LANE_FIFO_MODE"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc183_reg[] = {
	{"UB_PCS_LANE_REG.ST_PCS_RX_FIFO_STATUS1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc184_reg[] = {
	{"UB_PCS_COM_REG.ST_COMMON_PCS_STATUS0"}, {"UB_PCS_LANE_REG.CFG_PCS_LANE_FIFO_MODE"},
	{"UB_PCS_LANE_REG.CFG_PCS_LANE_FIFO_MODE"}, {"UB_PCS_LANE_REG.CFG_PCS_TX_FIFO_THR"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc185_reg[] = {
	{"UB_PCS_LANE_REG.ST_PCS_RX_AM_DEC_INF0"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc186_reg[] = {
	{"UB_PCS_LANE_REG.ST_PCS_RX_AM_DEC_INF3"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc187_reg[] = {
	{"UB_MAC_MUX_PORT_REG.ST_CH_TRAINING"}, {"UB_MAC_MUX_PORT_REG.ST_UB_STATUS4"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc188_reg[] = {
	{"UB_MAC_MUX_PORT_REG.ST_UB_STATUS4"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc189_reg[] = {
	{"UB_CFGSPACE_PHY.PHY_LINK_STA_1"}, {"UB_CFGSPACE_PHY.PHY_LINK_STA_2"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc190_reg[] = {
	{"UB_MAC_MUX_PORT_REG.ST_CH_TRAINING"}, {"UB_MAC_MUX_PORT_REG.ST_CH_TRAINING"},
	{"UB_CFGSPACE_PHY.PORT_LINK_STA"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_muxpcs_assoc191_reg[] = {
	{"UB_MAC_MUX_PORT_REG.ST_UB_DYNAMIC_LANE_INFO1"}, {"UB_MAC_MUX_PORT_REG.ST_UB_DYNAMIC_LANE_INFO1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc192_reg[] = {
	{"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_1BIT_ECC_ADDR_0"}, {"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_1BIT_ECC_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_dlphy_dlmac_assoc193_reg[] = {
	{"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_2BIT_ECC_ADDR_0"}, {"LQC_UB_DLMAC_GLB.ST_RX_DESKEW_2BIT_ECC_ADDR_1"}
};

static struct apei_reg g_apei_ub_lqc_eth_assoc224_reg[] = {
	{"NETWORK.ETH_MAC_COMMON.UBOE_REG_TIMOUT_ADDR"}
};

static struct apei_reg g_apei_ub_lqc_eth_assoc225_reg[] = {
	{"NETWORK.ETH_MAC_COMMON.MAG_FIFO_ERR_DFX"}, {"NETWORK.ETH_MAC_COMMON.MAG_TX_FIFO_DFX"}
};

static struct apei_fault_reg g_apei_ub_fault_reg[] = {
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 18, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 17, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 9, 8, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 1, 8, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 27, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 23, 3, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 19, 3, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 18, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_1", 0, 17, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 28, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 19, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 18, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 17, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 12, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 11, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 10, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 9, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 8, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 7, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 2, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_2", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 31, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 30, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 29, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 28, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 27, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 26, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 25, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 24, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 23, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 22, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 21, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 20, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 19, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 18, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 17, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 16, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 15, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 14, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 13, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 12, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 11, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 10, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 9, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 8, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_3", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 26, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 25, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 24, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 23, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc0_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 22, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc1_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 21, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc2_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 15, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 14, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 13, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 12, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 11, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc3_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 10, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 9, 1, 0, 0x81af8009, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ba_assoc4_reg)},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.BA", "DEVICE_RAS_STATUS_4", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 12, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc5_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc6_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc7_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 10, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc8_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc9_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 15, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc10_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc11_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 19, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc12_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc13_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc14_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 11, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc15_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc16_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 16, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc17_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 18, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc18_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 20, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc19_reg)},
	{"LQC.MISC.IMP", "DEVICE_RAS_STATUS_0", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_imp_assoc20_reg)},
	{"LQC.MISC.MISC_SLV", "DEVICE_RAS_STATUS_0", 14, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_misc_slv_assoc21_reg)},
	{"LQC.MISC.MISC_SLV", "DEVICE_RAS_STATUS_0", 13, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_misc_misc_slv_assoc22_reg)},
	{"LQC.TP.RXP", "TPRXP_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxp_assoc23_reg)},
	{"LQC.TP.RXP", "TPRXP_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxp_assoc24_reg)},
	{"LQC.TP.DAM", "TP_DAM_ECC1B", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc25_reg)},
	{"LQC.TP.DAM", "TP_DAM_ECC2B", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc26_reg)},
	{"LQC.TP.EUM", "TP_EUM_ECC1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc27_reg)},
	{"LQC.TP.EUM", "TP_EUM_ECC2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc28_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc29_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc30_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc31_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc32_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc33_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc34_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc35_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc36_reg)},
	{"LQC.TP.SCC", "SCC_MEM_ECC_ERR_1B_INT", 0, 4, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_assoc37_reg)},
	{"LQC.TP.SCC", "SCC_MEM_ECC_ERR_2B_INT", 0, 4, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_assoc38_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC1B_0", 0, 24, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc39_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC1B_1", 0, 24, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc40_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC1B_2", 0, 24, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc41_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC1B_3", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc42_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC1B_3", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc43_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC2B_0", 0, 24, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc44_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC2B_1", 0, 24, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc45_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC2B_2", 0, 24, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc46_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC2B_3", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc47_reg)},
	{"LQC.TP.TQEM", "TQEM_ECC2B_3", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc48_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_1B_INT_BANK0", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc49_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_1B_INT_BANK1", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc50_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_1B_INT_BANK2", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc51_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_2B_INT_BANK0", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc52_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_2B_INT_BANK1", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc53_reg)},
	{"LQC.TP.PPP", "PPP_TP_MEM_ECC_ERR_2B_INT_BANK2", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ppp_assoc54_reg)},
	{"LQC.TP.RXDMA_HEAD", "RXDMA_HEAD_BUFF_ECC_ERR_1B_INT", 0, 4, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxdma_head_assoc55_reg)},
	{"LQC.TP.RXDMA_HEAD", "RXDMA_HEAD_BUFF_ECC_ERR_2B_INT", 0, 4, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rxdma_head_assoc56_reg)},
	{"LQC.TP.TAI", "TAI_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_assoc57_reg)},
	{"LQC.TP.TAI", "TAI_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tai_assoc58_reg)},
	{"LQC.TP.TQS", "TQS_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc59_reg)},
	{"LQC.TP.TQS", "TQS_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc60_reg)},
	{"LQC.TP.TQS", "EXTQ_ECC1B", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc61_reg)},
	{"LQC.TP.TQS", "EXTQ_ECC2B", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc62_reg)},
	{"LQC.TP.TPP", "TPP_ECC1B", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc63_reg)},
	{"LQC.TP.TPP", "TPP_ECC1B_1", 0, 32, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc64_reg)},
	{"LQC.TP.TPP", "TPP_ECC2B", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc65_reg)},
	{"LQC.TP.TPP", "TPP_ECC2B_1", 0, 32, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpp_assoc66_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_ECC1B", 0, 8, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc67_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_ECC2B", 0, 8, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc68_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_ECC1B", 0, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc69_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_ECC2B", 0, 2, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc70_reg)},
	{"LQC.TP.RQM", "RQM_ECC1B", 0, 4, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc71_reg)},
	{"LQC.TP.RQM", "RQM_ECC2B", 0, 4, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc72_reg)},
	{"LQC.TP.LRB", "LRB_ECC_1B_INT", 0, 16, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_lrb_assoc73_reg)},
	{"LQC.TP.LRB", "LRB_ECC_2B_INT", 0, 16, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_lrb_assoc74_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_ECC1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc75_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_ECC2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc76_reg)},
	{"LQC.TP.UBOMMU", "UBOMMU_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ubommu_assoc77_reg)},
	{"LQC.TP.UBOMMU", "UBOMMU_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ubommu_assoc78_reg)},
	{"LQC.TP.UBOMMU", "UBOMMU_MEM_ECC_ERR_1B_INT", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ubommu_assoc79_reg)},
	{"LQC.TP.UBOMMU", "UBOMMU_MEM_ECC_ERR_2B_INT", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_ubommu_assoc80_reg)},
	{"LQC.TP.DAM", "TP_DAM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc81_reg)},
	{"LQC.TP.DAM", "TP_DAM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc82_reg)},
	{"LQC.TP.EUM", "TP_EUM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc83_reg)},
	{"LQC.TP.EUM", "TP_EUM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc84_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc85_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc86_reg)},
	{"LQC.TP.TIMER", "LQC_TP_TIMER_RESP_ERR_INFO", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_timer_assoc87_reg)},
	{"LQC.TP.SCC", "SCC_INT_SRC", 4, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_assoc88_reg)},
	{"LQC.TP.SCC", "SCC_INT_SRC", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_assoc89_reg)},
	{"LQC.TP.TQEM", "TQEM_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc90_reg)},
	{"LQC.TP.TQEM", "TQEM_ALM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqem_assoc91_reg)},
	{"LQC.TP.RXP", "TP_RD_WQE_PRC_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.RXP", "TP_RECM_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TP.TQS", "EXTQ_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc92_reg)},
	{"LQC.TP.TQS", "EXTQ_ALM", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc93_reg)},
	{"LQC.TP.TQS", "EXTQ_ALM", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tqs_assoc94_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc95_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc96_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc97_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc98_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_AXI_ALARM", 8, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc99_reg)},
	{"LQC.TP.RQM", "RQM_ALM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc100_reg)},
	{"LQC.TP.RQM", "RQM_ALM", 9, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc101_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc102_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc103_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_AXI_ALARM", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc104_reg)},
	{"LQC.TP.DAM", "TP_DAM_VFT_BT_ALARM", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc105_reg)},
	{"LQC.TP.DAM", "TP_DAM_VFT_BT_ALARM", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_dam_assoc106_reg)},
	{"LQC.TP.EUM", "TP_EUM_VFT_BT_ALARM", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc107_reg)},
	{"LQC.TP.EUM", "TP_EUM_VFT_BT_ALARM", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_eum_assoc108_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_VFT_BT_ALARM", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc109_reg)},
	{"LQC.TP.TPCM", "TP_TPCM_VFT_BT_ALARM", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpcm_assoc110_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_VFT_BT_ALARM", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc111_reg)},
	{"LQC.TP.TPGCM", "TP_TPGCM_VFT_BT_ALARM", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpgcm_assoc112_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_VFT_BT_ALARM", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc113_reg)},
	{"LQC.TP.TPMM", "TP_TPMM_VFT_BT_ALARM", 8, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_tpmm_assoc114_reg)},
	{"LQC.TP.RQM", "TP_RQM_DFX_JTG_NOR_ERR", 1, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc115_reg)},
	{"LQC.TP.RQM", "TP_RQM_DFX_JTG_NOR_ERR", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc116_reg)},
	{"LQC.TP.RQM", "TP_RQM_DFX_JFR_NOR_ERR", 1, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc117_reg)},
	{"LQC.TP.RQM", "TP_RQM_DFX_JFR_NOR_ERR", 0, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_rqm_assoc118_reg)},
	{"LQC.TP.SCC", "SCC_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_tp_scc_assoc119_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc120_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc121_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 2, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc122_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 3, 1, 0, 0x81af8004, MAMI_KERN_EVENT_MINOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc123_reg)},
	{"LQC.TA.TQC", "LQC_TA_TQC_INT_SRC", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqc_assoc124_reg)},
	{"LQC.TA.RSP", "LQC_TA_RSP.MEM_ECC_ERR_1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.MEM_ECC_ERR_1B", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.MEM_ECC_ERR_2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.RSP", "LQC_TA_RSP.MEM_ECC_ERR_2B", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.USI", "LQC_TA_USI.USI_INT_SRC", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_usi_assoc125_reg)},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.CQM", "LQC_TA_CQM_INT_SRC", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_1B", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_1B", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_2B", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.MRD", "LQC_TA_MRD_MEM_ECC_ERR_2B", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.MRD", "LQC_TA_MRD_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc126_reg)},
	{"LQC.TA.MRD", "LQC_TA_MRD_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_mrd_assoc127_reg)},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 8, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_1B_INT", 9, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 4, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 5, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 6, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 7, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 8, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_MEM_ECC_ERR_2B_INT", 9, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqeb_assoc128_reg)},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqeb_assoc129_reg)},
	{"LQC.TA.TQEB", "LQC_TA_TQEB_INT_SRC", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tqeb_assoc130_reg)},
	{"LQC.TA.TQMS", "TQMS_MEM_ECC_ERR_1B_INT", 0, 9, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "LQC_TA_TQMS_MEM_ECC_ERR_2B_INT", 0, 9, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "TQMS_INT_SRC", 2, 2, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "TQMS_INT_SRC", 4, 2, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TQMS", "TQMS_INT_SRC", 6, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.TA.TM", "SCH_RINT", 2, 28, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc131_reg)},
	{"LQC.TA.TM", "SCH_RINT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc132_reg)},
	{"LQC.TA.TM", "SCH_RINT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tm_assoc133_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_1B_INT", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 0, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 2, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_MEM_ECC_ERR_2B_INT", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 0, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc134_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 1, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc135_reg)},
	{"LQC.TA.TOM", "LQC_TA_TOM_INT_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_ta_tom_assoc136_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc137_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 1, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc138_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 2, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 3, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 7, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc145_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 1, 1, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc146_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 2, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 3, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 5, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 7, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 16, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc153_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 17, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc154_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 18, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 19, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc157_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 3, 1, 0, 0x81af8008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc158_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc159_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_0", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc160_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc161_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_0", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc162_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc163_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 5, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc164_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_0", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc165_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 6, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc166_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_0", 7, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc167_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 17, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc168_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 18, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc169_reg)},
	{"LQC.NL.DEVICE", "NL_CFG_GLB.DEVICE_RAS_STATUS_1", 21, 1, 0, 0x81af8000, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_device_assoc170_reg)},
	{"LQC.NL.PORT", "NL_CFG_GLB.RAS_STATUS_1", 22, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_nl_port_assoc171_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 4, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 2, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 0, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 20, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 8, 4, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 6, 2, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 14, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc172_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 22, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 15, 2, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc173_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_DL_SRC", 17, 2, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc174_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc175_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 1, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc176_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc177_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 3, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc178_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 4, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc179_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_PFA_SRC", 5, 1, 0, 0x81b1800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc180_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 7, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc181_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc182_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc183_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc184_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc185_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_PCS_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc186_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 5, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc187_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc188_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc189_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc190_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_muxpcs_assoc191_reg)},
	{"LQC.DLPHY.MUXPCS", "LQC_MUXPCS_RAS_INT_MACMUX_SRC", 28, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 0, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 1, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 2, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 3, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_NULL()},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 4, 1, 0, 0x0, MAMI_KERN_EVENT_INFO, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc192_reg)},
	{"LQC.DLPHY.DLMAC", "LQC_DLMAC_RAS_INT_MAC_SRC", 5, 1, 0, 0x81b18008, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_dlphy_dlmac_assoc193_reg)},
	{"LQC.ETH", "DEVICE_RAS_STATUS_0", 3, 1, 0, 0x8107800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_eth_assoc224_reg)},
	{"LQC.ETH", "DEVICE_RAS_STATUS_0", 2, 1, 0, 0x8107800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(),
	 APEI_ASSOC_REGS(g_apei_ub_lqc_eth_assoc225_reg)},
	{"LQC.ETH", "DEVICE_RAS_STATUS_0", 1, 1, 0, 0x8107800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
	{"LQC.ETH", "DEVICE_RAS_STATUS_0", 0, 1, 0, 0x8107800d, MAMI_KERN_EVENT_MAJOR, APEI_GROUP_NULL(), APEI_ASSOC_NULL()},
};


static struct local_ras_module g_apei_ub_v100_module_list;
static struct apei_submodule_table g_apei_ub_v100_submodule_tbl = {
	.num = ARRAY_SIZE(g_apei_ub_module),
	.modules = g_apei_ub_module
};
static struct apei_fault_reg_table g_apei_ub_v100_fault_reg_table = {
	.num = ARRAY_SIZE(g_apei_ub_fault_reg),
	.regs = g_apei_ub_fault_reg
};

static struct apei_module_id g_apei_ub_v100_id_map[] = {
	{MAMI_KERN_TYPE_ASCEND_V1, APEI_MODULE_UB_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG950_1P, APEI_MODULE_UB_KUNPENG},
	{MAMI_KERN_TYPE_UNIONS, APEI_MODULE_UB_ASCEND},
	{MAMI_KERN_TYPE_KUNPENG950_2P, APEI_MODULE_UB_KUNPENG},
	{MAMI_KERN_TYPE_KUNPENG950_4P, APEI_MODULE_UB_KUNPENG},
	{MAMI_KERN_TYPE_HSSU_V100, APEI_MODULE_UB_KUNPENG}
};

int apei_ub_v100_init(void)
{
	int ret;

	INIT_LIST_HEAD(&g_apei_ub_v100_module_list.sub_modules);

	ret = local_ras_module_from_config(&g_apei_ub_v100_submodule_tbl, &g_apei_ub_v100_fault_reg_table,
									   &g_apei_ub_v100_module_list);
	if (ret != 0) {
		ras_err("Load ub v100 ras table from config failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = local_ras_module_register(g_apei_ub_v100_id_map, ARRAY_SIZE(g_apei_ub_v100_id_map),
									&g_apei_ub_v100_module_list);
	if (ret != 0) {
		ras_err("Register ub v100 ras table failed. (ret=%d)\n", ret);
		local_ras_module_destroy(&g_apei_ub_v100_module_list);
		return ret;
	}

	ras_info("APEI ub v100 init success.\n");
	return 0;
}
DECLARE_FEATURE_AUTO_INIT(apei_ub_v100_init, FEATURE_LOADER_STAGE_3);

void apei_ub_v100_uninit(void)
{
	local_ras_module_unregister(g_apei_ub_v100_id_map, ARRAY_SIZE(g_apei_ub_v100_id_map));
	local_ras_module_destroy(&g_apei_ub_v100_module_list);
	ras_info("APEI ub v100 deinit success.\n");
}
DECLARE_FEATURE_AUTO_UNINIT(apei_ub_v100_uninit, FEATURE_LOADER_STAGE_3);
