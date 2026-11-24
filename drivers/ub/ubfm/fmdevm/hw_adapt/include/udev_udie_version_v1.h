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
#ifndef _UDEV_HW_UDIE_VERSION_V1_H_
#define _UDEV_HW_UDIE_VERSION_V1_H_

#include "ubfabric.h"

#define UDIE_NL_NUM			 5
#define UDIE_NL_NUM_KUNPENG_960		  2

#define NBAR_BASE_OFFSET		0x0000000
#define NBAR_MISC_OFFSET		0x1000000
#define NBAR_TP_OFFSET		  0x2000000
#define NBAR_BA0_OFFSET		 0x3000000
#define NBAR_TA_OFFSET		  0x4000000
#define NBAR_NL0_OFFSET		 0x5000000
#define NBAR_BA1_OFFSET		 0x6000000
#define NBAR_NL1_OFFSET		 0x7000000
#define NBAR_BA2_OFFSET		 0x8000000
#define NBAR_NL2_OFFSET		 0x9000000
#define NBAR_BA3_OFFSET		 0xA000000
#define NBAR_NL3_OFFSET		 0xB000000
#define NBAR_BA4_OFFSET		 0xC000000
#define NBAR_NL4_OFFSET		 0xD000000
#define NBAR_UMMU_OFFSET		0xE000000
#define NBAR_CCUM_OFFSET		0xF000000
#define NBAR_D2H_OFFSET		 0xE000000

#define NBAR_CCUA0_OFFSET	   0x803800000

#define NBAR_MSGQ_OFFSET		0x10000000
#define NBAR_MGTQ_OFFSET		0x01100000

#define SFC_HISS_SIZE		   0x4000000	   /* 64MB */

#define SFC_HISS0_OFFSET		0x0000000
#define SFC_HISS1_OFFSET		0x1000000
#define SFC_HISS2_OFFSET		0x2000000
#define SFC_HISS3_OFFSET		0x3000000

/* dcip offset */
#define NBAR_DCIP0_OFFSET	   0x200000
#define NBAR_DCIP1_OFFSET	   0x700000

#define UDEV_NL_UMV_TABLE_OFFSET 0x2000
#define UDEV_NL_ULAN_TABLE_OFFSET 0x2200
#define UDEV_NL_MNG_TABLE_OFFSET 0x2400
#define UDEV_NL_DFX_OFFSET 0x1000

#define UDEV_UERES0_BASE			0x18000000
#define UDEV_UERES2_BASE			0x19000000
#define UDEV_UERES2_CTRLQ_OFFSET	0x18800

#define MBOX_HSEC_MEM_ENTRY_NUM	 0x1
#define MBOX_HSEC_MEM_ENTRY_SIZE	0x6000

/* DIE_NUM (2) * MUE_NUM (18) = 0x24 */
#define CCC_HSEC_MEM_ENTRY_NUM	   0x24
/* sizeof(struct mamiTplCccCtx) * 64 */
#define CCC_HSEC_MEM_ENTRY_SIZE	  0x1000

#ifdef FPGA_MODE_ENABLE
/* Support total 64(0X40) UE */
#define SEU_HSEC_MEM_ENTRY_NUM	  0x40
/* 4KB-aligned(sizeof(struct seid_tbl_data) * SEU_TABLE_ENTRY_NUM_MAX) = 4KB-aligned(32 * 32) */
#define SEU_HSEC_MEM_ENTRY_SIZE	 0x1000
#else
/* Support total 512(0X200) UE */
#define SEU_HSEC_MEM_ENTRY_NUM	  0x200
/* 4KB-aligned(sizeof(struct seid_tbl_data) * SEU_TABLE_ENTRY_NUM_MAX) = 4KB-aligned(32 * 1024) = 32768(0x8000) */
#define SEU_HSEC_MEM_ENTRY_SIZE	 0x8000
#endif

#define ESPI_HSEC_MEM_ENTRY_NUM	 0x0
#define ESPI_HSEC_MEM_ENTRY_SIZE	0x0

