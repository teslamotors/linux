/*
 * drivers/power/bq20z75_battery.c
 *
 * Gas Gauge driver for TI's BQ20Z75
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>

#include <mach/gpio.h>

enum {
	REG_MANUFACTURER_DATA,
	REG_TEMPERATURE,
	REG_VOLTAGE,
	REG_CURRENT,
	REG_TIME_TO_EMPTY,
	REG_TIME_TO_FULL,
	REG_STATUS,
	REG_CYCLE_COUNT,
	REG_CAPACITY,
	REG_SERIAL_NUMBER,
	REG_MAX
};

#define BATTERY_MANUFACTURER_SIZE	12
#define BATTERY_NAME_SIZE		8

/* manufacturer access defines */
#define MANUFACTURER_ACCESS_STATUS	0x0006
#define MANUFACTURER_ACCESS_SLEEP	0x0011

/* battery status value bits */
#define BATTERY_INIT_DONE		0x80
#define BATTERY_DISCHARGING		0x40
#define BATTERY_FULL_CHARGED		0x20
#define BATTERY_FULL_DISCHARGED		0x10

#define BATTERY_POLL_PERIOD		30000

#define BQ20Z75_DATA(_psp, _addr, _min_value, _max_value)	\
	{							\
		.psp = POWER_SUPPLY_PROP_##_psp,		\
		.addr = _addr,					\
		.min_value = _min_value,			\
		.max_value = _max_value,			\
	}

static struct bq20z75_device_data {
	enum power_supply_property psp;
	u8 addr;
	int min_value;
	int max_value;
} bq20z75_data[] = {
	[REG_MANUFACTURER_DATA] = BQ20Z75_DATA(PRESENT, 0x00, 0, 65535),
	[REG_TEMPERATURE]       = BQ20Z75_DATA(TEMP, 0x08, 0, 65535),
	[REG_VOLTAGE]           = BQ20Z75_DATA(VOLTAGE_NOW, 0x09, 0, 20000),
	[REG_CURRENT]           = BQ20Z75_DATA(CURRENT_NOW, 0x0A, -32768, 32767),
	[REG_TIME_TO_EMPTY]     = BQ20Z75_DATA(TIME_TO_EMPTY_AVG, 0x12, 0, 65535),
	[REG_TIME_TO_FULL]      = BQ20Z75_DATA(TIME_TO_FULL_AVG, 0x13, 0, 65535),
	[REG_STATUS]            = BQ20Z75_DATA(STATUS, 0x16, 0, 65535),
	[REG_CYCLE_COUNT]       = BQ20Z75_DATA(CYCLE_COUNT, 0x17, 0, 65535),
	[REG_CAPACITY]          = BQ20Z75_DATA(CAPACITY, 0x0e, 0, 100),
	[REG_SERIAL_NUMBER]     = BQ20Z75_DATA(SERIAL_NUMBER, 0x1C, 0, 65535),
};

static enum power_supply_property bq20z75_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_SERIAL_NUMBER
};

static enum power_supply_property bq20z75_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *power_supplied_to[] = {
	"battery",
};

static int bq20z75_bat_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val);
static int bq20z75_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val);

enum supply_type {
	SUPPLY_TYPE_BATTERY = 0,
	SUPPLY_TYPE_AC,
};

static struct power_supply bq20z75_supply[] = {
	[SUPPLY_TYPE_BATTERY] = {
		.name		= "battery",
		.type		= POWER_SUPPLY_TYPE_BATTERY,
		.properties	= bq20z75_battery_properties,
		.num_properties	= ARRAY_SIZE(bq20z75_battery_properties),
		.get_property	= bq20z75_bat_get_property,
	},
	[SUPPLY_TYPE_AC] = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = power_supplied_to,
		.num_supplicants = ARRAY_SIZE(power_supplied_to),
		.properties = bq20z75_ac_properties,
		.num_properties = ARRAY_SIZE(bq20z75_ac_properties),
		.get_property = bq20z75_ac_get_property,
	},
};

static struct bq20z75_device_info {
	struct timer_list	battery_poll_timer;
	struct i2c_client	*client;
	int irq;
} *bq20z75_device;

static int bq20z75_get_ac_status(void)
{
	int charger_gpio = irq_to_gpio(bq20z75_device->irq);
	return !gpio_get_value(charger_gpio);
}

static int bq20z75_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = bq20z75_get_ac_status();
		break;
	default:
		dev_err(&bq20z75_device->client->dev,
			"%s: INVALID property\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int bq20z75_get_health(enum power_supply_property psp,
	union power_supply_propval *val)
{
	s32 ret;

