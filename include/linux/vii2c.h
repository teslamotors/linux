/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_VII2C_H
#define __LINUX_VII2C_H

#ifdef CONFIG_TEGRA_GRHOST_VII2C

struct platform_device *nvhost_vii2c_open(void);
void nvhost_vii2c_close(void);

int nvhost_vii2c_start(struct platform_device *pdev);
int nvhost_vii2c_end(struct platform_device *pdev);
int nvhost_vii2c_hw_sync_inc(struct platform_device *pdev, int n);
int nvhost_vii2c_flush(struct platform_device *pdev);
int nvhost_vii2c_reset(struct platform_device *pdev);

#else

static inline struct platform_device *nvhost_vii2c_open(void)
{
	return NULL;
}

static inline void nvhost_vii2c_close(void) {}

static inline int nvhost_vii2c_start(struct platform_device *pdev)
{
	return -EINVAL;
}

static inline int nvhost_vii2c_end(struct platform_device *pdev)
{
	return -EINVAL;
}

static inline int nvhost_vii2c_hw_sync_inc(struct platform_device *pdev, int n)
{
	return -EINVAL;
}

static inline int nvhost_vii2c_flush(struct platform_device *pdev)
{
	return -EINVAL;
}

static inline int nvhost_vii2c_reset(struct platform_device *pdev)
{
	return -EINVAL;
}


#endif

#endif
