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
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>

#include "ubfm_common.h"
#include "usi_log.h"
#include "usi_acpi.h"
#include "usi_ubus_api.h"

typedef acpi_status(*iort_match_node_callback_fn)(struct acpi_iort_node *node, struct ub_entity *uent,
												   struct device *dev);

STATIC acpi_status usi_iort_match_node_callback(struct acpi_iort_node *node, struct ub_entity *uent,
												struct device *dev)
{
	acpi_status status = AE_NOT_FOUND;
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	struct acpi_device *adev = NULL;
	struct acpi_iort_named_component *comp = NULL;
	struct device *tmp_dev = dev;

	if (node->type != ACPI_IORT_NODE_NAMED_COMPONENT) {
		return status;
	}

	do {
		adev = ACPI_COMPANION(tmp_dev);
		if (adev != NULL) {
			break;
		}

		tmp_dev = tmp_dev->parent;
	} while (tmp_dev != NULL);

	if (adev == NULL) {
		goto out;
	}

	status = acpi_get_name(adev->handle, ACPI_FULL_PATHNAME, &buf);
	if (ACPI_FAILURE(status)) {
		usi_err("Can't get dev name. (eid=%u; ret=%d)\n", uent->eid, status);
		goto out;
	}

	comp = (struct acpi_iort_named_component *)node->node_data;
	status = strcmp(comp->device_name, buf.pointer) == 0 ? AE_OK : AE_NOT_FOUND;
	acpi_os_free(buf.pointer);

out:
	return status;
}

STATIC struct acpi_iort_node *usi_iort_scan_node(enum acpi_iort_node_type type, iort_match_node_callback_fn callback,
												 struct ub_entity *uent, struct device *dev,
												 struct acpi_table_header **out_iort_table)
{
	struct acpi_iort_node *node = NULL;
	struct acpi_iort_node *end = NULL;
	struct acpi_table_iort *iort = NULL;
	struct acpi_table_header *iort_table = NULL;
	u32 i;
	acpi_status status;

	status = acpi_get_table(ACPI_SIG_IORT, 0, &iort_table);
	if (ACPI_FAILURE(status) || iort_table == NULL) {
		usi_err("Get iort table failed. (eid=%u)\n", uent->eid);
		return NULL;
	}

	*out_iort_table = iort_table;
	iort = (struct acpi_table_iort *)(void *)iort_table;
	node = ACPI_ADD_PTR(struct acpi_iort_node, iort, iort->node_offset);
	end = ACPI_ADD_PTR(struct acpi_iort_node, iort_table, iort_table->length);

	for (i = 0; i < iort->node_count; i++) {
		if (node >= end) {
			usi_err("IORT node pointer overflows, bad table. (eid=%u)\n", uent->eid);
			return NULL;
		}

		if (node->type == type && ACPI_SUCCESS(callback(node, uent, dev))) {
			return node;
		}

		node = ACPI_ADD_PTR(struct acpi_iort_node, node, node->length);
	}

	usi_err("Cannot find iort node. (eid=%u)\n", uent->eid);
	return NULL;
}

STATIC struct acpi_iort_node *usi_iort_node_get_id(struct acpi_iort_node *node, struct ub_entity *uent,
												   u32 *id, struct acpi_table_header *iort_table, int index)
{
	struct acpi_iort_node *parent = NULL;
	struct acpi_iort_id_mapping *map = NULL;

	if (node->mapping_offset == 0 || node->mapping_count == 0 || (u32)index >= node->mapping_count) {
		return NULL;
	}

	map = ACPI_ADD_PTR(struct acpi_iort_id_mapping, node, node->mapping_offset + index * sizeof(*map));
	/* firmware abnormal, format incorrect */
	if (map->output_reference == 0) {
		usi_err("ID map parent reference is NULL. (eid=%u; type=%d)\n", uent->eid, node->type);
		return NULL;
	}

	parent = ACPI_ADD_PTR(struct acpi_iort_node, iort_table, map->output_reference);
	if ((map->flags & ACPI_IORT_ID_SINGLE_MAPPING) != 0) {
		if (node->type == ACPI_IORT_NODE_NAMED_COMPONENT || node->type == ACPI_IORT_NODE_PCI_ROOT_COMPLEX ||
			node->type == ACPI_IORT_NODE_SMMU_V3 || node->type == ACPI_IORT_NODE_PMCG) {
			*id = map->output_base;
			return parent;
		}
	}

	return NULL;
}

