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
#ifndef FMDEVM_H
#define FMDEVM_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#include "mami_string.h"
#else
#include <securec.h>
#include <stdint.h>
#endif


#include "ubfm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHIP_MAX_NUM	4U	  /* such as kunpeng950 4P */
#define UDEV_CFG_HDR_MAGIC_LENGTH  8U
#define UDEV_MAR_MAX_NUM   5U
#define UE_MAX_NUM	  126U	/* Excludes UE_CCU and UE_PMU */
#define UDEV_CFG_HDR_MAGIC		 "FABRIC\0\0"
/* The firmware reserves 32 segments of address space for user's register operations,
 * that is, memory operations.
 */
#define UB_IO_REG_NUM   32U
#define UB_IO_REG_LEN_UNIT  4096U  /* Length unit is 4K */
#define UDEV_CHIP_RESV_SIZE		32

#define GUID_STR_LEN (MAMI_GUID_LEN * 2 + 8 + 1 + 5)
#define GUID_STR_PREFIX "guid_"
#define GUID_STR_PREFIX_LEN (sizeof(GUID_STR_PREFIX) - 1)
#define GUID_FMT \
	GUID_STR_PREFIX"%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x"
#define GUID_SCAN_FMT "guid_%2x%2x:%2x%2x:%2x%2x:%2x%2x:%2x%2x:%2x%2x:%2x%2x:%2x%2x"

#define I3C_ADDR_HI(addr) ((u32)((u64)(addr) >> 32))
#define I3C_ADDR_LW(addr) ((u32)((u64)(addr) & 0xffffffff))
#define I3C_DRV_INST_ID_SELF 0xFFFFFFFF

/*
 * it is a brief of logical plane id(MAMI_KERN_LOGIC_ID_TYPE_PLANE).
 * chip type: K + A.
 * |----8bit-----|-------8bit------|----8bit--------|------8bit--------|
 * |-----rsv-----|------chipid-----|----cluster-----|------dieid-------|
 * chip type: U.
 * |--------------------24bit-----------------------|------8bit--------|
 * |--------------------rsv-------------------------|-----chipid-------|
 *
 * brief of logical portset id(MAMI_KERN_LOGIC_ID_TYPE_PORT_SET).
 * |----8bit-----|-------8bit------|-----8bit-------|--2bit--|------6bit--------|
 * |---chipid----|-----cluster-----|-----dieid------|--ssuid-|----portsetid-----|
 *
 * brief of logical port id(MAMI_KERN_LOGIC_ID_TYPE_PORT).
 * |----8bit-----|-------8bit------|-----8bit-------|------8bit--------|
 * |-----rsv-----|------chipid-----|----dieid-------|-----portid-------|
 */
#define MAMI_PLANE_GET_LG_AK_CHIPID(lg_planeid)			 (((lg_planeid) >> 0x10) & 0xFF)
#define MAMI_PLANE_GET_LG_AK_DIEID(lg_planeid)			  ((lg_planeid) & 0xFF)
#define MAMI_PLANE_GET_LG_U_CHIPID(lg_planeid)			  ((lg_planeid) & 0xFF)
#define MAMI_PLANE_SET_LG_U_ID(chipid)					  ((chipid) & 0xFF)
#define MAMI_PLANE_SET_LG_AK_ID(chipid, dieid)			  ((((chipid) & 0xFF) << 0x10) | ((dieid) & 0xFF))

#define MAMI_PORTSET_GET_LG_CHIPID(lg_portsetid)			(((lg_portsetid) >> 0x18) & 0xFF)
#define MAMI_PORTSET_GET_LG_DIEID(lg_portsetid)			 (((lg_portsetid) >> 0x8) & 0xFF)
#define MAMI_PORTSET_GET_LG_SSUID(lg_portsetid)			 (((lg_portsetid) >> 0x6) & 0x3)
#define MAMI_PORTSET_GET_LG_PORTSETID(lg_portsetid)		 ((lg_portsetid) & 0x3F)
#define MAMI_PORTSET_SET_LG_ID(chipid, dieid, portsetid)	((((chipid) & 0xFF) << 0x18) | \
															 (((dieid) & 0xFF) << 0x8) | \
															 ((portsetid) & 0x3F))
