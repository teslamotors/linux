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

#include "nvjpg.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "nvjpg_t186.h"
#endif

#define NVJPG_AUTOSUSPEND_DELAY 500

static int nvjpg_boot(struct nvjpg *nvjpg);

static int nvjpg_runtime_resume(struct device *dev)
{
	struct nvjpg *nvjpg = dev_get_drvdata(dev);
	int err = 0;

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	err = tegra_unpowergate_partition(TEGRA_POWERGATE_NVJPG);
	if (err)
		return err;

	err = clk_prepare_enable(nvjpg->clk);
	if (err) {
		tegra_powergate_partition(TEGRA_POWERGATE_NVJPG);
		return err;
	}
#else
	if (nvjpg->rst) {
		err = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_NVJPG,
							nvjpg->clk, nvjpg->rst);
		if (err < 0)
			dev_err(dev, "failed to power up device\n");
	}
#endif
	err = nvjpg_boot(nvjpg);
	if (err < 0)
		goto boot_fail;

	return 0;

boot_fail:
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_power_off(TEGRA_POWERGATE_NVJPG);
#else
	clk_disable_unprepare(nvjpg->clk);
	tegra_powergate_partition(TEGRA_POWERGATE_NVJPG);
#endif
	return err;
}

static int nvjpg_runtime_suspend(struct device *dev)
{
	struct nvjpg *nvjpg = dev_get_drvdata(dev);

	clk_disable_unprepare(nvjpg->clk);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_partition(TEGRA_POWERGATE_NVJPG);
#else
	if (nvjpg->rst)
		reset_control_assert(nvjpg->rst);
	tegra_powergate_power_off(TEGRA_POWERGATE_NVJPG);
#endif

	nvjpg->booted = false;

	return 0;
}

static int nvjpg_boot(struct nvjpg *nvjpg)
{
	int err = 0;
	struct tegra_drm_client *client = &nvjpg->client;

	if (nvjpg->booted)
		return 0;

	if (!nvjpg->falcon.firmware.valid) {
		err = falcon_read_firmware(&nvjpg->falcon,
					   nvjpg->config->ucode_name);
		if (err < 0)
			return err;
	}

	/* ensure that the engine is in sane state */
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	if (nvjpg->rst) {
		reset_control_assert(nvjpg->rst);
		usleep_range(10, 100);
		reset_control_deassert(nvjpg->rst);
	} else {
		tegra_mc_flush(true);
		tegra_periph_reset_assert(nvjpg->clk);
		usleep_range(10, 100);
		tegra_periph_reset_deassert(nvjpg->clk);
		tegra_mc_flush_done(true);
	}
#else
	if (nvjpg->rst) {
		reset_control_assert(nvjpg->rst);
		usleep_range(10, 100);
		reset_control_deassert(nvjpg->rst);
	}
#endif

	if (client->ops->load_regs)
		client->ops->load_regs(client);

	err = falcon_boot(&nvjpg->falcon);
	if (err < 0)
		return err;

	err = falcon_wait_idle(&nvjpg->falcon);
	if (err < 0) {
		dev_err(nvjpg->dev,
			"failed to set application ID and FCE base\n");
		return err;
	}

	nvjpg->booted = true;

	return 0;
}

static void *nvjpg_falcon_alloc(struct falcon *falcon, size_t size,
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

static void nvjpg_falcon_free(struct falcon *falcon, size_t size,
			    dma_addr_t iova, void *va)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	dma_free_writecombine(falcon->dev, size, va, iova);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_free(tegra, size, va, iova);
#endif
}

static const struct falcon_ops nvjpg_falcon_ops = {
	.alloc = nvjpg_falcon_alloc,
	.free = nvjpg_falcon_free
};

static int nvjpg_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvjpg *nvjpg = to_nvjpg(drm);
	int err;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, nvjpg->dev);
		if (err < 0) {
			dev_err(nvjpg->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		nvjpg->domain = tegra->domain;
	}

	nvjpg->falcon.dev = nvjpg->dev;
	nvjpg->falcon.regs = nvjpg->regs;
	nvjpg->falcon.data = tegra;
	nvjpg->falcon.ops = &nvjpg_falcon_ops;
	err = falcon_init(&nvjpg->falcon);
	if (err < 0)
		goto detach_device;

	nvjpg->channel = host1x_channel_request(client->dev);
	if (!nvjpg->channel) {
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
	host1x_channel_free(nvjpg->channel);
exit_falcon:
	falcon_exit(&nvjpg->falcon);
detach_device:
	if (tegra->domain)
		iommu_detach_device(tegra->domain, nvjpg->dev);

	return err;
}

static int nvjpg_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct nvjpg *nvjpg = to_nvjpg(drm);
	int err;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_free(client->syncpts[0]);
	host1x_channel_free(nvjpg->channel);

	if (nvjpg->booted) {
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		if (nvjpg->rst) {
			reset_control_assert(nvjpg->rst);
			usleep_range(10, 100);
			reset_control_deassert(nvjpg->rst);
		} else {
			tegra_mc_flush(true);
			tegra_periph_reset_assert(nvjpg->clk);
			usleep_range(10, 100);
			tegra_periph_reset_deassert(nvjpg->clk);
			tegra_mc_flush_done(true);
		}
#else
		if (nvjpg->rst) {
			reset_control_assert(nvjpg->rst);
			usleep_range(10, 100);
			reset_control_deassert(nvjpg->rst);
		}
#endif
	}

	falcon_exit(&nvjpg->falcon);

	if (nvjpg->domain) {
		iommu_detach_device(nvjpg->domain, nvjpg->dev);
		nvjpg->domain = NULL;
	}

	return 0;
}

