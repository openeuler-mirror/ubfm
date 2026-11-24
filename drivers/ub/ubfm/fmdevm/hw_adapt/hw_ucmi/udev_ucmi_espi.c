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
#include <linux/vmalloc.h>
#include "ubfm_common.h"
#include "feature_loader.h"
#include "udev_ucmi_ops.h"
#include "ubfm_dfx_fs.h"
#include "udev_ucmi_espi.h"
#include "mami_string.h"


/* ESPI */
static struct ucmi_win_cfg g_ucmi_win_cfg = {0};

static struct ucmi_win_cfg *ucmi_get_win_cfg(void)
{
	return &g_ucmi_win_cfg;
}

STATIC struct udev_ucmi_info *ucmi_espi_info_get(u32 devid)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();

	return &cfg->info;
}

STATIC struct udev_ucmi_addr_info *ucmi_espi_find_addr_info(struct udev_ucmi_addr *addr)
{
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *addr_info = NULL;

	info = ucmi_espi_info_get(addr->devid);
	if (info == NULL) {
		hw_err("Get ucmi info failed. (devid=%u)\n", addr->devid);
		return NULL;
	}

	mutex_lock(&info->mtx);
	list_for_each_entry(addr_info, &info->addr_list, entry) {
		if ((strcmp(addr->name, addr_info->name) == 0) && (addr_info->dieid == addr->dieid)) {
			mutex_unlock(&info->mtx);
			return addr_info;
		}
	}
	mutex_unlock(&info->mtx);

	return NULL;
}

static void ucmi_espi_remove_addr_info(struct udev_ucmi_addr_info *addr_info)
{
	struct udev_ucmi_win *pos = NULL;
	struct udev_ucmi_win *tmp = NULL;

	list_for_each_entry_safe(pos, tmp, &addr_info->win_list, entry) {
		struct ucmi_win_node *node = (struct ucmi_win_node *)(uintptr_t)pos;

		list_del(&pos->entry);

		if (node->axi->count > 0) {
			node->axi->count--;
		} else {
			hw_err("Remove axi error. (axi_id=%u; win_id=%u; name=%s)\n", node->axi->axi_id,
				   node->win_id, node->win.name);
		}

		node->win.info = NULL;
	}
}

STATIC void ucmi_espi_remove_win(struct udev_ucmi_addr *addr)
{
	struct ucmi_win_cfg *cfg = NULL;
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	cfg = ucmi_get_win_cfg();
	mutex_lock(&cfg->mtx);
	info = &cfg->info;

	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		if ((strcmp(addr->name, pos->name) == 0) && (pos->dieid == addr->dieid)) {
			list_del(&pos->entry);
			ucmi_espi_remove_addr_info(pos);
			mutex_unlock(&info->mtx);
			mutex_unlock(&cfg->mtx);
			vfree(pos);
			return;
		}
	}
	mutex_unlock(&info->mtx);
	mutex_unlock(&cfg->mtx);
}

static void ucmi_espi_remove_all_win(struct udev_ucmi_info *info)
{
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		list_del(&pos->entry);
		ucmi_espi_remove_addr_info(pos);
		vfree(pos);
	}
	mutex_unlock(&info->mtx);
}

static int ucmi_espi_master_setup(void)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct udev_espi *espi = cfg->espi;

	if (espi == NULL) {
		hw_err("UCMI ESPI not init.\n");
		return -ENODATA;
	}

	mutex_lock(&cfg->mtx);

	udev_espi_writel(espi, UCMI_LOCK_OFFSET, UCMI_UNLOCK_VAL);
	udev_espi_writel(espi, UCMI_CTRL_OFFSET, 0x3C1);
	udev_espi_writel(espi, UCMI_MST_IDLE_CNT_OFFSET, 0x1F);
	udev_espi_writel(espi, UCMI_SCLK_OUT_DIV_OFFSET, 0x9);
	udev_espi_writel(espi, UCMI_MST_PIPE_EN_OFFSET, 0xF);
	udev_espi_writel(espi, UCMI_OT_GATE_OFFSET, 0xFFFFFFFF);
	udev_espi_writel(espi, UCMI_MST_TX_CFG_OFFSET, 0x0);
	udev_espi_writel(espi, UCMI_MST_RX_CFG_OFFSET, 0x65F);
	udev_espi_writel(espi, UCMI_FAULT_CLR_OFFSET, 0x1);
	udev_espi_writel(espi, UCMI_RAS_CE_EN_OFFSET, 0xFFFF);
	udev_espi_writel(espi, UCMI_RAS_NFE_EN_OFFSET, 0x0);
	udev_espi_writel(espi, UCMI_RAS_FE_EN_OFFSET, 0x0);
	udev_espi_writel(espi, UCMI_ABNOR_IMSK_OFFSET, 0x0);
	udev_espi_writel(espi, UCMI_LOCK_OFFSET, UCMI_LOCK_VAL);

	mutex_unlock(&cfg->mtx);
	return 0;
}

