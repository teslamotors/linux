/*
 * drivers/misc/bluedroid_pm.c
 *
 # Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/err.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/bluedroid_pm.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define PROC_DIR	"bluetooth/sleep"

/* 5 seconds of Min CPU configurations during resume */
#define DEFAULT_RESUME_CPU_TIMEOUT	5000000

#define TX_TIMER_INTERVAL 5

/* Macro to enable or disable debug logging */
/* #define BLUEDROID_PM_DBG */
#ifndef BLUEDROID_PM_DBG
#define BDP_DBG(fmt, ...)	pr_debug("%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define BDP_DBG(fmt, ...)	pr_warn("%s: " fmt, __func__, ##__VA_ARGS__)
#endif

#define BDP_WARN(fmt, ...)	pr_warn("%s: " fmt, __func__, ##__VA_ARGS__)
#define BDP_ERR(fmt, ...)	pr_err("%s: " fmt, __func__, ##__VA_ARGS__)

/* status flags for bluedroid_pm_driver */
#define BT_WAKE	0x01

struct bluedroid_pm_data {
	struct platform_device *pdev;
	int gpio_reset;
	int gpio_shutdown;
	int host_wake;
	int ext_wake;
	int is_blocked;
	int resume_min_frequency;
	unsigned long flags;
	int host_wake_irq;
	struct regulator *vdd_3v3;
	struct regulator *vdd_1v8;
	struct rfkill *rfkill;
	struct wake_lock wake_lock;
	struct pm_qos_request resume_cpu_freq_req;
	bool resumed;
	struct work_struct work;
	spinlock_t lock;
};

struct proc_dir_entry *proc_bt_dir, *bluetooth_sleep_dir;
static bool bluedroid_pm_blocked = 1;

static int create_bt_proc_interface(void *drv_data);
static void remove_bt_proc_interface(void);

static DEFINE_MUTEX(bt_wlan_sync);

void bt_wlan_lock(void)
{
	mutex_lock(&bt_wlan_sync);
}
EXPORT_SYMBOL(bt_wlan_lock);

void bt_wlan_unlock(void)
{
	mutex_unlock(&bt_wlan_sync);
}
EXPORT_SYMBOL(bt_wlan_unlock);

/** bluedroid_m busy timer */
static void bluedroid_pm_timer_expire(unsigned long data);
static DEFINE_TIMER(bluedroid_pm_timer, bluedroid_pm_timer_expire, 0, 0);
static int bluedroid_pm_gpio_get_value(unsigned int gpio);
static void bluedroid_pm_gpio_set_value(unsigned int gpio, int value);

static void bluedroid_work(struct work_struct *data)
{
	struct bluedroid_pm_data *bluedroid_pm =
			container_of(data, struct bluedroid_pm_data, work);
	struct device *dev = &bluedroid_pm->pdev->dev;
	char *resumed[2] = { "BT_STATE=RESUMED", NULL };
	char **uevent_envp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&bluedroid_pm->lock, flags);
	if (!bluedroid_pm->is_blocked && bluedroid_pm->resumed)
		uevent_envp = resumed;
	spin_unlock_irqrestore(&bluedroid_pm->lock, flags);

	if (uevent_envp) {
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, uevent_envp);
		BDP_DBG("is_blocked=%d, resumed=%d, uevent %s\n",
			bluedroid_pm->is_blocked, bluedroid_pm->resumed,
			uevent_envp[0]);
	} else {
		BDP_DBG("is_blocked=%d, resumed=%d, no uevent\n",
			bluedroid_pm->is_blocked, bluedroid_pm->resumed);
	}
}

static irqreturn_t bluedroid_pm_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	return IRQ_HANDLED;
}

static int bluedroid_pm_gpio_get_value(unsigned int gpio)
{
	if (gpio_cansleep(gpio))
		return gpio_get_value_cansleep(gpio);
	else
		return gpio_get_value(gpio);
}

static void bluedroid_pm_gpio_set_value(unsigned int gpio, int value)
{
	if (gpio_cansleep(gpio))
		gpio_set_value_cansleep(gpio, value);
	else
		gpio_set_value(gpio, value);
}

