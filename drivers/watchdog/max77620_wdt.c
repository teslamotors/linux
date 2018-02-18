/*
 * Watchdog timer for Max77620 PMIC.
 *
 * Copyright (c) 2014 - 2016 NVIDIA Corporation. All rights reserved.
 *
 * Author: Chaitanya Bandi <bandik@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/alarmtimer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/max77620.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/watchdog.h>
#include <linux/workqueue.h>

static bool nowayout = WATCHDOG_NOWAYOUT;

struct max77620_wdt {
	struct watchdog_device		wdt_dev;
	struct device			*dev;
	struct max77620_chip		*chip;
	struct rtc_device		*rtc;

	int				boot_timeout;
	int				org_boot_timeout;
	int				clear_time;
	bool				otp_wdtt;
	bool				otp_wdten;
	bool				enable_on_off;
	bool				wdt_suspend_enable;
	int				suspend_timeout;
	int				org_suspend_timeout;
	int				current_timeout;
	int				resume_timeout;
	struct delayed_work		clear_wdt_wq;
};

static int max77620_wdt_start(struct watchdog_device *wdt_dev)
{
	struct max77620_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;

	ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_WDTEN, MAX77620_WDTEN);
	if (ret < 0) {
		dev_err(wdt->dev, "wdt enable failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int max77620_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct max77620_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;

	if (wdt->otp_wdten == 0) {
		ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_WDTEN, 0);
		if (ret < 0) {
			dev_err(wdt->dev, "clear wdten failed %d\n", ret);
			return ret;
		}
	} else {
		dev_err(wdt->dev, "Can't stop WDTEN as OTP_WDTEN=1\n");
		return -EPERM;
	}
	return 0;
}

static int max77620_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct max77620_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;

	ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
		MAX77620_REG_CNFGGLBL3, MAX77620_WDTC_MASK, 0x1);
	if (ret < 0)
		dev_err(wdt->dev, "clear wdt failed: %d\n", ret);

	dev_dbg(wdt->dev, "wdt cleared\n");

	return ret;
}

static int _max77620_wdt_set_timeout(struct watchdog_device *wdt_dev,
		unsigned int timeout)
{
	struct max77620_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;
	u8 regval = 0;
	u8 val = 0;

	if (timeout <= 2)
		regval = MAX77620_TWD_2s;
	else if (timeout <= 16)
		regval = MAX77620_TWD_16s;
	else if (timeout <= 64)
		regval = MAX77620_TWD_64s;
	else if (timeout <= 128)
		regval = MAX77620_TWD_128s;
	else
		return -EINVAL;

	if (wdt->otp_wdtt) {
		/* if OTP_WDTT = 1, TWD can be changed when WDTEN = 0*/
		if (wdt->otp_wdten == 0) {
			/* Set WDTEN = 0 and change TWD*/
			ret = max77620_reg_read(wdt->chip->dev,
				MAX77620_PWR_SLAVE,
				MAX77620_REG_CNFGGLBL2, &val);

			ret = max77620_reg_update(wdt->chip->dev,
				MAX77620_PWR_SLAVE,
				MAX77620_REG_CNFGGLBL2, MAX77620_WDTEN, 0);
			if (ret < 0) {
				dev_err(wdt->dev,
					"clear wdten failed: %d\n", ret);
				return ret;
			}

			ret = max77620_reg_update(wdt->chip->dev,
				MAX77620_PWR_SLAVE,
				MAX77620_REG_CNFGGLBL2,
				MAX77620_TWD_MASK, regval);
			if (ret < 0) {
				dev_err(wdt->dev,
					"set wdt timer failed: %d\n", ret);
				return ret;
			}

			if (val & MAX77620_WDTEN) {
				ret = max77620_reg_update(wdt->chip->dev,
					MAX77620_PWR_SLAVE,
					MAX77620_REG_CNFGGLBL2,
					MAX77620_WDTEN, MAX77620_WDTEN);
				if (ret < 0) {
					dev_err(wdt->dev,
						"set wdten failed: %d\n", ret);
					return ret;
				}
			}

		} else {
			ret = max77620_reg_read(wdt->chip->dev,
				MAX77620_PWR_SLAVE,
				MAX77620_REG_CNFGGLBL2, &val);
			if (val & MAX77620_WDTEN) {
				dev_err(wdt->dev,
					"WDTEN is 1. Cannot update timer\n");
				return -EPERM;
			} else {
				ret = max77620_reg_update(wdt->chip->dev,
					MAX77620_PWR_SLAVE,
					MAX77620_REG_CNFGGLBL2,
					MAX77620_TWD_MASK, regval);
				if (ret < 0) {
					dev_err(wdt->dev,
					"set wdt timer failed: %d\n", ret);
					return ret;
				}
			}
		}
	} else {
		/*OTP_WDTT = 0, TWD can be changed by clearing the WDT first*/
		ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL3, MAX77620_WDTC_MASK, 0x1);
		if (ret < 0) {
			dev_err(wdt->dev, "clear wdt failed: %d\n", ret);
			return ret;
		}
		ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_TWD_MASK, regval);
		if (ret < 0) {
			dev_err(wdt->dev, "set wdt timer failed: %d\n", ret);
			return ret;
		}
	}

	wdt->current_timeout = timeout;
	wdt_dev->timeout = timeout;
	dev_info(wdt->dev, "Setting WDT timeout %d\n", timeout);
	return 0;
}

