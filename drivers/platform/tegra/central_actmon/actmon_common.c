/*
 * Copyright (C) 2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "linux/platform/tegra/actmon_common.h"

/* Global definitions */
static struct actmon_drv_data *actmon;
static struct device *mon_dev;

#define offs(dev_reg_offs) (actmon->base + dev_reg_offs)

static inline unsigned long do_percent(unsigned long val, unsigned int pct)
{
	return val * pct / 100;
}

static inline int actmon_dev_avg_wmark_set(
	struct actmon_dev *dev)
{
	u32 band = dev->avg_band_freq * actmon->sample_period;
	u32 avg = dev->avg_count;
	int ret = -EINVAL;

	if (!dev->ops.set_avg_up_wm)
		goto end;
	else
		dev->ops.set_avg_up_wm(avg + band, offs(dev->reg_offs));

	avg = max(avg, band);

	if (!dev->ops.set_avg_dn_wm)
		goto end;
	else
		dev->ops.set_avg_dn_wm(avg - band, offs(dev->reg_offs));
	return 0;
end:
	return ret;
}

static unsigned long actmon_dev_avg_freq_get(
	struct actmon_dev *dev)
{
	u64 val;

	if (dev->type == ACTMON_FREQ_SAMPLER)
		return dev->avg_count / actmon->sample_period;

	val = (u64)dev->avg_count * dev->cur_freq;
	do_div(val, actmon->freq * actmon->sample_period);
	return (u32)val;
}

static void actmon_dev_disable(struct actmon_dev *dev)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_ON) {
		dev->state = ACTMON_OFF;
		disable_irq(actmon->virq);
		val = actmon_dev_readl(offs(dev->reg_offs),
			ACTMON_CMN_DEV_CTRL);
		val &= ~ACTMON_CMN_DEV_CTRL_ENB;
		actmon_dev_writel(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL,
			val);
		dev->ops.set_intr_st(0xffffffff, offs(dev->reg_offs));
		actmon_wmb();
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void actmon_dev_enable(struct actmon_dev *dev)
{
	unsigned long flags;
	u32 val = 0;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state == ACTMON_OFF) {
		dev->state = ACTMON_ON;
		val = actmon_dev_readl(offs(dev->reg_offs),
				ACTMON_CMN_DEV_CTRL);
		val |= ACTMON_CMN_DEV_CTRL_ENB;
		actmon_dev_writel(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL,
				val);
		actmon_wmb();

		if (dev->actmon_dev_clk_enable)
			dev->actmon_dev_clk_enable(dev);

		enable_irq(actmon->virq);
	}

	spin_unlock_irqrestore(&dev->lock, flags);
}

static inline void actmon_dev_up_wmark_set(
	struct actmon_dev *dev)
{
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon->freq;
	u32 val;

	val = freq * actmon->sample_period;
	dev->ops.set_dev_up_wm(do_percent(val, dev->boost_up_threshold),
		offs(dev->reg_offs));
}

static inline void actmon_dev_down_wmark_set(
	struct actmon_dev *dev)
{
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon->freq;
	u32 val;

	val = freq * actmon->sample_period;
	dev->ops.set_dev_dn_wm(do_percent(val,
		dev->boost_down_threshold), offs(dev->reg_offs));
}

static inline int actmon_dev_wmark_set(struct actmon_dev *dev)
{
	unsigned long freq = (dev->type == ACTMON_FREQ_SAMPLER) ?
		dev->cur_freq : actmon->freq;
	int ret = -EINVAL;
	u32 val;

	val = freq * actmon->sample_period;

	if (!dev->ops.set_dev_up_wm)
		goto end;
	else
		dev->ops.set_dev_up_wm(do_percent(val, dev->boost_up_threshold),
			offs(dev->reg_offs));

	if (!dev->ops.set_dev_dn_wm)
		goto end;
	else
		dev->ops.set_dev_dn_wm(do_percent(val,
			dev->boost_down_threshold), offs(dev->reg_offs));
	return 0;

end:
	return ret;
}

static ssize_t avgactv_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct actmon_dev *dev = container_of(attr, struct actmon_dev,
		avgact_attr);
	unsigned long val = actmon_dev_avg_freq_get(dev);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", val);
}

