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
#ifndef I3C_KERN_H
#define I3C_KERN_H

#include "fmdevm.h"

typedef enum {
	I3C_STATUS_OK,
	I3C_STATUS_INVAL,
} i3c_status;

/* !
 * @brief		  Send the slave device address to 1825
 * @param[in]	  inst_id Pass DRV_INST_ID_SELF to get the whole board
 * @param[in]	  chip_id Indicates the 1825 chip number.
 * @retval		 DRV_OK(0) Function executed successfully
 * @retval		 Non-zero value Function failed and returned
 */
unsigned int i3c_drv_ubnic_probe(unsigned int inst_id, unsigned int chip_id);

/* !
 * @brief		  Send a write command to 1825, then get the acknowledgment reply from 1825
 * @param[in]	  inst_id Pass DRV_INST_ID_SELF to get the whole board
 * @param[in]	  chip_id Indicates the 1825 chip number.
 * @param[in]	  msg Pointer to the message to transfer
 * @retval		 DRV_OK(0) Function executed successfully
 * @retval		 Non-zero value Function failed and returned
 */
unsigned int i3c_drv_ubnic_xfer(unsigned int inst_id, unsigned int chip_id, drv_ubnic_xfer_msg *msg);

/* !
 * @brief		  Send a write command to 1825, then get the acknowledgment reply from 1825
 * @param[in]	  inst_id Pass DRV_INST_ID_SELF to get the whole board
 * @param[in]	  chip_id Indicates the 1825 chip number.
 * @param[in]	  status Get the presence status of 1825, 0 means present, 1 means not present
 * @retval		 DRV_OK(0) Function executed successfully
 * @retval		 Non-zero value Function failed and returned
 */
unsigned int i3c_drv_ubnic_online_status(unsigned int inst_id, unsigned int chip_id, unsigned int *status);

typedef unsigned int (*i3c_drv_ubnic_probe_func)(unsigned int inst_id, unsigned int chip_id);
typedef unsigned int (*i3c_drv_ubnic_xfer_func)(unsigned int inst_id, unsigned int chip_id, drv_ubnic_xfer_msg *msg);
typedef unsigned int (*i3c_drv_ubnic_online_status_func)(unsigned int inst_id, unsigned int chip_id, unsigned int *status);

int i3c_symbol_init(void);
void i3c_symbol_uninit(void);

#endif
