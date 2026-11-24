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
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "ures.h"
#include "uop.h"
#include "uop_log.h"
#include "uop_kern.h"
#include "kmap_kern.h"
#include "mami_string.h"
#include "fmdevm.h"
#include "ubfm_common.h"

#define KMAP_MAX_MEM_NUM	(DIE_MAX_NUM * URES_MEM_MAX_NUM)
#define KMAP_MAX_REG_NUM	(DIE_MAX_NUM * URES_REG_MAX_NUM)
#define KMAP_MAX_BARE_ITEM_NUM	((KMAP_MAX_MEM_NUM) + (KMAP_MAX_REG_NUM))
#define KMAP_MIN_IDLE_WIN_NUM	 2U
#define KMAP_MAX_ACC_NODE_NUM	 2U

static const struct kmap_ops *g_kmap_ops[UOP_ACC_MAX];

const struct kmap_ops *kmap_get_ops(enum uop_acc_type type)
{
	if (unlikely(type >= UOP_ACC_MAX)) {
		uop_err("Kmap get ops failed, type is illegal. (type=%u)\n", type);
		return NULL;
	}

	return g_kmap_ops[type];
}

static int kmap_get_die_num(u32 devid, u32 *die_num)
{
	u64 value = 0;
	int ret;

	ret = ures_get_key_value(devid, "die_num", &value);
	if (unlikely((ret != 0) || (value == 0) || (value > DIE_MAX_NUM))) {
		uop_err("Get die num fail. (ret=%d; devid=%u; die_num=%llu)\n", ret, devid, value);
		return -EINVAL;
	}
	*die_num = (u32)value;
	return 0;
}

