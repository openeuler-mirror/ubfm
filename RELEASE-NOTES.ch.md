<!-- SPDX-License-Identifier: CC-BY-4.0 -->

<!-- Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved. -->

# 版本说明

## v26.11.0

### 新增特性

* UBFM 内核驱动
    * UBFM（Unified Bus Fabric Manager，统一总线 Fabric 管理）驱动首次公开发布。
    * UBFABRIC 基础设施框架
        * 核心 Fabric 基础设施框架，提供 UCD、URES、UDM、UED、UID、ULOG 与 UOP 服务，覆盖命令分发、资源与设备生命周期管理、事件、中断、映射及日志。
    * FMDEVM 设备管理
        * 设备管理层，负责硬件发现与探测、通过 UDM 创建设备/销毁设备、标识映射、端口初始化、中断管理、故障管理与 RAS。
    * LINKCOM 链路通信
        * 基于 mgmtq 的主机 CPU 与设备控制 CPU 之间的报文通道。
    * MCTRLQ 队列
        * 包含 CTRLQ 带内控制队列与 MSGQ 管理面消息队列。
    * AUTH
        * 用户态进程与 `iammgr` 守护进程之间的身份识别代理。
    * XSHMEM 共享内存
        * 管理 POOL 与 BLOCK 的共享内存框架，支持 FSC 与固定大小块两种分配算法。
    * 访问通道与构建
        * 支持 LDST、CDMA、PCIE-FPGA 与 PCIE-I3C 访问通道。
