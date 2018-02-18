/*
 * bq2419x-charger.c -- BQ24190/BQ24192/BQ24192i/BQ24193 Charger driver
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power/bq2419x-charger.h>
#include <linux/regmap.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/alarmtimer.h>
#include <linux/power/battery-charger-gauge-comm.h>
#include <linux/workqueue.h>
#include <linux/extcon.h>

#define MAX_STR_PRINT 50

#define bq_chg_err(bq, fmt, ...)			\
		dev_err(bq->dev, "Charging Fault: " fmt, ##__VA_ARGS__)

#define BQ2419X_INPUT_VINDPM_OFFSET	3880
#define BQ2419X_CHARGE_ICHG_OFFSET	512
#define BQ2419X_PRE_CHG_IPRECHG_OFFSET	128
#define BQ2419X_PRE_CHG_TERM_OFFSET	128
#define BQ2419X_CHARGE_VOLTAGE_OFFSET	3504
#define BQ2419x_OTG_ENABLE_TIME		msecs_to_jiffies(30000)
#define BQ2419X_PC_USB_LP0_THRESHOLD	95
#define BQ2419x_TEMP_H_CHG_DISABLE	50
#define BQ2419x_TEMP_L_CHG_DISABLE	0

/* input current limit */
static const unsigned int iinlim[] = {
	100, 150, 500, 900, 1200, 1500, 2000, 3000,
};

static const struct regmap_config bq2419x_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= BQ2419X_MAX_REGS,
};

struct bq2419x_reg_info {
	u8 mask;
	u8 val;
};

const char *bq2419x_extcon_cable[] = {
	[0] = "USB",
	NULL,
};


struct bq2419x_chip {
	struct device			*dev;
	struct regmap			*regmap;
	int				irq;
	int				gpio_otg_iusb;
	int				auto_rechg_power_on_time;
	bool				emulate_input_disconnected;

	struct mutex			mutex;
	struct mutex			otg_mutex;
	int				in_current_limit;

	struct regulator_dev		*chg_rdev;
	struct regulator_desc		chg_reg_desc;
	struct regulator_init_data	*chg_ridata;
	struct device_node		*chg_np;

	struct regulator_dev		*vbus_rdev;
	struct regulator_desc		vbus_reg_desc;
	struct regulator_init_data	*vbus_ridata;
	struct device_node		*vbus_np;

	struct battery_charger_dev	*bc_dev;
	int				chg_status;

	struct delayed_work		otg_reset_work;
	struct delayed_work		wdt_restart_wq;
	int				is_otg_connected;
	int				battery_presense;
	bool				cable_connected;
	int				last_charging_current;
	bool				disable_suspend_during_charging;
	bool				wake_lock_released;
	int				last_temp;
	bool				shutdown_complete;
	bool				charging_disabled_on_suspend;
	bool				thermal_chg_disable;
	struct bq2419x_reg_info		input_src;
	struct bq2419x_reg_info		chg_current_control;
	struct bq2419x_reg_info		prechg_term_control;
	struct bq2419x_reg_info		ir_comp_therm;
	struct bq2419x_reg_info		chg_voltage_control;
	struct bq2419x_reg_info		watchdog_timer;
	struct bq2419x_vbus_platform_data *vbus_pdata;
	struct bq2419x_charger_platform_data *charger_pdata;

	struct delayed_work		thermal_init_work;
	struct thermal_zone_device	*die_tz_device;
	int				thermal_init_retry;
	struct extcon_dev		edev;
	const char			*ext_name;
	int				last_input_voltage;
	int				wdt_refresh_timeout;
	int				wdt_time_sec;
};

static int current_to_reg(const unsigned int *tbl,
			size_t size, unsigned int val)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (val < tbl[i])
			break;
	return i > 0 ? i - 1 : -EINVAL;
}

static int bq2419x_charger_enable(struct bq2419x_chip *bq2419x)
{
	int ret;

	if (bq2419x->battery_presense) {
		dev_info(bq2419x->dev, "Charging enabled\n");
		/* set default Charge regulation voltage */
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
			bq2419x->chg_voltage_control.mask,
			bq2419x->chg_voltage_control.val);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"VOLT_CTRL_REG update failed %d\n", ret);
			return ret;
		}
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_ENABLE_CHARGE);
	} else {
		dev_info(bq2419x->dev, "Charging disabled\n");
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				 BQ2419X_ENABLE_CHARGE_MASK,
				 BQ2419X_DISABLE_CHARGE);
	}
	if (ret < 0)
		dev_err(bq2419x->dev, "register update failed, err %d\n", ret);
	return ret;
}

static int bq2419x_reset_wdt(struct bq2419x_chip *bq2419x, const char *from)
{
	int ret = 0;
	unsigned int reg_val;
	int timeout;

	if (!bq2419x->wdt_refresh_timeout)
		return -EINVAL;

	dev_info(bq2419x->dev, "%s() from %s()\n", __func__, from);

	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &reg_val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG read failed: %d\n", ret);
		goto scrub;
	}

	reg_val |= BIT(6);

	/* Write two times to make sure reset WDT */
	ret = regmap_write(bq2419x->regmap, BQ2419X_PWR_ON_REG, reg_val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG write failed: %d\n", ret);
		goto scrub;
	}
	ret = regmap_write(bq2419x->regmap, BQ2419X_PWR_ON_REG, reg_val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG write failed: %d\n", ret);
		goto scrub;
	}

scrub:
	timeout = bq2419x->wdt_refresh_timeout;
	schedule_delayed_work(&bq2419x->wdt_restart_wq,
						msecs_to_jiffies(timeout * 1000));
	return ret;
}

static int bq2419x_watchdog_disable(struct bq2419x_chip *bq2419x)
{
	int ret;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419X_WD_MASK, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL_REG read failed: %d\n",
			ret);
	else
		dev_info(bq2419x->dev, "Charging WATCHDOG timer disabled\n");

	return ret;
}

static int bq2419x_watchdog_enable(struct bq2419x_chip *bq2419x, const char *from)
{
	int ret = 0;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			bq2419x->watchdog_timer.mask, bq2419x->watchdog_timer.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "TIME_CTRL_REG read failed: %d\n", ret);
		return ret;
	}

	ret = bq2419x_reset_wdt(bq2419x, from);
	if (ret < 0) {
		dev_err(bq2419x->dev, "bq2419x_reset_wdt failed: %d\n", ret);
		return ret;
	}

	dev_info(bq2419x->dev, "Charger Watchdog timer enabled in OTG mode\n");
	return ret;
}

static void bq2419x_wdt_restart_wq(struct work_struct *work)
{
	struct bq2419x_chip *bq2419x;
	int ret;

	bq2419x = container_of(work, struct bq2419x_chip, wdt_restart_wq.work);
	ret = bq2419x_reset_wdt(bq2419x, "Thread");
	if (ret < 0)
		dev_err(bq2419x->dev, "bq2419x_reset_wdt failed: %d\n", ret);

}

static int bq2419x_vbus_enable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS enabled, charging disabled\n");

	mutex_lock(&bq2419x->otg_mutex);
	bq2419x->is_otg_connected = true;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, BQ2419X_ENABLE_VBUS);
	if (ret < 0)
		dev_err(bq2419x->dev, "PWR_ON_REG update failed %d", ret);
	if (bq2419x->wdt_refresh_timeout)
		bq2419x_watchdog_enable(bq2419x, "OTG-ENABLE");
	mutex_unlock(&bq2419x->otg_mutex);

	return ret;
}

static int bq2419x_vbus_disable(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;

	dev_info(bq2419x->dev, "VBUS disabled, charging enabled\n");

	mutex_lock(&bq2419x->otg_mutex);
	if (bq2419x->wdt_refresh_timeout) {
		cancel_delayed_work_sync(&bq2419x->wdt_restart_wq);
		bq2419x_watchdog_disable(bq2419x);
	}
	bq2419x->is_otg_connected = false;
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0)
		dev_err(bq2419x->dev, "Charger enable failed %d", ret);
	mutex_unlock(&bq2419x->otg_mutex);

	return ret;
}

static int bq2419x_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int ret;
	unsigned int data;

	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &data);
	if (ret < 0) {
		dev_err(bq2419x->dev, "PWR_ON_REG read failed %d", ret);
		return ret;
	}
	return (data & BQ2419X_ENABLE_CHARGE_MASK) == BQ2419X_ENABLE_VBUS;
}

static struct regulator_ops bq2419x_vbus_ops = {
	.enable		= bq2419x_vbus_enable,
	.disable	= bq2419x_vbus_disable,
	.is_enabled	= bq2419x_vbus_is_enabled,
};

static int bq2419x_val_to_reg(int val, int offset, int div, int nbits,
	bool roundup)
{
	int max_val = offset + (BIT(nbits) - 1) * div;

	if (val <= offset)
		return 0;

	if (val >= max_val)
		return BIT(nbits) - 1;

	if (roundup)
		return DIV_ROUND_UP(val - offset, div);
	else
		return (val - offset) / div;
}

static int bq2419x_process_charger_plat_data(struct bq2419x_chip *bq2419x,
		struct bq2419x_charger_platform_data *chg_pdata)
{
	int voltage_input;
	int fast_charge_current;
	int pre_charge_current;
	int termination_current;
	int ir_compensation_resistor;
	int ir_compensation_voltage;
	int thermal_regulation_threshold;
	int charge_voltage_limit;
	int vindpm, ichg, iprechg, iterm, bat_comp, vclamp, treg, vreg,wtreg;