static int max77620_wdt_restart(struct watchdog_device *wdt_dev,
		unsigned int timeout)
{
	int ret;

	if (!timeout)
		return 0;

	max77620_wdt_ping(wdt_dev);

	ret = _max77620_wdt_set_timeout(wdt_dev, timeout);
	if (!ret)
		ret = max77620_wdt_start(wdt_dev);
	return ret;
}

static int max77620_wdt_set_timeout(struct watchdog_device *wdt_dev,
		unsigned int timeout)
{
	struct max77620_wdt *wdt = watchdog_get_drvdata(wdt_dev);

	if (wdt->boot_timeout) {
		cancel_delayed_work(&wdt->clear_wdt_wq);
		wdt->boot_timeout = 0;
	}

	return _max77620_wdt_set_timeout(wdt_dev, timeout);
}

static void max77620_wdt_clear_workqueue(struct work_struct *work)
{
	struct max77620_wdt *wdt;
	int ret;

	wdt = container_of(work, struct max77620_wdt, clear_wdt_wq.work);
	if (!wdt)
		return;

	ret = max77620_wdt_ping(&wdt->wdt_dev);
	if (ret < 0)
		dev_err(wdt->dev, "WDT reset failed: %d\n", ret);

	schedule_delayed_work(&wdt->clear_wdt_wq,
			msecs_to_jiffies(wdt->clear_time * HZ));
}

static const struct watchdog_info max77620_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
	.identity = "max77620 Watchdog",
};

static const struct watchdog_ops max77620_wdt_ops = {
	.owner = THIS_MODULE,
	.start = max77620_wdt_start,
	.stop = max77620_wdt_stop,
	.ping = max77620_wdt_ping,
	.set_timeout = max77620_wdt_set_timeout,
};

static ssize_t show_wdt_suspend_state(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);

        return sprintf(buf, "%s",
			(wdt->suspend_timeout) ? "enable\n" : "disable\n");
}

static ssize_t set_wdt_suspend_state(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);
	int enable;
        char *p = (char *)buf;
	char ch = *p;

	if ((ch == 'e') || (ch == 'E'))
		enable = 1;
	else if ((ch == 'd') || (ch == 'D'))
		enable = 0;
	else
		return -EINVAL;

	if (enable)
		wdt->suspend_timeout = wdt->org_suspend_timeout;
	else
		wdt->suspend_timeout = 0;

	alarmtimer_set_maximum_wakeup_interval_time(wdt->suspend_timeout);
        return count;
}
static DEVICE_ATTR(watchdog_suspend_state, 0644, show_wdt_suspend_state,
			set_wdt_suspend_state);

