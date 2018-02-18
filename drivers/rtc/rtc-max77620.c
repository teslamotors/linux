/*
 * Max77620 RTC driver
 *
 * Copyright (C) 2014 - 2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mfd/max77620.h>

#define MAX77620_RTC60S_MASK		(1 << 0)
#define MAX77620_RTCA1_MASK		(1 << 1)
#define MAX77620_RTCA2_MASK		(1 << 2)
#define MAX77620_RTC_SMPL_MASK		(1 << 3)
#define MAX77620_RTC_RTC1S_MASK		(1 << 4)
#define MAX77620_RTC_ALL_IRQ_MASK	0x1F

#define MAX77620_BCDM_MASK		(1 << 0)
#define MAX77620_HRMODEM_MASK		(1 << 1)

#define WB_UPDATE_MASK			(1 << 0)
#define FLAG_AUTO_CLEAR_MASK		(1 << 1)
#define FREEZE_SEC_MASK			(1 << 2)
#define RTC_WAKE_MASK			(1 << 3)
#define RB_UPDATE_MASK			(1 << 4)

#define MAX77620_UDF_MASK		(1 << 0)
#define MAX77620_RBUDF_MASK		(1 << 1)

#define SEC_MASK		0x7F
#define MIN_MASK		0x7F
#define HOUR_MASK		0x3F
#define WEEKDAY_MASK		0x7F
#define MONTH_MASK		0x1F
#define YEAR_MASK		0xFF
#define MONTHDAY_MASK		0x3F

#define ALARM_EN_MASK			0x80
#define ALARM_EN_SHIFT			7

#define RTC_YEAR_BASE			100
#define RTC_YEAR_MAX			99

#define ONOFF_WK_ALARM1_MASK		(1 << 2)

enum {
	RTC_SEC,
	RTC_MIN,
	RTC_HOUR,
	RTC_WEEKDAY,
	RTC_MONTH,
	RTC_YEAR,
	RTC_MONTHDAY,
	RTC_NR
};

struct max77620_rtc {
	struct rtc_device *rtc;
	struct device *dev;

	struct mutex io_lock;
	int irq;
	u8 irq_mask;
	bool disable_time_display_in_suspend;
};

static inline struct device *_to_parent(struct max77620_rtc *rtc)
{
	return rtc->dev->parent;
}

static inline int max77620_rtc_update_buffer(struct max77620_rtc *rtc,
					     int write)
{
	struct device *parent = _to_parent(rtc);
	u8 val =  FLAG_AUTO_CLEAR_MASK | RTC_WAKE_MASK;
	int ret;

	if (write)
		val |= WB_UPDATE_MASK;
	else
		val |= RB_UPDATE_MASK;

	dev_dbg(rtc->dev, "rtc_update_buffer: write=%d, addr=0x%x, val=0x%x\n",
		write, MAX77620_REG_RTCUPDATE0, val);
	ret = max77620_reg_write(parent, MAX77620_RTC_SLAVE,
						MAX77620_REG_RTCUPDATE0, val);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_update_buffer: "
			"Failed to get rtc update0\n");
		return ret;
	}

	/* Must wait 16ms for buffer update */
	usleep_range(16000, 16000);

	return 0;
}

static inline int max77620_rtc_write(struct max77620_rtc *rtc, u8 addr,
				     void *values, u32 len, int update_buffer)
{
	struct device *parent = _to_parent(rtc);
	int ret;

	mutex_lock(&rtc->io_lock);

	dev_dbg(rtc->dev, "rtc_write: addr=0x%x, values=0x%x, len=%u, "
		"update_buffer=%d\n",
		addr, *((u8 *)values), len, update_buffer);
	ret = max77620_reg_writes(parent, MAX77620_RTC_SLAVE,
						addr, len, values);
	if (ret < 0)
		goto out;

	if (update_buffer)
		ret = max77620_rtc_update_buffer(rtc, 1);

out:
	mutex_unlock(&rtc->io_lock);
	return ret;
}

