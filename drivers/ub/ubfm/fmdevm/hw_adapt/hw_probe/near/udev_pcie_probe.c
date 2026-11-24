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
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/delay.h>

#include "fmdevm.h"
#include "ubfm_common.h"
#include "fmdevm.h"
#include "feature_loader.h"
#include "udm.h"
#include "ures.h"
#include "udev_hw_pub.h"
#include "udev_ctrl.h"
#include "udev_pcie_pub.h"
#include "udev_ucmi_ops.h"
#include "mng_devid.h"
#include "fpga_adapter_kern.h"
#include "fmdevm.h"
#include "i3c_kern.h"

#define UDEV_HW_INFO_REG_OFFSET_KUNPENG950 0x30200FF000ULL
/* used by the kunpeng960 version */
#define UDEV_HW_INFO_REG_OFFSET_KUNPENG960 0x9D30000ULL
#define UDEV_HW_INFO_REG_OFFSET_ASCEND_V1_UNIONS 0x200200FF000ULL

/* Must be a power of 2. */
#define MAX_UCMI_NUM_PER_FPGA 32U

enum pcieChipType {
	PCIE_CHIP_TYPE_UNIONS = 2,
	PCIE_CHIP_TYPE_ASCEND_V1 = 3,
	PCIE_CHIP_TYPE_KUNPENG950_1P = 4,
	PCIE_CHIP_TYPE_KUNPENG950_2P = 5,
	PCIE_CHIP_TYPE_KUNPENG950_4P = 6,
	PCIE_CHIP_TYPE_UNIONS_210 = 7,
	PCIE_CHIP_TYPE_ASCEND_V2 = 8,
	PCIE_CHIP_TYPE_KUNPENG960_1P = 9,
	PCIE_CHIP_TYPE_KUNPENG960_2P = 10,
	PCIE_CHIP_TYPE_KUNPENG960_4P = 11,
	PCIE_CHIP_TYPE_1825 = 12,
	PCIE_CHIP_TYPE_MAX
};

struct udev_pcie_chips {
	u32 chip_num;
	struct udev_pcie *udev_chip;
};
static struct udev_pcie_chips g_pcie_chips = {0};
static DEFINE_MUTEX(g_pcie_mutex);

static int udev_config_ucmi_win(struct udev_pcie *pcie)
{
	int ret;
	struct udev_ucmi_addr addr = {
		.name = UCMI_WIN_NAME,
		.devid = pcie->devid,
		.dieid = 0,
		.chip_addr = pcie->ucmi_reg_base,
		.size = SIZE_KB(64)
	};

	ucmi_remove_win(UDEV_PCIE_ESPI, &addr); /* ensure the register can be configured to hardware (instead of being intercepted by the software table) */
	ret = ucmi_assign_win_by_axi(UDEV_PCIE_ESPI, 0, &addr);
	if (ret != 0) {
		if (ret != -ENODEV) {
			hw_err("Assign win failed. (ret=%d; devid=%u; name=%s)\n", ret, addr.devid, addr.name);
		}

		return ret;
	}

	return 0;
}

