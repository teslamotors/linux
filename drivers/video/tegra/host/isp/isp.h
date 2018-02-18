/*
 * Tegra Graphics Host ISP
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __NVHOST_ISP_H__
#define __NVHOST_ISP_H__

#if defined(CONFIG_TEGRA_CAMERA)
#include "camera_priv_defs.h"
#endif
#include <linux/platform/tegra/isomgr.h>

typedef void (*isp_callback)(void *);

struct tegra_isp_mfi {
	struct work_struct work;
};

struct isp {
	struct platform_device *ndev;
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle isomgr_handle;
#endif
	int dev_id;
	void __iomem    *base;
	spinlock_t lock;
	int irq;
	uint max_bw;
	struct workqueue_struct *isp_workqueue;
	struct tegra_isp_mfi *my_isr_work;
};

extern const struct file_operations tegra_isp_ctrl_ops;
int nvhost_isp_t124_finalize_poweron(struct platform_device *);
int nvhost_isp_t124_prepare_poweroff(struct platform_device *);
int nvhost_isp_t210_finalize_poweron(struct platform_device *);
void nvhost_isp_queue_isr_work(struct isp *tegra_isp);

#ifdef CONFIG_TEGRA_GRHOST_ISP
int tegra_isp_register_mfi_cb(isp_callback cb, void *cb_arg);
int tegra_isp_unregister_mfi_cb(void);
#else
static inline int tegra_isp_register_mfi_cb(isp_callback cb, void *cb_arg)
{
	return -ENOSYS;
}
static inline int tegra_isp_unregister_mfi_cb(void)
{
	return -ENOSYS;
}
#endif
#endif
