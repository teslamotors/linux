/*
 * Power off driver for Maxim MAX77620 device.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Chaitanya Bandi <bandik@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power/reset/system-pmic.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mfd/max77620.h>

#define MAX77620_PM_RTC_AUTO_CLEAR_MASK	(1 << 1)
#define MAX77620_PM_RTC_WAKE_MASK			(1 << 3)
#define MAX77620_PM_RTC_RB_UPDATE_MASK		(1 << 4)
#define MAX77620_PM_RTC_INT1_MASK			(1 << 1)

#define GPIO_REG_ADDR(offset) (MAX77620_REG_GPIO0 + offset)
#define MAX77620_MAX_GPIO	8
#define GPIO_VAL_TO_STATE(g, v) ((g << 8) | v)
#define GPIO_STATE_TO_GPIO(gv) (gv >> 8)
#define GPIO_STATE_TO_VAL(gv) (gv & 0xFF)

struct max77620_poweroff {
	struct device *dev;
	struct max77620_chip *max77620;
	struct system_pmic_dev *system_pmic_dev;
	struct notifier_block reset_nb;
	int gpio_state[MAX77620_MAX_GPIO];
	int ngpio_states;
	bool use_power_off;
	bool use_power_reset;
	bool need_rtc_power_on;
	bool avoid_power_off_command;
};

static int max77620_turn_off_gpio(
		struct max77620_poweroff *max77620_poweroff,
		int gpio, int value)
{
	struct device *dev = max77620_poweroff->dev;
	struct device *parent = max77620_poweroff->max77620->dev;
	int val;
	int ret;

	dev_info(dev, "Turning off GPIO %d\n", gpio);

	val = (value) ? MAX77620_CNFG_GPIO_OUTPUT_VAL_HIGH :
				MAX77620_CNFG_GPIO_OUTPUT_VAL_LOW;

	ret = max77620_reg_update(parent, MAX77620_PWR_SLAVE,
			GPIO_REG_ADDR(gpio),
			MAX77620_CNFG_GPIO_OUTPUT_VAL_MASK, val);
	if (ret < 0) {
		dev_err(dev, "CNFG_GPIOx val update failed: %d\n", ret);
		return ret;
	}

	ret = max77620_reg_update(parent, MAX77620_PWR_SLAVE,
		GPIO_REG_ADDR(gpio), MAX77620_CNFG_GPIO_DIR_MASK,
				MAX77620_CNFG_GPIO_DIR_OUTPUT);
	if (ret < 0)
		dev_err(dev, "CNFG_GPIOx dir update failed: %d\n", ret);
	return ret;
}

static void max77620_auto_power_on(struct max77620_poweroff *max77620_poweroff)
{
	int ret;
	u8 reg_val;
	u8 val =  MAX77620_PM_RTC_AUTO_CLEAR_MASK | MAX77620_PM_RTC_WAKE_MASK |
			MAX77620_PM_RTC_RB_UPDATE_MASK;

	dev_info(max77620_poweroff->dev, "Resetting through Max77620 RTC\n");

	ret = max77620_reg_write(max77620_poweroff->max77620->dev,
						MAX77620_RTC_SLAVE,
						MAX77620_REG_RTCUPDATE0, val);
	if (ret < 0) {
		dev_err(max77620_poweroff->dev,
			"MAX77620_REG_RTCUPDATE0 write failed\n");
		goto scrub;
	}

	/* Must wait 16ms for buffer update */
	usleep_range(16000, 16000);

	ret = max77620_reg_read(max77620_poweroff->max77620->dev,
			MAX77620_RTC_SLAVE,
			MAX77620_REG_RTCINTM, &reg_val);
	if (ret < 0) {
		dev_err(max77620_poweroff->dev,
			"Failed to read MAX77620_REG_RTCINTM: %d\n", ret);
		goto scrub;
	}

	reg_val = (reg_val & ~MAX77620_PM_RTC_INT1_MASK);
	ret = max77620_reg_writes(max77620_poweroff->max77620->dev,
			MAX77620_RTC_SLAVE, MAX77620_REG_RTCINTM, 1, &reg_val);
	if (ret < 0) {
		dev_err(max77620_poweroff->dev,
			"Failed to write MAX77620_REG_RTCINTM: %d\n", ret);
		goto scrub;
	}

	val =  MAX77620_PM_RTC_AUTO_CLEAR_MASK | MAX77620_PM_RTC_WAKE_MASK |
			MAX77620_PM_RTC_RB_UPDATE_MASK;
	ret = max77620_reg_write(max77620_poweroff->max77620->dev,
						MAX77620_RTC_SLAVE,
						MAX77620_REG_RTCUPDATE0, val);
	if (ret < 0) {
		dev_err(max77620_poweroff->dev,
			"MAX77620_REG_RTCUPDATE0 write failed\n");
		goto scrub;
	}

	/* Must wait 16ms for buffer update */
	usleep_range(16000, 16000);
	max77620_allow_atomic_xfer(max77620_poweroff->max77620);

	if (soc_specific_power_off)
		return;

	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG2,
		MAX77620_ONOFFCNFG2_SFT_RST_WK, 0);

	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG1,
		MAX77620_ONOFFCNFG1_SFT_RST, MAX77620_ONOFFCNFG1_SFT_RST);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"REG_ONOFFCNFG1 update failed, %d\n", ret);

	 while (1)
		dev_err(max77620_poweroff->dev, "Device should not be here\n");
