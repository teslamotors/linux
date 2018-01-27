/*
*
* CWP Trace module
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
* Contact Information: Yan, Like <like.yan@intel.com>
*
* BSD LICENSE
*
* Copyright (c) 2017 Intel Corporation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.
*   * Neither the name of Intel Corporation nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
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
* Like Yan <like.yan@intel.com>
*
*/

#define pr_fmt(fmt) "CWPTrace: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include "sbuf.h"


#define TRACE_SBUF_SIZE		(4 * 1024 * 1024)
#define TRACE_ELEMENT_SIZE	32 /* byte */
#define TRACE_ELEMENT_NUM	((TRACE_SBUF_SIZE - SBUF_HEAD_SIZE) /	\
				TRACE_ELEMENT_SIZE)

#define foreach_cpu(cpu, cpu_num)					\
	for ((cpu) = 0; (cpu) < (cpu_num); (cpu)++)

#define MAX_NR_CPUS	4
/* actual physical cpu number, initialized by module init */
static int pcpu_num;

static int nr_cpus = MAX_NR_CPUS;
module_param(nr_cpus, int, S_IRUSR | S_IWUSR);

static atomic_t open_cnt[MAX_NR_CPUS];
static shared_buf_t *sbuf_per_cpu[MAX_NR_CPUS];

static inline int get_id_from_devname(struct file *filep)
{
	uint32_t cpuid;
	int err;
	char id_str[16];
	struct miscdevice *dev = filep->private_data;

	strncpy(id_str, (void *)dev->name + sizeof("cwp_trace_") - 1, 16);
	id_str[15] = '\0';
	err = kstrtoul(&id_str[0], 10, (unsigned long *)&cpuid);

	if (err)
		return err;

	if (cpuid >= pcpu_num) {
		pr_err("%s, failed to get cpuid, cpuid %d\n",
			__func__, cpuid);
		return -1;
	}

	return cpuid;
}

/************************************************************************
 *
 * file_operations functions
 *
 ***********************************************************************/
static int cwp_trace_open(struct inode *inode, struct file *filep)
{
	int cpuid = get_id_from_devname(filep);

	pr_debug("%s, cpu %d\n", __func__, cpuid);
	if (cpuid < 0)
		return -ENXIO;

	/* More than one reader at the same time could get data messed up */
	if (atomic_read(&open_cnt[cpuid]))
		return -EBUSY;

	atomic_inc(&open_cnt[cpuid]);

	return 0;
}

static int cwp_trace_release(struct inode *inode, struct file *filep)
{
	int cpuid = get_id_from_devname(filep);

	pr_debug("%s, cpu %d\n", __func__, cpuid);
	if (cpuid < 0)
		return -ENXIO;

	atomic_dec(&open_cnt[cpuid]);

	return 0;
}

static int cwp_trace_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int cpuid = get_id_from_devname(filep);
	phys_addr_t paddr;

	pr_debug("%s, cpu %d\n", __func__, cpuid);
	if (cpuid < 0)
		return -ENXIO;

	BUG_ON(!virt_addr_valid(sbuf_per_cpu[cpuid]));
	paddr = virt_to_phys(sbuf_per_cpu[cpuid]);

	if (remap_pfn_range(vma, vma->vm_start,
				paddr >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot)) {
		pr_err("Failed to mmap sbuf for cpu%d\n", cpuid);
		return -EAGAIN;
	}

	return 0;
}

static const struct file_operations cwp_trace_fops = {
	.owner  = THIS_MODULE,
	.open   = cwp_trace_open,
	.release = cwp_trace_release,
	.mmap   = cwp_trace_mmap,
};

static struct miscdevice cwp_trace_dev0 = {
	.name   = "cwp_trace_0",
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &cwp_trace_fops,
};

static struct miscdevice cwp_trace_dev1 = {
	.name   = "cwp_trace_1",
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &cwp_trace_fops,
};

static struct miscdevice cwp_trace_dev2 = {
	.name   = "cwp_trace_2",
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &cwp_trace_fops,
};

static struct miscdevice cwp_trace_dev3 = {
	.name   = "cwp_trace_3",
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &cwp_trace_fops,
};

static struct miscdevice *cwp_trace_devs[4] = {
	&cwp_trace_dev0,
	&cwp_trace_dev1,
	&cwp_trace_dev2,
	&cwp_trace_dev3,
};

/*
 * cwp_trace_init()
 */
static int __init cwp_trace_init(void)
{
	int ret = 0;
	int i, cpu;

	/* TBD: we could get the native cpu number by hypercall later */
	pr_info("%s, cpu_num %d\n", __func__, nr_cpus);
	if (nr_cpus > MAX_NR_CPUS) {
		pr_err("nr_cpus %d exceed MAX_NR_CPUS %d !\n",
			nr_cpus, MAX_NR_CPUS);
		return -EINVAL;
	}
	pcpu_num = nr_cpus;

	foreach_cpu(cpu, pcpu_num) {
		/* allocate shared_buf */
		sbuf_per_cpu[cpu] = sbuf_allocate(TRACE_ELEMENT_NUM,
							TRACE_ELEMENT_SIZE);
		if (!sbuf_per_cpu[cpu]) {
			pr_err("Failed alloc SBuf, cpuid %d\n", cpu);
			ret = -ENOMEM;
			goto out_free;
		}
	}

	foreach_cpu(cpu, pcpu_num) {
		ret = sbuf_share_setup(cpu, 0, sbuf_per_cpu[cpu]);
		if (ret < 0) {
			pr_err("Failed to setup SBuf, cpuid %d\n", cpu);
			goto out_sbuf;
		}
	}

	foreach_cpu(cpu, pcpu_num) {
		ret = misc_register(cwp_trace_devs[cpu]);
		if (ret < 0) {
			pr_err("Failed to register cwp_trace_%d, errno %d\n",
				cpu, ret);
			goto out_dereg;
		}
	}

	return ret;

out_dereg:
	for (i = --cpu; i >= 0; i--)
		misc_deregister(cwp_trace_devs[i]);
	cpu = pcpu_num;

out_sbuf:
	for (i = --cpu; i >= 0; i--)
		sbuf_share_setup(i, 0, NULL);
	cpu = pcpu_num;

out_free:
	for (i = --cpu; i >= 0; i--)
		sbuf_free(sbuf_per_cpu[i]);

	return ret;
}

/*
 * cwp_trace_exit()
 */
static void __exit cwp_trace_exit(void)
{
	int cpu;

	pr_info("%s, cpu_num %d\n", __func__, pcpu_num);

	foreach_cpu(cpu, pcpu_num) {
		/* deregister devices */
		misc_deregister(cwp_trace_devs[cpu]);

		/* set sbuf pointer to NULL in HV */
		sbuf_share_setup(cpu, 0, NULL);

		/* free sbuf, sbuf_per_cpu[cpu] should be set NULL */
		sbuf_free(sbuf_per_cpu[cpu]);
	}
}

module_init(cwp_trace_init);
module_exit(cwp_trace_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corp., http://www.intel.com");
MODULE_DESCRIPTION("Driver for the Intel CWP Hypervisor Trace");
MODULE_VERSION("0.1");
