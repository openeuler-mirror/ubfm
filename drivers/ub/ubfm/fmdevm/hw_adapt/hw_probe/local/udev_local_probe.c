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
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>

#include "fmdevm.h"
#include "ubfm_common.h"
#include "feature_loader.h"
#include "udev_ctrl.h"
#include "udev_hw_info.h"
#include "fmdevm.h"
#include "udev_map_inner.h"
#include "mng_devid.h"
#include "udev_vsock.h"
#if IS_ENABLED(CONFIG_HISI_CCA)
#include <asm/rmi_cmds.h>
#endif

#define UBFABRIC_ADDR_STR "ubfabric_addr="
#define ACPI_SIG_UBRT "UBRT"
#define UB_TABLE_HEADER_NAME_LEN 16

enum {
	UBC_TABLE = 0,
	UMMU_TABLE,
	UB_RESERVED_MEM_TABLE,
	VIRTUAL_BUS_TABLE,
	CALL_ID_SERVICE_TABLE,
	UB_ENTITY_TABLE,
	UB_TOPOLOGY_TABLE,
	NOTIFY_INFO_TABLE,
	UBFM_TABLE
};

struct ubrt_sub_table {
	u8 type;
	u8 reserved[7];
	u64 pointer;
};

struct acpi_table_ubrt {
	struct acpi_table_header head;
	u32 count;
	struct ubrt_sub_table sub_table[];
};

struct ub_table_header {
	char name[UB_TABLE_HEADER_NAME_LEN];
	u32 total_size;
	u8 version;
	u8 reserved[3];
	u32 remaining_size;
	u32 checksum;
};

struct ubfm_table {
	struct ub_table_header header;
	u64 ubfabric_addr;
	u64 size;
	u8 reserved[16];
};

static int parse_ubfm_table(u64 base_addr, u64 *ubfabric_addr)
{
	void __iomem *va = NULL;
	u32 total_size;
	struct ubfm_table ubfm_table = {0};

	va = ioremap(base_addr, sizeof(struct ubfm_table));
	if (!va || IS_ERR(va)) {
		hw_err("Failed to ioremap ubfm table.\n");
		return -EFAULT;
	}

	total_size = readl(va + UB_TABLE_HEADER_NAME_LEN);
	if (total_size != sizeof(struct ubfm_table)) {
		iounmap(va);
		hw_err("UBFM table size is not match. (get=%u; expect=%zu)\n", total_size, sizeof(struct ubfm_table));
		return -EINVAL;
	}

	memcpy_fromio(&ubfm_table, va, total_size);
	*ubfabric_addr = ubfm_table.ubfabric_addr;
	iounmap(va);
	hw_info("Succeed to get ubfabric_addr from ubfm table.\n");
	return 0;
}

static int get_ubfabric_addr_by_acpi(u64 *ubfabric_addr)
{
	int ret = -ENODEV;
	u32 i;
	acpi_status status;
	struct acpi_table_header *header = NULL;
	struct acpi_table_ubrt *ubrt_table = NULL;
	struct ubrt_sub_table *sub_table = NULL;

	status = acpi_get_table(ACPI_SIG_UBRT, 0, &header);
	if (ACPI_FAILURE(status)) {
		hw_warn("Can not get %s table. (status=%d)\n", ACPI_SIG_UBRT, status);
		return -ENODEV;
	}

	ubrt_table = (struct acpi_table_ubrt *)header;
	for (i = 0; i < ubrt_table->count; i++) {
		sub_table = &ubrt_table->sub_table[i];
		if (sub_table->type != UBFM_TABLE) {
			continue;
		}

		ret = parse_ubfm_table(sub_table->pointer, ubfabric_addr);
		break;
	}

#ifndef EMU_ST
	acpi_put_table(header);
#endif
	return ret;
}


static int get_ubfabric_addr_by_cmdline(u64 *ubfabric_addr)
{
	u64 base_addr = 0;
	char *buf = NULL, *end = NULL, *start_ptr = NULL;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		hw_err("Failed to allocate memory.\n");
		return -ENOMEM;
	}

	ret = read_file_content("/proc/cmdline", 0, buf, PAGE_SIZE);
	if (ret != 0) {
		kfree(buf);
		return ret;
	}

	start_ptr = strstr(buf, UBFABRIC_ADDR_STR);
	if (!start_ptr) {
		kfree(buf);
		hw_err("Ubfabric table base addr is not found in cmdline.\n");
		return -EADDRNOTAVAIL;
	}

	start_ptr += strlen(UBFABRIC_ADDR_STR);
	end = strchr(start_ptr, ' ');
	if (end) {
		*end = '\0';
	}

	ret = kstrtoull(start_ptr, 0, &base_addr);
	if (ret != 0) {
		hw_err("Failed to parse start_addr value\n");
		kfree(buf);
		return -EADDRNOTAVAIL;
	}

	*ubfabric_addr = base_addr;
	kfree(buf);
	hw_info("Succeed to get ubfabric_addr from cmdline.\n");
	return 0;
}

