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

#include "mami_string.h"

#include <linux/slab.h>
#include "ubfm_common.h"
#include "ures.h"
#include "uop.h"
#include "ubfabric.h"
#include "feature_kern.h"
#include "port_pub.h"
#include "pa_tbl_cfg.h"
#include "ppp_tbl_cfg.h"
#include "port_info.h"
#include "idm.h"

#define UB_DLNLA_GLB_OFFSET  0x200000
#define NL_CFG_PORT_OFFSET  0x1000
#define ETH_MAC_COMMON_OFFSET 0x100000
#define ETH_MAC_PORT_LINK_MODE_OFFSET 0xc
#define ETH_MAC_PORT_LINK_MODE_X4 2U
#define ETH_MAC_PORT_LINK_MODE_X2 1U

#define GET_DST_NL_ID(entry) (((entry) >> 3) & 0x1F)
#define GET_DST_PORT_ID(entry) ((entry) & 0x7)
#define DL_CFG_MAC_PORT_MODE(dlnla_base, port_id) ((dlnla_base) + 0x8 + 0x400 * (port_id))
#define NL_LINK_MODE(nl_cfg_port_base, port_id) ((nl_cfg_port_base) + 0x200 * (port_id) + 0x010)

static void port_info_show(u32 devid, struct port_info_table *table)
{
	u32 index = 0;

	udev_port_info("Device port info. (devid=%u; num=%u)\n", devid, table->port_num);
	for (index = 0; index < table->port_num; index++) {
		udev_port_info("Port id info. (index=%u; phyport=%u; dlphy_port_id=%u, enable=%u; link_mode=%u; lane_mode=%u; "
			"die0_rxpa_tgtid_index=%u; die0_tx_tpp_tgtid_index=%u)\n", index, table->entry[index].phyport,
			table->entry[index].dlphy_port_id, table->entry[index].port_enable, table->entry[index].link_mode, table->entry[index].lane_mode,
			table->entry[index].rxpa_tgtid_index[0], table->entry[index].tx_ppp_tgtid_index[0]);
	}
}

static int port_info_read_rx_target_id_table(u32 devid, u32 die_id, u32 nl_id, u8 *data, u32 num)
{
	int ret;
	u32 i, val;
	struct nl_id_inst id_inst = {.devid = devid, .fdieid = die_id, .dbodyid = nl_id};

	for (i = 0; i < num; i++) {
		ret = pa_table_read(&id_inst, ROUTE_TABLE_CFG_TARGET_ID, i, (u8 *)&val, sizeof(val));
		if (ret != 0) {
			return ret;
		}

		data[i] = (u8)val;
	}

	return 0;
}

static int port_info_read_tx_target_id_table(u32 devid, u32 die_id, u8 *data, u32 num)
{
	int ret;
	u32 i, val;
	struct ures_inst inst = {.devid = devid, .die_id = die_id};

	for (i = 0; i < num; i++) {
		ret = ppp_table_read(&inst, ROUTE_TABLE_CFG_TARGET_ID, i, (u8 *)&val, sizeof(val));
		if (ret != 0) {
			return ret;
		}

		data[i] = (u8)val;
	}

	return 0;
}

static u64 port_info_get_ub_dlnla_glb_addr(u32 devid, u32 die_id, u32 nl_id)
{
	u64 addr = 0;

	addr = port_info_get_nl_base_addr(devid, die_id, nl_id);
	if (addr == 0) {
		return 0;
	}

	return (addr + UB_DLNLA_GLB_OFFSET);
}

static u64 port_info_get_nl_cfg_port_addr(u32 devid, u32 die_id, u32 nl_id)
{
	u64 addr = 0;

	addr = port_info_get_nl_base_addr(devid, die_id, nl_id);
	if (addr == 0) {
		return 0;
	}

	return (addr + NL_CFG_PORT_OFFSET);
}

