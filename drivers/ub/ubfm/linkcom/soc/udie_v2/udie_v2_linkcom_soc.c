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
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/unistd.h>
#include "mctrlq.h"
#include "ubfabric.h"
#include "ubfabric.h"
#include "ures.h"
#include "uop.h"
#include "linkcom_log.h"
#include "linkcom_mng.h"
#include "udie_v2_linkcom_soc.h"
#include "idm.h"
#include "linkcom_soc_reg.h"

int linkcom_update_rcqe_ci(struct read_pkt_ctx *ctx)
{
	int ret;
	u64 reg_addr;
	struct imp_ctrl_cpu_rcqe_buf rcqe_reg = {0};

	reg_addr = ctx->imp_base + IMP_CTRL_CPU_RCQE_BUF(ctx->inst.que_id);
	rcqe_reg.rcqe_ci = (ctx->rcqe_reg.rcqe_ci + 1) % linkcom_get_mgtq_cqe_depth(&ctx->inst); /* consume one cqe each time */
	ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, *(u32 *)(void *)&rcqe_reg);
	if (ret != 0) {
		linkcom_err("update rcqe ci failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_append_rpkt_node_v2(struct read_pkt_ctx *ctx, struct linkcom_rx_cqe *cqe)
{
	u8 *buff = NULL;
	int ret;
	u32 rpkt_buff_depth = 0;

	ctx->rpkt_base = linkcom_get_mgtq_rpkt_buf_base(&ctx->inst);
	if (ctx->rpkt_base == 0) {
		linkcom_err("rpkt_buf base addr is 0\n");
		return -EFAULT;
	}

	rpkt_buff_depth = linkcom_get_mgtq_rpkt_buff_depth(&ctx->inst);
	if ((cqe->pkt_len == 0) || (cqe->pkt_len > rpkt_buff_depth * CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN)) {
		linkcom_err("Invalid pkt_len: %u\n", cqe->pkt_len);
		return -EINVAL;
	}

	buff = kzalloc(cqe->pkt_len, GFP_KERNEL | __GFP_ACCOUNT);
	if (buff == NULL) {
		linkcom_err("alloc buff fail\n");
		return -ENOMEM;
	}

	ret = linkcom_read_rpkt_buf(ctx, cqe->pkt_buf_entry_id, cqe->pkt_len, buff);
	if (ret != 0) {
		kfree(buff);
		return ret;
	}

	ret = linkcom_add_list_node(ctx->head, *cqe, buff);

	kfree(buff);

	return ret;
}

int udie_v2_inband_read_rpkt(struct linkcom_id_inst *id_inst, struct linkcom_list_head *head, u32 *pkt_num)
{
	int ret;
	struct linkcom_rx_cqe cqe = {0};
	struct read_pkt_ctx ctx = {0};

	if (head->count >= MAX_BUFF_CNT) {
#ifndef EMU_ST
		linkcom_err("list is overflow. (devid=%u, dieid=%u, queid=%u)\n", id_inst->dev_id, id_inst->die_id, id_inst->que_id);
		return -EOVERFLOW;
#endif
	}

	ctx.inst = *id_inst;
	ctx.head = head;

	ctx.imp_base = linkcom_get_lqc_imp_base_addr(id_inst);
	if (ctx.imp_base == 0) {
		linkcom_err("reg base addr is 0\n");
		return -EFAULT;
	}

	ret = linkcom_get_cqe_ptr(id_inst, ctx.imp_base, &ctx.rcqe_reg, &ctx.rpkt_reg);
	if (ret != 0) {
		return ret;
	}

	if (ctx.rcqe_reg.rcqe_ci == ctx.rcqe_reg.rcqe_pi) {
		return -EAGAIN;
	}

	ret = linkcom_read_cqe(id_inst, ctx.rcqe_reg.rcqe_ci, &cqe);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_append_rpkt_node_v2(&ctx, &cqe);
	if (ret != 0) {
		return ret;
	}

	*pkt_num = *pkt_num + 1;
	ret = linkcom_write_rpkt_ci(&ctx);
	if (ret != 0) {
		return ret;
	}

	return linkcom_update_rcqe_ci(&ctx);
}
