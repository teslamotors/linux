/*
 * NVIDIA GPU ID functions, definitions.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _NVGPU_GPUID_T18X_H_
#define _NVGPU_GPUID_T18X_H_

#define NVGPU_GPUID_GP10B \
	GK20A_GPUID(NVGPU_GPU_ARCH_GP100, NVGPU_GPU_IMPL_GP10B)
#define NVGPU_GPUID_GP104 \
	GK20A_GPUID(NVGPU_GPU_ARCH_GP100, NVGPU_GPU_IMPL_GP104)
#define NVGPU_GPUID_GP106 \
	GK20A_GPUID(NVGPU_GPU_ARCH_GP100, NVGPU_GPU_IMPL_GP106)

#define NVGPU_COMPAT_TEGRA_GP10B "nvidia,tegra186-gp10b"
#define NVGPU_COMPAT_GENERIC_GP10B "nvidia,generic-gp10b"

#define TEGRA_18x_GPUID NVGPU_GPUID_GP10B
#define TEGRA_18x_GPUID_HAL gp10b_init_hal
#define TEGRA_18x_GPU_COMPAT_TEGRA NVGPU_COMPAT_TEGRA_GP10B
#define TEGRA_18x_GPU_COMPAT_GENERIC NVGPU_COMPAT_GENERIC_GP10B
#define TEGRA_18x_GPUID2 NVGPU_GPUID_GP104
#define TEGRA_18x_GPUID2_HAL gp106_init_hal
#define TEGRA_18x_GPUID3 NVGPU_GPUID_GP106
#define TEGRA_18x_GPUID3_HAL gp106_init_hal
struct gpu_ops;
extern int gp10b_init_hal(struct gk20a *);
extern int gp106_init_hal(struct gk20a *);
extern struct gk20a_platform t18x_gpu_tegra_platform;

#ifdef CONFIG_TEGRA_GR_VIRTUALIZATION
#define TEGRA_18x_GPUID_VGPU_HAL vgpu_gp10b_init_hal
extern int vgpu_gp10b_init_hal(struct gk20a *);
#endif
#endif
