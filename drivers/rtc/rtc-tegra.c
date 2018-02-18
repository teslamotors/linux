/*
 * drivers/rtc/rtc-tegra.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/rtc-tegra.h>
#include <linux/rtc.h>
#define CREATE_TRACE_POINTS
#include <trace/events/tegra_rtc.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <asm/io.h>
#include <asm/mach/time.h>

/* set to 1 = busy every eight 32kHz clocks during copy of sec+msec to AHB */
#define TEGRA_RTC_REG_BUSY			0x004
#define TEGRA_RTC_REG_SECONDS			0x008
#define TEGRA_RTC_REG_SHADOW_SECONDS            0x00c
#define TEGRA_RTC_REG_MILLI_SECONDS             0x010
#define TEGRA_RTC_REG_SECONDS_ALARM0            0x014
#define TEGRA_RTC_REG_SECONDS_ALARM1            0x018
#define TEGRA_RTC_REG_MILLI_SECONDS_ALARM0      0x01c
#define TEGRA_RTC_REG_MSEC_CDN_ALARM0		0x024
#define TEGRA_RTC_REG_INTR_MASK			0x028
/* write 1 bits to clear status bits */
#define TEGRA_RTC_REG_INTR_STATUS		0x02c

/* bits in INTR_MASK */
#define TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM       (1<<3)
#define TEGRA_RTC_INTR_MASK_MSEC_ALARM          (1<<2)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM1          (1<<1)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM0          (1<<0)

/* bits in INTR_STATUS */
#define TEGRA_RTC_INTR_STATUS_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_STATUS_SEC_CDN_ALARM     (1<<3)
#define TEGRA_RTC_INTR_STATUS_MSEC_ALARM        (1<<2)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM1        (1<<1)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM0        (1<<0)

/* Reference selection */
#define TEGRA_RTC_RTCRSR			0x038
#define TEGRA_RTC_RTCRSR_FR			(1<<0)
#define TEGRA_RTC_RTCRSR_MBS(x)			(((x) & 3) << 4)
#define TEGRA_RTC_RTCDR				0x03c
#define TEGRA_RTC_RTCDR_D(x)			(((x) & 0xffff) << 16)
#define TEGRA_RTC_RTCDR_N(x)			((x) & 0xffff)

/* Recommended values for reference and dividor */
/* RTC follows MTSC bit 11 (9+2) */
#define TEGRA_RTC_RTCRSR_USE_MTSC		(0x20)
/* N=1024 D=15625 assuming FNOM=31250 program n-1 */
#define TEGRA_RTC_RTCDR_USE_MTSC		(0x3D0803ff)


struct tegra_rtc_chip_data {
	bool has_clock;
	bool follow_tsc;
};

struct tegra_rtc_data {
	struct platform_device	*pdev;
	struct rtc_device	*rtc;
	void __iomem		*base;
	int			irq;
	const struct tegra_rtc_chip_data *cdata;
};

static struct tegra_rtc_data *tegra_rtc_dev;

/*
 * tegra_rtc_read - Reads the Tegra RTC registers
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
u64 tegra_rtc_read_ms(void)
{
	u32 ms = readl(tegra_rtc_dev->base + RTC_MILLISECONDS);
	u32 s = readl(tegra_rtc_dev->base + RTC_SHADOW_SECONDS);

	return (u64)s * MSEC_PER_SEC + ms;
}
EXPORT_SYMBOL(tegra_rtc_read_ms);

/* RTC hardware is busy when it is updating its values over AHB once
 * every eight 32kHz clocks (~250uS).
 * outside of these updates the CPU is free to write.
 * CPU is always free to read.
 */
static inline u32 tegra_rtc_check_busy(void)
{
	return readl(tegra_rtc_dev->base + TEGRA_RTC_REG_BUSY) & 1;
}

