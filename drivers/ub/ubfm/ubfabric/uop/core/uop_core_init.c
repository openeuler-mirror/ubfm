// SPDX-License-Identifier: GPL-2.0-only
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
#include "uop_kern.h"
#include "uop_proc.h"
#include "uop_core_init.h"

int uop_core_mod_init(void)
{
	uop_kern_mod_init();
	uop_proc_mod_init();
	return 0;
}

void uop_core_mod_uninit(void)
{
	uop_proc_mod_uninit();
	uop_kern_mod_uninit();
}

