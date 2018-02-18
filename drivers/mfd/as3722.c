/*
 * Core driver for ams AS3722 PMICs
 *
 * Copyright (C) 2013 AMS AG
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Florian Lobmaier <florian.lobmaier@ams.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mfd/as3722.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define AS3722_DEVICE_ID	0x0C

enum {
	AS3722_PINCTRL_ID,
	AS3722_REGULATOR_ID,
	AS3722_RTC_ID,
	AS3722_ADC,
	AS3722_POWER_OFF_ID,
	AS3722_CLK_ID,
	AS3722_THERMAL_ID,
	AS3722_WATCHDOG_ID,
};

static const struct resource as3722_rtc_resource[] = {
	{
		.name = "as3722-rtc-alarm",
		.start = AS3722_IRQ_RTC_ALARM,
		.end = AS3722_IRQ_RTC_ALARM,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource as3722_adc_resource[] = {
	{
		.name = "as3722-adc",
		.start = AS3722_IRQ_ADC,
		.end = AS3722_IRQ_ADC,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource as3722_thermal_resource[] = {
	{
		.name = "as3722-sd0-alarm",
		.start = AS3722_IRQ_TEMP_SD0_ALARM,
		.end = AS3722_IRQ_TEMP_SD0_ALARM,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "as3722-sd1-alarm",
		.start = AS3722_IRQ_TEMP_SD1_ALARM,
		.end = AS3722_IRQ_TEMP_SD1_ALARM,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "as3722-sd6-alarm",
		.start = AS3722_IRQ_TEMP_SD6_ALARM,
		.end = AS3722_IRQ_TEMP_SD6_ALARM,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct mfd_cell as3722_devs[] = {
	{
		.name = "as3722-pinctrl",
		.id = AS3722_PINCTRL_ID,
	},
	{
		.name = "as3722-regulator",
		.id = AS3722_REGULATOR_ID,
	},
	{
		.name = "as3722-clk",
		.id = AS3722_CLK_ID,
	},
	{
		.name = "as3722-rtc",
		.num_resources = ARRAY_SIZE(as3722_rtc_resource),
		.resources = as3722_rtc_resource,
		.id = AS3722_RTC_ID,
	},
	{
		.name = "as3722-adc-extcon",
		.num_resources = ARRAY_SIZE(as3722_adc_resource),
		.resources = as3722_adc_resource,
		.id = AS3722_ADC,
	},
	{
		.name = "as3722-power-off",
		.id = AS3722_POWER_OFF_ID,
	},
	{
		.name = "as3722-thermal",
		.num_resources = ARRAY_SIZE(as3722_thermal_resource),
		.resources = as3722_thermal_resource,
		.id = AS3722_THERMAL_ID,
	},
	{
		.name = "as3722-wdt",
		.id = AS3722_WATCHDOG_ID,
	},
};

static const struct regmap_irq as3722_irqs[] = {
	/* INT1 IRQs */
	[AS3722_IRQ_LID] = {
		.mask = AS3722_INTERRUPT_MASK1_LID,
	},
	[AS3722_IRQ_ACOK] = {
		.mask = AS3722_INTERRUPT_MASK1_ACOK,
	},
	[AS3722_IRQ_ENABLE1] = {
		.mask = AS3722_INTERRUPT_MASK1_ENABLE1,
	},
	[AS3722_IRQ_OCCUR_ALARM_SD0] = {
		.mask = AS3722_INTERRUPT_MASK1_OCURR_ALARM_SD0,
	},
	[AS3722_IRQ_ONKEY_LONG_PRESS] = {
		.mask = AS3722_INTERRUPT_MASK1_ONKEY_LONG,
	},
	[AS3722_IRQ_ONKEY] = {
		.mask = AS3722_INTERRUPT_MASK1_ONKEY,
	},
	[AS3722_IRQ_OVTMP] = {
		.mask = AS3722_INTERRUPT_MASK1_OVTMP,
	},
	[AS3722_IRQ_LOWBAT] = {
		.mask = AS3722_INTERRUPT_MASK1_LOWBAT,
	},

	/* INT2 IRQs */
	[AS3722_IRQ_SD0_LV] = {
		.mask = AS3722_INTERRUPT_MASK2_SD0_LV,
		.reg_offset = 1,
	},
	[AS3722_IRQ_SD1_LV] = {
		.mask = AS3722_INTERRUPT_MASK2_SD1_LV,
		.reg_offset = 1,
	},
	[AS3722_IRQ_SD2_LV] = {
		.mask = AS3722_INTERRUPT_MASK2_SD2345_LV,
		.reg_offset = 1,
	},
	[AS3722_IRQ_PWM1_OV_PROT] = {
		.mask = AS3722_INTERRUPT_MASK2_PWM1_OV_PROT,
		.reg_offset = 1,
	},
	[AS3722_IRQ_PWM2_OV_PROT] = {
		.mask = AS3722_INTERRUPT_MASK2_PWM2_OV_PROT,
		.reg_offset = 1,
	},
	[AS3722_IRQ_ENABLE2] = {
		.mask = AS3722_INTERRUPT_MASK2_ENABLE2,
		.reg_offset = 1,
	},
	[AS3722_IRQ_SD6_LV] = {
		.mask = AS3722_INTERRUPT_MASK2_SD6_LV,
		.reg_offset = 1,
	},
	[AS3722_IRQ_RTC_REP] = {
		.mask = AS3722_INTERRUPT_MASK2_RTC_REP,
		.reg_offset = 1,
	},

	/* INT3 IRQs */
	[AS3722_IRQ_RTC_ALARM] = {
		.mask = AS3722_INTERRUPT_MASK3_RTC_ALARM,
		.reg_offset = 2,
	},
	[AS3722_IRQ_GPIO1] = {
		.mask = AS3722_INTERRUPT_MASK3_GPIO1,
		.reg_offset = 2,
	},
	[AS3722_IRQ_GPIO2] = {
		.mask = AS3722_INTERRUPT_MASK3_GPIO2,
		.reg_offset = 2,
	},
	[AS3722_IRQ_GPIO3] = {
		.mask = AS3722_INTERRUPT_MASK3_GPIO3,
		.reg_offset = 2,
	},
	[AS3722_IRQ_GPIO4] = {
		.mask = AS3722_INTERRUPT_MASK3_GPIO4,
		.reg_offset = 2,
	},
	[AS3722_IRQ_GPIO5] = {
		.mask = AS3722_INTERRUPT_MASK3_GPIO5,
		.reg_offset = 2,
	},
	[AS3722_IRQ_WATCHDOG] = {
		.mask = AS3722_INTERRUPT_MASK3_WATCHDOG,
		.reg_offset = 2,
	},
	[AS3722_IRQ_ENABLE3] = {
		.mask = AS3722_INTERRUPT_MASK3_ENABLE3,
		.reg_offset = 2,
	},

	/* INT4 IRQs */
	[AS3722_IRQ_TEMP_SD0_SHUTDOWN] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD0_SHUTDOWN,
		.reg_offset = 3,
	},
	[AS3722_IRQ_TEMP_SD1_SHUTDOWN] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD1_SHUTDOWN,
		.reg_offset = 3,
	},
	[AS3722_IRQ_TEMP_SD2_SHUTDOWN] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD6_SHUTDOWN,
		.reg_offset = 3,
	},
	[AS3722_IRQ_TEMP_SD0_ALARM] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD0_ALARM,
		.reg_offset = 3,
	},
	[AS3722_IRQ_TEMP_SD1_ALARM] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD1_ALARM,
		.reg_offset = 3,
	},
	[AS3722_IRQ_TEMP_SD6_ALARM] = {
		.mask = AS3722_INTERRUPT_MASK4_TEMP_SD6_ALARM,
		.reg_offset = 3,
	},
	[AS3722_IRQ_OCCUR_ALARM_SD6] = {
		.mask = AS3722_INTERRUPT_MASK4_OCCUR_ALARM_SD6,
		.reg_offset = 3,
	},
	[AS3722_IRQ_ADC] = {
		.mask = AS3722_INTERRUPT_MASK4_ADC,
		.reg_offset = 3,
	},
};

