/* drivers/gpu/arm/.../platform/gpu_dvfs_api.c
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
 * @file gpu_dvfs_api.c
 * DVFS
 */

#include <mali_kbase.h>
#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

#define GPU_SET_CLK_VOL(kbdev, prev_clk, clk, vol)			\
({			\
	if (prev_clk < clk) {			\
		gpu_control_set_voltage(kbdev, vol);			\
		gpu_control_set_clock(kbdev, clk);			\
	} else {			\
		gpu_control_set_clock(kbdev, clk);			\
		gpu_control_set_voltage(kbdev, vol);			\
	}			\
})

extern struct kbase_device *pkbdev;

static int gpu_check_target_clock(struct exynos_context *platform, int clock)
{
	int target_clock = clock;

	DVFS_ASSERT(platform);

	if (gpu_dvfs_get_level(target_clock) < 0)
		return -1;

#ifdef CONFIG_MALI_DVFS
	if (!platform->dvfs_status)
		return target_clock;

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "clock: %d, min: %d, max: %d\n", clock, platform->min_lock, platform->max_lock);

	if ((platform->min_lock > 0) && (platform->power_status) &&
			((target_clock < platform->min_lock) || (platform->cur_clock < platform->min_lock)))
		target_clock = platform->min_lock;

	if ((platform->max_lock > 0) && (target_clock > platform->max_lock))
		target_clock = platform->max_lock;
#endif /* CONFIG_MALI_DVFS */

	platform->step = gpu_dvfs_get_level(target_clock);

	return target_clock;
}

#ifdef CONFIG_MALI_DVFS
static int gpu_update_cur_level(struct exynos_context *platform)
{
	unsigned long flags;
	int level = 0;

	DVFS_ASSERT(platform);

	level = gpu_dvfs_get_level(platform->cur_clock);
	if (level >= 0) {
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (platform->step != level)
			platform->down_requirement = platform->table[level].down_staycount;
		if (platform->step < level)
			platform->interactive.delay_count = 0;
		platform->step = level;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	} else {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid dvfs level returned %d\n", __func__, platform->cur_clock);
		return -1;
	}
	return 0;
}
#else
#define gpu_update_cur_level(platform) (0)
#endif

int gpu_set_target_clk_vol(int clk, bool pending_is_allowed)
{
	int ret = 0, target_clk = 0, target_vol = 0;
	int prev_clk = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	int level = 0;
#endif

	DVFS_ASSERT(platform);

	if (!gpu_control_is_power_on(pkbdev)) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock and voltage in the power-off state!\n", __func__);
		return -1;
	}

	mutex_lock(&platform->gpu_clock_lock);
#ifdef CONFIG_MALI_DVFS
	if (pending_is_allowed && platform->dvs_is_enabled) {
		if (!platform->dvfs_pending && clk < platform->cur_clock) {
			platform->dvfs_pending = clk;
			GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "pending to change the clock [%d -> %d\n", platform->cur_clock, platform->dvfs_pending);
		} else if (clk > platform->cur_clock) {
			platform->dvfs_pending = 0;
		}
		mutex_unlock(&platform->gpu_clock_lock);
		return 0;
	} else {
		platform->dvfs_pending = 0;
	}

	if (platform->dvs_is_enabled || !platform->power_status) {
		mutex_unlock(&platform->gpu_clock_lock);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't control clock and voltage in dvs and power off %d %d\n",
				__func__,
				platform->dvs_is_enabled,
				platform->power_status);
		return 0;
	}

#endif /* CONFIG_MALI_DVFS */

	target_clk = gpu_check_target_clock(platform, clk);
	if (target_clk < 0) {
		mutex_unlock(&platform->gpu_clock_lock);
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: mismatch clock error (source %d, target %d)\n", __func__, clk, target_clk);
		return -1;
	}

	target_vol = MAX(gpu_dvfs_get_voltage(target_clk) + platform->voltage_margin, platform->cold_min_vol);

	prev_clk = gpu_get_cur_clock(platform);

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	level = gpu_dvfs_get_level(clk);
	exynos7420_cl_dvfs_stop(ID_G3D, level);