static ssize_t show_wdt_normal_state(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);

        return sprintf(buf, "%s",
			(wdt->boot_timeout) ? "enable\n" : "disable\n");
}

static ssize_t set_wdt_normal_state(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);
	int enable;
        char *p = (char *)buf;
	char ch = *p;
	int ret;

	if ((ch == 'e') || (ch == 'E'))
		enable = 1;
	else if ((ch == 'd') || (ch == 'D'))
		enable = 0;
	else
		return -EINVAL;

	if (enable && wdt->boot_timeout)
		return count;
	if (!enable && !wdt->boot_timeout)
		return count;

	if (enable)
		wdt->boot_timeout = wdt->org_boot_timeout;
	else
		wdt->boot_timeout = 0;

	if (wdt->org_boot_timeout) {
		if (wdt->boot_timeout) {
			ret = max77620_wdt_restart(&wdt->wdt_dev,
					wdt->boot_timeout);
			if (ret < 0)
				dev_err(wdt->dev,
					"Watchdog not restarted %d\n", ret);

			schedule_delayed_work(&wdt->clear_wdt_wq,
				msecs_to_jiffies(wdt->clear_time * HZ));
		} else {
			ret = max77620_wdt_stop(&wdt->wdt_dev);
			if (ret < 0)
				dev_err(wdt->dev, "wdt stop failed: %d\n", ret);
			cancel_delayed_work(&wdt->clear_wdt_wq);
		}
	}
        return count;
}
static DEVICE_ATTR(watchdog_normal_state, 0644, show_wdt_normal_state,
			set_wdt_normal_state);