STATIC int usi_iort_get_id_mapping_index(struct acpi_iort_node *node, struct ub_entity *uent)
{
	struct acpi_iort_smmu_v3 *smmu = NULL;
	struct acpi_iort_pmcg *pmcg = NULL;

	switch (node->type) {
	case ACPI_IORT_NODE_SMMU_V3:
		if (node->revision < 1) {
			usi_err("Invalid revision. (eid=%u; revision=%u)\n", uent->eid, node->revision);
			return -EINVAL;
		}

		smmu = (struct acpi_iort_smmu_v3 *)node->node_data;
		if (smmu->event_gsiv != 0 && smmu->pri_gsiv != 0 && smmu->gerr_gsiv != 0 && smmu->sync_gsiv != 0) {
			usi_err("Invalid smmu. (eid=%u; e_gsiv=%u; p_gsiv=%u; g_gsiv=%u; s_gsiv=%u)\n",
					uent->eid, smmu->event_gsiv, smmu->pri_gsiv, smmu->gerr_gsiv, smmu->sync_gsiv);
			return -EINVAL;
		}

		if (smmu->id_mapping_index >= node->mapping_count) {
			usi_err("ID mapping index overflows. (eid=%u; type=%d)\n", uent->eid, node->type);
			return -EINVAL;
		}

		return smmu->id_mapping_index;
	case ACPI_IORT_NODE_PMCG:
		pmcg = (struct acpi_iort_pmcg *)node->node_data;
		if (pmcg->overflow_gsiv != 0 || node->mapping_count == 0) {
			usi_err("Invalid pmcg. (eid=%u; o_gsiv=%u; count=%u)\n", uent->eid, pmcg->overflow_gsiv,
					node->mapping_count);
			return -EINVAL;
		}

		return 0;
	default:
		usi_err("Node type not support. (eid=%u; type=%u)\n", uent->eid, node->type);
		break;
	}

	return -EINVAL;
}

STATIC int usi_iort_id_map(struct acpi_iort_id_mapping *map, struct usi_iort_map_id *map_id,
						   struct ub_entity *uent)
{
	if ((map->flags & ACPI_IORT_ID_SINGLE_MAPPING) != 0) {
		if (map_id->type == ACPI_IORT_NODE_NAMED_COMPONENT || map_id->type == ACPI_IORT_NODE_PCI_ROOT_COMPLEX) {
			map_id->out_ref = map->output_base;
			return 0;
		}

		usi_err("Flag not allowed, skipping ID map. (eid=%u; id=%u; type=%u)\n", uent->eid,
				map_id->map_id, map_id->type);
		return -ENXIO;
	}

	if (map_id->map_id < map->input_base || (map_id->map_id > map->input_base + map->id_count)) {
		usi_err("Invalid map id. (eid=%u; id=%u; input_base=%u; id_count=%u)\n", uent->eid, map_id->map_id,
				map->input_base, map->id_count);
		return -ENXIO;
	}

	if (map_id->out_ref != 0) {
		usi_err("Conflicting mapping. (eid=%u; id=%u)\n", uent->eid, map_id->map_id);
		if (map_id->map_id != map->input_base) {
			return -ENXIO;
		}

		usi_err("Applying workaround. (eid=%u)\n", uent->eid);
	}

	map_id->out_ref = map->output_base + (map_id->map_id - map->input_base);
	if (map->id_count > 0 && map_id->map_id == map->input_base + map->id_count) {
		return -EAGAIN;
	}
	return 0;
}

STATIC struct acpi_iort_node *usi_iort_node_by_map_id(struct acpi_iort_node *node, u32 id, struct ub_entity *uent,
													  struct acpi_table_header *iort_table)
{
	u32 tmp_id = id;
	struct acpi_iort_id_mapping *map = NULL;
	int idx, i;

