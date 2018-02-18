/*
 * Copyright (C) 2014-2016, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/clock.h>
#include <linux/irqchip/tegra-agic.h>
#include <linux/irq.h>

#include "ape_actmon.h"
#include "dev.h"

#define ACTMON_DEV_CTRL				0x00
#define ACTMON_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 << 26)
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	21
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK	(0x7 << 21)
#define ACTMON_DEV_CTRL_UP_WMARK_ENB		(0x1 << 19)
#define ACTMON_DEV_CTRL_DOWN_WMARK_ENB		(0x1 << 18)
#define ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB	(0x1 << 17)
#define ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB	(0x1 << 16)
#define ACTMON_DEV_CTRL_AT_END_ENB		(0x1 << 15)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 13)
#define ACTMON_DEV_CTRL_K_VAL_SHIFT		10
#define ACTMON_DEV_CTRL_K_VAL_MASK		(0x7 << 10)
#define ACTMON_DEV_CTRL_SAMPLE_PERIOD_VAL_SHIFT	(0)
#define ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK	(0xff << 0)

#define ACTMON_DEV_UP_WMARK			0x04
#define ACTMON_DEV_DOWN_WMARK			0x08
#define ACTMON_DEV_AVG_UP_WMARK			0x0c
#define ACTMON_DEV_AVG_DOWN_WMARK			0x10
#define ACTMON_DEV_INIT_AVG			0x14

#define ACTMON_DEV_COUNT			0x18
#define ACTMON_DEV_AVG_COUNT			0x1c

#define ACTMON_DEV_INTR_STATUS			0x20
#define ACTMON_DEV_INTR_UP_WMARK		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK		(0x1 << 29)
#define ACTMON_DEV_INTR_AVG_UP_WMARK		(0x1 << 28)

#define ACTMON_DEV_COUNT_WEGHT			0x24

#define ACTMON_DEV_SAMPLE_CTRL			0x28
#define ACTMON_DEV_SAMPLE_CTRL_TICK_65536	(0x1 << 2)
#define ACTMON_DEV_SAMPLE_CTRL_TICK_256		(0x0 << 1)

#define AMISC_ACTMON_0			0x54
#define AMISC_ACTMON_CNT_TARGET_ENABLE	(0x1 << 31)
#define ACTMON_DEFAULT_AVG_WINDOW_LOG2		7
/* 1/10 of % i.e 60 % of max freq */
#define ACTMON_DEFAULT_AVG_BAND		6
#define ACTMON_MAX_REG_OFFSET		0x2c
/* TBD: These would come via dts file */
#define ACTMON_REG_OFFSET			0x800
/* milli second divider as SAMPLE_TICK*/
#define SAMPLE_MS_DIVIDER			65536
/* Sample period in ms */
#define ACTMON_DEFAULT_SAMPLING_PERIOD	20
#define AVG_COUNT_THRESHOLD		100000

static struct actmon ape_actmon;
static struct actmon *apemon;

/* APE activity monitor: Samples ADSP activity */
static struct actmon_dev actmon_dev_adsp = {
	.reg = 0x000,
	.clk_name = "adsp_cpu",

	/* ADSP suspend activity floor */
	.suspend_freq = 51200,

	/* min step by which we want to boost in case of sudden boost request */
	.boost_freq_step = 51200,

	/* % of boost freq for boosting up  */
	.boost_up_coef = 200,

	/*
	 * % of boost freq for boosting down. Should be boosted down by
	 * exponential down
	 */
	.boost_down_coef = 80,

	/*
	 * % of device freq collected in a sample period set as boost up
	 * threshold. boost interrupt is generated when actmon_count
	 * (absolute actmon count in a sample period)
	 * crosses this threshold consecutively by up_wmark_window.
	 */
	.boost_up_threshold = 95,

	/*
	 * % of device freq collected in a sample period set as boost down
	 * threshold. boost interrupt is generated when actmon_count(raw_count)
	 * crosses this threshold consecutively by down_wmark_window.
	 */
	.boost_down_threshold = 80,

	/*
	 * No of times raw counts hits the up_threshold to generate an
	 * interrupt
	 */
	.up_wmark_window = 4,

