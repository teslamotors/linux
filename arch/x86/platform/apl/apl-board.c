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

/* Ideally, pin mappings should be provided by ACPI */
static const struct pinctrl_map uart2_mappings[] __initconst = {
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_INIT, "INT3452:00",
			  "uart2_in_gpio_grp", "uart2_gpio"),
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_DEFAULT, "INT3452:00",
			  "uart2_in_uart_grp", "uart2_uart"),
	PIN_MAP_MUX_GROUP("dw-apb-uart.10", PINCTRL_STATE_SLEEP, "INT3452:00",
			  "uart2_in_gpio_grp", "uart2_gpio"),
};

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
		.bus_num = 1,
		.chip_select = 2,
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
		.bus_num = 2,
		.chip_select = 2,
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
	if (!spi_register_board_info
			(apl_spi_slaves, ARRAY_SIZE(apl_spi_slaves))) {
		ret = 0;
		pr_warn("\nfailed to register the SPI slaves...\n");
	} else {
		pr_debug("\nsuccessfully registered the SPI slaves...\n");
	}
	return ret;
}

static int __init apl_board_init(void)
{
	int ret;

	pr_debug("\nregistering APL SPI devices...\n");
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
