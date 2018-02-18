/*
 * emc_dfs.c
 *
 * Emc dynamic frequency scaling due to APE
 *
 * Copyright (C) 2014-2016, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/tegra_nvadsp.h>
#include <linux/tick.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/debugfs.h>

#include "dev.h"

/* Register offsets */
#define ABRIDGE_STATS_READ_0		0x04
#define ABRIDGE_STATS_WRITE_0		0x0c
#define ABRIDGE_STATS_CLEAR_0		0x1b
#define ABRIDGE_STATS_HI_0FFSET		0x04

/* Sample Period in usecs */
#define DEFAULT_SAMPLE_PERIOD	500000
#define INT_SHIFT		32
#define make64(hi, low) ((((u64)hi) << INT_SHIFT) | (low))
#define SCALING_DIVIDER 2
#define BOOST_DOWN_COUNT 2
#define DEFAULT_BOOST_UP_THRESHOLD 2000000;
#define DEFAULT_BOOST_STEP 2

struct emc_dfs_info {
	void __iomem *abridge_base;
	struct timer_list cnt_timer;

	u64 rd_cnt;
	u64 wr_cnt;
	bool enable;
	u64 avg_cnt;

	unsigned long timer_rate;
	ktime_t prev_time;

	u32 dn_count;
	u32 boost_dn_count;

	u64 boost_up_threshold;
	u8 boost_step;

	struct work_struct clk_set_work;
	unsigned long cur_freq;
	bool speed_change_flag;
	unsigned long max_freq;

	struct clk *emcclk;
};

static struct emc_dfs_info global_emc_info;
static struct emc_dfs_info *einfo;
static struct task_struct *speedchange_task;
static spinlock_t speedchange_lock;

static u64 read64(u32 offset)
{
	u32 low;
	u32 hi;

	low = readl(einfo->abridge_base + offset);
	hi = readl(einfo->abridge_base + (offset + ABRIDGE_STATS_HI_0FFSET));
	return make64(hi, low);
}
static unsigned long count_to_emcfreq(void)
{
	unsigned long tfreq = 0;

	if (!einfo->avg_cnt) {
		if (einfo->dn_count >= einfo->boost_dn_count) {
			tfreq = einfo->cur_freq / SCALING_DIVIDER;
			einfo->dn_count = 0;
		} else
			einfo->dn_count++;
	} else if (einfo->avg_cnt >= einfo->boost_up_threshold) {
		if (einfo->boost_step)
			tfreq = einfo->cur_freq * einfo->boost_step;
	}

	pr_debug("%s:avg_cnt: %llu  current freq(kHz): %lu target freq(kHz): %lu\n",
		__func__, einfo->avg_cnt, einfo->cur_freq, tfreq);

	return tfreq;
}

static int clk_work(void *data)
{
	int ret;

	if (einfo->emcclk && einfo->speed_change_flag && einfo->cur_freq) {
		ret = clk_set_rate(einfo->emcclk, einfo->cur_freq * 1000);
		if (ret) {
			pr_err("failed to set ape.emc freq:%d\n", ret);
			BUG_ON(ret);
		}
		einfo->cur_freq = clk_get_rate(einfo->emcclk) / 1000;
		pr_info("ape.emc: setting emc clk: %lu\n", einfo->cur_freq);
	}

	mod_timer(&einfo->cnt_timer,
		  jiffies + usecs_to_jiffies(einfo->timer_rate));
	return 0;
}
static void emc_dfs_timer(unsigned long data)
{
	u64 cur_cnt;
	u64 delta_cnt;
	u64 prev_cnt;
	u64 delta_time;
	ktime_t now;
	unsigned long target_freq;
	unsigned long flags;

	spin_lock_irqsave(&speedchange_lock, flags);

	/* Return if emc dfs is disabled */
	if (!einfo->enable) {
		spin_unlock_irqrestore(&speedchange_lock, flags);
		return;
	}

	prev_cnt = einfo->rd_cnt + einfo->wr_cnt;

	einfo->rd_cnt = read64((u32)ABRIDGE_STATS_READ_0);
	einfo->wr_cnt = read64((u32)ABRIDGE_STATS_WRITE_0);
	pr_debug("einfo->rd_cnt: %llu einfo->wr_cnt: %llu\n",
		einfo->rd_cnt, einfo->wr_cnt);

	cur_cnt = einfo->rd_cnt + einfo->wr_cnt;
	delta_cnt = cur_cnt - prev_cnt;

	now = ktime_get();

	delta_time = ktime_to_ns(ktime_sub(now, einfo->prev_time));
	if (!delta_time) {
		pr_err("%s: time interval to calculate emc scaling is zero\n",
		__func__);
		spin_unlock_irqrestore(&speedchange_lock, flags);
		goto exit;
	}

	einfo->prev_time = now;
	einfo->avg_cnt = delta_cnt / delta_time;

	/* if 0: no scaling is required */
	target_freq = count_to_emcfreq();
	if (!target_freq) {
		einfo->speed_change_flag = false;
	} else {
		einfo->cur_freq = target_freq;
		einfo->speed_change_flag = true;
	}

	spin_unlock_irqrestore(&speedchange_lock, flags);
	pr_info("einfo->avg_cnt: %llu delta_cnt: %llu delta_time %llu emc_freq:%lu\n",
		einfo->avg_cnt, delta_cnt, delta_time, einfo->cur_freq);

exit:
	wake_up_process(speedchange_task);
}

