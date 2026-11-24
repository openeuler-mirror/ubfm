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
/* sys */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
/* external */
#include "mami_string.h"
/* user-space */
#include "ubfm_common.h"
/* kern-space common */
#include "ures.h"
#include "idm.h"
/* module internal */
#include "mng_kernel.h"
#include "fmdevm.h"

struct idm_kern_map {
	u32 is_init;
	enum mami_dev_type chip_type;
	enum mami_udie_version udie_version;
	u32 nl_num_per_die;
	u32 die_num_per_chip;
	u32 die_num_in_dev;
	u32 chip_num_in_dev;
	u32 phyport_to_lgport[DEV_PORT_MAX_NUM];
	u8 portset_to_body[DEV_DIE_MAX_NUM][NL_MAX_NUM];
	u8 body_to_portset[DEV_DIE_MAX_NUM][NL_MAX_NUM];
	struct port_info_table table;
};

STATIC struct idm_kern_map g_idm_map[DEV_MAX_NUM] = {0};
static DEFINE_MUTEX(g_idm_kern_map_mutex);

static int idm_get_port_info_table(u32 devid)
{
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_PORT_INFO_TBL, &g_idm_map[devid].table, sizeof(struct port_info_table));
	if (ret != 0) {
		mng_err("get port info table failed, ret=0x%x.\n", (u32)ret);
		return ret;
	}

	return 0;
}

