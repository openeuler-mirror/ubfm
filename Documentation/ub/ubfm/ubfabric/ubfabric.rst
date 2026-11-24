.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


=================================
UBFABRIC INFRASTRUCTURE FRAMEWORK
=================================

UBFABRIC is the core fabric infrastructure framework of the MAMI kernel
subsystem. It provides a modular, layered architecture that enables user-space
applications to interact with MAMI devices through a unified command dispatch
mechanism. In addition, it manages device lifecycle, hardware resources,
memory/register mappings, events, interrupts, and logging.

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
URES                             UB Device Resource
UDM                              UB Device Management
UED                              UB Device Event Dispatch
UOP                              UB Device Operation
ULOG                             UB Device Log
UID                              UB Interrupt Dispatch
CDMA                             Crystal DMA
LDST                             Load/Store
IAM                              Identity and Access Management
PM                               Process Manager
================================ ================================================

Overview
========

UBFABRIC driver uses a ``feature_loader`` mechanism to auto-initialize submodules
in ordered stages. It creates a ``/proc/mami`` top-level directory and loads
submodules in the following order:

  - Stage 0: UCD
  - Stage 1: URES
  - Stage 2: UDM
  - Stage 3: UED + UID + ULOG
  - Stage 4: UOP + ULOG fops/proc_fs

.. code-block:: none

     User Space
         |
         |-- ioctl on /dev/manager or /dev/ubfabricX
         |
         v
     +----------------------------------------------------------+
     |                           UCD                            |
     |  Validates permissions, looks up feature in kv table,    |
     |  routes to registered handler                            |
     +----------------------------------------------------------+
         |          |          |          |          |
         v          v          v          v          v
     +-------+  +-------+  +-------+  +-------+  +-------+
     | URES  |  |  UOP  |  |  UED  |  | ULOG  |  |  ...  |
     +-------+  +-------+  +-------+  +-------+  +-------+
                    |
          +---------+---------+
          |         |         |
          v         v         v
     +--------+ +--------+ +--------+
     |  kmap  | |  umap  | |  uops  |
     +--------+ +--------+ +--------+

     +----------------------------------------------------------+
     |                           UDM                            |
     |  Device add/remove triggers notifier chain               |
     |  Notifiers (priority-ordered):                           |
     |    PRI1: UED, UOP   PRI5: UID                            |
     |    (other priorities registered by sub-module drivers)   |
     +----------------------------------------------------------+
          |
          v
     +-------+
     | URES  |  <-- Provides memory/register/key/attr data
     +-------+

     +----------------------------------------------------------+
     |           UID (Unified Interrupt Dispatcher)             |
     |  ISR -> bottom-half (tasklet or workqueue)               |
     +----------------------------------------------------------+

     +----------------------------------------------------------+
     |                          ULOG                            |
     |  Per-device log level management, log deduplication      |
     +----------------------------------------------------------+

Below is the summary for the submodules in ubfabric driver:

  - 1) **UCD** (user command dispatch): receives
       ioctl calls from user space, validates permissions, and routes commands to
       registered feature handlers.
  - 2) **URES** (resource manager): manages device resource
       information including memory regions, register spaces, key-value pairs,
       attributes, and window configurations.
  - 3) **UDM** (device manager): manages device lifecycle
       (add/remove), provides char device nodes, and implements a priority-based
       notifier chain.
  - 4) **UED** (event dispatcher): implements in-kernel event
       publishing/subscribing mechanism with ring buffers and thread-level scheduling.
  - 5) **UID** (interrupt dispatcher): provides interrupt
       handling framework with configurable bottom-half processing (tasklet vs
       workqueue).
  - 6) **ULOG** (logging module): manages kernel log levels (per-device and
       global), log deduplication, and ioctl commands for log configuration.
  - 7) **UOP** (operation manager): provides device operation
       management including memory/register mapping, access node management, and
       window configuration.

Device Nodes
============

UBFABRIC creates two types of char device nodes for user-space interaction:

========= ================= ==================================================
Node      Path               Description
========= ================= ==================================================
manager   /dev/manager       Global command interface (non-device-specific)
ubfabric  /dev/ubfabricX     Device-specific command interface (per-device)
========= ================= ==================================================

Proc Filesystem
===============

UBFABRIC provides debugging and status information through the proc filesystem
under ``/proc/mami/``:

========================================= ======================================
Path                                      Purpose
========================================= ======================================
/proc/mami/ucd/register                   Show all registered UCD feature commands
/proc/mami/ucd/statistic                  Show feature execution statistics
/proc/mami/udm/udevice                    Show all device instances with GUIDs
/proc/mami/udm/notifiers                  Show notifier registration and call counts
/proc/mami/udm/create_delay               Device creation delay control
/proc/mami/ued/dev-X(inst-Y)/             Per-device event info
/proc/mami/ures/ures                      Show all resource information
/proc/mami/ulog/log_level                 Show log level configuration
/proc/mami/uop/uop                        Show UOP device/item state
/proc/mami/uop/back_stats                 Show UOP back-pressure statistics
/proc/mami/uop/back_enable                Show UOP back-pressure enable
========================================= ======================================

Module Parameters / sysfs
=========================

The ubfabric driver does not register any ``module_param`` and therefore exposes
no entries under ``/sys/module/mami_ubfabric/parameters/``. Runtime
configuration and status are provided through the proc filesystem (see above)
and the UCD ioctl commands documented per module.

Functionality Dependencies
==========================

The submodule loading order reflects dependency relationships:

  - 1) UCD is the foundation: all other modules register commands through UCD.
  - 2) UDM drives lifecycle: device add/remove events propagate through the UDM
       notifier chain to UED, UOP, and UID.
  - 3) URES provides data: resource information from URES is consumed by kmap
       during device initialization.
  - 4) UOP depends on URES + UDM: gets acc_type from ures key-value, receives
       UDM_INIT notification, then initializes kmap and uops.
  - 5) UED + UID both listen to UDM: both register as UDM notifiers at PRI1
       and PRI5 respectively.
  - 6) ULOG provides infrastructure: used by all modules for logging.

*****************************
User Command Dispatcher (UCD)
*****************************

UCD is the core command dispatch framework of the
UBFABRIC infrastructure. It receives ioctl calls from user space, validates
permissions, and routes commands to registered feature handlers. UCD serves as
the central dispatch hub for all user-to-kernel communication in the MAMI
subsystem.

UCD manages two types of char devices:

  - ``/dev/manager``: for global (non-device-specific) commands
  - ``/dev/ubfabricX``: for device-specific commands

Each feature command is identified by a key composed of ``(main_cmd << 8) | sub_cmd``,
which is used for hash-based lookup in a 256-bucket hash table.

UCD provides:

  - Feature command registration and unregistration
  - Permission checking (user group, process control)
  - Module-level open/release callback management
  - DFX statistics collection (timing, success/failure count)
  - Proc filesystem for debugging

Architecture
============