/**
 * Handles bluedroid_pm busy timer expiration.
 * @param data: bluedroid_pm strcuture.
 */
static void bluedroid_pm_timer_expire(unsigned long data)
{
	struct bluedroid_pm_data *bluedroid_pm =
				(struct bluedroid_pm_data *)data;

	/*
	 * if bluedroid_pm data is NULL or timer is deleted with TX busy.
	 * return from the function.
	 */
	if (!bluedroid_pm || test_bit(BT_WAKE, &bluedroid_pm->flags))
		return;

	if (!bluedroid_pm_gpio_get_value(bluedroid_pm->host_wake)) {
		/* BT can sleep */
		BDP_DBG("Tx and Rx are idle, BT sleeping");
		bluedroid_pm_gpio_set_value(bluedroid_pm->ext_wake, 0);
		wake_unlock(&bluedroid_pm->wake_lock);
	} else {
		/* BT Rx is busy, Reset Timer */
		BDP_DBG("Rx is busy, restarting the timer");
		mod_timer(&bluedroid_pm_timer,
					jiffies + (TX_TIMER_INTERVAL * HZ));
	}
}

static int bluedroid_pm_rfkill_set_power(void *data, bool blocked)
{
	struct bluedroid_pm_data *bluedroid_pm = data;
	int ret = 0;

	mdelay(100);
	if (blocked) {
		if (gpio_is_valid(bluedroid_pm->gpio_shutdown))
			bluedroid_pm_gpio_set_value(
				bluedroid_pm->gpio_shutdown, 0);
		if (gpio_is_valid(bluedroid_pm->gpio_reset))
			bluedroid_pm_gpio_set_value(
				bluedroid_pm->gpio_reset, 0);
		if (bluedroid_pm->vdd_3v3)
			ret |= regulator_disable(bluedroid_pm->vdd_3v3);
		if (bluedroid_pm->vdd_1v8)
			ret |= regulator_disable(bluedroid_pm->vdd_1v8);
		if (gpio_is_valid(bluedroid_pm->ext_wake))
			wake_unlock(&bluedroid_pm->wake_lock);
		if (bluedroid_pm->resume_min_frequency)
			pm_qos_remove_request(&bluedroid_pm->
						resume_cpu_freq_req);
	} else {
		if (bluedroid_pm->vdd_3v3)
			ret |= regulator_enable(bluedroid_pm->vdd_3v3);
		if (bluedroid_pm->vdd_1v8)
			ret |= regulator_enable(bluedroid_pm->vdd_1v8);
		if (gpio_is_valid(bluedroid_pm->gpio_shutdown))
			bluedroid_pm_gpio_set_value(
				bluedroid_pm->gpio_shutdown, 1);
		if (gpio_is_valid(bluedroid_pm->gpio_reset))
			bluedroid_pm_gpio_set_value(
				bluedroid_pm->gpio_reset, 1);
		if (bluedroid_pm->resume_min_frequency)
			pm_qos_add_request(&bluedroid_pm->
						resume_cpu_freq_req,
						PM_QOS_CPU_FREQ_MIN,
						PM_QOS_DEFAULT_VALUE);
	}
	bluedroid_pm->is_blocked = blocked;
	mdelay(100);

	return ret;
}

static const struct rfkill_ops bluedroid_pm_rfkill_ops = {
	.set_block = bluedroid_pm_rfkill_set_power,
};

/*
 * This API is added to set block state by ext driver,
 * when bluedroid_pm rfkill is not used but host_wake functionality to be used.
 * Eg: btwilink driver
 */
void bluedroid_pm_set_ext_state(bool blocked)
{
	bluedroid_pm_blocked = blocked;
}
EXPORT_SYMBOL(bluedroid_pm_set_ext_state);

