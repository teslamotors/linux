/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/pm.h>
#include <linux/jiffies.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/power/bq27441_battery.h>

#define BQ27441_DELAY			msecs_to_jiffies(30000)

#define BQ27441_CONTROL_STATUS		0x0000
#define BQ27441_DEVICE_TYPE		0x0001
#define BQ27441_FW_VERSION		0x0002
#define BQ27441_DM_CODE			0x0004
#define BQ27441_PREV_MACWRITE		0x0007
#define BQ27441_CHEM_ID			0x0008
#define BQ27441_BAT_INSERT		0x000C
#define BQ27441_BAT_REMOVE		0x000D
#define BQ27441_SET_HIBERNATE		0x0011
#define BQ27441_CLEAR_HIBERNATE		0x0012
#define BQ27441_SET_CFGUPDATE		0x0013
#define BQ27441_SHUTDOWN_ENABLE		0x001B
#define BQ27441_SHUTDOWN		0x001C
#define BQ27441_SEALED			0x0020
#define BQ27441_PULSE_SOC_INT		0x0023
#define BQ27441_RESET			0x0041
#define BQ27441_SOFT_RESET		0x0042

#define BQ27441_CONTROL_1		0x00
#define BQ27441_CONTROL_2		0x01
#define BQ27441_TEMPERATURE		0x02
#define BQ27441_VOLTAGE			0x04
#define BQ27441_FLAGS			0x06
#define BQ27441_FLAGS_1			0x07
#define BQ27441_FLAGS_FC_DETECT		BIT(1)
#define BQ27441_FLAGS_ITPOR		(1 << 5)
#define BQ27441_NOMINAL_AVAIL_CAPACITY	0x08
#define BQ27441_FULL_AVAIL_CAPACITY	0x0a
#define BQ27441_REMAINING_CAPACITY	0x0c
#define BQ27441_FULL_CHG_CAPACITY	0x0e
#define BQ27441_AVG_CURRENT		0x10
#define BQ27441_STANDBY_CURRENT		0x12
#define BQ27441_MAXLOAD_CURRENT		0x14
#define BQ27441_AVERAGE_POWER		0x18
#define BQ27441_STATE_OF_CHARGE		0x1c
#define BQ27441_INT_TEMPERATURE		0x1e
#define BQ27441_STATE_OF_HEALTH		0x20

#define BQ27441_BLOCK_DATA_CHECKSUM	0x60
#define BQ27441_BLOCK_DATA_CONTROL	0x61
#define BQ27441_DATA_BLOCK_CLASS	0x3E
#define BQ27441_DATA_BLOCK		0x3F

#define BQ27441_QMAX_CELL_1		0x40
#define BQ27441_QMAX_CELL_2		0x41
#define BQ27441_RESERVE_CAP_1		0x43
#define BQ27441_RESERVE_CAP_2		0x44
#define BQ27441_DESIGN_CAPACITY_1	0x4A
#define BQ27441_DESIGN_CAPACITY_2	0x4B
#define BQ27441_DESIGN_ENERGY_1		0x4C
#define BQ27441_DESIGN_ENERGY_2		0x4D
#define BQ27441_TAPER_RATE_1		0x5B
#define BQ27441_TAPER_RATE_2		0x5C
#define BQ27441_TERMINATE_VOLTAGE_1	0x50
#define BQ27441_TERMINATE_VOLTAGE_2	0x51
#define BQ27441_V_CHG_TERM_1		0x41
#define BQ27441_V_CHG_TERM_2		0x42
#define BQ27441_BATTERY_LOW		15
#define BQ27441_BATTERY_FULL		100

#define BQ27441_CC_GAIN			0x44
#define BQ27441_CC_DELTA		0x48

#define BQ27441_MAX_REGS		0x7F

#define BQ27441_DESIGN_CAPACITY_DEFAULT		7800
#define BQ27441_DESIGN_ENERGY_DEFAULT		28080
#define BQ27441_TAPER_RATE_DEFAULT		100
#define BQ27441_TERMINATE_VOLTAGE_DEFAULT	3200
#define BQ27441_VAT_CHG_TERM_DEFAULT		4100
#define BQ27441_CC_GAIN_DEFAULT		0x7F7806C9
#define BQ27441_CC_DELTA_DEFAULT	0x940D197A
#define BQ27441_QMAX_CELL_DEFAULT	16384
#define BQ27441_RESERVE_CAP_DEFAULT	0

#define BQ27441_OPCTEMPS_MASK	0x01
#define BQ27441_OPCONFIG_1	0x40
#define BQ27441_OPCONFIG_2	0x41

#define BQ27441_CHARGE_CURNT_THRESHOLD	2000
#define BQ27441_INPUT_POWER_THRESHOLD		7000
#define BQ27441_BATTERY_SOC_THRESHOLD		90

#define MAX_STRING_PRINT 50

struct bq27441_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct delayed_work		fc_work;
	struct power_supply		battery;
	struct bq27441_platform_data	*pdata;
	struct battery_gauge_dev	*bg_dev;
	struct regmap			*regmap;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;

	int temperature;
	int read_failed;

	int full_capacity;
	int design_energy;
	int taper_rate;
	int terminate_voltage;
	int v_chg_term;
	u32 cc_gain;
	u32 cc_delta;
	u32 qmax_cell;
	u32 reserve_cap;

	int lasttime_soc;
	int lasttime_status;
	int shutdown_complete;
	int charge_complete;
	int print_once;
	bool enable_temp_prop;
	bool full_charge_state;
	bool fcc_fg_initialize;
	bool low_battery_shutdown;
	u32 full_charge_capacity;
	struct mutex mutex;
};