static const struct regmap_irq_chip as3722_irq_chip = {
	.name = "as3722",
	.irqs = as3722_irqs,
	.num_irqs = ARRAY_SIZE(as3722_irqs),
	.num_regs = 4,
	.status_base = AS3722_INTERRUPT_STATUS1_REG,
	.mask_base = AS3722_INTERRUPT_MASK1_REG,
};

/* OTP version table
 * Version: ID1(90h): ID2(91h): ID3(CCh[7:4])
 * 1v0	0Ch	0h	0h
 * 1v1	0Ch	1h	0h
 * 1v2	0Ch	1h	0h
 * 1v21	0Ch	1h	1h
 *
 * Note: for 1V1 and 1V2, the actual version is detected by platform data.
 * If there is no platfrom data then use the 1V1.
 */
static int as3722_check_device_id(struct as3722 *as3722)
{
	u32 id1, id2, id3;
	int major, minor;
	int ret;

	/* Check that this is actually a AS3722 */
	ret = as3722_read(as3722, AS3722_ASIC_ID1_REG, &id1);
	if (ret < 0) {
		dev_err(as3722->dev, "ASIC_ID1 read failed: %d\n", ret);
		return ret;
	}

	if (id1 != AS3722_DEVICE_ID) {
		dev_err(as3722->dev, "Device is not AS3722, ID is 0x%x\n", id1);
		return -ENODEV;
	}

	ret = as3722_read(as3722, AS3722_ASIC_ID2_REG, &id2);
	if (ret < 0) {
		dev_err(as3722->dev, "ASIC_ID2 read failed: %d\n", ret);
		return ret;
	}

	ret = as3722_read(as3722, AS3722_ASIC_ID3_REG, &id3);
	if (ret < 0) {
		dev_err(as3722->dev, "ASIC_ID3 read failed: %d\n", ret);
		return ret;
	}

	dev_info(as3722->dev,
		"AS3722 ID: ID1:ID2:ID3 = 0x%02x:0x%02x:0x%02x\n",
		id1, id2, id3);

	id3 = (id3 >> 4) & 0xF;
	major = 1;
	switch (id2) {
	case 0:
		if (id3 == 0) {
			minor = 0;
		} else {
			dev_err(as3722->dev, "Non supported IDs\n");
			return -ENODEV;
		}
		break;
	case 1:
		if (id3 == 0) {
			minor = 1;
		} else if (id3 == 1) {
			minor = 21;
		} else {
			dev_err(as3722->dev, "Non supported IDs\n");
			return -ENODEV;
		}
		break;
	default:
		dev_err(as3722->dev, "Non supported IDs\n");
		return -ENODEV;
	}

	/* Override the major/minor for 1V1 and 1V2 only */
	if (as3722->major_rev && as3722->minor_rev && (minor == 1)) {
		dev_info(as3722->dev,
			"Device version %dV%d and platform version %dV%d\n",
			major, minor, as3722->major_rev, as3722->minor_rev);
	} else {
		as3722->major_rev = major;
		as3722->minor_rev = minor;
	}

	dev_info(as3722->dev, "Final OTP version %dV%d\n",
		as3722->major_rev, as3722->minor_rev);
	return 0;
}

