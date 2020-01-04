/*
 * arch/arm/mach-tegra/board-p852-gpio.c
 *
 * Copyright (C) 2010 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include "board-p852.h"

static struct gpio p852_sku23_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_LOW , "usbpwr0_ena" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_LOW, "usbpwr1_ena" },
	{TEGRA_GPIO_PA0, GPIOF_OUT_INIT_LOW, "a0" },
	{TEGRA_GPIO_PV2, GPIOF_OUT_INIT_HIGH, "v2" },
	{TEGRA_GPIO_PT4, GPIOF_OUT_INIT_LOW, "t4" },
	{TEGRA_GPIO_PD6, GPIOF_OUT_INIT_HIGH, "d6" },
	{TEGRA_GPIO_PI3, GPIOF_OUT_INIT_LOW, "i3" },
	{TEGRA_GPIO_PV3, GPIOF_OUT_INIT_HIGH, "v3" },
	{TEGRA_GPIO_PW4, GPIOF_IN, "w4" },
	{TEGRA_GPIO_PW5, GPIOF_IN, "w5" },
	{TEGRA_GPIO_PT1, GPIOF_OUT_INIT_LOW, "t1" },
	{TEGRA_GPIO_PW3, GPIOF_OUT_INIT_HIGH, "w3" },
	{TEGRA_GPIO_PD5, GPIOF_IN, "d5" },
	{TEGRA_GPIO_PBB1, GPIOF_OUT_INIT_LOW, "bb1" },
};

static struct gpio p852_sku23_b00_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_HIGH, "usbpwr0_ena" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_HIGH, "usbpwr1_ena" },
	{TEGRA_GPIO_PA0, GPIOF_OUT_INIT_LOW, "a0" },
	{TEGRA_GPIO_PV2, GPIOF_OUT_INIT_HIGH, "v2" },
	{TEGRA_GPIO_PT4, GPIOF_OUT_INIT_LOW, "t4" },
	{TEGRA_GPIO_PD6, GPIOF_OUT_INIT_HIGH, "d6" },
	{TEGRA_GPIO_PI3, GPIOF_OUT_INIT_LOW, "i3" },
	{TEGRA_GPIO_PV3, GPIOF_OUT_INIT_HIGH, "v3" },
	{TEGRA_GPIO_PW4, GPIOF_IN, "w4" },
	{TEGRA_GPIO_PW5, GPIOF_IN, "w5" },
	{TEGRA_GPIO_PT1, GPIOF_OUT_INIT_LOW, "t1" },
	{TEGRA_GPIO_PW3, GPIOF_OUT_INIT_HIGH, "w3" },
	{TEGRA_GPIO_PD5, GPIOF_IN, "d5" },
	{TEGRA_GPIO_PBB1, GPIOF_OUT_INIT_LOW, "bb1" },
};

static struct gpio p852_sku5_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_HIGH, "usbpwr0_ena" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_HIGH, "usbpwr1_ena" },
	{TEGRA_GPIO_PA0, GPIOF_OUT_INIT_LOW, "a0" },
	{TEGRA_GPIO_PV2, GPIOF_OUT_INIT_HIGH, "v2" },
	{TEGRA_GPIO_PT4, GPIOF_OUT_INIT_LOW, "t4" },
	{TEGRA_GPIO_PD6, GPIOF_OUT_INIT_HIGH, "d6" },
	{TEGRA_GPIO_PI3, GPIOF_OUT_INIT_LOW, "i3" },
	{TEGRA_GPIO_PV3, GPIOF_OUT_INIT_HIGH, "v3" },
	{TEGRA_GPIO_PW4, GPIOF_IN, "w4" },
	{TEGRA_GPIO_PW5, GPIOF_IN, "w5" },
	{TEGRA_GPIO_PT1, GPIOF_OUT_INIT_LOW, "t1" },
	{TEGRA_GPIO_PW3, GPIOF_OUT_INIT_HIGH, "w3" },
	{TEGRA_GPIO_PD5, GPIOF_IN, "d5" },
	{TEGRA_GPIO_PBB1, GPIOF_OUT_INIT_LOW, "bb1" },
	{TEGRA_GPIO_PS3, GPIOF_IN, "s3" },
};

static struct gpio p852_sku8_b00_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_HIGH , "w1" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_HIGH, "b2" },
};

static struct gpio p852_sku8_c01_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_HIGH , "w1" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_HIGH, "b2" },
	{TEGRA_GPIO_PR0, GPIOF_OUT_INIT_HIGH, "therm_shutdown" },
	{TEGRA_GPIO_PD4, GPIOF_IN, "therm_alert" },
};

static struct gpio p852_sku13_b00_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_HIGH, "w1" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_HIGH, "b2" },
	{TEGRA_GPIO_PW2, GPIOF_IN, "w2" },
	{TEGRA_GPIO_PW3, GPIOF_IN, "w3" },
	{TEGRA_GPIO_PD5, GPIOF_OUT_INIT_LOW, "d5" },
	{TEGRA_GPIO_PBB1, GPIOF_OUT_INIT_LOW, "bb1" },
	{TEGRA_GPIO_PN7, GPIOF_OUT_INIT_LOW, "n7" },
	{TEGRA_GPIO_PA6, GPIOF_OUT_INIT_HIGH, "a6" },
	{TEGRA_GPIO_PA7, GPIOF_OUT_INIT_HIGH, "a7" },
};

static struct gpio p852_gpios[] = {
	{TEGRA_GPIO_PW1, GPIOF_OUT_INIT_LOW , "w1" },
	{TEGRA_GPIO_PB2, GPIOF_OUT_INIT_LOW, "b2" },
	{TEGRA_GPIO_PW2, GPIOF_IN, "w2" },
	{TEGRA_GPIO_PW3, GPIOF_IN, "w3" },
	{TEGRA_GPIO_PD5, GPIOF_OUT_INIT_LOW, "d5" },
	{TEGRA_GPIO_PBB1, GPIOF_OUT_INIT_LOW, "bb1" },
	{TEGRA_GPIO_PN7, GPIOF_OUT_INIT_LOW, "n7" },
	{TEGRA_GPIO_PA6, GPIOF_OUT_INIT_HIGH, "a6" },
	{TEGRA_GPIO_PA7, GPIOF_OUT_INIT_HIGH, "a7" },
};


void __init p852_gpio_init(void)
{
	int pin_count=0;
	int i;
	struct gpio *gpios_info=NULL;

	if (machine_is_p852_sku23())
	{
		gpios_info = p852_sku23_gpios;
		pin_count = ARRAY_SIZE(p852_sku23_gpios);
	}
	else if (machine_is_p852_sku23_b00() || machine_is_p852_sku23_c01())
	{
		gpios_info = p852_sku23_b00_gpios;
		pin_count = ARRAY_SIZE(p852_sku23_b00_gpios);
	}
	else if (machine_is_p852_sku5_b00() || machine_is_p852_sku5_c01())
	{
		gpios_info = p852_sku5_gpios;
		pin_count = ARRAY_SIZE(p852_sku5_gpios);
	}
	else if (machine_is_p852_sku8_b00() || machine_is_p852_sku9_b00())
	{
		gpios_info = p852_sku8_b00_gpios;
		pin_count = ARRAY_SIZE(p852_sku8_b00_gpios);
	}
	else if (machine_is_p852_sku8_c01() || machine_is_p852_sku9_c01())
	{
		gpios_info = p852_sku8_c01_gpios;
		pin_count = ARRAY_SIZE(p852_sku8_c01_gpios);
	}
	else if (machine_is_p852_sku13_b00())
	{
		gpios_info = p852_sku13_b00_gpios;
		pin_count = ARRAY_SIZE(p852_sku13_b00_gpios);
	}
	else
	{
		gpios_info = p852_gpios;
		pin_count = ARRAY_SIZE(p852_gpios);
	}

	gpio_request_array(gpios_info,pin_count);
	for(i = 0; i < pin_count; i++)
	{
		tegra_gpio_enable(gpios_info[i].gpio);
		gpio_export(gpios_info[i].gpio,true);
	}
}