static int ucmi_espi_slave_setup(void)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct udev_espi *espi = cfg->espi;
	u32 axi_val;

	if (espi == NULL) {
		hw_err("UCMI ESPI not init.\n");
		return -ENODATA;
	}

	axi_val = readl(espi->remote_reg_base + UCMI_AXI_WIN_OFFSET(espi->remote_ucmi_axi));
	if (axi_val != UCMI_AXI_VALUE(espi->remote_regbank_base)) {
		hw_err("Remote UCMI has not init failed. (val=%#x; remote_reg=0x%llx)\n", axi_val,
			   espi->remote_regbank_base + UCMI_AXI_WIN_OFFSET(espi->remote_ucmi_axi));
		return -EINVAL;
	}

	return 0;
}

static void ucmi_master_win_cfg(struct udev_ucmi_addr *addr, struct ucmi_win_node *node, u32 win_type)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();

	udev_espi_writel(cfg->espi, UCMI_LOCK_OFFSET, UCMI_UNLOCK_VAL);

	if (win_type == UCMI_WIN_TYPE_4M) {
		udev_espi_writel(cfg->espi, UCMI_4M_WIN_OFFSET(node->win_id), node->win_val);
	} else if (win_type == UCMI_WIN_TYPE_16M) {
		udev_espi_writel(cfg->espi, UCMI_16M_WIN_OFFSET(node->win_id), node->win_val);
	}

	udev_espi_writel(cfg->espi, UCMI_LOCK_OFFSET, UCMI_LOCK_VAL);
}

static void ucmi_slave_axi_cfg(struct udev_ucmi_addr *addr, struct ucmi_axi_node *node)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();

	writel(UCMI_UNLOCK_VAL, cfg->espi->remote_reg_base + UCMI_LOCK_OFFSET);
	writel(node->base_addr, cfg->espi->remote_reg_base + UCMI_AXI_WIN_OFFSET(node->axi_id));
	writel(UCMI_LOCK_VAL, cfg->espi->remote_reg_base + UCMI_LOCK_OFFSET);
}

static int ucmi_espi_init(void *priv_data)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct udev_espi *espi = (struct udev_espi *)priv_data;

	if (espi == NULL) {
		hw_err("ESPI data is null.\n");
		return -EINVAL;
	}

	if (cfg->espi != NULL) {
		hw_info("UCMI already init.\n");
		return -EEXIST;
	}

	cfg->espi = espi;
	INIT_LIST_HEAD(&cfg->info.addr_list);
	mutex_init(&cfg->mtx);
	mutex_init(&cfg->info.mtx);

	return 0;
}

static void ucmi_espi_uninit(void *priv_data)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct udev_espi *espi = (struct udev_espi *)priv_data;

	if (espi == NULL) {
		hw_err("ESPI data is null.\n");
		return;
	}

	mutex_lock(&cfg->mtx);
	ucmi_espi_remove_all_win(&cfg->info);
	mutex_unlock(&cfg->mtx);
	mutex_destroy(&cfg->info.mtx);
	mutex_destroy(&cfg->mtx);
	(void)ubfm_memset(cfg, sizeof(struct ucmi_win_cfg), 0, sizeof(struct ucmi_win_cfg));
}

