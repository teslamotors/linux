/*
 * Maxim MAX77620 MFD Driver
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <linux/mfd/max77620.h>

static struct resource gpio_resources[] = {
	{
		.start	= MAX77620_IRQ_TOP_GPIO,
		.end	= MAX77620_IRQ_TOP_GPIO,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource rtc_resources[] = {
	{
		.start	= MAX77620_IRQ_TOP_RTC,
		.end	= MAX77620_IRQ_TOP_RTC,
		.flags  = IORESOURCE_IRQ,
	}
};

static struct resource thermal_resources[] = {
	{
		.start	= MAX77620_IRQ_LBT_TJALRM1,
		.end	= MAX77620_IRQ_LBT_TJALRM1,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start	= MAX77620_IRQ_LBT_TJALRM2,
		.end	= MAX77620_IRQ_LBT_TJALRM2,
		.flags  = IORESOURCE_IRQ,
	}
};

static const struct regmap_irq max77620_top_irqs[] = {
	[MAX77620_IRQ_TOP_GLBL] = {
		.mask = MAX77620_IRQ_TOP_GLBL_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_SD] = {
		.mask = MAX77620_IRQ_TOP_SD_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_LDO] = {
		.mask = MAX77620_IRQ_TOP_LDO_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_GPIO] = {
		.mask = MAX77620_IRQ_TOP_GPIO_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_RTC] = {
		.mask = MAX77620_IRQ_TOP_RTC_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_32K] = {
		.mask = MAX77620_IRQ_TOP_32K_MASK,
		.reg_offset = 0,
	},
	[MAX77620_IRQ_TOP_ONOFF] = {
		.mask = MAX77620_IRQ_TOP_ONOFF_MASK,
		.reg_offset = 0,
	},

	[MAX77620_IRQ_LBT_MBATLOW] = {
		.mask = MAX77620_IRQ_LBM_MASK,
		.reg_offset = 1,
	},
	[MAX77620_IRQ_LBT_TJALRM1] = {
		.mask = MAX77620_IRQ_TJALRM1_MASK,
		.reg_offset = 1,
	},
	[MAX77620_IRQ_LBT_TJALRM2] = {
		.mask = MAX77620_IRQ_TJALRM2_MASK,
		.reg_offset = 1,
	},

};

static const char *max77620_nverc[] = {
	"Shutdown-pin",
	"System WatchDog Timer",
	"Hard Reset",
	"Junction Temp Overload",
	"Main-Battery Low",
	"Main-Battery overvoltage Lockout",
	"Main-Battery undervoltage Lockout",
	"Reset input",
};

enum max77660_ids {
	MAX77620_PMIC_ID,
	MAX77620_GPIO_ID,
	MAX77620_RTC_ID,
	MAX77620_PINCTRL_ID,
	MAX77620_CLK_ID,
	MAX77620_POWER_OFF_ID,
	MAX77620_WDT_ID,
	MAX77620_THERMAL_ID,
};

#define MAX77620_SUB_MODULE_RES(_name, _id)			\
	[MAX77620_##_id##_ID] = {				\
		.name = "max77620-"#_name,			\
		.num_resources	= ARRAY_SIZE(_name##_resources), \
		.resources	= &_name##_resources[0],	\
		.id = MAX77620_##_id##_ID,			\
	}

#define MAX20024_SUB_MODULE_RES(_name, _id)			\
	[MAX77620_##_id##_ID] = {				\
		.name = "max20024-"#_name,			\
		.num_resources	= ARRAY_SIZE(_name##_resources), \
		.resources	= &_name##_resources[0],	\
		.id = MAX77620_##_id##_ID,			\
	}

#define MAX77620_SUB_MODULE_NO_RES(_name, _id)			\
	[MAX77620_##_id##_ID] = {				\
		.name = "max77620-"#_name,			\
		.id = MAX77620_##_id##_ID,			\
	}

#define MAX20024_SUB_MODULE_NO_RES(_name, _id)			\
	[MAX77620_##_id##_ID] = {				\
		.name = "max20024-"#_name,			\
		.id = MAX77620_##_id##_ID,			\
	}

static struct mfd_cell max77620_children[] = {
	MAX77620_SUB_MODULE_RES(gpio, GPIO),
	MAX77620_SUB_MODULE_NO_RES(pmic, PMIC),
	MAX77620_SUB_MODULE_RES(rtc, RTC),
	MAX77620_SUB_MODULE_NO_RES(pinctrl, PINCTRL),
	MAX77620_SUB_MODULE_NO_RES(clk, CLK),
	MAX77620_SUB_MODULE_NO_RES(power-off, POWER_OFF),
	MAX77620_SUB_MODULE_NO_RES(wdt, WDT),
	MAX77620_SUB_MODULE_RES(thermal, THERMAL),
};

static struct mfd_cell max20024_children[] = {
	MAX20024_SUB_MODULE_RES(gpio, GPIO),
	MAX20024_SUB_MODULE_NO_RES(pmic, PMIC),
	MAX20024_SUB_MODULE_RES(rtc, RTC),
	MAX20024_SUB_MODULE_NO_RES(pinctrl, PINCTRL),
	MAX20024_SUB_MODULE_NO_RES(clk, CLK),
	MAX20024_SUB_MODULE_NO_RES(power-off, POWER_OFF),
	MAX20024_SUB_MODULE_NO_RES(wdt, WDT),
	MAX20024_SUB_MODULE_RES(thermal, THERMAL),
};

struct max77620_sub_modules {
	struct mfd_cell *cells;
	int ncells;
	u32 id;
};

static const struct max77620_sub_modules max77620_cells = {
	.cells = max77620_children,
	.ncells = ARRAY_SIZE(max77620_children),
	.id = MAX77620,
};

static const struct max77620_sub_modules  max20024_cells = {
	.cells = max20024_children,
	.ncells = ARRAY_SIZE(max20024_children),
	.id = MAX20024,
};

static const struct of_device_id max77620_of_match[] = {
	{
		.compatible = "maxim,max77620",
		.data = &max77620_cells,
	}, {
		.compatible = "maxim,max20024",
		.data = &max20024_cells,
	}, {
	},
};
MODULE_DEVICE_TABLE(of, max77620_of_match);

static int max77620_top_irq_chip_pre_irq(void *data)
{
	struct max77620_chip *chip = data;
	int ret;

	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
		MAX77620_REG_INTENLBT, MAX77620_GLBLM_MASK,
		MAX77620_GLBLM_MASK);
	if (ret < 0)
		dev_err(chip->dev, "GLBLM masking failed: %d\n", ret);

	return ret;
}

static int max77620_top_irq_chip_post_irq(void *data)
{
	struct max77620_chip *chip = data;
	int ret;

	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
		MAX77620_REG_INTENLBT, MAX77620_GLBLM_MASK, 0);
	if (ret < 0)
		dev_err(chip->dev, "GLBLM unmasking failed: %d\n", ret);

	return ret;
}

static struct regmap_irq_chip max77620_top_irq_chip = {
	.name = "max77620-top",
	.irqs = max77620_top_irqs,
	.num_irqs = ARRAY_SIZE(max77620_top_irqs),
	.num_regs = 2,
	.status_base = MAX77620_REG_IRQTOP,
	.mask_base = MAX77620_REG_IRQTOPM,
	.pre_irq = max77620_top_irq_chip_pre_irq,
	.post_irq = max77620_top_irq_chip_post_irq,
};

static void max77620_regmap_config_lock(void *lock)
{
	struct max77620_chip *chip = lock;

	if (chip->shutdown && (in_atomic() || irqs_disabled())) {
		dev_info(chip->dev, "Xfer without lock\n");
		return;
	}

	mutex_lock(&chip->mutex_config);
}

static void max77620_regmap_config_unlock(void *lock)
{
	struct max77620_chip *chip = lock;

	if (chip->shutdown && (in_atomic() || irqs_disabled()))
		return;

	mutex_unlock(&chip->mutex_config);
}

static const struct regmap_range max77620_readable_ranges[] = {
	regmap_reg_range(MAX77620_REG_CNFGGLBL1, MAX77620_REG_DVSSD4),
};

static const struct regmap_access_table max77620_readable_table = {
	.yes_ranges = max77620_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(max77620_readable_ranges),
};

static const struct regmap_range max20024_readable_ranges[] = {
	regmap_reg_range(MAX77620_REG_CNFGGLBL1, MAX77620_REG_DVSSD4),
	regmap_reg_range(MAX20024_REG_0x70, MAX20024_REG_0x70),
};

static const struct regmap_access_table max20024_readable_table = {
	.yes_ranges = max20024_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(max20024_readable_ranges),
};

static const struct regmap_range max77620_writable_ranges[] = {
	regmap_reg_range(MAX77620_REG_CNFGGLBL1, MAX77620_REG_DVSSD4),
};

static const struct regmap_access_table max77620_writable_table = {
	.yes_ranges = max77620_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(max77620_writable_ranges),
};

static const struct regmap_range max77620_cacheable_ranges[] = {
	regmap_reg_range(MAX77620_REG_SD0_CFG, MAX77620_REG_LDO_CFG3),
	regmap_reg_range(MAX77620_REG_FPS_CFG0, MAX77620_REG_FPS_SD3),
};

static const struct regmap_access_table max77620_volatile_table = {
	.no_ranges = max77620_cacheable_ranges,
	.n_no_ranges = ARRAY_SIZE(max77620_cacheable_ranges),
};

static struct regmap_config max77620_regmap_config[] = {
	[MAX77620_PWR_SLAVE] = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = MAX77620_REG_DVSSD4 + 1,
		.lock = max77620_regmap_config_lock,
		.unlock = max77620_regmap_config_unlock,
		.cache_type = REGCACHE_RBTREE,
		.rd_table = &max77620_readable_table,
		.wr_table = &max77620_writable_table,
		.volatile_table = &max77620_volatile_table,
	},
	[MAX77620_RTC_SLAVE] = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = 0x1b,
		.lock = max77620_regmap_config_lock,
		.unlock = max77620_regmap_config_unlock,
	},
};

static int max77620_slave_address[MAX77620_NUM_SLAVES] = {
	MAX77620_PWR_I2C_ADDR,
	MAX77620_RTC_I2C_ADDR,
};

static int max77620_initialise_fps(struct max77620_chip *chip,
			struct device *dev)
{
	struct device_node *node;
	struct device_node *child;
	u32 reg, pval;
	int ret;
	int time_period = 40;
	int input_enable = 2;
	bool enable_fps = false;
	unsigned int mask;
	unsigned int config;
	int base_fps_time = (chip->id == MAX20024) ? 20 : 40;
	int i;

	node = of_get_child_by_name(dev->of_node, "fps");
	if (!node)
		goto skip_fps;

	for (reg = 0; reg < 3; ++reg) {
		chip->active_fps_period[reg] = -1;
		chip->suspend_fps_period[reg] = -1;
	}

	for_each_child_of_node(node, child) {
		ret = of_property_read_u32(child, "reg", &reg);
		if (ret) {
			dev_err(dev, "node %s does not have reg property\n",
					child->name);
			continue;
		}
		if (reg > 2) {
			dev_err(dev, "FPS%d is not supported\n", reg);
			continue;
		}

		mask = 0;
		ret = of_property_read_u32(child,
			"maxim,shutdown-fps-time-period-us", &pval);
		if (!ret) {
			time_period = min(pval, 5120U);
			mask |= MAX77620_FPS_TIME_PERIOD_MASK;
			chip->active_fps_period[reg] = time_period;
		}

		ret = of_property_read_u32(child,
				"maxim,suspend-fps-time-period-us", &pval);
		if (!ret)
			chip->suspend_fps_period[reg] = min(pval, 5120U);

		ret = of_property_read_u32(child, "maxim,fps-event-source",
						&pval);
		if (!ret) {
			if (pval > 2) {
				dev_err(dev,
					"FPS event-source %u is not supported\n",
					pval);
			} else {
				input_enable = pval;
				mask |= MAX77620_FPS_EN_SRC_MASK;
			}
		}

		if (input_enable == 2) {
			enable_fps = of_property_read_bool(child,
						"maxim,fps-sw-enable");
			mask |= MAX77620_FPS_ENFPS_MASK;
		}

		if (!chip->sleep_enable && !chip->enable_global_lpm) {
			ret = of_property_read_u32(child,
				"maxim,device-state-on-disabled-event", &pval);
			if (!ret) {
				if (pval == 0)
					chip->sleep_enable = true;
				else if (pval == 1)
					chip->enable_global_lpm = true;
			}
		}

		for (i = 0; i < 0x7; ++i) {
			int x = base_fps_time * BIT(i);
			if (x >= time_period)
				break;
		}
		config = (i & 0x7) << MAX77620_FPS_TIME_PERIOD_SHIFT;
		config |= (input_enable & 0x3) << MAX77620_FPS_EN_SRC_SHIFT;
		if (enable_fps)
			config |= 1;
		ret = max77620_reg_update(dev, MAX77620_PWR_SLAVE,
				MAX77620_REG_FPS_CFG0 + reg, mask, config);
		if (ret < 0) {
			dev_err(dev, "Reg 0x%02x write failed, %d\n",
				MAX77620_REG_FPS_CFG0 + reg, ret);
			return ret;
		}
	}

	config = chip->enable_global_lpm ? MAX77620_ONOFFCNFG2_SLP_LPM_MSK : 0;
	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG2,
			MAX77620_ONOFFCNFG2_SLP_LPM_MSK, config);
	if (ret < 0) {
		dev_err(dev, "Reg ONOFFCNFG2 update failed: %d\n", ret);
		return ret;
	}

skip_fps:
	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG2, MAX77620_ONOFFCNFG2_WK_EN0,
			MAX77620_ONOFFCNFG2_WK_EN0);
	if (ret < 0) {
		dev_err(dev, "Reg ONOFFCNFG2 WK_EN0 update failed: %d\n", ret);
		return ret;
	}

	if (!chip->sleep_enable) {
		ret = of_property_read_u32(dev->of_node,
			"maxim,device-state-on-disabled-event", &pval);
		if (!ret) {
			if (pval == 0)
				chip->sleep_enable = true;
		}
	}

	/* For MAX20024, SLPEN will be POR reset if CLRSE is b11 */
	if ((chip->id == MAX20024) && chip->sleep_enable) {
		config = MAX77620_ONOFFCNFG1_SLPEN | MAX20024_ONOFFCNFG1_CLRSE;
		ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG1, config, config);
		if (ret < 0) {
			dev_err(dev, "Reg ONOFFCNFG1 update failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int max77620_init_backup_battery_charging(struct max77620_chip *chip,
		struct device *dev)
{
	struct device_node *np;
	u32 pval;
	u8 config;
	int charging_current;
	int charging_voltage;
	int resistor;
	int ret;

	np = of_get_child_by_name(dev->of_node, "backup-battery");
	if (!np) {
		dev_info(dev, "Backup battery charging support disabled\n");
		ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGBBC, MAX77620_CNFGBBC_ENABLE, 0);
		return ret;
	}

	ret = of_property_read_u32(np,
			"maxim,backup-battery-charging-current", &pval);
	charging_current = (!ret) ? pval : 50;

	ret = of_property_read_u32(np,
			"maxim,backup-battery-charging-voltage", &pval);
	charging_voltage = (!ret) ? pval : 2500000;
	charging_voltage /= 1000;

	ret = of_property_read_u32(np,
			"maxim,backup-battery-output-resister", &pval);
	resistor = (!ret) ? pval : 1000;

	config = MAX77620_CNFGBBC_ENABLE;
	if (charging_current <= 50)
		config |= 0 << MAX77620_CNFGBBC_CURRENT_SHIFT;
	else if (charging_current <= 100)
		config |= 3 << MAX77620_CNFGBBC_CURRENT_SHIFT;
	else if (charging_current <= 200)
		config |= 0 << MAX77620_CNFGBBC_CURRENT_SHIFT;
	else if (charging_current <= 400)
		config |= 3 << MAX77620_CNFGBBC_CURRENT_SHIFT;
	else if (charging_current <= 600)
		config |= 1 << MAX77620_CNFGBBC_CURRENT_SHIFT;
	else
		config |= 2 << MAX77620_CNFGBBC_CURRENT_SHIFT;

	if (charging_current > 100)
		config |= MAX77620_CNFGBBC_LOW_CURRENT_ENABLE;

	if (charging_voltage <= 2500)
		config |= 0 << MAX77620_CNFGBBC_VOLTAGE_SHIFT;
	else if (charging_voltage <= 3000)
		config |= 1 << MAX77620_CNFGBBC_VOLTAGE_SHIFT;
	else if (charging_voltage <= 3300)
		config |= 2 << MAX77620_CNFGBBC_VOLTAGE_SHIFT;
	else
		config |= 3 << MAX77620_CNFGBBC_VOLTAGE_SHIFT;

	if (resistor <= 100)
		config |= 0 << MAX77620_CNFGBBC_RESISTOR_SHIFT;
	else if (resistor <= 1000)
		config |= 1 << MAX77620_CNFGBBC_RESISTOR_SHIFT;
	else if (resistor <= 3000)
		config |= 2 << MAX77620_CNFGBBC_RESISTOR_SHIFT;
	else if (resistor <= 6000)
		config |= 3 << MAX77620_CNFGBBC_RESISTOR_SHIFT;

	ret = max77620_reg_write(dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGBBC, config);
	if (ret < 0) {
		dev_err(dev, "Reg 0x%02x write failed, %d\n",
			MAX77620_REG_CNFGBBC, ret);
		return ret;
	}
	return 0;
}

