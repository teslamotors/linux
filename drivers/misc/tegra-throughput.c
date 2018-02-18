/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/throughput_ioctl.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/notifier.h>
#include <linux/tegra-throughput.h>
#include <mach/dc.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tegra_throughput.h>

#define DEFAULT_SYNC_RATE 60000 /* 60 Hz */

static unsigned int target_frame_time;
static ktime_t last_flip;
static spinlock_t lock;

#define EMA_PERIOD  8

static int frame_time_sum_init = 1;
static long frame_time_sum; /* used for fps EMA */
static u64 framecount;
static u64 frame_timestamp;

static struct work_struct work;
static int throughput_hint;

static int sync_rate;
static int throughput_active_app_count;

BLOCKING_NOTIFIER_HEAD(throughput_notifier_list);
EXPORT_SYMBOL(throughput_notifier_list);

static void set_throughput_hint(struct work_struct *work)
{
	trace_tegra_throughput_hint(throughput_hint);

	/* notify throughput hint clients here */
	blocking_notifier_call_chain(&throughput_notifier_list,
				     throughput_hint, NULL);
}

int tegra_throughput_get_hint(void)
{
	return throughput_hint;
}
EXPORT_SYMBOL(tegra_throughput_get_hint);

static void throughput_flip_callback(void)
{
	long timediff;
	ktime_t now;

	now = ktime_get();
	spin_lock(&lock);
	framecount++;
	frame_timestamp = ktime_to_us(now);
	spin_unlock(&lock);

	if (last_flip.tv64 != 0) {
		timediff = (long) ktime_us_delta(now, last_flip);
		trace_tegra_throughput_flip(timediff);

		if (timediff <= 0) {
			pr_debug("%s: flips %lld nsec apart\n",
				__func__, now.tv64 - last_flip.tv64);
			last_flip = now;
			return;
		}

		if (target_frame_time == 0)
			return;

		throughput_hint =
			((int) target_frame_time * 1000) / timediff;

		trace_tegra_throughput_gen_hint(throughput_hint, timediff);

		/* only deliver throughput hints when a single app is active */
		if (throughput_active_app_count == 1 && !work_pending(&work))
			schedule_work(&work);

		if (frame_time_sum_init) {
			frame_time_sum = timediff * EMA_PERIOD;
			frame_time_sum_init = 0;
		} else {
			int t = (frame_time_sum / EMA_PERIOD) *
				(EMA_PERIOD - 1);
			frame_time_sum = t + timediff;
		}
	}

	last_flip = now;
}

static void reset_target_frame_time(void)
{
	if (sync_rate == 0) {
		sync_rate = tegra_dc_get_panel_sync_rate();

		if (sync_rate == 0)
			sync_rate = DEFAULT_SYNC_RATE;
	}

	target_frame_time = (unsigned int) (1000000000 / sync_rate);

	pr_debug("%s: panel sync rate %d, target frame time %u\n",
		__func__, sync_rate, target_frame_time);
}

static int throughput_open(struct inode *inode, struct file *file)
{
	spin_lock(&lock);

	throughput_active_app_count++;
	frame_time_sum_init = 1;

	trace_tegra_throughput_open(throughput_active_app_count);

	spin_unlock(&lock);


	pr_debug("throughput_open node %p file %p\n", inode, file);

	return 0;
}

static int throughput_release(struct inode *inode, struct file *file)
{
	spin_lock(&lock);

	throughput_active_app_count--;
	frame_time_sum_init = 1;

	trace_tegra_throughput_release(throughput_active_app_count);

	if (throughput_active_app_count == 1)
		reset_target_frame_time();

	spin_unlock(&lock);

	pr_debug("throughput_release node %p file %p\n", inode, file);

	return 0;
}

