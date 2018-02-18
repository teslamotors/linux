/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/tegra-soc.h>

#include <linux/platform/tegra/tegra18_kfuse.h>

#include "tegra18_kfuse_priv.h"
#include "tegra_kfuse_priv.h"

struct tegra18_kfuse {
	unsigned int cg_refcount;
	struct mutex cg_refcount_mutex;
};

#define KFUSE_CG1_0 0x90

int tegra_kfuse_enable_sensing(void)
{
	struct kfuse *kfuse;
	struct tegra18_kfuse *tegra18_kfuse;
	int err = 0;

	/* check that kfuse driver is available.. */

	kfuse = tegra_kfuse_get();
	if (!kfuse)
		return -ENODEV;

	tegra18_kfuse = kfuse->private_data;
	if (!tegra18_kfuse)
		return -ENODEV;

	mutex_lock(&tegra18_kfuse->cg_refcount_mutex);

	/* increment refcount */
	tegra18_kfuse->cg_refcount++;

	/* if clock was already up, quit */
	if (tegra18_kfuse->cg_refcount > 1)
		goto exit_unlock;

	/* enable kfuse clock */
	err = clk_prepare_enable(kfuse->clk);
	if (err) {
		tegra18_kfuse->cg_refcount--;
		goto exit_unlock;
	}

	/* WAR to simulator bug in clockgating behavior; The register must
	 * not be written or the users will not be able to access kfuse */
	if (tegra_platform_is_linsim())
		goto exit_unlock;

	/* enable kfuse sensing */
	tegra_kfuse_writel(kfuse, 1, KFUSE_CG1_0);

exit_unlock:
	mutex_unlock(&tegra18_kfuse->cg_refcount_mutex);

	return err;
}
EXPORT_SYMBOL(tegra_kfuse_enable_sensing);

void tegra_kfuse_disable_sensing(void)
{
	struct kfuse *kfuse;
	struct tegra18_kfuse *tegra18_kfuse;

	/* check that kfuse driver is available.. */

	kfuse = tegra_kfuse_get();
	if (!kfuse)
		return;

	tegra18_kfuse = kfuse->private_data;
	if (!tegra18_kfuse)
		return;

	mutex_lock(&tegra18_kfuse->cg_refcount_mutex);

	if (WARN_ON(tegra18_kfuse->cg_refcount == 0))
		goto exit_unlock;

	/* decrement refcount */
	tegra18_kfuse->cg_refcount--;

	/* if there are still users, quit */
	if (tegra18_kfuse->cg_refcount > 0)
		goto exit_unlock;

	/* disable kfuse sensing */
	tegra_kfuse_writel(kfuse, 0, KFUSE_CG1_0);

	/* ..and disable kfuse clock */
	clk_disable_unprepare(kfuse->clk);

exit_unlock:
	mutex_unlock(&tegra18_kfuse->cg_refcount_mutex);
}
EXPORT_SYMBOL(tegra_kfuse_disable_sensing);

int tegra18_kfuse_deinit(struct kfuse *kfuse)
{
	struct tegra18_kfuse *tegra18_kfuse = kfuse->private_data;
	int ret = 0;

	/* ensure that no-one is using sensing now */
	mutex_lock(&tegra18_kfuse->cg_refcount_mutex);
	if (tegra18_kfuse->cg_refcount)
		ret = -EBUSY;
	mutex_unlock(&tegra18_kfuse->cg_refcount_mutex);

	return ret;
}

int tegra18_kfuse_init(struct kfuse *kfuse)
{
	struct tegra18_kfuse *tegra18_kfuse;

	tegra18_kfuse = devm_kzalloc(&kfuse->pdev->dev,
				     sizeof(*tegra18_kfuse),
				     GFP_KERNEL);
	if (!tegra18_kfuse)
		return -ENOMEM;

	mutex_init(&tegra18_kfuse->cg_refcount_mutex);

	kfuse->private_data = tegra18_kfuse;

	return 0;
}