#endif

	gpu_sustainable_pmqos(platform, target_clk);
	GPU_SET_CLK_VOL(kbdev, prev_clk, target_clk, target_vol);
	ret = gpu_update_cur_level(platform);

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	if (!platform->voltage_margin
		&& platform->cl_dvfs_start_base && platform->cur_clock >= platform->cl_dvfs_start_base)
		exynos7420_cl_dvfs_start(ID_G3D);
#endif
	mutex_unlock(&platform->gpu_clock_lock);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clk[%d -> %d], vol[%d (margin : %d)]\n",
		prev_clk, gpu_get_cur_clock(platform), gpu_get_cur_voltage(platform), platform->voltage_margin);

	return ret;
}

#ifdef CONFIG_MALI_DVFS
int gpu_set_target_clk_vol_pending(int clk)
{
	int ret = 0, target_clk = 0, target_vol = 0;
	int prev_clk = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	int level = 0;
#endif

	DVFS_ASSERT(platform);

	target_clk = gpu_check_target_clock(platform, clk);
	if (target_clk < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: mismatch clock error (source %d, target %d)\n", __func__, clk, target_clk);
		return -1;
	}

	target_vol = MAX(gpu_dvfs_get_voltage(target_clk) + platform->voltage_margin, platform->cold_min_vol);

	prev_clk = gpu_get_cur_clock(platform);
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	level = gpu_dvfs_get_level(clk);
	exynos7420_cl_dvfs_stop(ID_G3D, level);
#endif

	GPU_SET_CLK_VOL(kbdev, platform->cur_clock, target_clk, target_vol);
	ret = gpu_update_cur_level(platform);
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	if (!platform->voltage_margin && platform->cl_dvfs_start_base
			&& platform->cur_clock >= platform->cl_dvfs_start_base)
		exynos7420_cl_dvfs_start(ID_G3D);
#endif
	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "pending clk[%d -> %d], vol[%d (margin : %d)]\n",
		prev_clk, gpu_get_cur_clock(platform), gpu_get_cur_voltage(platform), platform->voltage_margin);

	return ret;
}

int gpu_dvfs_boost_lock(gpu_dvfs_boost_command boost_command)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		return 0;

	if ((boost_command < GPU_DVFS_BOOST_SET) || (boost_command > GPU_DVFS_BOOST_END)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid boost command is called (%d)\n", __func__, boost_command);
		return -1;
	}

	switch (boost_command) {
	case GPU_DVFS_BOOST_SET:
		platform->boost_is_enabled = true;
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, BOOST_LOCK, platform->boost_gpu_min_lock);
		if (platform->boost_egl_min_lock)
			gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_SET);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is enabled (CPU: %d, GPU %d)\n",
				__func__, platform->boost_egl_min_lock, platform->boost_gpu_min_lock);
		break;
	case GPU_DVFS_BOOST_UNSET:
		platform->boost_is_enabled = false;
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, BOOST_LOCK, 0);
		if (platform->boost_egl_min_lock)
			gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_RESET);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is disabled (CPU: %d, GPU %d)\n",
				__func__, platform->boost_egl_min_lock, platform->boost_gpu_min_lock);
		break;
	case GPU_DVFS_BOOST_GPU_UNSET:
		if (platform->boost_gpu_min_lock)
			gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, BOOST_LOCK, 0);
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: boost mode is disabled (GPU %d)\n",
				__func__, platform->boost_gpu_min_lock);
		break;
	default:
		break;
	}

	return 0;
}

