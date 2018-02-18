/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/host1x.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <linux/nvhost_isp_ioctl.h>

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/platform/tegra/mc.h>
#else
#include <soc/tegra/pmc.h>
#endif
#include "drm.h"

#define ISP_NUM_SYNCPTS 4

struct isp_config {
	u32 class_id;
	int powergate_id;
	const char *ctrl_node_name;
};

struct isp {
	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct iommu_domain *domain;
	struct device *dev;
	struct clk *clk;
	struct clk *emc_clk;
	struct reset_control *rst;

	/* ctrl node config */
	dev_t devno;
	struct class *class;
	struct device *ctrl;
	struct cdev cdev;

	/* Platform configuration */
	const struct isp_config *config;
};

static const struct file_operations tegra_isp_ctrl_ops;
static inline struct isp *to_isp(struct tegra_drm_client *client)
{
	return container_of(client, struct isp, client);
}


static int isp_power_on(struct device *dev)
{
	struct isp *isp = dev_get_drvdata(dev);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	int err;

	err = tegra_unpowergate_partition(isp->config->powergate_id);
	if (err)
		return err;

	err = clk_prepare_enable(isp->clk);
	if (err < 0) {
		dev_err(dev, "failed to enable isp clk\n");
		goto isp_enable_fail;
	}
	err = clk_prepare_enable(isp->emc_clk);
	if (err < 0) {
		dev_err(dev, "failed to enable isp emc clk\n");
		goto emc_enable_fail;
	}

	return 0;

emc_enable_fail:
	clk_disable_unprepare(isp->clk);
isp_enable_fail:
	tegra_powergate_partition(isp->config->powergate_id);

	return err;
#else
	return tegra_powergate_sequence_power_up(isp->config->powergate_id,
						 isp->clk, isp->rst);
#endif
}

static int isp_power_off(struct device *dev)
{
	struct isp *isp = dev_get_drvdata(dev);

	clk_disable_unprepare(isp->emc_clk);
	clk_disable_unprepare(isp->clk);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_partition(isp->config->powergate_id);

	return 0;
#else
	int err;

	err = reset_control_assert(isp->rst);
	if (err)
		return err;

	return tegra_powergate_power_off(isp->config->powergate_id);
#endif
}

static int isp_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	unsigned long flags = HOST1X_SYNCPT_HAS_BASE;
	struct isp *isp = to_isp(drm);
	unsigned int i;
	int err;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, isp->dev);
		if (err < 0) {
			dev_err(client->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		isp->domain = tegra->domain;
	}

	isp->channel = host1x_channel_request(client->dev);
	if (!isp->channel) {
		err = -ENOMEM;
		goto error_iommu_detach_device;
	}

	for (i = 0; i < ISP_NUM_SYNCPTS; i++) {
		client->syncpts[i] = host1x_syncpt_request(client->dev, flags);
		if (!client->syncpts[i]) {
			err = -ENOMEM;
			goto error_host1x_syncpt_free;
		}
	}

	err = tegra_drm_register_client(tegra, drm);
	if (err)
		goto error_host1x_syncpt_free;

	cdev_init(&isp->cdev, &tegra_isp_ctrl_ops);
	isp->cdev.owner = THIS_MODULE;

	isp->class = class_create(THIS_MODULE, isp->config->ctrl_node_name);
	if (IS_ERR(isp->class)) {
		err = PTR_ERR(isp->class);
		goto err_create_class;
	}

	err = alloc_chrdev_region(&isp->devno, 0, 1,
				  isp->config->ctrl_node_name);
	if (err < 0)
		goto err_alloc_chrdev_region;

	err = cdev_add(&isp->cdev, isp->devno, 1);
	if (err < 0)
		goto err_add_cdev;

	isp->ctrl = device_create(isp->class, NULL, isp->devno, NULL,
				  isp->config->ctrl_node_name);
	if (IS_ERR(isp->ctrl)) {
		err = PTR_ERR(isp->ctrl);
		goto err_create_device;
	}

	return 0;

err_create_device:
	cdev_del(&isp->cdev);
err_add_cdev:
	unregister_chrdev_region(isp->devno, 1);
err_alloc_chrdev_region:
	class_destroy(isp->class);
err_create_class:
	tegra_drm_unregister_client(tegra, drm);
error_host1x_syncpt_free:
	for (i = 0; i < ISP_NUM_SYNCPTS; i++)
		if (client->syncpts[i]) {
			host1x_syncpt_free(client->syncpts[i]);
			client->syncpts[i] = NULL;
		}
	host1x_channel_free(isp->channel);
error_iommu_detach_device:
	if (isp->domain) {
		iommu_detach_device(isp->domain, isp->dev);
		isp->domain = NULL;
	}
	return err;
}