/* Wait for hardware to be ready for writing.
 * This function tries to maximize the amount of time before the next update.
 * It does this by waiting for the RTC to become busy with its periodic update,
 * then returning once the RTC first becomes not busy.
 * This periodic update (where the seconds and milliseconds are copied to the
 * AHB side) occurs every eight 32kHz clocks (~250uS).
 * The behavior of this function allows us to make some assumptions without
 * introducing a race, because 250uS is plenty of time to read/write a value.
 */
static int tegra_rtc_wait_while_busy(void)
{
	int retries = 500; /* ~490 us is the worst case, ~250 us is best. */

	/* first wait for the RTC to become busy. this is when it
	 * posts its updated seconds+msec registers to AHB side. */
	while (tegra_rtc_check_busy()) {
		if (!retries--)
			goto retry_failed;
		udelay(1);
	}

	/* now we have about 250 us to manipulate registers */
	return 0;

retry_failed:
	pr_err("Tegra RTC: write failed:retry count exceeded.\n");
	return -ETIMEDOUT;
}

static int tegra_rtc_sec_alarm0_irq_enable(struct tegra_rtc_data *rtc,
					unsigned int enabled)
{
	unsigned int mask;
	int ret;

	ret = tegra_rtc_wait_while_busy();
	if (ret < 0) {
		dev_err(&rtc->pdev->dev, "Timeout accessing Tegra RTC\n");
		return ret;
	}
	mask = readl(rtc->base + TEGRA_RTC_REG_INTR_MASK);
	if (enabled)
		mask |= TEGRA_RTC_INTR_MASK_SEC_ALARM0;
	else
		mask &= ~TEGRA_RTC_INTR_MASK_SEC_ALARM0;
	writel(mask, rtc->base + TEGRA_RTC_REG_INTR_MASK);

	return 0;
}

static int tegra_rtc_msec_alarm_irq_enable(unsigned int enable)
{
	u32 status;

	/* read the original value, and OR in the flag. */
	status = readl(tegra_rtc_dev->base + TEGRA_RTC_REG_INTR_MASK);
	if (enable)
		status |= TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM; /* set it */
	else
		status &= ~TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM; /* clear it */

	writel(status, tegra_rtc_dev->base + TEGRA_RTC_REG_INTR_MASK);

	return 0;
}

static irqreturn_t tegra_rtc_interrupt(int irq, void *dev_id)
{
	struct tegra_rtc_data *rtc = dev_id;
	u32 status, mask;
	unsigned int sec;
	unsigned int msec;
	int ret;

	tegra_rtc_sec_alarm0_irq_enable(rtc, 0);
	msec = readl(rtc->base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(rtc->base + TEGRA_RTC_REG_SHADOW_SECONDS);
	trace_printk("%s: irq time %lu\n", __func__, sec * MSEC_PER_SEC + msec);

	status = readl(rtc->base + TEGRA_RTC_REG_INTR_STATUS);
	mask = readl(rtc->base + TEGRA_RTC_REG_INTR_MASK);
	mask &= ~status;
	if (status) {
		/* clear the interrupt masks and status on any irq. */
		ret = tegra_rtc_wait_while_busy();
		if (ret) {
			pr_err("Timeout accessing Tegra RTC\n");
			return ret;
		}
		writel(mask, rtc->base + TEGRA_RTC_REG_INTR_MASK);
		writel(status, rtc->base + TEGRA_RTC_REG_INTR_STATUS);
	}

	if (status & TEGRA_RTC_INTR_STATUS_SEC_ALARM0)
		rtc_aie_update_irq(rtc->rtc);
	else
		rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_UF);

	return IRQ_HANDLED;
}

