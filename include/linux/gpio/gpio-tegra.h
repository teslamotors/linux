/*
 * TEGRA GPIO driver header.
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_GPIO_TEGRA_H
#define __LINUX_GPIO_TEGRA_H

#ifdef CONFIG_PLATFORM_TEGRA
int tegra_gpio_get_bank_int_nr(int gpio);
int tegra_gpio_is_enabled(int gpio, int *is_gpio, int *is_input);
#else
static inline int tegra_gpio_is_enabled(int gpio, int *is_gpio, int *is_input)
{
	return 0;
}

static inline int tegra_gpio_get_bank_int_nr(int gpio)
{
	return 0;
}
#endif

#endif /* __LINUX_GPIO_TEGRA_H */
