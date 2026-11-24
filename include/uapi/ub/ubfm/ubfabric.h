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
#ifndef UBFABRIC_H
#define UBFABRIC_H

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/mm.h>
#include "mami_string.h"
#else
#include <unistd.h>
#include <securec.h>
#endif

#include "ubfm_common.h"
#include "mctrlq.h"
#include "linkcom.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCD_MAGIC 'V'
#define UCD_IOCTL_CMD _IO(UCD_MAGIC, 1)
#define UCD_IOCTL_DEV_CMD _IO(UCD_MAGIC, 2)


#define EVENT_MAX_GRP_NAME_LEN 128
#define UED_MAX_EVENT_LEN 1024

#define CTRLQ_CQE_DROP_SUB_EVENT 0 /* MAMI_EVENT_CTRLQ_CQE_DROP */

#define UBDEVM_CDMA_REMOVE_SUB_EVENT 0
#define UBDEVM_CDMA_PROBE_SUB_EVENT 1

#define SELF_LOG_BIT			(0x1 << 0U)
#define DEVID_LOG_BIT		   (0x1 << 1U)
#define CUR_LOG_BIT			 (0x1 << 2U)
#define KERN_LOG_BIT			(0x1 << 3U)
#define DEV_DEFAULT_LOG_BIT	 (0x1 << 4U)

#define UOP_MAX_DEVNUM	DEV_MAX_NUM
#define UOP_MAX_ITEM	  128U
#define UOP_MAX_BARE_ITEM 256U
#define UOP_MAX_ACC_NODE  64U

#define UOP_NAME_LEN	  32U

#define UOP_U32_BYTE_NUM  4U


#ifdef __KERNEL__
#define SYS_PAGE_SIZE PAGE_SIZE
#else
#define SYS_PAGE_SIZE ((size_t)getpagesize())
#endif

#define URES_MAX_DEVNUM	 DEV_MAX_NUM
#define URES_MAX_DIE_NUM	DIE_MAX_NUM

#define URES_NAME_LEN	   64U

#define URES_MEM_MAX_NUM	512U
#define URES_REG_MAX_NUM	512U

#define URES_ATTR_MAX_NUM   512U
#define URES_ATTR_MAX_SIZE  (45*1024U)

/* key value */
#define MAMI_UDIE_VER "udie_ver"
#define MAMI_CHIP_NUM "chip_num"
#define MAMI_DIE_NUM "die_num"
#define MAMI_DIE_NUM_PER_CHIP "die_num_per_chip"
#define MAMI_UE_NUM "ue_num"
#define MAMI_NL_NUM "nl_num"
#define MAMI_ACC_TYPE "acc_type"
#define MAMI_DIEN_PCIE_MASK "die%u_pcie_mask"
#define MAMI_DIEN_BODY_MASK "die%u_body_mask"
#define MAMI_VALID_BODY_MASK "die%u_valid_body_mask"

#define MAMI_NL0_REG "nl0_reg"
#define MAMI_NL1_REG "nl1_reg"
#define MAMI_NL2_REG "nl2_reg"
#define MAMI_NL3_REG "nl3_reg"
#define MAMI_NL4_REG "nl4_reg"

#define MAMI_BA0_REG "ba0_reg"
#define MAMI_BA1_REG "ba1_reg"
#define MAMI_BA2_REG "ba2_reg"
#define MAMI_BA3_REG "ba3_reg"
#define MAMI_BA4_REG "ba4_reg"

#define MAMI_UMMU_REG "ummu_reg"

#define MAMI_TA_REG "ta_reg"
#define MAMI_TP_REG "tp_reg"
#define MAMI_MISC_REG "misc_reg"
#define MAMI_MGTQ_REG "mgtq_reg"
#define MAMI_DCIP_REG "dcip_reg"
#define MAMI_D2H_REG "d2h_reg"

/* for udk ecc*/
#define MAMI_ECC_SMMU_A_REG "ecc_smmu_a_reg"
#define MAMI_ECC_SMMU_B_REG "ecc_smmu_b_reg"
#define MAMI_ECC_SMMU_C_REG "ecc_smmu_c_reg"
#define MAMI_ECC_DISP_A_REG "ecc_disp_a_reg"
#define MAMI_ECC_DISP_B_REG "ecc_disp_b_reg"
#define MAMI_ECC_SRAM_B_REG "ecc_sram_b_reg"