static int get_ubfabric_addr_by_vsock(u64 *ubfabric_addr)
{
#if IS_ENABLED(CONFIG_HISI_CCA)
	int ret;
	struct vsock_msg_pkt send_msg = {.cmd = VSOCK_CMD_GET_UBFABRIC_ADDR};
	struct vsock_msg_pkt recv_msg = {0};

	ret = vsock_client_init();
	if (ret != 0) {
		return ret;
	}

	ret = vsock_client_send_recv(&send_msg, &recv_msg);
	if (ret != 0 || recv_msg.cmd != VSOCK_CMD_GET_UBFABRIC_ADDR) {
		hw_err("get ubfabric addr by vsock failed, ret=%d, cmd=%d.\n", ret, recv_msg.cmd);
		vsock_client_exit();
		return -ENODATA;
	}

	*ubfabric_addr = recv_msg.u.get_ubfabric_addr_rsp.ubfabric_addr;
	hw_info("Succeed to get ubfabric_addr from vsock.\n");
	vsock_client_exit();
#endif
	return 0;
}

static int udev_local_get_hw_info(struct udev_hw_identify *hw)
{
	int ret;
	u64 resv_mem_base;

	if (udev_get_env_mode() == ENV_VSOCK_CLIENT) {
		ret = get_ubfabric_addr_by_vsock(&resv_mem_base);
	} else {
		ret = get_ubfabric_addr_by_acpi(&resv_mem_base);
		if (ret == -ENODEV) {
			ret = get_ubfabric_addr_by_cmdline(&resv_mem_base);
		}
	}

	if (ret != 0) {
		return ret;
	}

	ret = udev_get_hw_info(MAMI_ANY_DEVID, resv_mem_base, SIZE_KB(4), hw);	/* resv mem size is 4KB */
	if (ret != 0) {
		hw_err("Get hw info failed. (ret=%d)\n", ret);
		return ret;
	}

	if (hw->chip_type >= MAMI_KERN_TYPE_MAX || hw->chip_type <= MAMI_KERN_TYPE_INVALID) {
		hw_err("Chip type is invalid. (chip_type=%u)\n", hw->chip_type);
		return -EINVAL;
	}
	hw->access_type = UDEV_LDST;
	hw->location = UDM_LOCAL;
	hw->connect_type = UDEV_LOCAL;
	return 0;
}

int udev_local_dev_init(void)
{
	int ret;
	struct udev_hw_identify hw = {0};
	u32 is_local_ascend, devid;

	ret = udev_local_get_hw_info(&hw);
	if (ret != 0) {
		hw_err("Get hw info failed.\n");
		return ret;
	}

	if (udev_get_env_mode() == ENV_VSOCK_CLIENT) {
		hw.connect_type = UDEV_VM;
	}

	ret = udev_set_guid_dev(&hw);
	if (ret != 0) {
		hw_err("Set guid info failed.\n");
		return ret;
	}

	if (hw.chip_type == (u32)MAMI_KERN_TYPE_ASCEND_V1 || hw.chip_type == (u32)MAMI_KERN_TYPE_ASCEND_V2) {
		is_local_ascend = 1;
		ret = ures_set_attr(MAMI_IS_LOCAL_ASCEND, &is_local_ascend, sizeof(is_local_ascend));
		if (ret != 0) {
			hw_err("Set local device attr to ures failed. (ret=%d)\n", ret);
			return ret;
		}
	}

	ret = mng_alloc_devid(hw.slot_id, hw.socket_id, &devid);
	if (ret != 0 && ret != -EEXIST) {
		hw_err("Alloc devid failed. (slot_id=%u, socket_id=%u, ret=%d)\n", hw.slot_id, hw.socket_id, ret);
		return ret;
	}
	hw_info("Alloc devid. (slot_id=%u, socket_id=%u, devid=%u)\n", hw.slot_id, hw.socket_id, devid);

	return 0;
}

void udev_local_dev_uninit(void)
{
	ures_reset();
}

#if IS_ENABLED(CONFIG_HISI_CCA)
#define SEG_CNT_MAX 64
struct addr_map {
	u32 seg_cnt;
	struct addr_seg segs[SEG_CNT_MAX];
};

