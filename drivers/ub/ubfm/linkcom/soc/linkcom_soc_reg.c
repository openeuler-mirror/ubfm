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
#include "ubfabric.h"
#include "ubfabric.h"
#include "ures.h"
#include "uop.h"
#include "linkcom_pub.h"
#include "linkcom_log.h"
#include "linkcom_soc_reg.h"

static bool is_ndie(struct linkcom_id_inst *id_inst)
{
	if (id_inst->chip_type == MAMI_KERN_TYPE_KUNPENG960_1P ||
		id_inst->chip_type == MAMI_KERN_TYPE_KUNPENG960_2P ||
		id_inst->chip_type == MAMI_KERN_TYPE_KUNPENG960_4P) {
		return true;
	}
	return false;
}

u32 linkcom_get_mgtq_cqe_depth(struct linkcom_id_inst *id_inst)
{
	if (is_ndie(id_inst)) {
		return CTRL_CPU_NDIE_MGTQ_CQE_DEPTH;
	}
	return CTRL_CPU_UDIE_MGTQ_CQE_DEPTH;
}

u32 linkcom_get_mgtq_rpkt_buff_depth(struct linkcom_id_inst *id_inst)
{
	if (is_ndie(id_inst)) {
		return CTRL_CPU_NDIE_MGTQ_RPKT_BUFF_DEPTH;
	}
	return CTRL_CPU_UDIE_MGTQ_RPKT_BUFF_DEPTH;
}

u64 linkcom_get_misc_base_addr(struct linkcom_id_inst *id_inst)
{
	struct ures_inst inst = {.devid = id_inst->dev_id, .die_id = id_inst->die_id};
	struct ures_addr_info misc_reg = {0};
	int ret;

	if (id_inst->dev_id >= DEV_MAX_NUM) {
		return 0;
	}

	ret = ures_get_reg_base(&inst, MAMI_MISC_REG, &misc_reg);
	if (ret != 0) {
		linkcom_err("get misc reg base failed. (devid=%u; die_id=%u, ret=%d)\n", id_inst->dev_id, id_inst->die_id, ret);
		return 0;
	}

	return misc_reg.addr;
}

u64 linkcom_get_lqc_imp_base_addr(struct linkcom_id_inst *id_inst)
{
	u64 addr = 0;

	addr = linkcom_get_misc_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + MISC_LQC_IMP_BASE);
}

u64 linkcom_get_ta_base_addr(struct linkcom_id_inst *id_inst)
{
	struct ures_inst inst = {.devid = id_inst->dev_id, .die_id = id_inst->die_id};
	struct ures_addr_info ta_reg = {0};
	int ret;

	if (id_inst->dev_id >= DEV_MAX_NUM) {
		return 0;
	}

	ret = ures_get_reg_base(&inst, MAMI_TA_REG, &ta_reg);
	if (ret != 0) {
		linkcom_err("get ta reg base failed. (devid=%u; die_id=%u, ret=%d)\n", id_inst->dev_id, id_inst->die_id, ret);
		return 0;
	}

	return ta_reg.addr;
}

u64 linkcom_get_lqc_ta_dm_base_addr(struct linkcom_id_inst *id_inst)
{
	u64 addr = 0;

	addr = linkcom_get_ta_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + LQC_TA_DM);
}

u64 linkcom_get_tp_base_addr(struct linkcom_id_inst *id_inst)
{
	struct ures_inst inst = {.devid = id_inst->dev_id, .die_id = id_inst->die_id};
	struct ures_addr_info tp_reg = {0};
	int ret;

	if (id_inst->dev_id >= DEV_MAX_NUM) {
		return 0;
	}

	ret = ures_get_reg_base(&inst, MAMI_TP_REG, &tp_reg);
	if (ret != 0) {
		linkcom_err("get tp reg base failed. (devid=%u; die_id=%u)\n", id_inst->dev_id, id_inst->die_id);
		return 0;
	}

	return tp_reg.addr;
}

