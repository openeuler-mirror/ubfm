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
#include <linux/export.h>
#include <linux/gfp.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kernel.h>

#include "mami_string.h"
#include "ures.h"
#include "ures_log.h"

#include "ures_core.h"

#define URES_INVALID_IRQ	UINT_MAX
#define URES_IRQ_MAX_NUM	32U
#define URES_IRQ_TYPE_NUM   16U
#define URES_KEY_MAX_NUM	64U

#define PAGE_4K_ALIGNED(addr)		(((addr) & ((typeof(addr))(0x1000) - 1)) == 0)

struct ures_irq_info {
	u32 irq;
	u32 flag;
};

struct ures_node_irq {
	u32 irq_num;
	struct ures_irq_info *irq;
};

struct ures_node_mem {
	char name[URES_NAME_LEN];
	struct ures_addr_info mem;
	struct list_head list;
};

struct ures_node_reg {
	char name[URES_NAME_LEN];
	struct ures_addr_info reg;
	struct list_head list;
};

struct ures_node_key {
	char name[URES_NAME_LEN];
	u64 value;
	struct list_head list;
};

struct ures_node_attr {
	char name[URES_NAME_LEN];
	void *attr;
	size_t size;
	struct list_head list;
};

struct ures {
	struct ures_inst inst;

	struct rw_semaphore sem;

	u32 attr_num;
	struct list_head attr_head;

	u32 key_num;
	struct list_head key_head;

	u32 reg_num;
	struct list_head reg_head;

	u32 mem_num;
	struct list_head mem_head;

	struct ures_node_irq irq[URES_IRQ_TYPE_NUM];

	struct ures_win_info share_local_win[URES_WIN_TYPE_MAX];
};

static struct ures *g_die_ures[URES_MAX_DEVNUM][URES_MAX_DIE_NUM];
static struct ures *g_dev_ures[URES_MAX_DEVNUM];
static struct ures *g_ures;

static struct mutex ures_lock;

static inline int ures_inst_check(struct ures_inst *inst)
{
	if (unlikely(inst == NULL)) {
		return -EINVAL;
	}
	if (unlikely((inst->devid >= URES_MAX_DEVNUM) || (inst->die_id >= URES_MAX_DIE_NUM))) {
		return -EINVAL;
	}
	return 0;
}

static int ures_name_check(const char *name)
{
	size_t len;

	if (unlikely(name == NULL)) {
		ures_err("Name is NULL\n");
		return -EINVAL;
	}

	len = strnlen(name, URES_NAME_LEN);
	if (unlikely((len == 0) || (len >= URES_NAME_LEN))) {
		ures_err("Invalid name len. (len=%lu; max_len=%u)\n", len, URES_NAME_LEN);
		return -EINVAL;
	}

	return 0;
}

static struct ures *_ures_create(struct ures_inst *inst)
{
	struct ures *res = NULL;

	res = kvzalloc(sizeof(struct ures), GFP_KERNEL_ACCOUNT);
	if (unlikely(res == NULL)) {
		return NULL;
	}
	if (inst != NULL) {
		res->inst = *inst;
	}
	INIT_LIST_HEAD(&res->reg_head);
	INIT_LIST_HEAD(&res->mem_head);
	INIT_LIST_HEAD(&res->attr_head);
	INIT_LIST_HEAD(&res->key_head);
	init_rwsem(&res->sem);

	return res;
}

static inline void _ures_destroy(struct ures *res)
{
	kvfree(res);
}

static struct ures *ures_die_get(struct ures_inst *inst)
{
	struct ures *res = NULL;

	mutex_lock(&ures_lock);
	res = g_die_ures[inst->devid][inst->die_id];
	mutex_unlock(&ures_lock);

	return res;
}

static struct ures *ures_die_get_create(struct ures_inst *inst)
{
	struct ures *res = NULL;

	mutex_lock(&ures_lock);
	res = g_die_ures[inst->devid][inst->die_id];
	if (res != NULL) {
		mutex_unlock(&ures_lock);
		return res;
	}
	res = _ures_create(inst);
	g_die_ures[inst->devid][inst->die_id] = res;
	mutex_unlock(&ures_lock);

	return res;
}

static struct ures *ures_get(void)
{
	struct ures *res = NULL;

	mutex_lock(&ures_lock);
	res = g_ures;
	mutex_unlock(&ures_lock);

	return res;
}

static struct ures *ures_get_create(void)
{
	struct ures *res = NULL;
	struct ures_inst inst = {0};

	mutex_lock(&ures_lock);
	res = g_ures;
	if (res != NULL) {
		mutex_unlock(&ures_lock);
		return res;
	}
	res = _ures_create(&inst);
	g_ures = res;
	mutex_unlock(&ures_lock);

	return res;
}

static struct ures_node_mem *_ures_mem_create(const char *name, struct ures_addr_info *mem)
{
	struct ures_node_mem *res_mem = NULL;
	int ret;

	res_mem = kvzalloc(sizeof(struct ures_node_mem), GFP_KERNEL_ACCOUNT);
	if (unlikely(res_mem == NULL)) {
		ures_err("Create chip res mem fail. (name=%s; size=%lu)\n", name, sizeof(struct ures_node_mem));
		return NULL;
	}

	ret = ubfm_strcpy(res_mem->name, URES_NAME_LEN, name);
	if (unlikely(ret != 0)) {
		kvfree(res_mem);
		ures_err("Copy name fail. (ret=%d; name=%s)\n", ret, name);
		return NULL;
	}
	res_mem->mem.addr = mem->addr;
	res_mem->mem.size = mem->size;

	return res_mem;
}

static inline void _ures_mem_destroy(struct ures_node_mem *mem)
{
	kvfree(mem);
}

static int _ures_mem_add(struct ures *res, struct ures_node_mem *res_mem)
{
	if (unlikely(res->mem_num >= URES_MEM_MAX_NUM)) {
		return -ENOSPC;
	}

	list_add_tail(&res_mem->list, &res->mem_head);
	res->mem_num++;

	return 0;
}

static inline void _ures_mem_del(struct ures *res, struct ures_node_mem *mem)
{
	list_del(&mem->list);
	res->mem_num--;
}

static struct ures_node_mem *_ures_mem_find(struct ures *res, const char *name)
{
	struct ures_node_mem *mem = NULL;

	list_for_each_entry(mem, &res->mem_head, list) {
		if (strcmp(mem->name, name) == 0) {
			return mem;
		}
	}

	return NULL;
}

