/*
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
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

#ifndef _TEGRA_CLUSTER_CONTROL_H_
#define _TEGRA_CLUSTER_CONTROL_H_

#include <linux/notifier.h>
#include <linux/clk.h>

enum cluster {
	FAST_CLUSTER,
	SLOW_CLUSTER,
};

#if defined(CONFIG_TEGRA_HMP_CLUSTER_CONTROL) ||\
	defined(CONFIG_TEGRA_CLUSTER_CONTROL)

#define TEGRA_CLUSTER_PRE_SWITCH	1
#define TEGRA_CLUSTER_POST_SWITCH	2

/*
 * Register and unregister cluster switch notifiers.
 * A PRE_SWITCH notifier will fire before the cluster switch happens
 * and a POST_SWITCH notifier will fire after the cluster switch happens.
 */
int register_cluster_switch_notifier(struct notifier_block *notifier);
int unregister_cluster_switch_notifier(struct notifier_block *notifier);

static inline unsigned int is_lp_cluster(void)
{
	unsigned int reg;
#ifdef CONFIG_ARM64
	asm("mrs	%0, mpidr_el1\n"
	    "ubfx	%0, %0, #8, #4"
	    : "=r" (reg)
	    :
	    : "cc", "memory");
#else
	asm("mrc	p15, 0, %0, c0, c0, 5\n"
	    "ubfx	%0, %0, #8, #4"
	    : "=r" (reg)
	    :
	    : "cc", "memory");
#endif
	return reg & 1; /* 0 == G, 1 == LP*/
}
int tegra_cluster_control(unsigned int us, unsigned int flags);
int tegra_cluster_switch(struct clk *cpu_clk, struct clk *new_cluster_clk);
void tegra_cluster_switch_prolog(unsigned int flags);
void tegra_cluster_switch_epilog(unsigned int flags);
#else
static inline unsigned int is_lp_cluster(void)  { return 0; }
static inline int tegra_cluster_control(unsigned int us, unsigned int flags)
{
	return -EPERM;
}
static inline int tegra_cluster_switch(struct clk *cpu_clk,
				       struct clk *new_cluster_clk)
{
	return -EPERM;
}
static inline int register_cluster_switch_notifier(
					struct notifier_block *notifier)
{
	return -EPERM;
}
static inline int unregister_cluster_switch_notifier(
					struct notifier_block *notifier)
{
	return -EPERM;
}
static inline void tegra_cluster_switch_prolog(unsigned int flags) {}
static inline void tegra_cluster_switch_epilog(unsigned int flags) {}
#endif

#endif