static int bluedroid_pm_probe(struct platform_device *pdev)
{
	static struct bluedroid_pm_data *bluedroid_pm;
	struct bluedroid_pm_platform_data *pdata = pdev->dev.platform_data;
	struct rfkill *rfkill;
	struct resource *res;
	int ret;
	bool enable = false;  /* off */
	struct device_node *node;

	bluedroid_pm = kzalloc(sizeof(*bluedroid_pm), GFP_KERNEL);
	if (!bluedroid_pm)
		return -ENOMEM;

	bluedroid_pm->vdd_3v3 = regulator_get(&pdev->dev, "avdd");
	if (IS_ERR(bluedroid_pm->vdd_3v3)) {
		pr_warn("%s: regulator avdd not available\n", __func__);
		bluedroid_pm->vdd_3v3 = NULL;
	}
	bluedroid_pm->vdd_1v8 = regulator_get(&pdev->dev, "dvdd");
	if (IS_ERR(bluedroid_pm->vdd_1v8)) {
		pr_warn("%s: regulator dvdd not available\n", __func__);
		bluedroid_pm->vdd_1v8 = NULL;
	}

	if (pdev->dev.of_node) {
		node = pdev->dev.of_node;

		bluedroid_pm->gpio_reset =
				of_get_named_gpio(node, "bluedroid_pm,reset-gpio", 0);
		bluedroid_pm->gpio_shutdown =
				of_get_named_gpio(node, "bluedroid_pm,shutdown-gpio", 0);
		bluedroid_pm->host_wake =
				of_get_named_gpio(node, "bluedroid_pm,host-wake-gpio", 0);
		bluedroid_pm->host_wake_irq = platform_get_irq(pdev, 0);
		bluedroid_pm->ext_wake =
				of_get_named_gpio(node, "bluedroid_pm,ext-wake-gpio", 0);
	} else {
		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"reset_gpio");
		if (res)
			bluedroid_pm->gpio_reset = res->start;
		else
			bluedroid_pm->gpio_reset = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
							"shutdown_gpio");
		if (res)
			bluedroid_pm->gpio_shutdown = res->start;
		else
			bluedroid_pm->gpio_shutdown = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"gpio_host_wake");
		if (res)
			bluedroid_pm->host_wake = res->start;
		else
			bluedroid_pm->host_wake = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"host_wake");
		if (res)
			bluedroid_pm->host_wake_irq = res->start;
		else
			bluedroid_pm->host_wake_irq = -1;

		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"gpio_ext_wake");
		if (res)
			bluedroid_pm->ext_wake = res->start;
		else
			bluedroid_pm->ext_wake = -1;
	}

	if (gpio_is_valid(bluedroid_pm->gpio_reset)) {
		ret = gpio_request(bluedroid_pm->gpio_reset, "reset_gpio");
		if (ret) {
			BDP_ERR("Failed to get reset gpio\n");
			goto free_bluedriod_pm;
		}
		gpio_direction_output(bluedroid_pm->gpio_reset, enable);
	} else
		BDP_DBG("Reset gpio not registered.\n");

	if (gpio_is_valid(bluedroid_pm->gpio_shutdown)) {
		ret = gpio_request(bluedroid_pm->gpio_shutdown,
						"shutdown_gpio");
		if (ret) {
			BDP_ERR("Failed to get shutdown gpio\n");
			goto free_gpio_reset;
		}
		gpio_direction_output(bluedroid_pm->gpio_shutdown, enable);
	} else
		BDP_DBG("shutdown gpio not registered\n");

	/*
	 * make sure at-least one of the GPIO or regulators avaiable to
	 * register with rfkill is defined
	 */
	if ((gpio_is_valid(bluedroid_pm->gpio_reset)) ||
		(gpio_is_valid(bluedroid_pm->gpio_shutdown)) ||
		bluedroid_pm->vdd_1v8 || bluedroid_pm->vdd_3v3) {
		rfkill = rfkill_alloc(pdev->name, &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bluedroid_pm_rfkill_ops,
				bluedroid_pm);

		if (unlikely(!rfkill))
			goto free_gpio_shutdown;

		bluedroid_pm->is_blocked = !enable;
		rfkill_set_states(rfkill, bluedroid_pm->is_blocked, false);

		ret = rfkill_register(rfkill);

		if (unlikely(ret)) {
			goto free_rfkill_alloc;
		}
		bluedroid_pm->rfkill = rfkill;
	}

	if (gpio_is_valid(bluedroid_pm->host_wake)) {
		ret = gpio_request(bluedroid_pm->host_wake, "bt_host_wake");
		if (ret) {
			BDP_ERR("Failed to get host_wake gpio\n");
			goto free_rfkill_reg;
		}
		/* configure host_wake as input */
		gpio_direction_input(bluedroid_pm->host_wake);
	} else
		BDP_DBG("gpio_host_wake not registered\n");

	if (bluedroid_pm->host_wake_irq > -1) {
		BDP_DBG("found host_wake irq\n");
		ret = request_irq(bluedroid_pm->host_wake_irq,
					bluedroid_pm_hostwake_isr,
					IRQF_DISABLED | IRQF_TRIGGER_RISING,
					"bluetooth hostwake", bluedroid_pm);
		if (ret) {
			BDP_ERR("Failed to get host_wake irq\n");
			goto free_host_wake;
		}
	} else
		BDP_DBG("host_wake not registered\n");

	if (gpio_is_valid(bluedroid_pm->ext_wake)) {
		ret = gpio_request(bluedroid_pm->ext_wake, "bt_ext_wake");
		if (ret) {
			BDP_ERR("Failed to get ext_wake gpio\n");
			goto free_host_wake_irq;
		}
		/* configure ext_wake as output mode*/
		gpio_direction_output(bluedroid_pm->ext_wake, 1);
		if (create_bt_proc_interface(bluedroid_pm)) {
			BDP_ERR("Failed to create proc interface");
			goto free_ext_wake;
		}
		/* initialize wake lock */
		wake_lock_init(&bluedroid_pm->wake_lock, WAKE_LOCK_SUSPEND,
								"bluedroid_pm");
		/* Initialize timer */
		init_timer(&bluedroid_pm_timer);
		bluedroid_pm_timer.function = bluedroid_pm_timer_expire;
		bluedroid_pm_timer.data = (unsigned long)bluedroid_pm;
	} else
		BDP_DBG("gpio_ext_wake not registered\n");

	/* Update resume_min_frequency, if pdata is passed from board files */
	if (pdata)
		bluedroid_pm->resume_min_frequency =
						pdata->resume_min_frequency;

	INIT_WORK(&bluedroid_pm->work, bluedroid_work);
	spin_lock_init(&bluedroid_pm->lock);

	bluedroid_pm->pdev = pdev;
	platform_set_drvdata(pdev, bluedroid_pm);
	BDP_DBG("driver successfully registered");

	return 0;