scrub:
	return;
}

static int max77620_configure_power_on(void *drv_data,
	enum system_pmic_power_on_event event, void *event_data)
{
	struct max77620_poweroff *max77620_poweroff = drv_data;

	switch (event) {
	case SYSTEM_PMIC_RTC_ALARM:
		max77620_poweroff->need_rtc_power_on = true;
		break;

	default:
		dev_err(max77620_poweroff->dev, "power on event does not support\n");
		break;
	}
	return 0;
}

static void _max77620_prepare_system_power_off(
		struct max77620_poweroff *max77620_poweroff)
{
	int i;
	int gpio;
	int val;

	if (!max77620_poweroff->ngpio_states)
		return;

	dev_info(max77620_poweroff->dev, "Preparing power-off from PMIC\n");

	for (i = 0; i < max77620_poweroff->ngpio_states; ++i) {
		gpio = GPIO_STATE_TO_GPIO(max77620_poweroff->gpio_state[i]);
		val = GPIO_STATE_TO_VAL(max77620_poweroff->gpio_state[i]);
		max77620_turn_off_gpio(max77620_poweroff, gpio, val);
	}
}

static void max77620_prepare_system_power_off(
		struct max77620_poweroff *max77620_poff)
{
	if (!max77620_poff->ngpio_states)
		return;

	max77620_allow_atomic_xfer(max77620_poff->max77620);
	_max77620_prepare_system_power_off(max77620_poff);
}

static void max77620_prepare_power_off(void *drv_data)
{
	struct max77620_poweroff *max77620_poff = drv_data;

	max77620_prepare_system_power_off(max77620_poff);
}

static void max77620_override_poweroff_config(void *drv_data,
				bool enable)
{
	struct max77620_poweroff *max77620_poff = drv_data;

	if (enable)
		max77620_poff->avoid_power_off_command = true;
	else
		max77620_poff->avoid_power_off_command = false;
}

