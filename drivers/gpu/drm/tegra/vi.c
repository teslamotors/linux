/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/host1x.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/iommu.h>
#include <linux/regulator/consumer.h>
#include <linux/nvhost_vi_ioctl.h>
#include <media/mc_common.h>

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/platform/tegra/mc.h>
#else
#include <soc/tegra/pmc.h>
#endif

#include "drm.h"
#include "vi.h"

#define VI_NUM_SYNCPTS 6
#define VI_CTRL_NODE "nvhost-ctrl-vi"

struct vi_config {
	u32 class_id;
	int powergate_id;
	struct clk **clk;

	int clk_cnt;
};

struct vi {
	struct tegra_drm_client client;

	void __iomem *regs;

	struct host1x_channel *channel;
	struct device *dev;
	struct reset_control *rst;
	struct regulator *reg;

	struct iommu_domain *domain;

	/* control node config */
	struct class *class;
	struct device *ctrl;
	struct cdev cdev;

	/* Platform configuration */
	struct vi_config *config;

	struct tegra_mc_vi mc_vi;
};

static const struct file_operations tegra_vi_ctrl_ops;
static inline struct vi *to_vi(struct tegra_drm_client *client)
{
	return container_of(client, struct vi, client);
}

static inline void vi_writel(struct vi *vi, u32 v, u32 r)
{
	writel(v, vi->regs + r);
}

static int vi_power_off(struct device *dev)
{
	struct vi *vi = dev_get_drvdata(dev);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	int i;

	for (i = 0; i < vi->config->clk_cnt; i++)
		clk_disable_unprepare(vi->config->clk[i]);
	tegra_powergate_partition(vi->config->powergate_id);

	return 0;
#else
	int err;

	err = reset_control_assert(vi->rst);
	if (err)
		return err;

	err = regulator_disable(vi->reg);
	if (err) {
		reset_control_deassert(vi->rst);
		dev_err(dev, "disable csi regulator failed");
		return err;
	}

	clk_disable_unprepare(vi->config->clk[0]);

	return tegra_powergate_power_off(vi->config->powergate_id);
#endif
}

static int vi_power_on(struct device *dev)
{
	struct vi *vi = dev_get_drvdata(dev);
	int err;

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	int i;

	err = tegra_unpowergate_partition(vi->config->powergate_id);
	if (err)
		return err;

	for (i = 0; i < vi->config->clk_cnt; i++) {
		err = clk_prepare_enable(vi->config->clk[i]);
		if (err) {
			int j;

			dev_err(dev, "failed to enable clk:%d\n", i);
			for (j = 0; j < i; j++)
				clk_disable_unprepare(vi->config->clk[i]);
			tegra_powergate_partition(vi->config->powergate_id);
			return err;
		}
	}
#else
	err = tegra_powergate_sequence_power_up(vi->config->powergate_id,
						vi->config->clk[0], vi->rst);
	if (err)
		return err;

	err = regulator_enable(vi->reg);
	if (err) {
		tegra_powergate_power_off(TEGRA_POWERGATE_VENC);
		dev_err(dev, "enable csi regulator failed.\n");
		return err;
	}
#endif

	vi_writel(vi, T12_CG_2ND_LEVEL_EN, T12_VI_CFG_CG_CTRL);

	return 0;
}

static void vi_reset(struct device *dev)
{
	struct vi *vi = dev_get_drvdata(dev);

	vi_power_off(dev);
	vi_power_on(dev);

	/* Reset sensor A */
	vi_writel(vi, 0x00000000, T12_VI_CSI_0_CSI_IMAGE_DT);

	vi_writel(vi, T12_CSI_CSICIL_SW_SENSOR_A_RESET_SENSOR_A_RESET,
		  T12_CSI_CSICIL_SW_SENSOR_A_RESET);

	vi_writel(vi, T12_CSI_CSI_SW_SENSOR_A_RESET_SENSOR_A_RESET,
		  T12_CSI_CSI_SW_SENSOR_A_RESET);

	vi_writel(vi, T12_VI_CSI_0_SW_RESET_SHADOW_RESET |
		      T12_VI_CSI_0_SW_RESET_SENSORCTL_RESET |
		      T12_VI_CSI_0_SW_RESET_PF_RESET |
		      T12_VI_CSI_0_SW_RESET_MCINTF_RESET |
		      T12_VI_CSI_0_SW_RESET_ISPINTF_RESET,
		  T12_VI_CSI_0_SW_RESET);

	/* Reset sensor B */
	vi_writel(vi, 0x00000000, T12_VI_CSI_1_CSI_IMAGE_DT);
	vi_writel(vi, T12_CSI_CSICIL_SW_SENSOR_B_RESET_SENSOR_B_RESET,
		  T12_CSI_CSICIL_SW_SENSOR_B_RESET);

	vi_writel(vi, T12_CSI_CSI_SW_SENSOR_B_RESET_SENSOR_B_RESET,
		  T12_CSI_CSI_SW_SENSOR_B_RESET);

	vi_writel(vi, T12_VI_CSI_1_SW_RESET_SHADOW_RESET |
		      T12_VI_CSI_1_SW_RESET_SENSORCTL_RESET |
		      T12_VI_CSI_1_SW_RESET_PF_RESET |
		      T12_VI_CSI_1_SW_RESET_MCINTF_RESET |
		      T12_VI_CSI_1_SW_RESET_ISPINTF_RESET,
		  T12_VI_CSI_1_SW_RESET);

	udelay(10);

	vi_writel(vi, 0x00000000, T12_CSI_CSI_SW_SENSOR_A_RESET);
	vi_writel(vi, 0x00000000, T12_CSI_CSICIL_SW_SENSOR_A_RESET);
	vi_writel(vi, 0x00000000, T12_CSI_CSI_SW_SENSOR_B_RESET);
	vi_writel(vi, 0x00000000, T12_CSI_CSICIL_SW_SENSOR_B_RESET);
	vi_writel(vi, 0x00000000, T12_VI_CSI_0_SW_RESET);
	vi_writel(vi, 0x00000000, T12_VI_CSI_1_SW_RESET);
}