static int max77620_init_low_battery_monitor(struct max77620_chip *chip,
		struct device *dev)
{
	struct device_node *np;
	bool pval;
	u8 mask = 0;
	u8 val = 0;
	int ret;


	np = of_get_child_by_name(dev->of_node, "low-battery-monitor");
	if (!np)
		return 0;

	pval = of_property_read_bool(np, "maxim,low-battery-dac-enable");
	if (pval) {
		mask |= MAX77620_CNFGGLBL1_LBDAC_EN;
		val |= MAX77620_CNFGGLBL1_LBDAC_EN;
	}
	pval = of_property_read_bool(np, "maxim,low-battery-dac-disable");
	if (pval)
		mask |= MAX77620_CNFGGLBL1_LBDAC_EN;

	pval = of_property_read_bool(np, "maxim,low-battery-shutdown-enable");
	if (pval) {
		mask |= MAX77620_CNFGGLBL1_MPPLD;
		val |= MAX77620_CNFGGLBL1_MPPLD;
	}
	pval = of_property_read_bool(np, "maxim,low-battery-shutdown-disable");
	if (pval)
		mask |= MAX77620_CNFGGLBL1_MPPLD;

	pval = of_property_read_bool(np, "maxim,low-battery-reset-enable");
	if (pval) {
		mask |= MAX77620_CNFGGLBL1_LBRSTEN;
		val |= MAX77620_CNFGGLBL1_LBRSTEN;
	}
	pval = of_property_read_bool(np, "maxim,low-battery-reset-disable");
	if (pval)
		mask |= MAX77620_CNFGGLBL1_LBRSTEN;