static int _ures_set_mem(struct ures *res, const char *name, struct ures_addr_info *mem)
{
	struct ures_node_mem *res_mem = NULL;
	int ret;

	res_mem = _ures_mem_find(res, name);
	if (unlikely(res_mem != NULL)) {
		ures_err("Mem res already exists. (devid=%u; die_id=%u; name=%s)\n",
			res->inst.devid, res->inst.die_id, name);
		return -EEXIST;
	}

	res_mem = _ures_mem_create(name, mem);
	if (unlikely(res_mem == NULL)) {
		ures_err("Create chip res mem fail. (devid=%u; die_id=%u; name=%s)\n",
			res->inst.devid, res->inst.die_id, name);
		return -ENOMEM;
	}

	ret = _ures_mem_add(res, res_mem);
	if (unlikely(ret != 0)) {
		_ures_mem_destroy(res_mem);
		ures_err("Mem add fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
			ret, res->inst.devid, res->inst.die_id, name);
		return ret;
	}

	return 0;
}

static void _ures_mem_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures_node_mem *mem = NULL;
	int i = 0;

	if (res->mem_num == 0) {
		return;
	}
	show(priv, "  Mem:\n");
	list_for_each_entry(mem, &res->mem_head, list) {
		show(priv, "    [%d]:name=%s;addr=0x%lx;size=%#x\n", i++, mem->name, mem->mem.addr, mem->mem.size);
	}
}

static struct ures_node_reg *_ures_reg_create(const char *name, struct ures_addr_info *reg)
{
	struct ures_node_reg *res_reg = NULL;
	int ret;

	res_reg = kvmalloc(sizeof(struct ures_node_reg), GFP_KERNEL_ACCOUNT);
	if (unlikely(res_reg == NULL)) {
		ures_err("Create reg node fail. (name=%s; size=%lu)\n", name, sizeof(struct ures_node_reg));
		return NULL;
	}

	ret = ubfm_strcpy(res_reg->name, URES_NAME_LEN, name);
	if (unlikely(ret != 0)) {
		kvfree(res_reg);
		ures_err("Name copy fail. (ret=%d; name=%s)\n", ret, name);
		return NULL;
	}
	res_reg->reg.addr = reg->addr;
	res_reg->reg.size = reg->size;

	return res_reg;
}

static inline void _ures_reg_destroy(struct ures_node_reg *reg)
{
	kvfree(reg);
}

static inline int _ures_reg_add(struct ures *res, struct ures_node_reg *reg)
{
	if (unlikely(res->reg_num >= URES_REG_MAX_NUM)) {
		return -ENOSPC;
	}

	list_add_tail(&reg->list, &res->reg_head);
	res->reg_num++;

	return 0;
}

static inline void _ures_reg_del(struct ures *res, struct ures_node_reg *reg)
{
	list_del(&reg->list);
	res->reg_num--;
}

static struct ures_node_reg *_ures_reg_find(struct ures *res, const char *name)
{
	struct ures_node_reg *reg = NULL;

	list_for_each_entry(reg, &res->reg_head, list) {
		if (strcmp(reg->name, name) == 0) {
			return reg;
		}
	}

	return NULL;
}

static int _ures_set_reg(struct ures *res, const char *name, struct ures_addr_info *reg)
{
	struct ures_node_reg *res_reg = NULL;
	int ret;

	res_reg = _ures_reg_find(res, name);
	if (unlikely(res_reg != NULL)) {
		ures_err("Reg res already exists. (devid=%u; die_id=%u; name=%s)\n",
			res->inst.devid, res->inst.die_id, name);
		return -EEXIST;
	}

	res_reg = _ures_reg_create(name, reg);
	if (unlikely(res_reg == NULL)) {
		ures_err("Create chip res mem fail. (devid=%u; die_id=%u; name=%s)\n",
			res->inst.devid, res->inst.die_id, name);
		return -ENOMEM;
	}

	ret = _ures_reg_add(res, res_reg);
	if (unlikely(ret != 0)) {
		_ures_reg_destroy(res_reg);
		ures_err("Reg add fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
			ret, res->inst.devid, res->inst.die_id, name);
	}

	return ret;
}

static void _ures_reg_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures_node_reg *reg = NULL;
	int i = 0;

	if (res->reg_num == 0) {
		return;
	}

	show(priv, "  Reg:\n");
	list_for_each_entry(reg, &res->reg_head, list) {
		show(priv, "    [%d]:name=%s;addr=0x%lx;size=%#x\n", i++, reg->name, reg->reg.addr, reg->reg.size);
	}
}

int ures_set_mem_base(struct ures_inst *inst, const char *name, struct ures_addr_info *mem)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return -EINVAL;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		return ret;
	}

	if (unlikely((mem == NULL) || !PAGE_4K_ALIGNED(mem->addr) || !PAGE_4K_ALIGNED(mem->size) || (mem->size == 0))) {
		return -EINVAL;
	}

	res = ures_die_get_create(inst);
	if (unlikely(res == NULL)) {
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_mem(res, name, mem);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Ures mem set fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
			ret, inst->devid, inst->die_id, name);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_mem_base);

static int _ures_get_mem_base(struct ures *res, const char *name, struct ures_addr_info *mem)
{
	struct ures_node_mem *res_mem = _ures_mem_find(res, name);

	if (unlikely(res_mem == NULL)) {
		return -EEXIST;
	}
	mem->addr = res_mem->mem.addr;
	mem->size = res_mem->mem.size;
	return 0;
}

