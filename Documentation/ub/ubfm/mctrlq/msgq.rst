.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


***************************
MESSAGE QUEUE DRIVER (MSGQ)
***************************

MSGQ is the message queue subsystem of the ``mctrlq`` module in the MAMI
kernel subsystem. It provides a management-plane message transport between
the local host and remote devices (or between dies), implemented on top of
dedicated hardware message queues, with optional request/reply semantics.

MSGQ is polled by a per-device delayed work with a 50 ms period (one channel
per die). Its counterpart ``ctrlq`` is an interrupt-driven control queue, one
channel per device. The two subsystems are designed for different purposes:

  - ``ctrlq`` moves small, fixed-size control messages over data-path
    channels; ``msgq`` carries variable-length management messages through a
    dedicated payload area.
  - ``msgq`` supports optional request/reply matching by MSN; ``ctrlq``
    delivers one-way events to user space.

Both subsystems share the same surrounding infrastructure: UCD commands for
user-space control, UED events for user-space notification, URES keys for
device configuration, UDM notifiers for device lifecycle events, UOP
primitives for register and queue memory access, and ULOG for log output.

This document introduces the composition of the MSGQ driver, the user-visible
interfaces it provides (two UCD commands and one exported kernel API), the
message submission and reply-waiting flow, and the related debugging
information.

Acronyms
========

The following acronyms are used throughout this document:

================================ ================================================
Acronym                          Full Name
================================ ================================================
MAMI                             Management Architecture and Model Interface
MSGQ                             Message Queue
UCD                              UB Device Command Dispatch
UED                              UB Device Event Dispatch
URES                             UB Device Resource
UDM                              UB Device Management
UOP                              UB Device Operation
ULOG                             UB Device Log
MSN                              Message Sequence Number
SQ                               Submission Queue
CQ                               Completion Queue
RQ                               Reply Data Queue
SQE                              Submission Queue Entry
CQE                              Completion Queue Entry
================================ ================================================


Overview
========

MSGQ uses the ``feature_loader`` mechanism to auto-initialize its submodules
in ordered stages, together with the other mctrlq modules. The mctrlq module
entry (``mctrlq_mod_init``) runs the feature loader and registers a UDM
device notifier at ``UDM_PRI2``; the notifier drives the per-device
init/uninit flow. The msgq submodules are loaded in the following order:

  - Stage 0: SoC adaptation (``msgq_soc_v1``) registers the SoC operations
    for the supported uDie versions.
  - Stage 1: hardware lifecycle (``msgq_hw``), user command entry
    (``msgq_fops``) and the proc interface (``msgq_proc``) are initialized.

.. code-block:: text

     user space (management plane)
         |
         |-- ioctl (UCD_CMD_MSGQ) / UED events / URES keys
         |
         v
     +--------------------------------------------------------+
     |                    mctrlq module                       |
     |  +-----------+            +-------------------------+  |
     |  |   ctrlq   |            |         msgq            |  |
     |  | (intr)    |            |  core  hw  soc  fops    |  |
     |  |           |            |  proc  dispatch  trace  |  |
     |  +-----------+            +-------------------------+  |
     +--------------------------------------------------------+
         |                             |
         |  UOP access primitives      |  queue memory & registers
         |  (uop_kern_readl/writel,    |  (uop_kern_mem_read/write)
         v                             v
     +--------------------------------------------------------+
     |              device management firmware                |
     +--------------------------------------------------------+

Architecture
------------

A msgq *channel* is a set of three hardware ring queues per die, located in
device-accessible memory:

  - **SQ** -- the host posts *submission queue entries*
    (SQE) that describe a message. Each SQE points to a payload slot that
    holds the request data. The host writes the payload first, then the SQE,
    then publishes the new tail.
  - **CQ** -- the device firmware writes one *completion
    queue entry* (CQE) per completed message. A CQE carries the status, the
    MSN and the position of the reply data inside
    the RQ.
  - **RQ** -- a ring of fixed-size blocks that holds reply
    payloads.

The remote side of a queue -- the device management firmware -- consumes the
entries the host produces and produces the entries the host consumes. The
driver is agnostic to the underlying bus (PCIe, I3C, ...): all registers and
queue memory are read/written through UOP APIs. The device does not generate
a message interrupt for msgq; the receive path is polled by a per-device
delayed work every 50 ms.

The msgq submodules are summarized below; only the submodules that expose
user-visible interfaces (UCD commands or exported kernel APIs) are described
in detail in this document:

  - 1) User command entry (``msgq_fops``): the UCD commands ``MSGQ_SQ_SEND``
       and ``MSGQ_GET_SNAPSHOT``. Described in detail (see `Functions`_).
  - 2) Core kernel API (``msgq_kern``): the exported kernel send API
       ``msgq_kern_send()`` and the kernel send/deque entry points. Described
       in detail (see `Functions`_).
  - 3) Queue engine (``msgq_sq.c`` / ``msgq_cq.c`` / ``msgq_rq.c``): the
       SQ/CQ/RQ ring implementation, MSN allocation and reply waiting.
  - 4) Channel and device management (``msgq_kern.c``): per-device/per-die
       channel objects and the kernel-facing send/deque APIs.
  - 5) Hardware lifecycle and polling (``msgq_hw``): per-device init/uninit,
       the 50 ms receive polling work, channel health probing and recovery.
  - 6) SoC adaptation (``msgq_soc_v1``): per-SoC-version memory topology
       selection and queue register programming.
  - 7) Event dispatch (``msgq_dispatch``): maps incoming message codes and
       sub-codes to UED events.
  - 8) Proc interface and tracepoints (``msgq_proc`` / ``msgq_trace.h``):
       ``/proc/mami/msgq/msgq`` and the ftrace tracepoints used for debugging.

