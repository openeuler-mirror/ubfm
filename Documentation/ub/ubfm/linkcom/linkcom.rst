.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


=================================
LINKCOM Link Communication Module
=================================

The ``linkcom`` module is a kernel-space link communication driver for Huawei
UB-based devices. It provides a mgmtq based packet channel between the host
CPU and the device's control CPU, allowing user-space applications to send and
receive UB link packets (enumeration, control, config, and UB-memory packets)
through a channel-oriented interface.

LINKCOM is built on top of the UBFABRIC infrastructure and reuses UCD, UDM,
UID, and URES. It is initialized at ``FEATURE_LOADER_STAGE_6`` via the
``feature_loader`` mechanism.

This document describes the module's architecture, the six user-visible
functional areas (five UCD commands plus the proc debug interface), and the
kernel APIs exported for each function.

Acronyms
========

The following acronyms are used throughout this document:

================================ ================================================
Acronym                          Full Name
================================ ================================================
MAMI                             Management Architecture and Model Interface
UB                               Unified Bus
UBFABRIC                         UB Fabric
UCD                              UB Device Command Dispatch
UDM                              UB Device Management
UID                              UB Interrupt Dispatch
URES                             UB Device Resource
mgmtq                            Management Queue
ISR                              Interrupt Service Routine
IRQ                              Interrupt Request
WQE                              Work Queue Entry
CQE                              Completion Queue Entry
================================ ================================================

Overview
========

The LINKCOM driver is part of the MAMI component. The kernel module
(``mami_linkcom.ko``) acts as the link-packet
interaction layer: it manages per-channel instances, routes UCD ioctl
sub-commands to function handlers, and dispatches mgmtq interrupts into
per-channel receive lists so that user-space processes can send and receive
UB link packets.

The module exposes six functional areas, defined by the static UCD command
table ``g_linkcom_ucd_cmd`` plus the proc debug interface:

::

    +-----------------------------------------------------------+
    |              User-space management process                |
    +-----------------------------------------------------------+
          | open/ioctl on /dev/ubfabricX
          v
    +-----------------------------------------------------------+
    |                    mami_linkcom.ko                        |
    |  +-------+ +-------+ +-------+ +-------+ +----------+     |
    |  | OPEN  | | CLOSE | | SEND  | | RECV  | | EVENTFD  |     |
    |  +-------+ +-------+ +-------+ +-------+ +----------+     |
    |  +-------+                                                |
    |  | Proc  |                                                |
    |  +-------+                                                |
    +-----------------------------------------------------------+
          |  UCD / UDM / UID / URES (infrastructure)
          v
    +-----------------------------------------------------------+
    |              mami_ubfabric.ko (UBFABRIC)                  |
    +-----------------------------------------------------------+
          |
          v
    +-----------------------------------------------------------+
    |                    Hardware (UDIE / NDIE)                 |
    +-----------------------------------------------------------+

Overall Timing
--------------

The diagram below shows the full lifecycle of a linkcom channel, from device
addition to removal, and the ordering dependencies between the six functions.

.. code-block:: none

     ====================== Device Lifecycle ======================

     UDM_INIT (device add)
         |
         +-- linkcom_add_dev: create dev/chan instances, out-of-band cfg
         +-- mgmtq_irq_init:  init mgmtq hw state, enable inband intr
         |
         v
     +------------------------------------------------------------------+
     |                      Channel Active Window                       |
     |                                                                  |
     |  (1) OPEN -----> obtain handle (binds pid + start_time to chan)  |
     |        |                                                         |
     |        +-- (2) EVENTFD (optional) -> bond eventfd for wakeup     |
     |        |                                                         |
     |        +-- (3) SEND (repeatable) -----> hardware transmit        |
     |        |                                         |               |
     |        +-- (4) RECV (repeatable) -----> block on wq              |
     |                  ^                               |               |
     |                  |   +---------------------------+               |
     |                  |   |                                           |
     |                  |   v                                           |
     |                  |  IRQ / delayed-work:                          |
     |                  |    read hw pkt -> add to rx list              |
     |                  |    -> signal eventfd -> wake_up(wq)           |
     |                  |   ^                                           |
     |                  |   |                                           |
     |                  +---+                                           |
     |        |                                                         |
     |        +-- (5) CLOSE -> release eventfd, flush work,             |
     |                        clear rx list, set chan idle              |
     +------------------------------------------------------------------+
         |
         v
     UDM_UNINIT (device remove)
         |
         +-- mgmtq_irq_uninit: disable inband intr, release mgmtq hw state
         +-- linkcom_del_dev:  destroy dev/chan instances

     Dependency: CLOSE / SEND / RECV / EVENTFD all require a valid handle
     from OPEN.  RECV blocks until the interrupt (or delayed-work) path
     feeds packets into the per-channel rx list.

