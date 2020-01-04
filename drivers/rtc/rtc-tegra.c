/*
 * drivers/rtc/rtc-tegra.c
 *
 * An RTC driver for the NVIDIA Tegra 200 series internal RTC.
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 * Copyright (c) 2010 Jon Mayo <jmayo@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

/* how many attempts to wait in tegra_rtc_wait_while_busy(). */
#define RTC_TEGRA_RETRIES 15

#define tegra_rtc_read(ofs)		readl(rtc_base + (ofs))
#define tegra_rtc_write(ofs, val)	writel((val), rtc_base + (ofs))

/* STATUS: This bit is set when a write is initiated on the APB side. It is
 * cleared once the write completes in RTC 32KHz clock domain which could be
 * several thousands of APB clocks. This must be IDLE before a write is
 * initiated. Note that this bit is only for writes.
 * 0 = IDLE
 * 1 = BUSY
 */
#define RTC_TEGRA_REG_BUSY			0x004
#define RTC_TEGRA_REG_SECONDS			0x008
#define RTC_TEGRA_REG_SHADOW_SECONDS		0x00c
#define RTC_TEGRA_REG_MILLI_SECONDS		0x010
#define RTC_TEGRA_REG_SECONDS_ALARM0		0x014
#define RTC_TEGRA_REG_SECONDS_ALARM1		0x018
#define RTC_TEGRA_REG_MILLI_SECONDS_ALARM0	0x01c
#define RTC_TEGRA_REG_INTR_MASK			0x028
/* a write to this register performs a clear. reg=reg&(~x) */
#define RTC_TEGRA_REG_INTR_STATUS		0x02c

/* bits in INTR_MASK */
#define RTC_TEGRA_INTR_MASK_MSEC_CDN_ALARM (1<<4)
#define RTC_TEGRA_INTR_MASK_SEC_CDN_ALARM (1<<3)
#define RTC_TEGRA_INTR_MASK_MSEC_ALARM (1<<2)
#define RTC_TEGRA_INTR_MASK_SEC_ALARM1 (1<<1)
#define RTC_TEGRA_INTR_MASK_SEC_ALARM0 (1<<0)

/* bits in INTR_STATUS */
#define RTC_TEGRA_INTR_STATUS_MSEC_CDN_ALARM (1<<4)
#define RTC_TEGRA_INTR_STATUS_SEC_CDN_ALARM (1<<3)
#define RTC_TEGRA_INTR_STATUS_MSEC_ALARM (1<<2)
#define RTC_TEGRA_INTR_STATUS_SEC_ALARM1 (1<<1)
#define RTC_TEGRA_INTR_STATUS_SEC_ALARM0 (1<<0)

static struct rtc_device *rtc_dev;
static DEFINE_SPINLOCK(tegra_rtc_lock);
static void __iomem *rtc_base; /* NULL if not initialized. */
static int tegra_rtc_irq; /* alarm and periodic interrupt */

/* check is hardware is accessing APB. */
static inline u32 tegra_rtc_check_busy(void)
{
	return tegra_rtc_read(RTC_TEGRA_REG_BUSY);
}

/* wait for hardware to be ready for writing.
 * do not call this inside the spin lock because it sleeps.
 */
static int tegra_rtc_wait_while_busy(struct device *dev)
{
	/* TODO: wait for busy then not busy to catch a leading edge. */
	int retries = RTC_TEGRA_RETRIES;
	while (tegra_rtc_check_busy()) {
		if (!retries--) {
			dev_err(dev, "write failed:retry count exceeded.\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}
	return 0;
}

/* waits for the RTC to not be busy accessing APB, then write a single value. */
static int tegra_rtc_write_not_busy(struct device *dev, unsigned ofs, u32 value)
{
	unsigned long sl_irq_flags;
	int ret;
	spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);
	if(tegra_rtc_check_busy()) {
		spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);
		ret = tegra_rtc_wait_while_busy(dev);
		if (ret)
			return ret;
		spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);
	}
	tegra_rtc_write(ofs, value);
	spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);
	return 0;
}


static int tegra_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long sec, msec;
	unsigned long sl_irq_flags;

	spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);

	msec = tegra_rtc_read(RTC_TEGRA_REG_MILLI_SECONDS);
	sec = tegra_rtc_read(RTC_TEGRA_REG_SHADOW_SECONDS);

	spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);

	rtc_time_to_tm(sec, tm);

	dev_vdbg(dev, "time read as %lu. %d/%d/%d %d:%02u:%02u\n",
		sec,
		tm->tm_mon+1,
		tm->tm_mday,
		tm->tm_year+1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec
	);

	return 0;
}

static int tegra_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long sec;
	int ret;

	/* convert tm to seconds. */
	ret = rtc_valid_tm(tm);
	if (ret) return ret;
	rtc_tm_to_time(tm, &sec);

	dev_vdbg(dev, "time set to %lu. %d/%d/%d %d:%02u:%02u\n",
		sec,
		tm->tm_mon+1,
		tm->tm_mday,
		tm->tm_year+1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec
	);

	/* seconds only written if wait succeeded. */
	ret = tegra_rtc_write_not_busy(dev, RTC_TEGRA_REG_SECONDS, sec);

	dev_vdbg(
		dev, "time read back as %d\n",
		tegra_rtc_read(RTC_TEGRA_REG_SECONDS)
	);
	return ret;
}