#define MSGQ_HSEC_MEM_ENTRY_NUM	 0x1		 /* 1 */
#define MSGQ_HSEC_MEM_ENTRY_SIZE	0xA00000	/* 10MB */

#define PROCESS_HSEC_MEM_ENTRY_NUM  0x0
#define PROCESS_HSEC_MEM_ENTRY_SIZE 0x0

#define QOS_HSEC_MEM_ENTRY_NUM  0x2
#define QOS_HSEC_MEM_ENTRY_SIZE 0x80000  /* 512KB */

#define IO_DECODER_HSEC_MEM_ENTRY_NUM 0x1
#define IO_DECODER_HSEC_MEM_ENTRY_SIZE 0x220000  /* 2M+128K */

#define MDP_HSEC_MEM_ENTRY_NUM 0x1
#define MDP_HSEC_MEM_ENTRY_SIZE 0x1000000  /* 16M */

#define UMC_HSEC_MEM_ENTRY_NUM 0x1
#define UMC_HSEC_MEM_ENTRY_SIZE 0x8000  // 32KB

#define CAP_HSEC_MEM_ENTRY_NUM 0x1
#define CAP_HSEC_MEM_ENTRY_SIZE 0x1000  // 4KB

#define TOPO_HSEC_MEM_ENTRY_NUM 0x1
#define TOPO_HSEC_MEM_ENTRY_SIZE 0x400000  // 4M

#define UBMEM_PA_UBA_HSEC_MEM_ENTRY_NUM 0x1
#define UBMEM_PA_UBA_HSEC_MEM_ENTRY_SIZE 0x100000 // 1M

#define TP_HSEC_MEM_ENTRY_NUM  0x1
#define TP_HSEC_MEM_ENTRY_SIZE 0

#define HSEC_MEM_ITEM(NAME) {						   \
	.base_name = NAME##_HSEC_MEM_BASE,				  \
	.entry_name = NAME##_HSEC_MEM_ENTRY,				\
	.entry_num = NAME##_HSEC_MEM_ENTRY_NUM,			 \
	.entry_size = NAME##_HSEC_MEM_ENTRY_SIZE}

#define HSEC_MEM_LAST_ITEM(NAME) HSEC_MEM_ITEM(NAME) /* this item is placed last, occupying all remaining memory */

#define HSEC_MEM_LIST {	  \
	HSEC_MEM_ITEM(MBOX),			\
	HSEC_MEM_ITEM(CCC),			 \
	HSEC_MEM_ITEM(SEU),			 \
	HSEC_MEM_ITEM(ESPI),			\
	HSEC_MEM_ITEM(MSGQ),			\
	HSEC_MEM_ITEM(PROCESS),		 \
	HSEC_MEM_ITEM(QOS),			 \
	HSEC_MEM_ITEM(IO_DECODER),	  \
	HSEC_MEM_ITEM(UMC),           \
	HSEC_MEM_ITEM(CAP),           \
	HSEC_MEM_ITEM(TOPO),			\
	HSEC_MEM_LAST_ITEM(TP)		   \
}

#define HSEC_MEM_WITH_MDP {	  \
	HSEC_MEM_ITEM(MBOX),			\
	HSEC_MEM_ITEM(CCC),			 \
	HSEC_MEM_ITEM(SEU),			 \
	HSEC_MEM_ITEM(ESPI),			\
	HSEC_MEM_ITEM(MSGQ),			\
	HSEC_MEM_ITEM(PROCESS),		 \
	HSEC_MEM_ITEM(QOS),			 \
	HSEC_MEM_ITEM(IO_DECODER),	  \
	HSEC_MEM_ITEM(MDP),			 \
	HSEC_MEM_ITEM(UMC),           \
	HSEC_MEM_ITEM(CAP),           \
	HSEC_MEM_ITEM(TOPO),           \
	HSEC_MEM_ITEM(UBMEM_PA_UBA),    \
	HSEC_MEM_LAST_ITEM(TP)		   \
}

#endif
