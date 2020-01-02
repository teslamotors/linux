/*
 * Copyright (C) 2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/arm-smccc.h>
#include <linux/mfd/sms-mbox.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/bcd.h>

#include <asm/system_misc.h>

#include "../mtd/devices/MBOX.h"
#include "../mfd/mailbox_A72_SMS_interface.h"

struct sms_mbox_rtc_data {
	struct rtc_device *rtc;
	struct trav_mbox *mbox;
	struct rtc_time *rtc_time_buffer;
};

static int sms_mbox_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct sms_mbox_rtc_data *tdata = dev_get_drvdata(dev);
	uint32_t time_regs[6];

	(void)sms_mbox_get_time(tdata->mbox, time_regs);

	/*
	 * linux assumes year is offset from 1900, so we subtract 1900
	 * from the value we get from the RTC
	 */
	tm->tm_year = time_regs[0] - 1900u;
	tm->tm_mon = time_regs[1] - 1u; /* in tm_mon, months start at 0 */
	tm->tm_mday = time_regs[2];
	tm->tm_hour = time_regs[3];
	tm->tm_min = time_regs[4];
	tm->tm_sec = time_regs[5];

	pr_debug("rtc: hwclock time is %02u:%02u:%02u %02u/%02u/%04u\n",
			tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday,
			(tm->tm_mon + 1u), tm->tm_year);
	return 0;
}

static int sms_mbox_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct sms_mbox_rtc_data *tdata = dev_get_drvdata(dev);
	uint32_t time_regs[6];

	/*
	 * linux assumes year is offset from 1900, so we add 1900
	 * to the value before we transmit to the RTC
	 */
	time_regs[0] = tm->tm_year + 1900u;
	time_regs[1] = tm->tm_mon + 1u; /* in tm_mon, months start at 0 */
	time_regs[2] = tm->tm_mday;
	time_regs[3] = tm->tm_hour;
	time_regs[4] = tm->tm_min;
	time_regs[5] = tm->tm_sec;

	pr_debug("rtc: setting hwclock time to %02u:%02u:%02u %02u/%02u/%04u\n",
			time_regs[3], time_regs[4], time_regs[5], time_regs[2],
			time_regs[1], time_regs[0]);
	(void)sms_mbox_set_time(tdata->mbox, time_regs);

	return 0;
}

static const struct rtc_class_ops sms_mbox_rtc_ops = {
	.ioctl		= NULL,
	.proc		= NULL,
	.read_time	= sms_mbox_rtc_read_time,
	.set_time	= sms_mbox_rtc_set_time,
	.read_alarm	= NULL,
	.set_alarm	= NULL,
	.alarm_irq_enable = NULL,
};

static int sms_mbox_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct device *dev = &pdev->dev;
	struct trav_mbox *mbox;
	struct sms_mbox_rtc_data *tdata;

	tdata = devm_kzalloc(dev, sizeof(struct sms_mbox_rtc_data),
					GFP_KERNEL);
	if (!tdata)
		return -ENOMEM;

	mbox = dev_get_drvdata(dev->parent);
	tdata->mbox = mbox;

	platform_set_drvdata(pdev, tdata);

	rtc = devm_rtc_device_register(dev, SMS_MBOX_RTC_NAME,
					&sms_mbox_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		pr_alert("MBOX-RTC: device registration failed\n");
		return PTR_ERR(rtc);
	}
	tdata->rtc = rtc;

	return 0;
}

static const struct platform_device_id sms_mbox_rtc_table[] = {
	{ .name = SMS_MBOX_RTC_NAME },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(platform, sms_mbox_rtc_table);

static struct platform_driver sms_mbox_rtc_driver = {
	.probe  = sms_mbox_rtc_probe,
	.id_table = sms_mbox_rtc_table,
	.driver = {
		.name = SMS_MBOX_RTC_NAME,
	},
};

module_platform_driver(sms_mbox_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TRAV mailbox sms rtc driver");