	/*
	 * No of times raw counts hits the down_threshold to generate an
	 * interrupt.
	 */
	.down_wmark_window = 8,

	/*
	 * No of samples = 2^ avg_window_log2 for calculating exponential moving
	 * average.
	 */
	.avg_window_log2 = ACTMON_DEFAULT_AVG_WINDOW_LOG2,

	/*
	 * "weight" is used to scale the count to match the device freq
	 * When 256 adsp active cpu clock are generated, actmon count
	 * is increamented by 1. Making weight as 256 ensures that 1 adsp active
	 * clk increaments actmon_count by 1.
	 * This makes actmon_count exactly reflect active adsp cpu clk
	 * cycles.
	 */
	.count_weight = 0x100,

	/*
	 * FREQ_SAMPLER: samples number of device(adsp) active cycles
	 * weighted by count_weight to reflect  * actmon_count within a
	 * sample period.
	 * LOAD_SAMPLER: samples actmon active cycles weighted by
	 * count_weight to reflect actmon_count within a sample period.
	 */
	.type = ACTMON_FREQ_SAMPLER,
	.state = ACTMON_UNINITIALIZED,
};

static struct actmon_dev *actmon_devices[] = {
	&actmon_dev_adsp,
};

static inline u32 actmon_readl(u32 offset)
{
	return __raw_readl(apemon->base + offset);
}
static inline void actmon_writel(u32 val, u32 offset)
{
	__raw_writel(val, apemon->base + offset);
}
static inline void actmon_wmb(void)
{
	wmb();
}

#define offs(x)		(dev->reg + x)

static inline unsigned long do_percent(unsigned long val, unsigned int pct)
{
	return val * pct / 100;
}

static void actmon_update_sample_period(unsigned long period)
{
	u32 sample_period_in_clks;
	u32 val = 0;

	apemon->sampling_period = period;
	/*
	 * sample_period_in_clks <1..255> = (actmon_clk_freq<1..40800> *
	 * actmon_sample_period <10ms..40ms>) / SAMPLE_MS_DIVIDER(65536)
	 */
	sample_period_in_clks = (apemon->freq * apemon->sampling_period) /
		SAMPLE_MS_DIVIDER;

	val = actmon_readl(ACTMON_DEV_CTRL);
	val &= ~ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;
	val |= (sample_period_in_clks <<
		ACTMON_DEV_CTRL_SAMPLE_PERIOD_VAL_SHIFT)
		& ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;
	actmon_writel(val, ACTMON_DEV_CTRL);
}

static inline void actmon_dev_up_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
					 dev->cur_freq : apemon->freq;

	val = freq * apemon->sampling_period;
	actmon_writel(do_percent(val, dev->boost_up_threshold),
				  offs(ACTMON_DEV_UP_WMARK));
}

static inline void actmon_dev_down_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
				 dev->cur_freq : apemon->freq;

	val = freq * apemon->sampling_period;
	actmon_writel(do_percent(val, dev->boost_down_threshold),
				  offs(ACTMON_DEV_DOWN_WMARK));
}

static inline void actmon_dev_wmark_set(struct actmon_dev *dev)
{
	u32 val;
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
				 dev->cur_freq : apemon->freq;

	val = freq * apemon->sampling_period;

	actmon_writel(do_percent(val, dev->boost_up_threshold),
					  offs(ACTMON_DEV_UP_WMARK));
	actmon_writel(do_percent(val, dev->boost_down_threshold),
					  offs(ACTMON_DEV_DOWN_WMARK));
}

static inline void actmon_dev_avg_wmark_set(struct actmon_dev *dev)
{
	/*
	 * band: delta from current count to be set for avg upper
	 * and lower thresholds
	 */
	u32 band = dev->avg_band_freq * apemon->sampling_period;
	u32 avg = dev->avg_count;

	actmon_writel(avg + band, offs(ACTMON_DEV_AVG_UP_WMARK));
	avg = max(avg, band);
	actmon_writel(avg - band, offs(ACTMON_DEV_AVG_DOWN_WMARK));
}

