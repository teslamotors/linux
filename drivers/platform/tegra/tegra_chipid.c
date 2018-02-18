/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/tegra-soc.h>
#include <linux/platform/tegra/common.h>

#define MISCREG_HIDREV		0x4

static struct of_device_id tegra_chipid_of_match[] = {
	{ .compatible = "nvidia,tegra186-chipid" },
	{}
};

void tegra_get_tegraid_from_hw(void)
{
	struct device_node *np;
	u32 cid;
	char *priv = NULL;
	struct resource r;
	void __iomem *chipid_base;
	u32 offset = MISCREG_HIDREV;
	u32 opt_subrevision;
	char prime;

	np = of_find_matching_node(NULL, tegra_chipid_of_match);
	BUG_ON(!np);

	if (of_address_to_resource(np, 0, &r))
		BUG_ON("tegra-id: failed to resolve base address\n");

	chipid_base = of_iomap(np, 0);
	BUG_ON(!chipid_base);

	of_property_read_u32(np, "offset", &offset);
	if (r.start + offset > r.end)
		BUG_ON("tegra-id: invalid chipid offset\n");

	cid = readl(chipid_base + offset);

	pr_info("tegra-id: chipid=%x.\n", cid);

	opt_subrevision = tegra_get_fuse_opt_subrevision();
	if (opt_subrevision == 1) {
		prime = 'p';
		priv = &prime;
	} else if (opt_subrevision == 2) {
		prime = 'q';
		priv = &prime;
	} else if (opt_subrevision == 3) {
		prime = 'r';
		priv = &prime;
	}
	pr_info("tegra-id: opt_subrevision=%x.\n", opt_subrevision);

	tegra_set_tegraid((cid >> 8) & 0xff,
					  (cid >> 4) & 0xf,
					  (cid >> 16) & 0xf,
					  0,
					  0,
					  priv);
}