static int as3722_configure_pullups(struct as3722 *as3722)
{
	int ret;
	u32 val = 0;

	if (as3722->en_intern_int_pullup)
		val |= AS3722_INT_PULL_UP;
	if (as3722->en_intern_i2c_pullup)
		val |= AS3722_I2C_PULL_UP;

	ret = as3722_update_bits(as3722, AS3722_IOVOLTAGE_REG,
			AS3722_INT_PULL_UP | AS3722_I2C_PULL_UP, val);
	if (ret < 0)
		dev_err(as3722->dev, "IOVOLTAGE_REG update failed: %d\n", ret);
	return ret;
}

static const struct regmap_range as3722_readable_ranges[] = {
	regmap_reg_range(AS3722_SD0_VOLTAGE_REG, AS3722_SD6_VOLTAGE_REG),
	regmap_reg_range(AS3722_GPIO0_CONTROL_REG, AS3722_LDO7_VOLTAGE_REG),
	regmap_reg_range(AS3722_LDO9_VOLTAGE_REG, AS3722_REG_SEQU_MOD3_REG),
	regmap_reg_range(AS3722_SD_PHSW_CTRL_REG, AS3722_PWM_CONTROL_H_REG),
	regmap_reg_range(AS3722_WATCHDOG_TIMER_REG, AS3722_WATCHDOG_TIMER_REG),
	regmap_reg_range(AS3722_WATCHDOG_SOFTWARE_SIGNAL_REG,
					AS3722_BATTERY_VOLTAGE_MONITOR2_REG),
	regmap_reg_range(AS3722_SD_CONTROL_REG, AS3722_PWM_VCONTROL4_REG),
	regmap_reg_range(AS3722_BB_CHARGER_REG, AS3722_SRAM_REG),
	regmap_reg_range(AS3722_RTC_ACCESS_REG, AS3722_RTC_ACCESS_REG),
	regmap_reg_range(AS3722_RTC_STATUS_REG, AS3722_TEMP_STATUS_REG),
	regmap_reg_range(AS3722_ADC0_CONTROL_REG, AS3722_ADC_CONFIGURATION_REG),
	regmap_reg_range(AS3722_ASIC_ID1_REG, AS3722_ASIC_ID3_REG),
	regmap_reg_range(AS3722_LOCK_REG, AS3722_LOCK_REG),
	regmap_reg_range(AS3722_FUSE7_REG, AS3722_FUSE7_REG),
};

