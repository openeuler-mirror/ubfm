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
#include <linux/version.h>
#include <asm/pgtable.h>
#include "mami_string.h"

#include "uop_log.h"
#include "ures.h"
#include "umap_kern.h"

#define PAGE_4K_ALIGNED(addr)		(((addr) & ((typeof(addr))(0x1000) - 1)) == 0)

static const char *uop_vma_magic = "MAMI_UBFABRIC_MMAP";

static void *umap_get_vma_magic(void)
{
	return (void *)uop_vma_magic;
}

static bool is_uop_vma_magic(void *magic)
{
#ifndef EMU_ST
	return (magic == (void *)uop_vma_magic);
#else
	return true;
#endif
}

static inline struct rw_semaphore *umap_get_mmlock(struct mm_struct *mm)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	return &mm->mmap_lock;
#else
	return &mm->mmap_sem;
#endif
}

static int umap_get_mem_base(u32 devid, u32 die_id, const char *name, struct ures_addr_info *mem)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	int ret;

	ret = ures_get_mem_base(&inst, name, mem);
	if (unlikely(ret != 0)) {
		uop_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	if (unlikely(!PAGE_4K_ALIGNED(mem->addr))) {
		uop_err("Invalid mem addr.\n");
		return -EINVAL;
	}

	if (unlikely((mem->size == 0) || !PAGE_4K_ALIGNED(mem->size))) {
		uop_err("Invalid mem size. (devid=%u; die_id=%u; name=%s; size=%u)\n", devid, die_id, name, mem->size);
		return -EINVAL;
	}

	return ret;
}

static int umap_get_reg_base(u32 devid, u32 die_id, const char *name, struct ures_addr_info *reg)
{
	struct ures_inst inst = {.devid = devid, .die_id = die_id};
	int ret;

	ret =  ures_get_reg_base(&inst, name, reg);
	if (unlikely(ret != 0)) {
		uop_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	if (unlikely(!PAGE_4K_ALIGNED(reg->addr))) {
		uop_err("Invalid mem addr.\n");
		return -EINVAL;
	}

	if (unlikely((reg->size == 0) || !PAGE_4K_ALIGNED(reg->size))) {
		uop_err("Invalid mem size. (devid=%u; die_id=%u; name=%s; size=%u)\n", devid, die_id, name, reg->size);
		return -EINVAL;
	}

	return ret;
}

static struct vm_area_struct *_umap_find_vma(struct mm_struct *mm, u64 acc_addr, size_t size)
{
	struct vm_area_struct *vma = NULL;

	if (unlikely((size == 0) || !PAGE_ALIGNED(size) || !PAGE_ALIGNED(acc_addr))) {
		uop_err("Invalid size or addr. (size=%zu)\n", size);
		return NULL;
	}

	if (unlikely((acc_addr + size) < acc_addr)) {
		uop_err("u64 addr overflow. (size=%zu)\n", size);
		return NULL;
	}

	vma = find_vma(mm, (unsigned long)acc_addr);
	if (unlikely(vma == NULL)) {
		uop_err("Find vma fail. (size=%zu)\n", size);
		return NULL;
	}

	if (unlikely((vma->vm_start >= vma->vm_end) || (vma->vm_start > acc_addr) || (vma->vm_end < (acc_addr + size)))) {
		uop_err("Invalid addr.\n");
		return NULL;
	}

	return vma;
}

static inline void _umap_set_vmaflag(struct vm_area_struct *vma)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
	vm_flags_set(vma, vma->vm_flags | VM_LOCKED | VM_DONTEXPAND | VM_PFNMAP | VM_DONTDUMP | VM_DONTCOPY | VM_IO);
#else
	vma->vm_flags |= VM_LOCKED | VM_DONTEXPAND | VM_PFNMAP | VM_DONTDUMP | VM_DONTCOPY | VM_IO;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
static int umap_mremap(struct vm_area_struct *vma)
{
	uop_err("mremap not supported\n");
	return -EFAULT;
}
#endif

static const struct vm_operations_struct umap_vmaops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
		.mremap = umap_mremap,
#endif
};

