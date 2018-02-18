/*
 * Copyright (c) 2016 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc-bus.h>

static u32 tegra_hsp_mailbox_notify(void *data, u32 value)
{
	struct device *dev = data;

	tegra_hsp_notify(dev);
	return 0;
}

static int tegra_hsp_mailbox_probe(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair;

	pair = of_tegra_hsp_sm_pair_by_name(dev->of_node, "ivc-pair",
					tegra_hsp_mailbox_notify, NULL, dev);
	if (IS_ERR(pair))
		return PTR_ERR(pair);

	dev_set_drvdata(dev, pair);
	return 0;
}

static void tegra_hsp_mailbox_remove(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair = dev_get_drvdata(dev);

	tegra_hsp_sm_pair_free(pair);
}

static void tegra_hsp_mailbox_ring(struct device *dev)
{
	struct tegra_hsp_sm_pair *pair = dev_get_drvdata(dev);

	tegra_hsp_sm_pair_write(pair, 1);
}

static const struct of_device_id tegra_hsp_mailbox_of_match[] = {
	{ .compatible = "nvidia,tegra186-hsp-mailbox" },
	{ },
};

static const struct tegra_hsp_ops tegra_hsp_mailbox_ops = {
	.probe		= tegra_hsp_mailbox_probe,
	.remove		= tegra_hsp_mailbox_remove,
	.ring		= tegra_hsp_mailbox_ring,
};

static struct tegra_ivc_driver tegra_hsp_mailbox_driver = {
	.driver = {
		.name		= "tegra-hsp-mailbox",
		.bus		= &tegra_ivc_bus_type,
		.owner		= THIS_MODULE,
		.of_match_table	= tegra_hsp_mailbox_of_match,
	},
	.dev_type	= &tegra_hsp_type,
	.ops.hsp	= &tegra_hsp_mailbox_ops,
};
tegra_ivc_module_driver(tegra_hsp_mailbox_driver);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra HSP shared mailbox driver");
MODULE_LICENSE("GPL");