	while (node != NULL) {
		struct usi_iort_map_id map_id = {.map_id = tmp_id, .out_ref = 0, .type = node->type};
		int ret = 0;

		if (node->type == ACPI_IORT_NODE_ITS_GROUP) {
			return node;
		}

		if (node->mapping_offset == 0 || node->mapping_count == 0) {
			usi_err("Invalid mapping offset or mapping count. (eid=%u)\n", uent->eid);
			goto out;
		}

		map = ACPI_ADD_PTR(struct acpi_iort_id_mapping, node, node->mapping_offset);
		/* firmware error */
		if (map->output_reference == 0) {
			usi_err("ID map parent reference is NULL. (eid=%u; type=%d)\n", uent->eid, node->type);
			goto out;
		}

		idx = usi_iort_get_id_mapping_index(node, uent);
		for (i = 0; (u32)i < node->mapping_count; i++, map++) {
			if (i == idx) {
				continue;
			}

			ret = usi_iort_id_map(map, &map_id, uent);
			if (ret == 0) {
				break;
			}
			if (ret == -EAGAIN) {
				map_id.out_ref = map->output_reference;
			}
		}

		if ((u32)i == node->mapping_count && map_id.out_ref == 0) {
			goto out;
		}

		node = ACPI_ADD_PTR(struct acpi_iort_node, iort_table, ret ? map_id.out_ref : map->output_reference);
	}

out:
	return NULL;
}

STATIC struct acpi_iort_node *usi_iort_node_map_platform_id(struct acpi_iort_node *node, struct ub_entity *uent,
															struct acpi_table_header *iort_table, int index)
{
	struct acpi_iort_node *parent;
	u32 id;

	/* step 1: retrieve the initial dev id */
	parent = usi_iort_node_get_id(node, uent, &id, iort_table, index);
	if (parent == NULL) {
		return NULL;
	}

	if (parent->type != ACPI_IORT_NODE_ITS_GROUP) {
		parent = usi_iort_node_by_map_id(parent, id, uent, iort_table);
	}

	return parent;
}

STATIC struct acpi_iort_its_group *usi_get_device_domain_its(struct ub_entity *uent, struct device *dev)
{
	struct acpi_iort_node *node = NULL;
	struct acpi_iort_node *parent = NULL;
	struct acpi_iort_its_group *its = NULL;
	struct acpi_table_header *iort_table = NULL;
	int i;

	node = usi_iort_scan_node(ACPI_IORT_NODE_NAMED_COMPONENT, usi_iort_match_node_callback, uent, dev, &iort_table);
	if (node == NULL) {
		usi_err("Get iort node failed. (eid=%u)\n", uent->eid);
		return NULL;
	}

	for (i = 0; (u32)i < node->mapping_count; i++) {
		parent = usi_iort_node_map_platform_id(node, uent, iort_table, i);
		if (parent != NULL) {
			break;
		}
	}

	if (parent == NULL) {
		return NULL;
	}

	its = (struct acpi_iort_its_group *)parent->node_data;
	return its;
}

STATIC int usi_get_its_phy_addr(struct ub_entity *uent, u32 its_id, u64 *addr)
{
	struct acpi_table_madt *madt = NULL;
	unsigned long madt_end = 0;
	unsigned long entry = 0;
	acpi_status status;

	status = acpi_get_table(ACPI_SIG_MADT, 0, (struct acpi_table_header **)&madt);
	if (ACPI_FAILURE(status) || madt == NULL) {
		usi_err("Get madt failed. (cna=%u; eid=%u; entity_idx=%u; ret=%d)\n", uent->cna, uent->eid,
				uent->entity_idx, status);
		return -EEXIST;
	}

	entry = (unsigned long)(uintptr_t)madt;
	madt_end = entry + madt->header.length;
	entry += sizeof(struct acpi_table_madt);
	while (entry + sizeof(struct acpi_subtable_header) < madt_end) {
		struct acpi_madt_generic_translator *trans = NULL;
		struct acpi_subtable_header *header = (struct acpi_subtable_header *)(uintptr_t)entry;

		if (header->length < sizeof(struct acpi_subtable_header)) {
			return -ENODATA;
		}
		if (header->type != ACPI_MADT_TYPE_GENERIC_TRANSLATOR) {
			entry += header->length;
			continue;
		}
		if (header->length != sizeof(struct acpi_madt_generic_translator)) {
			entry += header->length;
			continue;
		}
		trans = (struct acpi_madt_generic_translator *)(uintptr_t)entry;
		/* after ubus switches to UBFI, there is no its_id under uent->ubc, the management plane needs to get its_id from the acpi IORT table, then get the PA, due to workload reasons, USI device-side is not supported for now,
		 * this implementation will be modified later
		 */
		if (trans->translation_id == its_id) {
			*addr = trans->base_address;
			return 0;
		}
		entry += header->length;
	}

	return -ENODATA;
}

