/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

#include <uapi/linux/intel-ipu4-psys.h>

static long native_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -ENOTTY;

	if (file->f_op->unlocked_ioctl)
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);

	return ret;
}

struct intel_ipu4_psys_buffer32 {
	uint64_t len;
	compat_uptr_t userptr;
	int fd;
	uint32_t flags;
	uint32_t reserved[2];
} __packed;

struct intel_ipu4_psys_command32 {
	uint64_t issue_id;
	uint32_t id;
	uint32_t priority;
	compat_uptr_t pg_manifest;
	compat_uptr_t buffers;
	int pg;
	uint32_t pg_manifest_size;
	uint32_t bufcount;
	uint32_t min_psys_freq;
	uint32_t reserved[2];
} __packed;

struct intel_ipu4_psys_manifest32 {
	uint32_t index;
	uint32_t size;
	compat_uptr_t manifest;
	uint32_t reserved[5];
} __packed;

static int get_intel_ipu4_psys_command32(struct intel_ipu4_psys_command *kp,
				struct intel_ipu4_psys_command32 __user *up)
{
	compat_uptr_t pgm, bufs;

	if (!access_ok(VERIFY_READ, up,
		sizeof(struct intel_ipu4_psys_buffer32)) ||
		get_user(kp->issue_id, &up->issue_id) ||
		get_user(kp->id, &up->id) ||
		get_user(kp->priority, &up->priority) ||
		get_user(pgm, &up->pg_manifest) ||
		get_user(bufs, &up->buffers) ||
		get_user(kp->pg, &up->pg) ||
		get_user(kp->pg_manifest_size, &up->pg_manifest_size) ||
		get_user(kp->bufcount, &up->bufcount) ||
		get_user(kp->min_psys_freq, &up->min_psys_freq))
			return -EFAULT;

	kp->pg_manifest = compat_ptr(pgm);
	kp->buffers = compat_ptr(bufs);

	return 0;
}

static int get_intel_ipu4_psys_buffer32(struct intel_ipu4_psys_buffer *kp,
				struct intel_ipu4_psys_buffer32 __user *up)
{
	compat_uptr_t ptr;

	if (!access_ok(VERIFY_READ, up,
		sizeof(struct intel_ipu4_psys_buffer32)) ||
		get_user(kp->len, &up->len) ||
		get_user(ptr, &up->userptr) ||
		get_user(kp->fd, &up->fd) ||
		get_user(kp->flags, &up->flags))
			return -EFAULT;

	kp->userptr = compat_ptr(ptr);

	return 0;
}

static int put_intel_ipu4_psys_buffer32(struct intel_ipu4_psys_buffer *kp,
				     struct intel_ipu4_psys_buffer32 __user *up)
{
	compat_uptr_t ptr = (u32)((unsigned long)kp->userptr);

	if (!access_ok(VERIFY_WRITE, up,
		       sizeof(struct intel_ipu4_psys_buffer32)) ||
	    put_user(kp->len, &up->len) ||
	    put_user(ptr, &up->userptr) ||
	    put_user(kp->fd, &up->fd) ||
	    put_user(kp->flags, &up->flags))
		return -EFAULT;

	return 0;
}

static int get_intel_ipu4_psys_manifest32(struct intel_ipu4_psys_manifest *kp,
				struct intel_ipu4_psys_manifest32 __user *up)
{
	compat_uptr_t ptr;

	if (!access_ok(VERIFY_READ, up,
		sizeof(struct intel_ipu4_psys_manifest32)) ||
		get_user(kp->index, &up->index) ||
		get_user(kp->size, &up->size) ||
		get_user(ptr, &up->manifest))
			return -EFAULT;

	kp->manifest = compat_ptr(ptr);

	return 0;
}

static int put_intel_ipu4_psys_manifest32(struct intel_ipu4_psys_manifest *kp,
				struct intel_ipu4_psys_manifest32 __user *up)
{
	compat_uptr_t ptr = (u32)((unsigned long)kp->manifest);

	if (!access_ok(VERIFY_WRITE, up,
		       sizeof(struct intel_ipu4_psys_manifest32)) ||
	    put_user(kp->index, &up->index) ||
	    put_user(kp->size, &up->size) ||
	    put_user(ptr, &up->manifest))
		return -EFAULT;

	return 0;
}

#define INTEL_IPU4_IOC_GETBUF32 _IOWR('A', 4, struct intel_ipu4_psys_buffer32)
#define INTEL_IPU4_IOC_PUTBUF32 _IOWR('A', 5, struct intel_ipu4_psys_buffer32)
#define INTEL_IPU4_IOC_QCMD32 _IOWR('A', 6, struct intel_ipu4_psys_command32)
#define INTEL_IPU4_IOC_CMD_CANCEL32 \
	_IOWR('A', 8, struct intel_ipu4_psys_command32)
#define INTEL_IPU4_IOC_GET_MANIFEST32 \
	_IOWR('A', 9, struct intel_ipu4_psys_manifest32)

long intel_ipu4_psys_compat_ioctl32(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	union {
		struct intel_ipu4_psys_buffer buf;
		struct intel_ipu4_psys_command cmd;
		struct intel_ipu4_psys_event ev;
		struct intel_ipu4_psys_manifest m;
	} karg;
	int compatible_arg = 1;
	int err = 0;
	void __user *up = compat_ptr(arg);

	switch (cmd) {
	case INTEL_IPU4_IOC_GETBUF32:
		cmd = INTEL_IPU4_IOC_GETBUF;
		break;
	case INTEL_IPU4_IOC_PUTBUF32:
		cmd = INTEL_IPU4_IOC_PUTBUF;
		break;
	case INTEL_IPU4_IOC_QCMD32:
		cmd = INTEL_IPU4_IOC_QCMD;
		break;
	case INTEL_IPU4_IOC_GET_MANIFEST32:
		cmd = INTEL_IPU4_IOC_GET_MANIFEST;
		break;
	}

	switch (cmd) {
	case INTEL_IPU4_IOC_GETBUF:
	case INTEL_IPU4_IOC_PUTBUF:
		err = get_intel_ipu4_psys_buffer32(&karg.buf, up);
		compatible_arg = 0;
		break;
	case INTEL_IPU4_IOC_QCMD:
		err = get_intel_ipu4_psys_command32(&karg.cmd, up);
		compatible_arg = 0;
		break;
	case INTEL_IPU4_IOC_GET_MANIFEST:
		err = get_intel_ipu4_psys_manifest32(&karg.m, up);
		compatible_arg = 0;
		break;
	}
	if (err)
		return err;

	if (compatible_arg) {
		err = native_ioctl(file, cmd, (unsigned long)up);
	} else {
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		err = native_ioctl(file, cmd, (unsigned long)&karg);
		set_fs(old_fs);
	}

	if (err)
		return err;

	switch (cmd) {
	case INTEL_IPU4_IOC_GETBUF:
		err = put_intel_ipu4_psys_buffer32(&karg.buf, up);
		break;
	case INTEL_IPU4_IOC_GET_MANIFEST:
		err = put_intel_ipu4_psys_manifest32(&karg.m, up);
		break;
	}
	return err;
}
EXPORT_SYMBOL_GPL(intel_ipu4_psys_compat_ioctl32);