	if (chg_pdata) {
		voltage_input = chg_pdata->input_voltage_limit_mV ?: 4200;
		fast_charge_current =
			chg_pdata->fast_charge_current_limit_mA ?: 4544;
		pre_charge_current =
			chg_pdata->pre_charge_current_limit_mA ?: 256;
		termination_current =
			chg_pdata->termination_current_limit_mA ?: 128;
		ir_compensation_resistor =
			chg_pdata->ir_compensation_resister_ohm ?: 70;
		ir_compensation_voltage =
			chg_pdata->ir_compensation_voltage_mV ?: 112;
		thermal_regulation_threshold =
			chg_pdata->thermal_regulation_threshold_degC ?: 100;
		charge_voltage_limit =
			chg_pdata->charge_voltage_limit_mV ?: 4208;
	} else {
		voltage_input = 4200;
		fast_charge_current = 4544;
		pre_charge_current = 256;
		termination_current = 128;
		ir_compensation_resistor = 70;
		ir_compensation_voltage = 112;
		thermal_regulation_threshold = 100;
		charge_voltage_limit = 4208;
	}

	vindpm = bq2419x_val_to_reg(voltage_input,
			BQ2419X_INPUT_VINDPM_OFFSET, 80, 4, 0);
	bq2419x->input_src.mask = BQ2419X_INPUT_VINDPM_MASK;
	bq2419x->input_src.val = vindpm << 3;
	bq2419x->input_src.mask |= BQ2419X_INPUT_IINLIM_MASK;
	bq2419x->input_src.val |= 0x2;

	ichg = bq2419x_val_to_reg(fast_charge_current,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	bq2419x->chg_current_control.mask = BQ2419X_CHRG_CTRL_ICHG_MASK;
	bq2419x->chg_current_control.val = ichg << 2;

	iprechg = bq2419x_val_to_reg(pre_charge_current,
			BQ2419X_PRE_CHG_IPRECHG_OFFSET, 128, 4, 0);
	bq2419x->prechg_term_control.mask = BQ2419X_CHRG_TERM_PRECHG_MASK;
	bq2419x->prechg_term_control.val = iprechg << 4;
	iterm =  bq2419x_val_to_reg(termination_current,
			BQ2419X_PRE_CHG_TERM_OFFSET, 128, 4, 0);
	bq2419x->prechg_term_control.mask |= BQ2419X_CHRG_TERM_TERM_MASK;
	bq2419x->prechg_term_control.val |= iterm;

	bat_comp = ir_compensation_resistor / 10;
	bq2419x->ir_comp_therm.mask = BQ2419X_THERM_BAT_COMP_MASK;
	bq2419x->ir_comp_therm.val = bat_comp << 5;
	vclamp = ir_compensation_voltage / 16;
	bq2419x->ir_comp_therm.mask |= BQ2419X_THERM_VCLAMP_MASK;
	bq2419x->ir_comp_therm.val |= vclamp << 2;
	bq2419x->ir_comp_therm.mask |= BQ2419X_THERM_TREG_MASK;
	if (thermal_regulation_threshold <= 60)
		treg = 0;
	else if (thermal_regulation_threshold <= 80)
		treg = 1;
	else if (thermal_regulation_threshold <= 100)
		treg = 2;
	else
		treg = 3;
	bq2419x->ir_comp_therm.val |= treg;

	vreg = bq2419x_val_to_reg(charge_voltage_limit,
			BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1);
	bq2419x->chg_voltage_control.mask = BQ2419X_CHG_VOLT_LIMIT_MASK;
	bq2419x->chg_voltage_control.val = vreg << 2;

	if (!bq2419x->wdt_time_sec)
		goto done;

	bq2419x->watchdog_timer.mask = BQ2419X_WD_MASK;
	if (bq2419x->wdt_time_sec <= 60) {
		wtreg = BQ2419X_WD_40ms;
		bq2419x->wdt_refresh_timeout = 25;
	} else if (bq2419x->wdt_time_sec <= 120) {
		wtreg = BQ2419X_WD_80ms;
		bq2419x->wdt_refresh_timeout = 50;
	} else {
		wtreg = BQ2419X_WD_160ms;
		bq2419x->wdt_refresh_timeout = 125;
	}

	bq2419x->watchdog_timer.val |= wtreg;

done:
	return 0;
}

static int bq2419x_safetytimer_disable(struct bq2419x_chip *bq2419x)
{
	int ret;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
					BQ2419X_EN_SFT_TIMER_MASK, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL update failed: %d\n", ret);
	else
		dev_info(bq2419x->dev, "Charging SAFETY timer disabled\n");

	return ret;
}

static int bq2419x_charger_init(struct bq2419x_chip *bq2419x)
{
	int ret;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
			bq2419x->chg_current_control.mask,
			bq2419x->chg_current_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL_REG write failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_TERM_REG,
			bq2419x->prechg_term_control.mask,
			bq2419x->prechg_term_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_TERM_REG write failed %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
			bq2419x->input_src.mask, bq2419x->input_src.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "INPUT_SRC_REG write failed %d\n", ret);
	bq2419x->last_input_voltage = (bq2419x->input_src.val >> 3) & 0xF;

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_THERM_REG,
		    bq2419x->ir_comp_therm.mask, bq2419x->ir_comp_therm.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "THERM_REG write failed: %d\n", ret);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
			bq2419x->chg_voltage_control.mask,
			bq2419x->chg_voltage_control.val);
	if (ret < 0)
		dev_err(bq2419x->dev, "VOLT_CTRL update failed: %d\n", ret);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419X_TIME_JEITA_ISET | BQ2419x_EN_CHARGE_TERM, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "TIME_CTRL update failed: %d\n", ret);

	/* disable safety timer */
	bq2419x_safetytimer_disable(bq2419x);

	/* disable sWDT timer */
	if (!bq2419x->is_otg_connected)
		ret = bq2419x_watchdog_disable(bq2419x);

	return ret;
}

static void bq2419x_otg_reset_work_handler(struct work_struct *work)
{
	int ret;
	struct bq2419x_chip *bq2419x = container_of(to_delayed_work(work),
			struct bq2419x_chip, otg_reset_work);

	if (!mutex_is_locked(&bq2419x->otg_mutex)) {
		mutex_lock(&bq2419x->otg_mutex);
		if (bq2419x->is_otg_connected) {
			ret = regmap_update_bits(bq2419x->regmap,
					BQ2419X_PWR_ON_REG,
					BQ2419X_ENABLE_CHARGE_MASK,
					BQ2419X_ENABLE_VBUS);
			if (ret < 0)
				dev_err(bq2419x->dev,
				"PWR_ON_REG update failed %d", ret);
		}
		if (bq2419x->wdt_refresh_timeout)
			bq2419x_watchdog_enable(bq2419x, "OTG-RESET-WORK");
		mutex_unlock(&bq2419x->otg_mutex);
	}
}

static int bq2419x_disable_otg_mode(struct bq2419x_chip *bq2419x)
{
	int ret = 0;

	mutex_lock(&bq2419x->otg_mutex);
	if (bq2419x->is_otg_connected) {
		if (bq2419x->wdt_refresh_timeout) {
			cancel_delayed_work_sync(&bq2419x->wdt_restart_wq);
			bq2419x_watchdog_disable(bq2419x);
		}
		ret = bq2419x_charger_enable(bq2419x);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"Charger enable failed %d", ret);
			mutex_unlock(&bq2419x->otg_mutex);
			return ret;
		}
		schedule_delayed_work(&bq2419x->otg_reset_work,
					BQ2419x_OTG_ENABLE_TIME);
	}
	mutex_unlock(&bq2419x->otg_mutex);

	return ret;
}

static int bq2419x_charger_input_voltage_configure(
		struct battery_charger_dev *bc_dev, int battery_soc)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	struct bq2419x_charger_platform_data *chg_pdata;
	u32 input_voltage_limit = 0;
	int ret;
	int i;
	int vreg;

	chg_pdata = bq2419x->charger_pdata;
	if (!bq2419x->cable_connected || !chg_pdata->n_soc_profile)
		return 0;

	for (i = 0; i < chg_pdata->n_soc_profile; ++i) {
		if (battery_soc < chg_pdata->soc_range[i]) {
			if (chg_pdata->input_voltage_soc_limit)
				input_voltage_limit =
					chg_pdata->input_voltage_soc_limit[i];
			break;
		}
	}

	if (!input_voltage_limit)
		return 0;

	/*Configure input voltage limit */
	vreg = bq2419x_val_to_reg(input_voltage_limit,
			BQ2419X_INPUT_VINDPM_OFFSET, 80, 4, 0);
	if (bq2419x->last_input_voltage == vreg)
		return 0;

	dev_info(bq2419x->dev, "Changing VINDPM to soc:voltage:vreg %d:%d:%d\n",
			battery_soc, input_voltage_limit, vreg);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_INPUT_VINDPM_MASK,
				(vreg << 3));
	if (ret < 0) {
		dev_err(bq2419x->dev, "INPUT_VOLTAGE update failed %d\n", ret);
		return ret;
	}
	bq2419x->last_input_voltage = vreg;

	return 0;
}

