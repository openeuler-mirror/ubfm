/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef _MAMI_STRING_H_
#define _MAMI_STRING_H_

#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <stdarg.h>

#define UBFM_STRING_MAX_LEN 0x7fffffffUL
#define UBFM_MEM_MAX_LEN 0x7fffffffUL

#define UBFM_SPRINTF(dst, dst_len, fmt, ...)							\
({																		\
	int ret;															\
																		\
	if (((dst) == NULL) || ((fmt) == NULL)) {							\
		ret = -EINVAL;													\
	} else if (((dst_len) == 0) || ((dst_len) > UBFM_STRING_MAX_LEN)) {	\
		ret = -ERANGE;													\
	} else {															\
		ret = snprintf((dst), (dst_len), (fmt),	##__VA_ARGS__);			\
	}																	\
	ret;																\
})

static inline int ubfm_strcpy(char *dst, size_t dst_len, const char *src)
{
	size_t src_len;

	if ((dst == NULL) || (src == NULL)) {
		return -EINVAL;
	}

	if ((dst_len == 0) || (dst_len > UBFM_STRING_MAX_LEN)) {
		return -ERANGE;
	}

	src_len = strnlen(src, dst_len) + 1;
	if (src_len > dst_len) {
		dst[0] = '\0';
		return -EINVAL;
	}

	strcpy(dst, src);
	return 0;
}

static inline int ubfm_strncpy(char *dst, size_t dst_len, const char *src, size_t cnt)
{
	if ((dst == NULL) || (src == NULL)) {
		return -EINVAL;
	}

	if ((dst_len == 0) || (dst_len > UBFM_STRING_MAX_LEN)) {
		return -ERANGE;
	}

	if (cnt >= dst_len) {
		dst[0] = '\0';
		return -EINVAL;
	}

	strncpy(dst, src, cnt);
	dst[cnt] = '\0';
	return 0;
}

static inline int ubfm_strcat(char *dst, size_t dst_len, const char *src)
{
	size_t dst_cur_len;
	size_t src_max_len;
	size_t src_len;

	if ((dst == NULL) || (src == NULL)) {
		return -EINVAL;
	}

	if ((dst_len == 0) || (dst_len > UBFM_STRING_MAX_LEN)) {
		return -ERANGE;
	}

	dst_cur_len = strnlen(dst, dst_len);
	src_max_len = dst_len - dst_cur_len;
	src_len = strnlen(src, src_max_len);
	if (src_len >= src_max_len) {
		dst[0] = '\0';
		return -EINVAL;
	}

	strcat(dst, src);
	return 0;
}

static inline int ubfm_memcpy(void *dst, size_t dst_len, const void *src, size_t src_len)
{
	if ((dst == NULL) || (src == NULL)) {
		return -EINVAL;
	}

	if ((dst_len == 0) || (dst_len > UBFM_MEM_MAX_LEN)) {
		return -ERANGE;
	}

	if (src_len > dst_len) {
		memset(dst, 0, dst_len);
		return -EINVAL;
	}

	memcpy(dst, src, src_len);
	return 0;
}

static inline int ubfm_memset(void *dst, size_t dst_len, int ch, size_t cnt)
{
	if (dst == NULL) {
		return -EINVAL;
	}

	if ((dst_len == 0) || (dst_len > UBFM_MEM_MAX_LEN)) {
		return -ERANGE;
	}

	if (cnt > dst_len) {
		memset(dst, ch & 0xFF, dst_len);
		return -EINVAL;
	}

	memset(dst, ch & 0xFF, cnt);
	return 0;
}
#endif
