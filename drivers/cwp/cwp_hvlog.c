/*
 * CWP Hypervisor logmsg
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information: Li Fei <fei1.li@intel.com>
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Li Fei <fei1.li@intel.com>
 *
 */
#define pr_fmt(fmt) "CWP HVLog: " fmt

#include <linux/memblock.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/miscdevice.h>

#include "sbuf.h"

#define LOG_ENTRY_SIZE		80
#define PCPU_NRS		4

#define foreach_cpu(cpu, cpu_num)					\
	for ((cpu) = 0; (cpu) < (cpu_num); (cpu)++)

#define foreach_hvlog_type(idx, hvlog_type)				\
	for ((idx) = 0; (idx) < (hvlog_type); (idx)++)

enum sbuf_hvlog_index {
	CWP_CURRNET_HVLOG = 0,
	CWP_LAST_HVLOG,
	CWP_HVLOG_TYPE
};

struct cwp_hvlog {
	struct miscdevice miscdev;
	shared_buf_t *sbuf;
	atomic_t open_cnt;
	int pcpu_num;
};

static unsigned long long hvlog_buf_size;
static unsigned long long hvlog_buf_base;

static int __init early_hvlog(char *p)
{
	int ret;

	pr_debug("%s(%s)\n", __func__, p);
	hvlog_buf_size = memparse(p, &p);
	if (*p != '@')
		return 0;
	hvlog_buf_base = memparse(p + 1, &p);

	if (!!hvlog_buf_base && !!hvlog_buf_size) {
		ret = memblock_reserve(hvlog_buf_base, hvlog_buf_size);
		if (ret) {
			pr_err("%s: Error reserving hvlog memblock\n",
				__func__);
			hvlog_buf_base = 0;
			hvlog_buf_size = 0;
			return ret;
		}
	}

	return 0;
}
early_param("hvlog", early_hvlog);


static inline shared_buf_t *hvlog_mark_unread(shared_buf_t *sbuf)
{
	/* sbuf must point to valid data.
	 * clear the lowest bit in the magic to indicate that
	 * the sbuf point to the last boot valid data, we should
	 * read it later.
	 */
	if (sbuf != NULL)
		sbuf->magic &= ~1;

	return sbuf;
}

static int cwp_hvlog_open(struct inode *inode, struct file *filp)
{
	struct cwp_hvlog *cwp_hvlog;

	cwp_hvlog = container_of(filp->private_data,
				struct cwp_hvlog, miscdev);
	pr_debug("%s, %s\n", __func__, cwp_hvlog->miscdev.name);

	if (cwp_hvlog->pcpu_num >= PCPU_NRS) {
		pr_err("%s, invalid pcpu_num: %d\n",
				__func__, cwp_hvlog->pcpu_num);
		return -EIO;
	}

	/* More than one reader at the same time could get data messed up */
	if (atomic_cmpxchg(&cwp_hvlog->open_cnt, 0, 1) != 0)
		return -EBUSY;

	filp->private_data = cwp_hvlog;

	return 0;
}

static int cwp_hvlog_release(struct inode *inode, struct file *filp)
{
	struct cwp_hvlog *cwp_hvlog;

	cwp_hvlog = filp->private_data;

	pr_debug("%s, %s\n", __func__, cwp_hvlog->miscdev.name);

	if (cwp_hvlog->pcpu_num >= PCPU_NRS) {
		pr_err("%s, invalid pcpu_num: %d\n",
				__func__, cwp_hvlog->pcpu_num);
		return -EIO;
	}

	atomic_dec(&cwp_hvlog->open_cnt);
	filp->private_data = NULL;

	return 0;
}

static ssize_t cwp_hvlog_read(struct file *filp, char __user *buf,
				size_t count, loff_t *offset)
{
	char data[LOG_ENTRY_SIZE];
	struct cwp_hvlog *cwp_hvlog;
	int ret;

	cwp_hvlog = (struct cwp_hvlog *)filp->private_data;

	pr_debug("%s, %s\n", __func__, cwp_hvlog->miscdev.name);

	if (cwp_hvlog->pcpu_num >= PCPU_NRS) {
		pr_err("%s, invalid pcpu_num: %d\n",
				__func__, cwp_hvlog->pcpu_num);
		return -EIO;
	}

	if (cwp_hvlog->sbuf != NULL) {
		ret = sbuf_get(cwp_hvlog->sbuf, (uint8_t *)&data);
		if (ret > 0) {
			if (copy_to_user(buf, &data, ret))
				return -EFAULT;
		}

		return ret;
	}

	return 0;
}

static const struct file_operations cwp_hvlog_fops = {
	.owner  = THIS_MODULE,
	.open   = cwp_hvlog_open,
	.release = cwp_hvlog_release,
	.read = cwp_hvlog_read,
};

