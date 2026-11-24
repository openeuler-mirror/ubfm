.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


=====================================
XSHMEM Driver
=====================================

Overview
========
XSHMEM driver provides a shared memory framework for
UB (UnifiedBus) based systems. It enables memory sharing between the host and
guest (or other tasks) by managing shared memory pools and blocks, and exposes
a user-space interface through a misc device for allocating, obtaining, and
releasing shared memory blocks.

The driver is located under ``drivers/ub/ubfm/xshmem`` and is built as
the ``drv_xshmem`` module, controlled by the ``CONFIG_UB_UBFM`` option.

Overall Structure
=================

The XSHMEM driver is organized into a core framework plus two pluggable
allocation algorithms. The framework provides POOL/BLOCK management and the
user-space ioctl interface, while each algorithm implements the actual
address allocation strategy within a POOL.

.. code-block:: none

        +---------------------------------------------------+
        |                 User-space Task                   |
        |            (open /dev/xshmem_dev)                 |
        +---------------------------------------------------+
                             | ioctl
                             v
        +---------------------------------------------------+
        |              xshmem_framework                     |
        |  (misc device, POOL/BLOCK mgmt, procfs, algo reg) |
        +---------------------------------------------------+
                             | algo ops
                             v               v
                    +-----------+   +-----------+
                    |  fsc_algo |   | blk_algo  |
                    +-----------+   +-----------+
                             |               |
                             v               v
        +---------------------------------------------------+
        |              Shared Memory Resource               |
        +---------------------------------------------------+

The figure above illustrates the composition of the XSHMEM driver:

  - **xshmem_framework**: the core module. It registers a misc device
    ``/dev/xshmem_dev``, manages POOLs and BLOCKs, and provides the ioctl
    interface to user space. It also maintains a registry of allocation
    algorithms and exposes procfs debug information.
  - **fsc_algo**: the Free Size Class allocation algorithm. It splits a POOL
    into variable-size blocks on demand and merges adjacent free blocks.
  - **blk_algo**: the fixed-size Block allocation algorithm. It divides a POOL
    into equal-size blocks and tracks free blocks with a bitmap.

Core Concepts
=============

POOL
----
A POOL is a shared memory region identified by a user-provided string ``key``
and an allocation algorithm. Multiple tasks can register to the same POOL.
Each POOL is assigned a unique ``pool_id`` (allocated via ``idr``) which is
returned to user space and used in subsequent operations.

The maximum POOL size is ``XSHMEM_MAX_POOL_SIZE`` (2GB).

BLOCK
-----
A BLOCK is a chunk of memory allocated from a POOL. A BLOCK is identified by
its ``offset`` within the POOL. Multiple tasks attached to the same POOL can
get (share) the same BLOCK; the BLOCK is destroyed only when its reference
count drops to zero.

TASK
----
A TASK is a per-file-handle context created on ``open(/dev/xshmem_dev)``. It
records the PID and tracks all POOLs the task has registered to, as well as
the BLOCKs it has allocated or referenced. All resources owned by a TASK are
released when the file descriptor is closed.

Algorithm
---------
An algorithm implements the ``struct xshm_pool_algo`` interface, providing
hooks for POOL initialization/free, BLOCK allocation/free, POOL comparison and
``xshm_pool_show`` debug show.

User-space Interface
====================

The driver exposes a misc device ``/dev/xshmem_dev``. User-space programs
interact with it through ioctl commands defined in
``drivers/ub/ubfm/xshmem/include/xshmem_framework.h``.

IOCTL Commands
--------------

XSHMEM_POOL_REGISTER
~~~~~~~~~~~~~~~~~~~~
Register a POOL with the given key and algorithm. If a POOL with the same key
already exists and the algorithm parameters match, the task attaches to the
existing POOL; otherwise a new POOL is created. Returns the ``pool_id`` on
success.

Argument: ``struct xshm_reg_arg``

.. kernel-doc:: drivers/ub/ubfm/xshmem/include/xshmem_framework.h
   :functions: xshm_reg_arg