	/* Write to ManufacturerAccess with
	 * ManufacturerAccess command and then
	 * read the status */
	ret = i2c_smbus_write_word_data(bq20z75_device->client,
		bq20z75_data[REG_MANUFACTURER_DATA].addr,
		MANUFACTURER_ACCESS_STATUS);
	if (ret < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: i2c write for battery presence "
			"failed\n", __func__);
		return -EINVAL;
	}

	ret = i2c_smbus_read_word_data(bq20z75_device->client,
		bq20z75_data[REG_MANUFACTURER_DATA].addr);
	if (ret < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: i2c read for battery presence "
			"failed\n", __func__);
		return -EINVAL;
	}

	if (ret >= bq20z75_data[REG_MANUFACTURER_DATA].min_value &&
	    ret <= bq20z75_data[REG_MANUFACTURER_DATA].max_value) {

		/* Mask the upper nibble of 2nd byte and
		 * lower byte of response then
		 * shift the result by 8 to get status*/
		ret &= 0x0F00;
		ret >>= 8;
		if (psp == POWER_SUPPLY_PROP_PRESENT) {
			if (ret == 0x0F)
				/* battery removed */
				val->intval = 0;
			else
				val->intval = 1;
		} else if (psp == POWER_SUPPLY_PROP_HEALTH) {
			if (ret == 0x09)
				val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			else if (ret == 0x0B)
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			else if (ret == 0x0C)
				val->intval = POWER_SUPPLY_HEALTH_DEAD;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}
	} else {
		val->intval = 0;
	}

	return 0;
}

static int bq20z75_get_psp(int reg_offset, enum power_supply_property psp,
	union power_supply_propval *val)
{
	s32 ret;
	int ac_status;

recheck:
	ret = i2c_smbus_read_word_data(bq20z75_device->client,
		bq20z75_data[reg_offset].addr);
	if (ret < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: i2c read for %d failed\n", __func__, reg_offset);
		return -EINVAL;
	}

	if (ret >= bq20z75_data[reg_offset].min_value &&
	    ret <= bq20z75_data[reg_offset].max_value) {
		val->intval = ret;
		if (psp == POWER_SUPPLY_PROP_STATUS) {
			/* mask the upper byte and then find the
			 * actual status */
			ret &= 0x00FF;

			/* check the error conditions
			 * lower nibble represent error
			 * 0 = no error, so check the remaining bits
			 * != 0 means error so return */
			if ((ret & 0x000F) >= 0x01) {
				val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
				return 0;
			}

			while (!(ret & BATTERY_INIT_DONE))
				goto recheck;

			ac_status = bq20z75_get_ac_status();
			val->intval = ac_status ?
				POWER_SUPPLY_STATUS_CHARGING :
				POWER_SUPPLY_STATUS_DISCHARGING;

			if (ret & BATTERY_FULL_CHARGED)
				val->intval = POWER_SUPPLY_STATUS_FULL;
		}
		/* bq20z75 provides battery tempreture in 0.1°K
		 * so convert it to °C */
		else if (psp == POWER_SUPPLY_PROP_TEMP)
			val->intval = ret - 2731;
	} else {
		val->intval = 0;
		if (psp == POWER_SUPPLY_PROP_STATUS)
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
	}

	return 0;
}

static int bq20z75_get_capacity(union power_supply_propval *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq20z75_device->client,
		bq20z75_data[REG_CAPACITY].addr);
	if (ret < 0) {
		dev_err(&bq20z75_device->client->dev, "%s: i2c read for %d "
			"failed\n", __func__, REG_CAPACITY);
		return -EINVAL;
	}

	/* bq20z75 spec says that this can be >100 %
	 * even if max value is 100 % */
	val->intval = ((ret >= 100) ? 100 : ret);

	return 0;
}

static char bq20z75_serial[5];
static int bq20z75_get_battery_serial_number(union power_supply_propval *val)
{
	int ret;

	ret = i2c_smbus_read_word_data(bq20z75_device->client,
		bq20z75_data[REG_SERIAL_NUMBER].addr);
	if (ret < 0)
		return ret;

	ret = sprintf(bq20z75_serial, "%04x", ret);
	val->strval = bq20z75_serial;

	return 0;
}