static inline int max77620_rtc_read(struct max77620_rtc *rtc, u8 addr,
				    void *values, u32 len, int update_buffer)
{
	struct device *parent = _to_parent(rtc);
	int ret;

	mutex_lock(&rtc->io_lock);

	if (update_buffer) {
		ret = max77620_rtc_update_buffer(rtc, 0);
		if (ret < 0)
			goto out;
	}

	ret = max77620_reg_reads(parent, MAX77620_RTC_SLAVE, addr, len, values);
	dev_dbg(rtc->dev, "rtc_read: addr=0x%x, values=0x%x, len=%u, "
		"update_buffer=%d\n",
		addr, *((u8 *)values), len, update_buffer);

out:
	mutex_unlock(&rtc->io_lock);
	return ret;
}

static inline int max77620_rtc_reg_to_tm(struct max77620_rtc *rtc, u8 *buf,
					 struct rtc_time *tm)
{
	int wday = buf[RTC_WEEKDAY] & WEEKDAY_MASK;

	if (unlikely(!wday)) {
		dev_err(rtc->dev,
			"rtc_reg_to_tm: Invalid day of week, %d\n", wday);
		return -EINVAL;
	}

	tm->tm_sec = (int)(buf[RTC_SEC] & SEC_MASK);
	tm->tm_min = (int)(buf[RTC_MIN] & MIN_MASK);
	tm->tm_hour = (int)(buf[RTC_HOUR] & HOUR_MASK);
	tm->tm_mday = (int)(buf[RTC_MONTHDAY] & MONTHDAY_MASK);
	tm->tm_mon = (int)(buf[RTC_MONTH] & MONTH_MASK) - 1;
	tm->tm_year = (int)(buf[RTC_YEAR] & YEAR_MASK) + RTC_YEAR_BASE;
	tm->tm_wday = ffs(wday) - 1;

	return 0;
}

static inline int max77620_rtc_tm_to_reg(struct max77620_rtc *rtc, u8 *buf,
					 struct rtc_time *tm, int alarm)
{
	u8 alarm_mask = alarm ? ALARM_EN_MASK : 0;

	if (unlikely((tm->tm_year < RTC_YEAR_BASE) ||
			(tm->tm_year > RTC_YEAR_BASE + RTC_YEAR_MAX))) {
		dev_err(rtc->dev,
			"rtc_tm_to_reg: Invalid year, %d\n", tm->tm_year);
		return -EINVAL;
	}

	buf[RTC_SEC] = tm->tm_sec | alarm_mask;
	buf[RTC_MIN] = tm->tm_min | alarm_mask;
	buf[RTC_HOUR] = tm->tm_hour | alarm_mask;
	buf[RTC_MONTHDAY] = tm->tm_mday | alarm_mask;
	buf[RTC_MONTH] = (tm->tm_mon + 1) | alarm_mask;
	buf[RTC_YEAR] = (tm->tm_year - RTC_YEAR_BASE) | alarm_mask;

	/* The wday is configured only when disabled alarm. */
	if (!alarm)
		buf[RTC_WEEKDAY] = (1 << tm->tm_wday);
	else {
	/* Configure its default reset value 0x01, and not enable it. */
		buf[RTC_WEEKDAY] = 0x01;
	}
	return 0;
}

static inline int max77620_rtc_irq_mask(struct max77620_rtc *rtc, u8 irq)
{
	u8 irq_mask = rtc->irq_mask | irq;
	int ret = 0;

	ret = max77620_rtc_write(rtc, MAX77620_REG_RTCINTM, &irq_mask, 1, 1);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_irq_mask: Failed to set rtc irq mask\n");
		goto out;
	}
	rtc->irq_mask = irq_mask;

out:
	return ret;
}

static inline int max77620_rtc_irq_unmask(struct max77620_rtc *rtc, u8 irq)
{
	u8 irq_mask = rtc->irq_mask & ~irq;
	int ret = 0;

	ret = max77620_rtc_write(rtc, MAX77620_REG_RTCINTM, &irq_mask, 1, 1);
	if (ret < 0) {
		dev_err(rtc->dev,
			"rtc_irq_unmask: Failed to set rtc irq mask\n");
		goto out;
	}
	rtc->irq_mask = irq_mask;

out:
	return ret;
}

