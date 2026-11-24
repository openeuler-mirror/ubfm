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
#include "udie_v1_linkcom_soc.h"
#include "idm.h"
#include "linkcom_soc_reg.h"

int linkcom_get_tx_pi(struct linkcom_id_inst *id_inst, u64 tpp_base, u64 imp_base, u32 *wqe_pi, u32 *buf_pi)
{
	u64 reg_addr;
	struct tpp_ctrl_cpu_wqe_pi tpp_wqe_pi = {0};
	struct imp_ctrl_cpu_twqe_pi imp_wqe_pi = {0};
	int ret;

	reg_addr = tpp_base + TPP_CTRL_CPU_WQE_PI(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)&tpp_wqe_pi);
	if (ret != 0) {
		return ret;
	}

	reg_addr = imp_base + IMP_CTRL_CPU_TWQE_PI(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)&imp_wqe_pi);
	if (ret != 0) {
		return ret;
	}

	*wqe_pi = imp_wqe_pi.ctrl_cpu_mgtq_twqe_pi;
	*buf_pi = tpp_wqe_pi.ctrl_cpu_qid_wqe_pi;

	return 0;
}

int linkcom_get_tx_ci(struct linkcom_id_inst *id_inst, u64 tpp_base, u64 imp_base, u32 *wqe_ci, u32 *buf_ci)
{
	u64 reg_addr;
	struct tpp_ctrl_cpu_wqe_ci tpp_wqe_ci = {0};
	struct imp_ctrl_cpu_twqe_ci imp_twqe_ci = {0};
	int ret;

	reg_addr = tpp_base + TPP_CTRL_CPU_WQE_CI(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)&tpp_wqe_ci);
	if (ret != 0) {
		linkcom_err("read TPP_CTRL_CPU_WQE_CI failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	reg_addr = imp_base + IMP_CTRL_CPU_TWQE_CI(id_inst->que_id);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)&imp_twqe_ci);
	if (ret != 0) {
		linkcom_err("read IMP_CTRL_CPU_TWQE_CI failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	*wqe_ci = imp_twqe_ci.ctrl_cpu_mgtq_twqe_ci;
	*buf_ci = tpp_wqe_ci.ctrl_cpu_qid_wqe_ci;

	return 0;
}

int linkcom_write_tpkt_buf(struct linkcom_id_inst *id_inst, struct mami_pkt_send_para para)
{
	u64 buf_base;
	int ret = 0;

	if ((para.pktLen > CTRL_CPU_MGTQ_TPKT_CREDIT_TOTAL_SIZE) || (para.pktLen <= 0)) {
		linkcom_err("exceed tx buff size, max[%d] cur[%u]\n", CTRL_CPU_MGTQ_TPKT_CREDIT_TOTAL_SIZE, para.pktLen);
		return -EINVAL;
	}

	buf_base = linkcom_get_ctrl_cpu_mgtq_tx_buf_base(id_inst);
	if (buf_base == 0) {
		linkcom_err("base addr is 0\n");
		return -EFAULT;
	}

	ret = uop_kern_io_buffer_write(id_inst->dev_id, buf_base, (void *)para.pkt, para.pktLen);
	if (ret != 0) {
		linkcom_err("io buffer write fail, ret[%d]\n", ret);
		return ret;
	}

	return 0;
}

static int linkcom_send_para_check(u32 devid, struct mami_pkt_send_para *send_para)
{
	if (send_para->cfgType >= LINKCOM_MAX_CONFIG_TYPE) {
		linkcom_err("Invalid cfgType, valid range: [0, %u). (devid=%u, cfgType=%u)\n", LINKCOM_MAX_CONFIG_TYPE, devid,
			send_para->cfgType);
		return -EINVAL;
	}

	if (send_para->vl >= LINKCOM_MAX_VL_NUM) {
		linkcom_err("Invalid vl, valid range: [0, %u). (devid=%u, vl=%u)\n", LINKCOM_MAX_VL_NUM, devid, send_para->vl);
		return -EINVAL;
	}
	return 0;
}

int linkcom_construct_wqe(u32 devid, u32 que_id, struct mami_pkt_send_para *send_para, struct linkcom_tx_wqe *wqe)
{
	u32 nl_id = 0, nl_port = 0;
	u64 value = 0;
	int ret;

	ret = linkcom_send_para_check(devid, send_para);
	if (ret != 0) {
		return ret;
	}

	ret = idm_kern_get_fbody_bport_by_lgport(devid, send_para->lgPortId, &nl_id, &nl_port);
	if (ret != 0) {
		linkcom_err("get nl_id and nl_port failed. (dev_id=%u; lg_port=%u; ret=%d)\n",
			devid, send_para->lgPortId, ret);
		return ret;
	}

	ret = ures_get_key_value(devid, MAMI_NL_NUM, &value);
	if (ret != 0 || value == 0) {
		linkcom_err("Get nl num failed. (devid=%u; ret=%d)\n", devid, ret);
		return -EINVAL;
	}

	wqe->opcode = 0x22; /* 0x22~0x26: enumeration config and ctrl_cpu management packet dispatch config this opcode */
	wqe->vl = send_para->vl;
	wqe->ctrl_cpu_ind = 1;
	wqe->soft_assign_ind = 1;
	wqe->icrc_cs = 0;
	wqe->src_nl_id = nl_id % (u32)value; /* value was validated to be non-zero earlier */
	wqe->src_port_id = nl_port;
	wqe->dst_nl_id = nl_id; /* cnl_id */
	wqe->dst_port_id = nl_port;
	wqe->ee_bits = 0b10;
	wqe->pkt_buf_entry_id = 0; /* always start at tx buff entry0 */
	wqe->pkt_len = send_para->pktLen;
	wqe->cfg = send_para->cfgType;
	wqe->cpu_qid = que_id + 1; /* 0:imp, 1~4:ctrl_cpu_qid0~3 */
	return 0;
}

int linkcom_set_wqe(struct linkcom_id_inst *id_inst, u32 wqe_ci, struct mami_pkt_send_para send_para)
{
	u64 wqe_base, wqe_addr;
	struct linkcom_tx_wqe wqe = {0};
	int ret;

	ret = linkcom_construct_wqe(id_inst->dev_id, id_inst->que_id, &send_para, &wqe);
	if (ret != 0) {
		return ret;
	}

	wqe_base = linkcom_get_ctrl_cpu_mgtq_wqe_base(id_inst);
	if (wqe_base == 0) {
		linkcom_err("base addr is 0\n");
		return -EFAULT;
	}

	wqe_addr = wqe_base + wqe_ci * CTRL_CPU_MGTQ_WQE_ENTRY_OFFSET;
	ret = uop_kern_io_buffer_write(id_inst->dev_id, wqe_addr, (void *)&wqe, CTRL_CPU_MGTQ_WQE_ENREY_LEN);
	if (ret != 0) {
		linkcom_err("io buffer write fail, ret[%d]\n", ret);
		return ret;
	}

	return 0;
}

int linkcom_update_tpkt_pi(struct linkcom_id_inst *id_inst, u64 tpp_base, u64 imp_base, u32 wqe_pi, u32 buf_pi)
{
	u64 reg_addr;
	struct tpp_ctrl_cpu_wqe_pi tpp_wqe_pi = {0};
	struct imp_ctrl_cpu_twqe_pi imp_wqe_pi = {0};
	int ret;

	imp_wqe_pi.ctrl_cpu_mgtq_twqe_pi = wqe_pi;
	tpp_wqe_pi.ctrl_cpu_qid_wqe_pi = buf_pi;

	reg_addr = tpp_base + TPP_CTRL_CPU_WQE_PI(id_inst->que_id);
	ret = uop_kern_writel(id_inst->dev_id, reg_addr, *(u32 *)(void *)&tpp_wqe_pi);
	if (ret != 0) {
		linkcom_err("write TPP_CTRL_CPU_WQE_PI failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	reg_addr = imp_base + IMP_CTRL_CPU_TWQE_PI(id_inst->que_id);
	ret = uop_kern_writel(id_inst->dev_id, reg_addr, *(u32 *)(void *)&imp_wqe_pi);
	if (ret != 0) {
		linkcom_err("write IMP_CTRL_CPU_TWQE_PI failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
	}

	return ret;
}

int linkcom_get_drop_cnt(struct linkcom_id_inst *id_inst, u64 tpp_base, u32 *drop_cnt)
{
	struct tpp_ctrl_cpu_drop_wqe_cnt drop_wqe_cnt  = {0};
	u64 reg_addr = 0;
	int ret = 0;

	/* register drop_cnt[1,4] corresponds to queue que[0,3] */
	reg_addr = tpp_base + TPP_CTRL_CPU_DROP_WQE_CNT(id_inst->que_id + 1);
	ret = uop_kern_readl(id_inst->dev_id, reg_addr, (u32 *)(void *)&drop_wqe_cnt);
	if (ret != 0) {
		linkcom_err("Read TPP_CTRL_CPU_DROP_WQE_CNT failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	*drop_cnt = drop_wqe_cnt.ctrl_cpu_qid_drop_wqe_cnt;
	return 0;
}

int linkcom_wait_tx_finish(struct linkcom_id_inst *id_inst, u64 tpp_base, u64 imp_base, u32 wqe_pi, u32 buf_pi, u32 drop_cnt)
{
	u32 drop_cnt_update = 0;
	u32 wqe_ci = 0;
	u32 buf_ci = 0;
	u32 count;
	int ret;

	for (count = 0; count < TX_PKT_WAIT_CNT; count++) { /* retry 300 times */
		ret = linkcom_get_tx_ci(id_inst, tpp_base, imp_base, &wqe_ci, &buf_ci);
		if (ret != 0) {
			return ret;
		}
		ret = linkcom_get_drop_cnt(id_inst, tpp_base, &drop_cnt_update);
		if (ret != 0) {
			return ret;
		}

		if ((wqe_ci == wqe_pi) && (((buf_ci + drop_cnt_update) % TPP_CTRL_CPU_DROP_CNT_MAX) == buf_pi)) {
			/*transmit is finish */
			if (drop_cnt == drop_cnt_update) {
				return 0;
			}
			return -ENETUNREACH; /*transmit is drop */
		}
		udelay(TX_PKT_WAIT_TIME); /* wait 100us for each time */
	}

	linkcom_err("Tx timeout, devid[%u] die[%u] qid[%u]\n", id_inst->dev_id, id_inst->die_id, id_inst->que_id);
	return -ETIMEDOUT;
}

int udie_v1_inband_send_link_pkt(struct linkcom_id_inst *id_inst, struct mami_pkt_send_para send_para)
{
	u64 tpp_base, imp_base;
	u32 wqe_ci = 0;
	u32 buf_ci = 0;
	u32 drop_cnt = 0;
	u32 wqe_pi, buf_pi;
	bool is_buf_equal, is_wqe_equal;
	int ret;

	tpp_base = linkcom_get_tp_tpp_base_addr(id_inst);
	if (tpp_base == 0) {
		linkcom_err("base addr is 0\n");
		return -EFAULT;
	}

	imp_base = linkcom_get_lqc_imp_base_addr(id_inst);
	if (imp_base == 0) {
		linkcom_err("base addr is 0\n");
		return -EFAULT;
	}

	ret = linkcom_get_tx_ci(id_inst, tpp_base, imp_base, &wqe_ci, &buf_ci);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_get_tx_pi(id_inst, tpp_base, imp_base, &wqe_pi, &buf_pi);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_get_drop_cnt(id_inst, tpp_base, &drop_cnt);
	if (ret != 0) {
		return ret;
	}

	is_buf_equal = (((drop_cnt + buf_ci) % TPP_CTRL_CPU_DROP_CNT_MAX) == buf_pi);
	is_wqe_equal = (wqe_ci == wqe_pi);
	if (!is_buf_equal || !is_wqe_equal) {
		linkcom_err("Tx send not finish.(dev_id=%u; die_id=%u; que_id=%u; wqe_ci=%u; wqe_pi=%u; buf_ci=%u; buf_pi=%u; drop_cnt=%u)\n",
			id_inst->dev_id, id_inst->die_id, id_inst->que_id, wqe_ci, wqe_pi, buf_ci, buf_pi, drop_cnt);
		return -EBUSY;
	}

	ret = linkcom_write_tpkt_buf(id_inst, send_para);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_set_wqe(id_inst, wqe_ci, send_para);
	if (ret != 0) {
		return ret;
	}

	wqe_pi = !wqe_ci;
	buf_pi = (buf_ci + drop_cnt + 1) & CTRL_CPU_MGTQ_WQE_CI_MSK;
	ret = linkcom_update_tpkt_pi(id_inst, tpp_base, imp_base, wqe_pi, buf_pi);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_wait_tx_finish(id_inst, tpp_base, imp_base, wqe_pi, buf_pi, drop_cnt);

	return ret;
}

int linkcom_get_24_28_rsv_reg(struct read_pkt_ctx *ctx)
{
	int ret;
	u64 reg_addr;

	ctx->rsv_base = linkcom_get_lqc_ta_dm_base_addr(&ctx->inst);
	if (ctx->rsv_base == 0) {
		linkcom_err("base addr is 0\n");
		return -EFAULT;
	}

	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_8(ctx->inst.que_id);
	ret = uop_kern_readl(ctx->inst.dev_id, reg_addr, &(ctx->dbg_reg.rsv_reg_8));
	if (ret != 0) {
		linkcom_err("Read DM_CFG_RSV_REG_8 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_4(ctx->inst.que_id);
	ret = uop_kern_readl(ctx->inst.dev_id, reg_addr, &(ctx->dbg_reg.rsv_reg_4));
	if (ret != 0) {
		linkcom_err("Read DM_CFG_RSV_REG_4 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_0(ctx->inst.que_id);
	ret = uop_kern_readl(ctx->inst.dev_id, reg_addr, &(ctx->dbg_reg.rsv_reg_0));
	if (ret != 0) {
		linkcom_err("read DM_CFG_RSV_REG_0 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_get_recv_status(struct read_pkt_ctx *ctx)
{
	int ret;
	u32 rsv_24;

	ret = linkcom_get_24_28_rsv_reg(ctx);
	if (ret != 0) {
		linkcom_err("Get DROP_CNT and DBG_RSV_REG failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	/* no abnormal scenario has been triggered */
	if ((ctx->dbg_reg.rsv_reg_8 == 0) && (ctx->dbg_reg.rsv_reg_4 == 0)) {
		linkcom_info("28 and 24 is all zero, normal\n");
		ctx->status = NORMAL_STS;
		return 0;
	}

	ret = uop_kern_readl(ctx->inst.dev_id, ctx->imp_base + IMP_RPKT_DROP_CNT(ctx->inst.que_id),
		&ctx->dbg_reg.drop_cnt);
	if (ret != 0) {
		linkcom_err("Read IMP_RPKT_DROP_CNT failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	rsv_24 = ctx->dbg_reg.rsv_reg_4;
	/* abnormal scenario has been triggered */
	if (ctx->dbg_reg.rsv_reg_8 != ctx->dbg_reg.drop_cnt) {
		/* packet drop detected in this round */
		rsv_24 += 1;				   /* bit28-bit0, low bits write original value + 1 */
		rsv_24 |= ((u32)0b11 << 30);   /* bit31 write 1, bit30 write 1 */
		rsv_24 &= ~((u32)0b1 << 29);   /* bit29 write 0 */
		ctx->status = ABNORMAL_WITH_DROP_STS;
	} else {
		/* no packet drop detected in this round */
		rsv_24 &= ~((u32)0b11 << 29);  /* bit30 write 0, bit29 write 0, other fields remain unchanged */
		ctx->status = ABNORMAL_NO_DROP_STS;
	}

	ret = uop_kern_writel(ctx->inst.dev_id, ctx->rsv_base + DM_CFG_RSV_REG_4(ctx->inst.que_id), rsv_24);
	if (ret != 0) {
		linkcom_err("Write DM_CFG_RSV_REG_4 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_process_abnormal_cqe(struct read_pkt_ctx *ctx, u32 rcqe_ci)
{
	int ret;
	u64 reg_addr;
	struct linkcom_rx_cqe abnormal_cqe = {0};
	u32 dbg_rsv_reg_20 = 0;

	/* read abnormal cqe */
	ret = linkcom_read_cqe(&ctx->inst, rcqe_ci, &abnormal_cqe);
	if (ret != 0) {
		return ret;
	}

	if ((ctx->dbg_reg.rsv_reg_4 & IMP_DBG_RSV_REG_24_MASK) % 8 == 0) { /* 8 abnormal events triggered, residual data fills one rpkbuf entry */
		dbg_rsv_reg_20 = ctx->dbg_reg.rsv_reg_0 + DIV_CEIL(abnormal_cqe.pkt_len, CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN) - 1;
	} else {
		dbg_rsv_reg_20 = ctx->dbg_reg.rsv_reg_0 + DIV_CEIL(abnormal_cqe.pkt_len, CTRL_CPU_MGTQ_RPKT_BUFF_ENREY_LEN);
	}

	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_0(ctx->inst.que_id);
	ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, dbg_rsv_reg_20);
	if (ret != 0) {
		linkcom_err("write DM_CFG_RSV_REG_0 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	ctx->dbg_reg.rsv_reg_0 = dbg_rsv_reg_20;
	return 0;
}

void linkcom_proc_rpkt_buff_entry(u32 cqe_entry_id, u32 rsv_20, u32 *entry_id)
{
	u32 dbg_rsv_reg_20 = rsv_20 & CTRL_CPU_MGTQ_RPKT_BUFF_ID_MASK;

	if (cqe_entry_id < dbg_rsv_reg_20) {
		*entry_id = cqe_entry_id + CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH - dbg_rsv_reg_20;
	} else {
		*entry_id = cqe_entry_id - dbg_rsv_reg_20;
	}
}

int linkcom_get_rpkt_buf(struct read_pkt_ctx *ctx, struct linkcom_rx_cqe *cqe, u8 *recv_buff)
{
	int ret;
	u32 entry_id = 0;

	if (ctx->status == NORMAL_STS) {
		entry_id = cqe->pkt_buf_entry_id;
	} else {
		linkcom_proc_rpkt_buff_entry(cqe->pkt_buf_entry_id, ctx->dbg_reg.rsv_reg_0, &entry_id);
	}

	ret = linkcom_read_rpkt_buf(ctx, entry_id, cqe->pkt_len, recv_buff);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int linkcom_append_rpkt_node_v1(struct read_pkt_ctx *ctx, struct linkcom_rx_cqe *cqe)
{
	int ret;
	u8 *buff = NULL;
	u32 rpkt_buff_depth = 0;

	if (ctx->rpkt_base == 0) {
		ctx->rpkt_base = linkcom_get_mgtq_rpkt_buf_base(&ctx->inst);
		if (ctx->rpkt_base == 0) {
			linkcom_err("rpkt_buf base addr is 0\n");
			return -EFAULT;
		}
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

	ret = linkcom_get_rpkt_buf(ctx, cqe, buff);
	if (ret != 0) {
		kfree(buff);
		return ret;
	}

	ret = linkcom_add_list_node(ctx->head, *cqe, buff);

	kfree(buff);

	return ret;
}

int linkcom_update_rsv_28reg(struct read_pkt_ctx *ctx)
{
	int ret;
	u32 rpkt_drop_cnt;
	u64 reg_addr;

	reg_addr = ctx->imp_base + IMP_RPKT_DROP_CNT(ctx->inst.que_id);
	ret = uop_kern_readl(ctx->inst.dev_id, reg_addr, &rpkt_drop_cnt);
	if (ret != 0) {
		linkcom_err("read IMP_RPKT_DROP_CNT failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	/* sync drop cnt to RSV28 register for comparison when processing this queue next time */
	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_8(ctx->inst.que_id);
	ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, rpkt_drop_cnt);
	if (ret != 0) {
		linkcom_err("Write DM_CFG_RSV_REG_8 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return ret;
}

int linkcom_update_rpkt_ci_with_drop(struct read_pkt_ctx *ctx)
{
	int ret;
	u64 reg_addr;
	u32 orig_block_cfg = LINKCOM_BLOCK_CFG_INIT_VAL;
	struct imp_ctrl_cpu_block_cfg cfg = ctx->cfg;

	reg_addr = ctx->imp_base + IMP_RPKT_BLOCK_CFG(ctx->inst.que_id);
	/* configure block_cfg to be the same as drop_cfg */
	if (cfg.block_cfg != cfg.drop_cfg) {
		orig_block_cfg = cfg.block_cfg;
		cfg.block_cfg = cfg.drop_cfg;
		ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, *(u32 *)(void *)&cfg);
		if (ret != 0) {
			linkcom_err("write IMP_RPKT_BLOCK_CFG failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
			return ret;
		}
	}

	ret = linkcom_write_rpkt_ci(ctx);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_update_rsv_28reg(ctx);
	if (ret != 0) {
		linkcom_err("Sync DM_CFG_RSV_REG_8 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	/* restore block_cfg */
	if (orig_block_cfg != LINKCOM_BLOCK_CFG_INIT_VAL) {
		cfg.block_cfg = orig_block_cfg;
		ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, *(u32 *)(void *)&cfg);
		if (ret != 0) {
			linkcom_err("write IMP_RPKT_BLOCK_CFG failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
			return ret;
		}
	}

	return ret;
}

int linkcom_process_normal_cqe_and_pkt(struct read_pkt_ctx *ctx, u32 rcqe_ci)
{
	int ret;
	struct linkcom_rx_cqe cqe = {0};

	ret = linkcom_read_cqe(&ctx->inst, rcqe_ci, &cqe);
	if (ret != 0) {
		return ret;
	}

	ret = linkcom_append_rpkt_node_v1(ctx, &cqe);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int linkcom_update_rpkt_ci_without_drop(struct read_pkt_ctx *ctx)
{
	u32 rpkt_drop_cnt = 0, dbg_rsv_reg_24 = 0, rcqe_ci = 0;
	u64 reg_addr = 0;
	int ret = 0;

	reg_addr = ctx->imp_base + IMP_RPKT_DROP_CNT(ctx->inst.que_id);
	ret = uop_kern_readl(ctx->inst.dev_id, reg_addr, &rpkt_drop_cnt);
	if (ret != 0) {
		linkcom_err("read IMP_RPKT_DROP_CNT failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	/* drop cnt not increased, refresh ci and end the flow */
	if (rpkt_drop_cnt == ctx->dbg_reg.drop_cnt) {
		return linkcom_write_rpkt_ci(ctx);
	}

	/* continue reading cqe to process packets, keep updating ctx->next_pkt_ci */
	rcqe_ci = ctx->rcqe_reg.rcqe_pi;
	while (((ctx->next_pkt_ci  + CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH - ctx->rpkt_reg.rpkt_ci) %
		CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH) < (CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH - ctx->cfg.drop_cfg) &&
		(ctx->next_pkt_ci != ctx->rpkt_reg.rpkt_ci)) {
		ret = linkcom_process_normal_cqe_and_pkt(ctx, rcqe_ci);
		if (ret != 0) {
			return ret;
		}
		rcqe_ci = (rcqe_ci + 1) % CTRL_CPU_MGTQ_CQE_DEPTH;
	}

	ret = linkcom_update_rpkt_ci_with_drop(ctx);
	if (ret != 0) {
		return ret;
	}

	dbg_rsv_reg_24 = ctx->dbg_reg.rsv_reg_4;
	dbg_rsv_reg_24 += 1;				 /* PUG: bit28-bit0 write original value + 1 */
	dbg_rsv_reg_24 |= ((u32)0b1 << 31);  /* PUG: bit31 write 1 */
	dbg_rsv_reg_24 &= ~((u32)0b1 << 30); /* PUG: bit30 write 0 */
	dbg_rsv_reg_24 |= ((u32)0b1 << 29);  /* PUG: bit29 write 1 */

	reg_addr = ctx->rsv_base + DM_CFG_RSV_REG_4(ctx->inst.que_id);
	ret = uop_kern_writel(ctx->inst.dev_id, reg_addr, dbg_rsv_reg_24);
	if (ret != 0) {
		linkcom_err("write DM_CFG_RSV_REG_4 failed. (dev_id=%u; ret=%d)\n", ctx->inst.dev_id, ret);
		return ret;
	}

	return 0;
}

int linkcom_update_rpkt_ci(struct read_pkt_ctx *ctx)
{
	if (ctx->status == ABNORMAL_WITH_DROP_STS) {
		return linkcom_update_rpkt_ci_with_drop(ctx);
	}

	return linkcom_update_rpkt_ci_without_drop(ctx);
}

int udie_v1_block_mode_read_rpkt(struct read_pkt_ctx *ctx, u32 *pkt_num)
{
	int ret;
	u32 rcqe_ci = 0;
	u32 pkt_cnt = (ctx->rcqe_reg.rcqe_pi + CTRL_CPU_MGTQ_CQE_DEPTH - ctx->rcqe_reg.rcqe_ci) % CTRL_CPU_MGTQ_CQE_DEPTH;

	if (pkt_cnt > (MAX_BUFF_CNT - ctx->head->count)) {
		linkcom_err("block mode rpkt cache is not enough. (remaining_cnt=%u; max_pkt_cnt=%u)\n",
			MAX_BUFF_CNT - ctx->head->count, pkt_cnt);
		return -ENOSPC;
	}

	for (rcqe_ci = ctx->rcqe_reg.rcqe_ci; rcqe_ci != ctx->rcqe_reg.rcqe_pi;
		rcqe_ci = (rcqe_ci + 1) % CTRL_CPU_MGTQ_CQE_DEPTH) {
		ret = linkcom_process_normal_cqe_and_pkt(ctx, rcqe_ci);
		if (ret != 0) {
			return ret;
		}
		*pkt_num = *pkt_num + 1;
	}

	ret = linkcom_write_rpkt_ci(ctx);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int udie_v1_drop_mode_read_rpkt(struct read_pkt_ctx *ctx, u32 *pkt_num)
{
	int ret;
	u32 rcqe_ci = 0;

	if ((MAX_BUFF_CNT - ctx->head->count) < (CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH - ctx->cfg.drop_cfg)) {
		linkcom_err("drop mode rpkt cache is not enough. (remaining_cnt=%u; max_pkt_cnt=%u)\n",
			MAX_BUFF_CNT - ctx->head->count, CTRL_CPU_MGTQ_RPKT_BUFF_DEPTH - ctx->cfg.drop_cfg);
		return -ENOSPC;
	}

	ret = linkcom_get_recv_status(ctx);
	if (ret != 0) {
		return ret;
	}

	for (rcqe_ci = ctx->rcqe_reg.rcqe_ci; rcqe_ci != ctx->rcqe_reg.rcqe_pi;
		rcqe_ci = (rcqe_ci + 1) % CTRL_CPU_MGTQ_CQE_DEPTH) {
		/* rsv24 bit29 or bit30 is non-zero, and this is the first cqe */
		if (((ctx->dbg_reg.rsv_reg_4 & IMP_DROP_INCREASE_MASK) != 0) && (rcqe_ci == ctx->rcqe_reg.rcqe_ci)) {
			ret = linkcom_process_abnormal_cqe(ctx, rcqe_ci);
			if (ret != 0) {
				return ret;
			}

			continue;
		}

		ret = linkcom_process_normal_cqe_and_pkt(ctx, rcqe_ci);
		if (ret != 0) {
			return ret;
		}
		*pkt_num = *pkt_num + 1;
	}

	ret = linkcom_update_rpkt_ci(ctx);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int udie_v1_inband_read_rpkt(struct linkcom_id_inst *id_inst, struct linkcom_list_head *head, u32 *pkt_num)
{
	int ret;
	struct read_pkt_ctx ctx = {0};

	if (head->count >= MAX_BUFF_CNT) {
#ifndef EMU_ST
		linkcom_err("list is overflow. (devid=%u, dieid=%u, queid=%u)\n", id_inst->dev_id, id_inst->die_id, id_inst->que_id);
		return -EOVERFLOW;
#endif
	}

	ctx.next_pkt_ci = LINKCOM_RPKT_NEXT_CI_INIT;
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

	ret = uop_kern_readl(id_inst->dev_id, ctx.imp_base + IMP_RPKT_BLOCK_CFG(id_inst->que_id), (u32 *)(void *)&ctx.cfg);
	if (ret != 0) {
		linkcom_err("read IMP_RPKT_BLOCK_CFG failed. (dev_id=%u; ret=%d)\n", id_inst->dev_id, ret);
		return ret;
	}

	if (ctx.cfg.drop_en == 1) {
		return udie_v1_drop_mode_read_rpkt(&ctx, pkt_num);
	}

	return udie_v1_block_mode_read_rpkt(&ctx, pkt_num);
}

void mgmtq_wqe_show(struct linkcom_id_inst *id_inst, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	u64 tpp_base, imp_base;
	u32 wqe_ci, buf_ci, wqe_pi, buf_pi;
	int ret;

	tpp_base = linkcom_get_tp_tpp_base_addr(id_inst);
	if (tpp_base == 0) {
		return;
	}
	imp_base = linkcom_get_lqc_imp_base_addr(id_inst);
	if (imp_base == 0) {
		linkcom_err("Show wqe imp base_addr is 0.\n");
		return;
	}
	ret = linkcom_get_tx_ci(id_inst, tpp_base, imp_base, &wqe_ci, &buf_ci);
	if (ret != 0) {
		linkcom_err("Show wqe get ci fail, ret[%d].\n", ret);
		return;
	}
	ret = linkcom_get_tx_pi(id_inst, tpp_base, imp_base, &wqe_pi, &buf_pi);
	if (ret != 0) {
		linkcom_err("Show wqe get pi fail, ret[%d].\n", ret);
		return;
	}

	show(priv, "  wqe: wqe_ci=%u; wqe_pi=%u; buf_ci=%u; buf_pi=%u;\n", wqe_ci, wqe_pi, buf_ci, buf_pi);
}

void mgmtq_cqe_show(struct linkcom_id_inst *id_inst, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	u64 reg_base;
	struct imp_ctrl_cpu_rcqe_buf rcqe = {0};
	struct imp_ctrl_cpu_rpkt_buf rpkt = {0};
	int ret;

	reg_base = linkcom_get_lqc_imp_base_addr(id_inst);
	if (reg_base == 0) {
		linkcom_err("Show lqc base addr is 0.\n");
		return;
	}

	ret = linkcom_get_cqe_ptr(id_inst, reg_base, &rcqe, &rpkt);
	if (ret != 0) {
		linkcom_err("Show get cqe ptr fail, ret[%d].\n", ret);
		return;
	}

	show(priv, "  cqe: rcqe_ci=%u; rcqe_pi=%u; rpkt_ci=%u; rpkt_pi=%u;\n",
		 rcqe.rcqe_ci, rcqe.rcqe_pi, rpkt.rpkt_ci, rpkt.rpkt_pi);
}

void udie_v1_mgmtq_show(struct linkcom_id_inst *id_inst, void (*show)(void *priv, const char *fmt, ...), void *priv)
{
	mgmtq_wqe_show(id_inst, show, priv);
	mgmtq_cqe_show(id_inst, show, priv);
}
