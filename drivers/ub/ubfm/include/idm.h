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
#ifndef IDM_H
#define IDM_H

#include <linux/types.h>


/* f: flatten => fdieid: flatten dieid.
 * ------------------------------------------------------
 * die_num_per_chip: K=1; A=2; U=2.
 * chip_num_in_dev: K=1,2,4; A=1; U=1;
 * die_num_in_dev: K=1,2,4; A=2; U=2;
 * K: dieid=0; A: dieid=[0, 1]; U: dieid=[0, 1].
 * K: fdieid=[0, 3]; A: fdieid=[0, 1]; U: fdieid=[0, 1].
 * K: chipid=[0, 3]; A: chipid=0; U: chipid=0.
 **/
int idm_kern_get_chip_die_num(u32 devid, u32 *die_num_per_chip, u32 *die_num_in_dev, u32 *chip_num_in_dev);

int idm_kern_lgid2phyid(u32 devid, struct mami_logical_id_info lg_id_info, struct mami_physical_id_info *phy_id_info);

int idm_kern_phyid2lgid(u32 devid, struct mami_physical_id_info phy_id_info, struct mami_logical_id_info *lg_id_info);

int idm_kern_get_plane_by_fdie(u32 devid, u32 fdieid, u32 *plane_id);

/* get body id(0-19) in dev, get port id(0-3) in body */
int idm_kern_get_fbody_bport_by_lgport(u32 devid, u32 lgport, u32 *fbodyid, u32 *bportid);
int idm_kern_get_lgport_by_fbody_bport(u32 devid, u32 fbodyid, u32 bportid, u32 *lgport);
int idm_kern_get_inport_by_chip_die_port(u32 devid, u32 chipid, u32 dieid, u32 portid, u32 *in_port);
int idm_kern_get_portset_by_chip_die_body(u32 devid, u32 chipid, u32 dieid, u32 body, u32 *portset);

bool idm_kern_is_nl_valid(u32 devid, u32 fdieid, u32 dbodyid);

#endif