	ret = max77620_reg_update(dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CNFGGLBL1, mask, val);
	if (ret < 0) {
		dev_err(dev, "Reg CNFGGLBL1 update failed, %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77620_initialise_chip(struct max77620_chip *chip,
			struct device *dev)
{
	struct device_node *node = dev->of_node;
	u32 mrt_time = 0;
	u32 pval;
	u8 reg_val;
	int ret;

	if (chip->id == MAX20024)
		return 0;

	of_property_read_u32(node, "maxim,hard-power-off-time", &mrt_time);
	if (!mrt_time)
		return 0;

	mrt_time = (mrt_time > 12) ? 12 : mrt_time;
	if (mrt_time <= 6)
		reg_val = mrt_time - 2;
	else
		reg_val = (mrt_time - 6) / 2 + 4;

	reg_val <<= MAX77620_ONOFFCNFG1_MRT_SHIFT;

	ret = max77620_reg_update(dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG1, MAX77620_ONOFFCNFG1_MRT_MASK,
			reg_val);
	if (ret < 0) {
		dev_err(dev, "Reg 0x%02x update failed, %d\n",
			MAX77620_REG_ONOFFCNFG1, reg_val);
		return ret;
	}

	ret = of_property_read_u32(node, "maxim,en0-delay", &pval);
	if (!ret) {
		if (pval)
			reg_val = MAX77620_ONOFFCNFG1_EN0_DLY;
		else
			reg_val = 0;
		ret = max77620_reg_update(dev, MAX77620_PWR_SLAVE,
				MAX77620_REG_ONOFFCNFG1,
				MAX77620_ONOFFCNFG1_EN0_DLY, reg_val);
		if (ret < 0)
			dev_err(dev, "Reg 0x%02x update failed, %d\n",
				MAX77620_REG_ONOFFCNFG1, reg_val);
		return ret;
	}
	return 0;
}

static int max77620_read_es_version(struct max77620_chip *chip)
{
	int ret;
	u8 val;
	u8 cid;
	int i;

	for (i = MAX77620_REG_CID0; i <= MAX77620_REG_CID5; ++i) {
		ret = max77620_reg_read(chip->dev, MAX77620_PWR_SLAVE,
				i, &cid);
		if (ret < 0) {
			dev_err(chip->dev, "CID%d register read failed: %d\n",
					i - MAX77620_REG_CID0, ret);
			return ret;
		}
		dev_info(chip->dev, "CID%d: 0x%02x\n",
			i - MAX77620_REG_CID0, cid);
	}

	/* Read OTP Version */
	ret = max77620_reg_read(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CID4, &val);
	if (ret < 0) {
		dev_err(chip->dev, "CID4 read failed: %d\n", ret);
		return ret;
	}
	dev_info(chip->dev, "MAX77620 PMIC OTP Version is 0x%02X\n", val);

	/* Read ES version */
	ret = max77620_reg_read(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_CID5, &val);
	if (ret < 0) {
		dev_err(chip->dev, "CID5 read failed: %d\n", ret);
		return ret;
	}
	chip->es_minor_version = MAX77620_CID5_DIDM(val);
	chip->es_major_version = 1;

	/* Read NVERC register */
	ret = max77620_reg_read(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_NVERC, &val);
	if (ret < 0) {
		dev_err(chip->dev, "NVERC read failed: %d\n", ret);
		return ret;
	}
	dev_dbg(chip->dev, "NVERC = 0x%02x\n", val);
	for (i = 0; i < 8; ++i) {
		if (val & BIT(i))
			dev_dbg(chip->dev, "NVERC: %s\n", max77620_nverc[i]);
	}
	return ret;
}

static irqreturn_t max77620_mbattlow_irq(int irq, void *data)
{
	struct max77620_chip *max77620 = data;

	dev_info(max77620->dev, "MBATTLOW interrupt occurred\n");

	return IRQ_HANDLED;
}

static int max77620_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	const struct max77620_sub_modules *children;
	struct max77620_chip *chip;
	int i = 0;
	int ret = 0;
	if (!node) {
		dev_err(&client->dev, "Device is not from DT\n");
		return -ENODEV;
	}

	children = of_get_match_device_data(max77620_of_match, &client->dev);
	if (!children)
		return -ENODEV;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		dev_err(&client->dev, "Memory alloc for chip failed\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, chip);
	chip->dev = &client->dev;
	chip->irq_base = -1;
	chip->chip_irq = client->irq;
	chip->id = children->id;

	if (chip->id == MAX20024) {
		max77620_regmap_config[MAX77620_PWR_SLAVE].rd_table =
					&max20024_readable_table;
		max77620_regmap_config[MAX77620_PWR_SLAVE].max_register =
				MAX20024_REG_0x70 + 1;
	}

	if (node)
		chip->avoid_rtc_bulk_write =  of_property_read_bool(node,
					"maxim,avoid-rtc-bulk-register-write");
	mutex_init(&chip->mutex_config);

	for (i = 0; i < MAX77620_NUM_SLAVES; i++) {
		if (max77620_slave_address[i] == client->addr)
			chip->clients[i] = client;
		else
			chip->clients[i] = i2c_new_dummy(client->adapter,
						max77620_slave_address[i]);
		if (!chip->clients[i]) {
			dev_err(&client->dev, "can't attach client %d\n", i);
			ret = -ENOMEM;
			goto fail_client_reg;
		}

		chip->clients[i]->dev.of_node = node;
		i2c_set_clientdata(chip->clients[i], chip);
		max77620_regmap_config[i].lock_arg = chip;
		chip->rmap[i] = devm_regmap_init_i2c(chip->clients[i],
		(const struct regmap_config *)&max77620_regmap_config[i]);
		if (IS_ERR(chip->rmap[i])) {
			ret = PTR_ERR(chip->rmap[i]);
			dev_err(&client->dev,
				"regmap %d init failed, err %d\n", i, ret);
			goto fail_client_reg;
		}
	}

	ret = max77620_read_es_version(chip);
	if (ret < 0) {
		dev_err(chip->dev, "Chip revision init failed: %d\n", ret);
		goto fail_client_reg;
	}

	ret = max77620_initialise_chip(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "Chip initialisation failed, %d\n", ret);
		goto fail_client_reg;
	}

	max77620_top_irq_chip.pre_post_irq_data = chip;

	ret = regmap_add_irq_chip(chip->rmap[MAX77620_PWR_SLAVE],
		chip->chip_irq, IRQF_ONESHOT | IRQF_SHARED, chip->irq_base,
		&max77620_top_irq_chip, &chip->top_irq_data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add top irq_chip %d\n", ret);
		goto fail_client_reg;
	}

	ret = max77620_initialise_fps(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "FPS initialisation failed, %d\n", ret);
		goto fail_free_irq;
	}

