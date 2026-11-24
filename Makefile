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
#
# Top-level Makefile for MAMI kernel-space driver (out-of-tree build).
#
# Supported targets:
#   make                # build all ko modules
#   make modules        # same as above
#   make clean          # clean all build outputs
#   make install        # install ko to INSTALL_MOD_PATH
#   make list           # list all buildable modules
#   make module M=<name>          # build a single module by name
#   make clean-module M=<name>    # clean a single module
#   make install-module M=<name>  # install a single module
#
# Build modes (local Linux or cross compile):
#   1) Local build (default):
#        make
#   2) Cross compile for arm64:
#        make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/arm64-kernel-src
#   3) Cross compile for x86 (on an arm64 host):
#        make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu- KDIR=/path/to/x86-kernel-src
#
# Override variables (see defaults below):
#   ARCH, CROSS_COMPILE, KDIR, KERNEL_SRC, INSTALL_MOD_PATH,
#   PRODUCT_SIDE, ENABLE_FPGA, ESL_MODE, BUILD_TYPE, DEBUG
#
# Optional feature switches (override on command line, e.g. make UBFM_ACC_CDMA=y):
#   UBFM_ACC_LDST, UBFM_ACC_CDMA, UBFM_ACC_PCIE_FPGA, UBFM_ACC_PCIE_I3C, UBFM_DEBUG
#   These map to CONFIG_UBFM_ACC_* Kconfig symbols; "y" enables, otherwise off.
#   Kconfig dependencies are mirrored automatically (see section 3b):
#     UBFM_ACC_CDMA=y     -> also enables UB_CDMA and UBFM_ACC_LDST
#     UBFM_ACC_PCIE_I3C=y -> also enables UBFM_ACC_PCIE_FPGA
# ===============================================================================

# ----------------------------------------------------------------------------
# 1. Toolchain & kernel source defaults
# ----------------------------------------------------------------------------
# KDIR: kernel source/build tree (must be configured & built).
#   - Local build:  /lib/modules/$(shell uname -r)/build
#   - Cross build:  point to the target kernel source tree.
KDIR            ?= /lib/modules/$(shell uname -r)/build
KERNEL_SRC      ?= $(KDIR)

# ARCH / CROSS_COMPILE follow the standard kernel convention.
# When ARCH is not specified, auto-detect from the host OS CPU architecture.
ARCH            ?= $(shell uname -m | sed -e 's/aarch64/arm64/' -e 's/x86_64/x86/' -e 's/i[3-6]86/x86/' -e 's/riscv64/riscv/')
CROSS_COMPILE   ?=

# Where to install modules (make install). Default: staged under ./install
INSTALL_MOD_PATH ?= $(MAMI_ROOT)/install

# ----------------------------------------------------------------------------
# 2. Build flavor defaults
# ----------------------------------------------------------------------------
# PRODUCT_SIDE: device | host  (device = target board, host = control CPU)
PRODUCT_SIDE    ?= device
# FPGA / ESL build toggles (default off).
ENABLE_FPGA     ?= off
ESL_MODE        ?= off
# BUILD_TYPE: Release | Debug
BUILD_TYPE      ?= Release
# Convenience alias: DEBUG=1 -> BUILD_TYPE=Debug
ifeq ($(DEBUG),1)
  BUILD_TYPE := Debug
endif

# MAMI_ROOT: canonical root for all MAMI source paths used by sub-makefiles.
# Derived from this Makefile's location (the kernel/ directory).
MAMI_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
MAMI_ROOT := $(patsubst %/,%,$(MAMI_ROOT))

# ----------------------------------------------------------------------------
# 3. UBFM feature switches -> CONFIG_UBFM_* symbols
# ----------------------------------------------------------------------------
# Defaults follow Kconfig: LDST=y, others=n.
UBFM_ACC_LDST     ?= y
UBFM_ACC_CDMA     ?=
UBFM_ACC_PCIE_FPGA ?=
UBFM_ACC_PCIE_I3C ?=
UBFM_DEBUG        ?=
ifeq ($(BUILD_TYPE),Debug)
  UBFM_DEBUG := y
endif

