/* drivers/gpu/arm/g71/r7p0/platform/exynos/gpu_exynos7870.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-g71 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos7870.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include "../../mali_midg_regmap.h"
#include <soc/samsung/exynos-pmu.h>

extern struct kbase_device *pkbdev;

#define PMU_BASE		0x11400000
#define GPU_RESET_STATUS	(PMU_BASE + 0x2F24)
#define GPU_RESET_MASK          0x00000003
#define GPU_RESET_DONE          0x3

#ifdef CONFIG_MALI_DVFS
#define CPU_MAX PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#else
#define CPU_MAX -1
#endif

#ifndef KHZ
#define KHZ (1000)
#endif

#ifndef MHZ
#define MHZ (1000*1000)
#endif

/*  clk,vol,abb,min,max,down stay, pm_qos mem, pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{1034, 900000, 0, 98, 100, 1, 0, 1539000, 534000, 1690000, 1794000},
	{962,  900000, 0, 98,  99, 1, 0, 1539000, 534000, 1690000, 1794000},
	{845,  900000, 0, 78,  98, 1, 0, 1539000, 534000, 1690000, 1794000},
	{756,  900000, 0, 78,  85, 1, 0, 1352000, 400000, 1586000, 1794000},
	{676,  900000, 0, 78,  85, 1, 0, 1144000, 400000, 1482000, 1794000},
	{548,  900000, 0, 78,  85, 1, 0,  845000, 267000, 1378000, 1794000},
	{449,  900000, 0, 78,  85, 1, 0,  676000, 267000,       0, 1794000},
	{343,  900000, 0, 78,  85, 1, 0,  450000, 134000,       0, 1794000},
};

static int mif_min_table[] = {
	 100000,  133000,  167000,
	 276000,  348000,  450000,
	 543000,  632000,  828000,
	1026000, 1264000, 1456000,
	1552000,
};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 1034},
	{GPU_MAX_CLOCK_LIMIT, 1034},
	{GPU_MIN_CLOCK, 343},
	{GPU_DVFS_START_CLOCK, 343},
	{GPU_DVFS_BL_CONFIG_CLOCK, 343},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 343},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 343},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 343},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 343},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 756},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 800000},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 962},
	{GPU_TEMP_THROTTLING2, 756},
	{GPU_TEMP_THROTTLING3, 548},
	{GPU_TEMP_THROTTLING4, 449},
	{GPU_TEMP_THROTTLING5, 343},
	{GPU_TEMP_TRIPPING, 343},
	{GPU_POWER_COEFF, 625}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 0},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 0},
	{GPU_PERF_GATHERING, 0},
#ifdef MALI_SEC_HWCNT
	{GPU_HWCNT_GATHERING, 0},
	{GPU_HWCNT_POLLING_TIME, 90},
	{GPU_HWCNT_UP_STEP, 3},
	{GPU_HWCNT_DOWN_STEP, 2},
	{GPU_HWCNT_GPR, 0},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0x56},
	{GPU_HWCNT_CHOOSE_SHADER , 0x560},
	{GPU_HWCNT_CHOOSE_TILER , 0x800},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0x80},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_PMQOS_INT_DISABLE, 0},
	{GPU_PMQOS_MIF_MAX_CLOCK, 1539000},
	{GPU_PMQOS_MIF_MAX_CLOCK_BASE, 0},
	{GPU_CL_DVFS_START_BASE, 419},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
#ifdef CONFIG_MALI_DVFS_USER
	{GPU_UDVFS_ENABLE, 1},
#endif
	{GPU_SUSTAINABLE_GPU_CLOCK, 449},
	{GPU_THRESHOLD_MAXLOCK, 10},
	{GPU_LOW_POWER_CPU_MAX_LOCK, 829000},
};

extern void preload_balance_init(struct kbase_device *kbdev);

int gpu_device_specific_init(struct kbase_device *kbdev) {
	return 1;
}

int gpu_dvfs_decide_max_clock(struct exynos_context *platform)
{
	if (!platform)
		return -1;

	return 0;
}

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

uintptr_t gpu_get_max_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MAX_CLOCK) * 1000;
}

uintptr_t gpu_get_min_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MIN_CLOCK) * 1000;
}

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
#ifdef CONFIG_MALI_DVFS
static int regulator_max_support_volt;
#endif
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
	void __iomem *g3d_status;
	g3d_status = ioremap(GPU_RESET_STATUS, SZ_32);
	return ((__raw_readl(g3d_status) & GPU_RESET_MASK) == GPU_RESET_DONE) ? 1 : 0;
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!pkbdev->clk_gpu) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(pkbdev->clk_gpu)/MHZ;
}



#ifdef CONFIG_MALI_DVFS
static int gpu_disable_clock(struct exynos_context *platform)
{
	int ret = 0;

	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock off in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

	if (pkbdev->clk_gpu) {
		(void)clk_disable_unprepare(pkbdev->clk_gpu);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF, 0u, 0u, "clock is disabled\n");
	}

	platform->clk_g3d_status = 0;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}


static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	unsigned long g3d_rate = clk * KHZ;
	int ret = 0;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the clock-off state!\n", __func__);
		goto err;
	}

	g3d_rate_prev = clk_get_rate(pkbdev->clk_gpu);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {

		if (pkbdev->mout_clk_gpu_switch_user) {
			(void) clk_prepare_enable(pkbdev->mout_clk_gpu_switch_user);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
				"clock is enabled\n");
		}

		ret = clk_set_parent(pkbdev->mout_clk_gpu_busd, pkbdev->mout_clk_gpu_switch_user);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [aclk_gpu_400]\n",
				__func__);
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(pkbdev->clock, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_rate"
				"[fout_g3d_pll]\n", __func__);
			goto err;
		}

		ret = clk_set_parent(pkbdev->mout_clk_gpu_busd, pkbdev->clock);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [fout_g3d_pll]\n",
				__func__);
			goto err;
		}

		if (pkbdev->mout_clk_gpu_switch_user) {
			(void)clk_disable_unprepare(pkbdev->mout_clk_gpu_switch_user);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
				0u, 0u, "clock is disabled\n");
		}

		g3d_rate_prev = g3d_rate;
	}

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (platform->cur_clock != clk_get_rate(pkbdev->mout_clk_gpu_busd_pll)/MHZ)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"clock value is wrong (aclk_gpu: %d,"
			" fout_g3d_pll: %d)\n",
			platform->cur_clock,
			(int) clk_get_rate(pkbdev->mout_clk_gpu_busd_pll)/MHZ);

	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, g3d_rate/MHZ,
		"clock set: %ld\n", g3d_rate/MHZ);
	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, platform->cur_clock,
		"clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
err:
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}
#endif

int gpu_register_dump(void)
{
#ifdef CONFIG_MALI_RT_PM
#ifdef CONFIG_REGULATOR
	if (gpu_is_power_on()) {
#endif
#endif
#ifdef MALI_SEC_INTEGRATION
		/* MCS Value check */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP,  0x10051224 , __raw_readl(EXYNOS7420_VA_SYSREG + 0x1224),
				"REG_DUMP: G3D_EMA_RF2_UHD_CON %x\n", __raw_readl(EXYNOS7420_VA_SYSREG + 0x1224));
		/* G3D PMU */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C4100, __raw_readl(EXYNOS_PMU_G3D_CONFIGURATION),
				"REG_DUMP: EXYNOS_PMU_G3D_CONFIGURATION %x\n", __raw_readl(EXYNOS_PMU_G3D_CONFIGURATION));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C4104, __raw_readl(EXYNOS_PMU_G3D_STATUS),
				"REG_DUMP: EXYNOS_PMU_G3D_STATUS %x\n", __raw_readl(EXYNOS_PMU_G3D_STATUS));
		/* G3D PLL */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C6100, __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL),
				"REG_DUMP: EXYNOS_PMU_GPU_DVS_CTRL %x\n", __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x10576104, __raw_readl(EXYNOS_PMU_GPU_DVS_STATUS),
				"REG_DUMP: GPU_DVS_STATUS %x\n", __raw_readl(EXYNOS_PMU_GPU_DVS_STATUS));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x10051234, __raw_readl(EXYNOS7420_VA_SYSREG + 0x1234),
				"REG_DUMP: G3D_G3DCFG_REG0 %x\n", __raw_readl(EXYNOS7420_VA_SYSREG + 0x1234));