static int max77620_wdt_probe(struct platform_device *pdev)
{
	struct max77620_wdt *wdt;
	struct watchdog_device *wdt_dev;
	int ret;
	u32 prop;
	struct device_node *pnode = pdev->dev.parent->of_node;
	struct device_node *np = NULL;

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt) {
		dev_err(&pdev->dev, "Failed to allocate mem\n");
		return -ENOMEM;
	}

	ret = max77620_reg_update(pdev->dev.parent, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_WDTOFFC,
			MAX77620_WDTOFFC);
	if (ret < 0) {
		dev_err(&pdev->dev, "CNFGGLBL2 update failed: %d\n", ret);
		return ret;
	}

	if (pnode) {
		np = of_get_child_by_name(pnode, "watchdog");
		if (!np) {
			dev_err(&pdev->dev,
				"Device is not having watchdog node\n");
			return -ENODEV;
		}
		ret = of_device_is_available(np);
		if (!ret) {
			dev_dbg(&pdev->dev, "WDT node is disabled\n");
			return -ENODEV;
		}

		ret = of_property_read_u32(np, "maxim,wdt-boot-timeout", &prop);
		if (!ret)
			wdt->boot_timeout = prop;
		wdt->clear_time = wdt->boot_timeout / 2;

		wdt->otp_wdtt = of_property_read_bool(np, "maxim,otp-wdtt");
		wdt->otp_wdten = of_property_read_bool(np, "maxim,otp-wdten");
		wdt->enable_on_off = of_property_read_bool(np,
					"maxim,enable-wdt-on-off");
		of_property_read_u32(np, "maxim,wdt-suspend-timeout",
				&wdt->org_suspend_timeout);
		wdt->wdt_suspend_enable = of_property_read_bool(np,
					"maxim,wdt-suspend-enable-from-user");
	} else {
		wdt->boot_timeout = 0;
		wdt->otp_wdtt = 0;
		wdt->otp_wdten = 0;
	}

	wdt->dev = &pdev->dev;
	wdt_dev = &wdt->wdt_dev;
	wdt->chip = dev_get_drvdata(pdev->dev.parent);
	wdt_dev->info = &max77620_wdt_info;
	wdt_dev->ops = &max77620_wdt_ops;
	wdt_dev->min_timeout = 2;
	wdt_dev->max_timeout = 128;

	if (!wdt->wdt_suspend_enable)
		wdt->suspend_timeout = wdt->org_suspend_timeout;
	wdt->org_boot_timeout = wdt->boot_timeout;

	watchdog_set_nowayout(wdt_dev, nowayout);
	watchdog_set_drvdata(wdt_dev, wdt);
	platform_set_drvdata(pdev, wdt);

	if (wdt->enable_on_off) {
		ret = max77620_reg_update(pdev->dev.parent, MAX77620_PWR_SLAVE,
				MAX77620_REG_CNFGGLBL2, MAX77620_WDTOFFC, 0);
		if (ret < 0) {
			dev_err(&pdev->dev, "CNFGGLBL2 update failed: %d\n",
				ret);
			return ret;
		}
	}

	ret = watchdog_register_device(wdt_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "watchdog registration failed: %d\n", ret);
		return ret;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_watchdog_suspend_state);
	if (ret < 0)
		dev_warn(&pdev->dev,
			"wdt suspend sysfs creation failed: %d\n", ret);

	ret = device_create_file(&pdev->dev, &dev_attr_watchdog_normal_state);
	if (ret < 0)
		dev_warn(&pdev->dev,
			"wdt normal sysfs creation failed: %d\n", ret);

	/*Enable WD_RST_WK - WDT expire results in a restart*/
	ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
		MAX77620_REG_ONOFFCNFG2, MAX77620_ONOFFCNFG2_WD_RST_WK,
					MAX77620_ONOFFCNFG2_WD_RST_WK);
	if (ret < 0)
		dev_err(wdt->dev, "onoffcnfg2 failed: %d\n", ret);

	/*Set WDT clear in OFF mode*/
	ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_WDTOFFC,
						MAX77620_WDTOFFC);
	if (ret < 0)
		dev_err(wdt->dev, "set wdtoffc failed %d\n", ret);

	/*Set WDT clear in Sleep mode*/
	ret = max77620_reg_update(wdt->chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL2, MAX77620_WDTSLPC,
						MAX77620_WDTSLPC);
	if (ret < 0)
		dev_err(wdt->dev, "set wdtslpc failed %d\n", ret);

	if (!wdt->boot_timeout) {
		ret = max77620_wdt_stop(wdt_dev);
		if (ret < 0) {
			dev_err(wdt->dev, "wdt stop failed: %d\n", ret);
			goto scrub;
		}
		wdt->current_timeout = 0;
		goto suspend_timeout;
	}

	ret = _max77620_wdt_set_timeout(wdt_dev, wdt->boot_timeout);
	if (ret < 0) {
		dev_err(wdt->dev, "wdt set timeout failed: %d\n", ret);
		goto scrub;
	}
	ret = max77620_wdt_start(&wdt->wdt_dev);
	if (ret < 0)
		dev_err(wdt->dev, "wdt start failed: %d\n", ret);

	INIT_DELAYED_WORK(&wdt->clear_wdt_wq, max77620_wdt_clear_workqueue);
	schedule_delayed_work(&wdt->clear_wdt_wq,
			msecs_to_jiffies(wdt->clear_time * HZ));

suspend_timeout:
	if (wdt->suspend_timeout) {
		int alarm_time = wdt->suspend_timeout;
		alarm_time = (alarm_time > 10) ? alarm_time - 10 : alarm_time;
		alarmtimer_set_maximum_wakeup_interval_time(alarm_time);
	}

	return 0;
scrub:
	watchdog_unregister_device(&wdt->wdt_dev);
	return ret;
}