Functionality Dependencies
--------------------------

  - **UCD**: all 5 commands are registered under ``UCD_CMD_UBLINKCOM``. UCD
    handles ioctl dispatch, permission checking, and input/output buffer
    management.
  - **UDM**: linkcom registers as a UDM notifier at ``UDM_PRI3``. Device
    add/remove events drive channel instance creation/destruction and mgmtq
    interrupt initialization.
  - **UID**: linkcom registers ``UID_TYPE_MGMTQ`` with a workqueue bottom-half
    (``block_flag = 1``) and a 50 ms coalescing window. The interrupt path feeds
    received packets into the per-channel rx list.
  - **URES**: linkcom queries resource keys from URES.

Usage Example
-------------

The following example demonstrates the complete flow of opening a channel,
sending a packet, receiving a response, and closing the channel. All ioctl
calls use ``UCD_IOCTL_DEV_CMD`` on ``/dev/ubfabric<devid>`` with the
``UCD_CMD_UBLINKCOM`` main command.

.. code-block:: c

    #include <fcntl.h>
    #include <unistd.h>
    #include <stdint.h>
    #include <string.h>

    #include "ubfabric.h"   /* UCD_IOCTL_DEV_CMD, UCD_CMD_UBLINKCOM, sub-commands */
    #include "linkcom.h"    /* linkcom_* I/O structs, handle macros */
    #include "linkcom_pkt_parse.h"  /* LINKCOM_UB_CFG_* constants */

    /* Step 1: Open the device node */
    int fd = open("/dev/ubfabric0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/ubfabric0");
        return -1;
    }

    /* Step 2: Open a channel (UBLINKCOM_OPEN) */
    struct linkcom_open_chan_in open_in = {
        .dev_id   = 0,
        .app_pid  = getpid(),
        .chan_id  = 0,
        .eventfd  = -1,  /* no eventfd; use blocking RECV */
    };
    int handle = INVALID_LINKCOM_HANDLE;
    struct ucd_ioctl_arg open_arg = {
        .main_cmd = UCD_CMD_UBLINKCOM,
        .sub_cmd  = UBLINKCOM_OPEN,
        .arg      = {
            .input     = &open_in,
            .input_len = sizeof(open_in),
            .output    = &handle,
            .output_len = sizeof(handle),
        },
    };
    if (ioctl(fd, UCD_IOCTL_DEV_CMD, &open_arg)) {
        perror("ioctl OPEN");
        close(fd);
        return -1;
    }

    /* Step 3: Send a packet (UBLINKCOM_SEND) */
    uint8_t send_buf[32];
    /* ... fill send_buf with packet payload ... */
    struct mami_pkt_send_para send_para = {
        .lgPortId = 1,
        .cfgType  = LINKCOM_UB_CFG_6,  /* inband control packet */
        .vl       = 0,
        .pkt      = send_buf,
        .pktLen   = sizeof(send_buf),
    };
    struct linkcom_send_chan_in send_in = {
        .handle     = handle,
        .send_para  = send_para,
    };
    struct ucd_ioctl_arg send_arg = {
        .main_cmd = UCD_CMD_UBLINKCOM,
        .sub_cmd  = UBLINKCOM_SEND,
        .arg      = {
            .input     = &send_in,
            .input_len = sizeof(send_in),
        },
    };
    if (ioctl(fd, UCD_IOCTL_DEV_CMD, &send_arg)) {
        perror("ioctl SEND");
    }

    /* Step 4: Receive a packet (UBLINKCOM_RECV) */
    uint8_t recv_buf[32768];
    struct linkcom_recv_chan_in recv_in = {
        .handle   = handle,
        .timeout  = 5000,   /* 5 seconds */
        .pkt_buff = recv_buf,
        .buf_len  = sizeof(recv_buf),
    };
    struct linkcom_recv_chan_out recv_out;
    struct ucd_ioctl_arg recv_arg = {
        .main_cmd = UCD_CMD_UBLINKCOM,
        .sub_cmd  = UBLINKCOM_RECV,
        .arg      = {
            .input     = &recv_in,
            .input_len = sizeof(recv_in),
            .output    = &recv_out,
            .output_len = sizeof(recv_out),
        },
    };
    if (ioctl(fd, UCD_IOCTL_DEV_CMD, &recv_arg)) {
        perror("ioctl RECV");
    } else {
        printf("Received %u bytes, reason=%u, port=%u\n",
               recv_out.recv_len, recv_out.reason, recv_out.port_index);
    }

    /* Step 5: Close the channel (UBLINKCOM_CLOSE) */
    struct linkcom_close_chan_in close_in = {
        .handle    = handle,
        .app_pid   = getpid(),
    };
    struct ucd_ioctl_arg close_arg = {
        .main_cmd = UCD_CMD_UBLINKCOM,
        .sub_cmd  = UBLINKCOM_CLOSE,
        .arg      = {
            .input     = &close_in,
            .input_len = sizeof(close_in),
        },
    };
    if (ioctl(fd, UCD_IOCTL_DEV_CMD, &close_arg)) {
        perror("ioctl CLOSE");
    }

    close(fd);

