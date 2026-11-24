.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


=====================
AUTH DRIVER (AUTH)
=====================

The ``drv_auth`` kernel driver is part of the Huawei IAM (Identity and Access
Management) subsystem. It is a Linux kernel misc device driver that exposes the
character device ``/dev/auth_dev``.

The driver itself does not implement any authentication business logic. Instead,
it acts as a broker for identification between ordinary user-space processes and
the ``iammgr`` daemon (the whitelist manager). This document describes the
composition, interfaces, data structures, and design of the AUTH driver.

Overview
========

The AUTH driver mediates authentication between requesters and the ``iammgr``
daemon as follows:

- An ordinary process issues an "obtain whitelist name" request to the driver
  through an ioctl;
- The driver enqueues the request into a kernel waiting queue and notifies
  ``iammgr`` through the ``poll`` mechanism;
- ``iammgr`` authenticates the requester by its PID according to the whitelist
  it maintains (matching the binary path against the whitelist);
- ``iammgr`` fills the custom name from the whitelist back to the driver through
  an ioctl, and the driver wakes up the waiting requester;
- A process that hits the whitelist is brought under the management of
  ``iammgr`` and enjoys higher resource-call permissions; otherwise an empty
  name is returned and the requester regards itself as unauthorized.

.. code-block:: none

        +-----------+                     +---------------------+
        |  normal   | --- ioctl(GET_PROCNAME) --> |  drv_auth.ko      |
        |  process  |                           |  /dev/auth_dev     |
        +-----------+                           +---------------------+
              ^                                       ^         |
              |   wake up + copy proc_name            | enqueue | poll readable
              +---------------------------------------+         v
                                       +------------+   +---------------------+
                                       |  iammgr    |<--| auth_task_list etc. |
                                       |  daemon    |   +---------------------+
                                       +------------+
                                              |
                                              v
                                       +------------+
                                       |  whitelist |
                                       +------------+

The driver is a pure C kernel module built upon the ``miscdevice`` framework. On
load it registers the misc device ``auth_dev`` (major number 10, dynamic minor
number). On unload it deregisters the device and releases all queued nodes.

The following table summarizes the source files of the AUTH driver:

=============  ============================================  =====
File           Role                                          Lines
=============  ============================================  =====
auth_module.c  driver main implementation                   522
auth_module.h  internal data-structure definitions           35
auth_ioctl.h   user/kernel shared ioctl protocol (UAPI)      22
=============  ============================================  =====

Module parameters
=================

The AUTH driver does not export any module parameter.

Supported ioctl commands
========================

The driver registers the misc device ``auth_dev`` and user space invokes
``ioctl(cmd, arg)`` on ``/dev/auth_dev``, where ``arg`` always points to a
``struct proc_info``.

The shared user/kernel ioctl protocol is defined in
``drivers/ub/ubfm/auth/include/auth_ioctl.h``:

===  ===========================  =====  ====  =================================
Cmd  Macro                        Dir    Nr    Description
===  ===========================  =====  ====  =================================
1    ``AUTH_IOCTL_GET_PROCNAME``  IOWR   'A'   Requester obtains its name
                                              (blocking)
2    ``AUTH_IOCTL_GET_PID``       IOR    'A'   iammgr fetches a pending PID
3    ``AUTH_IOCTL_SET_PROCNAME``  IOW    'A'   iammgr fills the whitelist name
                                              and wakes up the waiter
===  ===========================  =====  ====  =================================

Shared data structure
=====================

::

    #define AUTH_PROCNAME_SIZE 256U

    struct proc_info {
        pid_t pid;                              /* process TGID */
        char  proc_name[AUTH_PROCNAME_SIZE + 1];/* whitelist name (\0-terminated) */
        int   timeout;                          /* only used by GET_PROCNAME */
    };

The field semantics vary with the command:

=============  ===========================  =================  ================  =====================
Command        input ``pid``               input proc_name    input ``timeout``  output ``proc_name``
=============  ===========================  =================  ================  =====================
GET_PROCNAME   ``current->tgid``            ignored            timeout (ms)      whitelist name
               (user value ignored)
