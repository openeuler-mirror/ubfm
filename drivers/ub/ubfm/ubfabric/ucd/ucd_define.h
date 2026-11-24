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
#ifndef UCD_DEFINE_H
#define UCD_DEFINE_H

#include "ulog.h"

/*
 * kernel log
 */
#define MODULE_UCD "UCD"

#define ucd_err(fmt, ...)  mami_err(MODULE_UCD, fmt, ##__VA_ARGS__)

#define ucd_warn(fmt, ...)  mami_warn(MODULE_UCD, fmt, ##__VA_ARGS__)

#define ucd_info(fmt, ...)  mami_info(MODULE_UCD, fmt, ##__VA_ARGS__)

#define ucd_event(fmt, ...)  mami_event(MODULE_UCD, fmt, ##__VA_ARGS__)

#define ucd_debug(fmt, ...)  mami_debug(MODULE_UCD, fmt, ##__VA_ARGS__)

#define ucd_ex_notsupport_err(ret, fmt, ...) do {  \
	if ((ret) != -EOPNOTSUPP) {  \
		mami_err(MODULE_UCD, fmt, ##__VA_ARGS__);  \
	}											  \
} while (0)


#define UCD_MAX_INPUT_LEN  4096
#define UCD_MAX_OUTPUT_LEN  (64 * 1024)


#endif  /* __UCD_DEFINE_H__ */