static int vi_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	unsigned long flags = HOST1X_SYNCPT_HAS_BASE;
	struct vi *vi = to_vi(drm);
	unsigned int i;
	int err;
	dev_t devno;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, client->dev);
		if (err < 0) {
			dev_err(client->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		vi->domain = tegra->domain;
	}

	vi->channel = host1x_channel_request(client->dev);
	if (!vi->channel) {
		err = -ENOMEM;
		goto error_iommu_detach_device;
	}

	for (i = 0; i < VI_NUM_SYNCPTS; i++) {
		client->syncpts[i] = host1x_syncpt_request(client->dev, flags);
		if (!client->syncpts[i]) {
			err = -ENOMEM;
			goto error_host1x_syncpt_free;
		}
	}

	err = tegra_drm_register_client(dev->dev_private, drm);
	if (err)
		goto error_host1x_syncpt_free;

	cdev_init(&vi->cdev, &tegra_vi_ctrl_ops);
	vi->cdev.owner = THIS_MODULE;

	vi->class = class_create(THIS_MODULE, VI_CTRL_NODE);
	if (IS_ERR(vi->class)) {
		err = PTR_ERR(vi->class);
		goto err_create_class;
	}

	err = alloc_chrdev_region(&devno, 0, 1, VI_CTRL_NODE);
	if (err < 0)
		goto err_alloc_chrdev_region;

	err = cdev_add(&vi->cdev, devno, 1);
	if (err < 0)
		goto err_add_cdev;

	vi->ctrl = device_create(vi->class, NULL, devno, NULL, VI_CTRL_NODE);
	if (IS_ERR(vi->ctrl)) {
		err = PTR_ERR(vi->ctrl);
		goto err_create_device;
	}

	return 0;

err_create_device:
	cdev_del(&vi->cdev);
err_add_cdev:
	unregister_chrdev_region(devno, 1);
err_alloc_chrdev_region:
	class_destroy(vi->class);
err_create_class:
	tegra_drm_unregister_client(tegra, drm);
error_host1x_syncpt_free:
	for (i = 0; i < VI_NUM_SYNCPTS; i++)
		if (client->syncpts[i]) {
			host1x_syncpt_free(client->syncpts[i]);
			client->syncpts[i] = NULL;
		}
	host1x_channel_free(vi->channel);
error_iommu_detach_device:
	if (vi->domain) {
		iommu_detach_device(vi->domain, vi->dev);
		vi->domain = NULL;
	}
	return err;
}

static int vi_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct vi *vi = to_vi(drm);
	unsigned int i;
	int err;

	err = tegra_drm_unregister_client(dev->dev_private, drm);

	for (i = 0; i < VI_NUM_SYNCPTS; i++)
		if (client->syncpts[i]) {
			host1x_syncpt_free(client->syncpts[i]);
			client->syncpts[i] = NULL;
		}

	host1x_channel_free(vi->channel);

	if (vi->domain) {
		iommu_detach_device(vi->domain, vi->dev);
		vi->domain = NULL;
	}

	return 0;
}

static const struct host1x_client_ops vi_client_ops = {
	.init = vi_init,
	.exit = vi_exit,
};

static int vi_open_channel(struct tegra_drm_client *client,
			   struct tegra_drm_context *context)
{
	struct vi *vi = to_vi(client);