free_ext_wake:
	if (gpio_is_valid(bluedroid_pm->ext_wake))
		gpio_free(bluedroid_pm->ext_wake);
free_host_wake_irq:
	if (bluedroid_pm->host_wake_irq > -1)
		free_irq(bluedroid_pm->host_wake_irq, bluedroid_pm);
free_host_wake:
	if (gpio_is_valid(bluedroid_pm->host_wake))
		gpio_free(bluedroid_pm->host_wake);
free_rfkill_reg:
	if (bluedroid_pm->rfkill)
		rfkill_unregister(bluedroid_pm->rfkill);
free_rfkill_alloc:
	if (bluedroid_pm->rfkill)
		rfkill_destroy(bluedroid_pm->rfkill);
free_gpio_shutdown:
	if (gpio_is_valid(bluedroid_pm->gpio_shutdown))
		gpio_free(bluedroid_pm->gpio_shutdown);
free_gpio_reset:
	if (gpio_is_valid(bluedroid_pm->gpio_reset))
		gpio_free(bluedroid_pm->gpio_reset);
free_bluedriod_pm:
	if (bluedroid_pm->vdd_3v3)
		regulator_put(bluedroid_pm->vdd_3v3);
	if (bluedroid_pm->vdd_1v8)
		regulator_put(bluedroid_pm->vdd_1v8);
	kfree(bluedroid_pm);

	return -ENODEV;
}

