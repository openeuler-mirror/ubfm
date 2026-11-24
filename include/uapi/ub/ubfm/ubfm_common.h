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
#ifndef UBFM_COMMON_H
#define UBFM_COMMON_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "ubfm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: This is the common definition of user and kernel */
#if !(defined(__KERNEL__) && defined(__clang__))
#ifndef u64
typedef unsigned long long u64;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u16
typedef unsigned short u16;
#endif

#ifndef u8
typedef unsigned char u8;
#endif

#endif

#define MAMI_GUID_LEN 16
#define MAMI_PLANE_MAX_NUM 4

#define MAMI_EID_SIZE 128 /* byte */
#define MAMI_EID_DWORD_NUM (MAMI_EID_SIZE / 32)

/* Special event sequence number definitions */
#define MAMI_EVENT_SERIALNUM_NEWEST		 0xFFFFFFFF   /* Get the latest event, returns the eventSerialNum of the latest event */
#define MAMI_EVENT_SERIALNUM_LATEST		 0xFFFFFFFE   /* Get multiple events starting from the oldest, returns the eventSerialNum of the last event obtained */

#define MAMI_MAX_EVENT_NAME_LENGTH		  256
#define MAMI_MAX_EVENT_DATA_LENGTH		  128
#define MAMI_MAX_EVENT_RESV_LENGTH		  64

#define DIE_MAX_NUM	 MAMI_PLANE_MAX_NUM
#define DEV_MAX_NUM	 128U   /* npu + cpu + switch */
#define DEV_DIE_MAX_NUM 8
#define NL_PORT_MAX_NUM 4
#define DEV_PORT_MAX_NUM 64
#define NL_PORT_NUM_X4 2

#define MAMI_ANY_DEVID DEV_MAX_NUM

#define NL_NUM_PER_UDIE 5U
#define NL_NUM_PER_NDIE 2U

#define NL_MAX_NUM	  NL_NUM_PER_UDIE	  /* nl max number each die */

#define DIE_PORT_MAX_NUM	  4U	  /* PORT max number each nl */
#define PORT_MAX_NUM	  (NL_MAX_NUM * DIE_PORT_MAX_NUM)	  /* nl max number each die */

#define UNUSED(x)   ((void)(x))

#define PCIE_BAR_ERROR_VAL (0xFFFFFFFFU)
#define UCMI_PCIE_AXI_NUM 16U

#define UCMI_PCIE_WIN_4M_SIZE	   (4 * 1024 * 1024)

#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))

/* If tx or rx by cdma, the packet length is 256 bytes at most each time, which is limited by A's sram. */
#define MAMI_CDMA_RW_ONCE 0x100U

#ifdef EMU_ST
#define STATIC
#else
#define STATIC static
#endif

#define E_OPNOTSUPP_ON_DEV 0x1002 /* operation not supported on current device */
#define E_RES_NOT_INIT 0x1003 /* resource not init on current device */

/*
 * It is a brief of physical id.
 * |---4bit---|---4bit---|-----8bit-------|-----2bit-------|-----6bit-------|-----8bit-------|
 * |--chipid--|--cluster-|-----dieid------|-----ssu--------|-----body-------|----portid------|
 */
#define MAMI_P_CHIP(chipId)		 (((chipId) & 0xF) << 0x1C)
#define MAMI_P_CLUSTER(clusterId)   (((clusterId) & 0xF) << 0x18)
#define MAMI_P_DIE(dieId)		   (((dieId) & 0xFF) << 0x10)
#define MAMI_P_BODY(bodyId)		 (((bodyId) & 0x3F) << 0x8)
#define MAMI_P_SSU(ssuId)		   (((ssuId) & 0x3) << 14)
#define MAMI_P_PORT(portId)		 ((portId) & 0xFF)

#define MAMI_MAKE_PHY_ID(chipId, clusterId, dieId, bodyId, portId) \
											(MAMI_P_CHIP(chipId) | \
											MAMI_P_CLUSTER(clusterId) | \
											MAMI_P_DIE(dieId) | \
											MAMI_P_BODY(bodyId) | \
											MAMI_P_PORT(portId))