static unsigned long actmon_dev_avg_freq_get(struct actmon_dev *dev)
{
	u64 val;

	if (dev->type == ACTMON_FREQ_SAMPLER)
		return dev->avg_count / apemon->sampling_period;

	val = (u64) dev->avg_count * dev->cur_freq;
	do_div(val , apemon->freq * apemon->sampling_period);
	return (u32)val;
}

/* Activity monitor sampling operations */
static irqreturn_t ape_actmon_dev_isr(int irq, void *dev_id)
{
	u32 val, devval;
	unsigned long flags;
	struct actmon_dev *dev = (struct actmon_dev *)dev_id;

	spin_lock_irqsave(&dev->lock, flags);
	val = actmon_readl(offs(ACTMON_DEV_INTR_STATUS));
	actmon_writel(val, offs(ACTMON_DEV_INTR_STATUS)); /* clr all */
	devval = actmon_readl(offs(ACTMON_DEV_CTRL));

	if (val & ACTMON_DEV_INTR_AVG_UP_WMARK) {
		devval |= (ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB);
		dev->avg_count = actmon_readl(offs(ACTMON_DEV_AVG_COUNT));
		actmon_dev_avg_wmark_set(dev);
	} else if (val & ACTMON_DEV_INTR_AVG_DOWN_WMARK) {
		devval |= (ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB);
		dev->avg_count = actmon_readl(offs(ACTMON_DEV_AVG_COUNT));
		actmon_dev_avg_wmark_set(dev);
	}

	if (val & ACTMON_DEV_INTR_UP_WMARK) {
		devval |= (ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB);

		dev->boost_freq = dev->boost_freq_step +
			do_percent(dev->boost_freq, dev->boost_up_coef);
		if (dev->boost_freq >= dev->max_freq) {
			dev->boost_freq = dev->max_freq;
			devval &= ~ACTMON_DEV_CTRL_UP_WMARK_ENB;
		}
	} else if (val & ACTMON_DEV_INTR_DOWN_WMARK) {
		devval |= (ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB);

		dev->boost_freq =
			do_percent(dev->boost_freq, dev->boost_down_coef);
		if (dev->boost_freq == 0) {
			devval &= ~ACTMON_DEV_CTRL_DOWN_WMARK_ENB;
		}
	}

	actmon_writel(devval, offs(ACTMON_DEV_CTRL));
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t ape_actmon_dev_fn(int irq, void *dev_id)
{
	unsigned long flags, freq;
	struct actmon_dev *dev = (struct actmon_dev *)dev_id;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state != ACTMON_ON) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return IRQ_HANDLED;
	}

	freq = actmon_dev_avg_freq_get(dev);
	dev->avg_actv_freq = freq; /* in kHz */
	freq = do_percent(freq, dev->avg_sustain_coef);
	freq += dev->boost_freq;

	dev->target_freq = freq;
	spin_unlock_irqrestore(&dev->lock, flags);

	dev_dbg(dev->device, "%s(kHz): avg: %lu,  boost: %lu, target: %lu, current: %lu\n",
	dev->clk_name, dev->avg_actv_freq, dev->boost_freq, dev->target_freq,
	dev->cur_freq);

#if defined(CONFIG_TEGRA_ADSP_DFS)
	adsp_cpu_set_rate(freq);
#endif

	return IRQ_HANDLED;
}

