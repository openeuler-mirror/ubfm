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
#ifndef MSGQ_SOC_V1_H
#define MSGQ_SOC_V1_H

#define MSGQ_CHAN_ID	2

#define MSGQ_REG_BASE				   0x0
#define MSGQ_REG_OFFSET				 (MSGQ_REG_BASE + (0x120 * MSGQ_CHAN_ID))

#define MSGQ_SQ_ADDR_L				  0x0
#define MSGQ_SQ_ADDR_H				  0x4
#define MSGQ_SQ_TAIL					0x8
#define MSGQ_SQ_HEAD					0xC
#define MSGQ_SQ_DEPTH				   0x10
#define MSGQ_SQ_STATUS				  0x14
#define MSGQ_SQ_INT_MASK				0x18

#define MSGQ_RQ_ADDR_L				  0x40
#define MSGQ_RQ_ADDR_H				  0x44
#define MSGQ_RQ_TAIL					0x48
#define MSGQ_RQ_HEAD					0x4C
#define MSGQ_RQ_DEPTH				   0x50
#define MSGQ_RQ_SIZE					0x54
#define MSGQ_RQ_STATUS				  0x58

#define MSGQ_CQ_ADDR_L				  0x70
#define MSGQ_CQ_ADDR_H				  0x74
#define MSGQ_CQ_TAIL					0x78
#define MSGQ_CQ_HEAD					0x7C
#define MSGQ_CQ_DEPTH				   0x80
#define MSGQ_CQ_STATUS				  0x84

#define MSGQ_CQ_INT_MASK				0x88
#define MSGQ_CQ_INT_STATUS			  0x8C
#define MSGQ_CQ_INT_RO				  0x90
#define MSGQ_CQ_INT_SET				 0x94

#define MSGQ_QUE_RST					0xB0
#define MSGQ_INT_SEL					0xC0

#define MSGQ_REG_SQ_BASE_L			  (MSGQ_REG_OFFSET + MSGQ_SQ_ADDR_L)
#define MSGQ_REG_SQ_BASE_H			  (MSGQ_REG_OFFSET + MSGQ_SQ_ADDR_H)
#define MSGQ_REG_SQ_PI				  (MSGQ_REG_OFFSET + MSGQ_SQ_TAIL)
#define MSGQ_REG_SQ_CI				  (MSGQ_REG_OFFSET + MSGQ_SQ_HEAD)
#define MSGQ_REG_SQ_DEP				 (MSGQ_REG_OFFSET + MSGQ_SQ_DEPTH)
#define MSGQ_REG_SQ_STATUS			  (MSGQ_REG_OFFSET + MSGQ_SQ_STATUS)
#define MSGQ_REG_SQ_INT_MASK			(MSGQ_REG_OFFSET + MSGQ_SQ_INT_MASK)

#define MSGQ_REG_CQ_BASE_L			  (MSGQ_REG_OFFSET + MSGQ_CQ_ADDR_L)
#define MSGQ_REG_CQ_BASE_H			  (MSGQ_REG_OFFSET + MSGQ_CQ_ADDR_H)
#define MSGQ_REG_CQ_PI				  (MSGQ_REG_OFFSET + MSGQ_CQ_TAIL)
#define MSGQ_REG_CQ_CI				  (MSGQ_REG_OFFSET + MSGQ_CQ_HEAD)
#define MSGQ_REG_CQ_DEP				 (MSGQ_REG_OFFSET + MSGQ_CQ_DEPTH)
#define MSGQ_REG_CQ_STATUS			  (MSGQ_REG_OFFSET + MSGQ_CQ_STATUS)

#define MSGQ_REG_RQ_BASE_L			  (MSGQ_REG_OFFSET + MSGQ_RQ_ADDR_L)
#define MSGQ_REG_RQ_BASE_H			  (MSGQ_REG_OFFSET + MSGQ_RQ_ADDR_H)
#define MSGQ_REG_RQ_PI				  (MSGQ_REG_OFFSET + MSGQ_RQ_TAIL)
#define MSGQ_REG_RQ_CI				  (MSGQ_REG_OFFSET + MSGQ_RQ_HEAD)
#define MSGQ_REG_RQ_DEP				 (MSGQ_REG_OFFSET + MSGQ_RQ_DEPTH)
#define MSGQ_REG_RQ_SIZE				(MSGQ_REG_OFFSET + MSGQ_RQ_SIZE)
#define MSGQ_REG_RQ_STATUS			  (MSGQ_REG_OFFSET + MSGQ_RQ_STATUS)

#define MSGQ_REG_QUE_RST				(MSGQ_REG_OFFSET + MSGQ_QUE_RST)

#endif