struct bq27441_chip *bq27441_data;

static const struct regmap_config bq27441_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= BQ27441_MAX_REGS,
};

static int bq27441_read_u32(struct i2c_client *client, u8 reg)
{
	int ret;
	u32 val;

	struct bq27441_chip *chip = i2c_get_clientdata(client);

	ret = regmap_raw_read(chip->regmap, reg, (u8 *) &val, sizeof(val));
	if (ret < 0) {
		dev_err(&client->dev, "error reading reg: 0x%x\n", reg);
		return ret;
	}

	return val;
}

static int bq27441_read_word(struct i2c_client *client, u8 reg)
{
	int ret;
	u16 val;

	struct bq27441_chip *chip = i2c_get_clientdata(client);

	ret = regmap_raw_read(chip->regmap, reg, (u8 *) &val, sizeof(val));
	if (ret < 0) {
		dev_err(&client->dev, "error reading reg: 0x%x\n", reg);
		return ret;
	}

	return val;
}

static int bq27441_read_byte(struct i2c_client *client, u8 reg)
{
	int ret;
	u8 val;

	struct bq27441_chip *chip = i2c_get_clientdata(client);

	ret = regmap_raw_read(chip->regmap, reg, (u8 *) &val, sizeof(val));
	if (ret < 0) {
		dev_err(&client->dev, "error reading reg: 0x%x\n", reg);
		return ret;
	}

	return val;
}


static int bq27441_write_byte(struct i2c_client *client, u8 reg, u8 value)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);
	int ret;

	ret = regmap_write(chip->regmap, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register"
					"0x%02x err %d\n", __func__, reg, ret);

	return ret;
}

static int bq27441_get_battery_soc(struct battery_gauge_dev *bg_dev)
{
	struct bq27441_chip *chip = battery_gauge_get_drvdata(bg_dev);
	int val;

	val = bq27441_read_word(chip->client, BQ27441_STATE_OF_CHARGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		val =  battery_gauge_get_adjusted_soc(chip->bg_dev,
				chip->pdata->threshold_soc,
				chip->pdata->maximum_soc, val * 100);

	return val;
}

static int bq27441_update_soc_voltage(struct bq27441_chip *chip)
{
	int val;

	val = bq27441_read_word(chip->client, BQ27441_VOLTAGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->vcell = val;

	val = bq27441_read_word(chip->client, BQ27441_STATE_OF_CHARGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->soc = battery_gauge_get_adjusted_soc(chip->bg_dev,
				chip->pdata->threshold_soc,
				chip->pdata->maximum_soc, val * 100);

	if (chip->low_battery_shutdown && chip->soc == 0)
		chip->soc = 1;

	if (chip->soc == BQ27441_BATTERY_FULL && chip->charge_complete) {
		chip->status = POWER_SUPPLY_STATUS_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < BQ27441_BATTERY_LOW) {
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	} else {
		chip->charge_complete = 0;
		chip->status = chip->lasttime_status;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	return 0;
}

static void bq27441_work(struct work_struct *work)
{
	struct bq27441_chip *chip;

	chip = container_of(work, struct bq27441_chip, work.work);

	mutex_lock(&chip->mutex);
	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return;
	}

	bq27441_update_soc_voltage(chip);

	if (chip->soc != chip->lasttime_soc ||
		chip->status != chip->lasttime_status) {
		chip->lasttime_soc = chip->soc;
		power_supply_changed(&chip->battery);
	}

	mutex_unlock(&chip->mutex);
	battery_gauge_report_battery_soc(chip->bg_dev, chip->soc);
	schedule_delayed_work(&chip->work, BQ27441_DELAY);
}

static int bq27441_battemps_enable(struct bq27441_chip *chip)
{
	struct i2c_client *client = chip->client;
	u8 temp;
	int ret;
	int old_opconfig;
	int old_csum, new_csum;
	unsigned long timeout = jiffies + HZ;

	/* Unseal the fuel gauge for data access */
	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	/* setup fuel gauge state data block block for ram access */
	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 0x40);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x00);
	if (ret < 0)
		goto fail;
	mdelay(1);

	old_opconfig = bq27441_read_word(client, BQ27441_OPCONFIG_1);

	/* if the TEMPS bit is set seal the fuel gauge and return */
	if (BQ27441_OPCTEMPS_MASK & be16_to_cpu(old_opconfig)) {
		dev_info(&chip->client->dev, "FG TEMPS already enabled\n");
		goto seal;
	}

	/* read check sum */
	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	/* place the fuel gauge into config update */
	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x13);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	while (!(bq27441_read_byte(client, BQ27441_FLAGS) & 0x10)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&chip->client->dev,
					"timeout waiting for cfg update\n");
			goto fail;
		}
		msleep(1);
	}

	/* update TEMPS config to fuel gauge */
	ret = bq27441_write_byte(client, BQ27441_OPCONFIG_2,
				((old_opconfig >> 8) | BQ27441_OPCTEMPS_MASK));
	if (ret < 0)
		goto fail;

	temp = (255 - old_csum
		- (old_opconfig & 0xFF)
		- ((old_opconfig >> 8) & 0xFF)) % 256;

	new_csum = 255 - ((temp
				+ (old_opconfig & 0xFF)
				+ ((old_opconfig >> 8) |
				BQ27441_OPCTEMPS_MASK)) % 256);

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM,
				new_csum);
	if (ret < 0)
		goto fail;