/* Activity monitor configuration and control */
static void actmon_dev_configure(struct actmon_dev *dev,
		unsigned long freq)
{
	u32 val;

	dev->boost_freq = 0;
	dev->cur_freq = freq;
	dev->target_freq = freq;
	dev->avg_actv_freq = freq;

	if (dev->type == ACTMON_FREQ_SAMPLER) {
		/*
		 * max actmon count  = (count_weight * adsp_freq (khz)
				* sample_period (ms)) / (PULSE_N_CLK+1)
		 * As Count_weight is set as 256(0x100) and
		 * (PULSE_N_CLK+1) = 256. both would be
		 * compensated while coming up max_actmon_count.
		 * in other word
		 * max actmon count  = ((count_weight * adsp_freq *
		 *			 sample_period_reg * SAMPLE_TICK)
		 *			 / (ape_freq * (PULSE_N_CLK+1)))
		 * where -
		 * sample_period_reg : <1..255> sample period in no of
		 *			actmon clocks per sample
		 * SAMPLE_TICK : Arbtrary value for ms - 65536, us - 256
		 * (PULSE_N_CLK + 1) : 256 - No of adsp "active" clocks to
		 *			increament raw_count/ actmon_count
		 *			 by one.
		 */
		dev->avg_count = dev->cur_freq * apemon->sampling_period;
		dev->avg_band_freq = dev->max_freq *
						 ACTMON_DEFAULT_AVG_BAND / 1000;
	} else {
		dev->avg_count = apemon->freq * apemon->sampling_period;
		dev->avg_band_freq = apemon->freq *
					 ACTMON_DEFAULT_AVG_BAND / 1000;
	}
	actmon_writel(dev->avg_count, offs(ACTMON_DEV_INIT_AVG));

	BUG_ON(!dev->boost_up_threshold);
	dev->avg_sustain_coef = 100 * 100 / dev->boost_up_threshold;
	actmon_dev_avg_wmark_set(dev);
	actmon_dev_wmark_set(dev);

	actmon_writel(dev->count_weight, offs(ACTMON_DEV_COUNT_WEGHT));
	val = actmon_readl(ACTMON_DEV_CTRL);

	val |= (ACTMON_DEV_CTRL_PERIODIC_ENB |
		ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB |
		ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB);
	val |= ((dev->avg_window_log2 - 1) << ACTMON_DEV_CTRL_K_VAL_SHIFT) &
			ACTMON_DEV_CTRL_K_VAL_MASK;
	val |= ((dev->down_wmark_window - 1) <<
				ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT) &
			   ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK;
	val |=  ((dev->up_wmark_window - 1) <<
				 ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT) &
				ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK;
	val |= ACTMON_DEV_CTRL_DOWN_WMARK_ENB |
			ACTMON_DEV_CTRL_UP_WMARK_ENB;

	actmon_writel(val, offs(ACTMON_DEV_CTRL));
	actmon_wmb();
}

static void actmon_dev_enable(struct actmon_dev *dev)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_OFF) {
		dev->state = ACTMON_ON;

		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		val |= ACTMON_DEV_CTRL_ENB;
		actmon_writel(val, offs(ACTMON_DEV_CTRL));
		actmon_wmb();
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void actmon_dev_disable(struct actmon_dev *dev)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_ON) {
		dev->state = ACTMON_OFF;

		val = actmon_readl(offs(ACTMON_DEV_CTRL));
		val &= ~ACTMON_DEV_CTRL_ENB;
			actmon_writel(val, offs(ACTMON_DEV_CTRL));
			actmon_writel(0xffffffff, offs(ACTMON_DEV_INTR_STATUS));
			actmon_wmb();
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}
static int actmon_dev_probe(struct actmon_dev *dev)
{
	struct nvadsp_drv_data *drv_data = dev_get_drvdata(dev->device);
	int ret;

	dev->irq = drv_data->agic_irqs[ACTMON_VIRQ];
	ret = request_threaded_irq(dev->irq, ape_actmon_dev_isr,
			ape_actmon_dev_fn, IRQ_TYPE_LEVEL_HIGH,
			dev->clk_name, dev);
	if (ret) {
		dev_err(dev->device, "Failed irq %d request for %s\n", dev->irq,
			dev->clk_name);
		goto end;
	}
	disable_irq(dev->irq);
end:
	return ret;
}

static int actmon_dev_init(struct actmon_dev *dev)
{
	int ret = -EINVAL;
	unsigned long freq;

	spin_lock_init(&dev->lock);

	dev->clk = clk_get_sys(NULL, dev->clk_name);
	if (IS_ERR_OR_NULL(dev->clk)) {
		dev_err(dev->device, "Failed to find %s clock\n",
			dev->clk_name);
		goto end;
	}

	ret = clk_prepare_enable(dev->clk);
	if (ret) {
		dev_err(dev->device, "unable to enable %s clock\n",
			dev->clk_name);
		goto err_enable;
	}

	dev->max_freq = freq = clk_get_rate(dev->clk) / 1000;
	actmon_dev_configure(dev, freq);

	dev->state = ACTMON_OFF;
	actmon_dev_enable(dev);
	enable_irq(dev->irq);
	return 0;

err_enable:
	clk_put(dev->clk);
end:
	return ret;
}

