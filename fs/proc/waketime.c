// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>
#include <linux/suspend.h>

static int waketime_proc_show(struct seq_file *m, void *v)
{
	const ktime_t waketime_ktime = ktime_sub(ktime_get(), dpm_get_last_time_suspend_end());
	const struct timespec64 waketime = ns_to_timespec64(waketime_ktime);

	seq_printf(m, "%lu.%02lu\n",
			(unsigned long) waketime.tv_sec,
			(waketime.tv_nsec / (NSEC_PER_SEC / 100)));
	return 0;
}

static ssize_t waketime_proc_reset(struct file *file, const char __user *buffer,
				  size_t count, loff_t *pos)
{
	dpm_set_last_time_suspend_end();
	return count;
}

static int waketime_proc_open(struct inode *inode, struct file *file)
{
       return single_open(file, waketime_proc_show, NULL);
}

static const struct file_operations waketime_proc_fops = {
       .open           = waketime_proc_open,
       .read           = seq_read,
       .llseek         = seq_lseek,
       .release        = single_release,
       .write          = waketime_proc_reset,
};

static int __init proc_waketime_init(void)
{
	proc_create("waketime", 0644, NULL, &waketime_proc_fops);
	return 0;
}
fs_initcall(proc_waketime_init);

