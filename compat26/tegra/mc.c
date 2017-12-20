/* compat26/tegra/mc.c
 *
 * Copyright 2016 Codethink Ltd.
*/

#include <linux/kernel.h>
#include <linux/io.h>

#include <mach/mc.h>
#include <soc/tegra/mc.h>

#include "../tegra26/tegra3_emc.h"

/* note, mc base is 0x7000F000 */

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
void tegra_mc_set_priority(unsigned long client, unsigned long prio)
{
	tegra20_mc_set_priority(client, prio);
}

/* for tegra2 this is always 1 */
int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	return 1;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
/* tegra_mc_set_priority is macro-d out in mach/mc.h */

/* constants from 2.6 */
#define EMC_FBIO_CFG5		0x104
#define EMC_CFG5_TYPE_SHIFT	(0)

/* rewritten from original 2.6 source */
static int read_dram_type(void)
{
	static void __iomem *emc_base;
	unsigned int dram_type;

	if (!emc_base)
		emc_base = ioremap(0x7000F400, 1024);
	if (!emc_base)
		return -1;

	dram_type = readl_relaxed(emc_base + EMC_FBIO_CFG5);
	dram_type >>= EMC_CFG5_TYPE_SHIFT;
	return dram_type & 3;
}

int tegra_emc_get_dram_type(void)
{
	static bool got_dram_type;
	static int dram_type;

	if (!got_dram_type) {
		dram_type = read_dram_type();
		got_dram_type = true;
	}

	return dram_type;
}
EXPORT_SYMBOL(tegra_emc_get_dram_type);

int tegra_mc_get_tiled_memory_bandwidth_multiplier(void)
{
	int type = tegra_emc_get_dram_type();
	WARN_ONCE(type == -1, "unknown type DRAM because DVFS is disabled\n");

	return (type == DRAM_TYPE_DDR3) ? 2 : 1;
}
EXPORT_SYMBOL(tegra_mc_get_tiled_memory_bandwidth_multiplier);
#endif

	