static int udev_pcie_get_hw_info(struct udev_pcie *pcie)
{
	u64 resv_mem_base;
	int ret;
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct udev_ucmi_win *ucmi_win = NULL;
	struct udev_ucmi_addr addr = {
		.name = DEV_INFO_WIN_NAME,
		.devid = pcie->devid,
		.dieid = 0,
		.size = SIZE_KB(4)
	};

	if (pcie->chip_type == MAMI_KERN_TYPE_KUNPENG950_1P || pcie->chip_type == MAMI_KERN_TYPE_KUNPENG950_2P ||
		pcie->chip_type == MAMI_KERN_TYPE_KUNPENG950_4P) {
		addr.chip_addr = UDEV_HW_INFO_REG_OFFSET_KUNPENG950;
	} else if (pcie->chip_type >= MAMI_KERN_TYPE_KUNPENG960_1P && pcie->chip_type <= MAMI_KERN_TYPE_KUNPENG960_4P) {
		addr.chip_addr = UDEV_HW_INFO_REG_OFFSET_KUNPENG960;
	} else {
		addr.chip_addr = UDEV_HW_INFO_REG_OFFSET_ASCEND_V1_UNIONS;
	}

	ucmi_remove_win(UDEV_PCIE_ESPI, &addr);
	ret = ucmi_assign_win(UDEV_PCIE_ESPI, &addr);
	if (ret != 0) {
		hw_err("Assign axi window failed. (ret=%d; devid=%u)\n", ret, addr.devid);
		return ret;
	}
	addr_info = ucmi_get_addr_info(UDEV_PCIE_ESPI, addr.devid, addr.chip_addr, addr.name);
	if (addr_info == NULL) {
		hw_err("Get addr info failed. (devid=%u)\n", addr.devid);
		return -EINVAL;
	}
	if (list_empty(&addr_info->win_list)) {
		hw_err("win list is empty. (devid=%u)\n", addr.devid);
		ucmi_put_addr_info(UDEV_PCIE_ESPI, addr.devid);
		return -EINVAL;
	}
	ucmi_win = list_first_entry(&addr_info->win_list, struct udev_ucmi_win, entry);
	resv_mem_base = ucmi_win->ucmi_base + ucmi_win->ucmi_off; /* access to ucmi and the init table, uses the first bar by default */
	ucmi_put_addr_info(UDEV_PCIE_ESPI, addr.devid);
	ret = udev_get_hw_info(addr.devid, resv_mem_base, addr.size, &pcie->hw);
	if (ret != 0) {
		hw_err("Get hw info failed. (ret=%d)\n", ret);
		return ret;
	}
	if (pcie->hw.chip_type >= MAMI_KERN_TYPE_MAX || pcie->hw.chip_type <= MAMI_KERN_TYPE_INVALID) {
		hw_err("Chip type is invalid. (chip_type=%u; expected_range=(%u, %u))\n",
			pcie->hw.chip_type, MAMI_KERN_TYPE_INVALID, MAMI_KERN_TYPE_MAX);
		return -EINVAL;
	}

	return 0;
}

static int udev_pcie_update_hw_info(struct udev_pcie *pcie)
{
	u32 slot_id = 0, socket_id = 0;
	int ret = 0;

	ret = mng_get_slot_id_socket_id(pcie->devid, &slot_id, &socket_id);
	if (ret != 0) {
		return ret;
	}
	pcie->hw.slot_id = slot_id;
	pcie->hw.socket_id = socket_id;
	pcie->hw.access_type = UDEV_PCIE_LDST;
	pcie->hw.location = UDM_NEAR;
	pcie->hw.connect_type = UDEV_PCIE_ESPI;

	return 0;
}

STATIC int udev_pcie_hw_info_init(struct udev_pcie *pcie)
{
	int ret;

	ret = udev_pcie_get_hw_info(pcie);
	if (ret != 0) {
		return ret;
	}

	/* in the out-of-band EVB kunpeng950-2p scenario, the chip_type provided by starry is incorrect. uniformly use the chip_type from the bios init table */
	/* slot_id and socket_id use the values obtained from the starry interface */
	ret = udev_pcie_update_hw_info(pcie);
	if (ret != 0) {
		return ret;
	}

	return ret;
}

static u64 get_chip_reg_base(u32 chip_type)
{
	u64 reg_base = 0;

	if (chip_type == MAMI_KERN_TYPE_KUNPENG950_1P || chip_type == MAMI_KERN_TYPE_KUNPENG950_2P || chip_type == MAMI_KERN_TYPE_KUNPENG950_4P ||
		chip_type == MAMI_KERN_TYPE_KUNPENG960_1P || chip_type == MAMI_KERN_TYPE_KUNPENG960_2P || chip_type == MAMI_KERN_TYPE_KUNPENG960_4P) {
		reg_base = UCMI_REGBANK_BASE_KUNPENG950;
	} else {
		reg_base = UCMI_REGBANK_BASE_ASCEND_V1_UNIONS;
	}

	return reg_base;
}

static int i3c_send_recv_msg(u32 chip_id, drv_ubnic_xfer_msg *msg)
{
	return i3c_drv_ubnic_xfer(I3C_DRV_INST_ID_SELF, chip_id, msg);
}

STATIC bool udev_pcie_i3c_probe_check(u32 socket_id)
{
	u32 status = 1;
	int ret;

	ret = i3c_drv_ubnic_online_status(DRV_INST_ID_SELF, socket_id, &status);
	if (ret != 0 || status != 0) {
		return false;
	}
	ret = i3c_drv_ubnic_probe(DRV_INST_ID_SELF, socket_id);
	if (ret != 0) {
		return false;
	}
	return true;
}

