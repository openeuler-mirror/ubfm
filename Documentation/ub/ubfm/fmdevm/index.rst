.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


======================
FMDEVM Driver
======================

FMDEVM is the device management layer of the MAMI (Management Architecture
and Model Interface) UB fabric subsystem. It is responsible for hardware
discovery and probing, device creation/destruction through the UDM, logical/physical
identifier mapping, port information initialization, interrupt management,
fault event management and RAS error reporting. It builds on top of the
UBFABRIC infrastructure framework (UCD, URES, UDM, UID, UOP, ULOG) and
exposes a single character device and a set of in-kernel APIs that other
MAMI drivers consume.

.. toctree::
   :maxdepth: 2
   :numbered:

   fmdevm