Notes:

  - ``open()`` and ``close()`` are standard POSIX calls; the five linkcom
    operations (OPEN, CLOSE, SEND, RECV, EVENTFD) are all invoked via
    ``ioctl(fd, UCD_IOCTL_DEV_CMD, &arg)``.
  - OPEN and CLOSE require ``PROC_CTRL_MANAGED`` permission (management
    process only); SEND, RECV, and EVENTFD allow any process with device
    file permission.
  - The ``handle`` returned by OPEN is an opaque 32-bit integer. Pass it
    to all subsequent SEND, RECV, EVENTFD, and CLOSE calls.
  - For async notification, replace ``.eventfd = -1`` with a valid eventfd
    (created via ``eventfd(0, EFD_NONBLOCK)``), then use
    ``UBLINKCOM_EVENTFD`` to bind it to the channel.

LINKCOM does not create its own character device nodes. User-space interaction
is performed through the UBFABRIC device nodes (``/dev/ubfabricX``) via the
``UCD_CMD_UBLINKCOM`` main command. Device nodes are created by the UBFABRIC
UDM sub-module with permissions ``0660``, owned by ``uid=1201`` (mamiUser) /
``gid=1201``.


Module Initialization
=====================

The init sub-module is the entry point of the linkcom driver. It is registered
with ``feature_loader`` at ``FEATURE_LOADER_STAGE_6`` and performs ordered
initialization of all linkcom submodules.

Initialization Sequence
-----------------------

``linkcom_init()`` performs the following steps in order:

  1. ``mgmtq_proc_init()``: create ``/proc/mami/mgmtq/`` proc entries.
  2. ``mgmtq_irq_hw_init()``: initialize per-device mgmtq hardware state array.
  3. ``linkcom_mng_init()``: create the linkcom device array-context domain.
  4. ``udm_notifier_register()`` at ``UDM_PRI3``: register
     ``linkcom_dev_mng_notify``.
  5. ``linkcom_register_dev_ucd()``: register all UBLINKCOM sub-commands.
  6. ``uid_register_intr()`` for ``UID_TYPE_MGMTQ``: register interrupt type.

If any step fails, all previously initialized resources are rolled back in
reverse order. ``linkcom_uninit()`` reverses the sequence.

Device Lifecycle Callback
-------------------------

``linkcom_dev_mng_notify(devid, action, val)`` is invoked by the UDM notifier
chain:

  - ``UDM_INIT``: ``linkcom_add_dev(devid)`` then ``mgmtq_irq_init(devid)``.
  - ``UDM_UNINIT``: ``mgmtq_irq_uninit(devid)`` then
    ``linkcom_del_dev(devid)``.

API
---

.. code-block:: c

    int  linkcom_init(void);
    void linkcom_uninit(void);

    int  linkcom_register_dev_ucd(void);
    void linkcom_unregister_dev_ucd(void);

    int  linkcom_dev_mng_notify(u32 devid, enum udm_notified_action action, u32 val);


Functions
=========

The driver provides six functional areas, each initialized as part of the
module init. The table below summarizes the UCD commands, their permissions,
and dependencies.

.. list-table:: Function overview
   :widths: 22 22 16 40
   :header-rows: 1

   * - Function
     - UCD Command
     - Permission
     - Description
   * - Open Channel
     - ``UBLINKCOM_OPEN``
     - ``PROC_CTRL_MANAGED``
     - Open a link packet channel and obtain a handle.
   * - Close Channel
     - ``UBLINKCOM_CLOSE``
     - ``PROC_CTRL_MANAGED``
     - Close a channel and release resources (depends on OPEN).
   * - Send Link Packet
     - ``UBLINKCOM_SEND``
     - ``PROC_CTRL_ANY_USER``
     - Send a link packet on a channel (depends on OPEN).
   * - Receive Link Packet
     - ``UBLINKCOM_RECV``
     - ``PROC_CTRL_ANY_USER``
     - Receive a link packet from a channel (depends on OPEN).
   * - Eventfd Notification
     - ``UBLINKCOM_EVENTFD``
     - ``PROC_CTRL_ANY_USER``
     - Bond an eventfd to a channel for async notification (depends on OPEN).
   * - Proc Debug Interface
     - -- (non-UCD)
     - --
     - Debug and status interface under ``/proc/mami/mgmtq/``.

Open Channel (UBLINKCOM_OPEN)
-----------------------------

OPEN allocates a communication channel for the calling process and returns an
opaque handle used by all subsequent operations. The channel is bound to the
process's tgid and ``start_time`` to detect PID reuse.

Timing
~~~~~~