# ----------------------------------------------------------------------------
# 3b. Kconfig dependency mirroring
# ----------------------------------------------------------------------------
# Mirror the Kconfig `depends on` / `select` rules so a single switch is enough:
#   UBFM_ACC_CDMA=y     -> UBFM_ACC_LDST=y      (Kconfig: select UBFM_ACC_LDST)
#   UBFM_ACC_PCIE_I3C=y -> UBFM_ACC_PCIE_FPGA=y (Kconfig: select UBFM_ACC_PCIE_FPGA)
ifeq ($(filter y,$(UBFM_ACC_CDMA)),y)
  override UBFM_ACC_LDST := y
endif
ifeq ($(filter y,$(UBFM_ACC_PCIE_I3C)),y)
  override UBFM_ACC_PCIE_FPGA := y
endif

# ----------------------------------------------------------------------------
# 4. Resolve CONFIG_* and extra defines for the kernel build system
# ----------------------------------------------------------------------------
# CONFIG_UB_UBFM must be =m so obj-$(CONFIG_UB_UBFM) builds the modules.
# CONFIG_UB_CDMA auto-follows UBFM_ACC_CDMA (depends on UB_CDMA),
# so `make UBFM_ACC_CDMA=y` enables both by itself.
MAKE_KCONFIG := CONFIG_UB_UBFM=m \
                CONFIG_UB_CDMA=$(if $(filter y,$(UBFM_ACC_CDMA)),y,n)

# Per-feature CONFIG_* (only "y" passes through; empty -> not set).
ifdef UBFM_ACC_LDST
  MAKE_KCONFIG += CONFIG_UBFM_ACC_LDST=$(if $(filter y,$(UBFM_ACC_LDST)),y,n)
endif
ifdef UBFM_ACC_CDMA
  MAKE_KCONFIG += CONFIG_UBFM_ACC_CDMA=$(if $(filter y,$(UBFM_ACC_CDMA)),y,n)
endif
ifdef UBFM_ACC_PCIE_FPGA
  MAKE_KCONFIG += CONFIG_UBFM_ACC_PCIE_FPGA=$(if $(filter y,$(UBFM_ACC_PCIE_FPGA)),y,n)
endif
ifdef UBFM_ACC_PCIE_I3C
  MAKE_KCONFIG += CONFIG_UBFM_ACC_PCIE_I3C=$(if $(filter y,$(UBFM_ACC_PCIE_I3C)),y,n)
endif
ifdef UBFM_DEBUG
  MAKE_KCONFIG += CONFIG_UBFM_DEBUG=$(if $(filter y,$(UBFM_DEBUG)),y,n)
endif

# Build flags forwarded into the kernel module makefiles (EXTRA_CFLAGS consumers).
# FPGA_MODE / ESL_MODE expect "true" to activate (see sub-makefiles).
FPGA_FLAG  := $(if $(filter on,$(ENABLE_FPGA)),true,false)
ESL_FLAG   := $(if $(filter on,$(ESL_MODE)),true,false)

# Common variables forwarded to every sub-module build.
#   MAMI_ROOT      - canonical source root used by sub-makefiles for include
#                    paths and sub-Makefile includes.
#   PRODUCT_SIDE   - device/host routing.
#   TARGET_PRODUCT - kept for xshmem's `ifneq ($(TARGET_PRODUCT),UB)` branch.
FORWARD_VARS := MAMI_ROOT=$(MAMI_ROOT) \
                PRODUCT_SIDE=$(PRODUCT_SIDE) \
                TARGET_PRODUCT=MAMI \
                FPGA_MODE=$(FPGA_FLAG) \
                ESL_MODE=$(ESL_FLAG)

# Standard kernel cross-compile variables.
KERNEL_BUILD_VARS := ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
                     KERNEL_SRC=$(KERNEL_SRC) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH)

# ----------------------------------------------------------------------------
# 5. Module list (build order matters: ubfabric before fmdevm)
# ----------------------------------------------------------------------------
# Each entry: <dir>:<ko-name>. ko-name is informational (used by `list`).
UBFM_MODULES := \
    drivers/ub/ubfm/ubfabric:mami_ubfabric.ko \
    drivers/ub/ubfm/xshmem:drv_xshmem.ko \
    drivers/ub/ubfm/fmdevm:mami_fmdevm.ko \
    drivers/ub/ubfm/mctrlq:mami_mctrlq.ko \
    drivers/ub/ubfm/linkcom:mami_linkcom.ko \
    drivers/ub/ubfm/auth:drv_auth.ko