	ret = max77620_init_backup_battery_charging(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev,
			"Backup battery charging init failed, %d\n", ret);
		goto fail_free_irq;
	}

	ret = max77620_init_low_battery_monitor(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "Low battery monitor init failed, %d\n",
			ret);
		goto fail_free_irq;
	}

	ret =  mfd_add_devices(&client->dev, -1, children->cells,
			children->ncells, NULL, 0,
			regmap_irq_get_domain(chip->top_irq_data));
	if (ret < 0) {
		dev_err(&client->dev, "mfd add dev fail %d\n", ret);
		goto fail_free_irq;
	}

	chip->irq_mbattlow = max77620_irq_get_virq(chip->dev,
					MAX77620_IRQ_LBT_MBATLOW);
	if (chip->irq_mbattlow) {
		ret = devm_request_threaded_irq(chip->dev, chip->irq_mbattlow,
			NULL, max77620_mbattlow_irq,
			IRQF_ONESHOT, dev_name(chip->dev),
			chip);
		if (ret < 0)
			dev_err(&client->dev, "request irq %d failed: %d\n",
			chip->irq_mbattlow, ret);
	}

	dev_info(&client->dev, "max77620 probe successfully\n");
	return 0;

fail_free_irq:
	regmap_del_irq_chip(chip->chip_irq, chip->top_irq_data);