int ures_get_mem_base(struct ures_inst *inst, const char *name, struct ures_addr_info *mem)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		return ret;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		return ret;
	}

	if (unlikely(mem == NULL)) {
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_mem_base(res, name, mem);
	up_read(&res->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_mem_base);

static struct ures_mem *_ures_create_mem_list(struct ures *res)
{
	struct ures_mem *mem_ = NULL;
	u32 mem_num = res->mem_num;

	mem_ = kvcalloc(mem_num, sizeof(struct ures_mem), GFP_KERNEL_ACCOUNT);
	if (unlikely(mem_ == NULL)) {
		ures_err("Mem array alloc fail. (devid=%u; die_id=%u; size=%lu; mem_num=%u)\n",
			res->inst.devid, res->inst.die_id, sizeof(struct ures_mem), mem_num);
		return NULL;
	}

	return mem_;
}

static void _ures_destroy_mem_list(struct ures_mem *mem)
{
	kvfree(mem);
}

static int _ures_mem_list_init(struct ures *res, struct ures_mem *mem, u32 num)
{
	struct ures_node_mem *res_mem = NULL;
	u32 i = 0;

	list_for_each_entry(res_mem, &res->mem_head, list) {
		int ret;

		if (unlikely(i >= num)) {
			ures_err("List num mismatch. (devid=%u; die_id=%u; mem_num=%u)\n",
				res->inst.devid, res->inst.die_id, num);
			return -EINVAL;
		}
		ret = ubfm_strncpy(mem[i].name, URES_NAME_LEN, res_mem->name, URES_NAME_LEN - 1);
		if (unlikely(ret != 0)) {
			ures_err("Mem name copy fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
				ret, res->inst.devid, res->inst.die_id, res_mem->name);
			return -ENOMEM;
		}
		mem[i].info.addr = res_mem->mem.addr;
		mem[i].info.size = res_mem->mem.size;
		i++;
	}

	return 0;
}

static int _ures_get_mem_list(struct ures *res, struct ures_mem **mem, u32 *num)
{
	struct ures_mem *mem_ = NULL;
	u32 mem_num = res->mem_num;
	int ret;

	if (mem_num == 0) {
		*mem = NULL;
		*num = 0;
		return 0;
	}

	mem_ = _ures_create_mem_list(res);
	if (unlikely(mem_ == NULL)) {
		ures_err("Mem array alloc fail. (devid=%u; die_id=%u; size=%lu; mem_num=%u)\n",
			res->inst.devid, res->inst.die_id, sizeof(struct ures_mem), mem_num);
		return -ENOMEM;
	}

	ret = _ures_mem_list_init(res, mem_, mem_num);
	if (unlikely(ret != 0)) {
		_ures_destroy_mem_list(mem_);
		ures_err("Ures mem list init fail. (devid=%u; die_id=%u; mem_num=%u)\n",
			res->inst.devid, res->inst.die_id, mem_num);
		return ret;
	}

	*mem = mem_;
	*num = mem_num;

	return 0;
}

int ures_get_mem_list(struct ures_inst *inst, struct ures_mem **mem, u32 *num)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((num == NULL) || (mem == NULL))) {
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_mem_list(res, mem, num);
	up_read(&res->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_mem_list);

void ures_put_mem_list(struct ures_mem *mem)
{
	if (likely(mem != NULL)) {
		kvfree(mem);
	}
}
EXPORT_SYMBOL_GPL(ures_put_mem_list);

static void ures_mem_del_all(struct ures *res)
{
	struct ures_node_mem *mem = NULL, *n = NULL;

	list_for_each_entry_safe(mem, n, &res->mem_head, list) {
		_ures_mem_del(res, mem);
		_ures_mem_destroy(mem);
	}
}

int ures_set_reg_base(struct ures_inst *inst, const char *name, struct ures_addr_info *reg)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		return ret;
	}

	if (unlikely((reg == NULL) || !PAGE_4K_ALIGNED(reg->addr) || !PAGE_4K_ALIGNED(reg->size) || (reg->size == 0))) {
		return -EINVAL;
	}

	res = ures_die_get_create(inst);
	if (unlikely(res == NULL)) {
		ures_err("Get chip res fail. (name=%s; devid=%u; die_id=%u)\n", name, inst->devid, inst->die_id);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_reg(res, name, reg);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Ures reg set fail. (ret=%d; name=%s; devid=%u; die_id=%u)\n",
			ret, name, inst->devid, inst->die_id);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_reg_base);

static int _ures_get_reg_base(struct ures *res, const char *name, struct ures_addr_info *reg)
{
	struct ures_node_reg *res_reg = _ures_reg_find(res, name);

	if (unlikely(res_reg == NULL)) {
		return -EEXIST;
	}
	reg->addr = res_reg->reg.addr;
	reg->size = res_reg->reg.size;

	return 0;
}

int ures_get_reg_base(struct ures_inst *inst, const char *name, struct ures_addr_info *reg)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
		return ret;
	}

	if (unlikely(reg == NULL)) {
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}
	down_read(&res->sem);
	ret = _ures_get_reg_base(res, name, reg);
	up_read(&res->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_reg_base);

static struct ures_reg *_ures_create_reg_list(struct ures *res)
{
	struct ures_reg *reg = NULL;
	u32 reg_num = res->reg_num;

	if (unlikely(reg_num == 0)) {
		ures_err("No reg list. (devid=%u; die_id=%u)\n", res->inst.devid, res->inst.die_id);
		return NULL;
	}

	reg = kvcalloc(reg_num, sizeof(struct ures_reg), GFP_KERNEL_ACCOUNT);
	if (unlikely(reg == NULL)) {
		ures_err("Reg array alloc fail. (devid=%u; die_id=%u; size=%lu; mem_num=%u)\n",
			res->inst.devid, res->inst.die_id, sizeof(struct ures_reg), reg_num);
		return NULL;
	}

	return reg;
}

static void _ures_destroy_reg_list(struct ures_reg *reg)
{
	kvfree(reg);
}

static int _ures_reg_list_init(struct ures *res, struct ures_reg *reg, u32 num)
{
	struct ures_node_reg *res_reg = NULL;
	u32 i = 0;
	int ret;

	list_for_each_entry(res_reg, &res->reg_head, list) {
		if (unlikely(i >= num)) {
			ures_err("List num mismatch. (devid=%u; die_id=%u; num=%u)\n",
				res->inst.devid, res->inst.die_id, num);
			return -EINVAL;
		}
		ret = ubfm_strncpy(reg[i].name, URES_NAME_LEN, res_reg->name, URES_NAME_LEN - 1);
		if (unlikely(ret != 0)) {
			ures_err("Reg name copy fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
				ret, res->inst.devid, res->inst.die_id, res_reg->name);
			return -ENOMEM;
		}
		reg[i].info.addr = res_reg->reg.addr;
		reg[i].info.size = res_reg->reg.size;
		i++;
	}

	return 0;
}

static int _ures_get_reg_list(struct ures *res, struct ures_reg **reg, u32 *num)
{
	struct ures_reg *reg_ = NULL;
	u32 reg_num = res->reg_num;
	int ret;

	if (reg_num == 0) {
		*reg = NULL;
		*num = 0;
		return 0;
	}

	reg_ = _ures_create_reg_list(res);
	if (unlikely(reg_ == NULL)) {
		ures_err("Reg array alloc fail. (devid=%u; die_id=%u; reg_num=%u)\n",
			res->inst.devid, res->inst.die_id, reg_num);
		return -ENOMEM;
	}

	ret = _ures_reg_list_init(res, reg_, reg_num);
	if (unlikely(ret != 0)) {
		_ures_destroy_reg_list(reg_);
		ures_err("Ures reg list init fail. (ret=%d; devid=%u; die_id=%u; reg_num=%u)\n",
			ret, res->inst.devid, res->inst.die_id, reg_num);
		return ret;
	}

	*reg = reg_;
	*num = reg_num;

	return 0;
}