struct ucmi_axi_node *ucmi_assign_axi(struct udev_ucmi_addr *addr)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct ucmi_axi_node *axi_node = NULL;
	u32 i;

	for (i = 0; i < UCMI_SLAVE_AXI_NUM; i++) {
		axi_node = &cfg->axi[i];
		if (axi_node->count > 0) {
			if (axi_node->base_addr == UCMI_AXI_VALUE(addr->chip_addr)) {
				axi_node->count++;
				return axi_node;
			}

			continue;
		}

		axi_node->base_addr = UCMI_AXI_VALUE(addr->chip_addr);
		axi_node->axi_id = i;

		/* configure the slave axi high-bit register */
		ucmi_slave_axi_cfg(addr, axi_node);
		axi_node->count++;
		return axi_node;
	}

	return NULL;
}

static void ucmi_espi_remove_axi(struct ucmi_axi_node *axi_node)
{
	/* the axi_node table is an array, here only need to clear the information of this node */
	(void)ubfm_memset(axi_node, sizeof(struct ucmi_axi_node), 0, sizeof(struct ucmi_axi_node));
}

static inline u32 ucmi_get_win_max_num(u32 win_type)
{
	return (win_type == UCMI_WIN_TYPE_4M) ? UCMI_4M_WIN_NUM : UCMI_16M_WIN_NUM;
}

static inline struct ucmi_win_node *ucmi_get_win_table(struct udev_ucmi_addr *addr, u32 win_type)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();

	return (win_type == UCMI_WIN_TYPE_4M) ?  &cfg->win_4m[0] : &cfg->win_16m[0];
}

static inline u32 ucmi_get_win_val(u32 axi_id, u64 chip_addr, u32 win_type)
{
	return (win_type == UCMI_WIN_TYPE_4M) ? REMAP_4M_VAL(axi_id, chip_addr) : REMAP_16M_VAL(axi_id, chip_addr);
}

static inline u64 ucmi_get_ucmi_offset(u32 win_id, u32 dieid, u64 chip_addr, u32 win_type)
{
	return (win_type == UCMI_WIN_TYPE_4M) ? UCMI_ACC_ADDR_4M(win_id, dieid, chip_addr) :
		UCMI_ACC_ADDR_16M(win_id, dieid, chip_addr);
}

static int ucmi_espi_do_assign_win(u32 idx, u32 win_type, struct udev_ucmi_addr *addr,
								   struct ucmi_win_node **win_node, struct ucmi_axi_node *axi_node)
{
	struct ucmi_win_node *table = ucmi_get_win_table(addr, win_type);
	struct ucmi_win_node *node = NULL;
	u32 i;
	int ret;

	for (i = 0; i < ucmi_get_win_max_num(win_type); i++) {
		node = &table[i];
		if (node->win.info != NULL) {
			continue;
		}

		ret = UBFM_SPRINTF(&node->win.name[0], sizeof(node->win.name), "%s_win%u", addr->name, idx);
		if (ret <= 0) {
			hw_err("Format win name failed. (devid=%u; idx=%u; name=%s; size=%u)\n",
					addr->devid, idx, addr->name, addr->size);
			return -EINVAL;
		}

		if (axi_node != NULL) {
			node->axi = axi_node;
			axi_node->count++;
		} else {
			/* allocate an axi window */
			node->axi = ucmi_assign_axi(addr);
			if (node->axi == NULL) {
				hw_err("UCMI alloc axi failed. (devid=%u; idx=%u; name=%s; size=%u)\n",
					addr->devid, idx, addr->name, addr->size);
				return -ENOSPC;
			}
		}

		node->win.chip_addr = addr->chip_addr;
		node->win.size = addr->size;
		node->die_id = addr->dieid;
		node->win_id = i;
		node->win_type = win_type;
		node->win_val = ucmi_get_win_val(node->axi->axi_id, addr->chip_addr, win_type);
		node->win.ucmi_base = UCMI_BASE;
		node->win.ucmi_off = ucmi_get_ucmi_offset(i, addr->dieid, addr->chip_addr, win_type);

		/* configure the master window register */
		ucmi_master_win_cfg(addr, node, win_type);
		*win_node = node;

		return 0;
	}

	return -ENOSPC;
}