int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	int i;
	bool dirty = false;
	unsigned long flags;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		return 0;

	if ((lock_type < TMU_LOCK) || (lock_type >= NUMBER_LOCK)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid lock type is called (%d)\n", __func__, lock_type);
		return -1;
	}

	switch (lock_command) {
	case GPU_DVFS_MAX_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "max lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_max_lock[lock_type] = clock;
		platform->max_lock = clock;

		if (platform->max_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_max_lock[i] > 0)
					platform->max_lock = MIN(platform->max_lock, platform->user_max_lock[i]);
			}
		} else {
			platform->max_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->max_lock > 0) && (platform->cur_clock >= platform->max_lock))
			gpu_set_target_clk_vol(platform->max_lock, false);

		GPU_LOG(DVFS_DEBUG, LSI_GPU_MAX_LOCK, lock_type, clock,
			"lock max clk[%d], user lock[%d], current clk[%d]\n",
			platform->max_lock, platform->user_max_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MIN_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "min lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_min_lock[lock_type] = clock;
		platform->min_lock = clock;

		if (platform->min_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_min_lock[i] > 0)
					platform->min_lock = MAX(platform->min_lock, platform->user_min_lock[i]);
			}
		} else {
			platform->min_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->min_lock > 0)&& (platform->cur_clock < platform->min_lock)
						&& (platform->min_lock <= platform->max_lock))
			gpu_set_target_clk_vol(platform->min_lock, false);

		GPU_LOG(DVFS_DEBUG, LSI_GPU_MIN_LOCK, lock_type, clock,
			"lock min clk[%d], user lock[%d], current clk[%d]\n",
			platform->min_lock, platform->user_min_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MAX_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_max_lock[lock_type] = 0;
		platform->max_lock = platform->gpu_max_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_max_lock[i] > 0) {
				dirty = true;
				platform->max_lock = MIN(platform->user_max_lock[i], platform->max_lock);
			}
		}

		if (!dirty)
			platform->max_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, LSI_GPU_MAX_LOCK, lock_type, clock, "unlock max clk\n");
		break;
	case GPU_DVFS_MIN_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_min_lock[lock_type] = 0;
		platform->min_lock = platform->gpu_min_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_min_lock[i] > 0) {
				dirty = true;
				platform->min_lock = MAX(platform->user_min_lock[i], platform->min_lock);
			}
		}

		if (!dirty)
			platform->min_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, LSI_GPU_MIN_LOCK, lock_type, clock, "unlock min clk\n");
		break;
	default:
		break;
	}

	return 0;
}

void gpu_dvfs_timer_control(bool enable)
{
	unsigned long flags;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: DVFS is disabled\n", __func__);
		return;
	}
#ifdef CONFIG_MALI_DVFS_USER_GOVERNOR
	if (platform->udvfs_enable)
		return;
#endif
	if (kbdev->pm.backend.metrics.timer_active && !enable) {
		cancel_delayed_work(platform->delayed_work);
		flush_workqueue(platform->dvfs_wq);
	} else if (!kbdev->pm.backend.metrics.timer_active && enable) {
		queue_delayed_work_on(0, platform->dvfs_wq,
				platform->delayed_work, msecs_to_jiffies(platform->polling_speed));
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		platform->down_requirement = platform->table[platform->step].down_staycount;
		platform->interactive.delay_count = 0;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	}

	spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
	kbdev->pm.backend.metrics.timer_active = enable;
	spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
}

int gpu_dvfs_on_off(bool enable)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (enable && !platform->dvfs_status) {
		mutex_lock(&platform->gpu_dvfs_handler_lock);
		gpu_set_target_clk_vol(platform->cur_clock, false);
		gpu_dvfs_handler_init(kbdev);
		mutex_unlock(&platform->gpu_dvfs_handler_lock);

		gpu_dvfs_timer_control(true);
	} else if (!enable && platform->dvfs_status) {
		gpu_dvfs_timer_control(false);

		mutex_lock(&platform->gpu_dvfs_handler_lock);
		gpu_dvfs_handler_deinit(kbdev);
		gpu_set_target_clk_vol(platform->gpu_dvfs_config_clock, false);
		mutex_unlock(&platform->gpu_dvfs_handler_lock);
	} else {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: impossible state to change dvfs status (current: %d, request: %d)\n",
				__func__, platform->dvfs_status, enable);
		return -1;
	}

	return 0;
}

int gpu_dvfs_governor_change(int governor_type)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	gpu_dvfs_governor_setting(platform, governor_type);
	mutex_unlock(&platform->gpu_dvfs_handler_lock);

	return 0;
}
#endif /* CONFIG_MALI_DVFS */