int ures_get_reg_list(struct ures_inst *inst, struct ures_reg **reg, u32 *num)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_reg_list(res, reg, num);
	up_read(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Get reg list fail. (ret=%d; devid=%u; die_id=%u)\n", ret, inst->devid, inst->die_id);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_reg_list);

void ures_put_reg_list(struct ures_reg *reg)
{
	if (likely(reg != NULL)) {
		kvfree(reg);
	}
}
EXPORT_SYMBOL_GPL(ures_put_reg_list);

static void ures_reg_del_all(struct ures *res)
{
	struct ures_node_reg *reg = NULL, *n = NULL;

	list_for_each_entry_safe(reg, n, &res->reg_head, list) {
		_ures_reg_del(res, reg);
		_ures_reg_destroy(reg);
	}
}

static int _ures_irq_info_create(struct ures_node_irq *res_irq, u32 irq_num)
{
	struct ures_irq_info *irq = NULL;
	u32 index;

	if (unlikely((res_irq->irq_num != 0) || (res_irq->irq != NULL))) {
		ures_err("Irq num already set. (irq->irq_num=%u)\n", res_irq->irq_num);
		return -EINVAL;
	}

	irq = kvcalloc(irq_num, sizeof(struct ures_irq_info), GFP_KERNEL_ACCOUNT);
	if (unlikely(irq == NULL)) {
		ures_err("Irq array alloc fail. (size=%lu; irq_num=%u)\n", sizeof(struct ures_irq_info), irq_num);
		return -ENOMEM;
	}

	for (index = 0; index < irq_num; index++) {
		irq[index].irq = URES_INVALID_IRQ;
	}

	res_irq->irq = irq;
	return 0;
}

static void _ures_irq_info_destroy(struct ures_node_irq *res_irq)
{
	if (likely(res_irq->irq != NULL)) {
		kvfree(res_irq->irq);
		res_irq->irq = NULL;
	}
	res_irq->irq_num = 0;
}

static void ures_irq_del_all(struct ures *res)
{
	u32 irq_type;

	for (irq_type = 0; irq_type < URES_IRQ_TYPE_NUM; irq_type++) {
		_ures_irq_info_destroy(&res->irq[irq_type]);
	}
}

static int _ures_set_irq_num(struct ures *res, u32 irq_type, u32 irq_num)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];
	int ret;

	ret = _ures_irq_info_create(res_irq, irq_num);
	if (unlikely(ret != 0)) {
		ures_err("Irq info create fail. (ret=%d; devid=%u; die_id=%u; irq_type=%u; irq_num=%u)\n",
			ret, res->inst.devid, res->inst.die_id, irq_type, irq_num);
		return ret;
	}

	res_irq->irq_num = irq_num;

	return 0;
}

static int _ures_get_irq_num(struct ures *res, u32 irq_type, u32 *irq_num)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];

	if (unlikely((res_irq->irq == NULL) || (res_irq->irq_num == 0))) {
		ures_err("Irq num is not set. (devid=%u; die_id=%u; irq_type=%u; irq->irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, irq_type, res_irq->irq_num);
		return -EINVAL;
	}

	if (likely(irq_num != NULL)) {
		*irq_num = res_irq->irq_num;
	}

	return 0;
}

int ures_set_irq_num(struct ures_inst *inst, u32 irq_type, u32 irq_num)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM) || (irq_num == 0) || (irq_num >= URES_IRQ_MAX_NUM))) {
		ures_err("Invalid irq type or irq number. (irq_type=%u; max_irq_type=%u; irq_num=%u; max_irq_num=%u)\n",
			irq_type, URES_IRQ_TYPE_NUM, irq_num, URES_IRQ_MAX_NUM);
		return -EINVAL;
	}

	res = ures_die_get_create(inst);
	if (unlikely(res == NULL)) {
		ures_err("Get chip res fail. (devid=%u; die_id=%u)\n", inst->devid, inst->die_id);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_irq_num(res, irq_type, irq_num);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set irq num fail. (ret=%d; devid=%u; die_id=%u; irq_type=%u; irq_num=%u)\n",
			ret, inst->devid, inst->die_id, irq_type, irq_num);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_irq_num);

int ures_get_irq_num(struct ures_inst *inst, u32 irq_type, u32 *irq_num)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM))) {
		ures_err("Invalid irq type. (devid=%u; die_id=%u; irq_type=%u; max_irq_type=%u)\n",
			inst->devid, inst->die_id, irq_type, URES_IRQ_TYPE_NUM);
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_irq_num(res, irq_type, irq_num);
	up_read(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Get irq num fail. (ret=%d; devid=%u; die_ie=%u; irq_type=%u)\n",
			ret, inst->devid, inst->die_id, irq_type);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_irq_num);

static int _ures_set_irq_by_index(struct ures *res, u32 irq_type, u32 index, u32 irq)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];

	if (unlikely(index >= res_irq->irq_num)) {
		ures_err("Irq num is not set or invalid index. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}

	if (unlikely(res_irq->irq[index].irq != URES_INVALID_IRQ)) {
		ures_err("Irq already set by index. (devid=%u; die_id=%u; index=%u; irq=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq[index].irq);
		return -EINVAL;
	}
	res_irq->irq[index].irq = irq;

	return 0;
}

int ures_set_irq_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 irq)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM))) {
		ures_err("Invalid irq type. (devid=%u; die_id=%u; irq_type=%u; max_irq_type=%u)\n",
			inst->devid, inst->die_id, irq_type, URES_IRQ_TYPE_NUM);
		return -EINVAL;
	}

	res = ures_die_get_create(inst);
	if (unlikely(res == NULL)) {
		ures_err("Get ures fail. (devid=%u; die_id=%u)\n", inst->devid, inst->die_id);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_irq_by_index(res, irq_type, index, irq);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set irq by index fail. (ret=%d; devid=%u; die_id=%u; index=%u; irq=%u)\n",
			ret, inst->devid, inst->die_id, index, irq);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_irq_by_index);

static int _ures_get_irq_by_index(struct ures *res, u32 irq_type, u32 index, u32 *irq)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];

	if (unlikely((res_irq->irq == NULL) || (res_irq->irq_num == 0) || (index >= res_irq->irq_num))) {
		ures_err("Irq num is not set or invalid index. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}

	if (unlikely(res_irq->irq[index].irq == URES_INVALID_IRQ)) {
		ures_err("Irq is not set. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}
	*irq = res_irq->irq[index].irq;
	return 0;
}

int ures_get_irq_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 *irq)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM))) {
		ures_err("Invalid irq type. (devid=%u; die_id=%u; irq_type=%u; max_irq_type=%u)\n",
			inst->devid, inst->die_id, irq_type, URES_IRQ_TYPE_NUM);
		return -EINVAL;
	}

	if (unlikely(irq == NULL)) {
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_irq_by_index(res, irq_type, index, irq);
	up_read(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Get irq by index fail. (ret=%d; devid=%u; die_id=%u; irq_type=%u; index=%u)\n",
			ret, inst->devid, inst->die_id, irq_type, index);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_irq_by_index);