int udev_pcie_i3c_probe(struct udev_pcie *pcie)
{
	int ret;
	drv_ubnic_xfer_msg msg = {0};
	char guid_str[GUID_STR_LEN];
	struct udev_hw_identify guid_info = {0};
	i3c_dev_stat_req dev_stat_req = {0};
	i3c_dev_stat_rsp dev_stat_rsp = {0};

	if (udev_pcie_i3c_probe_check(pcie->socket_id) == false) {
		return -ENODEV;
	}
	dev_stat_req.slot_id = pcie->slot_id;
	dev_stat_req.socket_id = pcie->socket_id;
	msg.op = PCIE_I3C_GET_DEV_STAT;
	msg.write_len = sizeof(i3c_dev_stat_req);
	msg.read_len = sizeof(i3c_dev_stat_rsp);
	msg.write_buf = &dev_stat_req;
	msg.read_buf = &dev_stat_rsp;
	ret = i3c_send_recv_msg(pcie->socket_id, &msg);
	if (ret != 0) {
		hw_err("send msg fail. (ret=%d; slot=%u; socket_id=%u;)\n", ret, pcie->slot_id, pcie->socket_id);
		return ret;
	}
	if (dev_stat_rsp.status != 0) {
		hw_info("dev not ready. (ret=%d; slot=%u; socket_id=%u; status=%u)\n", ret, pcie->slot_id, pcie->socket_id, dev_stat_rsp.status);
		return -EBUSY;
	}
	udev_guid_copy(pcie->hw.guid, dev_stat_rsp.guid);
	pcie->hw.slot_id = pcie->slot_id;
	pcie->hw.socket_id = pcie->socket_id;
	pcie->hw.access_type = UDEV_PCIE_I3C_LDST;
	pcie->hw.location = UDM_NEAR;
	pcie->hw.connect_type = UDEV_PCIE_I3C;
	pcie->hw.chip_type = pcie->chip_type;

	ret = udev_guid_to_str(dev_stat_rsp.guid, guid_str);
	if (ret == 0 && ures_get_attr(guid_str, &guid_info, sizeof(guid_info)) != 0) {
		hw_info("udev_guid_to_str.(slot=%u; socket_id=%u;chip_type=%u;ubc_ue0_guid=%s;)\n", pcie->slot_id, pcie->socket_id, pcie->chip_type, guid_str);
	}

	ret = udev_set_guid_dev(&pcie->hw);
	if (ret != 0) {
		hw_err("udev_set_guid_dev fail. (ret=%d; slot=%u; socket_id=%u;)\n", ret, pcie->slot_id, pcie->socket_id);
		return ret;
	}

	return 0;
}

int udev_pcie_probe(struct udev_pcie *pcie)
{
	int ret;
	u32 reg_val = 0;

	if (pcie->chip_type == MAMI_KERN_TYPE_1825) {
		return udev_pcie_i3c_probe(pcie);
	}
	ret = (int)starry_drv_get_ucmi_bsp_state(DRV_INST_ID_SELF, pcie->socket_id, &reg_val);
	if (ret != 0 || reg_val != 0) {
#ifndef EMU_ST
		hw_err("check ucmi bsp state failed. (ret=%d, reg_val=%u, devid=%u)\n", ret, reg_val, pcie->devid);
		return -EBUSY;
#endif
	}

	ret = ucmi_init(UDEV_PCIE_ESPI, pcie);
	if (ret != 0) {
		hw_err("UCMI pcie init failed. (ret=%d)\n", ret);
		return ret;
	}

	ret = udev_config_ucmi_win(pcie);
	if (ret != 0) {
		goto fail;
	}

	ret = udev_pcie_hw_info_init(pcie);
	if (ret != 0) {
		goto fail;
	}

	ret = udev_set_guid_dev(&pcie->hw);
	if (ret != 0) {
		goto fail;
	}

	return 0;

fail:
	ucmi_uninit(UDEV_PCIE_ESPI, pcie);
	return ret;
}

