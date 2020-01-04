/*
 * arch/arm/mach-tegra/board-p852-sku13.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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

#include "board-p852.h"

static inline void p852_sku13_spi_init(void)
{
	p852_sku_peripherals |= P852_SKU_SPI_ENABLE;
	p852_spi_peripherals |=
	    ((P852_SPI_MASTER | P852_SPI_ENABLE) << P852_SPI1_SHIFT) |
	    ((P852_SPI_SLAVE | P852_SPI_ENABLE) << P852_SPI2_SHIFT) |
	    ((P852_SPI_SLAVE | P852_SPI_ENABLE) << P852_SPI3_SHIFT) |
	    ((P852_SPI_MASTER | P852_SPI_ENABLE) << P852_SPI4_SHIFT);

}

static inline void p852_sku13_i2s_init(void)
{
	p852_sku_peripherals |= P852_SKU_I2S_ENABLE;
	p852_i2s_peripherals |= (P852_I2S_ENABLE << P852_I2S1_SHIFT) |
	    (P852_I2S_ENABLE << P852_I2S2_SHIFT);
}

static inline void p852_sku13_sdhci_init(void)
{
	p852_sku_peripherals |= P852_SKU_SDHCI_ENABLE;
	p852_sdhci_peripherals |=
	    ((P852_SDHCI_ENABLE | P852_SDHCI_CD_EN | P852_SDHCI_WP_EN)
	     << P852_SDHCI1_SHIFT);

	p852_sdhci_platform_data[0].bus_width = 4;
	p852_sdhci_platform_data[2].bus_width = 4;
	p852_sdhci_platform_data[0].cd_gpio = TEGRA_GPIO_PV0;
	p852_sdhci_platform_data[0].wp_gpio = TEGRA_GPIO_PV1;
}

static inline void p852_sku13_uart_init(void)
{
	p852_sku_peripherals |= P852_SKU_UART_ENABLE;
	p852_uart_peripherals |=
	    ((P852_UART_ENABLE | P852_UART_DB) << P852_UARTA_SHIFT) |
	    ((P852_UART_ENABLE | P852_UART_HS) << P852_UARTB_SHIFT) |
	    ((P852_UART_ENABLE) << P852_UARTC_SHIFT);
}

static inline void p852_sku13_display_init(void)
{
	p852_sku_peripherals |= P852_SKU_DISPLAY_ENABLE;
	p852_display_peripherals |=
		(P852_DISP_ENABLE << P852_DISPA_SHIFT);
}

static inline void p852_sku13_i2c_init(void)
{
	p852_sku_peripherals |= P852_SKU_I2C_ENABLE;
	p852_i2c_peripherals |=
	    ((P852_I2C_ENABLE) << P852_I2C1_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C2_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C3_SHIFT) |
	    ((P852_I2C_ENABLE) << P852_I2C4_SHIFT);
	p852_i2c_set_default_clock(0, 40000);
}

static struct tegra_spi_i2s_platform_data spi_i2s_data = {
	.gpio_i2s = {
		.gpio_no = TEGRA_GPIO_PS3,
		.active_state = 0,
	},
	.gpio_spi = {
		.gpio_no =  TEGRA_GPIO_PS4,
		.active_state = 0,
	},
	.spi_i2s_timeout_ms = 25,
};

extern struct spi_board_info tegra_spi_i2s_device;
static inline void p852_sku13_spi_i2s_init(void)
{
	tegra_spi_i2s_device.platform_data = &spi_i2s_data;
	/* cpld_gpio_dir1 and cpld_gpio_dir2*/
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_KBCB, TEGRA_TRI_NORMAL);
	p852_spi_i2s_init();
}

static void __init p852_sku13_init(void)
{
	extern struct tegra_audio_platform_data tegra_i2s_audio_pdata[];

	tegra_i2s_audio_pdata[0].dap_clk = "clk_dev1";
	tegra_i2s_audio_pdata[0].mode = I2S_BIT_FORMAT_RJM;
	tegra_i2s_audio_pdata[1].dap_clk = "clk_dev1";
	tegra_i2s_audio_pdata[1].mode = I2S_BIT_FORMAT_RJM;

	p852_sku_peripherals |= P852_SKU_NAND_ENABLE;

	p852_sku13_spi_init();
	p852_sku13_i2s_init();
	p852_sku13_uart_init();
	p852_sku13_sdhci_init();
	p852_sku13_i2c_init();
	p852_sku13_display_init();

	p852_common_init();
	p852_sku13_spi_i2s_init();
}

MACHINE_START(P852_SKU13, "Tegra P852 SKU13")
	.boot_params  = 0x00000100,
	.phys_io      = IO_APB_PHYS,
	.io_pg_offst  = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.fixup        = tegra_p852_fixup,
	.init_irq     = tegra_init_irq,
	.init_machine = p852_sku13_init,
	.map_io       = tegra_map_common_io,
	.timer = &tegra_timer,
MACHINE_END
