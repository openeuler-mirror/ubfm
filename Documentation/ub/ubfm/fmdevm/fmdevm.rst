.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


*******************************
FMDEVM Device Management Driver
*******************************

This document describes the ``mami_fmdevm`` kernel driver (module name
``ubdevm``). The driver is licensed under **GPL-2.0-only** and is part of the
MAMI UB fabric subsystem.

The FMDEVM driver is the device management layer of the MAMI UB fabric. It is
responsible for hardware discovery and probing, device creation/destruction
through the UDM, logical/physical identifier mapping, port information
initialization, interrupt management, fault event management and RAS error
reporting. It builds on top of the UBFABRIC infrastructure framework (UCD,
URES, UDM, UID, UOP, ULOG) and exposes a single character device and a set of
in-kernel APIs that other MAMI drivers consume.

Acronyms
========

The following acronyms are used throughout this document:

================================ ================================================
Acronym                          Full Name
================================ ================================================
MAMI                             Management Architecture and Model Interface
UB                               Unified Bus
UBFABRIC                         UB Fabric Infrastructure Framework
UBUS                             UB System Bus
UCD                              UB Device Command Dispatch
URES                             UB Device Resource
UDM                              UB Device Management
UED                              UB Device Event Dispatch
UOP                              UB Device Operation
ULOG                             UB Device Log
UID                              UB Interrupt Dispatch
UCMI                             Unified Control Module Interface
USI                              UB System Interrupt
IDM                              ID Management
RAS                              Reliability, Availability, Serviceability
CDMA                             Crystal DMA
LDST                             Load/Store
vsock                            Virtual Socket
GUID                             Globally Unique Identifier
================================ ================================================

Overview
========

The FMDEVM driver uses the ``feature_loader`` mechanism to auto-initialize its
submodules in ordered stages. The platform driver ``ubdevm`` registers a
platform device and, on probe, runs the ``sysfs`` scoped feature initializers
so that debug sysfs groups are attached to the platform device. The main
submodules are loaded in the following order:

  - Stage 1: local ras register table init
  - Stage 2: vsock server, ucmi ops registration
  - Stage 3: mng (management char device + UCD command registration), local
    probe, ras table registration
  - Stage 5: hw_info chip-info collectors
  - Stage 6: hw_adapt, port_mng, irqmgr (usi/msi), faultmgr, faultmgr sysfs,
    ras sysfs (UDM notifier registration stage)
  - Stage 9: ras core, devmng procfs, pcie probe, remote probe, debug procfs/sysfs

.. code-block:: none

     User Space
         |
         |-- ioctl on /dev/ubdevm  (UCD_CMD_MNG / UCD_CMD_FAULTMGR / UCD_CMD_USI)
         |
         v
     +----------------------------------------------------------+
     |                           UCD                            |
     |  routes commands to registered feature handlers          |
     +----------------------------------------------------------+
         |             |              |
         v             v              v
     +--------+   +-----------+   +---------+
     |  mng   |   | faultmgr  |   |  usi    |
     | (MNG)  |   |(FAULTMGR) |   | (USI)   |
     +--------+   +-----------+   +---------+
         |
         v
     +----------------------------------------------------------+
     |                           UDM                            |
     |  device add/remove triggers priority notifier chain      |
     |  PRI0: hw_adapt   PRI2: port_mng / faultmgr              |
     |  PRI3: ras         PRI4: usi / msi                       |
     +----------------------------------------------------------+
         |          |          |          |          |
         v          v          v          v          v
     +--------+ +--------+ +--------+ +--------+ +--------+
     |hw_adapt| |port_mng| |faultmgr| |   ras  | | irqmgr |
     +--------+ +--------+ +--------+ +--------+ +--------+
         |
         v
     +----------------------------------------------------------+
     |              hw_adapt sub-modules                        |
     |  hw_probe | hw_info | hw_map | hw_ucmi                   |
     +----------------------------------------------------------+

Below is the summary for the submodules in the FMDEVM driver:

  - 1) mng: management module, creates the ``/dev/ubdevm`` character device and
       registers the MNG UCD commands (device create/destroy/probe, local device
       create, get local devid, ucmi window lost check). Hosts the IDM which
       provides logical/physical identifier translation, and the feature query
       helpers.
  - 2) devmng: device-id to SoC-id (slot/socket) mapping and procfs debug
       interface.
  - 3) hw_adapt: hardware adaptation layer. Probes MAMI devices (local CDMA,
       remote CDMA, PCIe FPGA, DPU message channel), collects per-chip hardware
       information, maps memory/register regions into URES and manages UCMI
       dynamic windows.
  - 4) port_mng: port management module. Reads the NL port link mode, lane mode
       and RX/TX target-id routing tables from hardware and publishes a
       ``port_info_table`` into URES.
  - 5) irqmgr: interrupt manager. Provides two backends: ``usi`` (UB System
       Interrupt, for LDST/CDMA access types and vsock-based interrupt
       forwarding) and ``msi`` (PCIe MSI/MSI-X for FPGA builds). Both feed
       interrupts into the UBFABRIC UID framework.
  - 6) faultmgr: fault manager. Maintains a ring-buffer fault event queue and
       exposes UCD commands to add/get fault events with wait-queue blocking.
       Reports fault events to the kernel log.
  - 7) ras: RAS module. Registers a UID interrupt handler for ``UID_TYPE_RAS``,
       parses the RAS data header/entries from device memory, decodes Local RAS
       and Protocol RAS error records and reports them as fault events.
  - 8) vsock: vsock socket helper. Provides a kernel AF_VSOCK server/client
       transport used by the usi module to forward interrupts across virtual
       machine boundaries.