int gpu_dvfs_init_time_in_state(void)
{
#ifdef CONFIG_MALI_DEBUG_SYS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++)
		platform->table[i].time = 0;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_update_time_in_state(int clock)
{
#if defined(CONFIG_MALI_DEBUG_SYS) && defined(CONFIG_MALI_DVFS)
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	u64 current_time;
	static u64 prev_time;
	int level = gpu_dvfs_get_level(clock);

	DVFS_ASSERT(platform);

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	if ((level >= gpu_dvfs_get_level(platform->gpu_max_clock)) && (level <= gpu_dvfs_get_level(platform->gpu_min_clock)))
		platform->table[level].time += current_time-prev_time;

	prev_time = current_time;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_get_level(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	if ((clock < platform->gpu_min_clock) || (clock > platform->gpu_max_clock))
		return -1;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return i;
	}

	return -1;
}

int gpu_dvfs_get_level_clock(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i, min, max;

	DVFS_ASSERT(platform);

	min = gpu_dvfs_get_level(platform->gpu_min_clock);
	max = gpu_dvfs_get_level(platform->gpu_max_clock);

	for (i = max; i <= min; i++)
		if (clock - (int)(platform->table[i].clock) >= 0)
			return platform->table[i].clock;

	return -1;
}

int gpu_dvfs_get_voltage(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return platform->table[i].voltage;
	}

	return -1;
}

int gpu_dvfs_get_cur_asv_abb(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if ((platform->step < 0) || (platform->step >= platform->table_size))
		return 0;

	return platform->table[platform->step].asv_abb;
}

int gpu_dvfs_get_clock(int level)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if ((level < 0) || (level >= platform->table_size))
		return -1;

	return platform->table[level].clock;
}

int gpu_dvfs_get_step(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->table_size;
}

#ifdef CONFIG_MALI_DVFS_USER
#define JOB_GET_SIZE_VAL 0xFFFFFFFF
static bool gpu_dvfs_check_valid_job(gpu_dvfs_job *job)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform;
	bool valid = true;
#ifdef CONFIG_PWRCAL
	struct dvfs_rate_volt rate_volt[48];
	int table_size;
#endif

	platform = kbdev ? (struct exynos_context *) kbdev->platform_context:NULL;
	if (platform == NULL)
		return false;

	switch(job->type)
	{
		case DVFS_REQ_GET_DVFS_TABLE:
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			if (job->data_size != (platform->table_size * sizeof(gpu_dvfs_info))) {
				valid = false;
			}
			break;
		case DVFS_REQ_GET_DVFS_ATTR:
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			if (job->data_size != gpu_get_config_attr_size()) {
				valid = false;
			}
			break;
		case DVFS_REQ_HWC_DUMP:
			if (job->data_size != sizeof(gpu_dvfs_hwc_data)) {
				valid = false;
			}
			break;
		case DVFS_REQ_HWC_SETUP:
			if (job->data_size != sizeof(gpu_dvfs_hwc_setup)) {
				valid = false;
			}
			break;
		case DVFS_REQ_GET_UTILIZATION:
			if (job->data_size != sizeof(gpu_dvfs_env_data)) {
				valid = false;
			}
			break;
		case DVFS_REQ_GET_MIF_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			table_size = cal_dfs_get_rate_asv_table(dvfs_mif, rate_volt);
			if (job->data_size != sizeof(unsigned int) * table_size) {
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get MIF Table size\n", __func__);
				valid = false;
			}
#else
			valid = false;
#endif
			break;
		case DVFS_REQ_GET_INT_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			table_size = cal_dfs_get_rate_asv_table(dvfs_int, rate_volt);
			if (job->data_size != sizeof(unsigned int) * table_size) {
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get INT Table size\n", __func__);
				valid = false;
			}
#else
			valid = false;
#endif
			break;
		case DVFS_REQ_GET_ATLAS_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			table_size = cal_dfs_get_rate_asv_table(dvfs_big, rate_volt);
			if (job->data_size != sizeof(unsigned int) * table_size) {
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get Big Table size\n", __func__);
				valid = false;
			}
#else
			valid = false;
#endif
			break;
		case DVFS_REQ_GET_APOLLO_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL)
				break;
			table_size = cal_dfs_get_rate_asv_table(dvfs_little, rate_volt);
			if (job->data_size != sizeof(unsigned int) * table_size) {
				GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get Little Table size\n", __func__);
				valid = false;
			}
#else
			valid = false;