u64 linkcom_get_tp_tpp_base_addr(struct linkcom_id_inst *id_inst)
{
	u64 addr = 0;

	addr = linkcom_get_tp_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + TP_TPP_BASE);
}

static u64 linkcom_get_ctrl_cpu_mgtq_base_addr(struct linkcom_id_inst *id_inst)
{
	struct ures_inst inst = {.devid = id_inst->dev_id, .die_id = id_inst->die_id};
	struct ures_addr_info ctrl_cpu_mgtq = {0};
	int ret;

	if (id_inst->dev_id >= DEV_MAX_NUM) {
		return 0;
	}

	ret = ures_get_reg_base(&inst, MAMI_MGTQ_REG, &ctrl_cpu_mgtq);
	if (ret != 0) {
		linkcom_err("get ctrl cpu mgtq base failed. (devid=%u; die_id=%u)\n", id_inst->dev_id, id_inst->die_id);
		return 0;
	}

	return ctrl_cpu_mgtq.addr + CTRL_CPU_MGTQ_OFFSET(id_inst->que_id);
}

u64 linkcom_get_ctrl_cpu_mgtq_tx_buf_base(struct linkcom_id_inst *id_inst)
{
	u64 addr;

	addr = linkcom_get_ctrl_cpu_mgtq_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + CTRL_CPU_MGTQ_TPKT_BUF);
}

u64 linkcom_get_ctrl_cpu_mgtq_wqe_base(struct linkcom_id_inst *id_inst)
{
	u64 addr;

	addr = linkcom_get_ctrl_cpu_mgtq_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + CTRL_CPU_MGTQ_WQE);
}

u64 linkcom_get_mgtq_cqe_base(struct linkcom_id_inst *id_inst)
{
	u64 addr;

	addr = linkcom_get_ctrl_cpu_mgtq_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + CTRL_CPU_MGTQ_CQE);
}

u64 linkcom_get_mgtq_rpkt_buf_base(struct linkcom_id_inst *id_inst)
{
	u64 addr;

	addr = linkcom_get_ctrl_cpu_mgtq_base_addr(id_inst);
	if (addr == 0) {
		return 0;
	}

	return (addr + CTRL_CPU_MGTQ_RPKT_BUF);
}

