/*
 * Copyright (C) 2014-2016 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/cache.h>
#include <linux/pstore.h>
#include <asm/barrier.h>
#include <asm/page.h>
#include <asm/io.h>
#include "internal.h"

struct rtrace_state {
	int enabled;
	int initialized;
	unsigned long upper_limit;
	unsigned long lower_limit;
};

static struct rtrace_state rtrace;

static int notrace pstore_rtrace_enabled(void)
{
	return rtrace.initialized && rtrace.enabled && (!oops_in_progress);
}

noinline void notrace pstore_rtrace_call(enum rtrace_event_type log_type,
					void *data, long val)
{
	unsigned long flags;
	struct pstore_rtrace_record rec = {};

	if (!pstore_rtrace_enabled())
		return;

	local_irq_save(flags);

	rec.raddr = virt_to_phys_in_hw(data);
	if (rec.raddr >= rtrace.lower_limit &&
				rec.raddr <= rtrace.upper_limit) {
		rec.cpu = raw_smp_processor_id();
		rec.event = log_type;
		rec.caller = (unsigned long)__builtin_return_address(0);
		rec.value = val;
		psinfo->write_buf(PSTORE_TYPE_RTRACE, 0, NULL, 0, (void *)&rec,
				  0, sizeof(rec), psinfo);
	}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(pstore_rtrace_call);

static DEFINE_MUTEX(pstore_rtrace_lock);

static ssize_t pstore_rtrace_knob_write(struct file *f, const char __user *buf,
					size_t count, loff_t *ppos)
{
	u8 on;
	ssize_t ret;

	ret = kstrtou8_from_user(buf, count, 2, &on);
	if (ret)
		return ret;

	mutex_lock(&pstore_rtrace_lock);

	if (!on ^ rtrace.enabled)
		goto out;

	rtrace.enabled = on;
out:
	ret = count;
	mutex_unlock(&pstore_rtrace_lock);

	return ret;
}

static ssize_t pstore_rtrace_knob_read(struct file *f, char __user *buf,
				       size_t count, loff_t *ppos)
{
	char val[] = { '0' + rtrace.enabled, '\n' };

	return simple_read_from_buffer(buf, count, ppos, val, sizeof(val));
}

static ssize_t rtrace_upper_limit_write(struct file *f, const char __user *buf,
					size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = kstrtoul_from_user(buf, count, 0, &rtrace.upper_limit);
	return ret ? ret : count;
}

static ssize_t rtrace_upper_limit_read(struct file *f, char __user *buf,
				       size_t count, loff_t *ppos)
{
	char val[24];
	ssize_t size = scnprintf(val, sizeof(val), "0x%016lx\n",
						rtrace.upper_limit);

	return simple_read_from_buffer(buf, count, ppos, val, size);
}

static ssize_t rtrace_lower_limit_write(struct file *f, const char __user *buf,
					size_t count, loff_t *ppos)
{
	ssize_t ret;

	ret = kstrtoul_from_user(buf, count, 0, &rtrace.lower_limit);
	return ret ? ret : count;
}

static ssize_t rtrace_lower_limit_read(struct file *f, char __user *buf,
				       size_t count, loff_t *ppos)
{
	char val[24];
	ssize_t size = scnprintf(val, sizeof(val), "0x%016lx\n",
						rtrace.lower_limit);

	return simple_read_from_buffer(buf, count, ppos, val, size);
}

static const struct file_operations pstore_knob_fops = {
	.open	= simple_open,
	.read	= pstore_rtrace_knob_read,
	.write	= pstore_rtrace_knob_write,
};

static const struct file_operations rtrace_upper_limit_fops = {
	.open	= simple_open,
	.read	= rtrace_upper_limit_read,
	.write	= rtrace_upper_limit_write,
};

static const struct file_operations rtrace_lower_limit_fops = {
	.open	= simple_open,
	.read	= rtrace_lower_limit_read,
	.write	= rtrace_lower_limit_write,
};

void pstore_register_rtrace(void)
{
	struct dentry *file;

	if (!psinfo->write_buf || !psinfo->debugfs_dir)
		return;

	file = debugfs_create_file("record_rtrace", 0600, psinfo->debugfs_dir,
				   NULL, &pstore_knob_fops);
	if (!file)
		pr_err("%s: unable to create record_rtrace file\n", __func__);

	file = debugfs_create_file("rtrace_upper_limit", 0600,
			 psinfo->debugfs_dir, NULL, &rtrace_upper_limit_fops);
	if (!file)
		pr_err("%s: unable to create rtrace_upper_limit file\n",
								    __func__);

	file = debugfs_create_file("rtrace_lower_limit", 0600,
			 psinfo->debugfs_dir, NULL, &rtrace_lower_limit_fops);
	if (!file)
		pr_err("%s: unable to create rtrace_lower_limit file\n",
								    __func__);

	rtrace.upper_limit = ULONG_MAX;
	rtrace.lower_limit = 0;
	rtrace.initialized = 1;
#ifdef CONFIG_PSTORE_RTRACE_ENABLE_AT_STARTUP
	rtrace.enabled = 1;
#endif
}
