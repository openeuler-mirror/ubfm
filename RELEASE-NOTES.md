<!-- SPDX-License-Identifier: CC-BY-4.0 -->

<!-- Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved. -->

# Release Notes

## v26.11.0

### New Features

* UBFM Kernel Driver
    * Initial public release of the UBFM (Unified Bus Fabric Manager) driver.
    * UBFABRIC Infrastructure Framework
        * Core fabric infrastructure framework providing UCD, URES, UDM, UED, UID, ULOG and UOP services for command dispatch, resource and device lifecycle management, events, interrupts, mapping and logging.
    * FMDEVM Device Management
        * Device management layer covering hardware discovery and probe, device creation/destruction through UDM, identifier mapping, port initialization, interrupt management, fault management and RAS.
    * LINKCOM Link Communication
        * mgmtq-based packet channel between the host CPU and the device control CPU.
    * MCTRLQ Queues
        * CTRLQ in-band control queue and MSGQ management-plane message queue.
    * AUTH
        * Identification broker between user-space processes and the `iammgr` daemon.
    * XSHMEM Shared Memory
        * Shared memory framework managing POOLs and BLOCKs with FSC and fixed-size block allocation algorithms.
    * Access Channels and Build
        * Supports LDST, CDMA, PCIE-FPGA and PCIE-I3C access channels.
