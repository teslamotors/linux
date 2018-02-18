/*
 * Tegra GPU Virtualization Interfaces to Server
 *
 * Copyright (c) 2015, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_VGPU_T18X_H
#define __TEGRA_VGPU_T18X_H

enum {
	TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_MAIN,
	TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_SPILL,
	TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_PAGEPOOL,
	TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_BETACB,
	TEGRA_VGPU_GR_BIND_CTXSW_BUFFER_LAST
};

enum {
	TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_WFI,
	TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_GFX_GFXP,
	TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_COMPUTE_CTA,
	TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_COMPUTE_CILP,
	TEGRA_VGPU_GR_CTXSW_PREEMPTION_MODE_LAST
};

enum {
	TEGRA_VGPU_ATTRIB_PREEMPT_CTX_SIZE = 64
};

#endif