.. code-block:: none

     User Space ioctl
         |
         v
     +--------------------------------------------------+
     |                     UCD Core                     |
     |  +-----------+  +----------+  +---------------+  |
     |  | File Ops  |  | KV Table |  | Access Ctrl   |  |
     |  | (ioctl)   |  | (lookup) |  | (permission)  |  |
     |  +-----------+  +----------+  +---------------+  |
     +--------------------------------------------------+
         |                |                |
         v                v                v
     +----------+  +-----------+  +--------------+
     | Feature  |  | Feature   |  | Feature      |
     | Handler1 |  | Handler2  |  | HandlerN     |
     | (URES)   |  | (UOP)     |  | (UED/ULOG)   |
     +----------+  +-----------+  +--------------+

Feature Registration
--------------------

UCD supports two types of feature registration:

  - ``ucd_register_cmd()``: Register a non-device-specific feature handler
    (accessed via ``/dev/manager``)
  - ``ucd_register_dev_cmd()``: Register a device-specific feature handler
    (accessed via ``/dev/ubfabricX``)

Each feature is described by ``struct tag_ucd_feature`` which contains the
main command, sub command, handler function, and access control profile.

Access Control
--------------

UCD implements a multi-layer access control mechanism:

  - **User group identification**: Distinguishes root, HwHiAiUser, and guest
    users with different privilege levels.
  - **Runtime environment**: Detects physical vs virtual machine via DMI.
  - **Feature profile**: Each feature has a profile defining which user groups
    and runtime environments can access it.
  - **Process control**: Bitmap-based permission check including:

====== ============================================
Flag   Description
====== ============================================
0x01   PROC_CTRL_ANY_USER - Any user allowed
0x02   PROC_CTRL_MANAGED - Managed process only
0x04   PROC_CTRL_PROCESS_MGR - Process manager
0x08   PROC_CTRL_PM_CHILD - PM child process
0x10   PROC_CTRL_IAM_MGR - IAM manager
====== ============================================

Key Data Structures
-------------------

``struct ucd_module_cb``
  Module callback structure with open/release/mmap function pointers.
  Registered per module type.

``struct tag_ucd_feature``
  Feature definition containing main_cmd, sub_cmd, handler function,
  access profile, and proc control whitelist.

``struct tag_feature_statistic``
  DFX statistics: used count, failed count, min/max/last execution time
  for each feature command.

``struct ucd_kv_table``
  Hash table with 256 buckets and rwlock for O(1) feature lookup by key.

``struct ucd_file_private_stru``
  File private data: devid, owner_pid, work_count, bound to each
  open file descriptor.

API Reference
=============

UCD Commands
------------

UCD itself does not register any UCD command; it is the dispatcher that hosts
the commands of all other modules. The generic ioctl call sequence handled by
UCD core is:

  1. User space calls ``ioctl()`` on ``/dev/manager`` (non-device command) or
     ``/dev/ubfabricX`` (device command).
  2. UCD ``file_operations`` enters ``ucd_feature_process()`` /
     ``ucd_dev_feature_process()``.
  3. For payloads larger than ``sizeof(u64)``, UCD allocates kernel
     input/output buffers and copies user data in.
  4. UCD access-control layer verifies the caller's user group, runtime
     environment (PM vs VM via DMI) and process-control bitmap.
  5. UCD looks up the handler in the 256-bucket kv hash table by
     ``(main_cmd << 8) | sub_cmd``.
  6. The matched feature handler is invoked with ``(devid, in, in_len, out,
     out_len)``.
  7. UCD copies the output buffer back to user space, updates DFX statistics
     (use count, fail count, min/max/last latency) and frees temporaries.
  8. On unregistration UCD drains in-flight callers (60 s timeout) before
     removing the kv entry, preventing use-after-free during module unload.

Feature Registration
--------------------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ucd/ucd_feature.c
   :functions: ucd_register_cmd ucd_unregister_cmd

Module Operations
-----------------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ucd/ucd_module.c
   :functions: ucd_register_module_ops ucd_register_module_dev_ops

Feature Dispatch
----------------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ucd/ucd_feature.c
   :functions: ucd_feature_process ucd_dev_feature_process

Device Open/Release Callbacks
-----------------------------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ucd/ucd_module.c
   :functions: ucd_call_open ucd_call_release ucd_call_dev_open ucd_call_dev_release ucd_call_dev_mmap

File Operations
---------------

UCD provides ``struct file_operations`` for both device types:

  - ``ucd_get_module_ops()``: Returns file_operations for ``/dev/manager``
  - ``ucd_get_module_dev_ops()``: Returns file_operations for ``/dev/ubfabricX``

Graceful Unregistration
-----------------------

When a feature is unregistered, UCD waits for all in-flight commands to
complete before removing it (with a 60-second timeout). This ensures no
use-after-free issues occur during module unload.

EXPORT_SYMBOL
-------------

The following UCD interfaces are exported for use by other kernel modules:

=============================================== ============================================
Symbol                                          Purpose
=============================================== ============================================
``ucd_register_cmd``                            Register a non-device feature handler
``ucd_unregister_cmd``                          Unregister a non-device feature handler
``ucd_register_dev_cmd``                        Register a device-specific feature handler
``ucd_unregister_dev_cmd``                      Unregister a device-specific feature handler
``ucd_register_module_ops``                     Register module ops for ``/dev/manager``
``ucd_register_module_dev_ops``                 Register module ops for ``/dev/ubfabricX``
``ucd_unregister_module_ops``                   Unregister module ops for ``/dev/manager``
``ucd_unregister_module_dev_ops``               Unregister module ops for ``/dev/ubfabricX``
``ucd_get_module_ops``                          Get file_operations for ``/dev/manager``
``ucd_get_module_dev_ops``                      Get file_operations for ``/dev/ubfabricX``
=============================================== ============================================

*******************************
Unified Resource Manager (URES)
*******************************

URES manages device resource information including
memory regions, register spaces, key-value pairs, attributes, IRQ information,
and window configurations. It serves as the resource discovery layer for the
UBFABRIC infrastructure.

URES is initialized at Stage 1 (after UCD) and provides a centralized
repository for all hardware resource information. Other modules (especially
kmap in UOP) query URES to discover available memory regions, register spaces,
and device attributes.

URES supports:

  - Per-device, per-die resource management
  - Memory region registration and query
  - Register region registration and query
  - Key-value pair storage for device parameters
  - Generic attribute storage
  - IRQ information management
  - Shared local window configuration

Architecture
============

.. code-block:: none

     +--------------------------------------------------+
     |                  URES Core                       |
     |                                                  |
     |  +--------+  +--------+  +------+  +---------+ |
     |  | Mem    |  | Reg    |  | Key  |  | Attr    | |
     |  | List   |  | List   |  | List |  | List    | |
     |  +--------+  +--------+  +------+  +---------+ |
     |                                                  |
     |  +------------+  +------------------+           |
     |  | IRQ Info   |  | Shared Local Win |           |
     |  +------------+  +------------------+           |
     +--------------------------------------------------+
         |          |          |          |
         v          v          v          v
     +-------+  +-------+  +-------+  +-------+
     | kmap  |  | UCD   |  | other |  | proc  |
     | (UOP) |  | ioctl |  | mod   |  | show  |
     +-------+  +-------+  +-------+  +-------+

Resource Hierarchy
------------------

Resources are organized in a three-level hierarchy:

  - **Device level**: Each device (identified by ``devid``) has its own resource set
  - **Die level**: Each device can have multiple dies (identified by ``die_id``)
  - **Resource type**: Memory, register, key, attribute, IRQ, window