#define MAMI_PORTSET_SET_LG_ID_SSU(chipid, dieid, portsetid, ssuid) ((((chipid) & 0xFF) << 0x18) | \
																	 (((dieid) & 0xFF) << 0x8) | \
																	 (((ssuid) & 0x3) << 0x6) | \
																	 ((portsetid) & 0x3F))

#define MAMI_PORT_GET_LG_CHIPID(lg_portid)				  (((lg_portid) >> 0x10) & 0xFF)
#define MAMI_PORT_GET_LG_DIEID(lg_portid)				   (((lg_portid) >> 0x8) & 0xFF)
#define MAMI_PORT_GET_LG_PORTID(lg_portid)				  ((lg_portid) & 0xFF)
#define MAMI_PORT_SET_LG_ID(chipid, dieid, portid)		  ((((chipid) & 0xFF) << 0x10) | \
															 (((dieid) & 0xFF) << 0x8) | \
															 ((portid) & 0xFF))

enum mami_dev_type {
	MAMI_KERN_TYPE_INVALID = 0,
	MAMI_KERN_TYPE_ASCEND_V1 = 1,
	MAMI_KERN_TYPE_KUNPENG950_1P,
	MAMI_KERN_TYPE_UNIONS,
	MAMI_KERN_TYPE_KUNPENG950_2P,
	MAMI_KERN_TYPE_KUNPENG950_4P,
	MAMI_KERN_TYPE_ASCEND_V2,
	MAMI_KERN_TYPE_UNIONS_210,
	MAMI_KERN_TYPE_KUNPENG960_1P,
	MAMI_KERN_TYPE_KUNPENG960_2P,
	MAMI_KERN_TYPE_KUNPENG960_4P,
	MAMI_KERN_TYPE_1825,
	MAMI_KERN_TYPE_HSSU_V100,
	MAMI_KERN_TYPE_MAX
};

enum mami_ue_type {
	MAMI_KERN_UE_TYPE_URMA_MUE,
	MAMI_KERN_UE_TYPE_URMA_VUE,
	MAMI_KERN_UE_TYPE_CDMA_MUE,
	MAMI_KERN_UE_TYPE_CDMA_VUE,
	MAMI_KERN_UE_TYPE_CTRL_UE,
	MAMI_KERN_UE_TYPE_UBC_UE,
	MAMI_KERN_UE_TYPE_PMU_UE,
	MAMI_KERN_UE_TYPE_CCU_UE,
	MAMI_KERN_UE_TYPE_HINIC_MUE,
	MAMI_KERN_UE_TYPE_HINIC_VUE,
};

enum route_table_cfg_type {
	ROUTE_TABLE_CFG_CAM = 0,
	ROUTE_TABLE_CFG_SEG = 1,
	ROUTE_TABLE_CFG_LPM_KEY = 2,
	ROUTE_TABLE_CFG_LPM_ENTRY = 3,
	ROUTE_TABLE_CFG_EXP = 4,
	ROUTE_TABLE_CFG_DEF = 5,
	ROUTE_TABLE_CFG_RX_NPI = 6,
	ROUTE_TABLE_CFG_VL = 7,
	ROUTE_TABLE_CFG_TARGET_ID = 8,
	ROUTE_TABLE_CFG_TYPE_MAX
};

struct nl_id_inst {
	u32 devid;
	u32 fdieid;	 /* flatten dieid in dev, scope=[0, 3]. */
	u32 dbodyid;	/* bodyid id in udie, scope=[0, 4]. */
	u32 ssuid;	  /* ssu id in body, scope=[0, 1]. */
};