static int idm_kern_init_ures_info(u32 devid)
{
	u64 die_num_each_chip = 0, die_num_in_device = 0, chip_num_in_device = 0, nl_num_per_die = 0, udie_version = 0;
	struct udev_hw_identify hw = {0};
	int ret;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		mng_err("Failed to get udev info. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_DIE_NUM_PER_CHIP, &die_num_each_chip);
	if (ret != 0) {
		mng_err("Failed to get die_num_each_chip. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &die_num_in_device);
	if (ret != 0) {
		mng_err("Failed to get die_num_in_device. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_CHIP_NUM, &chip_num_in_device);
	if (ret != 0) {
		mng_err("Failed to get chip_num_in_device. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_NL_NUM, &nl_num_per_die);
	if (ret != 0) {
		mng_err("Failed to get nl_num_per_die. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &udie_version);
	if (ret != 0) {
		mng_err("Failed to get udie_version. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	g_idm_map[devid].chip_type = (enum mami_dev_type)hw.chip_type;
	g_idm_map[devid].die_num_per_chip = (u32)die_num_each_chip;
	g_idm_map[devid].die_num_in_dev = (u32)die_num_in_device;
	g_idm_map[devid].chip_num_in_dev = (u32)chip_num_in_device;
	g_idm_map[devid].nl_num_per_die = (u32)nl_num_per_die;
	g_idm_map[devid].udie_version = (enum mami_udie_version)udie_version;
	return 0;
}

STATIC int idm_kern_get_mask(u32 devid, u32 fdieid, u32 *body_mask, u32 *pcie_mask)
{
	u64 body_mask_ = 0, pcie_mask_ = 0;
	char key[URES_NAME_LEN] = {0};
	int ret;

	(void)UBFM_SPRINTF(key, sizeof(key), MAMI_DIEN_BODY_MASK, fdieid);
	ret = ures_get_key_value(devid, key, &body_mask_);
	if (ret != 0) {
		return ret;
	}

	(void)UBFM_SPRINTF(key, sizeof(key), MAMI_DIEN_PCIE_MASK, fdieid);
	ret = ures_get_key_value(devid, key, &pcie_mask_);
	if (ret != 0) {
		return ret;
	}

	*body_mask = (u32)body_mask_;
	*pcie_mask = (u32)pcie_mask_;
	return 0;
}

static void map_portset_body_v1(u32 devid, u32 die_id, u32 body_mask, u32 pcie_mask)
{
	u32 nl_id;

	for (nl_id = 0; nl_id < g_idm_map[devid].nl_num_per_die; nl_id++) {
		if (!(((body_mask & ~pcie_mask) >> nl_id) & 0x1)) {
			continue;
		}

		g_idm_map[devid].portset_to_body[die_id][nl_id] = (u8)nl_id;
		g_idm_map[devid].body_to_portset[die_id][nl_id] = (u8)nl_id;
	}
}

static void map_portset_body_v2(u32 devid, u32 die_id, u32 body_mask, u32 pcie_mask)
{
	u32 portset = 0, body = 0;

	for (body = 0; body < g_idm_map[devid].nl_num_per_die; body++) {
		if (!((body_mask >> body) & 0x1)) {
			continue;
		}

		if ((pcie_mask >> portset) & 0x1) {
			portset++;
			continue;
		}

		g_idm_map[devid].portset_to_body[die_id][portset] = (u8)body;
		g_idm_map[devid].body_to_portset[die_id][body] = (u8)portset;
		portset++;
	}
}

STATIC int idm_init_portset_body_mapping(u32 devid)
{
	u32 body_mask = 0;
	u32 pcie_mask = 0;
	u32 die_id = 0;
	int ret = 0;

	(void)ubfm_memset(g_idm_map[devid].portset_to_body, sizeof(g_idm_map[devid].portset_to_body), NL_MAX_NUM,
		sizeof(g_idm_map[devid].portset_to_body));
	(void)ubfm_memset(g_idm_map[devid].body_to_portset, sizeof(g_idm_map[devid].body_to_portset), NL_MAX_NUM,
		sizeof(g_idm_map[devid].body_to_portset));

	for (die_id = 0; die_id < g_idm_map[devid].die_num_in_dev; die_id++) {
		ret = idm_kern_get_mask(devid, die_id, &body_mask, &pcie_mask);
		if (ret != 0) {
			mng_err("Failed to get bodymask. (devid=%u; die_id=%u; ret=%d)\n", devid, die_id, ret);
			return ret;
		}

		if (g_idm_map[devid].udie_version == UDIE_VERSION_V1) {
			map_portset_body_v1(devid, die_id, body_mask, pcie_mask);
		} else {
			map_portset_body_v2(devid, die_id, body_mask, pcie_mask);
		}
	}
	return 0;
}

STATIC int idm_kern_init(u32 devid)
{
	u32 i, phyport;
	int ret;

	if (devid >= DEV_MAX_NUM) {
		mng_err("devid is invalid. (devid=%u; max_devid=%u)\n", devid, DEV_MAX_NUM - 1);
		return -EINVAL;
	}

	if (g_idm_map[devid].is_init == 1) {
		return 0;
	}

	mutex_lock(&g_idm_kern_map_mutex);
	if (g_idm_map[devid].is_init == 1) {
		mutex_unlock(&g_idm_kern_map_mutex);
		return 0;
	}
	ret = idm_kern_init_ures_info(devid);
	if (ret != 0) {
		mutex_unlock(&g_idm_kern_map_mutex);
		return ret;
	}

	ret = idm_get_port_info_table(devid);
	if (ret != 0) {
		mutex_unlock(&g_idm_kern_map_mutex);
		mng_err("idm_get_port_info_table failed. (devid=%u; ret=0x%x)\n", devid, (u32)ret);
		return ret;
	}

	for (i = 0; i < DEV_PORT_MAX_NUM; i++) {
		g_idm_map[devid].phyport_to_lgport[i] = DEV_PORT_MAX_NUM;
	}

	for (i = 0; i < g_idm_map[devid].table.port_num; i++) {
		if (g_idm_map[devid].table.entry[i].port_enable) {
			phyport = g_idm_map[devid].table.entry[i].phyport;
			g_idm_map[devid].phyport_to_lgport[phyport] = i;
		}
	}

	ret = idm_init_portset_body_mapping(devid);
	if (ret != 0) {
		mutex_unlock(&g_idm_kern_map_mutex);
		mng_err("idm_init_portset_body_mapping failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	g_idm_map[devid].is_init = 1;
	mutex_unlock(&g_idm_kern_map_mutex);
	mng_info("idm_kern_init success. (devid=%u)\n", devid);
	return 0;
}

static bool idm_kern_is_unions(u32 devid)
{
	int ret;

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return false;
	}

	if ((g_idm_map[devid].chip_type == MAMI_KERN_TYPE_UNIONS) || (g_idm_map[devid].chip_type == MAMI_KERN_TYPE_UNIONS_210)) {
		return true;
	}

	return false;
}

int idm_kern_get_chip_die_num(u32 devid, u32 *die_num_per_chip, u32 *die_num_in_dev, u32 *chip_num_in_dev)
{
	int ret;

	if ((die_num_per_chip == NULL) || (die_num_in_dev == NULL) || (chip_num_in_dev == NULL)) {
		mng_err("Invalid para. (devid=%u;die_num_per_chip_is_null=%d;die_num_in_dev_is_null=%d;chip_num_in_dev_is_null=%d)\n",
				devid, (die_num_per_chip == NULL), (die_num_in_dev == NULL), (chip_num_in_dev == NULL));
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	*die_num_per_chip = g_idm_map[devid].die_num_per_chip;
	*die_num_in_dev = g_idm_map[devid].die_num_in_dev;
	*chip_num_in_dev = g_idm_map[devid].chip_num_in_dev;
	return 0;
}
EXPORT_SYMBOL_GPL(idm_kern_get_chip_die_num);

static int chip_die_id_check(u32 devid, u32 chipid, u32 dieid)
{
	if ((dieid >= g_idm_map[devid].die_num_per_chip) || (chipid >= g_idm_map[devid].chip_num_in_dev)) {
		mng_err("Invalid para. (devid=%u; dieid=%u; dieid_max=%u; chipid=%u; chipid_max=%u)\n",
			devid, dieid, (g_idm_map[devid].die_num_per_chip - 1), chipid,
			(g_idm_map[devid].chip_num_in_dev - 1));
		return -EINVAL;
	}

	return 0;
}

static int fdie_portset_id_check(u32 devid, u32 fdieid, u32 portsetid)
{
	if (fdieid >= g_idm_map[devid].die_num_in_dev || portsetid >= g_idm_map[devid].nl_num_per_die) {
		mng_err("Invalid para. (devid=%u; fdieid=%u; portset=%u; fdieid_max=%u; portset_max=%u)\n",
			devid, fdieid, portsetid, (g_idm_map[devid].die_num_in_dev - 1), (g_idm_map[devid].nl_num_per_die - 1));
		return -EINVAL;
	}

	if (g_idm_map[devid].portset_to_body[fdieid][portsetid] == NL_MAX_NUM) {
		mng_err("The portset cannot be used. (pg or pcie used) (devid=%u; fdieid=%u; portsetid=%u)\n", devid, fdieid, portsetid);
		return -EINVAL;
	}

	return 0;
}

static int port_id_check(u32 devid, u32 chipid, u32 portid)
{
	u32 port_chipid = 0;
	u32 phyport = 0;

	if (portid >= g_idm_map[devid].table.port_num) {
		mng_err("portid is invalid. (devid=%u; chipid=%u; portid=%u; max_portid=%u)\n",
			devid, chipid, portid, g_idm_map[devid].table.port_num - 1);
		return -EINVAL;
	}

	if (g_idm_map[devid].table.entry[portid].port_enable != 1) {
		mng_err("port is not enabled. (devid=%u; chipid=%u; portid=%u)\n", devid, chipid, portid);
		return -EINVAL;
	}

	phyport = g_idm_map[devid].table.entry[portid].phyport;
	port_chipid = phyport / NL_PORT_MAX_NUM / g_idm_map[devid].nl_num_per_die / g_idm_map[devid].die_num_per_chip;
	if (chipid != port_chipid) {
		mng_err("portid and chipid mismatch. (devid=%u; chipid=%u; portid=%u; port_chipid=%u)\n",
			devid, chipid, portid, port_chipid);
		return -EINVAL;
	}

	return 0;
}

static int idm_kern_plane_lg2phy(u32 devid, u32 lgid, struct mami_physical_id_info *physicalIdInfo)
{
	u32 in_chipid, in_dieid;
	int ret;

	if (idm_kern_is_unions(devid)) {
		in_chipid = MAMI_PLANE_GET_LG_U_CHIPID(lgid);
		if (in_chipid != 0) {
			mng_err("Invalid para. (devid=%u; in_chipid=%u)\n", devid, in_chipid);
			return -EINVAL;
		}

		physicalIdInfo->physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_CHIP;
		physicalIdInfo->physicalIdVal = MAMI_P_CHIP(in_chipid);
	} else {
		in_chipid = MAMI_PLANE_GET_LG_AK_CHIPID(lgid);
		in_dieid = MAMI_PLANE_GET_LG_AK_DIEID(lgid);
		ret = chip_die_id_check(devid, in_chipid, in_dieid);
		if (ret != 0) {
			return ret;
		}

		physicalIdInfo->physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_DIE;
		physicalIdInfo->physicalIdVal = (MAMI_P_CHIP(in_chipid) | MAMI_P_DIE(in_dieid));
	}

	return 0;
}

static int idm_kern_portset_lg2phy(u32 devid, u32 lgid, struct mami_physical_id_info *physicalIdInfo)
{
	u32 fdieid = 0;
	u32 in_chipid = MAMI_PORTSET_GET_LG_CHIPID(lgid);
	u32 in_dieid = MAMI_PORTSET_GET_LG_DIEID(lgid);
	u32 in_portsetid = MAMI_PORTSET_GET_LG_PORTSETID(lgid);
	int ret;

	ret = chip_die_id_check(devid, in_chipid, in_dieid);
	if (ret != 0) {
		return ret;
	}

	fdieid = in_chipid * g_idm_map[devid].die_num_per_chip + in_dieid;
	ret = fdie_portset_id_check(devid, fdieid, in_portsetid);
	if (ret != 0) {
		mng_err("portset id check failed. (devid=%u; fdieid=%u; portset=%u; ret=%d)\n", devid, fdieid, in_portsetid, ret);
		return -EINVAL;
	}

	physicalIdInfo->physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_BODY;
	physicalIdInfo->physicalIdVal = (MAMI_P_CHIP(in_chipid) |  MAMI_P_DIE(in_dieid) | MAMI_P_BODY(in_portsetid));
	return 0;
}

static int idm_kern_port_lg2phy(u32 devid, u32 lgid, struct mami_physical_id_info *physicalIdInfo)
{
	u32 in_chipid = MAMI_PORT_GET_LG_CHIPID(lgid);
	u32 in_dieid = MAMI_PORT_GET_LG_DIEID(lgid);
	u32 in_portid = MAMI_PORT_GET_LG_PORTID(lgid);
	int ret;

	if (in_dieid != 0) {
		mng_err("Invalid para. (devid=%u; in_dieid=%u)\n", devid, in_dieid);
		return -EINVAL;
	}

	ret = chip_die_id_check(devid, in_chipid, in_dieid);
	if (ret != 0) {
		mng_err("Chip die id check failed. (devid=%u; chipid=%u; dieid=%u; ret=%d)\n",
					devid, in_chipid, in_dieid, ret);
		return ret;
	}

	ret = port_id_check(devid, in_chipid, in_portid);
	if (ret != 0) {
		mng_err("Port id check failed. (devid=%u; chipid=%u; portid=%u; ret=%d)\n",
					devid, in_chipid, in_portid, ret);
		return ret;
	}

	physicalIdInfo->physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_PORT;
	physicalIdInfo->physicalIdVal = (MAMI_P_CHIP(in_chipid) | MAMI_P_PORT(in_portid));
	return 0;
}

int idm_kern_lgid2phyid(u32 devid, struct mami_logical_id_info lg_id_info, struct mami_physical_id_info *phy_id_info)
{
	int ret;

	if ((phy_id_info == NULL) || (lg_id_info.logicalIdType >= MAMI_KERN_LOGIC_ID_TYPE_MAX)) {
		mng_err("invalid para. (physic_info_is_null=%d; logicalIdType=%u)\n", (phy_id_info == NULL),
			lg_id_info.logicalIdType);
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	switch (lg_id_info.logicalIdType) {
	case MAMI_KERN_LOGIC_ID_TYPE_PLANE:
		return idm_kern_plane_lg2phy(devid, lg_id_info.logicalIdVal, phy_id_info);
	case MAMI_KERN_LOGIC_ID_TYPE_PORT_SET:
		return idm_kern_portset_lg2phy(devid, lg_id_info.logicalIdVal, phy_id_info);
	case MAMI_KERN_LOGIC_ID_TYPE_PORT:
		return idm_kern_port_lg2phy(devid, lg_id_info.logicalIdVal, phy_id_info);
	default:
		mng_err("Operation is not supported. (devid=%u; logicalIdType=%u; logicalIdVal=%u)\n", devid,
			lg_id_info.logicalIdType, lg_id_info.logicalIdVal);
		return E_OPNOTSUPP_ON_DEV;
	}
}
EXPORT_SYMBOL_GPL(idm_kern_lgid2phyid);

static int idm_chip_phy2lg(u32 devid, u32 phyid, struct mami_logical_id_info *logicalIdInfo)
{
	if (phyid != 0) {
		mng_err("Invalid para. (devid=%u; physicalIdVal=%u)\n", devid, phyid);
		return -EINVAL;
	}

	logicalIdInfo->logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PLANE;
	logicalIdInfo->logicalIdVal = MAMI_PLANE_SET_LG_U_ID(0);
	return 0;
}

static int idm_kern_die_phy2lg(u32 devid, u32 phyid, struct mami_logical_id_info *logicalIdInfo)
{
	u32 in_chipid = MAMI_GET_PHY_CHIP_ID(phyid);
	u32 in_dieid = MAMI_GET_PHY_DIE_ID(phyid);
	int ret;

	ret = chip_die_id_check(devid, in_chipid, in_dieid);
	if (ret != 0) {
		return ret;
	}

	logicalIdInfo->logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PLANE;
	logicalIdInfo->logicalIdVal = MAMI_PLANE_SET_LG_AK_ID(in_chipid, in_dieid);
	return 0;
}

static int idm_kern_body_phy2lg(u32 devid, u32 phyid, struct mami_logical_id_info *logicalIdInfo)
{
	u32 fdieid = 0;
	u32 in_chipid = MAMI_GET_PHY_CHIP_ID(phyid);
	u32 in_dieid = MAMI_GET_PHY_DIE_ID(phyid);
	u32 in_bodyid = MAMI_GET_PHY_BODY_ID(phyid);
	int ret;

	ret = chip_die_id_check(devid, in_chipid, in_dieid);
	if (ret != 0) {
		return ret;
	}

	fdieid = in_chipid * g_idm_map[devid].die_num_per_chip + in_dieid;
	ret = fdie_portset_id_check(devid, fdieid, in_bodyid);
	if (ret != 0) {
		mng_err("body id check failed. (devid=%u; fdieid=%u; bodyid=%u; ret=%d)\n", devid, fdieid, in_bodyid, ret);
		return ret;
	}

	logicalIdInfo->logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PORT_SET;
	logicalIdInfo->logicalIdVal = MAMI_PORTSET_SET_LG_ID(in_chipid, in_dieid, in_bodyid);
	return 0;
}

static int idm_kern_port_phy2lg(u32 devid, u32 phyid, struct mami_logical_id_info *logicalIdInfo)
{
	u32 in_chipid = MAMI_GET_PHY_CHIP_ID(phyid);
	u32 in_dieid = MAMI_GET_PHY_DIE_ID(phyid);
	u32 in_bodyid = MAMI_GET_PHY_BODY_ID(phyid);
	u32 in_portid = MAMI_GET_PHY_PORT_ID(phyid);
	int ret;

	if ((in_dieid != 0) || (in_bodyid != 0)) {
		mng_err("Invalid para. (devid=%u; in_dieid=%u; in_bodyid=%u)\n", devid, in_dieid, in_bodyid);
		return -EINVAL;
	}

	ret = chip_die_id_check(devid, in_chipid, in_dieid);
	if (ret != 0) {
		mng_err("Chip die id check failed. (devid=%u; chipd=%u; dieid=%u; ret=%d)\n",
			devid, in_chipid, in_dieid, ret);
		return ret;
	}

	ret = port_id_check(devid, in_chipid, in_portid);
	if (ret != 0) {
		mng_err("Port id check failed. (devid=%u; chipid=%u; portid=%u; ret=%d)\n",
					devid, in_chipid, in_portid, ret);
		return ret;
	}

	logicalIdInfo->logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PORT;
	logicalIdInfo->logicalIdVal = MAMI_PORT_SET_LG_ID(in_chipid, in_dieid, in_portid);
	return 0;
}

int idm_kern_phyid2lgid(u32 devid, struct mami_physical_id_info phy_id_info, struct mami_logical_id_info *lg_id_info)
{
	int ret;

	if ((lg_id_info == NULL) || (phy_id_info.physicalIdType >= MAMI_KERN_PHYSIC_ID_TYPE_MAX)) {
		mng_err("invalid para. (lg_info_is_null=%d; physicalIdType=%u)\n", (lg_id_info == NULL),
			phy_id_info.physicalIdType);
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	switch (phy_id_info.physicalIdType) {
	case MAMI_KERN_PHYSIC_ID_TYPE_CHIP:
		if (!idm_kern_is_unions(devid)) {
			mng_err("Not support on this chipType.(devid=%u)\n", devid);
			return E_OPNOTSUPP_ON_DEV;
		}
		return idm_chip_phy2lg(devid, phy_id_info.physicalIdVal, lg_id_info);
	case MAMI_KERN_PHYSIC_ID_TYPE_DIE:
		if (idm_kern_is_unions(devid)) {
			mng_err("Not support on this chipType.(devid=%u)\n", devid);
			return E_OPNOTSUPP_ON_DEV;
		}
		return idm_kern_die_phy2lg(devid, phy_id_info.physicalIdVal, lg_id_info);
	case MAMI_KERN_PHYSIC_ID_TYPE_BODY:
		return idm_kern_body_phy2lg(devid, phy_id_info.physicalIdVal, lg_id_info);
	case MAMI_KERN_PHYSIC_ID_TYPE_PORT:
		return idm_kern_port_phy2lg(devid, phy_id_info.physicalIdVal, lg_id_info);
	default:
		mng_err("Operation is not supported. (devid=%u; physicalIdType=%u; physicalIdVal=%u)\n", devid,
			phy_id_info.physicalIdType, phy_id_info.physicalIdVal);
		return E_OPNOTSUPP_ON_DEV;
	}
}
EXPORT_SYMBOL_GPL(idm_kern_phyid2lgid);

int idm_kern_get_plane_by_fdie(u32 devid, u32 fdieid, u32 *plane_id)
{
	struct mami_logical_id_info  logic_info = { .logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PLANE };
	struct mami_physical_id_info phy_id_info = {0};
	u32 phy_chipid = 0;
	u32 phy_dieid = 0;
	int ret;

	if (plane_id == NULL) {
		mng_err("invalid para (devid=%u; planeid_is_null=%d).\n", devid, (plane_id == NULL));
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	phy_chipid = fdieid / g_idm_map[devid].die_num_per_chip;
	phy_dieid = fdieid % g_idm_map[devid].die_num_per_chip;

	if (idm_kern_is_unions(devid)) {
		phy_id_info.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_CHIP;
		phy_id_info.physicalIdVal = MAMI_P_CHIP(phy_chipid);
	} else {
		phy_id_info.physicalIdType = MAMI_KERN_PHYSIC_ID_TYPE_DIE;
		phy_id_info.physicalIdVal = MAMI_P_CHIP(phy_chipid) | MAMI_P_DIE(phy_dieid);
	}
	ret = idm_kern_phyid2lgid(devid, phy_id_info, &logic_info);
	if (ret != 0) {
		return ret;
	}

	*plane_id = logic_info.logicalIdVal;
	return 0;
}
EXPORT_SYMBOL_GPL(idm_kern_get_plane_by_fdie);

int idm_kern_get_fbody_bport_by_lgport(u32 devid, u32 lgport, u32 *fbodyid, u32 *bportid)
{
	struct mami_physical_id_info phy_id_info = {0};
	u32 phy_portid = 0;
	int ret;

	if ((fbodyid == NULL) || (bportid == NULL)) {
		mng_err("invalid para. (devid=%u; fbodyid_is_null=%d; bportid_is_null=%d)\n",
			devid, (fbodyid == NULL), (bportid == NULL));
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	ret = idm_kern_port_lg2phy(devid, lgport, &phy_id_info);
	if (ret != 0) {
		mng_err("idm_kern_port_lg2phy failed. (devid=%u; lgport=%u; ret=%d)\n", devid, lgport, ret);
		return ret;
	}

	phy_portid = g_idm_map[devid].table.entry[MAMI_PORT_GET_LG_PORTID(lgport)].phyport;
	*fbodyid = phy_portid / NL_PORT_MAX_NUM;
	*bportid = phy_portid % NL_PORT_MAX_NUM;
	return 0;
}
EXPORT_SYMBOL_GPL(idm_kern_get_fbody_bport_by_lgport);

int idm_kern_get_lgport_by_fbody_bport(u32 devid, u32 fbodyid, u32 bportid, u32 *lgport)
{
	u32 phy_portid = DEV_PORT_MAX_NUM;
	u32 chipid;
	int ret;

	if (lgport == NULL) {
		mng_err("lgport is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	if (fbodyid >= g_idm_map[devid].die_num_in_dev * g_idm_map[devid].nl_num_per_die || bportid >= NL_PORT_MAX_NUM) {
		mng_err("invalid params. (devid=%u; fbodyid=%u; bportid=%u)\n", devid, fbodyid, bportid);
		return -EINVAL;
	}

	phy_portid = fbodyid * NL_PORT_MAX_NUM + bportid;
	if (phy_portid >= DEV_PORT_MAX_NUM || g_idm_map[devid].phyport_to_lgport[phy_portid] == DEV_PORT_MAX_NUM) {
		mng_err("port is not enabled. (devid=%u; fbodyid=%u; bportid=%u)\n", devid, fbodyid, bportid);
		return -EINVAL;
	}

	chipid = fbodyid / g_idm_map[devid].nl_num_per_die / g_idm_map[devid].die_num_per_chip;
	*lgport = MAMI_PORT_SET_LG_ID(chipid, 0, g_idm_map[devid].phyport_to_lgport[phy_portid]);
	return 0;
}
EXPORT_SYMBOL_GPL(idm_kern_get_lgport_by_fbody_bport);

int idm_kern_get_inport_by_chip_die_port(u32 devid, u32 chipid, u32 dieid, u32 portid, u32 *in_port)
{
	u32 fdieid, phyport, phyport_indie;
	int ret;

	if (in_port == NULL) {
		mng_err("in_port is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	ret = chip_die_id_check(devid, chipid, dieid);
	if (ret != 0) {
		return ret;
	}
	fdieid = chipid * g_idm_map[devid].die_num_per_chip + dieid;
	if (fdieid >= g_idm_map[devid].die_num_in_dev) {
		mng_err("Invalid fdieid. (devid=%u; fdieid=%u; chipid=%u; dieid=%u)\n", devid, fdieid, chipid, dieid);
		return -EINVAL;
	}

	phyport_indie = portid;
	if (g_idm_map[devid].udie_version == UDIE_VERSION_V1) {
		phyport_indie *= NL_PORT_NUM_X4;
	}

	if (phyport_indie >= g_idm_map[devid].nl_num_per_die * NL_PORT_MAX_NUM) {
		mng_err("Invalid portid. (devid=%u; fdieid=%u; chipid=%u; portid=%u)\n", devid, fdieid, chipid, portid);
		return -EINVAL;
	}

	phyport = fdieid * g_idm_map[devid].nl_num_per_die * NL_PORT_MAX_NUM + phyport_indie;
	if (phyport >= DEV_PORT_MAX_NUM || g_idm_map[devid].phyport_to_lgport[phyport] == DEV_PORT_MAX_NUM) {
		mng_err("Invalid portid. (devid=%u; fdieid=%u; chipid=%u; portid=%u)\n", devid, fdieid, chipid, portid);
		return -EINVAL;
	}

	*in_port = g_idm_map[devid].phyport_to_lgport[phyport];
	return 0;
}
EXPORT_SYMBOL_GPL(idm_kern_get_inport_by_chip_die_port);

int idm_kern_get_portset_by_chip_die_body(u32 devid, u32 chipid, u32 dieid, u32 body, u32 *portset)
{
	u32 fdieid;
	int ret;

	if (portset == NULL) {
		mng_err("portset is NULL. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = idm_kern_init(devid);
	if (ret != 0) {
		return ret;
	}

	ret = chip_die_id_check(devid, chipid, dieid);
	if (ret != 0) {
		return ret;
	}

	fdieid = chipid * g_idm_map[devid].die_num_per_chip + dieid;
	if (fdieid >= g_idm_map[devid].die_num_in_dev) {
		mng_err("Invalid fdieid. (devid=%u; fdieid=%u; chipid=%u; dieid=%u)\n", devid, fdieid, chipid, dieid);
		return -EINVAL;
	}

	if (body >= g_idm_map[devid].nl_num_per_die || g_idm_map[devid].body_to_portset[fdieid][body] == NL_MAX_NUM) {
		return -EINVAL;
	}

	*portset = g_idm_map[devid].body_to_portset[fdieid][body];
	return 0;
}

STATIC u32 calc_valid_body_mask(u32 body_mask, u32 pcie_mask, u32 nl_num_per_die, enum mami_udie_version udie_version)
{
	u32 not_pcie_mask = (~pcie_mask);   // invert, uniformly use 1 to indicate body is available
	u32 valid_body_mask = 0;
	u32 i = 0;
	u32 j = 0;

	if (udie_version == UDIE_VERSION_V1) {
		/* Ascend_v1 pcie_mask and body_mask correspond bit by bit, clear the bits in body_mask that correspond to pcie_mask */
		valid_body_mask = (body_mask & not_pcie_mask);
	} else {
		for (i = 0; i < nl_num_per_die; ++i) {
			if (!((body_mask >> i) & 0x1)) {
				continue;
			}
			/* when body[i] is available, valid_body_mask[bit i] depends on pciemask[bit j] */
			valid_body_mask |= ((not_pcie_mask >> j) & 0x1) << i;
			++j;	/* pcie body only corresponds to available body, skip PG body */
		}
	}
	return valid_body_mask;
}

bool idm_kern_is_nl_valid(u32 devid, u32 fdieid, u32 dbodyid)
{
	u64 udie_version = 0, nl_num_per_die = 0, valid_mask = 0;
	u32 body_mask = 0, pcie_mask = 0, valid_body_mask = 0;
	char valid_body_mask_key[URES_NAME_LEN] = {0};
	int ret;

	ret = ures_get_key_value(devid, MAMI_NL_NUM, &nl_num_per_die);
	if (ret != 0 || nl_num_per_die == 0) {
		mng_err("Failed to get nl_num_per_die. (devid=%u; ret=%d)\n", devid, ret);
		return false;
	}

	if (dbodyid >= (u32)nl_num_per_die) {
		mng_err("Invalid dbodyid. (devid=%u; dbodyid=%u; nl_num_per_die=%u)\n", devid, dbodyid, (u32)nl_num_per_die);
		return false;
	}

	(void)UBFM_SPRINTF(valid_body_mask_key, sizeof(valid_body_mask_key), MAMI_VALID_BODY_MASK, fdieid);
	ret = ures_get_key_value(devid, valid_body_mask_key, &valid_mask);
	if (ret == 0) {
		return ((valid_mask >> dbodyid) & 0x1) != 0;
	}

	ret = ures_get_key_value(devid, MAMI_UDIE_VER, &udie_version);
	if (ret != 0 || udie_version >= UDIE_VERSION_MAX) {
		mng_err("Failed to get udie_version. (devid=%u; ret=%d)\n", devid, ret);
		return false;
	}

	ret = idm_kern_get_mask(devid, fdieid, &body_mask, &pcie_mask);
	if (ret != 0) {
		mng_err("get bodymask failed. (devid=%u; fdieid=%u; ret=%d)\n", devid, fdieid, ret);
		return false;
	}

	valid_body_mask = calc_valid_body_mask(body_mask, pcie_mask, (u32)nl_num_per_die, (enum mami_udie_version)udie_version);
	ret = ures_set_key_value(devid, valid_body_mask_key, (u64)valid_body_mask);
	if (ret != 0) {
		mng_err("set valid_body_mask failed. (devid=%u; fdieid=%u; ret=%d)\n", devid, fdieid, ret);
	} else {
		mng_info("set valid_body_mask success. (devid=%u; fdieid=%u; valid_body_mask=0x%x)\n", devid, fdieid, valid_body_mask);
	}

	return ((valid_body_mask >> dbodyid) & 0x1) != 0;
}
EXPORT_SYMBOL_GPL(idm_kern_is_nl_valid);