int linkcom_get_cqe_ptr(struct linkcom_id_inst *id_inst, u64 reg_base, struct imp_ctrl_cpu_rcqe_buf *rcqe_ptr, struct imp_ctrl_cpu_rpkt_buf *rpkt_ptr)
{
	u64 reg_addr;
	int ret;

	reg_addr = reg_base + IMP_CTRL_CPU_RCQE_BUF(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)rcqe_ptr);
	if (ret != 0) {
		linkcom_err("read IMP_CTRL_CPU_RCQE_BUF failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	reg_addr = reg_base + IMP_CTRL_CPU_RPKT_BUF(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)rpkt_ptr);
	if (ret != 0) {
		linkcom_err("read IMP_CTRL_CPU_RPKT_BUF failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_read_cqe(struct linkcom_id_inst *id_inst, u32 cqe_ci, struct linkcom_rx_cqe *cqe)
{
	u64 cqe_entry, cqe_base, cqe_depth;
	int ret = 0;

	cqe_base = linkcom_get_mgtq_cqe_base(id_inst);
	if (cqe_base == 0) {
		linkcom_err("cqe base addr is 0\n");
		return -EFAULT;
	}

	cqe_depth = linkcom_get_mgtq_cqe_depth(id_inst);

	cqe_entry = cqe_base + (cqe_ci % cqe_depth) * CTRL_CPU_MGTQ_CQE_ENREY_LEN;

	ret = uop_kern_readq(id_inst->dev_id, cqe_entry, (u64 *)(uintptr_t)cqe);
	if (ret != 0) {
		linkcom_err("uop_kern_readq fail, ret[%d]\n", ret);
		return ret;
	}

	return 0;
}

int linkcom_read_rpkt_buf(struct read_pkt_ctx *ctx, u32 entry_id, u32 pkt_len, u8 *recv_buff)
{
	u64 entry_addr, pkt_addr, end_addr;
	u32 rpkt_buff_depth, invalid_len;
	int ret;

	rpkt_buff_depth = linkcom_get_mgtq_rpkt_buff_depth(&ctx->inst);
	entry_addr = ctx->rpkt_base + (entry_id % rpkt_buff_depth) * CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN;
	end_addr = ctx->rpkt_base + rpkt_buff_depth * CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN;

	if (ctx->status != NORMAL_STS) {
		invalid_len = (ctx->dbg_reg.rsv_reg_4 & IMP_DBG_RSV_REG_24_MASK) % 8 * 16; /* 8 abnormalities fill one entry, offset 16 Bytes each time */
		pkt_addr = entry_addr + invalid_len;
	} else {
		pkt_addr = entry_addr;
	}

	if ((pkt_addr + pkt_len) >= end_addr) {
		/* queue wraparound, need to read in 2 segments */
		ret = uop_kern_io_buffer_read(ctx->inst.dev_id, pkt_addr, (void *)recv_buff, end_addr - pkt_addr);
		if (ret != 0) {
			linkcom_err("io buffer read fail, addr[0x%llx], ret[%d]\n", pkt_addr, ret);
			return ret;
		}

		ret = uop_kern_io_buffer_read(ctx->inst.dev_id, ctx->rpkt_base, (void *)(recv_buff + end_addr - pkt_addr),
			pkt_len - (end_addr - pkt_addr));
		if (ret != 0) {
			linkcom_err("io buffer read fail, addr[0x%llx], ret[%d]\n", ctx->rpkt_base, ret);
			return ret;
		}
	} else {
		ret = uop_kern_io_buffer_read(ctx->inst.dev_id, pkt_addr, (void *)recv_buff, pkt_len);
		if (ret != 0) {
			linkcom_err("io buffer read fail, addr[0x%llx], ret[%d]\n", pkt_addr, ret);
			return ret;
		}
	}

	ctx->next_pkt_ci = (entry_id + DIV_CEIL(pkt_len, CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN)) % rpkt_buff_depth;
	return 0;
}

int linkcom_write_rpkt_ci(struct read_pkt_ctx *ctx)
{
	int ret;
	u64 reg_addr;
	u32 rpkt_buff_depth;
	struct imp_ctrl_cpu_rpkt_buf rpkt_reg = {0};

	/* used for the scenario where there is only one abnormal cqe but no packet in the queue, in this case rpkt_ci equals rpkt_pi, no need to refresh */
	if (ctx->next_pkt_ci == LINKCOM_RPKT_NEXT_CI_INIT) {
		return 0;
	}

	reg_addr = ctx->imp_base + IMP_CTRL_CPU_RPKT_BUF(ctx->inst.que_id);
	if (ctx->rpkt_reg.rpkt_ci == ctx->next_pkt_ci) {
		/* 128 entries consumed at once, need to free in multiple times, here free one entry first */
		rpkt_buff_depth = linkcom_get_mgtq_rpkt_buff_depth(&ctx->inst);
		rpkt_reg.rpkt_ci = (ctx->rpkt_reg.rpkt_ci + 1) % rpkt_buff_depth;
		ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, *(u32 *)(void *)&rpkt_reg);
		if (ret != 0) {
			linkcom_err("update rpkt ci failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
			return ret;
		}
	}

	rpkt_reg.rpkt_ci = ctx->next_pkt_ci;
	ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, *(u32 *)(void *)&rpkt_reg);
	if (ret != 0) {
		linkcom_err("update rpkt ci failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return 0;
}
