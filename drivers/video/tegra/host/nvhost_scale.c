/*
 * Tegra Graphics Host Unit clock scaling
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/pm_qos.h>
#include <trace/events/nvhost.h>
#include <linux/uaccess.h>

#if defined(CONFIG_ARCH_TEGRA_210_SOC)
#include <soc/tegra/tegra-dvfs.h>
#include <linux/clk-provider.h>
#endif

#include <governor.h>

#include "dev.h"
#include "debug.h"
#include "chip_support.h"
#include "nvhost_acm.h"
#include "nvhost_scale.h"
#include "host1x/host1x_actmon.h"

static ssize_t nvhost_scale_load_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	u32 busy_time;
	ssize_t res;

	actmon_op().read_avg_norm(profile->actmon[ENGINE_ACTMON],
				&busy_time);
	res = snprintf(buf, PAGE_SIZE, "%u\n", busy_time);

	return res;
}

static DEVICE_ATTR(load, S_IRUGO, nvhost_scale_load_show, NULL);

/*
 * nvhost_scale_make_freq_table(profile)
 *
 * This function initialises the frequency table for the given device profile
 */

static int tegra_update_freq_table(struct clk *c,
				   struct nvhost_device_data *pdata,
				   int *num_freqs)
{
	unsigned long max_freq =  clk_round_rate(c, UINT_MAX);
	unsigned long min_freq =  clk_round_rate(c, 0);
	unsigned long clk_step, found_rate, last_rate, rate;
	int cnt = 0;

	/* check if clk scaling is available */
	if (min_freq == 0 || max_freq == 0)
		return 0;

	/* initial default min freq */
	pdata->freqs[0] = min_freq;
	last_rate = min_freq;
	cnt++;

	/* pick clk_step with assumption that all frequency table gets full.
	 * If it is too small, we will not get any high frequencies to the table
	 */
	clk_step =  (max_freq + min_freq) / NVHOST_MODULE_MAX_FREQS;
	/* tune to get next freq in steps */
	for (rate = min_freq + clk_step; rate <= max_freq; rate += clk_step) {
		found_rate = clk_round_rate(c, rate);
		if (found_rate > last_rate) {
			pdata->freqs[cnt] = found_rate;
			last_rate = found_rate;
			cnt++;
		}
		if (cnt == NVHOST_MODULE_MAX_FREQS)
			break;
	}

	/* fill the remaining table with max_freq */
	for (; cnt < NVHOST_MODULE_MAX_FREQS; cnt++)
		pdata->freqs[cnt] = max_freq;

	*num_freqs = cnt;

	return 0;
}

static int nvhost_scale_make_freq_table(struct nvhost_device_profile *profile)
{
	unsigned long *freqs = NULL;
	int num_freqs = 0, err = 0, low_end_cnt = 0;
	unsigned long max_freq =  clk_round_rate(profile->clk, UINT_MAX);
	unsigned long min_freq =  clk_round_rate(profile->clk, 0);
	struct nvhost_device_data *pdata = platform_get_drvdata(profile->pdev);

	if (!tegra_platform_is_silicon())
		goto exit;

#if defined(CONFIG_PLATFORM_TEGRA) && !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	err = tegra_dvfs_get_freqs(clk_get_parent(profile->clk),
				   &freqs, &num_freqs);
	if (err)
		return err;
#endif
	if (!freqs)
		err = tegra_update_freq_table(profile->clk, pdata, &num_freqs);

	if (pdata->freqs[0])
		freqs = pdata->freqs;

	/* check for duplicate frequencies at higher end */
	while (((num_freqs >= 2) &&
		(freqs[num_freqs - 2] == freqs[num_freqs - 1])) ||
	       (num_freqs && (max_freq < freqs[num_freqs - 1])))
		num_freqs--;

	/* check low end */
	while (((num_freqs >= 2) && (freqs[low_end_cnt] == freqs[low_end_cnt + 1])) ||
	       (num_freqs && (freqs[low_end_cnt] < min_freq))) {
		low_end_cnt++;
		num_freqs--;
	}
	freqs += low_end_cnt;

exit:
	if (!num_freqs)
		dev_warn(&profile->pdev->dev, "dvfs table had no applicable frequencies!\n");

	profile->devfreq_profile.freq_table = (unsigned long *)freqs;
	profile->devfreq_profile.max_state = num_freqs;

	return err;
}

/*
 * nvhost_scale_target(dev, *freq, flags)
 *
 * This function scales the clock
 */

static int nvhost_scale_target(struct device *dev, unsigned long *freq,
			       u32 flags)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