static int bq2419x_configure_charging_current(struct bq2419x_chip *bq2419x,
	int in_current_limit)
{
	int val = 0;
	int ret = 0;
	int floor = 0;
	int battery_soc = 0;

	/* Clear EN_HIZ */
	if (!bq2419x->emulate_input_disconnected) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
			BQ2419X_EN_HIZ, 0);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed %d\n", ret);
			return ret;
		}
	}

	/* Configure input voltage limit*/
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
			bq2419x->input_src.mask, bq2419x->input_src.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "INPUT_SRC_REG update failed %d\n", ret);
		return ret;
	}
	bq2419x->last_input_voltage =  (bq2419x->input_src.val >> 3) & 0xF;

	/* Configure input current limit in steps */
	val = current_to_reg(iinlim, ARRAY_SIZE(iinlim), in_current_limit);
	floor = current_to_reg(iinlim, ARRAY_SIZE(iinlim), 500);
	if (val < 0 || floor < 0)
		return 0;

	for (; floor <= val; floor++) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419x_CONFIG_MASK, floor);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"INPUT_SRC_REG update failed: %d\n", ret);
		udelay(BQ2419x_CHARGING_CURRENT_STEP_DELAY_US);
	}
	bq2419x->in_current_limit = in_current_limit;

	if (bq2419x->charger_pdata->n_soc_profile) {
		battery_soc = battery_gauge_get_battery_soc(bq2419x->bc_dev);
		if (battery_soc > 0)
			bq2419x_charger_input_voltage_configure(
				bq2419x->bc_dev, battery_soc);
	}

	return ret;
}

static int bq2419x_set_charging_current(struct regulator_dev *rdev,
			int min_uA, int max_uA)
{
	struct bq2419x_chip *bq2419x = rdev_get_drvdata(rdev);
	int in_current_limit;
	int old_current_limit;
	int ret = 0;
	int val;

	dev_info(bq2419x->dev, "Setting charging current %d\n", max_uA/1000);
	bq2419x->chg_status = BATTERY_DISCHARGING;

	if (!bq2419x->is_otg_connected) {
		ret = bq2419x_charger_enable(bq2419x);
		if (ret < 0) {
			dev_err(bq2419x->dev, "Charger enable failed %d", ret);
			return ret;
		}
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0)
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed: %d\n", ret);

	if (max_uA == 0 && val != 0)
		return ret;
	if (!max_uA)
		bq2419x->wake_lock_released = false;

	old_current_limit = bq2419x->in_current_limit;
	bq2419x->last_charging_current = max_uA;
	if ((val & BQ2419x_VBUS_STAT) == BQ2419x_VBUS_UNKNOWN) {
		in_current_limit = 500;
		bq2419x->cable_connected = 0;
		bq2419x->chg_status = BATTERY_DISCHARGING;
		battery_charger_thermal_stop_monitoring(
				bq2419x->bc_dev);
		/* Clear JEITA_VSET bit if cable disconnected */
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_MISC_OPER_REG,
					BQ2419X_JEITA_VSET_MASK, 0);
		if (ret < 0)
			dev_err(bq2419x->dev, "JEITA_VSET update failed: %d\n",
					ret);
	} else if ((val & BQ2419x_CHRG_STATE_MASK) ==
				BQ2419x_CHRG_STATE_CHARGE_DONE) {
		dev_dbg(bq2419x->dev, "Charging completed\n");
		bq2419x->chg_status = BATTERY_CHARGING_DONE;
		bq2419x->cable_connected = 1;
		in_current_limit = max_uA/1000;
		battery_charger_thermal_stop_monitoring(
				bq2419x->bc_dev);
	} else {
		in_current_limit = max_uA/1000;
		bq2419x->cable_connected = 1;
		bq2419x->chg_status = BATTERY_CHARGING;
		battery_charger_thermal_start_monitoring(
				bq2419x->bc_dev);
		/* Set JEITA_VSET bit if charger cable connected */
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_MISC_OPER_REG,
			BQ2419X_JEITA_VSET_MASK, BQ2419X_JEITA_VSET_42V);
		if (ret < 0)
			dev_err(bq2419x->dev, "JEITA_VSET update failed: %d\n",
				ret);
	}
	if (bq2419x->wake_lock_released)
		in_current_limit = 500;
	ret = bq2419x_configure_charging_current(bq2419x, in_current_limit);
	if (ret < 0)
		goto error;

	battery_charging_status_update(bq2419x->bc_dev, bq2419x->chg_status);
	if (bq2419x->disable_suspend_during_charging) {
		if (bq2419x->cable_connected && in_current_limit > 500
			&& (bq2419x->chg_status != BATTERY_CHARGING_DONE))
			battery_charger_acquire_wake_lock(bq2419x->bc_dev);
		else if (!bq2419x->cable_connected && old_current_limit > 500)
			battery_charger_release_wake_lock(bq2419x->bc_dev);
	}

	return 0;
error:
	dev_err(bq2419x->dev, "Charger enable failed, err = %d\n", ret);
	return ret;
}

static struct regulator_ops bq2419x_tegra_regulator_ops = {
	.set_current_limit = bq2419x_set_charging_current,
};

static int bq2419x_set_charging_current_suspend(struct bq2419x_chip *bq2419x,
			int in_current_limit)
{
	int ret;
	int val;

	dev_info(bq2419x->dev, "Setting charging current %d mA\n",
			in_current_limit);

	if (!bq2419x->is_otg_connected) {
		ret = bq2419x_charger_enable(bq2419x);
		if (ret < 0) {
			dev_err(bq2419x->dev, "Charger enable failed %d", ret);
			return ret;
		}
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0)
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed: %d\n", ret);

	if (!bq2419x->cable_connected) {
		ret = bq2419x_configure_charging_current(bq2419x,
				in_current_limit);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int bq2419x_reconfigure_charger_param(struct bq2419x_chip *bq2419x,
		const char *from)
{
	int ret;

	dev_info(bq2419x->dev, "Reconfiguring charging param from %s\n", from);

	ret = bq2419x_charger_init(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger init failed: %d\n", ret);
		return ret;
	}

	ret = bq2419x_configure_charging_current(bq2419x,
			bq2419x->in_current_limit);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Current config failed: %d\n", ret);
		return ret;
	}

	mutex_lock(&bq2419x->otg_mutex);
	if (bq2419x->is_otg_connected) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_ENABLE_VBUS);
		if (ret < 0)
			dev_err(bq2419x->dev, "PWR_ON_REG update failed %d", ret);

		ret = bq2419x_watchdog_enable(bq2419x, from);
		if (ret < 0)
			dev_err(bq2419x->dev, "BQWDT enable failed %d\n", ret);
	} else {
		ret = bq2419x_watchdog_disable(bq2419x);
		if (ret < 0)
			dev_err(bq2419x->dev, "BQWDT enable failed %d\n", ret);
	}
	mutex_unlock(&bq2419x->otg_mutex);

	return ret;
}

static int bq2419x_thermal_read_temp(void *data, long *temp)
{
	struct bq2419x_chip *bq2419x = data;
	unsigned int reg_06;
	unsigned int reg_08;
	int treg;
	int ret;
	long curr_temp;

	ret = regmap_read(bq2419x->regmap, BQ2419X_THERM_REG, &reg_06);
	if (ret < 0) {
		dev_err(bq2419x->dev, "BQ2419X_THERM_REG rd failed %d\n", ret);
		return ret;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &reg_08);
	if (ret < 0) {
		dev_err(bq2419x->dev, "BQ2419X_SYS_STAT_REG rd failed %d\n", ret);
		return ret;
	}
	treg = reg_06 & BQ2419x_TREG;
	curr_temp = (treg * 20 + 60) * 1000;

	if (reg_08 & BQ2419x_THERM_STAT)
		curr_temp += 5000;
	else
		curr_temp -= 5000;

	*temp = curr_temp;
	return 0;
}

#ifdef CONFIG_THERMAL
static int bq2419x_thermal_check_therm_regulation(struct bq2419x_chip *bq2419x)
{
	if (bq2419x->die_tz_device)
		thermal_zone_device_update(bq2419x->die_tz_device);
	return 0;
}

static void bq2419x_thermal_init_work(struct work_struct *work)
{
	int ret;
	struct bq2419x_chip *bq2419x = container_of(to_delayed_work(work),
			struct bq2419x_chip, thermal_init_work);
	struct thermal_zone_device *die_tz_device;

	die_tz_device = thermal_zone_of_sensor_register(bq2419x->dev,
					0, bq2419x, bq2419x_thermal_read_temp,
					NULL);
	if (IS_ERR(die_tz_device)) {
		ret = PTR_ERR(die_tz_device);
		if ((ret == -EPROBE_DEFER) && (bq2419x->thermal_init_retry)) {
			schedule_delayed_work(&bq2419x->thermal_init_work,
					msecs_to_jiffies(1000));
			bq2419x->thermal_init_retry--;
		}
		return;
	}
	bq2419x->die_tz_device = die_tz_device;
	dev_info(bq2419x->dev, "Thermal Zone registration success\n");
	return;
}
#endif