static int port_info_get_port_link_mode(u32 devid, u32 die_id, u32 nl_id, u32 portid, enum port_link_mode *link_mode)
{
	u32 value;
	u64 nl_cfg_port_base = 0;
	int ret;

	nl_cfg_port_base = port_info_get_nl_cfg_port_addr(devid, die_id, nl_id);
	if (nl_cfg_port_base == 0) {
		return -EINVAL;
	}

	ret = uop_kern_readl(devid, NL_LINK_MODE(nl_cfg_port_base, portid), &value);
	if (ret != 0) {
		udev_port_err("Read link mode failed. (devid=%u; die_id=%u; nl_id=%u; port_id=%u; ret=%d)\n",
			devid, die_id, nl_id, portid, ret);
		return ret;
	}

	udev_port_debug("devid=%u; die_id=%u; nl_id=%u; port_id=%u; link_mode=0x%x\n",
		devid, die_id, nl_id, portid, value);

	if (value != ETH_MODE && value != UB_MODE && value != PCIE_MODE && value != DISABLE_MODE) { /* under kunpeng960 x4 mode, ports 1 and 3 are configured as 0 by imp; previously the default config was UB_MODE */
		udev_port_err("Port link mode is invalid. (devid=%u; die_id=%u; nl_id=%u; port_id=%u)\n",
			devid, die_id, nl_id, portid);
		return -EINVAL;
	}

	*link_mode = value;
	return 0;
}

static int port_info_get_ub_port_lane_mode(u32 devid, u32 die_id, u32 nl_id, struct port_info_entry *entry)
{
	u64 ub_dlnla_glb_addr = 0;
	u32 value;
	int ret;

	ub_dlnla_glb_addr = port_info_get_ub_dlnla_glb_addr(devid, die_id, nl_id);
	if (ub_dlnla_glb_addr == 0) {
		return -EINVAL;
	}

	ret = uop_kern_readl(devid, DL_CFG_MAC_PORT_MODE(ub_dlnla_glb_addr, entry->dlphy_port_id), &value);
	if (ret != 0) {
		udev_port_err("Read ub port mode failed. (devid=%u; die_id=%u; nl_id=%u; port_id=%u; ret=%d)\n",
			devid, die_id, nl_id, entry->dlphy_port_id, ret);
		return ret;
	}

	entry->lane_mode = (value & 0x3);
	entry->port_enable = ((value >> 2) & 0x1);  /* 2 is bit */

	udev_port_debug("devid=%u; die_id=%u; nl_id=%u; port_id=%u; value=0x%x; port_enable=%u; lane_mode=0x%x\n",
		devid, die_id, nl_id, entry->dlphy_port_id, value, entry->port_enable, entry->lane_mode);

	return 0;
}

/* only used for Ascend_v2, Ascend_v1 also has this register but imp does not configure it */
static int port_info_get_uboe_port_lane_mode(u32 devid, u32 die_id, u32 nl_id, enum port_lane_mode *mode)
{
	int ret;
	u32 value = 0;
	u64 addr, nl_base;

	nl_base = port_info_get_nl_base_addr(devid, die_id, nl_id);
	if (nl_base == 0) {
		return -EFAULT;
	}

	addr = nl_base + ETH_MAC_COMMON_OFFSET + ETH_MAC_PORT_LINK_MODE_OFFSET;
	ret = uop_kern_readl(devid, addr, &value);
	if (ret != 0) {
		udev_port_err("Read uboe port mode failed. (devid=%u; die_id=%u; nl_id=%u; ret=%d)\n",
			devid, die_id, nl_id, ret);
		return ret;
	}

	udev_port_info("devid=%u; die_id=%u; nl_id=%u; eth_mac_port_link_mode=0x%x\n", devid, die_id, nl_id, value);

	if ((value & 0xF) == ETH_MAC_PORT_LINK_MODE_X4) {
		*mode = PORT_MODE_X4;
		return 0;
	} else if ((value & 0xF) == ETH_MAC_PORT_LINK_MODE_X2) {
		*mode = PORT_MODE_X2;
		return 0;
	} else {
		return -EINVAL;
	}
}