Key Data Structures
-------------------

``struct ures``
  Full resource container: instance (devid + die_id), semaphore, and linked
  lists for mem/reg/key/attr nodes, IRQ info, and shared local windows.

``struct ures_node_mem``
  Memory region node: name, address info (addr + size), linked list entry.

``struct ures_node_reg``
  Register region node: name, address info (addr + size), linked list entry.

``struct ures_node_key``
  Key-value node: name, u64 value, linked list entry.

``struct ures_node_attr``
  Attribute node: name, void* data, size, linked list entry.

``struct ures_irq_info``
  IRQ entry: irq number and flag.

``struct ures_addr_info``
  Address information: base address and size.

API Reference
=============

UCD Commands
------------

URES registers the following ioctl commands under ``UCD_CMD_URES``. All of them
are dispatched through the generic UCD ioctl flow (see the UCD section); the
per-command call sequence is:

  1. User space opens ``/dev/ubfabricX`` (device commands) or ``/dev/manager``
     (global commands) and issues ``ioctl(fd, UCD_CMD_URES, sub_cmd, payload)``.
  2. UCD validates the caller, looks up the URES handler in the kv table and
     invokes the URES fops callback (e.g. ``ures_fops_get_mem_base``).
  3. The URES fops callback validates ``in_len``/``out_len``, builds a
     ``struct ures_inst`` from ``devid``/``die_id`` and calls the matching
     ``ures_*`` core API.
  4. The core API searches the per-device, per-die resource lists under the
     URES semaphore and fills the output buffer.
  5. UCD copies the output buffer back to user space and records DFX stats.

============================ =================== ================================
Command                      Permission          Description
============================ =================== ================================
URES_GET_MEM_BASE            PROC_CTRL_MANAGED   Get specific memory region base
URES_GET_REG_BASE            PROC_CTRL_MANAGED   Get specific register region base
URES_GET_MEM_LIST            PROC_CTRL_MANAGED   Get all memory regions
URES_GET_REG_LIST            PROC_CTRL_MANAGED   Get all register regions
URES_GET_KEY_VALUE           PROC_CTRL_ANY_USER  Get device key-value pair
URES_SET_KEY_VALUE           PROC_CTRL_MANAGED   Set device key-value pair
URES_GET_DEV_ATTR            PROC_CTRL_ANY_USER  Get device attribute
URES_GET_ATTR                PROC_CTRL_ANY_USER  Get global attribute
URES_GET_ATTR_KEY_LIST       PROC_CTRL_ANY_USER  Get all attribute keys
URES_GET_WIN_INFO            PROC_CTRL_MANAGED   Get shared local window info
============================ =================== ================================

Kernel-doc
----------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_init.c
   :functions: ures_init ures_uninit

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_core.c
   :functions: ures_get_mem_base ures_get_reg_base ures_get_mem_list ures_get_reg_list

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_core.c
   :functions: ures_get_key_value ures_set_key_value

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_core.c
   :functions: ures_get_attr ures_get_attr_key_list ures_dev_get_attr

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_core.c
   :functions: ures_get_share_local_win

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ures/ures_core.c
   :functions: ures_die_res_show ures_dev_res_show ures_show

EXPORT_SYMBOL
-------------

The following URES interfaces are exported for use by other kernel modules
(such as UOP/kmap during device initialization):

========================================== ============================================
Symbol                                     Purpose
========================================== ============================================
``ures_set_mem_base``                      Set a memory region base
``ures_get_mem_base``                      Get a memory region base
``ures_get_mem_list``                      Get the memory region list
``ures_put_mem_list``                      Release the memory region list
``ures_set_reg_base``                      Set a register region base
``ures_get_reg_base``                      Get a register region base
``ures_get_reg_list``                      Get the register region list
``ures_put_reg_list``                      Release the register region list
``ures_set_key_value``                     Set a device key-value pair
``ures_get_key_value``                     Get a device key-value pair
``ures_dev_set_attr``                      Set a per-device attribute
``ures_set_attr``                          Set a global attribute
``ures_dev_get_attr``                      Get a per-device attribute
``ures_get_attr``                          Get a global attribute
``ures_del_attr``                          Delete an attribute
``ures_get_attr_key_list``                 Get the list of attribute keys
``ures_put_attr_key_list``                 Release the attribute key list
``ures_set_irq_num``                       Set IRQ count
``ures_get_irq_num``                       Get IRQ count
``ures_set_irq_by_index``                  Set IRQ number by index
``ures_get_irq_by_index``                  Get IRQ number by index
``ures_set_irq_flag_by_index``             Set IRQ flag by index
``ures_get_irq_flag_by_index``             Get IRQ flag by index
``ures_set_share_local_win``               Set shared local window info
``ures_get_share_local_win``               Get shared local window info
``ures_die_reset``                         Reset resources of a specific die
``ures_dev_reset``                         Reset resources of a specific device
``ures_reset``                             Reset all URES resources
========================================== ============================================

Constants
=========

======================== =======
Name                     Value
======================== =======
URES_MAX_DEVNUM          Max number of devices
URES_MAX_DIE_NUM         Max dies per device
URES_IRQ_MAX_NUM         32
URES_IRQ_TYPE_NUM        16
URES_KEY_MAX_NUM         64
URES_MEM_MAX_NUM         Max memory regions
URES_REG_MAX_NUM         Max register regions
URES_ATTR_MAX_NUM        Max attributes
======================== =======

****************************
Unified Device Manager (UDM)
****************************

UDM manages device lifecycle (add/remove), provides
character device nodes (``/dev/ubfabricX``), and implements a priority-based
notifier chain so other modules can react to device add/remove events with
priority-based ordering.

UDM is initialized at Stage 2 (after URES) and serves as the central device
management module. It creates/destroys char device nodes, maintains device
instance information, and notifies other modules when devices are added or removed.

UDM supports:

  - Device instance management with reference counting
  - Character device creation with configurable ownership and permissions
  - Priority-based notifier chain for device lifecycle events
  - Device attribute get/set operations
  - Device status tracking (init, prepare, reset, etc.)

Architecture
============

.. code-block:: none

     +--------------------------------------------------+
     |                  UDM Core                        |
     |                                                  |
     |  +-----------+  +-----------+  +--------------+  |
     |  | Device    |  | Char Dev  |  | Notifier     |  |
     |  | Instances |  | (cdev)    |  | Chain        |  |
     |  +-----------+  +-----------+  +--------------+  |
     +--------------------------------------------------+
         |                  |                  |
         v                  v                  v
     +-----------+   +-----------+   +------------------+
     | Attribute |   | /dev/     |   | Priority Chain:  |
     | Query/Set |   | ubfabricX |   | PRI0 -> PRI1 ->  |
     +-----------+   +-----------+   | ... -> PRI_MAX   |
                                     +------------------+
                                         |       |
                                         v       v
                                     +------+ +------+
                                     | UED  | | UID  |
                                     |(PRI1)| |(PRI5)|
                                     +------+ +------+

Device Lifecycle
----------------

Device instances follow a lifecycle managed by status bits:

====== ============================= ============================================
Bit    Name                          Description
====== ============================= ============================================
0      UDM_STATUS_INIT_MASK          Device initialized
1      UDM_STATUS_PREPARE_MASK       Device prepared
2      UDM_STATUS_RESET_MASK         Device in reset state
3      UDM_STATUS_AFTER_RESET_MASK   Device after reset
4      UDM_STATUS_IRT_INIT_MASK      IRT initialized
====== ============================= ============================================

Notifier Chain
--------------

UDM implements a priority-ordered notifier chain. Modules register handlers
at specific priority levels. When a device event occurs, all handlers are called
in priority order (lower number = higher priority).

Notified Actions:

================ ================== ==========================================
Action           Enum Value         Description
================ ================== ==========================================
UDM_INIT         Device add         Device has been added to the system
UDM_UNINIT       Device remove      Device is being removed from the system
UDM_PREPARE      Device prepare     Device is being prepared for operation
UDM_RESET        Device reset       Device is undergoing reset
UDM_AFTER_RESET  After reset        Device has completed reset
UDM_IRQ_INIT     IRQ init           Interrupt handling initialized
UDM_IRQ_UNINIT   IRQ uninit         Interrupt handling uninitialized
================ ================== ==========================================

Key Data Structures
-------------------

``struct udm_dev_inst``
  Device instance: reference count, udevid, inst_id, status bitmap,
  device parameters, char device pointer.

``struct udm_dev_para``
  Device parameters: location, udevid, cna, eid, chip_type, guid.

``struct udm_cdev``
  Character device: name, device pointer, cdev pointer, device number,
  mutex for protection.

``struct udm_notifier_node``
  Notifier entry: list node, status array per device, name, callback
  function, call count, call finish flag.

``struct udm_notifiers``
  Notifier chain: priority-head array with ``UDM_PRI_MAX`` levels,
  rw_semaphore for concurrent access.

API Reference
=============

UCD Commands
------------

UDM does not register any UCD ioctl command. It is driven purely from kernel
space: a bus/HDF probe path calls ``udm_add_dev()`` to publish a device, which
walks the priority-ordered notifier chain (PRI0 -> PRI1 -> ... -> PRI_MAX); the
reverse happens on ``udm_remove_dev()``.

Kernel-doc
----------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/udm/udm_dev.c
   :functions: udm_add_dev udm_remove_dev udm_get_dev_attr udm_set_dev_attr udm_dev_inst_get udm_dev_inst_put udm_get_local_devid udm_is_udevid_exist

.. kernel-doc:: drivers/ub/ubfm/ubfabric/udm/udm_cdev.c
   :functions: udm_create_cdev udm_cdev_remove_dev

.. kernel-doc:: drivers/ub/ubfm/ubfabric/udm/udm_notifier.c
   :functions: udm_notifier_register udm_notifier_unregister udm_notifier_call udm_get_notifiers

EXPORT_SYMBOL
-------------

========================================== ============================================
Symbol                                     Purpose
========================================== ============================================
``udm_add_dev``                            Add a device instance and notify the chain
``udm_remove_dev``                         Remove a device instance and notify the chain
``udm_get_dev_attr``                       Get a device attribute
``udm_set_dev_attr``                       Set a device attribute
``udm_get_local_devid``                    Get the local device id
``udm_is_udevid_exist``                    Check whether a udevid exists
``udm_notifier_register``                  Register a device-lifecycle notifier
``udm_notifier_unregister``                Unregister a device-lifecycle notifier
``udm_notifier_call``                      Trigger the notifier chain
========================================== ============================================

Device Node Configuration
-------------------------

Character device nodes are created with the following attributes:

  - **Name pattern**: ``/dev/ubfabric0``, ``/dev/ubfabric1``, etc.
  - **Major:Minor**: Allocated dynamically via ``alloc_chrdev_region``
  - **Owner UID/GID**: 1201 (mamiUser)
  - **Mode**: 0660 (owner and group read/write)

******************************
Unified Event Dispatcher (UED)
******************************

UED implements an in-kernel event publishing/
subscribing mechanism. Applications create groups, subscribe to events, and wait
for events through ring buffers with thread-level scheduling.

UED is initialized at Stage 3 and registers as a UDM notifier at PRI1 to receive
device add/remove events. It manages a hierarchical context structure: device ->
task -> group -> thread, providing per-thread event delivery via ring buffers.

UED supports:

  - Event group creation and destruction
  - Event subscription with bitmap-based event filtering
  - Ring buffer-based event queue (128 entries per group)
  - Thread-level scheduling with wait_queue blocking
  - Event publishing from both user space (ioctl) and kernel space
  - DFX statistics (publish count, schedule count, drop count)

Architecture
============

.. code-block:: none

     +----------------------------------------------------------+
     |                       UED Core                           |
     |                                                          |
     |  +----------------+    +----------------+                |
     |  | Device Context |    | Device Context |    ...         |
     |  | (dev_ctx)      |    | (dev_ctx)      |                |
     |  +----------------+    +----------------+                |
     |    |                        |                            |
     |    v                        v                            |
     |  +----------------+    +----------------+                |
     |  | Task Context   |    | Task Context   |    ...         |
     |  | (task_ctx)     |    | (task_ctx)     |                |
     |  +----------------+    +----------------+                |
     |    |                        |                            |
     |    v                        v                            |
     |  +--------------+    +--------------+                    |
     |  | Group Context|    | Group Context|    ...             |
     |  | (grp_ctx)    |    | (grp_ctx)    |                    |
     |  +--------------+    +--------------+                    |
     |    |                                                     |
     |    v                                                     |
     |  +----------+ +----------+ +----------+ +----------+     |
     |  | Thread 0 | | Thread 1 | | Thread 2 | | Thread 3 |     |
     |  | (idle)   | | (ready)  | | (run)    | | (idle)   |     |
     |  +----------+ +----------+ +----------+ +----------+     |
     |    |               |           |               |         |
     |    v               v           v               v         |
     |  +----------------------------------------------+        |
     |  |           Ring Buffer (128 entries)          |        |
     |  +----------------------------------------------+        |
     +----------------------------------------------------------+

Context Hierarchy
-----------------

The UED context is organized as a four-level hierarchy:

  - **Device Context** (``ued_dev_ctx``): Per-device, contains subscriber array
    and task domain
  - **Task Context** (``ued_task_ctx``): Per-process (pid), contains group
    context array
  - **Group Context** (``ued_grp_ctx``): Per-group, contains event bitmap,
    statistics, ring buffer, and thread context array
  - **Thread Context** (``ued_thread_ctx``): Per-thread, manages scheduling
    state (idle/ready/run) with wait_queue

Thread Scheduling
-----------------

Each thread in a group has a scheduling state machine:

.. code-block:: none

     +------+  event arrived  +-------+  scheduled  +-----+
     | IDLE | --------------> | READY | ----------> | RUN |
     +------+                 +-------+             +-----+
         ^                                              |
         |              event processed                 |
         +----------------------------------------------+

  - **IDLE**: Thread is waiting, no events to process
  - **READY**: Events are pending in the ring buffer, thread can be scheduled
  - **RUN**: Thread is actively processing events

Event Queue
-----------

Events are stored in a ring buffer (``ued_event_que``) with the following
properties:

  - **Depth**: 128 entries (``UED_EVENT_QUE_DEPTH``)
  - **Concurrency**: Protected by spinlock
  - **Overflow**: When the ring buffer is full, events are dropped and
    ``full_drop_event_num`` is incremented