#ifdef CONFIG_DEBUG_FS

#define RW_MODE (S_IWUSR | S_IRUSR)
#define RO_MODE S_IRUSR

static struct dentry *clk_debugfs_root;

static int type_show(struct seq_file *s, void *data)
{
	struct actmon_dev *dev = s->private;

	seq_printf(s, "%s\n", (dev->type == ACTMON_LOAD_SAMPLER) ?
			"Load Activity Monitor" : "Frequency Activity Monitor");
	return 0;
}
static int type_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_show, inode->i_private);
}
static const struct file_operations type_fops = {
	.open		= type_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actv_get(void *data, u64 *val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;

	spin_lock_irqsave(&dev->lock, flags);
	*val = actmon_dev_avg_freq_get(dev);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(actv_fops, actv_get, NULL, "%llu\n");

static int step_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_freq_step * 100 / dev->max_freq;
	return 0;
}
static int step_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;

	if (val > 100)
		val = 100;

	spin_lock_irqsave(&dev->lock, flags);
	dev->boost_freq_step = do_percent(dev->max_freq, (unsigned int)val);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(step_fops, step_get, step_set, "%llu\n");

static int count_weight_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->count_weight;
	return 0;
}
static int count_weight_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;

	spin_lock_irqsave(&dev->lock, flags);
	dev->count_weight = (u32) val;
	actmon_writel(dev->count_weight, offs(ACTMON_DEV_COUNT_WEGHT));
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cnt_wt_fops, count_weight_get,
		 count_weight_set, "%llu\n");

static int up_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_up_threshold;
	return 0;
}
static int up_threshold_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;
	unsigned int up_threshold = (unsigned int)val;

	if (up_threshold > 100)
		up_threshold = 100;

	spin_lock_irqsave(&dev->lock, flags);

	if (up_threshold <= dev->boost_down_threshold)
		up_threshold = dev->boost_down_threshold;
	if (up_threshold)
		dev->avg_sustain_coef = 100 * 100 / up_threshold;
	dev->boost_up_threshold = up_threshold;

	actmon_dev_up_wmark_set(dev);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(up_threshold_fops, up_threshold_get,
						up_threshold_set, "%llu\n");

static int down_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->boost_down_threshold;
	return 0;
}
static int down_threshold_set(void *data, u64 val)
{
	unsigned long flags;
	struct actmon_dev *dev = data;
	unsigned int down_threshold = (unsigned int)val;

	spin_lock_irqsave(&dev->lock, flags);

	if (down_threshold >= dev->boost_up_threshold)
		down_threshold = dev->boost_up_threshold;
	dev->boost_down_threshold = down_threshold;

	actmon_dev_down_wmark_set(dev);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(down_threshold_fops, down_threshold_get,
					down_threshold_set, "%llu\n");

static int state_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;
	*val = dev->state;
	return 0;
}
static int state_set(void *data, u64 val)
{
	struct actmon_dev *dev = data;

	if (val)
		actmon_dev_enable(dev);
	else
		actmon_dev_disable(dev);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(state_fops, state_get, state_set, "%llu\n");

/* Get period in msec */
static int period_get(void *data, u64 *val)
{
	*val = apemon->sampling_period;
	return 0;
}
/* Set period in msec */
static int period_set(void *data, u64 val)
{
	int i;
	unsigned long flags;
	u8 period = (u8)val;

	if (period) {
		actmon_update_sample_period(period);

		for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
			struct actmon_dev *dev = actmon_devices[i];
			spin_lock_irqsave(&dev->lock, flags);
			actmon_dev_wmark_set(dev);
			spin_unlock_irqrestore(&dev->lock, flags);
		}
		actmon_wmb();
		return 0;
	}
	return -EINVAL;
}
DEFINE_SIMPLE_ATTRIBUTE(period_fops, period_get, period_set, "%llu\n");


