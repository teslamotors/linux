.. SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

=============
Introduction:
=============

Few storage technologies such is EMMC, UFS, and NVMe support RPMB
hardware partition with common protocol and frame layout.
The RPMB partition `cannot` be accessed via standard block layer,
but by a set of specific commands:

WRITE, READ, GET_WRITE_COUNTER, and PROGRAM_KEY.

The commands and the data are embedded within :c:type:`rpmb_frame <rpmb_frame>`.

An RPMB partition provides authenticated and replay protected access,
hence it is suitable as a secure storage.

In-kernel API
-------------
The RPMB layer aims to provide in-kernel API for Trusted Execution
Environment (TEE) devices that are capable to securely compute the block
frame signature. In case a TEE device wish to store a replay protected
data, it creates an RPMB frame with requested data and computes HMAC of
the frame, then it requests the storage device via RPMB layer to store
the data.

The layer provides APIs, for :c:func:`rpmb_seq_cmd()` for issuing sequence
of raw RPMB protocol frames, which is close to the functionality provided
by emmc multi ioctl interface.

.. c:function:: int rpmb_cmd_seq(struct rpmb_dev *rdev, u8 target, struct rpmb_cmd *cmds, u32 ncmds);


A TEE driver can claim the RPMB interface, for example, via
:c:func:`class_interface_register`:

.. code-block:: c

        struct class_interface tee_rpmb_intf = {
                .class      = &rpmb_class;
                .add_dev    = rpmb_add_device;
                .remove_dev = rpmb_remove_device;
        }
        class_interface_register(&tee_rpmb_intf);


RPMB device registeration
----------------------------

A storage device registers its RPMB hardware (eMMC) partition or RPMB
W-LUN (UFS) with the RPMB layer :c:func:`rpmb_dev_register` providing
an implementation for :c:func:`rpmb_seq_cmd()` handler. The interface
enables sending sequence of RPMB standard frames.

.. code-block:: c

        struct rpmb_ops mmc_rpmb_dev_ops = {
                .cmd_seq = mmc_blk_rpmb_cmd_seq,
                .type = RPMB_TYPE_EMMC,
                ...
        }
        rpmb_dev_register(disk_to_dev(part_md->disk), &mmc_rpmb_dev_ops);


User space API
--------------

A parallel user space API is provided via /dev/rpmbX character
device with two IOCTL commands.
- First ``RPMB_IOC_VER_CMD``, return driver protocol version,
- second ``RPMB_IOC_CAP_CMD`` return capability structure,
- last ``RPMB_IOC_SEQ_CMD`` where the whole RPMB sequence, and
  including ``RESULT_READ`` is supplied by the caller.
https://android.googlesource.com/trusty/app/storage/

.. code-block:: c

        struct rpmb_ioc_req_cmd ireq;
        int ret;

        ireq.req_type = RPMB_WRITE_DATA;
        rpmb_ioc_cmd_set(ireq.icmd, RPMB_F_WRITE, frames_in, cnt_in);
        rpmb_ioc_cmd_set(ireq.ocmd, 0, frames_out, cnt_out);

        ret = ioctl(fd, RPMB_IOC_REQ_CMD, &ireq);


API
---
.. kernel-doc:: include/linux/rpmb.h

.. kernel-doc:: drivers/char/rpmb/core.c

.. kernel-doc:: include/uapi/linux/rpmb.h

.. kernel-doc:: drivers/char/rpmb/cdev.c
