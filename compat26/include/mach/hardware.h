/* compat26/include/mach/hardware.h
 *
 * 2.6 compatbility layer includes
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * 
*/

#ifndef __COMPAT26_MACH_H
#define __COMPAT26_MACH_H __FILE__

#include <soc/tegra/fuse.h>

enum tegra_chipid {
        TEGRA_CHIPID_UNKNOWN = 0,
        TEGRA_CHIPID_TEGRA2 = TEGRA20,
        TEGRA_CHIPID_TEGRA3 = TEGRA30,
};

static inline enum tegra_chipid tegra_get_chipid(void)
{
	return tegra_read_chipid() >> 8 & 0xff;
}

static inline enum tegra_revision tegra_get_revision(void)
{
	return tegra_sku_info.revision;
}
#endif