STATIC int usi_cfgspace_set_to_pa(struct ub_entity *uent, struct device *dev)
{
	struct acpi_iort_its_group *its = NULL;
	u64 its_addr = 0;
	int ret;

	its = usi_get_device_domain_its(uent, dev);
	if (its == NULL || its->its_count == 0) {
		usi_err("Get its domain failed. (eid=%u)\n", uent->eid);
		return -ENODEV;
	}

	usi_info("Get its success. (eid=%u; its_id=%u)\n", uent->eid, its->identifiers[0]);
	ret = usi_get_its_phy_addr(uent, its->identifiers[0], &its_addr);
	if (ret != 0) {
		usi_err("Get its phy addr failed. (eid=%u; its_id=%u; ret=%d)\n", uent->eid, its->identifiers[0], ret);
		return ret;
	}

	/* ARM ITS specification definition */
	its_addr += 0x10040;
	ret = usi_ub_cfg_write_dword(uent, UB_INT_TYPE1_INT_ADDR_L, (u32)(its_addr & 0xFFFFFFFF));
	if (ret != 0) {
		usi_err("Config its low addr failed. (cna=%u; eid=%u; ret=%d)\n", uent->cna, uent->eid, ret);
		return ret;
	}

	ret = usi_ub_cfg_write_dword(uent, UB_INT_TYPE1_INT_ADDR_H, (u32)((its_addr >> 32) & 0xFFFFFFFF)); /* take the high 32-bit address */
	if (ret != 0) {
		usi_err("Config its high addr failed. (cna=%u; eid=%u; ret=%d)\n", uent->cna, uent->eid, ret);
	}

	return ret;
}

STATIC acpi_status usi_acpi_process_ubc(acpi_handle handle, u32 lvl, void *context, void **rv)
{
	struct ub_entity *uent = (struct ub_entity *)context;
	acpi_status status;
	unsigned long long uid;
	struct acpi_device *adev = NULL;
	struct device *dev = NULL;
	int ret;

	status = acpi_evaluate_integer(handle, "_UID", NULL, &uid);
	if (ACPI_FAILURE(status)) {
		return AE_CTRL_TERMINATE;
	}

	usi_info("ubc acpi info. (ctl_no=%u; uid=%llu; eid=%u)\n", uent->ubc->ctl_no, uid, uent->eid);
	if (uent->ubc->ctl_no != (u32)uid) {
		return AE_OK;
	}

	adev = acpi_get_acpi_dev(handle);
	if (adev == NULL) {
		usi_err("Get acpi dev failed. (eid=%u)\n", uent->eid);
		return AE_CTRL_TERMINATE;
	}

	dev = bus_find_device_by_acpi_dev(&platform_bus_type, adev);
	if (dev == NULL) {
		usi_err("Find device by acpi dev failed. (eid=%u)\n", uent->eid);
		status = AE_CTRL_TERMINATE;
		goto fail;
	}

	ret = usi_cfgspace_set_to_pa(uent, dev);
	if (ret != 0) {
		usi_err("Cfgspace to pa failed. (eid=%u; ret=%d)\n", uent->eid, ret);
		status = AE_CTRL_TERMINATE;
	}

fail:
	acpi_put_acpi_dev(adev);
	return status;
}

#define ACPI_UBC_DEVICE_HID "HISI0541"

int usi_cfgspace_to_its_pa_by_acpi(struct ub_entity *uent)
{
	acpi_status status;

	status = acpi_get_devices(ACPI_UBC_DEVICE_HID, usi_acpi_process_ubc, uent, NULL);
	if (ACPI_FAILURE(status)) {
		usi_err("Get ubc dev failed. (eid=%u; ret=%d)\n", uent->eid, status);
		return -ENODEV;
	}

	usi_info("Cfgspace to its pa success. (eid=%u)\n", uent->eid);
	return 0;
}