static int udev_guid_list_detected_del(struct udev_pcie *chip, struct ures_key *key, u32 num)
{
	u32 i;
	int ret;
	char guid_str[GUID_STR_LEN];

	ret = udev_guid_to_str(chip->hw.guid, guid_str);
	if (unlikely(ret != 0)) {
		hw_err("Trans guid to string faled.\n");
		return ret;
	}
	for (i = 0; i < num; i++) {
		if (strcmp(guid_str, key[i].name) == 0) {
			key[i].name[0] = '\0';
			break;
		}
	}
	return 0;
}

#define UDEV_PROBE_PRINT_INTERVAL 30U
static u32 g_probe_cnt;
static u32 g_last_probe_num = UINT_MAX;

int udev_pcie_probe_all(struct ures_key *key, u32 num)
{
	u32 i;
	int ret;
	struct udev_pcie *chip = NULL;
	u32 probe_num = 0;

	mutex_lock(&g_pcie_mutex);
	for (i = 0; i < g_pcie_chips.chip_num; i++) {
		chip = &g_pcie_chips.udev_chip[i];
		if (!chip->chip_valid) {
			continue;
		}
		ret = udev_pcie_probe(chip);
		if (ret != 0) {
			if (ret != -ENODEV) {
				hw_err("Single udev probe failed. (ret:%d)\n", ret);
			}
		} else {
			probe_num++;
			ret = udev_guid_list_detected_del(chip, key, num);
			if (unlikely(ret != 0)) {
				continue;
			}
		}
	}
	ret = (probe_num == 0) ? -EINVAL : 0;
	g_probe_cnt++;
	if (probe_num != g_last_probe_num) {
		hw_info("pcie probe end. (probe num:%u, call cnt:%u)\n", probe_num, g_probe_cnt);
		g_last_probe_num = probe_num;
	} else if (g_probe_cnt % UDEV_PROBE_PRINT_INTERVAL == 0) {
		hw_info("pcie probe end. (probe num:%u, call cnt:%u)\n", probe_num, g_probe_cnt);
	}
	mutex_unlock(&g_pcie_mutex);
	return ret;
}

static int udev_chip_mng_check(chip_ub_mng *chip_mng)
{
	int i = 0;

	hw_info("starry get chip mng. (chip_type:%u, bar_num:%u, ucmi_id:%u)\n",
		chip_mng->chip_type, chip_mng->ucmi_map_bar_num, chip_mng->ucmi_id);
	if (chip_mng->chip_type < PCIE_CHIP_TYPE_UNIONS || chip_mng->chip_type >= PCIE_CHIP_TYPE_MAX) {
		hw_err("starry get chip type invalid. (min_type:%u, max_type:%u, cur_type:%u)\n", PCIE_CHIP_TYPE_UNIONS,
			PCIE_CHIP_TYPE_MAX, chip_mng->chip_type);
		return -EINVAL;
	}
	if (chip_mng->chip_type == PCIE_CHIP_TYPE_1825) {
		return 0;
	}
	for (i = 0; i < UCMI_MAP_BAR_NUM_MAX; i++) {
		hw_info("starry get bar%d. (bar_addr:%#llx, bar_size:%#llx)\n", i, chip_mng->base_addr[i], chip_mng->size[i]);
	}
	if ((chip_mng->ucmi_map_bar_num == 0 || chip_mng->ucmi_map_bar_num > UCMI_MAP_BAR_NUM_MAX)) {
		hw_err("starry get bar num invalid. (min_bar:0, max_bar:%u, cur_bar:%u)\n", UCMI_MAP_BAR_NUM_MAX, chip_mng->ucmi_map_bar_num);
		return -EINVAL;
	}
	if (chip_mng->ucmi_id >= MAX_UCMI_NUM_PER_FPGA) {
		hw_err("starry get ucmi_id invalid. (max_ucmi:%u, cur_ucmi:%u)\n", MAX_UCMI_NUM_PER_FPGA, chip_mng->ucmi_id);
		return -EINVAL;
	}
	return 0;
}

