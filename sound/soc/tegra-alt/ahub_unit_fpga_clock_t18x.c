/*
 * ahub_unit_fpga_clock_t18x.c
 *
 * Author: Mohan Kumar <mkumard@nvidia.com>
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */


#define VERBOSE_DEBUG
#define DEBUG
#include <linux/module.h>
#include <asm/types.h>
#include <linux/io.h>
#include <linux/printk.h>
#include "ahub_unit_fpga_clock.h"
#include "ahub_unit_fpga_clock_t18x.h"


void program_dspk_clk(int dspk_clk)
{
	void __iomem *misc_base;
	struct ahub_unit_fpga *ahub_unit_fpga_private;

	ahub_unit_fpga_private = get_ahub_unit_fpga_private();
	misc_base = ahub_unit_fpga_private->ape_fpga_misc_base;
#if !SYSTEM_FPGA
	writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_I2C1_0);

	switch (dspk_clk) {
	case 256000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0xf, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 512000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x7, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 1024000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 1411200:
		program_cdc_pll(2, CLK_OUT_5_6448_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 1536000:
		program_cdc_pll(2, CLK_OUT_6_1440_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 2048000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 2822400:
		program_cdc_pll(2, CLK_OUT_5_6448_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 3072000:
		program_cdc_pll(2, CLK_OUT_6_1440_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 4096000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 5644800:
		program_cdc_pll(2, CLK_OUT_11_2896_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 6144000:
		program_cdc_pll(2, CLK_OUT_12_2888_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 11289600:
		program_cdc_pll(2, CLK_OUT_11_2896_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	case 12288000:
		program_cdc_pll(2, CLK_OUT_12_2888_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DSPK1_0);
		break;
	default:
		pr_err("Unsupported sample rate and OSR combination\n");
	}
	#endif
}
EXPORT_SYMBOL(program_dspk_clk);
MODULE_LICENSE("GPL");
