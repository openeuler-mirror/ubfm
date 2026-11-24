<!-- SPDX-License-Identifier: CC-BY-4.0 -->

<!-- Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved. -->

# MAMI Kernel Driver

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](License/GPL-2.0-only)
[![Docs: CC-BY-4.0](https://img.shields.io/badge/Docs-CC--BY--4.0-lightgrey.svg)](License/CC-BY-4.0)

MAMI (Management Architecture and Model Interface) kernel-space driver provides the **UBFM (Unified Bus Fabric Manager)** subsystem for managing hardware fabric devices on unified bus platforms. It includes the UBFABRIC infrastructure framework, FMDEVM device management, LINKCOM link communication, MCTRLQ control/message queues, AUTH and XSHMEM modules.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Directory Structure](#directory-structure)
- [Kernel Modules](#kernel-modules)
- [Build](#build)
- [Configuration](#configuration)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Documentation](#documentation)
- [License](#license)

## Overview

UBFM is the kernel-space management layer for the MAMI framework. It provides:

- **UBFABRIC** — the UB Fabric infrastructure framework: UB Device Command
  Dispatch (UCD), UB Device Management (UDM), UB Device Resource (URES),
  UB Device Operation (UOP), UB Device Event Dispatch (UED), UB Interrupt
  Dispatch (UID) and UB Device Log (ULOG)
- **FMDEVM** — the device management layer: hardware discovery and probing,
  device creation/destruction through UDM, ID mapping, port initialization,
  interrupt management, fault management and RAS
- **LINKCOM** — a mgmtq based packet channel between the host CPU and the
  device control CPU
- **MCTRLQ** — the CTRLQ in-band control queue and the MSGQ management-plane
  message queue
- **AUTH** — an identification broker between user-space processes and the
  `iammgr` daemon
- **XSHMEM** — a shared memory framework managing POOLs and BLOCKs

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    User Space                           │
│  ┌─────────┬──────────┬──────────┬──────────┬────────┐  │
│  │ UBFABRIC│ MCTRLQ   │ LINKCOM  │ AUTH     │ XSHMEM │  │
│  │ API     │ API      │ API      │ API      │ API    │  │
│  └────┬────┴────┬─────┴────┬─────┴────┬─────┴───┬────┘  │
│       │         │          │          │         │       │
│       └─────────┴──────────┴──────────┴─────────┘       │
│                          │ ioctl / char dev              │
├──────────────────────────┼───────────────────────────────┤
│                          ▼                               │
│                    Kernel Space                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │            UBFABRIC Core (mami_ubfabric)         │    │
│  │  ┌─────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐      │    │
│  │  │ UCD │ │ UDM│ │ UED│ │ UID│ │UOP │ │URES│      │    │
│  │  └─────┘ └────┘ └────┘ └────┘ └────┘ └────┘      │    │
│  └──────────────────────────────────────────────────┘    │
│  ┌────────────┐ ┌────────────┐ ┌──────────────────── ┐   │
│  │ MCTRLQ     │ │ LINKCOM    │ │ FMDEVM              │   │
│  │ (ctrlq+msgq)│ │ (mgmtq+mng)│ │(devmng+hw_adapt+   │   │
│  │            │ │            │ │ irqmgr+faultmgr+ras)│   │
│  └────────────┘ └────────────┘ └────────────────────┘    │
│  ┌────────────┐ ┌────────────┐                           │
│  │   AUTH     │ │  XSHMEM    │                           │
│  └────────────┘ └────────────┘                           │
│                          │                               │
│                          ▼                               │
│              Hardware (PCIe / LDST / CDMA)               │
└─────────────────────────────────────────────────────────┘
```

## Directory Structure

```
kernel/
├── Makefile                          # Top-level out-of-tree build Makefile
├── License/                          # License texts
│   ├── GPL-2.0-only                  # GPL-2.0-only (source code)
│   ├── CC-BY-4.0                     # CC-BY-4.0 (documentation)
│   └── README.md                     # License scope description
├── README.md                         # This file
│
├── include/uapi/ub/ubfm/             # User-space API headers
│   ├── ubfm_common.h                 # Common definitions
│   ├── fmdevm.h                      # Device manager ioctl interface
│   ├── linkcom.h                     # LINKCOM interface
│   ├── mctrlq.h                      # CTRLQ/MSGQ interface
│   └── ubfabric.h                    # UBFABRIC (UCD) interface
│
├── drivers/ub/ubfm/                  # Kernel module source
│   ├── Kconfig                       # Kconfig options
│   ├── Makefile                      # In-tree Kconfig build entry
│   │
│   ├── ubfabric/                     # UBFABRIC core module (mami_ubfabric.ko)
│   │   ├── ucd/                      # UB Device Command Dispatch
│   │   ├── udm/                      # UB Device Management
│   │   ├── ued/                      # UB Device Event Dispatch
│   │   ├── uid/                      # UB Interrupt Dispatch
│   │   ├── uop/                      # UB Device Operation (kmap/umap/uops)
│   │   ├── ures/                     # UB Device Resource
│   │   └── ulog/                     # UB Device Log
│   │
│   ├── fmdevm/                       # FMDEVM device management (mami_fmdevm.ko)
│   │   ├── devmng/                   # Device-id to SoC-id mapping
│   │   ├── mng/                      # Control interface (char dev) + IDM (ID Management)
│   │   ├── hw_adapt/                 # Hardware adaptation layer
│   │   │   ├── hw_info/              # Per-chip hardware info tables
│   │   │   ├── hw_map/               # Hardware address mapping
│   │   │   ├── hw_probe/             # Device probe (local/near/remote)
│   │   │   └── hw_ucmi/              # Unified Control Module Interface
│   │   ├── port_mng/                 # Port management
│   │   ├── irqmgr/                   # Interrupt management (MSI / USI = UB System Interrupt)
│   │   ├── faultmgr/                 # Fault management & injection
│   │   ├── ras/                      # RAS (Reliability, Availability, Serviceability)
│   │   └── vsock/                    # Virtual Socket for LDST access
│   │
│   ├── linkcom/                      # LINKCOM link communication (mami_linkcom.ko)
│   │   ├── core/                     # LINKCOM core
│   │   ├── mgmtq/                    # Management Queue (mgmtq)
│   │   ├── mng/                      # Link management
│   │   └── soc/                      # SoC-specific adapters (udie_v1/v2)
│   │
│   ├── mctrlq/                       # MCTRLQ queues (mami_mctrlq.ko)
│   │   ├── ctrlq/                    # CTRLQ in-band control queue
│   │   └── msgq/                     # MSGQ management-plane message queue
│   │
│   ├── auth/                         # AUTH driver (drv_auth.ko)
│   └── xshmem/                       # XSHMEM shared memory (drv_xshmem.ko)
│
└── Documentation/ub/ubfm/            # Per-module documentation
    ├── auth/
    ├── fmdevm/
    ├── linkcom/
    ├── mctrlq/
    ├── ubfabric/
    └── xshmem/
```

## Kernel Modules

| Module | File | Description |
|--------|------|-------------|
| `mami_ubfabric` | `mami_ubfabric.ko` | UBFABRIC core: UCD, UDM, UED, UID, UOP, URES, ULOG |
| `mami_fmdevm` | `mami_fmdevm.ko` | FMDEVM: device management, hardware adaptation, port, interrupt, fault, RAS |
| `mami_linkcom` | `mami_linkcom.ko` | LINKCOM: mgmtq packet channel, packet parsing, SoC adapters |
| `mami_mctrlq` | `mami_mctrlq.ko` | MCTRLQ: CTRLQ in-band control queue and MSGQ message queue |
| `drv_auth` | `drv_auth.ko` | AUTH: identification broker between user-space processes and `iammgr` |
| `drv_xshmem` | `drv_xshmem.ko` | XSHMEM: POOL/BLOCK shared memory framework |

**Build order** : `ubfabric` → `fmdevm` → `mctrlq` / `linkcom`. `auth` and `xshmem` have no cross-module build dependencies.

## Build

### Prerequisites

- A configured and built Linux kernel source tree (`KDIR`)
- A toolchain matching the target kernel (native GCC or a cross-compiler)
- GNU Make

### Quick Start

```bash
cd kernel

# Local build (uses the running kernel's build tree)
make

# Cross build for arm64
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/arm64-kernel-src

# Cross build for x86
make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu- KDIR=/path/to/x86-kernel-src
```

Run `make help` for the full option list.


### Build Options

| Variable | Default | Description |
|----------|---------|-------------|
| `KDIR` | `/lib/modules/$(uname -r)/build` | Kernel source/build tree |
| `KERNEL_SRC` | `$(KDIR)` | Kernel source tree forwarded to the sub-builds |
| `ARCH` | Auto-detected (`uname -m`) | Target architecture (arm64/x86) |
| `CROSS_COMPILE` | *(empty)* | Cross-compiler prefix |
| `MAMI_ROOT` | Derived from `kernel/Makefile` location | Canonical MAMI source root used by sub-makefiles (auto-detected; can be overridden) |
| `PRODUCT_SIDE` | `device` | `device` (target board) or `host` (control CPU) |
| `UBFM_ACC_LDST` | `y` | Enable LDST access channel |
| `UBFM_ACC_CDMA` | *(off)* | Enable CDMA access channel (also enables LDST and `UB_CDMA`) |
| `UBFM_ACC_PCIE_FPGA` | *(off)* | Enable PCIE-FPGA access channel |
| `UBFM_ACC_PCIE_I3C` | *(off)* | Enable PCIE-I3C access channel (also enables PCIE-FPGA) |
| `UBFM_DEBUG` | *(off)* | Debug build with fault injection, RAS test interfaces |
| `BUILD_TYPE` | `Release` | `Release` or `Debug`; `Debug` implies `UBFM_DEBUG=y` |
| `DEBUG` | *(empty)* | `DEBUG=1` forces `BUILD_TYPE=Debug` |
| `ENABLE_FPGA` | `off` | FPGA build mode (`on` adds `-DFPGA_MODE_ENABLE`) |
| `ESL_MODE` | `off` | ESL (Emulation System Level) build mode (`on` adds `-DMAMI_ESL_MODE`) |
| `INSTALL_MOD_PATH` | `./install` | Module install staging directory |

### Build Targets

| Target | Description |
|--------|-------------|
| `make` / `make modules` | Build all kernel modules |
| `make clean` | Remove build artifacts |
| `make install` | Build + stage `.ko` files to `INSTALL_MOD_PATH` |
| `make list` | List buildable modules |
| `make help` | Show usage information |
| `make module M=<name>` | Build a single module (`M` accepts a directory basename, full directory, `.ko` name, or `.ko` basename) |
| `make clean-module M=<name>` | Clean a single module |
| `make install-module M=<name>` | Build + install a single module |
| `make ubfabric\|xshmem\|fmdevm\|mctrlq\|linkcom\|auth` | Shortcut for building one module (and its dependencies) |

## Configuration

### Kconfig Options

The symbols are defined in `drivers/ub/ubfm/Kconfig`. For out-of-tree builds the
top-level Makefile passes the equivalent `CONFIG_*` values on the kernel build
command line, so the command-line variables listed above are enough.

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `UB_UBFM` | tristate (depends on `UB`) | `n` | Enable UBFM driver support |
| `UBFM_ACC_LDST` | bool | `y` | LDST (Load/Store) access channel |
| `UBFM_ACC_CDMA` | bool (depends on `UB_UBFM && UB_CDMA`, selects `UBFM_ACC_LDST`) | `n` | CDMA (DMA engine) access channel |
| `UBFM_ACC_PCIE_FPGA` | bool | `n` | PCIE+FPGA outband access channel |
| `UBFM_ACC_PCIE_I3C` | bool (selects `UBFM_ACC_PCIE_FPGA`) | `n` | PCIE+I3C outband access channel |
| `UBFM_DEBUG` | bool | `n` | Debug mode (fault injection, RAS test) |

The Kconfig symbols are translated into the preprocessor macros tested by the
sources: `CFG_UBFM_SUPPORT_LDST`, `CFG_UBFM_SUPPORT_CDMA`,
`CFG_UBFM_SUPPORT_PCIE_FPGA`, `CFG_UBFM_SUPPORT_PCIE_I3C` and
`CFG_BUILD_DEBUG`. The build flavor adds `FPGA_MODE_ENABLE` (`ENABLE_FPGA=on`),
`MAMI_ESL_MODE` (`ESL_MODE=on`) and `MAMI_SUPPORT_HOST_CTRLCPU`
(`PRODUCT_SIDE=host`).

### Access Channels

UBFM supports multiple hardware access channels. Select one based on your platform:

- **LDST** — Direct Load/Store access for local devices (enabled by default)
- **CDMA** — DMA engine-based access for inband devices (requires `CONFIG_UB_CDMA`)
- **PCIE-FPGA** — Outband access through the PCIe bus with an FPGA backplane
- **PCIE-I3C** — Outband access with I3C sideband control (implies PCIE-FPGA)

The channel set is selected with the `UBFM_ACC_*` switches above (for example
`make UBFM_ACC_PCIE_FPGA=y`).

## Dependencies

### Kernel Dependencies

- `CONFIG_UB` (UB bus subsystem, required by `UB_UBFM`)
- `CONFIG_UB_CDMA` (optional, required for the CDMA access channel)

### User-Space Dependencies

- MAMI user-space library (companion repository)
- Standard Linux userspace headers (libc, libudev)

## Installation

```bash
# Build and stage modules
make install

# Modules are staged under ./install/lib/modules/<kernel-version>/extra/

# Load modules (in dependency order)
sudo modprobe mami_ubfabric
sudo modprobe mami_fmdevm
sudo modprobe mami_mctrlq
sudo modprobe mami_linkcom
sudo modprobe drv_auth
sudo modprobe drv_xshmem

# Verify
lsmod | grep -E 'mami_|drv_'
```

## Documentation

Per-module documentation is located in `Documentation/ub/ubfm/`:

- `auth/` — AUTH driver interface and flow
- `fmdevm/` — FMDEVM device management, hardware adaptation, interrupt/RAS
- `linkcom/` — LINKCOM protocol and packet formats
- `mctrlq/` — CTRLQ and MSGQ API and usage
- `ubfabric/` — UBFABRIC infrastructure framework
- `xshmem/` — XSHMEM POOL/BLOCK shared memory management

## License

See [License/README.md](License/README.md) for the full license scope.

- Source code, header files, Kconfig and Makefiles under `drivers/` and
  `include/` are licensed under
  [GPL-2.0-only](License/GPL-2.0-only)
  (`SPDX-License-Identifier: GPL-2.0-only`).
- Documentation under `Documentation/` is licensed under
  [CC-BY-4.0](License/CC-BY-4.0)
  (`SPDX-License-Identifier: CC-BY-4.0`).

```
Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2.

```