#endif
			break;
		case DVFS_REQ_SET_LEVEL:
		case DVFS_REQ_GET_LEVEL:
		case DVFS_REQ_GET_GPU_MIN_LOCK:
		case DVFS_REQ_SET_GPU_MIN_LOCK:
		case DVFS_REQ_GET_GPU_MAX_LOCK:
		case DVFS_REQ_SET_GPU_MAX_LOCK:
		case DVFS_REQ_GET_ATLAS_MIN_LOCK:
		case DVFS_REQ_SET_ATLAS_MIN_LOCK:
		case DVFS_REQ_GET_APOLLO_MIN_LOCK:
		case DVFS_REQ_SET_APOLLO_MIN_LOCK:
		case DVFS_REQ_GET_MIF_MIN_LOCK:
		case DVFS_REQ_SET_MIF_MIN_LOCK:
		case DVFS_REQ_GET_INT_MIN_LOCK:
		case DVFS_REQ_SET_INT_MIN_LOCK:
			if (job->data_size != sizeof(int)) {
				valid = false;
			}
			break;
		case DVFS_REQ_REGISTER_CTX:
			break;
		default:
			valid = false;
			break;
	}
	return valid;
}

static inline void gpu_dvfs_notify_info(base_jd_event_code event)
{
	struct kbase_device *kbdev = pkbdev;
	struct kbase_jd_atom *katom;
	struct exynos_context *platform;
	struct kbase_jd_context *jctx;
	struct kbase_context *dvfs_kctx;

	platform = kbdev ? (struct exynos_context *) kbdev->platform_context:NULL;
	if (platform == NULL)
		return;

	dvfs_kctx = platform->dvfs_kctx;
	if (dvfs_kctx == NULL)
		return;

	if (platform->udvfs_enable == false)
		return;

	jctx = &dvfs_kctx->jctx;
	katom = &jctx->atoms[platform->atom_idx++];
	if (platform->atom_idx == DVFS_USER_NOTIFIER_ATOM_NUMBER_MAX)
		platform->atom_idx = DVFS_USER_NOTIFIER_ATOM_NUMBER_BASE;

	mutex_lock(&jctx->lock);

	katom->status = KBASE_JD_ATOM_STATE_COMPLETED;
	katom->kctx = dvfs_kctx;
	katom->extres = NULL;
	katom->coreref_state = KBASE_ATOM_COREREF_STATE_NO_CORES_REQUESTED;
	katom->core_req = BASE_JD_REQ_SOFT_DVFS;
	katom->event_code = event;
	kbase_event_post(platform->dvfs_kctx, katom);

	mutex_unlock(&jctx->lock);
	return;
}

void gpu_dvfs_check_destroy_context(struct kbase_context *kctx)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform;

	platform = kbdev ? (struct exynos_context *) kbdev->platform_context:NULL;
	if (platform == NULL)
		return;

	mutex_lock(&platform->gpu_process_job_lock);
	if (platform->dvfs_kctx == kctx)
	{
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,"gpu_dvfs_check_destroy_context %p\n", kctx);
		platform->dvfs_kctx = NULL;
		kfree(platform->mif_table);
		kfree(platform->int_table);
		kfree(platform->atlas_table);
		kfree(platform->apollo_table);
	}
	mutex_unlock(&platform->gpu_process_job_lock);
}


void gpu_dvfs_notify_poweroff(void)
{
	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u,"gpu_dvfs_notify_poweroff\n");
	gpu_dvfs_notify_info(BASE_JD_EVENT_DVFS_INFO_POWER_OFF);
	return;
}

void gpu_dvfs_notify_poweron(void)
{
	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u,"gpu_dvfs_notify_poweron\n");
	gpu_dvfs_notify_info(BASE_JD_EVENT_DVFS_INFO_POWER_ON);
	return;
}

static void __user *
get_compat_pointer(struct kbase_context *kctx, const union kbase_pointer *p)
{
#ifdef CONFIG_COMPAT
	if (kctx->is_compat)
		return compat_ptr(p->compat_value);
	else
#endif
		return p->value;
}