static inline int max77620_rtc_do_irq(struct max77620_rtc *rtc)
{
	struct device *parent = _to_parent(rtc);
	u8 irq_status;
	int ret;

	ret = max77620_reg_read(parent, MAX77620_RTC_SLAVE,
					MAX77620_REG_RTCINT, &irq_status);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_irq: Failed to get rtc irq status\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_do_irq: irq_mask=0x%02x, irq_status=0x%02x\n",
		rtc->irq_mask, irq_status);

	if (!(rtc->irq_mask & MAX77620_RTCA1_MASK) &&
			(irq_status & MAX77620_RTCA1_MASK))
		rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);

	if (!(rtc->irq_mask & MAX77620_RTC_RTC1S_MASK) &&
			(irq_status & MAX77620_RTC_RTC1S_MASK))
		rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_UF);

	return ret;
}

static irqreturn_t max77620_rtc_irq(int irq, void *data)
{
	struct max77620_rtc *rtc = (struct max77620_rtc *)data;

	max77620_rtc_do_irq(rtc);

	return IRQ_HANDLED;
}

static int max77620_rtc_alarm_irq_enable(struct device *dev,
					 unsigned int enabled)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);
	int ret = 0;

	if (rtc->irq < 0)
		return -ENXIO;

	/* Handle pending interrupt */
	ret = max77620_rtc_do_irq(rtc);
	if (ret < 0)
		goto out;

	/* Config alarm interrupt */
	if (enabled) {
		ret = max77620_rtc_irq_unmask(rtc, MAX77620_RTCA1_MASK);
		if (ret < 0)
			goto out;
	} else {
		ret = max77620_rtc_irq_mask(rtc, MAX77620_RTCA1_MASK);
		if (ret < 0)
			goto out;
	}
out:
	return ret;
}

static int max77620_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_NR];
	int ret;

	ret = max77620_rtc_read(rtc, MAX77620_REG_RTCSEC, buf, sizeof(buf), 1);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_read_time: Failed to read rtc time\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_read_time: "
		"buf: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buf[RTC_SEC], buf[RTC_MIN], buf[RTC_HOUR], buf[RTC_WEEKDAY],
		buf[RTC_MONTH], buf[RTC_YEAR], buf[RTC_MONTHDAY]);

	ret = max77620_rtc_reg_to_tm(rtc, buf, tm);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_read_time: "
			"Failed to convert register format into time format\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_read_time: "
		"tm: %d-%02d-%02d %02d:%02d:%02d, wday=%d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
		tm->tm_sec, tm->tm_wday);

	return ret;
}

static int max77620_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_NR];
	int ret;

	dev_dbg(rtc->dev, "rtc_set_time: "
		"tm: %d-%02d-%02d %02d:%02d:%02d, wday=%d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min,
		tm->tm_sec, tm->tm_wday);

	ret = max77620_rtc_tm_to_reg(rtc, buf, tm, 0);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_set_time: "
			"Failed to convert time format into register format\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_set_time: "
		"buf: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buf[RTC_SEC], buf[RTC_MIN], buf[RTC_HOUR], buf[RTC_WEEKDAY],
		buf[RTC_MONTH], buf[RTC_YEAR], buf[RTC_MONTHDAY]);

	return max77620_rtc_write(rtc, MAX77620_REG_RTCSEC,
					buf, sizeof(buf), 1);
}

static int max77620_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_NR];
	int ret;

	ret = max77620_rtc_read(rtc, MAX77620_REG_RTCSECA1,
					buf, sizeof(buf), 1);
	if (ret < 0) {
		dev_err(rtc->dev,
			"rtc_read_alarm: Failed to read rtc alarm time\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_read_alarm: "
		"buf: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buf[RTC_SEC], buf[RTC_MIN], buf[RTC_HOUR], buf[RTC_WEEKDAY],
		buf[RTC_MONTH], buf[RTC_YEAR], buf[RTC_MONTHDAY]);

	buf[RTC_YEAR] &= ~ALARM_EN_MASK;
	ret = max77620_rtc_reg_to_tm(rtc, buf, &alrm->time);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_read_alarm: "
			"Failed to convert register format into time format\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_read_alarm: "
		"tm: %d-%02d-%02d %02d:%02d:%02d, wday=%d\n",
		alrm->time.tm_year, alrm->time.tm_mon, alrm->time.tm_mday,
		alrm->time.tm_hour, alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday);

	if (rtc->irq_mask & MAX77620_RTCA1_MASK)
		alrm->enabled = 0;
	else
		alrm->enabled = 1;

	return 0;
}