static int bq2419x_fault_clear_sts(struct bq2419x_chip *bq2419x,
	unsigned int *reg09_val)
{
	int ret;
	unsigned int reg09_1, reg09_2;

	ret = regmap_read(bq2419x->regmap, BQ2419X_FAULT_REG, &reg09_1);
	if (ret < 0) {
		dev_err(bq2419x->dev, "FAULT_REG read failed: %d\n", ret);
		return ret;
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_FAULT_REG, &reg09_2);
	if (ret < 0)
		dev_err(bq2419x->dev, "FAULT_REG read failed: %d\n", ret);

	if (reg09_val) {
		unsigned int reg09 = 0;

		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_WATCHDOG_FAULT)
			reg09 |= BQ2419x_FAULT_WATCHDOG_FAULT;
		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_BOOST_FAULT)
			reg09 |= BQ2419x_FAULT_BOOST_FAULT;
		if ((reg09_1 | reg09_2) & BQ2419x_FAULT_BAT_FAULT)
			reg09 |= BQ2419x_FAULT_BAT_FAULT;
		if (((reg09_1 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_SAFTY) ||
			((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_SAFTY))
			reg09 |= BQ2419x_FAULT_CHRG_SAFTY;
		else if (((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_INPUT))
			reg09 |= BQ2419x_FAULT_CHRG_INPUT;
		else if (((reg09_1 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_THERMAL) ||
			((reg09_2 & BQ2419x_FAULT_CHRG_FAULT_MASK) ==
				BQ2419x_FAULT_CHRG_THERMAL))
			reg09 |= BQ2419x_FAULT_CHRG_THERMAL;

		reg09 |= reg09_2 &BQ2419x_FAULT_NTC_FAULT;
		*reg09_val = reg09;
	}
	return ret;
}

static int bq2419x_extcon_cable_update(struct bq2419x_chip *bq2419x,
							unsigned int val)
{
	if ((val & BQ2419x_VBUS_PG_STAT) == BQ2419x_PG_VBUS_USB) {
		extcon_set_cable_state(&bq2419x->edev,
						bq2419x_extcon_cable[0], true);
		dev_dbg(bq2419x->dev, "USB is connected\n");
	} else if ((val & BQ2419x_VBUS_PG_STAT) == BQ2419x_VBUS_UNKNOWN) {
		extcon_set_cable_state(&bq2419x->edev,
						bq2419x_extcon_cable[0], false);
		dev_info(bq2419x->dev, "USB is disconnected\n");
	}

	return 0;
}

static irqreturn_t bq2419x_irq(int irq, void *data)
{
	struct bq2419x_chip *bq2419x = data;
	int ret;
	unsigned int val;
	int check_chg_state = 0;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete)
		goto out;

	ret = bq2419x_fault_clear_sts(bq2419x, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		goto out;
	}

	dev_dbg(bq2419x->dev, "%s() Irq %d status 0x%02x\n",
		__func__, irq, val);

	if (val & BQ2419x_FAULT_BOOST_FAULT) {
		bq_chg_err(bq2419x, "VBUS Overloaded\n");
		ret = bq2419x_disable_otg_mode(bq2419x);
		if (ret < 0) {
			bq_chg_err(bq2419x, "otg mode disable failed\n");
			goto out;
		}
		regulator_notifier_call_chain(bq2419x->vbus_rdev,
				REGULATOR_EVENT_OVER_CURRENT, NULL);
	}

	if (!bq2419x->battery_presense)
		goto sys_stat_read;

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT) {
		bq_chg_err(bq2419x, "Charger WatchDog Timer expired\n");
		ret = bq2419x_reconfigure_charger_param(bq2419x,
									"WDT-EXP-ISR");
		if (ret < 0)
			dev_err(bq2419x->dev, "BQ reconfig failed %d\n", ret);
	}

	switch (val & BQ2419x_FAULT_CHRG_FAULT_MASK) {
	case BQ2419x_FAULT_CHRG_INPUT:
		bq_chg_err(bq2419x,
			"Input Fault (VBUS OVP or VBAT<VBUS<3.8V)\n");
		break;
	case BQ2419x_FAULT_CHRG_THERMAL:
		bq_chg_err(bq2419x, "Thermal shutdown\n");
		check_chg_state = 1;
		ret = bq2419x_disable_otg_mode(bq2419x);
		if (ret < 0) {
			bq_chg_err(bq2419x, "otg mode disable failed\n");
			goto out;
		}
		if (bq2419x->is_otg_connected)
			regulator_notifier_call_chain(bq2419x->vbus_rdev,
				REGULATOR_EVENT_OVER_TEMP, NULL);
		break;
	default:
		break;
	}

	if (val & BQ2419x_FAULT_NTC_FAULT) {
		bq_chg_err(bq2419x, "NTC fault %d\n",
				val & BQ2419x_FAULT_NTC_FAULT);
		check_chg_state = 1;
	}

sys_stat_read:
	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed %d\n", ret);
		goto out;
	}

	bq2419x_extcon_cable_update(bq2419x, val);

	if (!bq2419x->battery_presense)
		goto out;

#ifdef CONFIG_THERMAL
	if (val & BQ2419x_THERM_STAT) {
		dev_info(bq2419x->dev, "Charger in thermal regulation\n");
		bq2419x_thermal_check_therm_regulation(bq2419x);
	}
#endif

	if ((val & BQ2419x_CHRG_STATE_MASK) == BQ2419x_CHRG_STATE_CHARGE_DONE) {
		dev_dbg(bq2419x->dev, "Charging completed\n");
		bq2419x->chg_status = BATTERY_CHARGING_DONE;
		battery_charging_status_update(bq2419x->bc_dev,
					bq2419x->chg_status);
		if (bq2419x->disable_suspend_during_charging)
			battery_charger_release_wake_lock(bq2419x->bc_dev);
		battery_charger_thermal_stop_monitoring(
					bq2419x->bc_dev);
	}

	if ((val & BQ2419x_VSYS_STAT_MASK) == BQ2419x_VSYS_STAT_BATT_LOW)
		dev_info(bq2419x->dev,
			"In VSYSMIN regulation, battery is too low\n");

	/* Update Charging status based on STAT register */
	if (check_chg_state &&
	  ((val & BQ2419x_CHRG_STATE_MASK) == BQ2419x_CHRG_STATE_NOTCHARGING)) {
		bq2419x->chg_status = BATTERY_DISCHARGING;
		battery_charging_status_update(bq2419x->bc_dev,
				bq2419x->chg_status);
		if (bq2419x->disable_suspend_during_charging)
			battery_charger_release_wake_lock(bq2419x->bc_dev);
	}

out:
	mutex_unlock(&bq2419x->mutex);
	return IRQ_HANDLED;
}

static int bq2419x_init_charger_regulator(struct bq2419x_chip *bq2419x,
		struct bq2419x_platform_data *pdata)
{
	int ret = 0;
	struct regulator_config rconfig = { };

	if (!pdata->bcharger_pdata) {
		dev_err(bq2419x->dev, "No charger platform data\n");
		return 0;
	}

	bq2419x->chg_reg_desc.name  = "bq2419x-charger";
	bq2419x->chg_reg_desc.ops   = &bq2419x_tegra_regulator_ops;
	bq2419x->chg_reg_desc.type  = REGULATOR_CURRENT;
	bq2419x->chg_reg_desc.owner = THIS_MODULE;

	bq2419x->chg_ridata = pdata->bcharger_pdata->ridata;
	bq2419x->chg_ridata->driver_data = bq2419x;
	bq2419x->chg_ridata->constraints.ignore_current_constraint_init = true;
	bq2419x->chg_ridata->constraints.valid_modes_mask =
						REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_STANDBY;
	bq2419x->chg_ridata->constraints.valid_ops_mask =
						REGULATOR_CHANGE_MODE |
						REGULATOR_CHANGE_STATUS |
						REGULATOR_CHANGE_CURRENT;
	rconfig.dev = bq2419x->dev;
	rconfig.of_node =  bq2419x->chg_np;
	rconfig.init_data = bq2419x->chg_ridata;
	rconfig.driver_data = bq2419x;
	bq2419x->chg_rdev = devm_regulator_register(bq2419x->dev,
				&bq2419x->chg_reg_desc, &rconfig);
	if (IS_ERR(bq2419x->chg_rdev)) {
		ret = PTR_ERR(bq2419x->chg_rdev);
		dev_err(bq2419x->dev,
			"vbus-charger regulator register failed %d\n", ret);
	}
	return ret;
}

static int bq2419x_init_vbus_regulator(struct bq2419x_chip *bq2419x,
		struct bq2419x_platform_data *pdata)
{
	int ret = 0;
	struct regulator_config rconfig = { };

	if (!pdata->vbus_pdata) {
		dev_err(bq2419x->dev, "No vbus platform data\n");
		return 0;
	}

	bq2419x->gpio_otg_iusb = pdata->vbus_pdata->gpio_otg_iusb;
	bq2419x->vbus_reg_desc.name = "bq2419x-vbus";
	bq2419x->vbus_reg_desc.ops = &bq2419x_vbus_ops;
	bq2419x->vbus_reg_desc.type = REGULATOR_VOLTAGE;
	bq2419x->vbus_reg_desc.owner = THIS_MODULE;
	bq2419x->vbus_reg_desc.enable_time = 220000;

	bq2419x->vbus_ridata = pdata->vbus_pdata->ridata;
	bq2419x->vbus_ridata->constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL |
					REGULATOR_MODE_STANDBY;
	bq2419x->vbus_ridata->constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE |
					REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE;

	if (gpio_is_valid(bq2419x->gpio_otg_iusb)) {
		ret = gpio_request_one(bq2419x->gpio_otg_iusb,
				GPIOF_OUT_INIT_HIGH, dev_name(bq2419x->dev));
		if (ret < 0) {
			dev_err(bq2419x->dev, "gpio request failed  %d\n", ret);
			return ret;
		}
	}

	/* Register the regulators */
	rconfig.dev = bq2419x->dev;
	rconfig.of_node =  bq2419x->vbus_np;
	rconfig.init_data = bq2419x->vbus_ridata;
	rconfig.driver_data = bq2419x;
	bq2419x->vbus_rdev = devm_regulator_register(bq2419x->dev,
				&bq2419x->vbus_reg_desc, &rconfig);
	if (IS_ERR(bq2419x->vbus_rdev)) {
		ret = PTR_ERR(bq2419x->vbus_rdev);
		dev_err(bq2419x->dev,
			"VBUS regulator register failed %d\n", ret);
		goto scrub;
	}

	/* Disable the VBUS regulator and enable charging */
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charging enable failed %d", ret);
		goto scrub;
	}
	return ret;

