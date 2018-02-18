/*
 * drivers/video/tegra/host/vi/vi_irq.h
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_VI_IRQ_H
#define __DRIVERS_VIDEO_VI_IRQ_H

#include "camera_priv_defs.h"

int vi_intr_init(struct vi *vi);
int vi_intr_free(struct vi *vi);
void vi_stats_worker(struct work_struct *work);
int vi_enable_irq(struct vi *vi);
int vi_disable_irq(struct vi *vi);
#endif