static int max77620_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_NR];
	int ret;

	dev_dbg(rtc->dev, "rtc_set_alarm: "
		"tm: %d-%02d-%02d %02d:%02d:%02d, wday=%d [%s]\n",
		alrm->time.tm_year, alrm->time.tm_mon, alrm->time.tm_mday,
		alrm->time.tm_hour, alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday, alrm->enabled ? "enable" : "disable");

	ret = max77620_rtc_tm_to_reg(rtc, buf, &alrm->time, 1);
	if (ret < 0) {
		dev_err(rtc->dev, "rtc_set_alarm: "
			"Failed to convert time format into register format\n");
		return ret;
	}

	dev_dbg(rtc->dev, "rtc_set_alarm: "
		"buf: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buf[RTC_SEC], buf[RTC_MIN], buf[RTC_HOUR], buf[RTC_WEEKDAY],
		buf[RTC_MONTH], buf[RTC_YEAR], buf[RTC_MONTHDAY]);

	ret = max77620_rtc_write(rtc,
			MAX77620_REG_RTCSECA1, buf, sizeof(buf), 1);
	if (ret < 0) {
		dev_err(rtc->dev,
			"rtc_set_alarm: Failed to write rtc alarm time\n");
		return ret;
	}

	ret = max77620_rtc_alarm_irq_enable(dev, alrm->enabled);
	if (ret < 0) {
		dev_err(rtc->dev,
			"rtc_set_alarm: Failed to enable rtc alarm\n");
		return ret;
	}

	return ret;
}

static const struct rtc_class_ops max77620_rtc_ops = {
	.read_time = max77620_rtc_read_time,
	.set_time = max77620_rtc_set_time,
	.read_alarm = max77620_rtc_read_alarm,
	.set_alarm = max77620_rtc_set_alarm,
	.alarm_irq_enable = max77620_rtc_alarm_irq_enable,
};

static int max77620_rtc_preinit(struct max77620_rtc *rtc)
{
	struct device *parent = _to_parent(rtc);
	u8 val;
	int ret;

	/* Mask all interrupts */
	rtc->irq_mask = 0xFF;
	ret = max77620_rtc_write(rtc, MAX77620_REG_RTCINTM,
						&rtc->irq_mask, 1, 1);
	if (ret < 0) {
		dev_err(rtc->dev, "preinit: Failed to set rtc irq mask\n");
		return ret;
	}

	max77620_rtc_read(rtc, MAX77620_REG_RTCINT, &val, 1, 0);

	/* Configure Binary mode and 24hour mode */
	val = MAX77620_HRMODEM_MASK;
	ret = max77620_rtc_write(rtc, MAX77620_REG_RTCCNTL, &val, 1, 1);
	if (ret < 0) {
		dev_err(rtc->dev, "preinit: Failed to set rtc control\n");
		return ret;
	}

	/* It should be disabled alarm wakeup to wakeup from sleep
	 * by EN1 input signal */
	ret = max77620_reg_update(parent, MAX77620_PWR_SLAVE,
		MAX77620_REG_ONOFFCNFG2, ONOFF_WK_ALARM1_MASK, 0);
	if (ret < 0) {
		dev_err(rtc->dev, "preinit: Failed to set onoff cfg2\n");
		return ret;
	}

	return 0;
}


static ssize_t max77620_rtc_set_time_display_suspend(struct device *dev,
		struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "enabled\n") || sysfs_streq(buf, "1"))
		rtc->disable_time_display_in_suspend = false;
	else if (sysfs_streq(buf, "disabled\n") || sysfs_streq(buf, "0"))
		rtc->disable_time_display_in_suspend = true;
	else
		dev_err(dev, "Configuring invalid mode\n");
	return count;
}

static ssize_t max77620_rtc_show_time_display_suspend(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77620_rtc *rtc = dev_get_drvdata(dev);

	if (rtc->disable_time_display_in_suspend)
		return sprintf(buf, "disabled\n");
	return sprintf(buf, "enabled\n");
}

static DEVICE_ATTR(time_display_suspend, S_IRUGO | S_IWUSR,
		max77620_rtc_show_time_display_suspend,
		max77620_rtc_set_time_display_suspend);

static struct attribute *max77620_rtc_attributes[] = {
	&dev_attr_time_display_suspend.attr,
	NULL,
};