scrub:
	if (gpio_is_valid(bq2419x->gpio_otg_iusb))
		gpio_free(bq2419x->gpio_otg_iusb);
	return ret;
}

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *debugfs_root;
static ssize_t bq2419x_show_suspend_state(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *client = file->private_data;
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	char buf[64] = { 0, };
	ssize_t ret = 0;

	if (bq2419x->wake_lock_released ||
			(bq2419x->chg_status == BATTERY_CHARGING_DONE) ||
			(bq2419x->in_current_limit <= 500))
		ret = snprintf(buf, sizeof(buf), "Wake lock disabled\n");
	else if (!bq2419x->wake_lock_released ||
			(bq2419x->in_current_limit > 500))
		ret = snprintf(buf, sizeof(buf), "Wake lock enabled\n");

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t bq2419x_enable_suspend_on_charging(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *client = file->private_data;
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	char buf[64] = { 0, };
	ssize_t buf_size;
	bool enabled;
	int ret;

	if (!bq2419x->cable_connected ||
			(bq2419x->chg_status == BATTERY_CHARGING_DONE))
		return -EINVAL;

	if (!bq2419x->disable_suspend_during_charging)
		return -EINVAL;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if ((*buf == 'E') || (*buf == 'e'))
		enabled = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		enabled = false;
	else
		return -EINVAL;

	if (enabled && !bq2419x->wake_lock_released) {
		if (bq2419x->in_current_limit == 500)
			return -EINVAL;
		ret = bq2419x_configure_charging_current(bq2419x, 500);
		if (ret  < 0) {
			dev_err(bq2419x->dev,
				"Charging Current config faild: %d\n", ret);
			return ret;
		}
		battery_charger_release_wake_lock(bq2419x->bc_dev);
		bq2419x->wake_lock_released = true;
	} else if (!enabled && bq2419x->wake_lock_released) {
		bq2419x->wake_lock_released = false;
		ret = bq2419x_configure_charging_current(bq2419x,
				bq2419x->last_charging_current/1000);
		if (ret  < 0) {
			dev_err(bq2419x->dev,
				"Charging Current config faild: %d\n", ret);
			return ret;
		}
		battery_charger_acquire_wake_lock(bq2419x->bc_dev);
	}
	return count;
}

static const struct file_operations bq2419x_debug_fops = {
	.open		= simple_open,
	.write		= bq2419x_enable_suspend_on_charging,
	.read		= bq2419x_show_suspend_state,
};

static int bq2419x_debugfs_init(struct i2c_client *client)
{
	debugfs_root = debugfs_create_dir("battery_charger", NULL);
	if (!debugfs_root)
		pr_warn("bq2419x: Failed to create debugfs directory\n");

	debugfs_create_file("allow_suspend_on_charging", S_IRUGO | S_IWUSR,
			debugfs_root, (void *)client, &bq2419x_debug_fops);
	return 0;
}
#else
static int bq2419x_debugfs_init(struct i2c_client *client)
{
	return 0;
}
#endif

static int bq2419x_show_chip_version(struct bq2419x_chip *bq2419x)
{
	int ret;
	unsigned int val;

	ret = regmap_read(bq2419x->regmap, BQ2419X_REVISION_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "REVISION_REG read failed: %d\n", ret);
		return ret;
	}

	if ((val & BQ2419X_IC_VER_MASK) == BQ24190_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ24190 detected\n");
	else if ((val & BQ2419X_IC_VER_MASK) == BQ24192_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ2419X/3 detected\n");
	else if ((val & BQ2419X_IC_VER_MASK) == BQ24192i_IC_VER)
		dev_info(bq2419x->dev, "chip type BQ2419Xi detected\n");
	return 0;
}


static ssize_t bq2419x_show_input_charging_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_INPUT_SRC_REG, &reg_val);
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(bq2419x->dev, "INPUT_SRC read failed: %d\n", ret);
		return ret;
	}
	ret = iinlim[BQ2419x_CONFIG_MASK & reg_val];
	return snprintf(buf, MAX_STR_PRINT, "%d mA\n", ret);
}

static ssize_t bq2419x_set_input_charging_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int ret;
	int in_current_limit;
	char *p = (char *)buf;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	in_current_limit = memparse(p, &p);
	ret = bq2419x_configure_charging_current(bq2419x, in_current_limit);
	mutex_unlock(&bq2419x->mutex);
	if (ret  < 0) {
		dev_err(dev, "Current %d mA configuration faild: %d\n",
			in_current_limit, ret);
		return ret;
	}
	return count;
}

static ssize_t bq2419x_show_charging_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_PWR_ON_REG, &reg_val);
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(dev, "BQ2419X_PWR_ON register read failed: %d\n", ret);
		return ret;
	}

	if ((reg_val & BQ2419X_ENABLE_CHARGE_MASK) == BQ2419X_ENABLE_CHARGE)
		return snprintf(buf, MAX_STR_PRINT, "enabled\n");
	else
		return snprintf(buf, MAX_STR_PRINT, "disabled\n");
}

static ssize_t bq2419x_set_charging_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	bool enabled;
	int ret;

	if ((*buf == 'E') || (*buf == 'e'))
		enabled = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		enabled = false;
	else
		return -EINVAL;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	if (enabled)
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			BQ2419X_ENABLE_CHARGE_MASK, BQ2419X_ENABLE_CHARGE);
	else
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
			 BQ2419X_ENABLE_CHARGE_MASK, BQ2419X_DISABLE_CHARGE);
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(bq2419x->dev, "register update failed, %d\n", ret);
		return ret;
	}

	return count;
}

static ssize_t bq2419x_show_input_cable_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	unsigned int reg_val;
	int ret;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_INPUT_SRC_REG, &reg_val);
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(dev, "BQ2419X_PWR_ON register read failed: %d\n", ret);
		return ret;
	}

	if ((reg_val & BQ2419X_EN_HIZ) == BQ2419X_EN_HIZ)
		return snprintf(buf, MAX_STR_PRINT, "Disconnected\n");
	else
		return snprintf(buf, MAX_STR_PRINT, "Connected\n");
}

static ssize_t bq2419x_set_input_cable_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	bool connect;
	int ret;

	if ((*buf == 'C') || (*buf == 'c'))
		connect = true;
	else if ((*buf == 'D') || (*buf == 'd'))
		connect = false;
	else
		return -EINVAL;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	if (connect) {
		bq2419x->emulate_input_disconnected = false;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_EN_HIZ, 0);
	} else {
		bq2419x->emulate_input_disconnected = true;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_INPUT_SRC_REG,
				BQ2419X_EN_HIZ, BQ2419X_EN_HIZ);
	}
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(bq2419x->dev, "register update failed, %d\n", ret);
		return ret;
	}
	if (connect)
		dev_info(bq2419x->dev,
			"Emulation of charger cable disconnect disabled\n");
	else
		dev_info(bq2419x->dev,
			"Emulated as charger cable Disconnected\n");
	return count;
}

static ssize_t bq2419x_show_output_charging_current(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int ret;
	unsigned int data;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG, &data);
	mutex_unlock(&bq2419x->mutex);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL read failed %d", ret);
		return ret;
	}
	data >>= 2;
	data = data * 64 + BQ2419X_CHARGE_ICHG_OFFSET;
	return snprintf(buf, MAX_STR_PRINT, "%u mA\n", data);
}

static ssize_t bq2419x_set_output_charging_current(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	int curr_val, ret;
	int ichg;

	if (kstrtouint(buf, 0, &curr_val)) {
		dev_err(dev, "\nfile: %s, line=%d return %s()",
					__FILE__, __LINE__, __func__);
		return -EINVAL;
	}

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->shutdown_complete) {
		mutex_unlock(&bq2419x->mutex);
		return -EIO;
	}
	ichg = bq2419x_val_to_reg(curr_val, BQ2419X_CHARGE_ICHG_OFFSET,
						64, 6, 0);
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
				BQ2419X_CHRG_CTRL_ICHG_MASK, ichg << 2);
	mutex_unlock(&bq2419x->mutex);

	return count;
}

static ssize_t bq2419x_show_output_charging_current_values(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i <= 63; i++)
		ret += snprintf(buf + strlen(buf), MAX_STR_PRINT,
				"%d mA\n", i * 64 + BQ2419X_CHARGE_ICHG_OFFSET);

	return ret;
}

static DEVICE_ATTR(output_charging_current, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_output_charging_current,
		bq2419x_set_output_charging_current);

static DEVICE_ATTR(output_current_allowed_values, S_IRUGO,
		bq2419x_show_output_charging_current_values, NULL);

static DEVICE_ATTR(input_charging_current_mA, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_charging_current,
		bq2419x_set_input_charging_current);

static DEVICE_ATTR(charging_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_charging_state, bq2419x_set_charging_state);

static DEVICE_ATTR(input_cable_state, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		bq2419x_show_input_cable_state, bq2419x_set_input_cable_state);

static struct attribute *bq2419x_attributes[] = {
	&dev_attr_output_charging_current.attr,
	&dev_attr_output_current_allowed_values.attr,
	&dev_attr_input_charging_current_mA.attr,
	&dev_attr_charging_state.attr,
	&dev_attr_input_cable_state.attr,
	NULL
};

static const struct attribute_group bq2419x_attr_group = {
	.attrs = bq2419x_attributes,
};

static int bq2419x_get_input_current_limit(struct battery_charger_dev *bc_dev)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);

	if (!bq2419x->cable_connected)
		return 0;

	return bq2419x->in_current_limit;
}

