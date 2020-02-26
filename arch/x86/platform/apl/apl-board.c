/*
 * Copyright (c) 2016, Intel Corporation
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/spi/spidev.h>
#include <linux/spi/spi.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/pwm.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/kernel.h>
#include <linux/reboot.h>

/* Ideally, pin mappings should be provided by ACPI */
static const struct pinctrl_map uart2_mappings[] __initconst = {
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_INIT, "INT3452:00",
			  "uart2_in_gpio_grp", "uart2_gpio"),
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_DEFAULT, "INT3452:00",
			  "uart2_in_uart_grp", "uart2_uart"),
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_SLEEP, "INT3452:00",
			  "uart2_in_gpio_grp", "uart2_gpio"),
};

#define DRVNAME "apl-board"

static struct pxa2xx_spi_chip chip_data = {
	.gpio_cs = -EINVAL,
	.dma_burst_size = 1,
	.pio_dma_threshold = 8,
};

static struct spi_board_info apl_spi_slaves[] = {
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.chip_select = 0,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.chip_select = 1,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 2,
		.chip_select = 0,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 2,
		.chip_select = 1,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 3,
		.chip_select = 0,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 3,
		.chip_select = 1,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	},
	{
		.modalias = "spidev",
		.max_speed_hz = 50000000,
		.bus_num = 3,
		.chip_select = 2,
		.controller_data = &chip_data,
		.mode = SPI_MODE_0,
	}
};

static int apl_spi_board_setup(void)
{
	int ret = -1;

	/* Register the SPI devices */
	ret = spi_register_board_info(apl_spi_slaves,
				ARRAY_SIZE(apl_spi_slaves));
	if (ret)
		pr_warn(DRVNAME ": failed to register the SPI slaves...\n");
	else
		pr_debug(DRVNAME ": successfully registered the SPI slaves...\n");
	return ret;
}

/* WARM reset hook */
static int warm_reset_hook_panic_notifier_call(struct notifier_block *notifier,
		unsigned long what, void *data)
{
	(void) notifier;
	(void) what;
	(void) data;

	// WARM reset with PCI type
	reboot_mode = REBOOT_WARM;
	reboot_type = BOOT_CF9_FORCE;

	return NOTIFY_DONE;
}

static struct notifier_block warm_reset_hook_panic_notifier = {
	.notifier_call = warm_reset_hook_panic_notifier_call,
};

static int __init apl_board_init(void)
{
	int ret;

	/* Register WARM reset hook for ramoops with ABL */
	atomic_notifier_chain_register(&panic_notifier_list,
			&warm_reset_hook_panic_notifier);

	pr_debug(DRVNAME ": registering APL SPI devices...\n");
	ret = apl_spi_board_setup();
	if (ret)
		goto exit;

exit:
	return ret;
}
arch_initcall(apl_board_init);

static int __init uart2_init(void)
{
	return pinctrl_register_mappings(uart2_mappings,
					 ARRAY_SIZE(uart2_mappings));
}
postcore_initcall(uart2_init);

MODULE_LICENSE("GPL v2");