static const struct regmap_access_table as3722_readable_table = {
	.yes_ranges = as3722_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(as3722_readable_ranges),
};

static const struct regmap_range as3722_writable_ranges[] = {
	regmap_reg_range(AS3722_SD0_VOLTAGE_REG, AS3722_SD6_VOLTAGE_REG),
	regmap_reg_range(AS3722_GPIO0_CONTROL_REG, AS3722_LDO7_VOLTAGE_REG),
	regmap_reg_range(AS3722_LDO9_VOLTAGE_REG, AS3722_GPIO_SIGNAL_OUT_REG),
	regmap_reg_range(AS3722_REG_SEQU_MOD1_REG, AS3722_REG_SEQU_MOD3_REG),
	regmap_reg_range(AS3722_SD_PHSW_CTRL_REG, AS3722_PWM_CONTROL_H_REG),
	regmap_reg_range(AS3722_WATCHDOG_TIMER_REG, AS3722_WATCHDOG_TIMER_REG),
	regmap_reg_range(AS3722_WATCHDOG_SOFTWARE_SIGNAL_REG,
					AS3722_BATTERY_VOLTAGE_MONITOR2_REG),
	regmap_reg_range(AS3722_SD_CONTROL_REG, AS3722_PWM_VCONTROL4_REG),
	regmap_reg_range(AS3722_BB_CHARGER_REG, AS3722_SRAM_REG),
	regmap_reg_range(AS3722_INTERRUPT_MASK1_REG, AS3722_TEMP_STATUS_REG),
	regmap_reg_range(AS3722_ADC0_CONTROL_REG, AS3722_ADC1_CONTROL_REG),
	regmap_reg_range(AS3722_ADC1_THRESHOLD_HI_MSB_REG,
					AS3722_ADC_CONFIGURATION_REG),
	regmap_reg_range(AS3722_LOCK_REG, AS3722_LOCK_REG),
};

static void as3722_regmap_config_lock(void *lock)
{
	struct as3722 *as3722 = lock;

	if (as3722->shutdown && (in_atomic() || irqs_disabled())) {
		dev_info(as3722->dev, "Xfer without lock\n");
		return;
	}

	mutex_lock(&as3722->mutex_config);
}

static void as3722_regmap_config_unlock(void *lock)
{
	struct as3722 *as3722 = lock;

	if (as3722->shutdown && (in_atomic() || irqs_disabled()))
		return;

	mutex_unlock(&as3722->mutex_config);
}


static const struct regmap_access_table as3722_writable_table = {
	.yes_ranges = as3722_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(as3722_writable_ranges),
};

static const struct regmap_range as3722_cacheable_ranges[] = {
	regmap_reg_range(AS3722_SD0_VOLTAGE_REG, AS3722_LDO11_VOLTAGE_REG),
	regmap_reg_range(AS3722_SD_CONTROL_REG, AS3722_LDOCONTROL1_REG),
};