static struct addr_map g_addr_map = {0};
static u64 g_ubfabric_addr;

static int init_pm_addr_map(void);

static int check_addr_seg_overlap(u64 hpa_start, u32 size, u32 index)
{
	u32 i = 0;

	for (i = 0; i < index; i++) {
		if ((hpa_start <= (g_addr_map.segs[i].hpa_start + g_addr_map.segs[i].size - 1)) &&
			(g_addr_map.segs[i].hpa_start <= (hpa_start + size - 1))) {
			hw_err("addr seg overlap. (seg_a=%u; seg_b=%d)\n", i, index);
			return -EINVAL;
		}
	}

	return 0;
}

static int init_vm_addr_map(void)
{
	int ret;
	u32 i;
	struct vsock_msg_pkt send_msg = {.cmd = VSOCK_CMD_GET_HPA}, recv_msg = {0};

	ret = init_pm_addr_map();
	if (ret != 0) {
		return ret;
	}

	if (g_addr_map.seg_cnt == 0) {
		return -EINVAL;
	}

	ret = vsock_client_init();
	if (ret != 0) {
		return ret;
	}

	for (i = 0; i < g_addr_map.seg_cnt; i++) {
		send_msg.u.get_hpa_req.seg_index = i;
		ret = vsock_client_send_recv(&send_msg, &recv_msg);
		if (ret != 0 || recv_msg.cmd != VSOCK_CMD_GET_HPA) {
			hw_err("get hpa failed. (i=%u; ret=%d; cmd=%d)\n", i, ret, recv_msg.cmd);
			ret = -ENODATA;
			goto exit;
		}

		if (recv_msg.u.get_hpa_rsp.size != g_addr_map.segs[i].size) {
			hw_err("size is not matched. (i=%u; hpa=%u; gpa=%u)\n", i, recv_msg.u.get_hpa_rsp.size,
				g_addr_map.segs[i].size);
			ret = -EINVAL;
			goto exit;
		}

		ret = check_addr_seg_overlap(recv_msg.u.get_hpa_rsp.hpa, recv_msg.u.get_hpa_rsp.size, i);
		if (ret != 0) {
			goto exit;
		}

		g_addr_map.segs[i].gpa_start = g_addr_map.segs[i].hpa_start;  /* gpa store in hpa_start */
		g_addr_map.segs[i].hpa_start = recv_msg.u.get_hpa_rsp.hpa;
		g_addr_map.segs[i].is_share = recv_msg.u.get_hpa_rsp.is_share;
		ret = rsi_set_memory_range(g_addr_map.segs[i].gpa_start, g_addr_map.segs[i].gpa_start + g_addr_map.segs[i].size,
			g_addr_map.segs[i].is_share ? RSI_RIPAS_EMPTY : RSI_RIPAS_DEV, RSI_CHANGE_DESTROYED); /* share: empty, private: dev */
		if (ret != 0) {
			goto exit;
		}
	}

exit:
	vsock_client_exit();
	return ret;
}

static int init_pm_addr_map(void)
{
	u32 i = 0;
	struct acpi_device *adev = NULL;
	struct list_head resource_list;
	struct resource_entry *entry;

	INIT_LIST_HEAD(&resource_list);

	adev = acpi_dev_get_first_match_dev("HISI05A1", NULL, -1);
	if (!adev) {
		hw_err("ACPI device HISI05A1 not found\n");
		return -ENODEV;
	}

	acpi_dev_get_resources(adev, &resource_list, NULL, NULL);
	list_for_each_entry(entry, &resource_list, node) {
		if (resource_type(entry->res) == IORESOURCE_MEM) {
			g_addr_map.segs[i].hpa_start = entry->res->start;
			g_addr_map.segs[i].size = (u32)resource_size(entry->res);
			g_addr_map.segs[i].is_share = entry->res->flags & IORESOURCE_MEM_WRITEABLE; /* read_write:true, read_only:false */
			i++;
			if (i >= SEG_CNT_MAX) {
				acpi_dev_free_resource_list(&resource_list);
				acpi_dev_put(adev);
				return -EINVAL;
			}
		}
	}
	g_addr_map.seg_cnt = i;
	acpi_dev_free_resource_list(&resource_list);
	acpi_dev_put(adev);
	return 0;
}

