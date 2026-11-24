.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


****************************************
CTRLQ Control Queue Module
****************************************

CTRLQ is the in-band "Control Queue" module of the MAMI kernel subsystem.
Based on the device-side hardware control queue, it provides a pair of
software queues to upper kernel modules: the SQ and the CQ. Upper modules
pack control messages into SQE and write them into the SQ; after the
device-side processing completes, CQE are written back through the CQ. The
ctrlq driver converts CQE into UED events reported to user space, and
provides kernel-space callback pass-through capability for dedicated channels
(S channels).

CTRLQ is built on the UBFABRIC infrastructure and reuses common services such
as UCD, UDM, UID, URES, UOP, UED and IDM. The module is initialized through
the ``feature_loader``
mechanism in ``FEATURE_LOADER_STAGE_0``, ``FEATURE_LOADER_STAGE_1`` and
``FEATURE_LOADER_STAGE_9``, and device-level lifecycle is driven through the
UDM notification chain.

This module exposes the following capability domains to the outside, defined
by the static UCD command table ``g_ctrlq_fops``, the EXPORT_SYMBOL exported
symbols and the proc debug interface:

  - 1) **UCD commands**: CHAN_SWITCH (channel on/off), GET_SNAPSHOT (channel
       snapshot), SEND_SQE_TS (SQE send timestamp reporting).
  - 2) **Exported symbols**: mami_ctrlq_schan_get_addr (S-channel address
       retrieval), mami_ctrlq_schan_op (S-channel callback register/unregister),
       ctrlq_kern_enque_cq (CQE injection, DEBUG build only).
  - 3) **Proc**: debug and status interfaces under ``/proc/mami/ctrlq/ctrlq``.

Acronyms
========

The following acronyms are used throughout this document:

================================ ================================================
Acronym                          Full Name
================================ ================================================
MAMI                             Management Architecture and Model Interface
UBFABRIC                         UB Fabric
UCD                              UB Device Command Dispatch
UDM                              UB Device Management
UID                              UB Interrupt Dispatch
URES                             UB Device Resource
UOP                              UB Device Operation
UED                              UB Device Event Dispatch
IDM                              ID Management
SQ                               Submission Queue
CQ                               Completion Queue
SQE                              Submission Queue Entry
CQE                              Completion Queue Entry
UE                               UB Entity
VUE/MUE                          Virtual UE/Machine UE
================================ ================================================

Overall Sequence
================

The figure below shows the complete lifecycle of a ctrlq device from add to
remove, plus the dependencies among the three core running paths of send /
receive / S channel:

.. code-block:: none

     ===================== Device Lifecycle (UDM notify chain, UDM_PRI2) =====================

     UDM_INIT (device add)
         |
         +-- ctrlq_kern_dev_init: read HW identity -> ctrlq_is_support filter
         |      -> ctrlq_hw_init (ctrlq_hw_create + ctrlq_hw_add)
         |          |-- ctrlq_kern_init:  init global info and locks
         |          +-- _ctrlq_hw_init:   enable UE resources per die ->
         |                    per-channel attr_pack + ctrlq_kern_chan_add ->
         |                    write MAMI_DIEN_CTRLQ_CHAN_INFO ->
         |                    ctrlq_asic_addr_map (merge-map CQ/SQ memory)
         |
         v
     UDM_IRQ_INIT
         |
         +-- ctrlq_kern_dev_irq_init -> ctrlq_kern_irq_unmask (irq_enable=1,
             on FPGA write int_mask=0 to unblock the interrupt)
         |
         v
     +------------------------------------------------------------------+
     |                     Channel active window                        |
     |                                                                  |
     |  (1) SEND (send path, called by kernel modules)                  |
     |      ctrlq_kern_seq_alloc (seq = idr 0x8000..0xFFFF)             |
     |        -> ctrlq_kern_enque -> fill_sqe -> sqe_submit             |
     |        (mem_write SQE + writel tail + writel intr=1 trigger)     |
     |      [HW/control plane processes, writes back CQE when done]     |
     |        -> ctrlq_kern_seq_free (seq must be returned in any case) |
     |                                                                  |
     |  (2) RECV (receive path, UID interrupt, two optional branches)   |
     |      Top half: ctrlq_intr_pre_handle -> ctrlq_irq_triggered      |
     |               parses dieid + chan_status -> sets pending_chans   |
     |      Bottom half: ctrlq_intr_handle -> ctrlq_irq_proc            |
     |              -> ctrlq_deque_all_chan -> _ctrlq_kern_deque        |
     |        default branch: _ctrlq_deque (<=128 entries/round)        |
     |                      -> _ctrlq_event_pack -> _ctrlq_send_event   |
     |                      -> ued_submit_event_to_dev                  |
     |        S-channel branch: schan_cqe_exist (tail != head)          |
     |                      -> invoke schan_ops.handle (register first) |
     |                                                                  |
     |  (3) CHAN_SWITCH (UCD command, anytime)                          |
     |      enable: CQ reset + cq/sq on_off=CHAN_ON                     |
     |      disable: CQ reset + cq/sq on_off=CHAN_OFF (stop send/recv)  |
     +------------------------------------------------------------------+
         |
         v
     UDM_IRQ_UNINIT
         |
         +-- ctrlq_kern_dev_irq_uninit -> ctrlq_kern_irq_mask (irq_enable=0)
         v
     UDM_UNINIT (device remove)
         |
         +-- ctrlq_kern_dev_uninit -> ctrlq_hw_uninit
         |    -> stop=true (stop scanning) -> ctrlq_kern_uninit -> destroy

     Dependencies: UDM_IRQ_INIT must precede any send/receive use (to
     guarantee irq_enable=1). Enabling/disabling a channel affects the send
     path (enque checks sq.on_off) and the receive path (deque checks
     cq.on_off).

Architecture
============