#endif
#ifdef CONFIG_MALI_RT_PM
	} else {
#ifdef CONFIG_REGULATOR
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: Power Status %d\n", __func__, gpu_is_power_on());
#endif
	}
#endif

	return 0;
}

static int gpu_enable_clock(struct exynos_context *platform)
{
	int ret = 0;

	if(!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u,
			"%s: can't set clock on in power off status\n", 	__func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

	if (pkbdev->clk_gpu) {
		(void) clk_prepare_enable(pkbdev->clk_gpu);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u, "clock is enabled\n");
	}

	platform->clk_g3d_status = 1;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform =
		(struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	kbdev->clock = clk_get(kbdev->dev, "mout_clk_gpu_pll");
	if (IS_ERR_OR_NULL(kbdev->clock) || kbdev->clock == NULL) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_clk_gpu_pll]\n", __func__);
		goto err_gpu_pll;
	}

	kbdev->mout_clk_gpu_switch_user = clk_get(kbdev->dev, "mout_clk_gpu_switch_user");
	if (IS_ERR_OR_NULL(kbdev->mout_clk_gpu_switch_user) || kbdev->mout_clk_gpu_switch_user == NULL) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_clk_gpu_switch_user]\n", __func__);
		goto err_gpu_switch;
	}

	kbdev->mout_clk_gpu_busd = clk_get(kbdev->dev, "mout_clk_gpu_busd");
	if (IS_ERR_OR_NULL(kbdev->mout_clk_gpu_busd) || kbdev->mout_clk_gpu_busd == NULL) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_clk_gpu_busd]\n", __func__);
		goto err_gpu_bus;
	}

	kbdev->clk_gpu = clk_get(kbdev->dev, "clk_gpu");
	if (IS_ERR_OR_NULL(kbdev->clk_gpu) || kbdev->clk_gpu == NULL) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [clk_gpu]\n", __func__);
		goto err_gpu;
	}

	gpu_enable_clock(platform);

	return 0;