static u32 udev_get_chip_type(u32 starry_chip_type)
{
	static int chip_type_map[] = {
		[PCIE_CHIP_TYPE_UNIONS] = MAMI_KERN_TYPE_UNIONS,
		[PCIE_CHIP_TYPE_ASCEND_V1] = MAMI_KERN_TYPE_ASCEND_V1,
		[PCIE_CHIP_TYPE_KUNPENG950_1P] = MAMI_KERN_TYPE_KUNPENG950_1P,
		[PCIE_CHIP_TYPE_KUNPENG950_2P] = MAMI_KERN_TYPE_KUNPENG950_2P,
		[PCIE_CHIP_TYPE_KUNPENG950_4P] = MAMI_KERN_TYPE_KUNPENG950_4P,
		[PCIE_CHIP_TYPE_UNIONS_210] = MAMI_KERN_TYPE_UNIONS_210,
		[PCIE_CHIP_TYPE_ASCEND_V2] = MAMI_KERN_TYPE_ASCEND_V2,
		[PCIE_CHIP_TYPE_KUNPENG960_1P] = MAMI_KERN_TYPE_KUNPENG960_1P,
		[PCIE_CHIP_TYPE_KUNPENG960_2P] = MAMI_KERN_TYPE_KUNPENG960_2P,
		[PCIE_CHIP_TYPE_KUNPENG960_4P] = MAMI_KERN_TYPE_KUNPENG960_4P,
		[PCIE_CHIP_TYPE_1825] = MAMI_KERN_TYPE_1825,
	};

	if (starry_chip_type >= PCIE_CHIP_TYPE_MAX || starry_chip_type < PCIE_CHIP_TYPE_UNIONS) {
		return MAMI_KERN_TYPE_INVALID;
	}

	return chip_type_map[starry_chip_type];
}

STATIC int udev_pcie_i3c_symbol_check(void)
{
	if (i3c_symbol_init() != 0) {
		return 1;
	}
	return 0;
}

STATIC int udev_pcie_i3c_alloc_dev_id(u32 slot_id, u32 socket_id, struct udev_pcie *chip)
{
	int ret;

	ret = mng_alloc_devid(slot_id, socket_id, &chip->devid);
	if (ret != 0 && ret != -EEXIST) {
		chip->chip_valid = false;
		hw_err("Alloc i3c devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", slot_id, socket_id, ret);
		return ret;
	}
	hw_info("Alloc i3c devid. (slot_id=%u, socket_id=%u, devid=%u)\n", slot_id, socket_id, chip->devid);
	return 0;
}

#define CIB_PCIE_RX_PRO_BASE 0x5B000U
#define UCMI_BIT_MASK (MAX_UCMI_NUM_PER_FPGA - 1U)
#define FPGA_UCMI_OFFSET 0x4U
#define FPGA_BACK_REG 0x1E0U

static u64 get_fpga_back_reg_addr(u64 fpga_reg_base, u32 ucmi_id)
{
	return fpga_reg_base + CIB_PCIE_RX_PRO_BASE + (ucmi_id & UCMI_BIT_MASK) * FPGA_UCMI_OFFSET + FPGA_BACK_REG;
}

#define FPGA_INT_REG_OFFSET  0x51000UL
#define FPGA_INT_OFFSET 0x4U

static u64 get_fpga_msi_int_addr(u64 fpga_reg_base, u32 msi_start)
{
	return fpga_reg_base + FPGA_INT_REG_OFFSET + msi_start * FPGA_INT_OFFSET;
}