seal:
	/* seal the fuel gauge before exit */
	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x20);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	dev_info(&chip->client->dev, "FG OPCONFIG TEMPS enable failed\n");
	return -EIO;
}

static int bq27441_initialize_cc_cal_block(struct bq27441_chip *chip)
{
	struct i2c_client *client = chip->client;
	int old_csum, new_csum;
	u8 temp;
	u32 old_cc_gain, old_cc_delta;
	u8 addr, val;
	int i = 0;
	int ret;

	/* setup cc cal data block block for ram access */

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 105);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x00);
	if (ret < 0)
		goto fail;

	mdelay(1);

	/* read check sum */
	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	/* read all the old values that we want to update */
	old_cc_gain = bq27441_read_u32(client, BQ27441_CC_GAIN);
	old_cc_delta = bq27441_read_u32(client, BQ27441_CC_DELTA);

	dev_info(&chip->client->dev, "cc cal values:\n gain old: %08x new: %08x\n"
				"delta old: %08x new: %08x\n",
				be32_to_cpu(old_cc_gain), chip->cc_gain,
				be32_to_cpu(old_cc_delta), chip->cc_delta);

	if (be32_to_cpu(old_cc_delta) != chip->cc_delta ||
		be32_to_cpu(old_cc_gain) != chip->cc_gain) {
		temp = (255 - old_csum
		- (old_cc_gain & 0xFF)
		- ((old_cc_gain >> 8) & 0xFF)
		- ((old_cc_gain >> 16) & 0xFF)
		- ((old_cc_gain >> 24) & 0xFF)
		- (old_cc_delta & 0xFF)
		- ((old_cc_delta >> 8) & 0xFF)
		- ((old_cc_delta >> 16) & 0xFF)
		- ((old_cc_delta >> 24) & 0xFF)) % 256;

		new_csum = 255 - (((temp
				+ (chip->cc_gain & 0xFF)
				+ ((chip->cc_gain >> 8) & 0xFF)
				+ ((chip->cc_gain >> 16) & 0xFF)
				+ ((chip->cc_gain >> 24) & 0xFF)
				+ (chip->cc_delta & 0xFF)
				+ ((chip->cc_delta >> 8) & 0xFF)
				+ ((chip->cc_delta >> 16) & 0xFF)
				+ ((chip->cc_delta >> 24) & 0xFF)
				)) % 256);

		/* program cc_gain */
		for (i = 0; i < sizeof(u32); i++) {
			val = (chip->cc_gain >> (sizeof(u32) - i - 1)*8) & 0xFF;
			addr = BQ27441_CC_GAIN + i;
			ret = bq27441_write_byte(client, addr, val);
			if (ret < 0)
				goto fail;
		}

		/* program cc_delta */
		for (i = 0; i < sizeof(u32); i++) {
			val = ((chip->cc_delta >> (sizeof(u32) - i - 1)*8)
				& 0xFF);
			addr = BQ27441_CC_DELTA + i;
			ret = bq27441_write_byte(client, addr, val);
			if (ret < 0)
				goto fail;
		}
		ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM,
					new_csum);
		if (ret < 0)
			goto fail;
	}
	return 0;
fail:
	return -EIO;
}