fail_client_reg:
	for (i = 0; i < MAX77620_NUM_SLAVES; i++) {
		if (!chip->clients[i] || chip->clients[i] == client)
			continue;
		i2c_unregister_device(chip->clients[i]);
	}
	return ret;
}

static int max77620_remove(struct i2c_client *client)
{

	struct max77620_chip *chip = i2c_get_clientdata(client);
	int i;

	mfd_remove_devices(chip->dev);
	regmap_del_irq_chip(chip->chip_irq, chip->top_irq_data);

	for (i = 0; i < MAX77620_NUM_SLAVES; i++) {
		if (chip->clients[i] != client)
			i2c_unregister_device(chip->clients[i]);
	}

	return 0;
}

static void max77620_shutdown(struct i2c_client *i2c)
{
	struct max77620_chip *chip = i2c_get_clientdata(i2c);

	chip->shutdown = true;
}

#ifdef CONFIG_PM_SLEEP
static int max77620_set_fps_period(struct max77620_chip *chip,
	int fps_id, int time_period)
{
	unsigned int config;
	struct device *dev = chip->dev;
	int base_fps_time = (chip->id == MAX20024) ? 20 : 40;
	int ret;
	int i;

	for (i = 0; i < 0x7; ++i) {
		int x = base_fps_time * BIT(i);
		if (x >= time_period)
			break;
	}
	config = (i & 0x7) << MAX77620_FPS_TIME_PERIOD_SHIFT;
	ret = max77620_reg_update(dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_FPS_CFG0 + fps_id,
			MAX77620_FPS_TIME_PERIOD_MASK, config);
	if (ret < 0) {
		dev_err(dev, "Reg 0x%02x write failed, %d\n",
			MAX77620_REG_FPS_CFG0 + fps_id, ret);
		return ret;
	}
	return 0;
}

