/*
 * arch/arm/mach-tegra/tegra21_init.c
 *
 * NVIDIA Tegra210 initialization support
 *
 * Copyright (C) 2013-2016 NVIDIA Corporation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/tegra-fuse.h>
#include <soc/tegra/tegra_bpmp.h>
#include <mach/powergate.h>

#include <linux/platform/tegra/reset.h>
#include "apbio.h"
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/common.h>
#include "devices.h"
#include "iomap.h"
#include "board.h"

#define MC_SECURITY_CFG2        0x7c

#define AHB_ARBITRATION_PRIORITY_CTRL           0x4
#define   AHB_PRIORITY_WEIGHT(x)        (((x) & 0x7) << 29)
#define   PRIORITY_SELECT_USB   BIT(6)
#define   PRIORITY_SELECT_USB2  BIT(18)
#define   PRIORITY_SELECT_USB3  BIT(17)
#define   PRIORITY_SELECT_SE BIT(14)

#define AHB_GIZMO_AHB_MEM               0xc
#define   ENB_FAST_REARBITRATE  BIT(2)
#define   DONT_SPLIT_AHB_WR     BIT(7)
#define   WR_WAIT_COMMIT_ON_1K  BIT(8)
#define   EN_USB_WAIT_COMMIT_ON_1K_STALL        BIT(9)

#define   RECOVERY_MODE BIT(31)
#define   BOOTLOADER_MODE       BIT(30)
#define   FORCED_RECOVERY_MODE  BIT(1)

#define AHB_GIZMO_USB           0x1c
#define AHB_GIZMO_USB2          0x78
#define AHB_GIZMO_USB3          0x7c
#define AHB_GIZMO_SE            0x4c
#define   IMMEDIATE     BIT(18)

#define AHB_MEM_PREFETCH_CFG3   0xe0
#define AHB_MEM_PREFETCH_CFG4   0xe4
#define AHB_MEM_PREFETCH_CFG1   0xec
#define AHB_MEM_PREFETCH_CFG2   0xf0
#define AHB_MEM_PREFETCH_CFG6   0xcc
#define   PREFETCH_ENB  BIT(31)
#define   MST_ID(x)     (((x) & 0x1f) << 26)
#define   AHBDMA_MST_ID MST_ID(5)
#define   USB_MST_ID    MST_ID(6)
#define   USB2_MST_ID   MST_ID(18)
#define   USB3_MST_ID   MST_ID(17)
#define   SE_MST_ID     MST_ID(14)
#define   ADDR_BNDRY(x) (((x) & 0xf) << 21)
#define   INACTIVITY_TIMEOUT(x) (((x) & 0xffff) << 0)


/* TODO: check  the correct init values */