static int hpa_to_gpa_list(u64 hpa, u32 len, struct addr_seg **seg, u32 *cnt)
{
	u32 i;
	u32 seg_num = 0;
	u64 req_start = hpa;
	u64 req_end = hpa + len;
	u64 seg_start, seg_end;
	u64 inter_start, inter_end;
	struct addr_seg *segs = NULL;

	segs = kcalloc(g_addr_map.seg_cnt, sizeof(struct addr_seg), GFP_KERNEL | __GFP_ACCOUNT);
	if (segs == NULL) {
		return -ENOMEM;
	}

	for (i = 0; i < g_addr_map.seg_cnt; i++) {
		seg_start = g_addr_map.segs[i].hpa_start;
		seg_end = g_addr_map.segs[i].hpa_start + g_addr_map.segs[i].size;

		inter_start = (req_start > seg_start) ? req_start : seg_start;
		inter_end = (req_end < seg_end) ? req_end : seg_end;

		if (inter_start < inter_end) {
			segs[seg_num].hpa_start = inter_start;
			segs[seg_num].gpa_start = g_addr_map.segs[i].gpa_start + (inter_start - seg_start);
			segs[seg_num].size = (u32)(inter_end - inter_start);
			segs[seg_num].is_share = g_addr_map.segs[i].is_share;
			seg_num++;
		}
	}

	if (seg_num == 0) {
		hw_err("Failed to get gpa list. (len=%u)\n", len);
		kfree(segs);
		return -ENOENT;
	}

	*seg = segs;
	*cnt = seg_num;
	return 0;
}

static int udev_vm_dev_init(void)
{
	int ret;

	ret = init_vm_addr_map();
	if (ret != 0) {
		hw_err("Failed to invoke init_vm_addr_map. (ret=%d)\n", ret);
		return ret;
	}

	ret = udev_register_map_func(hpa_to_gpa_list);
	if (ret != 0) {
		return ret;
	}

	return udev_local_dev_init();
}

static int vsock_server_hpa_handler(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp)
{
	if (req->cmd != VSOCK_CMD_GET_HPA || req->u.get_hpa_req.seg_index >= g_addr_map.seg_cnt) {
		return -EINVAL;
	}

	resp->cmd = VSOCK_CMD_GET_HPA;
	resp->u.get_hpa_rsp.hpa = g_addr_map.segs[req->u.get_hpa_req.seg_index].hpa_start;
	resp->u.get_hpa_rsp.size = g_addr_map.segs[req->u.get_hpa_req.seg_index].size;
	resp->u.get_hpa_rsp.is_share = g_addr_map.segs[req->u.get_hpa_req.seg_index].is_share;
	return 0;
}

static int vsock_server_ubfabric_addr_handler(const struct vsock_msg_pkt *req, struct vsock_msg_pkt *resp)
{
	if (req->cmd != VSOCK_CMD_GET_UBFABRIC_ADDR) {
		return -EINVAL;
	}

	resp->cmd = VSOCK_CMD_GET_UBFABRIC_ADDR;
	resp->u.get_ubfabric_addr_rsp.ubfabric_addr = g_ubfabric_addr;
	return 0;
}

static int get_hpa_info(void)
{
	int ret;

	ret = init_pm_addr_map();
	if (ret != 0) {
		return ret;
	}

	ret = get_ubfabric_addr_by_acpi(&g_ubfabric_addr);
	if (ret != 0) {
		return ret;
	}

	ret = vsock_register_server_cmd_handler(VSOCK_CMD_GET_HPA, vsock_server_hpa_handler);
	if (ret != 0) {
		return ret;
	}

	ret = vsock_register_server_cmd_handler(VSOCK_CMD_GET_UBFABRIC_ADDR, vsock_server_ubfabric_addr_handler);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int udev_local_probe_init(void)
{
	int env_mode;

	env_mode = udev_get_env_mode();
	if (env_mode == ENV_VSOCK_CLIENT) {
		return udev_vm_dev_init();
	} else if (env_mode == ENV_VSOCK_SERVER) {
		return get_hpa_info();
	} else {
		return udev_local_dev_init();
	}
}

static void udev_local_probe_uninit(void)
{
	int env_mode;

	env_mode = udev_get_env_mode();
	if (env_mode == ENV_VSOCK_SERVER) {
		return;
	}

	udev_local_dev_uninit();
}
#endif

#if IS_ENABLED(CONFIG_HISI_CCA)
DECLARE_FEATURE_AUTO_INIT(udev_local_probe_init, FEATURE_LOADER_STAGE_3);
DECLARE_FEATURE_AUTO_UNINIT(udev_local_probe_uninit, FEATURE_LOADER_STAGE_3);
#else
DECLARE_FEATURE_AUTO_INIT(udev_local_dev_init, FEATURE_LOADER_STAGE_3);
DECLARE_FEATURE_AUTO_UNINIT(udev_local_dev_uninit, FEATURE_LOADER_STAGE_3);
#endif