static void emc_dfs_enable(void)
{
	einfo->rd_cnt = read64((u32)ABRIDGE_STATS_READ_0);
	einfo->wr_cnt = read64((u32)ABRIDGE_STATS_WRITE_0);

	einfo->prev_time = ktime_get();
	mod_timer(&einfo->cnt_timer, jiffies + 2);
}
static void emc_dfs_disable(void)
{
	einfo->rd_cnt = read64((u32)ABRIDGE_STATS_READ_0);
	einfo->wr_cnt = read64((u32)ABRIDGE_STATS_WRITE_0);

	del_timer_sync(&einfo->cnt_timer);
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *emc_dfs_root;

#define RW_MODE (S_IWUSR | S_IRUSR)
#define RO_MODE S_IRUSR

/* Get emc dfs staus: 0: disabled 1:enabled */
static int dfs_enable_get(void *data, u64 *val)
{
	*val = einfo->enable;
	return 0;
}
/* Enable/disable emc dfs */
static int dfs_enable_set(void *data, u64 val)
{
	einfo->enable = (bool) val;
		/*
		 * If enabling: activate a timer to execute in next 2 jiffies,
		 * so that emc scaled value takes effect immidiately.
		 */
	if (einfo->enable)
		emc_dfs_enable();
	else
		emc_dfs_disable();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(enable_fops, dfs_enable_get,
	dfs_enable_set, "%llu\n");

/* Get emc dfs staus: 0: disabled 1:enabled */
static int boost_up_threshold_get(void *data, u64 *val)
{
	*val = einfo->boost_up_threshold;
	return 0;
}
/* Enable/disable emc dfs */
static int boost_up_threshold_set(void *data, u64 val)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&speedchange_lock, flags);

	if (!einfo->enable) {
		pr_info("EMC dfs is not enabled\n");
		ret = -EINVAL;
		goto err;
	}

	if (val)
		einfo->boost_up_threshold = val;

err:
	spin_unlock_irqrestore(&speedchange_lock, flags);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(up_threshold_fops,
	boost_up_threshold_get,	boost_up_threshold_set, "%llu\n");

/* scaling emc freq in multiple of boost factor */
static int boost_step_get(void *data, u64 *val)
{
	*val = einfo->boost_step;
	return 0;
}
/* Set period in usec */
static int boost_step_set(void *data, u64 val)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&speedchange_lock, flags);

	if (!einfo->enable) {
		pr_info("EMC dfs is not enabled\n");
		ret = -EINVAL;
		goto err;
	}

	if (!val)
		einfo->boost_step = 1;
	else
		einfo->boost_step = (u8) val;
err:
	spin_unlock_irqrestore(&speedchange_lock, flags);
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(boost_fops, boost_step_get,
	boost_step_set, "%llu\n");

/* minimum time after that emc scaling down happens in usec */
static int boost_down_count_get(void *data, u64 *val)
{
	*val = einfo->boost_dn_count;
	return 0;
}
/* Set period in usec */
static int boost_down_count_set(void *data, u64 val)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&speedchange_lock, flags);

	if (!einfo->enable) {
		pr_info("EMC dfs is not enabled\n");
		ret = -EINVAL;
		goto err;
	}

	if (val)
		einfo->boost_dn_count = (u32) val;
		ret = 0;
err:
	spin_unlock_irqrestore(&speedchange_lock, flags);
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(down_cnt_fops, boost_down_count_get,
	boost_down_count_set, "%llu\n");