.. code-block:: none

     User space
         |
         |-- ioctl on /dev/ubfabricX (UCD_CMD_CTRLQ) -----> UCD command dispatch
         |                                                      (fops)
         |
     Other kernel modules
         |
         |-- EXPORT_SYMBOL: schan_get_addr / schan_op / enque_cq
         |
         v
     +----------------------------------------------------------------+
     |                        CTRLQ module                            |
     |                                                                |
     |  core/           core logic (channel mgmt, send/recv           |
     |                  orchestration, snapshot, proc)                |
     |    ctrlq_kern.c  SQ/CQ/channel lifecycle, enque/deque,         |
     |                  event pack & report, S-channel callback,      |
     |                  snapshot, on/off                              |
     |    ctrlq_fops.c  UCD command handling (input check + forward)  |
     |    ctrlq_debug.c CQE injection in DEBUG build (enque_cq)       |
     |    ctrlq_proc.c  /proc/mami/ctrlq/ctrlq debug entry            |
     |  hw/            hardware adaptation and interrupt handling     |
     |    ctrlq_hw.c   device instance management, UE enable,         |
     |                  bottom-half scheduling                        |
     |    ctrlq_irq.c  UID interrupt registration (top/bottom half)   |
     |    chip/        chip adaptation (reg base resolution, UE       |
     |                  mapping table, S-channel address calc)        |
     |  ctrlq_init.c   UDM lifecycle callbacks (dev/irq init/uninit)  |
     +----------------------------------------------------------------+
         |        |        |        |        |        |
         v        v        v        v        v        v
       UCD      UDM      UID      URES     UOP      UED / IDM
      cmd       device   intr     res      reg      event / plane
      dispatch  lifecycle         disc     access   report / map

Device Nodes
============

CTRLQ does not create its own character device node. User-space interaction is
done through the UBFABRIC device node (``/dev/ubfabricX``) via the
``UCD_CMD_CTRLQ`` main command; other kernel modules use the EXPORT_SYMBOL
exported symbols to access ctrlq capabilities directly.

Internal Dependency Constraints
===============================

There are strict dependency orders among the internal sub-modules of ctrlq.
Violating these constraints leads to initialization failure, interrupts not
being delivered, or channels being stopped. The conventions are as follows:

Module Loading Order Constraints
--------------------------------

.. list-table::
   :header-rows: 1
   :widths: 16 32 26

   * - Dependency
     - Constraint
     - Consequence of violation
   * - chip before hw
     - g_chip_ops must register the attr_pack operation set in
       FEATURE_LOADER_STAGE_0 (ctrlq_chip_mod_init); hw uses it in
       FEATURE_LOADER_STAGE_1
     - _ctrlq_hw_create returns NULL, dev_init fails
   * - kern before fops
     - ctrlq_hw_mod_init runs ctrlq_kern_mod_init first, then
       ctrlq_fops_mod_init internally
     - fops uses structures not validated by BUILD_BUG_ON and uninitialized
       locks
   * - fops depends on UCD
     - ctrlq_fops_mod_init registers the 3 UCD sub-commands via
       ucd_register_dev_cmd
     - UCD commands unavailable
   * - irq registered last
     - ctrlq_irq_init registers the UID_TYPE_CTRLQ interrupt in
       FEATURE_LOADER_STAGE_9
     - interrupt events cannot be dispatched to ctrlq
   * - UDM notifier
     - mctrlq_mod_init registers the mctrlq_udm_notify notifier at UDM_PRI2
     - device lifecycle cannot drive ctrlq

Device Lifecycle Order Constraints
----------------------------------

  - In ``UDM_INIT``, ``ctrlq_kern_dev_init`` must run before msgq's
    ``msgq_kern_dev_init``; when msgq fails, mctrlq calls
    ``ctrlq_kern_dev_uninit`` in reverse order to roll back.
  - ``UDM_IRQ_INIT`` (irq unmask) must precede any enque / S-channel
    send-receive; otherwise ``irq_enable`` is still 0 and channel interrupts
    are not enabled.
  - chan enable/disable and schan address requests depend on the device
    instance having been created (``ctrlq_get`` must be able to obtain the
    instance).
  - ``UDM_UNINIT`` runs in reverse order: first ``ctrlq_kern_irq_mask``, then
    ``ctrlq_hw_uninit``.

Runtime Constraints
-------------------

  - **Seq pairing**: ``ctrlq_kern_seq_alloc`` must be called to obtain a seq
    (idr range ``CTRLQ_SEQ_MIN(0x8000)`` ~ ``CTRLQ_SEQ_MAX(0xFFFF)``) before
    enque; seq must be returned via ``ctrlq_kern_seq_free`` after enque
    regardless of success or failure.
  - **Event serialization**: a non-NULL ``cq->cqe`` means the previous event
    has not been sent; ``_ctrlq_event_pack`` retries directly and does not
    consume the next CQE until it succeeds.
  - **UED failure recovery**: when ``ued_submit_event_to_dev`` keeps failing
    for more than 4 s, the current CQE is dropped and the CQ is reset
    (``ctrlq_cq_recovery``).
  - **Invalid CQE**: if the CQE's type / bb_num / ret (when seq < 0x8000) is
    illegal, ``ctrlq_cqe_fault_notify_recovery`` is triggered: reset the CQ and
    report a drop event (``ctrlq_send_cqe_drop_event``).
  - **Channel permanent stop**: when ``_ctrlq_kern_deque`` returns ``-ENODEV``
    (dev/chan initialization failed), the ``status`` bitmap bit is set and the
    channel no longer participates in later scanning.
  - **S channel**: supported only on Kunpeng950 (``UDEV_PCIE_ESPI``)
    platforms; REG/UNREG must be paired, and only one callback is kept on the
    S channel of the same die at a time (a later registration overwrites an
    earlier one).
  - **Channel on/off idempotence**: chan enable/disable on a channel already
    in the target state returns 0 without resetting it again.
  - **Context constraint**: ``ctrlq_kern_enque`` and ``ctrlq_kern_enque_cq``
    are forbidden in interrupt/softirq context (return ``-EDEADLK``).