static int bq27441_initialize(struct bq27441_chip *chip)
{
	struct i2c_client *client = chip->client;
	int old_csum;
	u8 temp;
	int new_csum;
	int old_des_cap;
	int old_des_energy;
	int old_taper_rate;
	int old_terminate_voltage;
	int old_v_chg_term;
	u32 old_qmax_cell;
	u32 old_reserve_cap;
	int flags_lsb;

	unsigned long timeout = jiffies + HZ;
	int ret;

	flags_lsb = bq27441_read_byte(client, BQ27441_FLAGS);

	/* Unseal the fuel gauge for data access */

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;
	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x80);
	if (ret < 0)
		goto fail;

	/* setup fuel gauge state data block block for ram access */

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 0x52);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x00);
	if (ret < 0)
		goto fail;


	/* read check sum */
	mdelay(1);

	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	/* read all the old values that we want to update */

	old_qmax_cell = bq27441_read_word(client, BQ27441_QMAX_CELL_1);

	old_reserve_cap = bq27441_read_word(client, BQ27441_RESERVE_CAP_1);

	old_des_cap = bq27441_read_word(client, BQ27441_DESIGN_CAPACITY_1);

	old_des_energy = bq27441_read_word(client, BQ27441_DESIGN_ENERGY_1);

	old_taper_rate = bq27441_read_word(client, BQ27441_TAPER_RATE_1);

	old_terminate_voltage = bq27441_read_word(client,
						BQ27441_TERMINATE_VOLTAGE_1);

	dev_info(&chip->client->dev, "FG values:\n capacity old: %d new: %d\n"
			"design_energy old:%d new:%d\n"
			"taper_rate old:%d new:%d\n"
			"qmax_cell old:%d new:%d\n"
			"reserve_cap old:%d new:%d\n"
			"terminate_voltage old:%d new:%d\n"
			"itpor flag:%d checksum:%d\n",
			be16_to_cpu(old_des_cap), chip->full_capacity,
			be16_to_cpu(old_des_energy), chip->design_energy,
			be16_to_cpu(old_taper_rate), chip->taper_rate,
			be16_to_cpu(old_qmax_cell), chip->qmax_cell,
			be16_to_cpu(old_reserve_cap), chip->reserve_cap,
			be16_to_cpu(old_terminate_voltage),
			chip->terminate_voltage,
			(flags_lsb & BQ27441_FLAGS_ITPOR), old_csum);

	/*
	if the values match with the required ones or if POR bit is not set
	seal the fuel gauge and return
	*/
	if ((chip->full_capacity == be16_to_cpu(old_des_cap))
	  && (chip->design_energy == be16_to_cpu(old_des_energy))
	  && (chip->taper_rate == be16_to_cpu(old_taper_rate))
	  && (chip->terminate_voltage == be16_to_cpu(old_terminate_voltage))
	  && (!(flags_lsb & BQ27441_FLAGS_ITPOR)) && !chip->fcc_fg_initialize) {
		dev_info(&chip->client->dev, "FG is already programmed\n");
		goto seal;
	}

	/* place the fuel gauge into config update */

	dev_info(&chip->client->dev, "Programming Bq27441!\n");

	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x13);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	while (!(bq27441_read_byte(client, BQ27441_FLAGS) & 0x10)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&chip->client->dev,
					"timeout waiting for cfg update\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}


	/* update new config to fuel gauge */
	ret = bq27441_write_byte(client, BQ27441_QMAX_CELL_1,
					(chip->qmax_cell & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_QMAX_CELL_2,
					chip->qmax_cell & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_RESERVE_CAP_1,
					(chip->reserve_cap & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_RESERVE_CAP_2,
					chip->reserve_cap & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_CAPACITY_1,
					(chip->full_capacity & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_CAPACITY_2,
					chip->full_capacity & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_ENERGY_1,
					(chip->design_energy & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DESIGN_ENERGY_2,
					chip->design_energy & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TAPER_RATE_1,
					(chip->taper_rate & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TAPER_RATE_2,
					chip->taper_rate & 0xFF);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TERMINATE_VOLTAGE_1,
				(chip->terminate_voltage & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_TERMINATE_VOLTAGE_2,
					chip->terminate_voltage & 0xFF);
	if (ret < 0)
		goto fail;

	/* calculate the new checksum */
	temp = (255 - old_csum
		- (old_des_cap & 0xFF)
		- ((old_des_cap >> 8) & 0xFF)
		- (old_des_energy & 0xFF)
		- ((old_des_energy >> 8) & 0xFF)
		- (old_taper_rate & 0xFF)
		- ((old_taper_rate >> 8) & 0xFF)
		- (old_qmax_cell & 0xFF)
		- ((old_qmax_cell >> 8) & 0xFF)
		- (old_reserve_cap & 0xFF)
		- ((old_reserve_cap >> 8) & 0xFF)
		- (old_terminate_voltage & 0xFF)
		- ((old_terminate_voltage >> 8) & 0xFF)) % 256;

	new_csum = 255 - (((temp + (chip->full_capacity & 0xFF)
				+ ((chip->full_capacity & 0xFF00) >> 8)
				+ (chip->design_energy & 0xFF)
				+ ((chip->design_energy & 0xFF00) >> 8)
				+ (chip->taper_rate & 0xFF)
				+ ((chip->taper_rate & 0xFF00) >> 8)
				+ (chip->qmax_cell & 0xFF)
				+ ((chip->qmax_cell & 0xFF00) >> 8)
				+ (chip->reserve_cap & 0xFF)
				+ ((chip->reserve_cap & 0xFF00) >> 8)
				+ (chip->terminate_voltage & 0xFF)
				+ ((chip->terminate_voltage & 0xFF00) >> 8)
				)) % 256);

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM, new_csum);
	if (ret < 0)
		goto fail;

	/* setup fuel gauge state data block page 1 for ram access */
	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CONTROL, 0x00);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK_CLASS, 0x52);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_DATA_BLOCK, 0x01);
	if (ret < 0)
		goto fail;

	mdelay(1);

	old_csum = bq27441_read_byte(client, BQ27441_BLOCK_DATA_CHECKSUM);

	old_v_chg_term = bq27441_read_word(client, BQ27441_V_CHG_TERM_1);

	ret = bq27441_write_byte(client, BQ27441_V_CHG_TERM_1,
					(chip->v_chg_term & 0xFF00) >> 8);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_V_CHG_TERM_2,
					chip->v_chg_term & 0xFF);
	if (ret < 0)
		goto fail;

	temp = (255 - old_csum
		- (old_v_chg_term & 0xFF)
		- ((old_v_chg_term >> 8) & 0xFF)) % 256;

	new_csum = 255 - ((temp
				+ (chip->v_chg_term & 0xFF)
				+ ((chip->v_chg_term & 0xFF00) >> 8)
				) % 256);

	ret = bq27441_write_byte(client, BQ27441_BLOCK_DATA_CHECKSUM, new_csum);
	if (ret < 0)
		goto fail;

	/* program the cc_cal block */
	if (bq27441_initialize_cc_cal_block(chip) < 0)
		goto fail;

	/* exit config update mode */
	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x42);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	while (!(bq27441_read_byte(client, BQ27441_FLAGS) & 0x10)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&chip->client->dev,
					"timeout waiting for cfg update\n");
			return -ETIMEDOUT;
		}
		msleep(1);
	}

