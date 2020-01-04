/*
 * arch/arm/mach-tegra/include/mach/spi_i2s.h
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


#ifndef __ARCH_ARM_MACH_TEGRA_SPI_I2S_H
#define __ARCH_ARM_MACH_TEGRA_SPI_I2S_H

struct tegra_spi_i2s_gpio {
	unsigned int gpio_no;
	unsigned int  active_state;
};

struct tegra_spi_i2s_platform_data {
	struct tegra_spi_i2s_gpio gpio_spi;
	struct tegra_spi_i2s_gpio gpio_i2s;
	unsigned int spi_i2s_timeout_ms;
};

#endif /* __ARCH_ARM_MACH_TEGRA_SPI_I2S_H */