Overall Timing
--------------

The diagram below shows the lifecycle of a msgq channel, from device addition
to removal, and the ordering between the send, reply and receive paths.

.. code-block:: none

     ====================== Device Lifecycle ======================

     UDM_INIT (device add)
         |
         +-- msgq_kern_dev_init:
         |     read die_num / uDie version from URES
         |     msgq_kern_init (devid): create struct msgq_dev
         |       +-- local device: register kernel msg op of UOP
         |     msgq_hw_attr_dev_init: SoC ops init (queue memory pools)
         |     for each die:
         |       get_attr (program queue registers) + msgq_kern_chan_add
         |     start delayed work msgq_hw_work (50 ms period)
         |
         v
     +------------------------------------------------------------------+
     |                      Channel Active Window                        |
     |                                                                  |
     |  (1) MSGQ_SQ_SEND (UCD) or msgq_kern_send (kernel)               |
     |        |                                                          |
     |        +-- msgq_chan_send: MSN alloc -> SQE submit -> CQ wait     |
     |        |                 -> RQ read (reply) -> MSN release        |
     |        |                                                          |
     |        +-- async send: SQE submit only, no reply wait             |
     |        |                                                          |
     |  (2) Receive path (delayed work, every 50 ms):                    |
     |        msgq_kern_deque -> cache CQE from hw ring to sw ring       |
     |        -> map msg code to UED event -> submit event to dev        |
     |                                                                  |
     |  (3) MSGQ_GET_SNAPSHOT (UCD): dump SQ/CQ/RQ state of all dies    |
     +------------------------------------------------------------------+
         |
         v
     UDM_UNINIT (device remove)
         |
         +-- cancel delayed work
         +-- SoC ops uninit (tear down queue memory pools)
         +-- msgq_kern_uninit: destroy struct msgq_dev

Functionality Dependencies
--------------------------

Some functions of the msgq driver rely on the surrounding infrastructure and
on the configuration prepared by the management software:

  - 1) UCD is the interface: the send and snapshot commands are registered as
       UCD device features and dispatched through the mami device ioctl.
  - 2) UDM drives lifecycle: device add/remove events propagate through the
       UDM notifier chain (msgq registers at ``UDM_PRI2``) and trigger the
       per-device msgq init/uninit.
  - 3) URES provides configuration: the die number, the uDie version and the
       queue memory regions are described by URES keys/attributes prepared by
       the management software during device bring-up.
  - 4) UOP provides access: all queue registers and queue memory are
       read/written through UOP primitives (``uop_kern_readl``,
       ``uop_kern_writel``, ``uop_kern_mem_read``, ``uop_kern_mem_write``).
       For the local device, msgq registers a kernel message operation
       (``uop_kern_register_msg_ops``) so that other kernel modules can send
       messages through UOP. For devices whose chip type uses the DPU
       interface, messages are routed to ``uop_kern_send_recv_msg()`` and the
       local queues are not used.
  - 5) UED delivers events: incoming messages are published to user space as
       UED events (``ued_submit_event_to_dev``).
  - 6) ULOG provides logging: all mctrlq log macros are backed by ulog.