seal:
	/* seal the fuel gauge before exit */
	ret = bq27441_write_byte(client, BQ27441_CONTROL_1, 0x20);
	if (ret < 0)
		goto fail;

	ret = bq27441_write_byte(client, BQ27441_CONTROL_2, 0x00);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	return -EIO;
}

static int bq27441_get_temperature(void)
{
	int val;
	int retries = 2;
	int i;

	if (bq27441_data->shutdown_complete)
		return bq27441_data->temperature;

	for (i = 0; i < retries; i++) {
		val = bq27441_read_word(bq27441_data->client,
					BQ27441_TEMPERATURE);
		if (val < 0)
			continue;
	}

	if (val < 0) {
		bq27441_data->read_failed++;
		dev_err(&bq27441_data->client->dev,
					"%s: err %d\n", __func__, val);
		if (bq27441_data->read_failed > 50)
			return val;
		return bq27441_data->temperature;
	}
	bq27441_data->read_failed = 0;
	bq27441_data->temperature = val / 100;

	return val / 100;
}

static enum power_supply_property bq27441_battery_props[] = {
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CHARGER_STANDARD,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int bq27441_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bq27441_chip *chip = container_of(psy,
				struct bq27441_chip, battery);
	int temperature;
	int power_mw = 0, input_curnt_ma = 0;
	int ret = 0;

	mutex_lock(&chip->mutex);
	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 1000 * chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;

		if (chip->print_once && (chip->soc > 15 || chip->soc < 5))
			chip->print_once = 0;

		if (chip->soc == 15 && chip->print_once != 15) {
			chip->print_once = 15;
			dev_warn(&chip->client->dev,
			"System Running low on battery - 15 percent\n");
		}
		if (chip->soc == 10 && chip->print_once != 10) {
			chip->print_once = 10;
			dev_warn(&chip->client->dev,
			"System Running low on battery - 10 percent\n");
		}
		if (chip->soc == 5 && chip->print_once != 5) {
			chip->print_once = 5;
			dev_warn(&chip->client->dev,
			"System Running low on battery - 5 percent\n");
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_CHARGER_STANDARD:
		val->strval = "unknown";
		input_curnt_ma = battery_gauge_get_input_current_limit(
							chip->bg_dev);

		battery_gauge_get_input_power(chip->bg_dev, &power_mw);
		power_mw = abs(power_mw);

		if (input_curnt_ma >= BQ27441_CHARGE_CURNT_THRESHOLD &&
				power_mw <= BQ27441_INPUT_POWER_THRESHOLD &&
				chip->soc < BQ27441_BATTERY_SOC_THRESHOLD)
			val->strval = "sub-standard";
		else if (input_curnt_ma == 0 && power_mw == 0)
			val->strval = "no-charger";
		else
			val->strval = "standard";
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_INFO:
		if (chip->enable_temp_prop) {
			temperature = bq27441_get_temperature();
			val->intval = temperature;
		} else if (chip->pdata->tz_name) {
			ret = battery_gauge_get_battery_temperature(
					chip->bg_dev, &temperature);
			if (ret < 0) {
				dev_err(&chip->client->dev,
						"temp invalid %d\n", ret);
				chip->read_failed++;
				if (chip->read_failed > 50)
					break;
				temperature = chip->temperature;
				ret = 0;
			} else {
				chip->read_failed = 0;
				chip->temperature = temperature;
			}
			val->intval = temperature * 10;
		} else {
			ret = -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = 0;
		input_curnt_ma = battery_gauge_get_input_current_limit(
					chip->bg_dev);
		if (input_curnt_ma > 0)
			val->intval = input_curnt_ma * 1000;
		else
			ret = input_curnt_ma;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		ret = battery_gauge_get_battery_current(chip->bg_dev,
					&input_curnt_ma);
		if (!ret)
			val->intval = 1000 * input_curnt_ma;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&chip->mutex);
	return ret;
}

static int bq27441_update_battery_status(struct battery_gauge_dev *bg_dev,
		enum battery_charger_status status)
{
	struct bq27441_chip *chip = battery_gauge_get_drvdata(bg_dev);
	int val;

	mutex_lock(&chip->mutex);
	if (chip->shutdown_complete) {
		mutex_unlock(&chip->mutex);
		return 0;
	}

	val = bq27441_read_word(chip->client, BQ27441_STATE_OF_CHARGE);
	if (val < 0)
		dev_err(&chip->client->dev, "%s: err %d\n", __func__, val);
	else
		chip->soc = battery_gauge_get_adjusted_soc(chip->bg_dev,
				chip->pdata->threshold_soc,
				chip->pdata->maximum_soc, val * 100);