static int _ucmi_espi_assign_win(struct udev_ucmi_addr *addr, struct ucmi_axi_node *axi_node,
								 struct udev_ucmi_addr_info *addr_info)
{
	struct ucmi_win_node *node = NULL;
	struct udev_ucmi_addr tmp_addr;
	u32 total_size, win_size, win_left_size, win_type, idx;
	u64 chip_addr, next_win_addr;
	int ret;

	chip_addr = addr->chip_addr;
	total_size = addr->size;
	idx = 0;
	while (total_size > 0) {
		if (total_size <= UCMI_WIN_SIZE_4M) {
			next_win_addr = (chip_addr + UCMI_WIN_SIZE_4M) & (~UCMI_WIN_SIZE_4M_MASK);
			win_type = UCMI_WIN_TYPE_4M;
		} else {
			next_win_addr = (chip_addr + UCMI_WIN_SIZE_16M) & (~UCMI_WIN_SIZE_16M_MASK);
			win_type = UCMI_WIN_TYPE_16M;
		}
		win_left_size = (u32)(next_win_addr - chip_addr);
		win_size = win_left_size > total_size ? total_size : win_left_size;

		(void)ubfm_memcpy(&tmp_addr, sizeof(struct udev_ucmi_addr), addr, sizeof(struct udev_ucmi_addr));
		tmp_addr.size = win_size;
		tmp_addr.chip_addr = chip_addr;

		ret = ucmi_espi_do_assign_win(idx, win_type, &tmp_addr, &node, axi_node);
		if (ret != 0) {
			hw_err("Do assign win failed. (devid=%u; dieid=%u; name=%s; size=%u; idx=%u; ret=%d)\n",
				   addr->devid, addr->dieid, addr->name, addr->size, idx, ret);
			ucmi_espi_remove_addr_info(addr_info);
			return ret;
		}

		node->win.info = addr_info;
		list_add_tail(&node->win.entry, &addr_info->win_list);
		chip_addr += (u64)win_size;
		total_size -= win_size;
		idx++;
	}

	return 0;
}

static int _ucmi_espi_assign_win_by_axi(struct ucmi_win_cfg *cfg, struct udev_ucmi_addr *addr,
										struct ucmi_axi_node *axi_node)
{
	struct udev_ucmi_addr_info *addr_info = NULL;
	int ret = 0;

	addr_info = ucmi_espi_find_addr_info(addr);
	if (addr_info != NULL) {
		hw_err("Addr info already exists. (devid=%u; name=%s; size=0x%x)\n",
			   addr->devid, addr->name, addr->size);
		return -EEXIST;
	}

	addr_info = vzalloc(sizeof(struct udev_ucmi_addr_info));
	if (addr_info == NULL) {
		hw_err("Alloc addr info failed. (devid=%u; name=%s; size=0x%x)\n",
			   addr->devid, addr->name, addr->size);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&addr_info->win_list);
	ret = UBFM_SPRINTF(&addr_info->name[0], sizeof(addr_info->name), "%s", addr->name);
	if (ret <= 0) {
		hw_err("Format addr info name failed. (ret=%d; devid=%u; name=%s; size=0x%x)\n",
			   ret, addr->devid, addr->name, addr->size);
		vfree(addr_info);
		return -EINVAL;
	}

	addr_info->chip_addr = addr->chip_addr;
	addr_info->size = addr->size;
	addr_info->dieid = addr->dieid;

	ret = _ucmi_espi_assign_win(addr, axi_node, addr_info);
	if (ret != 0) {
		hw_err("Assign win failed. (ret=%d; devid=%u; name=%s; size=0x%x)\n",
			   ret, addr->devid, addr->name, addr->size);
		vfree(addr_info);
		return ret;
	}

	mutex_lock(&cfg->info.mtx);
	list_add_tail(&addr_info->entry, &cfg->info.addr_list);
	mutex_unlock(&cfg->info.mtx);

	return 0;
}

static int ucmi_espi_assign_win(struct udev_ucmi_addr *addr)
{
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	int ret;

	mutex_lock(&cfg->mtx);
	ret = _ucmi_espi_assign_win_by_axi(cfg, addr, NULL);
	mutex_unlock(&cfg->mtx);
	return ret;
}