static int isp_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct isp *isp = to_isp(drm);
	unsigned int i;
	int err;

	err = tegra_drm_unregister_client(tegra, drm);

	for (i = 0; i < ISP_NUM_SYNCPTS; i++)
		if (client->syncpts[i]) {
			host1x_syncpt_free(client->syncpts[i]);
			client->syncpts[i] = NULL;
		}

	host1x_channel_free(isp->channel);

	if (isp->domain) {
		iommu_detach_device(isp->domain, isp->dev);
		isp->domain = NULL;
	}

	return 0;
}

static int isp_get_clk_rate(struct host1x_client *client, u64 *data, u32 type)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct isp *isp = to_isp(drm);

	switch (type) {
	case DRM_TEGRA_REQ_TYPE_CLK_KHZ:
		*data = clk_get_rate(isp->clk);
		break;
	case DRM_TEGRA_REQ_TYPE_BW_KBPS:
		*data = clk_get_rate(isp->emc_clk);
		break;
	default:
		dev_err(isp->dev, "Unknown Clock request type\n");
		return -EINVAL;
	}
	return 0;
}

static int isp_set_clk_rate(struct host1x_client *client, u64 data, u32 type)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct isp *isp = to_isp(drm);

	switch (type) {
	case DRM_TEGRA_REQ_TYPE_CLK_KHZ:
		return clk_set_rate(isp->clk, data);
	case DRM_TEGRA_REQ_TYPE_BW_KBPS:
		return clk_set_rate(isp->emc_clk,
				tegra_emc_bw_to_freq_req(data) * 1000);
	default:
		dev_err(isp->dev, "Unknown Clock request type\n");
		return -EINVAL;
	}
}

static const struct host1x_client_ops isp_client_ops = {
	.init = isp_init,
	.exit = isp_exit,
	.set_clk_rate = isp_set_clk_rate,
	.get_clk_rate = isp_get_clk_rate,
};

static int isp_open_channel(struct tegra_drm_client *client,
			     struct tegra_drm_context *context)
{
	struct isp *isp = to_isp(client);
	int err;

	err = pm_runtime_get_sync(isp->dev);
	if (err < 0)
		return err;

	context->channel = host1x_channel_get(isp->channel);
	if (!context->channel) {
		pm_runtime_put(isp->dev);
		return -ENOMEM;
	}

	return 0;
}

static void isp_close_channel(struct tegra_drm_context *context)
{
	struct isp *isp;

	if (!context->channel)
		return;

	host1x_channel_put(context->channel);
	isp = to_isp(context->client);
	pm_runtime_put(isp->dev);
}

static const struct tegra_drm_client_ops isp_ops = {
	.open_channel = isp_open_channel,
	.close_channel = isp_close_channel,
	.submit = tegra_drm_submit,
};

static long isp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != NVHOST_ISP_IOCTL_MAGIC)
		return -EFAULT;

	return 0;
}

static int isp_open(struct inode *inode, struct file *file)
{
	struct isp *isp = container_of(inode->i_cdev, struct isp, cdev);

	file->private_data = isp;

	return 0;
}

static int isp_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tegra_isp_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = isp_open,
	.unlocked_ioctl = isp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = isp_ioctl,
#endif
	.release = isp_release,
};

static const struct isp_config ispa_t210_config = {
	.class_id = HOST1X_CLASS_ISPA,
	.powergate_id = TEGRA_POWERGATE_VENC,
	.ctrl_node_name = "nvhost-ctrl-isp",
};

