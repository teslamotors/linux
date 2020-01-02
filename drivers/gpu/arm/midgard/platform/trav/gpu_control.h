/* drivers/gpu/arm/.../platform/gpu_control.h
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
 * @file gpu_control.h
 * DVFS
 */

#ifndef _GPU_CONTROL_H_
#define _GPU_CONTROL_H_

struct gpu_control_ops {
	int (*is_power_on)(void);

	int (*set_voltage)(struct exynos_context *platform, int vol);
	int (*set_voltage_pre)(struct exynos_context *platform, bool is_up);
	int (*set_voltage_post)(struct exynos_context *platform, bool is_up);

	int (*set_clock)(struct exynos_context *platform, int clk);
	int (*set_clock_pre)(struct exynos_context *platform, int clk, bool is_up);
	int (*set_clock_post)(struct exynos_context *platform, int clk, bool is_up);
	int (*set_clock_to_osc)(struct exynos_context *platform);

	int (*enable_clock)(struct exynos_context *platform);
	int (*disable_clock)(struct exynos_context *platform);
};

int get_cpu_clock_speed(u32 *cpu_clock);
int gpu_control_set_voltage(struct kbase_device *kbdev, int voltage);
int gpu_control_set_clock(struct kbase_device *kbdev, int clock);
int gpu_control_enable_clock(struct kbase_device *kbdev);
int gpu_control_disable_clock(struct kbase_device *kbdev);
int gpu_control_is_power_on(struct kbase_device *kbdev);

int gpu_is_power_on(void);
int gpu_power_init(struct kbase_device *kbdev);
int gpu_get_cur_voltage(struct exynos_context *platform);
int gpu_get_cur_clock(struct exynos_context *platform);
int gpu_is_clock_on(void);
int gpu_register_dump(void);
int gpu_clock_init(struct kbase_device *kbdev);
struct gpu_control_ops *gpu_get_control_ops(void);

int gpu_control_enable_customization(struct kbase_device *kbdev);
int gpu_control_disable_customization(struct kbase_device *kbdev);

int gpu_enable_dvs(struct exynos_context *platform);
int gpu_disable_dvs(struct exynos_context *platform);

int gpu_regulator_init(struct exynos_context *platform);

int gpu_control_module_init(struct kbase_device *kbdev);
void gpu_control_module_term(struct kbase_device *kbdev);

int gpu_exynos7420_set_rate(struct kbase_device *kbdev, int clk);
void gpu_exynos7420_clock_disable(struct kbase_device *kbdev);
int gpu_device_specific_init(struct kbase_device *kbdev);

int *get_mif_table(int *size);
#endif /* _GPU_CONTROL_H_ */