Usage Example
-------------

Sending a message from a kernel caller through the exported API::

    struct msgq_msg msg = { .type = MSGQ_TYPE_PROTOCOL,
                            .opcode = MY_OPCODE,
                            .req_len = sizeof(req),
                            .req_data = &req,
                            .rep_len = sizeof(rep),
                            .rep_data = &rep };

    ret = msgq_kern_send(devid, die_id, &msg);
    if (ret != 0) {
        /* handle the failure */
    }


Device Nodes
============

MSGQ does not create a char device node of its own. User space reaches the
driver through the MAMI device char node (for example ``/dev/ubfabricX`` or
the manager node) created by the ubfabric infrastructure, issuing the ioctl
command ``UCD_CMD_MSGQ`` through the UCD framework. All msgq commands are
registered as device-specific features (``ucd_register_dev_cmd``), so they
are bound to a device instance.

Proc Filesystem
===============

MSGQ provides debugging and status information through the proc filesystem
under ``/proc/mami/``:

========================================= ======================================
Path                                      Purpose
========================================= ======================================
/proc/mami/msgq/msgq                      Show the SQ/CQ/RQ register values,
                                          the software queue state and the
                                          head SQE/CQE of every channel of
                                          every device
========================================= ======================================


Module Initialization
=====================

The init sub-module is the entry point of the msgq driver. The msgq
submodules are registered with ``feature_loader`` and initialized in ordered
stages together with the other mctrlq modules.

Initialization Sequence
-----------------------

``mctrlq_mod_init()`` performs the following steps in order:

  1. ``module_feature_auto_init()``: run the feature loader, which
     initializes the msgq submodules at their registered stages:

     - Stage 0: SoC adaptation (``msgq_soc_v1_mod_init``) registers the SoC
       operations for the supported uDie versions through
       ``msgq_kern_hw_register``.
     - Stage 1: hardware lifecycle (``msgq_hw_mod_init``, which internally
       calls ``msgq_kern_mod_init``), user command entry
       (``msgq_fops_mod_init``) and the proc interface
       (``msgq_proc_mod_init``).

  2. ``udm_notifier_register()`` at ``UDM_PRI2``: register
     ``mctrlq_udm_notify`` so that device add/remove events drive the
     per-device msgq init/uninit.

If any step fails, all previously initialized resources are rolled back in
reverse order. ``mctrlq_mod_exit()`` reverses the sequence.

Device Lifecycle Callback
-------------------------

``mctrlq_udm_notify(devid, action, val)`` is invoked by the UDM notifier
chain. For msgq, ``msgq_kern_dev_init(devid)`` runs on device add:

  1. Read ``die_num`` and the uDie version from URES; allocate ``struct
     msgq_hw``.
  2. ``msgq_kern_init(devid)`` -- create ``struct msgq_dev`` and add it to
     the global table. If the device is the *local* device, register the
     kernel message operation of the UOP layer so that other kernel modules
     can send msgq messages through ``uop_kern_send_recv_msg()``.
  3. ``msgq_hw_attr_dev_init`` -- call the SoC ops ``init``, which allocates
     the queue memory pools.
  4. For every die, call the SoC ops ``get_attr`` to program the queue
     registers and obtain ``struct msgq_attr``, then
     ``msgq_kern_chan_add(devid, die_id, &attr)`` to create the channel
     (SQ, CQ, RQ). Each queue reads back its registers during creation and
     validates depth/head/tail.
  5. Start the per-device delayed work ``msgq_hw_work`` (50 ms period) that
     drives the receive path.

On device removal, the delayed work is canceled, the SoC pools are torn
down and the device object is removed.

API
---

.. code-block:: c

    int  mctrlq_mod_init(void);
    void mctrlq_mod_exit(void);

    int  msgq_kern_dev_init(u32 devid);
    void msgq_kern_dev_uninit(u32 devid);

    int  msgq_kern_init(u32 devid);
    void msgq_kern_uninit(u32 devid);

    int  msgq_kern_chan_add(u32 devid, u32 die_id, struct msgq_attr *attr);
    int  msgq_kern_mod_init(void);