#ifdef CONFIG_ARCH_TEGRA
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
	if(!__clk_get_enable_count(profile->clk)) {
#else
	if (!tegra_is_clk_enabled(profile->clk)) {
#endif
		*freq = profile->devfreq_profile.freq_table[0];
		return 0;
	}
#endif

#if defined(CONFIG_PLATFORM_TEGRA) && !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	*freq = clk_round_rate(clk_get_parent(profile->clk), *freq);
#else
	*freq = clk_round_rate(profile->clk, *freq);
#endif
	if (clk_get_rate(profile->clk) == *freq)
		return 0;

	nvhost_module_set_rate(profile->pdev, pdata->power_manager,
				*freq, 0, NVHOST_CLOCK);
	if (pdata->scaling_post_cb)
		pdata->scaling_post_cb(profile, *freq);

	*freq = clk_get_rate(profile->clk);

	return 0;
}

/*
 * update_load_estimate_actmon(profile)
 *
 * Update load estimate using hardware actmon. The actmon value is normalised
 * based on the time it was asked last time.
 */

static void update_load_estimate_actmon(struct nvhost_device_profile *profile)
{
	ktime_t t;
	unsigned long dt;
	u32 busy_time;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time = dt;
	profile->last_event_time = t;
	actmon_op().read_avg_norm(profile->actmon[ENGINE_ACTMON],
				&busy_time);
	profile->dev_stat.busy_time = (busy_time * dt) / 1000;
}

/*
 * nvhost_scale_notify(pdev, busy)
 *
 * Calling this function informs that the device is idling (..or busy). This
 * data is used to estimate the current load
 */

static void nvhost_scale_notify(struct platform_device *pdev, bool busy)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	struct devfreq *devfreq = pdata->power_manager;

	/* Is the device profile initialised? */
	if (!profile)
		return;

	if (nvhost_debug_trace_actmon) {
		u32 load;

		actmon_op().read_avg_norm(profile->actmon[ENGINE_ACTMON],
					&load);
		if (load)
			trace_nvhost_scale_notify(pdev->name, load, busy);
	}

	/* If defreq is disabled, set the freq to max or min */
	if (!devfreq) {
		unsigned long freq = busy ? UINT_MAX : 0;
		nvhost_scale_target(&pdev->dev, &freq, 0);
		return;
	}

	mutex_lock(&devfreq->lock);
	profile->dev_stat.busy = busy;
#if defined(CONFIG_PM_DEVFREQ)
	update_devfreq(devfreq);
#endif
	mutex_unlock(&devfreq->lock);
}

void nvhost_scale_notify_idle(struct platform_device *pdev)
{
	nvhost_scale_notify(pdev, false);

}

void nvhost_scale_notify_busy(struct platform_device *pdev)
{
	nvhost_scale_notify(pdev, true);
}

/*
 * nvhost_scale_get_dev_status(dev, *stat)
 *
 * This function queries the current device status.
 */

static int nvhost_scale_get_dev_status(struct device *dev,
					      struct devfreq_dev_status *stat)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	/* Make sure there are correct values for the current frequency */
	profile->dev_stat.current_frequency = clk_get_rate(profile->clk);

	if (profile->actmon[ENGINE_ACTMON])
		update_load_estimate_actmon(profile);

	/* Copy the contents of the current device status */
	*stat = profile->dev_stat;

	/* Finally, clear out the local values */
	profile->dev_stat.total_time = 0;
	profile->dev_stat.busy_time = 0;

	return 0;
}

/*
 * nvhost_scale_set_low_wmark(dev, threshold)
 *
 * This functions sets the high watermark threshold.
 *
 */

static int nvhost_scale_set_low_wmark(struct device *dev,
				      unsigned int threshold)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	actmon_op().set_low_wmark(profile->actmon[ENGINE_ACTMON],
			threshold);

	return 0;
}

/*
 * nvhost_scale_set_high_wmark(dev, threshold)
 *
 * This functions sets the high watermark threshold.
 *
 */

static int nvhost_scale_set_high_wmark(struct device *dev,
				       unsigned int threshold)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	actmon_op().set_high_wmark(profile->actmon[ENGINE_ACTMON],
			threshold);

	return 0;
}

/*
 * nvhost_scale_init(pdev)
 */