Internal Dependencies
~~~~~~~~~~~~~~~~~~~~~

This subsection describes the internal dependency constraints of the FMDEVM
driver, including external module dependencies, submodule init stage ordering,
UDM notifier chain priorities, intra-module data dependencies, the device
lifecycle sequence, and build-time (Kconfig) dependencies.

External Module Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``mami_fmdevm.ko`` module declares a hard ko-level dependency on
``mami_ubfabric.ko`` (see ``drivers/ub/ubfm/fmdevm/CMakeLists.txt``), so the
UBFABRIC framework module must load and complete ``module_init`` before
FMDEVM. ubfabric initializes its sub-frameworks -- UCD, URES, UDM, UID, UOP,
ULOG -- in stages 0-4, so by the time FMDEVM's ``ubdevm_init_module()`` runs,
all of them are already live.

FMDEVM also has a runtime (not Kconfig) dependency on the sibling ubfm modules
``linkcom`` and ``mctrlq``: for example the UCMI PCIe window configuration
relies on mctrlq having configured address windows first
(``hw_ucmi/udev_ucmi_pcie_espi.c``). These sibling modules are built under the
same ``CONFIG_UB_UBFM`` but are separate ``.ko`` objects registered through
their own ``module_init``.

Submodule Init Stage Ordering
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``feature_loader`` iterates ``STAGE_0`` through ``STAGE_9`` ascending. The
FMDEVM submodules register at the following stages:

+----------+----------------------------------------------+---------------------------+
| Stage    | Submodule / initializer                      | Reason                    |
+==========+==============================================+===========================+
| STAGE_1  | local_ras_register_init                      | Build the ras module      |
|          |                                              | registry before any ras   |
|          |                                              | table registers.          |
+----------+----------------------------------------------+---------------------------+
| STAGE_2  | vsock_server_init, ucmi_pcie_ops_init        | Early transport/UCMI ops  |
|          |                                              | registration so later     |
|          |                                              | notifiers can find them.  |
+----------+----------------------------------------------+---------------------------+
| STAGE_3  | mng_init (cdev + UCD cmds), local probe      | Stand up the user-facing  |
|          | init, ras table registration (apei_*)        | cdev and fill the ras     |
|          |                                              | registry created at       |
|          |                                              | STAGE_1.                  |
+----------+----------------------------------------------+---------------------------+
| STAGE_5  | udev_add_chip_*_info (hw_info collectors)    | Publish static chip-info  |
|          |                                              | blobs into URES before    |
|          |                                              | hw_adapt consumes them.   |
+----------+----------------------------------------------+---------------------------+
| STAGE_6  | udev_hw_adapt_init, udev_port_mng_init,      | UDM-notifier registration |
|          | faultmgr_init, usi_init, msi_init,           | stage: all consumers      |
|          | ras_sysfs_init, faultmgr_sysfs_init          | register before any       |
|          |                                              | device appears.           |
+----------+----------------------------------------------+---------------------------+
| STAGE_9  | ras_module_init, mng_proc_fs_init,           | Late/optional procfs,     |
|          | udev_pcie_init, hw_adapt_ucd_cmd_init,       | sysfs and probe tails.    |
|          | udev_debug_procfs_init,                      |                           |
|          | ucmi_pcie_sysfs_init                         |                           |
+----------+----------------------------------------------+---------------------------+

UDM Notifier Chain Priorities
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each submodule that needs to react to device add/remove registers a UDM
notifier. The priority (PRI0 highest -> PRI5 lowest) determines the call order
on ``UDM_INIT`` and the reverse order on ``UDM_UNINIT``:

+----------+--------------+----------------------------------------------+
| Submodule| Priority     | Events handled                               |
+==========+==============+==============================================+
| hw_adapt | UDM_PRI0     | UDM_INIT, UDM_UNINIT                         |
+----------+--------------+----------------------------------------------+
| port_mng | UDM_PRI2     | UDM_INIT, UDM_UNINIT                         |
+----------+--------------+----------------------------------------------+
| faultmgr | UDM_PRI2     | UDM_INIT, UDM_UNINIT                         |
+----------+--------------+----------------------------------------------+
| ras      | UDM_PRI3     | UDM_INIT, UDM_UNINIT, UDM_IRQ_INIT,          |
|          |              | UDM_IRQ_UNINIT                               |
+----------+--------------+----------------------------------------------+
| ucmi     | UDM_PRI3     | UDM_INIT, UDM_UNINIT                         |
+----------+--------------+----------------------------------------------+
| usi      | UDM_PRI4     | UDM_INIT, UDM_UNINIT                         |
+----------+--------------+----------------------------------------------+
| msi      | UDM_PRI4     | UDM_INIT, UDM_UNINIT (internally fires       |
|          |              | UDM_IRQ_INIT / UDM_IRQ_UNINIT)               |
+----------+--------------+----------------------------------------------+

