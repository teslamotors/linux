/*
 * Copyright (C) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/host1x.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/platform/tegra/mc.h>
#else
#include <soc/tegra/pmc.h>
#endif

#include "nvenc.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "nvenc_t186.h"
#endif

#define NVENC_AUTOSUSPEND_DELAY 500

static int nvenc_boot(struct nvenc *nvenc);

static int nvenc_runtime_resume(struct device *dev)
{
	struct nvenc *nvenc = dev_get_drvdata(dev);
	int err = 0;

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	err = tegra_unpowergate_partition(TEGRA_POWERGATE_NVENC);
	if (err)
		return err;

	err = clk_prepare_enable(nvenc->clk);
	if (err) {
		tegra_powergate_partition(TEGRA_POWERGATE_NVENC);
		return err;
	}
#else
	if (nvenc->rst) {
		err = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_NVENC,
							nvenc->clk, nvenc->rst);
		if (err < 0)
			dev_err(dev, "failed to power up device\n");
	}
#endif
	err = nvenc_boot(nvenc);
	if (err < 0)
		goto nvenc_boot_fail;

	return 0;
nvenc_boot_fail:
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_power_off(TEGRA_POWERGATE_NVENC);
#else
	clk_disable_unprepare(nvenc->clk);
	tegra_powergate_partition(TEGRA_POWERGATE_NVENC);
#endif
	return err;

}

static int nvenc_runtime_suspend(struct device *dev)
{
	struct nvenc *nvenc = dev_get_drvdata(dev);

	clk_disable_unprepare(nvenc->clk);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_partition(TEGRA_POWERGATE_NVENC);
#else
	if (nvenc->rst)
		reset_control_assert(nvenc->rst);
	tegra_powergate_power_off(TEGRA_POWERGATE_NVENC);
#endif

	nvenc->booted = false;

	return 0;
}

static int nvenc_boot(struct nvenc *nvenc)
{
	int err = 0;
	struct tegra_drm_client *client = &nvenc->client;

	if (nvenc->booted)
		return 0;

	if (!nvenc->falcon.firmware.valid) {
		err = falcon_read_firmware(&nvenc->falcon,
					   nvenc->config->ucode_name);
		if (err < 0)
			return err;
	}

	/* ensure that the engine is in sane state */
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	if (nvenc->rst) {
		reset_control_assert(nvenc->rst);
		usleep_range(10, 100);
		reset_control_deassert(nvenc->rst);
	} else {
		tegra_mc_flush(true);
		tegra_periph_reset_assert(nvenc->clk);
		usleep_range(10, 100);
		tegra_periph_reset_deassert(nvenc->clk);
		tegra_mc_flush_done(true);
	}
#else
	if (nvenc->rst) {
		reset_control_assert(nvenc->rst);
		usleep_range(10, 100);
		reset_control_deassert(nvenc->rst);
	}
#endif

	if (client->ops->load_regs)
		client->ops->load_regs(client);

	err = falcon_boot(&nvenc->falcon);
	if (err < 0)
		return err;

	err = falcon_wait_idle(&nvenc->falcon);
	if (err < 0) {
		dev_err(nvenc->dev,
			"failed to set application ID and FCE base\n");
		return err;
	}

	nvenc->booted = true;

	return 0;
}

static void *nvenc_falcon_alloc(struct falcon *falcon, size_t size,
			       dma_addr_t *iova)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	return dma_alloc_writecombine(falcon->dev, size, iova,
					   GFP_KERNEL | __GFP_NOWARN);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_alloc(tegra, size, iova);
#endif
}

static void nvenc_falcon_free(struct falcon *falcon, size_t size,
			    dma_addr_t iova, void *va)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	dma_free_writecombine(falcon->dev, size, va, iova);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_free(tegra, size, va, iova);
#endif
}

static const struct falcon_ops nvenc_falcon_ops = {
	.alloc = nvenc_falcon_alloc,
	.free = nvenc_falcon_free
};

static int nvenc_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvenc *nvenc = to_nvenc(drm);
	int err;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, nvenc->dev);
		if (err < 0) {
			dev_err(nvenc->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		nvenc->domain = tegra->domain;
	}

	nvenc->falcon.dev = nvenc->dev;
	nvenc->falcon.regs = nvenc->regs;
	nvenc->falcon.data = tegra;
	nvenc->falcon.ops = &nvenc_falcon_ops;
	err = falcon_init(&nvenc->falcon);
	if (err < 0)
		goto detach_device;

	nvenc->channel = host1x_channel_request(client->dev);
	if (!nvenc->channel) {
		err = -ENOMEM;
		goto exit_falcon;
	}

	client->syncpts[0] = host1x_syncpt_request(client->dev, 0);
	if (!client->syncpts[0]) {
		err = -ENOMEM;
		goto free_channel;
	}

	err = tegra_drm_register_client(tegra, drm);
	if (err < 0)
		goto free_syncpt;

	return 0;

free_syncpt:
	host1x_syncpt_free(client->syncpts[0]);
free_channel:
	host1x_channel_free(nvenc->channel);
exit_falcon:
	falcon_exit(&nvenc->falcon);
detach_device:
	if (tegra->domain)
		iommu_detach_device(tegra->domain, nvenc->dev);

	return err;
}