static int actmon_debugfs_create_dev(struct actmon_dev *dev)
{
	struct dentry *dir, *d;

	if (dev->state == ACTMON_UNINITIALIZED)
		return 0;

	dir = debugfs_create_dir(dev->clk_name, clk_debugfs_root);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file(
			"actv_type", RO_MODE, dir, dev, &type_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
			"avg_activity", RO_MODE, dir, dev, &actv_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
			"boost_step", RW_MODE, dir, dev, &step_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u32(
		"boost_rate_dec", RW_MODE, dir, (u32 *)&dev->boost_down_coef);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u32(
		"boost_rate_inc", RW_MODE, dir, (u32 *)&dev->boost_up_coef);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"boost_threshold_dn", RW_MODE, dir, dev, &down_threshold_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"boost_threshold_up", RW_MODE, dir, dev, &up_threshold_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"state", RW_MODE, dir, dev, &state_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"cnt_wt", RW_MODE, dir, dev, &cnt_wt_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

static int actmon_debugfs_init(struct nvadsp_drv_data *drv)
{
	int i;
	int ret = -ENOMEM;
	struct dentry *d;

	if (!drv->adsp_debugfs_root)
		return ret;
	d = debugfs_create_dir("adsp_actmon", drv->adsp_debugfs_root);
	if (!d)
		return ret;
	clk_debugfs_root = d;

	d = debugfs_create_file("period", RW_MODE, d, NULL, &period_fops);
	if (!d)
		goto err_out;

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		ret = actmon_debugfs_create_dev(actmon_devices[i]);
		if (ret)
			goto err_out;
	}
	return 0;

err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return ret;
}

#endif

/* freq in KHz */
void actmon_rate_change(unsigned long freq, bool override)
{
	struct actmon_dev *dev = &actmon_dev_adsp;
	unsigned long flags;

	if (override) {
		actmon_dev_disable(dev);
		spin_lock_irqsave(&dev->lock, flags);
		dev->cur_freq = freq;
		dev->avg_count = freq * apemon->sampling_period;
		actmon_writel(dev->avg_count, offs(ACTMON_DEV_INIT_AVG));
		actmon_dev_avg_wmark_set(dev);
		actmon_dev_wmark_set(dev);
		actmon_wmb();
		spin_unlock_irqrestore(&dev->lock, flags);
		actmon_dev_enable(dev);
	} else {
		spin_lock_irqsave(&dev->lock, flags);
		dev->cur_freq = freq;
		if (dev->state == ACTMON_ON) {
			actmon_dev_wmark_set(dev);
			actmon_wmb();
		}
		spin_unlock_irqrestore(&dev->lock, flags);
	}
	/* change ape rate as half of adsp rate */
	clk_set_rate(apemon->clk, freq * 500);
};

int ape_actmon_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		actmon_devices[i]->device = &pdev->dev;
		ret = actmon_dev_probe(actmon_devices[i]);
		dev_dbg(&pdev->dev, "%s actmon: %s probe (%d)\n",
		actmon_devices[i]->clk_name, ret ? "Failed" : "Completed", ret);
	}
	return ret;
}

static int ape_actmon_rc_cb(
	struct notifier_block *nb, unsigned long rate, void *v)
{
	struct actmon_dev *dev = &actmon_dev_adsp;
	unsigned long flags;
	u32 init_cnt;

	if (dev->state != ACTMON_ON) {
		dev_dbg(dev->device, "adsp actmon is not ON\n");
		goto exit_out;
	}

	actmon_dev_disable(dev);

	spin_lock_irqsave(&dev->lock, flags);
	init_cnt = actmon_readl(offs(ACTMON_DEV_AVG_COUNT));
	/* update sample period to maintain number of clock */
	apemon->freq = rate / 1000; /* in KHz */
	actmon_update_sample_period(ACTMON_DEFAULT_SAMPLING_PERIOD);
	actmon_writel(init_cnt, offs(ACTMON_DEV_INIT_AVG));
	spin_unlock_irqrestore(&dev->lock, flags);

