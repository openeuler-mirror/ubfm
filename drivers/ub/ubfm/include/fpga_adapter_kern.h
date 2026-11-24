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
#ifndef FPGA_ADAPTER_KERN_H
#define FPGA_ADAPTER_KERN_H

#include <linux/compiler.h>
#include "ubfm_common.h"
#include "ubfabric.h"

#define DRV_INST_ID_SELF 0xFFFFFFFF

#define UCMI_MAP_BAR_NUM_MAX 3
typedef struct {
	unsigned int chip_type;  /* Type 0x2:unions_iodie 0x3:Ascend_v1_iodie 0x4:1P kunpeng950_iodie 0x5:2P kunpeng950_iodie */
	unsigned int ucmi_map_bar_num;  /* Number of bars mapped by ucmi */
	unsigned long long base_addr[UCMI_MAP_BAR_NUM_MAX];  /* Base address of register space mapping */
	unsigned long long size[UCMI_MAP_BAR_NUM_MAX];       /* Length of register space mapping in PCIe space */
	unsigned int msi_start;   /* Starting msi interrupt number */
	unsigned int msi_len;     /* msi interrupt number length */
	unsigned int ucmi_id;     /* ucmi bus number */
	unsigned int devnum;      /* FPGA bdf number */
	unsigned int logic_id;    /* FPGA ID number */
	unsigned int udie_irq_index_id; /* index_id of module power-on/power-off/reset interrupt */
	unsigned int rsv[0x4];			/* Reserved field */
} chip_ub_mng;

typedef enum {
	DRV_INTR_TYPE_IODIE_POWER_DOWN = 64,		/* IODIE sub-card power-off interrupt */
	DRV_INTR_TYPE_IODIE_RESET = 65,				/* IODIE sub-card reset interrupt */
} drv_interrupt_type_e;

typedef unsigned int (*drv_interrupt_callback_fn)(unsigned int index_id, unsigned int type, void *p);

typedef struct {
	union {
		struct {
			unsigned int ras_int : 1;
			unsigned int abnormal_int : 1;
			unsigned int msgq_int : 1;
			unsigned int rsv : 29;
		} int_bit;
		unsigned int data;
	} int_unions;
	unsigned int ctrlq_int_low32;
	unsigned int ctrlq_int_high32;
	unsigned int rsv[8];
} fpga_int_reg;

typedef struct {
	unsigned int ind_fifo_ovwl_flag;
	unsigned int ind_fifo_debug_cnt;
	unsigned int rsv[8];
} rx_proc;

enum fpga_ops_type {
	FPGA_REG_OPS_TYPE,
	FPGA_API_OPS_TYPE,
	FPGA_MAX_OPS_TYPE
};

struct fpga_ops {
	int (*read_back_val)(u32 devid, u32 *val);
	int (*cfg_win_4m)(u32 devid, u32 win_4m_id, u32 value);
	int (*read_irq_status)(u32 devid, u32 die_id, u32 *status);
	int (*write_irq_status)(u32 devid, u32 die_id, u32 status);
	int (*read_irq_mask)(u32 devid, u32 die_id, u32 *mask);
	int (*write_irq_mask)(u32 devid, u32 die_id, u32 mask);
};

int starry_api_init(void);
void starry_api_uninit(void);

u32 starry_drv_board_get_ucmi_chip_num(u32 inst_id, u32 *num);
u32 starry_drv_board_get_tor_slotid(u32 *slot_id);
u32 starry_drv_get_ucmi_bsp_state(u32 inst_id, u32 chip_id, u32 *value);
u32 starry_drv_pci_get_device_bar(u32 dev_num, u32 uiBarNum, u64 *base, u64 *size);
u32 starry_drv_board_get_chip_mng(u32 inst_id, u32 chip_id, chip_ub_mng *chip_mng);
u32 starry_drv_intr_set_regist(u32 index_id, u32 type, drv_interrupt_callback_fn fn, void *p);
u32 starry_drv_intr_set_unregist(u32 index_id, u32 type);

u32 starry_drv_fpga_ucmi_int_status_read(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *status);
u32 starry_drv_fpga_ucmi_int_status_write(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg status);
u32 starry_drv_fpga_ucmi_int_mask_read(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg *mask);
u32 starry_drv_fpga_ucmi_int_mask_write(u32 inst_id, u32 chip_id, u32 die_id, fpga_int_reg mask);
u32 starry_drv_ucmi_4m_win_config(u32 inst_id, u32 chip_id, u32 win_id, u32 win_point);
u32 starry_drv_fpga_ucmi_get_rx_proc(u32 inst_id, u32 chip_id, rx_proc *proc);

int uop_fpga_info_init_kern(u32 devid, u32 socket_id, u64 back_reg_addr, u64 msi_int_base);
u32 uop_fpga_get_socket_id(u32 devid);
void __iomem *uop_fpga_get_back_vaddr(u32 devid);
int uop_fpga_get_back_paddr(u32 devid, u64 *phy_addr);
void __iomem *uop_fpga_get_msi_int_vaddr(u32 devid);
void uop_fpga_info_uninit_all_kern(void);

int ldst_back_readl_kern(u32 devid, u64 addr, u32 *data);
int ldst_back_writel_kern(u32 devid, u64 addr, u32 data);
int ldst_back_readq_kern(u32 devid, u64 addr, u64 *val);
int ldst_back_writeq_kern(u32 devid, u64 addr, u64 val);
int ldst_back_mem_read_kern(u32 devid, u64 addr, void *data, u32 len);
int ldst_back_mem_write_kern(u32 devid, u64 addr, void *data, u32 len);

void uop_kern_back_show(u32 devid, void (* show)(void *priv, const char *fmt, ...), void *priv);
void uop_fpga_back_enable(bool enable);
bool uop_get_fpga_back_state(void);

const struct fpga_ops *fpga_ops_get_g_reg_ops(void);
const struct fpga_ops *fpga_ops_get_g_api_ops(void);

const struct fpga_ops *fpga_ops_get_by_type(enum fpga_ops_type type);
const struct fpga_ops *fpga_ops_get(void);

int ucmi_starry_init(u32 devid);
void ucmi_starry_uninit(u32 devid);

typedef union {
	struct {
		unsigned int ctrlq_num : 2;  /* ctrlq number：2'b00=17；2'b01=48；2'b10=64 */
		unsigned int tx_type : 2;    /* type：2'b00=mgmt；2'b01=jetty */
		unsigned int rsv : 28;
	} int_bit;
	unsigned int data;
} ucmi_global_cfg_s;

u32 starry_drv_fpga_ucmi_write_ucmi_global_cfg(u32 inst_id, u32 chip_id, ucmi_global_cfg_s global_cfg);
u32 starry_drv_fpga_ucmi_write_chip_acc_win_doorbell(u32 inst_id, u32 chip_id, u32 die_id, u32 ucmi_db_win);
u32 starry_drv_fpga_ucmi_write_chip_acc_win_sqe(u32 inst_id, u32 chip_id, u32 sqe_win);
u32 starry_drv_fpga_ucmi_write_cfg_done_flag(u32 inst_id, u32 chip_id, u32 cfg_done);

#endif /* end of FPGA_ADAPTER_KERN_H */