static int tegra_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	unsigned long sec;
	unsigned long sl_irq_flags;
	unsigned tmp;

	spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);

	sec = tegra_rtc_read(RTC_TEGRA_REG_SECONDS_ALARM0);

	spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);

	if (sec == 0) {
		/* alarm is disabled. */
		t->enabled = 0;
		t->time.tm_mon = -1;
		t->time.tm_mday = -1;
		t->time.tm_year = -1;
		t->time.tm_hour = -1;
		t->time.tm_min = -1;
		t->time.tm_sec = -1;
	} else {
		/* alarm is enabled. */
		t->enabled = 1;
		rtc_time_to_tm(sec, &t->time);
	}

	tmp = tegra_rtc_read(RTC_TEGRA_REG_INTR_STATUS);
	t->pending = (tmp & RTC_TEGRA_INTR_STATUS_SEC_ALARM0) != 0;

	return 0;
}

static int tegra_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	unsigned status;
	unsigned long sl_irq_flags;
	int ret;

	spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);
	if(tegra_rtc_check_busy()) { /* wait for the busy bit to clear. */
		spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);
		ret = tegra_rtc_wait_while_busy(dev);
		if (ret)
			return ret;
		spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);
	}
	/* read the original value, and OR in the flag. */
	status = tegra_rtc_read(RTC_TEGRA_REG_INTR_MASK);
	if (enabled)
		status |= RTC_TEGRA_INTR_MASK_SEC_ALARM0; /* set it */
	else
		status &= ~RTC_TEGRA_INTR_MASK_SEC_ALARM0; /* clear it */
	tegra_rtc_write(RTC_TEGRA_REG_INTR_MASK, status);
	spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);

	return 0;
}

static int tegra_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	int ret;
	unsigned long sec;

	if (t->enabled)
		rtc_tm_to_time(&t->time, &sec);
	else
		sec = 0;

	ret = tegra_rtc_write_not_busy(dev, RTC_TEGRA_REG_SECONDS_ALARM0, sec);
	dev_vdbg(
		dev, "alarm read back as %d\n",
		tegra_rtc_read(RTC_TEGRA_REG_SECONDS_ALARM0)
	);

	/* if successfully written and alarm is enabled ... */
	if (ret == 0 && sec) {
		tegra_rtc_alarm_irq_enable(dev, 1);

		dev_vdbg(dev, "alarm set as %lu. %d/%d/%d %d:%02u:%02u\n",
			sec,
			t->time.tm_mon+1,
			t->time.tm_mday,
			t->time.tm_year+1900,
			t->time.tm_hour,
			t->time.tm_min,
			t->time.tm_sec
		);
	} else {
		/* disable alarm if 0 or write error. */
		dev_vdbg(dev, "alarm disabled\n");
		tegra_rtc_alarm_irq_enable(dev, 0);
	}

	return ret;
}

static int tegra_rtc_ioctl(
	struct device *dev, unsigned int cmd, unsigned long arg)
{
	/* use default ioctl handlers for:
	 * RTC_RD_TIME, RTC_SET_TIME, RTC_ALM_SET, RTC_ALM_READ, RTC_WKALM_SET,
	 * RTC_WKALM_RD, RTC_IRQP_SET, RTC_IRQP_READ, RTC_PIE_ON, RTC_PIE_OFF
	 */
	return -ENOIOCTLCMD;
}

/* additional proc lines. */
static int tegra_rtc_proc(struct device *dev, struct seq_file *seq)
{
	if (!dev || !dev->driver)
		return 0;
	return seq_printf(seq, "name\t\t: %s\n", dev_name(dev));
}

static irqreturn_t tegra_rtc_irq_handler(int irq, void *dev_id)
{
	unsigned long events = 0;
	unsigned status;
	unsigned long sl_irq_flags;

	status = tegra_rtc_read(RTC_TEGRA_REG_INTR_STATUS);

	if (status) {
		/* clear the interrupt masks and status on any irq. */
		spin_lock_irqsave(&tegra_rtc_lock, sl_irq_flags);
		tegra_rtc_write(RTC_TEGRA_REG_INTR_MASK, 0);
		tegra_rtc_write(RTC_TEGRA_REG_INTR_STATUS, -1);
		spin_unlock_irqrestore(&tegra_rtc_lock, sl_irq_flags);
	}

	/* check if Alarm */
	if ((status & RTC_TEGRA_INTR_STATUS_SEC_ALARM0))
		events |= RTC_IRQF | RTC_AF;

	/* check if Periodic */
	if ((status & RTC_TEGRA_INTR_STATUS_SEC_CDN_ALARM))
		events |= RTC_IRQF | RTC_PF;

	rtc_update_irq(rtc_dev, 1, events);
	return IRQ_HANDLED;
}

