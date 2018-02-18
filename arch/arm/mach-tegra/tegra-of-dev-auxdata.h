/*
 * Define the OF_DEV_AUXDATA for different Tegra controllers.
 *
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
 */

#ifndef MACH_TEGRA_OF_DEV_AUXDATA_H__
#define MACH_TEGRA_OF_DEV_AUXDATA_H__

#ifdef CONFIG_USE_OF

#define T114_SPI_OF_DEV_AUXDATA	\
	OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d400, "spi-tegra114.0", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d600, "spi-tegra114.1", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000d800, "spi-tegra114.2", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000da00, "spi-tegra114.3", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000dc00, "spi-tegra114.4", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra114-spi", 0x7000de00, "spi-tegra114.5", NULL)

#define T124_SPI_OF_DEV_AUXDATA	\
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000d400, "spi-tegra114.0", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000d600, "spi-tegra114.1", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000d800, "spi-tegra114.2", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000da00, "spi-tegra114.3", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000dc00, "spi-tegra114.4", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi", 0x7000de00, \
				"spi-tegra114.5", NULL),  \
		/* slave */ \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000d400, \
				"spi-tegra114-slave.0", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000d600, \
				"spi-tegra114-slave.1", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000d800, \
				"spi-tegra114-slave.2", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000da00, \
				"spi-tegra114-slave.3", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000dc00, \
				"spi-tegra114-slave.4", NULL),  \
		OF_DEV_AUXDATA("nvidia,tegra124-spi-slave", 0x7000de00, \
				"spi-tegra114-slave.5", NULL)

#define T124_SDMMC_OF_DEV_AUXDATA \
        OF_DEV_AUXDATA("nvidia,tegra124-sdhci", 0x700b0600, "sdhci-tegra.3", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-sdhci", 0x700b0400, "sdhci-tegra.2", NULL),  \
        OF_DEV_AUXDATA("nvidia,tegra124-sdhci", 0x700b0000, "sdhci-tegra.0", NULL)


#endif

#endif /* MACH_TEGRA_OF_DEV_AUXDATA_H__ */