GET_PID        ignored                      ignored            ignored           only ``pid`` valid
SET_PROCNAME   required (target PID)        required (name)    ignored           -
=============  ===========================  =================  ================  =====================

Detail of the ioctl interfaces
===============================

AUTH_IOCTL_GET_PROCNAME (requester side)
----------------------------------------

The calling process applies to be brought under the whitelist management and
blocks until ``iammgr`` fills the name back.

``timeout`` semantics:

=====  ==============================================================
value  behavior
=====  ==============================================================
>= 0    Block, but waiting for at least 100 ms (values below 100 are
       raised to 100); returns ``-ETIMEDOUT`` on timeout
== -1  Wait forever (``AUTH_EVENT_NO_TIMEOUT_FLAG``), no timeout
other  Returns ``-EINVAL``
=====  ==============================================================

Frequency control: a single TGID is limited to at most 200 calls within a
1-second sliding window; exceeding the limit returns ``-EBUSY``.

Multithreading: when multiple threads of the same process call concurrently,
each thread creates and enqueues its own node in ``auth_add_node``. Then, in
``auth_get_procname``, nodes are bound per thread in list order by matching
``pid == tgid && !is_used`` and setting ``is_used = true``. ``is_used`` ensures
each thread binds to a distinct node (claimed in enqueue order), avoiding a
use-after-free where several threads obtain the same node and the first one
wakes up and is freed by ``auth_del_node``. Ultimately each thread waits on its
own node's wait queue, and ``iammgr`` wakes them one by one through successive
``GET_PID``/``SET_PROCNAME`` calls.

Return values:

============  ========================================================================
Code          Meaning
============  ========================================================================
0             Success, ``proc_name`` has been filled in
-EBUSY        Frequency control triggered (over 200 calls in 1 s for one TGID), or
              ``auth_timestamp_list`` is full (10 TGIDs) and no oldest record can
              be evicted (indirectly throttles the enqueue rate of
              ``auth_task_list``), or ``auth_task_list`` has reached
              ``AUTH_MAX_TASK_LIST_SIZE`` (the pending request queue is full,
              e.g. when ``iammgr`` is offline or cannot keep up with the enqueue
              rate)
-ETIMEDOUT    Waiting for ``iammgr`` to fill in timed out
-EFAULT       Node for this process not found / filled-in name is empty /
              ``copy_to_user`` failed
-ENOMEM       ``kmalloc`` of a node failed
-ERESTARTSYS  Waiting was interrupted by a signal (can be restarted)
-EINVAL       ``file`` or ``arg`` is NULL, or ``timeout`` is invalid
============  ========================================================================

AUTH_IOCTL_GET_PID (iammgr side)
--------------------------------

``iammgr`` calls this after being woken up by ``poll`` to fetch a node for which
the name has not yet been filled (``proc_name`` empty) and copies the ``pid``
back to user space.

The node is not consumed: it remains in the list until the later
``SET_PROCNAME``.

Return values:

=======  ==============================================================
Code     Meaning
=======  ==============================================================
0        Success, ``pid`` has been filled in
-EAGAIN  No pending request (all nodes already filled)
-EFAULT  ``copy_to_user`` failed
=======  ==============================================================

AUTH_IOCTL_SET_PROCNAME (iammgr side)
-------------------------------------

After matching the pid's binary path against the whitelist, ``iammgr`` fills the
custom name from the whitelist back to the driver; the driver sets ``status = 1``
and calls ``wake_up_interruptible`` to wake the waiter.

When the whitelist is not hit, ``iammgr`` must call this command to fill back an
empty string (``proc_name[0] == '\0'``). The waiter then regards itself as
unauthorized, and the driver deletes the node and returns ``-EFAULT``.

Return values:

====  ==============================================================
Code  Meaning
====  ==============================================================
0     Success, the waiter has been woken up
-EFAULT  ``copy_from_user`` failed / ``strscpy`` failed / pid not found
         in the list