/* hilink */
#define MAMI_HILINK_SUBCTRL "hilink_subctrl"
#define MAMI_HILINK_M0_M1 "hilink_m0_m1"
#define MAMI_HILINK_M2_M3 "hilink_m2_m3"
#define MAMI_HILINK_M4 "hilink_m4"
#define MAMI_HILINK_M5_M6 "hilink_m5_m6"
#define MAMI_HILINK_M7_M8 "hilink_m7_m8"
#define MAMI_HILINK_M9 "hilink_m9"

/* reset */
#define MAMI_PERI_REG "mami_peri_reg"

/* imp boot status addr*/
#define MAMI_IMP_UDIE0_BOOT_STATUS_REG "mami_imp_udie0_reg"
#define MAMI_IMP_UDIE1_BOOT_STATUS_REG "mami_imp_udie1_reg"

#define MAMI_SLLC0_REG  "sllc0_reg"
#define MAMI_SLLC1_REG  "sllc1_reg"
#define MAMI_SLLC2_REG  "sllc2_reg"
#define MAMI_AA0_REG	"aa0_reg"
#define MAMI_AA1_REG	"aa1_reg"
#define MAMI_AA2_REG	"aa2_reg"
#define MAMI_AA3_REG	"aa3_reg"
#define MAMI_AA4_REG	"aa4_reg"

/* ddie mpt */
#define MAMI_BROADCAST_HA_REG	 "broadcast_ha_reg"
#define MAMI_BROADCAST_ACCESS_REG "broadcast_access_reg"
#define MAMI_ACCESS_AGENT_REG	  "access_agent_reg"
#define MAMI_L3T0_REG			 "l3T0_reg"
#define MAMI_L3T1_REG			 "l3T1_reg"
#define MAMI_HA_TOP0_REG		  "ha0_reg"
#define MAMI_ACCESS_TOP0_REG	  "access0_reg"

#define MAMI_UPA0_REG   "upa0_reg"
#define MAMI_UPA1_REG   "upa1_reg"
#define MAMI_UAMU_REG   "uamu_reg"

/* mem addr */
#define MAMI_CTRLQ_MEM "ctrlq_mem"
#define MAMI_HSEC_MEM "hsec_mem"
#define MAMI_MSGQ_SRAM_MEM "msgq_sram_mem"
#define MAMI_RAS_MEM "ras_mem"
#define MAMI_CCUA0_MEM "ccua0_mem"
#define MAMI_SRAM_LOG_BIST_SLT_MEM "log_bist_slt_mem"
#define MAMI_SRAM_LOG_IMU_MEM "log_imu_sram_mem"

#define MAMI_BBOX_MEM "bbox_mem"
/* local device info : eid, cna */
#define MAMI_LOCAL_UDEV_INFO	 "local_udev_info"
#define MAMI_UDEV_INFO		   "udev_info"
#define MAMI_UDEV_DIEN_CNA_EID		"udev_die%u_cna_eid"
#define MAMI_UDEV_RECREATE_CASE  "udev_recreate_case"

#define MAMI_UDEV_DIEN_RES_CFG  "udev_die%u_res_cfg"
#define MAMI_UDEV_HW_IDEN_INFO  "udev_hw_iden_info"
#define MAMI_UDEV_DIEN_CFG	  "udev_die%u_cfg"
#define MAMI_UDEV_CHIP_INFO	 "udev_chip_info"
#define MAMI_UDEV_CFG_INFO	  "udev_cfg_info"
#define MAMI_UDEV_AXI_ID_INFO   "udev_axi_id_info"
#define MAMI_UDEV_DYNAMIC_AXI_ID "udev_dynamic_axi_id"
#define MAMI_UDEV_CFG_HDR	   "udev_cfg_hdr"
#define MAMI_UDEV_IO_INFO	   "udev_io_info"

