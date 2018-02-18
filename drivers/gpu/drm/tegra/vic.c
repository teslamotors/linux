/*
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
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

#include "drm.h"
#include "falcon.h"
#include "vic.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "vic_t186.h"
#endif

#define VIC_AUTOSUSPEND_DELAY 500

static int vic_boot(struct vic *vic);

static int vic_runtime_resume(struct device *dev)
{
	struct vic *vic = dev_get_drvdata(dev);
	int err = 0;

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	err = tegra_unpowergate_partition(TEGRA_POWERGATE_VIC);
	if (err)
		return err;

	err = clk_prepare_enable(vic->clk);
	if (err) {
		tegra_powergate_partition(TEGRA_POWERGATE_VIC);
		return err;
	}
#else
	if (vic->rst) {
		err = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_VIC,
							vic->clk, vic->rst);
		if (err < 0)
			dev_err(dev, "failed to power up device\n");
	}
#endif
	err = vic_boot(vic);
	if (err < 0)
		goto fail_vic_boot;

	return 0;

fail_vic_boot:
#ifndef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_power_off(TEGRA_POWERGATE_VIC);
#else
	clk_disable_unprepare(vic->clk);
	tegra_powergate_partition(TEGRA_POWERGATE_VIC);
#endif
	return err;

}

static int vic_runtime_suspend(struct device *dev)
{
	struct vic *vic = dev_get_drvdata(dev);

	clk_disable_unprepare(vic->clk);

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	tegra_powergate_partition(TEGRA_POWERGATE_VIC);
#else
	if (vic->rst)
		reset_control_assert(vic->rst);
	tegra_powergate_power_off(TEGRA_POWERGATE_VIC);
#endif

	vic->booted = false;

	return 0;
}

static int vic_boot(struct vic *vic)
{
	u32 fce_ucode_size, fce_bin_data_offset;
	struct tegra_drm_client *client = &vic->client;
	void *hdr;
	int err = 0;

	if (vic->booted)
		return 0;

	if (!vic->falcon.firmware.valid) {
		err = falcon_read_firmware(&vic->falcon,
					   vic->config->ucode_name);
		if (err < 0)
			return err;
	}

	/* ensure that the engine is in sane state */
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	if (vic->rst) {
		reset_control_assert(vic->rst);
		usleep_range(10, 100);
		reset_control_deassert(vic->rst);
	} else {
		tegra_mc_flush(true);
		tegra_periph_reset_assert(vic->clk);
		usleep_range(10, 100);
		tegra_periph_reset_deassert(vic->clk);
		tegra_mc_flush_done(true);
	}
#else
	if (vic->rst) {
		reset_control_assert(vic->rst);
		usleep_range(10, 100);
		reset_control_deassert(vic->rst);
	}
#endif

	/* setup clockgating registers */
	vic_writel(vic, CG_IDLE_CG_DLY_CNT(4) |
			CG_IDLE_CG_EN |
			CG_WAKEUP_DLY_CNT(4),
		   NV_PVIC_MISC_PRI_VIC_CG);

	if (client->ops->load_regs)
		client->ops->load_regs(client);

	err = falcon_boot(&vic->falcon);
	if (err < 0)
		return err;

	hdr = vic->falcon.firmware.vaddr;
	fce_bin_data_offset = *(u32 *)(hdr + VIC_UCODE_FCE_DATA_OFFSET);
	hdr = vic->falcon.firmware.vaddr +
		*(u32 *)(hdr + VIC_UCODE_FCE_HEADER_OFFSET);
	fce_ucode_size = *(u32 *)(hdr + FCE_UCODE_SIZE_OFFSET);

	falcon_execute_method(&vic->falcon, VIC_SET_APPLICATION_ID, 1);
	falcon_execute_method(&vic->falcon, VIC_SET_FCE_UCODE_SIZE,
			      fce_ucode_size);
	falcon_execute_method(&vic->falcon, VIC_SET_FCE_UCODE_OFFSET,
			      (vic->falcon.firmware.paddr + fce_bin_data_offset)
				>> 8);

	err = falcon_wait_idle(&vic->falcon);
	if (err < 0) {
		dev_err(vic->dev,
			"failed to set application ID and FCE base\n");
		return err;
	}

	vic->booted = true;

	return 0;
}

static void *vic_falcon_alloc(struct falcon *falcon, size_t size,
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

static void vic_falcon_free(struct falcon *falcon, size_t size,
			    dma_addr_t iova, void *va)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	dma_free_writecombine(falcon->dev, size, va, iova);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_free(tegra, size, va, iova);
#endif
}

static const struct falcon_ops vic_falcon_ops = {
	.alloc = vic_falcon_alloc,
	.free = vic_falcon_free
};