#define MAMI_MAKE_PHY_ID_SSU(chipId, clusterId, dieId, bodyId, ssuId, portId) \
											(MAMI_P_CHIP(chipId) | \
											MAMI_P_CLUSTER(clusterId) | \
											MAMI_P_DIE(dieId) | \
											MAMI_P_BODY(bodyId) | \
											MAMI_P_SSU(ssuId) | \
											MAMI_P_PORT(portId))

#define MAMI_GET_PHY_CHIP_ID(physicalId)	(((physicalId) >> 0x1C) & 0xF)
#define MAMI_GET_PHY_CLUSTER_ID(physicalId) (((physicalId) >> 0x18) & 0xF)
#define MAMI_GET_PHY_DIE_ID(physicalId)	 (((physicalId) >> 0x10) & 0xFF)
#define MAMI_GET_PHY_BODY_ID(physicalId)	(((physicalId) >> 0x8) & 0x3F)
#define MAMI_GET_PHY_SSU_ID(physicalId)	 (((physicalId) >> 14) & 0x3)
#define MAMI_GET_PHY_PORT_ID(physicalId)	((physicalId) & 0xFF)

enum mami_udie_version {
	UDIE_VERSION_V1 = 0,
	UDIE_VERSION_V2 = 1,
	UDIE_VERSION_MAX
};

/* Method of the device prober */
enum udm_dev_location {
	UDM_LOCAL, /* Local device, with all control capabilities of the SOC */
	UDM_NEAR, /* Near devices, connected through buses such as PCIe and Ub, with partial control capabilities of SOC. */
	UDM_REMOTE, /* Remote device, connected through the network/cdma/msgq/etc cannot directly control the SOC. */
	UDM_LOCATION_MAX
};

enum udev_connect_type {
	UDEV_PCIE_ESPI = 0,
	UDEV_ESPI,
	UDEV_LOCAL,
	UDEV_UBUS,
	UDEV_PCIE_I3C,
	UDEV_VM,
	UDEV_CONNECT_TYPE_NUM
};

struct udev_hw_identify {
	u8 guid[MAMI_GUID_LEN];
	u32 slot_id;
	u32 socket_id;
	u32 chip_type;
	u32 location;	   /* LOCAL, NEAR, REMOTE */
	u32 connect_type;   /* PCIE, ESPI, UB, LOCAL */
	u32 access_type;	/* LOCAL, CDMA, MSGQ */
	u64 reg_base;
	u64 mem_base;
	u64 resv_mem_base;
	u64 resv_mem_size;
};

struct udev_ucmi_axi_id_info {
	u32 ucmi_axi_id_val[DIE_MAX_NUM];
};

enum port_link_mode {
	DISABLE_MODE = 0,
	ETH_MODE = 1,
	UB_MODE = 2,
	PCIE_MODE = 4
};

/* port_mode, only used when port_mode_en is true */
enum port_lane_mode {
	PORT_MODE_X4,
	PORT_MODE_X2,
};

struct port_lane_state {
	enum port_lane_mode lane_mode;
	u32 lane_mode_en;
};

enum body_lane_mode {
	BODY_LANE_MODE_2X4 = 0,	/* Pure X4 mode */
	BODY_LANE_MODE_4X2,		/* Pure X2 mode */
	BODY_LANE_MODE_1X4_2X2, /* 1X4 + 2x2 mixed */
	BODY_LANE_MODE_2X2_1X4, /* 2X2 + 1x4 mixed */
};

enum ndie_p2p_shift_mode {
	SHIFT_MODE_00,
	SHIFT_MODE_01,
	SHIFT_MODE_10,
	SHIFT_MODE_11,
	SHIFT_MODE_MAX
};

enum ndie_p2p_port_mode {
	PORT_MODE_8,
	PORT_MODE_16,
	PORT_MODE_32,
	PORT_MODE_64,
	PORT_MODE_MAX
};

enum mami_logic_id_type {
	MAMI_KERN_LOGIC_ID_TYPE_DEVICE = 0,
	MAMI_KERN_LOGIC_ID_TYPE_PLANE = 1,
	MAMI_KERN_LOGIC_ID_TYPE_PORT_SET = 2,
	MAMI_KERN_LOGIC_ID_TYPE_PORT = 3,
	MAMI_KERN_LOGIC_ID_TYPE_MAX = 4,
};

