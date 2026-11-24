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
#include "linkcom_pkt_parse.h"
#include <linux/stddef.h>
#include "mami_string.h"

u16 linkcom_ntohs(u16 netshort)
{
	return ((netshort >> 8) & 0xFF) | ((netshort << 8) & 0xFF00); /* 8 : shift by byte */
}

void linkcom_ubh_ntoh(void *pkt, void *ubh, u32 len)
{
	u8 *src = (u8 *)pkt;
	u8 *dst = (u8 *)ubh;
	u16 val_net = 0;
	u16 val_host = 0;
	u32 cnt = len / sizeof(u16);
	u32 i;
	(void)ubfm_memcpy(dst, len, src, len);
	for (i = 0; i < cnt; i++) {
		ubfm_memcpy(&val_net, sizeof(u16), src + i * sizeof(u16), sizeof(u16));
		val_host = linkcom_ntohs(val_net);
		ubfm_memcpy(dst + i * sizeof(u16), sizeof(u16), &val_host, sizeof(u16));
	}
}

void linkcom_parse_tah(void *pkt, u32 len)
{
	void *mask_pkt = NULL;
	u32 mask_len = 0;

	if (len < sizeof(linkcom_ub_tah)) {
		return;
	}
	mask_len = len - sizeof(linkcom_ub_tah);
	if (mask_len == 0) {
		return;
	}
	mask_pkt = pkt + sizeof(linkcom_ub_tah);
	/* mask etah and payload */
	(void)ubfm_memset(mask_pkt, mask_len, 0, mask_len);
}

void linkcom_parse_mem_tah(void *pkt, u32 len)
{
	void *mask_pkt = NULL;
	u32 mask_len = 0;

	if (len < sizeof(linkcom_ub_mem_tah)) {
		return;
	}
	mask_len = len - sizeof(linkcom_ub_mem_tah);
	if (mask_len == 0) {
		return;
	}
	mask_pkt = pkt + sizeof(linkcom_ub_mem_tah);
	/* mask etah and payload */
	(void)ubfm_memset(mask_pkt, mask_len, 0, mask_len);
}

void linkcom_parse_cip(void *pkt, u32 len)
{
	linkcom_ub_cip cip = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cip)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cip);
	next_pkt = pkt + sizeof(linkcom_ub_cip);
	linkcom_ubh_ntoh(pkt, &cip, sizeof(linkcom_ub_cip));
	switch (cip.nlp) {
	case 0:  /* 0 : 32Bit UPI + 256bit UEID */
		if (next_len > sizeof(linkcom_ub_upi_eid)) {
			next_len = next_len - sizeof(linkcom_ub_upi_eid);
			next_pkt = next_pkt + sizeof(linkcom_ub_upi_eid);
			linkcom_parse_tah(next_pkt, next_len);
		}
		break;
	case 1:  /* 1 : 16bit UPI and 40bit UEID */
		if (next_len > sizeof(linkcom_ub_compact_upi_eid)) {
			next_len = next_len - sizeof(linkcom_ub_compact_upi_eid);
			next_pkt = next_pkt + sizeof(linkcom_ub_compact_upi_eid);
			linkcom_parse_tah(next_pkt, next_len);
		}
		break;
	case 2:  /* 2 : TAH */
		linkcom_parse_tah(next_pkt, next_len);
		break;
	default:
		break;
	}
}

void linkcom_parse_tph_by_nlp(void *pkt, u32 len, u8 nlp)
{
	u32 next_len = 0;
	void *next_pkt = NULL;

	switch (nlp) {
	case 0:  /* 0 : TAH / ATAH */
		linkcom_parse_tah(pkt, len);
		break;
	case 1:  /* 1 : 32Bit UPI + 256bit UEID */
		if (len > sizeof(linkcom_ub_upi_eid)) {
			next_len = len - sizeof(linkcom_ub_upi_eid);
			next_pkt = pkt + sizeof(linkcom_ub_upi_eid);
			linkcom_parse_tah(next_pkt, next_len);
		}
		break;
	case 2:  /* 2 : reserved */
		break;
	case 3:  /* 3 : CIP */
		linkcom_parse_cip(pkt, len);
		break;
	default:
		break;
	}
}