static int nvenc_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvenc *nvenc = to_nvenc(drm);
	int err;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_free(client->syncpts[0]);
	host1x_channel_free(nvenc->channel);

	if (nvenc->booted) {
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		if (nvenc->rst) {
			reset_control_assert(nvenc->rst);
			usleep_range(10, 100);
			reset_control_deassert(nvenc->rst);
		} else {
			tegra_mc_flush(true);
			tegra_periph_reset_assert(nvenc->clk);
			usleep_range(10, 100);
			tegra_periph_reset_deassert(nvenc->clk);
			tegra_mc_flush_done(true);
		}
#else
		if (nvenc->rst) {
			reset_control_assert(nvenc->rst);
			usleep_range(10, 100);
			reset_control_deassert(nvenc->rst);
		}
#endif
	}

	falcon_exit(&nvenc->falcon);

	if (nvenc->domain) {
		iommu_detach_device(nvenc->domain, nvenc->dev);
		nvenc->domain = NULL;
	}

	return 0;
}

static const struct host1x_client_ops nvenc_client_ops = {
	.init = nvenc_init,
	.exit = nvenc_exit,
};

int nvenc_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct nvenc *nvenc = to_nvenc(client);

	context->channel = host1x_channel_get(nvenc->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

void nvenc_close_channel(struct tegra_drm_context *context)
{
	host1x_channel_put(context->channel);
}

static const struct tegra_drm_client_ops nvenc_ops = {
	.open_channel = nvenc_open_channel,
	.close_channel = nvenc_close_channel,
	.submit = tegra_drm_submit,
};

static const struct nvenc_config nvenc_t210_config = {
	.ucode_name = "tegra21x/nvhost_nvenc050.fw",
	.drm_client_ops = &nvenc_ops,
};

static const struct of_device_id nvenc_match[] = {
	{ .compatible = "nvidia,tegra210-nvenc", .data = &nvenc_t210_config },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-nvenc", .data = &nvenc_t186_config },
#endif
	{ },
};

static int nvenc_probe(struct platform_device *pdev)
{
	struct nvenc_config *nvenc_config = NULL;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	const struct of_device_id *match;
	struct nvenc *nvenc;
	int err;

	match = of_match_device(nvenc_match, dev);
	nvenc_config = (struct nvenc_config *)match->data;

	nvenc = devm_kzalloc(dev, sizeof(*nvenc), GFP_KERNEL);
	if (!nvenc)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "failed to get registers\n");
		return -ENXIO;
	}

	nvenc->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(nvenc->regs))
		return PTR_ERR(nvenc->regs);

	nvenc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(nvenc->clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		return PTR_ERR(nvenc->clk);
	}

	nvenc->rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(nvenc->rst)) {
		dev_err(&pdev->dev, "cannot get reset\n");
		nvenc->rst = NULL;
	}

	platform_set_drvdata(pdev, nvenc);

	INIT_LIST_HEAD(&nvenc->client.base.list);
	nvenc->client.base.ops = &nvenc_client_ops;
	nvenc->client.base.dev = dev;
	nvenc->client.base.class = HOST1X_CLASS_NVENC;
	nvenc->client.base.syncpts = syncpts;
	nvenc->client.base.num_syncpts = 1;
	nvenc->dev = dev;
	nvenc->config = nvenc_config;

	INIT_LIST_HEAD(&nvenc->client.list);
	nvenc->client.ops = nvenc->config->drm_client_ops;

	err = host1x_client_register(&nvenc->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	/* BL can keep partition powered ON,
	 * if so power off partition explicitly
	 */
	if (tegra_powergate_is_powered(TEGRA_POWERGATE_NVENC))
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		tegra_powergate_partition(TEGRA_POWERGATE_NVENC);
#else
		tegra_powergate_power_off(TEGRA_POWERGATE_NVENC);
#endif

	pm_runtime_set_autosuspend_delay(dev, NVENC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		err = nvenc_runtime_resume(&pdev->dev);
		if (err < 0)
			goto unregister_client;
	}

	dev_info(&pdev->dev, "initialized");

	return 0;

unregister_client:
	host1x_client_unregister(&nvenc->client.base);

	return err;
}

static int nvenc_remove(struct platform_device *pdev)
{
	struct nvenc *nvenc = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&nvenc->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		nvenc_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops nvenc_pm_ops = {
	SET_RUNTIME_PM_OPS(nvenc_runtime_suspend, nvenc_runtime_resume, NULL)
};

struct platform_driver tegra_nvenc_driver = {
	.driver = {
		.name = "tegra-nvenc",
		.of_match_table = nvenc_match,
		.pm = &nvenc_pm_ops
	},
	.probe = nvenc_probe,
	.remove = nvenc_remove,
};