Functions
=========

The driver exposes its functionality through two UCD commands and one
exported kernel API. There are no sysfs entries. The driver does not expose
any configuration through ``/sys``; all runtime configuration comes from
URES keys prepared by the management software. The table below summarizes
the user-visible interfaces, their permissions, and their handlers.

.. list-table:: Interface overview
   :widths: 24 22 24 30
   :header-rows: 1

   * - Interface
     - Type
     - Permission / Scope
     - Description
   * - ``MSGQ_SQ_SEND``
     - UCD command (``UCD_CMD_MSGQ``)
     - ``PROC_CTRL_MANAGED``
     - Submit a message: copies the request from user space, then sends it
       synchronously or asynchronously; the reply is copied back for a
       synchronous send.
   * - ``MSGQ_GET_SNAPSHOT``
     - UCD command (``UCD_CMD_MSGQ``)
     - ``PROC_CTRL_ANY_USER``
     - Dump the SQ/CQ/RQ register and software-queue state of all dies into
       a user buffer (up to 64 KiB).
   * - ``msgq_kern_send``
     - Exported kernel API (``EXPORT_SYMBOL_GPL``)
     - Other kernel modules
     - Synchronous send from kernel space; returns the reply in the caller
       provided buffer.

Both UCD commands are registered at feature-loading Stage 1 as device-specific
features (``ucd_register_dev_cmd``) under the main command ``UCD_CMD_MSGQ``,
declared by the static command table::

    static const struct msgq_fops g_msgq_fops[] = {
        [0] = MSGQ_FOPS_ADD_CMD(MSGQ_SQ_SEND, PROC_CTRL_MANAGED, msgq_fops_send),
        [1] = MSGQ_FOPS_ADD_CMD(MSGQ_GET_SNAPSHOT, PROC_CTRL_ANY_USER, msgq_kern_get_snapshot),
    };

Send Message (MSGQ_SQ_SEND)
---------------------------

``MSGQ_SQ_SEND`` submits a message to the queue of the target die. The
command is restricted to management processes (``PROC_CTRL_MANAGED``). The
handler copies the request payload from user space, then either posts the
message without waiting (asynchronous send, MSN supplied by the caller) or
waits for the matching response and copies the reply back (synchronous
send).

Timing
~~~~~~

.. code-block:: none

     User Space        msgq_fops_send          msgq_kern_send            queue engine
         |                   |                       |                       |
         |--ioctl(UCD_CMD_MSGQ, MSGQ_SQ_SEND)------->|                       |
         |                   |--validate in_len      |                       |
         |                   |--msg init:            |                       |
         |                   |  build struct msgq_msg|                       |
         |                   |  alloc req/rep buffers|                       |
         |                   |--u2k copy:            |                       |
         |                   |  copy_from_user(req)  |                       |
         |                   |--async? ------------->|                       |
         |                   |   |  msgq_kern_send_async(devid, die_id,      |
         |                   |   |      msn, &msg) -- no reply wait          |
         |                   |   v                                            |
         |                   |--sync: msgq_kern_send(devid, die_id, &msg)-->|
         |                   |                       |--udm_get_dev_attr     |
         |                   |                       |  (DPU route if 1825)  |
         |                   |                       |--msgq_msg_check       |
         |                   |                       |--msgq_get/get_chan    |
         |                   |                       |--msgq_chan_send------>|
         |                   |                       |   MSN alloc           |
         |                   |                       |   SQ submit           |
         |                   |                       |   CQ wait (reply)     |
         |                   |                       |   RQ read (reply)     |
         |                   |                       |   MSN release         |
         |                   |<--------------------- |<----------------------|
         |                   |--k2u copy:            |                       |
         |                   |  copy_to_user(rep)    |                       |
         |                   |--msg uninit:          |                       |
         |                   |  free req/rep buffers |                       |
         |<--ret-------------|                       |                       |