The ordering matters because hw_adapt (PRI0) runs first on ``UDM_INIT`` to read
the chip identity from URES and write the device's access type / hw-info back
into URES. port_mng (PRI2) then writes ``MAMI_PORT_INFO_TBL`` into URES. ras /
ucmi (PRI3) read hw identity from URES and therefore must run after PRI0 has
populated it. usi / msi (PRI4) are the last consumers; msi additionally emits
``UDM_IRQ_INIT`` which ras (PRI3) listens for, so ras must be registered before
msi fires that sub-event.

Intra-Module Data Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  - **ras -> hw_adapt (via URES)**: ``ras_udm_notify`` reads
    ``MAMI_UDIE_VER`` from URES, which hw_adapt populated at PRI0.
  - **msi -> hw_adapt (via URES)**: ``msi_udm_notify`` reads
    ``MAMI_UDEV_HW_IDEN_INFO`` from URES, written by hw_adapt at PRI0.
  - **IDM (mng) -> port_mng (via URES)**: ``idm_get_port_info_table`` reads
    ``MAMI_PORT_INFO_TBL`` from URES, exactly what ``port_info_init`` wrote at
    PRI2. IDM is lazily initialized, so it implicitly assumes port_mng's
    ``UDM_INIT`` has already run for that devid.
  - **hw_adapt -> hw_info tables (via URES)**: ``udev_hw_get_dev_attr`` reads
    the chip-info blobs published by every ``udev_add_chip_*_info`` at
    STAGE_5.
  - **ucmi_pcie -> mctrlq (cross-module)**: UCMI PCIe window configuration
    depends on mctrlq having configured address windows first.
  - **faultmgr**: consumes only UCD and its own queue; no URES read. It is
    independent of the other submodules' data.

Device Lifecycle Sequence
^^^^^^^^^^^^^^^^^^^^^^^^^

The required order, derived from the UDM notifier flow:

  1. ``udm_notifier_call(devid, UDM_INIT)`` dispatched in priority order:
     hw_adapt (PRI0) populates URES -> port_mng (PRI2) writes ``MAMI_PORT_INFO_TBL`` ->
     faultmgr (PRI2) increments online-dev count and reinits the queue ->
     ras / ucmi (PRI3) attach ras context / map UCMI windows -> usi / msi
     (PRI4) bind the local USI driver / request MSI vectors.
  2. Inside msi's PRI4 ``UDM_INIT`` handler, after ``request_irq`` succeeds,
     msi fires ``udm_notifier_call(devid, UDM_IRQ_INIT)`` -- picked up by ras
     (PRI3), which calls ``ras_irq_init`` and registers the UID interrupt.
  3. Teardown is the mirror: ``UDM_IRQ_UNINIT`` is fired by msi before it frees
     IRQs, so ras can unregister its UID handler first; then ``UDM_UNINIT``
     runs in reverse-priority order (msi frees vectors -> ras / ucmi tear down
     -> faultmgr decrements -> port_mng uninit -> hw_adapt uninit and clears
     URES).