static const struct regmap_access_table as3722_volatile_table = {
	.no_ranges = as3722_cacheable_ranges,
	.n_no_ranges = ARRAY_SIZE(as3722_cacheable_ranges),
};

static const struct regmap_range vsel_sd_range =
	regmap_reg_range(AS3722_SD0_VOLTAGE_REG, AS3722_SD6_VOLTAGE_REG);

static const struct regmap_range vsel_ldo_range =
	regmap_reg_range(AS3722_LDO0_VOLTAGE_REG, AS3722_LDO11_VOLTAGE_REG);


bool is_volatile_as3722_register(struct device *dev, unsigned int reg)
{
	if (regmap_reg_in_range(reg, &vsel_sd_range) ||
	    regmap_reg_in_range(reg, &vsel_ldo_range)) {
		struct as3722 *as3722 = dev_get_drvdata(dev);
		return test_bit(reg, as3722->volatile_vsel_registers);
	}

	if (regmap_reg_in_ranges(reg, as3722_volatile_table.no_ranges,
				 as3722_volatile_table.n_no_ranges))
		return false;

	return true;
}

int as3722_vsel_volatile_set(struct device *dev, unsigned int reg,
			      bool is_volatile)
{
	if (regmap_reg_in_range(reg, &vsel_sd_range) ||
	    regmap_reg_in_range(reg, &vsel_ldo_range)) {
		struct as3722 *as3722 = dev_get_drvdata(dev);
		if (is_volatile)
			set_bit(reg, as3722->volatile_vsel_registers);
		else
			clear_bit(reg, as3722->volatile_vsel_registers);
		return 0;
	}

	return -EINVAL;
}

static struct regmap_config as3722_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = AS3722_MAX_REGISTER,
	.cache_type = REGCACHE_RBTREE,
	.rd_table = &as3722_readable_table,
	.wr_table = &as3722_writable_table,
	.volatile_reg = is_volatile_as3722_register,
	.lock = as3722_regmap_config_lock,
	.unlock = as3722_regmap_config_unlock,
	.reg_volatile_set = as3722_vsel_volatile_set,
};

static int as3722_i2c_of_probe(struct i2c_client *i2c,
			struct as3722 *as3722)
{
	struct device_node *np = i2c->dev.of_node;
	struct irq_data *irq_data;
	u32 pval;
	int ret;

	if (!np) {
		dev_err(&i2c->dev, "Device Tree not found\n");
		return -EINVAL;
	}

	irq_data = irq_get_irq_data(i2c->irq);
	if (!irq_data) {
		dev_err(&i2c->dev, "Invalid IRQ: %d\n", i2c->irq);
		return -EINVAL;
	}

	as3722->en_intern_int_pullup = of_property_read_bool(np,
					"ams,enable-internal-int-pullup");
	as3722->en_intern_i2c_pullup = of_property_read_bool(np,
					"ams,enable-internal-i2c-pullup");
	as3722->en_ac_ok_pwr_on = of_property_read_bool(np,
					"ams,enable-ac-ok-power-on");
	as3722->irq_flags = irqd_get_trigger_type(irq_data);
	as3722->irq_base = -1;
	of_property_read_u32(np, "ams,major-rev", &as3722->major_rev);
	of_property_read_u32(np, "ams,minor-rev", &as3722->minor_rev);

	as3722->backup_battery_chargable =
		of_property_read_bool(np, "ams,backup-battery-chargable");
	if (!as3722->backup_battery_chargable)
		goto skip_chg_param;

	ret = of_property_read_u32(np, "ams,battery-backup-charge-current",
			&pval);
	if (!ret)
		as3722->backup_battery_charge_current = pval;

	as3722->battery_backup_enable_bypass = of_property_read_bool(np,
			"ams,battery-backup-bypass-out-resistor");

	ret = of_property_read_u32(np, "ams,battery-backup-charge-mode", &pval);
	if (!ret)
		as3722->battery_backup_charge_mode = pval;

skip_chg_param:
	of_property_read_u32(np, "ams,oc-pg-mask", &as3722->oc_pg_mask);
	dev_dbg(&i2c->dev, "IRQ flags are 0x%08lx\n", as3722->irq_flags);
	return 0;
}