static int max77620_wdt_remove(struct platform_device *pdev)
{
	struct max77620_wdt *wdt = platform_get_drvdata(pdev);

	max77620_wdt_stop(&wdt->wdt_dev);
	if (wdt->boot_timeout)
		cancel_delayed_work(&wdt->clear_wdt_wq);
	watchdog_unregister_device(&wdt->wdt_dev);
	return 0;
}

static void max77620_wdt_shutdown(struct platform_device *pdev)
{
	struct max77620_wdt *wdt = platform_get_drvdata(pdev);

	max77620_wdt_stop(&wdt->wdt_dev);
	if (wdt->boot_timeout)
		cancel_delayed_work(&wdt->clear_wdt_wq);
}

#ifdef CONFIG_PM_SLEEP
static int max77620_wdt_suspend(struct device *dev)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);
	int ret;

	if (!wdt->suspend_timeout) {
		ret = max77620_wdt_stop(&wdt->wdt_dev);
		if (ret < 0)
			dev_err(wdt->dev, "wdt stop failed: %d\n", ret);
		if (wdt->boot_timeout)
			cancel_delayed_work(&wdt->clear_wdt_wq);
		return ret;
	}

	wdt->resume_timeout = wdt->current_timeout;
	ret = max77620_wdt_restart(&wdt->wdt_dev, wdt->suspend_timeout);
	if (ret < 0)
		dev_err(wdt->dev, "Watchdog not restarted %d\n", ret);
	if (wdt->boot_timeout)
		cancel_delayed_work(&wdt->clear_wdt_wq);
	return 0;
}

static int max77620_wdt_resume(struct device *dev)
{
	struct max77620_wdt *wdt = dev_get_drvdata(dev);
	int ret;

	if (!wdt->suspend_timeout) {
		if (wdt->resume_timeout) {
			ret = max77620_wdt_start(&wdt->wdt_dev);
			if (ret < 0) {
				dev_err(wdt->dev, "wdt start failed:%d\n", ret);
				return ret;
			}
		}
		goto wq_start;
	}

	ret = max77620_wdt_ping(&wdt->wdt_dev);
	if (ret < 0)
		dev_err(wdt->dev, "wdt ping failed: %d\n", ret);

	if (!wdt->resume_timeout) {
		ret = max77620_wdt_stop(&wdt->wdt_dev);
		if (ret < 0)
			dev_err(wdt->dev, "wdt stop failed: %d\n", ret);
		return ret;
	}

	ret = max77620_wdt_restart(&wdt->wdt_dev, wdt->resume_timeout);
	if (ret < 0)
		dev_err(wdt->dev, "Watchdog not restarted %d\n", ret);

wq_start:
	if (wdt->boot_timeout)
		schedule_delayed_work(&wdt->clear_wdt_wq,
				msecs_to_jiffies(wdt->clear_time * HZ));
	return 0;
}
#endif

static const struct dev_pm_ops max77620_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77620_wdt_suspend, max77620_wdt_resume)
};

static struct platform_device_id max77620_wdt_devtype[] = {
	{
		.name = "max77620-wdt",
	},
	{
		.name = "max20024-wdt",
	},
};

static struct platform_driver max77620_wdt_driver = {
	.driver	= {
		.name	= "max77620-wdt",
		.owner	= THIS_MODULE,
		.pm = &max77620_wdt_pm_ops,
	},
	.probe	= max77620_wdt_probe,
	.remove	= max77620_wdt_remove,
	.shutdown = max77620_wdt_shutdown,
	.id_table = max77620_wdt_devtype,
};

static int __init max77620_wdt_init(void)
{
	return platform_driver_register(&max77620_wdt_driver);
}
subsys_initcall_sync(max77620_wdt_init);

static void __exit max77620_wdt_exit(void)
{
	platform_driver_unregister(&max77620_wdt_driver);
}
module_exit(max77620_wdt_exit);

MODULE_ALIAS("platform:max77620-wdt");
MODULE_DESCRIPTION("Max77620 watchdog timer driver");
MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_LICENSE("GPL v2");
