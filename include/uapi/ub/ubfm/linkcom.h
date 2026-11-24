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
#ifndef LINKCOM_H_
#define LINKCOM_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "ubfm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * It is a brief of linkcom handle.
 * |-------------------------------------------int(32bit)-----------------------------------------------|
 * |---vld----|----------rsv-----------|-----------dev_id-------------|-----------channel_id------------|
 * |---1bit---|----------7bit----------|-----------16bit--------------|--------------8bit---------------|
 * |----------|------------------------|----------[0xFFFF00]----------|------------[0xFF]---------------|
 * valid : vld - 0
 * invalid : vld - 1
 */

#define LINKCOM_DIE_MAX_CHAN_NUM	 4
#define LINKCOM_UNIONS_MAX_CHAN_NUM  8

#define LINKCOM_CHANNEL_OFFSET 0
#define LINKCOM_CHANNEL_MASK   0xFF
#define LINKCOM_DEVID_OFFSET   8
#define LINKCOM_DEVID_MASK	 0xFFFF
#define LINKCOM_VLD_OFFSET	 31
#define LINKCOM_VLD_MASK	   0x1

#define UNMASK_LINKCOM_HANDLE_TO_CHANNEL(handle)  (((handle) >> LINKCOM_CHANNEL_OFFSET) & LINKCOM_CHANNEL_MASK)
#define UNMASK_LINKCOM_HANDLE_TO_DEVID(handle)	(((handle) >> LINKCOM_DEVID_OFFSET) & LINKCOM_DEVID_MASK)
#define UNMASK_LINKCOM_HANDLE_TO_VLD(handle)	  (((handle) >> LINKCOM_VLD_OFFSET) & LINKCOM_VLD_MASK)

#define MASK_LINKCOM_HANDLE(dev_id, chan_id)	  ((((chan_id) & LINKCOM_CHANNEL_MASK) << LINKCOM_CHANNEL_OFFSET) | \
													(((dev_id) & LINKCOM_DEVID_MASK) << LINKCOM_DEVID_OFFSET))

#define INVALID_LINKCOM_HANDLE	(-1)

enum mami_linkcom_minor_cmd {
	MAMI_KERN_UBLINKCOM_OPEN = 0,
	MAMI_KERN_UBLINKCOM_CLOSE,
	MAMI_KERN_UBLINKCOM_CONFIG,
	MAMI_KERN_UBLINKCOM_DROP_CONFIG,
	MAMI_KERN_UBLINKCOM_SEND_STATE_ADDR,
	MAMI_KERN_LINKCOM_MAX
};

struct mami_pkt_send_para {
	uint32_t lgPortId; /* Destination port */
	uint32_t cfgType; /* Packet type */
	uint32_t vl;  /* vl */
	void *pkt; /* Data start address of the packet to be sent, allocated and freed by the caller */
	uint32_t pktLen; /* Indicates the size of the packet to be sent */
	uint32_t rsv[16];
};

struct mami_pkt_recv_para {
	uint32_t reason;  /* Packet report reason: 0: CAM hit; 1: protocol check failed, e.g. ttl check failed; 2: route miss; 3: npi check failed; 4: enumeration or local packet hit; 5: mirrored packet; 6: ncsi packet */
	uint32_t lgPortId; /* Source port of the packet */
	uint32_t enumVld; /* Whether it is an enumeration packet: 1 for enumeration packet, 0 for non-enumeration packet */
	uint32_t msgCode; /* message code in AH (this field is only valid for config packets and UB MCTP, invalid for enumeration packets) */
	void *pktBuff; /* Data buffer for receiving packets, allocated by the caller */
	uint32_t buffLen; /* Indicates the space size of pkt_buff when called */
	uint32_t recvLen; /* Indicates the actual size of the received packet */
	uint32_t rsv[16];
};

#ifdef __cplusplus
}
#endif

#endif