Key Data Structures
-------------------

``struct ued_dev_ctx``
  Device context: devid, current task_id, subscriber array, task domain,
  proc entry.

``struct ued_task_ctx``
  Task context: pid, devid, task_id, valid flag, timestamps, group context
  array, proc entry.

``struct ued_grp_ctx``
  Group context: valid flag, gid, pid, thread_num, event_bitmap, statistics
  array, event queue, thread context array.

``struct ued_thread_ctx``
  Thread context: status, tid, pid, scheduled event number, mutex, wait_queue.

``struct ued_event_que``
  Ring buffer: depth, head, tail, lock, event array.

``struct ued_event_subscriber``
  Subscriber: pid and gid of the subscribing group.

``struct ued_grp_stat``
  Per-group statistics: publish_event_num, sched_event_num, full_drop_event_num.

API Reference
=============

UCD Commands
------------

UED registers the following ioctl commands under ``UCD_CMD_UED``. The typical
event-delivery call sequence from user space is:

  1. ``UED_CREATE_GRP`` - the application creates a group on ``/dev/ubfabricX``
     (or ``/dev/manager``). UED allocates a ``ued_grp_ctx`` with a 128-entry
     ring buffer and a thread pool.
  2. ``UED_SUBSCRIBE_EVENT`` - the application registers the event bitmap it is
     interested in; UED records it on the group and adds the (pid, gid) tuple
     to the device subscriber list.
  3. ``UED_WAIT_EVENT`` - the application thread blocks on the group wait_queue.
     When a matching event arrives, the thread transitions IDLE -> READY -> RUN
     and the event is dequeued from the ring buffer.
  4. ``UED_PUBLISH_EVENT`` (user space) or ``ued_submit_event_to_dev()`` (kernel
     space) - a publisher injects an event. UED matches it against subscribers,
     enqueues it into each matching group's ring buffer and wakes one ready
     thread.
  5. ``UED_DESTROY_GRP`` - the application releases the group; UED flushes the
     ring buffer, frees the thread context and removes the subscriber entry.

All commands go through the generic UCD ioctl flow (permission check, kv lookup,
handler invocation, DFX accounting).

======================= =================== ===================================
Command                 Permission          Description
======================= =================== ===================================
UED_CREATE_GRP          PROC_CTRL_ANY_USER  Create an event group
UED_DESTROY_GRP         PROC_CTRL_ANY_USER  Destroy an event group
UED_SUBSCRIBE_EVENT     PROC_CTRL_MANAGED   Subscribe to events by bitmap
UED_WAIT_EVENT          PROC_CTRL_ANY_USER  Wait for events with timeout
UED_PUBLISH_EVENT       PROC_CTRL_MANAGED   Publish an event to subscribers
======================= =================== ===================================

Kernel-doc
----------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ued/ued_core.c
   :functions: ued_core_init ued_core_uninit ued_core_add_dev ued_core_del_dev

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ued/ued_core.c
   :functions: ued_core_add_task ued_core_del_task

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ued/ued_core.c
   :functions: ued_core_create_grp ued_core_destroy_grp

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ued/ued_core.c
   :functions: ued_core_subscribe_event ued_core_wait_event ued_core_publish_event

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ued/ued_core.c
   :functions: ued_submit_event_to_dev

EXPORT_SYMBOL
-------------

========================================== ============================================
Symbol                                     Purpose
========================================== ============================================
``ued_submit_event_to_dev``                Inject an event into a device's subscriber
                                           groups from kernel space
========================================== ============================================

Constants
=========

===================== =======
Name                  Value
===================== =======
UED_MAX_TASK          32
UED_MAX_GRP           16
UED_MAX_THREAD        4
UED_MAX_SUBSCRIBER    11
UED_EVENT_QUE_DEPTH   128
===================== =======

**********************************
Unified Interrupt Dispatcher (UID)
**********************************

UID provides an interrupt handling framework
with configurable bottom-half processing. It supports both tasklet and workqueue
based bottom-half mechanisms, with optional scheduling delay for interrupt
coalescing.

UID is initialized at Stage 3 and registers as a UDM notifier at PRI5. When a
device is added, UID initializes per-device interrupt instances. When a hardware
interrupt occurs, UID triggers the registered bottom-half handler.

UID supports:

  - Interrupt type registration with configurable attributes
  - Tasklet-based bottom-half for non-blocking handlers
  - Workqueue-based bottom-half for blocking handlers
  - Delayed work for interrupt coalescing
  - Per-device interrupt instance management
  - Atomic trigger count tracking

Architecture
============

.. code-block:: none

     Hardware Interrupt (ISR)
         |
         v
     +--------------------------------------------------+
     |                  UID Core                        |
     |                                                  |
     |  +------------+    +------------+                |
     |  | Intr Type  |    | Intr Type  |    ...         |
     |  | (registered|    | (registered|                |
     |  |  handler)  |    |  handler)  |                |
     |  +------------+    +------------+                |
     |         |                 |                      |
     |         v                 v                      |
     |  +-------------+  +-------------+                |
     |  | Intr Inst   |  | Intr Inst   |    ...         |
     |  | (per-device)|  | (per-device)|                |
     |  +-------------+  +-------------+                |
     |    |           |                                 |
     |    v           v                                 |
     |  +--------+ +--------+                           |
     |  |tasklet | |workqueue|                          |
     |  |(non-   | |(blocking|                          |
     |  |blocking)| |capable) |                         |
     |  +--------+ +--------+                           |
     +--------------------------------------------------+

Bottom-Half Processing
----------------------

UID supports two bottom-half mechanisms:

  - **Tasklet**: Used for non-blocking interrupt handlers. Runs in softirq
    context with lower latency but cannot sleep.
  - **Workqueue**: Used for blocking-capable interrupt handlers. Runs in
    process context and can sleep, but has higher latency.

The choice between tasklet and workqueue is determined by the interrupt
type's ``attr`` during registration.

Interrupt Coalescing
--------------------

When ``max_sched_time > 0``, UID uses a delayed work mechanism to coalesce
multiple interrupts before scheduling the bottom-half handler. This reduces
interrupt overhead for high-frequency events.

Key Data Structures
-------------------

``struct uid_intr``
  Registered interrupt type: flag, attributes, and operation callbacks.

``struct uid_intr_inst``
  Per-device interrupt instance: type, init_flag, attributes, delayed_work,
  tasklet/work union, workqueue, parameters, timing info, triggered (atomic),
  timeout_flag.

``struct uid_dev_ctx``
  Device context: udevid and interrupt instance array.

API Reference
=============

UCD Commands
------------

UID does not register any UCD ioctl command. Interrupt flow is driven from
kernel space only:

  1. A driver calls ``uid_register_intr()`` to register an interrupt type with
     its ``uid_intr_attr`` (tasklet vs workqueue, ``max_sched_time`` for
     coalescing) and ``uid_intr_ops`` (bottom-half handler).
  2. When UDM announces ``UDM_INIT`` for a device, UID's notifier callback runs
     ``uid_add_dev()`` to create the per-device interrupt instances.
  3. The hardware ISR calls ``uid_trigger_intr(udevid, type, irq_info)``; UID
     arms the configured bottom-half (tasklet / workqueue / delayed_work).
  4. The bottom-half handler runs out-of-band and invokes the registered
     ``handler`` callback.
  5. On device removal (``UDM_UNINIT``), ``uid_del_dev()`` tears down the
     instances; ``uid_unregister_intr()`` removes the interrupt type.