static int bq20z75_bat_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	u8 count;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_HEALTH:
		if (bq20z75_get_health(psp, val))
			return -EINVAL;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (bq20z75_get_capacity(val))
			return -EINVAL;
		break;

	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		if (bq20z75_get_battery_serial_number(val))
			return -EINVAL;
		break;

	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		for (count = 0; count < REG_MAX; count++) {
			if (psp == bq20z75_data[count].psp)
				break;
		}

		if (bq20z75_get_psp(count, psp, val))
			return -EINVAL;
		break;

	default:
		dev_err(&bq20z75_device->client->dev,
			"%s: INVALID property\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t ac_present_irq(int irq, void *data)
{
	power_supply_changed(&bq20z75_supply[SUPPLY_TYPE_AC]);
	power_supply_changed(&bq20z75_supply[SUPPLY_TYPE_BATTERY]);
	return IRQ_HANDLED;
}

static void battery_poll_timer_func(unsigned long unused)
{
	power_supply_changed(&bq20z75_supply[SUPPLY_TYPE_BATTERY]);
	power_supply_changed(&bq20z75_supply[SUPPLY_TYPE_AC]);
	mod_timer(&bq20z75_device->battery_poll_timer,
		jiffies + msecs_to_jiffies(BATTERY_POLL_PERIOD));
}

static int bq20z75_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc, i, flags;
	int supply_index = SUPPLY_TYPE_BATTERY;

	bq20z75_device = kzalloc(sizeof(*bq20z75_device), GFP_KERNEL);
	if (!bq20z75_device)
		return -ENOMEM;

	memset(bq20z75_device, 0, sizeof(*bq20z75_device));

	bq20z75_device->client = client;
	flags = bq20z75_device->client->flags;
	bq20z75_device->client->flags &= ~I2C_M_IGNORE_NAK;

	rc = i2c_smbus_read_word_data(bq20z75_device->client,
		bq20z75_data[REG_SERIAL_NUMBER].addr);
	if (rc < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: no battery present(%d)\n", __func__, rc);
		supply_index = SUPPLY_TYPE_AC;
	}

	bq20z75_device->client->flags = flags;
	bq20z75_device->irq = client->irq;
	i2c_set_clientdata(client, bq20z75_device);

	rc = request_threaded_irq(bq20z75_device->irq, NULL,
		ac_present_irq,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"ac_present", bq20z75_device);
	if (rc < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: request_irq failed(%d)\n", __func__, rc);
		goto fail_irq;
	}

	for (i = supply_index; i < ARRAY_SIZE(bq20z75_supply); i++) {
		rc = power_supply_register(&client->dev,
			&bq20z75_supply[i]);
		if (rc) {
			dev_err(&bq20z75_device->client->dev,
				"%s: Failed to register power supply\n",
				 __func__);
			goto fail_power_register;
		}
	}

	setup_timer(&bq20z75_device->battery_poll_timer,
		battery_poll_timer_func, 0);
	mod_timer(&bq20z75_device->battery_poll_timer,
		jiffies + msecs_to_jiffies(BATTERY_POLL_PERIOD));

	dev_info(&bq20z75_device->client->dev, "driver registered\n");
	return 0;

fail_power_register:
	while (i--)
		power_supply_unregister(&bq20z75_supply[i]);
	free_irq(bq20z75_device->irq, bq20z75_device);
fail_irq:
	kfree(bq20z75_device);
	return rc;
}

static int bq20z75_remove(struct i2c_client *client)
{
	struct bq20z75_device_info *bq20z75_device;
	int i;

	bq20z75_device = i2c_get_clientdata(client);
	del_timer_sync(&bq20z75_device->battery_poll_timer);

	for (i = 0; i < ARRAY_SIZE(bq20z75_supply); i++)
		power_supply_unregister(&bq20z75_supply[i]);

	kfree(bq20z75_device);

	return 0;
}

#if defined (CONFIG_PM)
static int bq20z75_suspend(struct i2c_client *client,
	pm_message_t state)
{
	s32 ret;
	struct bq20z75_device_info *bq20z75_device;

	bq20z75_device = i2c_get_clientdata(client);
	del_timer_sync(&bq20z75_device->battery_poll_timer);

	/* write to manufacture access with sleep command */
	ret = i2c_smbus_write_word_data(bq20z75_device->client,
		bq20z75_data[REG_MANUFACTURER_DATA].addr,
		MANUFACTURER_ACCESS_SLEEP);
	if (ret < 0) {
		dev_err(&bq20z75_device->client->dev,
			"%s: i2c write for %d failed\n",
			__func__, MANUFACTURER_ACCESS_SLEEP);
		return -EINVAL;
	}

	return 0;
}

/* any smbus transaction will wake up bq20z75 */
static int bq20z75_resume(struct i2c_client *client)
{
	setup_timer(&bq20z75_device->battery_poll_timer,
		battery_poll_timer_func, 0);
	mod_timer(&bq20z75_device->battery_poll_timer,
		jiffies + msecs_to_jiffies(BATTERY_POLL_PERIOD));
	return 0;
}
#endif

static const struct i2c_device_id bq20z75_id[] = {
	{ "bq20z75-battery", 0 },
	{},
};

static struct i2c_driver bq20z75_battery_driver = {
	.probe		= bq20z75_probe,
	.remove		= bq20z75_remove,
#if defined (CONFIG_PM)
	.suspend	= bq20z75_suspend,
	.resume		= bq20z75_resume,
#endif
	.id_table	= bq20z75_id,
	.driver = {
		.name	= "bq20z75-battery",
	},
};

static int __init bq20z75_battery_init(void)
{
	int ret;

	ret = i2c_add_driver(&bq20z75_battery_driver);
	if (ret)
		dev_err(&bq20z75_device->client->dev,
			"%s: i2c_add_driver failed\n", __func__);

	return ret;
}
module_init(bq20z75_battery_init);

static void __exit bq20z75_battery_exit(void)
{
	i2c_del_driver(&bq20z75_battery_driver);
}
module_exit(bq20z75_battery_exit);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("BQ20z75 battery monitor driver");
MODULE_LICENSE("GPL");