Call Flow
~~~~~~~~~

  1. **UCD dispatch**: the caller issues ``ioctl(fd, UCD_IOCTL_DEV_CMD, ...)``
     on the MAMI device node with main command ``UCD_CMD_MSGQ`` and sub
     command ``MSGQ_SQ_SEND``. UCD checks the ``PROC_CTRL_MANAGED``
     permission bit, allocates the input/output buffers and calls
     ``msgq_fops_send()``.
  2. **Input validation** (``msgq_fops_send``): ``in_len`` must match
     ``sizeof(struct msgq_cmd_send)``; the request data must be non-NULL with
     ``0 < req_len <= PAGE_SIZE``.
  3. **Message setup** (``_msgq_fops_msg_init``): builds ``struct msgq_msg``
     from the command input and allocates the kernel request/reply buffers.
  4. **User-to-kernel copy** (``_msgq_fops_msg_u2k``): copies the request
     payload from user space into the kernel buffer.
  5. **Send**: if ``async`` is set, ``msgq_kern_send_async()`` posts the SQE
     without waiting for a response -- the caller supplies the MSN (``msn``)
     and matches the eventual response itself; otherwise ``msgq_kern_send()``
     runs (see below), and on success ``_msgq_fops_msg_k2u`` copies the reply
     payload back to user space (``copy_to_user``).
  6. **Cleanup** (``_msgq_fops_msg_uninit``): frees the kernel buffers.

``msgq_kern_send()`` internally:

  a. Queries the device attribute; for devices whose chip type uses the DPU
     interface the message is routed to ``uop_kern_send_recv_msg()`` and the
     local queues are not used.
  b. Validates the message (``msgq_msg_check``) and looks up the device and
     the channel of the target die (``msgq_get`` / ``msgq_get_chan``).
  c. ``msgq_chan_send()``: allocates an MSN from the per-type bitmap, writes
     the request payload and the SQE (payload first, then SQE, then the new
     SQ tail, with write memory barriers), waits for the matching response
     CQE (``msgq_cq_wait``, 8 s timeout by default, 60 s for long-running
     operations such as firmware upgrade), copies the reply payload from the
     RQ (``msgq_rqe_get``) and releases the MSN.

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct msgq_cmd_send``
    UCD input: ``async`` (bool), ``die_id``, ``msn`` (for async sends) and
    the ``struct msgq_msg`` payload.

``struct msgq_msg``
    Message descriptor exchanged by kernel and user-space callers: message
    type, packed opcode, local flag, device type, request length/data and an
    optional reply buffer.

API
~~~

.. code-block:: c

    static int msgq_fops_send(u32 devid, void *in, u32 in_len,
                              void *out, u32 out_len);
    int msgq_kern_send(u32 devid, u32 die_id, struct msgq_msg *msg);
    int msgq_kern_send_async(u32 devid, u32 die_id, u16 msn,
                             struct msgq_msg *msg);

Get Snapshot (MSGQ_GET_SNAPSHOT)
---------------------------------

``MSGQ_GET_SNAPSHOT`` dumps the queue state of all dies into a user buffer.
Unlike the send command, it is open to any process with the device file
permission (``PROC_CTRL_ANY_USER``). The dump text (SQ/CQ/RQ register values
and software queue state) can be included in the fault logs collected by the
management software.

Timing
~~~~~~

.. code-block:: none

     User Space        msgq_kern_get_snapshot           core
         |                       |                        |
         |--ioctl(UCD_CMD_MSGQ, MSGQ_GET_SNAPSHOT)------>|
         |                       |--validate in_len       |
         |                       |--validate para         |
         |                       |  (buf/buf_len)         |
         |                       |--kzalloc(buf_len)      |
         |                       |--msgq_get(devid)       |
         |                       |--for each die:         |
         |                       |  msgq_chan_get_snapshot
         |                       |    (registers + sw queue|
         |                       |     state into buf)     |
         |                       |--copy snapshot to user |
         |                       |  (buf, read_size)      |
         |                       |--kfree(buf)            |
         |<--ret-----------------|                        |

