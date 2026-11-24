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
#ifndef USR_AUTH_IOCTL_H
#define USR_AUTH_IOCTL_H

#define AUTH_PROCNAME_SIZE 256U

struct proc_info {
	pid_t pid;
	char proc_name[AUTH_PROCNAME_SIZE + 1];
	int timeout;
};

#define AUTH_IOC_MAGIC					'A'
#define AUTH_IOCTL_GET_PROCNAME			_IOWR(AUTH_IOC_MAGIC, 1, struct proc_info)
#define AUTH_IOCTL_GET_PID				_IOR(AUTH_IOC_MAGIC, 2, struct proc_info)
#define AUTH_IOCTL_SET_PROCNAME			_IOW(AUTH_IOC_MAGIC, 3, struct proc_info)

#endif