STATIC int port_info_init_link_mode(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	int ret;
	u32 i, die_id, nl_id, nl_port_id, phyport;

	for (i = 0; i < table->port_num; i++) {
		phyport = table->entry[i].phyport;
		if (phyport >= DEV_PORT_MAX_NUM) {
			continue;
		}

		die_id = phyport / (nl_num * NL_PORT_MAX_NUM);
		nl_id = phyport / NL_PORT_MAX_NUM % nl_num;
		nl_port_id = phyport % NL_PORT_MAX_NUM;
		ret = port_info_get_port_link_mode(devid, die_id, nl_id, nl_port_id, &table->entry[i].link_mode);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

STATIC int port_info_init_ub_lane_mode(u32 devid, u32 die_num, u32 die_nl_num, struct port_info_table *table)
{
	int ret;
	u32 i, fdie_id, nl_id;

	for (i = 0; i < table->port_num; i++) {
		if (table->entry[i].port_enable != 1) {
			continue;
		}

		if (table->entry[i].link_mode == ETH_MODE) {
			continue;
		}

		if (table->entry[i].link_mode != UB_MODE) {
			table->entry[i].port_enable = 0;
			continue;
		}

		fdie_id = table->entry[i].phyport / (die_nl_num * NL_PORT_MAX_NUM);
		nl_id = table->entry[i].phyport / NL_PORT_MAX_NUM % die_nl_num;
		ret = port_info_get_ub_port_lane_mode(devid, fdie_id, nl_id, &table->entry[i]);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static void port_info_init_tgtid_index(u32 die_id, u32 is_rx, u8 *data, u32 num, struct port_info_table *table)
{
	u32 i, dst_nl_id, dst_port_id, phyport;
	u8 phyport_to_index[DEV_PORT_MAX_NUM];
	(void)ubfm_memset(phyport_to_index, sizeof(phyport_to_index), DEV_PORT_MAX_NUM, sizeof(phyport_to_index));

	for (i = 0; i < num && i < table->port_num; i++) {
		dst_nl_id = GET_DST_NL_ID(data[i]);
		dst_port_id = GET_DST_PORT_ID(data[i]);
		phyport = dst_nl_id * NL_PORT_MAX_NUM + dst_port_id;
		if (phyport >= DEV_PORT_MAX_NUM) {
			continue;
		}
		phyport_to_index[phyport] = (u8)i;
		if (is_rx == 1) {
			table->rxpa_tgtid_table[die_id][i] = phyport;
		} else {
			table->tx_tpp_tgtid_table[die_id][i] = phyport;
		}
	}

	for (i = 0; i < table->port_num; i++) {
		phyport = table->entry[i].phyport;
		if (phyport >= DEV_PORT_MAX_NUM || phyport_to_index[phyport] == DEV_PORT_MAX_NUM) {
			continue;
		}

		if (is_rx == 1) {
			table->entry[i].rxpa_tgtid_index[die_id] = phyport_to_index[phyport];
		} else {
			table->entry[i].tx_ppp_tgtid_index[die_id] = phyport_to_index[phyport];
		}
		table->entry[i].port_enable = 1; /* only indicates that the port exists in the target id table */
	}
}

static int port_info_init_rx_tgtid_table(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	int ret;
	u32 die_id, nl_id, entry_num;
	enum ndie_p2p_port_mode port_mode = 0;
	u8 data[DEV_PORT_MAX_NUM] = {0};

	for (die_id = 0; die_id < die_num; die_id++) {
		for (nl_id = 0; nl_id < nl_num; nl_id++) {
			if (!idm_kern_is_nl_valid(devid, die_id, nl_id)) {
				continue;
			}

			if (feature_kern_is_ndie(devid)) {
				ret = get_ndie_p2p_port_mode(devid, die_id, nl_id, &port_mode);
				if (ret != 0) {
					return ret;
				}
				table->port_mode = port_mode;
			}

			ret = feature_kern_get_rx_tgtid_table_spec(devid, port_mode, &entry_num);
			if (ret != 0) {
				return ret;
			}

			ret = port_info_read_rx_target_id_table(devid, die_id, nl_id, data, entry_num);
			if (ret != 0) {
				return ret;
			}

			port_info_init_tgtid_index(die_id, 1, data, entry_num, table);
			break;
		}
	}
	return 0;
}

static int port_info_init_tx_tgtid_table(u32 devid, u32 die_num, struct port_info_table *table)
{
	int ret;
	u32 die_id, entry_num;
	u8 data[DEV_PORT_MAX_NUM] = {0};

	ret = feature_kern_get_tx_tgtid_table_spec(devid, &entry_num);
	if (ret != 0) {
		return ret;
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		ret = port_info_read_tx_target_id_table(devid, die_id, data, entry_num);
		if (ret != 0) {
			return ret;
		}

		port_info_init_tgtid_index(die_id, 0, data, entry_num, table);
	}

	return 0;
}

static int port_info_init_uboe_lane_mode_v1(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	u32 i, die_id, port_id;
	u32 port_num[DEV_DIE_MAX_NUM] = {0};
	u32 uboe_port_index[DEV_DIE_MAX_NUM][PORT_MAX_NUM] = {0};
	enum port_lane_mode lane_mode;

	for (i = 0; i < table->port_num; i++) {
		if (table->entry[i].port_enable == 1 && table->entry[i].link_mode == ETH_MODE) {
			die_id = table->entry[i].phyport / (nl_num * NL_PORT_MAX_NUM);
			uboe_port_index[die_id][port_num[die_id]] = i;
			port_num[die_id]++;
		}
	}

	for (die_id = 0; die_id < die_num; die_id++) {
		lane_mode = (port_num[die_id] == 1) ? PORT_MODE_X4 : PORT_MODE_X2;
		for (i = 0; i < port_num[die_id]; i++) {
			port_id = uboe_port_index[die_id][i];
			table->entry[port_id].port_enable = 1;
			table->entry[port_id].lane_mode = lane_mode;
			if (lane_mode == PORT_MODE_X4) {
				break;
			}
		}
	}

	return 0;
}

static int port_info_init_uboe_lane_mode_v2(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	int ret;
	u32 i, die_id, nl_id;
	enum port_lane_mode mode;

	for (i = 0; i < table->port_num; i++) {
		if (table->entry[i].link_mode != ETH_MODE) {
			continue;
		}

		die_id = table->entry[i].phyport / (nl_num * NL_PORT_MAX_NUM);
		nl_id = table->entry[i].phyport / NL_PORT_MAX_NUM % nl_num;
		ret = port_info_get_uboe_port_lane_mode(devid, die_id, nl_id, &mode);
		if (ret != 0) {
			return ret;
		}

		table->entry[i].port_enable = 1;
		table->entry[i].lane_mode = mode;

		/* two uboe ports are adjacent, set the next port */
		i++;
		table->entry[i].lane_mode = mode;
		if (mode == PORT_MODE_X2) {
			table->entry[i].port_enable = 1;
		} else {
			table->entry[i].port_enable = 0;
		}
	}

	return 0;
}

static int port_info_init_uboe_lane_mode(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	int ret;
	enum mami_udie_version udie_version;

	ret = feature_kern_get_udie_version(devid, &udie_version);
	if (ret != 0) {
		return ret;
	}

	if (udie_version == UDIE_VERSION_V1) {
		return port_info_init_uboe_lane_mode_v1(devid, die_num, nl_num, table);
	} else {
		return port_info_init_uboe_lane_mode_v2(devid, die_num, nl_num, table);
	}
}

int port_info_init_port_info_table(u32 devid, u32 die_num, u32 nl_num, struct port_info_table *table)
{
	int ret;

	ret = port_info_init_rx_tgtid_table(devid, die_num, nl_num, table);
	if (ret != 0) {
		return ret;
	}

	if (!feature_kern_is_dpu(devid)) {
		ret = port_info_init_tx_tgtid_table(devid, die_num, table);
		if (ret != 0) {
			return ret;
		}
	}

	ret = port_info_init_link_mode(devid, die_num, nl_num, table);
	if (ret != 0) {
		return ret;
	}

	ret = port_info_init_ub_lane_mode(devid, die_num, nl_num, table);
	if (ret != 0) {
		return ret;
	}

	ret = port_info_init_uboe_lane_mode(devid, die_num, nl_num, table);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int port_info_get_die_info(u32 devid, u32 *die_num, u32 *nl_num)
{
	u64 value;
	int ret;

	ret = ures_get_key_value(devid, MAMI_DIE_NUM, &value);
	if (ret != 0) {
		udev_port_err("Get die num failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	*die_num = (u32)value;

	ret = ures_get_key_value(devid, MAMI_NL_NUM, &value);
	if (ret != 0) {
		udev_port_err("Get nl num failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}
	*nl_num = (u32)value;

	if ((*die_num == 0) || (*die_num > DIE_MAX_NUM) || (*nl_num == 0) || (*nl_num > NL_MAX_NUM)) {
		udev_port_err("Invalid para. (devid=%u; die_num=%u; nl_num=%u)\n", devid, *die_num, *nl_num);
		return -EINVAL;
	}

	udev_port_debug("Device info. (devid=%u; die_num=%u; nl_num=%u)\n", devid, *die_num, *nl_num);

	return 0;
}

static int port_info_pre_init_port_info_table(u32 devid, struct port_info_table *table)
{
	int ret;
	u32 i = 0, max_port_num = DEV_PORT_MAX_NUM;
	u32 lgport_to_phyport[DEV_PORT_MAX_NUM];

	ret = feature_kern_get_port_mapping(devid, lgport_to_phyport, &max_port_num);
	if (ret != 0) {
		udev_port_err("Get port mapping failed. (devid=%u; ret=%d)\n", devid, ret);
		return ret;
	}

	table->port_num = max_port_num;
	for (i = 0; i < max_port_num; i++) {
		table->entry[i].phyport = lgport_to_phyport[i];
		table->entry[i].dlphy_port_id = (feature_kern_is_ssd(devid)) ? i : (lgport_to_phyport[i] % NL_PORT_MAX_NUM);
		(void)ubfm_memset(table->entry[i].tx_ppp_tgtid_index, DEV_DIE_MAX_NUM, DEV_PORT_MAX_NUM, DEV_DIE_MAX_NUM);
		(void)ubfm_memset(table->entry[i].rxpa_tgtid_index, DEV_DIE_MAX_NUM, DEV_PORT_MAX_NUM, DEV_DIE_MAX_NUM);
	}
	(void)ubfm_memset(table->tx_tpp_tgtid_table, sizeof(table->tx_tpp_tgtid_table), DEV_PORT_MAX_NUM, sizeof(table->tx_tpp_tgtid_table));
	(void)ubfm_memset(table->rxpa_tgtid_table, sizeof(table->rxpa_tgtid_table), DEV_PORT_MAX_NUM, sizeof(table->rxpa_tgtid_table));
	return 0;
}

int port_info_init(u32 devid)
{
	u32 die_num, nl_num;
	int ret;
	struct port_info_table *table = NULL;

	ret = port_info_get_die_info(devid, &die_num, &nl_num);
	if (ret != 0) {
		return ret;
	}

	table = (struct port_info_table *)kzalloc(sizeof(struct port_info_table), GFP_KERNEL);
	if (table == NULL) {
		return -ENOMEM;
	}

	ret = port_info_pre_init_port_info_table(devid, table);
	if (ret != 0) {
		kfree(table);
		return ret;
	}

	ret = port_info_init_port_info_table(devid, die_num, nl_num, table);
	if (ret != 0) {
		kfree(table);
		return ret;
	}

	port_info_show(devid, table);

	ret = ures_dev_set_attr(devid, MAMI_PORT_INFO_TBL, table, sizeof(struct port_info_table));
	if (ret != 0) {
		udev_port_err("Set port info table attr failed. (devid=%u; ret=%d)\n", devid, ret);
	}
	kfree(table);

	return ret;
}
