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

#include "ubfm_common.h"
#include "fmdevm.h"
#include "ures.h"
#include "udev_hw_pub.h"
#include "udev_map.h"
#include "udev_ucmi_ops.h"
#include "mng_devid.h"
#include "udev_map_inner.h"


static int udev_mem_asic_to_ures(struct ures_inst *inst, enum uop_item_type type, const char *name,
								 struct ures_addr_info *addr)
{
	int ret;

	if (type == UOP_ITEM_MEM) {
		ret = ures_set_mem_base(inst, name, addr);
	} else {
		ret = ures_set_reg_base(inst, name, addr);
	}

	return ret;
}

static int udev_mem_local_asic_to_ures(struct ures_inst *inst, enum uop_item_type type, const char *name,
									   struct ures_addr_info *local_addr)
{
	struct ures_key key;
	int ret;

	ret = UBFM_SPRINTF(key.name, sizeof(key.name), "%s_local", name);
	if (ret <= 0) {
		hw_err("Format mem ures name failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, name);
		return -EINVAL;
	}

	return udev_mem_asic_to_ures(inst, type, key.name, local_addr);
}

static int udev_mem_vm_local_asic_to_ures(struct ures_inst *inst, enum uop_item_type type, const char *name,
									   struct ures_addr_info *local_addr, u32 is_share)
{
	struct ures_key key;
	int ret;

	ret = UBFM_SPRINTF(key.name, sizeof(key.name), is_share ? "%s_local_sm" : "%s_local_pm", name);
	if (ret <= 0) {
		hw_err("Format mem ures name failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, name);
		return -EINVAL;
	}

	return udev_mem_asic_to_ures(inst, type, key.name, local_addr);
}

static int udev_mem_local_asic_map(struct ures_inst *inst, enum uop_item_type type, const char *name,
								   struct ures_addr_info *asic_addr, struct ures_addr_info *local_addr)
{
	struct ures_key key;
	int ret;

	ret = UBFM_SPRINTF(key.name, sizeof(key.name), "%s_local", name);
	if (ret <= 0) {
		hw_err("Format mem ures name failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, name);
		return -EINVAL;
	}

	return uop_kern_add_memory(inst, key.name, type, asic_addr, local_addr);
}

static int udev_mem_ures_find(struct ures_inst *inst, enum uop_item_type type, const char *name)
{
	struct ures_addr_info addr_info = {0};
	int ret;

	if (type == UOP_ITEM_MEM) {
		ret = ures_get_mem_base(inst, name, &addr_info);
	} else {
		ret = ures_get_reg_base(inst, name, &addr_info);
	}

	return ret;
}

static int udev_mem_normal_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem,
							   struct udev_hw_identify *hw, u32 is_uop_map)
{
	struct ures_key local_key = {0};
	struct ures_addr_info asic_addr = {0};
	struct ures_addr_info local_addr = {0};
	int ret;