struct mami_log_cfg {
	union {
		struct {
			uint32_t rsv1[15];
			uint32_t configMask;			/* bit0 selfLogLevel, bit1 devid, bit2 devCurLogLevel */
											/* bit3 kernLogLevel, bit4 devDefaultLogLevel */
			uint32_t selfLogLevel;		  /* mami log level of the user process, i.e. the interface caller */
			uint32_t devid;				 /* devid to be operated on */
			uint32_t devCurLogLevel;		/* Log level of the management process where the device resides, depends on devid */
			uint32_t kernLogLevel;		  /* mami driver log level, depends on devid */
			uint32_t devDefaultLogLevel;	/* Default log level of the management process, depends on devid */
			uint32_t rsv2[27];
		};
		struct {
			uint32_t devid;
			uint32_t strLen;
			char *str;
			uint32_t rsv[44];
		} snapshot;
	};
};

enum mami_recreate_case {
	MAMI_KERN_RECREATE_CASE_INITIALIZE = 0,   /* Assigned when creating the device for the first time */
	MAMI_KERN_RECREATE_CASE_DEV_RESET = 1,	/* Device reset, the management process will re-dispatch the configuration */
	MAMI_KERN_RECREATE_CASE_PROC_RESTART = 2, /* Device management process restart, the management plane will restore configuration state data from the standby management device */
	MAMI_KERN_RECREATE_CASE_LIVE_MIGRATE = 3, /* Migration scenario, the management plane will restore configuration state data from the standby management device */
	MAMI_KERN_RECREATE_CASE_MAX
};

struct mami_dev_info {
	uint8_t guid[MAMI_GUID_LEN];
	uint32_t cna; /* cna address */
	uint32_t eid; /* Device eid information */
	uint32_t chipType; /* Device chipType, value refers to enum mami_dev_type */
	enum mami_recreate_case createCase; /* Device creation scenario */
	uint32_t rsv[8];
};

enum PCIE_I3C_OP_TYPE {
	PCIE_I3C_UB_MESSAGE = 0x8080,
	PCIE_I3C_GET_DEV_STAT = 0x8081,
	PCIE_I3C_REG_WR = 0x8082,
	PCIE_I3C_REG_RD = 0x8083
};

typedef struct {
	u32 op;
	u32 write_len;
	u32 read_len;
	void *write_buf;
	void *read_buf;
} drv_ubnic_xfer_msg;

typedef struct {
	u8 guid[MAMI_GUID_LEN];
	u32 write_addr_l;
	u32 write_addr_h;
	u32 write_data;
} i3c_reg_wr_req;

typedef struct {
	u32 status : 8;
	u32 rsv : 24;
} i3c_reg_wr_rsp;

typedef struct {
	u8 guid[MAMI_GUID_LEN];
	u32 read_addr_l;
	u32 read_addr_h;
} i3c_reg_rd_req;

typedef struct {
	u32 status : 8;
	u32 rsv : 24;
	u32 read_data;
} i3c_reg_rd_rsp;

typedef struct {
	u32 slot_id;
	u32 socket_id;
} i3c_dev_stat_req;

typedef struct {
	u8 guid[MAMI_GUID_LEN];
	u32 status : 8;
	u32 rsv : 24;
} i3c_dev_stat_rsp;