static int ucmi_espi_assign_win_by_axi(u32 axi_id, struct udev_ucmi_addr *addr)
{
	struct ucmi_axi_node *axi_node = NULL;
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	int ret;

	if (axi_id >= UCMI_SLAVE_AXI_NUM) {
		hw_err("Invalid axi_id. (devid=%u; name=%s; axi_id=%u)\n", addr->devid, addr->name, axi_id);
		return -EINVAL;
	}

	mutex_lock(&cfg->mtx);
	axi_node = &cfg->axi[axi_id];

	if (axi_node->count > 0) {
		if ((axi_node->base_addr != UCMI_AXI_VALUE(addr->chip_addr)) || (axi_node->axi_id != axi_id)) {
			hw_err("axi_node already in use. (devid=%u; name=%s; axi_id=%u; axi_node_id=%u; axi_count=%u)\n",
				addr->devid, addr->name, axi_id, axi_node->axi_id, axi_node->count);
			mutex_unlock(&cfg->mtx);
			return -EINVAL;
		}
	} else {
		axi_node->base_addr = UCMI_AXI_VALUE(addr->chip_addr);
		axi_node->axi_id = axi_id;
	}

	ret = _ucmi_espi_assign_win_by_axi(cfg, addr, axi_node);
	if ((ret != 0) && (axi_node->count == 0)) {
		ucmi_espi_remove_axi(axi_node);
	}
	mutex_unlock(&cfg->mtx);

	return ret;
}

static void ucmi_espi_remove_all_dynamic_win_fn(u32 devid)
{
	struct ucmi_win_cfg *cfg = NULL;
	struct udev_ucmi_info *info = NULL;
	struct udev_ucmi_addr_info *pos = NULL;
	struct udev_ucmi_addr_info *tmp = NULL;

	cfg = ucmi_get_win_cfg();
	mutex_lock(&cfg->mtx);
	info = &cfg->info;
	mutex_lock(&info->mtx);
	list_for_each_entry_safe(pos, tmp, &info->addr_list, entry) {
		if (strcmp(pos->name, UCMI_WIN_NAME) == 0 || strcmp(pos->name, DEV_INFO_WIN_NAME) == 0) {
			continue;
		}

		list_del(&pos->entry);
		hw_info("Debug: remove win(devid=%u; name=%s)\n", devid, pos->name);
		ucmi_espi_remove_addr_info(pos);
		vfree(pos);
	}
	mutex_unlock(&info->mtx);
	mutex_unlock(&cfg->mtx);
}

DEFINE_MUTEX(g_espi_lock); /* lock protecting the two global variables below */
static u32 g_espi_win_type;
static u32 g_espi_last_item_idx;

STATIC ssize_t espi_info_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const char *p = buf;

	mutex_lock(&g_espi_lock);
	SIMPLE_STRTOUL_SAFE(p, g_espi_win_type, 0);
	g_espi_last_item_idx = 0;

	hw_info("Set win_type=%u\n", g_espi_win_type);
	mutex_unlock(&g_espi_lock);
	return (ssize_t)count;
}