void linkcom_parse_stph(void *pkt, u32 len)
{
	linkcom_ub_stph stph = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_stph)) {
		return;
	}
	linkcom_ubh_ntoh(pkt, &stph, sizeof(linkcom_ub_stph));
	next_len = len - sizeof(linkcom_ub_stph);
	next_pkt = pkt + sizeof(linkcom_ub_stph);
	linkcom_parse_tph_by_nlp(next_pkt, next_len, stph.nlp);
}

void linkcom_parse_tph(void *pkt, u32 len)
{
	linkcom_ub_tph tph = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;
	u8 tp_opcode = 0;

	if (len < sizeof(linkcom_ub_tph)) {
		return;
	}

	next_len = len - sizeof(linkcom_ub_tph);
	next_pkt = pkt + sizeof(linkcom_ub_tph);

	linkcom_ubh_ntoh(pkt, &tph, sizeof(linkcom_ub_tph));
	if (tph.mn != 0) {
		linkcom_parse_stph(next_pkt, next_len);
		return;
	}
	tp_opcode = (tph.tp_opcode) & 0x7F;  /* get TPOpcode[6:0] */
	switch (tp_opcode) {
	case LINKCOM_UB_TP_OP_0:
	case LINKCOM_UB_TP_OP_1:
	case LINKCOM_UB_TP_OP_2:
	case LINKCOM_UB_TP_OP_4:
	case LINKCOM_UB_TP_OP_7:
		linkcom_parse_tph_by_nlp(next_pkt, next_len, tph.nlp);
		break;
	default:
		break;
	}
}

void linkcom_parse_ctph(void *pkt, u32 len)
{
	linkcom_ub_ctph ctph = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_ctph)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_ctph);
	next_pkt = pkt + sizeof(linkcom_ub_ctph);

	linkcom_ubh_ntoh(pkt, &ctph, sizeof(linkcom_ub_ctph));
	switch (ctph.nlp) {
	case 0:  /* 0 : TAH */
		linkcom_parse_tah(next_pkt, next_len);
		break;
	case 1:  /* 1 : 32Bit UPI + 256bit UEID */
		if (next_len > sizeof(linkcom_ub_upi_eid)) {
			next_len = next_len - sizeof(linkcom_ub_upi_eid);
			next_pkt = next_pkt + sizeof(linkcom_ub_upi_eid);
			linkcom_parse_tah(next_pkt, next_len);
		}
		break;
	case 2:  /* 2 : 16Bit UPI + 40bit UEID */
		if (next_len > sizeof(linkcom_ub_compact_upi_eid)) {
			next_len = next_len - sizeof(linkcom_ub_compact_upi_eid);
			next_pkt = next_pkt + sizeof(linkcom_ub_compact_upi_eid);
			linkcom_parse_tah(next_pkt, next_len);
		}
		break;
	case 3:  /* 3 : CIP */
		linkcom_parse_cip(next_pkt, next_len);
		break;
	default:
		break;
	}
}

void linkcom_parse_cfg3_nth(void *pkt, u32 len)
{
	linkcom_ipv4 *ipv4 = NULL;
	linkcom_udp *udp = NULL;
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cfg3_nth)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cfg3_nth);
	next_pkt = pkt + sizeof(linkcom_ub_cfg3_nth);
	if (next_len < sizeof(linkcom_ipv4)) {
		return;
	}
	ipv4 = (linkcom_ipv4 *)next_pkt;
	if (ipv4->protocol != 0x11) { /* 0x11 : UDP */
		return;
	}
	next_len = next_len - sizeof(linkcom_ipv4);
	next_pkt = next_pkt + sizeof(linkcom_ipv4);
	if (next_len < sizeof(linkcom_udp)) {
		return;
	}
	udp = (linkcom_udp *)(next_pkt);
	if (linkcom_ntohs(udp->dst_port) != LINKCOM_UDP_DST_PORT_UB) {
		return;
	}
	next_pkt = next_pkt + sizeof(linkcom_udp);
	next_len = next_len - sizeof(linkcom_udp);

	linkcom_parse_tph(next_pkt, next_len);
}