#define MAMI_DIEN_UE1_EID		 "die%u_ue1_eid%u"
#define MAMI_DIEN_UE1_UPI		 "die%u_ue1_upi"
#define MAMI_PORT_INFO_TBL		"port_info_table"
#define MAMI_DIEN_CTRLQ_CHAN_INFO "die%u_ctrlq_chan_info"

#define MBOX_HSEC_MEM_BASE	  "mbox_hsec_mem_base"
#define CCC_HSEC_MEM_BASE	   "ccc_hsec_mem_base"
#define SEU_HSEC_MEM_BASE	   "seu_hsec_mem_base"
#define ESPI_HSEC_MEM_BASE	  "espi_hsec_mem_base"
#define MSGQ_HSEC_MEM_BASE	  "msgq_hsec_mem_base"
#define PROCESS_HSEC_MEM_BASE   "process_hsec_mem_base"
#define QOS_HSEC_MEM_BASE	   "qos_hsec_mem_base"
#define IO_DECODER_HSEC_MEM_BASE "io_decoder_hsec_mem_base"
#define MDP_HSEC_MEM_BASE       "mdp_hsec_mem_base"
#define UMC_HSEC_MEM_BASE       "umc_hsec_mem_base"
#define CAP_HSEC_MEM_BASE       "cap_hsec_mem_base"
#define TOPO_HSEC_MEM_BASE       "topo_hsec_mem_base"
#define UBMEM_PA_UBA_HSEC_MEM_BASE "ubmem_pa_uba_hsec_mem_base"
#define TP_HSEC_MEM_BASE        "tp_hsec_mem_base"

#define MBOX_HSEC_MEM_ENTRY	  "mbox_hsec_mem_entry"
#define CCC_HSEC_MEM_ENTRY	   "ccc_hsec_mem_entry"
#define SEU_HSEC_MEM_ENTRY	   "seu_hsec_mem_entry"
#define ESPI_HSEC_MEM_ENTRY	  "espi_hsec_mem_entry"
#define MSGQ_HSEC_MEM_ENTRY	  "msgq_hsec_mem_entry"
#define PROCESS_HSEC_MEM_ENTRY   "process_hsec_mem_entry"
#define QOS_HSEC_MEM_ENTRY	   "qos_hsec_mem_entry"
#define IO_DECODER_HSEC_MEM_ENTRY "io_decoder_hsec_mem_entry"
#define MDP_HSEC_MEM_ENTRY       "mdp_hsec_mem_entry"
#define UMC_HSEC_MEM_ENTRY       "umc_hsec_mem_entry"
#define CAP_HSEC_MEM_ENTRY       "cap_hsec_mem_entry"
#define TOPO_HSEC_MEM_ENTRY       "topo_hsec_mem_entry"
#define UBMEM_PA_UBA_HSEC_MEM_ENTRY "ubmem_pa_uba_hsec_mem_entry"
#define TP_HSEC_MEM_ENTRY        "tp_hsec_mem_entry"

#define MAMI_CTRLQ_REG "ctrlq_reg"

#define MAMI_MSGQ_REG  "msgq_reg"

#define MAMI_UERES0_BASE "ueres0_base"
#define MAMI_UERES2_BASE "ueres2_base"

#define MAMI_CDMA_UE_EID "cdma_ue_eid"

#define MAMI_DIEN_CTRL_UE_CNA "die%u_ctrl_ue_cna"
#define MAMI_CTRL_UE_ID "ctrl_ue_id"
#define MAMI_DIEN_CTRL_UE_GUID  "die%u_ctrl_ue_guid"
#define MAMI_DIEN_CTRL_UE_IDEV_ID "die%u_ctrl_ue_idev_id"

#define MAMI_CUSTOM_MEM "custom_mem"
#define MAMI_DIEN_HB_LOST_CNT	   "die%u_hb_lost_cnt"
#define MAMI_OS_REINIT_CNT	 "os_reinit_cnt"

#define MAMI_AUTO_LOCAL_DEVID "auto_local_devid"
#define MAMI_IS_LOCAL_ASCEND "is_local_ascend"

