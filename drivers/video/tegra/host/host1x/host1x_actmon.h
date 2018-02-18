/*
 * Tegra Graphics Host Actmon
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __HOST1X_ACTMON_H
#define __HOST1X_ACTMON_H

struct dentry;
struct host1x_actmon;

enum init_e {
	ACTMON_OFF = 0,
	ACTMON_READY = 1,
	ACTMON_SLEEP = 2
};

enum type_e {
	ENGINE_ACTMON = 0,
	MAX_ACTMON = 1
};

enum wmark_type_e {
	ACTMON_INTR_ABOVE_WMARK = 1,
	ACTMON_INTR_BELOW_WMARK = 2
};

struct host1x_actmon {
	/* Set to 1 if actmon has been initialized */
	enum init_e init;

	enum type_e type;
	struct nvhost_master *host;
	void __iomem *regs;
	struct clk *clk;

	/* Store actmon period. clks_per_sample can be used even when host1x is
	 * not active. */
	long usecs_per_sample;
	long clks_per_sample;

	int k;
	int divider;
	struct platform_device *pdev;
};

#endif