====  ==============================================================

poll behavior
-------------

``iammgr`` listens for readable events by ``poll(/dev/auth_dev)``:

- When there is a node with ``status == 0`` (not yet filled) in
  ``auth_task_list``, it returns ``POLLIN | POLLRDNORM``;
- Otherwise it blocks on ``poll_wait_queue``, which is woken up by
  ``wake_up_interruptible(&poll_wait_queue)`` in ``ioctl_get_procname``.

iammgr usage constraint: currently iammgr processes ``GET_PID -> authenticate ->
SET_PROCNAME`` serially in a single thread; only after finishing one request does
it enter the next poll round. ``GET_PID`` does not change ``status``, so ``poll``
returns the same node repeatedly while iammgr is handling that pid — iammgr's own
"one pid at a time" semantics avoid duplicate filling. Multithreaded iammgr or
concurrent ``GET_PID`` is not supported by the current design.

Data structures
===============

The driver defines the following kernel data structures.

struct proc_info
----------------

The UAPI shared protocol (defined in ``auth_ioctl.h``) that carries data across
user/kernel space.

struct auth_task_node
---------------------

::

    struct auth_task_node {
        struct proc_info proc_info;
        atomic_t status;
        struct list_head list;
        wait_queue_head_t wait_queue;
        bool is_used; /* avoid reclaiming the same node concurrently */
    };

A node represents one complete "request-fill" session and is queued in
``auth_task_list``. Its lifetime spans from enqueue by ``GET_PROCNAME`` until it
is freed by ``auth_del_node`` after ``SET_PROCNAME`` or on timeout/failure.

``status``: ``0`` means not yet filled, ``1`` means filled (set by
``SET_PROCNAME``).

``is_used``: prevents ``GET_PROCNAME`` from acquiring the same node repeatedly in
a multithreaded scenario (see the multithreading note above).

struct auth_timestamp_node
--------------------------

::

    struct auth_timestamp_node {
        pid_t pid;
        struct list_head list;
        ktime_t timestamp_queue[AUTH_MAX_TIMESTAMP_QUEUE_SIZE];
        unsigned int queue_head;
        unsigned int queue_tail;
        unsigned int queue_size;
    };

A per-TGID frequency-control record that stores the most recent 200 timestamps
in a circular queue. It is created on the first invocation of a PID and is
evicted when the list is full and the oldest record is older than 1 second.

Key macros/constants
====================

==============================  =======  ================================================
Macro                           Value    Purpose
==============================  =======  ================================================
AUTH_MAX_TIMESTAMP_QUEUE_SIZE   200      per-PID sliding window limit (max calls in 1 s)
AUTH_MAX_PROCESS_LIST_SIZE      10       max number of PIDs in ``auth_timestamp_list``
AUTH_MAX_TASK_LIST_SIZE         2000     max number of pending nodes in ``auth_task_list``
AUTH_US_PER_S                   1000000  1 second in microseconds
AUTH_EVENT_NO_TIMEOUT_FLAG      -1       ``timeout`` wait-forever flag
AUTH_EVENT_WAIT_TIME_MIN        100      minimum wait of 100 ms
AUTH_PROCNAME_SIZE              256      ``proc_name`` buffer length (+1 for ``\0``)
==============================  =======  ================================================

Sequence flow
=============

Main flow (successful authentication)
-------------------------------------

.. code-block:: none

    Requester                drv_auth                        iammgr
    --------                --------                        ------
      |  ioctl(GET_PROCNAME)   |                               |
      |----------------------->|                               |
      |    auth_check_frequency(tgid, now)                     |
      |    auth_add_node(tgid)   (enqueue to auth_task_list)   |
      |    wake_up_interruptible(poll_wait_queue)              |
      |    auth_get_procname -> bind node (is_used=true)       |
      |    auth_wait_procname -> wait on node.wait_queue       |
      |  (blocked)             |                               |
      |                        |           poll -> POLLIN      |
      |                        |<-------------------------------|
      |                        |  ioctl(GET_PID)               |
      |                        |------------------------------->
      |                        |  return node.pid              |
      |                        |<-------------------------------|
      |                        |    match /proc/<pid>/exe      |
      |                        |    against whitelist          |
      |                        |  ioctl(SET_PROCNAME, name)    |
      |                        |<-------------------------------|
      |                        |  strscpy + status=1           |
      |                        |  wake_up_interruptible(node)  |
      |  (woken up)            |                               |
      |<-----------------------|  copy_to_user(proc_info)=0    |
      |  auth_del_node         |                               |
      |  got whitelist name    |                               |

