.. SPDX-License-Identifier: GPL-2.0-only
====
XPin
====

XPin: Like LoadPin but for eXecutable code.

What is XPin?
=============

XPin is a Linux Security Module that ensures all memory pages mapped with
``PROT_EXEC`` must be backed by a file on a filesystem backed by a block
device protected by dm-verity. This allows systems that boot off of a
dm-verity backed rootfs (and may mount other dm-verity volumes) to enforce
that all user-mode code is verified without needing to individually
code-sign every file.

How to enable XPin?
===================

The LSM is selectable at build-time with ``CONFIG_SECURITY_XPIN``. Both
logging and enforcement can be configured at build-time (via setting
``CONFIG_SECURITY_XPIN_LOGGING`` / ``CONFIG_SECURITY_XPIN_ENFORCE``), at
boot-time (by setting ``xpin.logging`` / ``xpin.enforce``), or at runtime
(by writing to ``/sys/kernel/security/xpin/{logging,enforce}``). It also
has an option for enabling "lockdown mode", which is intended to be used
by production-mode devices. This can be enabled by writing to
``/sys/kernel/security/xpin/lockdown``. By default, logging, enforcement,
and lockdown mode are all disabled.

XPin works by adding hooks to operations such as ``mmap``, ``mprotect``,
and ``execve``, among others. For ``mmap`` and ``mprotect``, if the operation
is attempting to set ``PROT_EXEC``, XPin will only allow the operation if the
page comes from a dm-verity backed file. Also, it will block the request if
it's attempting to set both ``PROT_EXEC`` and ``PROT_WRITE``.

XPin has robust support for exceptions, meant to allow specific programs
(like a web brower's Javascript JIT) to run unsigned code. Exceptions can be
imported by adding a file to a dm-verity protected volume which starts with
the line "``## XPin Exceptions List``", followed by a list of exception
descriptions on the subsequent lines. Each exception description line starts
with zero or more attributes, followed by the absolute path to the file for
which the exception applies.

The following exception attributes are supported:

* ``lazy``: The file path may not exist yet, but the exception will be
  applied before or during the first time the specified program is executed.
  The exception will only be applied if the file is protected by dm-verity,
  as this feature is intended to support lazy-mounted volumes.

* ``deny``: The mentioned file is not actually granted an exception. This is
  mainly useful in conjunction with other exception attributes.

* ``jit``: The exception is considered a JIT-only exception. It allows a
  program to generate and execute code in-memory, but it does not allow running
  unsigned programs somewhere like ``/tmp`` or ``/var``.

* ``inherit``: The exception will inherit across execve.

* ``uninherit``: When the mentioned program is executed, its exception flags
  will override any inheritable flags that the task already has. This is not the
  default behavior. Normally, tasks with exceptions that execute other programs
  that have exceptions will keep the inheritable exception flags rather than
  those from the file.

* ``nonelf``: The mentioned program is allowed to map the contents of non-ELF
  files (still protected by dm-verity) as code. Normally, XPin only allows a
  process to map the contents of a file as code if it's a dm-verity file that
  starts with the ELF magic bytes. This attribute lifts the file format
  restriction. An example use case of this is for Wine to run code from Windows
  executables and DLLs.

* ``quiet``: Violations will not be logged. This is useful in conjunction with
  the ``deny`` attribute for processes that attempt to do some action that XPin
  would block but still run normally if that action is denied.

* ``+``: This sets the ``AT_SECURE`` attribute when the program is executed.

To load a properly formatted exceptions list file into the XPin LSM, a process
with ``CAP_MAC_ADMIN`` can invoke the ``XPIN_IOC_ADD_EXCEPTIONS`` ioctl on
``/sys/kernel/security/xpin/control``, providing as an argument an open file
descriptor with read access to the exceptions list file.

Exceptions that have been loaded into the kernel can be listed by running
``cat /sys/kernel/security/xpin/exceptions``. Note that this feature is mainly
useful for development, and as such it is not available in lockdown mode.

XPin also adds ``/proc/<pid>/attr/xpin/current``, which shows the task's
exception status and attributes in a short format.