static __initdata struct tegra_clk_init_table tegra21x_clk_init_table[] = {
	/* name         parent          rate            enabled */
	{ "clk_m",      NULL,           0,              true },
	{ "emc",        NULL,           0,              true },
	{ "cpu",        NULL,           0,              true },
	{ "kfuse",      NULL,           0,              true },
	{ "fuse",       NULL,           0,              true },
	{ "sclk",       NULL,           0,              true },
	{ "pll_p",      NULL,           0,              true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out2", NULL,           204000000,      false, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "sclk_mux",   "pll_p_out2",   204000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "sclk_div",   "sclk_mux",	102000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "hclk",       "sclk",         102000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pclk",       "hclk",         51000000,       true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out3", NULL,           102000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out4", NULL,           204000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out5", NULL,           204000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "host1x",     "pll_p",        102000000,      false, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "mselect",    "pll_p",        102000000,      true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_ref", "pll_p",       51000000,       true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_soc", "pll_p",       51000000,       true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ "csite",      "pll_p",        408000000,      false},
	{ "pll_u_out",  NULL,           0,		true },
	{ "pll_u_out1", NULL,           0,		true },
	{ "pll_u_out2", NULL,           0,		true },
	{ "pll_re_vco", NULL,           624000000,      true },
	{ "pll_re_out", NULL,           624000000,      false },
	{ "xusb_falcon_src",    "pll_re_out",	312000000,      false},
	{ "xusb_host_src",      "pll_p_out_xusb",   102000000,      false},
	{ "xusb_dev_src",       "pll_p_out_xusb",   102000000,      false},
	{ "xusb_ss_src",        "pll_u_480M",       120000000,      false},
	{ "xusb_ssp_src",       "xusb_ss_src",      120000000,      false},
	{ "xusb_hs_src",        "xusb_ss_src",      120000000,       false},
	{ "xusb_fs_src",        "pll_u_48M",        48000000,       false},
	{ "sdmmc1",     "pll_p",        46000000,       false},
	{ "sdmmc2",     "pll_p",        46000000,       false},
	{ "sdmmc3",     "pll_p",        46000000,       false},
	{ "sdmmc4",     "pll_p",        46000000,       false},
	{ "sdmmc1_ddr",	"pll_p",	46000000,	false},
	{ "sdmmc2_ddr",	"pll_p",	46000000,	false},
	{ "sdmmc3_ddr",	"pll_p",	46000000,	false},
	{ "sdmmc4_ddr",	"pll_p",	46000000,	false},
	{ "sdmmc_legacy", "pll_p",	12000000,	false},
	{ "tsec",	"pll_p",	204000000,	false},
	{ "cpu.mselect",	NULL,	102000000,	true},
	{ "gpu_gate",       NULL,           0,              true},
	{ "gpu_ref",        NULL,           0,              true},
	{ "gm20b.gbus",     NULL,           384000000,      false},
	{ "mc_capa",        "mc",           0,              true},
	{ "mc_cbpa",        "mc",           0,              true},
	{ "mc_ccpa",        "mc",           0,              true},
	{ "mc_cdpa",        "mc",           0,              true},
	{ "mc_cpu",	    "mc",           0,              true},
	{ "mc_bbc",	    "mc",           0,              true},
#ifdef CONFIG_TEGRA_PLLM_SCALED
	{ "vi",         "pll_p",        0,              false},
	{ "isp",        "pll_p",        0,              false},
#endif
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",  "pll_p",        51000000,       false },
	{ "tsensor",    "clk_m",        500000,         false },
#endif
	{ "dbgapb",     "clk_m",        9600000,        true },
	{ "ape",	"pll_p",	25500000,	true },
	{ "adma.ape",	NULL,		25500000,	false },
	{ "adsp.ape",	NULL,		25500000,	false },
	{ "xbar.ape",	NULL,		25500000,	false },
	{ "adsp_cpu.abus", NULL,	600000000,	false },
	{ "apb2ape",	NULL,		0,		true },
	{ "maud",	"pll_p",	102000000,	false },
	{ NULL,         NULL,           0,              0},

};

static __initdata struct tegra_clk_init_table tegra21x_cbus_init_table[] = {
	/* Initialize c2bus, c3bus, or cbus at the end of the list
	   * after all the clocks are moved under the proper parents.
	*/
	{ "c2bus",      "pll_c2",       250000000,      false },
	{ "c3bus",      "pll_c3",       250000000,      false },
	{ "cbus",       "pll_c",        250000000,      false },
	{ "pll_c_out1", "pll_c",        100000000,      false },
	{ NULL,         NULL,           0,              0},
};

static __initdata struct tegra_clk_init_table tegra21x_sbus_init_table[] = {
	/* Initialize sbus (system clock) users after cbus init PLLs */
	{ "sbc1.sclk",  NULL,           40000000,       false},
	{ "sbc2.sclk",  NULL,           40000000,       false},
	{ "sbc3.sclk",  NULL,           40000000,       false},
	{ "sbc4.sclk",  NULL,           40000000,       false},
	{ "boot.apb.sclk",  NULL,      136000000,       true },
	{ NULL,		NULL,		0,		0},
};

static __initdata struct tegra_clk_init_table uart_non_si_clk_init_data[] = {
	{ "uarta",	"clk_m",	0,	false},
	{ "uartb",	"clk_m",	0,	true},
	{ "uartc",	"clk_m",	0,	false},
	{ "uartd",	"clk_m",	0,	false},
	{ NULL,		NULL,		0,		0},
};

static void __init tegra_perf_init(void)
{
	u32 reg;

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, PMCR_EL0" : "=r"(reg));
	reg >>= 11;
	reg = (1 << (reg & 0x1f))-1;
	reg |= 0x80000000;
	asm volatile("msr PMINTENCLR_EL1, %0" : : "r"(reg));
	reg = 1;
	asm volatile("msr PMUSERENR_EL0, %0" : : "r"(reg));
#endif
}

static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static void __init tegra_init_power(void)
{
   /* TODO : Do the required power initilizations here */
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	tegra_powergate_partition(TEGRA_POWERGATE_PCIE);
#endif
}

static void __init tegra_init_ahb_gizmo_settings(void)
{
	unsigned long val;

	val = gizmo_readl(AHB_GIZMO_AHB_MEM);
	val |= ENB_FAST_REARBITRATE | IMMEDIATE | DONT_SPLIT_AHB_WR;

	val |= WR_WAIT_COMMIT_ON_1K | EN_USB_WAIT_COMMIT_ON_1K_STALL;
	gizmo_writel(val, AHB_GIZMO_AHB_MEM);

	val = gizmo_readl(AHB_GIZMO_USB);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB);

	val = gizmo_readl(AHB_GIZMO_USB2);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB2);

	val = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	val |= PRIORITY_SELECT_USB | PRIORITY_SELECT_USB2 | PRIORITY_SELECT_USB3
		| AHB_PRIORITY_WEIGHT(7);
	val |= PRIORITY_SELECT_SE;

	gizmo_writel(val, AHB_ARBITRATION_PRIORITY_CTRL);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | AHBDMA_MST_ID |
		ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG1));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG3));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB2_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG4));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG6);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | SE_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG6));
}

void __init tegra21x_init_early(void)
{
	display_tegra_dt_info();
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra21x_init_clocks();
	tegra21x_init_dvfs();
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra21x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra21x_cbus_init_table);
	tegra_clk_init_from_table(tegra21x_sbus_init_table);
	tegra_non_dt_clock_reset_init();
	if (!tegra_platform_is_silicon())
		tegra_clk_init_from_table(uart_non_si_clk_init_data);
	tegra_powergate_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
}