Functionality Dependencies
==========================

  - **UCD**: all 3 commands are registered under ``UCD_CMD_CTRLQ``. UCD is
    responsible for ioctl dispatch, permission checking and input/output
    buffer management.
  - **UDM**: mctrlq registers the UDM notification callback
    ``mctrlq_udm_notify`` at ``UDM_PRI2`` priority. Device add/remove events
    drive the device and interrupt init/uninit of ctrlq.
  - **UID**: ctrlq registers the ``UID_TYPE_CTRLQ`` interrupt type, uses a
    work-queue bottom half (``block_flag = 1``), with a scheduling merge
    window of 50 ms (1000 ms for PCIE FPGA).
  - **URES**: queries register bases (``MAMI_MISC_REG``,
    ``ctrlq_reg_base_win%u_local``), memory bases
    (``ctrlq_mem_base_win%u_local``), device attributes
    (``MAMI_UDEV_HW_IDEN_INFO``), per-die configurations
    (``MAMI_UDEV_DIEN_CFG``) and the UE count (``MAMI_UE_NUM``), and
    writes back channel information (``MAMI_DIEN_CTRLQ_CHAN_INFO``).
  - **UOP**: accesses SQ/CQ registers and queue memory via
    ``uop_kern_readl`` / ``uop_kern_writel`` / ``uop_kern_mem_write``.
  - **UED**: wraps CQE into UED events and delivers them to upper-layer
    subscribers via ``ued_submit_event_to_dev``.
  - **IDM**: obtains the plane id via ``idm_kern_get_plane_by_fdie`` (used to
    assemble CQ drop events).

****************************************
Module Initialization
****************************************

The init sub-module is the entry point of the ctrlq driver. It is registered
in stages through the ``feature_loader`` mechanism and completes the
initialization of all ctrlq sub-modules in order; device-level lifecycle is
driven by the UDM notifier registered by ``mctrlq_mod_init``.

Initialization Sequence
=======================

``feature_loader`` runs initialization in the following stages:

  1. ``FEATURE_LOADER_STAGE_0``: ``ctrlq_chip_mod_init()`` registers the chip
     adaptation operation set ``g_chip_ops`` (attr_pack) into the hw-layer
     global ``g_hw_ops``.
  2. ``FEATURE_LOADER_STAGE_1``: ``ctrlq_hw_mod_init()`` first runs
     ``ctrlq_kern_mod_init()`` (BUILD_BUG_ON validating SQE/CQE are 32 bytes,
     initializing the per-device read-write locks), then runs
     ``ctrlq_fops_mod_init()`` to register the 3 UCD sub-commands;
     ``ctrlq_proc_mod_init()`` creates the ``/proc/mami/ctrlq/ctrlq`` entry.
  3. ``FEATURE_LOADER_STAGE_9``: ``ctrlq_irq_init()`` registers the
     ``UID_TYPE_CTRLQ`` interrupt type (top-half pre_handle + bottom-half
     handle).

When any step fails, already registered/initialized resources are rolled back
in reverse order. On unload, cleanup runs in the reverse order of init
(``ctrlq_hw_mod_uninit`` / ``ctrlq_proc_mod_uninit`` / ``ctrlq_irq_uninit``).

Device Lifecycle Callbacks
==========================

``mctrlq_udm_notify(devid, action, val)`` is called by the UDM notification
chain (``UDM_PRI2``):

  - ``UDM_INIT``: first ``ctrlq_kern_dev_init(devid)`` (read hardware identity
    ``MAMI_UDEV_HW_IDEN_INFO``, filter UNIONS / 1825 / UNIONS_210, then
    ``ctrlq_hw_init``), then ``msgq_kern_dev_init(devid)``; when msgq fails,
    ctrlq is rolled back in reverse order.
  - ``UDM_UNINIT``: first ``msgq_kern_dev_uninit(devid)``, then
    ``ctrlq_kern_dev_uninit(devid)`` (i.e. ``ctrlq_hw_uninit``).
  - ``UDM_IRQ_INIT``: ``ctrlq_kern_dev_irq_init(devid)`` -- after the filter,
    calls ``ctrlq_kern_irq_unmask``.
  - ``UDM_IRQ_UNINIT``: ``ctrlq_kern_dev_irq_uninit(devid)`` -- calls
    ``ctrlq_kern_irq_mask``.

``ctrlq_hw_init`` completes the following steps in order:

  1. ``ctrlq_hw_create``: first runs ``ctrlq_kern_init(devid, chip_type)`` to
     initialize the global information structure and locks, then runs
     ``_ctrlq_hw_init``: reset the pending-channel bitmap, read global
     information (``ctrlq_kern_get_info``), then for each die enable UE
     resources (write ``FUNCTION_ENTITY_ENABLE``), read the die
     configuration, run attr_pack and ``ctrlq_kern_chan_add`` per channel
     (channels whose UE is uninitialized and which are not S channels have the
     ``status`` bit set to skip), and collect the CQ/SQ address ranges;
     finally ``ctrlq_asic_addr_map`` merge-maps the CQ/SQ memory of all dies
     onto ``ctrlq_mem_base`` (constraint: the memory of each die is contiguous
     with no holes).
  2. ``ctrlq_hw_add``: hooks the instance into ``g_ctrlq_hw[devid]`` (on
     failure, ``ctrlq_hw_destroy`` rolls back in reverse order).

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_chip_mod_init ctrlq_hw_mod_init ctrlq_kern_mod_init ctrlq_fops_mod_init ctrlq_proc_mod_init ctrlq_irq_init

.. note::
   Kernel documentation reference: mctrlq_mod_init mctrlq_udm_notify ctrlq_kern_dev_init ctrlq_kern_dev_uninit ctrlq_kern_dev_irq_init ctrlq_kern_dev_irq_uninit

.. note::
   Kernel documentation reference: ctrlq_hw_init ctrlq_hw_uninit ctrlq_hw_create ctrlq_hw_add ctrlq_kern_init ctrlq_kern_uninit _ctrlq_hw_init ctrlq_asic_addr_map