static int max77620_i2c_suspend(struct device *dev)
{
	struct max77620_chip *chip = dev_get_drvdata(dev);
	unsigned int config;
	int fps;
	int ret;

	for (fps = 0; fps < 2; ++fps) {
		if (chip->suspend_fps_period[fps] < 0)
			continue;

		ret = max77620_set_fps_period(chip, fps,
				chip->suspend_fps_period[fps]);
		if (ret < 0)
			dev_err(dev, "FPS%d config failed\n", fps);
	}

	/*
	 * For MAX20024: No need to configure SLPEN on suspend as
	 * it will be configured on Init.
	 */
	if (chip->id == MAX20024)
		return 0;

	config = (chip->sleep_enable) ? MAX77620_ONOFFCNFG1_SLPEN : 0;
	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG1, MAX77620_ONOFFCNFG1_SLPEN,
			config);
	if (ret < 0)
		dev_err(dev, "Reg ONOFFCNFG1 update failed: %d\n", ret);

	/* Disable WK_EN0 */
	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
			MAX77620_REG_ONOFFCNFG2, MAX77620_ONOFFCNFG2_WK_EN0, 0);
	if (ret < 0) {
		dev_err(dev, "Reg ONOFFCNFG2 WK_EN0 update failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int max77620_i2c_suspend_no_irq(struct device *dev)
{
	struct max77620_chip *chip = dev_get_drvdata(dev);

	return regmap_irq_suspend_noirq(chip->top_irq_data);
}

static int max77620_i2c_resume(struct device *dev)
{
	struct max77620_chip *chip = dev_get_drvdata(dev);
	int ret;
	int fps;

	for (fps = 0; fps < 2; ++fps) {
		if (chip->active_fps_period[fps] < 0)
			continue;

		ret = max77620_set_fps_period(chip, fps,
				chip->active_fps_period[fps]);
		if (ret < 0)
			dev_err(dev, "FPS%d config failed\n", fps);
	}

	/*
	 * For MAX20024: No need to configure WKEN0 on resume as
	 * it is configured on Init.
	 */
	if (chip->id == MAX20024)
		return 0;

	/* Enable WK_EN0 */
	ret = max77620_reg_update(chip->dev, MAX77620_PWR_SLAVE,
		MAX77620_REG_ONOFFCNFG2, MAX77620_ONOFFCNFG2_WK_EN0,
		MAX77620_ONOFFCNFG2_WK_EN0);
	if (ret < 0) {
		dev_err(dev, "Reg ONOFFCNFG2 WK_EN0 update failed: %d\n", ret);
		return ret;
	}

	return regmap_irq_resume(chip->top_irq_data);
}
#endif

static const struct i2c_device_id max77620_id[] = {
	{"max77620", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max77620_id);

static const struct dev_pm_ops max77620_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77620_i2c_suspend, max77620_i2c_resume)
#ifdef CONFIG_PM_SLEEP
	.suspend_noirq = max77620_i2c_suspend_no_irq,
#endif
};

static struct i2c_driver max77620_driver = {
	.driver = {
		.name = "max77620",
		.owner = THIS_MODULE,
		.pm = &max77620_pm_ops,
		.of_match_table = max77620_of_match,
	},
	.probe = max77620_probe,
	.remove = max77620_remove,
	.shutdown = max77620_shutdown,
	.id_table = max77620_id,
};

static int __init max77620_init(void)
{
	return i2c_add_driver(&max77620_driver);
}
subsys_initcall(max77620_init);

static void __exit max77620_exit(void)
{
	i2c_del_driver(&max77620_driver);
}
module_exit(max77620_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX77620 Multi Function Device Core Driver");
MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_ALIAS("i2c:max77620-core");