STATIC ssize_t espi_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const int max_items_print_once = 50;	 /* display up to 50 items at a time */
	struct ucmi_win_cfg *cfg = ucmi_get_win_cfg();
	struct ucmi_win_node *win_node = NULL;
	ssize_t count = 0;
	int ret, print_count = 0;
	u32 win_type, last_item_idx;

	mutex_lock(&g_espi_lock);
	win_type = g_espi_win_type;
	last_item_idx = g_espi_last_item_idx;
	mutex_unlock(&g_espi_lock);

	ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "win_id\twin_type\tdie_id\treg_offset\treg_val\tname\t"
					"chip_addr\taxi_id\taxi_val\taxi_count\tucmi_offset\n");
	if (ret > 0) {
		count += (ssize_t)ret;
	}

	mutex_lock(&cfg->mtx);
	while (print_count < max_items_print_once) {
		if (last_item_idx >= ucmi_get_win_max_num(win_type)) {
			last_item_idx = 0;
			break;
		}

		if (win_type == UCMI_WIN_TYPE_16M) {
			win_node = &cfg->win_16m[last_item_idx++];
		} else {
			win_node = &cfg->win_4m[last_item_idx++];
		}

		if (win_node->axi == NULL) {
			continue;
		}

		print_count++;
		ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "%u\t%s\t%u\t0x%x\t0x%x\t%s\t0x%llx\t%u\t"
						"0x%llx\t%u\t0x%llx\n",
					win_node->win_id, (win_type == UCMI_WIN_TYPE_16M) ? "16M" : "4M", win_node->die_id,
					(win_type == UCMI_WIN_TYPE_16M) ? UCMI_16M_WIN_OFFSET(win_node->win_id) :
					UCMI_4M_WIN_OFFSET(win_node->win_id), win_node->win_val, win_node->win.name,
					win_node->win.chip_addr, win_node->axi->axi_id, win_node->axi->base_addr,
					win_node->axi->count, win_node->win.ucmi_off);
		if (ret > 0) {
			count += (ssize_t)ret;
		}
	}
	mutex_unlock(&cfg->mtx);

	if (last_item_idx != 0) {
		ret = UBFM_SPRINTF(buf + count, PAGE_SIZE - count, "more ...\n");
		if (ret > 0) {
			count += (ssize_t)ret;
		}
	}

	mutex_lock(&g_espi_lock);
	if (win_type == g_espi_win_type) {
		g_espi_last_item_idx = last_item_idx;
	}
	mutex_unlock(&g_espi_lock);

	return count;
}

static struct udev_ucmi_ops g_ucmi_espi_ops = {
	.init = ucmi_espi_init,
	.uninit = ucmi_espi_uninit,
	.master_setup = ucmi_espi_master_setup,
	.slave_setup = ucmi_espi_slave_setup,
	.assign_win = ucmi_espi_assign_win,
	.assign_win_by_axi = ucmi_espi_assign_win_by_axi,
	.remove_win = ucmi_espi_remove_win,
	.info_get = ucmi_espi_info_get,
	.info_put = NULL,
	.remove_all_dynamic_win = ucmi_espi_remove_all_dynamic_win_fn,
	.get_win_mode = NULL,
};

int ucmi_espi_ops_init(void)
{
	int ret;

	ret = udev_ucmi_ops_register(UDEV_ESPI, &g_ucmi_espi_ops);
	if (ret != 0) {
		hw_err("Register espi ucmi ops failed. (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}
DECLARE_FEATURE_AUTO_INIT(ucmi_espi_ops_init, FEATURE_LOADER_STAGE_2);

void ucmi_espi_ops_uninit(void)
{
	udev_ucmi_ops_unregister(UDEV_ESPI);
}
DECLARE_FEATURE_AUTO_UNINIT(ucmi_espi_ops_uninit, FEATURE_LOADER_STAGE_2);

static DEVICE_ATTR(info, UBFM_FS_MODE_OWNER_RW, espi_info_show, espi_info_store);

static struct attribute *g_espi_sysfs_attrs[] = {
	&dev_attr_info.attr,
	NULL
};

static struct attribute_group g_espi_attr_grp = {
	.name = "espi",
	.attrs = g_espi_sysfs_attrs,
};

STATIC int espi_sysfs_init(void *priv)
{
	struct platform_device *pdev = NULL;
	int ret;

	if (unlikely(priv == NULL)) {
		hw_err("Invalid param.\n");
		return -EINVAL;
	}
	pdev = (struct platform_device *)priv;
	ret = sysfs_create_group(&pdev->dev.kobj, &g_espi_attr_grp);
	if (unlikely(ret != 0)) {
		hw_err("Create group failed. (ret=%d)\n", ret);
	}

	return ret;
}
DECLARE_FEATURE_AUTO_INIT_BY_SCOPE(sysfs, espi_sysfs_init, FEATURE_LOADER_STAGE_9);

STATIC void espi_sysfs_uninit(void *priv)
{
	struct platform_device *pdev = NULL;

	if (unlikely(priv == NULL)) {
		hw_err("Invalid param.\n");
		return;
	}
	pdev = (struct platform_device *)priv;
	sysfs_remove_group(&pdev->dev.kobj, &g_espi_attr_grp);
}
DECLARE_FEATURE_AUTO_UNINIT_BY_SCOPE(sysfs, espi_sysfs_uninit, FEATURE_LOADER_STAGE_9);