static struct rtc_class_ops tegra_rtc_ops = {
	.ioctl		= tegra_rtc_ioctl,
	.read_time	= tegra_rtc_read_time,
	.set_time	= tegra_rtc_set_time,
	.read_alarm	= tegra_rtc_read_alarm,
	.set_alarm	= tegra_rtc_set_alarm,
	.proc		= tegra_rtc_proc,
	.alarm_irq_enable = tegra_rtc_alarm_irq_enable,
};

static int __init tegra_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(
			&pdev->dev, "Unable to allocate resources for device.\n"
		);
		return -EBUSY;
	}

	tegra_rtc_irq = platform_get_irq(pdev, 0);
	if (tegra_rtc_irq <= 0) {
		tegra_rtc_irq = 0;
		return -EBUSY;
	}

	rtc_base = ioremap(res->start, res->end - res->start + 1);
	if (!rtc_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		return -EBUSY;
	}

	/* clear out the hardware. */
	tegra_rtc_write_not_busy(&pdev->dev, RTC_TEGRA_REG_SECONDS_ALARM0, 0);
	tegra_rtc_write_not_busy(&pdev->dev, RTC_TEGRA_REG_INTR_STATUS, -1);
	tegra_rtc_write_not_busy(&pdev->dev, RTC_TEGRA_REG_INTR_MASK, 0);

	device_init_wakeup(&pdev->dev, 1);

	rtc_dev = rtc_device_register(
		pdev->name, &pdev->dev, &tegra_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc_dev)) {
		ret = PTR_ERR(rtc_dev);
		rtc_dev = NULL;
		dev_err(&pdev->dev, "Unable to register device (err=%d).\n", ret);
		goto err_iounmap;
	}

	ret = request_irq(tegra_rtc_irq, tegra_rtc_irq_handler,
		IRQF_DISABLED, "rtc alarm", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Unable to request interrupt for device (err=%d).\n", ret);
		goto err_dev_unreg;
	}

	dev_notice(&pdev->dev, "Tegra internal Real Time Clock\n");

	return 0;

err_dev_unreg:
	rtc_device_unregister(rtc_dev);
	rtc_dev = NULL;
err_iounmap:
	iounmap(rtc_base);
	rtc_base = NULL;

	return ret;
}

static int __devexit tegra_rtc_remove(struct platform_device *pdev)
{
	if (rtc_dev) {
		rtc_device_unregister(rtc_dev);
		rtc_dev = NULL;
	}
	return 0;
}

#ifdef CONFIG_PM
static int tegra_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev=&pdev->dev;

	/* only use ALARM0 as a wake source. */
	tegra_rtc_write(RTC_TEGRA_REG_INTR_STATUS, -1);
	tegra_rtc_write(RTC_TEGRA_REG_INTR_MASK,
		RTC_TEGRA_INTR_STATUS_SEC_ALARM0);
	dev_vdbg(dev, "alarm sec = %d\n",
		tegra_rtc_read(RTC_TEGRA_REG_SECONDS_ALARM0));

	dev_vdbg(dev, "Suspend (device_may_wakeup=%d) irq:%d\n",
		device_may_wakeup(dev), tegra_rtc_irq);
	/* leave the alarms on as a wake source. */
	if (device_may_wakeup(dev))
		enable_irq_wake(tegra_rtc_irq);
	return 0;
}

static int tegra_rtc_resume(struct platform_device *pdev)
{
	struct device *dev=&pdev->dev;
	unsigned int intr_status;

	/* clear */
	intr_status = tegra_rtc_read(RTC_TEGRA_REG_INTR_STATUS);
	if (intr_status & RTC_TEGRA_INTR_STATUS_SEC_ALARM0) {
		tegra_rtc_write_not_busy(dev, RTC_TEGRA_REG_INTR_MASK, 0);
		tegra_rtc_write_not_busy(dev, RTC_TEGRA_REG_INTR_STATUS, -1);
	}

	dev_vdbg(dev, "Resume (device_may_wakeup=%d)\n", device_may_wakeup(dev));
	/* alarms were left on as a wake source, turn them off. */
	if (device_may_wakeup(dev))
		disable_irq_wake(tegra_rtc_irq);
	return 0;
}
#endif

static void tegra_rtc_shutdown(struct platform_device *pdev)
{
	dev_vdbg(&pdev->dev, "disabling interrupts.\n");
	tegra_rtc_alarm_irq_enable(&pdev->dev, 0);
}

static struct platform_driver tegra_rtc_driver = {
	.remove		= __devexit_p(tegra_rtc_remove),
	.shutdown	= tegra_rtc_shutdown,
	.driver		= {
		.name	= "tegra_rtc",
		.owner	= THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend	= tegra_rtc_suspend,
	.resume		= tegra_rtc_resume,
#endif
};

static int __init tegra_rtc_init(void)
{
	return platform_driver_probe(&tegra_rtc_driver, tegra_rtc_probe);
}
module_init(tegra_rtc_init);

static void __exit tegra_rtc_exit(void)
{
	platform_driver_unregister(&tegra_rtc_driver);
}
module_exit(tegra_rtc_exit);

MODULE_ALIAS("platform:tegra_rtc");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("driver for Tegra internal RTC");
MODULE_LICENSE("GPL");
