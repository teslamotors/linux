/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>

#include <governor.h>

enum watermark_type {
	NO_WATERMARK_EVENT = 0,
	HIGH_WATERMARK_EVENT = 1,
	LOW_WATERMARK_EVENT = 2
};

struct wmark_gov_info {
	/* probed from the devfreq */
	unsigned long		*freqlist;
	int			freq_count;

	/* algorithm parameters */
	unsigned int		p_high_wmark;
	unsigned int		p_low_wmark;

	/* dynamically changing data */
	enum watermark_type	event;
	unsigned long		last_request;

	/* common data */
	struct devfreq		*df;
	struct platform_device	*pdev;
	struct dentry		*debugdir;
};

static unsigned long freqlist_up(struct wmark_gov_info *wmarkinfo,
			unsigned long curr_freq)
{
	int i, pos;

	for (i = 0; i < wmarkinfo->freq_count; i++)
		if (wmarkinfo->freqlist[i] > curr_freq)
			break;

	pos = min(wmarkinfo->freq_count - 1, i);

	return wmarkinfo->freqlist[pos];
}

static unsigned long freqlist_down(struct wmark_gov_info *wmarkinfo,
			unsigned long curr_freq)
{
	int i, pos;

	for (i = wmarkinfo->freq_count - 1; i >= 0; i--)
		if (wmarkinfo->freqlist[i] < curr_freq)
			break;

	pos = max(0, i);
	return wmarkinfo->freqlist[pos];
}

static int devfreq_watermark_target_freq(struct devfreq *df,
			unsigned long *freq)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	struct devfreq_dev_status dev_stat;
	int err;

	err = df->profile->get_dev_status(df->dev.parent, &dev_stat);
	if (err < 0)
		return err;

	switch (wmarkinfo->event) {
	case HIGH_WATERMARK_EVENT:
		*freq = freqlist_up(wmarkinfo, dev_stat.current_frequency);

		/* always enable low watermark */
		df->profile->set_low_wmark(df->dev.parent,
					   wmarkinfo->p_low_wmark);

		/* disable high watermark if no change */
		if (*freq == wmarkinfo->last_request)
			df->profile->set_high_wmark(df->dev.parent, 1000);
		break;
	case LOW_WATERMARK_EVENT:
		*freq = freqlist_down(wmarkinfo, dev_stat.current_frequency);

		/* always enable high watermark */
		df->profile->set_high_wmark(df->dev.parent,
					    wmarkinfo->p_high_wmark);

		/* disable low watermark if no change */
		if (*freq == wmarkinfo->last_request)
			df->profile->set_low_wmark(df->dev.parent, 0);
		break;
	default:
		break;
	}

	/* Mark that you handled event */
	wmarkinfo->event = NO_WATERMARK_EVENT;
	wmarkinfo->last_request = *freq;

	return 0;
}

static void devfreq_watermark_debug_start(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	struct dentry *f;
	char dirname[128];

	snprintf(dirname, sizeof(dirname), "%s_scaling",
		to_platform_device(df->dev.parent)->name);

	if (!wmarkinfo)
		return;

	wmarkinfo->debugdir = debugfs_create_dir(dirname, NULL);
	if (!wmarkinfo->debugdir) {
		pr_warn("cannot create debugfs directory\n");
		return;
	}

#define CREATE_DBG_FILE(fname) \
	do {\
		f = debugfs_create_u32(#fname, S_IRUGO | S_IWUSR, \
			wmarkinfo->debugdir, &wmarkinfo->p_##fname); \
		if (NULL == f) { \
			pr_warn("cannot create debug entry " #fname "\n"); \
			return; \
		} \
	} while (0)

	CREATE_DBG_FILE(low_wmark);
	CREATE_DBG_FILE(high_wmark);
#undef CREATE_DBG_FILE

}

static void devfreq_watermark_debug_stop(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo = df->data;
	debugfs_remove_recursive(wmarkinfo->debugdir);
}

static int devfreq_watermark_start(struct devfreq *df)
{
	struct wmark_gov_info *wmarkinfo;
	struct platform_device *pdev = to_platform_device(df->dev.parent);

	if (!df->profile->freq_table) {
		dev_err(&pdev->dev, "Frequency table missing\n");
		return -EINVAL;
	}

	wmarkinfo = kzalloc(sizeof(struct wmark_gov_info), GFP_KERNEL);
	if (!wmarkinfo)
		return -ENOMEM;

	df->data = (void *)wmarkinfo;
	wmarkinfo->freqlist = df->profile->freq_table;
	wmarkinfo->freq_count = df->profile->max_state;
	wmarkinfo->event = NO_WATERMARK_EVENT;
	wmarkinfo->df = df;
	wmarkinfo->pdev = pdev;
	wmarkinfo->p_low_wmark = 100;
	wmarkinfo->p_high_wmark = 600;

	devfreq_watermark_debug_start(df);

	return 0;
}

static int devfreq_watermark_event_handler(struct devfreq *df,
			unsigned int event, void *wmark_type)
{
	int ret = 0;
	struct wmark_gov_info *wmarkinfo = df->data;
	enum watermark_type *type = wmark_type;

	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_watermark_start(df);
		wmarkinfo = df->data;
		if (df->profile->set_low_wmark)
			df->profile->set_low_wmark(df->dev.parent,
						   wmarkinfo->p_low_wmark);
		if (df->profile->set_high_wmark)
			df->profile->set_high_wmark(df->dev.parent,
						    wmarkinfo->p_high_wmark);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_watermark_debug_stop(df);
		break;
	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(df);
		break;

	case DEVFREQ_GOV_RESUME:
		if (df->profile->set_low_wmark)
			df->profile->set_low_wmark(df->dev.parent,
						   wmarkinfo->p_low_wmark);
		if (df->profile->set_high_wmark)
			df->profile->set_high_wmark(df->dev.parent,
						    wmarkinfo->p_high_wmark);
		devfreq_monitor_resume(df);
		break;

	case DEVFREQ_GOV_WMARK:
		/* Set watermark interrupt type */
		wmarkinfo->event = *type;

		mutex_lock(&df->lock);
		update_devfreq(df);
		mutex_unlock(&df->lock);

		break;

	default:
		break;
	}

	return ret;
}

static struct devfreq_governor devfreq_watermark = {
	.name = "wmark_simple",
	.get_target_freq = devfreq_watermark_target_freq,
	.event_handler = devfreq_watermark_event_handler,
};


static int __init devfreq_watermark_init(void)
{
	return devfreq_add_governor(&devfreq_watermark);
}

static void __exit devfreq_watermark_exit(void)
{
	devfreq_remove_governor(&devfreq_watermark);
}

rootfs_initcall(devfreq_watermark_init);
module_exit(devfreq_watermark_exit);