void nvhost_scale_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile;
	int err, i;
	struct host1x_actmon *actmon;

	if (pdata->power_profile)
		return;

	profile = kzalloc(sizeof(struct nvhost_device_profile), GFP_KERNEL);
	if (!profile)
		return;
	pdata->power_profile = profile;
	profile->pdev = pdev;
	profile->clk = pdata->clk[0];
	profile->dev_stat.busy = false;
	profile->num_actmons = nvhost_get_host(pdev)->info.nb_actmons;

	/* Create frequency table */
	err = nvhost_scale_make_freq_table(profile);
	if (err || !profile->devfreq_profile.max_state)
		goto err_get_freqs;

	err = nvhost_module_busy(nvhost_get_host(pdev)->dev);
	if (err) {
		nvhost_warn(&pdev->dev, "failed to power on host1x.");
		goto err_module_busy;
	}

	/* Initialize actmon */
	if (pdata->actmon_enabled) {

		if (device_create_file(&pdev->dev,
		    &dev_attr_load))
			goto err_create_sysfs_entry;

		profile->actmon = kzalloc(profile->num_actmons *
					sizeof(struct host1x_actmon *),
					  GFP_KERNEL);
		if (!profile->actmon)
			goto err_allocate_actmons;

		for (i = 0; i < profile->num_actmons; i++) {
			profile->actmon[i] = kzalloc(
					sizeof(struct host1x_actmon),
					GFP_KERNEL);

			if (!profile->actmon[i])
				goto err_allocate_actmon;

			actmon = profile->actmon[i];
			actmon->host = nvhost_get_host(pdev);
			actmon->pdev = pdev;

			actmon->type = i;

			actmon->regs = actmon_op().get_actmon_regs(actmon);
			if (!actmon->regs) {
				nvhost_err(&pdev->dev,
					"can't access actmon regs");
				goto err_get_actmon_regs;
			}

			actmon_op().init(actmon);
			nvhost_actmon_debug_init(actmon, pdata->debugfs);
			actmon_op().deinit(actmon);
		}
	}

	/* initialize devfreq if governor is set and actmon enabled */
	if (pdata->actmon_enabled && pdata->devfreq_governor) {
		struct devfreq *devfreq;

		profile->devfreq_profile.initial_freq =
			profile->devfreq_profile.freq_table[0];
		profile->devfreq_profile.target = nvhost_scale_target;
		profile->devfreq_profile.get_dev_status =
			nvhost_scale_get_dev_status;
		profile->devfreq_profile.set_low_wmark =
			nvhost_scale_set_low_wmark;
		profile->devfreq_profile.set_high_wmark =
			nvhost_scale_set_high_wmark;

		devfreq = devfreq_add_device(&pdev->dev,
					&profile->devfreq_profile,
					pdata->devfreq_governor, NULL);

		if (IS_ERR(devfreq))
			devfreq = NULL;

		pdata->power_manager = devfreq;
		if (nvhost_module_add_client(pdev, devfreq)) {
			nvhost_err(&pdev->dev,
				"failed to register devfreq as acm client");
		}
	}

	nvhost_module_idle(nvhost_get_host(pdev)->dev);

	return;
err_get_actmon_regs:
err_allocate_actmon:
	kfree(profile->actmon);
err_allocate_actmons:
	nvhost_module_idle(nvhost_get_host(pdev)->dev);
err_module_busy:
err_get_freqs:
	device_remove_file(&pdev->dev, &dev_attr_load);
err_create_sysfs_entry:
	kfree(pdata->power_profile);
	pdata->power_profile = NULL;
}

/*
 * nvhost_scale_deinit(dev)
 *
 * Stop scaling for the given device.
 */

void nvhost_scale_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (!profile)
		return;

	/* Remove devfreq from acm client list */
	nvhost_module_remove_client(pdev, pdata->power_manager);

	if (pdata->power_manager)
		devfreq_remove_device(pdata->power_manager);

	if (pdata->actmon_enabled)
		device_remove_file(&pdev->dev, &dev_attr_load);

	kfree(profile->devfreq_profile.freq_table);
	kfree(profile->actmon);
	kfree(profile);
	pdata->power_profile = NULL;
}

void nvhost_scale_actmon_irq(struct platform_device *pdev, int type)
{
	struct nvhost_device_data *engine_pdata =
		platform_get_drvdata(pdev);
	struct devfreq *df = engine_pdata->power_manager;

	devfreq_watermark_event(df, type);
}

/*
 * nvhost_scale_hw_init(dev)
 *
 * Initialize hardware portion of the device
 */

int nvhost_scale_hw_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	int i;

	if (!(profile && profile->actmon))
		return 0;

	/* initialize actmon */
	for (i = 0; i < profile->num_actmons; i++)
		actmon_op().init(profile->actmon[i]);

	/* load engine specific actmon settings */
	if (pdata->mamask_addr)
		host1x_writel(pdev, pdata->mamask_addr,
			      pdata->mamask_val);
	if (pdata->borps_addr)
		host1x_writel(pdev, pdata->borps_addr,
			      pdata->borps_val);

	return 0;
}