****************************************
API Description -- UCD Commands
****************************************

UCD commands are the configuration and diagnostic entries that ctrlq provides
to user space (via the UBFABRIC ``/dev/ubfabricX`` ioctl). This module
registers 3 UCD sub-commands in total (main command ``UCD_CMD_CTRLQ``).

``sysfs`` interface: ctrlq does not provide any sysfs attribute, so this
document does not contain a sysfs section.

Channel Switch (CTRLQ_CHAN_SWITCH)
==================================

CHAN_SWITCH enables or disables the specified channel on the specified device.
Enabling first resets the CQ, then sets SQ/CQ to
``CTRLQ_CHAN_ON``; disabling resets the CQ and sets ``CTRLQ_CHAN_OFF``, so the
channel stops sending and receiving. It is mainly used in out-of-band
management scenarios (e.g. channel restart after a fault).

**Permission**: ``PROC_CTRL_MANAGED``

Sequence
--------

.. code-block:: none

     User space           UCD (fops)              core                     HW
        |                    |                     |                       |
        |--ioctl(switch)---->|                     |                       |
        |                    |--check in_len == sizeof(cmd)                |
        |                    |--dispatch by on_off                         |
        |                    |  on_off==true: enable                       |
        |                    |--ctrlq_kern_chan_enable-------------------->|
        |                    |                     |--ctrlq_get(devid)     |
        |                    |                     |--get_chan(chan_id)    |
        |                    |                     |--mutex_lock(chan)     |
        |                    |                     |--ctrlq_cq_enable      |
        |                    |                     |   |--CQ reset:        |
        |                    |                     |   |  read tail        |
        |                    |                     |   |  write head=tail->|
        |                    |                     |   |  set cq.on_off=ON |
        |                    |                     |--ctrlq_sq_enable      |
        |                    |                     |   |--set sq.on_off=ON |
        |                    |                     |--mutex_unlock(chan)   |
        |                    |                     |--ctrlq_put            |
        |                    |  on_off==false: disable (symmetric path)    |
        |<--ret--------------|<--ret---------------|                       |

Call Chain Details
------------------

  1. **handle_ucd** (``ctrlq_fops_chan_switch``): validates that the input is
     non-NULL and ``in_len == sizeof(struct ctrlq_cmd_switch)``; when
     ``on_off == true`` calls ``ctrlq_kern_chan_enable``, otherwise calls
     ``ctrlq_kern_chan_disable``.
  2. **core** (``ctrlq_kern_chan_enable/disable``): obtains the device
     instance via ``ctrlq_get``, obtains the channel via ``ctrlq_get_chan``,
     and takes ``chan->lock``.
  3. **enable** (``_ctrlq_chan_enable``): ``ctrlq_cq_enable`` (returns
     directly if ``cq.on_off`` is already ON; otherwise ``ctrlq_cq_reset``
     resets the CQ and sets it ON), then ``ctrlq_sq_enable`` sets the SQ to ON.
  4. **disable** (``ctrlq_chan_disable``): ``ctrlq_cq_disable`` resets the
     CQ and sets it OFF, then sets the SQ to OFF.
  5. **cleanup**: releases the channel lock and the device instance reference;
     on failure prints a trace and returns the error code.

Key Data Structures
-------------------

``struct ctrlq_cmd_switch``
  Input: ``chan_id`` (channel id), ``on_off`` (true = enable, false =
  disable).

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_fops_chan_switch ctrlq_kern_chan_enable ctrlq_kern_chan_disable

Channel Snapshot (CTRLQ_GET_SNAPSHOT)
======================================

GET_SNAPSHOT iterates all channels of the specified device and formats each
channel's SQ/CQ configuration (asic base / tail / head / depth) and the live
register state (read HW head / tail / depth) into a user-provided buffer for
fault diagnosis.

**Permission**: ``PROC_CTRL_ANY_USER``

Sequence
--------

.. code-block:: none

     User space           UCD (fops)              core                   HW
        |                    |                     |                      |
        |--ioctl(snap)------>|                     |                      |
        |                    |--ctrlq_kern_get_snapshot                  |
        |                    |  (directly hooked as func)                 |
        |                    |--check in_len                              |
        |                    |--ctrlq_kern_snapshot_para_check           |
        |                    |  (devid/buf/buf_len/read_size check)       |
        |                    |--kzalloc(buf_len)                          |
        |                    |--ctrlq_get(devid)                          |
        |                    |--loop channels (chan_id < ctrlq->num):     |
        |                    |   ctrlq_chan_get_snapshot                 |
        |                    |     |--SQ: ctrlq_sq_get_snapshot          |
        |                    |     |   print sqe queue scene              |
        |                    |     |--CQ: ctrlq_cq_get_snapshot          |
        |                    |     |   read cq reg head/tail/depth------->|
        |                    |     |   print cqe queue scene              |
        |                    |   cond_resched()                           |
        |                    |--ctrlq_put(devid)                          |
        |                    |--copy_snapshot_to_user                    |
        |                    |  (copy buf and read_size back to user)     |
        |<--ret--------------|<--ret (kfree buf)                          |

Call Chain Details
------------------

  1. **handle_ucd** (``ctrlq_kern_get_snapshot`` used directly as the fops
     function): validates ``in_len``, then ``ctrlq_kern_snapshot_para_check``
     validates devid, ``buf``, ``buf_len`` and ``read_size``.
  2. **buffer**: allocates a kernel buffer of ``buf_len`` and appends
     formatted text step by step.
  3. **iteration**: ``ctrlq_get`` obtains the instance and calls
     ``ctrlq_chan_get_snapshot`` per channel: the SQ side outputs the sqe
     queue (sq idr counter, queue usage/remaining) and the register state; the CQ
     side outputs the cqe queue and the live tail/head/depth values read from
     the hardware. After each channel ``cond_resched`` yields the CPU.
  4. **write back**: ``ctrlq_copy_snapshot_to_user`` copies the text and
     ``read_size`` back to user space, and releases the kernel buffer.