static int _ures_set_irq_flag_by_index(struct ures *res, u32 irq_type, u32 index, u32 flag)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];

	if (unlikely((res_irq->irq == NULL) || (res_irq->irq_num == 0) || (index >= res_irq->irq_num))) {
		ures_err("Irq num is not set or invalid index. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}

	res_irq->irq[index].flag |= flag;

	return 0;
}

int ures_set_irq_flag_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 flag)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM))) {
		ures_err("Invalid irq type. (devid=%u; die_id=%u; irq_type=%u; max_irq_type=%u)\n",
			inst->devid, inst->die_id, irq_type, URES_IRQ_TYPE_NUM);
		return -EINVAL;
	}

	res = ures_die_get_create(inst);
	if (unlikely(res == NULL)) {
		ures_err("Get ures res fail. (devid=%u; die_id=%u)\n", inst->devid, inst->die_id);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_irq_flag_by_index(res, irq_type, index, flag);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set irq flag by index fail. (ret=%d; devid=%u; die_id=%u; irq_type=%u; index=%u)\n",
			ret, inst->devid, inst->die_id, irq_type, index);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_irq_flag_by_index);

static int _ures_get_irq_flag_by_index(struct ures *res, u32 irq_type, u32 index, u32 *flag)
{
	struct ures_node_irq *res_irq = &res->irq[irq_type];

	if (unlikely((res_irq->irq == NULL) || (res_irq->irq_num == 0) || (index >= res_irq->irq_num))) {
		ures_err("Irq num is not set or invalid index. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}

	if (unlikely(res_irq->irq[index].irq == URES_INVALID_IRQ)) {
		ures_err("Irq is not set. (devid=%u; die_id=%u; index=%u; irq_num=%u)\n",
			res->inst.devid, res->inst.die_id, index, res_irq->irq_num);
		return -EINVAL;
	}

	*flag = res_irq->irq[index].flag;
	return 0;
}

int ures_get_irq_flag_by_index(struct ures_inst *inst, u32 irq_type, u32 index, u32 *flag)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Inst check fail. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((irq_type >= URES_IRQ_TYPE_NUM))) {
		ures_err("Invalid irq type. (devid=%u; die_id=%u; irq_type=%u; max_irq_type=%u)\n",
			inst->devid, inst->die_id, irq_type, URES_IRQ_TYPE_NUM);
		return -EINVAL;
	}

	if (unlikely(flag == NULL)) {
		return -EINVAL;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return -ENODEV;
	}

	down_read(&res->sem);
	ret = _ures_get_irq_flag_by_index(res, irq_type, index, flag);
	up_read(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Get irq flag by index fail. (ret=%d; devid=%u; die_id=%u; irq_type=%u; index=%u)\n",
			ret, inst->devid, inst->die_id, irq_type, index);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_irq_flag_by_index);

static void _ures_irq_show(struct ures_node_irq *res_irq, u32 irq_type,
	void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 index;

	if (res_irq->irq_num == 0) {
		return;
	}
	show(priv, "  Irq type[%u]:\n", irq_type);
	for (index = 0; index < res_irq->irq_num; index++) {
		show(priv, "    [%u]:irq=%u, flag=0x%x\n", index, res_irq->irq[index].irq, res_irq->irq[index].flag);
	}
}

static void ures_irq_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 irq_type;

	for (irq_type = 0; irq_type < URES_IRQ_TYPE_NUM; irq_type++) {
		struct ures_node_irq *res_irq = &res->irq[irq_type];

		_ures_irq_show(res_irq, irq_type, show, priv);
	}
}

static struct ures *ures_dev_get(u32 devid)
{
	struct ures *res = NULL;

	mutex_lock(&ures_lock);
	res = g_dev_ures[devid];
	mutex_unlock(&ures_lock);

	return res;
}

static struct ures *ures_dev_get_create(u32 devid)
{
	struct ures_inst inst = {.devid = devid};
	struct ures *res = NULL;

	mutex_lock(&ures_lock);
	res = g_dev_ures[devid];
	if (res != NULL) {
		mutex_unlock(&ures_lock);
		return res;
	}
	res = _ures_create(&inst);
	g_dev_ures[devid] = res;
	mutex_unlock(&ures_lock);

	return res;
}

static void _ures_share_local_win_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	u32 win_type;
	struct ures_win_info *res_win = NULL;

	for (win_type = 0; win_type < URES_WIN_TYPE_MAX; win_type++) {
		res_win = &res->share_local_win[win_type];
		if (res_win->win_num != 0) {
			show(priv, "  Share local win:\n");
			break;
		}
	}

	for (win_type = 0; win_type < URES_WIN_TYPE_MAX; win_type++) {
		res_win = &res->share_local_win[win_type];
		if (res_win->win_num == 0) {
			continue;
		}
		show(priv, "    [%u]: win_id_start=%u, win_num=%u, win_size=%#x\n", win_type,
			res_win->win_id_start, res_win->win_num, res_win->win_size);
	}
}

int ures_set_share_local_win(u32 devid, enum ures_win_type win_type, struct ures_win_info *win_info)
{
	struct ures *res = NULL;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (unlikely((win_type >= URES_WIN_TYPE_MAX))) {
		ures_err("Invalid win type. (devid=%u)\n", devid);
		return -EINVAL;
	}

	res = ures_dev_get_create(devid);
	if (unlikely(res == NULL)) {
		ures_err("Get create ures res fail. (devid=%u)\n", devid);
		return -ENOMEM;
	}

	down_write(&res->sem);
	res->share_local_win[win_type].win_id_start = win_info->win_id_start;
	res->share_local_win[win_type].win_num = win_info->win_num;
	res->share_local_win[win_type].win_size = win_info->win_size;
	up_write(&res->sem);

	return 0;
}
EXPORT_SYMBOL_GPL(ures_set_share_local_win);

int ures_get_share_local_win(u32 devid, enum ures_win_type win_type, struct ures_win_info *win_info)
{
	struct ures *res = NULL;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	if (unlikely((win_type >= URES_WIN_TYPE_MAX))) {
		ures_err("Invalid win type. (devid=%u)\n", devid);
		return -EINVAL;
	}

	res = ures_dev_get(devid);
	if (unlikely(res == NULL)) {
		return -ENOENT;
	}

	down_read(&res->sem);
	win_info->win_id_start = res->share_local_win[win_type].win_id_start;
	win_info->win_num = res->share_local_win[win_type].win_num;
	win_info->win_size = res->share_local_win[win_type].win_size;
	up_read(&res->sem);

	return 0;
}
EXPORT_SYMBOL_GPL(ures_get_share_local_win);

static struct ures_node_key *_ures_find_key(struct ures *res, const char *name)
{
	struct ures_node_key *key = NULL;

	list_for_each_entry(key, &res->key_head, list) {
		if (strcmp(key->name, name) == 0) {
			return key;
		}
	}

