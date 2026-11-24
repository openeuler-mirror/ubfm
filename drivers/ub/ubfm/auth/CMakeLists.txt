# ===============================================================================
# SPDX-License-Identifier: GPL-2.0-only
# DESCRIPTION: auth
# Copyright (c) 2019-2020 Huawei Technologies Co., Ltd. All rights reserved.
# Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# ===============================================================================

cmake_minimum_required(VERSION 3.14.1)

if (NOT DEFINED RELEASE_VERSION OR RELEASE_VERSION STREQUAL "")
    set(RELEASE_VERSION "1.3.0.1")
endif()

if (NOT DEFINED API_VERSION OR API_VERSION STREQUAL "")
    set(API_VERSION "1")
endif()

# Set CMake DIRs.
set(CMAKE_AUTH_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Build linux core side ko, task name: drv_auth
list(APPEND MAKE_ARGS CONFIG_UB_UBFM=m)
if (${PRODUCT_SIDE} STREQUAL device)
    add_device_ko(LOCAL_MODULE drv_auth
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
    )
else()
    add_host_ko(LOCAL_MODULE drv_auth
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
    )
endif()