Key Data Structures
-------------------

``struct ctrlq_get_snapshot_in``
  Input: ``buf`` (user buffer pointer), ``buf_len`` (buffer length),
  ``read_size`` (pointer to the actual output length).

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_kern_get_snapshot ctrlq_kern_snapshot_para_check ctrlq_chan_get_snapshot

.. note::
   Kernel documentation reference: ctrlq_sq_get_snapshot ctrlq_cq_get_snapshot ctrlq_copy_snapshot_to_user

SQE Send Timestamp (CTRLQ_SEND_SQE_TS)
======================================

SEND_SQE_TS wraps the channel/queue/head information associated with an SQE
submission and triggers the ``trace_sqe`` tracepoint, serving as the debug and
performance-analysis entry of the send path. This command does not access the
hardware; it only records a trace in the kernel side.

**Permission**: ``PROC_CTRL_MANAGED``

Sequence
--------

.. code-block:: none

     User space           UCD (fops)                                trace subsystem
        |                    |                                             |
        |--ioctl(sqe_ts)---->|                                             |
        |                    |--check in_len == sizeof(struct ctrlq_sqe_ts)|
        |                    |--ctrlq_fops_sqe_ts                          |
        |                    |   |--extract chan/que/head fields from ts   |
        |                    |   |--trace_sqe(&chan,&que,&head) ---------->|
        |                    |   |                      -- record chan/que/
        |                    |   |                          msg_head(trace)|
        |                    |   |--return 0                               |
        |<--ret(0)-------- --|<--ret---------------------------------------|

Call Chain Details
------------------

  1. **handle_ucd** (``ctrlq_fops_sqe_ts``): returns ``-EINVAL`` when
     ``in == NULL`` or ``in_len != sizeof(struct ctrlq_sqe_ts)``.
  2. **assembly**: fills the chan triple (devid/die_id/chan_id) carried from
     user space, the queue (sq_tail/sq_head/sq_depth) and the message header
     (ver/type/bb_num/op_code/seq) into the ``ctrlq_chan`` / ``ctrlq_que`` /
     ``ctrlq_msg_head`` structures, and calls ``trace_sqe``.
  3. **return**: the command always returns 0.

Key Data Structures
-------------------

``struct ctrlq_sqe_ts``
  Input: devid, die_id, chan_id, sq_tail, sq_head, sq_depth, sqe_ver,
  sqe_type, sqe_bb_num, sqe_op_code, sqe_seq.

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_fops_sqe_ts trace_sqe

****************************************
API Description -- EXPORT_SYMBOL
****************************************

ctrlq exports the following symbols to other kernel modules via
``EXPORT_SYMBOL_GPL``. These symbols are the kernel-space entry points of the
ctrlq capabilities and are called directly by modules such as MAMI / LCNE.

S-Channel Address Retrieval (mami_ctrlq_schan_get_addr)
=======================================================

``mami_ctrlq_schan_get_addr`` lets kernel-space modules obtain the three types
of addresses (REG / TX / RX) of the S channel (``CTRLQ_SCHAN_ID``) on the
specified die, mapped into BAR space, for MAMI / LCNE to directly access the
S-channel registers and send/receive memory.

**Precondition**: the device is Kunpeng950 (1P/2P/4P) with connection type
``UDEV_PCIE_ESPI``; ``CTRLQ_SCHAN_ID`` is 0 in the emulation environment
(``EMU_ST``).

Sequence
--------

.. code-block:: none

     Caller module       ctrlq_kern (check_pub)   chip (addr calc)      URES/UOP
        |                    |                       |                    |
        |--schan_get_addr--->|                       |                    |
        |  (devid, die_id,   |                       |                    |
        |   type, addr)      |                       |                    |
        |                    |--ctrlq_schan_check_pub |                    |
        |                    |   |--devid < DEV_MAX_NUM                    |
        |                    |   |--feature_is_support_schan               |
        |                    |   |   (Kunpeng950 + PCIE_ESPI)              |
        |                    |   |--ctrlq_kern_get_info(die_num)           |
        |                    |   |--die_id < die_num                       |
        |                    |--type==REG ? yes:                           |
        |                    |   ctrlq_schan_get_reg_addr----------------->|
        |                    |   |                   |--ures_get_reg_base  |
        |                    |   |                   |  "ctrlq_reg_base   |
        |                    |   |                   |   _win%d_local"--->|
        |                    |   |                   |--addr = reg.addr   |
        |                    |   |                   |   + CTRLQ_REG_BASE |
        |                    |   |                   |   + SCHAN*CHAN_DIFF|
        |                    |--type==TX/RX:   ctrlq_schan_get_mem_addr    |
        |                    |   |                   |--ures_get_mem_base  |
        |                    |   |                   |  "ctrlq_mem_base   |
        |                    |   |                   |   _win%d_local"--->|
        |                    |   |                   |--addr = mem.addr   |
        |                    |   |                   |   + SCHAN*CHAN_DIFF|
        |                    |   |                   |   (+0x10000 TX)    |
        |                    |--invalid type/addr => -EINVAL               |
        |<--addr-------------|<--addr----------------|                    |

Call Chain Details
------------------

  1. **common precondition** (``ctrlq_schan_check_pub``): validates the devid
     range, the S-channel platform support (``feature_is_support_schan``) and
     the die range.
  2. **parameter check**: ``type`` must be within ``MAMI_KERN_SCHAN_ADDR_MAX``
     and ``addr`` must be non-NULL, otherwise ``-EINVAL``.
  3. **REG** (``ctrlq_schan_get_reg_addr``): reads the
     ``ctrlq_reg_base_win%u_local`` register base from URES and adds
     ``CTRLQ_REG_BASE(0x18800)`` and ``CTRLQ_SCHAN_ID * CHAN_BASE_DIFF``.
  4. **TX/RX** (``ctrlq_schan_get_mem_addr``): reads the
     ``ctrlq_mem_base_win%u_local`` memory base from URES and adds
     ``CTRLQ_SCHAN_ID * CHAN_BASE_DIFF``; TX appends another ``0x10000`` (the
     TX/RX regions are offset).
  5. **return**: 0 with a valid ``*addr``; a negative error code on failure.

