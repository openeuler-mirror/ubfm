.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


======================
LINKCOM Driver
======================

LINKCOM is the link communication module of the MAMI (Management Architecture
and Model Interface) UB fabric subsystem. It provides a mgmtq based packet channel
between the host CPU and the device's control CPU, allowing user-space
applications to send and receive UB link packets (enumeration, control,
config, and UB-memory packets) through a channel-oriented interface. LINKCOM
is built on top of the UBFABRIC infrastructure and reuses UCD, UDM, UID, and
URES.

.. toctree::
   :maxdepth: 2
   :numbered:

   linkcom
