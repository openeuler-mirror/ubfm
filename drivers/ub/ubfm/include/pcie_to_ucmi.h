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
#ifndef PCIE_TO_UCMI_H
#define PCIE_TO_UCMI_H

#include "ubfm_common.h"

#define UCMI_PCIE_DYN_4M_WIN_NUM 4ULL

/**
 * @brief config dynamic 4m window reg
 * @param [in] : devid, device id
 * @param [in] : dyn_4m_id, index of the 4m window, valid range: 0 to UCMI_PCIE_DYN_4M_WIN_NUM (exclusive)
 * @param [in] : aligned_addr, addr be 4m aligned
 * @param [out] : local_addr, local addr
 * @return success (0) or error code
 */
int ucmi_cfg_dyn_win(u32 devid, u32 dyn_4m_id, u64 aligned_addr, u64 *local_addr);

/**
 * @brief clear dynamic 4m window reg
 * @param [in] : devid, device id
 * @param [in] : dyn_4m_id, index of the 4m window, valid range: 0 to UCMI_PCIE_DYN_4M_WIN_NUM (exclusive)
 */
void ucmi_clear_dyn_win(u32 devid, u32 dyn_4m_id);

int ucmi_get_local_addr(u32 devid, u32 win_4m_id, u64 *local_addr);
int ucmi_get_idle_local_win(u32 devid, u32 *win_4m_id_start, u32 *win_4m_num, u32 *win_4m_size);
void ucmi_clear_win(u32 devid, u32 win_4m_id);
int ucmi_cfg_win(u32 devid, u32 win_4m_id, u64 aligned_addr, u32 *axi_id_out);

#endif /* end of PCIE_TO_UCMI_H */