STATIC int udev_get_starry_config(void)
{
	int ret = 0;
	u32 i = 0, slot_id;
	u64 bar_size;
	struct udev_pcie *chip = NULL;

	ret = (int)starry_drv_board_get_ucmi_chip_num(DRV_INST_ID_SELF, &g_pcie_chips.chip_num);
	hw_info("starry get chip num. (ret:%d, chip_num:%u)\n", ret, g_pcie_chips.chip_num);
	if (ret != 0 || (g_pcie_chips.chip_num == 0) || (g_pcie_chips.chip_num > 0x2 * DEV_MAX_NUM)) { /* socket_id validation */
		hw_err("starry get chip num failed. (ret:%d, chip_num:%u)\n", ret, g_pcie_chips.chip_num);
		return -EINVAL;
	}

	ret = (int)starry_drv_board_get_tor_slotid(&slot_id);
	hw_info("starry get slot_id. (ret:%d, slot_id:%u)\n", ret, slot_id);
	if (ret != 0 || !is_slot_id_valid(slot_id)) { /* reported by lcne, in some lab environments the slot interface reports errors. in this scenario, slot_id is set to 1 */
		slot_id = 1;
	}

	g_pcie_chips.udev_chip = kcalloc(g_pcie_chips.chip_num, sizeof(struct udev_pcie), GFP_KERNEL | __GFP_ACCOUNT);
	if (g_pcie_chips.udev_chip == NULL) {
		g_pcie_chips.chip_num = 0;
		return -ENOMEM;
	}

	for (i = 0; i < g_pcie_chips.chip_num; i++) {
		chip = &g_pcie_chips.udev_chip[i];
		chip->slot_id = slot_id;
		chip->socket_id = i;

		ret = (int)starry_drv_board_get_chip_mng(DRV_INST_ID_SELF, i, &chip->chip_mng);
		hw_info("starry get chip mng ret. (ret:%d, socket_id:%u)\n", ret, i);
		if (ret != 0) {
			chip->chip_valid = false;
			continue;
		}

		ret = udev_chip_mng_check(&chip->chip_mng);
		if (ret != 0) {
			chip->chip_valid = false;
			continue;
		} else if (chip->chip_mng.chip_type == PCIE_CHIP_TYPE_1825) {
			if (udev_pcie_i3c_symbol_check() == 0 && udev_pcie_i3c_alloc_dev_id(slot_id, i, chip) == 0) {
				chip->chip_valid = true;
				chip->chip_type = MAMI_KERN_TYPE_1825;
			}
			continue;
		}

		chip->chip_type = udev_get_chip_type(chip->chip_mng.chip_type); /* chip_type is validated in the chip mng check */
		chip->ucmi_reg_base = get_chip_reg_base(chip->chip_type);

		/* Ascend_v1/kunpeng950_2p occupies 2 chip_num, devid is calculated by the first chip_num */
		if ((i % 2 == 1) && (chip->chip_type == MAMI_KERN_TYPE_ASCEND_V1 || chip->chip_type == MAMI_KERN_TYPE_KUNPENG950_2P ||
								chip->chip_type == MAMI_KERN_TYPE_ASCEND_V2 || chip->chip_type == MAMI_KERN_TYPE_KUNPENG960_2P)) {
			chip->chip_valid = false;
			continue;
		}

		ret = (int)starry_drv_pci_get_device_bar(chip->chip_mng.devnum, 0, &chip->fpga_reg_base, &bar_size);
		hw_info("starry get device bar. (ret:%d, socket_id:%u, fpga_reg_base:%#llx, fpga_bar_size:%#llx)\n",
			ret, i, chip->fpga_reg_base, bar_size);
		if (ret != 0) {
			chip->chip_valid = false;
			continue;
		}

		ret = mng_alloc_devid(slot_id, i, &chip->devid);
		if (ret != 0 && ret != -EEXIST) {
			chip->chip_valid = false;
			hw_err("Alloc devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", slot_id, i, ret);
			continue;
		}
		hw_info("Alloc devid. (slot_id=%u, socket_id=%u, devid=%u)\n", slot_id, i, chip->devid);

		ret = uop_fpga_info_init_kern(chip->devid, chip->socket_id,
			get_fpga_back_reg_addr(chip->fpga_reg_base, chip->chip_mng.ucmi_id),
			get_fpga_msi_int_addr(chip->fpga_reg_base, chip->chip_mng.msi_start));
		if (ret != 0) {
			chip->chip_valid = false;
			continue;
		}

		chip->chip_valid = true;
	}
	return 0;
}

int udev_pcie_init(void)
{
	int ret;

	ret = udev_get_starry_config();
	if (ret != 0) {
		hw_err("udev get starry config failed. (ret=%d)\n", ret);
		return ret;
	}
	return 0;
}

int udev_pcie_uninit(void)
{
	mutex_lock(&g_pcie_mutex);
	if (g_pcie_chips.udev_chip != NULL) {
		kfree(g_pcie_chips.udev_chip);
		g_pcie_chips.udev_chip = NULL;
	}
	mutex_unlock(&g_pcie_mutex);
	i3c_symbol_uninit();
	uop_fpga_info_uninit_all_kern();
	ures_reset();
	return 0;
}
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
DECLARE_FEATURE_AUTO_INIT(udev_pcie_init, FEATURE_LOADER_STAGE_9);
DECLARE_FEATURE_AUTO_UNINIT(udev_pcie_uninit, FEATURE_LOADER_STAGE_9);
#endif