	context->channel = host1x_channel_get(vi->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

static void vi_close_channel(struct tegra_drm_context *context)
{
	if (!context->channel)
		return;

	host1x_channel_put(context->channel);
	context->channel = NULL;
}

static int vi_is_addr_reg(struct device *dev, u32 class, u32 offset, u32 val)
{
	return 0;
}

static const struct tegra_drm_client_ops vi_ops = {
	.open_channel = vi_open_channel,
	.close_channel = vi_close_channel,
	.is_addr_reg = vi_is_addr_reg,
	.submit = tegra_drm_submit,
	.reset = vi_reset,
};

static long vi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != NVHOST_VI_IOCTL_MAGIC)
		return -EFAULT;

	return 0;
}

static int vi_open(struct inode *inode, struct file *file)
{
	struct vi *vi = container_of(inode->i_cdev, struct vi, cdev);

	file->private_data = vi;

	return 0;
}

static int vi_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tegra_vi_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = vi_open,
	.unlocked_ioctl = vi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vi_ioctl,
#endif
	.release = vi_release,
};

static const struct vi_config vi_t210_config = {
	.class_id = HOST1X_CLASS_VI,
	.powergate_id = TEGRA_POWERGATE_VENC,
};

static const struct of_device_id vi_match[] = {
	{ .compatible = "nvidia,tegra210-vi", .data = &vi_t210_config },
	{ },
};

static int vi_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct vi_config *vi_config;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	struct vi *vi;
	int err;
	int i =  0;
	struct device_node *np;
	const char *name;

	if (!dev->of_node) {
		dev_err(dev, "no dt node\n");
		return -EINVAL;
	}

	np = pdev->dev.of_node;
	match = of_match_node(vi_match, pdev->dev.of_node);
	if (match)
		vi_config = (struct vi_config *)match->data;
	else
		return -ENXIO;

	vi = devm_kzalloc(dev, sizeof(*vi), GFP_KERNEL);
	if (!vi)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, VI_NUM_SYNCPTS * sizeof(*syncpts),
			       GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "failed to get registers\n");
		return -ENXIO;
	}
	vi->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(vi->regs))
		return PTR_ERR(vi->regs);

	vi->rst = devm_reset_control_get(dev, "vi");
	if (IS_ERR(vi->rst)) {
		dev_err(dev, "cannot get reset\n");
		return PTR_ERR(vi->rst);
	}

	vi_config->clk_cnt = of_property_count_strings(np, "clock-names");
	vi_config->clk = devm_kzalloc(dev, vi_config->clk_cnt * sizeof(struct clk *), GFP_KERNEL);
	if (!vi_config->clk)
		return -ENOMEM;

	for (i = 0; i < vi_config->clk_cnt; i++) {
		of_property_read_string_index(np, "clock-names", i, &name);

		if (IS_ENABLED(CONFIG_COMMON_CLK))
			vi_config->clk[i] = devm_clk_get(dev, name);
		else
			vi_config->clk[i] = clk_get_sys("tegra_vi", name);
		if (IS_ERR(vi_config->clk[i]))  {
			dev_err(dev, "cannot get clk[%s].[%d]\n", name, i);
			return PTR_ERR(vi_config->clk[i]);
		}
	}

#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	vi->reg = devm_regulator_get(dev, "avdd-dsi-csi");
	if (IS_ERR(vi->reg)) {
		err = PTR_ERR(vi->reg);
		return err;
	}
#endif

	INIT_LIST_HEAD(&vi->client.base.list);
	vi->client.base.ops = &vi_client_ops;
	vi->client.base.dev = dev;
	vi->client.base.class = vi_config->class_id;
	vi->client.base.syncpts = syncpts;
	vi->client.base.num_syncpts = VI_NUM_SYNCPTS;
	vi->dev = dev;
	vi->config = vi_config;

	INIT_LIST_HEAD(&vi->client.list);
	vi->client.ops = &vi_ops;

	platform_set_drvdata(pdev, vi);

	err = vi_power_on(dev);
	if (err) {
		dev_err(dev, "cannot power on device\n");
		return err;
	}

	err = host1x_client_register(&vi->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		goto error_vi_power_off;
	}

	err = tegra_vi_media_controller_init(&vi->mc_vi, pdev);
	if (err) {
		dev_err(dev, "failed to register media controller%d\n", err);
		goto error_client_unregister;
	}

	dev_info(dev, "initialized");

	return 0;

error_client_unregister:
	host1x_client_unregister(&vi->client.base);
error_vi_power_off:
	vi_power_off(dev);
	return err;
}

static int vi_remove(struct platform_device *pdev)
{
	struct vi *vi = platform_get_drvdata(pdev);
	int err;

	tegra_vi_media_controller_cleanup(&vi->mc_vi);

	err = host1x_client_unregister(&vi->client.base);
	if (err < 0)
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);

	vi_power_off(&pdev->dev);

	return err;
}

struct platform_driver tegra_vi_driver = {
	.driver = {
		.name = "tegra-vi",
		.of_match_table = vi_match,
	},
	.probe = vi_probe,
	.remove = vi_remove,
};