/* ue elr*/
#define MAMI_NL_RX_BYPASS_REG "nl%u_rx_bypass_reg"

/* linkcom EC status */
#define MAMI_LINKCOM_EC_STATUS "linkcom_ec_status"

enum ued_event_id {
	UED_EVENT_LINK_STATUS_CHANGE = 0, /* MAMI_EVENT_LINK_CHANGE */
	UED_EVENT_DEV_STATUS_CHANGE = 1, /* MAMI_EVENT_PORT_EVENT */
	UED_EVENT_TPL_CTRL_MSG,
	UED_EVENT_UB_MEM_CTRL_MSG,
	UED_EVENT_DRM_CTRL_MSG,
	UED_EVENT_DRM_MSGQ_MSG,
	UED_EVENT_IRT_CTRL_MSG,
	UED_EVENT_UBQOS_CTRL_MSG,
	UED_EVENT_UBTPL_EVENT_MSG = 8, /* MAMI_EVENT_TPL_EVENT */

	UED_EVENT_PORT_MSGQ_MSG,

	UED_EVENT_CTRLQ_EVENT_MSG = 11, /* MAMI_EVENT_CTRLQ_EVENT */
	UED_EVENT_PORT_CTRLQ_MSG,
	UED_EVENT_UB_MEM_MSGQ_MSG,
	UED_EVENT_DEVM_CTRL_MSG,
	UED_EVENT_DEVREG_EVENT = 15, /* MAMI_EVENT_DEVREG_EVENT, */
	UED_EVENT_MAX_NUM
};

struct ued_event {
	u32 event_id;
	u32 subevent_id;
	u64 timestamp;
	u32 msg_len;
	u8 msg[UED_MAX_EVENT_LEN];
} __attribute__((packed));

struct ued_grp_para {
	u32 thread_num;  /* thread_num range: [0, 3] */
	char grp_name[EVENT_MAX_GRP_NAME_LEN];
};

enum uop_item_type {
	UOP_ITEM_MEM = 0,
	UOP_ITEM_REG,
	UOP_ITEM_TYPE_MAX,
};

enum uop_acc_type {
	UOP_ACC_LDST = 0,
	UOP_ACC_CDMA,
	UOP_ACC_PCIE_LDST,
	UOP_ACC_PCIE_I3C,
	UOP_ACC_MAX,
};

enum uop_win_mode {
	UOP_MODE_EXCLUSIVE = 0,
	UOP_MODE_SHARE = 1,
	UOP_MODE_MAX,
};

struct uop_item {
	char name[UOP_NAME_LEN];
	u64 asic_addr;	  /* Asic address */
	u64 acc_addr;	   /* Access address */
	u64 align_acc_addr; /* Page-aligned Access address */
	u32 len;
	void *user_ctx;
};

struct uop_bare_item {
	char name[UOP_NAME_LEN];
	u64 asic_addr;
	u32 len;
};

struct uop_acc_node {
	u64 acc_addr;
	u64 asic_addr;
	u32 len;
	u32 local_win_id;
	u32 axi_id;
};

typedef int (*uop_mem_read_t)(u32 devid, u64 addr, void *data, u32 len, void *user_ctx);
typedef int (*uop_mem_write_t)(u32 devid, u64 addr, void *data, u32 len, void *user_ctx);
typedef int (*uop_readl_t)(u32 devid, u64 addr, u32 *val, void *user_ctx);
typedef int (*uop_writel_t)(u32 devid, u64 addr, u32 val, void *user_ctx);
typedef int (*uop_readq_t)(u32 devid, u64 addr, u64 *val, void *user_ctx);
typedef int (*uop_writeq_t)(u32 devid, u64 addr, u64 val, void *user_ctx);
typedef void (*uop_barrier_t)(u32 devid, u64 addr, u32 val);
typedef int (*uop_init_dev_t)(u32 devid);
typedef void (*uop_uninit_dev_t)(u32 devid);
typedef int (*uop_msg_send_recv_t)(u32 devid, void *user_ctx);