Key Data Structures
-------------------

``enum mami_kern_schan_addr_type``
  ``MAMI_KERN_SCHAN_ADDR_REG`` (register), ``MAMI_KERN_SCHAN_ADDR_TX`` (send
  memory), ``MAMI_KERN_SCHAN_ADDR_RX`` (receive memory).

API Reference
=============

.. note::
   Kernel documentation reference: mami_ctrlq_schan_get_addr ctrlq_schan_get_reg_addr ctrlq_schan_get_mem_addr ctrlq_schan_check_pub

S-Channel Callback Registration (mami_ctrlq_schan_op)
=====================================================

``mami_ctrlq_schan_op`` registers or unregisters the CQE handler callback of
the S channel. After registration (``MAMI_KERN_SCHAN_OP_REG``), when the
interrupt bottom half finds a new CQE on the S channel (tail != head), it calls
the registered ``handle`` callback directly to do the moving, instead of going
through the default UED event path; unregistration
(``MAMI_KERN_SCHAN_OP_UNREG``) restores the default empty handler
``mami_ctrlq_schan_handle``.

**Precondition**: same as ``mami_ctrlq_schan_get_addr`` (Kunpeng950 + ESPI).

Sequence
--------

.. code-block:: none

     ---- register / unregister (caller context) ----

     Caller module          ctrlq_kern                                 chan->lock
        |                       |                                           |
        |--mami_ctrlq_schan_op->|                                           |
        |  (devid, die_id,      |                                           |
        |   type, ops)          |                                           |
        |                       |--ctrlq_schan_check_pub                    |
        |                       |--ctrlq_schan_check_para                   |
        |                       |   (type range / ops non-NULL /            |
        |                       |    handle non-NULL on REG / rsv all zero) |
        |                       |--ctrlq_get(devid)                         |
        |                       |--chan = get_chan(                         |
        |                       |         CTRLQ_SCHAN_ID + die*18)          |
        |                       |--mutex_lock(&chan->lock)----------------->|
        |                       |--ctrlq_chan_max_op                        |
        |                       |   REG:   store ops->handle                |
        |                       |   UNREG: = mami_ctrlq_schan_handle (empty)|
        |                       |--mutex_unlock---------------------------->|
        |                       |--ctrlq_put                                |
        |<--ret 0---------------|                                           |

     ---- the interrupt receive path triggers the callback (UID bottom-half context) ----

     Top half      Bottom half (ctrlq_irq_proc)    ctrlq_deque_all_chan         schan_ops
        |                    |                              |                       |
        |--IRQ---->set pending_chans                        |                       |
        |                    |--ctrlq_deque_all_chan------->|                       |
        |                    |  poll pending_chans          |                       |
        |                    |    -> _ctrlq_kern_deque      |                       |
        |                    |      schan_ops.handle !=     |                       |
        |                    |      empty                   |                       |
        |                    |      -> schan_cqe_exist      |                       |
        |                    |         (read cq tail/head)  |                       |
        |                    |      -> chan->schan_ops.     |                       |
        |                    |         handle(devid,        |                       |
        |                    |         die_id, NULL)------->|--invoke registered    |
        |                    |    (default UED event        |   callback            |
        |                    |     path if not registered)  |                       |

Call Chain Details
------------------

  1. **common precondition**: ``ctrlq_schan_check_pub`` (platform support +
     parameter range check).
  2. **parameter check** (``ctrlq_schan_check_para``): type must be less than
     ``MAMI_KERN_SCHAN_OP_MAX``; ops must be non-NULL; on REG ``handle`` must
     be non-NULL; rsv must be all zero.
  3. **install/restore** (``ctrlq_chan_max_op``): REG records ``ops->handle``;
     UNREG restores the default empty handler ``mami_ctrlq_schan_handle``. The
     target channel is ``chan_id = CTRLQ_SCHAN_ID + die_id *
     CTRLQ_CHAN_MAX_NUM``.
  4. **trigger**: in the interrupt bottom half ``_ctrlq_kern_deque``, when
     ``schan_ops.handle != mami_ctrlq_schan_handle``, first confirms a new CQE
     via ``schan_cqe_exist`` (reads HW tail/head), then calls the callback.
  5. **constraint**: only one callback is effective on the S channel of the
     same die at a time; the user must unregister before ``UDM_UNINIT`` to
     avoid a dangling callback after device removal.

Key Data Structures
-------------------

``enum mami_kern_schan_op_type``
  ``MAMI_KERN_SCHAN_OP_REG`` (register), ``MAMI_KERN_SCHAN_OP_UNREG``
  (unregister).

``struct mami_kern_ctrlq_schan_handle_ops``
  ``handle`` (callback: void(*)(devid, die_id, rsv)), ``rsv[16]`` (must be
  zeroed).

API Reference
=============

.. note::
   Kernel documentation reference: mami_ctrlq_schan_op ctrlq_chan_max_op ctrlq_schan_check_para mami_ctrlq_schan_handle

CQE Injection Debug (ctrlq_kern_enque_cq)
=========================================

``ctrlq_kern_enque_cq`` is compiled only in the ``CFG_BUILD_DEBUG`` debug
build and is exclusively for use by DT: it injects software-constructed CQEs
into the CQ of the specified device/channel to simulate a hardware completion
write-back, and is used to verify the event reporting chain (inject CQE ->
interrupt/poll -> event dispatch).

**Precondition**: DEBUG build (macro ``CFG_BUILD_DEBUG``) and the channel is
enabled.

Sequence
--------