void tegra_rtc_set_trigger(unsigned long cycles)
{
	unsigned long msec;
	unsigned long now;
	unsigned long tgt;
	int ret;

	/* Convert to msec */
	msec = cycles / 1000;

	if (msec)
		msec = 0x80000000UL | (0x0fffffff & msec);

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		pr_err("Timeout accessing Tegra RTC\n");
		return;
	}
	now = readl(tegra_rtc_dev->base + TEGRA_RTC_REG_MILLI_SECONDS);
	now += readl(tegra_rtc_dev->base + TEGRA_RTC_REG_SHADOW_SECONDS) *
						MSEC_PER_SEC;
	tgt = now + cycles / 1000;

	writel(msec, tegra_rtc_dev->base + TEGRA_RTC_REG_MSEC_CDN_ALARM0);
	trace_tegra_rtc_set_alarm(now, tgt);

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		pr_err("Timeout accessing Tegra RTC\n");
		return;
	}

	if (msec)
		tegra_rtc_msec_alarm_irq_enable(1);
	else
		tegra_rtc_msec_alarm_irq_enable(0);
}
EXPORT_SYMBOL(tegra_rtc_set_trigger);

static int tegra_rtc_read_time(struct device *dev, struct rtc_time *time)
{
	struct tegra_rtc_data *rtc = dev_get_drvdata(dev);
	unsigned long sec;
	int ret;

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}
	/* Update to Shadow registers in APB can take upto 250us
	 * after register write is complete. Adding delay to
	 * avoid race condition
	 */
	udelay(260);
	sec = readl(rtc->base + TEGRA_RTC_REG_SECONDS);
	rtc_time_to_tm(sec, time);

	return 0;
}

static int tegra_rtc_set_time(struct device *dev, struct rtc_time *time)
{
	struct tegra_rtc_data *rtc = dev_get_drvdata(dev);
	unsigned long period;
	int ret;

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}
	rtc_tm_to_time(time, &period);
	writel(period, rtc->base + TEGRA_RTC_REG_SECONDS);

	return 0;
}

static int tegra_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct tegra_rtc_data *rtc = dev_get_drvdata(dev);
	unsigned long sec_alarm;
	unsigned int mask;
	int ret;

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}
	sec_alarm = readl(rtc->base + TEGRA_RTC_REG_SECONDS_ALARM0);
	rtc_time_to_tm(sec_alarm, &wkalrm->time);

	mask = readl(rtc->base + TEGRA_RTC_REG_INTR_MASK);
	if (mask & TEGRA_RTC_INTR_MASK_SEC_ALARM0)
		wkalrm->enabled = 1;
	else
		wkalrm->enabled = 0;

	return 0;
}

static int __tegra_rtc_set_alarm(unsigned long period, bool enabled)
{
	unsigned int sec, msec;
	int ret;
	struct device *dev = &tegra_rtc_dev->pdev->dev;

	ret = tegra_rtc_wait_while_busy();
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}
	msec = readl(tegra_rtc_dev->base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(tegra_rtc_dev->base + TEGRA_RTC_REG_SHADOW_SECONDS);
	if (period < sec)
		dev_warn(dev, "alarm time set in past\n");

	writel(period, tegra_rtc_dev->base + TEGRA_RTC_REG_SECONDS_ALARM0);

	ret = tegra_rtc_sec_alarm0_irq_enable(tegra_rtc_dev, enabled);
	if (ret < 0) {
		dev_err(dev,
			"rtc_set_alarm: Failed to enable rtc alarm\n");
		return ret;
	}

	trace_tegra_rtc_set_alarm(sec * MSEC_PER_SEC + msec,
				period * MSEC_PER_SEC);
	dev_dbg(dev, "alarm set to fire after %lu sec\n", (period - sec));

	return 0;
}

static int tegra_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	unsigned long period;

	rtc_tm_to_time(&wkalrm->time, &period);
	return __tegra_rtc_set_alarm(period, wkalrm->enabled);
}

static void tegra_rtc_debug_set_alarm(unsigned int period)
{
	unsigned int sec;
	int ret;

	sec = readl(tegra_rtc_dev->base + TEGRA_RTC_REG_SECONDS);
	ret = __tegra_rtc_set_alarm((sec + period), 1);
	if (ret < 0)
		pr_err("Tegra RTC: setting debug alarm failed\n");
}

