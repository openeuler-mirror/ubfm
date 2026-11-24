.. SPDX-License-Identifier: CC-BY-4.0

.. Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.

.. This documentation is licensed under the Creative Commons Attribution 4.0
.. International License (CC-BY-4.0). See the License/CC-BY-4.0 file for the
.. full text of the license.


======================
MCTRLQ Driver
======================

MCTRLQ is the control queue module of the MAMI (Management Architecture and
Model Interface) UB fabric subsystem. It comprises two cooperating sub-modules:
CTRLQ, the interrupt-driven in-band control queue carrying small fixed-size
control messages over data-path channels (one channel per device); and MSGQ,
the management-plane message queue carrying variable-length management
messages between the host and remote devices or between dies (one channel per
die, polled by a per-device delayed work). MCTRLQ is built on top of the
UBFABRIC infrastructure and reuses common services such as UCD, UDM, UID,
URES, UOP, UED and IDM.

.. toctree::
   :maxdepth: 2
   :numbered:

   ctrlq
   msgq