#ifdef CONFIG_DEBUG_FS

#define RW_MODE (S_IWUSR | S_IRUGO)
#define RO_MODE	S_IRUGO

static struct dentry *dbgfs_root;

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
	struct actmon_dev *dev = data;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	*val = actmon_dev_avg_freq_get(dev);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(actv_fops, actv_get, NULL,
	"%llu\n");

static int step_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;

	*val = dev->boost_freq_step * 100 / dev->max_freq;
	return 0;
}
static int step_set(void *data, u64 val)
{
	struct actmon_dev *dev = data;
	unsigned long flags;

	if (val > 100)
		val = 100;

	spin_lock_irqsave(&dev->lock, flags);
	dev->boost_freq_step = do_percent(dev->max_freq, (unsigned int)val);
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(step_fops, step_get, step_set,
	"%llu\n");

static int up_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;

	*val = dev->boost_up_threshold;
	return 0;
}
static int up_threshold_set(void *data, u64 val)
{
	unsigned int up_threshold = (unsigned int)val;
	struct actmon_dev *dev = data;
	unsigned long flags;

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
DEFINE_SIMPLE_ATTRIBUTE(up_threshold_fops,
	up_threshold_get, up_threshold_set, "%llu\n");

static int down_threshold_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;

	*val = dev->boost_down_threshold;
	return 0;
}
static int down_threshold_set(void *data, u64 val)
{
	unsigned int down_threshold = (unsigned int)val;
	struct actmon_dev *dev = data;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	if (down_threshold >= dev->boost_up_threshold)
		down_threshold = dev->boost_up_threshold;
	dev->boost_down_threshold = down_threshold;

	actmon_dev_down_wmark_set(dev);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(down_threshold_fops,
	down_threshold_get, down_threshold_set, "%llu\n");

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
DEFINE_SIMPLE_ATTRIBUTE(state_fops, state_get, state_set,
	"%llu\n");

static int period_get(void *data, u64 *val)
{
	*val = actmon->sample_period;
	return 0;
}

static int period_set(void *data, u64 val)
{
	unsigned long flags;
	u8 period = (u8)val;
	int i;

	if (period) {
		actmon->sample_period = period;
		actmon->ops.set_sample_prd(period - 1, actmon->base);

		for (i = 0; i < MAX_DEVICES; i++) {
			struct actmon_dev *dev = &actmon->devices[i];

			spin_lock_irqsave(&dev->lock, flags);
			actmon_dev_wmark_set(dev);
			spin_unlock_irqrestore(&dev->lock, flags);
		}
		actmon_wmb();
		return 0;
	}
	return -EINVAL;
}
DEFINE_SIMPLE_ATTRIBUTE(period_fops, period_get,
	period_set, "%llu\n");

static int up_wmark_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;

	*val = dev->up_wmark_window;
	return 0;
}
static int up_wmark_set(void *data, u64 val)
{
	struct actmon_dev *dev = data;
	u8 wmark = (unsigned int)val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	dev->up_wmark_window = wmark;
	val = actmon_dev_readl(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL);
	val |=  (((dev->up_wmark_window - 1) <<
		ACTMON_CMN_DEV_CTRL_UP_WMARK_NUM_SHIFT) &
		ACTMON_CMN_DEV_CTRL_UP_WMARK_NUM_MASK);
	actmon_dev_writel(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL, val);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(up_wmark_fops, up_wmark_get,
	up_wmark_set, "%llu\n");

static int down_wmark_get(void *data, u64 *val)
{
	struct actmon_dev *dev = data;

	*val = dev->down_wmark_window;
	return 0;
}
static int down_wmark_set(void *data, u64 val)
{
	struct actmon_dev *dev = data;
	u8 wmark = (unsigned int)val;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	dev->down_wmark_window = wmark;
	val = actmon_dev_readl(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL);
	val |=  (((dev->down_wmark_window - 1) <<
		ACTMON_CMN_DEV_CTRL_DOWN_WMARK_NUM_SHIFT) &
		ACTMON_CMN_DEV_CTRL_DOWN_WMARK_NUM_MASK);
	actmon_dev_writel(offs(dev->reg_offs), ACTMON_CMN_DEV_CTRL, val);
	actmon_wmb();

	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(down_wmark_fops,
	down_wmark_get, down_wmark_set, "%llu\n");

static int actmon_debugfs_create_dev(struct actmon_dev *dev)
{
	struct dentry *dir, *d;

	if (dev->state == ACTMON_UNINITIALIZED)
		return 0;

	dir = debugfs_create_dir(dev->dev_name, dbgfs_root);
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
		"up_wmark", RW_MODE, dir, dev, &up_wmark_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"down_wmark", RW_MODE, dir, dev, &down_wmark_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

static int __init actmon_debugfs_init(void)
{
	int ret = -ENOMEM;
	struct dentry *d = NULL;
	int i;

	dbgfs_root = debugfs_create_dir("tegra_central_actmon", NULL);
	if (!dbgfs_root)
		return ret;

	d = debugfs_create_file("period", RW_MODE, dbgfs_root, NULL,
		&period_fops);
	if (!d)
		goto err_out;

	for (i = 0; i < MAX_DEVICES; i++) {
		ret = actmon_debugfs_create_dev(&actmon->devices[i]);
		if (ret)
			goto err_out;
	}
	return 0;

err_out:
	debugfs_remove_recursive(dbgfs_root);
	return ret;
}
#endif

/* Activity monitor sampling operations */
static irqreturn_t actmon_dev_isr(int irq, void *dev_id)
{

	struct actmon_dev *dev = (struct actmon_dev *)dev_id;
	unsigned long flags;
	u32 int_val = 0;
	u32 val;

	if (!actmon->ops.get_glb_intr_st)
		return IRQ_NONE;
	val = actmon->ops.get_glb_intr_st(actmon->base) &
				dev->glb_status_irq_mask;
	if (!val)
		return IRQ_NONE;

	spin_lock_irqsave(&dev->lock, flags);

	if (!dev->ops.get_avg_cnt) {
		dev_err(mon_dev, "get_avg_cnt operation is not registered\n");
		return IRQ_NONE;
	}
	dev->avg_count = dev->ops.get_avg_cnt(offs(dev->reg_offs));
	actmon_dev_avg_wmark_set(dev);

	if (!dev->ops.get_dev_intr_enb) {
		dev_err(mon_dev, "get_dev_intr_enb operation is not registered\n");
		return IRQ_NONE;
	}
	int_val = dev->ops.get_dev_intr_enb(offs(dev->reg_offs));

	if (!dev->ops.get_intr_st) {
		dev_err(mon_dev, "get_intr_st operation is not registered\n");
		return IRQ_NONE;
	}
	val = dev->ops.get_intr_st(offs(dev->reg_offs));

	if (val & ACTMON_CMN_DEV_INTR_UP_WMARK) {
		dev_dbg(mon_dev, "raw_cnt_up:%u\n",
			dev->ops.get_raw_cnt(offs(dev->reg_offs)));
		if (!dev->ops.enb_dev_wm) {
			dev_err(mon_dev, "enb_dev_wm operation is not registered\n");
			return IRQ_NONE;
		}
		dev->ops.enb_dev_wm(&int_val);

		dev->boost_freq = dev->boost_freq_step +
			do_percent(dev->boost_freq, dev->boost_up_coef);
		if (dev->boost_freq >= dev->max_freq) {
			dev->boost_freq = dev->max_freq;
			if (!dev->ops.disb_dev_up_wm) {
				dev_err(mon_dev,
					"disb_dev_up_wm operation is not registered\n");
				return IRQ_NONE;
			}
			dev->ops.disb_dev_up_wm(&int_val);
		}
	} else if (val & ACTMON_CMN_DEV_INTR_DOWN_WMARK) {
		dev_dbg(mon_dev, "raw_cnt_down:%u\n",
			dev->ops.get_raw_cnt(offs(dev->reg_offs)));
		if (!dev->ops.enb_dev_wm) {
			dev_err(mon_dev, "enb_dev_wm operation is not registered\n");
			return IRQ_NONE;
		}
		dev->ops.enb_dev_wm(&int_val);

		dev->boost_freq =
			do_percent(dev->boost_freq, dev->boost_down_coef);

		if (dev->boost_freq == 0) {
			if (!dev->ops.disb_dev_dn_wm) {
				dev_err(mon_dev, "disb_dev_dn_wm operation is not registered\n");
				return IRQ_NONE;
			}
			dev->ops.disb_dev_dn_wm(&int_val);
		}
	}

	if (!dev->ops.enb_dev_intr) {
		dev_err(mon_dev, "enb_dev_intr operation is not registered\n");
		return IRQ_NONE;
	}
	dev->ops.enb_dev_intr(int_val, offs(dev->reg_offs));

	/* clr all */
	if (!dev->ops.set_intr_st) {
		dev_err(mon_dev, "set_intr_st operation is not registered\n");
		return IRQ_NONE;
	}
	dev->ops.set_intr_st(0xffffffff, offs(dev->reg_offs));

	actmon_wmb();
	spin_unlock_irqrestore(&dev->lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t actmon_dev_fn(int irq, void *dev_id)
{
	struct actmon_dev *dev = (struct actmon_dev *)dev_id;
	unsigned long flags, freq;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->state != ACTMON_ON) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return IRQ_HANDLED;
	}

	freq = actmon_dev_avg_freq_get(dev);
	dev->avg_actv_freq = freq;
	freq = do_percent(freq, dev->avg_sustain_coef);
	freq += dev->boost_freq;

	dev->target_freq = freq;

	spin_unlock_irqrestore(&dev->lock, flags);

	dev_dbg(mon_dev, "%s(kHz): avg: %lu,  boost: %lu, target: %lu, current: %lu\n",
	dev->dev_name, dev->avg_actv_freq, dev->boost_freq, dev->target_freq,
	dev->cur_freq);

	dev->actmon_dev_set_rate(dev, freq);

	return IRQ_HANDLED;
}


/* Activity monitor configuration and control */
static int actmon_dev_configure(struct actmon_dev *dev,
		unsigned long freq)
{
	int ret = -EINVAL;

	dev->cur_freq = freq;
	dev->target_freq = freq;
	dev->avg_actv_freq = freq;

	if (dev->type == ACTMON_FREQ_SAMPLER) {
		dev->avg_count = dev->cur_freq * actmon->sample_period;
		dev->avg_band_freq = dev->max_freq *
			ACTMON_DEFAULT_AVG_BAND / 1000;
	} else {
		dev->avg_count = actmon->freq * actmon->sample_period;
		dev->avg_band_freq = actmon->freq *
			ACTMON_DEFAULT_AVG_BAND / 1000;
	}

	if (!dev->ops.set_init_avg)
		goto end;
	else
		dev->ops.set_init_avg(dev->avg_count, offs(dev->reg_offs));

	BUG_ON(!dev->boost_up_threshold);

	dev->avg_sustain_coef = 100 * 100 / dev->boost_up_threshold;

	if (actmon_dev_avg_wmark_set(dev))
		goto end;

	if (actmon_dev_wmark_set(dev))
		goto end;

	if (!dev->ops.set_cnt_wt)
		goto end;
	else
		dev->ops.set_cnt_wt(dev->count_weight, offs(dev->reg_offs));

	if (!dev->ops.set_intr_st)
		goto end;
	else
		dev->ops.set_intr_st(0xffffffff, offs(dev->reg_offs));

	if (!dev->ops.init_dev_cntrl)
		goto end;
	else
		dev->ops.init_dev_cntrl(dev, offs(dev->reg_offs));

	if (!dev->ops.enb_dev_intr_all)
		goto end;
	else
		dev->ops.enb_dev_intr_all(offs(dev->reg_offs));

	actmon_wmb();
	return 0;
end:
	return ret;
}

static int actmon_rate_notify_cb(
	struct notifier_block *nb, unsigned long rate, void *v)
{
	unsigned long flags;
	struct actmon_dev *dev = container_of(
		nb, struct actmon_dev, rate_change_nb);

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->actmon_dev_post_change_rate)
		dev->cur_freq = dev->actmon_dev_post_change_rate(dev, v) /
						1000;
	else
		dev->cur_freq = rate / 1000;

	dev_dbg(mon_dev, "rate:%lu\n", dev->cur_freq);

	if (dev->type == ACTMON_FREQ_SAMPLER) {
		actmon_dev_wmark_set(dev);
		actmon_wmb();
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return NOTIFY_OK;
};

static int actmon_dev_parse_dt(struct actmon_dev *dev,
		struct platform_device *pdev)
{
	int ret = 0;
	const void *prop;

	ret = of_property_read_u32(dev->dn, "nvidia,reg_offs", &dev->reg_offs);
	if (ret) {
		dev_err(mon_dev, "<nvidia,reg_offs> property is not provided for the device %s\n",
			dev->dn->name);
			ret = -EINVAL;
		goto err_out;
	}

	ret = of_property_read_u32(dev->dn, "nvidia,irq_mask",
			&dev->glb_status_irq_mask);
	if (ret) {
		dev_err(mon_dev, "<nvidia,irq_mask> property is not provided for the device %s\n",
			dev->dn->name);
			ret = -EINVAL;
		goto err_out;
	}

	prop = of_get_property(dev->dn, "nvidia,suspend_freq", NULL);
	if (prop)
		dev->suspend_freq = of_read_ulong(prop, 1);
	else {
		dev->suspend_freq = DEFAULT_SUSPEND_FREQ;
		dev_info(mon_dev, "<nvidia,suspend_freq> property is not provided by the device %s setting up default value:%lu\n",
			dev->dn->name, dev->suspend_freq);
	}

	prop = of_get_property(dev->dn, "nvidia,boost_freq_step", NULL);
	if (prop)
		dev->boost_freq_step = of_read_ulong(prop, 1);
	else {
		dev->boost_freq_step = EMC_PLLP_FREQ_MAX;
		dev_info(mon_dev, "<nvidia,boost_freq_step> property is not provided for the device %s setting up default value:%lu\n",
			dev->dn->name, dev->boost_freq_step);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,boost_up_coef",
			&dev->boost_up_coef);
	if (ret) {
		dev->boost_up_coef = DEFAULT_BOOST_UP_COEF;
		dev_info(mon_dev, "<nvidia,boost_up_coef> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->boost_up_coef);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,boost_down_coef",
			&dev->boost_down_coef);
	if (ret) {
		dev->boost_down_coef = DEFAULT_BOOST_DOWN_COEF;
		dev_info(mon_dev, "<nvidia,boost_down_coef> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->boost_down_coef);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,boost_up_threshold",
			&dev->boost_up_threshold);
	if (ret) {
		dev->boost_up_threshold = DEFAULT_BOOST_UP_THRESHOLD;
		dev_info(mon_dev, "<nvidia,boost_up_threshold> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->boost_up_threshold);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,boost_down_threshold",
			&dev->boost_down_threshold);
	if (ret) {
		dev->boost_down_threshold = DEFAULT_BOOST_DOWN_THRESHOLD;
		dev_info(mon_dev, "<nvidia,boost_down_threshold> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->boost_down_threshold);
	}

	ret = of_property_read_u8(dev->dn, "nvidia,up_wmark_window",
			&dev->up_wmark_window);
	if (ret) {
		dev->up_wmark_window = DEFAULT_UP_WMARK_WINDOW;
		dev_info(mon_dev, "<nvidia,up_wmark_window> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->up_wmark_window);
	}

	ret = of_property_read_u8(dev->dn, "nvidia,down_wmark_window",
			&dev->down_wmark_window);
	if (ret) {
		dev->down_wmark_window = DEFAULT_DOWN_WMARK_WINDOW;
		dev_info(mon_dev, "<nvidia,down_wmark_window> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->down_wmark_window);
	}

	ret = of_property_read_u8(dev->dn, "nvidia,avg_window_log2",
			&dev->avg_window_log2);
	if (ret) {
		dev->avg_window_log2 = DEFAULT_EWMA_COEF_K;
		dev_info(mon_dev, "<nvidia,avg_window_log2> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->avg_window_log2);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,count_weight",
			&dev->count_weight);
	if (ret) {
		dev->count_weight = DEFAULT_COUNT_WEIGHT;
		dev_info(mon_dev, "<nvidia,count_weight> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->count_weight);
	}

	ret = of_property_read_u32(dev->dn, "nvidia,type",
			&dev->type);
	if (ret) {
		dev->type = DEFAULT_ACTMON_TYPE;
		dev_info(mon_dev, "<nvidia,type> property is not provided for the device %s setting up default value:%u\n",
			dev->dn->name, dev->type);
	}
	return 0;

err_out:
		return ret;

}

static int __init actmon_dev_init(struct actmon_dev *dev,
		struct platform_device *pdev)
{
	unsigned long freq;
	int ret = 0;

	spin_lock_init(&dev->lock);

	ret = actmon_dev_parse_dt(dev, pdev);
	if (ret) {
		dev_err(mon_dev, "nvidia, type property is not provided for the device %s\n",
			dev->dn->name);
			ret = -EINVAL;
		goto err_out;
	}
	dev->state = ACTMON_UNINITIALIZED;

	/* By default register rate change notifier */
	dev->rate_change_nb.notifier_call = actmon_rate_notify_cb;

	ret = actmon_dev_platform_register(dev, pdev);
	if (ret) {
		dev_err(mon_dev, "actmon device %s platform initialization failed\n",
			dev->dn->name);
			ret = -EINVAL;
		goto err_out;
	}

	freq = dev->actmon_dev_get_rate(dev) / 1000;

	ret = actmon_dev_configure(dev, freq);
	if (ret) {
		dev_err(mon_dev, "Failed %s configuration\n",
			dev_name(&pdev->dev));
		return ret;
	}

	ret = devm_request_threaded_irq(&pdev->dev,
			actmon->virq, actmon_dev_isr, actmon_dev_fn,
			IRQ_TYPE_LEVEL_HIGH, dev_name(&pdev->dev), dev);
	if (ret) {
		dev_err(mon_dev, "Failed irq %d request for.%s\n",
		actmon->virq, dev_name(&pdev->dev));
		goto err_out;
	}

	disable_irq(actmon->virq);
	dev->state = ACTMON_OFF;
	actmon_dev_enable(dev);
	return 0;

err_out:
	if (actmon->dev_free_resource)
		actmon->dev_free_resource(dev, pdev);

	return ret;
}

static int actmon_clock_disable(struct platform_device *pdev)
{
	if (actmon->clock_deinit)
		return actmon->clock_deinit(pdev);
	else
		return -EINVAL;
}

static int actmon_clock_enable(struct platform_device *pdev)
{
	int ret = -EINVAL;

	if (actmon->clock_init)
		ret = actmon->clock_init(pdev);

	return ret;
}
static int __init actmon_map_resource(struct platform_device *pdev)
{
	struct resource *res = NULL;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(mon_dev, "Failed to get resource for actmon device\n");
			ret = -EINVAL;
			goto err_out;
	}

	actmon->base = devm_ioremap_resource(mon_dev, res);
	if (IS_ERR(actmon->base)) {
		ret = PTR_ERR(actmon->base);
		dev_err(mon_dev, "Failed to iomap resource reg 0 %d:\n", ret);
		goto err_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
			dev_err(mon_dev, "Failed to get virtual irq for actmon interrupt\n");
			ret = -EINVAL;
			goto err_out;
		}
	actmon->virq = res->start;
	return 0;

err_out:
	if (actmon->base)
		devm_iounmap(mon_dev, actmon->base);
	return ret;
}

static inline int actmon_reset_init(struct platform_device *pdev)
{
	int ret = -EINVAL;

	if (actmon->reset_init)
		ret = actmon->reset_init(pdev);

	return ret;
}
static int actmon_set_sample_prd(struct platform_device *pdev)
{
	int ret = 0;

	ret = of_property_read_u8(mon_dev->of_node, "nvidia,sample_period",
			&actmon->sample_period);
	if (ret) {
		actmon->sample_period = ACTMON_DEFAULT_SAMPLING_PERIOD;
		dev_err(mon_dev, "<nvidia,sample_period> property is not provided for the device %s setting up default value:%u\n",
			mon_dev->of_node->name, actmon->sample_period);
		ret = 0;
	}

	if (!actmon->ops.set_sample_prd)
		ret = -EINVAL;
	else
		actmon->ops.set_sample_prd(actmon->sample_period - 1,
			actmon->base);

	return ret;
}
static int actmon_reset_deinit(struct platform_device *pdev)
{
	int ret = -EINVAL;

	if (actmon->reset_deinit)
		ret = actmon->reset_deinit(pdev);

	return ret;
}

static void actmon_free_resource(struct platform_device *pdev)
{
	int i;

	actmon_reset_deinit(pdev);
	actmon_clock_disable(pdev);

	if (actmon->base)
		devm_iounmap(mon_dev, actmon->base);

	for (i = 0; i < MAX_DEVICES; i++) {
		if (actmon->devices[i].dn)
			sysfs_remove_file(actmon->actmon_kobj,
				&actmon->devices[i].avgact_attr.attr);

		actmon->dev_free_resource(&actmon->devices[i], pdev);
	}
	devm_kfree(mon_dev, actmon);
}

static int __init actmon_probe(struct platform_device *pdev)
{
	struct device_node *dn = NULL;
	int ret = 0;
	u32 i;

	mon_dev = &pdev->dev;
	dev_info(mon_dev, "in probe()...\n");

	actmon = devm_kzalloc(mon_dev, sizeof(*actmon),
				GFP_KERNEL);
	if (!actmon)
		return -ENOMEM;

	platform_set_drvdata(pdev, actmon);
	actmon->pdev = pdev;

	ret = actmon_map_resource(pdev);
	if (ret)
		goto err_out;

	ret = actmon_platform_register(pdev);
	if (ret)
		goto err_out;

	ret = actmon_clock_enable(pdev);
	if (ret)
		goto err_out;

	ret = actmon_reset_init(pdev);
	if (ret)
		goto err_out;

	ret = actmon_set_sample_prd(pdev);
	if (ret)
		goto err_out;

	if (actmon->ops.set_glb_intr)
		actmon->ops.set_glb_intr(0xff, actmon->base);

	actmon->actmon_kobj = kobject_create_and_add("actmon_avg_activity",
					kernel_kobj);
	if (!actmon->actmon_kobj) {
		pr_err("%s: Couldn't create avg_actv kobj\n",
				__func__);
	}

	for (i = 0; i < MAX_DEVICES; i++) {
		dn = of_get_next_available_child(pdev->dev.of_node, dn);
		if (!dn)
			break;
		actmon->devices[i].dn = dn;
		/* Initialize actmon devices */
		ret = actmon_dev_init(&actmon->devices[i], pdev);
		dev_info(mon_dev, "initialization %s for the device %s\n",
				ret ? "Failed" : "Completed", dn->name);
		if (ret)
			goto err_out;

		sysfs_attr_init(&actmon->devices[i].avgact_attr.attr);
		actmon->devices[i].avgact_attr.attr.name = dn->name;
		actmon->devices[i].avgact_attr.attr.mode = S_IRUGO;
		actmon->devices[i].avgact_attr.show = avgactv_show;

		ret = sysfs_create_file(actmon->actmon_kobj,
			&actmon->devices[i].avgact_attr.attr);
		if (ret) {
			pr_err("%s: Couldn't create avg_actv files\n",
				__func__);
		}
	}

#ifdef CONFIG_DEBUG_FS
	ret = actmon_debugfs_init();
	if (ret)
		goto err_out;
#endif
	return 0;
err_out:
	actmon_free_resource(pdev);
	return ret;
}

static int actmon_remove(struct platform_device *pdev)
{
	actmon_free_resource(pdev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id actmon_of_match[] = {
	{ .compatible = "nvidia,tegra186-cactmon", .data = NULL, },
	{ .compatible = "nvidia,tegra210-cactmon", .data = NULL, },
	{},
};
#endif

static struct platform_driver actmon_driver __refdata = {
	.driver	= {
		.name	= "tegra_central_actmon",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(actmon_of_match),
	},
	.probe		= actmon_probe,
	.remove		= actmon_remove,
};

module_platform_driver(actmon_driver);

MODULE_AUTHOR("Nvidia");
MODULE_DESCRIPTION("central actmon driver for Nvidia Tegra soc");
MODULE_LICENSE("GPL");
