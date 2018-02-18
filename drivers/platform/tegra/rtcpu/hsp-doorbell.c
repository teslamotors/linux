/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION. All rights reserved.
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
#include <linux/slab.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/of_platform.h>
#include <linux/mailbox_controller.h>
#include <linux/tegra_ast.h>
#include <linux/tegra-ivc-bus.h>

#define NV(p) "nvidia," #p

#define TEGRA_IVC_BUS_HSP_DATA_ARRAY_SIZE	3

struct tegra_hsp_doorbell_priv {
	struct platform_device *hsp;
	enum tegra_hsp_master master;
	enum tegra_hsp_doorbell doorbell;
};

static void tegra_hsp_doorbell_notify(void *data)
{
	struct device *dev = data;

	tegra_hsp_notify(dev);
}

static int tegra_hsp_doorbell_probe(struct device *dev)
{
	struct tegra_hsp_doorbell_priv *db;
	struct of_phandle_args dbspec;
	int ret;

	ret = of_parse_phandle_with_fixed_args(dev->of_node, NV(hsp-doorbell),
						2, 0, &dbspec);
	if (ret)
		return ret;

	db = devm_kmalloc(dev, sizeof(*db), GFP_KERNEL);
	if (unlikely(db == NULL)) {
		of_node_put(dbspec.np);
		return -ENOMEM;
	}

	dev_set_drvdata(dev, db);
	db->hsp = of_find_device_by_node(dbspec.np);
	db->master = dbspec.args[0];
	db->doorbell = dbspec.args[1];
	of_node_put(dbspec.np);

	if (db->hsp == NULL)
		return -EPROBE_DEFER;

	ret = sysfs_create_link(&dev->kobj, &db->hsp->dev.kobj, "hsp");
	if (ret)
		goto error;

	/* Register for notifications from remote processor */
	ret = tegra_hsp_db_add_handler(db->master, tegra_hsp_doorbell_notify,
					dev);
	if (ret) {
		dev_err(dev, "HSP doorbell %s error: %d\n", "register", ret);
		goto error2;
	}

	/* Allow remote processor to ring */
	ret = tegra_hsp_db_enable_master(db->master);
	if (ret) {
		dev_err(dev, "HSP doorbell %s error: %d\n", "enable", ret);
		goto error3;
	}

	return ret;
error3:
	tegra_hsp_db_del_handler(db->master);
error2:
	sysfs_delete_link(&dev->kobj, &db->hsp->dev.kobj, "hsp");
error:
	platform_device_put(db->hsp);
	return ret;
}

static void tegra_hsp_doorbell_remove(struct device *dev)
{
	struct tegra_hsp_doorbell_priv *db = dev_get_drvdata(dev);

	tegra_hsp_db_disable_master(db->master);
	tegra_hsp_db_del_handler(db->master);
	sysfs_delete_link(&dev->kobj, &db->hsp->dev.kobj, "hsp");
	platform_device_put(db->hsp);
}

static void tegra_hsp_doorbell_ring(struct device *dev)
{
	struct tegra_hsp_doorbell_priv *db = dev_get_drvdata(dev);
	int ret = tegra_hsp_db_ring(db->doorbell);
	if (ret)
		dev_err(dev, "HSP doorbell %s error: %d\n", "ring", ret);
}

static const struct of_device_id tegra_hsp_doorbell_of_match[] = {
	{ .compatible = "nvidia,tegra186-hsp-doorbell" },
	{ },
};

static const struct tegra_hsp_ops tegra_hsp_doorbell_ops = {
	.probe		= tegra_hsp_doorbell_probe,
	.remove		= tegra_hsp_doorbell_remove,
	.ring		= tegra_hsp_doorbell_ring,
};

static struct tegra_ivc_driver tegra_hsp_doorbell_driver = {
	.driver = {
		.name		= "tegra-hsp-doorbell",
		.bus		= &tegra_ivc_bus_type,
		.owner		= THIS_MODULE,
		.of_match_table	= tegra_hsp_doorbell_of_match,
	},
	.dev_type	= &tegra_hsp_type,
	.ops.hsp	= &tegra_hsp_doorbell_ops,
};
tegra_ivc_module_driver(tegra_hsp_doorbell_driver);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra HSP doorbell driver");
MODULE_LICENSE("GPL");