static int throughput_set_target_fps(unsigned long arg)
{
	pr_debug("%s: target fps %lu requested\n", __func__, arg);
	trace_tegra_throughput_set_target_fps(arg);

	if (throughput_active_app_count != 1) {
		pr_debug("%s: %d active apps, disabling fps usage\n",
			__func__, throughput_active_app_count);
		return 0;
	}

	if (arg == 0)
		reset_target_frame_time();
	else
		target_frame_time = (unsigned int) (1000000 / arg);

	return 0;
}

static long
throughput_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	if ((_IOC_TYPE(cmd) != TEGRA_THROUGHPUT_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > TEGRA_THROUGHPUT_IOCTL_MAXNR))
		return -EFAULT;

	switch (cmd) {
	case TEGRA_THROUGHPUT_IOCTL_TARGET_FPS:
		pr_debug("%s: TEGRA_THROUGHPUT_IOCTL_TARGET_FPS %lu\n",
			__func__, arg);
		err = throughput_set_target_fps(arg);
		break;

	default:
		err = -ENOTTY;
	}

	return err;
}

static const struct file_operations throughput_user_fops = {
	.owner			= THIS_MODULE,
	.open			= throughput_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= throughput_ioctl,
#endif
	.release		= throughput_release,
	.unlocked_ioctl		= throughput_ioctl,
};

#define TEGRA_THROUGHPUT_MINOR 1

static struct miscdevice throughput_miscdev = {
	.minor = TEGRA_THROUGHPUT_MINOR,
	.name  = "tegra-throughput",
	.fops  = &throughput_user_fops,
	.mode  = 0666,
};

static ssize_t fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int frame_time_avg;
	ktime_t now;
	long timediff;
	int fps = 0;

	if (frame_time_sum_init)
		goto DONE;

	now = ktime_get();
	timediff = (long) ktime_us_delta(now, last_flip);
	if (timediff > 1000000)
		goto DONE;

	frame_time_avg = frame_time_sum / EMA_PERIOD;
	fps = frame_time_avg > 0 ? 1000000 / frame_time_avg : 0;

DONE:
	return snprintf(buf, PAGE_SIZE, "%d\n", fps);
}

static DEVICE_ATTR_RO(fps);

static ssize_t framecount_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u64 fcount;
	u64 fstamp;

	spin_lock(&lock);
	fcount = framecount;
	fstamp = frame_timestamp;
	spin_unlock(&lock);

	return snprintf(buf, PAGE_SIZE, "%llu %llu\n",
		       fcount, fstamp);
}

static DEVICE_ATTR_RO(framecount);

static int __init throughput_init_miscdev(void)
{
	int ret_md, ret_f1, ret_f2;

	pr_debug("%s: initializing\n", __func__);

	spin_lock_init(&lock);
	INIT_WORK(&work, set_throughput_hint);

	ret_md = misc_register(&throughput_miscdev);
	ret_f1 = device_create_file(throughput_miscdev.this_device,
				    &dev_attr_fps);
	ret_f2 = device_create_file(throughput_miscdev.this_device,
				    &dev_attr_framecount);

	if (ret_md == 0 && ret_f1 == 0 && ret_f2 == 0) {
		tegra_dc_set_flip_callback(throughput_flip_callback);

		return 0;
	}

	if (ret_f2 == 0)
		device_remove_file(throughput_miscdev.this_device,
				   &dev_attr_framecount);
	if (ret_f1 == 0)
		device_remove_file(throughput_miscdev.this_device,
				   &dev_attr_fps);
	if (ret_md == 0)
		misc_deregister(&throughput_miscdev);

	if (ret_md)
		return ret_md;
	if (ret_f1)
		return ret_f1;
	return ret_f2;
}

module_init(throughput_init_miscdev);

static void __exit throughput_exit_miscdev(void)
{
	pr_debug("%s: exiting\n", __func__);

	tegra_dc_unset_flip_callback();

	cancel_work_sync(&work);

	device_remove_file(throughput_miscdev.this_device,
			   &dev_attr_framecount);
	device_remove_file(throughput_miscdev.this_device,
			   &dev_attr_fps);

	misc_deregister(&throughput_miscdev);
}

module_exit(throughput_exit_miscdev);