Kernel-doc
----------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/uid/uid_core.c
   :functions: uid_core_init uid_core_uninit

.. kernel-doc:: drivers/ub/ubfm/ubfabric/uid/uid_core.c
   :functions: uid_register_intr uid_unregister_intr

.. kernel-doc:: drivers/ub/ubfm/ubfabric/uid/uid_core.c
   :functions: uid_trigger_intr

.. kernel-doc:: drivers/ub/ubfm/ubfabric/uid/uid_core.c
   :functions: uid_add_dev uid_del_dev

EXPORT_SYMBOL
-------------

========================================== ============================================
Symbol                                     Purpose
========================================== ============================================
``uid_register_intr``                      Register an interrupt type and its ops
``uid_unregister_intr``                    Unregister an interrupt type
``uid_trigger_intr``                       Trigger the bottom-half for an interrupt
========================================== ============================================

Usage Example
=============

Registering an interrupt type::

    struct uid_intr_attr attr = {
        .max_sched_time = 10,  /* coalesce for 10ms */
        .flags = UID_INTR_FLAG_WORKQUEUE,  /* use workqueue */
    };
    struct uid_intr_ops ops = {
        .handler = my_intr_handler,
        .handler_data = my_data,
    };
    uid_register_intr(MY_INTR_TYPE, &attr, &ops);

Triggering from ISR::

    irqreturn_t my_isr(int irq, void *dev_id)
    {
        uid_trigger_intr(udevid, MY_INTR_TYPE, irq_info);
        return IRQ_HANDLED;
    }

******************
Unified Log (ULOG)
******************

ULOG manages kernel log levels (per-device and global), log
deduplication, and provides ioctl commands for setting/querying log
configuration. It serves as the logging infrastructure for all UBFABRIC modules.

ULOG provides two main functionalities:

  - **Log level management**: Per-device and global log level configuration,
    allowing fine-grained control over log output verbosity.
  - **Log deduplication**: An LRU hash cache that identifies and suppresses
    duplicate log messages, preventing log flooding.

Architecture
============

.. code-block:: none

     +--------------------------------------------------+
     |                  ULOG Core                       |
     |                                                  |
     |  +--------------+  +---------------------------+  |
     |  | Log Level    |  | Deduplication Cache       |  |
     |  | Management   |  | (128-entry LRU hash)      |  |
     |  |              |  |                           |  |
     |  | +----------+ |  | +-----+ +-----+ +-----+  |  |
     |  | | global   | |  | |entry| |entry| |entry|  |  |
     |  | | level    | |  | | 0   | | 1   | | N   |  |  |
     |  | +----------+ |  | +-----+ +-----+ +-----+  |  |
     |  | +----------+ |  +---------------------------+  |
     |  | |dev0 level| |                                 |
     |  | +----------+ |                                 |
     |  | +----------+ |                                 |
     |  | |dev1 level| |                                 |
     |  | +----------+ |                                 |
     |  +--------------+                                 |
     +--------------------------------------------------+
         |            |
         v            v
     +---------+  +-----------+
     | UCD cmd |  | All other |
     | (ioctl) |  | modules   |
     +---------+  +-----------+

Log Level Management
--------------------

ULOG supports two levels of log configuration:

  - **Global log level**: Applies to all devices when no device-specific
    level is configured.
  - **Per-device log level**: Each device can have its own ``devUserLogLevel``
    and ``devDefaultLogLevel``.

The log drop check follows this logic:

  1. If the message level is above the configured level, it is dropped
  2. If a device-specific user level is set and the message is from that
     device, use the device level; otherwise use the global level

Log Deduplication
-----------------

ULOG implements a 128-entry LRU hash cache to identify and suppress duplicate
log messages. When a log message is about to be printed:

  1. The format string is hashed
  2. The hash is looked up in the cache
  3. If the same hash was seen recently, the message is suppressed
  4. Otherwise, the message is printed and the cache is updated

This prevents log flooding from repeated identical messages.

Key Data Structures
-------------------

``struct ulog_dev``
  Per-device log configuration: devUserLogLevel and devDefaultLogLevel.

``struct ulog_item``
  Log cache entry: hash value and timestamp for deduplication.

``struct ulog_cache``
  128-entry LRU cache for log deduplication.

API Reference
=============

UCD Commands
------------

ULOG registers the following ioctl commands under ``UCD_CMD_ULOG``. Call
sequence:

  1. User space issues ``ioctl(fd, UCD_CMD_ULOG, ULOG_SET_LOG_CFG, cfg)`` to
     push a new log-level configuration.
  2. UCD dispatches to ``ulog_ucd_set_log_cfg()`` (in ``ulog_handle.c``), which
     parses the user payload and calls ``ulog_kern_set_log_cfg()`` to update the
     global level and/or the per-device ``devUserLogLevel``/``devDefaultLogLevel``.
  3. Subsequent ``mami_kern_log_drop()`` calls from any UBFABRIC module compare
     the message level against the active level (device level overrides global)
     and consult the 128-entry LRU dedup cache before printing.
  4. ``ULOG_GET_LOG_CFG`` returns the current configuration back to user space
     for verification.

================== =================== =====================================
Command            Permission          Description
================== =================== =====================================
ULOG_SET_LOG_CFG   PROC_CTRL_MANAGED   Set log level configuration
ULOG_GET_LOG_CFG   PROC_CTRL_MANAGED   Get log level configuration
================== =================== =====================================

Kernel-doc
----------

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ulog/ulog_core.c
   :functions: ulog_kern_set_log_cfg ulog_kern_get_log_cfg

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ulog/ulog_core.c
   :functions: mami_kern_log_drop

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ulog/ulog_core.c
   :functions: ulog_is_duplicate

.. kernel-doc:: drivers/ub/ubfm/ubfabric/ulog/ulog_core.c
   :functions: ulog_kern_dump_snapshot

EXPORT_SYMBOL
-------------

========================================== ============================================
Symbol                                     Purpose
========================================== ============================================
``mami_kern_log_drop``                     Check whether a log message should be
                                           dropped per current level + dedup cache
``ulog_is_duplicate``                      Test a log message against the LRU cache
``ulog_kern_dump_snapshot``               Dump the current ULOG configuration
                                           snapshot
========================================== ============================================

*******************************
Unified Operation Manager (UOP)
*******************************

UOP provides device operation management including
memory/register mapping, access node management, and window configuration.
It has a layered architecture with kernel mapping (kmap), user mapping (umap),
operations (uops), and chip-specific adapters.

UOP is initialized at Stage 4 and registers as a UDM notifier at PRI1 to receive
device add/remove events. When a device is added, UOP initializes the kernel
mapping layer (kmap), registers chip-specific operations (uops), and sets up
the core device operation management.

UOP supports:

  - Kernel-space memory/register mapping (kmap)
  - User-space memory mapping (umap)
  - Chip-specific operation adapters (CDMA, LDST, PCIe I3C, PCIe LDST)
  - Access node management for multi-die topology
  - Window configuration for shared local windows
  - FPGA register mapping