static int tegra_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct tegra_rtc_data *rtc = dev_get_drvdata(dev);

	return tegra_rtc_sec_alarm0_irq_enable(rtc, enabled);
}
static const struct rtc_class_ops tegra_rtc_ops = {
	.read_time = tegra_rtc_read_time,
	.set_time = tegra_rtc_set_time,
	.read_alarm = tegra_rtc_read_alarm,
	.set_alarm = tegra_rtc_set_alarm,
	.alarm_irq_enable = tegra_rtc_alarm_irq_enable,
};

static unsigned int alarm_period;
static unsigned int alarm_period_msec;

static int tegra_debug_pm_suspend(void)
{
	if (alarm_period)
		tegra_rtc_debug_set_alarm(alarm_period);

	if (alarm_period_msec)
		tegra_rtc_set_trigger(alarm_period_msec * 1000);

	if (alarm_period || alarm_period_msec ||
			device_may_wakeup(&tegra_rtc_dev->pdev->dev))
		enable_irq_wake(tegra_rtc_dev->irq);

	return 0;
}

static void tegra_debug_pm_resume(void)
{
	if (alarm_period || alarm_period_msec ||
			device_may_wakeup(&tegra_rtc_dev->pdev->dev))
		disable_irq_wake(tegra_rtc_dev->irq);
}

static struct syscore_ops tegra_debug_pm_syscore_ops = {
	.suspend = tegra_debug_pm_suspend,
	.resume = tegra_debug_pm_resume,
};

