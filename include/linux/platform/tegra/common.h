/*
 * Copyright (C) 2011-2015, NVIDIA Corporation. All rights reserved.
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

#ifndef __MACH_TEGRA_COMMON_H
#define __MACH_TEGRA_COMMON_H

extern struct smp_operations tegra_smp_ops;

extern phys_addr_t tegra_tsec_start;
extern phys_addr_t tegra_tsec_size;

#ifdef CONFIG_CACHE_L2X0
void tegra_init_cache(bool init);
#else
static inline void tegra_init_cache(bool init) {}
#endif

extern void tegra_cpu_die(unsigned int cpu);
extern int tegra_cpu_kill(unsigned int cpu);
extern phys_addr_t tegra_avp_kernel_start;
extern phys_addr_t tegra_avp_kernel_size;
void ahb_gizmo_writel(unsigned long val, void __iomem *reg);

extern struct device tegra_generic_cma_dev;
extern struct device tegra_vpr_cma_dev;
extern int tegra_with_secure_firmware;

extern struct device tegra_generic_dev;
extern struct device tegra_vpr_dev;
extern struct device tegra_iram_dev;
extern struct dma_resize_notifier_ops vpr_dev_ops;

u32 tegra_get_sku_id(void);
u32 tegra_get_chip_id(void);
u32 tegra_get_bct_strapping(void);
u32 tegra_get_fuse_opt_subrevision(void);
enum tegra_revision tegra_chip_get_revision(void);
void __init display_tegra_dt_info(void);

void tegra_register_idle_unidle(int (*gk20a_do_idle)(void),
				int (*gk20a_do_unidle)(void));
void tegra_unregister_idle_unidle(void);

static inline int tegra_cpu_is_secure(void)
{
	return tegra_with_secure_firmware;
}

int tegra_state_idx_from_name(char *state_name);
#endif