Architecture
============

.. code-block:: none

     +----------------------------------------------+
     |                      UOP Core                |
     |                                              |
     |  +------------------+  +------------------+  |
     |  |    kmap          |  |    umap          |  |
     |  | (kernel mapping) |  | (user mapping)   |  |
     |  |                  |  |                  |  |
     |  | +------+ +-----+|  | +------+ +------+|   |
     |  | |mem   | |reg  ||  | |mem   | |reg   ||   |
     |  | |map   | |map  ||  | |map   | |map   ||   |
     |  | +------+ +-----+|  | +------+ +------+|   |
     |  | +------+ +-----+|  | +------+        ||   |
     |  | |acc   | |bare ||  | |fpga  |        ||   |
     |  | |node  | |item ||  | |reg   |        ||   |
     |  | |map   | |map  ||  | |map   |        ||   |
     |  | +------+ +-----+|  | +------+        ||   |
     |  +------------------+  +------------------+  |
     |                                              |
     |  +------------------+  +------------------+  |
     |  |    uops          |  |   adapters       |  |
     |  | (chip-specific   |  | (FPGA, I3C)      |  |
     |  |  operations)     |  |                  |  |
     |  | +------+ +-----+ |  |                  |  |
     |  | |CDMA  | |LDST | |  |                  |  |
     |  | +------+ +-----+ |  |                  |  |
     |  | +------+ +-----+ |  |                  |  |
     |  | |PCIe  | |PCIe | |  |                  |  |
     |  | |I3C   | |LDST | |  |                  |  |
     |  | +------+ +-----+ |  |                  |  |
     |  +------------------+  +------------------+  |
     +----------------------------------------------+
         |          |          |          |
         v          v          v          v
     +-------+  +-------+  +-------+  +-------+
     | URES  |  | UCD   |  | UDM   |  | UID   |
     +-------+  +-------+  +-------+  +-------+

Sub-modules
===========

core
----

The core sub-module provides device operation management, items registry,
and access node management.

Key Data Structures:

``struct uop_dev``
  Device: devid, ops, extend_ops, kref, kmap_dev pointer, items array,
  access nodes.

``struct uop_items``
  Items by type: type, semaphore, atomic index, num, item array.

``struct uop_acc_nodes``
  Access nodes: bare_item_num, bare_items, acc_node_num, nodes,
  last_acc_idx, semaphore.

API:

  - ``uop_kern_register_ops(devid, ops, type)``: Register ops for device
  - ``uop_kern_add_item_with_op_dev(map_dev, item, type)``: Add item to device
  - ``uop_kern_get_items(map_dev, type, item, num, &real_num)``: Get items
  - ``uop_kern_add_acc_node(map_dev)`` / ``uop_kern_get_acc_node()``: Manage access nodes
  - ``uop_kern_register_win_ops(ops)`` / ``uop_kern_unregister_win_ops()``: Window ops
  - ``uop_kern_get_idle_local_win()`` / ``uop_kern_cfg_win()`` / ``uop_kern_clear_win()``: Window operations
  - ``uop_kern_register_msg_ops(ops)``: Register message ops
  - ``uop_kern_add_memory(devid, ...)``: Add a memory region to a device
  - ``uop_kern_writel`` / ``uop_kern_readl``: 32-bit register write/read
  - ``uop_kern_writeq`` / ``uop_kern_readq``: 64-bit register write/read
  - ``uop_kern_mem_write`` / ``uop_kern_mem_read``: Device memory write/read
  - ``uop_kern_io_buffer_write`` / ``uop_kern_io_buffer_read``: IO buffer write/read
  - ``uop_i3c_send_recv_kern``: I3C send/receive
  - ``uop_kern_send_recv_msg``: Message send/receive

kmap
----

The kmap (kernel mapping) sub-module maps ASIC resources (memory, registers)
from URES into kernel-space addressable structures.

Key Data Structures:

``struct kmap_dev``
  Device: devid, die_num, acc_type, mem_num, mem array, reg_num, reg array,
  bare_item_num, bare_item array, acc_node_num, acc_node array.

``struct kmap_addr``
  Address: die_id, name, acc_addr, local_asic_addr, asic_addr, size.

``struct kmap_bare_addr``
  Bare address: die_id, name, asic_addr, size.

``struct kmap_acc_node``
  Access node: acc_addr, local_asic_addr, local_win_id, win_size.

``struct kmap_ops``
  Operations: mem_map/unmap, reg_map/unmap, acc_node_map/unmap.

API:

  - ``kmap_dev_init(devid, type, map_dev)`` / ``kmap_dev_uninit(map_dev)``: Init/uninit mapping
  - ``kmap_ops_register(type, ops)`` / ``kmap_ops_unregister(type)``: Ops registry

Adapter-specific kmap implementations:

  - **kmap_cdma**: CDMA access type kernel mapping
  - **kmap_ldst**: Load/Store access type kernel mapping
  - **kmap_pcie_i3c**: PCIe I3C access type kernel mapping
  - **kmap_pcie_ldst**: PCIe Load/Store access type kernel mapping

umap
----

The umap (user mapping) sub-module provides user-space memory mapping
capabilities, allowing applications to directly access device memory and
registers through mmap.

API:

  - ``umap_kern_mem_map(devid, die_id, name, acc_addr, size)``: Map memory in kernel
  - ``umap_kern_reg_map(devid, die_id, name, acc_addr, size)``: Map register in kernel
  - ``umap_kern_unmap(acc_addr, size)``: Unmap kernel mapping
  - ``umap_mem_map(acc_addr, size, mem, name)``: Map memory for user space
  - ``umap_fpga_reg_map(devid, acc_addr, phy_addr, size)``: Map FPGA register
  - ``umap_fpga_reg_unmap(...)``: Unmap FPGA register
  - ``umap_mmap(devid, vma)``: mmap handler for char device

uops
----

The uops sub-module provides chip-specific operation implementations that
are registered per access type.

API:

  - ``uops_kern_init(devid, type)``: Initialize ops for device
  - ``uops_kern_uninit(devid)``: Reset ops for device
  - ``uops_kern_register(type, ops)`` / ``uops_kern_unregister(type)``: Register/unregister ops

Adapter-specific uops:

  - **uops_cdma**: CDMA operation set
  - **uops_cdma_symbol**: DMA symbol resolution for CDMA
  - **uops_ldst**: Load/Store operation set
  - **uops_pcie_i3c**: PCIe I3C operation set
  - **uops_pcie_ldst**: PCIe Load/Store operation set

Adapters
--------

  - **fpga_back_kern.c**: FPGA backend operations for register access
  - **i3c_kern.c**: I3C bus operations for device communication

API Reference
=============

UCD Commands
------------