.. code-block:: none

     Caller module       ctrlq_kern (debug)                      HW (CQ)
        |                       |                                  |
        |--ctrlq_kern_enque_cq->|                                  |
        |  (devid, chan_id,     |                                  |
        |   msg)                |                                  |
        |                       |--in_interrupt()? yes => -EDEADLK |
        |                       |--ctrlq_msg_check_cq              |
        |                       |   (len > 0 and <= 32*CQE_SIZE)   |
        |                       |--ctrlq_get(devid)                |
        |                       |--chan = get_chan(chan_id)        |
        |                       |--mutex_lock(&chan->cq.lock)      |
        |                       |--_ctrlq_enque_cq                 |
        |                       |   |--check chan->cq.on_off       |
        |                       |   |  (OFF -> -ENOLINK)           |
        |                       |   |--ctrlq_fill_cqe              |
        |                       |   |   split and copy CQE by len  |
        |                       |   |--ctrlq_cqe_submit            |
        |                       |   |   |--_ctrlq_cqe_can_submit   |
        |                       |   |   |   (refresh head if       |
        |                       |   |   |    queue space full)     |
        |                       |   |   |--_ctrlq_cqe_submit       |
        |                       |   |      |--mem_write(cqe)------>|
        |                       |   |      |--writel(tail)-------->|
        |                       |   |      |--update local tail    |
        |                       |--mutex_unlock(&chan->cq.lock)    |
        |                       |--ctrlq_put                       |
        |<--ret-----------------|                                  |

Call Chain Details
------------------

  1. **context and parameter check**: interrupt/softirq context is forbidden
     (``-EDEADLK``); the message length must satisfy
     ``0 < len <= sizeof(struct ctrlq_cqe) * CTRLQ_MAX_BB_NUM``.
  2. **split** (``ctrlq_fill_cqe``): calculates the number of CQEs needed by
     ``CQE_SIZE (32B)`` (at most ``CTRLQ_MAX_BB_NUM (32)``), zero-fills the
     incomplete last slice, then copies the message data.
  3. **submit** (``ctrlq_cqe_submit``): refreshes the HW head first when the
     queue space is insufficient (``_ctrlq_cq_head_update`` + ``rmb``); when
     space is sufficient, ``_ctrlq_cqe_submit`` runs: ``uop_kern_mem_write``
     writes the CQE, ``wmb``, and ``uop_kern_writel`` updates the CQ tail.
  4. **cleanup**: releases the CQE cache, unlocks, and returns the device
     reference.

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_kern_enque_cq ctrlq_fill_cqe ctrlq_cqe_submit ctrlq_msg_check_cq

Note: this symbol is for debug/test only and is not exported in product
builds.

**********************************************
Interrupt Reception and Event Reporting Path
**********************************************

Interrupt reception is the core hardware path of ctrlq at run time. It
converts CQE written back by the device through the CQ into upper-layer
consumable events (UED) or callbacks (S channel). This path underpins the
semantics of ``mami_ctrlq_schan_get_addr`` / ``mami_ctrlq_schan_op`` and
``ctrlq_kern_enque_cq``; among the UCD commands of this module, GET_SNAPSHOT
also depends on its runtime state.

Interrupt Registration
======================

``ctrlq_irq_init`` registers ``UID_TYPE_CTRLQ`` in
``FEATURE_LOADER_STAGE_9``:

  - ``g_ctrlq_intr_ops.top_half_pre_handle = ctrlq_intr_pre_handle``;
  - ``g_ctrlq_intr_ops.handle = ctrlq_intr_handle``;
  - ``g_ctrlq_intr_attr.block_flag = 1`` (work-queue bottom half);
  - ``g_ctrlq_intr_attr.max_sched_time = 50`` ms (1000 under
    ``CFG_UBFM_SUPPORT_PCIE_FPGA``).

Top Half
========

``ctrlq_intr_pre_handle`` first records a trace (``trace_irq_top_half``),
then calls ``ctrlq_irq_triggered``: it parses the ``dieid`` and the channel
interrupt-status bitmap (``chan_status``) from ``uid_irq_para``, converts
them into the local channel bitmask through the UE mapping table
(``ctrlq_get_chan_mask``), and writes the result into the ``pending_chans``
bitmap to be processed.

Bottom Half
===========

``ctrlq_intr_handle`` calls ``ctrlq_irq_proc``:

  - ``para == 0`` (periodic scheduling task): sets all valid channels of all
    dies as pending according to ``ctrlq_get_chan_mask``;
  - ``ctrlq_deque_all_chan`` loop-drains pending channels
    (``ctrlq_get_pending_chan``, polling from ``last_chan_id + 1`` on),
    skips channels already stopped in the ``status`` bitmap, and calls
    ``_ctrlq_kern_deque`` for each of them; channels returning ``-ENODEV``
    have their ``status`` bit set and are permanently removed from scanning.

Default Event Path
==================

Non-S channels (or an S channel without a registered callback) go through the
default path: ``_ctrlq_deque`` processes at most 128 CQE per round.

  1. ``_ctrlq_event_pack``: retries directly if ``cq->cqe`` is non-NULL (the
     previous event has not been sent); otherwise reads the first CQE and
     validates it (``_ctrlq_cqe_valid``: type in
     ``CQE_TYPE_TP_MEM .. CQE_TYPE_MAX``, bb_num in (0, ``CTRLQ_MAX_BB_NUM``),
     and ret must be 0 when seq < 0x8000). An illegal CQE triggers
     ``ctrlq_cqe_fault_notify_recovery`` (reset CQ + report drop event) and
     returns ``-EBADR``. For a valid CQE, records the trace, composes the
     message length (``bb_num * 32B``), continues reading the remaining CQE
     when bb_num > 1, and assembles a ``ctrlq_event`` (die_id, chan_id, len,
     data, timestamp_ns).
  2. ``_ctrlq_send_event``: ``ctrlq_type_to_event`` maps the type to the UED
     event id, and ``ued_submit_event_to_dev`` reports it; on reporting
     failure, records the first timestamp and sets ``fail_flag``, and when the
     failure lasts more than 4 s calls ``ctrlq_cq_recovery`` to drop the
     current CQE and reset the CQ; on success sets ``cq->cqe = NULL`` to
     consume the next one.

