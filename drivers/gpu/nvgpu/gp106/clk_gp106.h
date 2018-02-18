/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef CLK_GP106_H
#define CLK_GP106_H

#include <linux/mutex.h>

#define CLK_NAMEMAP_INDEX_GPC2CLK	0x00
#define CLK_NAMEMAP_INDEX_XBAR2CLK	0x02
#define CLK_NAMEMAP_INDEX_SYS2CLK	0x07	/* SYSPLL */
#define CLK_NAMEMAP_INDEX_DRAMCLK	0x20	/* DRAMPLL */

#define CLK_DEFAULT_CNTRL_SETTLE_RETRIES 10
#define CLK_DEFAULT_CNTRL_SETTLE_USECS   5

#define XTAL_CNTR_CLKS		27000	/* 1000usec at 27KHz XTAL */
#define XTAL_CNTR_DELAY		1000	/* we need acuracy up to the ms   */
#define XTAL_SCALE_TO_KHZ	1



struct namemap_cfg {
	u32 namemap;
	u32 is_enable;	/* Namemap enabled */
	u32 is_counter;	/* Using cntr */
	struct gk20a *g;
	union {
		struct {
			u32 reg_ctrl_addr;
			u32 reg_ctrl_idx;
			u32 reg_cntr_addr;
		} cntr;
		struct {
			/* Todo */
		} pll;
	};
	u32 scale;
	char name[24];
};

void gp106_init_clk_ops(struct gpu_ops *gops);

#endif /* CLK_GP106_H */