	return NULL;
}

static inline int _ures_key_add(struct ures *res, struct ures_node_key *key)
{
	if (unlikely(res->key_num >= URES_KEY_MAX_NUM)) {
		return -ENOSPC;
	}
	list_add_tail(&key->list, &res->key_head);
	res->key_num++;
	return 0;
}

static inline void _ures_key_del(struct ures *res, struct ures_node_key *key)
{
	list_del(&key->list);
	res->key_num--;
}

static struct ures_node_key *_ures_key_create(const char *name, u64 value)
{
	struct ures_node_key *key = NULL;
	int ret;

	key = kvzalloc(sizeof(struct ures_node_key), GFP_KERNEL_ACCOUNT);
	if (unlikely(key == NULL)) {
		ures_err("Create key res mem fail. (name=%s; size=%lu)\n", name, sizeof(struct ures_node_key));
		return NULL;
	}
	key->value = value;

	ret = ubfm_strcpy(key->name, URES_NAME_LEN, name);
	if (unlikely(ret != 0)) {
		ures_err("Name copy fail. (ret=%d; name=%s)\n", ret, name);
		kvfree(key);
		return NULL;
	}

	return key;
}

static void _ures_key_destroy(struct ures_node_key *key)
{
	if (likely(key != NULL)) {
		kvfree(key);
	}
}

static void _ures_key_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures_node_key *key = NULL;
	int i = 0;

	if (res->key_num == 0) {
		return;
	}

	show(priv, "  Key:\n");
	list_for_each_entry(key, &res->key_head, list) {
		show(priv, "    [%d]:name=%s;value=0x%llx\n", i++, key->name, key->value);
	}
}

static int _ures_set_key_val(struct ures *res, const char *name, u64 value)
{
	struct ures_node_key *key = NULL;
	int ret;

	key = _ures_find_key(res, name);
	if (unlikely(key != NULL)) {
		key->value = value;
		return 0;
	}

	key = _ures_key_create(name, value);
	if (unlikely(key == NULL)) {
		ures_err("Create res key fail. (devid=%u; name=%s)\n",
			res->inst.devid, name);
		return -ENOMEM;
	}

	ret = _ures_key_add(res, key);
	if (unlikely(ret != 0)) {
		ures_err("Ures key add fail. (devid=%u; name=%s)\n", res->inst.devid, name);
		_ures_key_destroy(key);
		return ret;
	}

	return ret;
}

int ures_set_key_value(u32 devid, const char *name, u64 value)
{
	struct ures *res = NULL;
	int ret;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u; max_devnum=%u)\n", devid, URES_MAX_DEVNUM);
		return -EINVAL;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	res = ures_dev_get_create(devid);
	if (unlikely(res == NULL)) {
		ures_err("Get ures res fail. (devid=%u; name=%s)\n", devid, name);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_key_val(res, name, value);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set key val fail. (ret=%d; devid=%u; name=%s)\n", ret, devid, name);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_key_value);

static int _ures_get_key_value(struct ures *res, const char *name, u64 *value)
{
	struct ures_node_key *key = _ures_find_key(res, name);

	if (unlikely(key == NULL)) {
		return -ENODATA;
	}
	*value = key->value;
	return 0;
}

int ures_get_key_value(u32 devid, const char *name, u64 *value)
{
	struct ures *res = NULL;
	int ret;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u; max_devnum=%u)\n", devid, URES_MAX_DEVNUM);
		return -EINVAL;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Check name fail. (ret=%d; devid=%u)\n", ret, devid);
		return ret;
	}

	if (unlikely(value == NULL)) {
		return -EINVAL;
	}

	res = ures_dev_get(devid);
	if (unlikely(res == NULL)) {
		return -EEXIST;
	}
	down_read(&res->sem);
	ret = _ures_get_key_value(res, name, value);
	up_read(&res->sem);

	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_key_value);

static void ures_key_del_all(struct ures *res)
{
	struct ures_node_key *key = NULL, *n = NULL;

	list_for_each_entry_safe(key, n, &res->key_head, list) {
		_ures_key_del(res, key);
		_ures_key_destroy(key);
	}
}

static int _ures_attr_add(struct ures *res, struct ures_node_attr *attr)
{
	if (unlikely(res->attr_num >= URES_ATTR_MAX_NUM)) {
		return -ENOSPC;
	}

	list_add_tail(&attr->list, &res->attr_head);
	res->attr_num++;
	return 0;
}

static void _ures_attr_del(struct ures *res, struct ures_node_attr *attr)
{
	list_del(&attr->list);
	res->attr_num--;
}

static struct ures_node_attr *_ures_attr_create(const char *name, void *attr, size_t size)
{
	struct ures_node_attr *res_attr = NULL;
	int ret;

	res_attr = kvzalloc(sizeof(struct ures_node_attr), GFP_KERNEL_ACCOUNT);
	if (unlikely(res_attr == NULL)) {
		ures_err("Create key res mem fail. (name=%s; size=%lu)\n", name, sizeof(struct ures_node_attr));
		return NULL;
	}

	res_attr->attr = kvmalloc(size, GFP_KERNEL_ACCOUNT);
	if (unlikely(res_attr->attr == NULL)) {
		ures_err("Attr alloc fail. (name=%s; size=%lu)\n", name, size);
		goto err_attr_alloc;
	}

	ret = ubfm_strcpy(res_attr->name, URES_NAME_LEN, name);
	if (unlikely(ret != 0)) {
		ures_err("Name copy fail. (ret=%d; name=%s; size=%lu)\n", ret, name, size);
		goto err_strcpy;
	}
	res_attr->size = size;
	(void)ubfm_memcpy(res_attr->attr, size, attr, size); /* Src size == Dest size, Don't mind return value */

	return res_attr;

err_strcpy:
	kvfree(res_attr->attr);
	res_attr->attr = NULL;
err_attr_alloc:
	kvfree(res_attr);
	return NULL;
}

static void _ures_attr_destroy(struct ures_node_attr *attr)
{
	if (likely(attr != NULL)) {
		if (likely(attr->attr != NULL)) {
			kvfree(attr->attr);
			attr->attr = NULL;
		}
		kvfree(attr);
	}
}

static struct ures_node_attr *_ures_attr_find(struct ures *res, const char *name)
{
	struct ures_node_attr *attr = NULL;

	list_for_each_entry(attr, &res->attr_head, list) {
		if (strcmp(attr->name, name) == 0) {
			return attr;
		}
	}
	return NULL;
}

