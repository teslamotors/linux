/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __HYPER_DMABUF_IOCTL_H__
#define __HYPER_DMABUF_IOCTL_H__

typedef int (*hyper_dmabuf_ioctl_t)(struct file *filp, void *data);

struct hyper_dmabuf_ioctl_desc {
	unsigned int cmd;
	int flags;
	hyper_dmabuf_ioctl_t func;
	const char *name;
};

#define HYPER_DMABUF_IOCTL_DEF(ioctl, _func, _flags)	\
	[_IOC_NR(ioctl)] = {				\
			.cmd = ioctl,			\
			.func = _func,			\
			.flags = _flags,		\
			.name = #ioctl			\
	}

long hyper_dmabuf_ioctl(struct file *filp,
			unsigned int cmd, unsigned long param);

int hyper_dmabuf_unexport_ioctl(struct file *filp, void *data);

#endif //__HYPER_DMABUF_IOCTL_H__