bool gpu_dvfs_process_job(void *pkatom)
{
	struct kbase_jd_atom *katom = (struct kbase_jd_atom *)pkatom;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform;
	gpu_dvfs_job dvfs_job;
	gpu_dvfs_job *job;
	void __user *data;
	void __user *job_addr;
	int clock, cur_clock, i;
	int level, step;
	unsigned int ret_val = 0;

	platform = kbdev ? (struct exynos_context *) kbdev->platform_context:NULL;
	if (platform == NULL)
		return false;

	mutex_lock(&platform->gpu_process_job_lock);

	job = &dvfs_job;

	job_addr = get_compat_pointer(katom->kctx, (union kbase_pointer *)&katom->jc);
	if (copy_from_user(&dvfs_job, job_addr, sizeof(gpu_dvfs_job)) != 0)
		goto out;

	data = (gpu_dvfs_job __user *)get_compat_pointer(katom->kctx, (union kbase_pointer *)&job->data);

	job->event = DVFS_JOB_EVENT_ERROR;
	katom->core_req |= BASEP_JD_REQ_EVENT_NEVER;

	if (job->data_size && data == NULL) {
		mutex_unlock(&platform->gpu_process_job_lock);
		return false;
	}

	if (gpu_dvfs_check_valid_job(job) == false) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "gpu_dvfs_check_valid_job %d, %u\n", job->type, job->data_size);
		mutex_unlock(&platform->gpu_process_job_lock);
		return false;
	}

	job->event = DVFS_JOB_EVENT_DONE;

	switch(job->type)
	{
		case DVFS_REQ_REGISTER_CTX:
			if (platform->dvfs_kctx == NULL) {
				platform->atom_idx = DVFS_USER_NOTIFIER_ATOM_NUMBER_BASE;
			}
			platform->dvfs_kctx = katom->kctx;
			update_cal_table();
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "DVFS_REQ_REGISTER_CTX 0x%p\n", katom->kctx);
			break;
		case DVFS_REQ_GET_DVFS_TABLE:
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = platform->table_size * sizeof(gpu_dvfs_info);
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			if (copy_to_user(data,  platform->table, job->data_size) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_DVFS_ATTR:
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = gpu_get_config_attr_size();
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			if (copy_to_user(data,  gpu_get_config_attributes(), job->data_size) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_UTILIZATION:
			gpu_dvfs_calculate_env_data(pkbdev);
			mutex_unlock(&kbdev->pm.lock);
			if (copy_to_user(data, &platform->env_data, job->data_size) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_MIF_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = sizeof(unsigned int) * platform->mif_table_size;
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			for (i = 0; i < platform->mif_table_size; i++) {
				ret_val = platform->mif_table[i];
				if (copy_to_user(&((unsigned int*)data)[i], &ret_val, sizeof(int)) != 0)
					goto out;
			}
#else
			goto out;
#endif
			break;
		case DVFS_REQ_GET_INT_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = sizeof(unsigned int) * platform->int_table_size;
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			for (i = 0; i < platform->int_table_size; i++) {
				ret_val = platform->int_table[i];
				if (copy_to_user(&((unsigned int*)data)[i], &ret_val, sizeof(int)) != 0)
					goto out;
			}
#else
			goto out;
#endif
			break;
		case DVFS_REQ_GET_ATLAS_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = sizeof(unsigned int) * platform->atlas_table_size;
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			for (i = 0; i < platform->atlas_table_size; i++) {
				ret_val = platform->atlas_table[i];
				if (copy_to_user(&((unsigned int*)data)[i], &ret_val, sizeof(int)) != 0)
					goto out;
			}
#else
			goto out;
#endif
			break;
		case DVFS_REQ_GET_APOLLO_TABLE:
#ifdef CONFIG_PWRCAL
			if (job->data_size == JOB_GET_SIZE_VAL) {
				ret_val = sizeof(unsigned int) * platform->apollo_table_size;
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}
			for (i = 0; i < platform->apollo_table_size; i++) {
				ret_val = platform->apollo_table[i];
				if (copy_to_user(&((unsigned int*)data)[i], &ret_val, sizeof(int)) != 0)
					goto out;
			}
#else
			goto out;
#endif
			break;
		case DVFS_REQ_SET_LEVEL:
			if (copy_from_user(&level, data, sizeof(int)) != 0)
				goto out;
			clock = gpu_dvfs_get_clock(level);
			gpu_set_target_clk_vol(clock, true);
			cur_clock = platform->cur_clock;

			/* set clock successfully */
			if (clock != cur_clock) {
				if ((platform->max_lock > 0) || (platform->min_lock > 0)) {
					job->event = DVFS_JOB_EVENT_LOCKED;
				} else {
					job->event = DVFS_JOB_EVENT_ERROR;
				}
			}

			ret_val = gpu_dvfs_get_level(cur_clock);
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_GPU_MIN_LOCK:
			step = gpu_dvfs_get_level(platform->min_lock);
			if(step == -1)
				ret_val = gpu_dvfs_get_level(platform->gpu_min_clock);
			else
				ret_val = step;
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;

			break;
		case DVFS_REQ_SET_GPU_MIN_LOCK:
			if (copy_from_user(&level, data, sizeof(int)) != 0)
				goto out;

			if(level == -1)
			{
				gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, USER_LOCK, 0);
				ret_val = gpu_dvfs_get_level(platform->cur_clock);
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}

			clock = gpu_dvfs_get_clock(level);
			if ((clock < platform->gpu_min_clock) || (clock > platform->gpu_max_clock)) {
				GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, clock);
				mutex_unlock(&platform->gpu_process_job_lock);
				return -ENOENT;
			}

			if (clock > platform->gpu_max_clock)
				clock = platform->gpu_max_clock;

			if ((clock == platform->gpu_min_clock) || clock == 0)
				gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, USER_LOCK, 0);
			else
				gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, USER_LOCK, clock);

			ret_val = gpu_dvfs_get_level(clock);
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_GPU_MAX_LOCK:
			ret_val = gpu_dvfs_get_level(platform->gpu_max_clock);
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_SET_GPU_MAX_LOCK:
			if (copy_from_user(&level, data, sizeof(int)) != 0)
				goto out;
			if(level == -1)
			{
				gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, USER_LOCK, 0);
				ret_val = gpu_dvfs_get_level(platform->cur_clock);
				if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
					goto out;
				break;
			}

			clock = gpu_dvfs_get_clock(level);
			if ((clock > platform->gpu_max_clock) || (clock < platform->gpu_min_clock)) {
				GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid clock value (%d)\n", __func__, clock);
				mutex_unlock(&platform->gpu_process_job_lock);
				return -ENOENT;
			}

			if ((clock == platform->gpu_max_clock) || clock == 0)
				gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, USER_LOCK, 0);
			else
				gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, USER_LOCK, clock);

			ret_val = gpu_dvfs_get_level(clock);
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_GET_ATLAS_MIN_LOCK:
			ret_val = platform->apollo_min_step;
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_SET_ATLAS_MIN_LOCK:
			if (copy_from_user(&step, data, sizeof(int)) != 0)
				goto out;
			platform->atlas_min_step = step;
			gpu_atlas_min_pmqos(platform, step == -1 ? 0 : platform->atlas_min_step);
			break;
		case DVFS_REQ_GET_APOLLO_MIN_LOCK:
			ret_val = platform->apollo_min_step;
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_SET_APOLLO_MIN_LOCK:
			if (copy_from_user(&step, data, sizeof(int)) != 0)
				goto out;
			platform->apollo_min_step = step;
			gpu_apollo_min_pmqos(platform, step == -1 ? 0 : platform->apollo_min_step);
			break;
		case DVFS_REQ_GET_MIF_MIN_LOCK:
			ret_val = platform->mif_min_step;
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_SET_MIF_MIN_LOCK:
			if (copy_from_user(&step, data, sizeof(int)) != 0)
				goto out;
			platform->mif_min_step = step;
			gpu_mif_min_pmqos(platform, step == -1 ? 0 : platform->mif_min_step);
			break;
		case DVFS_REQ_GET_INT_MIN_LOCK:
			ret_val = platform->int_min_step;
			if (copy_to_user(data, &ret_val, sizeof(int)) != 0)
				goto out;
			break;
		case DVFS_REQ_SET_INT_MIN_LOCK:
			if (copy_from_user(&step, data, sizeof(int)) != 0)
				goto out;
			platform->int_min_step = step;
			gpu_int_min_pmqos(platform, step == -1 ? 0 : platform->int_min_step);
			break;
		case DVFS_REQ_HWC_SETUP:
		{
			gpu_dvfs_hwc_setup hwc_setup;
			if (copy_from_user(&hwc_setup, data, sizeof(gpu_dvfs_hwc_setup)) != 0)
				goto out;
			printk("DVFS_REQ_HWC_SETUP !!! %d\n", hwc_setup.jm_bm);

			if (hwc_setup.profile_mode)
				gpu_dvfs_notify_info(BASE_JD_EVENT_DVFS_INFO_PROFILE_MODE_ON);
			else
				gpu_dvfs_notify_info(BASE_JD_EVENT_DVFS_INFO_PROFILE_MODE_OFF);

			mutex_lock(&kbdev->pm.lock);
#ifdef MALI_SEC_HWCNT
			platform->hwcnt_choose_jm = kbdev->hwcnt.suspended_state.jm_bm = hwc_setup.jm_bm;
			platform->hwcnt_choose_tiler = kbdev->hwcnt.suspended_state.tiler_bm = hwc_setup.tiler_bm;
			platform->hwcnt_choose_shader = kbdev->hwcnt.suspended_state.shader_bm = hwc_setup.sc_bm;
			platform->hwcnt_choose_mmu_l2 = kbdev->hwcnt.suspended_state.mmu_l2_bm = hwc_setup.memory_bm;
#endif
			mutex_unlock(&kbdev->pm.lock);
		}
			break;
		case DVFS_REQ_HWC_DUMP:
			gpu_dvfs_calculate_env_data(pkbdev);
			platform->hwc_data.data[HWC_DATA_CLOCK] = platform->cur_clock;
			platform->hwc_data.data[HWC_DATA_UTILIZATION] = platform->env_data.utilization;

			if (copy_to_user(data, &platform->hwc_data, job->data_size) != 0)
				goto out;
			break;
		default:
			break;
	}

	mutex_unlock(&platform->gpu_process_job_lock);

	if (copy_to_user(job_addr, &dvfs_job, sizeof(gpu_dvfs_job)) != 0)
		return false;

	return true;