static int bq2419x_charger_termination_configure(
		struct battery_charger_dev *bc_dev, int full_charge_state)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	int ret;

	dev_err(bq2419x->dev, "Configure charge termination: %d\n",
				full_charge_state);

	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_TIME_CTRL_REG,
			BQ2419x_EN_CHARGE_TERM, (full_charge_state ?
			BQ2419x_EN_CHARGE_TERM : 0));
	if (ret < 0) {
		dev_err(bq2419x->dev, "TIME_CTRL update failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int bq2419x_charger_get_status(struct battery_charger_dev *bc_dev)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);

	return bq2419x->chg_status;
}

static int bq2419x_charger_thermal_configure(
		struct battery_charger_dev *bc_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage)
{
	struct bq2419x_chip *bq2419x = battery_charger_get_drvdata(bc_dev);
	struct bq2419x_charger_platform_data *chg_pdata;
	int fast_charge_current = 0;
	u32 charge_voltage_limit = 0;
	int ichg;
	int ret;
	int i;
	int curr_ichg, vreg;

	if (bq2419x->shutdown_complete)
		return 0;

	chg_pdata = bq2419x->charger_pdata;
	if (!bq2419x->cable_connected || !chg_pdata->n_temp_profile)
		return 0;

	if (bq2419x->last_temp == temp)
		return 0;

	bq2419x->last_temp = temp;

	dev_info(bq2419x->dev, "Battery temp %d\n", temp);

	if ((temp > BQ2419x_TEMP_H_CHG_DISABLE ||
				temp < BQ2419x_TEMP_L_CHG_DISABLE) &&
				!bq2419x->thermal_chg_disable) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				 BQ2419X_ENABLE_CHARGE_MASK,
				 BQ2419X_DISABLE_CHARGE);
		if (ret < 0) {
			dev_err(bq2419x->dev, "REG update failed, %d\n", ret);
			return ret;
		}
		dev_info(bq2419x->dev, "Thermal: Charging disabled\n");
		bq2419x->thermal_chg_disable = true;
	}

	if ((temp <= BQ2419x_TEMP_H_CHG_DISABLE &&
				temp >= BQ2419x_TEMP_L_CHG_DISABLE) &&
				bq2419x->thermal_chg_disable) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_ENABLE_CHARGE);
		if (ret < 0) {
			dev_err(bq2419x->dev, "REG update failed, %d\n", ret);
			return ret;
		}
		dev_info(bq2419x->dev, "Thermal: Charging enabled\n");
		bq2419x->thermal_chg_disable = false;
	}

	for (i = 0; i < chg_pdata->n_temp_profile; ++i) {
		if (temp <= chg_pdata->temp_range[i]) {
			fast_charge_current = chg_pdata->chg_current_limit[i];
			if (chg_pdata->chg_thermal_voltage_limit)
				charge_voltage_limit =
					chg_pdata->chg_thermal_voltage_limit[i];
			break;
		}
	}
	if (!fast_charge_current || !temp) {
		dev_info(bq2419x->dev, "Disable charging done by HW\n");
		return 0;
	}

	/* Fast charger become 50% when temp is at < 10 degC */
	if (temp <= 10)
		fast_charge_current *= 2;

	curr_ichg = bq2419x->chg_current_control.val >> 2;
	ichg = bq2419x_val_to_reg(fast_charge_current,
			BQ2419X_CHARGE_ICHG_OFFSET, 64, 6, 0);
	if (curr_ichg == ichg)
		return 0;

	bq2419x->chg_current_control.val = ichg << 2;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_CHRG_CTRL_REG,
			BQ2419X_CHRG_CTRL_ICHG_MASK, ichg << 2);
	if (ret < 0) {
		dev_err(bq2419x->dev, "CHRG_CTRL_REG update failed %d\n", ret);
		return ret;
	}

	if (!charge_voltage_limit)
		return 0;

	/* Charge voltage limit */
	vreg = bq2419x_val_to_reg(charge_voltage_limit,
			BQ2419X_CHARGE_VOLTAGE_OFFSET, 16, 6, 1);
	bq2419x->chg_voltage_control.mask = BQ2419X_CHG_VOLT_LIMIT_MASK;
	bq2419x->chg_voltage_control.val = vreg << 2;
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_VOLT_CTRL_REG,
				bq2419x->chg_voltage_control.mask,
				bq2419x->chg_voltage_control.val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "VOLT_CTRL_REG update failed %d\n", ret);
		return ret;
	}

	return 0;
}

static struct battery_charging_ops bq2419x_charger_bci_ops = {
	.get_charging_status = bq2419x_charger_get_status,
	.charge_term_configure = bq2419x_charger_termination_configure,
	.get_input_current_limit = bq2419x_get_input_current_limit,
	.input_voltage_configure = bq2419x_charger_input_voltage_configure,
	.thermal_configure = bq2419x_charger_thermal_configure,
};

static struct battery_charger_info bq2419x_charger_bci = {
	.cell_id = 0,
	.bc_ops = &bq2419x_charger_bci_ops,
};

static struct bq2419x_platform_data *bq2419x_dt_parse(struct i2c_client *client,
		struct device_node **chg_np, struct device_node **vbus_np)
{
	struct device_node *np = client->dev.of_node;
	struct bq2419x_platform_data *pdata;
	struct device_node *batt_reg_node;
	struct device_node *vbus_reg_node;
	int ret;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	batt_reg_node = of_find_node_by_name(np, "charger");
	if (batt_reg_node) {
		int auto_rechg_power_on_time;
		int count;
		int soc_range_len, inut_volt_lim_len = 0;
		int temp_range_len, chg_current_lim_len, chg_voltage_lim_len;
		int temp_polling_time;
		int wdt_timeout;
		struct bq2419x_charger_platform_data *chg_pdata;
		struct bq2419x_charger_platform_data *bcharger_pdata;
		u32 pval;

		if (!of_device_is_available(batt_reg_node)) {
			dev_info(&client->dev,
				"charger node status is disabled\n");
			goto vbus_node;
		}

		pdata->bcharger_pdata = devm_kzalloc(&client->dev,
				sizeof(*(pdata->bcharger_pdata)), GFP_KERNEL);
		if (!pdata->bcharger_pdata)
			return ERR_PTR(-ENOMEM);
		bcharger_pdata = pdata->bcharger_pdata;

		chg_pdata = pdata->bcharger_pdata;
		chg_pdata->ridata = of_get_regulator_init_data(&client->dev,
								batt_reg_node);
		if (!chg_pdata->ridata)
			return ERR_PTR(-EINVAL);

		ret = of_property_read_u32(batt_reg_node,
				"ti,input-voltage-limit-millivolt", &pval);
		if (!ret)
			bcharger_pdata->input_voltage_limit_mV = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,fast-charge-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->fast_charge_current_limit_mA =
							pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,pre-charge-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->pre_charge_current_limit_mA = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,charge-term-current-limit-milliamp", &pval);
		if (!ret)
			bcharger_pdata->termination_current_limit_mA = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-resister-ohm", &pval);
		if (!ret)
			bcharger_pdata->ir_compensation_resister_ohm = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,ir-comp-voltage-millivolt", &pval);
		if (!ret)
			bcharger_pdata->ir_compensation_voltage_mV = pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,thermal-regulation-threshold-degc", &pval);
		if (!ret)
			bcharger_pdata->thermal_regulation_threshold_degC =
						pval;

		ret = of_property_read_u32(batt_reg_node,
				"ti,charge-voltage-limit-millivolt", &pval);
		if (!ret)
			pdata->bcharger_pdata->charge_voltage_limit_mV = pval;

		pdata->bcharger_pdata->disable_suspend_during_charging =
				of_property_read_bool(batt_reg_node,
				"ti,disbale-suspend-during-charging");

		ret = of_property_read_u32(batt_reg_node,
					"ti,auto-rechg-power-on-time",
					&auto_rechg_power_on_time);
		if (!ret)
			pdata->bcharger_pdata->auto_rechg_power_on_time =
					auto_rechg_power_on_time;
		else
			pdata->bcharger_pdata->auto_rechg_power_on_time = 25;

		ret = of_property_read_u32(batt_reg_node,
				"ti,watchdog-timeout", &wdt_timeout);
		if (!ret)
			pdata->bcharger_pdata->wdt_timeout = wdt_timeout;

		count = of_property_count_u32(batt_reg_node, "ti,soc-range");
		soc_range_len = (count > 0) ? count : 0;

		if (soc_range_len) {
			chg_pdata->n_soc_profile = soc_range_len;
			chg_pdata->soc_range = devm_kzalloc(&client->dev,
				sizeof(u32) * soc_range_len, GFP_KERNEL);
			if (!chg_pdata->soc_range)
				return ERR_PTR(-ENOMEM);

			ret = of_property_read_u32_array(batt_reg_node,
					"ti,soc-range",
					chg_pdata->soc_range, soc_range_len);
			if (ret < 0)
				return ERR_PTR(ret);

			count =  of_property_count_u32(batt_reg_node,
					"ti,input-voltage-soc-limit");
			inut_volt_lim_len = (count > 0) ? count : 0;
		}

		if (inut_volt_lim_len) {
			chg_pdata->input_voltage_soc_limit =
					devm_kzalloc(&client->dev,
					sizeof(u32) * inut_volt_lim_len,
					GFP_KERNEL);
			if (!chg_pdata->input_voltage_soc_limit)
				return ERR_PTR(-ENOMEM);

			ret = of_property_read_u32_array(batt_reg_node,
					"ti,input-voltage-soc-limit",
					chg_pdata->input_voltage_soc_limit,
					inut_volt_lim_len);
			if (ret < 0)
				return ERR_PTR(ret);
		}

		ret = of_property_read_u32(batt_reg_node,
			"ti,temp-polling-time-sec", &temp_polling_time);
		if (!ret)
			bcharger_pdata->temp_polling_time_sec =
						temp_polling_time;

		chg_pdata->tz_name = of_get_property(batt_reg_node,
						"ti,thermal-zone", NULL);

		count = of_property_count_u32(batt_reg_node, "ti,temp-range");
		temp_range_len = (count > 0) ? count : 0;

		count = of_property_count_u32(batt_reg_node,
					"ti,charge-current-limit");
		if (count <= 0)
			count = of_property_count_u32(batt_reg_node,
					"ti,charge-thermal-current-limit");
		chg_current_lim_len = (count > 0) ? count : 0;

		count = of_property_count_u32(batt_reg_node,
					"ti,charge-thermal-voltage-limit");
		chg_voltage_lim_len = (count > 0) ? count : 0;

		if (!temp_range_len)
			goto vbus_node;

		if (temp_range_len != chg_current_lim_len) {
			dev_info(&client->dev,
				"current thermal profile is not correct\n");
			goto vbus_node;
		}

		if (chg_voltage_lim_len && (temp_range_len != chg_voltage_lim_len)) {
			dev_info(&client->dev,
				"voltage thermal profile is not correct\n");
			goto vbus_node;
		}

		chg_pdata->temp_range = devm_kzalloc(&client->dev,
				sizeof(u32) * temp_range_len, GFP_KERNEL);
		if (!chg_pdata->temp_range)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node, "ti,temp-range",
				chg_pdata->temp_range, temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

		chg_pdata->chg_current_limit = devm_kzalloc(&client->dev,
				sizeof(u32) * temp_range_len, GFP_KERNEL);
		if (!chg_pdata->chg_current_limit)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-current-limit",
				chg_pdata->chg_current_limit,
				temp_range_len);
		if (ret < 0)
			ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-thermal-current-limit",
					chg_pdata->chg_current_limit,
					temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