static int bluedroid_pm_remove(struct platform_device *pdev)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);

	cancel_work_sync(&bluedroid_pm->work);
	if (bluedroid_pm->host_wake_irq > -1)
		free_irq(bluedroid_pm->host_wake_irq, bluedroid_pm);
	if (gpio_is_valid(bluedroid_pm->host_wake))
		gpio_free(bluedroid_pm->host_wake);
	if (gpio_is_valid(bluedroid_pm->ext_wake)) {
		wake_lock_destroy(&bluedroid_pm->wake_lock);
		gpio_free(bluedroid_pm->ext_wake);
		remove_bt_proc_interface();
		del_timer(&bluedroid_pm_timer);
	}
	if ((gpio_is_valid(bluedroid_pm->gpio_reset)) ||
		(gpio_is_valid(bluedroid_pm->gpio_shutdown)) ||
		bluedroid_pm->vdd_1v8 || bluedroid_pm->vdd_3v3) {
		rfkill_unregister(bluedroid_pm->rfkill);
		rfkill_destroy(bluedroid_pm->rfkill);
	}
	if (gpio_is_valid(bluedroid_pm->gpio_shutdown))
		gpio_free(bluedroid_pm->gpio_shutdown);
	if (gpio_is_valid(bluedroid_pm->gpio_reset))
		gpio_free(bluedroid_pm->gpio_reset);
	if (bluedroid_pm->vdd_3v3)
		regulator_put(bluedroid_pm->vdd_3v3);
	if (bluedroid_pm->vdd_1v8)
		regulator_put(bluedroid_pm->vdd_1v8);
	kfree(bluedroid_pm);

	return 0;
}

static int bluedroid_pm_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);
	unsigned long flags;

	if (bluedroid_pm->host_wake)
		if (!bluedroid_pm->is_blocked || !bluedroid_pm_blocked)
			enable_irq_wake(bluedroid_pm->host_wake_irq);

	spin_lock_irqsave(&bluedroid_pm->lock, flags);
	bluedroid_pm->resumed = false;
	spin_unlock_irqrestore(&bluedroid_pm->lock, flags);
	cancel_work_sync(&bluedroid_pm->work);

	return 0;
}

static int bluedroid_pm_resume(struct platform_device *pdev)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);
	unsigned long flags;

	if (bluedroid_pm->host_wake)
		if (!bluedroid_pm->is_blocked || !bluedroid_pm_blocked)
			disable_irq_wake(bluedroid_pm->host_wake_irq);

	spin_lock_irqsave(&bluedroid_pm->lock, flags);
	bluedroid_pm->resumed = true;
	schedule_work(&bluedroid_pm->work);
	spin_unlock_irqrestore(&bluedroid_pm->lock, flags);

	return 0;
}

static void bluedroid_pm_shutdown(struct platform_device *pdev)
{
	struct bluedroid_pm_data *bluedroid_pm = platform_get_drvdata(pdev);

	cancel_work_sync(&bluedroid_pm->work);

	if (gpio_is_valid(bluedroid_pm->gpio_shutdown))
		bluedroid_pm_gpio_set_value(
			bluedroid_pm->gpio_shutdown, 0);
	if (gpio_is_valid(bluedroid_pm->gpio_reset))
		bluedroid_pm_gpio_set_value(
			bluedroid_pm->gpio_reset, 0);
	if (bluedroid_pm->vdd_3v3)
		regulator_disable(bluedroid_pm->vdd_3v3);
	if (bluedroid_pm->vdd_1v8)
		regulator_disable(bluedroid_pm->vdd_1v8);

}

static struct of_device_id bdroid_of_match[] = {
	{ .compatible = "nvidia,tegra-bluedroid_pm", },
	{ },
};
MODULE_DEVICE_TABLE(of, bdroid_of_match);