Call Flow
~~~~~~~~~

  1. **UCD dispatch**: ``ioctl`` with main command ``UCD_CMD_MSGQ`` and sub
     command ``MSGQ_GET_SNAPSHOT``; the ``PROC_CTRL_ANY_USER`` permission
     bit allows any process that holds the device file permission.
  2. **Input validation**: ``in_len`` must match ``sizeof(struct
     msgq_get_snapshot_in)``; the buffer and its length are validated
     (``msgq_kern_snapshot_para_check``).
  3. **Dump**: a kernel buffer of ``buf_len`` bytes is allocated; for every
     die that has a channel, ``msgq_chan_get_snapshot()`` appends the
     SQ/CQ/RQ register values (read back through UOP) and the software queue
     state. If the device is missing, a diagnostic line is produced through
     ``ulog_kern_dump_snapshot``.
  4. **Return**: ``msgq_copy_snapshot_to_user`` copies the dump text into
     the user buffer and stores the size into ``read_size``; the kernel
     buffer is freed.

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct msgq_get_snapshot_in``
    UCD input: user buffer pointer (``buf``), buffer length (``buf_len``)
    and the output size pointer (``read_size``).

API
~~~

.. code-block:: c

    int msgq_kern_get_snapshot(u32 devid, void *in, u32 in_len,
                                void *out, u32 out_len);

Exported Kernel API (EXPORT_SYMBOL_GPL)
---------------------------------------

The msgq driver exports its kernel send API to the rest of the MAMI kernel
subsystem. All exported symbols use ``EXPORT_SYMBOL_GPL``, so that only
GPL-compatible kernel modules can use them; the ``mctrlq`` module itself is
released under GPL (``MODULE_LICENSE("GPL")``). The complete list of
exported symbols is:

.. list-table:: Exported symbols
   :widths: 26 30 44
   :header-rows: 1

   * - Symbol
     - Export form
     - Description
   * - ``msgq_kern_send``
     - ``EXPORT_SYMBOL_GPL``
     - Synchronous send of a message to the target die; blocks until the
       matching response arrives (or times out) and returns the reply in the
       caller-provided buffer.

``msgq_kern_send(devid, die_id, msg)`` is the only exported interface of the
msgq driver. All other ``msgq_kern_*`` functions (``msgq_kern_deque``,
``msgq_kern_chan_add``, ``msgq_kern_chan_check``, ``msgq_kern_init``, ...)
are internal to the driver and are not exported.

Call Flow
~~~~~~~~~

  1. ``udm_get_dev_attr(devid)``: query the device attribute. For devices
     whose chip type uses the DPU interface, the message is routed to
     ``uop_kern_send_recv_msg()`` and the local queues are not used.
  2. ``msgq_msg_check(devid, die_id, msg)``: validate the message (request
     data non-NULL; ``MSGQ_TYPE_PROTOCOL`` messages need a request length of
     at least ``sizeof(struct msg_pkt_header)``).
  3. ``msgq_get(devid)`` / ``msgq_get_chan(mdev, die_id)``: look up the
     device and the channel of the target die.
  4. ``msgq_chan_send(chan, msg)``: allocate an MSN, submit the SQE, wait
     for the matching response CQE and read the reply from the RQ (see
     `Send Message (MSGQ_SQ_SEND)`_).
  5. ``msgq_put(mdev)``: release the device reference.

API
~~~

.. code-block:: c

    int msgq_kern_send(u32 devid, u32 die_id, struct msgq_msg *msg);

Proc Debug Interface and Tracepoints
------------------------------------

The proc interface (``msgq/core/msgq_proc.c``) and the tracepoints
(``msgq/core/msgq_trace.h``) provide the debugging interfaces of the msgq
driver. Both are registered at feature-loading Stage 1.

Proc Entries
~~~~~~~~~~~~

``/proc/mami/msgq/msgq`` (created with ``proc_mkdir("mami/msgq")`` plus
``proc_create_data("msgq")``) prints, for every channel of every device:

  - the SQ/CQ/RQ register values (head/tail/depth) read back through UOP;
  - the software queue state (head/tail/depth/pld_size);
  - the head SQE/CQE of the queue.

The same information can be dumped into a user buffer through the
``MSGQ_GET_SNAPSHOT`` UCD command (up to 64 KiB).

Tracepoints
~~~~~~~~~~~

``msgq_trace.h`` defines the tracepoints ``msgq_sqe``, ``msgq_hw_cqe``,
``msgq_sw_cqe``, ``msgq_rq_cqe`` and ``msgq_rq``, which trace every SQE
submit and every CQE move between the hardware ring, the software cache and
the RQ.


Loading and Unloading
=====================

