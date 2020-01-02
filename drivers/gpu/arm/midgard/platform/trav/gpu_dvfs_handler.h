/* drivers/gpu/arm/.../platform/gpu_dvfs_handler.h
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
 * @file gpu_dvfs_handler.h
 * DVFS
 */

#ifndef _GPU_DVFS_HANDLER_H_
#define _GPU_DVFS_HANDLER_H_

#define DVFS_ASSERT(x) \
do { if (x) break; \
	printk(KERN_EMERG "### ASSERTION FAILED %s: %s: %d: %s\n", __FILE__, __func__, __LINE__, #x); dump_stack(); \
} while (0)

typedef enum {
	GPU_DVFS_MAX_LOCK = 0,
	GPU_DVFS_MIN_LOCK,
	GPU_DVFS_MAX_UNLOCK,
	GPU_DVFS_MIN_UNLOCK,
} gpu_dvfs_lock_command;

typedef enum {
	GPU_DVFS_BOOST_SET = 0,
	GPU_DVFS_BOOST_UNSET,
	GPU_DVFS_BOOST_GPU_UNSET,
	GPU_DVFS_BOOST_END,
} gpu_dvfs_boost_command;

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation);
int gpu_dvfs_handler_init(struct kbase_device *kbdev);
int gpu_dvfs_handler_deinit(struct kbase_device *kbdev);

/* gpu_dvfs_api.c */
int gpu_set_target_clk_vol(int clk, bool pending_is_allowed);
int gpu_set_target_clk_vol_pending(int clk);
int gpu_dvfs_boost_lock(gpu_dvfs_boost_command boost_command);
int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock);
void gpu_dvfs_timer_control(bool enable);
int gpu_dvfs_on_off(bool enable);
int gpu_dvfs_governor_change(int governor_type);
int gpu_dvfs_init_time_in_state(void);
int gpu_dvfs_update_time_in_state(int clock);
int gpu_dvfs_get_level(int clock);
int gpu_dvfs_get_level_clock(int clock);
int gpu_dvfs_get_voltage(int clock);
int gpu_dvfs_get_cur_asv_abb(void);
int gpu_dvfs_get_clock(int level);
int gpu_dvfs_get_step(void);

int gpu_dvfs_decide_max_clock(struct exynos_context *platform);

/* gpu_utilization */
int gpu_dvfs_start_env_data_gathering(struct kbase_device *kbdev);
int gpu_dvfs_stop_env_data_gathering(struct kbase_device *kbdev);
int gpu_dvfs_reset_env_data(struct kbase_device *kbdev);
int gpu_dvfs_calculate_env_data(struct kbase_device *kbdev);
int gpu_dvfs_calculate_env_data_ppmu(struct kbase_device *kbdev);
int gpu_dvfs_utilization_init(struct kbase_device *kbdev);
int gpu_dvfs_utilization_deinit(struct kbase_device *kbdev);

/* gpu_pmqos.c */
typedef enum {
	GPU_CONTROL_PM_QOS_INIT = 0,
	GPU_CONTROL_PM_QOS_DEINIT,
	GPU_CONTROL_PM_QOS_SET,
	GPU_CONTROL_PM_QOS_RESET,
	GPU_CONTROL_PM_QOS_EGL_SET,
	GPU_CONTROL_PM_QOS_EGL_RESET,
} gpu_pmqos_state;

int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state);
int gpu_sustainable_pmqos(struct exynos_context *platform, int clock);
int gpu_mif_pmqos(struct exynos_context *platform, int mem_freq);
#ifdef CONFIG_MALI_DVFS_USER
int proactive_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state);
int gpu_mif_min_pmqos(struct exynos_context *platform, int mem_step);
int gpu_int_min_pmqos(struct exynos_context *platform, int int_step);
int gpu_apollo_min_pmqos(struct exynos_context *platform, int apollo_step);
int gpu_atlas_min_pmqos(struct exynos_context *platform, int atlas_step);
int gpu_dvfs_update_hwc(struct kbase_device *kbdev);
#endif
#endif /* _GPU_DVFS_HANDLER_H_ */