static void max77620_pm_power_off(void *drv_data)
{
	struct max77620_poweroff *max77620_poweroff = drv_data;
	struct max77620_chip *max77620_chip = max77620_poweroff->max77620;
	int ret;
	u8 reg_val;

	dev_info(max77620_poweroff->dev, "Powering off system\n");

	max77620_allow_atomic_xfer(max77620_poweroff->max77620);

	/* Clear power key interrupts */
	ret = max77620_reg_read(max77620_poweroff->max77620->dev,
			MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFIRQ, &reg_val);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"Interrupt status reg 0x%x read failed: %d\n",
			MAX77620_REG_ONOFFIRQ, ret);

	/* Clear RTC interrupts */
	ret = max77620_reg_read(max77620_poweroff->max77620->dev,
			MAX77620_RTC_SLAVE,
			MAX77620_REG_RTCINT, &reg_val);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"RTC Intr. status reg 0x%x read failed: %d\n",
			MAX77620_REG_RTCINT, ret);

	/* Clear TOP interrupts */
	ret = max77620_reg_read(max77620_poweroff->max77620->dev,
			MAX77620_PWR_SLAVE,
			MAX77620_REG_IRQTOP, &reg_val);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"Interrupt status reg 0x%x read failed: %d\n",
			MAX77620_REG_IRQTOP, ret);

	_max77620_prepare_system_power_off(max77620_poweroff);

	if (max77620_poweroff->need_rtc_power_on) {
		max77620_auto_power_on(max77620_poweroff);
		return;
	}

	if (soc_specific_power_off)
		return;

	if (max77620_chip->id == MAX20024) {
		if (max77620_poweroff->avoid_power_off_command)
			return;
		dev_err(max77620_poweroff->dev,
		    "SYSTEM PMIC MAX20024 POWER-OFF NOT SUPPORTED: SPINNING\n");
		while (1);
	}

	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG2,
		MAX77620_ONOFFCNFG2_SFT_RST_WK, 0);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"ONOFFCNFG2 update for SFT_RST_WK failed, %d\n", ret);

	if (max77620_poweroff->avoid_power_off_command)
		return;
	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG1,
		MAX77620_ONOFFCNFG1_SFT_RST, MAX77620_ONOFFCNFG1_SFT_RST);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"ONOFFCNFG1 update for SFT_RST failed, %d\n", ret);
}

static void max77620_pm_power_reset(void *drv_data)
{
	struct max77620_poweroff *max77620_poweroff = drv_data;
	int ret;

	dev_info(max77620_poweroff->dev, "Power resetting system\n");

	max77620_allow_atomic_xfer(max77620_poweroff->max77620);

	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG2,
		MAX77620_ONOFFCNFG2_SFT_RST_WK, MAX77620_ONOFFCNFG2_SFT_RST_WK);

	_max77620_prepare_system_power_off(max77620_poweroff);

	ret = max77620_reg_update(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_ONOFFCNFG1,
		MAX77620_ONOFFCNFG1_SFT_RST, MAX77620_ONOFFCNFG1_SFT_RST);
	if (ret < 0)
		dev_err(max77620_poweroff->dev,
			"REG_ONOFFCNFG1 update failed, %d\n", ret);
}

static int max77620_restart_notify(struct notifier_block *nb,
			    unsigned long action, void *data)
{
	struct max77620_poweroff *max77620_poweroff;

	max77620_poweroff = container_of(nb, struct max77620_poweroff,
					reset_nb);
	max77620_prepare_system_power_off(max77620_poweroff);
	return NOTIFY_OK;
};

static struct system_pmic_ops max77620_pm_ops = {
	.power_off = max77620_pm_power_off,
	.power_reset = max77620_pm_power_reset,
	.configure_power_on = max77620_configure_power_on,
	.prepare_power_off = max77620_prepare_power_off,
	.override_poweroff_config = max77620_override_poweroff_config,
};