Out-of-order consequences: if port_mng's ``UDM_INIT`` runs after a consumer
(e.g. IDM's lazy ``idm_kern_init``), ``idm_get_port_info_table`` fails and
portset/body mapping breaks. If msi's ``UDM_INIT`` runs before hw_adapt's, the
``MAMI_UDEV_HW_IDEN_INFO`` read fails, MSI vectors are never requested,
``UDM_IRQ_INIT`` is never emitted and ras never binds its interrupt handler --
silent loss of RAS reporting.

Build-Time (Kconfig) Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  - ``CONFIG_UB_UBFM`` (tristate, depends on ``UB``) gates the entire ubfm
    tree including FMDEVM.
  - ``CONFIG_UBFM_ACC_LDST`` (default y) -- LDST access channel; the vsock
    submodule compiles only when this is ``y``.
  - ``CONFIG_UBFM_ACC_CDMA`` (default n, depends on ``UB_UBFM && UB_CDMA``,
    selects ``UBFM_ACC_LDST``) -- gates the CDMA remote probe path.
  - ``CONFIG_UBFM_ACC_PCIE_FPGA`` (default n) -- gates the PCIe FPGA / UCMI
    outband path.
  - ``CONFIG_UBFM_ACC_PCIE_I3C`` (default n, selects
    ``UBFM_ACC_PCIE_FPGA``) -- enables the PCIE+I3C outband channel. It is
    consumed by the UBFABRIC UOP adapters (``CFG_UBFM_SUPPORT_PCIE_I3C``).
  - ``CONFIG_UBFM_DEBUG`` (default n) -- adds ``-DCFG_BUILD_DEBUG``, enabling
    sysfs fault-injection and RAS test interfaces. These must not ship in
    production.
  - ras, port_mng, faultmgr, hw_adapt core, usi, mng and devmng have no
    per-submodule Kconfig; they always compile when ``CONFIG_UB_UBFM`` is on.
    Only the access-channel and debug variants are conditionally compiled.

Device Nodes
============

FMDEVM creates one character device node for user-space interaction:

========= ================= ==================================================
Node      Path               Description
========= ================= ==================================================
ubdevm    /dev/ubdevm        Device management command interface (UCD based)
========= ================= ==================================================

The node is created with owner/group ``1201`` (mamiUser) and mode ``0660``.
Its file operations are obtained from ``ucd_get_module_ops()`` so that all
ioctls are dispatched through the UBFABRIC UCD framework.

Proc Filesystem
===============

FMDEVM provides debugging and control information through the proc filesystem
under ``/proc/mami/``:

========================================= =======================================
Path                                      Purpose
========================================= =======================================
/proc/mami/devm_mng/devid2Socid           Show devid to slot_id/socket_id mapping
========================================= =======================================

Sysfs (Debug Builds)
====================

When built with ``CONFIG_UBFM_DEBUG`` (``CFG_BUILD_DEBUG``) the driver exposes
debug attribute groups under the ``ubdevm`` platform device:

============================================= ==========================================
Path                                          Purpose
============================================= ==========================================
/sys/devices/platform/.../faultmgr/inject     Inject a test fault event
/sys/devices/platform/.../ras/ras_cmd          Inject Local/Protocol RAS error data
/sys/devices/platform/.../ras/notify           Trigger an RAS interrupt by die id
============================================= ==========================================

Module Parameters
=================

env_mode

This parameter selects the interrupt delivery environment. It is mainly used
for virtual machine / partition scenarios where interrupts have to be forwarded
through a vsock channel instead of being delivered directly by the UBUS driver.

======== ================================================
Value    Description
======== ================================================
0        ENV_DEFAULT - direct UBUS interrupt delivery (default)
1        ENV_VSOCK_SERVER - run the vsock interrupt server side
2        ENV_VSOCK_CLIENT - run the vsock interrupt client side
======== ================================================

Set at load time::

    insmod mami_fmdevm.ko env_mode=1

***********************
Management Module (mng)
***********************

The mng module is the core of the FMDEVM driver. It creates the ``/dev/ubdevm``
character device and registers the MNG family of UCD commands. It also hosts
two helper sub-units: the **IDM** which translates between logical and physical
identifiers, and the **feature** helpers which expose chip-type and udie-version
capabilities to other drivers.

mng is initialized at Stage 3 (after UCD / URES / UDM are available).

Architecture
============

.. code-block:: none

     User Space ioctl on /dev/ubdevm
         |
         v
     +--------------------------------------------------+
     |                    mng core                      |
     |  +-----------+  +----------+  +---------------+  |
     |  | char dev  |  | UCD cmds |  |   dev lock    |  |
     |  | /dev/     |  | (MNG_*)  |  | (g_mng_ucd_   |  |
     |  | ubdevm    |  |          |  |  dev_lock)    |  |
     |  +-----------+  +----------+  +---------------+  |
     +--------------------------------------------------+
         |                |                |
         v                v                v
     +-----------+  +-----------+   +------------------+
     | create /  |  | local dev |   | ucmi win lost    |
     | destroy   |  | create /  |   | check (PCIe FPGA)|
     | device    |  | get devid |   |                  |
     +-----------+  +-----------+   +------------------+
         |
         v
     +--------------------------------------------------+
     |  UDM (udm_add_dev / udm_remove_dev)              |
     +--------------------------------------------------+
         |
         v
     +--------------------------------------------------+
     |  UDM notifier chain -> hw_adapt / port_mng /     |
     |  faultmgr / ras / irqmgr                         |
     +--------------------------------------------------+

ID Manager (IDM)
================

The IDM maintains a per-device map that captures the chip/die/body topology and
the physical-port to logical-port translation table. It is lazily initialized
on first use for a given ``devid`` (``idm_kern_init``) and protected by a
mutex. The topology information (die_num_per_chip, die_num_in_dev,
chip_num_in_dev, nl_num_per_die, udie_version, ``port_info_table`` and
portset/body masks) is read from URES.

The IDM supports two udie versions which differ in how the portset<->body
mapping is computed:

  - **UDIE_VERSION_V1**: portset and body correspond bit-by-bit.
  - **UDIE_VERSION_V2**: PCIe bodies are skipped when computing the portset
    index, so the portset id may be smaller than the body id.

IDM is the single source of truth for logical<->physical id translation in the
MAMI subsystem and is consumed by linkcom, ctrlq, ras and other drivers.

Logical/Physical Identifier Encoding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The logical identifiers are packed 32-bit values (see ``ubfm_common.h``):

  - **Plane** (``MAMI_KERN_LOGIC_ID_TYPE_PLANE``): for Kunpeng/Ascend (K+A)
    ``[rsv:8][chipid:8][cluster:8][dieid:8]``; for Unions (U)
    ``[rsv:24][chipid:8]``.
  - **PortSet** (``MAMI_KERN_LOGIC_ID_TYPE_PORT_SET``):
    ``[chipid:8][cluster:8][dieid:8][ssuid:2][portsetid:6]``.
  - **Port** (``MAMI_KERN_LOGIC_ID_TYPE_PORT``):
    ``[rsv:8][chipid:8][dieid:8][portid:8]``.

The physical identifiers (``MAMI_KERN_PHYSIC_ID_TYPE_CHIP/DIE/BODY/PORT``) are
encoded through the ``MAMI_P_CHIP`` / ``MAMI_P_DIE`` / ``MAMI_P_BODY`` /
``MAMI_P_PORT`` macros.

Feature Helpers
===============

The feature helpers (``feature.c``) query URES attributes to expose chip-type
capabilities to other drivers:

  - ``feature_kern_is_dpu(devid)``: true for 1825 chips.
  - ``feature_kern_is_ssd(devid)``: true for hssu_v100 chips.
  - ``feature_kern_is_ndie(devid)``: true for kunpeng960 (1P/2P/4P).
  - ``feature_kern_ucmi_module_is_ascend(devid)`` / ``is_kunpeng(devid)``:
    classify the UCMI module family.
  - ``feature_kern_get_udie_version(devid, &ver)``: return the udie version.
  - ``feature_kern_get_rx_tgtid_table_spec`` /
    ``feature_kern_get_tx_tgtid_table_spec``: return the RX/TX target-id
    routing table size depending on chip type and udie version.
  - ``feature_kern_get_port_mapping(devid, map, &num)``: compute the
    logical-port to physical-port mapping for the device, honoring the body
    and PCIe masks.
  - ``feature_kern_is_support_ras_report()``: global flag (based on
    ``MAMI_IS_LOCAL_ASCEND``) controlling whether RAS reporting is enabled.

UCD Commands
============

mng registers the following ioctl commands under ``UCD_CMD_MNG``. User space
issues them through ``ioctl()`` on ``/dev/ubdevm``; the UCD (UB device command
dispatch) framework routes each command to the registered handler.

============================ =================== =====================================
Command                      Permission          Description
============================ =================== =====================================
MNG_CREATE_DEV               PROC_CTRL_ANY_USER  Create a device from a GUID
MNG_DESTROY_DEV              PROC_CTRL_ANY_USER  Destroy a device by devid
MNG_DEVICE_PROBE             PROC_CTRL_ANY_USER  Trigger PCIe FPGA device probe
MNG_CREATE_LOCAL_DEV         PROC_CTRL_ANY_USER  Auto-create the local device
MNG_GET_LOCAL_DEVID          PROC_CTRL_ANY_USER  Query the local device id
MNG_GET_UCMI_WIN_LOST        PROC_CTRL_MANAGED   Check UCMI window lost (PCIe FPGA)
============================ =================== =====================================

Call Timing / Sequence
~~~~~~~~~~~~~~~~~~~~~~

**MNG_CREATE_DEV** (``mng_ucd_create_dev``):

  1. Validate the input ``struct mami_dev_info`` (GUID, cna, eid, chipType,
     createCase).
  2. Convert the GUID to a string key and look up the previously-probed
     ``struct udev_hw_identify`` attribute in URES.
  3. Compute the ``devid`` from ``slot_id``/``socket_id`` via
     ``mng_get_devid``.
  4. Under ``g_mng_ucd_dev_lock``: store the recreate-case attribute, reject
     manual creation if the device was auto-created locally, then call
     ``udm_add_dev`` which fires the UDM notifier chain (hw_adapt -> port_mng
     -> faultmgr -> ras -> irqmgr).
  5. Return the allocated ``devid`` (or ``EEXIST``) to user space.

**MNG_CREATE_LOCAL_DEV** (``mng_ucd_create_local_dev``):

  1. Scan the URES attribute key list for ``guid_*`` attributes whose
     ``udev_hw_identify.location == UDM_LOCAL``.
  2. Compute the devid and call ``udm_add_dev`` (same notifier chain as above).
  3. Persist ``MAMI_AUTO_LOCAL_DEVID`` in URES so that a subsequent manual
     ``MNG_CREATE_DEV`` for the same device is rejected.

**MNG_DESTROY_DEV** (``mng_ucd_destroy_dev``):

  1. Acquire ``g_mng_ucd_dev_lock``.
  2. Call ``udm_remove_dev(devid)`` which triggers the ``UDM_UNINIT``
     notifier chain in reverse priority so every submodule releases its
     per-device resources.

EXPORT_SYMBOL_GPL API Reference
===============================

The following APIs are exported by the mng / devmng submodules and may be
called by other in-tree MAMI drivers.

.. note::
   Kernel documentation reference: idm_kern_get_chip_die_num idm_kern_lgid2phyid
   idm_kern_phyid2lgid idm_kern_get_plane_by_fdie
   idm_kern_get_fbody_bport_by_lgport idm_kern_get_lgport_by_fbody_bport
   idm_kern_get_inport_by_chip_die_port idm_kern_is_nl_valid

.. note::
   Kernel documentation reference: mng_get_devid mng_get_slot_id_socket_id

**************************
Device-ID Manager (devmng)
**************************

The devmng submodule maintains the mapping between the MAMI ``devid`` and the
SoC location (``slot_id``/``socket_id``). The SoC id is a 32-bit value with the
slot id in the high 16 bits and the socket id in the low 16 bits. ``mng_devid``
is a simple array indexed by devid, protected by a mutex, that allocates a new
devid when a previously unseen (slot, socket) pair is registered.

Proc Interface
==============

The proc entry ``/proc/mami/devm_mng/devid2Socid`` dumps the full devid to
slot_id/socket_id mapping.

EXPORT_SYMBOL_GPL API Reference
===============================

.. note::
   Kernel documentation reference: mng_get_devid mng_get_slot_id_socket_id

******************************
Hardware Adaptation (hw_adapt)
******************************

The hw_adapt submodule is the hardware bring-up layer. It is registered as a
UDM notifier at **PRI0** so that it runs first when a device is added or
removed. On ``UDM_INIT`` it reads the chip type / connection type / access
type from the URES ``udev_hw_identify`` attribute and dispatches to the
registered channel-type interface (``MAMI_CHNL_TYPE`` for MAMI devices or
``DPU_CHNL_TYPE`` for DPU/message-channel devices).

hw_adapt supports several probe backends selected at compile time:

  - **local probe** (LDST access type): probe the local MAMI device from
    reserved memory.
  - **CDMA probe** (CDMA access type, ``CONFIG_UB_CDMA``): probe remote MAMI
    devices through the UBUS CDMA transport.
  - **PCIe probe** (PCIe FPGA, ``CONFIG_UBFM_ACC_PCIE_FPGA``): probe PCIe FPGA
    devices.
  - **remote msg probe / msg pub**: probe and communicate with DPU devices
    through a message channel.

Sub-modules
===========

hw_probe
~~~~~~~~

Implements the device probe flow and the ``udev_ctrl`` helpers that read the
device configuration header (``udev_cfg_hdr`` / ``udev_cfg_info``) from
reserved memory and register the GUID attribute in URES.

hw_info
~~~~~~~

Per-chip hardware information collectors. Each supported chip type
(``ascend_v1``, ``kunpeng950``, ``hssu_v100``, ``1825``, ``unions``) has its
own implementation that populates URES keys (die number, NL number, body/PCIe
masks, UE EIDs, port information table source data, etc.).

hw_map
~~~~~~

Translates the chip-specific memory/register layout into URES address
resources. For PCIe-eSPI connected devices it uses the UCMI window manager; for
VM-connected devices it stores primary/secondary mappings; for directly
accessible devices it stores the ASIC address directly. Exports
``udev_mem_map``.

hw_ucmi
~~~~~~~

UCMI window management. Provides dynamic window allocation/clear
(``ucmi_cfg_dyn_win``, ``ucmi_clear_dyn_win``,
``ucmi_cfg_win``, ``ucmi_clear_win``), 256M window assignment
(``ucmi_assign_256m_win``) and address-info lookup
(``ucmi_get_addr_info`` / ``ucmi_put_addr_info`` /
``ucmi_get_idle_local_win`` / ``ucmi_get_local_addr``). Supports the PCIe-eSPI
connection type.

EXPORT_SYMBOL_GPL API Reference
===============================

.. note::
   Kernel documentation reference: udev_mem_map

.. note::
   Kernel documentation reference: ucmi_cfg_dyn_win ucmi_clear_dyn_win
   ucmi_cfg_win ucmi_clear_win ucmi_get_idle_local_win ucmi_get_local_addr

.. note::
   Kernel documentation reference: ucmi_assign_256m_win ucmi_get_addr_info
   ucmi_put_addr_info

**************************
Port Management (port_mng)
**************************

The port_mng submodule initializes the per-device port information table. It is
registered as a UDM notifier at **PRI2** and runs on ``UDM_INIT``.

On device add, port_mng:

  1. Reads ``MAMI_DIE_NUM`` and ``MAMI_NL_NUM`` from URES.
  2. Calls ``feature_kern_get_port_mapping`` to build the logical-port to
     physical-port table.
  3. Reads the RX target-id routing table (per die/NL) and the TX PPP
     target-id routing table from hardware through the PA/PPP table helpers.
  4. Reads each port's link mode (ETH/UB/PCIe/DISABLE) and, for UB ports, the
     lane mode (x4/x2) from the NL config registers; for UBOE (ETH) ports it
     reads the MAC port link mode.
  5. Stores the resulting ``struct port_info_table`` into URES as
     ``MAMI_PORT_INFO_TBL`` so that IDM and other drivers can consume it.

The module name registered with UDM is ``udev_port_mng``.

**************************
Interrupt Manager (irqmgr)
**************************

The irqmgr submodule owns hardware interrupt request and dispatch. It provides
two independent backends, selected at compile time by the access-type Kconfig
options, and feeds all interrupts into the UBFABRIC UID framework
(``uid_trigger_intr``).

USI
===

The USI backend is used for the LDST and CDMA access types
(``CONFIG_UBFM_ACC_LDST`` / ``CONFIG_UBFM_ACC_CDMA``). It registers a UBUS
driver (``usi_irq_drv``) whose id table matches the MAMI CtrlUe device ids.
When a CtrlUe entity is probed, USI allocates IRQ vectors (CTRLQ, RAS, MSGQ)
and binds them to the per-die ``usi_irq_handler`` which calls
``uid_trigger_intr`` with the appropriate ``UID_TYPE_*``.

USI supports three environments through the ``env_mode`` module parameter:

  - **ENV_DEFAULT**: direct interrupt delivery. Local devices postpone IRQ
    registration until ``UDM_INIT`` so that the devid is known.
  - **ENV_VSOCK_SERVER**: the local side runs a vsock server thread that
    collects interrupt bits from all dies (atomic per-die bitmaps merged in a
    wait queue) and serves them to the remote client. The CtrlUe probe uses
    ``usi_server_irq_handler`` which only sets the per-die atomic flag.
  - **ENV_VSOCK_CLIENT**: the remote side runs a vsock client thread that
    periodically issues ``VSOCK_CMD_GET_IRQ`` and parses the returned bitmap
    into ``uid_trigger_intr`` calls.

UCD Commands (USI)
~~~~~~~~~~~~~~~~~~

USI registers the following ioctl commands under ``UCD_CMD_USI``. The client
side is started/stopped from user space through these commands.

============================ =================== =====================================
Command                      Permission          Description
============================ =================== =====================================
USI_CREATE_CLIENT            PROC_CTRL_MANAGED   Start the vsock interrupt client
USI_CLOSE_CLIENT             PROC_CTRL_MANAGED   Stop the vsock interrupt client
============================ =================== =====================================

Call Timing / Sequence
~~~~~~~~~~~~~~~~~~~~~~

**USI_CREATE_CLIENT**: user space issues this command on the remote/vsock-client
side to start the vsock client thread. The thread connects to the server (port
12345), registers for IRQ notification and begins polling ``VSOCK_CMD_GET_IRQ``;
each returned interrupt bitmap is parsed into ``uid_trigger_intr`` calls.

**USI_CLOSE_CLIENT**: user space issues this command to stop the client thread,
close the vsock socket and stop forwarding interrupts.

MSI
===

The MSI backend is used for the PCIe FPGA build
(``CONFIG_UBFM_ACC_PCIE_FPGA``). It is registered as a UDM notifier at
**PRI4**. On ``UDM_INIT`` for a PCIe-eSPI device it locates the PCI device
through the board chip-management info, allocates MSI/MSI-X vectors and binds
them to ``msi_irq_handler``. The handler reads the FPGA IRQ status register,
dispatches CTRLQ/RAS interrupts to UID and clears the status. After all vectors
are requested it fires ``UDM_IRQ_INIT`` through the UDM notifier chain so that
the RAS module can complete its per-device IRQ setup.

vsock Helper
============

The vsock helper (``vsock/udev_vsock.c``) provides a kernel AF_VSOCK
``SOCK_STREAM`` server/client transport on port 12345. It is used by the USI
backend to forward interrupt events between a virtual-machine host and guest.
The server registers per-command handlers (``VSOCK_CMD_REG_IRQ``,
``VSOCK_CMD_GET_VERSION``, ``VSOCK_CMD_GET_IRQ``) and a disconnect handler;
the client exchanges ``struct vsock_msg_pkt`` messages with the server.

************************
Fault Manager (faultmgr)
************************

The faultmgr submodule provides a kernel fault-event queue that user space can
read with blocking semantics. It is registered as a UDM notifier at **PRI2**
so that the queue capacity can be resized whenever a device goes online or
offline.

faultmgr supports:

  - A ring-buffer event queue whose capacity grows with the number of online
    devices (32 entries per online device, ``FAULTMGR_CAPACITY_SHIFT_PER_DEV``).
  - Event serial numbers (low 28 bits) so that user space can request "all
    events after serial N".
  - Blocking ``faultmgr_ucd_event_get`` with configurable timeout (-1 = never
    timeout, [0, 30000] ms), limited to 8 concurrent readers.
  - Kernel-side reporting through ``ubdevm_kern_report_event`` (used by the
    RAS module and the debug sysfs injector).
  - A debug sysfs entry ``faultmgr/inject`` to push a test event.

Architecture
============

.. code-block:: none

     +--------------------------------------------------+
     |                  faultmgr core                   |
     |                                                  |
     |  +--------------+    +------------------------+   |
     |  | wait queue   |    | event ring buffer      |   |
     |  | (faultmgr_wq)|    | (faultmgr_queue)       |   |
     |  +--------------+    +------------------------+   |
     |                     | capacity = online_dev  |   |
     |                     |       << 5             |   |
     |                     +------------------------+   |
     +--------------------------------------------------+
         ^                ^                ^
         |                |                |
     +--------+     +-----------+    +-------------+
     | ras    |     | UCD add   |    | sysfs       |
     | report |     | (user)    |    | inject      |
     +--------+     +-----------+    +-------------+
                         |
                         v
                    +-----------+
                    | UCD get   |
                    | (user)    |
                    +-----------+

UCD Commands
============

faultmgr registers the following ioctl commands under ``UCD_CMD_FAULTMGR``:

====================  ================================  ============================
Command                Permission                        Description
====================  ================================  ============================
FAULTMGR_EVENT_ADD     MANAGED | PROCESS_MGR |           Add a fault event
                      PM_CHILD | IAM_MGR
FAULTMGR_EVENT_GET     PROC_CTRL_ANY_USER                Get fault events (blocking)
====================  ================================  ============================

Call Timing / Sequence
~~~~~~~~~~~~~~~~~~~~~~

**FAULTMGR_EVENT_ADD** (``faultmgr_ucd_event_add``) and the in-kernel
``ubdevm_kern_report_event``:

  1. If ``feature_kern_is_support_ras_report()`` is false, return
     ``E_OPNOTSUPP_ON_DEV``.
  2. Validate the assertion type and NUL-terminate ``eventName``.
  3. Push the event into the ring buffer (``faultmgr_queue_add_event``).
  4. Log the event (warn for recovery, error for occurrence/one-time events).
  5. Wake up ``faultmgr_wq`` so blocked readers can proceed.

**FAULTMGR_EVENT_GET** (``faultmgr_ucd_event_get``):

  1. Validate the parameter (event array pointer, inEventNum, timeout range).
  2. Resolve ``MAMI_EVENT_SERIALNUM_NEWEST`` to the current queue serial num.
  3. Acquire one of the 8 parallel-reader slots (atomic counter).
  4. If timeout > 0, ``wait_event_interruptible_timeout`` until a newer event
     arrives; if timeout < 0, ``wait_event_interruptible`` (no timeout); if
     timeout == 0, return immediately.
  5. Copy available events into the user-supplied array and return the new
     serial number and ``outEventNum``.

Sysfs (Debug Builds)
====================

The ``faultmgr/inject`` sysfs entry (debug builds only) pushes a test fault
event into the ring buffer, which can then be read back through
``FAULTMGR_EVENT_GET``.

****************
RAS Module (ras)
****************

The RAS module parses hardware error records from device memory and reports
them as fault events through the faultmgr. It is registered as a UDM notifier
at **PRI3** and registers a UID interrupt handler for ``UID_TYPE_RAS`` with a
workqueue bottom-half (RAS parsing needs to call UOP / URES, so it must run in
process context). The bottom-half scheduling delay is 1000 ms for interrupt
coalescing.

RAS supports two error record formats:

  - **Local RAS** (``ras_local.c``): per-core/per-port module error registers
    decoded into ``struct local_ras_error_data``.
  - **Protocol RAS** (``ras_protoc.c``): UBC controller protocol errors
    decoded into ``struct protoc_ras_error_data``.

The RAS data region is located through the URES ``MAMI_RAS_MEM`` memory base.
It starts with a ``struct ras_data_header`` (magic, version, item_num,
item_len, CRC) followed by an array of ``struct ras_data_entry_item``. Each
entry points to a payload of ``item_len`` bytes that is decoded by the local
or protocol parser depending on the entry type.

On ``UDM_IRQ_INIT`` (fired by the MSI backend) RAS enables the per-die
interrupt status/unmask handling; on ``UDM_IRQ_UNINIT`` it tears it down.

RAS is skipped on UDIE_VERSION_V2 devices (the warning "RAS is not supported, it
needs to adapt." is logged) and when
``feature_kern_is_support_ras_report()`` is false.

Sysfs (Debug Builds)
====================

The ``ras`` sysfs group (debug builds only) provides two entries:

  - ``ras_cmd``: inject a Local or Protocol RAS error record into device
    memory. Usage: ``echo <devid> <type> <options...> > ras_cmd`` where type
    is 1 for Local RAS and 2 for Protocol RAS.
  - ``notify``: trigger the RAS interrupt handler for a die.
    Usage: ``echo <devid> <die_id> > notify``.

***************
API Usage Guide
***************

This section shows a typical user-space workflow for bringing up a MAMI device
through the FMDEVM driver, from probing hardware to destroying the device. The
example assumes the UBFABRIC infrastructure is already loaded and that the
FMDEVM driver has created ``/dev/ubdevm``.

Device Probe and Create Flow
============================

  1. **Create the device** from user space by issuing an ioctl with
     ``main_cmd = UCD_CMD_MNG``, ``sub_cmd = MNG_CREATE_DEV`` and a
     ``struct mami_dev_info`` carrying the GUID, cna, eid and chipType. The
     driver computes the devid, calls ``udm_add_dev`` and returns the devid in
     ``struct devm_create_dev_out``. The UDM notifier chain then brings up
     hw_adapt, port_mng, faultmgr, ras and irqmgr for the new device.

  2. **(Optional) Auto-create the local device** with
     ``MNG_CREATE_LOCAL_DEV``. This is the typical path on the host: the
     driver finds the local GUID attribute and creates the device, marking it
     as auto-created so that a later manual create for the same GUID is
     rejected.

  3. **Read fault events** by issuing ``UCD_CMD_FAULTMGR`` /
     ``FAULTMGR_EVENT_GET`` with a ``struct mami_read_event_param`` describing
     the caller's event buffer, desired serial number and timeout. The call
     blocks until an event is available or the timeout elapses, then returns
     the new serial number and the number of events copied.

  4. **Destroy the device** with ``MNG_DESTROY_DEV`` passing the devid. The
     ``UDM_UNINIT`` notifier chain releases all per-device resources.

Kernel-Space Consumer Example
=============================

A second MAMI driver that needs to translate a logical plane id into a
physical id can call the exported IDM API::

    #include "idm.h"
    #include "ubfm_common.h"

    struct mami_logical_id_info lg = {
        .logicalIdType = MAMI_KERN_LOGIC_ID_TYPE_PLANE,
        .logicalIdVal  = MAMI_PLANE_SET_LG_AK_ID(chipid, dieid),
    };
    struct mami_physical_id_info phy;
    int ret = idm_kern_lgid2phyid(devid, lg, &phy);
    if (ret == 0) {
        /* phy.physicalIdType / physicalIdVal usable for register access */
    }

A driver that needs to map a chip memory region into URES so that UOP can later
map it into kernel space uses the exported hw_map helper::

    #include "udev_hw_pub.h"

    struct ures_inst inst = { .devid = devid, .die_id = die_id };
    ret = udev_mem_map(&inst, UOP_ITEM_MEM, &mem_region, true /* need_map */);

CONTRIBUTING
============

Patches are welcome through the vendor's support channel. Please follow the
Linux kernel coding style, keep the ``SPDX-License-Identifier: GPL-2.0-only``
header in every new source file and add new feature commands through the UCD
registration helpers so that they are discoverable through
``/proc/mami/ucd/register``.

AUTHORS
=======

HiSilicon Technologies Co., Ltd.

ChangeLog
=========

- 2026: Initial public documentation of the ``mami_fmdevm`` driver.

Support
=======

If there is any issue or question, please contact the vendor's support channel.