struct uop_ops {
	uop_mem_read_t  mem_read;	   /* UOP_ITEM_MEM */
	uop_mem_write_t mem_write;	  /* UOP_ITEM_MEM */
	uop_readl_t	 readl;		  /* UOP_ITEM_REG */
	uop_writel_t	writel;		 /* UOP_ITEM_REG */
	uop_readq_t	 readq;		  /* UOP_ITEM_REG */
	uop_writeq_t	writeq;		 /* UOP_ITEM_REG */
	uop_init_dev_t  init_dev;
	uop_uninit_dev_t uninit_dev;
	uop_barrier_t   barrier;
};

struct uop_extend_ops {
	uop_msg_send_recv_t send_recv_msg;
};

union fpga_back_reg {
	struct {
		u32 fifo_cnt:6;   /* buffer count of ind_fifo */
		u32 ovfl_flag:1;  /* whether the ind_fifo buffer exceeds the configured watermark: 1 exceeds, 0 not */
		u32 rsv:25;
	};
	u32 val;
};

static inline int uop_item_pack(struct uop_item *item, const char *name, u64 asic_addr, u64 acc_addr, u32 len)
{
	int ret;

	item->asic_addr = asic_addr;
	/* When local_addr is not page-aligned, the acc_addr obtained by (io_)remap_pfn_range does not correspond to local_addr, but is mapped to the nearest page boundary to local_addr */
	item->align_acc_addr = acc_addr;
	/* Therefore the acc_addr used for actual access needs to add the offset caused by page alignment */
	item->acc_addr = acc_addr + (asic_addr & (SYS_PAGE_SIZE - 1));
	item->len = len;
#ifdef __KERNEL__
	ret = ubfm_strcpy(item->name, UOP_NAME_LEN, name);
#else
	ret = strcpy_s(item->name, UOP_NAME_LEN, name);
#endif
	return (ret != 0) ? ENOMEM : 0;
}

static inline int uop_bare_item_pack(struct uop_bare_item *item, const char *name, u64 asic_addr, u32 len)
{
	int ret;

	item->asic_addr = asic_addr;
	item->len = len;
#ifdef __KERNEL__
	ret = ubfm_strcpy(item->name, UOP_NAME_LEN, name);
#else
	ret = strcpy_s(item->name, UOP_NAME_LEN, name);
#endif
	return (ret != 0) ? ENOMEM : 0;
}

struct ures_addr_info {
	u64 addr;
	u32 size;
};

struct ures_inst {
	u32 devid;
	u32 die_id;
};

struct ures_reg {
	char name[URES_NAME_LEN];
	struct ures_addr_info info;
};

struct ures_mem {
	char name[URES_NAME_LEN];
	struct ures_addr_info info;
};

struct ures_key {
	char name[URES_NAME_LEN];
};

enum ures_win_type {
	URES_WIN_TYPE_KERN = 0,
	URES_WIN_TYPE_USER = 1,
	URES_WIN_TYPE_MAX,
};

struct ures_win_info {
	u32 win_id_start;
	u32 win_num;
	u32 win_size;
};

struct port_info_entry {
	u32 port_enable;
	u32 phyport;							/* Global physical port number; for ssd chip it refers to the nl layer physical port number */
	u32 dlphy_port_id;					  /* Global physical port number, its value equals phyport; for ssd chip it refers to the dlphy layer physical port number */
	enum port_link_mode link_mode;		  /* Link mode: ub or uboe */
	enum port_lane_mode lane_mode;		  /* Port X2/X4 mode */
	u8 tx_ppp_tgtid_index[DEV_DIE_MAX_NUM]; /* Index of the current port in the tx_tpp target id table of each udie */
	u8 rxpa_tgtid_index[DEV_DIE_MAX_NUM];   /* Index of the current port in the rxpa target id table of each udie */
};

struct port_info_table {
	u32 port_num;											 /* Max available port number supported by the chip */
	enum ndie_p2p_port_mode port_mode;						/* p2p port mode, only used for kunpeng960 */
	u8 tx_tpp_tgtid_table[DEV_DIE_MAX_NUM][DEV_PORT_MAX_NUM]; /* tx_tpp target id table of each udie */
	u8 rxpa_tgtid_table[DEV_DIE_MAX_NUM][DEV_PORT_MAX_NUM];   /* rxpa target id table of each udie */
	struct port_info_entry entry[DEV_PORT_MAX_NUM];		   /* Information of each port */
};

