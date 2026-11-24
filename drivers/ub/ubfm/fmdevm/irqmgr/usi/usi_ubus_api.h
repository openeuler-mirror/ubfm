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
#ifndef USI_UBUS_API_H
#define USI_UBUS_API_H

#include <ub/ubus/ubus.h>

int usi_ubus_api_init(void);
void usi_ubus_api_uninit(void);

int usi_ub_cfg_write_dword(struct ub_entity *uent, u64 pos, u32 val);
void usi_ub_disable_intr(struct ub_entity *uent);
int usi_ub_irq_vector(struct ub_entity *uent, unsigned int nr);
void usi_ub_unset_user_info(struct ub_entity *uent);
int usi_ub_set_user_info(struct ub_entity *uent);
void usi_ub_entity_enable(struct ub_entity *uent, u8 enable);
int usi_ub_alloc_irq_vectors(struct ub_entity *uent, unsigned int min, unsigned int max);
struct ub_entity *usi_ub_get_ent_by_eid(unsigned int eid);
int usi_ub_register_driver(struct ub_driver *drv);
void usi_ub_unregister_driver(struct ub_driver *drv);

#endif /* End of USI_UBUS_API_H */