# Direct build dependencies by dir basename (transitively ordered).
# Mirrors KBUILD_EXTRA_SYMBOLS in each module Makefile: a single-module build
# must build these first so their Module.symvers exists for modpost.
MODULE_DEPS_fmdevm  := ubfabric
MODULE_DEPS_mctrlq  := ubfabric fmdevm
MODULE_DEPS_linkcom := ubfabric fmdevm

# ----------------------------------------------------------------------------
# 5b. Single-module selection (M=<name>)
# ----------------------------------------------------------------------------
# M selects a single module. It accepts any of:
#   - dir basename:   ubfabric, fmdevm, linkcom, mctrlq, auth, xshmem
#   - full dir path:  drivers/ub/ubfm/ubfabric
#   - ko name:        mami_ubfabric.ko
#   - ko basename:    mami_ubfabric, drv_auth
# Empty M => build all modules (used by `modules`/`clean`/`install`).
M :=

# At parse time, resolve M to the matching "dir:ko" entry (if any).
# SELECTED_DIR / SELECTED_KO hold the chosen module's dir and ko; empty if no
# match or M is empty. SELECTED_ENTRY is the full "dir:ko" string.
SELECTED_ENTRY :=
SELECTED_DIR   :=
SELECTED_KO    :=
ifneq ($(M),)
  # Walk UBFM_MODULES; the first entry whose dir basename, full dir, ko
  # name, or ko basename equals M wins. Uses $(foreach) + $(firstword).
  SELECTED_ENTRY := $(firstword \
    $(foreach e,$(UBFM_MODULES),\
      $(if $(filter $(M),\
        $(notdir $(word 1,$(subst :, ,$(e)))) \
        $(word 1,$(subst :, ,$(e))) \
        $(word 2,$(subst :, ,$(e))) \
        $(basename $(word 2,$(subst :, ,$(e))))),\
        $(e))))
  ifneq ($(SELECTED_ENTRY),)
    SELECTED_DIR := $(word 1,$(subst :, ,$(SELECTED_ENTRY)))
    SELECTED_KO  := $(word 2,$(subst :, ,$(SELECTED_ENTRY)))
  endif
endif

# Entries actually built by `modules` (and thus `install`): when M is set,
# prepend that module's direct dependencies so their Module.symvers exists
# before modpost runs for the selected module.
BUILD_ENTRIES := $(UBFM_MODULES)
ifneq ($(M),)
  BUILD_ENTRIES := $(foreach d,$(MODULE_DEPS_$(notdir $(SELECTED_DIR))) $(notdir $(SELECTED_DIR)),\
    $(foreach e,$(UBFM_MODULES),\
      $(if $(filter $(d),$(notdir $(word 1,$(subst :, ,$(e))))),$(e))))
endif

# ----------------------------------------------------------------------------
# 6. Build rules
# ----------------------------------------------------------------------------
.PHONY: all modules clean install list help check-kdir check-module \
        module clean-module install-module \
        ubfabric xshmem fmdevm mctrlq linkcom auth

all: modules

# modules: when M is set, build the selected module and its direct dependencies;
# otherwise iterate over all UBFM_MODULES.
# clean / install: when M is set, operate on SELECTED_ENTRY only.

modules: check-kdir check-module
	@echo "==> MAMI kernel build: ARCH=$(ARCH) CROSS_COMPILE='$(CROSS_COMPILE)' KDIR=$(KDIR)"
	@echo "==> PRODUCT_SIDE=$(PRODUCT_SIDE) BUILD_TYPE=$(BUILD_TYPE)"
	@for entry in $(BUILD_ENTRIES); do \
	    dir=$${entry%%:*}; \
	    ko=$${entry##*:}; \
	    echo ""; \
	    echo "==> Building $$ko ($$dir)"; \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$$dir \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) $(MAKE_KCONFIG) modules || \
	        { echo "!! Build failed: $$ko"; exit 1; }; \
	done
	@echo ""; echo "==> Build done. Artifacts:"
	@find $(MAMI_ROOT)/drivers/ub/ubfm -name '*.ko' -print | sort

clean: check-module
	@for entry in $(if $(M),$(SELECTED_ENTRY),$(UBFM_MODULES)); do \
	    dir=$${entry%%:*}; \
	    echo "==> Cleaning $$dir"; \
	    if [ -f "$(KDIR)/Makefile" ]; then \
	        $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$$dir \
	            $(KERNEL_BUILD_VARS) $(FORWARD_VARS) clean; \
	    else \
	        echo "   KDIR invalid, removing artifacts directly"; \
	        find $(MAMI_ROOT)/$$dir -type f \
	            \( -name '*.ko' -o -name '*.o' -o -name '*.mod' -o -name '*.mod.c' \
	               -o -name '.*.cmd' -o -name '.*.o.d' \) -delete; \
	        rm -rf $(MAMI_ROOT)/$$dir/.tmp_versions \
	               $(MAMI_ROOT)/$$dir/modules.order $(MAMI_ROOT)/$$dir/Module.symvers; \
	    fi; \
	done
	@echo "==> Clean done."