.. code-block:: none

     User Space          handle_ucd              core                 mng
         |                   |                    |                    |
         |--ioctl(OPEN)----->|                    |                    |
         |                   |--validate in/out   |                    |
         |                   |--core_open-------->|                    |
         |                   |                    |--get_pid_start_time|
         |                   |                    |--get_dev_inst----->|
         |                   |                    |--check chan_id     |
         |                   |                    |--mutex_lock        |
         |                   |                    |  if chan IDLE or   |
         |                   |                    |     owner exited:  |
         |                   |                    |    bind pid+time   |
         |                   |                    |    occupy=OCCUPIED |
         |                   |                    |    handle=MASK()   |
         |                   |                    |  else:             |
         |                   |                    |    handle=INVALID  |
         |                   |                    |    ret=-EINVAL     |
         |                   |                    |--mutex_unlock      |
         |                   |                    |--put_dev_inst----->|
         |                   |<--handle-----------|                    |
         |<--handle----------|                    |                    |

Call Flow
~~~~~~~~~

  1. **handle_ucd** (``linkcom_ucd_open_link_pkt``): validates input/output
     buffer sizes, extracts ``dev_id``, ``chan_id``, ``app_pid`` from
     ``linkcom_open_chan_in``, calls ``linkcom_core_open_link_pkt``.
  2. **core** (``linkcom_core_open_link_pkt``): resolves the process start time
     via ``linkcom_get_pid_start_time`` (used later for owner verification),
     acquires the device instance, validates ``chan_id < chan_num``, takes the
     channel mutex.
  3. **Channel binding**: if the channel is idle (``FUNC_IDLE_STS``) or the
     previous owner process has exited (``linkcom_is_process_exit``), binds the
     current ``pid`` + ``start_time``, sets ``occupy_status =
     FUNC_OCCUPIED_STS``, and returns ``handle = MASK_LINKCOM_HANDLE(dev_id,
     chan_id)``. Otherwise returns ``-EINVAL``.
  4. **Cleanup**: releases mutex and device instance reference.

Handle Format
~~~~~~~~~~~~~

.. list-table::
   :widths: 15 15 70
   :header-rows: 1

   * - Field
     - Bit Range
     - Description
   * - vld
     - [31]
     - Validity flag (0 = valid, 1 = invalid)
   * - rsv
     - [30:24]
     - Reserved
   * - dev
     - [23:8]
     - Device id (16 bits)
   * - chan
     - [7:0]
     - Channel id (8 bits); device-level index computed as
       ``die_id * 4 + que_id`` (4 channels per die)

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct linkcom_open_chan_in``
    Input: dev_id, app_pid, chan_id, eventfd.

``struct linkcom_chan_inst``
    Per-channel instance: chan_id (device-level index: ``die_id * 4 + que_id``),
    die_id, que_id, app_pid, occupy_status, time (process start time), mutex,
    wait queue, rx list head, eventfd, delayed_work, work_run_flag.

API
~~~

.. code-block:: c

    int linkcom_ucd_open_link_pkt(u32 dev_id, void *in, u32 in_len,
                                  void *out, u32 out_len);
    int linkcom_core_open_link_pkt(u32 dev_id, u32 chan_id, u32 pid, int *handle);

    static int  linkcom_get_pid_start_time(u32 pid, TASK_TIME_TYPE *time);
    static bool linkcom_is_process_exit(u32 tgid, TASK_TIME_TYPE start_time);

Close Channel (UBLINKCOM_CLOSE)
-------------------------------

CLOSE releases a previously opened channel: recycles the eventfd context,
flushes the delayed-work, clears the rx list, and resets the channel to idle.

Timing
~~~~~~

.. code-block:: none

     User Space          handle_ucd              core                 mng
         |                   |                    |                    |
         |--ioctl(CLOSE)---->|                    |                    |
         |                   |--validate in       |                    |
         |                   |--extract chan_id   |                    |
         |                   |   from handle      |                    |
         |                   |--core_close------->|                    |
         |                   |                    |--get_pid_start_time|
         |                   |                    |--get_dev_inst----->|
         |                   |                    |--check chan_id     |
         |                   |                    |--mutex_lock        |
         |                   |                    |--verify owner      |
         |                   |                    |  (pid + time)      |
         |                   |                    |--recycle eventfd   |
         |                   |                    |--clear pid+time    |
         |                   |                    |--mutex_unlock      |
         |                   |                    |--release_lst_rsrc: |
         |                   |                    |    flush work      |
         |                   |                    |    del all nodes   |
         |                   |                    |    occupy=IDLE---->|
         |                   |                    |--put_dev_inst----->|
         |<--ret-------------|<--ret--------------|                    |

Call Flow
~~~~~~~~~

  1. **handle_ucd** (``linkcom_ucd_close_link_pkt``): validates input,
     extracts ``chan_id`` from the handle via
     ``UNMASK_LINKCOM_HANDLE_TO_CHANNEL``.
  2. **core** (``linkcom_core_close_link_pkt``): resolves process start time,
     acquires device instance, takes channel mutex, verifies owner (``app_pid``
     and ``time`` must match).
  3. **Eventfd recycle**: calls ``linkcom_core_chan_eventctx_recycle`` to
     release the eventfd context if bonded.
  4. **Owner clear**: clears ``app_pid`` and ``time``, releases mutex.
  5. **Resource release** (``linkcom_core_release_lst_rsrc``): flushes the
     delayed-work (``linkcom_flush_recv_pkt_work``), deletes all rx list nodes
     (``linkcom_del_all_list_node``), sets ``occupy_status = FUNC_IDLE_STS``.

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct linkcom_close_chan_in``
    Input: handle, app_pid.

