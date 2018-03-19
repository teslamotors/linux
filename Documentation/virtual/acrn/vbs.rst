================================
Virtio and Backend Service (VBS)
================================

The Virtio and Backend Service (VBS) in part of ACRN Project.

The VBS can be further divided into two parts: VBS in user space (VBS-U)
and VBS in kernel space (VBS-K).

Example:
--------
A reference driver for VBS-K can be found at :c:type:`struct vbs_rng`.

.. kernel-doc:: drivers/vbs/vbs_rng.c

APIs:
-----

.. kernel-doc:: include/linux/vbs/vbs.h
.. kernel-doc:: include/linux/vbs/vq.h