	if (chip->low_battery_shutdown && chip->soc == 0)
		chip->soc = 1;

	if (status == BATTERY_CHARGING) {
		chip->charge_complete = 0;
		chip->status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (status == BATTERY_CHARGING_DONE) {
		if (chip->soc == BQ27441_BATTERY_FULL) {
			chip->charge_complete = 1;
			chip->status = POWER_SUPPLY_STATUS_FULL;
			chip->capacity_level =
					POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		}
		goto done;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
		chip->charge_complete = 0;
	}
	chip->lasttime_status = chip->status;

done:
	mutex_unlock(&chip->mutex);
	power_supply_changed(&chip->battery);
	dev_dbg(&chip->client->dev,
		"%s() Battery status: %d and SoC: %d%% UI status: %d\n",
		__func__, status, chip->soc, chip->status);
	return 0;
}

static struct battery_gauge_ops bq27441_bg_ops = {
	.update_battery_status = bq27441_update_battery_status,
	.get_battery_temp = bq27441_get_temperature,
	.get_battery_soc = bq27441_get_battery_soc,
};

static struct battery_gauge_info bq27441_bgi = {
	.cell_id = 0,
	.bg_ops = &bq27441_bg_ops,
	.current_channel_name = "battery-current",
};

static void bq27441_fc_work(struct work_struct *work)
{
	struct bq27441_chip *chip;
	int val = 0;
	chip = container_of(to_delayed_work(work),
				struct bq27441_chip, fc_work);

	if (chip->full_charge_capacity) {
		val = bq27441_read_word(chip->client, BQ27441_FULL_CHG_CAPACITY);
		if (val < 0) {
			dev_err(&chip->client->dev, "FCC read failed %d\n",
					val);
		} else if (val < chip->full_charge_capacity) {
			dev_info(&chip->client->dev,
					"Full charge capacity %d\n", val);
			chip->fcc_fg_initialize = true;
			val = bq27441_initialize(chip);
			if (val < 0)
				dev_err(&chip->client->dev,
					"chip init failed:%d\n", val);
			schedule_delayed_work(&chip->fc_work, BQ27441_DELAY);
			return;
		} else
			dev_info(&chip->client->dev,
					"Full Charge Capacity %d\n", val);
		chip->fcc_fg_initialize = false;
	}

	dev_info(&chip->client->dev, "Full charge status: %d\n",
					chip->full_charge_state);
	battery_gauge_fc_state(chip->bg_dev, chip->full_charge_state);
}

static ssize_t bq27441_show_lowbat_shutdown_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27441_chip *bq27441 = i2c_get_clientdata(client);

	mutex_lock(&bq27441->mutex);
	if (bq27441->shutdown_complete) {
		mutex_unlock(&bq27441->mutex);
		return -EIO;
	}
	mutex_unlock(&bq27441->mutex);

	if (bq27441->low_battery_shutdown)
		return snprintf(buf, MAX_STRING_PRINT, "disabled\n");
	else
		return snprintf(buf, MAX_STRING_PRINT, "enabled\n");
}

static ssize_t bq27441_set_lowbat_shutdown_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27441_chip *bq27441 = i2c_get_clientdata(client);
	bool enabled;

	if ((*buf == 'E') || (*buf == 'e'))
		enabled = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		enabled = false;
	else
		return -EINVAL;

	mutex_lock(&bq27441->mutex);
	if (bq27441->shutdown_complete) {
		mutex_unlock(&bq27441->mutex);
		return -EIO;
	}
	if (enabled)
		bq27441->low_battery_shutdown = false;
	else
		bq27441->low_battery_shutdown = true;
	mutex_unlock(&bq27441->mutex);

	return count;
}

static DEVICE_ATTR(low_battery_shutdown, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq27441_show_lowbat_shutdown_state, bq27441_set_lowbat_shutdown_state);

static struct attribute *bq27441_attributes[] = {
	&dev_attr_low_battery_shutdown.attr,
	NULL
};

static const struct attribute_group bq27441_attr_group = {
	.attrs = bq27441_attributes,
};

static irqreturn_t bq27441_irq(int id, void *dev)
{
	struct bq27441_chip *chip = dev;
	struct i2c_client *client = chip->client;

	bq27441_update_soc_voltage(chip);
	power_supply_changed(&chip->battery);
	dev_info(&client->dev, "%s() Battery Voltage %dmV and SoC %d%%\n",
				__func__, chip->vcell, chip->soc);

	if (chip->soc == BQ27441_BATTERY_FULL && !chip->full_charge_state) {
		chip->full_charge_state = 1;
		schedule_delayed_work(&chip->fc_work, 0);
	} else if (chip->soc < BQ27441_BATTERY_FULL &&
					chip->full_charge_state) {
		chip->full_charge_state = 0;
		schedule_delayed_work(&chip->fc_work, 0);
	}

	return IRQ_HANDLED;
}

static void of_bq27441_parse_platform_data(struct i2c_client *client,
				struct bq27441_platform_data *pdata)
{
	u32 tmp;
	char const *pstr;
	bool dt_param_not_found = 0;
	struct device_node *np = client->dev.of_node;