CQE type to UED event mapping:

.. list-table::
   :header-rows: 1
   :widths: 15 35

   * - CQE type
     - Mapped UED event
   * - 0x1
     - UED_EVENT_TPL_CTRL_MSG
   * - 0x2
     - UED_EVENT_DRM_CTRL_MSG
   * - 0x3
     - UED_EVENT_IRT_CTRL_MSG
   * - 0x4
     - UED_EVENT_UBQOS_CTRL_MSG
   * - 0x5
     - UED_EVENT_UB_MEM_CTRL_MSG
   * - 0x6
     - UED_EVENT_PORT_CTRLQ_MSG

S-Channel Callback Path
=======================

When the channel is an S channel (``chan_id == CTRLQ_SCHAN_ID``) and a
callback has been registered through ``mami_ctrlq_schan_op``,
``_ctrlq_kern_deque`` switches to the pass-through path: ``schan_cqe_exist``
reads the HW tail/head to determine whether a new CQE exists, and if so calls
``chan->schan_ops.handle(devid, die_id, NULL)`` to let the registering side do
the moving itself; otherwise it falls back to the default event path.

Exception and Fault Recovery
============================

  - ``-EBADR`` (invalid CQE): resets the CQ, prints 64 bytes around the first
    CQE, and reports a drop event (``mami_ctrlq_drop_cqe_event`` /
    ``ctrlq_send_cqe_drop_event``, containing devid, planeId, fChanId, cnt,
    dropTime, cqeType, bbNum).
  - ``-ENOSPC`` (no new CQE in the queue / tail has not advanced): waits for
    the next poll.
  - ``-EFAULT`` (uop access failure): does not recover head, waits for the
    next poll so the CQE is resent.
  - ``-EINVAL`` (invalid event_id): does not recover, waits for the next poll.
  - UED reporting timeout (4 s): drops the CQE and resets the CQ to recover
    the queue.

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_irq_init ctrlq_intr_pre_handle ctrlq_intr_handle ctrlq_irq_proc ctrlq_irq_triggered ctrlq_deque_all_chan

.. note::
   Kernel documentation reference: _ctrlq_kern_deque _ctrlq_deque _ctrlq_event_pack _ctrlq_send_event ctrlq_type_to_event ued_submit_event_to_dev

.. note::
   Kernel documentation reference: schan_cqe_exist ctrlq_cqe_fault_notify_recovery ctrlq_cq_recovery ctrlq_send_cqe_drop_event

****************************************
Proc Debug Interface
****************************************

The proc sub-module provides debug and status information under
``/proc/mami/ctrlq/``.

Proc Entries
============

.. list-table::
   :header-rows: 1
   :widths: 22 8 60

   * - Path
     - Mode
     - Description
   * - /proc/mami/ctrlq/ctrlq
     - 0444
     - Iterate all devices and output the SQ/CQ configuration and live
       register state (head / tail / depth) per channel.

Display Output
==============

``ctrlq_proc_show`` iterates device ids from 0 to ``DEV_MAX_NUM``, and for
each existing device iterates all channels through ``ctrlq_kern_show``:

  - SQ display: the asic tail/head/depth/base addresses, and the live
    tail/head/depth read from the hardware.
  - CQ display: the asic tail/head/depth/base addresses and the live register
    values.

API Reference
=============

.. note::
   Kernel documentation reference: ctrlq_proc_mod_init ctrlq_proc_mod_uninit ctrlq_proc_show ctrlq_kern_show

Constants
=========

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Name
     - Value / Description
   * - CTRLQ_SCHAN_ID
     - 15 (0 under the EMU_ST emulation), channel id of the S channel
   * - CTRLQ_CHAN_MAX_NUM
     - 18 (base value of the max channel number per die)
   * - CTRLQ_CHAN_MAX_NUM_ASCEND_V2
     - 21 (Ascend_v2)
   * - CTRLQ_CHAN_MAX_NUM_V2_KUNPENG_960
     - 45 (Kunpeng960)
   * - CTRLQ_CHAN_MAX_NUM_SSD
     - 4 (HSSU_V100)
   * - CTRLQ_MAX_BB_NUM
     - 32 (max BB number of a single message)
   * - CTRLQ_SEQ_MIN / CTRLQ_SEQ_MAX
     - 0x8000 / 0xFFFF (seq allocation range)
   * - CTRLQ_DEPTH_STRIDE
     - 8 (multiple between the register-reported depth and the actual depth)
   * - CTRLQ_REG_BASE
     - 0x18800 (S-channel register base offset)
   * - CHAN_BASE_DIFF
     - 0x20000 (address gap between adjacent channels)
   * - SQE / CQE size
     - 32 bytes (enforced by BUILD_BUG_ON)
   * - CQ / SQ per-round processing limit
     - 128 (_ctrlq_deque loop)
   * - UED event failure timeout
     - 4 s (UED_ERR_TIMEOUT)
   * - UID scheduling merge window
     - 50 ms (1000 ms for PCIE FPGA)

****************************************
UCD Command Summary
****************************************

CTRLQ registers the following ioctl commands under ``UCD_CMD_CTRLQ``:

.. list-table::
   :header-rows: 1
   :widths: 22 22 56

   * - Command
     - Permission
     - Description
   * - CTRLQ_CHAN_SWITCH
     - PROC_CTRL_MANAGED
     - Enable/disable the specified channel
   * - CTRLQ_GET_SNAPSHOT
     - PROC_CTRL_ANY_USER
     - Get the SQ/CQ snapshot of all channels of the device
   * - CTRLQ_SEND_SQE_TS
     - PROC_CTRL_MANAGED
     - Report the SQE send timestamp (trace)

Support
=======

If there is any issue or question, please contact the vendor's support channel.