API
~~~

.. code-block:: c

    int linkcom_ucd_close_link_pkt(u32 dev_id, void *in, u32 in_len,
                                   void *out, u32 out_len);
    int linkcom_core_close_link_pkt(u32 dev_id, u32 chan_id, u32 pid);

    void linkcom_core_chan_eventctx_recycle(struct linkcom_chan_inst *chan_inst);
    void linkcom_core_release_lst_rsrc(struct linkcom_chan_inst *chan_inst);

Send Link Packet (UBLINKCOM_SEND)
---------------------------------

SEND transmits a UB link packet on an opened channel. The driver validates the
input, copies the packet from user space, and submits it to hardware for
transmission. The call is synchronous and returns once the hardware has
processed the packet or an error is detected.

Timing
~~~~~~

.. code-block:: none

     User    handle_ucd        core
       |         |               |
       |-SEND--->|               |
       |         |--validate in  |
       |         |--copy_from_user(pkt)
       |         |--extract chan_id
       |         |--core_send--->|
       |         |               |--get_dev_inst
       |         |               |--check send ability
       |         |               |--mutex_lock
       |         |               |--verify owner
       |         |               |--inband_send (hardware)
       |         |               |--mutex_unlock
       |         |               |--put_dev_inst
       |         |<--ret---------|
       |         |--kfree(pkt)   |
       |<--ret---|               |

Call Flow
~~~~~~~~~

  1. **handle_ucd** (``linkcom_ucd_send_link_pkt``): validates input, validates
     ``pkt`` non-NULL and ``pktLen`` in ``(0, LINK_PKT_BUFF_LEN_MAX]``. Copies
     the packet from user space.
  2. **core** (``linkcom_core_send_link_pkt``): acquires device instance,
     checks channel send ability, takes channel mutex, verifies owner.
  3. **Hardware transmit**: submits the packet to hardware and polls for
     completion. Returns ``-ENETUNREACH`` if a drop is detected,
     ``-ETIMEDOUT`` on timeout (30 ms).
  4. **Cleanup**: releases mutex, device instance, and frees the packet buffer.

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct linkcom_send_chan_in``
    Input: handle, ``mami_pkt_send_para`` (lgPortId, cfgType, vl, pkt, pktLen).

API
~~~

.. code-block:: c

    int linkcom_ucd_send_link_pkt(u32 dev_id, void *in, u32 in_len,
                                  void *out, u32 out_len);
    int linkcom_core_send_link_pkt(u32 dev_id, u32 chan_id,
                                   struct mami_pkt_send_para para);

Receive Link Packet (UBLINKCOM_RECV)
------------------------------------

RECV retrieves a received link packet from an opened channel. It blocks on the
channel wait queue until a packet is available (fed by the interrupt or
delayed-work path), then pops a node from the rx list, masks the packet
payload, and copies the result to user space.

Timing
~~~~~~

The RECV function involves two cooperating paths: the **interrupt/delayed-work
path** that reads hardware packets into the rx list, and the **user wait path**
that blocks until a packet is available.

.. code-block:: none

     === Path A: Interrupt feeds rx list (background) ===

     HW ISR        UID           mgmtq_irq         core
       |            |                |               |
       |--IRQ------>|                |               |
       |            |--pre_handle--->|               |
       |            |                |--set pending  |
       |            |                |  bitmap       |
       |            |--bh(handle)--->|               |
       |            |                |--irq_proc:    |
       |            |                | for each pend |
       |            |                |  chan:        |
       |            |                |--read_rpkt--->|
       |            |                |               |--read hw pkt
       |            |                |               |  into rx list
       |            |                |               |--event_signal
       |            |                |               |--wake_up(wq)
       |            |                |--clear/unmask|
       |            |                |  intr         |

     === Path B: User receives (blocking wait) ===

     User    handle_ucd          core                  mng
       |         |                 |                    |
       |-RECV--->|                 |                    |
       |         |--validate in    |                    |
       |         |--extract        |                    |
       |         |  chan_id        |                    |
       |         |--kzalloc buf    |                    |
       |         |--try_module_get                      |
       |         |--core_recv--->  |                    |
       |         |                 |--check timeout     |
       |         |                 |--get_dev_inst----->|
       |         |                 |--verify owner      |
       |         |                 |--inband_recv:      |
       |         |                 |  wait_for_pkt(0):  |
       |         |                 |    non-block pop   |
       |         |                 |    if got -> return|
       |         |                 |  if -EAGAIN:       |
       |         |                 |  wait_for_pkt(to): |
       |         |                 |    wait_event(wq)  |
       |         |                 |   [woken by Path A]|
       |         |                 |    pause work      |
       |         |                 |    lock+verify     |
       |         |                 |    pop_list_node   |
       |         |                 |    idm lgPort map  |
       |         |                 |--put_dev_inst----->|
       |         |--module_put     |                    |
       |         |--pkt_mask       |                    |
       |         |  (parse headers,|                    |
       |         |   zero payload) |                    |
       |         |--copy_to_user   |                    |
       |         |--kfree buf      |                    |
       |<--ret---|                 |                    |