static int _ures_set_attr(struct ures *res, const char *name, void *attr, size_t size)
{
	struct ures_node_attr *res_attr = NULL;
	int ret;

	res_attr = _ures_attr_find(res, name);
	if (unlikely(res_attr != NULL)) {
		ret = ubfm_strcpy(res_attr->name, URES_NAME_LEN, name);
		if (unlikely(ret != 0)) {
			ures_err("Name copy fail. (ret=%d; name=%s; size=%lu)\n", ret, name, size);
			return ret;
		}
		if (res_attr->size == size) {
			(void)ubfm_memcpy(res_attr->attr, size, attr, size); /* Src size == Dest size, Don't mind return value */
			return 0;
		}
		_ures_attr_del(res, res_attr);
		_ures_attr_destroy(res_attr);
	}

	res_attr = _ures_attr_create(name, attr, size);
	if (unlikely(res_attr == NULL)) {
		ures_err("Create res attr fail. (name=%s; size=%lu)\n", name, size);
		return -ENOMEM;
	}

	ret = _ures_attr_add(res, res_attr);
	if (unlikely(ret != 0)) {
		_ures_attr_destroy(res_attr);
		ures_err("Res attr add fail. (ret=%d; name=%s; size=%lu)\n", ret, name, size);
	}

	return ret;
}

int ures_dev_set_attr(u32 devid, const char *name, void *attr, size_t size)
{
	struct ures *res = NULL;
	int ret;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u)\n", devid);
		return -EINVAL;
	}

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Invalid name. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((attr == NULL) || (size == 0) || (size > URES_ATTR_MAX_SIZE))) {
		ures_err("Invalid size or attr is NULL. (devid=%u; name=%s; size=%lu)\n", devid, name, size);
		return -EINVAL;
	}

	res = ures_dev_get_create(devid);
	if (unlikely(res == NULL)) {
		ures_err("Get res fail. (devid=%u; name=%s; size=%lu)\n", devid, name, size);
		return -ENOMEM;
	}

	down_write(&res->sem);
	ret = _ures_set_attr(res, name, attr, size);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set attr fail. (ret=%d; devid=%u; name=%s; size=%lu)\n", ret, devid, name, size);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_dev_set_attr);

int ures_set_attr(const char *name, void *attr, size_t size)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Invalid name. (ret=%d)\n", ret);
		return ret;
	}

	if (unlikely((attr == NULL) || (size == 0) || (size > URES_ATTR_MAX_SIZE))) {
		ures_err("Invalid size or attr is NULL. (name=%s; size=%lu)\n", name, size);
		return -EINVAL;
	}

	res = ures_get_create();
	if (unlikely(res == NULL)) {
		ures_err("Get res fail. (name=%s; size=%lu)\n", name, size);
		return -ENOMEM;
	}
	down_write(&res->sem);
	ret = _ures_set_attr(res, name, attr, size);
	up_write(&res->sem);
	if (unlikely(ret != 0)) {
		ures_err("Set attr fail. (ret=%d; name=%s; size=%lu)\n", ret, name, size);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ures_set_attr);

static int _ures_get_attr(struct ures *res, const char *name, void *attr, size_t size)
{
	struct ures_node_attr *res_attr = NULL;

	res_attr = _ures_attr_find(res, name);
	if (unlikely(res_attr == NULL)) {
		return -EEXIST;
	}

	if (unlikely(res_attr->size != size)) {
		ures_err("Attr size invalid. (name=%s; attr->size=%lu; size=%lu)\n", name, res_attr->size, size);
		return -EINVAL;
	}

	/* Src size == Dest size, Don't mind return value */
	(void)ubfm_memcpy(attr, size, res_attr->attr, res_attr->size);
	return 0;
}

int ures_dev_get_attr(u32 devid, const char *name, void *attr, size_t size)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Invalid name.\n");
		return ret;
	}

	if (unlikely(attr == NULL)) {
		ures_err("Invalid attr. (name=%s; size=%lu)\n", name, size);
		return -EINVAL;
	}

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u; name=%s)\n", devid, name);
		return -EINVAL;
	}

	res = ures_dev_get(devid);
	if (unlikely(res == NULL)) {
		return -EEXIST;
	}

	down_read(&res->sem);
	ret = _ures_get_attr(res, name, attr, size);
	up_read(&res->sem);
	return ret;
}
EXPORT_SYMBOL_GPL(ures_dev_get_attr);

int ures_get_attr(const char *name, void *attr, size_t size)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Invalid name.\n");
		return ret;
	}

	if (unlikely(attr == NULL)) {
		ures_err("Invalid attr. (name=%s; size=%lu)\n", name, size);
		return -EINVAL;
	}

	res = ures_get();
	if (unlikely(res == NULL)) {
		return -EEXIST;
	}

	down_read(&res->sem);
	ret = _ures_get_attr(res, name, attr, size);
	up_read(&res->sem);
	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_attr);

static void _ures_del_attr(struct ures *res, const char *name)
{
	struct ures_node_attr *res_attr = NULL;

	res_attr = _ures_attr_find(res, name);
	if (unlikely(res_attr == NULL)) {
		return;
	}
	_ures_attr_del(res, res_attr);
	_ures_attr_destroy(res_attr);
}

int ures_del_attr(const char *name)
{
	struct ures *res = NULL;
	int ret = 0;

	ret = ures_name_check(name);
	if (unlikely(ret != 0)) {
		ures_err("Invalid name.\n");
		return ret;
	}

	res = ures_get();
	if (unlikely(res == NULL)) {
		return -EEXIST;
	}

	down_write(&res->sem);
	_ures_del_attr(res, name);
	up_write(&res->sem);
	return ret;
}
EXPORT_SYMBOL_GPL(ures_del_attr);

static struct ures_key *_ures_create_key_list(struct ures *res)	/* point array */
{
	u32 key_num = res->attr_num;
	struct ures_key *key_ = NULL;

	key_ = kvcalloc(key_num, sizeof(struct ures_key), GFP_KERNEL_ACCOUNT);
	if (unlikely(key_ == NULL)) {
		ures_err("Key array alloc fail. (devid=%u; die_id=%u; key_num=%u)\n",
			res->inst.devid, res->inst.die_id, key_num);
		return NULL;
	}
	return key_;
}

static void _ures_destroy_key_list(struct ures_key *key_)
{
	kvfree(key_);
}

static int _ures_key_list_init(struct ures *res, struct ures_key *key, u32 *num)
{
	struct ures_node_attr *attr = NULL;
	u32 attr_num = res->attr_num;
	int i = 0;
	int ret;

	list_for_each_entry(attr, &res->attr_head, list) {
		if (unlikely(i >= attr_num)) {
			ures_err("List num mismatch. (devid=%u; die_id=%u; num=%u)\n",
				res->inst.devid, res->inst.die_id, attr_num);
			return -EINVAL;
		}
		ret = ubfm_strcpy(key[i].name, URES_NAME_LEN, attr->name);
		if (unlikely(ret != 0)) {
			return ret;
		}
		i++;
	}
	*num = i;
	return 0;
}