static int as3722_i2c_non_of_probe(struct i2c_client *i2c,
			struct as3722 *as3722)
{
	struct as3722_platform_data *pdata = dev_get_platdata(&i2c->dev);

	if (!pdata)
		return -EINVAL;

	as3722->irq_flags = pdata->irq_type;
	as3722->irq_base = pdata->irq_base;
	as3722->en_intern_i2c_pullup = pdata->use_internal_i2c_pullup;
	as3722->en_intern_int_pullup = pdata->use_internal_int_pullup;
	as3722->en_ac_ok_pwr_on = pdata->enable_ac_ok_power_on;
	as3722->major_rev = pdata->major_rev;
	as3722->minor_rev = pdata->minor_rev;
	as3722->backup_battery_chargable = pdata->backup_battery_chargable;
	as3722->backup_battery_charge_current =
		pdata->backup_battery_charge_current;
	as3722->battery_backup_enable_bypass =
		pdata->battery_backup_enable_bypass;
	as3722->battery_backup_charge_mode = pdata->battery_backup_charge_mode;
	as3722->oc_pg_mask = pdata->oc_pg_mask;
	return 0;
}

static int as3722_i2c_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	struct as3722 *as3722;
	unsigned long irq_flags;
	int ret;
	u8 val = 0;

	as3722 = devm_kzalloc(&i2c->dev, sizeof(struct as3722), GFP_KERNEL);
	if (!as3722)
		return -ENOMEM;

	as3722->dev = &i2c->dev;
	as3722->chip_irq = i2c->irq;
	i2c_set_clientdata(i2c, as3722);
	as3722->client = i2c;

	ret = as3722_i2c_non_of_probe(i2c, as3722);
	if (ret < 0) {
		ret = as3722_i2c_of_probe(i2c, as3722);
		if (ret < 0)
			return ret;
	}

	mutex_init(&as3722->mutex_config);
	as3722_regmap_config.lock_arg = as3722;
	as3722->regmap = devm_regmap_init_i2c(i2c,
			(const struct regmap_config *)&as3722_regmap_config);
	if (IS_ERR(as3722->regmap)) {
		ret = PTR_ERR(as3722->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	ret = as3722_check_device_id(as3722);
	if (ret < 0)
		return ret;

	irq_flags = as3722->irq_flags | IRQF_ONESHOT;
	ret = regmap_add_irq_chip(as3722->regmap, as3722->chip_irq,
			irq_flags, as3722->irq_base, &as3722_irq_chip,
			&as3722->irq_data);
	if (ret < 0) {
		dev_err(as3722->dev, "Failed to add regmap irq: %d\n", ret);
		return ret;
	}

	ret = as3722_configure_pullups(as3722);
	if (ret < 0)
		goto scrub;

	if (as3722->en_ac_ok_pwr_on)
		val = AS3722_CTRL_SEQ1_AC_OK_PWR_ON;
	ret = as3722_update_bits(as3722, AS3722_CTRL_SEQU1_REG,
			AS3722_CTRL_SEQ1_AC_OK_PWR_ON, val);
	if (ret < 0) {
		dev_err(as3722->dev, "CTRL_SEQ1 update failed: %d\n", ret);
		goto scrub;
	}
	if (as3722->backup_battery_chargable) {
		unsigned int val;
		unsigned int mask;

		mask = AS3722_BBCCUR_MASK | AS3722_BBCRESOFF_MASK |
				AS3722_BBCMODE_MASK;

		val = AS3722_BBCCUR_VAL(as3722->backup_battery_charge_current) |
				as3722->battery_backup_charge_mode;
		if (as3722->battery_backup_enable_bypass)
			val |= AS3722_BBCRESOFF_MASK;
		ret = as3722_update_bits(as3722, AS3722_BB_CHARGER_REG,
			mask, val);
		if (ret < 0) {
			dev_err(as3722->dev,
				"BB_CHARGING update failed: %d\n", ret);
			goto scrub;
		}
	}
	if (as3722->oc_pg_mask) {
		unsigned int mask1 = 0;
		unsigned int mask2 = 0;
		unsigned int oc_pg_mask = as3722->oc_pg_mask;

		if (oc_pg_mask & AS3722_OC_PG_MASK_AC_OK)
			mask1 |= AS3722_PG_AC_OK_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_GPIO3)
			mask1 |= AS3722_PG_GPIO3_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_GPIO4)
			mask1 |= AS3722_PG_GPIO4_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_GPIO5)
			mask1 |= AS3722_PG_GPIO5_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_PWRGOOD_SD0)
			mask1 |= AS3722_PG_PWRGOOD_SD0_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_OVCURR_SD0)
			mask1 |= AS3722_PG_OVCURR_SD0_MASK;

		if (oc_pg_mask & AS3722_OC_PG_MASK_POWERGOOD_SD6)
			mask2 |= AS3722_PG_POWERGOOD_SD6_MASK;
		if (oc_pg_mask & AS3722_OC_PG_MASK_OVCURR_SD6)
			mask2 |= AS3722_PG_OVCURR_SD6_MASK;

		ret = as3722_update_bits(as3722, AS3722_OC_PG_CTRL_REG,
				mask1, mask1);
		if (ret < 0) {
			dev_err(as3722->dev, "oc_pg_ctrl update failed: %d\n",
				ret);
			goto scrub;
		}

		ret = as3722_update_bits(as3722, AS3722_OC_PG_CTRL2_REG,
				mask2, mask2);
		if (ret < 0) {
			dev_err(as3722->dev, "oc_pg_ctrl2 update failed: %d\n",
				ret);
			goto scrub;
		}
	}
	ret = mfd_add_devices(&i2c->dev, -1, as3722_devs,
			ARRAY_SIZE(as3722_devs), NULL, 0,
			regmap_irq_get_domain(as3722->irq_data));
	if (ret) {
		dev_err(as3722->dev, "Failed to add MFD devices: %d\n", ret);
		goto scrub;
	}

	dev_dbg(as3722->dev, "AS3722 core driver initialized successfully\n");
	return 0;