		if (!chg_voltage_lim_len)
			goto skip_thermal_volt_profle;

		chg_pdata->chg_thermal_voltage_limit =
					devm_kzalloc(&client->dev,
					sizeof(u32) * temp_range_len,
					GFP_KERNEL);
		if (!chg_pdata->chg_thermal_voltage_limit)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(batt_reg_node,
				"ti,charge-thermal-voltage-limit",
				chg_pdata->chg_thermal_voltage_limit,
				temp_range_len);
		if (ret < 0)
			return ERR_PTR(ret);

skip_thermal_volt_profle:
		chg_pdata->n_temp_profile = temp_range_len;
	}

vbus_node:
	vbus_reg_node = of_find_node_by_name(np, "vbus");
	if (vbus_reg_node) {
		pdata->vbus_pdata = devm_kzalloc(&client->dev,
			sizeof(*(pdata->vbus_pdata)), GFP_KERNEL);
		if (!pdata->vbus_pdata)
			return ERR_PTR(-ENOMEM);

		pdata->vbus_pdata->ridata = of_get_regulator_init_data(
					&client->dev, vbus_reg_node);
		if (!pdata->vbus_pdata->ridata)
			return ERR_PTR(-EINVAL);
		pdata->vbus_pdata->gpio_otg_iusb =
				of_get_named_gpio(vbus_reg_node,
					"ti,otg-iusb-gpio", 0);
	}

	*chg_np = batt_reg_node;
	*vbus_np = vbus_reg_node;
	return pdata;
}

static int bq2419x_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct bq2419x_chip *bq2419x;
	struct bq2419x_platform_data *pdata = NULL;
	struct device_node *chg_np = NULL;
	struct device_node *vbus_np = NULL;
	int ret = 0;
	int val = 0;

	if (client->dev.platform_data)
		pdata = client->dev.platform_data;

	if (!pdata && client->dev.of_node) {
		pdata = bq2419x_dt_parse(client, &chg_np, &vbus_np);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			dev_err(&client->dev, "Parsing of node failed, %d\n",
				ret);
			return ret;
		}
	}

	if (!pdata) {
		dev_err(&client->dev, "No Platform data");
		return -EINVAL;
	}

	bq2419x = devm_kzalloc(&client->dev, sizeof(*bq2419x), GFP_KERNEL);
	if (!bq2419x) {
		dev_err(&client->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}
	bq2419x->charger_pdata = pdata->bcharger_pdata;
	bq2419x->vbus_pdata = pdata->vbus_pdata;
	bq2419x->ext_name = pdata->ext_name;
	bq2419x->chg_np = chg_np;
	bq2419x->vbus_np = vbus_np;

	bq2419x->regmap = devm_regmap_init_i2c(client, &bq2419x_regmap_config);
	if (IS_ERR(bq2419x->regmap)) {
		ret = PTR_ERR(bq2419x->regmap);
		dev_err(&client->dev, "regmap init failed with err %d\n", ret);
		return ret;
	}

	bq2419x->dev = &client->dev;
	i2c_set_clientdata(client, bq2419x);
	bq2419x->irq = client->irq;
	mutex_init(&bq2419x->otg_mutex);
	bq2419x->is_otg_connected = 0;
	bq2419x->shutdown_complete = 0;
	bq2419x->wake_lock_released = false;
	bq2419x->charging_disabled_on_suspend = 0;

	ret = bq2419x_show_chip_version(bq2419x);
	if (ret < 0) {
		dev_err(&client->dev, "version read failed %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&client->dev.kobj, &bq2419x_attr_group);
	if (ret < 0) {
		dev_err(&client->dev, "sysfs create failed %d\n", ret);
		return ret;
	}

	bq2419x_debugfs_init(client);

	bq2419x->edev.supported_cable = bq2419x_extcon_cable;
	bq2419x->edev.name = bq2419x->ext_name;
	bq2419x->edev.dev.parent = bq2419x->dev;

	ret = extcon_dev_register(&bq2419x->edev);
	if (ret < 0) {
		dev_err(bq2419x->dev, "failed to register extcon device\n");
		return ret;
	}
	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG rd failed %d\n", ret);
		return ret;
	}
	ret = bq2419x_extcon_cable_update(bq2419x, val);

	mutex_init(&bq2419x->mutex);

	if (!pdata->bcharger_pdata) {
		dev_info(&client->dev, "No battery charger supported\n");
		ret = bq2419x_watchdog_disable(bq2419x);
		if (ret < 0) {
			dev_err(bq2419x->dev, "WDT disable failed: %d\n", ret);
			goto scrub_mutex;
		}

		/* disable safety timer */
		ret = bq2419x_safetytimer_disable(bq2419x);
		if (ret < 0) {
			dev_err(bq2419x->dev,
				"Safety timer disable failed: %d\n", ret);
			goto scrub_mutex;
		}

		/* disable chrg and bat fault interrupt */
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_MISC_OPER_REG,
				BQ2419x_CHRG_BAT_FAULT_MASK,
				BQ2419x_CHRG_BAT_FAULT_DISABLE);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"Chrg and Bat fault interrupt disable failed %d\n", ret);

		ret = bq2419x_fault_clear_sts(bq2419x, NULL);
		if (ret < 0) {
			dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
			goto scrub_mutex;
		}
		goto skip_bcharger_init;
	}

	bq2419x->battery_presense = true;
	bq2419x->disable_suspend_during_charging =
			pdata->bcharger_pdata->disable_suspend_during_charging;
	bq2419x->auto_rechg_power_on_time =
			pdata->bcharger_pdata->auto_rechg_power_on_time;
	bq2419x->wdt_time_sec = pdata->bcharger_pdata->wdt_timeout;

	bq2419x_process_charger_plat_data(bq2419x, pdata->bcharger_pdata);

	ret = bq2419x_charger_init(bq2419x);
	if (ret < 0) {
		dev_err(bq2419x->dev, "Charger init failed: %d\n", ret);
		goto scrub_mutex;
	}

	ret = bq2419x_init_charger_regulator(bq2419x, pdata);
	if (ret < 0) {
		dev_err(&client->dev,
			"Charger regualtor init failed %d\n", ret);
		goto scrub_mutex;
	}

	bq2419x_charger_bci.polling_time_sec =
			pdata->bcharger_pdata->temp_polling_time_sec;
	bq2419x_charger_bci.tz_name = pdata->bcharger_pdata->tz_name;
	bq2419x->bc_dev = battery_charger_register(bq2419x->dev,
			&bq2419x_charger_bci, bq2419x);
	if (IS_ERR(bq2419x->bc_dev)) {
		ret = PTR_ERR(bq2419x->bc_dev);
		dev_err(bq2419x->dev, "battery charger register failed: %d\n",
			ret);
		goto scrub_mutex;
	}

	ret = bq2419x_fault_clear_sts(bq2419x, NULL);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		goto scrub_wq;
	}

	INIT_DELAYED_WORK(&bq2419x->wdt_restart_wq, bq2419x_wdt_restart_wq);

skip_bcharger_init:
	ret = devm_request_threaded_irq(bq2419x->dev, bq2419x->irq, NULL,
		bq2419x_irq, IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			dev_name(bq2419x->dev), bq2419x);
	if (ret < 0) {
		dev_warn(bq2419x->dev, "request IRQ %d fail, err = %d\n",
				bq2419x->irq, ret);
		dev_info(bq2419x->dev,
			"Supporting bq driver without interrupt\n");
		ret = 0;
	}
	device_set_wakeup_capable(bq2419x->dev, true);
	device_wakeup_enable(bq2419x->dev);

	ret = bq2419x_init_vbus_regulator(bq2419x, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "VBUS regulator init failed %d\n", ret);
		goto scrub_wq;
	}

	/* enable charging */
	ret = bq2419x_charger_enable(bq2419x);
	if (ret < 0)
		goto scrub_wq;

	if (bq2419x->battery_presense) {
		bq2419x->thermal_init_retry = 10;
#ifdef CONFIG_THERMAL
		INIT_DELAYED_WORK(&bq2419x->thermal_init_work,
				bq2419x_thermal_init_work);
		bq2419x_thermal_init_work(&bq2419x->thermal_init_work.work);
#endif
	}

	INIT_DELAYED_WORK(&bq2419x->otg_reset_work,
				bq2419x_otg_reset_work_handler);

	return 0;
