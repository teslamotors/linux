/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef TEGRA186_AOWAKE_H
#define TEGRA186_AOWAKE_H

extern unsigned long tegra_aowake_read(unsigned int reg_offset);
extern int tegra_aowake_write(unsigned long val, unsigned int reg_offset);
extern int tegra_aowake_update(unsigned int reg_offset, unsigned long mask,
		unsigned long val);
#ifdef CONFIG_PM_SLEEP
int pm_irq_init(void);

#else
static inline int pm_irq_init(void)
{
	return 0;
}
#endif
#endif /* TEGRA186_AOWAKE_H */
