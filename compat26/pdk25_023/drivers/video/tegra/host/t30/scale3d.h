/*
 * drivers/video/tegra/host/t30/scale3d.h
 *
 * Tegra Graphics Host 3D Clock Scaling
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef NVHOST_T30_SCALE3D_H
#define NVHOST_T30_SCALE3D_H

struct nvhost_module;
struct device;
struct dentry;

/* Initialization and de-initialization for module */
void nvhost_scale3d_init(struct device *, struct nvhost_module *);
void nvhost_scale3d_deinit(struct device *, struct nvhost_module *);

/* Suspend is called when powering down module */
void nvhost_scale3d_suspend(struct nvhost_module *);

/* reset 3d module load counters, called on resume */
void nvhost_scale3d_reset(void);

/*
 * call when performing submit to notify scaling mechanism that 3d module is
 * in use
 */
void nvhost_scale3d_notify_busy(struct nvhost_module *);
void nvhost_scale3d_notify_idle(struct nvhost_module *);

void nvhost_scale3d_debug_init(struct dentry *de);

#endif
