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
#ifndef RAS_DEF_H
#define RAS_DEF_H

#include "ubfm_common.h"

enum RAS_ERRDATA_TYPE {
	RAS_ERRDATA_TYPE_LOCAL = 1,
	RAS_ERRDATA_TYPE_PROTOC = 2,
	RAS_ERRDATA_TYPE_MAX
};

#pragma pack(1)

/* According to the RAS Data structure definition, the minimum is 16K */
#define RAS_DATA_MEM_SIZE					   0x4000
#define RAS_INFO_HEADER_MAGIC_LEN			   8
#define RAS_INFO_HEADER_MAGIC				   "RASINFO\0"

#define RAS_DATA_ENTRY_MIN_SIZE				 0x400
#define RAS_DATA_ENTRY_MAX_SIZE				 0x800

struct ras_data_header {
	char magic[RAS_INFO_HEADER_MAGIC_LEN];
	u32 version : 8;				/* [7:0] reserved for future protocol upgrades */
	u32 valid : 1;				  /* [8] indicates whether the RAS info shared memory is valid, 1 means initialized and usable, 0 means not initialized */
	u32 auto_ubc : 1;			   /* [9] in multi-UBC scenarios supports selecting an appropriate UBC, 0 means auto-selection not supported and local UBC is selected, 1 means auto-selection is supported */
	u32 resv0 : 6;				  /* [15:10] reserved */
	u32 aging_time : 8;			 /* [23:16] aging time reported by UBC when auto-selecting UBC */
	u32 item_num : 8;			   /* [31:24] number of entries */
	u32 item_len;				   /* size of a single RAS entry */
	u64 valid_submodule;			/* each bit represents a submodule, 1 means filtered, 0 means not filtered */
	u32 avail_ubc_bitmap;		   /* 32bit, each bit indicates whether ubc is valid, 0 means invalid, 1 means valid */
	u8 avail_ubc_count;			 /* indicates the number of available reporting ubcs */
	u8 control_mode;				/* RAS reporting mode, 1 means in-band, 1 means out-of-band */
	u16 header_crc;				 /* header crc */
};

struct ras_data_entry_item {
	u64 ras_info_addr;		  /* RAS entry data start address */
	u32 ras_info_len;		   /* RAS data length, in bytes */
	u16 used : 1;			   /* [0] Bit0 indicates whether this entry has RAS data, 1 means has data, 0 means idle */
	u16 type : 3;			   /* [3:1] records RAS type, 1 means LocalRAS, 2 means protocol RAS */
	u16 resv0 : 12;			 /* [15:4] reserved */
	u16 crc;					/* CRC value of each RAS entry */
	u64 time_stamp;			 /* records RAS */
	u32 ubc_bitmap;
	u8 chip_id;
	u8 die_id;
	u16 resv1;
};

struct ras_data_entry {
	u32 item_num;
	struct ras_data_entry_item entry[];
};

/* Local RAS refers to the CPU's UB RAS */
struct local_ras_error_data {
	u32 validation_bits;
	u8 version;
	u8 soc_id;
	u8 socket_id;
	u8 totem_id;
	u8 nimbus_id;
	u8 sub_system_id;
	u8 module_id;
	u8 sub_module_id;
	u8 core_id;
	u8 port_id;
	u16 err_type;
	u64 pcie_info;
	u8 err_severity;
	u8 resv[3];
	u32 register_array_size;
	u8 register_array[];
};

typedef union {
	struct {
		u64 eid_valid : 1;			  /* Bit[0] */
		u64 cna_valid : 1;			  /* Bit[1] */
		u64 port_valid : 1;			 /* Bit[2] */
		u64 base_info_valid : 1;		/* Bit[3] */
		u64 overflow_valid : 1;		 /* Bit[4] */
		u64 aer_info_valid : 1;		 /* Bit[5] */
		u64 reserved : 58;			  /* Bit[63..6] */
	} bits;
	u64 val;
} valid_field;

struct ubdev_base_info {
	u32 oui;
	u16 dev_id;
	u16 dev_type;
	u16 class_code;
	u16 reserved;
};

#define RAS_SPEC_ERR_LEN						28
#define RAS_VENDOR_ERR_LEN					  2

struct ubdev_aer_info {
	u32 err_ctrl;
	u64 uncor_status;
	u64 uncor_mask;
	u64 uncor_severity;
	u64 cor_status;
	u64 cor_mask;
	u64 cor_severity;
	u32 err_info_ctrl;
	u32 ub_def_err_info[RAS_SPEC_ERR_LEN];
	u32 vendor_def_err_info[RAS_VENDOR_ERR_LEN];
};

/* RAS carried by the UB protocol */
struct protoc_ras_error_data {
	valid_field field;
	u32 eid;
	u32 cna;
	u32 port;
	struct ubdev_base_info base_info;
	u32 overflow_flag;
	struct ubdev_aer_info err_info;
};

#pragma pack()

#endif /* End of RAS_DEF_H */