static const struct isp_config ispb_t210_config = {
	.class_id = HOST1X_CLASS_ISPB,
#ifdef TEGRA_POWERGATE_VE2
	.powergate_id = TEGRA_POWERGATE_VE2,
#endif
	.ctrl_node_name = "nvhost-ctrl-isp.1",
};

static const struct of_device_id ispa_match[] = {
	{ .compatible = "nvidia,tegra210-isp", .data = &ispa_t210_config },
	{ },
};

static const struct of_device_id ispb_match[] = {
	{ .compatible = "nvidia,tegra210-isp", .data = &ispb_t210_config },
	{ },
};

static int isp_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct isp_config *isp_config;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	unsigned int dev_id;
	struct isp *isp;
	int err;
	char devname[64];

	if (sscanf(pdev->name, "isp.%1u", &dev_id) != 1)
		return -EINVAL;

	if (dev_id == 0)
		match = of_match_node(ispa_match, pdev->dev.of_node);
	else if (dev_id == 1)
		match = of_match_node(ispb_match, pdev->dev.of_node);
	else {
		dev_err(dev, "bad isp alias id %u\n", dev_id);
		return -EINVAL;
	}

	if (!match)
		return -ENXIO;

	isp_config = match->data;

	isp = devm_kzalloc(dev, sizeof(*isp), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, ISP_NUM_SYNCPTS * sizeof(*syncpts),
			       GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "failed to get registers\n");
		return -ENXIO;
	}

	isp->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(isp->regs))
		return PTR_ERR(isp->regs);

	snprintf(devname, sizeof(devname),
		 (dev_id < 0) ? "tegra_isp" : "tegra_isp.%d",dev_id);

	if (IS_ENABLED(CONFIG_COMMON_CLK))
		isp->clk = devm_clk_get(dev, "isp");
	else
		isp->clk = clk_get_sys(devname, "isp");
	if (IS_ERR(isp->clk)) {
		dev_err(dev, "failed to get isp clock\n");
		return PTR_ERR(isp->clk);
	}
	if (IS_ENABLED(CONFIG_COMMON_CLK))
		isp->emc_clk = devm_clk_get(dev, "emc");
	else
		isp->emc_clk = clk_get_sys(devname, "emc");
	if (IS_ERR(isp->emc_clk)) {
		dev_err(dev, "failed to get emc clock\n");
		return PTR_ERR(isp->emc_clk);
	}

	isp->rst = devm_reset_control_get(dev, "isp");
	if (IS_ERR(isp->rst)) {
		dev_err(dev, "cannot get reset\n");
		return PTR_ERR(isp->rst);
	}

	INIT_LIST_HEAD(&isp->client.base.list);
	isp->client.base.ops = &isp_client_ops;
	isp->client.base.dev = dev;
	isp->client.base.class = isp_config->class_id;
	isp->client.base.syncpts = syncpts;
	isp->client.base.num_syncpts = ISP_NUM_SYNCPTS;
	isp->dev = dev;
	isp->config = isp_config;

	INIT_LIST_HEAD(&isp->client.list);
	isp->client.ops = &isp_ops;

	platform_set_drvdata(pdev, isp);

	err = host1x_client_register(&isp->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	pm_runtime_enable(dev);
	if (!pm_runtime_enabled(dev)) {
		err = isp_power_on(dev);
		if (err) {
			dev_err(dev, "cannot power on device\n");
			goto unregister_client;
		}
	}

	dev_info(dev, "initialized");

	return 0;

unregister_client:
	host1x_client_unregister(&isp->client.base);

	return err;
}

static int isp_remove(struct platform_device *pdev)
{
	struct isp *isp = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&isp->client.base);
	if (err < 0)
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		isp_power_off(&pdev->dev);

	return err;
}

static int __maybe_unused isp_runtime_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return isp_power_on(dev);
}

static int __maybe_unused isp_runtime_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return isp_power_off(dev);
}

static const struct dev_pm_ops isp_pm_ops = {
	SET_RUNTIME_PM_OPS(isp_runtime_suspend, isp_runtime_resume, NULL)
};

struct platform_driver tegra_isp_driver = {
	.driver = {
		.name = "tegra-isp",
		.of_match_table = ispa_match,
		.pm = &isp_pm_ops,
	},
	.probe = isp_probe,
	.remove = isp_remove,
};
