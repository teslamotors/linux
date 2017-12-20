/* compat26/tegra26/devices.h
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Re implementation of arch/arm/mach-tegra/board.h
*/

#include <linux/nvhost.h>

extern struct platform_device tegra_grhost_device;
extern struct platform_device tegra_nvmap_device;

extern struct nvhost_device tegra_disp1_device;
extern struct nvhost_device p1852_disp2_device;