void linkcom_parse_cfg4_nth(void *pkt, u32 len)
{
	linkcom_ipv6 *ipv6 = NULL;
	linkcom_udp *udp = NULL;
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cfg4_nth)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cfg4_nth);
	next_pkt = pkt + sizeof(linkcom_ub_cfg4_nth);
	if (next_len < sizeof(linkcom_ipv6)) {
		return;
	}
	ipv6 = (linkcom_ipv6 *)next_pkt;
	if (ipv6->next_header != 0x11) { /* 0x11 : UDP */
		return;
	}
	next_len = next_len - sizeof(linkcom_ipv6);
	next_pkt = next_pkt + sizeof(linkcom_ipv6);
	if (next_len < sizeof(linkcom_udp)) {
		return;
	}
	udp = (linkcom_udp *)(next_pkt);
	if (linkcom_ntohs(udp->dst_port) != LINKCOM_UDP_DST_PORT_UB) {
		return;
	}
	next_pkt = next_pkt + sizeof(linkcom_udp);
	next_len = next_len - sizeof(linkcom_udp);
	linkcom_parse_tph(next_pkt, next_len);
}

void linkcom_parse_cfg6_nth(void *pkt, u32 len)
{
	linkcom_ub_cfg6_nth cfg6_nth = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cfg6_nth)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cfg6_nth);
	next_pkt = pkt + sizeof(linkcom_ub_cfg6_nth);

	linkcom_ubh_ntoh(pkt, &cfg6_nth, sizeof(linkcom_ub_cfg6_nth));
	switch (cfg6_nth.nlp) {
	case 0:  /* 0 : with CTPH */
		linkcom_parse_ctph(next_pkt, next_len);
		break;
	case 1:  /* 1 : without TPH, CIP, EID and TAH, with 16bit UPI, used for enumeration management protocol */
		break;
	default:
		break;
	}
}

void linkcom_parse_cfg7_nth(void *pkt, u32 len)
{
	linkcom_ub_cfg7_nth cfg7_nth = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cfg7_nth)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cfg7_nth);
	next_pkt = pkt + sizeof(linkcom_ub_cfg7_nth);
	linkcom_ubh_ntoh(pkt, &cfg7_nth, sizeof(linkcom_ub_cfg7_nth));
	switch (cfg7_nth.nlp) {
	case 0:  /* 0 : with CTPH */
		linkcom_parse_ctph(next_pkt, next_len);
		break;
	case 1:  /* 1 : without TPH, CIP, EID and TAH, with 16bit UPI, used for enumeration management protocol */
		break;
	case 2:  /* 2 : with TPH */
		linkcom_parse_tph(next_pkt, next_len);
		break;
	default:
		break;
	}
}

void linkcom_parse_cfg9_nth(void *pkt, u32 len)
{
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_cfg9_nth)) {
		return;
	}
	next_len = len - sizeof(linkcom_ub_cfg9_nth);
	next_pkt = pkt + sizeof(linkcom_ub_cfg9_nth);
	linkcom_parse_mem_tah(next_pkt, next_len);
}

void linkcom_parse_lh(void *pkt, u32 len)
{
	linkcom_ub_lh ublh = {0};
	void *next_pkt = NULL;
	u32 next_len = 0;

	if (len < sizeof(linkcom_ub_lh)) {
		return;
	}

	next_len = len - sizeof(linkcom_ub_lh);
	next_pkt = pkt + sizeof(linkcom_ub_lh);
	linkcom_ubh_ntoh(pkt, &ublh, sizeof(linkcom_ub_lh));
	switch (ublh.cfg) {
	case LINKCOM_UB_CFG_3:
		linkcom_parse_cfg3_nth(next_pkt, next_len);
		break;
	case LINKCOM_UB_CFG_4:
		linkcom_parse_cfg4_nth(next_pkt, next_len);
		break;
	case LINKCOM_UB_CFG_6:
		linkcom_parse_cfg6_nth(next_pkt, next_len);
		break;
	case LINKCOM_UB_CFG_7:
		linkcom_parse_cfg7_nth(next_pkt, next_len);
		break;
	case LINKCOM_UB_CFG_9:
		linkcom_parse_cfg9_nth(next_pkt, next_len);
		break;
	default:
		break;
	}
}

void linkcom_recv_pkt_mask(struct mami_pkt_recv_para *para)
{
	switch (para->reason) {
	case LINKCOM_PROTOCOL_CHECK_FAIL:
	case LINKCOM_ROUTE_MISS:
	case LINKCOM_NPI_CHECK_FAIL:
	case LINKCOM_MIRROR_TO_CTRLCPU:
		linkcom_parse_lh(para->pktBuff, para->recvLen);
		return;
	default:
		break;
	}
}