Call Flow -- Interrupt Path (Path A)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  1. **ISR / UID**: hardware interrupt -> UID dispatches to registered handlers.
  2. **Top-half pre-handle** (``mgmtq_intr_pre_handle``): sets the
     pending-channels bitmap for the die's 4 channels.
  3. **Bottom-half handle** (``mgmtq_intr_handle``): iterates pending channels
     in round-robin order. For each occupied channel with interrupt status set:
  4. **core** (``linkcom_core_read_rpkt``): takes channel mutex, reads
     available packets from hardware into the per-channel rx list via the SoC
     layer.
  5. **Post-read**: signals eventfd (``linkcom_event_signal``), wakes the wait
     queue (``wake_up``), clears and unmasks the interrupt.

Call Flow -- User Wait Path (Path B)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  1. **handle_ucd** (``linkcom_ucd_recv_link_pkt``): validates input, extracts
     ``chan_id``, validates ``buf_len`` in ``(0, LINK_PKT_BUFF_LEN_MAX]``,
     allocates a kernel receive buffer, takes a module reference
     (``try_module_get``).
  2. **core** (``linkcom_core_recv_link_pkt``): validates timeout in
     ``[-1, 30000]`` ms, acquires device instance, verifies channel owner.
  3. **First attempt** (``linkcom_core_inband_recv_link_pkt``):
     ``linkcom_wait_for_pkt`` with ``timeout = 0`` -- non-blocking pop from the
     rx list. If a packet is available, returns immediately.
  4. **Blocking wait**: if ``-EAGAIN``, calls ``linkcom_wait_for_pkt`` with the
     requested timeout (``-1`` = infinite). Blocks on
     ``wait_event_interruptible_timeout(wq->head, wq->flag, ...)``. When woken
     (by Path A's ``wake_up``), pauses the delayed-work, takes the channel
     lock, verifies owner, pops a node from the rx list, and translates
     ``nl_id``/``port_id`` to ``lgPortId``.
  5. **Post-processing** (``linkcom_ucd_recv_link_pkt``): calls
     ``linkcom_recv_pkt_mask`` to parse UB headers and mask payload (see
     Packet Masking below), copies the result to user space, fills the output
     struct (reason, port_index, recv_len), releases the module reference and
     frees the buffer.

Packet Masking
~~~~~~~~~~~~~~

Before returning a received packet to user space, ``linkcom_recv_pkt_mask``
parses the UB protocol header chain and zeroes out the payload for certain
report reasons:

  - ``LINKCOM_PROTOCOL_CHECK_FAIL``
  - ``LINKCOM_ROUTE_MISS``
  - ``LINKCOM_NPI_CHECK_FAIL``
  - ``LINKCOM_MIRROR_TO_CTRLCPU``

The parser walks: LH -> CFG3/4/6/7/9 NTH -> TPH/STPH/CTPH -> TAH/CIP/EID, and
zeroes everything after the relevant transport header so only diagnostic
header fields are preserved.

UB Packet Configuration Types:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - CFG Type
     - Description
   * - ``LINKCOM_UB_CFG_0``
     - Data link layer control packet
   * - ``LINKCOM_UB_CFG_3``
     - IPv4 packet (cfg3_nth -> IPv4/UDP/TPH)
   * - ``LINKCOM_UB_CFG_4``
     - IPv6 packet (cfg4_nth -> IPv6/UDP/TPH)
   * - ``LINKCOM_UB_CFG_6``
     - Inband control packet (cfg6_nth -> CTPH/UPI)
   * - ``LINKCOM_UB_CFG_7``
     - UB Domain network packet (cfg7_nth -> CTPH/TPH/UPI)
   * - ``LINKCOM_UB_CFG_9``
     - UB Memory packet (cfg9_nth -> mem TAH)

Report Reasons:

.. list-table::
   :widths: 35 65
   :header-rows: 1

   * - Reason
     - Description
   * - ``LINKCOM_CAM_HIT``
     - CAM hit
   * - ``LINKCOM_PROTOCOL_CHECK_FAIL``
     - Protocol check failure (e.g. TTL)
   * - ``LINKCOM_ROUTE_MISS``
     - Route miss
   * - ``LINKCOM_NPI_CHECK_FAIL``
     - NPI check failure
   * - ``LINKCOM_ENUM_OR_HIT_LOCAL_MSG``
     - Enumeration or local message hit
   * - ``LINKCOM_MIRROR_TO_CTRLCPU``
     - Mirrored packet to control CPU
   * - ``LINKCOM_NCSI``
     - NCSI packet

Key Data Structures
~~~~~~~~~~~~~~~~~~~

``struct linkcom_recv_chan_in``
    Input: handle, timeout (ms), pkt_buff, buf_len.

``struct linkcom_recv_chan_out``
    Output: reason, port_index, enum_vld, msg_code, recv_len.

API
~~~

.. code-block:: c

    int linkcom_ucd_recv_link_pkt(u32 dev_id, void *in, u32 in_len,
                                  void *out, u32 out_len);
    int linkcom_core_recv_link_pkt(u32 dev_id, u32 chan_id, int timeout,
                                   struct mami_pkt_recv_para *para);

    void linkcom_recv_pkt_mask(struct mami_pkt_recv_para *para);

Eventfd Notification (UBLINKCOM_EVENTFD)
----------------------------------------

EVENTFD bonds an eventfd file descriptor to an opened channel. When packets
arrive (via the interrupt path), the eventfd is signaled, allowing user space
to use ``poll``/``epoll`` for asynchronous notification instead of blocking in
RECV.

Timing
~~~~~~

.. code-block:: none

     User Space          handle_ucd               core
         |                   |                     |
         |--ioctl(EVENTFD)-->|                     |
         |                   |--validate in        |
         |                   |--get_dev_inst       |
         |                   |--check chan_id      |
         |                   |--mutex_lock         |
         |                   |--verify owner       |
         |                   |  (pid + time)       |
         |                   |--recycle old ctx--- |
         |                   |  (if leaked)        |
         |                   |--bond_eventfd_inner:|
         |                   |  if eventfd >= 0:   |
         |                   |    eventfd_ctx_fdget|
         |                   |    store ctx        |
         |                   |    if pkts pending: |
         |                   |      event_signal   |
         |                   |      (force wakeup) |
         |                   |  if eventfd == -1:  |
         |                   |    clear eventfd    |
         |                   |  else: -EINVAL      |
         |                   |--mutex_unlock       |
         |                   |--put_dev_inst       |
         |<--ret-------------|                     |

Call Flow
~~~~~~~~~

  1. **handle_ucd** (``linkcom_ucd_bond_eventfd``): validates input, acquires
     device instance, validates ``chan_id``, takes channel mutex, verifies
     owner (``pid`` + ``time``).
  2. **Leak recovery**: calls ``linkcom_core_chan_eventctx_recycle`` to
     release any previously bonded eventfd context (detects resource abnormal
     leakage).
  3. **Bond** (``linkcom_core_bond_eventfd_inner``):

     - If ``eventfd >= 0``: calls ``eventfd_ctx_fdget`` to obtain the kernel
       context, stores it in ``chan_inst->eventctx``. If packets are already
       pending in the rx list (``head.count > 0``), issues a forced wakeup via
       ``linkcom_event_signal`` so the user is immediately notified.
     - If ``eventfd == -1``: clears the eventfd (no-eventfd mode).
     - Otherwise: returns ``-EINVAL``.
  4. **Cleanup**: releases mutex and device instance.

Eventfd Signaling
~~~~~~~~~~~~~~~~~

When the interrupt path reads packets into the rx list, it calls
``linkcom_event_signal`` which signals the eventfd once per packet (or once
with the count on kernels before 6.7.12). This allows user space to wake from
``poll``/``epoll`` and issue RECV to retrieve the packets.

API
~~~

.. code-block:: c

    int linkcom_ucd_bond_eventfd(u32 dev_id, void *in, u32 in_len,
                                 void *out, u32 out_len);
    int linkcom_core_bond_eventfd_inner(u32 dev_id, struct linkcom_open_chan_in *kin,
                                        struct linkcom_chan_inst *chan_inst);

    void linkcom_event_signal(struct linkcom_chan_inst *chan_inst, u32 pkt_num);
    void linkcom_core_chan_eventctx_recycle(struct linkcom_chan_inst *chan_inst);

Proc Debug Interface
--------------------

The proc sub-module provides debugging and status information under
``/proc/mami/mgmtq/``.

Proc Entries
~~~~~~~~~~~~

.. list-table::
   :widths: 40 10 50
   :header-rows: 1

   * - Path
     - Mode
     - Description
   * - ``/proc/mami/mgmtq/mgmtq``
     - 0400
     - Show mgmtq WQE/CQE/RPKT CI/PI state for all devices, dies, and
       channels.
   * - ``/proc/mami/mgmtq/outband_enable``
     - 0400
     - Show (read) or set (write ``0``/``1``) the out-of-band enable flag.
       When enabled, local devices skip the inband interrupt status check.

mgmtq Show Output
~~~~~~~~~~~~~~~~~

The ``mgmtq`` entry iterates over all device ids and, for each device, over
all dies and channels, printing:

  - Device id, die id, channel id, instance queue id, CPU queue id, and the
    current rx list count.
  - WQE state: ``wqe_ci``, ``wqe_pi``, ``buf_ci``, ``buf_pi``.
  - CQE state: ``rcqe_ci``, ``rcqe_pi``, ``rpkt_ci``, ``rpkt_pi``.

Outband Enable
~~~~~~~~~~~~~~

The ``outband_enable`` flag controls whether local (``UDM_NEAR``) devices skip
the inband interrupt status check. When enabled (``1``), the interrupt handler
always treats the channel as having a pending interrupt, relying on
out-of-band notification instead.

API
~~~

.. code-block:: c

    int  mgmtq_proc_init(void);
    void mgmtq_proc_uninit(void);

    int     mgmtq_proc_open(struct inode *inode, struct file *file);
    int     mgmtq_outband_enable_open(struct inode *inode, struct file *file);
    ssize_t mgmtq_outband_enable_write(struct file *filp, const char __user *ubuf,
                                       size_t count, loff_t *ppos);

    bool linkcom_get_outband_enable(void);
    void linkcom_set_outband_enable(bool enable);


Loading and Unloading
=====================

::

    # Load
    insmod mami_linkcom.ko

    # Unload
    rmmod mami_linkcom

On load, the ``feature_loader`` framework invokes ``linkcom_init()`` at
``FEATURE_LOADER_STAGE_6``, which creates the proc entries, initializes the
mgmtq hardware state, registers the UDM notifier, registers all UCD
sub-commands, and registers the mgmtq interrupt type. Channel instances are
not created at load time; they are instantiated dynamically when devices are
added via the UDM notifier chain (typically triggered by the ``ubdevm``
platform driver on device probe).

On unload, ``linkcom_uninit()`` reverses the sequence: unregisters the
interrupt type, UCD commands, UDM notifier, and destroys the mgmtq state and
proc entries.


Security Considerations
=======================

* Character device nodes are created by the UBFABRIC UDM sub-module with
  permissions ``0660`` and owned by a dedicated ``mamiUser`` (uid/gid 1201),
  restricting access to management processes.
* Each UCD command has per-command access control via ``proc_ctrl_bitmap``.
  ``UBLINKCOM_OPEN`` and ``UBLINKCOM_CLOSE`` require ``PROC_CTRL_MANAGED``
  (management process only); ``UBLINKCOM_SEND``, ``UBLINKCOM_RECV``, and
  ``UBLINKCOM_EVENTFD`` require ``PROC_CTRL_ANY_USER`` (any process with
  device file permission).
* All user-space pointers are validated via ``copy_from_user`` /
  ``copy_to_user``. Input/output buffer sizes are bounded by the UCD limits
  (``UCD_MAX_INPUT_LEN`` 4 KiB, ``UCD_MAX_OUTPUT_LEN`` 64 KiB).
* Channel ownership is enforced: SEND, RECV, EVENTFD, and CLOSE verify that
  the caller's ``pid`` and process ``start_time`` match the values bound
  during OPEN, preventing PID-reuse attacks.


File Layout
===========

::

    drivers/ub/ubfm/linkcom/
    +-- linkcom_module.c              # Module entry (module_init/exit)
    +-- linkcom_init.c                # Init sequence, UCD/UDM/UID registration
    +-- linkcom_pub.h                 # Core data structures (dev/chan/wqe/cqe)
    +-- linkcom_log.h                 # Logging macros
    +-- linkcom_handle_ucd.c          # UCD command handlers (5 functions)
    +-- linkcom_handle_ucd.h
    +-- linkcom_pkt_parse.c           # UB protocol header parsing & masking
    +-- linkcom_pkt_parse.h
    +-- core/
    |   +-- linkcom_core.c            # Channel lifecycle, send/recv, eventfd
    |   +-- linkcom_core.h
    +-- mng/
    |   +-- linkcom_mng.c             # Device/channel instances, rx list
    |   +-- linkcom_mng.h
    +-- mgmtq/
    |   +-- linkcom_mgmtq_irq.c       # Interrupt handling, pending bitmap
    |   +-- linkcom_mgmtq_irq.h
    +-- soc/
        +-- linkcom_soc_adapt.c       # Version dispatch (function pointers)
        +-- linkcom_soc_adapt.h
        +-- linkcom_soc_reg.c         # Register address calculation
        +-- linkcom_soc_reg.h
        +-- mgmtq_proc.c              # /proc/mami/mgmtq/ entries
        +-- mgmtq_proc.h
        +-- udie_v1/
        |   +-- udie_v1_linkcom_soc.c # V1 send + recv implementation
        |   +-- udie_v1_linkcom_soc.h
        +-- udie_v2/
            +-- udie_v2_linkcom_soc.c # V2 recv implementation
            +-- udie_v2_linkcom_soc.h

    include/uapi/ub/ubfm/
    +-- linkcom.h                     # UAPI: handle macros, enums, ioctls
    +-- ubfabric.h                    # UAPI: UCD command enums, I/O structs


Support
=======

If there is any issue or question, please contact the vendor's support channel.
