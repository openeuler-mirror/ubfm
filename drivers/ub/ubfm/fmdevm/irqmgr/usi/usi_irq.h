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
#ifndef USI_IRQ_H
#define USI_IRQ_H

#include "ubfm_common.h"

#define USI_IRQ_NAME_LEN					32

enum usi_irq_vector {
	USI_IRQ_VEC_CTRLQ = 0,
	USI_IRQ_VEC_MSGQ,
	USI_IRQ_VEC_RAS,
	USI_IRQ_VEC_NUM,
};

struct usi_irq_data {
	char name[USI_IRQ_NAME_LEN];
	int irq;
	u32 devid;
	u32 dieid;
	u32 cna;
	u32 eid;
	u32 irq_idx;
};

struct usi_irq_data_vector {
	struct usi_irq_data vector[USI_IRQ_VEC_NUM];
	struct list_head entry;
};

#endif /* End of USI_IRQ_H */
