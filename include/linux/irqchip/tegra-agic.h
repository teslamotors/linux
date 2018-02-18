/*
 * include/linux/irqchip/tegra-agic.h
 *
 * Header file for managing AGIC interrupt controller
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#ifndef _TEGRA_AGIC_H_
#define _TEGRA_AGIC_H_

#define MAX_GIC_NR	2

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
#include <linux/irqchip/tegra-t18x-agic.h>
#else
#include <linux/irqchip/tegra-t210-agic.h>
#endif /* CONFIG_ARCH_TEGRA_18x_SOC */

#ifdef CONFIG_TEGRA_APE_AGIC
extern int tegra_agic_route_interrupt(int irq, enum tegra_agic_cpu cpu);
extern bool tegra_agic_irq_is_active(int irq);
extern bool tegra_agic_irq_is_pending(int irq);

extern void tegra_agic_save_registers(void);
extern void tegra_agic_restore_registers(void);
extern void tegra_agic_clear_pending(int irq);
extern void tegra_agic_clear_active(int irq);
#else
static inline int tegra_agic_route_interrupt(int irq, enum tegra_agic_cpu cpu)
{
	return -EINVAL;
}

static inline bool tegra_agic_irq_is_active(int irq)
{
	return false;
}

static inline bool tegra_agic_irq_is_pending(int irq)
{
	return true;
}

static inline void tegra_agic_save_registers(void)
{
	return;
}

static inline void tegra_agic_restore_registers(void)
{
	return;
}

static inline void tegra_agic_clear_pending(int irq)
{
	return;
}

static inline void tegra_agic_clear_active(int irq)
{
	return;
}
#endif /* CONFIG_TEGRA_APE_AGIC */

#endif /* _TEGRA_AGIC_H_ */