install: modules
	@echo "==> Installing ko to $(INSTALL_MOD_PATH)"
	@for entry in $(if $(M),$(SELECTED_ENTRY),$(UBFM_MODULES)); do \
	    dir=$${entry%%:*}; \
	    ko=$${entry##*:}; \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$$dir \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) \
	        INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install || \
	        { echo "!! Install failed: $$ko"; exit 1; }; \
	done
	@echo "==> Install done. See $(INSTALL_MOD_PATH)"

# module: build a single module (requires M=<name>).
module: check-kdir
	@if [ -z "$(M)" ]; then \
	    echo "!! Usage: make module M=<name>"; \
	    echo "!! Available: ubfabric xshmem fmdevm mctrlq linkcom auth"; \
	    exit 1; \
	fi
	@if [ -z "$(SELECTED_ENTRY)" ]; then \
	    echo "!! Module '$(M)' not found."; \
	    echo "!! Available: ubfabric xshmem fmdevm mctrlq linkcom auth"; \
	    exit 1; \
	fi
	@for d in $(MODULE_DEPS_$(notdir $(SELECTED_DIR))) $(notdir $(SELECTED_DIR)); do \
	    entry=$$(for e in $(UBFM_MODULES); do case "$${e%%:*}" in $$d|*/$$d) echo "$$e";; esac; done); \
	    dir=$${entry%%:*}; ko=$${entry##*:}; \
	    echo "==> Building $$ko ($$dir)"; \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$$dir \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) $(MAKE_KCONFIG) modules || \
	        { echo "!! Build failed: $$ko"; exit 1; }; \
	done
	@echo ""; echo "==> Build done. Artifact:"
	@find $(MAMI_ROOT)/drivers/ub/ubfm -name '*.ko' -print | sort

# clean-module: clean a single module (requires M=<name>).
clean-module:
	@if [ -z "$(M)" ]; then \
	    echo "!! Usage: make clean-module M=<name>"; \
	    exit 1; \
	fi
	@if [ -z "$(SELECTED_ENTRY)" ]; then \
	    echo "!! Module '$(M)' not found."; \
	    echo "!! Available: ubfabric xshmem fmdevm mctrlq linkcom auth"; \
	    exit 1; \
	fi
	@echo "==> Cleaning $(SELECTED_DIR)"
	@if [ -f "$(KDIR)/Makefile" ]; then \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$(SELECTED_DIR) \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) clean; \
	else \
	    echo "   KDIR invalid, removing artifacts directly"; \
	    find $(MAMI_ROOT)/$(SELECTED_DIR) -type f \
	        \( -name '*.ko' -o -name '*.o' -o -name '*.mod' -o -name '*.mod.c' \
	           -o -name '.*.cmd' -o -name '.*.o.d' \) -delete; \
	    rm -rf $(MAMI_ROOT)/$(SELECTED_DIR)/.tmp_versions \
	           $(MAMI_ROOT)/$(SELECTED_DIR)/modules.order \
	           $(MAMI_ROOT)/$(SELECTED_DIR)/Module.symvers; \
	fi
	@echo "==> Clean done."