int _ures_get_attr_key_list(struct ures *res, struct ures_key **key_list, u32 *num)
{
	int ret;

	if (res->attr_num == 0) {
		*num = 0;
		return 0;
	}

	*key_list = _ures_create_key_list(res);
	if (unlikely(*key_list == NULL)) {
		return -ENOMEM;
	}
	ret = _ures_key_list_init(res, *key_list, num);
	if (ret != 0) {
		_ures_destroy_key_list(*key_list);
		return ret;
	}
	return 0;
}

int ures_get_attr_key_list(struct ures_key **key, u32 *num)
{
	struct ures *res = NULL;
	int ret;

	if (unlikely(key == NULL || num == NULL)) {
		ures_err("Invalid attr key.\n");
		return -EINVAL;
	}

	res = ures_get_create();
	if (unlikely(res == NULL)) {
		return -EEXIST;
	}

	down_read(&res->sem);
	ret = _ures_get_attr_key_list(res, key, num);
	up_read(&res->sem);
	return ret;
}
EXPORT_SYMBOL_GPL(ures_get_attr_key_list);

void ures_put_attr_key_list(struct ures_key *key_list)
{
	if (likely(key_list != NULL)) {
		kvfree(key_list);
	}
}
EXPORT_SYMBOL_GPL(ures_put_attr_key_list);

static void _ures_attr_show(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures_node_attr *attr = NULL;
	int i = 0;

	if (res->attr_num == 0) {
		return;
	}

	show(priv, "  Attr:\n");
	list_for_each_entry(attr, &res->attr_head, list) {
		show(priv, "    [%d]:name=%s;size=%lu\n", i++, attr->name, attr->size);
	}
}

static void _ures_show_all(struct ures *res, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	down_read(&res->sem);
	_ures_mem_show(res, show, priv);
	_ures_reg_show(res, show, priv);
	ures_irq_show(res, show, priv);
	_ures_attr_show(res, show, priv);
	_ures_key_show(res, show, priv);
	_ures_share_local_win_show(res, show, priv);
	up_read(&res->sem);
}

void ures_die_res_show(struct ures_inst *inst, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures *res = NULL;
	int ret;

	if (unlikely(show == NULL)) {
		return;
	}

	ret = ures_inst_check(inst);
	if (unlikely(ret != 0)) {
		ures_err("Ures inst check fail. (ret=%d)\n", ret);
		return;
	}

	res = ures_die_get(inst);
	if (unlikely(res == NULL)) {
		return;
	}
	show(priv, "Die res:[devid=%u,die_id=%u]\n", inst->devid, inst->die_id);
	_ures_show_all(res, show, priv);
}

void ures_dev_res_show(u32 devid, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures *res = NULL;

	if (unlikely(devid >= URES_MAX_DEVNUM)) {
		ures_err("Invalid devid. (devid=%u; max_devnum=%u)\n", devid, URES_MAX_DEVNUM);
		return;
	}

	res = ures_dev_get(devid);
	if (unlikely(res == NULL)) {
		return;
	}

	show(priv, "Dev res: devid[%u]\n", res->inst.devid);
	_ures_show_all(res, show, priv);
}

void ures_show(void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	struct ures *res = NULL;

	if (show == NULL) {
		return;
	}

	res = ures_get();
	if (res == NULL) {
		return;
	}
	show(priv, "Res:\n");
	_ures_show_all(res, show, priv);
}

static void ures_attr_del_all(struct ures *res)
{
	struct ures_node_attr *attr = NULL, *n = NULL;

	list_for_each_entry_safe(attr, n, &res->attr_head, list) {
		_ures_attr_del(res, attr);
		_ures_attr_destroy(attr);
	}
}

static inline void _ures_del_all(struct ures *res)
{
	down_write(&res->sem);
	ures_key_del_all(res);
	ures_reg_del_all(res);
	ures_mem_del_all(res);
	ures_irq_del_all(res);
	ures_attr_del_all(res);
	up_write(&res->sem);
	(void)ubfm_memset(res->share_local_win, sizeof(res->share_local_win), 0, sizeof(res->share_local_win));
}

static void ures_die_destroy_all(void)
{
	u32 devid, die_id;

	for (devid = 0; devid < URES_MAX_DEVNUM; devid++) {
		for (die_id = 0; die_id < URES_MAX_DIE_NUM; die_id++) {
			mutex_lock(&ures_lock);
			if (g_die_ures[devid][die_id] != NULL) {
				_ures_del_all(g_die_ures[devid][die_id]);
				_ures_destroy(g_die_ures[devid][die_id]);
				g_die_ures[devid][die_id] = NULL;
			}
			mutex_unlock(&ures_lock);
		}
	}
}

static void ures_dev_destroy_all(void)
{
	u32 devid;

	for (devid = 0; devid < URES_MAX_DEVNUM; devid++) {
		mutex_lock(&ures_lock);
		if (g_dev_ures[devid] != NULL) {
			_ures_del_all(g_dev_ures[devid]);
			_ures_destroy(g_dev_ures[devid]);
			g_dev_ures[devid] = NULL;
		}
		mutex_unlock(&ures_lock);
	}
}

static void ures_destroy_all(void)
{
	mutex_lock(&ures_lock);
	if (g_ures != NULL) {
		_ures_del_all(g_ures);
		_ures_destroy(g_ures);
		g_ures = NULL;
	}
	mutex_unlock(&ures_lock);
}

static void ures_uninit_all(void)
{
	ures_die_destroy_all();
	ures_dev_destroy_all();
	ures_destroy_all();
}

void ures_die_reset(struct ures_inst *inst)
{
	struct ures *res = NULL;
	int ret;

	ret = ures_inst_check(inst);
	if (ret != 0) {
		ures_err("Invalid inst. (ret=%d)\n", ret);
		return;
	}

	res = ures_die_get(inst);
	if (res != NULL) {
		_ures_del_all(res);
	}
}
EXPORT_SYMBOL_GPL(ures_die_reset);

void ures_dev_reset(u32 devid)
{
	struct ures *res = NULL;

	if (devid >= URES_MAX_DEVNUM) {
		ures_err("Invalid devid. (devid=%u)\n", devid);
		return;
	}

	res = ures_dev_get(devid);
	if (res != NULL) {
		_ures_del_all(res);
	}
}
EXPORT_SYMBOL_GPL(ures_dev_reset);

void ures_reset(void)
{
	struct ures *res = NULL;

	res = ures_get();
	if (res != NULL) {
		_ures_del_all(res);
	}
}
EXPORT_SYMBOL_GPL(ures_reset);

void ures_init(void)
{
	mutex_init(&ures_lock);
}

void ures_uninit(void)
{
	ures_uninit_all();
	mutex_destroy(&ures_lock);
}