XSHMEM_POOL_UNREGISTER
~~~~~~~~~~~~~~~~~~~~~~
Detach the task from the POOL specified by ``pool_id`` (passed as the ioctl
argument). When the last task detaches, the POOL is destroyed.

XSHMEM_BLOCK_ALLOC
~~~~~~~~~~~~~~~~~~
Allocate a BLOCK of the requested size from the POOL identified by
``pool_id``. On success the ``offset`` field is filled with the BLOCK offset
within the POOL.

Argument: ``struct xshm_block_arg``

.. kernel-doc:: drivers/ub/ubfm/xshmem/include/xshmem_framework.h
   :functions: xshm_block_arg

XSHMEM_BLOCK_GET
~~~~~~~~~~~~~~~~
Get (reference) an existing BLOCK identified by ``pool_id`` and ``offset``.
Increments the reference count of the BLOCK for the calling task.

XSHMEM_BLOCK_PUT
~~~~~~~~~~~~~~~~
Put (dereference) a BLOCK identified by ``pool_id`` and ``offset``. Decrements
the reference count. When the total reference count reaches zero the BLOCK is
freed back to the POOL.

Supported Algorithms
====================

FSC Algorithm (fsc_algo)
------------------------
The Free Size Class algorithm implements variable-size block allocation using
a size-class based free list. Key characteristics:

  - POOL size must be aligned to ``XSHMEM_FSC_ALIGN`` (16 bytes) and cannot
    exceed ``XSHMEM_MAX_POOL_SIZE``.
  - Allocation sizes are classified into types by the position of the highest
    set bit (``fls``), enabling fast free-block lookup via a bitmap.
  - When a free block is larger than requested, it is split; the remaining
    space forms a new free block.
  - On free, adjacent free blocks (previous and next) are merged to reduce
    fragmentation.
  - The maximum single allocation size is ``XSHMEM_FSC_MAX_ALLOC_SIZE``
    (2GB).

Block Algorithm (blk_algo)
--------------------------
The Block algorithm implements fixed-size block allocation. Key
characteristics:

  - Both ``pool_size`` and ``block_size`` must be aligned to
    ``XSHMEM_BLK_ALIGN`` (16 bytes).
  - The POOL is divided into ``pool_size / block_size`` equal-size blocks.
  - Free blocks are tracked with a bitmap; allocation finds the next free
    bit using ``find_next_zero_bit``.
  - The allocation size must not exceed ``block_size``; each allocation
    consumes exactly one fixed-size block.
  - The maximum block size is ``XSHMEM_MAX_BLOCK_SIZE`` (2GB).

Procfs Interface
================

The driver creates a ``/proc/xshmem`` directory with the following read-only
entries for debugging:

task_info
---------
Lists each task (by PID) and the POOLs it has registered to, including the
allocated size and real allocated size per POOL.

  Example::

    $ cat /proc/xshmem/task_info

pool_info
---------
Lists all POOLs in the system, showing the pool_id, key, reference count,
algorithm name, and algorithm-specific details (free size for fsc, free block
count for blk).

  Example::

    $ cat /proc/xshmem/pool_info

Configuration
=============

The XSHMEM driver is enabled through the kernel config option
``CONFIG_UB_UBFM``. It depends on ``CONFIG_UB`` and can be built as a module
(``drv_xshmem.ko``) or built-in.

To enable it::

    CONFIG_UB=y
    CONFIG_UB_UBFM=m

Module parameters
-----------------
The driver currently exposes no module parameters.

File Layout
===========

The driver source files are organized as follows:

============================  =============================================================
File                           Description
============================  =============================================================
``xshmem_framework.c``         Core framework: misc device, POOL/BLOCK management,
                               ioctl handler, procfs, algorithm registry
``xshmem_framework.h``         Internal header with POOL/BLOCK/algo structures
``xshmem_fsc.c``               FSC (Free Size Class) allocation algorithm
``xshmem_blk.c``               Fixed-size Block allocation algorithm
``include/xshmem_framework.h``  User-space ABI: ioctl definitions and
                                    argument structures
============================  =============================================================

Support
=======
If there is any issue or question, please contact the vendor's support channel.