static inline void _umap_set_vmaops(struct vm_area_struct *vma)
{
	vma->vm_ops = &umap_vmaops;
}

static int _umap_check_addr_map(struct vm_area_struct *vma, u64 acc_addr, u64 asic_addr, size_t size)
{
	unsigned long start = acc_addr;
	unsigned long end = 0;
	unsigned long pfn;

	if (unlikely((acc_addr + size) < acc_addr)) {
		uop_err("u64 addr overflow. (size=%zu)\n", size);
		return -EINVAL;
	}
	end = acc_addr + size; /* size is page aligned */

	for (; start < end; start += PAGE_SIZE) {
		if (follow_pfn(vma, start, &pfn) == 0) {
			uop_err("Already has pte. (start=0x%lx; size=%lu)\n", start, size);
			return -EINVAL;
		}
	}

	return 0;
}

static bool ends_with(const char *str, const char *suffix)
{
	size_t len_str, len_suffix;

	if (str == NULL || suffix == NULL) {
		return false;
	}

	len_str = strlen(str);
	len_suffix = strlen(suffix);
	if (len_suffix > len_str) {
		return false;
	}

	return strcmp(str + len_str - len_suffix, suffix) == 0;
}

static int umap_mem_map_inner(struct vm_area_struct *vma, u64 acc_addr, u64 asic_addr, size_t size, const char *name)
{
	u64 align_local_addr = asic_addr & ~(PAGE_SIZE - 1);
	u64 align_offset = asic_addr & (PAGE_SIZE - 1);
	size_t align_size = PAGE_ALIGN(size + align_offset);
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	pgprot_t prot = pgprot_device(vma->vm_page_prot);
#else
	pgprot_t prot = pgprot_writecombine(vma->vm_page_prot);
#endif
	int ret;

	ret = _umap_check_addr_map(vma, acc_addr, asic_addr, size);
	if (unlikely(ret != 0)) {
		uop_err("Check map addr fail. (ret=%d)\n", ret);
		return ret;
	}
#ifdef CFG_UBFM_SUPPORT_PCIE_FPGA
	return io_remap_pfn_range(vma, (unsigned long)acc_addr, __phys_to_pfn(align_local_addr), align_size, prot);
#else
	if (ends_with(name, "_sm")) {
		return io_remap_pfn_range(vma, (unsigned long)acc_addr, __phys_to_pfn(align_local_addr), align_size, prot);
	}

	return remap_pfn_range(vma, (unsigned long)acc_addr, __phys_to_pfn(align_local_addr), align_size, prot);
#endif
}

int umap_mem_map(u64 acc_addr, size_t size, struct ures_addr_info *mem, const char *name)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	int ret = -EFAULT;

	if (unlikely(mm == NULL)) {
		/* Map is not supported in kernel thread */
		return -EINPROGRESS;
	}

	down_write(umap_get_mmlock(mm));
	vma = _umap_find_vma(mm, acc_addr, size);
	if ((vma != NULL) && (is_uop_vma_magic(vma->vm_private_data))) {
		ret = umap_mem_map_inner(vma, acc_addr, mem->addr, min_t(size_t, size, (size_t)mem->size), name);
	}
	up_write(umap_get_mmlock(mm));

	if (unlikely(ret != 0)) {
		uop_err("Mem map fail. (ret=%d; size=%lu)\n", ret, size);
	}

	return ret;
}