.. code-block:: none

    # Load
    insmod mami_mctrlq.ko

    # Unload
    rmmod mami_mctrlq

On load, the ``feature_loader`` framework initializes the msgq submodules in
their registered stages (Stage 0: SoC adaptation; Stage 1: hardware
lifecycle, user command entry, proc interface), and the mctrlq module
registers a UDM device notifier at ``UDM_PRI2``. Channel instances are not
created at load time; they are instantiated dynamically when devices are
added via the UDM notifier chain.

On unload, the sequence is reversed: the UDM notifier is unregistered and
the submodules are uninitialized in reverse order.

The ``mctrlq`` module registers no module parameters. The verbosity of the
driver log output is controlled by the ubfabric logging (``ulog``) framework
shared by all MAMI drivers:

  - The driver prints through the mctrlq log macros (``mctrlq_err``,
    ``mctrlq_warn``, ``mctrlq_info``, ``mctrlq_debug``), which are backed by
    the ulog framework and filtered against the kernel log level plus
    per-device default and current log levels.
  - Repeated error prints are rate-limited: ulog keeps an LRU cache of recent
    log hashes and suppresses duplicates (``mctrlq_err_limit``).
  - The management software can adjust the log levels at runtime through the
    UCD commands ``ULOG_SET_LOG_CFG`` / ``ULOG_GET_LOG_CFG`` (main command
    ``UCD_CMD_ULOG``); the current levels can be read from
    ``/proc/mami/ulog/log_level``.

The module is released under GPL (``MODULE_LICENSE("GPL")``).


Security Considerations
=======================

* Character device nodes are created by the UBFABRIC infrastructure with
  permissions ``0660`` and owned by a dedicated MAMI user (uid/gid 1201),
  restricting access to management processes.
* Each UCD command has per-command access control via ``proc_ctrl_bitmap``:
  ``MSGQ_SQ_SEND`` requires ``PROC_CTRL_MANAGED`` (management process only);
  ``MSGQ_GET_SNAPSHOT`` allows any process with device file permission
  (``PROC_CTRL_ANY_USER``).
* All user-space pointers are validated via ``copy_from_user`` /
  ``copy_to_user``. Input lengths are checked against the expected structure
  sizes, the request payload is bounded by ``PAGE_SIZE``, and the snapshot
  buffer length is validated before the kernel buffer is allocated.
* The exported kernel API is restricted to GPL-compatible modules through
  ``EXPORT_SYMBOL_GPL``.
* The driver does not interpret the payload beyond the packet header; the
  message semantics are implemented in user space.


File Layout
===========

.. code-block:: none

    drivers/ub/ubfm/mctrlq/
    +-- mctrlq_init.c                  # Module entry (module_init/exit)
    +-- ctrlq/                         # ctrlq subsystem (interrupt-driven)
    +-- msgq/
        +-- msgq_init.c                # msgq_kern_dev_init/uninit (UDM notifier)
        +-- core/
        |   +-- msgq_fops.c            # UCD command handlers (2 commands)
        |   +-- msgq_fops.h
        |   +-- msgq_kern.c            # Kernel send/deque APIs, exported API
        |   +-- msgq_kern.h
        |   +-- msgq_sq.c              # SQ ring implementation
        |   +-- msgq_sq.h
        |   +-- msgq_cq.c              # CQ ring implementation
        |   +-- msgq_cq.h
        |   +-- msgq_rq.c              # RQ ring implementation
        |   +-- msgq_rq.h
        |   +-- msgq_dispatch.c        # Message code -> UED event mapping
        |   +-- msgq_proc.c            # /proc/mami/msgq/msgq entries
        |   +-- msgq_trace.h           # ftrace tracepoints
        +-- hw/
        |   +-- msgq_hw.c              # Hardware lifecycle, delayed work
        |   +-- msgq_hw.h
        +-- hw/soc/
            +-- msgq_soc_v1.c          # SoC adaptation (Stage 0)
            +-- msgq_soc_v1.h

    include/uapi/ub/ubfm/
    +-- ubfabric.h                     # UAPI: UCD_CMD_MSGQ, msgq sub-commands
    +-- mctrlq.h                       # UAPI: struct msgq_msg, msgq types


Support
=======

If there is any issue or question, please contact the vendor's support channel.