static int max77620_poweroff_probe(struct platform_device *pdev)
{
	struct max77620_poweroff *max77620_poweroff;
	struct device_node *np = pdev->dev.parent->of_node;
	struct max77620_chip *max77620 = dev_get_drvdata(pdev->dev.parent);
	struct system_pmic_config config;
	bool use_power_off = false;
	bool use_power_reset = false;
	bool avoid_power_off_command = false;
	u8 poweroff_event_recorder;
	int count;
	int ret;

	if (np) {
		bool system_pc;

		use_power_off = of_property_read_bool(np,
				"maxim,system-pmic-power-off");
		use_power_reset = of_property_read_bool(np,
				"maxim,system-pmic-power-reset");
		system_pc = of_property_read_bool(np,
				"maxim,system-power-controller");
		avoid_power_off_command = of_property_read_bool(np,
				"maxim,avoid-power-off-commands");
		if (system_pc) {
			use_power_off = true;
			use_power_reset = true;
		}
	}

	if (!use_power_off && !use_power_reset) {
		dev_warn(&pdev->dev,
			"power off and reset functionality not selected\n");
		return 0;
	}

	max77620_poweroff = devm_kzalloc(&pdev->dev, sizeof(*max77620_poweroff),
				GFP_KERNEL);
	if (!max77620_poweroff)
		return -ENOMEM;

	if (!np)
		goto gpio_done;

	count = of_property_count_u32(np, "maxim,power-reset-gpio-states");
	if (count == -EINVAL)
		goto gpio_done;

	if (count % 2) {
		dev_warn(&pdev->dev, "Not able to parse reset-gpio-states\n");
		goto gpio_done;
	}
	max77620_poweroff->ngpio_states = count / 2;
	for (count = 0; count < max77620_poweroff->ngpio_states; ++count) {
		u32 gpio = 0;
		u32 val = 0;
		int index = count * 2;

		of_property_read_u32_index(np, "maxim,power-reset-gpio-states",
				index, &gpio);
		of_property_read_u32_index(np, "maxim,power-reset-gpio-states",
				index + 1, &val);
		max77620_poweroff->gpio_state[count] =
					GPIO_VAL_TO_STATE(gpio, val);
	}

gpio_done:
	max77620_poweroff->max77620 = max77620;
	max77620_poweroff->dev = &pdev->dev;
	max77620_poweroff->use_power_off = use_power_off;
	max77620_poweroff->use_power_reset = use_power_reset;
	max77620_poweroff->avoid_power_off_command = avoid_power_off_command;

	config.allow_power_off = use_power_off;
	config.allow_power_reset = use_power_reset;
	config.avoid_power_off_command = avoid_power_off_command;

	max77620_poweroff->system_pmic_dev = system_pmic_register(&pdev->dev,
				&max77620_pm_ops, &config, max77620_poweroff);
	if (IS_ERR(max77620_poweroff->system_pmic_dev)) {
		ret = PTR_ERR(max77620_poweroff->system_pmic_dev);

		dev_err(&pdev->dev, "System PMIC registration failed: %d\n",
			ret);
		return ret;
	}

	ret = max77620_reg_read(max77620_poweroff->max77620->dev,
		MAX77620_PWR_SLAVE, MAX77620_REG_NVERC,
		&poweroff_event_recorder);
	if (ret < 0) {
		dev_err(max77620_poweroff->dev,
			"REG_NVERC read failed, %d\n", ret);
		return ret;
	} else
		dev_info(&pdev->dev, "Event recorder REG_NVERC : 0x%x\n",
				poweroff_event_recorder);

	max77620_poweroff->reset_nb.notifier_call = max77620_restart_notify;
	max77620_poweroff->reset_nb.priority = 200;
	ret = register_restart_handler(&max77620_poweroff->reset_nb);
	if (ret < 0) {
		dev_err(&pdev->dev, "restart handler registration failed\n");
		return ret;
	}

	return 0;
}

static int max77620_poweroff_remove(struct platform_device *pdev)
{
	struct max77620_poweroff *max77620_poweroff =
					platform_get_drvdata(pdev);
	system_pmic_unregister(max77620_poweroff->system_pmic_dev);
	return 0;
}

static struct platform_device_id max77620_poweroff_devtype[] = {
	{
		.name = "max77620-power-off",
	},
	{
		.name = "max20024-power-off",
	},
};

static struct platform_driver max77620_poweroff_driver = {
	.driver = {
		.name = "max77620-power-off",
		.owner = THIS_MODULE,
	},
	.probe = max77620_poweroff_probe,
	.remove = max77620_poweroff_remove,
	.id_table = max77620_poweroff_devtype,
};

module_platform_driver(max77620_poweroff_driver);

MODULE_DESCRIPTION("Power off driver for MAX77620 PMIC Device");
MODULE_ALIAS("platform:max77620-power-off");
MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_LICENSE("GPL v2");