scrub:
	regmap_del_irq_chip(as3722->chip_irq, as3722->irq_data);
	return ret;
}

static int as3722_i2c_remove(struct i2c_client *i2c)
{
	struct as3722 *as3722 = i2c_get_clientdata(i2c);

	mfd_remove_devices(as3722->dev);
	regmap_del_irq_chip(as3722->chip_irq, as3722->irq_data);
	return 0;
}

static int as3722_i2c_suspend_no_irq(struct device *dev)
{
	struct as3722 *as3722 = dev_get_drvdata(dev);

	return regmap_irq_suspend_noirq(as3722->irq_data);
}

static int palmas_i2c_resume(struct device *dev)
{
	struct as3722 *as3722 = dev_get_drvdata(dev);

	return regmap_irq_resume(as3722->irq_data);
}

static void as3722_i2c_shutdown(struct i2c_client *i2c)
{
	struct as3722 *as3722 = i2c_get_clientdata(i2c);

	as3722->shutdown = true;
}

static const struct of_device_id as3722_of_match[] = {
	{ .compatible = "ams,as3722", },
	{},
};
MODULE_DEVICE_TABLE(of, as3722_of_match);

static const struct i2c_device_id as3722_i2c_id[] = {
	{ "as3722", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, as3722_i2c_id);

static const struct dev_pm_ops as3722_pm_ops = {
	.suspend_noirq = as3722_i2c_suspend_no_irq,
	.resume = palmas_i2c_resume,
};

static struct i2c_driver as3722_i2c_driver = {
	.driver = {
		.name = "as3722",
		.owner = THIS_MODULE,
		.of_match_table = as3722_of_match,
		.pm = &as3722_pm_ops,
	},
	.probe = as3722_i2c_probe,
	.remove = as3722_i2c_remove,
	.shutdown = as3722_i2c_shutdown,
	.id_table = as3722_i2c_id,
};

static int __init as3722_i2c_init(void)
{
	return i2c_add_driver(&as3722_i2c_driver);
}

subsys_initcall(as3722_i2c_init);

static void __exit as3722_i2c_exit(void)
{
	i2c_del_driver(&as3722_i2c_driver);
}

module_exit(as3722_i2c_exit);

MODULE_DESCRIPTION("I2C support for AS3722 PMICs");
MODULE_AUTHOR("Florian Lobmaier <florian.lobmaier@ams.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL");
