/*
 * arch/arm/mach-tegra/board-harmony-sdhci.c
 *
 * Copyright (C) 2010 Google, Inc.
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

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>
#include <mach/pinmux.h>

#include "gpio-names.h"

#define HARMONY_WLAN_PWR	TEGRA_GPIO_PK5
#define HARMONY_WLAN_RST	TEGRA_GPIO_PK6
#define HARMONY_WLAN_TPS6586_GPIO_2	TEGRA_NR_GPIOS + 1

static int harmony_wifi_power_on(void)
{
	int status;

	status = gpio_request(HARMONY_WLAN_TPS6586_GPIO_2, "wifi_core");
	if (status < 0) {
		printk(KERN_INFO "WIFI gpio request fail\n");
		return status;
	}

	status = gpio_direction_output(HARMONY_WLAN_TPS6586_GPIO_2, 1);
	if (status < 0) {
		printk(KERN_INFO "WIFI_CORE power on fail\n");
		goto fail3;
	}

	status = gpio_request(HARMONY_WLAN_PWR, "wlan_power");
	if (status < 0) {
		printk(KERN_INFO "WLAN_PWR request fail\n");
		goto fail3;
	}

	status = gpio_request(HARMONY_WLAN_RST, "wlan_rst");
	if (status < 0) {
		printk(KERN_INFO "WLAN_RST gpio request fail\n");
		goto fail2;
	}

	tegra_gpio_enable(HARMONY_WLAN_PWR);
	tegra_gpio_enable(HARMONY_WLAN_RST);

	/* Murata wifi chip reset sequence */
	status = gpio_direction_output(HARMONY_WLAN_PWR, 0);
	if (status < 0) {
		printk(KERN_INFO "WLAN_PWR gpio output set fail\n");
		goto fail1;
	}

	status = gpio_direction_output(HARMONY_WLAN_RST, 0);
	if (status < 0) {
		printk(KERN_INFO "WLAN_RST gpio output set fail\n");
		goto fail1;
	}

	mdelay(20);

	status = gpio_direction_output(HARMONY_WLAN_PWR, 1);
	if (status < 0) {
		printk(KERN_INFO "WLAN_PWR gpio output set fail\n");
		goto fail1;
	}

	status = gpio_direction_output(HARMONY_WLAN_RST, 1);
	if (status < 0) {
		printk(KERN_INFO "WLAN_RST gpio output set fail\n");
		goto fail1;
	}

	gpio_export(HARMONY_WLAN_PWR, 0);
	gpio_export(HARMONY_WLAN_RST, 0);

	return 0;

fail1:
	gpio_free(HARMONY_WLAN_RST);
fail2:
	gpio_free(HARMONY_WLAN_PWR);
fail3:
	gpio_free(HARMONY_WLAN_TPS6586_GPIO_2);

	return status;
}

static struct resource sdhci_resource1[] = {
	[0] = {
	       .start = INT_SDMMC1,
	       .end = INT_SDMMC1,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC1_BASE,
	       .end = TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource sdhci_resource2[] = {
	[0] = {
	       .start = INT_SDMMC2,
	       .end = INT_SDMMC2,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC2_BASE,
	       .end = TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct resource sdhci_resource4[] = {
	[0] = {
	       .start = INT_SDMMC4,
	       .end = INT_SDMMC4,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       .start = TEGRA_SDMMC4_BASE,
	       .end = TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE - 1,
	       .flags = IORESOURCE_MEM,
	       },
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data1 = {
	.clk_id = NULL,
	.force_hs = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.platform_init = harmony_wifi_power_on,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.clk_id = NULL,
	.force_hs = 1,
	.bus_width = 4,
	.cd_gpio = TEGRA_GPIO_PI5,
	.wp_gpio = TEGRA_GPIO_PH1,
	.power_gpio = TEGRA_GPIO_PT3,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data4 = {
	.clk_id = NULL,
	.force_hs = 0,
	.cd_gpio = TEGRA_GPIO_PH2,
	.wp_gpio = TEGRA_GPIO_PH3,
	.power_gpio = TEGRA_GPIO_PI6,
};

static struct platform_device tegra_sdhci_device1 = {
	.name = "sdhci-tegra",
	.id = 0,
	.resource = sdhci_resource1,
	.num_resources = ARRAY_SIZE(sdhci_resource1),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data1,
		},
};

static struct platform_device tegra_sdhci_device2 = {
	.name = "sdhci-tegra",
	.id = 1,
	.resource = sdhci_resource2,
	.num_resources = ARRAY_SIZE(sdhci_resource2),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data2,
		},
};

static struct platform_device tegra_sdhci_device4 = {
	.name = "sdhci-tegra",
	.id = 3,
	.resource = sdhci_resource4,
	.num_resources = ARRAY_SIZE(sdhci_resource4),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data4,
		},
};

int __init harmony_sdhci_init(void)
{
	gpio_request(tegra_sdhci_platform_data2.power_gpio, "sdhci2_power");
	gpio_request(tegra_sdhci_platform_data2.cd_gpio, "sdhci2_cd");
	gpio_request(tegra_sdhci_platform_data2.wp_gpio, "sdhci2_wp");

	tegra_gpio_enable(tegra_sdhci_platform_data2.power_gpio);
	tegra_gpio_enable(tegra_sdhci_platform_data2.cd_gpio);
	tegra_gpio_enable(tegra_sdhci_platform_data2.wp_gpio);

	gpio_request(tegra_sdhci_platform_data4.power_gpio, "sdhci4_power");
	gpio_request(tegra_sdhci_platform_data4.cd_gpio, "sdhci4_cd");
	gpio_request(tegra_sdhci_platform_data4.wp_gpio, "sdhci4_wp");

	tegra_gpio_enable(tegra_sdhci_platform_data4.power_gpio);
	tegra_gpio_enable(tegra_sdhci_platform_data4.cd_gpio);
	tegra_gpio_enable(tegra_sdhci_platform_data4.wp_gpio);

	gpio_direction_output(tegra_sdhci_platform_data2.power_gpio, 1);
	gpio_direction_output(tegra_sdhci_platform_data4.power_gpio, 1);

	platform_device_register(&tegra_sdhci_device1);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device4);

	return 0;
}