Exception branch (whitelist miss)
---------------------------------

.. code-block:: none

    Requester                drv_auth                        iammgr
      |  ioctl(GET_PROCNAME)    |                               |
      |------------------------>|                               |
      |    enqueue + wake poll  |                               |
      |                         |  ioctl(GET_PID) -> pid        |
      |                         |<-------------------------------|
      |                         |  whitelist miss               |
      |                         |  ioctl(SET_PROCNAME, "", pid) |
      |                         |<-------------------------------|
      |                         |  status=1, wake_up            |
      |  blocked returns        |                               |
      |    detect proc_name[0]=='\0'                             |
      |  return -EFAULT, auth_del_node                           |

Exception branch (timeout)
--------------------------

.. code-block:: none

    Requester                drv_auth
      |  ioctl(GET_PROCNAME, timeout=500ms)  |
      |-------------------------------------->|
      |    enqueue + wake poll                |
      |    (iammgr fails to fill on time)     |
      |<-- wait_event returns 0 (timeout) ----|
      |  return -ETIMEDOUT, auth_del_node     |

Global state
============

=============  ===========================  ==================  ==================================
Name           Type                         Protection          Use
=============  ===========================  ==================  ==================================
auth_task_list  ``LIST_HEAD``               ``auth_task_mutex``  pending authentication request queue
auth_timestamp_list  ``LIST_HEAD``          ``auth_timestamp_mutex``  frequency-control record queue
poll_wait_queue  ``wait_queue_head_t``      -                   iammgr poll blocking queue
auth_dev       ``struct miscdevice``        -                   character device descriptor
=============  ===========================  ==================  ==================================

Concurrency and synchronization
===============================

====  =============================  ==========================================
Shared resource                    Synchronization  Critical section
                                    primitive
====  =============================  ==========================================
auth_task_list                     ``auth_task_mutex``  all insert/delete/lookup
                                                         (auth_add_node, auth_del_node,
                                                         auth_get_procname, ioctl_get_pid,
                                                         ioctl_set_procname, auth_poll, exit)
auth_timestamp_list                ``auth_timestamp_mutex``  auth_check_frequency,
                                                         remove_timestamp_node, exit
auth_task_node.status              ``atomic_t`` + wait queue  SET_PROCNAME writes 1; GET_PROCNAME
                                                         waits on the wait queue
auth_task_node.is_used             protected indirectly   multithreaded node binding mark
                                                         by ``auth_task_mutex``
====  =============================  ==========================================

Note: in ``auth_get_procname`` there is an "unlock then relock" window (the lock
is not held while waiting). This is intentional; otherwise ``iammgr``'s
``SET_PROCNAME`` would self-deadlock.

Note: ``ioctl_set_procname`` calls ``wake_up_interruptible(&node->wait_queue)``
while holding ``auth_task_mutex``. This is intentional: if the wake-up were moved
after ``mutex_unlock``, the woken-up ``auth_get_procname`` thread could run
``auth_del_node`` and free the node before ``SET_PROCNAME`` releases the lock,
leading to a NULL pointer access. The current implementation trades a brief
lock-holding wake-up for safety.

Usage
=====

Requester (ordinary process)
----------------------------

.. code-block:: c

    int fd = open("/dev/auth_dev", O_RDWR);
    struct proc_info info = { .timeout = 5000 };  /* 5 s timeout, or -1 forever */
    int ret = ioctl(fd, AUTH_IOCTL_GET_PROCNAME, &info);
    if (ret == 0 && info.proc_name[0] != '\0') {
        /* brought under iammgr management; info.proc_name is the whitelist name */
    } else {
        /* unauthorized or failed; no automatic recovery defined on the business side */
    }
    close(fd);