/*
 * nvhost_scale_hw_deinit(dev)
 *
 * Deinitialize the hw partition related to scaling
 */

void nvhost_scale_hw_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	int i;

	if (profile && profile->actmon) {
		if (pdata->mamask_addr)
			host1x_writel(pdev, pdata->mamask_addr, 0x0);

		if (pdata->borps_addr)
			host1x_writel(pdev, pdata->borps_addr, 0x0);

		for (i = 0; i < profile->num_actmons; i++)
			actmon_op().deinit(profile->actmon[i]);
	}
}

/* activity monitor */
static int actmon_count_norm_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_count_norm(actmon, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_count_norm_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_count_norm_show, inode->i_private);
}

static const struct file_operations actmon_count_norm_fops = {
	.open		= actmon_count_norm_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_count_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_count(actmon, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_count_show, inode->i_private);
}

static const struct file_operations actmon_count_fops = {
	.open		= actmon_count_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_avg_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_avg(actmon, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_avg_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_avg_show, inode->i_private);
}

static const struct file_operations actmon_avg_fops = {
	.open		= actmon_avg_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_avg_norm_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_avg_norm(actmon, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_avg_norm_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_avg_norm_show, inode->i_private);
}

static const struct file_operations actmon_avg_norm_fops = {
	.open		= actmon_avg_norm_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_sample_period_norm_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	long period = actmon_op().get_sample_period_norm(actmon);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_sample_period_norm_open(struct inode *inode,
						struct file *file)
{
	return single_open(file, actmon_sample_period_norm_show,
		inode->i_private);
}

static ssize_t actmon_sample_period_norm_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct host1x_actmon *actmon = s->private;
	char buffer[40];
	int buf_size;
	unsigned long period;

	memset(buffer, 0, sizeof(buffer));
	buf_size = min(count, (sizeof(buffer)-1));

	if (copy_from_user(buffer, user_buf, buf_size))
		return -EFAULT;

	if (strlen(buffer) > buf_size)
		return -EFAULT;

	if (kstrtoul(buffer, 10, &period))
		return -EINVAL;

	actmon_op().set_sample_period_norm(actmon, period);

	return count;
}

static const struct file_operations actmon_sample_period_norm_fops = {
	.open		= actmon_sample_period_norm_open,
	.read		= seq_read,
	.write          = actmon_sample_period_norm_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_sample_period_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	long period = actmon_op().get_sample_period(actmon);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_sample_period_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_sample_period_show, inode->i_private);
}

static const struct file_operations actmon_sample_period_fops = {
	.open		= actmon_sample_period_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_k_show(struct seq_file *s, void *unused)
{
	struct host1x_actmon *actmon = s->private;
	long period = actmon_op().get_k(actmon);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_k_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_k_show, inode->i_private);
}

static ssize_t actmon_k_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct host1x_actmon *actmon = s->private;
	char buffer[40];
	int buf_size;
	unsigned long k;

	memset(buffer, 0, sizeof(buffer));
	buf_size = min(count, (sizeof(buffer)-1));

	if (copy_from_user(buffer, user_buf, buf_size))
		return -EFAULT;

	if (strlen(buffer) > buf_size)
		return -EFAULT;

	if (kstrtoul(buffer, 10, &k))
		return -EINVAL;

	actmon_op().set_k(actmon, k);

	return count;
}

static const struct file_operations actmon_k_fops = {
	.open		= actmon_k_open,
	.read		= seq_read,
	.write          = actmon_k_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nvhost_actmon_debug_init(struct host1x_actmon *actmon,
				     struct dentry *de)
{
	BUG_ON(!actmon);
	debugfs_create_file("actmon_k", S_IRUGO, de,
			actmon, &actmon_k_fops);
	debugfs_create_file("actmon_sample_period", S_IRUGO, de,
			actmon, &actmon_sample_period_fops);
	debugfs_create_file("actmon_sample_period_norm", S_IRUGO, de,
			actmon, &actmon_sample_period_norm_fops);
	debugfs_create_file("actmon_avg_norm", S_IRUGO, de,
			actmon, &actmon_avg_norm_fops);
	debugfs_create_file("actmon_avg", S_IRUGO, de,
			actmon, &actmon_avg_fops);
	debugfs_create_file("actmon_count", S_IRUGO, de,
			actmon, &actmon_count_fops);
	debugfs_create_file("actmon_count_norm", S_IRUGO, de,
			actmon, &actmon_count_norm_fops);
	/* additional hardware specific debugfs nodes */
	if (actmon_op().debug_init)
		actmon_op().debug_init(actmon, de);

}