static struct platform_driver bluedroid_pm_driver = {
	.probe = bluedroid_pm_probe,
	.remove = bluedroid_pm_remove,
	.suspend = bluedroid_pm_suspend,
	.resume = bluedroid_pm_resume,
	.shutdown = bluedroid_pm_shutdown,
	.driver = {
		.name = "bluedroid_pm",
		.of_match_table = of_match_ptr(bdroid_of_match),
		.owner = THIS_MODULE,
	},
};

static ssize_t lpm_read_proc(struct file *file, char __user *buf, size_t size,
					loff_t *ppos)
{
	char msg[50];
	struct bluedroid_pm_data *bluedroid_pm = PDE_DATA(file_inode(file));

	sprintf(msg, "BT LPM Status: TX %x, RX %x\n",
			bluedroid_pm_gpio_get_value(bluedroid_pm->ext_wake),
			bluedroid_pm_gpio_get_value(bluedroid_pm->host_wake));
	return simple_read_from_buffer(buf, size, ppos, msg, strlen(msg));
}

static ssize_t lpm_write_proc(struct file *file, const char __user *buffer,
					size_t count, loff_t *ppos)
{
	char *buf;
	struct bluedroid_pm_data *bluedroid_pm = PDE_DATA(file_inode(file));

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (!bluedroid_pm->is_blocked) {
		if (buf[0] == '0') {
			if (!bluedroid_pm_gpio_get_value(
				bluedroid_pm->host_wake)) {
				/* BT can sleep */
				BDP_DBG("Tx and Rx are idle, BT sleeping");
					bluedroid_pm_gpio_set_value(
						bluedroid_pm->ext_wake, 0);
					wake_unlock(&bluedroid_pm->wake_lock);
				} else {
					/* Reset Timer */
					BDP_DBG("Rx is busy, restarting the timer");
					mod_timer(&bluedroid_pm_timer,
						jiffies + (TX_TIMER_INTERVAL * HZ));
				}
			clear_bit(BT_WAKE, &bluedroid_pm->flags);
		} else if (buf[0] == '1') {
			BDP_DBG("Tx is busy, wake_lock taken, delete timer");
			bluedroid_pm_gpio_set_value(
				bluedroid_pm->ext_wake, 1);
			wake_lock(&bluedroid_pm->wake_lock);
			del_timer(&bluedroid_pm_timer);
			set_bit(BT_WAKE, &bluedroid_pm->flags);
		} else {
			kfree(buf);
			return -EINVAL;
		}
	}

	kfree(buf);
	return count;
}

static const struct file_operations lpm_fops = {
	.read		= lpm_read_proc,
	.write		= lpm_write_proc,
	.llseek		= default_llseek,
};

static void remove_bt_proc_interface(void)
{
	remove_proc_entry("lpm", bluetooth_sleep_dir);
	remove_proc_entry("sleep", proc_bt_dir);
	remove_proc_entry("bluetooth", 0);
}

static int create_bt_proc_interface(void *drv_data)
{
	int retval;
	struct proc_dir_entry *ent;

	proc_bt_dir = proc_mkdir("bluetooth", NULL);
	if (proc_bt_dir == NULL) {
		BDP_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	bluetooth_sleep_dir = proc_mkdir("sleep", proc_bt_dir);
	if (bluetooth_sleep_dir == NULL) {
		BDP_ERR("Unable to create /proc/bluetooth directory");
		retval = -ENOMEM;
		goto free_bluetooth;
	}

	/* Creating read/write "btwake" entry */
	ent = proc_create_data("lpm", 0622, bluetooth_sleep_dir, &lpm_fops, drv_data);
	if (ent == NULL) {
		BDP_ERR("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto free_sleep;
	}
	return 0;

free_sleep:
	remove_proc_entry("sleep", proc_bt_dir);
free_bluetooth:
	remove_proc_entry("bluetooth", 0);
	return retval;
}

static int __init bluedroid_pm_init(void)
{
	return platform_driver_register(&bluedroid_pm_driver);
}

static void __exit bluedroid_pm_exit(void)
{
	platform_driver_unregister(&bluedroid_pm_driver);
}

module_init(bluedroid_pm_init);
module_exit(bluedroid_pm_exit);

MODULE_DESCRIPTION("bluedroid PM");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