static int kmap_dev_assign(u32 devid, enum uop_acc_type type, struct kmap_dev *map_dev)
{
	u32 die_num = 0;
	int ret;

	ret = kmap_get_die_num(devid, &die_num);
	if (unlikely(ret != 0)) {
		uop_err("Get die num fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	map_dev->devid = devid;
	map_dev->die_num = die_num;
	map_dev->acc_type = type;
	map_dev->mem_num = 0;
	map_dev->mem = NULL;
	map_dev->reg_num = 0;
	map_dev->reg = NULL;
	map_dev->bare_item_num = 0;
	map_dev->bare_item = NULL;
	map_dev->acc_node_num = 0;
	map_dev->acc_node = NULL;

	return 0;
}

static int _kmap_mem_add(struct kmap_dev *map_dev, struct ures_mem *mem, u32 die_id)
{
	struct ures_inst inst = {.devid = map_dev->devid, .die_id = die_id};
	struct ures_addr_info asic_mem = {0};
	char asic_name[URES_NAME_LEN] = {0};
	struct kmap_addr *map_mem = NULL;
	u32 index = map_dev->mem_num;
	int ret;

	if (strstr(mem->name, "_local") == NULL) {
		return 0;
	}

	/* Asic name will be checked in ures_user_get_mem_base, ignore regurn value */
	(void)ubfm_memcpy(asic_name, URES_NAME_LEN, mem->name, (size_t)(strstr(mem->name, "_local") - mem->name));
	ret = ures_get_mem_base(&inst, asic_name, &asic_mem);
	if (unlikely((ret != 0) || (asic_mem.size != mem->info.size))) {
		uop_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u; name=%s; asic_size=%u; size=%u)\n",
			ret, inst.devid, inst.die_id, mem->name, asic_mem.size, mem->info.size);
		return -EINVAL;
	}

	if (unlikely(index >= KMAP_MAX_MEM_NUM)) {
		uop_err("No space. (devid=%u; die_id=%u; name=%s; max_mem_num=%u)\n",
			map_dev->devid, die_id, mem->name, (u32)KMAP_MAX_MEM_NUM);
		return -ENOSPC;
	}

	map_mem = &map_dev->mem[index];
	ret = ubfm_strncpy(map_mem->name, URES_NAME_LEN, mem->name, URES_NAME_LEN - 1);
	if (unlikely(ret != 0)) {
		uop_err("Name copy fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return -ENOMEM;
	}
	map_mem->acc_addr = 0;
	map_mem->local_asic_addr = mem->info.addr;
	map_mem->asic_addr = asic_mem.addr;
	map_mem->size = mem->info.size;
	map_mem->die_id = die_id;
	map_dev->mem_num++;

	return 0;
}

static int kmap_mem_add(struct kmap_dev *map_dev, struct ures_mem *mem, u32 num, u32 die_id)
{
	u32 i;
	int ret;

	for (i = 0; i < num; i++) {
		ret = _kmap_mem_add(map_dev, &mem[i], die_id);
		if (unlikely(ret != 0)) {
			return ret;
		}
	}
	return 0;
}

STATIC int kmap_mem_init(struct kmap_dev *map_dev)
{
	u32 die_id;
	u32 num = 0;
	int ret;

	map_dev->mem = (struct kmap_addr *)vmalloc(sizeof(struct kmap_addr) * KMAP_MAX_MEM_NUM);
	if (unlikely(map_dev->mem == NULL)) {
		uop_err("Mem list alloc fail. (devid=%u; size=%zu)\n",
			map_dev->devid, sizeof(struct kmap_addr) * KMAP_MAX_MEM_NUM);
		return -ENOMEM;
	}

	for (die_id = 0; die_id < map_dev->die_num; die_id++) {
		struct ures_inst inst = {.devid = map_dev->devid, .die_id = die_id};
		struct ures_mem *mem = NULL;

		ret = ures_get_mem_list(&inst, &mem, &num);
		if (unlikely((ret != 0) || (num == 0) || (mem == NULL))) {
			continue;
		}
		ret = kmap_mem_add(map_dev, mem, num, die_id);
		ures_put_mem_list(mem);
		if (unlikely(ret != 0)) {
			uop_err("Mem add fail. (ret=%d; devid=%u; die_id=%u; num=%u)\n", ret, map_dev->devid, die_id, num);
			vfree(map_dev->mem);
			map_dev->mem = NULL;
			map_dev->mem_num = 0;
			return ret;
		}
	}

	if ((map_dev->mem != NULL) && (map_dev->mem_num == 0)) {
		vfree(map_dev->mem);
		map_dev->mem = NULL;
	}
	return 0;
}

static void kmap_mem_uninit(struct kmap_dev *map_dev)
{
	if (map_dev->mem != NULL) {
		vfree(map_dev->mem);
		map_dev->mem = NULL;
	}
	map_dev->mem_num = 0;
}

static int _kmap_reg_add(struct kmap_dev *map_dev, struct ures_reg *reg, u32 die_id)
{
	struct ures_inst inst = {.devid = map_dev->devid, .die_id = die_id};
	struct ures_addr_info asic_reg = {0};
	char asic_name[URES_NAME_LEN] = {0};
	struct kmap_addr *map_reg = NULL;
	u32 index = map_dev->reg_num;
	int ret;

	if (strstr(reg->name, "_local") == NULL) {
		return 0;
	}

	/* Asic name will be checked in ures_user_get_mem_base, ignore regurn value */
	(void)ubfm_memcpy(asic_name, URES_NAME_LEN, reg->name, (size_t)(strstr(reg->name, "_local") - reg->name));
	ret = ures_get_reg_base(&inst, asic_name, &asic_reg);
	if (unlikely((ret != 0) || (asic_reg.size != reg->info.size))) {
		uop_err("Get reg base fail. (devid=%u; die_id=%u; name=%s; asic_size=%u; size=%u)\n",
			inst.devid, inst.die_id, reg->name, asic_reg.size, reg->info.size);
		return -EINVAL;
	}

	if (unlikely(index >= KMAP_MAX_REG_NUM)) {
		uop_err("No space. (devid=%u; die_id=%u; name=%s; max_mem_num=%u)\n",
			map_dev->devid, die_id, reg->name, (u32)KMAP_MAX_REG_NUM);
		return -ENOSPC;
	}

	map_reg = &map_dev->reg[index];
	ret = ubfm_strncpy(map_reg->name, URES_NAME_LEN, reg->name, URES_NAME_LEN - 1);
	if (unlikely(ret != 0)) {
		uop_err("Name copy fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return -ENOMEM;
	}

	map_reg->acc_addr = 0;
	map_reg->local_asic_addr = reg->info.addr;
	map_reg->asic_addr = asic_reg.addr;
	map_reg->size = reg->info.size;
	map_reg->die_id = die_id;
	map_dev->reg_num++;

	return 0;
}

STATIC int kmap_reg_add(struct kmap_dev *map_dev, struct ures_reg *reg, u32 num, u32 die_id)
{
	u32 i;
	int ret;

	for (i = 0; i < num; i++) {
		ret = _kmap_reg_add(map_dev, &reg[i], die_id);
		if (unlikely(ret != 0)) {
			return ret;
		}
	}
	return 0;
}

static int kmap_reg_init(struct kmap_dev *map_dev)
{
	u32 die_id;
	u32 num = 0;
	int ret;

	map_dev->reg = (struct kmap_addr *)vmalloc(sizeof(struct kmap_addr) * KMAP_MAX_REG_NUM);
	if (unlikely(map_dev->reg == NULL)) {
		uop_err("Reg list alloc fail. (devid=%u; size=%zu)\n",
			map_dev->devid, sizeof(struct kmap_addr) * KMAP_MAX_REG_NUM);
		return -ENOMEM;
	}

	for (die_id = 0; die_id < map_dev->die_num; die_id++) {
		struct ures_inst inst = {.devid = map_dev->devid, .die_id = die_id};
		struct ures_reg *reg = NULL;

		ret = ures_get_reg_list(&inst, &reg, &num);
		if (unlikely((ret != 0) || (num == 0) || (reg == NULL))) {
			continue;
		}

		ret = kmap_reg_add(map_dev, reg, num, die_id);
		ures_put_reg_list(reg);
		if (unlikely(ret != 0)) {
			uop_err("Reg add fail. (ret=%d; devid=%u; die_id=%u; num=%u)\n", ret, map_dev->devid, die_id, num);
			vfree(map_dev->reg);
			map_dev->reg = NULL;
			map_dev->reg_num = 0;
			return ret;
		}
	}

	if ((map_dev->reg != NULL) && (map_dev->reg_num == 0)) {
		vfree(map_dev->reg);
		map_dev->reg = NULL;
	}
	return 0;
}

static void kmap_reg_uninit(struct kmap_dev *map_dev)
{
	if (map_dev->reg != NULL) {
		vfree(map_dev->reg);
		map_dev->reg = NULL;
	}
	map_dev->reg_num = 0;
}

static int kmap_bare_item_add(struct kmap_dev *map_dev, const char *name, struct ures_addr_info *info, u32 die_id)
{
	struct kmap_bare_addr *bare_addr = NULL;
	u32 index = map_dev->bare_item_num;
	int ret;
	u32 i;

	if (map_dev->acc_type != UOP_ACC_PCIE_LDST) {
		return 0;
	}

	if (strstr(name, "_win") != NULL) {
		return 0;
	}

	for (i = 0; i < map_dev->reg_num; i++) {
		if (strstr(map_dev->reg[i].name, name) != NULL) {
			return 0;
		}
	}

	for (i = 0; i < map_dev->mem_num; i++) {
		if (strstr(map_dev->mem[i].name, name) != NULL) {
			return 0;
		}
	}

	if (unlikely(index >= KMAP_MAX_BARE_ITEM_NUM)) {
		uop_err("No space. (devid=%u; die_id=%u; name=%s; max_bare_item_num=%u)\n",
			map_dev->devid, die_id, name, (u32)KMAP_MAX_BARE_ITEM_NUM);
		return -ENOSPC;
	}

	bare_addr = &map_dev->bare_item[index];
	ret = ubfm_strncpy(bare_addr->name, URES_NAME_LEN, name, URES_NAME_LEN - 1);
	if (unlikely(ret != 0)) {
		uop_err("Name copy fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return -ENOMEM;
	}

	bare_addr->asic_addr = info->addr;
	bare_addr->size = info->size;
	bare_addr->die_id = die_id;
	map_dev->bare_item_num++;

	return 0;
}

static int kmap_add_bare_item_from_reg(struct kmap_dev *map_dev)
{
	struct ures_inst inst = {0};
	struct ures_reg *reg = NULL;
	u32 num, die_id, i;
	int ret;

	for (die_id = 0; die_id < map_dev->die_num; die_id++) {
		inst.devid = map_dev->devid;
		inst.die_id = die_id;
		reg = NULL;
		num = 0;

		ret = ures_get_reg_list(&inst, &reg, &num);
		if (unlikely((ret != 0) || (num == 0) || (reg == NULL))) {
			continue;
		}
		for (i = 0; i < num; i++) {
			ret = kmap_bare_item_add(map_dev, reg[i].name, &reg[i].info, die_id);
			if (unlikely(ret != 0)) {
				break;
			}
		}
		ures_put_reg_list(reg);
		if (unlikely(ret != 0)) {
			uop_err("Reg add bare item fail. (ret=%d; devid=%u; die_id=%u; num=%u)\n", ret, map_dev->devid, die_id, num);
			return ret;
		}
	}
	return 0;
}

static int kmap_add_bare_item_from_mem(struct kmap_dev *map_dev)
{
	struct ures_inst inst = {0};
	struct ures_mem *mem = NULL;
	u32 num, die_id, i;
	int ret;

	for (die_id = 0; die_id < map_dev->die_num; die_id++) {
		inst.devid = map_dev->devid;
		inst.die_id = die_id;
		mem = NULL;
		num = 0;

		ret = ures_get_mem_list(&inst, &mem, &num);
		if (unlikely((ret != 0) || (num == 0) || (mem == NULL))) {
			continue;
		}
		for (i = 0; i < num; i++) {
			ret = kmap_bare_item_add(map_dev, mem[i].name, &mem[i].info, die_id);
			if (unlikely(ret != 0)) {
				break;
			}
		}
		ures_put_mem_list(mem);
		if (unlikely(ret != 0)) {
			uop_err("Mem add bare item fail. (ret=%d; devid=%u; die_id=%u; num=%u)\n", ret, map_dev->devid, die_id, num);
			return ret;
		}
	}
	return 0;
}

static int kmap_bare_item_init(struct kmap_dev *map_dev)
{
	int ret;

	map_dev->bare_item = (struct kmap_bare_addr *)vmalloc(sizeof(struct kmap_bare_addr) * KMAP_MAX_BARE_ITEM_NUM);
	if (unlikely(map_dev->bare_item == NULL)) {
		uop_err("Bare item alloc fail. (devid=%u; size=%zu)\n",
			map_dev->devid, sizeof(struct kmap_bare_addr) * KMAP_MAX_BARE_ITEM_NUM);
		return -ENOMEM;
	}

	ret = kmap_add_bare_item_from_mem(map_dev);
	if (unlikely(ret != 0)) {
		vfree(map_dev->bare_item);
		map_dev->bare_item = NULL;
		map_dev->bare_item_num = 0;
		return ret;
	}

	ret = kmap_add_bare_item_from_reg(map_dev);
	if (unlikely(ret != 0)) {
		vfree(map_dev->bare_item);
		map_dev->bare_item = NULL;
		map_dev->bare_item_num = 0;
		return ret;
	}
	if ((map_dev->bare_item != NULL) && (map_dev->bare_item_num == 0)) {
		vfree(map_dev->bare_item);
		map_dev->bare_item = NULL;
	}
	return 0;
}

static void kmap_bare_item_uninit(struct kmap_dev *map_dev)
{
	if (map_dev->bare_item != NULL) {
		vfree(map_dev->bare_item);
		map_dev->bare_item = NULL;
	}
	map_dev->bare_item_num = 0;
}

static int kamp_acc_node_num_init(u32 devid, u32 win_id_start, u32 win_num, u32 win_size, u32 *acc_node_num)
{
	struct ures_win_info win_info[URES_WIN_TYPE_MAX] = {0};
	struct udev_hw_identify hw = {0};
	u32 i;
	int ret;
	/* at least one idle win for userspace and kernelspace each */
	if (unlikely(win_num < KMAP_MIN_IDLE_WIN_NUM)) {
		uop_err("Get idle local win not enough. (devid=%u; win_num=%u)\n", devid, win_num);
		return -ENOSPC;
	}

	*acc_node_num = win_num > KMAP_MAX_ACC_NODE_NUM  ? KMAP_MAX_ACC_NODE_NUM : 1;

	ret = ures_dev_get_attr(devid, MAMI_UDEV_HW_IDEN_INFO, &hw, sizeof(struct udev_hw_identify));
	if (unlikely(ret != 0)) {
		uop_err("Get hw identity fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	win_info[URES_WIN_TYPE_KERN].win_id_start = win_id_start;
	win_info[URES_WIN_TYPE_KERN].win_num = *acc_node_num;
	win_info[URES_WIN_TYPE_KERN].win_size = win_size;

	win_info[URES_WIN_TYPE_USER].win_id_start = win_id_start + *acc_node_num;
	win_info[URES_WIN_TYPE_USER].win_num = win_num - *acc_node_num;
	win_info[URES_WIN_TYPE_USER].win_size = win_size;

	for (i = 0; i < URES_WIN_TYPE_MAX; i++) {
		ret = ures_set_share_local_win(devid, i, &win_info[i]);
		if (unlikely(ret != 0)) {
			return ret;
		}
	}
	return 0;
}

static int kmap_acc_node_init(struct kmap_dev *map_dev)
{
	u32 local_win_id_start = 0;
	u32 local_win_num = 0;
	u32 local_win_size = 0;
	struct kmap_acc_node *acc_node = NULL;
	u32 i;
	int ret;

	if (map_dev->bare_item_num == 0) {
		return 0;
	}

	ret = uop_kern_get_idle_local_win(map_dev->devid, &local_win_id_start, &local_win_num, &local_win_size);
	if (unlikely(ret != 0)) {
		uop_err("Get idle local win fail. (devid=%u; ret=%d)\n", map_dev->devid, ret);
		return ret;
	}

	ret = kamp_acc_node_num_init(map_dev->devid, local_win_id_start, local_win_num, local_win_size,
		&map_dev->acc_node_num);
	if (unlikely(ret != 0)) {
		return ret;
	}

	map_dev->acc_node = (struct kmap_acc_node *)vmalloc(sizeof(struct kmap_acc_node) * map_dev->acc_node_num);
	if (unlikely(map_dev->acc_node == NULL)) {
		uop_err("Acc node alloc fail. (devid=%u; size=%zu)\n",
			map_dev->devid, sizeof(struct kmap_acc_node) * map_dev->acc_node_num);
		return -ENOMEM;
	}

	for (i = 0; i < map_dev->acc_node_num; i++) {
		acc_node = &map_dev->acc_node[i];
		acc_node->local_win_id = local_win_id_start + i;
		acc_node->win_size = local_win_size;
		acc_node->local_asic_addr = 0;
		acc_node->acc_addr = 0;
		ret = uop_kern_get_local_addr(map_dev->devid, acc_node->local_win_id, &acc_node->local_asic_addr);
		if (ret != 0) {
			uop_err("Get local addr fail. (devid=%u; local_win_id=%u; ret=%d)\n",
				map_dev->devid, acc_node->local_win_id, ret);
			vfree(map_dev->acc_node);
			map_dev->acc_node = NULL;
			map_dev->acc_node_num = 0;
			return ret;
		}
	}
	return 0;
}

static void kmap_acc_node_uninit(struct kmap_dev *map_dev)
{
	if (map_dev->acc_node != NULL) {
		vfree(map_dev->acc_node);
		map_dev->acc_node = NULL;
	}
	map_dev->acc_node_num = 0;
}

static int kmap_addr_init(struct kmap_dev *map_dev)
{
	int ret;

	ret = kmap_mem_init(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Mem init fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = kmap_reg_init(map_dev);
	if (unlikely(ret != 0)) {
		kmap_mem_uninit(map_dev);
		uop_err("Reg init fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = kmap_bare_item_init(map_dev);
	if (unlikely(ret != 0)) {
		kmap_reg_uninit(map_dev);
		kmap_mem_uninit(map_dev);
		uop_err("Bare item init fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = kmap_acc_node_init(map_dev);
	if (unlikely(ret != 0)) {
		kmap_bare_item_uninit(map_dev);
		kmap_reg_uninit(map_dev);
		kmap_mem_uninit(map_dev);
		uop_err("Acc node init fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
	}
	return ret;
}

static void kmap_addr_uninit(struct kmap_dev *map_dev)
{
	kmap_acc_node_uninit(map_dev);
	kmap_bare_item_uninit(map_dev);
	kmap_reg_uninit(map_dev);
	kmap_mem_uninit(map_dev);
}

static int _kmap_mem_map(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	struct kmap_addr *mem = map_dev->mem;
	u32 num = map_dev->mem_num;
	u32 i, j;
	int ret;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type isn't init. (acc_type=%u)\n", map_dev->acc_type);
		return -EINVAL;
	}

	if ((mem == NULL) || (num == 0)) {
		/* No memory addr. Map isn't needed */
		return 0;
	}

	for (i = 0; i < num; i++) {
		ret = ops->mem_map(map_dev->devid, &mem[i]);
		if (unlikely(ret != 0)) {
			uop_err("Mem map fail. (ret=%d; devid=%u; name=%s; num=%u; i=%u)\n",
				ret, map_dev->devid, mem->name, num, i);
			goto out;
		}
	}
	return 0;
out:
	for (j = 0; j < i; j++) {
		ops->mem_unmap(map_dev->devid, &mem[j]);
	}
	return ret;
}

static void _kmap_mem_unmap(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	struct kmap_addr *mem = map_dev->mem;
	u32 num = map_dev->mem_num;
	u32 i;

	if (unlikely(ops == NULL)) {
		return;
	}

	if ((mem == NULL) || (num == 0)) {
		return;
	}

	for (i = 0; i < num; i++) {
		ops->mem_unmap(map_dev->devid, &mem[i]);
	}
}

static int _kmap_acc_node_map(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	int ret;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type isn't init. (acc_type=%u)\n", map_dev->acc_type);
		return -EINVAL;
	}

	if (map_dev->acc_node_num == 0 || map_dev->acc_node == NULL) {
		return 0;
	}

	if (ops->acc_node_map == NULL) {
		uop_err("Acc node map isn't init. (acc_type=%u)\n", map_dev->acc_type);
		return -EINVAL;
	}

	ret = ops->acc_node_map(map_dev->devid, map_dev->acc_node, map_dev->acc_node_num);
	if (unlikely(ret != 0)) {
		uop_err("Acc node map fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}
	return 0;
}

static void _kmap_acc_node_unmap(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	u32 i;

	if (unlikely(ops == NULL)) {
		return;
	}
	if (map_dev->acc_node_num == 0 || map_dev->acc_node == NULL) {
		return;
	}

	if (ops->acc_node_unmap == NULL) {
		uop_err("Acc node unmap isn't init. (acc_type=%u)\n", map_dev->acc_type);
		return;
	}

	for (i = 0; i < map_dev->acc_node_num; i++) {
		struct kmap_addr node = {.acc_addr = map_dev->acc_node[i].acc_addr, .size = map_dev->acc_node[i].win_size};

		ops->acc_node_unmap(map_dev->devid, &node);
	}
}

static int _kmap_reg_map(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	struct kmap_addr *reg = map_dev->reg;
	u32 num = map_dev->reg_num;
	u32 i, j;
	int ret;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type isn't init. (acc_type=%u)\n", map_dev->acc_type);
		return -EINVAL;
	}

	if ((reg == NULL) || (num == 0)) {
		/* No memory addr. Map isn't needed */
		return 0;
	}

	for (i = 0; i < num; i++) {
		ret = ops->reg_map(map_dev->devid, &reg[i]);
		if (unlikely(ret != 0)) {
			uop_err("Reg map fail. (ret=%d; devid=%u; name=%s; num=%u; i=%u)\n",
				ret, map_dev->devid, reg->name, num, i);
			goto out;
		}
	}

	return 0;
out:
	for (j = 0; j < i; j++) {
		ops->reg_unmap(map_dev->devid, &reg[j]);
	}
	return ret;
}

static void _kmap_reg_unmap(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	struct kmap_addr *reg = map_dev->reg;
	u32 num = map_dev->reg_num;
	u32 i;

	if (unlikely(ops == NULL)) {
		return;
	}

	if ((reg == NULL) || (num == 0)) {
		return;
	}

	for (i = 0; i < num; i++) {
		ops->reg_unmap(map_dev->devid, &reg[i]);
	}
}

static int _kmap_addr_map(struct kmap_dev *map_dev)
{
	int ret;

	ret = _kmap_mem_map(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Mem map fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = _kmap_reg_map(map_dev);
	if (unlikely(ret != 0)) {
		_kmap_mem_unmap(map_dev);
		uop_err("Reg map fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = _kmap_acc_node_map(map_dev);
	if (unlikely(ret != 0)) {
		_kmap_reg_unmap(map_dev);
		_kmap_mem_unmap(map_dev);
		uop_err("Acc node map fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}
	return 0;
}

static void _kmap_addr_unmap(struct kmap_dev *map_dev)
{
	_kmap_acc_node_unmap(map_dev);
	_kmap_reg_unmap(map_dev);
	_kmap_mem_unmap(map_dev);
}

static int _kmap_item_mem_add(struct kmap_dev *map_dev)
{
	struct kmap_addr *mem = map_dev->mem;
	u32 num = map_dev->mem_num;
	u32 i;
	int ret;

	for (i = 0; ((i < num) && (mem != NULL)); i++) {
		struct uop_item item;
		(void)uop_item_pack(&item, mem[i].name, mem[i].asic_addr, mem[i].acc_addr, (u32)mem[i].size);
		ret = uop_kern_add_item_with_op_dev(map_dev, &item, UOP_ITEM_MEM);
		if (unlikely(ret != 0)) {
			uop_err("Add mem item fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
				ret, map_dev->devid, mem[i].die_id, mem[i].name);
			return ret;
		}
	}

	return 0;
}

static int _kmap_item_reg_add(struct kmap_dev *map_dev)
{
	struct kmap_addr *reg = map_dev->reg;
	u32 num = map_dev->reg_num;
	u32 i;
	int ret;

	for (i = 0; ((i < num) && (reg != NULL)); i++) {
		struct uop_item item;
		(void)uop_item_pack(&item, reg[i].name, reg[i].asic_addr, reg[i].acc_addr, (u32)reg[i].size);
		ret = uop_kern_add_item_with_op_dev(map_dev, &item, UOP_ITEM_REG);
		if (unlikely(ret != 0)) {
			uop_err("Add reg item fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
				ret, map_dev->devid, reg[i].die_id, reg[i].name);
			return ret;
		}
	}

	return 0;
}

static int _kmap_acc_node_add(struct kmap_dev *map_dev)
{
	if (map_dev->bare_item_num == 0 || map_dev->bare_item == NULL) {
		return 0;
	}

	if (map_dev->acc_node_num == 0 || map_dev->acc_node == NULL) {
		return 0;
	}
	return uop_kern_add_acc_node(map_dev);
}

static int _kmap_item_add(struct kmap_dev *map_dev)
{
	int ret;

	ret = _kmap_item_mem_add(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Mem item add fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = _kmap_item_reg_add(map_dev);
	if (unlikely(ret != 0)) {
		/* Do not need to remove items here. Items will be removed in uop_kern_reset */
		uop_err("Reg item add fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = _kmap_acc_node_add(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Acc node add fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
	}
	return ret;
}

static int _kmap_dev_init(struct kmap_dev *map_dev)
{
	int ret;

	ret = kmap_addr_init(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Kmap addr init fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		return ret;
	}

	ret = _kmap_addr_map(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Kmap addr map fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
		goto out;
	}

	ret = _kmap_item_add(map_dev);
	if (unlikely(ret != 0)) {
		_kmap_addr_unmap(map_dev);
		uop_err("Item add fail. (ret=%d; devid=%u)\n", ret, map_dev->devid);
	}
out:
	kmap_addr_uninit(map_dev);
	return ret;
}

int kmap_dev_init(u32 devid, enum uop_acc_type type, struct kmap_dev *map_dev)
{
	int ret;

	if (unlikely((devid >= UOP_MAX_DEVNUM) || (type >= UOP_ACC_MAX))) {
		uop_err("Invalid param. (devid=%u; type=%u)\n", devid, type);
		return -EINVAL;
	}

	ret = kmap_dev_assign(devid, type, map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Kmap dev create fail. (devid=%u; type=%u)\n", devid, type);
		return ret;
	}

	ret = _kmap_dev_init(map_dev);
	if (unlikely(ret != 0)) {
		uop_err("Kmap dev init fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}
	return 0;
}

static void kmap_unmap_all_acc_node(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	struct uop_acc_node *acc_node = NULL;
	const u32 num = UOP_MAX_ACC_NODE;
	u32 real_num  = 0;
	int ret;
	u32 i;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type invalid. (devid=%u; type=%u)\n", map_dev->devid, map_dev->acc_type);
		return;
	}

	if (ops->acc_node_unmap == NULL) {
		return;
	}

	acc_node = (struct uop_acc_node *)vmalloc(sizeof(struct uop_acc_node) * num);
	if (unlikely(acc_node == NULL)) {
		uop_err("Acc node alloc fail. (devid=%u; num=%u; size=%zu)\n", map_dev->devid, num, sizeof(struct uop_acc_node));
		return;
	}

	ret = uop_kern_get_acc_node(map_dev, acc_node, num, &real_num);
	if (unlikely((ret != 0) || (real_num > num))) {
		goto out;
	}

	for (i = 0; i < real_num; i++) {
		struct kmap_addr node = {.acc_addr = acc_node[i].acc_addr, .size = acc_node[i].len};

		ops->acc_node_unmap(map_dev->devid, &node);
	}
out:
	vfree(acc_node);
}

static void kmap_unmap_all_mem(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	const u32 num = KMAP_MAX_MEM_NUM;
	struct uop_item *item = NULL;
	u32 real_num  = 0;
	int ret;
	u32 i;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type invalid. (devid=%u; type=%u)\n", map_dev->devid, map_dev->acc_type);
		return;
	}

	item = (struct uop_item *)vmalloc(sizeof(struct uop_item) * num);
	if (unlikely(item == NULL)) {
		uop_err("Item list alloc fail. (devid=%u; num=%u; size=%zu)\n", map_dev->devid, num, sizeof(struct uop_item));
		return;
	}

	ret = uop_kern_get_items(map_dev, UOP_ITEM_MEM, item, num, &real_num);
	if (unlikely((ret != 0) || (real_num > num))) {
		goto out;
	}

	for (i = 0; i < real_num; i++) {
		struct kmap_addr mem = {.acc_addr = item[i].align_acc_addr, .size = item[i].len};

		ops->mem_unmap(map_dev->devid, &mem);
	}
out:
	vfree(item);
}

static void kmap_unmap_all_reg(struct kmap_dev *map_dev)
{
	const struct kmap_ops *ops = g_kmap_ops[map_dev->acc_type];
	const u32 num = KMAP_MAX_REG_NUM;
	struct uop_item *item = NULL;
	u32 real_num  = 0;
	int ret;
	u32 i;

	if (unlikely(ops == NULL)) {
		uop_err("Acc type invalid. (devid=%u; type=%u)\n", map_dev->devid, map_dev->acc_type);
		return;
	}

	item = (struct uop_item *)vmalloc(sizeof(struct uop_item) * num);
	if (unlikely(item == NULL)) {
		uop_err("Item list alloc fail. (devid=%u; num=%u; size=%zu)\n", map_dev->devid, num, sizeof(struct uop_item));
		return;
	}

	ret = uop_kern_get_items(map_dev, UOP_ITEM_REG, item, num, &real_num);
	if (unlikely((ret != 0) || (real_num > num))) {
		goto out;
	}

	for (i = 0; i < real_num; i++) {
		struct kmap_addr reg = {.acc_addr = item[i].align_acc_addr, .size = item[i].len};

		ops->reg_unmap(map_dev->devid, &reg);
	}
out:
	vfree(item);
}

static void kmap_unmap_all(struct kmap_dev *map_dev)
{
	kmap_unmap_all_acc_node(map_dev);
	kmap_unmap_all_mem(map_dev);
	kmap_unmap_all_reg(map_dev);
}

void kmap_dev_uninit(struct kmap_dev *map_dev)
{
	if (unlikely(map_dev == NULL)) {
		return;
	}
	kmap_unmap_all(map_dev);
}

int kmap_ops_register(enum uop_acc_type type, const struct kmap_ops *ops)
{
	if (unlikely((type >= UOP_ACC_MAX) || (ops == NULL))) {
		uop_err("Invalid param. (type=%u)\n", type);
		return -EINVAL;
	}

	if (unlikely(g_kmap_ops[type] != NULL)) {
		uop_err("Already registered. (type=%u)\n", type);
		return -EINVAL;
	}
	g_kmap_ops[type] = ops;

	return 0;
}

void kmap_ops_unregister(enum uop_acc_type type)
{
	if (likely(type < UOP_ACC_MAX)) {
		g_kmap_ops[type] = NULL;
	}
}