static int umap_reg_map_inner(struct vm_area_struct *vma, u64 acc_addr, u64 asic_addr, size_t size, const char *name)
{
	u64 align_local_addr = asic_addr & ~(PAGE_SIZE - 1);
	u64 align_offset = asic_addr & (PAGE_SIZE - 1);
	size_t align_size = PAGE_ALIGN(size + align_offset);
	pgprot_t prot = pgprot_device(vma->vm_page_prot);
	int ret;

	ret = _umap_check_addr_map(vma, acc_addr, asic_addr, size);
	if (unlikely(ret != 0)) {
		uop_err("Check map addr fail. (ret=%d)\n", ret);
		return ret;
	}
	if (ends_with(name, "_pm")) {
		return remap_pfn_range(vma, (unsigned long)acc_addr, __phys_to_pfn(align_local_addr), align_size, prot);
	}

	return io_remap_pfn_range(vma, (unsigned long)acc_addr, __phys_to_pfn(align_local_addr), align_size, prot);
}

static int umap_reg_map(u64 acc_addr, size_t size, struct ures_addr_info *reg, const char *name)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	int ret = -EFAULT;

	if (unlikely(mm == NULL)) {
		/* Map is not supported in kernel thread */
		return -EINPROGRESS;
	}

	down_write(umap_get_mmlock(mm));
	vma = _umap_find_vma(mm, acc_addr, size);
	if ((vma != NULL) && (is_uop_vma_magic(vma->vm_private_data))) {
		ret = umap_reg_map_inner(vma, acc_addr, reg->addr, min_t(size_t, size, (size_t)reg->size), name);
	}
	up_write(umap_get_mmlock(mm));

	if (unlikely(ret != 0)) {
		uop_err("Mem map fail. (ret=%d; size=%lu)\n", ret, size);
	}

	return ret;
}

static int get_ures_name_by_idx(const char *name, char *out_name, u32 len, u32 idx)
{
	char prefix[URES_NAME_LEN] = "", suffix[URES_NAME_LEN] = "";
	u32 prefix_len = 0, suffix_len = 0;
	int ret = 0;

	if (strstr(name, "_win") == NULL) {
		return -EINVAL;
	}
	prefix_len = (size_t)(strstr(name, "_win") - name);
	ret = ubfm_strncpy(prefix, URES_NAME_LEN, name, prefix_len);
	if (unlikely(ret != 0)) {
		return -ENOMEM;
	}
	if (strlen(name) < prefix_len + strlen("_win0")) {
		return -EINVAL;
	}
	suffix_len = (size_t)strlen(name) - prefix_len - strlen("_win0");
	ret = ubfm_strncpy(suffix, URES_NAME_LEN, name + prefix_len + strlen("_win0"), suffix_len);
	if (unlikely(ret != 0)) {
		return -ENOMEM;
	}
	ret = UBFM_SPRINTF(out_name, len, "%s_win%u%s", prefix, idx, suffix);
	if (ret <= 0) {
		return -EINVAL;
	}
	return 0;
}

int umap_kern_mem_map(u32 devid, u32 die_id, const char *name, u64 acc_addr, size_t size)
{
	struct ures_addr_info mem;
	size_t mapped_size = 0, aligned_size = 0;
	char cur_name[URES_NAME_LEN] = "";
	u64 cur_acc = acc_addr, aligned_offset = 0;
	u32 idx = 0;
	int ret = 0;

	if (!PAGE_ALIGNED(acc_addr) || (size == 0) || !PAGE_ALIGNED(size)) {
#ifndef EMU_ST
		uop_err("invalid para. (size=%zu; devid=%u; die_id=%u; name=%s)\n", size, devid, die_id, name);
		return -EINVAL;
#endif
	}
	while (mapped_size < size) {
		ret = get_ures_name_by_idx(name, cur_name, URES_NAME_LEN, idx);
		if (ret != 0) {
			uop_err("Get ures name failed. (ret=%d; devid=%u; die_id=%u; name=%s; idx=%u)\n", ret, devid, die_id, name, idx);
			goto out;
		}
		idx++;

		ret = umap_get_mem_base(devid, die_id, cur_name, &mem);
		if (unlikely(ret != 0)) {
			uop_err("Get mem base failed. (ret=%d; devid=%u; die_id=%u; name=%s)\n", ret, devid, die_id, cur_name);
			goto out;
		}
		aligned_offset = mem.addr & (PAGE_SIZE - 1);
		aligned_size = PAGE_ALIGN(mem.size + aligned_offset);
		if (aligned_size > size - mapped_size) {
			uop_err("Mem base unexpected. (devid=%u; name=%s; mapped_size=%zu; cur_size=%u; total_size=%zu)\n",
				devid, name, mapped_size, mem.size, size);
			ret = -EINVAL;
			goto out;
		}
		if (unlikely(cur_acc + (u64)aligned_size < cur_acc)) {
			uop_err("cur_acc overflow. (devid=%u; mapped_size=%zu; aligned_size=%zu)\n", devid, mapped_size, aligned_size);
			ret = -EINVAL;
			goto out;
		}
		ret = umap_mem_map(cur_acc, aligned_size, &mem, name);
		if (unlikely(ret != 0)) {
			uop_err("Mem map fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n", ret, devid, die_id, name);
			goto out;
		}
		cur_acc += (u64)aligned_size;
		mapped_size += aligned_size;
	}
out:
	if (mapped_size != size) {
		(void)umap_kern_unmap(acc_addr, mapped_size);
	}
	return ret;
}

