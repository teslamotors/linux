/*
 * Copyright (C) 2010 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max8907c.h>
#include <linux/regulator/max8907c-regulator.h>
#include <linux/gpio.h>
#include <mach/suspend.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "gpio-names.h"
#include "power.h"
#include "wakeups-t2.h"
#include "board.h"

#define PMC_CTRL		0x0
 #define PMC_CTRL_INTR_LOW	(1 << 17)

static struct regulator_consumer_supply max8907c_SD1_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply max8907c_SD2_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
};

static struct regulator_consumer_supply max8907c_SD3_supply[] = {
	REGULATOR_SUPPLY("vddio_sys", NULL),
};

static struct regulator_consumer_supply max8907c_LDO1_supply[] = {
	REGULATOR_SUPPLY("vddio_rx_ddr", NULL),
};

static struct regulator_consumer_supply max8907c_LDO2_supply[] = {
	REGULATOR_SUPPLY("avdd_plla", NULL),
};

static struct regulator_consumer_supply max8907c_LDO3_supply[] = {
	REGULATOR_SUPPLY("vdd_vcom_1v8b", NULL),
};

static struct regulator_consumer_supply max8907c_LDO4_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", NULL),
	REGULATOR_SUPPLY("avdd_usb_pll", NULL),
};

static struct regulator_consumer_supply max8907c_LDO5_supply[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.1"),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vmmc", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply max8907c_LDO6_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", NULL),
};

static struct regulator_consumer_supply max8907c_LDO7_supply[] = {
	REGULATOR_SUPPLY("avddio_audio", NULL),
};

static struct regulator_consumer_supply max8907c_LDO8_supply[] = {
	REGULATOR_SUPPLY("vdd_vcom_3v0", NULL),
};

static struct regulator_consumer_supply max8907c_LDO9_supply[] = {
	REGULATOR_SUPPLY("vdd_cam1", NULL),
};

static struct regulator_consumer_supply max8907c_LDO10_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_ic", NULL),
};

static struct regulator_consumer_supply max8907c_LDO11_supply[] = {
	REGULATOR_SUPPLY("vddio_pex_clk", NULL),
	REGULATOR_SUPPLY("avdd_hdmi", NULL),
};

static struct regulator_consumer_supply max8907c_LDO12_supply[] = {
	REGULATOR_SUPPLY("vddio_sdio", NULL),
};

static struct regulator_consumer_supply max8907c_LDO13_supply[] = {
	REGULATOR_SUPPLY("vdd_vcore_phtn", NULL),
	REGULATOR_SUPPLY("vdd_vcore_af", NULL),
};

static struct regulator_consumer_supply max8907c_LDO14_supply[] = {
	REGULATOR_SUPPLY("avdd_vdac", NULL),
};

static struct regulator_consumer_supply max8907c_LDO15_supply[] = {
	REGULATOR_SUPPLY("vdd_vcore_temp", NULL),
	REGULATOR_SUPPLY("vdd_vcore_hdcp", NULL),
};

static struct regulator_consumer_supply max8907c_LDO16_supply[] = {
	REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

static struct regulator_consumer_supply max8907c_LDO17_supply[] = {
	REGULATOR_SUPPLY("vddio_mipi", NULL),
};

static struct regulator_consumer_supply max8907c_LDO18_supply[] = {
	REGULATOR_SUPPLY("vcsi", "tegra_camera"),
};

static struct regulator_consumer_supply max8907c_LDO19_supply[] = {
	REGULATOR_SUPPLY("vddio_lx", NULL),
};

static struct regulator_consumer_supply max8907c_LDO20_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr_1v2", NULL),
	REGULATOR_SUPPLY("vddio_hsic", NULL),
};

#define MAX8907C_REGULATOR_DEVICE(_id, _minmv, _maxmv)			\
static struct regulator_init_data max8907c_##_id##_data = {		\
	.constraints = {						\
		.min_uV = (_minmv),					\
		.max_uV = (_maxmv),					\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
				     REGULATOR_MODE_STANDBY),		\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
				   REGULATOR_CHANGE_STATUS |		\
				   REGULATOR_CHANGE_VOLTAGE),		\
	},								\
	.num_consumer_supplies = ARRAY_SIZE(max8907c_##_id##_supply),	\
	.consumer_supplies = max8907c_##_id##_supply,			\
};									\
static struct platform_device max8907c_##_id##_device = {		\
	.name	= "max8907c-regulator",					\
	.id	= MAX8907C_##_id,					\
	.dev	= {							\
		.platform_data = &max8907c_##_id##_data,		\
	},								\
}

MAX8907C_REGULATOR_DEVICE(SD1, 637500, 1425000);
MAX8907C_REGULATOR_DEVICE(SD2, 650000, 2225000);
MAX8907C_REGULATOR_DEVICE(SD3, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO1, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO2, 650000, 2225000);
MAX8907C_REGULATOR_DEVICE(LDO3, 650000, 2225000);
MAX8907C_REGULATOR_DEVICE(LDO4, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO5, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO6, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO7, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO8, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO9, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO10, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO11, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO12, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO13, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO14, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO15, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO16, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO17, 650000, 2225000);
MAX8907C_REGULATOR_DEVICE(LDO18, 650000, 2225000);
MAX8907C_REGULATOR_DEVICE(LDO19, 750000, 3900000);
MAX8907C_REGULATOR_DEVICE(LDO20, 750000, 3900000);

static struct platform_device *whistler_max8907c_power_devices[] = {
	&max8907c_SD1_device,
	&max8907c_SD2_device,
	&max8907c_SD3_device,
	&max8907c_LDO1_device,
	&max8907c_LDO2_device,
	&max8907c_LDO3_device,
	&max8907c_LDO4_device,
	&max8907c_LDO5_device,
	&max8907c_LDO6_device,
	&max8907c_LDO7_device,
	&max8907c_LDO8_device,
	&max8907c_LDO9_device,
	&max8907c_LDO10_device,
	&max8907c_LDO11_device,
	&max8907c_LDO12_device,
	&max8907c_LDO13_device,
	&max8907c_LDO14_device,
	&max8907c_LDO15_device,
	&max8907c_LDO16_device,
	&max8907c_LDO17_device,
	&max8907c_LDO18_device,
	&max8907c_LDO19_device,
	&max8907c_LDO20_device,
};

static struct max8907c_platform_data max8907c_pdata = {
	.num_subdevs = ARRAY_SIZE(whistler_max8907c_power_devices),
	.subdevs = whistler_max8907c_power_devices,
};

static struct i2c_board_info __initdata whistler_regulators[] = {
	{
		I2C_BOARD_INFO("max8907c", 0x3C),
		.platform_data	= &max8907c_pdata,
	},
};

static struct tegra_suspend_platform_data whistler_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 0,
	.suspend_mode	= TEGRA_SUSPEND_NONE,
	.core_timer	= 0x7e7e,
	.core_off_timer = 0,
	.separate_req	= true,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.wake_enb	= 0,
	.wake_high	= 0,
	.wake_low	= 0,
	.wake_any	= 0,
};

int __init whistler_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/* configure the power management controller to trigger PMU
	 * interrupts when low */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	i2c_register_board_info(4, whistler_regulators, 1);

	tegra_init_suspend(&whistler_suspend_data);

	return 0;
}
