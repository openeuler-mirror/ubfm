<!-- SPDX-License-Identifier: CC-BY-4.0 -->

<!-- Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved. -->

# MAMI 内核驱动

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](License/GPL-2.0-only)
[![Docs: CC-BY-4.0](https://img.shields.io/badge/Docs-CC--BY--4.0-lightgrey.svg)](License/CC-BY-4.0)

MAMI（管理架构与模型接口，Management Architecture and Model Interface）内核空间驱动为统一总线平台上的硬件结构（fabric）设备管理提供了 **UBFM（统一总线结构管理器，Unified Bus Fabric Manager）** 子系统。它包含 UBFABRIC 基础设施框架、FMDEVM 设备管理、LINKCOM 链路通信、MCTRLQ 控制/消息队列、AUTH 与 XSHMEM 模块。

## 目录

- [概述](#概述)
- [架构](#架构)
- [目录结构](#目录结构)
- [内核模块](#内核模块)
- [构建](#构建)
- [配置](#配置)
- [依赖](#依赖)
- [安装](#安装)
- [文档](#文档)
- [许可证](#许可证)

## 概述

UBFM 是 MAMI 框架的内核空间管理层。它提供：

- **UBFABRIC** — UB Fabric 基础设施框架：UB 设备命令分发（UCD）、
  UB 设备管理（UDM）、UB 设备资源（URES）、UB 设备操作（UOP）、
  UB 设备事件分发（UED）、UB 中断分发（UID）与 UB 设备日志（ULOG）
- **FMDEVM** — 设备管理层：硬件发现与探测、通过 UDM 创建/销毁设备、ID 映射、
  端口初始化、中断管理、故障管理与 RAS
- **LINKCOM** — 主机 CPU 与设备控制 CPU 之间基于 mgmtq 的报文通道
- **MCTRLQ** — CTRLQ 带内控制队列与 MSGQ 管理面消息队列
- **AUTH** — 用户空间进程与 `iammgr` 守护进程之间的标识代理
- **XSHMEM** — 管理 POOL 与 BLOCK 的共享内存框架

## 架构

```
┌─────────────────────────────────────────────────────────┐
│                    用户空间                              │
│  ┌─────────┬──────────┬──────────┬──────────┬────────┐  │
│  │ UBFABRIC│ MCTRLQ   │ LINKCOM  │ AUTH     │ XSHMEM │  │
│  │ API     │ API      │ API      │ API      │ API    │  │
│  └────┬────┴────┬─────┴────┬─────┴────┬─────┴───┬────┘  │
│       │         │          │          │         │       │
│       └─────────┴──────────┴──────────┴─────────┘       │
│                          │ ioctl / 字符设备              │
├──────────────────────────┼───────────────────────────────┤
│                          ▼                               │
│                    内核空间                               │
│  ┌──────────────────────────────────────────────────┐    │
│  │            UBFABRIC 核心 (mami_ubfabric)          │    │
│  │  ┌─────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐    │    │
│  │  │ UCD │ │ UDM│ │ UED│ │ UID│ │UOP │ │URES│    │    │
│  │  └─────┘ └────┘ └────┘ └────┘ └────┘ └────┘    │    │
│  └──────────────────────────────────────────────────┘    │
│  ┌────────────┐ ┌────────────┐ ┌────────────────────┐    │
│  │ MCTRLQ     │ │ LINKCOM    │ │ FMDEVM              │    │
│  │ (ctrlq+msgq)│ │ (mgmtq+mng)│ │(devmng+hw_adapt+   │    │
│  │            │ │            │ │ irqmgr+faultmgr+ras)│   │
│  └────────────┘ └────────────┘ └────────────────────┘    │
│  ┌────────────┐ ┌────────────┐                            │
│  │   AUTH     │ │  XSHMEM    │                            │
│  └────────────┘ └────────────┘                            │
│                          │                                │
│                          ▼                                │
│              硬件（PCIe / LDST / CDMA）                   │
└─────────────────────────────────────────────────────────┘
```

## 目录结构

```
kernel/
├── Makefile                          # 顶层树外构建 Makefile
├── License/                          # 许可证文本
│   ├── GPL-2.0-only                  # GPL-2.0-only（源代码）
│   ├── CC-BY-4.0                     # CC-BY-4.0（文档）
│   └── README.md                     # 许可证适用范围说明
├── README.md                         # 本文件
│
├── include/uapi/ub/ubfm/             # 用户空间 API 头文件
│   ├── ubfm_common.h                 # 公共定义
│   ├── fmdevm.h                      # 设备管理器 ioctl 接口
│   ├── linkcom.h                     # 链路通信接口
│   ├── mctrlq.h                      # CTRLQ/MSGQ 接口
│   └── ubfabric.h                    # UBFABRIC（UCD）接口
│
├── drivers/ub/ubfm/                  # 内核模块源码
│   ├── Kconfig                       # Kconfig 选项
│   ├── Makefile                      # 树内 Kconfig 构建入口
│   │
│   ├── ubfabric/                     # UBFABRIC 核心模块 (mami_ubfabric.ko)
│   │   ├── ucd/                      # UB 设备命令分发（UB Device Command Dispatch）
│   │   ├── udm/                      # UB 设备管理（UB Device Management）
│   │   ├── ued/                      # UB 设备事件分发（UB Device Event Dispatch）
│   │   ├── uid/                      # UB 中断分发（UB Interrupt Dispatch）
│   │   ├── uop/                      # UB 设备操作（UB Device Operation，kmap/umap/uops）
│   │   ├── ures/                     # UB 设备资源（UB Device Resource）
│   │   └── ulog/                     # UB 设备日志（UB Device Log）
│   │
│   ├── fmdevm/                       # FMDEVM 设备管理 (mami_fmdevm.ko)
│   │   ├── devmng/                   # 设备 ID 到 SoC ID 映射
│   │   ├── mng/                      # 控制接口（字符设备）与 IDM（ID Management）
│   │   ├── hw_adapt/                 # 硬件适配层
│   │   │   ├── hw_info/              # 每芯片硬件信息表
│   │   │   ├── hw_map/               # 硬件地址映射
│   │   │   ├── hw_probe/             # 设备探测（本地/邻近/远端）
│   │   │   └── hw_ucmi/              # 统一控制模块接口（Unified Control Module Interface）
│   │   ├── port_mng/                 # 端口管理
│   │   ├── irqmgr/                   # 中断管理（MSI / USI 即 UB System Interrupt）
│   │   ├── faultmgr/                 # 故障管理与注入
│   │   ├── ras/                      # RAS（可靠性、可用性、可服务性）
│   │   └── vsock/                    # 用于 LDST 访问的虚拟套接字（Virtual Socket）
│   │
│   ├── linkcom/                      # LINKCOM 链路通信 (mami_linkcom.ko)
│   │   ├── core/                     # LINKCOM 核心
│   │   ├── mgmtq/                    # 管理队列（Management Queue，mgmtq）
│   │   ├── mng/                      # 链路管理
│   │   └── soc/                      # SoC 专用适配器（udie_v1/v2）
│   │
│   ├── mctrlq/                       # MCTRLQ 队列 (mami_mctrlq.ko)
│   │   ├── ctrlq/                    # CTRLQ 带内控制队列
│   │   └── msgq/                     # MSGQ 管理面消息队列
│   │
│   ├── auth/                         # AUTH 驱动（drv_auth.ko）
│   └── xshmem/                       # XSHMEM 共享内存（drv_xshmem.ko）
│
└── Documentation/ub/ubfm/            # 各模块文档
    ├── auth/
    ├── fmdevm/
    ├── linkcom/
    ├── mctrlq/
    ├── ubfabric/
    └── xshmem/
```

## 内核模块

| 模块 | 文件 | 描述 |
|--------|------|-------------|
| `mami_ubfabric` | `mami_ubfabric.ko` | UBFABRIC 核心：UCD、UDM、UED、UID、UOP、URES、ULOG |
| `mami_fmdevm` | `mami_fmdevm.ko` | FMDEVM：设备管理、硬件适配、端口、中断、故障、RAS |
| `mami_linkcom` | `mami_linkcom.ko` | LINKCOM：mgmtq 报文通道、报文解析、SoC 适配器 |
| `mami_mctrlq` | `mami_mctrlq.ko` | MCTRLQ：CTRLQ 带内控制队列与 MSGQ 消息队列 |
| `drv_auth` | `drv_auth.ko` | AUTH：用户空间进程与 `iammgr` 之间的标识代理 |
| `drv_xshmem` | `drv_xshmem.ko` | XSHMEM：POOL/BLOCK 共享内存框架 |

**构建顺序**：`ubfabric` → `fmdevm` → `mctrlq` / `linkcom`。`auth` 与 `xshmem` 无跨模块构建依赖。

## 构建

### 前置条件

- 已配置并构建的 Linux 内核源码树（`KDIR`）
- 与目标内核匹配的工具链（本机 GCC 或交叉编译器）
- GNU Make

### 快速开始

```bash
cd kernel

# 本地构建（使用当前运行内核的构建树）
make

# 面向 arm64 的交叉构建
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/arm64-kernel-src

# 面向 x86 的交叉构建
make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu- KDIR=/path/to/x86-kernel-src
```

运行 `make help` 查看完整选项列表。

### 构建选项

| 变量 | 默认值 | 描述 |
|----------|---------|-------------|
| `KDIR` | `/lib/modules/$(uname -r)/build` | 内核源码/构建树 |
| `KERNEL_SRC` | `$(KDIR)` | 传递给子构建的内核源码树 |
| `ARCH` | 自动检测（`uname -m`） | 目标架构（arm64/x86） |
| `CROSS_COMPILE` | *（空）* | 交叉编译器前缀 |
| `MAMI_ROOT` | 由 `kernel/Makefile` 所在路径自动推导 | 子 Makefile 使用的 MAMI 源码根目录（可覆盖） |
| `PRODUCT_SIDE` | `device` | `device`（目标板）或 `host`（控制 CPU） |
| `UBFM_ACC_LDST` | `y` | 启用 LDST 访问通道 |
| `UBFM_ACC_CDMA` | *（关闭）* | 启用 CDMA 访问通道（同时启用 LDST 与 `UB_CDMA`） |
| `UBFM_ACC_PCIE_FPGA` | *（关闭）* | 启用 PCIE-FPGA 访问通道 |
| `UBFM_ACC_PCIE_I3C` | *（关闭）* | 启用 PCIE-I3C 访问通道（同时启用 PCIE-FPGA） |
| `UBFM_DEBUG` | *（关闭）* | 带故障注入、RAS 测试接口的调试构建 |
| `BUILD_TYPE` | `Release` | `Release` 或 `Debug`；`Debug` 隐含 `UBFM_DEBUG=y` |
| `DEBUG` | *（空）* | `DEBUG=1` 强制 `BUILD_TYPE=Debug` |
| `ENABLE_FPGA` | `off` | FPGA 构建模式（`on` 时添加 `-DFPGA_MODE_ENABLE`） |
| `ESL_MODE` | `off` | ESL（仿真系统级）构建模式（`on` 时添加 `-DMAMI_ESL_MODE`） |
| `INSTALL_MOD_PATH` | `./install` | 模块安装暂存目录 |

### 构建目标

| 目标 | 描述 |
|--------|-------------|
| `make` / `make modules` | 构建所有内核模块 |
| `make clean` | 移除构建产物 |
| `make install` | 构建并将 `.ko` 文件暂存到 `INSTALL_MOD_PATH` |
| `make list` | 列出可构建的模块 |
| `make help` | 显示使用信息 |
| `make module M=<name>` | 构建单个模块（`M` 可为目录名、完整目录、`.ko` 名或 `.ko` 基名） |
| `make clean-module M=<name>` | 清理单个模块 |
| `make install-module M=<name>` | 构建并安装单个模块 |
| `make ubfabric\|xshmem\|fmdevm\|mctrlq\|linkcom\|auth` | 单模块构建快捷目标（含依赖） |

## 配置

### Kconfig 选项

这些符号定义在 `drivers/ub/ubfm/Kconfig` 中。树外构建时，顶层 Makefile 会把
等价的 `CONFIG_*` 值通过内核构建命令行传入，因此使用上面列出的命令行变量即可。

| 符号 | 类型 | 默认值 | 描述 |
|--------|------|---------|-------------|
| `UB_UBFM` | tristate（依赖 `UB`） | `n` | 启用 UBFM 驱动支持 |
| `UBFM_ACC_LDST` | bool | `y` | LDST（Load/Store）访问通道 |
| `UBFM_ACC_CDMA` | bool（依赖 `UB_UBFM && UB_CDMA`，select `UBFM_ACC_LDST`） | `n` | CDMA（DMA 引擎）访问通道 |
| `UBFM_ACC_PCIE_FPGA` | bool | `n` | PCIE+FPGA 带外访问通道 |
| `UBFM_ACC_PCIE_I3C` | bool（select `UBFM_ACC_PCIE_FPGA`） | `n` | PCIE+I3C 带外访问通道 |
| `UBFM_DEBUG` | bool | `n` | 调试模式（故障注入、RAS 测试） |

Kconfig 符号会被翻译成源码实际判断的预处理宏：
`CFG_UBFM_SUPPORT_LDST`、`CFG_UBFM_SUPPORT_CDMA`、
`CFG_UBFM_SUPPORT_PCIE_FPGA`、`CFG_UBFM_SUPPORT_PCIE_I3C` 和
`CFG_BUILD_DEBUG`。构建形态还会引入 `FPGA_MODE_ENABLE`（`ENABLE_FPGA=on`）、
`MAMI_ESL_MODE`（`ESL_MODE=on`）和 `MAMI_SUPPORT_HOST_CTRLCPU`
（`PRODUCT_SIDE=host`）。

### 访问通道

UBFM 支持多种硬件访问通道。请根据您的平台选择一种：

- **LDST** — 本地设备的直接 Load/Store 访问（默认启用）
- **CDMA** — 基于 DMA 引擎的带内设备访问（需要 `CONFIG_UB_CDMA`）
- **PCIE-FPGA** — 通过带 FPGA 背板的 PCIe 总线进行带外访问
- **PCIE-I3C** — 带 I3C 边带控制的带外访问（隐式依赖 PCIE-FPGA）

通道组合通过上面的 `UBFM_ACC_*` 开关选择（例如 `make UBFM_ACC_PCIE_FPGA=y`）。

## 依赖

### 内核依赖

- `CONFIG_UB`（UB 总线子系统，`UB_UBFM` 依赖）
- `CONFIG_UB_CDMA`（可选，CDMA 访问通道必需）

### 用户空间依赖

- MAMI 用户空间库（配套仓库）
- 标准 Linux 用户空间头文件（libc、libudev）

## 安装

```bash
# 构建并暂存模块
make install

# 模块被暂存到 ./install/lib/modules/<内核版本>/extra/

# 加载模块（按依赖顺序）
sudo modprobe mami_ubfabric
sudo modprobe mami_fmdevm
sudo modprobe mami_mctrlq
sudo modprobe mami_linkcom
sudo modprobe drv_auth
sudo modprobe drv_xshmem

# 验证
lsmod | grep -E 'mami_|drv_'
```

## 文档

各模块文档位于 `Documentation/ub/ubfm/`：

- `auth/` — AUTH 驱动接口与流程
- `fmdevm/` — FMDEVM 设备管理、硬件适配、中断/RAS
- `linkcom/` — LINKCOM 协议与报文格式
- `mctrlq/` — CTRLQ 与 MSGQ API 与用法
- `ubfabric/` — UBFABRIC 基础设施框架
- `xshmem/` — XSHMEM POOL/BLOCK 共享内存管理

## 许可证

许可证适用范围详见 [License/README.md](License/README.md)。

- `drivers/` 与 `include/` 下的源代码、头文件、Kconfig 和 Makefile 采用
  [GPL-2.0-only](License/GPL-2.0-only) 授权
  （`SPDX-License-Identifier: GPL-2.0-only`）。
- `Documentation/` 下的文档采用 [CC-BY-4.0](License/CC-BY-4.0) 授权
  （`SPDX-License-Identifier: CC-BY-4.0`）。

```
Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2.
```