static inline void ures_inst_pack(struct ures_inst *inst, u32 devid, u32 die_id)
{
	inst->devid = devid;
	inst->die_id = die_id;
}

static inline void ures_addr_info_pack(struct ures_addr_info *addr_info, u64 addr, u32 size)
{
	addr_info->addr = addr;
	addr_info->size = size;
}

struct ucd_cmd_arg {
	const void *input;
	u32 input_len;
	void *output;
	u32 output_len;
};

struct ucd_ioctl_arg {
	u8 main_cmd;
	u8 sub_cmd;
	u8 rsv[2];
	struct ucd_cmd_arg arg;
};

enum ucd_main_cmd {
	UCD_CMD_URES = 0,
	UCD_CMD_UED = 1,
	UCD_CMD_UOP = 2,
	UCD_CMD_MNG = 3,
	UCD_CMD_HW_ADAPT = 4,
	UCD_CMD_MSGQ,
	UCD_CMD_UBLINKCOM,
	UCD_CMD_CTRLQ,
	UCD_CMD_FAULTMGR,
	UCD_CMD_ULOG,
	UCD_CMD_USI,
	UCD_CMD_NUM
};

enum ctrlq_sub_cmd {
	CTRLQ_CHAN_SWITCH = 0,
	CTRLQ_GET_SNAPSHOT,
	CTRLQ_SEND_SQE_TS,
	CTRLQ_CMD_NUM
};

struct ctrlq_get_snapshot_in {
	char *buf;
	uint32_t buf_len;
	uint32_t *read_size;
};

struct ctrlq_cmd_switch {
	u32 chan_id;
	bool on_off;	/* true: enable false: disable */
};

struct ctrlq_sqe_ts {
	uint32_t devid;
	uint32_t die_id;
	uint32_t chan_id;
	uint32_t sq_tail;
	uint32_t sq_head;
	uint32_t sq_depth;
	uint32_t sqe_ver;
	uint32_t sqe_type;
	uint32_t sqe_bb_num;
	uint32_t sqe_op_code;
	uint32_t sqe_seq;
};

struct devm_device_dump_in {
	u32 dev_id;
	uint64_t addr;
	uint32_t inSize;
};

struct devm_create_dev_out {
	u32 devid;
	int ret;
};

enum faultmgr_sub_cmd {
	FAULTMGR_EVENT_ADD,
	FAULTMGR_EVENT_GET,
	FAULTMGR_CMD_NUM
};

enum hwa_sub_cmd {
	HWA_TRIGGER_DEV_PROBE,
	HWA_CMD_NUM
};

enum mng_sub_cmd {
	MNG_CREATE_DEV,
	MNG_DESTROY_DEV,
	MNG_DEVICE_PROBE,
	MNG_CREATE_LOCAL_DEV,
	MNG_GET_LOCAL_DEVID,
	MNG_GET_UCMI_WIN_LOST,
	MNG_SET_MANAGED_PID,
	MNG_CMD_NUM
};

enum ucmi_win_check_mode {
	UCMI_WIN_CHECK_ALL = 0,
	UCMI_WIN_CHECK_AXI2,
	UCMI_WIN_CHECK_MODE_MAX
};

enum msgq_sub_cmd {
	MSGQ_SQ_SEND = 0,
	MSGQ_GET_SNAPSHOT,
	MSGQ_CMD_NUM
};

struct msgq_cmd_send {
	bool async;
	u32 die_id;
	u16 msn;
	struct msgq_msg msg;
};

struct msgq_get_snapshot_in {
	char *buf;
	uint32_t buf_len;
	uint32_t *read_size;
};

enum ublinkcom_sub_cmd {
	UBLINKCOM_OPEN,
	UBLINKCOM_CLOSE,
	UBLINKCOM_SEND,
	UBLINKCOM_RECV,
	UBLINKCOM_EVENTFD,
	UBLINKCOM_CMD_NUM
};