static int vic_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct vic *vic = to_vic(drm);
	int err;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, vic->dev);
		if (err < 0) {
			dev_err(vic->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		vic->domain = tegra->domain;
	}

	vic->falcon.dev = vic->dev;
	vic->falcon.regs = vic->regs;
	vic->falcon.data = tegra;
	vic->falcon.ops = &vic_falcon_ops;
	err = falcon_init(&vic->falcon);
	if (err < 0)
		goto detach_device;

	vic->channel = host1x_channel_request(client->dev);
	if (!vic->channel) {
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
	host1x_channel_free(vic->channel);
exit_falcon:
	falcon_exit(&vic->falcon);
detach_device:
	if (tegra->domain)
		iommu_detach_device(tegra->domain, vic->dev);

	return err;
}

static int vic_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct vic *vic = to_vic(drm);
	int err;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_free(client->syncpts[0]);
	host1x_channel_free(vic->channel);

	if (vic->booted) {
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		if (vic->rst) {
			reset_control_assert(vic->rst);
			usleep_range(10, 100);
			reset_control_deassert(vic->rst);
		} else {
			tegra_mc_flush(true);
			tegra_periph_reset_assert(vic->clk);
			usleep_range(10, 100);
			tegra_periph_reset_deassert(vic->clk);
			tegra_mc_flush_done(true);
		}
#else
		if (vic->rst) {
			reset_control_assert(vic->rst);
			usleep_range(10, 100);
			reset_control_deassert(vic->rst);
		}
#endif
	}

	falcon_exit(&vic->falcon);

	if (vic->domain) {
		iommu_detach_device(vic->domain, vic->dev);
		vic->domain = NULL;
	}

	return 0;
}

static const struct host1x_client_ops vic_client_ops = {
	.init = vic_init,
	.exit = vic_exit,
};

int vic_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct vic *vic = to_vic(client);

	context->channel = host1x_channel_get(vic->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

void vic_close_channel(struct tegra_drm_context *context)
{
	host1x_channel_put(context->channel);
}

int vic_is_addr_reg(struct device *dev, u32 class, u32 offset, u32 val)
{
	struct vic *vic = dev_get_drvdata(dev);

	if (class != HOST1X_CLASS_VIC)
		return false;

	/*
	 * Method call parameter. Check stored value to see if set method uses
	 * parameter as memory address.
	 */
	if (offset == FALCON_UCLASS_METHOD_DATA >> 2)
		return vic->method_data_is_addr_reg;

	/* Method call number store. */
	if (offset == FALCON_UCLASS_METHOD_OFFSET >> 2) {
		u32 method = val << 2;

		if ((method >= VIC_SET_SURFACE0_SLOT0_LUMA_OFFSET &&
		     method <= VIC_SET_SURFACE7_SLOT4_CHROMAV_OFFSET) ||
		    (method >= VIC_SET_CONFIG_STRUCT_OFFSET &&
		     method <= VIC_SET_OUTPUT_SURFACE_CHROMAV_OFFSET))
			vic->method_data_is_addr_reg = true;
		else
			vic->method_data_is_addr_reg = false;
	}

	return false;
}

static const struct tegra_drm_client_ops vic_ops = {
	.open_channel = vic_open_channel,
	.close_channel = vic_close_channel,
	.is_addr_reg = vic_is_addr_reg,
	.submit = tegra_drm_submit,
};

static const struct vic_config vic_t124_config = {
	.ucode_name = "vic03_ucode.bin",
	.drm_client_ops = &vic_ops,
};

static const struct vic_config vic_t210_config = {
	.ucode_name = "tegra21x/vic04_ucode.bin",
	.drm_client_ops = &vic_ops,
};

static const struct of_device_id vic_match[] = {
	{ .compatible = "nvidia,tegra124-vic", .data = &vic_t124_config },
	{ .compatible = "nvidia,tegra210-vic", .data = &vic_t210_config },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-vic", .data = &vic_t186_config },
#endif
	{ },
};

static int vic_probe(struct platform_device *pdev)
{
	struct vic_config *vic_config = NULL;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	const struct of_device_id *match;
	struct vic *vic;
	int err;

	match = of_match_device(vic_match, dev);
	vic_config = (struct vic_config *)match->data;

	vic = devm_kzalloc(dev, sizeof(*vic), GFP_KERNEL);
	if (!vic)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "failed to get registers\n");
		return -ENXIO;
	}

	vic->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(vic->regs))
		return PTR_ERR(vic->regs);

	vic->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(vic->clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		return PTR_ERR(vic->clk);
	}

	vic->rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(vic->rst))
		dev_err(&pdev->dev, "cannot get reset\n");

	platform_set_drvdata(pdev, vic);

	INIT_LIST_HEAD(&vic->client.base.list);
	vic->client.base.ops = &vic_client_ops;
	vic->client.base.dev = dev;
	vic->client.base.class = HOST1X_CLASS_VIC;
	vic->client.base.syncpts = syncpts;
	vic->client.base.num_syncpts = 1;
	vic->dev = dev;
	vic->config = vic_config;

	INIT_LIST_HEAD(&vic->client.list);
	vic->client.ops = vic->config->drm_client_ops;

	err = host1x_client_register(&vic->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	/* BL can keep partition powered ON,
	 * if so power off partition explicitly
	 */
	if (tegra_powergate_is_powered(TEGRA_POWERGATE_VIC))
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		tegra_powergate_partition(TEGRA_POWERGATE_VIC);
#else
		tegra_powergate_power_off(TEGRA_POWERGATE_VIC);
#endif

	pm_runtime_set_autosuspend_delay(dev, VIC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		err = vic_runtime_resume(&pdev->dev);
		if (err < 0)
			goto unregister_client;
	}

	dev_info(&pdev->dev, "initialized");

	return 0;

unregister_client:
	host1x_client_unregister(&vic->client.base);

	return err;
}

static int vic_remove(struct platform_device *pdev)
{
	struct vic *vic = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&vic->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		vic_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops vic_pm_ops = {
	SET_RUNTIME_PM_OPS(vic_runtime_suspend, vic_runtime_resume, NULL)
};

struct platform_driver tegra_vic_driver = {
	.driver = {
		.name = "tegra-vic",
		.of_match_table = vic_match,
		.pm = &vic_pm_ops
	},
	.probe = vic_probe,
	.remove = vic_remove,
};