	if (!of_property_read_u32(np, "ti,design-capacity", &tmp))
		pdata->full_capacity = (unsigned long)tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read design-capacity\n");
	}

	if (!of_property_read_u32(np, "ti,design-energy", &tmp))
		pdata->full_energy = (unsigned long)tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read design-energy\n");
	}

	if (!of_property_read_u32(np, "ti,taper-rate", &tmp))
		pdata->taper_rate = (unsigned long)tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read taper-rate\n");
	}

	if (!of_property_read_u32(np, "ti,terminate-voltage", &tmp))
		pdata->terminate_voltage = (unsigned long)tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read terminate-voltage\n");
	}

	if (!of_property_read_u32(np, "ti,v-at-chg-term", &tmp))
		pdata->v_at_chg_term = (unsigned long)tmp;

	if (!of_property_read_u32(np, "ti,cc-gain", &tmp))
		pdata->cc_gain = tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read cc-gain\n");
	}

	if (!of_property_read_u32(np, "ti,cc-delta", &tmp))
		pdata->cc_delta = tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read cc-delta\n");
	}

	if (!of_property_read_u32(np, "ti,qmax-cell", &tmp))
		pdata->qmax_cell = tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read qmax-cell\n");
	}

	if (!of_property_read_u32(np, "ti,reserve-cap-mah", &tmp))
		pdata->reserve_cap = tmp;
	else {
		dt_param_not_found = 1;
		dev_warn(&client->dev, "fail to read reserve-cap-mah\n");
	}

	if (!of_property_read_string(np, "ti,tz-name", &pstr))
		pdata->tz_name = pstr;
	else
		dev_warn(&client->dev, "Failed to read tz-name\n");

	if (!of_property_read_u32(np, "ti,kernel-threshold-soc", &tmp))
		pdata->threshold_soc = tmp;

	if (!of_property_read_u32(np, "ti,kernel-maximum-soc", &tmp))
		pdata->maximum_soc = tmp;
	else
		pdata->maximum_soc = 100;

	if (!of_property_read_u32(np, "ti,full-charge-capacity", &tmp))
		pdata->full_charge_capacity = tmp;

	pdata->enable_temp_prop = of_property_read_bool(np,
					"ti,enable-temp-prop");

	pdata->support_battery_current = of_property_read_bool(np,
						"io-channel-names");

	if (dt_param_not_found)
		dev_warn(&client->dev,
				"All the FG properties not provided in DT\n");
}

static int bq27441_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct bq27441_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;

	if (client->dev.of_node) {
		chip->pdata = devm_kzalloc(&client->dev,
					sizeof(*chip->pdata), GFP_KERNEL);
		if (!chip->pdata)
			return -ENOMEM;
		of_bq27441_parse_platform_data(client, chip->pdata);
	} else {
		chip->pdata = client->dev.platform_data;
	}

	if (!chip->pdata)
		return -ENODATA;

	chip->full_capacity = 1200;
	chip->print_once = 0;
	chip->full_charge_state = 0;
	chip->fcc_fg_initialize = 0;
	chip->low_battery_shutdown = 0;

	chip->full_capacity = chip->pdata->full_capacity ?:
				BQ27441_DESIGN_CAPACITY_DEFAULT;
	chip->design_energy = chip->pdata->full_energy ?:
				BQ27441_DESIGN_ENERGY_DEFAULT;
	chip->taper_rate = chip->pdata->taper_rate ?:
				BQ27441_TAPER_RATE_DEFAULT;
	chip->terminate_voltage = chip->pdata->terminate_voltage ?:
				BQ27441_TERMINATE_VOLTAGE_DEFAULT;
	chip->v_chg_term = chip->pdata->v_at_chg_term ?:
				BQ27441_VAT_CHG_TERM_DEFAULT;
	chip->cc_gain = chip->pdata->cc_gain ?: BQ27441_CC_GAIN_DEFAULT;
	chip->cc_delta = chip->pdata->cc_delta ?: BQ27441_CC_DELTA_DEFAULT;
	chip->qmax_cell = chip->pdata->qmax_cell ?: BQ27441_QMAX_CELL_DEFAULT;
	chip->reserve_cap = chip->pdata->reserve_cap ?:
				BQ27441_RESERVE_CAP_DEFAULT;
	chip->enable_temp_prop = chip->pdata->enable_temp_prop;
	chip->full_charge_capacity = chip->pdata->full_charge_capacity;

	dev_info(&client->dev, "Battery capacity is %d\n", chip->full_capacity);

	bq27441_data = chip;
	mutex_init(&chip->mutex);
	chip->shutdown_complete = 0;
	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= bq27441_get_property;
	chip->battery.properties	= bq27441_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(bq27441_battery_props);
	chip->status			= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->lasttime_status		= POWER_SUPPLY_STATUS_DISCHARGING;
	chip->charge_complete		= 0;

	if (chip->pdata->tz_name)
		bq27441_battery_props[0] = POWER_SUPPLY_PROP_TEMP_INFO;

	/* Remove current property if it is not supported */
	if (!chip->pdata->support_battery_current)
		chip->battery.num_properties--;

	if (!chip->enable_temp_prop && chip->pdata->tz_name)
		chip->temperature = 23;

	chip->regmap = devm_regmap_init_i2c(client, &bq27441_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		goto error;
	}

	/* Dummy read to check if the slave is present */
	ret = bq27441_read_word(chip->client, BQ27441_VOLTAGE);
	if (ret < 0) {
		dev_err(&chip->client->dev, "Exiting driver as xfer failed\n");
		goto error;
	}

	bq27441_bgi.tz_name = chip->pdata->tz_name;

	chip->bg_dev = battery_gauge_register(&client->dev, &bq27441_bgi,
				chip);
	if (IS_ERR(chip->bg_dev)) {
		ret = PTR_ERR(chip->bg_dev);
		dev_err(&client->dev, "battery gauge register failed: %d\n",
			ret);
		goto bg_err;
	}

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}

	ret = bq27441_initialize(chip);
	if (ret < 0)
		dev_err(&client->dev, "chip init failed - %d\n", ret);

	if (chip->enable_temp_prop)
		bq27441_battemps_enable(chip);

	bq27441_update_soc_voltage(chip);

	INIT_DEFERRABLE_WORK(&chip->work, bq27441_work);
	schedule_delayed_work(&chip->work, 0);

	INIT_DELAYED_WORK(&chip->fc_work, bq27441_fc_work);

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
			NULL, bq27441_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(&client->dev), chip);
		if (ret < 0) {
			dev_err(&client->dev,
				"%s: request IRQ %d fail, err = %d\n",
				__func__, client->irq, ret);
			client->irq = 0;
			goto irq_reg_error;
		}
	}
	device_set_wakeup_capable(&client->dev, 1);
	device_wakeup_enable(&client->dev);

	dev_info(&client->dev, "Battery Voltage %dmV and SoC %d%%\n",
			chip->vcell, chip->soc);

	if (chip->soc == BQ27441_BATTERY_FULL && !chip->full_charge_state) {
		chip->full_charge_state = 1;
		schedule_delayed_work(&chip->fc_work, 0);
	}

	ret = sysfs_create_group(&client->dev.kobj, &bq27441_attr_group);
	if (ret < 0) {
		dev_err(&client->dev, "sysfs create failed %d\n", ret);
		return ret;
	}

	return 0;