UOP registers the following ioctl commands under ``UCD_CMD_UOP``. They are all
device commands reached through ``/dev/ubfabricX``. Common call sequence:

  1. User space opens ``/dev/ubfabricX`` and issues the ioctl with the
     ``UOP_MEM_*``/``UOP_REG_*``/``UOP_ACC_NODE_*``/``UOP_*_WIN`` sub-commands
     and a payload describing the target die, name, address and size.
  2. UCD dispatches to the UOP fops handler (e.g. ``uop_fops_mem_map``).
  3. For map commands, the handler calls into the umap sub-module which resolves
     the access type via URES key-value, programs the access node / window
     (``UOP_CFG_WIN``/``UOP_CLR_WIN``) and returns the user-mappable address.
  4. For unmap commands, the handler tears down the mapping and releases the
     access node / window resources.
  5. ``UOP_FPGA_BACK_REG_MAP``/``UOP_FPGA_BACK_REG_UNMAP`` target the FPGA
     backend register window used during bring-up.
  6. UCD copies the result back to user space and updates DFX statistics.

=========================== =================== =====================================
Command                     Permission          Description
=========================== =================== =====================================
UOP_MEM_MAP                 PROC_CTRL_MANAGED   Map device memory region
UOP_MEM_UNMAP               PROC_CTRL_MANAGED   Unmap device memory region
UOP_REG_MAP                 PROC_CTRL_MANAGED   Map device register region
UOP_REG_UNMAP               PROC_CTRL_MANAGED   Unmap device register region
UOP_FPGA_BACK_REG_MAP       PROC_CTRL_MANAGED   Map FPGA backend register
UOP_FPGA_BACK_REG_UNMAP     PROC_CTRL_MANAGED   Unmap FPGA backend register
UOP_ACC_NODE_MAP            PROC_CTRL_MANAGED   Map access node
UOP_ACC_NODE_UNMAP          PROC_CTRL_MANAGED   Unmap access node
UOP_CFG_WIN                 PROC_CTRL_MANAGED   Configure window
UOP_CLR_WIN                 PROC_CTRL_MANAGED   Clear window
=========================== =================== =====================================

EXPORT_SYMBOL
-------------

The following UOP interfaces are exported for use by other kernel modules.

core (``drivers/ub/ubfm/ubfabric/uop/core/uop_kern.c``):

============================================================ ============================================
Symbol                                                       Purpose
============================================================ ============================================
``uop_kern_register_win_ops``                                Register window ops
``uop_kern_unregister_win_ops``                              Unregister window ops
``uop_kern_register_msg_ops``                                Register message ops
``uop_kern_add_memory``                                      Add a memory region to a device
``uop_kern_writel`` / ``uop_kern_readl``                     32-bit register write/read
``uop_kern_writeq`` / ``uop_kern_readq``                     64-bit register write/read
``uop_kern_mem_write`` / ``uop_kern_mem_read``               Device memory write/read
``uop_kern_io_buffer_write`` / ``uop_kern_io_buffer_read``   IO buffer write/read
``uop_i3c_send_recv_kern``                                   I3C send/receive
``uop_kern_send_recv_msg``                                   Message send/receive
============================================================ ============================================

CDMA symbol resolution (``drivers/ub/ubfm/ubfabric/uop/uops/uops_cdma_symbol.c``):

============================================================ ============================================
Symbol                                                       Purpose
============================================================ ============================================
``dma_read_symbol`` / ``dma_write_symbol``                   DMA read/write via symbol table
``dma_get_device_list_symbol`` /
``dma_free_device_list_symbol``                              Get/free DMA device list
``dma_register_client_symbol`` /
``dma_unregister_client_symbol``                             Register/unregister DMA client
``dma_register_seg_symbol`` /
``dma_unregister_seg_symbol`` /
``dma_import_seg_symbol`` /
``dma_unimport_seg_symbol``                                  DMA segment lifecycle
``dma_alloc_queue_symbol`` / ``dma_poll_queue_symbol`` /
``dma_free_queue_symbol``                                    DMA queue alloc/poll/free
``dma_create_context_symbol`` /
``dma_delete_context_symbol``                                DMA context create/delete
============================================================ ============================================

FPGA backend (``drivers/ub/ubfm/ubfabric/uop/fpga_adapter/fpga_back_kern.c``):

============================================================ ============================================
Symbol                                                       Purpose
============================================================ ============================================
``uop_fpga_info_init_kern``                                  Initialize FPGA backend info
``uop_fpga_info_uninit_all_kern``                            Uninitialize all FPGA backend info
``uop_fpga_get_socket_id``                                   Get socket id
``uop_fpga_get_back_vaddr``                                  Get backend virtual address
``uop_fpga_get_msi_int_vaddr``                               Get MSI interrupt virtual address
``fpga_ops_get``                                             Get FPGA ops
``starry_drv_board_get_ucmi_chip_num``                       Get UCMI chip count
``starry_drv_board_get_tor_slotid``                          Get ToR slot id
``starry_drv_get_ucmi_bsp_state``                            Get UCMI BSP state
``starry_drv_pci_get_device_bar``                            Get PCI device BAR
``starry_drv_board_get_chip_mng``                            Get chip management info
``starry_drv_intr_set_regist`` /
``starry_drv_intr_set_unregist``                             Register/unregister interrupt
``starry_drv_fpga_ucmi_int_status_read`` /
``starry_drv_fpga_ucmi_int_status_write``                    UCMI interrupt status read/write
``starry_drv_fpga_ucmi_int_mask_read`` /
``starry_drv_fpga_ucmi_int_mask_write``                      UCMI interrupt mask read/write
``starry_drv_ucmi_4m_win_config``                            Configure 4M window
``starry_drv_fpga_ucmi_get_rx_proc``                         Get RX proc
``starry_drv_fpga_ucmi_write_ucmi_global_cfg``
                                                             Write UCMI global config
``starry_drv_fpga_ucmi_write_chip_acc_win_doorbell``
                                                             Write chip access window doorbell
``starry_drv_fpga_ucmi_write_chip_acc_win_sqe``
                                                             Write chip access window SQE
``starry_drv_fpga_ucmi_write_cfg_done_flag``
                                                             Write config-done flag
``ldst_back_readl_kern`` / ``ldst_back_writel_kern``
                                                             32-bit LDST backend read/write
``ldst_back_readq_kern`` / ``ldst_back_writeq_kern``
                                                             64-bit LDST backend read/write
``ldst_back_mem_read_kern`` / ``ldst_back_mem_write_kern``
                                                             LDST backend memory read/write
============================================================ ============================================

I3C adapter (``drivers/ub/ubfm/ubfabric/uop/i3c_adapter/i3c_kern.c``):

============================================================ ============================================
Symbol                                                       Purpose
============================================================ ============================================
``i3c_symbol_init`` / ``i3c_symbol_uninit``                  I3C symbol init/uninit
``i3c_drv_ubnic_probe``                                      I3C ubnic probe
``i3c_drv_ubnic_xfer``                                       I3C ubnic transfer
``i3c_drv_ubnic_online_status``                              I3C ubnic online status
============================================================ ============================================

Access Types
============

UOP supports the following access types, each with its own kmap and uops
implementation:

================= ============================================================
Access Type       Description
================= ============================================================
CDMA              Central DMA (Direct Memory Access) based access with
                  symbol resolution
LDST              Direct Load/Store access
PCIe I3C          PCIe bus with I3C (Improved Inter-Integrated Circuit)
                  sideband communication
PCIe LDST         PCIe bus with direct Load/Store access
================= ============================================================

Support
=======

If there is any issue or question, please contact the vendor's support channel.