enum mami_physic_id_type {
	MAMI_KERN_PHYSIC_ID_TYPE_GUID = 0,
	MAMI_KERN_PHYSIC_ID_TYPE_CHIP = 1, /* generated from chip id */
	MAMI_KERN_PHYSIC_ID_TYPE_CLUSTER = 2,
	MAMI_KERN_PHYSIC_ID_TYPE_DIE = 3, /* generated from chip id, die id */
	MAMI_KERN_PHYSIC_ID_TYPE_BODY = 4, /* generated from chip id, die id, body id, ssu id */
	MAMI_KERN_PHYSIC_ID_TYPE_PORT = 5, /* generated from chip id, port id */
	MAMI_KERN_PHYSIC_ID_TYPE_MAX = 6,
};

struct mami_logical_id_info {
	enum mami_logic_id_type logicalIdType;
	uint32_t logicalIdVal;
	uint32_t rsv;
};

struct mami_physical_id_info {
	enum mami_physic_id_type physicalIdType;
	uint32_t physicalIdVal;
	uint32_t rsv;
};

enum mami_log_level {
	MAMI_KERN_LOG_EMERG,
	MAMI_KERN_LOG_ALERT,
	MAMI_KERN_LOG_CRIT,
	MAMI_KERN_LOG_ERR,
	MAMI_KERN_LOG_WARNING,
	MAMI_KERN_LOG_NOTICE,
	MAMI_KERN_LOG_INFO,
	MAMI_KERN_LOG_DEBUG,
	MAMI_KERN_LOG_MAX,
};

enum mami_assert_type {
	MAMI_KERN_FAULT_RECOVERY = 0,
	MAMI_KERN_FAULT_OCCUR,
	MAMI_KERN_ONE_TIME_EVENT,
	MAMI_KERN_ASSERT_TYPE_MAX
};

enum mami_severity_level {
	MAMI_KERN_EVENT_INFO = 0,
	MAMI_KERN_EVENT_MINOR,
	MAMI_KERN_EVENT_MAJOR,
	MAMI_KERN_EVENT_CRIT,
	MAMI_KERN_EVENT_MAX
};

struct mami_eid {
	uint8_t compressedFlag; /* 0: not compress eid is 128 bit; 1: compress eid is 20 bit */
	uint8_t rsv[3];
	union {
		uint32_t eid[MAMI_EID_DWORD_NUM];
		uint32_t compressedEid; /* 20bit */
	};
};


struct mami_fault_event {
	uint32_t eventId;		 /* Event id，include: host/device, node_type, sensor_type, event_type */
	uint32_t devid;			 /* Device id */
	enum mami_logic_id_type nodeIdType; /* Node id type */
	uint32_t nodeId;			/* Node id */
	uint64_t detailCode; /* Detailed code: combining RAS Code and Event ID to distinguish RAS details. */
	uint8_t severity;		/* Event level: 0: Informational, 1: Minor, 2: Major, 3: Critical */
	uint8_t assertion;		/* Event type: 0: Fault recovery，1: Fault occurrence, 2: One-Time Event */
	uint32_t eventSerialNum;	/* Alarm sequence number */
	uint64_t alarmRaisedTime;	/* Event Generation Time: milliseconds since January 1, 1970, 00:00:00 UTC */
	char eventName[MAMI_MAX_EVENT_NAME_LENGTH];	 /* Event desc(String): recommended for unique ID and location. */
	uint8_t additionalInfo[MAMI_MAX_EVENT_DATA_LENGTH];	 /* Event additional information (Binary) */
	uint8_t resv[MAMI_MAX_EVENT_RESV_LENGTH];	/* Reserved Area: Extended content added above with size unchanged. Only pre-Reserved length is validated. */
};

struct mami_read_event_param {
	int32_t timeout;				/* Timeout in ms, -1: never timeout, [-1, 30000] */
	uint32_t eventSerialNum;		/* Return events after this sequence number; special event sequence number, behavior depends on this sequence number */
	struct mami_fault_event *event;	/* Array storing events */
	uint32_t inEventNum;			/* Array size */
	uint32_t outEventNum;			/* Number of returned events, [0, inEventNum] */
	uint32_t rsv[10];
};


#ifdef __cplusplus
}
#endif

#endif