static inline int udev_guid_to_str(const u8 guid[], char *guid_str)
{
#ifdef __KERNEL__
	int ret = UBFM_SPRINTF(guid_str, GUID_STR_LEN, GUID_FMT,
		guid[0], guid[1], guid[2], guid[3], guid[4], guid[5], guid[6], guid[7],
		guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
#else
	int ret = sprintf_s(guid_str, GUID_STR_LEN, GUID_FMT,
		guid[0], guid[1], guid[2], guid[3], guid[4], guid[5], guid[6], guid[7],
		guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
#endif
	return (ret <= 0) ? -EINVAL : 0;
}

static inline int udev_str_to_guid(const char *guid_str, u8 param_guid[MAMI_GUID_LEN])
{
	u32 guid[MAMI_GUID_LEN];
	u32 i;
#ifdef __KERNEL__
	int ret = sscanf(guid_str, GUID_SCAN_FMT,
		&guid[0], &guid[1], &guid[2], &guid[3], &guid[4], &guid[5],
		&guid[6], &guid[7], &guid[8], &guid[9], &guid[10], &guid[11],
		&guid[12], &guid[13], &guid[14], &guid[15]);
#else
	int ret = sscanf_s(guid_str, GUID_SCAN_FMT,
		&guid[0], &guid[1], &guid[2], &guid[3], &guid[4], &guid[5],
		&guid[6], &guid[7], &guid[8], &guid[9], &guid[10], &guid[11],
		&guid[12], &guid[13], &guid[14], &guid[15]);
#endif
	if (ret != MAMI_GUID_LEN) {
		return -EINVAL;
	}
	for (i = 0; i < MAMI_GUID_LEN; i++) {
		param_guid[i] = (u8)guid[i];
	}
	return 0;
}

static inline void udev_guid_copy(u8 dst_guid[], const u8 src_guid[])
{
	int i;

	for (i = 0; i < MAMI_GUID_LEN; i++) {
		dst_guid[i] = src_guid[i];
	}
}

static inline int udev_guid_compare(u8 dst_guid[], const u8 src_guid[])
{
	int i;

	for (i = 0; i < MAMI_GUID_LEN; i++) {
		if (dst_guid[i] != src_guid[i]) {
			return -EINVAL;
		}
	}
	return 0;
}

#pragma pack(1)
struct udev_ue_info_from_bios {
	u8 ue_index;
	u8 ue_type;
	u8 ue_init  :1;
	u8 ue_enable:1;
	u8 port_id  :6;
	u8 mue_index;				/* the mue of vue */
	u8 devue_id;
	u8 vport_id : 7;
	u8 vport_enable : 1;
	u8 ue_sub_type;
	u8 ue_resv[3];
};

struct udev_ue_info {
	u16 ue_index;
	u8 ue_type;
	u8 ue_init  :1;
	u8 ue_enable:1;
	u8 port_id  :6;
	u8 mue_index;				/* the mue of vue */
	u8 devue_id;
	u8 vport_id : 7;
	u8 vport_enable : 1;
	u8 ue_sub_type;
	u8 ue_resv[3];
};

struct udev_mar_info {
	u32 ub_mem_decoder_hpa_cc_base;
	u32 ub_mem_decoder_hpa_cc_size;
	u32 ub_mem_decoder_hpa_nc_base;
	u32 ub_mem_decoder_hpa_nc_size;
};

struct udev_die_cfg_from_bios {
	u8 die_id;
	u8 ubc_ue0_guid[MAMI_GUID_LEN];
	u8 cdma_ue0_guid[MAMI_GUID_LEN];
	u8 urma_ue0_guid[MAMI_GUID_LEN];
	u8 cdma_soft_ue0;
	u8 urma_soft_ue0;
	u8 port_num;
	u64 ub_io_decoder_hpa_base;
	u64 ub_io_decoder_hpa_size;
	u32 ub_memory_version;
	u8 body_mask;
	struct udev_mar_info mar_info[UDEV_MAR_MAX_NUM];
	u16 decoder_entry_num;
	u16 ub_memory_token_id_max;
	u32 pcie_mask; /* PCIE mask 1 for PCIE,0 for UB */
	u8 rsvd[110];		/* 114 byte is reserved for ubmem future use */
	u8 ue_num;
	struct udev_ue_info_from_bios ue_info[];
};

struct udev_die_cfg_kernel {
	u8 die_id;
	u8 ubc_ue0_guid[MAMI_GUID_LEN];
	u8 cdma_ue0_guid[MAMI_GUID_LEN];
	u8 urma_ue0_guid[MAMI_GUID_LEN];
	u16 cdma_soft_ue0;
	u16 urma_soft_ue0;
	u8 port_num;
	u64 ub_io_decoder_hpa_base;
	u64 ub_io_decoder_hpa_size;
	u32 ub_memory_version;
	u8 body_mask;
	struct udev_mar_info mar_info[UDEV_MAR_MAX_NUM];
	u16 decoder_entry_num;
	u16 ub_memory_token_id_max;
	u32 pcie_mask; /* PCIE mask 1 for PCIE,0 for UB */
	u8 rsvd[110];		/* 114 byte is reserved for ubmem future use */
	u16 ue_num;
	struct udev_ue_info ue_info[];
};

struct udev_chip {
	u8 chip_index;
	u8 udie_num;
	struct udev_die_cfg_from_bios die_info[];
};

struct udev_chip_info {
	u8 chip_num;					/* max chip num is 4 */
	struct udev_chip *chip[CHIP_MAX_NUM];
	u8 *reserved;				   /* Point to reserved memory, size UDEV_CHIP_RESV_SIZE */
};

struct ub_io_reg_base {
	u64 start_base;
	u32 start_len;
};

struct ub_hsec_mem_entry {
	u32 entry_num;
	u32 entry_size;
};

struct ub_io_info {
	u8 reg_count;					   /* max reg_count is UB_IO_REG_NUM */
	struct ub_io_reg_base regs[UB_IO_REG_NUM];
	u64 resv_mem_base;
	u32 resv_mem_len;
};

struct udev_cfg {
	u16 device_type;
	u8 slot_id;	 /* Slot number of the current board in the rack */
	u8 socket_id;   /* Number of the current soc on the current board */
	u8 rec;		 /* 8byte alignment is reserved for future use */
	u64 ras_err_status_block_addr;
	u32 ras_err_status_block_len;
	u64 black_box_addr;
	u32 black_box_len;
};

struct udev_cfg_hdr {
	u16 crc;
	u16 length;	 /* Total length of the header and valid content */
	u8 magic[UDEV_CFG_HDR_MAGIC_LENGTH];
	u32 config_version;
	u32 ubCap;
	u8 reserved[4];	 /* 4 byte reserved for future use */
};

/* Device initialization table
 * +------------------------------+-----------------------+ 0x0000
 * |	udev_cfg_hdr			  |		 0x18		  |
 * +------------------------------+-----------------------+ 0x0018
 * |	udev_cfg				  |		 0x1D		  |
 * +------------------------------+-----------------------+ 0x0035
 * |	ub_io_info				|		 0x18D		 |
 * +------------------------------+-----------------------+ 0x01C2
 * |	chip_num				  |		 0x1		   |
 * +-------------+----------------+-----------------------+ 0x01C3
 * | chip0 info  | chip idx0	  |					   |
 * +-------------+----------------+-----------------------+
 * |			 | udie num	   |					   |
 * +-------------+----------------+-----------------------+
 * |			 |  die0 info	 |					   |
 * +-------------+----------------+-----------------------+
 * |			 |  ...		   |					   |
 * +-------------+----------------+-----------------------+
 * |			 |  dieN info	 |					   |
 * +-------------+----------------+-----------------------+
 * | ...		 |  ...		   |					   |
 * +-------------+----------------+-----------------------+
 * | chipN info  |  ...		   |					   |
 * +-------------+----------------+-----------------------+
 * | chip reserved				  |		 0x20		  |
 * +------------------------------+-----------------------+
 */
struct udev_cfg_info {
	struct udev_cfg cfg;
	struct ub_io_info ub_io;
	/* Note: starting with chip info, the field positions are not fixed, cannot be
	 * accessed directly through a struct pointer.
	 */
};
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* FMDEVM_H */