static int period_get(void *data, u64 *val)
{
	*val = einfo->timer_rate;
	return 0;
}

/* Set period in usec */
static int period_set(void *data, u64 val)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&speedchange_lock, flags);

	if (!einfo->enable) {
		pr_info("EMC dfs is not enabled\n");
		ret = -EINVAL;
		goto err;
	}

	if (val)
		einfo->timer_rate = (unsigned long)val;

err:
	spin_unlock_irqrestore(&speedchange_lock, flags);
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(period_fops, period_get, period_set, "%llu\n");


static int emc_dfs_debugfs_init(struct nvadsp_drv_data *drv)
{
	int ret = -ENOMEM;
	struct dentry *d;

	if (!drv->adsp_debugfs_root)
		return ret;

	emc_dfs_root  = debugfs_create_dir("emc_dfs", drv->adsp_debugfs_root);
	if (!emc_dfs_root)
		goto err_out;

	d = debugfs_create_file("enable", RW_MODE, emc_dfs_root, NULL,
		&enable_fops);
	if (!d)
		goto err_root;

	d = debugfs_create_file("boost_up_threshold", RW_MODE, emc_dfs_root,
		NULL, &up_threshold_fops);
	if (!d)
		goto err_root;

	d = debugfs_create_file("boost_step", RW_MODE, emc_dfs_root, NULL,
		&boost_fops);
	if (!d)
		goto err_root;

	d = debugfs_create_file("boost_down_count", RW_MODE, emc_dfs_root,
		NULL, &down_cnt_fops);
	if (!d)
		goto err_root;

	d = debugfs_create_file("period", RW_MODE, emc_dfs_root, NULL,
		&period_fops);
	if (!d)
		goto err_root;

	return 0;

err_root:
	debugfs_remove_recursive(emc_dfs_root);

err_out:
	return ret;
}

#endif

status_t __init emc_dfs_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret = 0;

	einfo = &global_emc_info;
	einfo->abridge_base = drv->base_regs[ABRIDGE];

	einfo->emcclk = clk_get_sys("ape", "emc");
	if (IS_ERR_OR_NULL(einfo->emcclk)) {
		dev_info(&pdev->dev, "unable to find ape.emc clock\n");
		return PTR_ERR(einfo->emcclk);
	}

	einfo->timer_rate = DEFAULT_SAMPLE_PERIOD;
	einfo->boost_up_threshold = DEFAULT_BOOST_UP_THRESHOLD;
	einfo->boost_step = DEFAULT_BOOST_STEP;
	einfo->dn_count = 0;
	einfo->boost_dn_count = BOOST_DOWN_COUNT;
	einfo->enable = 1;

	einfo->max_freq = clk_round_rate(einfo->emcclk, ULONG_MAX);
	ret = clk_set_rate(einfo->emcclk, einfo->max_freq);
	if (ret) {
		dev_info(&pdev->dev, "failed to set ape.emc freq:%d\n", ret);
		return PTR_ERR(einfo->emcclk);
	}
	einfo->max_freq /= 1000;
	einfo->cur_freq = clk_get_rate(einfo->emcclk) / 1000;
	if (!einfo->cur_freq) {
		dev_info(&pdev->dev, "ape.emc freq is NULL:\n");
		return PTR_ERR(einfo->emcclk);
	}

	dev_info(&pdev->dev, "einfo->cur_freq %lu\n", einfo->cur_freq);

	spin_lock_init(&speedchange_lock);
	init_timer(&einfo->cnt_timer);
	einfo->cnt_timer.function = emc_dfs_timer;

	speedchange_task = kthread_create(clk_work, NULL, "emc_dfs");
	if (IS_ERR(speedchange_task))
		return PTR_ERR(speedchange_task);

	sched_setscheduler_nocheck(speedchange_task, SCHED_FIFO, &param);
	get_task_struct(speedchange_task);

	/* NB: wake up so the thread does not look hung to the freezer */
	wake_up_process(speedchange_task);

	emc_dfs_enable();

	dev_info(&pdev->dev, "APE EMC DFS is initialized\n");

#ifdef CONFIG_DEBUG_FS
	emc_dfs_debugfs_init(drv);
#endif

	return ret;
}
void __exit emc_dfs_exit(void)
{
	kthread_stop(speedchange_task);
	put_task_struct(speedchange_task);
}
