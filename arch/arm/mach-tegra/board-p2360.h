/*
 * arch/arm/mach-tegra/board-p2360.h
 * Based on arch/arm/mach-tegra/board-p1859.h
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_P2360_H
#define _MACH_TEGRA_BOARD_P2360_H

#include <mach/irqs.h>
#include <linux/mfd/max77663-core.h>
#include "gpio-names.h"

int p2360_regulator_init(void);

/* Tegra GPIOs */
#define TEGRA_GPIO_GPUPWR		TEGRA_GPIO_PR2

/* External peripheral act as gpio */
#define MAX77663_IRQ_BASE		TEGRA_NR_IRQS
#define MAX77663_IRQ_END		(MAX77663_IRQ_BASE + MAX77663_IRQ_NR)
#define MAX77663_GPIO_BASE		TEGRA_NR_GPIOS
#define MAX77663_GPIO_END		(MAX77663_GPIO_BASE + MAX77663_GPIO_NR)

#endif