iammgr side
-----------

.. code-block:: c

    int fd = open("/dev/auth_dev", O_RDWR);
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    while (running) {
        poll(&pfd, 1, -1);                /* wait for a new request */
        struct proc_info info;
        if (ioctl(fd, AUTH_IOCTL_GET_PID, &info) != 0)
            continue;
        /* 1. read the binary path via /proc/<info.pid>/exe */
        /* 2. match the path against the whitelist */
        if (matched)
            snprintf(info.proc_name, sizeof(info.proc_name), "%s", white_name);
        else
            info.proc_name[0] = '\0';     /* miss: fill an empty string */
        ioctl(fd, AUTH_IOCTL_SET_PROCNAME, &info);
    }

Loading and unloading
---------------------

.. code-block:: bash

    insmod drv_auth.ko    # creates /dev/auth_dev (misc, dynamic minor number)
    rmmod drv_auth        # auto cleans both lists

Unload precondition: if any user-space thread is still blocked on
``wait_event_interruptible_timeout`` / ``wait_event_interruptible`` at unload,
``rmmod`` should not be executed (the kernel module reference count / held
user-space fds usually prevent unloading). Forcing an unload frees all queued
nodes, and waiting threads that wake up or return then access freed memory,
triggering a UAF/panic. On the O&M side, ensure all requesters and iammgr have
exited before unloading.

Design points and known constraints
===================================

1. Frequency-control threshold: 200 calls per second per TGID, with at most 10
   TGIDs recorded globally. The threshold of 200 is an explicit specification
   (empirical value).
2. Minimum wait of 100 ms: avoids frequent ``-ETIMEDOUT`` with very small
   timeouts.
3. Wait-forever support: ``timeout = -1`` handles the CPU high-load scenario;
   the requester should evaluate whether to enable it.
4. Multithreaded node binding: when multiple threads of the same process call
   ``GET_PROCNAME`` concurrently, each thread enqueues its own independent node
   in ``auth_add_node``; in ``auth_get_procname``, nodes are bound to different
   threads in list order via the ``is_used`` flag, avoiding a UAF where the first
   woken thread's ``auth_del_node`` frees a node still referenced by others.
   ``is_used`` does not affect ``poll``/``GET_PID``, which only look at
   ``status``.
5. No business-side recovery on failure: the business side currently does not
   retry after a non-zero return.
6. No persistence: all state lives in kernel memory and is lost on module unload.
7. Whitelist miss equals empty-string fill: ``iammgr`` must explicitly fill back
   an empty string to wake the waiter — it cannot skip filling, otherwise the
   waiter can only exit via timeout.
8. iammgr single-thread constraint: the current ``poll``/``GET_PID``/
   ``SET_PROCNAME`` protocol assumes iammgr processes serially in a single
   thread. ``GET_PID`` does not change ``status``, so the same node is returned
   repeatedly by ``poll`` until ``SET_PROCNAME``; iammgr's own "one pid at a
   time" semantics deduplicate. Multithreaded iammgr is not supported.
9. Unload precondition: before unloading, ensure all requesters and iammgr have
   exited with no in-flight blocked request threads, otherwise a UAF/panic may
   occur (see Loading and unloading above).
10. ``auth_task_list`` hard cap: the pending request queue is bounded by
    ``AUTH_MAX_TASK_LIST_SIZE`` (2000). When the queue is full, new
    ``GET_PROCNAME`` requests are rejected with ``-EBUSY`` without allocating
    or enqueueing a node. This prevents unbounded kernel heap consumption when
    ``iammgr`` is offline or cannot keep up with the enqueue rate (e.g. a
    local low-privilege attacker issuing ``timeout=-1`` requests would
    otherwise pin nodes in the list forever).

Support
=======

If there is any issue or question, please contact the vendor's support channel.