scrub_wq:
	if (pdata->bcharger_pdata)
		battery_charger_unregister(bq2419x->bc_dev);
scrub_mutex:
	extcon_dev_unregister(&bq2419x->edev);
	mutex_destroy(&bq2419x->mutex);
	mutex_destroy(&bq2419x->otg_mutex);
	return ret;
}

static int bq2419x_remove(struct i2c_client *client)
{
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);

	if (bq2419x->battery_presense)
		battery_charger_unregister(bq2419x->bc_dev);
	extcon_dev_unregister(&bq2419x->edev);

	if (bq2419x->is_otg_connected) {
		cancel_delayed_work_sync(&bq2419x->otg_reset_work);
		bq2419x_vbus_disable(bq2419x->vbus_rdev);
	}

	mutex_destroy(&bq2419x->mutex);
	mutex_destroy(&bq2419x->otg_mutex);
	cancel_delayed_work_sync(&bq2419x->thermal_init_work);
	if (bq2419x->battery_presense && bq2419x->die_tz_device)
		thermal_zone_of_sensor_unregister(bq2419x->dev,
			bq2419x->die_tz_device);
	return 0;
}

static void bq2419x_shutdown(struct i2c_client *client)
{
	struct bq2419x_chip *bq2419x = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int ret;
	int next_poweron_time = 0;

	mutex_lock(&bq2419x->mutex);
	bq2419x->shutdown_complete = 1;
	mutex_unlock(&bq2419x->mutex);

	if (bq2419x->is_otg_connected) {
		cancel_delayed_work_sync(&bq2419x->otg_reset_work);
		bq2419x_vbus_disable(bq2419x->vbus_rdev);
	}

	if (!bq2419x->battery_presense)
		return;

	if (!bq2419x->cable_connected)
		goto end;

	/* Clear JEITA_VSET bit if cable connected while device off */
	ret = regmap_update_bits(bq2419x->regmap, BQ2419X_MISC_OPER_REG,
					BQ2419X_JEITA_VSET_MASK, 0);
	if (ret < 0)
		dev_err(bq2419x->dev, "JEITA_VSET update failed: %d\n", ret);

	battery_charger_thermal_stop_monitoring(bq2419x->bc_dev);

	if (bq2419x->chg_status == BATTERY_CHARGING_DONE) {
		dev_info(bq2419x->dev, "Battery charging done\n");
		goto end;
	}

	if (bq2419x->in_current_limit <= 500)
		dev_info(bq2419x->dev, "Battery charging with 500mA\n");
	else {
		dev_info(bq2419x->dev, "Battery charging with high current\n");
		next_poweron_time = bq2419x->auto_rechg_power_on_time;
	}

	if (next_poweron_time) {
		ret = battery_charging_system_reset_after(bq2419x->bc_dev,
				next_poweron_time);
		if (ret < 0)
			dev_err(dev,
			"System poweron after %d config failed %d\n",
			next_poweron_time, ret);
	}
end:
	if (next_poweron_time)
		dev_info(dev, "System-charger will power-ON after %d sec\n",
				next_poweron_time);
	else
		dev_info(bq2419x->dev, "System-charger will not power-ON\n");

	battery_charging_system_power_on_usb_event(bq2419x->bc_dev);
	cancel_delayed_work_sync(&bq2419x->thermal_init_work);
}

#ifdef CONFIG_PM_SLEEP
static int bq2419x_suspend(struct device *dev)
{
	struct bq2419x_chip *bq2419x = dev_get_drvdata(dev);
	int ret = 0;
	int battery_soc = 0;

	if (device_may_wakeup(bq2419x->dev) && (bq2419x->irq > 0))
		enable_irq_wake(bq2419x->irq);

	if (!bq2419x->battery_presense)
		return 0;

	mutex_lock(&bq2419x->mutex);
	if (bq2419x->is_otg_connected) {
		ret = bq2419x_reset_wdt(bq2419x, "Suspend");
		if (ret < 0)
			dev_err(bq2419x->dev, "Reset WDT failed: %d\n", ret);
	}
	mutex_unlock(&bq2419x->mutex);

	battery_soc = battery_gauge_get_battery_soc(bq2419x->bc_dev);
	if (battery_soc > BQ2419X_PC_USB_LP0_THRESHOLD &&
			(bq2419x->in_current_limit <= 500) &&
			!bq2419x->is_otg_connected) {
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_DISABLE_CHARGE);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"REG update failed: err %d\n", ret);
		else
			bq2419x->charging_disabled_on_suspend = true;
	}

	if (!bq2419x->cable_connected)
		goto end;

	if (bq2419x->chg_status == BATTERY_CHARGING_DONE) {
		dev_info(bq2419x->dev, "Battery charging done\n");
		goto end;
	}

	if (bq2419x->in_current_limit <= 500) {
		dev_info(bq2419x->dev, "Battery charging with 500mA\n");
		ret = bq2419x_set_charging_current_suspend(bq2419x, 500);
		if (ret < 0)
			dev_err(bq2419x->dev,
			"Config of charging failed: %d\n", ret);
		if (battery_soc > BQ2419X_PC_USB_LP0_THRESHOLD &&
					!bq2419x->is_otg_connected) {
			ret = regmap_update_bits(bq2419x->regmap,
				BQ2419X_PWR_ON_REG, BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_DISABLE_CHARGE);
			if (ret < 0)
				dev_err(bq2419x->dev,
					"REG update failed: err %d\n", ret);
			else
				bq2419x->charging_disabled_on_suspend = true;
		}
	} else
		dev_info(bq2419x->dev, "Battery charging with high current\n");

end:
	return ret;
}

static int bq2419x_resume(struct device *dev)
{
	int ret = 0;
	struct bq2419x_chip *bq2419x = dev_get_drvdata(dev);
	unsigned int val;

	if (device_may_wakeup(bq2419x->dev) && (bq2419x->irq > 0))
		disable_irq_wake(bq2419x->irq);

	if (bq2419x->charging_disabled_on_suspend) {
		bq2419x->charging_disabled_on_suspend = false;
		ret = regmap_update_bits(bq2419x->regmap, BQ2419X_PWR_ON_REG,
				BQ2419X_ENABLE_CHARGE_MASK,
				BQ2419X_ENABLE_CHARGE);
		if (ret < 0)
			dev_err(bq2419x->dev,
				"register update failed: err %d\n", ret);
	}

	ret = regmap_read(bq2419x->regmap, BQ2419X_SYS_STAT_REG, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "SYS_STAT_REG read failed %d\n", ret);
		return IRQ_HANDLED;
	}
	if ((val & BQ2419x_VBUS_PG_STAT) == BQ2419x_PG_VBUS_USB) {
		extcon_set_cable_state(&bq2419x->edev,
						bq2419x_extcon_cable[0], true);
		if (!bq2419x->cable_connected)
			dev_info(bq2419x->dev, "USB is connected\n");
	} else if ((val & BQ2419x_VBUS_PG_STAT) == BQ2419x_VBUS_UNKNOWN) {
		extcon_set_cable_state(&bq2419x->edev,
						bq2419x_extcon_cable[0], false);
		if (bq2419x->cable_connected)
			dev_info(bq2419x->dev, "USB is disconnected\n");
	}

	if (!bq2419x->battery_presense)
		return 0;

	ret = bq2419x_fault_clear_sts(bq2419x, &val);
	if (ret < 0) {
		dev_err(bq2419x->dev, "fault clear status failed %d\n", ret);
		return ret;
	}

	if (val & BQ2419x_FAULT_WATCHDOG_FAULT) {
		bq_chg_err(bq2419x, "Watchdog Timer Expired\n");
		ret = bq2419x_reconfigure_charger_param(bq2419x,
								"WDT-EXP-RESUME");
		if (ret < 0)
			dev_err(bq2419x->dev, "BQ reconfig failed %d\n", ret);
	}

	return 0;
};
#endif

static const struct dev_pm_ops bq2419x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq2419x_suspend, bq2419x_resume)
};

static const struct i2c_device_id bq2419x_id[] = {
	{.name = "bq2419x",},
	{},
};
MODULE_DEVICE_TABLE(i2c, bq2419x_id);

static struct i2c_driver bq2419x_i2c_driver = {
	.driver = {
		.name	= "bq2419x",
		.owner	= THIS_MODULE,
		.pm = &bq2419x_pm_ops,
	},
	.probe		= bq2419x_probe,
	.remove		= bq2419x_remove,
	.shutdown	= bq2419x_shutdown,
	.id_table	= bq2419x_id,
};

static int __init bq2419x_module_init(void)
{
	return i2c_add_driver(&bq2419x_i2c_driver);
}
subsys_initcall(bq2419x_module_init);

static void __exit bq2419x_cleanup(void)
{
	i2c_del_driver(&bq2419x_i2c_driver);
}
module_exit(bq2419x_cleanup);

MODULE_DESCRIPTION("BQ24190/BQ24192/BQ24192i/BQ24193 battery charger driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin <srafiuddin@nvidia.com");
MODULE_LICENSE("GPL v2");