static void _umap_addr_unmap(struct vm_area_struct *vma, u64 acc_addr, size_t size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
	int ret;

	ret = zap_vma_ptes(vma, acc_addr, size);
	if (unlikely(ret != 0)) {
		uop_err("Zap_vma_ptes fail. (va=0x%llx; ret=%d; flags=0x%lx; start=0x%lx; end=0x%lx)\n",
			acc_addr, ret, vma->vm_flags, vma->vm_start, vma->vm_end);
	}
#else
	zap_vma_ptes(vma, acc_addr, size);
#endif
}

int umap_kern_reg_map(u32 devid, u32 die_id, const char *name, u64 acc_addr, size_t size)
{
	struct ures_addr_info reg;
	int ret;

	ret = umap_get_reg_base(devid, die_id, name, &reg);
	if (unlikely(ret != 0)) {
		uop_err("Get mem base fail. (ret=%d; devid=%u; die_id=%u)\n", ret, devid, die_id);
		return ret;
	}

	ret = umap_reg_map(acc_addr, size, &reg, name);
	if (unlikely(ret != 0)) {
		uop_err("Reg map fail. (ret=%d; devid=%u; die_id=%u; name=%s)\n",
			ret, devid, die_id, name);
	}
	return ret;
}

int umap_kern_unmap(u64 acc_addr, size_t size)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;

	if (unlikely(mm == NULL)) {
		/* Unmap is not supported in kernel thread */
		return -EINPROGRESS;
	}

	if (unlikely(!PAGE_ALIGNED(acc_addr) || (size == 0) || !PAGE_ALIGNED(size))) {
		uop_err("Invalid param. (size=%zu)\n", size);
		return -EINVAL;
	}
	down_write(umap_get_mmlock(mm));
	vma = _umap_find_vma(mm, acc_addr, size);
	if ((vma != NULL) && (is_uop_vma_magic(vma->vm_private_data))) {
		_umap_addr_unmap(vma, acc_addr, size);
	}
	up_write(umap_get_mmlock(mm));
	return 0;
}

int umap_fpga_reg_map(u32 devid, u64 acc_addr, u64 phy_addr, size_t size)
{
	struct ures_addr_info reg = { .addr = phy_addr, .size = size };
#ifndef EMU_ST
	return umap_reg_map(acc_addr, size, &reg, NULL);
#else
	return 0;
#endif
}

int umap_fpga_reg_unmap(u64 acc_addr, size_t size)
{
	return umap_kern_unmap(acc_addr, size);
}
int umap_mmap(u32 devid, struct vm_area_struct *vma)
{
	if (vma == NULL) {
		return -EINVAL;
	}

	_umap_set_vmaflag(vma);
	_umap_set_vmaops(vma);
	vma->vm_private_data = umap_get_vma_magic();

	return 0;
}