out:
	job->event = 0x1234;
	mutex_unlock(&platform->gpu_process_job_lock);
	if (copy_to_user(job_addr, &dvfs_job, sizeof(gpu_dvfs_job)) != 0)
		return false;

	return false;
}

#ifdef CONFIG_PWRCAL
bool update_cal_table()
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform;
	struct dvfs_rate_volt rate_volt[48];
	int table_size, i;

	platform = kbdev ? (struct exynos_context *) kbdev->platform_context:NULL;
	if (platform == NULL)
		return false;

	/* update mif table */
	table_size = cal_dfs_get_rate_asv_table(dvfs_mif, rate_volt);
	platform->mif_table = kmalloc(sizeof(int) * table_size, GFP_KERNEL);
	for (i = 0; i < table_size; i++) {
		platform->mif_table[i] = rate_volt[i].rate;
	}
	platform->mif_table_size = table_size;
	/* update little table */
	table_size = cal_dfs_get_rate_asv_table(dvfs_little, rate_volt);
	platform->apollo_table = kmalloc(sizeof(int) * table_size, GFP_KERNEL);
	for (i = 0; i < table_size; i++) {
		platform->apollo_table[i] = rate_volt[i].rate;
	}
	platform->apollo_table_size = table_size;
	/* update big table */
	table_size = cal_dfs_get_rate_asv_table(dvfs_big, rate_volt);
	platform->atlas_table = kmalloc(sizeof(int) * table_size, GFP_KERNEL);
	for (i = 0; i < table_size; i++) {
		platform->atlas_table[i] = rate_volt[i].rate;
	}
	platform->atlas_table_size = table_size;
	/* update int table */
	table_size = cal_dfs_get_rate_asv_table(dvfs_int, rate_volt);
	platform->int_table = kmalloc(sizeof(int) * table_size, GFP_KERNEL);
	for (i = 0; i < table_size; i++) {
		platform->int_table[i] = rate_volt[i].rate;
	}
	platform->int_table_size = table_size;

	return true;
}
#endif
#endif