	ret = UBFM_SPRINTF(local_key.name, sizeof(local_key.name), "%s_win0", mem->name);
	if (ret <= 0) {
		hw_err("Format mem ures name failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return -EINVAL;
	}

	ures_addr_info_pack(&asic_addr, mem->phy_addr, mem->len);
	ret = udev_mem_asic_to_ures(inst, type, mem->name, &asic_addr);
	if (ret != 0) {
		hw_err("Asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return ret;
	}

	ret = udev_mem_asic_to_ures(inst, type, local_key.name, &asic_addr);
	if (ret != 0) {
		hw_err("Asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return ret;
	}

	ures_addr_info_pack(&local_addr, mem->phy_addr, mem->len);
	ret = udev_mem_local_asic_to_ures(inst, type, local_key.name, &local_addr);
	if (ret != 0) {
		hw_err("Local asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return ret;
	}

	if (is_uop_map != UOP_NEED_MAP) {
		return 0;
	}

	ret = udev_mem_local_asic_map(inst, type, local_key.name, &asic_addr, &local_addr);
	if (ret != 0) {
		hw_err("Local asic map failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
	}

	return ret;
}

static addr_map_func g_map_func;

int udev_register_map_func(addr_map_func func)
{
	if (func == NULL) {
		hw_err("Invalid map func.\n");
		return -EINVAL;
	}

	g_map_func = func;
	return 0;
}

int udev_get_gpa_list(u64 hpa, u32 size, struct addr_seg **segs, u32 *cnt)
{
	if (segs == NULL || cnt == NULL) {
		return -EINVAL;
	}

	if (g_map_func == NULL) {
		return -EPERM;
	}

	return g_map_func(hpa, size, segs, cnt);
}

void put_gpa_list(struct addr_seg *segs)
{
	if (segs != NULL) {
		kfree(segs);
	}
}

static int udev_mem_vm_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem,
							   struct udev_hw_identify *hw, u32 is_uop_map)
{
	struct ures_key local_key = {0};
	struct ures_addr_info asic_addr = {0}, local_addr = {0};
	struct addr_seg *seg = NULL;
	size_t i = 0;
	u32 cnt = 0;
	int ret;

	ures_addr_info_pack(&asic_addr, mem->phy_addr, mem->len);
	ret = udev_mem_asic_to_ures(inst, type, mem->name, &asic_addr);
	if (ret != 0) {
		hw_err("Asic to ures failed. (ret=%d; dieid=%u; name=%s)\n", ret, inst->die_id, mem->name);
		return ret;
	}

	ret = udev_get_gpa_list(mem->phy_addr, mem->len, &seg, &cnt);
	if (ret != 0) {
		return ret;
	}

	for (i = 0; i < cnt; i++) {
		ret = UBFM_SPRINTF(local_key.name, sizeof(local_key.name), "%s_win%zu", mem->name, i);
		if (ret <= 0) {
			hw_err("Format mem ures name failed. (ret=%d; dieid=%u; name=%s; i=%zu)\n", ret,
				inst->die_id, mem->name, i);
			ret = -EINVAL;
			break;
		}

		ures_addr_info_pack(&asic_addr, seg[i].hpa_start, seg[i].size);
		ret = udev_mem_asic_to_ures(inst, type, local_key.name, &asic_addr);
		if (ret != 0) {
			hw_err("Asic to ures failed. (ret=%d; dieid=%u; name=%s)\n", ret, inst->die_id, mem->name);
			break;
		}

		ures_addr_info_pack(&local_addr, seg[i].gpa_start, seg[i].size);
		ret = udev_mem_vm_local_asic_to_ures(inst, type, local_key.name, &local_addr, seg[i].is_share);
		if (ret != 0) {
			hw_err("Local asic to ures failed. (ret=%d; dieid=%u; name=%s)\n", ret, inst->die_id, mem->name);
			break;
		}

		if (is_uop_map != UOP_NEED_MAP) {
			continue;
		}

		ret = udev_mem_local_asic_map(inst, type, local_key.name, &asic_addr, &local_addr);
		if (ret != 0) {
			hw_err("Local asic map failed. (ret=%d; dieid=%u; name=%s)\n", ret, inst->die_id, mem->name);
			break;
		}
	}

	put_gpa_list(seg);
	return ret;
}

static void udev_ucmi_addr_pack(struct udev_ucmi_addr *ucmi_addr, struct ures_inst *inst, struct udev_soc_addr *mem)
{
	ucmi_addr->name = mem->name;
	ucmi_addr->chip_addr = mem->phy_addr;
	ucmi_addr->size = mem->len;
	ucmi_addr->devid = inst->devid;
	ucmi_addr->dieid = inst->die_id;
}

static bool udev_ignore_mem_ucmi_map(struct ures_inst *inst, struct udev_soc_addr *mem, struct udev_hw_identify *hw)
{
	u32 win_mode = UOP_MODE_EXCLUSIVE;

	if (hw->chip_type != MAMI_KERN_TYPE_UNIONS && hw->chip_type != MAMI_KERN_TYPE_UNIONS_210 &&
		hw->chip_type != MAMI_KERN_TYPE_ASCEND_V2) {
		return false;
	}
	(void)ucmi_get_win_mode(hw->connect_type, inst->devid, &win_mode);

	if (mem->perf_mark == HW_PERF_INSENSITIVE && win_mode == UOP_MODE_SHARE) {
		return true;
	}
	if (mem->perf_mark == HW_PERF_INSENSITIVE && hw->chip_type == MAMI_KERN_TYPE_ASCEND_V2) {
		return true;
	}
	return false;
}

static int udev_mem_ucmi_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem,
							 struct udev_hw_identify *hw, u32 is_uop_map)
{
	struct ures_addr_info asic_addr = {0};
	struct ures_addr_info local_addr = {0};
	struct udev_ucmi_addr ucmi_addr = {0};
	struct udev_ucmi_addr_info *addr_info = NULL;
	struct udev_ucmi_win *pos = NULL;

	int ret = 0;

	udev_ucmi_addr_pack(&ucmi_addr, inst, mem);
	ures_addr_info_pack(&asic_addr, mem->phy_addr, mem->len);
	ret = udev_mem_asic_to_ures(inst, type, mem->name, &asic_addr);
	if (ret != 0) {
		hw_err("Asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return ret;
	}

	if (udev_ignore_mem_ucmi_map(inst, mem, hw)) {
		return 0;
	}

	addr_info = ucmi_get_addr_info(hw->connect_type, ucmi_addr.devid, mem->phy_addr, mem->name);
	if (addr_info == NULL) {
		ret = ucmi_assign_win(hw->connect_type, &ucmi_addr);
		if (ret != 0) {
			hw_err("Assign window failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
				inst->devid, inst->die_id, mem->name);
			return ret;
		}
		addr_info = ucmi_get_addr_info(hw->connect_type, ucmi_addr.devid, mem->phy_addr, mem->name);
		if (addr_info == NULL) {
			hw_err("Null addr info. (devid=%u;)\n", inst->devid);
			return -EINVAL;
		}
	}

	list_for_each_entry(pos, &addr_info->win_list, entry) {
		ures_addr_info_pack(&asic_addr, pos->chip_addr, pos->size);
		ures_addr_info_pack(&local_addr, pos->ucmi_off + pos->ucmi_base, pos->size);

		ret = udev_mem_asic_to_ures(inst, type, pos->name, &asic_addr);
		if (ret != 0) {
			ucmi_remove_win(hw->connect_type, &ucmi_addr);
			ucmi_put_addr_info(hw->connect_type, ucmi_addr.devid);
			hw_err("Asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
				inst->devid, inst->die_id, mem->name);
			return ret;
		}

		ret = udev_mem_local_asic_to_ures(inst, type, pos->name, &local_addr);
		if (ret != 0) {
			ucmi_remove_win(hw->connect_type, &ucmi_addr);
			ucmi_put_addr_info(hw->connect_type, ucmi_addr.devid);
			hw_err("Local asic to ures failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
				   inst->devid, inst->die_id, mem->name);
			return ret;
		}

		if (is_uop_map != UOP_NEED_MAP) {
			continue;
		}

		ret = udev_mem_local_asic_map(inst, type, pos->name, &asic_addr, &local_addr);
		if (ret != 0) {
			/* currently only added once when creating a device, memory can be freed when destroying the device */
			ucmi_remove_win(hw->connect_type, &ucmi_addr);
			ucmi_put_addr_info(hw->connect_type, ucmi_addr.devid);
			hw_err("Local asic map failed. (ret=%d; devid=%u; dieid=%u; name=%s)\n", ret,
				   inst->devid, inst->die_id, mem->name);
			return ret;
		}
	}

	ucmi_put_addr_info(hw->connect_type, ucmi_addr.devid);

	return 0;
}

int udev_mem_map(struct ures_inst *inst, enum uop_item_type type, struct udev_soc_addr *mem, u32 need_map)
{
	int ret;
	struct udev_hw_identify hw = {0};

	if (inst == NULL || mem == NULL || mem->name == NULL) {
		hw_err("Invalid para.\n");
		return -EINVAL;
	}

	if ((type < UOP_ITEM_MEM) || (type >= UOP_ITEM_TYPE_MAX)) {
		hw_err("Invalid type.");
		return -EINVAL;
	}

	if (strnlen(mem->name, URES_NAME_LEN) == URES_NAME_LEN) {
		hw_err("Invalid name.\n");
		return -EINVAL;
	}

	/* query whether the mem name already exists, if so return directly, do not add repeatedly */
	ret = udev_mem_ures_find(inst, type, mem->name);
	if (ret == 0) {
		return 0;
	}

	ret = ures_dev_get_attr(inst->devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (ret != 0) {
		hw_err("Get hw attr failed. (ret=%d; devid=%u; dieid=%u; addr_name=%s)\n", ret,
			   inst->devid, inst->die_id, mem->name);
		return ret;
	}

	if (hw.connect_type == UDEV_PCIE_ESPI || hw.connect_type == UDEV_ESPI) {
		return udev_mem_ucmi_map(inst, type, mem, &hw, need_map);
	} else if (hw.connect_type == UDEV_VM) {
		return udev_mem_vm_map(inst, type, mem, &hw, need_map);
	}

	return udev_mem_normal_map(inst, type, mem, &hw, need_map);
}
EXPORT_SYMBOL_GPL(udev_mem_map);
