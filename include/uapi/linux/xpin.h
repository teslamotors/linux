/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * XPin: eXecute Pin, like LoadPin but for memory pages with PROT_EXEC
 *
 * XPin is a Linux Security Module (LSM) which enforces that all executable
 * memory pages must be backed by a file on a dm-verity protected volume.
 *
 * Copyright (C) 2023 Tesla Motors. All rights reserved.
 *
 * Author: Kevin Colley <kcolley@tesla.com>
 */

#ifndef _UAPI_LINUX_XPIN_H
#define _UAPI_LINUX_XPIN_H

#include <linux/types.h>


/* These ioctls should be issued on /sys/kernel/security/xpin/control */

#define XPIN_IOC_MAGIC 'X'

struct xpin_check_file {
	__u32 size;

	/* Input */
	__u32 fd;

	/* Output */
	__s32 result;

	/* Extra info */

	/* Bits 0-3 */
	__u32 caller_has_exception : 1;
	__u32 is_verity : 1;
	__u32 is_shmem : 1;
	__u32 is_memfd : 1;

	/* Bits 4-7 */
	__u32 is_elf : 1;
	__u32 is_exception : 1;
	__u32 is_deny : 1;
	__u32 is_jit_only : 1;

	/* Bits 8-11 */
	__u32 is_inheritable : 1;
	__u32 allows_non_elf : 1;
	__u32 is_quiet : 1;
	__u32 is_secure_exec : 1;

	/* Bit 12 */
	__u32 is_uninherit : 1;

	__u32 _unused : 32 - 13;
};

/*
 * XPIN_IOC_ADD_EXCEPTIONS - Provide the file descriptor to an XPin exceptions
 * list file.
 */
#define XPIN_IOC_ADD_EXCEPTIONS _IOW(XPIN_IOC_MAGIC, 0x01, unsigned int)

/*
 * XPIN_IOC_CHECK_FILE - Check how XPin would treat attempts to map code from
 * the passed file path.
 */
#define XPIN_IOC_CHECK_FILE _IOWR(XPIN_IOC_MAGIC, 0x02, struct xpin_check_file)

/*
 * XPIN_IOC_CLEAR_EXCEPTIONS - Clear one or all XPin exceptions on files.
 * This will fail if XPin's lockdown mode is enabled.
 */
#define XPIN_IOC_CLEAR_EXCEPTIONS _IOW(XPIN_IOC_MAGIC, 0x03, int)

#endif /* _UAPI_LINUX_XPIN_H */