err_gpu:
	clk_put(kbdev->mout_clk_gpu_busd);

err_gpu_bus:
	clk_put(kbdev->mout_clk_gpu_switch_user);

err_gpu_switch:
	clk_put(kbdev->clock);

err_gpu_pll:
	return -1;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	ret = regulator_get_voltage(g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return ret;
}

#ifdef CONFIG_MALI_DVFS
static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	if (gpu_get_cur_voltage(platform) == vol)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set voltage in the power-off state!\n", __func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	regulator_sync_voltage(g3d_regulator);
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	if (regulator_set_voltage(g3d_regulator, vol, regulator_max_support_volt) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage, voltage: %d\n", __func__, vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage, "voltage set: %d, voltage get:%d\n", vol, platform->cur_voltage);

	return 0;
}

static int gpu_set_voltage_pre(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (!is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}
#endif

static struct gpu_control_ops ctr_ops = {
	.is_power_on = gpu_is_power_on,
#ifdef CONFIG_MALI_DVFS
	.set_voltage = gpu_set_voltage,
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
	.set_clock_to_osc = NULL,
	.set_clock = gpu_set_clock,
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
	.enable_clock = gpu_enable_clock,
	.disable_clock = gpu_disable_clock,
#endif
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
extern int s2m_set_dvs_pin(bool gpio_val);
int gpu_enable_dvs(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_RT_PM
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	int level = 0;
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	if (!platform->dvs_status)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set dvs in the power-off state!\n", __func__);
		return -1;
	}

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	if (!platform->dvs_is_enabled)
	{
		level = gpu_dvfs_get_level(gpu_get_cur_clock(platform));
		exynos_cl_dvfs_stop(ID_G3D, level);
	}
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	/* Do not need to enable dvs during suspending */
	if (!pkbdev->pm.suspending) {
		if (cal_dfs_ext_ctrl(dvfs_g3d, cal_dfs_dvs, 1) != 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable dvs\n", __func__);
			return -1;
		}
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is enabled (vol: %d)\n", gpu_get_cur_voltage(platform));
#endif
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform)
{
	if (!platform->dvs_status)
		return 0;

#ifdef CONFIG_MALI_RT_PM
	if (cal_dfs_ext_ctrl(dvfs_g3d, cal_dfs_dvs, 0) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to disable dvs\n", __func__);
		return -1;
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is disabled (vol: %d)\n", gpu_get_cur_voltage(platform));
#endif
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_DVFS
	int gpu_voltage = 0;

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR_OR_NULL(g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d regulator, 0x%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	regulator_max_support_volt = regulator_get_max_support_voltage(g3d_regulator);
	gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage [%d]\n", __func__, gpu_voltage);
		return -1;
	}

	if (platform->dvs_status)
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs GPIO PMU status enable\n");

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");
#endif

	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}