static const struct attribute_group max77620_rtc_attr_group = {
	.attrs  = max77620_rtc_attributes,
};

static int max77620_rtc_probe(struct platform_device *pdev)
{
	static struct max77620_rtc *rtc;
	struct device_node *np = pdev->dev.parent->of_node;

	int ret = 0;

	if (of_property_read_bool(np,"maxim,disable-rtc")) {
		dev_info(&pdev->dev, "probe: device is disabled by DT\n");
		return -ENODEV;
	}

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc) {
		dev_err(&pdev->dev, "probe: kzalloc() failed\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, rtc);
	rtc->dev = &pdev->dev;
	mutex_init(&rtc->io_lock);

	rtc->disable_time_display_in_suspend = of_property_read_bool(np,
				"maxim,disable-time-print-suspend-resume");

	ret = sysfs_create_group(&pdev->dev.kobj, &max77620_rtc_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "probe: Failed to create sysfs\n");
		return ret;
	}

	ret = max77620_rtc_preinit(rtc);
	if (ret) {
		dev_err(&pdev->dev, "probe: Failed to rtc preinit\n");
		goto fail_preinit;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc = devm_rtc_device_register(&pdev->dev, "max77620-rtc",
				       &max77620_rtc_ops, THIS_MODULE);
	if (IS_ERR_OR_NULL(rtc->rtc)) {
		dev_err(&pdev->dev, "probe: Failed to register rtc\n");
		ret = PTR_ERR(rtc->rtc);
		goto fail_preinit;
	}

	rtc->irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL,
			max77620_rtc_irq, IRQF_ONESHOT | IRQF_EARLY_RESUME,
			"max77620-rtc", rtc);
	if (ret < 0) {
		dev_err(rtc->dev, "probe: Failed to request irq %d\n",
			rtc->irq);
		rtc->irq = -1;
		goto fail_preinit;
	}

	device_init_wakeup(rtc->dev, 1);

	return 0;

fail_preinit:
	sysfs_remove_group(&pdev->dev.kobj, &max77620_rtc_attr_group);
	mutex_destroy(&rtc->io_lock);
	return ret;
}

static int max77620_rtc_remove(struct platform_device *pdev)
{
	struct max77620_rtc *rtc = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&rtc->io_lock);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77620_rtc_suspend(struct device *dev)
{
	struct max77620_rtc *max77620_rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		int ret;
		struct rtc_wkalrm alm;

		enable_irq_wake(max77620_rtc->irq);
		if (max77620_rtc->disable_time_display_in_suspend)
			return 0;

		ret = max77620_rtc_read_alarm(dev, &alm);
		if (!ret)
			dev_info(dev, "%s() alrm %d time %d %d %d %d %d %d\n",
				__func__, alm.enabled,
				alm.time.tm_year, alm.time.tm_mon,
				alm.time.tm_mday, alm.time.tm_hour,
				alm.time.tm_min, alm.time.tm_sec);
	}

	return 0;
}

static int max77620_rtc_resume(struct device *dev)
{
	struct max77620_rtc *max77620_rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		struct rtc_time tm;
		int ret;

		disable_irq_wake(max77620_rtc->irq);

		if (max77620_rtc->disable_time_display_in_suspend)
			return 0;

		ret = max77620_rtc_read_time(dev, &tm);
		if (!ret)
			dev_info(dev, "%s() %d %d %d %d %d %d\n",
				__func__, tm.tm_year, tm.tm_mon, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	return 0;
}
#endif

static const struct dev_pm_ops max77620_rtc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77620_rtc_suspend, max77620_rtc_resume)
};

static struct platform_device_id max77620_rtc_devtype[] = {
	{
		.name = "max77620-rtc",
	},
	{
		.name = "max20024-rtc",
	},
};

static struct platform_driver max77620_rtc_driver = {
	.probe = max77620_rtc_probe,
	.remove = max77620_rtc_remove,
	.id_table = max77620_rtc_devtype,
	.driver = {
			.name = "max77620-rtc",
			.owner = THIS_MODULE,
			.pm = &max77620_rtc_pm_ops,
	},
};

module_platform_driver(max77620_rtc_driver);

MODULE_DESCRIPTION("max77620 RTC driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_ALIAS("platform:max77620-rtc");