irq_reg_error:
	cancel_delayed_work_sync(&chip->work);
	cancel_delayed_work_sync(&chip->fc_work);
bg_err:
	power_supply_unregister(&chip->battery);
error:
	mutex_destroy(&chip->mutex);

	return ret;
}

static int bq27441_remove(struct i2c_client *client)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);

	battery_gauge_unregister(chip->bg_dev);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work_sync(&chip->work);
	mutex_destroy(&chip->mutex);

	return 0;
}

static void bq27441_shutdown(struct i2c_client *client)
{
	struct bq27441_chip *chip = i2c_get_clientdata(client);

	if (chip->client->irq)
		disable_irq(chip->client->irq);

	mutex_lock(&chip->mutex);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->mutex);

	cancel_delayed_work_sync(&chip->work);
	dev_info(&chip->client->dev, "At shutdown Voltage %dmV and SoC %d%%\n",
			chip->vcell, chip->soc);
}

#ifdef CONFIG_PM_SLEEP
static int bq27441_suspend(struct device *dev)
{
	struct bq27441_chip *chip = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&chip->work);

	if (device_may_wakeup(&chip->client->dev) && chip->client->irq)
		enable_irq_wake(chip->client->irq);

	dev_info(&chip->client->dev, "At suspend Voltage %dmV and SoC %d%%\n",
			chip->vcell, chip->soc);
	return 0;
}

static int bq27441_resume(struct device *dev)
{
	struct bq27441_chip *chip = dev_get_drvdata(dev);

	if (device_may_wakeup(&chip->client->dev) && chip->client->irq)
		disable_irq_wake(chip->client->irq);

	mutex_lock(&chip->mutex);
	bq27441_update_soc_voltage(chip);
	power_supply_changed(&chip->battery);
	mutex_unlock(&chip->mutex);

	dev_info(&chip->client->dev, "At resume Voltage %dmV and SoC %d%%\n",
			chip->vcell, chip->soc);
	schedule_delayed_work(&chip->work, BQ27441_DELAY);
	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(bq27441_pm_ops, bq27441_suspend, bq27441_resume);

#ifdef CONFIG_OF
static const struct of_device_id bq27441_dt_match[] = {
	{ .compatible = "ti,bq27441" },
	{ },
};
MODULE_DEVICE_TABLE(of, bq27441_dt_match);
#endif

static const struct i2c_device_id bq27441_id[] = {
	{ "bq27441", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq27441_id);

static struct i2c_driver bq27441_i2c_driver = {
	.driver	= {
		.name	= "bq27441",
		.of_match_table = of_match_ptr(bq27441_dt_match),
		.pm = &bq27441_pm_ops,
	},
	.probe		= bq27441_probe,
	.remove		= bq27441_remove,
	.id_table	= bq27441_id,
	.shutdown	= bq27441_shutdown,
};

static int __init bq27441_init(void)
{
	return i2c_add_driver(&bq27441_i2c_driver);
}
fs_initcall_sync(bq27441_init);

static void __exit bq27441_exit(void)
{
	i2c_del_driver(&bq27441_i2c_driver);
}
module_exit(bq27441_exit);

MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_DESCRIPTION("BQ27441 Fuel Gauge");
MODULE_LICENSE("GPL");