static struct cwp_hvlog cwp_hvlog_devs[CWP_HVLOG_TYPE][PCPU_NRS] = {
	[CWP_CURRNET_HVLOG] = {
		{
			.miscdev = {
				.name   = "cwp_hvlog_cur_0",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 0,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_cur_1",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 1,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_cur_2",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 2,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_cur_3",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 3,
		},
	},
	[CWP_LAST_HVLOG] = {
		{
			.miscdev = {
				.name   = "cwp_hvlog_last_0",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 0,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_last_1",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 1,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_last_2",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 2,
		},
		{
			.miscdev = {
				.name   = "cwp_hvlog_last_3",
				.minor  = MISC_DYNAMIC_MINOR,
				.fops   = &cwp_hvlog_fops,
			},
			.pcpu_num = 3,
		},
	}
};

static int __init cwp_hvlog_init(void)
{
	int ret = 0;
	int i, j, idx;
	uint32_t pcpu_id;
	uint64_t logbuf_base0;
	uint64_t logbuf_base1;
	uint64_t logbuf_size;
	uint32_t ele_size;
	uint32_t ele_num;
	uint32_t size;
	bool sbuf_constructed = false;

	shared_buf_t *sbuf0[PCPU_NRS];
	shared_buf_t *sbuf1[PCPU_NRS];

	pr_info("%s\n", __func__);
	if (!hvlog_buf_base || !hvlog_buf_size) {
		pr_warn("no fixed memory reserve for hvlog.\n");
		return 0;
	}

	logbuf_base0 = hvlog_buf_base;
	logbuf_size = (hvlog_buf_size >> 1);
	logbuf_base1 = hvlog_buf_base + logbuf_size;

	size = (logbuf_size / PCPU_NRS);
	ele_size = LOG_ENTRY_SIZE;
	ele_num = (size - SBUF_HEAD_SIZE) / ele_size;

	foreach_cpu(pcpu_id, PCPU_NRS) {
		sbuf0[pcpu_id] = sbuf_check_valid(ele_num, ele_size,
					logbuf_base0 + size * pcpu_id);
		sbuf1[pcpu_id] = sbuf_check_valid(ele_num, ele_size,
					logbuf_base1 + size * pcpu_id);
	}

	foreach_cpu(pcpu_id, PCPU_NRS) {
		if (sbuf0[pcpu_id] == NULL)
			continue;

		foreach_cpu(pcpu_id, PCPU_NRS) {
			cwp_hvlog_devs[CWP_LAST_HVLOG][pcpu_id].sbuf =
					hvlog_mark_unread(sbuf0[pcpu_id]);
			cwp_hvlog_devs[CWP_CURRNET_HVLOG][pcpu_id].sbuf =
				sbuf_construct(ele_num, ele_size,
					logbuf_base1 + size * pcpu_id);
		}
		sbuf_constructed = true;
	}

	if (sbuf_constructed == false) {
		foreach_cpu(pcpu_id, PCPU_NRS) {
			if (sbuf1[pcpu_id] == NULL)
				continue;

			foreach_cpu(pcpu_id, PCPU_NRS) {
				cwp_hvlog_devs[CWP_LAST_HVLOG][pcpu_id].sbuf =
					hvlog_mark_unread(sbuf1[pcpu_id]);
			}
		}
		foreach_cpu(pcpu_id, PCPU_NRS) {
			cwp_hvlog_devs[CWP_CURRNET_HVLOG][pcpu_id].sbuf =
				sbuf_construct(ele_num, ele_size,
					logbuf_base0 + size * pcpu_id);
		}
		sbuf_constructed = true;
	}

	idx = CWP_CURRNET_HVLOG;
	{
		foreach_cpu(pcpu_id, PCPU_NRS) {
			ret = sbuf_share_setup(pcpu_id, CWP_HVLOG,
					cwp_hvlog_devs[idx][pcpu_id].sbuf);
			if (ret < 0) {
				pr_err("Failed to setup %s, errno %d\n",
				cwp_hvlog_devs[idx][pcpu_id].miscdev.name, ret);
				goto setup_err;
			}
		}
	}

	foreach_hvlog_type(idx, CWP_HVLOG_TYPE) {
		foreach_cpu(pcpu_id, PCPU_NRS) {
			atomic_set(&cwp_hvlog_devs[idx][pcpu_id].open_cnt, 0);

			ret = misc_register(
					&cwp_hvlog_devs[idx][pcpu_id].miscdev);
			if (ret < 0) {
				pr_err("Failed to register %s, errno %d\n",
				cwp_hvlog_devs[idx][pcpu_id].miscdev.name, ret);
				goto reg_err;
			}
		}
	}

	return 0;

reg_err:
	foreach_hvlog_type(i, idx) {
		foreach_cpu(j, PCPU_NRS) {
			misc_deregister(&cwp_hvlog_devs[i][j].miscdev);
		}
	}

	foreach_cpu(j, pcpu_id) {
		misc_deregister(&cwp_hvlog_devs[idx][j].miscdev);
	}

	pcpu_id = PCPU_NRS;
setup_err:
	idx = CWP_CURRNET_HVLOG;
	{
		foreach_cpu(j, pcpu_id) {
			sbuf_share_setup(j, CWP_HVLOG, 0);
			sbuf_deconstruct(cwp_hvlog_devs[idx][j].sbuf);
		}
	}

	return ret;
}

static void __exit cwp_hvlog_exit(void)
{
	int idx;
	uint32_t pcpu_id;

	pr_info("%s\n", __func__);

	foreach_hvlog_type(idx, CWP_HVLOG_TYPE) {
		foreach_cpu(pcpu_id, PCPU_NRS) {
			misc_deregister(&cwp_hvlog_devs[idx][pcpu_id].miscdev);
		}
	}

	idx = CWP_CURRNET_HVLOG;
	{
		foreach_cpu(pcpu_id, PCPU_NRS) {
			sbuf_share_setup(pcpu_id, CWP_HVLOG, 0);
			sbuf_deconstruct(cwp_hvlog_devs[idx][pcpu_id].sbuf);
		}
	}
}

module_init(cwp_hvlog_init);
module_exit(cwp_hvlog_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corp., http://www.intel.com");
MODULE_DESCRIPTION("Driver for the Intel CWP Hypervisor Logmsg");
MODULE_VERSION("0.1");