# install-module: build + install a single module (requires M=<name>).
install-module: check-kdir
	@if [ -z "$(M)" ]; then \
	    echo "!! Usage: make install-module M=<name>"; \
	    exit 1; \
	fi
	@if [ -z "$(SELECTED_ENTRY)" ]; then \
	    echo "!! Module '$(M)' not found."; \
	    exit 1; \
	fi
	@for d in $(MODULE_DEPS_$(notdir $(SELECTED_DIR))) $(notdir $(SELECTED_DIR)); do \
	    entry=$$(for e in $(UBFM_MODULES); do case "$${e%%:*}" in $$d|*/$$d) echo "$$e";; esac; done); \
	    dir=$${entry%%:*}; ko=$${entry##*:}; \
	    echo "==> Building $$ko ($$dir)"; \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$$dir \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) $(MAKE_KCONFIG) modules || \
	        { echo "!! Build failed: $$ko"; exit 1; }; \
	done
	@echo "==> Installing $(SELECTED_KO) to $(INSTALL_MOD_PATH)"
	@$(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/$(SELECTED_DIR) \
	    $(KERNEL_BUILD_VARS) $(FORWARD_VARS) \
	    INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install || \
	    { echo "!! Install failed: $(SELECTED_KO)"; exit 1; }
	@echo "==> Install done. See $(INSTALL_MOD_PATH)"

list:
	@echo "Buildable MAMI kernel modules (in build order):"
	@for entry in $(UBFM_MODULES); do \
	    dir=$${entry%%:*}; ko=$${entry##*:}; \
	    printf "   %-28s -> %s\n" "$$dir" "$$ko"; \
	done
	@echo ""
	@echo "Single-module: make module M=<name>  (name = dir basename, ko name, or ko basename)"

# Convenience aliases: make ubfabric / make fmdevm / ...
# Resolves $@ to SELECTED_DIR at parse time via a per-target lookup.
ubfabric xshmem fmdevm mctrlq linkcom auth: check-kdir
	@for d in $(MODULE_DEPS_$@) $@; do \
	    echo "==> Building $$d [shortcut]"; \
	    $(MAKE) -C $(KDIR) M=$(MAMI_ROOT)/drivers/ub/ubfm/$$d \
	        $(KERNEL_BUILD_VARS) $(FORWARD_VARS) $(MAKE_KCONFIG) modules || \
	        { echo "!! Build failed: $$d"; exit 1; }; \
	done
	@echo ""; echo "==> Build done. Artifact:"
	@find $(MAMI_ROOT)/drivers/ub/ubfm -name '*.ko' -print | sort

check-module:
	@if [ -n "$(M)" ] && [ -z "$(SELECTED_ENTRY)" ]; then \
	    echo "!! Module '$(M)' not found."; \
	    echo "!! Available: ubfabric xshmem fmdevm mctrlq linkcom auth"; \
	    exit 1; \
	fi

check-kdir:
	@if [ ! -d "$(KDIR)" ] || [ ! -f "$(KDIR)/Makefile" ]; then \
	    echo "!! KDIR='$(KDIR)' is not a valid kernel build tree."; \
	    echo "!! Set KDIR=/path/to/kernel-src (must be configured & built)."; \
	    echo "!! Local example: KDIR=/lib/modules/\$$(uname -r)/build"; \
	    exit 1; \
	fi

help:
	@echo "MAMI kernel driver build"
	@echo ""
	@echo "Usage:"
	@echo "  make                         Build all ko (local kernel)"
	@echo "  make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=<arm64-ksrc>"
	@echo "                               Cross-build for arm64"
	@echo "  make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu- KDIR=<x86-ksrc>"
	@echo "                               Cross-build for x86 (from non-x86 host)"
	@echo "  make clean                   Remove build artifacts"
	@echo "  make install                 Build + stage ko to INSTALL_MOD_PATH"
	@echo "  make list                    List buildable modules"
	@echo ""
	@echo "Single-module build (M = dir name, ko name, or ko basename):"
	@echo "  make module M=ubfabric       Build one module"
	@echo "  make module M=mami_fmdevm.ko Build by ko name"
	@echo "  make clean-module M=<name>   Clean one module"
	@echo "  make install-module M=<name> Build + install one module"
	@echo "  make ubfabric|fmdevm|linkcom|mctrlq|auth|xshmem   Shortcut"
	@echo ""
	@echo "Common overrides:"
	@echo "  KDIR=<kernel-src>            Kernel source/build tree"
	@echo "  PRODUCT_SIDE=device|host     device=target board, host=control CPU"
	@echo "  UBFM_ACC_CDMA=y              Enable CDMA (needs UB_CDMA/LDST)"
	@echo "  UBFM_ACC_PCIE_FPGA=y         Enable PCIE-FPGA access"
	@echo "  UBFM_ACC_PCIE_I3C=y          Enable PCIE-I3C access (needs PCIE-FPGA)"
	@echo "  ENABLE_FPGA=on               FPGA build mode"
	@echo "  ESL_MODE=on                  ESL build mode"
	@echo "  BUILD_TYPE=Debug  or DEBUG=1 Debug build (enables UBFM_DEBUG)"
	@echo "  INSTALL_MOD_PATH=<dir>       Module install staging dir"