	actmon_dev_enable(dev);
exit_out:
	return NOTIFY_OK;
}
int ape_actmon_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);
	static void __iomem *amisc_base;
	u32 sample_period_in_clks;
	struct clk *p;
	u32 val = 0;
	int i, ret;

	if (drv->actmon_initialized)
		return 0;

	apemon = &ape_actmon;
	apemon->base = drv->base_regs[AMISC] + ACTMON_REG_OFFSET;
	amisc_base = drv->base_regs[AMISC];

	apemon->clk = clk_get_sys(NULL, "adsp.ape");
	if (!apemon->clk) {
		dev_err(&pdev->dev, "Failed to find actmon clock\n");
		ret = -EINVAL;
		goto err_out;
	}

	ret = clk_prepare_enable(apemon->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable actmon clock\n");
		ret = -EINVAL;
		goto err_out;
	}
	apemon->clk_rc_nb.notifier_call = ape_actmon_rc_cb;

	/*
	 * "adsp.ape" clk is shared bus user clock and "ape" is bus clock
	 * but rate change notification should come from bus clock itself.
	 */
	p = clk_get_parent(apemon->clk);
	if (!p) {
		dev_err(&pdev->dev, "Failed to find actmon parent clock\n");
		ret = -EINVAL;
		goto clk_err_out;
	}

	ret = tegra_register_clk_rate_notifier(p, &apemon->clk_rc_nb);
	if (ret) {
		dev_err(&pdev->dev, "Registration fail: %s rate change notifier for %s\n",
			p->name, apemon->clk->name);
		goto clk_err_out;
	}
	apemon->freq = clk_get_rate(apemon->clk) / 1000; /* in KHz */

	apemon->sampling_period = ACTMON_DEFAULT_SAMPLING_PERIOD;

	/*
	 * sample period as no of actmon clocks
	 * Actmon is derived from APE clk.
	 * suppose APE clk is 204MHz = 204000 KHz and want to calculate
	 * clocks in 10ms sample
	 * in 1ms = 204000 cycles
	 * 10ms = 204000 * 10 APE cycles
	 * SAMPLE_MS_DIVIDER is an arbitrary number
	 */
	sample_period_in_clks = (apemon->freq * apemon->sampling_period)
		/ SAMPLE_MS_DIVIDER;

	/* set ms mode */
	actmon_writel(ACTMON_DEV_SAMPLE_CTRL_TICK_65536,
		ACTMON_DEV_SAMPLE_CTRL);
	val = actmon_readl(ACTMON_DEV_CTRL);
	val &= ~ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;
	val |= (sample_period_in_clks <<
		ACTMON_DEV_CTRL_SAMPLE_PERIOD_VAL_SHIFT)
		& ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;
	actmon_writel(val, ACTMON_DEV_CTRL);

	/* Enable AMISC_ACTMON */
	val = __raw_readl(amisc_base + AMISC_ACTMON_0);
	val |= AMISC_ACTMON_CNT_TARGET_ENABLE;
	__raw_writel(val, amisc_base + AMISC_ACTMON_0);

	actmon_writel(0xffffffff, ACTMON_DEV_INTR_STATUS); /* clr all */

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		ret = actmon_dev_init(actmon_devices[i]);
		dev_dbg(&pdev->dev, "%s actmon device: %s initialization (%d)\n",
		actmon_devices[i]->clk_name, ret ? "Failed" : "Completed", ret);
	}

#ifdef CONFIG_DEBUG_FS
	actmon_debugfs_init(drv);
#endif

	drv->actmon_initialized = true;

	dev_dbg(&pdev->dev, "adsp actmon initialized ....\n");
	return 0;
clk_err_out:
	if (apemon->clk)
		clk_disable_unprepare(apemon->clk);
err_out:
	if (apemon->clk)
		clk_put(apemon->clk);
	return ret;
}

int ape_actmon_exit(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);
	struct actmon_dev *dev;
	status_t ret = 0;
	int i;

	/* return if actmon is not initialized */
	if (!drv->actmon_initialized)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(actmon_devices); i++) {
		dev = actmon_devices[i];
		actmon_dev_disable(dev);
		disable_irq(dev->irq);
		clk_disable_unprepare(dev->clk);
		clk_put(dev->clk);
	}

	tegra_unregister_clk_rate_notifier(clk_get_parent(apemon->clk),
		&apemon->clk_rc_nb);

	clk_disable_unprepare(apemon->clk);
	clk_put(apemon->clk);

	drv->actmon_initialized = false;

	dev_dbg(&pdev->dev, "adsp actmon has exited ....\n");

	return ret;
}
