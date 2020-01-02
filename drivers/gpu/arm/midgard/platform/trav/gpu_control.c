/* drivers/gpu/arm/.../platform/gpu_control.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_control.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_domain.h>
#include <linux/clk.h>
#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

static struct gpu_control_ops *ctr_ops;

#ifdef CONFIG_MALI_RT_PM
static struct exynos_pm_domain *gpu_get_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	struct exynos_pm_domain *pd_temp, *pd = NULL;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd")
	{
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd_temp = platform_get_drvdata(pdev);
		if (!strcmp("pd-g3d", pd_temp->genpd.name)) {
			pd = pd_temp;
			break;
		}
	}

	return pd;
}
#endif /* CONFIG_MALI_RT_PM */

int gpu_control_set_voltage(struct kbase_device *kbdev, int voltage)
{
	int ret = 0;
	bool is_up = false;
	static int prev_voltage = -1;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	if (platform->dvs_is_enabled) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set voltage in the dvs mode (requested voltage %d)\n", __func__, voltage);
		return 0;
	}

	if (voltage < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid voltage error (%d)\n", __func__, voltage);
		return -1;
	}

	is_up = prev_voltage < voltage;

	if (ctr_ops->set_voltage_pre)
		ctr_ops->set_voltage_pre(platform, is_up);

	if (ctr_ops->set_voltage)
		ret = ctr_ops->set_voltage(platform, voltage);

	if (ctr_ops->set_voltage_post)
		ctr_ops->set_voltage_post(platform, is_up);

	prev_voltage = voltage;

	return ret;
}

int gpu_control_set_clock(struct kbase_device *kbdev, int clock)
{
	int ret = 0;
	bool is_up = false;
	static int prev_clock = -1;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	if (platform->dvs_is_enabled) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock in the dvs mode (requested clock %d)\n", __func__, clock);
		return 0;
	}
#ifdef CONFIG_MALI_DVFS
	if (gpu_dvfs_get_level(clock) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: mismatch clock error (%d)\n", __func__, clock);
		return -1;
	}
#endif

	is_up = prev_clock < clock;

	if (is_up)
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);

	if (ctr_ops->set_clock_pre)
		ctr_ops->set_clock_pre(platform, clock, is_up);

	if (ctr_ops->set_clock)
		ret = ctr_ops->set_clock(platform, clock);

	if (ctr_ops->set_clock_post)
		ctr_ops->set_clock_post(platform, clock, is_up);

#ifdef CONFIG_MALI_DVFS
	if (is_up && ret)
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);
	else if (!is_up && !ret)
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);
#endif /* CONFIG_MALI_DVFS */

	gpu_dvfs_update_time_in_state(prev_clock);
	prev_clock = clock;

	return ret;
}

int gpu_control_enable_clock(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (ctr_ops->enable_clock)
		ret = ctr_ops->enable_clock(platform);
	mutex_unlock(&platform->gpu_clock_lock);

	gpu_dvfs_update_time_in_state(0);

	return ret;
}

int gpu_control_disable_clock(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (ctr_ops->disable_clock)
		ret = ctr_ops->disable_clock(platform);
	mutex_unlock(&platform->gpu_clock_lock);

#ifdef MALI_SEC_HWCNT
	dvfs_hwcnt_clear_tripipe(kbdev);
#endif
	gpu_dvfs_update_time_in_state(platform->cur_clock);
#ifdef CONFIG_MALI_DVFS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_RESET);
#ifdef CONFIG_MALI_DVFS_USER
	proactive_pm_qos_command(platform, GPU_CONTROL_PM_QOS_RESET);
#endif
#endif /* CONFIG_MALI_DVFS */

	return ret;
}

int gpu_control_is_power_on(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (ctr_ops->is_power_on)
		ret = ctr_ops->is_power_on();
	mutex_unlock(&platform->gpu_clock_lock);

	return ret;
}

int gpu_control_enable_customization(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_REGULATOR
	if (!platform->dvs_status)
		return 0;

	mutex_lock(&platform->gpu_clock_lock);

	if (ctr_ops->set_clock_to_osc)
		ctr_ops->set_clock_to_osc(platform);

	ret = gpu_enable_dvs(platform);
	platform->dvs_is_enabled = true;

	mutex_unlock(&platform->gpu_clock_lock);
#endif /* CONFIG_REGULATOR */

	return ret;
}

int gpu_control_disable_customization(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_REGULATOR
	if (!platform->dvs_status)
		return 0;

	mutex_lock(&platform->gpu_clock_lock);
	ret = gpu_disable_dvs(platform);

	platform->dvs_is_enabled = false;

	if (ctr_ops->set_clock_to_osc && ctr_ops->set_clock) {
#ifdef CONFIG_MALI_DVFS
		if (platform->dvfs_pending) {
			gpu_set_target_clk_vol_pending(platform->dvfs_pending);
			platform->dvfs_pending = 0;
		} else
#endif /* CONFIG_MALI_DVFS */
		ret = ctr_ops->set_clock(platform, platform->cur_clock);
	}

	mutex_unlock(&platform->gpu_clock_lock);
#endif /* CONFIG_REGULATOR */

	return ret;
}

int gpu_control_module_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	platform->exynos_pm_domain = gpu_get_pm_domain();
#endif /* CONFIG_MALI_RT_PM */

	ctr_ops = gpu_get_control_ops();

	if (gpu_power_init(kbdev) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize power\n", __func__);
		goto out;
	}

	if (gpu_clock_init(kbdev) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize clock\n", __func__);
		goto out;
	}

#ifdef CONFIG_REGULATOR
	if (gpu_regulator_init(platform) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize regulator\n", __func__);
		goto out;
	}
#endif /* CONFIG_REGULATOR */

	return 0;
out:
	return -EPERM;
}

void gpu_control_module_term(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

#ifdef CONFIG_MALI_RT_PM
	platform->exynos_pm_domain = NULL;
#endif /* CONFIG_MALI_RT_PM */
}
