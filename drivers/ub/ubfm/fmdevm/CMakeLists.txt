# SPDX-License-Identifier: GPL-2.0-only
#
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

if(${ENABLE_FPGA})
    list(APPEND MAKE_ARGS
        FPGA_MODE=true
    )
endif ()

if(${ENABLE_ESL})
    list(APPEND MAKE_ARGS
        ESL_MODE=true
    )
endif ()

if (${INBAND})
    list(APPEND MAKE_ARGS CONFIG_UBFM_ACC_LDST=y CONFIG_UBFM_ACC_CDMA=y)
else ()
    list(APPEND MAKE_ARGS CONFIG_UBFM_ACC_PCIE_FPGA=y)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    list(APPEND MAKE_ARGS CONFIG_UBFM_DEBUG=y)
endif()

if (${PRODUCT_SIDE} STREQUAL host)
    set(mami_fmdevm_depends mami_ubfabric)

    list(APPEND MAKE_ARGS
        PRODUCT_SIDE=host
    )
    add_host_ko(LOCAL_MODULE mami_fmdevm
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${mami_fmdevm_depends}"
        CFLAGS_MODULE ${EXTRA_VAR})
else()
        if (${TARGET_LINUX_DISTRIBUTOR_ID} STREQUAL rtos)
                set(DISTRIBUTOR_ID rtos)
        else()
                set(DISTRIBUTOR_ID linux)
        endif()
    list(APPEND MAKE_ARGS
        PRODUCT_SIDE=device
    )
    set(mami_fmdevm_depends mami_ubfabric)
    add_device_ko(LOCAL_MODULE mami_fmdevm
        KO_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}
        MAKE_ARGS ${MAKE_ARGS}
        TARGETE_DPENDS "${mami_fmdevm_depends}"
        CFLAGS_MODULE ${EXTRA_VAR})
endif()