static const struct host1x_client_ops nvjpg_client_ops = {
	.init = nvjpg_init,
	.exit = nvjpg_exit,
};

int nvjpg_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct nvjpg *nvjpg = to_nvjpg(client);

	context->channel = host1x_channel_get(nvjpg->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

void nvjpg_close_channel(struct tegra_drm_context *context)
{
	host1x_channel_put(context->channel);
}

static const struct tegra_drm_client_ops nvjpg_ops = {
	.open_channel = nvjpg_open_channel,
	.close_channel = nvjpg_close_channel,
	.submit = tegra_drm_submit,
};

static const struct nvjpg_config nvjpg_t210_config = {
	.ucode_name = "tegra21x/nvhost_nvjpg010.fw",
	.drm_client_ops = &nvjpg_ops,
};

static const struct of_device_id nvjpg_match[] = {
	{ .compatible = "nvidia,tegra210-nvjpg", .data = &nvjpg_t210_config },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-nvjpg", .data = &nvjpg_t186_config },
#endif
	{ },
};

static int nvjpg_probe(struct platform_device *pdev)
{
	struct nvjpg_config *nvjpg_config = NULL;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	const struct of_device_id *match;
	struct nvjpg *nvjpg;
	int err;

	match = of_match_device(nvjpg_match, dev);
	nvjpg_config = (struct nvjpg_config *)match->data;

	nvjpg = devm_kzalloc(dev, sizeof(*nvjpg), GFP_KERNEL);
	if (!nvjpg)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "failed to get registers\n");
		return -ENXIO;
	}

	nvjpg->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(nvjpg->regs))
		return PTR_ERR(nvjpg->regs);

	nvjpg->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(nvjpg->clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		return PTR_ERR(nvjpg->clk);
	}

	nvjpg->rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(nvjpg->rst)) {
		dev_err(&pdev->dev, "cannot get reset\n");
		nvjpg->rst = NULL;
	}

	platform_set_drvdata(pdev, nvjpg);

	INIT_LIST_HEAD(&nvjpg->client.base.list);
	nvjpg->client.base.ops = &nvjpg_client_ops;
	nvjpg->client.base.dev = dev;
	nvjpg->client.base.class = HOST1X_CLASS_NVJPG;
	nvjpg->client.base.syncpts = syncpts;
	nvjpg->client.base.num_syncpts = 1;
	nvjpg->dev = dev;
	nvjpg->config = nvjpg_config;

	INIT_LIST_HEAD(&nvjpg->client.list);
	nvjpg->client.ops = nvjpg->config->drm_client_ops;

	err = host1x_client_register(&nvjpg->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	/* BL can keep partition powered ON,
	 * if so power off partition explicitly
	 */
	if (tegra_powergate_is_powered(TEGRA_POWERGATE_NVJPG))
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		tegra_powergate_partition(TEGRA_POWERGATE_NVJPG);
#else
		tegra_powergate_power_off(TEGRA_POWERGATE_NVJPG);
#endif

	pm_runtime_set_autosuspend_delay(dev, NVJPG_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		err = nvjpg_runtime_resume(&pdev->dev);
		if (err < 0)
			goto unregister_client;
	}

	dev_info(&pdev->dev, "initialized");

	return 0;

unregister_client:
	host1x_client_unregister(&nvjpg->client.base);

	return err;
}

static int nvjpg_remove(struct platform_device *pdev)
{
	struct nvjpg *nvjpg = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&nvjpg->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		nvjpg_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops nvjpg_pm_ops = {
	SET_RUNTIME_PM_OPS(nvjpg_runtime_suspend, nvjpg_runtime_resume, NULL)
};

struct platform_driver tegra_nvjpg_driver = {
	.driver = {
		.name = "tegra-nvjpg",
		.of_match_table = nvjpg_match,
		.pm = &nvjpg_pm_ops
	},
	.probe = nvjpg_probe,
	.remove = nvjpg_remove,
};
