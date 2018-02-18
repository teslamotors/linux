/*
 * NVCSI driver for T186
 *
 * Copyright (c) 2014-2016, NVIDIA Corporation.  All rights reserved.
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

#include <linux/export.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <asm/ioctls.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/nvhost_nvcsi_ioctl.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "dev.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "t186/t186.h"
#include "nvcsi.h"
#include "csi/csi.h"

static struct of_device_id tegra_nvcsi_of_match[] = {
	{ .compatible = "nvidia,tegra186-nvcsi",
		.data = (struct nvhost_device_data *)&t18_nvcsi_info },
	{ },
};

struct nvcsi {
	struct platform_device *pdev;
	struct regulator *regulator;
	struct tegra_csi_device csi;
};

struct nvcsi_private {
	struct platform_device *pdev;
};

int nvcsi_finalize_poweron(struct platform_device *pdev)
{
	struct nvcsi *nvcsi = nvhost_get_private_data(pdev);
	int ret;

	if (nvcsi->regulator) {
		ret = regulator_enable(nvcsi->regulator);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable csi regulator failed.");
			return ret;
		}
	}

	return 0;
}

int nvcsi_prepare_poweroff(struct platform_device *pdev)
{
	struct nvcsi *nvcsi = nvhost_get_private_data(pdev);
	int ret;

	if (nvcsi->regulator) {
		ret = regulator_disable(nvcsi->regulator);
		if (ret)
			dev_err(&pdev->dev, "failed to disabled csi regulator failed.");
	}

	return 0;
}

static int nvcsi_probe_regulator(struct nvcsi *nvcsi)
{
	struct device *dev = &nvcsi->pdev->dev;
	struct regulator *regulator;
	const char *regulator_name;
	int err;

	err = of_property_read_string(dev->of_node, "nvidia,csi_regulator",
				      &regulator_name);
	if (err)
		return err;

	regulator = devm_regulator_get(dev, regulator_name);
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	nvcsi->regulator = regulator;

	return 0;
}

static int nvcsi_probe(struct platform_device *dev)
{
	int err = 0;
	struct nvhost_device_data *pdata = NULL;
	struct nvcsi *nvcsi = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_nvcsi_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else {
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;
	}

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		err = -ENODATA;
		goto err_get_pdata;
	}

	nvcsi = devm_kzalloc(&dev->dev, sizeof(*nvcsi), GFP_KERNEL);
	if (!nvcsi) {
		err = -ENOMEM;
		goto err_alloc_nvcsi;
	}

	nvcsi->pdev = dev;
	pdata->pdev = dev;
	mutex_init(&pdata->lock);
	platform_set_drvdata(dev, pdata);
	pdata->private_data = nvcsi;

	err = nvcsi_probe_regulator(nvcsi);
	if (err)
		dev_info(&dev->dev, "failed to get regulator (%d)\n", err);

	err = nvhost_client_device_get_resources(dev);
	if (err)
		goto err_get_resources;

	err = nvhost_module_init(dev);
	if (err)
		goto err_module_init;

#ifdef CONFIG_PM_GENERIC_DOMAINS
	err = nvhost_module_add_domain(&pdata->pd, dev);
	if (err)
		goto err_add_domain;
#endif

	err = nvhost_client_device_init(dev);
	if (err)
		goto err_client_device_init;

	err = tegra_csi_media_controller_init(&nvcsi->csi, dev);
	if (err < 0)
		goto err_mediacontroller_init;

	return 0;

err_mediacontroller_init:
err_client_device_init:
#ifdef CONFIG_PM_GENERIC_DOMAINS
err_add_domain:
#endif
	nvhost_module_deinit(dev);
err_module_init:
err_get_resources:
err_alloc_nvcsi:
err_get_pdata:

	return err;
}

static int __exit nvcsi_remove(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct nvcsi *nvcsi = (struct nvcsi *)pdata->private_data;

	tegra_csi_media_controller_remove(&nvcsi->csi);

	return 0;
}

static struct platform_driver nvcsi_driver = {
	.probe = nvcsi_probe,
	.remove = __exit_p(nvcsi_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "nvcsi",
#ifdef CONFIG_OF
		.of_match_table = tegra_nvcsi_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	},
};

#ifdef CONFIG_PM_GENERIC_DOMAINS
static struct of_device_id tegra_nvcsi_domain_match[] = {
	{.compatible = "nvidia,tegra186-ve-pd",
	.data = (struct nvhost_device_data *)&t18_nvcsi_info},
	{},
};
#endif

static long nvcsi_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct nvcsi_private *priv = file->private_data;
	struct platform_device *pdev = priv->pdev;

	switch (cmd) {
	case NVHOST_NVCSI_IOCTL_SET_NVCSI_CLK: {
		long rate;

		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		if (get_user(rate, (long __user *)arg))
			return -EFAULT;

		return nvhost_module_set_rate(pdev, priv, rate, 0,
						NVHOST_CLOCK);
	}
	}
	return -ENOIOCTLCMD;
}

static int nvcsi_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata = container_of(inode->i_cdev,
					struct nvhost_device_data, ctrl_cdev);
	struct platform_device *pdev = pdata->pdev;
	struct nvcsi_private *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (unlikely(priv == NULL))
		return -ENOMEM;

	priv->pdev = pdev;

	if (nvhost_module_add_client(pdev, priv)) {
		kfree(priv);
		return -ENOMEM;
	}
	file->private_data = priv;

	return nonseekable_open(inode, file);
}

static int nvcsi_release(struct inode *inode, struct file *file)
{
	struct nvcsi_private *priv = file->private_data;
	struct platform_device *pdev = priv->pdev;

	nvhost_module_remove_client(pdev, priv);
	kfree(priv);
	return 0;
}

const struct file_operations tegra_nvcsi_ctrl_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = nvcsi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvcsi_ioctl,
#endif
	.open = nvcsi_open,
	.release = nvcsi_release,
};

static int __init nvcsi_init(void)
{

#ifdef CONFIG_PM_GENERIC_DOMAINS
	int ret;
	ret = nvhost_domain_init(tegra_nvcsi_domain_match);
	if (ret)
		return ret;
#endif

	return platform_driver_register(&nvcsi_driver);
}

static void __exit nvcsi_exit(void)
{
	platform_driver_unregister(&nvcsi_driver);
}

module_init(nvcsi_init);
module_exit(nvcsi_exit);