struct linkcom_open_chan_in {
	u32 dev_id;
	u32 app_pid;
	u32 chan_id;
	int eventfd;
};

struct linkcom_close_chan_in {
	int handle;
	u32 app_pid;
};

struct linkcom_send_chan_in {
	int handle;
	struct mami_pkt_send_para send_para;
};

struct linkcom_recv_chan_in {
	int handle;
	int timeout;
	u8 *pkt_buff; /* Data buffer for receiving packets, allocated by the caller */
	u32 buf_len;
};

struct linkcom_recv_chan_out {
	u32 reason;	 /* Packet report reason */
	u32 port_index; /* Source port of the packet */
	u32 enum_vld;   /* Whether it is an enumeration packet: 1 for enumeration packet, 0 for non-enumeration packet */
	u32 msg_code;   /* message code in AH (this field is only valid for config packets and UB MCTP, invalid for enumeration packets) */
	u32 recv_len;   /* Indicates the actual size of the received packet */
};

enum ued_sub_cmd {
	UED_CREATE_GRP,
	UED_DESTROY_GRP,
	UED_SUBSCRIBE_EVENT,
	UED_WAIT_EVENT,
	UED_PUBLISH_EVENT,
	UED_CMD_NUM
};

struct ued_wait_event_para {
	u32 gid;
	u32 tid;
	int timeout;
	struct ued_event *event; /* output */
};

struct ued_publish_event_para {
	u32 event_id;
	u32 subevent_id;
	u32 msg_len;
	u8 *msg;
};

struct ued_subscribe_event_para {
	int pid;
	u32 gid;
	u64 event_bitmap;
};

enum ulog_sub_cmd {
	ULOG_SET_LOG_CFG,
	ULOG_GET_LOG_CFG,
	ULOG_CMD_NUM
};

enum uop_sub_cmd {
	UOP_MEM_MAP = 0,
	UOP_MEM_UNMAP,
	UOP_REG_MAP,
	UOP_REG_UNMAP,
	UOP_FPGA_BACK_REG_MAP,
	UOP_FPGA_BACK_REG_UNMAP,
	UOP_ACC_NODE_MAP,
	UOP_ACC_NODE_UNMAP,
	UOP_CFG_WIN,
	UOP_CLR_WIN,
	UOP_CMD_NUM
};

struct uop_map_in {
	u32 die_id;
	u64 acc_addr;
	u32 size;
	char name[URES_NAME_LEN];
};

struct uop_unmap_in {
	u64 acc_addr;
	u32 size;
};

enum ures_sub_cmd {
	URES_GET_MEM_BASE = 0,
	URES_GET_REG_BASE,
	URES_GET_MEM_LIST,
	URES_GET_REG_LIST,
	URES_GET_KEY_VALUE,
	URES_SET_KEY_VALUE,
	URES_GET_DEV_ATTR,
	URES_GET_ATTR,
	URES_GET_ATTR_KEY_LIST,
	URES_GET_WIN_INFO,
	URES_CMD_NUM
};

struct ures_get_mem_base_in {
	u32 die_id;
	char name[URES_NAME_LEN];
};

struct ures_get_mem_list_in {
	u32 die_id;
	u32 num;
	struct ures_mem *mem;
};

struct ures_get_mem_list_out {
	u32 num;
};

struct ures_get_reg_base_in {
	u32 die_id;		  /* input */
	char name[URES_NAME_LEN];	   /* input */
};

struct ures_get_reg_list_in {
	u32 die_id;
	u32 num;
	struct ures_reg *reg;
};

struct ures_get_reg_list_out {
	u32 num;
};

struct ures_get_attr_key_list_in {
	u32 num;
	struct ures_key *key_list;
};

struct ures_get_attr_key_list_out {
	u32 num;
};

struct ures_get_dev_key_in {
	u32 devid;
	char name[URES_NAME_LEN];
};

struct ures_set_dev_key_in {
	char name[URES_NAME_LEN];
	u64 value;
};

enum usi_sub_cmd {
	USI_CREATE_CLIENT,
	USI_CLOSE_CLIENT
};


#ifdef __cplusplus
}
#endif

#endif