static int alarm_set(void *data, u64 val)
{
	alarm_period = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(alarm_fops, NULL, alarm_set, "%llu\n");

static int alarm_msec_set(void *data, u64 val)
{
	alarm_period_msec = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(alarm_msec_fops, NULL, alarm_msec_set, "%llu\n");

static struct dentry *pm_dentry;

static int debugfs_init(void)
{
	struct dentry *root = NULL;

	root = debugfs_create_dir("tegra-rtc", NULL);
	if (!root)
		return -ENOMEM;

	if (!debugfs_create_file("alarm", S_IWUSR, root, NULL, &alarm_fops))
		goto err_out;

	if (!debugfs_create_file("alarm_msec", S_IWUSR, root, NULL,
							&alarm_msec_fops))
		goto err_out;

	pm_dentry = root;
	return 0;

err_out:
	debugfs_remove_recursive(root);
	return -ENOMEM;
}

/*
 * tegra_read_persistent_clock -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
static void tegra_rtc_read_persistent_clock(struct timespec *ts)
{
	ts->tv_nsec = NSEC_PER_MSEC *
			readl(tegra_rtc_dev->base + RTC_MILLISECONDS);
	ts->tv_sec = readl(tegra_rtc_dev->base + RTC_SHADOW_SECONDS);
}

static struct tegra_rtc_chip_data t18x_rtc_cdata = {
	.has_clock = false,
	.follow_tsc = true,
};

static struct tegra_rtc_chip_data tegra_rtc_cdata = {
	.has_clock = true,
	.follow_tsc = false,
};

static const struct of_device_id tegra_rtc_of_match[] = {
	{
		.compatible = "nvidia,tegra-rtc",
		.data = &tegra_rtc_cdata,
	},
	{
		.compatible = "nvidia,tegra18-rtc",
		.data = &t18x_rtc_cdata,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_rtc_of_match);

static void tegra_rtc_follow_tsc(struct tegra_rtc_data *rtc)
{
	int ret;

	if(!rtc->cdata->follow_tsc)
		return;

	ret = tegra_rtc_wait_while_busy();
	if (ret < 0) {
		dev_err(&rtc->pdev->dev, "Timeout accessing Tegra RTC\n");
		return;
	}
	writel(TEGRA_RTC_RTCDR_USE_MTSC, rtc->base +
						TEGRA_RTC_RTCDR);
	ret = tegra_rtc_wait_while_busy();
	if (ret < 0) {
		dev_err(&rtc->pdev->dev, "Timeout accessing Tegra RTC\n");
		return;
	}
	writel(TEGRA_RTC_RTCRSR_USE_MTSC, rtc->base +
						TEGRA_RTC_RTCRSR);
}

static int tegra_rtc_probe(struct platform_device *pdev)
{
	static struct tegra_rtc_data *tegra_rtc;
	struct resource	*res;
	const struct of_device_id *match;
	struct clk *clk;
	int ret = 0;

	tegra_rtc = devm_kzalloc(&pdev->dev, sizeof(*tegra_rtc), GFP_KERNEL);
	if (!tegra_rtc)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, tegra_rtc);
	tegra_rtc->pdev = pdev;

	if (pdev->dev.of_node) {
		match = of_match_device(tegra_rtc_of_match, &pdev->dev);
		if (match)
			tegra_rtc->cdata = match->data;
	}
	/*
	 * rtc registers are used by read_persistent_clock, keep the rtc clock
	 * enabled
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No IO memory resource\n");
		return -ENODEV;
	}
	tegra_rtc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tegra_rtc->base)) {
		dev_err(&pdev->dev, "Cannot map registers\n");
		return PTR_ERR(tegra_rtc->base);
	}
	tegra_rtc_dev = tegra_rtc;

	tegra_rtc->irq = platform_get_irq(pdev, 0);
	if (tegra_rtc->irq <= 0) {
		dev_err(&pdev->dev, "failed to get interrupt\n");
		return -ENXIO;
	}

	if(tegra_rtc->cdata->has_clock) {
		clk = devm_clk_get(&pdev->dev, "rtc");
		if (IS_ERR(clk))
			clk = clk_get_sys("rtc-tegra", NULL);
		if (IS_ERR(clk))
			dev_warn(&pdev->dev, "Unable to get rtc-tegra clock\n");
		else
			clk_prepare_enable(clk);
	}

	/* clear out the hardware. */
	writel(0, tegra_rtc->base + TEGRA_RTC_REG_MSEC_CDN_ALARM0);
	writel(0xffffffff, tegra_rtc->base + TEGRA_RTC_REG_INTR_STATUS);
	writel(0, tegra_rtc->base + TEGRA_RTC_REG_INTR_MASK);

	tegra_rtc_follow_tsc(tegra_rtc);

	ret = devm_request_threaded_irq(&pdev->dev, tegra_rtc->irq,
					NULL, tegra_rtc_interrupt,
					IRQF_ONESHOT | IRQF_EARLY_RESUME,
					"tegra_rtc", tegra_rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register RTC IRQ: %d\n", ret);
		/* system cannot proceed without rtc */
		BUG();
	}
	device_init_wakeup(&pdev->dev, 1);

	tegra_rtc->rtc = devm_rtc_device_register(&pdev->dev, "tegra-rtc",
				       &tegra_rtc_ops, THIS_MODULE);
	if (IS_ERR_OR_NULL(tegra_rtc->rtc)) {
		dev_err(&pdev->dev, "probe: Failed to register rtc\n");
		return PTR_ERR(tegra_rtc->rtc);
	}

	register_syscore_ops(&tegra_debug_pm_syscore_ops);
	ret = debugfs_init();
	if (ret) {
		pr_err("%s: Can't init debugfs", __func__);
		BUG();
	}
	register_persistent_clock(NULL, tegra_rtc_read_persistent_clock);

	return 0;
}

static int tegra_rtc_remove(struct platform_device *pdev)
{
	unregister_syscore_ops(&tegra_debug_pm_syscore_ops);
	debugfs_remove_recursive(pm_dentry);

	return 0;
}

static struct platform_driver tegra_rtc_driver = {
	.probe = tegra_rtc_probe,
	.remove = tegra_rtc_remove,
	.driver = {
			.name = "tegra-rtc",
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(tegra_rtc_of_match),
	},
};
module_platform_driver(tegra_rtc_driver);
