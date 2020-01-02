/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Author: Shaik Ameer Basha <shaik.ameer@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk-provider.h>
#include <linux/of.h>

#include "clk.h"
#include <dt-bindings/clock/trav-clk.h>

/* Register Offset definitions for CMU_CMU (0x11c10000) */
#define PLL_LOCKTIME_PLL_SHARED0			0x0
#define PLL_LOCKTIME_PLL_SHARED1			0x4
#define PLL_LOCKTIME_PLL_SHARED2			0x8
#define PLL_LOCKTIME_PLL_SHARED3			0xc
#define PLL_CON0_PLL_SHARED0				0x100
#define PLL_CON0_PLL_SHARED1				0x120
#define PLL_CON0_PLL_SHARED2				0x140
#define PLL_CON0_PLL_SHARED3				0x160
#define MUX_CMU_CIS0_CLKMUX				0x1000
#define MUX_CMU_CIS1_CLKMUX				0x1004
#define MUX_CMU_CIS2_CLKMUX				0x1008
#define MUX_CMU_CPUCL_SWITCHMUX				0x100c
#define MUX_CMU_DPRX_SWITCHCLK_MUX			0x1010
#define MUX_CMU_FSYS1_ACLK_MUX				0x1014
#define MUX_CMU_GPU_SWITCHCLK_MUX			0x1018
#define MUX_CMU_TRIP_SWITCHCLK_MUX			0x101c
#define MUX_PLL_SHARED0_MUX				0x1020
#define MUX_PLL_SHARED1_MUX				0x1024
#define DIV_CMU_CIS0_CLK				0x1800
#define DIV_CMU_CIS1_CLK				0x1804
#define DIV_CMU_CIS2_CLK				0x1808
#define DIV_CMU_CMU_ACLK				0x180c
#define DIV_CMU_CPUCL_SWITCH				0x1810
#define DIV_CMU_DPRX_DSC_CLK				0x1814
#define DIV_CMU_DPRX_SWITCHCLK				0x1818
#define DIV_CMU_FSYS0_SHARED0DIV4			0x181c
#define DIV_CMU_FSYS0_SHARED1DIV3			0x1820
#define DIV_CMU_FSYS0_SHARED1DIV4			0x1824
#define DIV_CMU_FSYS1_SHARED0DIV4			0x1828
#define DIV_CMU_FSYS1_SHARED0DIV8			0x182c
#define DIV_CMU_GPU_MAIN_SWITCH				0x1830
#define DIV_CMU_IMEM_ACLK				0x1834
#define DIV_CMU_IMEM_DMACLK				0x1838
#define DIV_CMU_IMEM_TCUCLK				0x183c
#define DIV_CMU_ISP_TCUCLK				0x1840
#define DIV_CMU_PERIC_SHARED0DIV20			0x1844
#define DIV_CMU_PERIC_SHARED0DIV3_TBUCLK		0x1848
#define DIV_CMU_PERIC_SHARED1DIV36			0x184c
#define DIV_CMU_PERIC_SHARED1DIV4_DMACLK		0x1850
#define DIV_CMU_TRIP_SWITCHCLK				0x1854
#define DIV_PLL_SHARED0_DIV2				0x1858
#define DIV_PLL_SHARED0_DIV3				0x185c
#define DIV_PLL_SHARED0_DIV4				0x1860
#define DIV_PLL_SHARED0_DIV6				0x1864
#define DIV_PLL_SHARED1_DIV3				0x1868
#define DIV_PLL_SHARED1_DIV36				0x186c
#define DIV_PLL_SHARED1_DIV4				0x1870
#define DIV_PLL_SHARED1_DIV9				0x1874
#define GAT_CMU_CIS0_CLKGATE				0x2000
#define GAT_CMU_CIS1_CLKGATE				0x2004
#define GAT_CMU_CIS2_CLKGATE				0x2008
#define GAT_CMU_CPUCL_SWITCH_GATE			0x200c
#define GAT_CMU_DPRX_DSC_CLK_GATE			0x2010
#define GAT_CMU_DPRX_SWITCHCLK_GATE			0x2014
#define GAT_CMU_FSYS0_SHARED0DIV4_GATE			0x2018
#define GAT_CMU_FSYS0_SHARED1DIV4_CLK			0x201c
#define GAT_CMU_FSYS0_SHARED1DIV4_GATE			0x2020
#define GAT_CMU_FSYS1_SHARED0DIV4_GATE			0x2024
#define GAT_CMU_FSYS1_SHARED1DIV4_GATE			0x2028
#define GAT_CMU_GPU_SWITCHCLK_GATE			0x202c
#define GAT_CMU_IMEM_ACLK_GATE				0x2030
#define GAT_CMU_IMEM_DMACLK_GATE			0x2034
#define GAT_CMU_IMEM_TCUCLK_GATE			0x2038
#define GAT_CMU_ISP_TCUCLK_GATE				0x203c
#define GAT_CMU_PERIC_SHARED0DIVE3_TBUCLK_GATE		0x2040
#define GAT_CMU_PERIC_SHARED0DIVE4_GATE			0x2044
#define GAT_CMU_PERIC_SHARED1DIV4_DMACLK_GATE		0x2048
#define GAT_CMU_PERIC_SHARED1DIVE4_GATE			0x204c
#define GAT_CMU_TRIP_SWITCHCLK_GATE			0x2050
#define GAT_CMU_CMU_CMU_IPCLKPORT_PCLK			0x2054
#define GAT_CMU_AXI2APB_CMU_IPCLKPORT_ACLK		0x2058
#define GAT_CMU_NS_BRDG_CMU_IPCLKPORT_CLK__PSOC_CMU__CLK_CMU	0x205c
#define GAT_CMU_SYSREG_CMU_IPCLKPORT_PCLK		0x2060

static const unsigned long cmu_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_SHARED0,
	PLL_LOCKTIME_PLL_SHARED1,
	PLL_LOCKTIME_PLL_SHARED2,
	PLL_LOCKTIME_PLL_SHARED3,
	PLL_CON0_PLL_SHARED0,
	PLL_CON0_PLL_SHARED1,
	PLL_CON0_PLL_SHARED2,
	PLL_CON0_PLL_SHARED3,
	MUX_CMU_CIS0_CLKMUX,
	MUX_CMU_CIS1_CLKMUX,
	MUX_CMU_CIS2_CLKMUX,
	MUX_CMU_CPUCL_SWITCHMUX,
	MUX_CMU_DPRX_SWITCHCLK_MUX,
	MUX_CMU_FSYS1_ACLK_MUX,
	MUX_CMU_GPU_SWITCHCLK_MUX,
	MUX_CMU_TRIP_SWITCHCLK_MUX,
	MUX_PLL_SHARED0_MUX,
	MUX_PLL_SHARED1_MUX,
	DIV_CMU_CIS0_CLK,
	DIV_CMU_CIS1_CLK,
	DIV_CMU_CIS2_CLK,
	DIV_CMU_CMU_ACLK,
	DIV_CMU_CPUCL_SWITCH,
	DIV_CMU_DPRX_DSC_CLK,
	DIV_CMU_DPRX_SWITCHCLK,
	DIV_CMU_FSYS0_SHARED0DIV4,
	DIV_CMU_FSYS0_SHARED1DIV3,
	DIV_CMU_FSYS0_SHARED1DIV4,
	DIV_CMU_FSYS1_SHARED0DIV4,
	DIV_CMU_FSYS1_SHARED0DIV8,
	DIV_CMU_GPU_MAIN_SWITCH,
	DIV_CMU_IMEM_ACLK,
	DIV_CMU_IMEM_DMACLK,
	DIV_CMU_IMEM_TCUCLK,
	DIV_CMU_ISP_TCUCLK,
	DIV_CMU_PERIC_SHARED0DIV20,
	DIV_CMU_PERIC_SHARED0DIV3_TBUCLK,
	DIV_CMU_PERIC_SHARED1DIV36,
	DIV_CMU_PERIC_SHARED1DIV4_DMACLK,
	DIV_CMU_TRIP_SWITCHCLK,
	DIV_PLL_SHARED0_DIV2,
	DIV_PLL_SHARED0_DIV3,
	DIV_PLL_SHARED0_DIV4,
	DIV_PLL_SHARED0_DIV6,
	DIV_PLL_SHARED1_DIV3,
	DIV_PLL_SHARED1_DIV36,
	DIV_PLL_SHARED1_DIV4,
	DIV_PLL_SHARED1_DIV9,
	GAT_CMU_CIS0_CLKGATE,
	GAT_CMU_CIS1_CLKGATE,
	GAT_CMU_CIS2_CLKGATE,
	GAT_CMU_CPUCL_SWITCH_GATE,
	GAT_CMU_DPRX_DSC_CLK_GATE,
	GAT_CMU_DPRX_SWITCHCLK_GATE,
	GAT_CMU_FSYS0_SHARED0DIV4_GATE,
	GAT_CMU_FSYS0_SHARED1DIV4_CLK,
	GAT_CMU_FSYS0_SHARED1DIV4_GATE,
	GAT_CMU_FSYS1_SHARED0DIV4_GATE,
	GAT_CMU_FSYS1_SHARED1DIV4_GATE,
	GAT_CMU_GPU_SWITCHCLK_GATE,
	GAT_CMU_IMEM_ACLK_GATE,
	GAT_CMU_IMEM_DMACLK_GATE,
	GAT_CMU_IMEM_TCUCLK_GATE,
	GAT_CMU_ISP_TCUCLK_GATE,
	GAT_CMU_PERIC_SHARED0DIVE3_TBUCLK_GATE,
	GAT_CMU_PERIC_SHARED0DIVE4_GATE,
	GAT_CMU_PERIC_SHARED1DIV4_DMACLK_GATE,
	GAT_CMU_PERIC_SHARED1DIVE4_GATE,
	GAT_CMU_TRIP_SWITCHCLK_GATE,
	GAT_CMU_CMU_CMU_IPCLKPORT_PCLK,
	GAT_CMU_AXI2APB_CMU_IPCLKPORT_ACLK,
	GAT_CMU_NS_BRDG_CMU_IPCLKPORT_CLK__PSOC_CMU__CLK_CMU,
	GAT_CMU_SYSREG_CMU_IPCLKPORT_PCLK,
};

static const struct samsung_pll_rate_table pll_shared0_rate_table[] __initconst = {
	PLL_35XX_RATE(2000000000, 250, 3, 0),
};

static const struct samsung_pll_rate_table pll_shared1_rate_table[] __initconst = {
	PLL_35XX_RATE(2400000000, 200, 2, 0),
};

static const struct samsung_pll_rate_table pll_shared2_rate_table[] __initconst = {
	PLL_35XX_RATE(2400000000, 200, 2, 0),
};

static const struct samsung_pll_rate_table pll_shared3_rate_table[] __initconst = {
	PLL_35XX_RATE(1800000000, 150, 2, 0),
};

static const struct samsung_pll_clock cmu_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_shared0", "fin_pll", PLL_LOCKTIME_PLL_SHARED0, PLL_CON0_PLL_SHARED0, pll_shared0_rate_table),
	PLL(pll_142xx, 0, "fout_pll_shared1", "fin_pll", PLL_LOCKTIME_PLL_SHARED1, PLL_CON0_PLL_SHARED1, pll_shared1_rate_table),
	PLL(pll_142xx, 0, "fout_pll_shared2", "fin_pll", PLL_LOCKTIME_PLL_SHARED2, PLL_CON0_PLL_SHARED2, pll_shared2_rate_table),
	PLL(pll_142xx, 0, "fout_pll_shared3", "fin_pll", PLL_LOCKTIME_PLL_SHARED3, PLL_CON0_PLL_SHARED3, pll_shared3_rate_table),
};

/* List of parent clocks for Muxes in CMU_CMU */
PNAME(mout_cmu_shared0_pll_p) = { "fin_pll", "fout_pll_shared0" };
PNAME(mout_cmu_shared1_pll_p) = { "fin_pll", "fout_pll_shared1" };
PNAME(mout_cmu_shared2_pll_p) = { "fin_pll", "fout_pll_shared2" };
PNAME(mout_cmu_shared3_pll_p) = { "fin_pll", "fout_pll_shared3" };
PNAME(mout_cmu_cis0_clkmux_p) = { "fin_pll", "dout_cmu_pll_shared0_div4" };
PNAME(mout_cmu_cis1_clkmux_p) = { "fin_pll", "dout_cmu_pll_shared0_div4" };
PNAME(mout_cmu_cis2_clkmux_p) = { "fin_pll", "dout_cmu_pll_shared0_div4" };
PNAME(mout_cmu_cpucl_switchmux_p) = { "mout_cmu_pll_shared2", "mout_cmu_pll_shared0_mux" };
PNAME(mout_cmu_dprx_switchclk_mux_p) = { "dout_cmu_pll_shared0_div3", "dout_cmu_pll_shared1_div3" };
PNAME(mout_cmu_fsys1_aclk_mux_p) = { "dout_cmu_pll_shared0_div4", "fin_pll" };
PNAME(mout_cmu_gpu_switchclk_mux_p) = { "mout_cmu_pll_shared3", "dout_cmu_pll_shared0_div2" };
PNAME(mout_cmu_trip_switchclk_mux_p) = { "mout_cmu_pll_shared3", "mout_cmu_pll_shared2" };
PNAME(mout_cmu_pll_shared0_mux_p) = { "fin_pll", "mout_cmu_pll_shared0" };
PNAME(mout_cmu_pll_shared1_mux_p) = { "fin_pll", "mout_cmu_pll_shared1" };

static const struct samsung_mux_clock cmu_mux_clks[] __initconst = {

	MUX(0, "mout_cmu_pll_shared0", mout_cmu_shared0_pll_p, PLL_CON0_PLL_SHARED0, 4, 1),
	MUX(0, "mout_cmu_pll_shared1", mout_cmu_shared1_pll_p, PLL_CON0_PLL_SHARED1, 4, 1),
	MUX(0, "mout_cmu_pll_shared2", mout_cmu_shared2_pll_p, PLL_CON0_PLL_SHARED2, 4, 1),
	MUX(0, "mout_cmu_pll_shared3", mout_cmu_shared3_pll_p, PLL_CON0_PLL_SHARED3, 4, 1),
	MUX(0, "mout_cmu_cis0_clkmux", mout_cmu_cis0_clkmux_p, MUX_CMU_CIS0_CLKMUX, 0, 1),
	MUX(0, "mout_cmu_cis1_clkmux", mout_cmu_cis1_clkmux_p, MUX_CMU_CIS1_CLKMUX, 0, 1),
	MUX(0, "mout_cmu_cis2_clkmux", mout_cmu_cis2_clkmux_p, MUX_CMU_CIS2_CLKMUX, 0, 1),
	MUX(0, "mout_cmu_cpucl_switchmux", mout_cmu_cpucl_switchmux_p, MUX_CMU_CPUCL_SWITCHMUX, 0, 1),
	MUX(0, "mout_cmu_dprx_switchclk_mux", mout_cmu_dprx_switchclk_mux_p, MUX_CMU_DPRX_SWITCHCLK_MUX, 0, 1),
	MUX(0, "mout_cmu_fsys1_aclk_mux", mout_cmu_fsys1_aclk_mux_p, MUX_CMU_FSYS1_ACLK_MUX, 0, 1),
	MUX(0, "mout_cmu_gpu_switchclk_mux", mout_cmu_gpu_switchclk_mux_p, MUX_CMU_GPU_SWITCHCLK_MUX, 0, 1),
	MUX(0, "mout_cmu_trip_switchclk_mux", mout_cmu_trip_switchclk_mux_p, MUX_CMU_TRIP_SWITCHCLK_MUX, 0, 1),
	MUX(0, "mout_cmu_pll_shared0_mux", mout_cmu_pll_shared0_mux_p, MUX_PLL_SHARED0_MUX, 0, 1),
	MUX(0, "mout_cmu_pll_shared1_mux", mout_cmu_pll_shared1_mux_p, MUX_PLL_SHARED1_MUX, 0, 1),
};

static const struct samsung_div_clock cmu_div_clks[] __initconst = {
	DIV(0, "dout_cmu_cis0_clk", "cmu_cis0_clkgate", DIV_CMU_CIS0_CLK, 0, 4),
	DIV(0, "dout_cmu_cis1_clk", "cmu_cis1_clkgate", DIV_CMU_CIS1_CLK, 0, 4),
	DIV(0, "dout_cmu_cis2_clk", "cmu_cis2_clkgate", DIV_CMU_CIS2_CLK, 0, 4),
	DIV(0, "dout_cmu_cmu_aclk", "dout_cmu_pll_shared1_div9", DIV_CMU_CMU_ACLK, 0, 4),
	DIV(0, "dout_cmu_cpucl_switch", "cmu_cpucl_switch_gate", DIV_CMU_CPUCL_SWITCH, 0, 4),
	DIV(DOUT_CMU_DPRX_DSC_CLK, "dout_cmu_dprx_dsc_clk", "cmu_dprx_dsc_clk_gate", DIV_CMU_DPRX_DSC_CLK, 0, 4),
	DIV(DOUT_CMU_DPRX_SWITCHCLK, "dout_cmu_dprx_switchclk", "cmu_dprx_switchclk_gate", DIV_CMU_DPRX_SWITCHCLK, 0, 4),
	DIV(DOUT_CMU_FSYS0_SHARED0DIV4, "dout_cmu_fsys0_shared0div4", "cmu_fsys0_shared0div4_gate", DIV_CMU_FSYS0_SHARED0DIV4, 0, 4),
	DIV(0, "dout_cmu_fsys0_shared1div3", "cmu_fsys0_shared1div4_clk", DIV_CMU_FSYS0_SHARED1DIV3, 0, 4),
	DIV(DOUT_CMU_FSYS0_SHARED1DIV4, "dout_cmu_fsys0_shared1div4", "cmu_fsys0_shared1div4_gate", DIV_CMU_FSYS0_SHARED1DIV4, 0, 4),
	DIV(DOUT_CMU_FSYS1_SHARED0DIV4, "dout_cmu_fsys1_shared0div4", "cmu_fsys1_shared0div4_gate", DIV_CMU_FSYS1_SHARED0DIV4, 0, 4),
	DIV(DOUT_CMU_FSYS1_SHARED0DIV8, "dout_cmu_fsys1_shared0div8", "cmu_fsys1_shared1div4_gate", DIV_CMU_FSYS1_SHARED0DIV8, 0, 4),
	DIV(DOUT_CMU_GPU_MAIN_SWITCH, "dout_cmu_gpu_main_switch", "cmu_gpu_switchclk_gate", DIV_CMU_GPU_MAIN_SWITCH, 0, 4),
	DIV(DOUT_CMU_IMEM_ACLK, "dout_cmu_imem_aclk", "cmu_imem_aclk_gate", DIV_CMU_IMEM_ACLK, 0, 4),
	DIV(DOUT_CMU_IMEM_DMACLK, "dout_cmu_imem_dmaclk", "cmu_imem_dmaclk_gate", DIV_CMU_IMEM_DMACLK, 0, 4),
	DIV(DOUT_CMU_IMEM_TCUCLK, "dout_cmu_imem_tcuclk", "cmu_imem_tcuclk_gate", DIV_CMU_IMEM_TCUCLK, 0, 4),
	DIV(DOUT_CMU_ISP_TCUCLK, "dout_cmu_isp_tcuclk", "cmu_isp_tcuclk_gate", DIV_CMU_ISP_TCUCLK, 0, 4),
	DIV(DOUT_CMU_PERIC_SHARED0DIV20, "dout_cmu_peric_shared0div20", "cmu_peric_shared0dive4_gate", DIV_CMU_PERIC_SHARED0DIV20, 0, 4),
	DIV(DOUT_CMU_PERIC_SHARED0DIV3_TBUCLK, "dout_cmu_peric_shared0div3_tbuclk", "cmu_peric_shared0dive3_tbuclk_gate", DIV_CMU_PERIC_SHARED0DIV3_TBUCLK, 0, 4),
	DIV(DOUT_CMU_PERIC_SHARED1DIV36, "dout_cmu_peric_shared1div36", "cmu_peric_shared1dive4_gate", DIV_CMU_PERIC_SHARED1DIV36, 0, 4),
	DIV(DOUT_CMU_PERIC_SHARED1DIV4_DMACLK, "dout_cmu_peric_shared1div4_dmaclk", "cmu_peric_shared1div4_dmaclk_gate", DIV_CMU_PERIC_SHARED1DIV4_DMACLK, 0, 4),
	DIV(0, "dout_cmu_trip_switchclk", "cmu_trip_switchclk_gate", DIV_CMU_TRIP_SWITCHCLK, 0, 4),
	DIV(0, "dout_cmu_pll_shared0_div2", "mout_cmu_pll_shared0_mux", DIV_PLL_SHARED0_DIV2, 0, 4),
	DIV(0, "dout_cmu_pll_shared0_div3", "mout_cmu_pll_shared0_mux", DIV_PLL_SHARED0_DIV3, 0, 4),
	DIV(DOUT_CMU_PLL_SHARED0_DIV4, "dout_cmu_pll_shared0_div4", "dout_cmu_pll_shared0_div2", DIV_PLL_SHARED0_DIV4, 0, 4),
	DIV(DOUT_CMU_PLL_SHARED0_DIV6, "dout_cmu_pll_shared0_div6", "dout_cmu_pll_shared0_div3", DIV_PLL_SHARED0_DIV6, 0, 4),
	DIV(0, "dout_cmu_pll_shared1_div3", "mout_cmu_pll_shared1_mux", DIV_PLL_SHARED1_DIV3, 0, 4),
	DIV(0, "dout_cmu_pll_shared1_div36", "dout_cmu_pll_shared1_div9", DIV_PLL_SHARED1_DIV36, 0, 4),
	DIV(0, "dout_cmu_pll_shared1_div4", "mout_cmu_pll_shared1_mux", DIV_PLL_SHARED1_DIV4, 0, 4),
	DIV(0, "dout_cmu_pll_shared1_div9", "dout_cmu_pll_shared1_div3", DIV_PLL_SHARED1_DIV9, 0, 4),
};

static const struct samsung_gate_clock cmu_gate_clks[] __initconst = {
	GATE(0, "cmu_cis0_clkgate", "mout_cmu_cis0_clkmux", GAT_CMU_CIS0_CLKGATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_cis1_clkgate", "mout_cmu_cis1_clkmux", GAT_CMU_CIS1_CLKGATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_cis2_clkgate", "mout_cmu_cis2_clkmux", GAT_CMU_CIS2_CLKGATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(CMU_CPUCL_SWITCH_GATE, "cmu_cpucl_switch_gate", "mout_cmu_cpucl_switchmux", GAT_CMU_CPUCL_SWITCH_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_dprx_dsc_clk_gate", "dout_cmu_pll_shared1_div4", GAT_CMU_DPRX_DSC_CLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_dprx_switchclk_gate", "mout_cmu_dprx_switchclk_mux", GAT_CMU_DPRX_SWITCHCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(GAT_CMU_FSYS0_SHARED0DIV4, "cmu_fsys0_shared0div4_gate", "dout_cmu_pll_shared0_div4", GAT_CMU_FSYS0_SHARED0DIV4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_fsys0_shared1div4_clk", "dout_cmu_pll_shared1_div3", GAT_CMU_FSYS0_SHARED1DIV4_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_fsys0_shared1div4_gate", "dout_cmu_pll_shared1_div4", GAT_CMU_FSYS0_SHARED1DIV4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_fsys1_shared0div4_gate", "mout_cmu_fsys1_aclk_mux", GAT_CMU_FSYS1_SHARED0DIV4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_fsys1_shared1div4_gate", "dout_cmu_fsys1_shared0div4", GAT_CMU_FSYS1_SHARED1DIV4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_gpu_switchclk_gate", "mout_cmu_gpu_switchclk_mux", GAT_CMU_GPU_SWITCHCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_imem_aclk_gate", "dout_cmu_pll_shared1_div9", GAT_CMU_IMEM_ACLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_imem_dmaclk_gate", "mout_cmu_pll_shared1_mux", GAT_CMU_IMEM_DMACLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_imem_tcuclk_gate", "dout_cmu_pll_shared0_div3", GAT_CMU_IMEM_TCUCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_isp_tcuclk_gate", "dout_cmu_pll_shared1_div3", GAT_CMU_ISP_TCUCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_peric_shared0dive3_tbuclk_gate", "dout_cmu_pll_shared0_div3", GAT_CMU_PERIC_SHARED0DIVE3_TBUCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_peric_shared0dive4_gate", "dout_cmu_pll_shared0_div4", GAT_CMU_PERIC_SHARED0DIVE4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_peric_shared1div4_dmaclk_gate", "dout_cmu_pll_shared1_div4", GAT_CMU_PERIC_SHARED1DIV4_DMACLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_peric_shared1dive4_gate", "dout_cmu_pll_shared1_div36", GAT_CMU_PERIC_SHARED1DIVE4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_trip_switchclk_gate", "mout_cmu_trip_switchclk_mux", GAT_CMU_TRIP_SWITCHCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_uid_cmu_cmu_cmu_ipclkport_pclk", "dout_cmu_cmu_aclk", GAT_CMU_CMU_CMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_uid_axi2apb_cmu_ipclkport_aclk", "dout_cmu_cmu_aclk", GAT_CMU_AXI2APB_CMU_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_uid_ns_brdg_cmu_ipclkport_clk__psoc_cmu__clk_cmu", "dout_cmu_cmu_aclk", GAT_CMU_NS_BRDG_CMU_IPCLKPORT_CLK__PSOC_CMU__CLK_CMU, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cmu_uid_sysreg_cmu_ipclkport_pclk", "dout_cmu_cmu_aclk", GAT_CMU_SYSREG_CMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info cmu_cmu_info __initconst = {
	.pll_clks		= cmu_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(cmu_pll_clks),
	.mux_clks		= cmu_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(cmu_mux_clks),
	.div_clks		= cmu_div_clks,
	.nr_div_clks		= ARRAY_SIZE(cmu_div_clks),
	.gate_clks		= cmu_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(cmu_gate_clks),
	.nr_clk_ids		= CMU_NR_CLK,
	.clk_regs		= cmu_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(cmu_clk_regs),
};

static void __init trav_clk_cmu_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &cmu_cmu_info);
}

CLK_OF_DECLARE(trav_clk_cmu, "turbo,trav-clock-cmu", trav_clk_cmu_init);


/* Register Offset definitions for CMU_PERIC (0x14010000) */
#define PLL_CON0_PERIC_DMACLK_MUX		0x100
#define PLL_CON0_PERIC_EQOS_BUSCLK_MUX		0x120
#define PLL_CON0_PERIC_PCLK_MUX			0x140
#define PLL_CON0_PERIC_TBUCLK_MUX		0x160
#define PLL_CON0_SPI_CLK			0x180
#define PLL_CON0_SPI_PCLK			0x1a0
#define PLL_CON0_UART_CLK			0x1c0
#define PLL_CON0_UART_PCLK			0x1e0
#define MUX_PERIC_EQOS_PHYRXCLK			0x1000
#define DIV_EQOS_BUSCLK				0x1800
#define DIV_PERIC_MCAN_CLK			0x1804
#define DIV_RGMII_CLK				0x1808
#define DIV_RII_CLK				0x180c
#define DIV_RMII_CLK				0x1810
#define DIV_SPI_CLK				0x1814
#define DIV_UART_CLK				0x1818
#define GAT_EQOS_TOP_IPCLKPORT_CLK_PTP_REF_I	0x2000
#define GAT_GPIO_PERIC_IPCLKPORT_OSCCLK		0x2004
#define GAT_PERIC_ADC0_IPCLKPORT_I_OSCCLK	0x2008
#define GAT_PERIC_CMU_PERIC_IPCLKPORT_PCLK	0x200c
#define GAT_PERIC_PWM0_IPCLKPORT_I_OSCCLK	0x2010
#define GAT_PERIC_PWM1_IPCLKPORT_I_OSCCLK	0x2014
#define GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKM	0x2018
#define GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKS	0x201c
#define GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKM	0x2020
#define GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKS	0x2024
#define GAT_AXI2APB_PERIC0_IPCLKPORT_ACLK	0x2028
#define GAT_AXI2APB_PERIC1_IPCLKPORT_ACLK	0x202c
#define GAT_AXI2APB_PERIC2_IPCLKPORT_ACLK	0x2030
#define GAT_BUS_D_PERIC_IPCLKPORT_DMACLK	0x2034
#define GAT_BUS_D_PERIC_IPCLKPORT_EQOSCLK	0x2038
#define GAT_BUS_D_PERIC_IPCLKPORT_MAINCLK	0x203c
#define GAT_BUS_P_PERIC_IPCLKPORT_EQOSCLK	0x2040
#define GAT_BUS_P_PERIC_IPCLKPORT_MAINCLK	0x2044
#define GAT_BUS_P_PERIC_IPCLKPORT_SMMUCLK	0x2048
#define GAT_EQOS_TOP_IPCLKPORT_ACLK_I		0x204c
#define GAT_EQOS_TOP_IPCLKPORT_CLK_RX_I		0x2050
#define GAT_EQOS_TOP_IPCLKPORT_HCLK_I		0x2054
#define GAT_EQOS_TOP_IPCLKPORT_RGMII_CLK_I	0x2058
#define GAT_EQOS_TOP_IPCLKPORT_RII_CLK_I	0x205c
#define GAT_EQOS_TOP_IPCLKPORT_RMII_CLK_I	0x2060
#define GAT_GPIO_PERIC_IPCLKPORT_PCLK		0x2064
#define GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_D	0x2068
#define GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_P	0x206c
#define GAT_PERIC_ADC0_IPCLKPORT_PCLK_S0	0x2070
#define GAT_PERIC_DMA0_IPCLKPORT_ACLK		0x2074
#define GAT_PERIC_DMA1_IPCLKPORT_ACLK		0x2078
#define GAT_PERIC_I2C0_IPCLKPORT_I_PCLK		0x207c
#define GAT_PERIC_I2C1_IPCLKPORT_I_PCLK		0x2080
#define GAT_PERIC_I2C2_IPCLKPORT_I_PCLK		0x2084
#define GAT_PERIC_I2C3_IPCLKPORT_I_PCLK		0x2088
#define GAT_PERIC_I2C4_IPCLKPORT_I_PCLK		0x208c
#define GAT_PERIC_I2C5_IPCLKPORT_I_PCLK		0x2090
#define GAT_PERIC_I2C6_IPCLKPORT_I_PCLK		0x2094
#define GAT_PERIC_I2C7_IPCLKPORT_I_PCLK		0x2098
#define GAT_PERIC_MCAN0_IPCLKPORT_CCLK		0x209c
#define GAT_PERIC_MCAN0_IPCLKPORT_PCLK		0x20a0
#define GAT_PERIC_MCAN1_IPCLKPORT_CCLK		0x20a4
#define GAT_PERIC_MCAN1_IPCLKPORT_PCLK		0x20a8
#define GAT_PERIC_MCAN2_IPCLKPORT_CCLK		0x20ac
#define GAT_PERIC_MCAN2_IPCLKPORT_PCLK		0x20b0
#define GAT_PERIC_MCAN3_IPCLKPORT_CCLK		0x20b4
#define GAT_PERIC_MCAN3_IPCLKPORT_PCLK		0x20b8
#define GAT_PERIC_PWM0_IPCLKPORT_I_PCLK_S0	0x20bc
#define GAT_PERIC_PWM1_IPCLKPORT_I_PCLK_S0	0x20c0
#define GAT_PERIC_SMMU_IPCLKPORT_CCLK		0x20c4
#define GAT_PERIC_SMMU_IPCLKPORT_PERIC_BCLK	0x20c8
#define GAT_PERIC_SPI0_IPCLKPORT_I_PCLK		0x20cc
#define GAT_PERIC_SPI0_IPCLKPORT_I_SCLK_SPI	0x20d0
#define GAT_PERIC_SPI1_IPCLKPORT_I_PCLK		0x20d4
#define GAT_PERIC_SPI1_IPCLKPORT_I_SCLK_SPI	0x20d8
#define GAT_PERIC_SPI2_IPCLKPORT_I_PCLK		0x20dc
#define GAT_PERIC_SPI2_IPCLKPORT_I_SCLK_SPI	0x20e0
#define GAT_PERIC_TDM0_IPCLKPORT_HCLK_M		0x20e4
#define GAT_PERIC_TDM0_IPCLKPORT_PCLK		0x20e8
#define GAT_PERIC_TDM1_IPCLKPORT_HCLK_M		0x20ec
#define GAT_PERIC_TDM1_IPCLKPORT_PCLK		0x20f0
#define GAT_PERIC_UART0_IPCLKPORT_I_SCLK_UART	0x20f4
#define GAT_PERIC_UART0_IPCLKPORT_PCLK		0x20f8
#define GAT_PERIC_UART1_IPCLKPORT_I_SCLK_UART	0x20fc
#define GAT_PERIC_UART1_IPCLKPORT_PCLK		0x2100
#define GAT_SYSREG_PERI_IPCLKPORT_PCLK		0x2104

static const unsigned long peric_clk_regs[] __initconst = {
	PLL_CON0_PERIC_DMACLK_MUX,
	PLL_CON0_PERIC_EQOS_BUSCLK_MUX,
	PLL_CON0_PERIC_PCLK_MUX,
	PLL_CON0_PERIC_TBUCLK_MUX,
	PLL_CON0_SPI_CLK,
	PLL_CON0_SPI_PCLK,
	PLL_CON0_UART_CLK,
	PLL_CON0_UART_PCLK,
	MUX_PERIC_EQOS_PHYRXCLK,
	DIV_EQOS_BUSCLK,
	DIV_PERIC_MCAN_CLK,
	DIV_RGMII_CLK,
	DIV_RII_CLK,
	DIV_RMII_CLK,
	DIV_SPI_CLK,
	DIV_UART_CLK,
	GAT_EQOS_TOP_IPCLKPORT_CLK_PTP_REF_I,
	GAT_GPIO_PERIC_IPCLKPORT_OSCCLK,
	GAT_PERIC_ADC0_IPCLKPORT_I_OSCCLK,
	GAT_PERIC_CMU_PERIC_IPCLKPORT_PCLK,
	GAT_PERIC_PWM0_IPCLKPORT_I_OSCCLK,
	GAT_PERIC_PWM1_IPCLKPORT_I_OSCCLK,
	GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKM,
	GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKS,
	GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKM,
	GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKS,
	GAT_AXI2APB_PERIC0_IPCLKPORT_ACLK,
	GAT_AXI2APB_PERIC1_IPCLKPORT_ACLK,
	GAT_AXI2APB_PERIC2_IPCLKPORT_ACLK,
	GAT_BUS_D_PERIC_IPCLKPORT_DMACLK,
	GAT_BUS_D_PERIC_IPCLKPORT_EQOSCLK,
	GAT_BUS_D_PERIC_IPCLKPORT_MAINCLK,
	GAT_BUS_P_PERIC_IPCLKPORT_EQOSCLK,
	GAT_BUS_P_PERIC_IPCLKPORT_MAINCLK,
	GAT_BUS_P_PERIC_IPCLKPORT_SMMUCLK,
	GAT_EQOS_TOP_IPCLKPORT_ACLK_I,
	GAT_EQOS_TOP_IPCLKPORT_CLK_RX_I,
	GAT_EQOS_TOP_IPCLKPORT_HCLK_I,
	GAT_EQOS_TOP_IPCLKPORT_RGMII_CLK_I,
	GAT_EQOS_TOP_IPCLKPORT_RII_CLK_I,
	GAT_EQOS_TOP_IPCLKPORT_RMII_CLK_I,
	GAT_GPIO_PERIC_IPCLKPORT_PCLK,
	GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_D,
	GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_P,
	GAT_PERIC_ADC0_IPCLKPORT_PCLK_S0,
	GAT_PERIC_DMA0_IPCLKPORT_ACLK,
	GAT_PERIC_DMA1_IPCLKPORT_ACLK,
	GAT_PERIC_I2C0_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C1_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C2_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C3_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C4_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C5_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C6_IPCLKPORT_I_PCLK,
	GAT_PERIC_I2C7_IPCLKPORT_I_PCLK,
	GAT_PERIC_MCAN0_IPCLKPORT_CCLK,
	GAT_PERIC_MCAN0_IPCLKPORT_PCLK,
	GAT_PERIC_MCAN1_IPCLKPORT_CCLK,
	GAT_PERIC_MCAN1_IPCLKPORT_PCLK,
	GAT_PERIC_MCAN2_IPCLKPORT_CCLK,
	GAT_PERIC_MCAN2_IPCLKPORT_PCLK,
	GAT_PERIC_MCAN3_IPCLKPORT_CCLK,
	GAT_PERIC_MCAN3_IPCLKPORT_PCLK,
	GAT_PERIC_PWM0_IPCLKPORT_I_PCLK_S0,
	GAT_PERIC_PWM1_IPCLKPORT_I_PCLK_S0,
	GAT_PERIC_SMMU_IPCLKPORT_CCLK,
	GAT_PERIC_SMMU_IPCLKPORT_PERIC_BCLK,
	GAT_PERIC_SPI0_IPCLKPORT_I_PCLK,
	GAT_PERIC_SPI0_IPCLKPORT_I_SCLK_SPI,
	GAT_PERIC_SPI1_IPCLKPORT_I_PCLK,
	GAT_PERIC_SPI1_IPCLKPORT_I_SCLK_SPI,
	GAT_PERIC_SPI2_IPCLKPORT_I_PCLK,
	GAT_PERIC_SPI2_IPCLKPORT_I_SCLK_SPI,
	GAT_PERIC_TDM0_IPCLKPORT_HCLK_M,
	GAT_PERIC_TDM0_IPCLKPORT_PCLK,
	GAT_PERIC_TDM1_IPCLKPORT_HCLK_M,
	GAT_PERIC_TDM1_IPCLKPORT_PCLK,
	GAT_PERIC_UART0_IPCLKPORT_I_SCLK_UART,
	GAT_PERIC_UART0_IPCLKPORT_PCLK,
	GAT_PERIC_UART1_IPCLKPORT_I_SCLK_UART,
	GAT_PERIC_UART1_IPCLKPORT_PCLK,
	GAT_SYSREG_PERI_IPCLKPORT_PCLK,
};

static const struct samsung_fixed_rate_clock peric_fixed_clks[] __initconst = {
	FRATE(PERIC_EQOS_PHYRXCLK, "eqos_phyrxclk", NULL, 0, 125000000),
};

/* List of parent clocks for Muxes in CMU_PERIC */
PNAME(mout_peric_dmaclk_p) = { "fin_pll", "cmu_peric_shared1div4_dmaclk_gate" };
PNAME(mout_peric_eqos_busclk_p) = { "fin_pll", "dout_cmu_pll_shared0_div4" };
PNAME(mout_peric_pclk_p) = { "fin_pll", "dout_cmu_peric_shared1div36" };
PNAME(mout_peric_tbuclk_p) = { "fin_pll", "dout_cmu_peric_shared0div3_tbuclk" };
PNAME(mout_peric_spi_clk_p) = { "fin_pll", "dout_cmu_peric_shared0div20" };
PNAME(mout_peric_spi_pclk_p) = { "fin_pll", "dout_cmu_peric_shared1div36" };
PNAME(mout_peric_uart_clk_p) = { "fin_pll", "dout_cmu_peric_shared1div4_dmaclk" };
PNAME(mout_peric_uart_pclk_p) = { "fin_pll", "dout_cmu_peric_shared1div36" };
PNAME(mout_peric_eqos_phyrxclk_p) = { "dout_peric_rgmii_clk", "eqos_phyrxclk" };

static const struct samsung_mux_clock peric_mux_clks[] __initconst = {
	MUX(0, "mout_peric_dmaclk", mout_peric_dmaclk_p, PLL_CON0_PERIC_DMACLK_MUX, 4, 1),
	MUX(0, "mout_peric_eqos_busclk", mout_peric_eqos_busclk_p, PLL_CON0_PERIC_EQOS_BUSCLK_MUX, 4, 1),
	MUX(0, "mout_peric_pclk", mout_peric_pclk_p, PLL_CON0_PERIC_PCLK_MUX, 4, 1),
	MUX(0, "mout_peric_tbuclk", mout_peric_tbuclk_p, PLL_CON0_PERIC_TBUCLK_MUX, 4, 1),
	MUX(0, "mout_peric_spi_clk", mout_peric_spi_clk_p, PLL_CON0_SPI_CLK, 4, 1),
	MUX(0, "mout_peric_spi_pclk", mout_peric_spi_pclk_p, PLL_CON0_SPI_PCLK, 4, 1),
	MUX(0, "mout_peric_uart_clk", mout_peric_uart_clk_p, PLL_CON0_UART_CLK, 4, 1),
	MUX(0, "mout_peric_uart_pclk", mout_peric_uart_pclk_p, PLL_CON0_UART_PCLK, 4, 1),
	MUX(PERIC_EQOS_PHYRXCLK_MUX, "mout_peric_eqos_phyrxclk", mout_peric_eqos_phyrxclk_p, MUX_PERIC_EQOS_PHYRXCLK, 0, 1),
};

static const struct samsung_div_clock peric_div_clks[] __initconst = {
	DIV(0, "dout_peric_eqos_busclk", "mout_peric_eqos_busclk", DIV_EQOS_BUSCLK, 0, 4),
	DIV(0, "dout_peric_mcan_clk", "mout_peric_dmaclk", DIV_PERIC_MCAN_CLK, 0, 4),
	DIV(PERIC_DOUT_RGMII_CLK, "dout_peric_rgmii_clk", "mout_peric_eqos_busclk", DIV_RGMII_CLK, 0, 4),
	DIV(0, "dout_peric_rii_clk", "dout_peric_rmii_clk", DIV_RII_CLK, 0, 4),
	DIV(0, "dout_peric_rmii_clk", "dout_peric_rgmii_clk", DIV_RMII_CLK, 0, 4),
	DIV(0, "dout_peric_spi_clk", "mout_peric_spi_clk", DIV_SPI_CLK, 0, 6),
	DIV(0, "dout_peric_uart_clk", "mout_peric_uart_clk", DIV_UART_CLK, 0, 6),
};

static const struct samsung_gate_clock peric_gate_clks[] __initconst = {
	GATE(PERIC_EQOS_TOP_IPCLKPORT_CLK_PTP_REF_I, "peric_eqos_top_ipclkport_clk_ptp_ref_i", "fin_pll", GAT_EQOS_TOP_IPCLKPORT_CLK_PTP_REF_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_gpio_peric_ipclkport_oscclk", "fin_pll", GAT_GPIO_PERIC_IPCLKPORT_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_ADCIF, "peric_adc0_ipclkport_i_oscclk", "fin_pll", GAT_PERIC_ADC0_IPCLKPORT_I_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_cmu_peric_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_CMU_PERIC_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_pwm0_ipclkport_i_oscclk", "fin_pll", GAT_PERIC_PWM0_IPCLKPORT_I_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_pwm1_ipclkport_i_oscclk", "fin_pll", GAT_PERIC_PWM1_IPCLKPORT_I_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_async_apb_dma0_ipclkport_pclkm", "mout_peric_dmaclk", GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_async_apb_dma0_ipclkport_pclks", "mout_peric_pclk", GAT_ASYNC_APB_DMA0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_async_apb_dma1_ipclkport_pclkm", "mout_peric_dmaclk", GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_async_apb_dma1_ipclkport_pclks", "mout_peric_pclk", GAT_ASYNC_APB_DMA1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_axi2apb_peric0_ipclkport_aclk", "mout_peric_pclk", GAT_AXI2APB_PERIC0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_axi2apb_peric1_ipclkport_aclk", "mout_peric_pclk", GAT_AXI2APB_PERIC1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_axi2apb_peric2_ipclkport_aclk", "mout_peric_pclk", GAT_AXI2APB_PERIC2_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_bus_d_peric_ipclkport_dmaclk", "mout_peric_dmaclk", GAT_BUS_D_PERIC_IPCLKPORT_DMACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_BUS_D_PERIC_IPCLKPORT_EQOSCLK, "peric_bus_d_peric_ipclkport_eqosclk", "dout_peric_eqos_busclk", GAT_BUS_D_PERIC_IPCLKPORT_EQOSCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_bus_d_peric_ipclkport_mainclk", "mout_peric_tbuclk", GAT_BUS_D_PERIC_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_BUS_P_PERIC_IPCLKPORT_EQOSCLK, "peric_bus_p_peric_ipclkport_eqosclk", "dout_peric_eqos_busclk", GAT_BUS_P_PERIC_IPCLKPORT_EQOSCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_bus_p_peric_ipclkport_mainclk", "mout_peric_pclk", GAT_BUS_P_PERIC_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_bus_p_peric_ipclkport_smmuclk", "mout_peric_tbuclk", GAT_BUS_P_PERIC_IPCLKPORT_SMMUCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_EQOS_TOP_IPCLKPORT_ACLK_I, "peric_eqos_top_ipclkport_aclk_i", "dout_peric_eqos_busclk", GAT_EQOS_TOP_IPCLKPORT_ACLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_EQOS_TOP_IPCLKPORT_CLK_RX_I, "peric_eqos_top_ipclkport_clk_rx_i", "mout_peric_eqos_phyrxclk", GAT_EQOS_TOP_IPCLKPORT_CLK_RX_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_EQOS_TOP_IPCLKPORT_HCLK_I, "peric_eqos_top_ipclkport_hclk_i", "dout_peric_eqos_busclk", GAT_EQOS_TOP_IPCLKPORT_HCLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_EQOS_TOP_IPCLKPORT_RGMII_CLK_I, "peric_eqos_top_ipclkport_rgmii_clk_i", "dout_peric_rgmii_clk", GAT_EQOS_TOP_IPCLKPORT_RGMII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_eqos_top_ipclkport_rii_clk_i", "dout_peric_rii_clk", GAT_EQOS_TOP_IPCLKPORT_RII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_eqos_top_ipclkport_rmii_clk_i", "dout_peric_rmii_clk", GAT_EQOS_TOP_IPCLKPORT_RMII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_gpio_peric_ipclkport_pclk", "mout_peric_pclk", GAT_GPIO_PERIC_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_ns_brdg_peric_ipclkport_clk__psoc_peric__clk_peric_d", "mout_peric_tbuclk", GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_ns_brdg_peric_ipclkport_clk__psoc_peric__clk_peric_p", "mout_peric_pclk", GAT_NS_BRDG_PERIC_IPCLKPORT_CLK__PSOC_PERIC__CLK_PERIC_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_adc0_ipclkport_pclk_s0", "mout_peric_pclk", GAT_PERIC_ADC0_IPCLKPORT_PCLK_S0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_DMA0_IPCLKPORT_ACLK, "peric_dma0_ipclkport_aclk", "mout_peric_dmaclk", GAT_PERIC_DMA0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_DMA1_IPCLKPORT_ACLK, "peric_dma1_ipclkport_aclk", "mout_peric_dmaclk", GAT_PERIC_DMA1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C0, "peric_i2c0_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C1, "peric_i2c1_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C2, "peric_i2c2_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C3, "peric_i2c3_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C3_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C4, "peric_i2c4_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C4_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C5, "peric_i2c5_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C5_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C6, "peric_i2c6_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C6_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_HSI2C7, "peric_i2c7_ipclkport_i_pclk", "mout_peric_pclk", GAT_PERIC_I2C7_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN0_IPCLKPORT_CCLK, "peric_mcan0_ipclkport_cclk", "dout_peric_mcan_clk", GAT_PERIC_MCAN0_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN0_IPCLKPORT_PCLK, "peric_mcan0_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_MCAN0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN1_IPCLKPORT_CCLK, "peric_mcan1_ipclkport_cclk", "dout_peric_mcan_clk", GAT_PERIC_MCAN1_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN1_IPCLKPORT_PCLK, "peric_mcan1_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_MCAN1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN2_IPCLKPORT_CCLK, "peric_mcan2_ipclkport_cclk", "dout_peric_mcan_clk", GAT_PERIC_MCAN2_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN2_IPCLKPORT_PCLK, "peric_mcan2_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_MCAN2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN3_IPCLKPORT_CCLK, "peric_mcan3_ipclkport_cclk", "dout_peric_mcan_clk", GAT_PERIC_MCAN3_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_MCAN3_IPCLKPORT_PCLK, "peric_mcan3_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_MCAN3_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PWM0_IPCLKPORT_I_PCLK_S0, "peric_pwm0_ipclkport_i_pclk_s0", "mout_peric_pclk", GAT_PERIC_PWM0_IPCLKPORT_I_PCLK_S0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PWM1_IPCLKPORT_I_PCLK_S0, "peric_pwm1_ipclkport_i_pclk_s0", "mout_peric_pclk", GAT_PERIC_PWM1_IPCLKPORT_I_PCLK_S0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_smmu_ipclkport_cclk", "mout_peric_tbuclk", GAT_PERIC_SMMU_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_smmu_ipclkport_peric_bclk", "mout_peric_tbuclk", GAT_PERIC_SMMU_IPCLKPORT_PERIC_BCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_SPI0, "peric_spi0_ipclkport_i_pclk", "mout_peric_spi_pclk", GAT_PERIC_SPI0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_SCLK_SPI0, "peric_spi0_ipclkport_i_sclk_spi", "dout_peric_spi_clk", GAT_PERIC_SPI0_IPCLKPORT_I_SCLK_SPI, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_SPI1, "peric_spi1_ipclkport_i_pclk", "mout_peric_spi_pclk", GAT_PERIC_SPI1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_SCLK_SPI1, "peric_spi1_ipclkport_i_sclk_spi", "dout_peric_spi_clk", GAT_PERIC_SPI1_IPCLKPORT_I_SCLK_SPI, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_SPI2, "peric_spi2_ipclkport_i_pclk", "mout_peric_spi_pclk", GAT_PERIC_SPI2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_SCLK_SPI2, "peric_spi2_ipclkport_i_sclk_spi", "dout_peric_spi_clk", GAT_PERIC_SPI2_IPCLKPORT_I_SCLK_SPI, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_HCLK_TDM0, "peric_tdm0_ipclkport_hclk_m", "mout_peric_pclk", GAT_PERIC_TDM0_IPCLKPORT_HCLK_M, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_TDM0, "peric_tdm0_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_TDM0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_HCLK_TDM1, "peric_tdm1_ipclkport_hclk_m", "mout_peric_pclk", GAT_PERIC_TDM1_IPCLKPORT_HCLK_M, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_TDM1, "peric_tdm1_ipclkport_pclk", "mout_peric_pclk", GAT_PERIC_TDM1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_SCLK_UART0, "peric_uart0_ipclkport_i_sclk_uart", "dout_peric_uart_clk", GAT_PERIC_UART0_IPCLKPORT_I_SCLK_UART, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_UART0, "peric_uart0_ipclkport_pclk", "mout_peric_uart_pclk", GAT_PERIC_UART0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_SCLK_UART1, "peric_uart1_ipclkport_i_sclk_uart", "dout_peric_uart_clk", GAT_PERIC_UART1_IPCLKPORT_I_SCLK_UART, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PERIC_PCLK_UART1, "peric_uart1_ipclkport_pclk", "mout_peric_uart_pclk", GAT_PERIC_UART1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "peric_sysreg_peri_ipclkport_pclk", "mout_peric_pclk", GAT_SYSREG_PERI_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info peric_cmu_info __initconst = {
	.mux_clks		= peric_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(peric_mux_clks),
	.div_clks		= peric_div_clks,
	.nr_div_clks		= ARRAY_SIZE(peric_div_clks),
	.gate_clks		= peric_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(peric_gate_clks),
	.fixed_clks		= peric_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(peric_fixed_clks),
	.nr_clk_ids		= PERIC_NR_CLK,
	.clk_regs		= peric_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(peric_clk_regs),
};

static void __init trav_clk_peric_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &peric_cmu_info);
}

CLK_OF_DECLARE(trav_clk_peric, "turbo,trav-clock-peric", trav_clk_peric_init);


/* Register Offset definitions for CMU_FSYS0 (0x15010000) */
#define PLL_CON0_CLKCMU_FSYS0_UNIPRO		0x100
#define PLL_CON0_CLK_FSYS0_SLAVEBUSCLK		0x140
#define PLL_CON0_EQOS_RGMII_125_MUX1		0x160
#define DIV_CLK_UNIPRO				0x1800
#define DIV_EQS_RGMII_CLK_125			0x1804
#define DIV_PERIBUS_GRP				0x1808
#define DIV_EQOS_RII_CLK2O5			0x180c
#define DIV_EQOS_RMIICLK_25			0x1810
#define DIV_PCIE_PHY_OSCCLK			0x1814
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_PTP_REF_I	0x2004
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_RX_I	0x2008
#define GAT_FSYS0_FSYS0_CMU_FSYS0_IPCLKPORT_PCLK	0x200c
#define GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_OSCCLK	0x2010
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_XO	0x2014
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_IMMORTAL_CLK	0x2018
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_AUX_CLK_SOC	0x201c
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL24	0x2020
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL26	0x2024
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL24	0x2028
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL26	0x202c
#define GAT_FSYS0_USB20DRD_IPCLKPORT_REF_CLK	0x2030
#define GAT_FSYS0_USB20DRD_IPCLKPORT_USB20_CLKCORE_0	0x2034
#define GAT_FSYS0_AHBBR_FSYS0_IPCLKPORT_HCLK	0x2038
#define GAT_FSYS0_AXI2APB_FSYS0_IPCLKPORT_ACLK	0x203c
#define GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_MAINCLK	0x2040
#define GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_PERICLK	0x2044
#define GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_MAINCLK	0x2048
#define GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_TCUCLK	0x204c
#define GAT_FSYS0_CPE425_IPCLKPORT_ACLK		0x2050
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_ACLK_I	0x2054
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_HCLK_I	0x2058
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RGMII_CLK_I	0x205c
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RII_CLK_I	0x2060
#define GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RMII_CLK_I	0x2064
#define GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_PCLK	0x2068
#define GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D	0x206c
#define GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D1	0x2070
#define GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_P	0x2074
#define GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_S	0x2078
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_I_APB_PCLK	0x207c
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_SYSPLL	0x2080
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_APB_PCLK_0	0x2084
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_DBI_ACLK_SOC	0x2088
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK	0x208c
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_MSTR_ACLK_SOC	0x2090
#define GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_SLV_ACLK_SOC	0x2094
#define GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_CCLK	0x2098
#define GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_FSYS0_BCLK	0x209c
#define GAT_FSYS0_SYSREG_FSYS0_IPCLKPORT_PCLK	0x20a0
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_HCLK_BUS	0x20a4
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_ACLK	0x20a8
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_CLK_UNIPRO	0x20ac
#define GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_FMP_CLK	0x20b0
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_HCLK_BUS	0x20b4
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_ACLK	0x20b8
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_CLK_UNIPRO	0x20bc
#define GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_FMP_CLK	0x20c0
#define GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS	0x20c4
#define GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS_PHYCTRL	0x20c8
#define GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_PHYCTRL	0x20cc
#define GAT_FSYS0_USB20DRD_IPCLKPORT_BUS_CLK_EARLY	0x20d0
#define GAT_FSYS0_RII_CLK_DIVGATE			0x20d4

static const unsigned long fsys0_clk_regs[] __initconst = {
	PLL_CON0_CLKCMU_FSYS0_UNIPRO,
	PLL_CON0_CLK_FSYS0_SLAVEBUSCLK,
	PLL_CON0_EQOS_RGMII_125_MUX1,
	DIV_CLK_UNIPRO,
	DIV_EQS_RGMII_CLK_125,
	DIV_PERIBUS_GRP,
	DIV_EQOS_RII_CLK2O5,
	DIV_EQOS_RMIICLK_25,
	DIV_PCIE_PHY_OSCCLK,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_PTP_REF_I,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_RX_I,
	GAT_FSYS0_FSYS0_CMU_FSYS0_IPCLKPORT_PCLK,
	GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_OSCCLK,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_XO,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_IMMORTAL_CLK,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_AUX_CLK_SOC,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL24,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL26,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL24,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL26,
	GAT_FSYS0_USB20DRD_IPCLKPORT_REF_CLK,
	GAT_FSYS0_USB20DRD_IPCLKPORT_USB20_CLKCORE_0,
	GAT_FSYS0_AHBBR_FSYS0_IPCLKPORT_HCLK,
	GAT_FSYS0_AXI2APB_FSYS0_IPCLKPORT_ACLK,
	GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_MAINCLK,
	GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_PERICLK,
	GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_MAINCLK,
	GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_TCUCLK,
	GAT_FSYS0_CPE425_IPCLKPORT_ACLK,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_ACLK_I,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_HCLK_I,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RGMII_CLK_I,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RII_CLK_I,
	GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RMII_CLK_I,
	GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_PCLK,
	GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D,
	GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D1,
	GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_P,
	GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_S,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_I_APB_PCLK,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_SYSPLL,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_APB_PCLK_0,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_DBI_ACLK_SOC,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_MSTR_ACLK_SOC,
	GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_SLV_ACLK_SOC,
	GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_CCLK,
	GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_FSYS0_BCLK,
	GAT_FSYS0_SYSREG_FSYS0_IPCLKPORT_PCLK,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_HCLK_BUS,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_ACLK,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_CLK_UNIPRO,
	GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_FMP_CLK,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_HCLK_BUS,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_ACLK,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_CLK_UNIPRO,
	GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_FMP_CLK,
	GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS,
	GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS_PHYCTRL,
	GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_PHYCTRL,
	GAT_FSYS0_USB20DRD_IPCLKPORT_BUS_CLK_EARLY,
	GAT_FSYS0_RII_CLK_DIVGATE,
};

static const struct samsung_fixed_rate_clock fsys0_fixed_clks[] __initconst = {
	FRATE(0, "pad_eqos0_phyrxclk", NULL, 0, 125000000),
	FRATE(0, "i_mphy_refclk_ixtal26", NULL, 0, 26000000),
	FRATE(0, "xtal_clk_pcie_phy", NULL, 0, 100000000),
};

/* List of parent clocks for Muxes in CMU_FSYS0 */
PNAME(mout_fsys0_clkcmu_fsys0_unipro_p) = { "fin_pll", "dout_cmu_pll_shared0_div6" };
PNAME(mout_fsys0_clk_fsys0_slavebusclk_p) = { "fin_pll", "dout_cmu_fsys0_shared1div4" };
PNAME(mout_fsys0_eqos_rgmii_125_mux1_p) = { "fin_pll", "dout_cmu_fsys0_shared0div4" };

static const struct samsung_mux_clock fsys0_mux_clks[] __initconst = {
	MUX(0, "mout_fsys0_clkcmu_fsys0_unipro", mout_fsys0_clkcmu_fsys0_unipro_p, PLL_CON0_CLKCMU_FSYS0_UNIPRO, 4, 1),
	MUX(0, "mout_fsys0_clk_fsys0_slavebusclk", mout_fsys0_clk_fsys0_slavebusclk_p, PLL_CON0_CLK_FSYS0_SLAVEBUSCLK, 4, 1),
	MUX(0, "mout_fsys0_eqos_rgmii_125_mux1", mout_fsys0_eqos_rgmii_125_mux1_p, PLL_CON0_EQOS_RGMII_125_MUX1, 4, 1),
};

static const struct samsung_div_clock fsys0_div_clks[] __initconst = {
	DIV(0, "dout_fsys0_clk_unipro", "mout_fsys0_clkcmu_fsys0_unipro", DIV_CLK_UNIPRO, 0, 4),
	DIV(0, "dout_fsys0_eqs_rgmii_clk_125", "mout_fsys0_eqos_rgmii_125_mux1", DIV_EQS_RGMII_CLK_125, 0, 4),
	DIV(0, "dout_fsys0_peribus_grp", "mout_fsys0_clk_fsys0_slavebusclk", DIV_PERIBUS_GRP, 0, 4),
	DIV(0, "dout_fsys0_eqos_rii_clk2o5", "fsys0_rii_clk_divgate", DIV_EQOS_RII_CLK2O5, 0, 4),
	DIV(0, "dout_fsys0_eqos_rmiiclk_25", "mout_fsys0_eqos_rgmii_125_mux1", DIV_EQOS_RMIICLK_25, 0, 5),
	DIV(0, "dout_fsys0_pcie_phy_oscclk", "mout_fsys0_eqos_rgmii_125_mux1", DIV_PCIE_PHY_OSCCLK, 0, 4),
};

static const struct samsung_gate_clock fsys0_gate_clks[] __initconst = {
	GATE(FSYS0_EQOS_TOP0_IPCLKPORT_CLK_RX_I, "fsys0_eqos_top0_ipclkport_clk_rx_i", "pad_eqos0_phyrxclk", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_RX_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_SUBCTRL_INST0_AUX_CLK_SOC, "fsys0_pcie_top_ipclkport_trav_pcie_sub_ctrl_inst_0_aux_clk_soc", "fin_pll", GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_AUX_CLK_SOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_fsys0_cmu_fsys0_ipclkport_pclk", "dout_fsys0_peribus_grp", GAT_FSYS0_FSYS0_CMU_FSYS0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_ieee1500_wrapper_for_pcieg3_phy_x4_inst_0_pll_refclk_from_xo", "xtal_clk_pcie_phy", GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_XO, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_MPHY_REFCLK_IXTAL24, "fsys0_ufs_top0_ipclkport_i_mphy_refclk_ixtal24", "i_mphy_refclk_ixtal26", GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL24, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_MPHY_REFCLK_IXTAL26, "fsys0_ufs_top0_ipclkport_i_mphy_refclk_ixtal26", "i_mphy_refclk_ixtal26", GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_MPHY_REFCLK_IXTAL26, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_MPHY_REFCLK_IXTAL24, "fsys0_ufs_top1_ipclkport_i_mphy_refclk_ixtal24", "i_mphy_refclk_ixtal26", GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL24, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_MPHY_REFCLK_IXTAL26, "fsys0_ufs_top1_ipclkport_i_mphy_refclk_ixtal26", "i_mphy_refclk_ixtal26", GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_MPHY_REFCLK_IXTAL26, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_ahbbr_fsys0_ipclkport_hclk", "dout_fsys0_peribus_grp", GAT_FSYS0_AHBBR_FSYS0_IPCLKPORT_HCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_axi2apb_fsys0_ipclkport_aclk", "dout_fsys0_peribus_grp", GAT_FSYS0_AXI2APB_FSYS0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_bus_d_fsys0_ipclkport_mainclk", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_bus_d_fsys0_ipclkport_periclk", "dout_fsys0_peribus_grp", GAT_FSYS0_BUS_D_FSYS0_IPCLKPORT_PERICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_bus_p_fsys0_ipclkport_mainclk", "dout_fsys0_peribus_grp", GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_bus_p_fsys0_ipclkport_tcuclk", "mout_fsys0_eqos_rgmii_125_mux1", GAT_FSYS0_BUS_P_FSYS0_IPCLKPORT_TCUCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_cpe425_ipclkport_aclk", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_CPE425_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(FSYS0_EQOS_TOP0_IPCLKPORT_ACLK_I, "fsys0_eqos_top0_ipclkport_aclk_i", "dout_fsys0_peribus_grp", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_ACLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(FSYS0_EQOS_TOP0_IPCLKPORT_HCLK_I, "fsys0_eqos_top0_ipclkport_hclk_i", "dout_fsys0_peribus_grp", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_HCLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(FSYS0_EQOS_TOP0_IPCLKPORT_RGMII_CLK_I, "fsys0_eqos_top0_ipclkport_rgmii_clk_i", "dout_fsys0_eqs_rgmii_clk_125", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RGMII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_eqos_top0_ipclkport_rii_clk_i", "dout_fsys0_eqos_rii_clk2o5", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_eqos_top0_ipclkport_rmii_clk_i", "dout_fsys0_eqos_rmiiclk_25", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_RMII_CLK_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_gpio_fsys0_ipclkport_pclk", "dout_fsys0_peribus_grp", GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_gpio_fsys0_ipclkport_oscclk", "fin_pll", GAT_FSYS0_GPIO_FSYS0_IPCLKPORT_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_ns_brdg_fsys0_ipclkport_clk__psoc_fsys0__clk_fsys0_d", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_ns_brdg_fsys0_ipclkport_clk__psoc_fsys0__clk_fsys0_d1", "mout_fsys0_eqos_rgmii_125_mux1", GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_D1, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_ns_brdg_fsys0_ipclkport_clk__psoc_fsys0__clk_fsys0_p", "dout_fsys0_peribus_grp", GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_ns_brdg_fsys0_ipclkport_clk__psoc_fsys0__clk_fsys0_s", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_NS_BRDG_FSYS0_IPCLKPORT_CLK__PSOC_FSYS0__CLK_FSYS0_S, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_ieee1500_wrapper_for_pcieg3_phy_x4_inst_0_i_apb_pclk", "dout_fsys0_peribus_grp", GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_I_APB_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_ieee1500_wrapper_for_pcieg3_phy_x4_inst_0_pll_refclk_from_syspll", "dout_fsys0_pcie_phy_oscclk", GAT_FSYS0_PCIE_TOP_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG3_PHY_X4_INST_0_PLL_REFCLK_FROM_SYSPLL, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_pipe_pal_inst_0_i_apb_pclk_0", "dout_fsys0_peribus_grp", GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_APB_PCLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_pipe_pal_inst_0_i_immortal_clk", "fin_pll", GAT_FSYS0_PCIE_TOP_IPCLKPORT_PIPE_PAL_INST_0_I_IMMORTAL_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_SUBCTRL_INST0_DBI_ACLK_SOC, "fsys0_pcie_top_ipclkport_trav_pcie_sub_ctrl_inst_0_dbi_aclk_soc", "dout_fsys0_peribus_grp", GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_DBI_ACLK_SOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_pcie_top_ipclkport_trav_pcie_sub_ctrl_inst_0_i_driver_apb_clk", "dout_fsys0_peribus_grp", GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_SUBCTRL_INST0_MSTR_ACLK_SOC, "fsys0_pcie_top_ipclkport_trav_pcie_sub_ctrl_inst_0_mstr_aclk_soc", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_MSTR_ACLK_SOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_SUBCTRL_INST0_SLV_ACLK_SOC, "fsys0_pcie_top_ipclkport_trav_pcie_sub_ctrl_inst_0_slv_aclk_soc", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_PCIE_TOP_IPCLKPORT_TRAV_PCIE_SUB_CTRL_INST_0_SLV_ACLK_SOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_smmu_fsys0_ipclkport_cclk", "mout_fsys0_eqos_rgmii_125_mux1", GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_CCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_smmu_fsys0_ipclkport_fsys0_bclk", "mout_fsys0_clk_fsys0_slavebusclk", GAT_FSYS0_SMMU_FSYS0_IPCLKPORT_FSYS0_BCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_sysreg_fsys0_ipclkport_pclk", "dout_fsys0_peribus_grp", GAT_FSYS0_SYSREG_FSYS0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_TOP0_HCLK_BUS, "fsys0_ufs_top0_ipclkport_hclk_bus", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP0_IPCLKPORT_HCLK_BUS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_TOP0_ACLK, "fsys0_ufs_top0_ipclkport_i_aclk", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_TOP0_CLK_UNIPRO, "fsys0_ufs_top0_ipclkport_i_clk_unipro", "dout_fsys0_clk_unipro", GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_CLK_UNIPRO, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS0_TOP0_FMP_CLK, "fsys0_ufs_top0_ipclkport_i_fmp_clk", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP0_IPCLKPORT_I_FMP_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_TOP1_HCLK_BUS, "fsys0_ufs_top1_ipclkport_hclk_bus", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP1_IPCLKPORT_HCLK_BUS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_TOP1_ACLK, "fsys0_ufs_top1_ipclkport_i_aclk", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_TOP1_CLK_UNIPRO, "fsys0_ufs_top1_ipclkport_i_clk_unipro", "dout_fsys0_clk_unipro", GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_CLK_UNIPRO, 21, CLK_IGNORE_UNUSED, 0),
	GATE(UFS1_TOP1_FMP_CLK, "fsys0_ufs_top1_ipclkport_i_fmp_clk", "dout_fsys0_peribus_grp", GAT_FSYS0_UFS_TOP1_IPCLKPORT_I_FMP_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_ACLK_BUS, "fsys0_usb20drd_ipclkport_aclk_bus", "dout_fsys0_peribus_grp", GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_ACLK_BUS_PHYCTRL, "fsys0_usb20drd_ipclkport_aclk_bus_phyctrl", "dout_fsys0_peribus_grp", GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_BUS_PHYCTRL, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_ACLK_PHYCTRL, "fsys0_usb20drd_ipclkport_aclk_phyctrl", "dout_fsys0_peribus_grp", GAT_FSYS0_USB20DRD_IPCLKPORT_ACLK_PHYCTRL, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_BUS_CLK_EARLY, "fsys0_usb20drd_ipclkport_bus_clk_early", "dout_fsys0_peribus_grp", GAT_FSYS0_USB20DRD_IPCLKPORT_BUS_CLK_EARLY, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys0_rii_clk_divgate", "dout_fsys0_eqos_rmiiclk_25", GAT_FSYS0_RII_CLK_DIVGATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(FSYS0_EQOS_TOP0_IPCLKPORT_CLK_PTP_REF_I, "fsys0_eqos_top0_ipclkport_clk_ptp_ref_i", "fin_pll", GAT_FSYS0_EQOS_TOP0_IPCLKPORT_CLK_PTP_REF_I, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_REF_CLK, "fsys0_usb20drd_ipclkport_ref_clk", "fin_pll", GAT_FSYS0_RII_CLK_DIVGATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(USB20DRD_IPCLKPORT_USB20_CLKCORE_0, "fsys0_usb20drd_ipclkport_usb20_clkcore_0", "fin_pll", GAT_FSYS0_USB20DRD_IPCLKPORT_USB20_CLKCORE_0, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info fsys0_cmu_info __initconst = {
	.mux_clks		= fsys0_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(fsys0_mux_clks),
	.div_clks		= fsys0_div_clks,
	.nr_div_clks		= ARRAY_SIZE(fsys0_div_clks),
	.gate_clks		= fsys0_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(fsys0_gate_clks),
	.fixed_clks		= fsys0_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(fsys0_fixed_clks),
	.nr_clk_ids		= FSYS0_NR_CLK,
	.clk_regs		= fsys0_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(fsys0_clk_regs),
};

static void __init trav_clk_fsys0_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &fsys0_cmu_info);
}

CLK_OF_DECLARE(trav_clk_fsys0, "turbo,trav-clock-fsys0", trav_clk_fsys0_init);


/* Register Offset definitions for CMU_FSYS1 (0x16810000) */
#define PLL_CON0_ACLK_FSYS1_BUSP_MUX			0x100
#define PLL_CON0_PCLKL_FSYS1_BUSP_MUX			0x180
#define DIV_CLK_FSYS1_PHY0_OSCCLK			0x1800
#define DIV_CLK_FSYS1_PHY1_OSCCLK			0x1804
#define GAT_FSYS1_CMU_FSYS1_IPCLKPORT_PCLK	0x2000
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_AUXCLK		0x2004
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_SOC_REF_CLK	0x2008
#define GAT_FSYS1_PCIE_LINK1_IPCLKPORT_AUXCLK		0x200c
#define GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_XTAL	0x202c
#define GAT_FSYS1_PHY0_OSCCLLK				0x2034
#define GAT_FSYS1_PHY1_OSCCLK				0x2038
#define GAT_FSYS1_AXI2APB_FSYS1_IPCLKPORT_ACLK		0x203c
#define GAT_FSYS1_BUS_D0_FSYS1_IPCLKPORT_MAINCLK	0x2040
#define GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_M250CLK	0x2048
#define GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_MAINCLK	0x204c
#define GAT_FSYS1_CPE425_0_FSYS1_IPCLKPORT_ACLK		0x2054
#define GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_D0	0x205c
#define GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_S0	0x2064
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_DBI_ACLK		0x206c
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_APB_CLK	0x2070
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_DRIVER_APB_CLK	0x2074
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_MSTR_ACLK	0x2078
#define GAT_FSYS1_PCIE_LINK0_IPCLKPORT_SLV_ACLK		0x207c
#define GAT_FSYS1_PCIE_LINK1_IPCLKPORT_DBI_ACLK		0x2080
#define GAT_FSYS1_PCIE_LINK1_IPCLKPORT_I_DRIVER_APB_CLK	0x2084
#define GAT_FSYS1_PCIE_LINK1_IPCLKPORT_MSTR_ACLK	0x2088
#define GAT_FSYS1_PCIE_LINK1_IPCLKPORT_SLV_ACLK		0x208c
#define GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_APB_CLK		0x20a4
#define GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_SOC_PLL	0x20a8
#define GAT_FSYS1_SYSREG_FSYS1_IPCLKPORT_PCLK		0x20b4
#define GAT_FSYS1_TBU0_FSYS1_IPCLKPORT_ACLK		0x20b8

static const unsigned long fsys1_clk_regs[] __initconst = {
	PLL_CON0_ACLK_FSYS1_BUSP_MUX,
	PLL_CON0_PCLKL_FSYS1_BUSP_MUX,
	DIV_CLK_FSYS1_PHY0_OSCCLK,
	DIV_CLK_FSYS1_PHY1_OSCCLK,
	GAT_FSYS1_CMU_FSYS1_IPCLKPORT_PCLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_AUXCLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_SOC_REF_CLK,
	GAT_FSYS1_PCIE_LINK1_IPCLKPORT_AUXCLK,
	GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_XTAL,
	GAT_FSYS1_PHY0_OSCCLLK,
	GAT_FSYS1_PHY1_OSCCLK,
	GAT_FSYS1_AXI2APB_FSYS1_IPCLKPORT_ACLK,
	GAT_FSYS1_BUS_D0_FSYS1_IPCLKPORT_MAINCLK,
	GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_M250CLK,
	GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_MAINCLK,
	GAT_FSYS1_CPE425_0_FSYS1_IPCLKPORT_ACLK,
	GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_D0,
	GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_S0,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_DBI_ACLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_APB_CLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_DRIVER_APB_CLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_MSTR_ACLK,
	GAT_FSYS1_PCIE_LINK0_IPCLKPORT_SLV_ACLK,
	GAT_FSYS1_PCIE_LINK1_IPCLKPORT_DBI_ACLK,
	GAT_FSYS1_PCIE_LINK1_IPCLKPORT_I_DRIVER_APB_CLK,
	GAT_FSYS1_PCIE_LINK1_IPCLKPORT_MSTR_ACLK,
	GAT_FSYS1_PCIE_LINK1_IPCLKPORT_SLV_ACLK,
	GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_APB_CLK,
	GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_SOC_PLL,
	GAT_FSYS1_SYSREG_FSYS1_IPCLKPORT_PCLK,
	GAT_FSYS1_TBU0_FSYS1_IPCLKPORT_ACLK,
};

static const struct samsung_fixed_rate_clock fsys1_fixed_clks[] __initconst = {
	FRATE(0, "clk_fsys1_phy0_ref", NULL, 0, 100000000),
	FRATE(0, "clk_fsys1_phy1_ref", NULL, 0, 100000000),
};

/* List of parent clocks for Muxes in CMU_FSYS1 */
PNAME(mout_fsys1_pclkl_fsys1_busp_mux_p) = { "fin_pll", "dout_cmu_fsys1_shared0div8" };
PNAME(mout_fsys1_aclk_fsys1_busp_mux_p) = { "fin_pll", "dout_cmu_fsys1_shared0div4" };

static const struct samsung_mux_clock fsys1_mux_clks[] __initconst = {
	MUX(0, "mout_fsys1_pclkl_fsys1_busp_mux", mout_fsys1_pclkl_fsys1_busp_mux_p, PLL_CON0_PCLKL_FSYS1_BUSP_MUX, 4, 1),
	MUX(0, "mout_fsys1_aclk_fsys1_busp_mux", mout_fsys1_aclk_fsys1_busp_mux_p, PLL_CON0_ACLK_FSYS1_BUSP_MUX, 4, 1),
};

static const struct samsung_div_clock fsys1_div_clks[] __initconst = {
	DIV(0, "dout_fsys1_clk_fsys1_phy0_oscclk", "fsys1_phy0_osccllk", DIV_CLK_FSYS1_PHY0_OSCCLK, 0, 4),
	DIV(0, "dout_fsys1_clk_fsys1_phy1_oscclk", "fsys1_phy1_oscclk", DIV_CLK_FSYS1_PHY1_OSCCLK, 0, 4),
};

static const struct samsung_gate_clock fsys1_gate_clks[] __initconst = {
	GATE(0, "fsys1_cmu_fsys1_ipclkport_pclk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_CMU_FSYS1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_phy0_ipclkport_i_ref_xtal", "clk_fsys1_phy0_ref", GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_XTAL, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_phy0_osccllk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PHY0_OSCCLLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_phy1_oscclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PHY1_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_axi2apb_fsys1_ipclkport_aclk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_AXI2APB_FSYS1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_bus_d0_fsys1_ipclkport_mainclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_BUS_D0_FSYS1_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_bus_s0_fsys1_ipclkport_m250clk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_M250CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_bus_s0_fsys1_ipclkport_mainclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_BUS_S0_FSYS1_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_cpe425_0_fsys1_ipclkport_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_CPE425_0_FSYS1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_ns_brdg_fsys1_ipclkport_clk__psoc_fsys1__clk_fsys1_d0", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_D0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_ns_brdg_fsys1_ipclkport_clk__psoc_fsys1__clk_fsys1_s0", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_NS_BRDG_FSYS1_IPCLKPORT_CLK__PSOC_FSYS1__CLK_FSYS1_S0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK0_IPCLKPORT_DBI_ACLK, "fsys1_pcie_link0_ipclkport_dbi_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_DBI_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_link0_ipclkport_i_apb_clk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_APB_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_link0_ipclkport_i_soc_ref_clk", "fin_pll", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_SOC_REF_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_link0_ipclkport_i_driver_apb_clk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_I_DRIVER_APB_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK0_IPCLKPORT_MSTR_ACLK, "fsys1_pcie_link0_ipclkport_mstr_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_MSTR_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK0_IPCLKPORT_SLV_ACLK, "fsys1_pcie_link0_ipclkport_slv_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_SLV_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK1_IPCLKPORT_DBI_ACLK, "fsys1_pcie_link1_ipclkport_dbi_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK1_IPCLKPORT_DBI_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_link1_ipclkport_i_driver_apb_clk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK1_IPCLKPORT_I_DRIVER_APB_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK1_IPCLKPORT_MSTR_ACLK, "fsys1_pcie_link1_ipclkport_mstr_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK1_IPCLKPORT_MSTR_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK1_IPCLKPORT_SLV_ACLK, "fsys1_pcie_link1_ipclkport_slv_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_PCIE_LINK1_IPCLKPORT_SLV_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_phy0_ipclkport_i_apb_clk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_APB_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK0_IPCLKPORT_AUX_ACLK, "fsys1_pcie_link0_ipclkport_auxclk", "fin_pll", GAT_FSYS1_PCIE_LINK0_IPCLKPORT_AUXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(PCIE_LINK1_IPCLKPORT_AUX_ACLK, "fsys1_pcie_link1_ipclkport_auxclk", "fin_pll", GAT_FSYS1_PCIE_LINK1_IPCLKPORT_AUXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_pcie_phy0_ipclkport_i_ref_soc_pll", "dout_fsys1_clk_fsys1_phy0_oscclk", GAT_FSYS1_PCIE_PHY0_IPCLKPORT_I_REF_SOC_PLL, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_sysreg_fsys1_ipclkport_pclk", "mout_fsys1_pclkl_fsys1_busp_mux", GAT_FSYS1_SYSREG_FSYS1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "fsys1_tbu0_fsys1_ipclkport_aclk", "mout_fsys1_aclk_fsys1_busp_mux", GAT_FSYS1_TBU0_FSYS1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info fsys1_cmu_info __initconst = {
	.mux_clks		= fsys1_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(fsys1_mux_clks),
	.div_clks		= fsys1_div_clks,
	.nr_div_clks		= ARRAY_SIZE(fsys1_div_clks),
	.gate_clks		= fsys1_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(fsys1_gate_clks),
	.fixed_clks		= fsys1_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(fsys1_fixed_clks),
	.nr_clk_ids		= FSYS1_NR_CLK,
	.clk_regs		= fsys1_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(fsys1_clk_regs),
};

static void __init trav_clk_fsys1_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &fsys1_cmu_info);
}

CLK_OF_DECLARE(trav_clk_fsys1, "turbo,trav-clock-fsys1", trav_clk_fsys1_init);


/* Register Offset definitions for CMU_CORE_GT (0x11310000) */
#define PLL_LOCKTIME_PLL_CORE_GT			0x0
#define PLL_CON0_PLL_CORE_GT				0x100
#define MUX_CLK_CORE_NOC				0x1000
#define MUX_CLK_CORE_PCLK				0x1004
#define MUX_CLK_CORE_TRIP_MUX				0x1008
#define DIV_CLK_CORE_TCU				0x1800
#define DIV_CLK_CORE_ATB				0x1804
#define DIV_MIF_SWITCHCLK				0x1808
#define DIV_PLL_CORE_DIV4				0x180c
#define GAT_CLK_BLK_CORE_GT_UID_CORE_GT_CMU_CORE_GT_IPCLKPORT_PCLK	0x2000
#define GAT_MIF_SWITCHCLK				0x2004
#define GAT_AXI2APB_CORE_IPCLKPORT_ACLK			0x2008
#define GAT_LHS_ATB_CCC_IPCLKPORT_I_CLK			0x2014
#define GAT_NS_CORE_GROUP_GT_IPCLKPORT_CLK__PNOC_GROUP_3__REGBUS	0x2018
#define GAT_NS_CORE_TRIP_MMU_IPCLKPORT_CLK__P_CORE_TRIP_MMU__CLK_TRIPBU_P	0x2024
#define GAT_NS_SYSTEM_IPCLKPORT_CLK__SYSTEM__NOC	0x2028
#define GAT_SYSREG_CORE_IPCLKPORT_PCLK			0x203c

static const unsigned long core_gt_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CORE_GT,
	PLL_CON0_PLL_CORE_GT,
	MUX_CLK_CORE_NOC,
	MUX_CLK_CORE_PCLK,
	MUX_CLK_CORE_TRIP_MUX,
	DIV_CLK_CORE_TCU,
	DIV_CLK_CORE_ATB,
	DIV_MIF_SWITCHCLK,
	DIV_PLL_CORE_DIV4,
	GAT_CLK_BLK_CORE_GT_UID_CORE_GT_CMU_CORE_GT_IPCLKPORT_PCLK,
	GAT_MIF_SWITCHCLK,
	GAT_AXI2APB_CORE_IPCLKPORT_ACLK,
	GAT_LHS_ATB_CCC_IPCLKPORT_I_CLK,
	GAT_NS_CORE_GROUP_GT_IPCLKPORT_CLK__PNOC_GROUP_3__REGBUS,
	GAT_NS_CORE_TRIP_MMU_IPCLKPORT_CLK__P_CORE_TRIP_MMU__CLK_TRIPBU_P,
	GAT_NS_SYSTEM_IPCLKPORT_CLK__SYSTEM__NOC,
	GAT_SYSREG_CORE_IPCLKPORT_PCLK,
};

static const struct samsung_pll_rate_table pll_core_gt_rate_table[] __initconst = {
	PLL_35XX_RATE(1072000000, 268, 3, 1),
};

static const struct samsung_pll_clock core_gt_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_core_gt", "fin_pll", PLL_LOCKTIME_PLL_CORE_GT, PLL_CON0_PLL_CORE_GT, pll_core_gt_rate_table),
};

PNAME(mout_core_gt_pll_p) = { "fin_pll", "fout_pll_core_gt" };
PNAME(mout_core_gt_clk_core_noc_p) = { "fin_pll", "mout_core_gt_pll" };
PNAME(mout_core_gt_clk_core_pclk_p) = { "fin_pll", "dout_core_gt_pll_core_div4" };
PNAME(mout_core_gt_clk_core_trip_mux_p) = { "fin_pll", "mout_core_gt_pll" };

static const struct samsung_mux_clock core_gt_mux_clks[] __initconst = {
	MUX(0, "mout_core_gt_pll", mout_core_gt_pll_p, PLL_CON0_PLL_CORE_GT, 4, 1),
	MUX(0, "mout_core_gt_clk_core_noc", mout_core_gt_clk_core_noc_p, MUX_CLK_CORE_NOC, 0, 1),
	MUX(0, "mout_core_gt_clk_core_pclk", mout_core_gt_clk_core_pclk_p, MUX_CLK_CORE_PCLK, 0, 1),
	MUX(0, "mout_core_gt_clk_core_trip_mux", mout_core_gt_clk_core_trip_mux_p, MUX_CLK_CORE_TRIP_MUX, 0, 1),
};

static const struct samsung_div_clock core_gt_div_clks[] __initconst = {
	DIV(0, "dout_core_gt_pll_core_div4", "mout_core_gt_pll", DIV_PLL_CORE_DIV4, 0, 4),
	DIV(DOUT_CORE_GT_DIV_MIF_SWITCHCLK, "dout_core_gt_div_mif_switchclk", "core_gt_mif_switchclk", DIV_MIF_SWITCHCLK, 0, 4),
	DIV(0, "dout_core_gt_clk_core_tcu", "mout_core_gt_clk_core_trip_mux", DIV_CLK_CORE_TCU, 0, 4),
	DIV(0, "dout_core_gt_div_clk_core_atb", "mout_core_gt_pll", DIV_CLK_CORE_ATB, 0, 4),
};

static const struct samsung_gate_clock core_gt_gate_clks[] __initconst = {
	GATE(0, "core_gt_cmu_core_gt_ipclkport_pclk", "mout_core_gt_clk_core_pclk", GAT_CLK_BLK_CORE_GT_UID_CORE_GT_CMU_CORE_GT_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_mif_switchclk", "mout_core_gt_pll", GAT_MIF_SWITCHCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_axi2apb_core_ipclkport_aclk", "mout_core_gt_clk_core_pclk", GAT_AXI2APB_CORE_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_lhs_atb_ccc_ipclkport_i_clk", "dout_core_gt_div_clk_core_atb", GAT_LHS_ATB_CCC_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_ns_core_group_gt_ipclkport_clk__pnoc_group_3__regbus", "mout_core_gt_clk_core_pclk", GAT_NS_CORE_GROUP_GT_IPCLKPORT_CLK__PNOC_GROUP_3__REGBUS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_ns_core_trip_mmu_ipclkport_clk__p_core_trip_mmu__clk_tripbu_p", "mout_core_gt_clk_core_pclk", GAT_NS_CORE_TRIP_MMU_IPCLKPORT_CLK__P_CORE_TRIP_MMU__CLK_TRIPBU_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_ns_system_ipclkport_clk__system__noc", "mout_core_gt_clk_core_noc", GAT_NS_SYSTEM_IPCLKPORT_CLK__SYSTEM__NOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "core_gt_sysreg_core_ipclkport_pclk", "mout_core_gt_clk_core_pclk", GAT_SYSREG_CORE_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info core_gt_cmu_info __initconst = {
	.pll_clks		= core_gt_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(core_gt_pll_clks),
	.mux_clks		= core_gt_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(core_gt_mux_clks),
	.div_clks		= core_gt_div_clks,
	.nr_div_clks		= ARRAY_SIZE(core_gt_div_clks),
	.gate_clks		= core_gt_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(core_gt_gate_clks),
	.nr_clk_ids		= CORE_GT_NR_CLK,
	.clk_regs		= core_gt_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(core_gt_clk_regs),
};

static void __init trav_clk_core_gt_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &core_gt_cmu_info);
}

CLK_OF_DECLARE(trav_clk_core_gt, "turbo,trav-clock-core_gt", trav_clk_core_gt_init);

/* Register Offset definitions for CMU_CORE_MIFL (0x11a10000) */
#define PLL_LOCKTIME_PLL_CORE_MIFL	0x0
#define PLL_CON0_PLL_CORE_MIFL	0x140
#define MUX_PLL_MIFL_CLK	0x1000
#define GAT_CORE_MIFL_CMU_CORE_MIFL_IPCLKPORT_PCLK	0x200c

static const unsigned long core_mifl_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CORE_MIFL,
	PLL_CON0_PLL_CORE_MIFL,
	MUX_PLL_MIFL_CLK,
	GAT_CORE_MIFL_CMU_CORE_MIFL_IPCLKPORT_PCLK,
};

static const struct samsung_fixed_rate_clock core_mifl_fixed_clks[] __initconst = {
	FRATE(0, "spl_mifl_pclk", NULL, 0, 268000000),
};

static const struct samsung_pll_rate_table pll_core_mifl_rate_table[] __initconst = {
	PLL_35XX_RATE(932000000, 233, 3, 1),
	PLL_35XX_RATE(1068000000, 267, 3, 1),
};

static const struct samsung_pll_clock core_mifl_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_core_mifl", "fin_pll", PLL_LOCKTIME_PLL_CORE_MIFL, PLL_CON0_PLL_CORE_MIFL, pll_core_mifl_rate_table),
};

PNAME(mout_core_mifl_pll_p) = { "fin_pll", "fout_pll_core_mifl" };
PNAME(mout_core_mifl_pll_mifl_clk_p) = { "mout_core_mifl_pll", "dout_core_gt_div_mif_switchclk" };

static const struct samsung_mux_clock core_mifl_mux_clks[] __initconst = {
	MUX(0, "mout_core_mifl_pll", mout_core_mifl_pll_p, PLL_CON0_PLL_CORE_MIFL, 4, 1),
	MUX(MOUT_CORE_MIFL_PLL_MIFL_CLK, "mout_core_mifl_pll_mifl_clk", mout_core_mifl_pll_mifl_clk_p, MUX_PLL_MIFL_CLK, 0, 1),
};

static const struct samsung_gate_clock core_mifl_gate_clks[] __initconst = {
	GATE(0, "core_mifl_cmu_core_mifl_ipclkport_pclk", "spl_mifl_pclk", GAT_CORE_MIFL_CMU_CORE_MIFL_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info core_mifl_cmu_info __initconst = {
	.pll_clks		= core_mifl_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(core_mifl_pll_clks),
	.mux_clks		= core_mifl_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(core_mifl_mux_clks),
	.gate_clks		= core_mifl_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(core_mifl_gate_clks),
	.fixed_clks		= core_mifl_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(core_mifl_fixed_clks),
	.nr_clk_ids		= CORE_MIFL_NR_CLK,
	.clk_regs		= core_mifl_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(core_mifl_clk_regs),
};

static void __init trav_clk_core_mifl_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &core_mifl_cmu_info);
}

CLK_OF_DECLARE(trav_clk_core_mifl, "turbo,trav-clock-core_mifl", trav_clk_core_mifl_init);


/* Register Offset definitions for CMU_CORE_MIFR (0x11b10000) */
#define PLL_LOCKTIME_PLL_CORE_MIFR	0x0
#define PLL_CON0_PLL_CORE_MIFR	0x140
#define MUX_PLL_MIFR_CLK	0x1000
#define GAT_CLK_BLK_CORE_MIFR_UID_CORE_MIFR_CMU_CORE_MIFR_IPCLKPORT_PCLK	0x200c

static const unsigned long core_mifr_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CORE_MIFR,
	PLL_CON0_PLL_CORE_MIFR,
	MUX_PLL_MIFR_CLK,
	GAT_CLK_BLK_CORE_MIFR_UID_CORE_MIFR_CMU_CORE_MIFR_IPCLKPORT_PCLK,
};

static const struct samsung_fixed_rate_clock core_mifr_fixed_clks[] __initconst = {
	FRATE(0, "spl_mifr_pclk", NULL, 0, 268000000),
};

static const struct samsung_pll_rate_table pll_core_mifr_rate_table[] __initconst = {
	PLL_35XX_RATE(932000000, 233, 3, 1),
	PLL_35XX_RATE(1068000000, 267, 3, 1),
};

static const struct samsung_pll_clock core_mifr_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_core_mifr", "fin_pll", PLL_LOCKTIME_PLL_CORE_MIFR, PLL_CON0_PLL_CORE_MIFR, pll_core_mifr_rate_table),
};

PNAME(mout_core_mifr_pll_p) = { "fin_pll", "fout_pll_core_mifr" };
PNAME(mout_core_mifr_pll_mifr_clk_p) = { "mout_core_mifr_pll", "dout_core_gt_div_mif_switchclk" };

static const struct samsung_mux_clock core_mifr_mux_clks[] __initconst = {
	MUX(0, "mout_core_mifr_pll", mout_core_mifr_pll_p, PLL_CON0_PLL_CORE_MIFR, 4, 1),
	MUX(MOUT_CORE_MIFR_PLL_MIFR_CLK, "mout_core_mifr_pll_mifr_clk", mout_core_mifr_pll_mifr_clk_p, MUX_PLL_MIFR_CLK, 0, 1),
};

static const struct samsung_gate_clock core_mifr_gate_clks[] __initconst = {
	GATE(0, "core_mifr_clk_blk_core_mifr_uid_core_mifr_cmu_core_mifr_ipclkport_pclk", "spl_mifr_pclk", GAT_CLK_BLK_CORE_MIFR_UID_CORE_MIFR_CMU_CORE_MIFR_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info core_mifr_cmu_info __initconst = {
	.pll_clks		= core_mifr_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(core_mifr_pll_clks),
	.mux_clks		= core_mifr_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(core_mifr_mux_clks),
	.gate_clks		= core_mifr_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(core_mifr_gate_clks),
	.fixed_clks		= core_mifr_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(core_mifr_fixed_clks),
	.nr_clk_ids		= CORE_MIFR_NR_CLK,
	.clk_regs		= core_mifr_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(core_mifr_clk_regs),
};

static void __init trav_clk_core_mifr_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &core_mifr_cmu_info);
}

CLK_OF_DECLARE(trav_clk_core_mifr, "turbo,trav-clock-core_mifr", trav_clk_core_mifr_init);



/* Register Offset definitions for CMU_CPUCL (0x11010000) */
#define PLL_LOCKTIME_PLL_CPUCL					0x0
#define PLL_CON0_CPUCL_SWITCHCLK_MUX				0x120
#define PLL_CON0_PLL_CPUCL					0x140
#define MUX_CLK_CLUSTER_CLK_MUX					0x1000
#define DIV_CLK_CLUSTER_CLK					0x1800
#define DIV_CLK_CPUCL_DROOP_DETECTOR				0x1804
#define DIV_CPUCL_CMUREF_CLK					0x1808
#define DIV_CPUCL_HPM						0x180c
#define DIV_CPUCL_PCLK						0x1810
#define DIV_CPUCL_PCLK_DD					0x1814
#define DIV_CLK_CPUCL0_CPU					0x1818
#define DIV_CLK_CPUCL1_CPU					0x181c
#define DIV_CLK_CPUCL2_CPU					0x1820
#define DIV_CPUCL0_ACLK						0x1824
#define DIV_CPUCL0_ADB400					0x1828
#define DIV_CPUCL0_ATCLK					0x182c
#define DIV_CPUCL0_CNTCLK					0x1830
#define DIV_CPUCL0_PCLKDBG					0x1834
#define DIV_CPUCL1_ACLK						0x1838
#define DIV_CPUCL1_ADB400					0x183c
#define DIV_CPUCL1_ATCLK					0x1840
#define DIV_CPUCL1_CNTCLK					0x1844
#define DIV_CPUCL1_PCLKDBG					0x1848
#define DIV_CPUCL2_ACLK						0x184c
#define DIV_CPUCL2_ADB400					0x1850
#define DIV_CPUCL2_ATCLK					0x1854
#define DIV_CPUCL2_CNTCLK					0x1858
#define DIV_CPUCL2_PCLKDBG					0x185c
#define GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_CLK			0x2004
#define GAT_CPUCL_CLUSTER0_IPCLKPORT_CLK			0x2008
#define GAT_CPUCL_CLUSTER0_IPCLKPORT_PCLKDBG			0x200c
#define GAT_CPUCL_CLUSTER1_IPCLKPORT_CLK			0x2010
#define GAT_CPUCL_CLUSTER1_IPCLKPORT_PCLKDBG			0x2014
#define GAT_CPUCL_CLUSTER2_IPCLKPORT_CLK			0x2018
#define GAT_CPUCL_CLUSTER2_IPCLKPORT_PCLKDBG			0x201c
#define GAT_CPUCL_CPUCL_CMU_CPUCL_IPCLKPORT_PCLK		0x2020
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_CK_IN	0x2024
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_CK_IN	0x2028
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_CK_IN	0x202c
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_CK_IN	0x2030
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_CK_IN	0x2034
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_CK_IN	0x2038
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_CK_IN	0x203c
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_CK_IN	0x2040
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_CK_IN	0x2044
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_CK_IN	0x2048
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_CK_IN	0x204c
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_CK_IN	0x2050
#define GAT_CPUCL_HPM_CPUCL_IPCLKPORT_I_HPM_TARGETCLK_C		0x2054
#define GAT_CPUCL_HPM_GATE					0x2058
#define GAT_CPUCL_PCLK_GATE					0x205c
#define GAT_CPUCL_CLUSTER_CLK					0x2060
#define GAT_CPUCL0_ACLK						0x2064
#define GAT_CPUCL0_ADB400					0x2068
#define GAT_CPUCL0_ATCLK					0x206c
#define GAT_CPUCL0_CNTCLK					0x2070
#define GAT_CPUCL0_PCLKDBG					0x2074
#define GAT_CPUCL1_ACLK						0x2078
#define GAT_CPUCL1_ADB400					0x207c
#define GAT_CPUCL1_ATCLK					0x2080
#define GAT_CPUCL1_CNTCLK					0x2084
#define GAT_CPUCL1_PCLKDBG					0x2088
#define GAT_CPUCL2_ACLK						0x208c
#define GAT_CPUCL2_ADB400					0x2090
#define GAT_CPUCL2_ATCLK					0x2094
#define GAT_CPUCL2_CNTCLK					0x2098
#define GAT_CPUCL2_PCLKDBG					0x209c
#define GAT_CPUCL_CMUREF_CLK					0x20a0
#define GAT_CPUCL_DROOP_DETECTOR				0x20a4
#define GAT_CPUCL_PCLK_DD					0x20a8
#define GAT_CPUCL_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM		0x20ac
#define GAT_CPUCL_ADM_APB_G_CLUSTER1_IPCLKPORT_PCLKM		0x20b0
#define GAT_CPUCL_ADM_APB_G_CLUSTER2_IPCLKPORT_PCLKM		0x20b4
#define GAT_CPUCL_ADM_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKM		0x20b8
#define GAT_CPUCL_ADM_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKM		0x20bc
#define GAT_CPUCL_ADM_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKM		0x20c0
#define GAT_CPUCL_ADS_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKS		0x20c4
#define GAT_CPUCL_ADS_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKS		0x20c8
#define GAT_CPUCL_ADS_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKS		0x20cc
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM		0x20d0
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS		0x20d4
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKM		0x20d8
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKS		0x20dc
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKM		0x20e0
#define GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKS		0x20e4
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKM	0x20e8
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKS	0x20ec
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKM	0x20f0
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKS	0x20f4
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKM	0x20f8
#define GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKS	0x20fc
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKM	0x2100
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKS	0x2104
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKM	0x2108
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKS	0x210c
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKM	0x2110
#define GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKS	0x2114
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKM	0x2118
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKS	0x211c
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKM	0x2120
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKS	0x2124
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKM	0x2128
#define GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKS	0x212c
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKM	0x2130
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKS	0x2134
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKM	0x2138
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKS	0x213c
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKM	0x2140
#define GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKS	0x2144
#define GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER0_IPCLKPORT_I_CLK	0x2148
#define GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER1_IPCLKPORT_I_CLK	0x214c
#define GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER2_IPCLKPORT_I_CLK	0x2150
#define GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER0_IPCLKPORT_I_CLK	0x2154
#define GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER1_IPCLKPORT_I_CLK	0x2158
#define GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER2_IPCLKPORT_I_CLK	0x215c
#define GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER0_IPCLKPORT_I_CLK	0x2160
#define GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER1_IPCLKPORT_I_CLK	0x2164
#define GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER2_IPCLKPORT_I_CLK	0x2168
#define GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER0_IPCLKPORT_I_CLK	0x216c
#define GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER1_IPCLKPORT_I_CLK	0x2170
#define GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER2_IPCLKPORT_I_CLK	0x2174
#define GAT_CPUCL_AXI2APB_CPUCL_S0_IPCLKPORT_ACLK		0x2178
#define GAT_CPUCL_AXI2APB_CPUCL_S1_IPCLKPORT_ACLK		0x217c
#define GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_PCLK			0x2180
#define GAT_CPUCL_BUS_P_CPUCL_IPCLKPORT_MAINCLK			0x2184
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_PCLK		0x2188
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_PCLK		0x218c
#define GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_PCLK		0x2190
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_PCLK		0x2194
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_PCLK		0x2198
#define GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_PCLK		0x219c
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_PCLK		0x21a0
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_PCLK		0x21a4
#define GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_PCLK		0x21a8
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_PCLK		0x21ac
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_PCLK		0x21b0
#define GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_PCLK		0x21b4
#define GAT_CPUCL_DUMP_PC_CPUCL0_IPCLKPORT_I_PCLK		0x21b8
#define GAT_CPUCL_DUMP_PC_CPUCL1_IPCLKPORT_I_PCLK		0x21bc
#define GAT_CPUCL_DUMP_PC_CPUCL2_IPCLKPORT_I_PCLK		0x21c0
#define GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D1	0x21c4
#define GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D2	0x21c8
#define GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D3	0x21cc
#define GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_P	0x21d0
#define GAT_CPUCL_SYSREG_CPUCL0_IPCLKPORT_PCLK			0x21d4
#define GAT_CPUCL_SYSREG_CPUCL1_IPCLKPORT_PCLK			0x21d8
#define GAT_CPUCL_SYSREG_CPUCL2_IPCLKPORT_PCLK			0x21dc

static const unsigned long cpucl_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CPUCL,
	PLL_CON0_CPUCL_SWITCHCLK_MUX,
	PLL_CON0_PLL_CPUCL,
	MUX_CLK_CLUSTER_CLK_MUX,
	DIV_CLK_CLUSTER_CLK,
	DIV_CLK_CPUCL_DROOP_DETECTOR,
	DIV_CPUCL_CMUREF_CLK,
	DIV_CPUCL_HPM,
	DIV_CPUCL_PCLK,
	DIV_CPUCL_PCLK_DD,
	DIV_CLK_CPUCL0_CPU,
	DIV_CLK_CPUCL1_CPU,
	DIV_CLK_CPUCL2_CPU,
	DIV_CPUCL0_ACLK,
	DIV_CPUCL0_ADB400,
	DIV_CPUCL0_ATCLK,
	DIV_CPUCL0_CNTCLK,
	DIV_CPUCL0_PCLKDBG,
	DIV_CPUCL1_ACLK,
	DIV_CPUCL1_ADB400,
	DIV_CPUCL1_ATCLK,
	DIV_CPUCL1_CNTCLK,
	DIV_CPUCL1_PCLKDBG,
	DIV_CPUCL2_ACLK,
	DIV_CPUCL2_ADB400,
	DIV_CPUCL2_ATCLK,
	DIV_CPUCL2_CNTCLK,
	DIV_CPUCL2_PCLKDBG,
	GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_CLK,
	GAT_CPUCL_CLUSTER0_IPCLKPORT_CLK,
	GAT_CPUCL_CLUSTER0_IPCLKPORT_PCLKDBG,
	GAT_CPUCL_CLUSTER1_IPCLKPORT_CLK,
	GAT_CPUCL_CLUSTER1_IPCLKPORT_PCLKDBG,
	GAT_CPUCL_CLUSTER2_IPCLKPORT_CLK,
	GAT_CPUCL_CLUSTER2_IPCLKPORT_PCLKDBG,
	GAT_CPUCL_CPUCL_CMU_CPUCL_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_CK_IN,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_CK_IN,
	GAT_CPUCL_HPM_CPUCL_IPCLKPORT_I_HPM_TARGETCLK_C,
	GAT_CPUCL_HPM_GATE,
	GAT_CPUCL_PCLK_GATE,
	GAT_CPUCL_CLUSTER_CLK,
	GAT_CPUCL0_ACLK,
	GAT_CPUCL0_ADB400,
	GAT_CPUCL0_ATCLK,
	GAT_CPUCL0_CNTCLK,
	GAT_CPUCL0_PCLKDBG,
	GAT_CPUCL1_ACLK,
	GAT_CPUCL1_ADB400,
	GAT_CPUCL1_ATCLK,
	GAT_CPUCL1_CNTCLK,
	GAT_CPUCL1_PCLKDBG,
	GAT_CPUCL2_ACLK,
	GAT_CPUCL2_ADB400,
	GAT_CPUCL2_ATCLK,
	GAT_CPUCL2_CNTCLK,
	GAT_CPUCL2_PCLKDBG,
	GAT_CPUCL_CMUREF_CLK,
	GAT_CPUCL_DROOP_DETECTOR,
	GAT_CPUCL_PCLK_DD,
	GAT_CPUCL_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
	GAT_CPUCL_ADM_APB_G_CLUSTER1_IPCLKPORT_PCLKM,
	GAT_CPUCL_ADM_APB_G_CLUSTER2_IPCLKPORT_PCLKM,
	GAT_CPUCL_ADM_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKM,
	GAT_CPUCL_ADM_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKM,
	GAT_CPUCL_ADM_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKM,
	GAT_CPUCL_ADS_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKS,
	GAT_CPUCL_ADS_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKS,
	GAT_CPUCL_ADS_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKS,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKS,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKM,
	GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKS,
	GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER0_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER1_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER2_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER0_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER1_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER2_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER0_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER1_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER2_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER0_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER1_IPCLKPORT_I_CLK,
	GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER2_IPCLKPORT_I_CLK,
	GAT_CPUCL_AXI2APB_CPUCL_S0_IPCLKPORT_ACLK,
	GAT_CPUCL_AXI2APB_CPUCL_S1_IPCLKPORT_ACLK,
	GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_PCLK,
	GAT_CPUCL_BUS_P_CPUCL_IPCLKPORT_MAINCLK,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_PCLK,
	GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_PCLK,
	GAT_CPUCL_DUMP_PC_CPUCL0_IPCLKPORT_I_PCLK,
	GAT_CPUCL_DUMP_PC_CPUCL1_IPCLKPORT_I_PCLK,
	GAT_CPUCL_DUMP_PC_CPUCL2_IPCLKPORT_I_PCLK,
	GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D1,
	GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D2,
	GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D3,
	GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_P,
	GAT_CPUCL_SYSREG_CPUCL0_IPCLKPORT_PCLK,
	GAT_CPUCL_SYSREG_CPUCL1_IPCLKPORT_PCLK,
	GAT_CPUCL_SYSREG_CPUCL2_IPCLKPORT_PCLK,
};

static const struct samsung_pll_rate_table pll_cpucl_rate_table[] __initconst = {
	PLL_35XX_RATE(2600000000, 325, 3, 0),
	PLL_35XX_RATE(2400000000, 300, 3, 0),
	PLL_35XX_RATE(2200000000, 275, 3, 0),
	PLL_35XX_RATE(2000000000, 250, 3, 0),
	PLL_35XX_RATE(1600000000, 200, 3, 0),
	PLL_35XX_RATE(1500000000, 125, 2, 0),
	PLL_35XX_RATE(1400000000, 175, 3, 0),
	PLL_35XX_RATE(1200000000, 100, 2, 0),
	PLL_35XX_RATE(1000000000, 125, 3, 0),
	PLL_35XX_RATE(950000000, 475, 3, 2),
	PLL_35XX_RATE(850000000, 425, 3, 2),
};

static const struct samsung_pll_clock cpucl_pll_clks[] __initconst = {
	PLL(pll_142xx, CLK_ARM_CLK, "fout_pll_cpucl", "fin_pll", PLL_LOCKTIME_PLL_CPUCL, PLL_CON0_PLL_CPUCL, pll_cpucl_rate_table),
};

PNAME(mout_cpucl_pll_p) = { "fin_pll", "fout_pll_cpucl" };
PNAME(mout_cpucl_clk_cluster_clk_mux_p) = { "mout_cpucl_pll", "mout_cpucl_switchclk_mux" };
PNAME(mout_cpucl_switchclk_mux_p) = { "fin_pll", "cmu_cpucl_switch_gate" };

static const struct samsung_mux_clock cpucl_mux_clks[] __initconst = {
	MUX(MOUT_CPUCL_PLL, "mout_cpucl_pll", mout_cpucl_pll_p, PLL_CON0_PLL_CPUCL, 4, 1),
	MUX(CPUCL_CLUSTER_CLK_MUX, "mout_cpucl_clk_cluster_clk_mux", mout_cpucl_clk_cluster_clk_mux_p, MUX_CLK_CLUSTER_CLK_MUX, 0, 1),
	MUX(CPUCL_SWITCHCLK_MUX, "mout_cpucl_switchclk_mux", mout_cpucl_switchclk_mux_p, PLL_CON0_CPUCL_SWITCHCLK_MUX, 4, 1),
};

static const struct samsung_div_clock cpucl_div_clks[] __initconst = {
	DIV(0, "dout_clk_cluster_clk", "cpucl_cluster_clk", DIV_CLK_CLUSTER_CLK, 0, 4),
	DIV(0, "dout_clk_cpucl_droop_detector", "cpucl_droop_detector", DIV_CLK_CPUCL_DROOP_DETECTOR, 0, 4),
	DIV(0, "dout_cpucl_cmuref_clk", "cpucl_cmuref_clk", DIV_CPUCL_CMUREF_CLK, 0, 4),
	DIV(0, "dout_cpucl_hpm", "cpucl_hpm_gate", DIV_CPUCL_HPM, 0, 4),
	DIV(0, "dout_cpucl_pclk", "cpucl_pclk_gate", DIV_CPUCL_PCLK, 0, 4),
	DIV(0, "dout_cpucl_pclk_dd", "cpucl_pclk_dd", DIV_CPUCL_PCLK_DD, 0, 4),
	DIV(0, "dout_cpucl0_aclk", "cpucl0_aclk", DIV_CPUCL0_ACLK, 0, 4),
	DIV(0, "dout_cpucl0_adb400", "cpucl0_adb400", DIV_CPUCL0_ADB400, 0, 4),
	DIV(0, "dout_cpucl0_atclk", "cpucl0_atclk", DIV_CPUCL0_ATCLK, 0, 4),
	DIV(0, "dout_cpucl0_cntclk", "cpucl0_cntclk", DIV_CPUCL0_CNTCLK, 0, 4),
	DIV(0, "dout_cpucl0_pclkdbg", "cpucl0_pclkdbg", DIV_CPUCL0_PCLKDBG, 0, 4),
	DIV(0, "dout_cpucl1_aclk", "cpucl1_aclk", DIV_CPUCL1_ACLK, 0, 4),
	DIV(0, "dout_cpucl1_adb400", "cpucl1_adb400", DIV_CPUCL1_ADB400, 0, 4),
	DIV(0, "dout_cpucl1_atclk", "cpucl1_atclk", DIV_CPUCL1_ATCLK, 0, 4),
	DIV(0, "dout_cpucl1_cntclk", "cpucl1_cntclk", DIV_CPUCL1_CNTCLK, 0, 4),
	DIV(0, "dout_cpucl1_pclkdbg", "cpucl1_pclkdbg", DIV_CPUCL1_PCLKDBG, 0, 4),
	DIV(0, "dout_cpucl2_aclk", "cpucl2_aclk", DIV_CPUCL2_ACLK, 0, 4),
	DIV(0, "dout_cpucl2_adb400", "cpucl2_adb400", DIV_CPUCL2_ADB400, 0, 4),
	DIV(0, "dout_cpucl2_atclk", "cpucl2_atclk", DIV_CPUCL2_ATCLK, 0, 4),
	DIV(0, "dout_cpucl2_cntclk", "cpucl2_cntclk", DIV_CPUCL2_CNTCLK, 0, 4),
	DIV(0, "dout_cpucl2_pclkdbg", "cpucl2_pclkdbg", DIV_CPUCL2_PCLKDBG, 0, 4),
};

static const struct samsung_gate_clock cpucl_gate_clks[] __initconst = {
	GATE(0, "cpucl_cluster0_ipclkport_clk", "dout_clk_cpucl0_cpu", GAT_CPUCL_CLUSTER0_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster0_ipclkport_pclkdbg", "dout_cpucl0_pclkdbg", GAT_CPUCL_CLUSTER0_IPCLKPORT_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster1_ipclkport_clk", "dout_clk_cpucl1_cpu", GAT_CPUCL_CLUSTER1_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster1_ipclkport_pclkdbg", "dout_cpucl1_pclkdbg", GAT_CPUCL_CLUSTER1_IPCLKPORT_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster2_ipclkport_clk", "dout_clk_cpucl2_cpu", GAT_CPUCL_CLUSTER2_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster2_ipclkport_pclkdbg", "dout_cpucl2_pclkdbg", GAT_CPUCL_CLUSTER2_IPCLKPORT_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cpucl_cmu_cpucl_ipclkport_pclk", "dout_cpucl_pclk", GAT_CPUCL_CPUCL_CMU_CPUCL_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl0_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl1_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl2_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl0_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl1_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl2_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl0_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl1_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl2_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl0_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl1_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl2_ipclkport_ck_in", "dout_clk_cpucl_droop_detector", GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_hpm_cpucl_ipclkport_i_hpm_targetclk_c", "dout_cpucl_hpm", GAT_CPUCL_HPM_CPUCL_IPCLKPORT_I_HPM_TARGETCLK_C, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_hpm_gate", "dout_clk_cluster_clk", GAT_CPUCL_HPM_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_busif_hpmcpucl_ipclkport_clk", "fin_pll", GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_pclk_gate", "dout_clk_cluster_clk", GAT_CPUCL_PCLK_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cluster_clk", "mout_cpucl_clk_cluster_clk_mux", GAT_CPUCL_CLUSTER_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl0_aclk", "dout_clk_cpucl0_cpu", GAT_CPUCL0_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl0_adb400", "dout_clk_cpucl0_cpu", GAT_CPUCL0_ADB400, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl0_atclk", "dout_clk_cpucl0_cpu", GAT_CPUCL0_ATCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl0_cntclk", "dout_clk_cpucl0_cpu", GAT_CPUCL0_CNTCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl0_pclkdbg", "dout_clk_cpucl0_cpu", GAT_CPUCL0_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl1_aclk", "dout_clk_cpucl1_cpu", GAT_CPUCL1_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl1_adb400", "dout_clk_cpucl1_cpu", GAT_CPUCL1_ADB400, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl1_atclk", "dout_clk_cpucl1_cpu", GAT_CPUCL1_ATCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl1_cntclk", "dout_clk_cpucl1_cpu", GAT_CPUCL1_CNTCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl1_pclkdbg", "dout_clk_cpucl1_cpu", GAT_CPUCL1_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl2_aclk", "dout_clk_cpucl2_cpu", GAT_CPUCL2_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl2_adb400", "dout_clk_cpucl2_cpu", GAT_CPUCL2_ADB400, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl2_atclk", "dout_clk_cpucl2_cpu", GAT_CPUCL2_ATCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl2_cntclk", "dout_clk_cpucl2_cpu", GAT_CPUCL2_CNTCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl2_pclkdbg", "dout_clk_cpucl2_cpu", GAT_CPUCL2_PCLKDBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_cmuref_clk", "dout_clk_cluster_clk", GAT_CPUCL_CMUREF_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector", "dout_clk_cluster_clk", GAT_CPUCL_DROOP_DETECTOR, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_pclk_dd", "dout_clk_cluster_clk", GAT_CPUCL_PCLK_DD, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_apb_g_cluster0_ipclkport_pclkm", "dout_cpucl0_pclkdbg", GAT_CPUCL_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_apb_g_cluster1_ipclkport_pclkm", "dout_cpucl1_pclkdbg", GAT_CPUCL_ADM_APB_G_CLUSTER1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_apb_g_cluster2_ipclkport_pclkm", "dout_cpucl2_pclkdbg", GAT_CPUCL_ADM_APB_G_CLUSTER2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_axi4st_i_cpucl0_ipclkport_aclkm", "dout_cpucl0_adb400", GAT_CPUCL_ADM_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_axi4st_i_cpucl1_ipclkport_aclkm", "dout_cpucl1_adb400", GAT_CPUCL_ADM_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_adm_axi4st_i_cpucl2_ipclkport_aclkm", "dout_cpucl2_adb400", GAT_CPUCL_ADM_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ads_axi4st_i_cpucl0_ipclkport_aclks", "dout_cpucl0_adb400", GAT_CPUCL_ADS_AXI4ST_I_CPUCL0_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ads_axi4st_i_cpucl1_ipclkport_aclks", "dout_cpucl1_adb400", GAT_CPUCL_ADS_AXI4ST_I_CPUCL1_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ads_axi4st_i_cpucl2_ipclkport_aclks", "dout_cpucl2_adb400", GAT_CPUCL_ADS_AXI4ST_I_CPUCL2_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl0_ipclkport_pclkm", "dout_cpucl0_pclkdbg", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl0_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl1_ipclkport_pclkm", "dout_cpucl1_pclkdbg", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl1_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl2_ipclkport_pclkm", "dout_cpucl2_pclkdbg", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_dump_pc_cpucl2_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_DUMP_PC_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl0_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl0_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl1_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl1_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl2_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector0_cpucl2_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR0_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl0_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl0_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl1_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl1_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl2_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector1_cpucl2_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR1_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl0_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl0_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl1_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl1_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl2_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector2_cpucl2_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR2_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl0_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl0_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl1_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl1_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl2_ipclkport_pclkm", "dout_cpucl_pclk_dd", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ad_apb_d_detector3_cpucl2_ipclkport_pclks", "dout_cpucl_pclk", GAT_CPUCL_AD_APB_D_DETECTOR3_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si0_cluster0_ipclkport_i_clk", "dout_cpucl0_atclk", GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER0_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si0_cluster1_ipclkport_i_clk", "dout_cpucl1_atclk", GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER1_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si0_cluster2_ipclkport_i_clk", "dout_cpucl2_atclk", GAT_CPUCL_ASYNC_ATB_SI0_CLUSTER2_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si1_cluster0_ipclkport_i_clk", "dout_cpucl0_atclk", GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER0_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si1_cluster1_ipclkport_i_clk", "dout_cpucl1_atclk", GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER1_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si1_cluster2_ipclkport_i_clk", "dout_cpucl2_atclk", GAT_CPUCL_ASYNC_ATB_SI1_CLUSTER2_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si2_cluster0_ipclkport_i_clk", "dout_cpucl0_atclk", GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER0_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si2_cluster1_ipclkport_i_clk", "dout_cpucl1_atclk", GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER1_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si2_cluster2_ipclkport_i_clk", "dout_cpucl2_atclk", GAT_CPUCL_ASYNC_ATB_SI2_CLUSTER2_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si3_cluster0_ipclkport_i_clk", "dout_cpucl0_atclk", GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER0_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si3_cluster1_ipclkport_i_clk", "dout_cpucl1_atclk", GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER1_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_async_atb_si3_cluster2_ipclkport_i_clk", "dout_cpucl2_atclk", GAT_CPUCL_ASYNC_ATB_SI3_CLUSTER2_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_axi2apb_cpucl_s0_ipclkport_aclk", "dout_cpucl_pclk", GAT_CPUCL_AXI2APB_CPUCL_S0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_axi2apb_cpucl_s1_ipclkport_aclk", "dout_cpucl_pclk", GAT_CPUCL_AXI2APB_CPUCL_S1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_busif_hpmcpucl_ipclkport_pclk", "dout_cpucl_pclk", GAT_CPUCL_BUSIF_HPMCPUCL_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_bus_p_cpucl_ipclkport_mainclk", "dout_cpucl_pclk", GAT_CPUCL_BUS_P_CPUCL_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl0_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR0_CPUCL0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl1_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR0_CPUCL1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector0_cpucl2_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR0_CPUCL2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl0_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR1_CPUCL0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl1_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR1_CPUCL1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector1_cpucl2_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR1_CPUCL2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl0_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR2_CPUCL0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl1_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR2_CPUCL1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector2_cpucl2_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR2_CPUCL2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl0_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR3_CPUCL0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl1_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR3_CPUCL1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_droop_detector3_cpucl2_ipclkport_pclk", "dout_cpucl_pclk_dd", GAT_CPUCL_DROOP_DETECTOR3_CPUCL2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_dump_pc_cpucl0_ipclkport_i_pclk", "dout_cpucl0_pclkdbg", GAT_CPUCL_DUMP_PC_CPUCL0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_dump_pc_cpucl1_ipclkport_i_pclk", "dout_cpucl1_pclkdbg", GAT_CPUCL_DUMP_PC_CPUCL1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_dump_pc_cpucl2_ipclkport_i_pclk", "dout_cpucl2_pclkdbg", GAT_CPUCL_DUMP_PC_CPUCL2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ns_brdg_cpucl_ipclkport_clk__pcpucl__clk_cpucl_d1", "dout_cpucl0_aclk", GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D1, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ns_brdg_cpucl_ipclkport_clk__pcpucl__clk_cpucl_d2", "dout_cpucl1_aclk", GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D2, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ns_brdg_cpucl_ipclkport_clk__pcpucl__clk_cpucl_d3", "dout_cpucl2_aclk", GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_D3, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_ns_brdg_cpucl_ipclkport_clk__pcpucl__clk_cpucl_p", "dout_cpucl_pclk", GAT_CPUCL_NS_BRDG_CPUCL_IPCLKPORT_CLK__PCPUCL__CLK_CPUCL_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_sysreg_cpucl0_ipclkport_pclk", "dout_cpucl_pclk", GAT_CPUCL_SYSREG_CPUCL0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_sysreg_cpucl1_ipclkport_pclk", "dout_cpucl_pclk", GAT_CPUCL_SYSREG_CPUCL1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cpucl_sysreg_cpucl2_ipclkport_pclk", "dout_cpucl_pclk", GAT_CPUCL_SYSREG_CPUCL2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_fixed_factor_clock cpucl_fixed_factor_clks[] __initconst = {
	FFACTOR(0, "dout_clk_cpucl0_cpu", "dout_clk_cluster_clk", 1, 1, 0),
	FFACTOR(0, "dout_clk_cpucl1_cpu", "dout_clk_cluster_clk", 1, 1, 0),
	FFACTOR(0, "dout_clk_cpucl2_cpu", "dout_clk_cluster_clk", 1, 1, 0),
};

static const struct samsung_cmu_info cpucl_cmu_info __initconst = {
	.pll_clks		= cpucl_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(cpucl_pll_clks),
	.mux_clks		= cpucl_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(cpucl_mux_clks),
	.div_clks		= cpucl_div_clks,
	.nr_div_clks		= ARRAY_SIZE(cpucl_div_clks),
	.gate_clks		= cpucl_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(cpucl_gate_clks),
	.fixed_factor_clks	= cpucl_fixed_factor_clks,
	.nr_fixed_factor_clks	= ARRAY_SIZE(cpucl_fixed_factor_clks),
	.nr_clk_ids		= CPUCL_NR_CLK,
	.clk_regs		= cpucl_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(cpucl_clk_regs),
};

static void __init trav_clk_cpucl_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &cpucl_cmu_info);
}

CLK_OF_DECLARE(trav_clk_cpucl, "turbo,trav-clock-cpucl", trav_clk_cpucl_init);


/* Register Offset definitions for CMU_IMEM (0x10010000) */
#define PLL_CON0_CLK_IMEM_ACLK				0x100
#define PLL_CON0_CLK_IMEM_INTMEMCLK			0x120
#define PLL_CON0_CLK_IMEM_TCUCLK			0x140
#define DIV_OSCCLK_IMEM_TMUTSCLK			0x1800
#define GAT_IMEM_IMEM_CMU_IMEM_IPCLKPORT_PCLK		0x2000
#define GAT_IMEM_MCT_IPCLKPORT_OSCCLK__ALO		0x2004
#define GAT_IMEM_OTP_CON_TOP_IPCLKPORT_I_OSCCLK		0x2008
#define GAT_IMEM_RSTNSYNC_OSCCLK_IPCLKPORT_CLK		0x200c
#define GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK		0x2010
#define GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK_TS		0x2014
#define GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK		0x2018
#define GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK_TS		0x201c
#define GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK		0x2020
#define GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK_TS		0x2024
#define GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK			0x2028
#define GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK_TS		0x202c
#define GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK		0x2030
#define GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK_TS		0x2034
#define GAT_IMEM_WDT0_IPCLKPORT_CLK			0x2038
#define GAT_IMEM_WDT1_IPCLKPORT_CLK			0x203c
#define GAT_IMEM_WDT2_IPCLKPORT_CLK			0x2040
#define GAT_IMEM_ADM_AXI4ST_I0_IMEM_IPCLKPORT_ACLKM	0x2044
#define GAT_IMEM_ADM_AXI4ST_I1_IMEM_IPCLKPORT_ACLKM	0x2048
#define GAT_IMEM_ADM_AXI4ST_I2_IMEM_IPCLKPORT_ACLKM	0x204c
#define GAT_IMEM_ADS_AXI4ST_I0_IMEM_IPCLKPORT_ACLKS	0x2050
#define GAT_IMEM_ADS_AXI4ST_I1_IMEM_IPCLKPORT_ACLKS	0x2054
#define GAT_IMEM_ADS_AXI4ST_I2_IMEM_IPCLKPORT_ACLKS	0x2058
#define GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKM		0x205c
#define GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKS		0x2060
#define GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKM		0x2064
#define GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKS		0x2068
#define GAT_IMEM_AXI2APB_IMEMP0_IPCLKPORT_ACLK		0x206c
#define GAT_IMEM_AXI2APB_IMEMP1_IPCLKPORT_ACLK		0x2070
#define GAT_IMEM_BUS_D_IMEM_IPCLKPORT_MAINCLK		0x2074
#define GAT_IMEM_BUS_P_IMEM_IPCLKPORT_MAINCLK		0x2078
#define GAT_IMEM_BUS_P_IMEM_IPCLKPORT_PERICLK		0x207c
#define GAT_IMEM_BUS_P_IMEM_IPCLKPORT_TCUCLK		0x2080
#define GAT_IMEM_DMA0_IPCLKPORT_ACLK			0x2084
#define GAT_IMEM_DMA1_IPCLKPORT_ACLK			0x2088
#define GAT_IMEM_GIC500_INPUT_SYNC_IPCLKPORT_CLK	0x208c
#define GAT_IMEM_GIC_IPCLKPORT_CLK			0x2090
#define GAT_IMEM_INTMEM_IPCLKPORT_ACLK			0x2094
#define GAT_IMEM_MAILBOX_SCS_CA72_IPCLKPORT_PCLK	0x2098
#define GAT_IMEM_MAILBOX_SMS_CA72_IPCLKPORT_PCLK	0x209c
#define GAT_IMEM_MCT_IPCLKPORT_PCLK			0x20a0
#define GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_D	0x20a4
#define GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_TCU	0x20a8
#define GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSOC_IMEM__CLK_IMEM_P	0x20ac
#define GAT_IMEM_OTP_CON_TOP_IPCLKPORT_PCLK		0x20b0
#define GAT_IMEM_RSTNSYNC_ACLK_IPCLKPORT_CLK		0x20b4
#define GAT_IMEM_RSTNSYNC_INTMEMCLK_IPCLKPORT_CLK	0x20b8
#define GAT_IMEM_RSTNSYNC_TCUCLK_IPCLKPORT_CLK		0x20bc
#define GAT_IMEM_SFRIF_TMU0_IMEM_IPCLKPORT_PCLK		0x20c0
#define GAT_IMEM_SFRIF_TMU1_IMEM_IPCLKPORT_PCLK		0x20c4
#define GAT_IMEM_SYSREG_IMEM_IPCLKPORT_PCLK		0x20c8
#define GAT_IMEM_TBU_IMEM_IPCLKPORT_ACLK		0x20cc
#define GAT_IMEM_TCU_IPCLKPORT_ACLK			0x20d0
#define GAT_IMEM_WDT0_IPCLKPORT_PCLK			0x20d4
#define GAT_IMEM_WDT1_IPCLKPORT_PCLK			0x20d8
#define GAT_IMEM_WDT2_IPCLKPORT_PCLK			0x20dc

static const unsigned long imem_clk_regs[] __initconst = {
	PLL_CON0_CLK_IMEM_ACLK,
	PLL_CON0_CLK_IMEM_INTMEMCLK,
	PLL_CON0_CLK_IMEM_TCUCLK,
	DIV_OSCCLK_IMEM_TMUTSCLK,
	GAT_IMEM_IMEM_CMU_IMEM_IPCLKPORT_PCLK,
	GAT_IMEM_MCT_IPCLKPORT_OSCCLK__ALO,
	GAT_IMEM_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
	GAT_IMEM_RSTNSYNC_OSCCLK_IPCLKPORT_CLK,
	GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK,
	GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK_TS,
	GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK,
	GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK_TS,
	GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK,
	GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK_TS,
	GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK,
	GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK_TS,
	GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK,
	GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK_TS,
	GAT_IMEM_WDT0_IPCLKPORT_CLK,
	GAT_IMEM_WDT1_IPCLKPORT_CLK,
	GAT_IMEM_WDT2_IPCLKPORT_CLK,
	GAT_IMEM_ADM_AXI4ST_I0_IMEM_IPCLKPORT_ACLKM,
	GAT_IMEM_ADM_AXI4ST_I1_IMEM_IPCLKPORT_ACLKM,
	GAT_IMEM_ADM_AXI4ST_I2_IMEM_IPCLKPORT_ACLKM,
	GAT_IMEM_ADS_AXI4ST_I0_IMEM_IPCLKPORT_ACLKS,
	GAT_IMEM_ADS_AXI4ST_I1_IMEM_IPCLKPORT_ACLKS,
	GAT_IMEM_ADS_AXI4ST_I2_IMEM_IPCLKPORT_ACLKS,
	GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKM,
	GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKS,
	GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKM,
	GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKS,
	GAT_IMEM_AXI2APB_IMEMP0_IPCLKPORT_ACLK,
	GAT_IMEM_AXI2APB_IMEMP1_IPCLKPORT_ACLK,
	GAT_IMEM_BUS_D_IMEM_IPCLKPORT_MAINCLK,
	GAT_IMEM_BUS_P_IMEM_IPCLKPORT_MAINCLK,
	GAT_IMEM_BUS_P_IMEM_IPCLKPORT_PERICLK,
	GAT_IMEM_BUS_P_IMEM_IPCLKPORT_TCUCLK,
	GAT_IMEM_DMA0_IPCLKPORT_ACLK,
	GAT_IMEM_DMA1_IPCLKPORT_ACLK,
	GAT_IMEM_GIC500_INPUT_SYNC_IPCLKPORT_CLK,
	GAT_IMEM_GIC_IPCLKPORT_CLK,
	GAT_IMEM_INTMEM_IPCLKPORT_ACLK,
	GAT_IMEM_MAILBOX_SCS_CA72_IPCLKPORT_PCLK,
	GAT_IMEM_MAILBOX_SMS_CA72_IPCLKPORT_PCLK,
	GAT_IMEM_MCT_IPCLKPORT_PCLK,
	GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_D,
	GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_TCU,
	GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSOC_IMEM__CLK_IMEM_P,
	GAT_IMEM_OTP_CON_TOP_IPCLKPORT_PCLK,
	GAT_IMEM_RSTNSYNC_ACLK_IPCLKPORT_CLK,
	GAT_IMEM_RSTNSYNC_INTMEMCLK_IPCLKPORT_CLK,
	GAT_IMEM_RSTNSYNC_TCUCLK_IPCLKPORT_CLK,
	GAT_IMEM_SFRIF_TMU0_IMEM_IPCLKPORT_PCLK,
	GAT_IMEM_SFRIF_TMU1_IMEM_IPCLKPORT_PCLK,
	GAT_IMEM_SYSREG_IMEM_IPCLKPORT_PCLK,
	GAT_IMEM_TBU_IMEM_IPCLKPORT_ACLK,
	GAT_IMEM_TCU_IPCLKPORT_ACLK,
	GAT_IMEM_WDT0_IPCLKPORT_PCLK,
	GAT_IMEM_WDT1_IPCLKPORT_PCLK,
	GAT_IMEM_WDT2_IPCLKPORT_PCLK,
};

PNAME(mout_imem_clk_imem_tcuclk_p) = { "fin_pll", "dout_cmu_imem_tcuclk" };
PNAME(mout_imem_clk_imem_aclk_p) = { "fin_pll", "dout_cmu_imem_aclk" };
PNAME(mout_imem_clk_imem_intmemclk_p) = { "fin_pll", "dout_cmu_imem_dmaclk" };

static const struct samsung_mux_clock imem_mux_clks[] __initconst = {
	MUX(0, "mout_imem_clk_imem_tcuclk", mout_imem_clk_imem_tcuclk_p, PLL_CON0_CLK_IMEM_TCUCLK, 4, 1),
	MUX(0, "mout_imem_clk_imem_aclk", mout_imem_clk_imem_aclk_p, PLL_CON0_CLK_IMEM_ACLK, 4, 1),
	MUX(0, "mout_imem_clk_imem_intmemclk", mout_imem_clk_imem_intmemclk_p, PLL_CON0_CLK_IMEM_INTMEMCLK, 4, 1),
};

static const struct samsung_div_clock imem_div_clks[] __initconst = {
	DIV(0, "dout_imem_oscclk_imem_tmutsclk", "fin_pll", DIV_OSCCLK_IMEM_TMUTSCLK, 0, 4),
};

static const struct samsung_gate_clock imem_gate_clks[] __initconst = {
	GATE(0, "imem_imem_cmu_imem_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_IMEM_CMU_IMEM_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_otp_con_top_ipclkport_i_oscclk", "fin_pll", GAT_IMEM_OTP_CON_TOP_IPCLKPORT_I_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tmu_top_ipclkport_i_clk", "fin_pll", GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tmu_gt_ipclkport_i_clk", "fin_pll", GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tmu_cpu0_ipclkport_i_clk", "fin_pll", GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tmu_gpu_ipclkport_i_clk", "fin_pll", GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_mct_ipclkport_oscclk__alo", "fin_pll", GAT_IMEM_MCT_IPCLKPORT_OSCCLK__ALO, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_wdt0_ipclkport_clk", "fin_pll", GAT_IMEM_WDT0_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_wdt1_ipclkport_clk", "fin_pll", GAT_IMEM_WDT1_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_wdt2_ipclkport_clk", "fin_pll", GAT_IMEM_WDT2_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_TMU_CPU0_IPCLKPORT_I_CLK_TS, "imem_tmu_cpu0_ipclkport_i_clk_ts", "dout_imem_oscclk_imem_tmutsclk", GAT_IMEM_TMU_CPU0_IPCLKPORT_I_CLK_TS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_TMU_CPU2_IPCLKPORT_I_CLK_TS, "imem_tmu_cpu2_ipclkport_i_clk_ts", "dout_imem_oscclk_imem_tmutsclk", GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK_TS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_TMU_GPU_IPCLKPORT_I_CLK_TS, "imem_tmu_gpu_ipclkport_i_clk_ts", "dout_imem_oscclk_imem_tmutsclk", GAT_IMEM_TMU_GPU_IPCLKPORT_I_CLK_TS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_TMU_GT_IPCLKPORT_I_CLK_TS, "imem_tmu_gt_ipclkport_i_clk_ts", "dout_imem_oscclk_imem_tmutsclk", GAT_IMEM_TMU_GT_IPCLKPORT_I_CLK_TS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_TMU_TOP_IPCLKPORT_I_CLK_TS, "imem_tmu_top_ipclkport_i_clk_ts", "dout_imem_oscclk_imem_tmutsclk", GAT_IMEM_TMU_TOP_IPCLKPORT_I_CLK_TS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_adm_axi4st_i0_imem_ipclkport_aclkm", "mout_imem_clk_imem_aclk", GAT_IMEM_ADM_AXI4ST_I0_IMEM_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_adm_axi4st_i1_imem_ipclkport_aclkm", "mout_imem_clk_imem_aclk", GAT_IMEM_ADM_AXI4ST_I1_IMEM_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_adm_axi4st_i2_imem_ipclkport_aclkm", "mout_imem_clk_imem_aclk", GAT_IMEM_ADM_AXI4ST_I2_IMEM_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ads_axi4st_i0_imem_ipclkport_aclks", "mout_imem_clk_imem_aclk", GAT_IMEM_ADS_AXI4ST_I0_IMEM_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ads_axi4st_i1_imem_ipclkport_aclks", "mout_imem_clk_imem_aclk", GAT_IMEM_ADS_AXI4ST_I1_IMEM_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ads_axi4st_i2_imem_ipclkport_aclks", "mout_imem_clk_imem_aclk", GAT_IMEM_ADS_AXI4ST_I2_IMEM_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_async_dma0_ipclkport_pclkm", "mout_imem_clk_imem_tcuclk", GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_async_dma0_ipclkport_pclks", "mout_imem_clk_imem_aclk", GAT_IMEM_ASYNC_DMA0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_async_dma1_ipclkport_pclkm", "mout_imem_clk_imem_tcuclk", GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_async_dma1_ipclkport_pclks", "mout_imem_clk_imem_aclk", GAT_IMEM_ASYNC_DMA1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_axi2apb_imemp0_ipclkport_aclk", "mout_imem_clk_imem_aclk", GAT_IMEM_AXI2APB_IMEMP0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_axi2apb_imemp1_ipclkport_aclk", "mout_imem_clk_imem_aclk", GAT_IMEM_AXI2APB_IMEMP1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_bus_d_imem_ipclkport_mainclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_BUS_D_IMEM_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_bus_p_imem_ipclkport_mainclk", "mout_imem_clk_imem_aclk", GAT_IMEM_BUS_P_IMEM_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_bus_p_imem_ipclkport_pericclk", "mout_imem_clk_imem_aclk", GAT_IMEM_BUS_P_IMEM_IPCLKPORT_PERICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_bus_p_imem_ipclkport_tcuclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_BUS_P_IMEM_IPCLKPORT_TCUCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_DMA0_IPCLKPORT_ACLK, "imem_dma0_ipclkport_aclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_DMA0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED | CLK_IS_CRITICAL, 0),
	GATE(IMEM_DMA1_IPCLKPORT_ACLK, "imem_dma1_ipclkport_aclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_DMA1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED | CLK_IS_CRITICAL, 0),
	GATE(0, "imem_gic500_input_sync_ipclkport_clk", "mout_imem_clk_imem_aclk", GAT_IMEM_GIC500_INPUT_SYNC_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_gic_ipclkport_clk", "mout_imem_clk_imem_aclk", GAT_IMEM_GIC_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_intmem_ipclkport_aclk", "mout_imem_clk_imem_intmemclk", GAT_IMEM_INTMEM_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_mailbox_scs_ca72_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_MAILBOX_SCS_CA72_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_mailbox_sms_ca72_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_MAILBOX_SMS_CA72_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_MCT_PCLK, "imem_mct_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_MCT_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ns_brdg_imem_ipclkport_clk__psco_imem__clk_imem_d", "mout_imem_clk_imem_tcuclk", GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ns_brdg_imem_ipclkport_clk__psco_imem__clk_imem_tcu", "mout_imem_clk_imem_tcuclk", GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSCO_IMEM__CLK_IMEM_TCU, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_ns_brdg_imem_ipclkport_clk__psoc_imem__clk_imem_p", "mout_imem_clk_imem_aclk", GAT_IMEM_NS_BRDG_IMEM_IPCLKPORT_CLK__PSOC_IMEM__CLK_IMEM_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_otp_con_top_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_OTP_CON_TOP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_rstnsync_aclk_ipclkport_clk", "mout_imem_clk_imem_aclk", GAT_IMEM_RSTNSYNC_ACLK_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_rstnsync_oscclk_ipclkport_clk", "fin_pll", GAT_IMEM_RSTNSYNC_OSCCLK_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_rstnsync_intmemclk_ipclkport_clk", "mout_imem_clk_imem_intmemclk", GAT_IMEM_RSTNSYNC_INTMEMCLK_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_rstnsync_tcuclk_ipclkport_clk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_RSTNSYNC_TCUCLK_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_sfrif_tmu0_imem_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_SFRIF_TMU0_IMEM_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_sfrif_tmu1_imem_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_SFRIF_TMU1_IMEM_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tmu_cpu2_ipclkport_i_clk", "fin_pll", GAT_IMEM_TMU_CPU2_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_sysreg_imem_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_SYSREG_IMEM_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tbu_imem_ipclkport_aclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_TBU_IMEM_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "imem_tcu_ipclkport_aclk", "mout_imem_clk_imem_tcuclk", GAT_IMEM_TCU_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_WDT0_IPCLKPORT_PCLK, "imem_wdt0_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_WDT0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_WDT1_IPCLKPORT_PCLK, "imem_wdt1_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_WDT1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(IMEM_WDT2_IPCLKPORT_PCLK, "imem_wdt2_ipclkport_pclk", "mout_imem_clk_imem_aclk", GAT_IMEM_WDT2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info imem_cmu_info __initconst = {
	.mux_clks		= imem_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(imem_mux_clks),
	.div_clks		= imem_div_clks,
	.nr_div_clks		= ARRAY_SIZE(imem_div_clks),
	.gate_clks		= imem_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(imem_gate_clks),
	.nr_clk_ids		= IMEM_NR_CLK,
	.clk_regs		= imem_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(imem_clk_regs),
};

static void __init trav_clk_imem_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &imem_cmu_info);
}

CLK_OF_DECLARE(trav_clk_imem, "turbo,trav-clock-imem", trav_clk_imem_init);


/* Register Offset definitions for CMU_PMU (0x11410000) */
#define PLL_CON0_PMU_ATCLK_SCS					0x100
#define PLL_CON0_PMU_BUSP_SCS					0x120
#define DIV_CLK_DBG_ATCLK					0x1800
#define DIV_CLK_PMU_BUSP					0x1804
#define GAT_PMU_GPIO_PMU_IPCLKPORT_OSCCLK			0x2000
#define GAT_PMU_CMU_PMU_IPCLKPORT_PCLK				0x2004
#define GAT_PMU_IPCLKPORT_OSCCLK				0x2008
#define GAT_PMU_IPCLKPORT_XTCXO					0x200c
#define GAT_PMU_RSTNSYNC_CLKCMU_PMU_OSCCLK_IPCLKPORT_CLK	0x2010
#define GAT_PMU_ADS_AHB_G_SCS_IPCLKPORT_HCLKS			0x2014
#define GAT_PMU_ADS_APB_G_CORE_IPCLKPORT_PCLKS			0x2018
#define GAT_PMU_ADS_APB_G_CPUCL0_IPCLKPORT_PCLKS		0x201c
#define GAT_PMU_ADS_APB_G_CPUCL1_IPCLKPORT_PCLKS		0x2020
#define GAT_PMU_ADS_APB_G_CPUCL2_IPCLKPORT_PCLKS		0x2024
#define GAT_PMU_ADS_APB_G_SMS_IPCLKPORT_PCLKS			0x2028
#define GAT_PMU_ADS_AXI_G_SCS_IPCLKPORT_ACLKS			0x202c
#define GAT_PMU_ASYNC_ATB_MI0_CPUCL0_IPCLKPORT_ATCLKM		0x2030
#define GAT_PMU_ASYNC_ATB_MI0_CPUCL1_IPCLKPORT_ATCLKM		0x2034
#define GAT_PMU_ASYNC_ATB_MI0_CPUCL2_IPCLKPORT_ATCLKM		0x2038
#define GAT_PMU_ASYNC_ATB_MI1_CPUCL0_IPCLKPORT_ATCLKM		0x203c
#define GAT_PMU_ASYNC_ATB_MI1_CPUCL1_IPCLKPORT_ATCLKM		0x2040
#define GAT_PMU_ASYNC_ATB_MI1_CPUCL2_IPCLKPORT_ATCLKM		0x2044
#define GAT_PMU_ASYNC_ATB_MI2_CPUCL0_IPCLKPORT_ATCLKM		0x2048
#define GAT_PMU_ASYNC_ATB_MI2_CPUCL1_IPCLKPORT_ATCLKM		0x204c
#define GAT_PMU_ASYNC_ATB_MI2_CPUCL2_IPCLKPORT_ATCLKM		0x2050
#define GAT_PMU_ASYNC_ATB_MI3_CPUCL0_IPCLKPORT_ATCLKM		0x2054
#define GAT_PMU_ASYNC_ATB_MI3_CPUCL1_IPCLKPORT_ATCLKM		0x2058
#define GAT_PMU_ASYNC_ATB_MI3_CPUCL2_IPCLKPORT_ATCLKM		0x205c
#define GAT_PMU_ASYNC_ATB_MI_CCC0_IPCLKPORT_ATCLKM		0x2060
#define GAT_PMU_ASYNC_ATB_MI_CCC1_IPCLKPORT_ATCLKM		0x2064
#define GAT_PMU_ASYNC_ATB_MI_CCC2_IPCLKPORT_ATCLKM		0x2068
#define GAT_PMU_ASYNC_ATB_MI_CCC3_IPCLKPORT_ACLK		0x206c
#define GAT_PMU_ASYNC_ATB_MI_SCS_IPCLKPORT_ATCLKM		0x2070
#define GAT_PMU_ASYNC_ATB_MI_SMS_IPCLKPORT_ATCLKM		0x2074
#define GAT_PMU_AXI2APB_CORESIGHT_IPCLKPORT_ACLK		0x2078
#define GAT_PMU_AXI2APB_ERR_IPCLKPORT_ACLK			0x207c
#define GAT_PMU_AXI2APB_PMU_IPCLKPORT_ACLK			0x2080
#define GAT_PMU_BUS_P_PMU_IPCLKPORT_MAINCLK			0x2084
#define GAT_PMU_CSSYS_IPCLKPORT_DBG_ATCLK			0x2088
#define GAT_PMU_CSSYS_IPCLKPORT_PCLK_DBG			0x208c
#define GAT_PMU_GPIO_PMU_IPCLKPORT_PCLK				0x2090
#define GAT_PMU_NS_BRDG_PMU_FROM_CORE_IPCLKPORT_CLK__PPMU__CLK_PMU_P	0x2094
#define GAT_PMU_NS_BRDG_PMU_FROM_SCS_IPCLKPORT_CLK__PPMU__CLK_PMU_P	0x2098
#define GAT_PMU_NS_BRDG_PMU_FROM_SMS_IPCLKPORT_CLK__PPMU__CLK_PMU_P	0x209c
#define GAT_PMU_NS_CSSYS0_IPCLKPORT_ACLK			0x20a0
#define GAT_PMU_NS_CSSYS1_IPCLKPORT_ACLK			0x20a4
#define GAT_PMU_NS_CSSYS2_IPCLKPORT_ACLK			0x20a8
#define GAT_PMU_RSTNSYNC_CLKCMU_PMU_BUS_IPCLKPORT_CLK		0x20ac
#define GAT_PMU_RSTNSYNC_CLKCMU_PMU_CSSYS_IPCLKPORT_CLK		0x20b0
#define GAT_PMU_SECUREJTAG_IPCLKPORT_I_CLK			0x20b4
#define GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_PMU_IPCLKPORT_PCLK	0x20b8
#define GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_SCS_IPCLKPORT_PCLK	0x20bc
#define GAT_PMU_SFRIF_CENTRAL_PMU_IPCLKPORT_PCLK		0x20c0
#define GAT_PMU_SYSREG_PMU_IPCLKPORT_PCLK			0x20c4

static const unsigned long pmu_clk_regs[] __initconst = {
	PLL_CON0_PMU_ATCLK_SCS,
	PLL_CON0_PMU_BUSP_SCS,
	DIV_CLK_DBG_ATCLK,
	DIV_CLK_PMU_BUSP,
	GAT_PMU_GPIO_PMU_IPCLKPORT_OSCCLK,
	GAT_PMU_CMU_PMU_IPCLKPORT_PCLK,
	GAT_PMU_IPCLKPORT_OSCCLK,
	GAT_PMU_IPCLKPORT_XTCXO,
	GAT_PMU_RSTNSYNC_CLKCMU_PMU_OSCCLK_IPCLKPORT_CLK,
	GAT_PMU_ADS_AHB_G_SCS_IPCLKPORT_HCLKS,
	GAT_PMU_ADS_APB_G_CORE_IPCLKPORT_PCLKS,
	GAT_PMU_ADS_APB_G_CPUCL0_IPCLKPORT_PCLKS,
	GAT_PMU_ADS_APB_G_CPUCL1_IPCLKPORT_PCLKS,
	GAT_PMU_ADS_APB_G_CPUCL2_IPCLKPORT_PCLKS,
	GAT_PMU_ADS_APB_G_SMS_IPCLKPORT_PCLKS,
	GAT_PMU_ADS_AXI_G_SCS_IPCLKPORT_ACLKS,
	GAT_PMU_ASYNC_ATB_MI0_CPUCL0_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI0_CPUCL1_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI0_CPUCL2_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI1_CPUCL0_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI1_CPUCL1_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI1_CPUCL2_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI2_CPUCL0_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI2_CPUCL1_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI2_CPUCL2_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI3_CPUCL0_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI3_CPUCL1_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI3_CPUCL2_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI_CCC0_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI_CCC1_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI_CCC2_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI_CCC3_IPCLKPORT_ACLK,
	GAT_PMU_ASYNC_ATB_MI_SCS_IPCLKPORT_ATCLKM,
	GAT_PMU_ASYNC_ATB_MI_SMS_IPCLKPORT_ATCLKM,
	GAT_PMU_AXI2APB_CORESIGHT_IPCLKPORT_ACLK,
	GAT_PMU_AXI2APB_ERR_IPCLKPORT_ACLK,
	GAT_PMU_AXI2APB_PMU_IPCLKPORT_ACLK,
	GAT_PMU_BUS_P_PMU_IPCLKPORT_MAINCLK,
	GAT_PMU_CSSYS_IPCLKPORT_DBG_ATCLK,
	GAT_PMU_CSSYS_IPCLKPORT_PCLK_DBG,
	GAT_PMU_GPIO_PMU_IPCLKPORT_PCLK,
	GAT_PMU_NS_BRDG_PMU_FROM_CORE_IPCLKPORT_CLK__PPMU__CLK_PMU_P,
	GAT_PMU_NS_BRDG_PMU_FROM_SCS_IPCLKPORT_CLK__PPMU__CLK_PMU_P,
	GAT_PMU_NS_BRDG_PMU_FROM_SMS_IPCLKPORT_CLK__PPMU__CLK_PMU_P,
	GAT_PMU_NS_CSSYS0_IPCLKPORT_ACLK,
	GAT_PMU_NS_CSSYS1_IPCLKPORT_ACLK,
	GAT_PMU_NS_CSSYS2_IPCLKPORT_ACLK,
	GAT_PMU_RSTNSYNC_CLKCMU_PMU_BUS_IPCLKPORT_CLK,
	GAT_PMU_RSTNSYNC_CLKCMU_PMU_CSSYS_IPCLKPORT_CLK,
	GAT_PMU_SECUREJTAG_IPCLKPORT_I_CLK,
	GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_PMU_IPCLKPORT_PCLK,
	GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_SCS_IPCLKPORT_PCLK,
	GAT_PMU_SFRIF_CENTRAL_PMU_IPCLKPORT_PCLK,
	GAT_PMU_SYSREG_PMU_IPCLKPORT_PCLK,
};

/* [TODO] How to handle the SCS clocks in A72. Currently defined as FixedRate Clocks */
static const struct samsung_fixed_rate_clock pmu_fixed_clks[] __initconst = {
	FRATE(0, "pll_scs_div16", NULL, 0, 24000000),
	FRATE(0, "pll_scs_div4", NULL, 0, 24000000),
};

PNAME(mout_pmu_busp_scs_p) = { "fin_pll", "pll_scs_div16" };
PNAME(mout_pmu_atclk_scs_p) = { "fin_pll", "pll_scs_div4" };

static const struct samsung_mux_clock pmu_mux_clks[] __initconst = {
	MUX(0, "mout_pmu_busp_scs", mout_pmu_busp_scs_p, PLL_CON0_PMU_BUSP_SCS, 4, 1),
	MUX(0, "mout_pmu_atclk_scs", mout_pmu_atclk_scs_p, PLL_CON0_PMU_ATCLK_SCS, 4, 1),
};

static const struct samsung_div_clock pmu_div_clks[] __initconst = {
	DIV(0, "dout_pmu_clk_dbg_atclk", "mout_pmu_atclk_scs", DIV_CLK_DBG_ATCLK, 0, 4),
	DIV(0, "dout_pmu_clk_pmu_busp", "mout_pmu_busp_scs", DIV_CLK_PMU_BUSP, 0, 4),
};

static const struct samsung_gate_clock pmu_gate_clks[] __initconst = {
	GATE(0, "pmu_cmu_pmu_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_CMU_PMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ipclkport_xtcxo", "fin_pll", GAT_PMU_IPCLKPORT_XTCXO, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_ahb_g_scs_ipclkport_hclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_AHB_G_SCS_IPCLKPORT_HCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_apb_g_core_ipclkport_pclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_APB_G_CORE_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_apb_g_cpucl0_ipclkport_pclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_APB_G_CPUCL0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_apb_g_cpucl1_ipclkport_pclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_APB_G_CPUCL1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_apb_g_cpucl2_ipclkport_pclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_APB_G_CPUCL2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_apb_g_sms_ipclkport_pclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_APB_G_SMS_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ads_axi_g_scs_ipclkport_aclks", "dout_pmu_clk_pmu_busp", GAT_PMU_ADS_AXI_G_SCS_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi0_cpucl0_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI0_CPUCL0_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi0_cpucl1_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI0_CPUCL1_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi0_cpucl2_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI0_CPUCL2_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi1_cpucl0_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI1_CPUCL0_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi1_cpucl1_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI1_CPUCL1_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi1_cpucl2_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI1_CPUCL2_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi2_cpucl0_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI2_CPUCL0_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi2_cpucl1_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI2_CPUCL1_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi2_cpucl2_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI2_CPUCL2_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi3_cpucl0_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI3_CPUCL0_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi3_cpucl1_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI3_CPUCL1_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi3_cpucl2_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI3_CPUCL2_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_ccc0_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_CCC0_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_ccc1_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_CCC1_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_ccc2_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_CCC2_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_ccc3_ipclkport_aclk", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_CCC3_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_scs_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_SCS_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_async_atb_mi_sms_ipclkport_atclkm", "dout_pmu_clk_dbg_atclk", GAT_PMU_ASYNC_ATB_MI_SMS_IPCLKPORT_ATCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_axi2apb_coresight_ipclkport_aclk", "dout_pmu_clk_pmu_busp", GAT_PMU_AXI2APB_CORESIGHT_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_axi2apb_err_ipclkport_aclk", "dout_pmu_clk_pmu_busp", GAT_PMU_AXI2APB_ERR_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_axi2apb_pmu_ipclkport_aclk", "dout_pmu_clk_pmu_busp", GAT_PMU_AXI2APB_PMU_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_bus_p_pmu_ipclkport_mainclk", "dout_pmu_clk_pmu_busp", GAT_PMU_BUS_P_PMU_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_cssys_ipclkport_dbg_atclk", "dout_pmu_clk_dbg_atclk", GAT_PMU_CSSYS_IPCLKPORT_DBG_ATCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_cssys_ipclkport_pclk_dbg", "dout_pmu_clk_pmu_busp", GAT_PMU_CSSYS_IPCLKPORT_PCLK_DBG, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_gpio_pmu_ipclkport_oscclk", "fin_pll", GAT_PMU_GPIO_PMU_IPCLKPORT_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_gpio_pmu_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_GPIO_PMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_brdg_pmu_from_core_ipclkport_clk__ppmu__clk_pmu_p", "dout_pmu_clk_pmu_busp", GAT_PMU_NS_BRDG_PMU_FROM_CORE_IPCLKPORT_CLK__PPMU__CLK_PMU_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_brdg_pmu_from_scs_ipclkport_clk__ppmu__clk_pmu_p", "dout_pmu_clk_pmu_busp", GAT_PMU_NS_BRDG_PMU_FROM_SCS_IPCLKPORT_CLK__PPMU__CLK_PMU_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_brdg_pmu_from_sms_ipclkport_clk__ppmu__clk_pmu_p", "dout_pmu_clk_pmu_busp", GAT_PMU_NS_BRDG_PMU_FROM_SMS_IPCLKPORT_CLK__PPMU__CLK_PMU_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_cssys0_ipclkport_aclk", "dout_pmu_clk_pmu_busp", GAT_PMU_NS_CSSYS0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_cssys1_ipclkport_aclk", "dout_pmu_clk_dbg_atclk", GAT_PMU_NS_CSSYS1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ns_cssys2_ipclkport_aclk", "dout_pmu_clk_dbg_atclk", GAT_PMU_NS_CSSYS2_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_rstnsync_clkcmu_pmu_bus_ipclkport_clk", "dout_pmu_clk_pmu_busp", GAT_PMU_RSTNSYNC_CLKCMU_PMU_BUS_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_rstnsync_clkcmu_pmu_cssys_ipclkport_clk", "dout_pmu_clk_dbg_atclk", GAT_PMU_RSTNSYNC_CLKCMU_PMU_CSSYS_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_rstnsync_clkcmu_pmu_oscclk_ipclkport_clk", "fin_pll", GAT_PMU_RSTNSYNC_CLKCMU_PMU_OSCCLK_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_securejtag_ipclkport_i_clk", "dout_pmu_clk_pmu_busp", GAT_PMU_SECUREJTAG_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_sfrif_central_pmu_cssys_pmu_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_PMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_sfrif_central_pmu_cssys_scs_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_SFRIF_CENTRAL_PMU_CSSYS_SCS_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_sfrif_central_pmu_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_SFRIF_CENTRAL_PMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_sysreg_pmu_ipclkport_pclk", "dout_pmu_clk_pmu_busp", GAT_PMU_SYSREG_PMU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "pmu_ipclkport_oscclk", "fin_pll", GAT_PMU_IPCLKPORT_OSCCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info pmu_cmu_info __initconst = {
	.mux_clks		= pmu_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(pmu_mux_clks),
	.div_clks		= pmu_div_clks,
	.nr_div_clks		= ARRAY_SIZE(pmu_div_clks),
	.gate_clks		= pmu_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(pmu_gate_clks),
	.fixed_clks		= pmu_fixed_clks,
	.nr_fixed_clks		= ARRAY_SIZE(pmu_fixed_clks),
	.nr_clk_ids		= PMU_NR_CLK,
	.clk_regs		= pmu_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(pmu_clk_regs),
};

static void __init trav_clk_pmu_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &pmu_cmu_info);
}

CLK_OF_DECLARE(trav_clk_pmu, "turbo,trav-clock-pmu", trav_clk_pmu_init);


/* Register Offset definitions for CMU_MIF0 (0x10810000) */
#define PLL_CON0_MIF0_PLL_MUX					0x100
#define DIV_MIF0_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF0_PLLDIV_PCLK					0x1808
#define GAT_MIF0_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF0_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF0_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF0_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF0_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_D	0x2024
#define GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_P	0x2028
#define GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF0_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif0_clk_regs[] __initconst = {
	PLL_CON0_MIF0_PLL_MUX,
	DIV_MIF0_BYPASSDIV4_ACLK,
	DIV_MIF0_PLLDIV_PCLK,
	GAT_MIF0_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF0_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF0_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF0_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF0_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_D,
	GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_P,
	GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF0_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif0_pll_mux_p) = { "fin_pll", "mout_core_mifl_pll_mifl_clk" };

static const struct samsung_mux_clock mif0_mux_clks[] __initconst = {
	MUX(0, "mout_mif0_pll_mux", mout_mif0_pll_mux_p, PLL_CON0_MIF0_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif0_div_clks[] __initconst = {
	DIV(0, "dout_mif0_bypassdiv4_aclk", "mout_mif0_pll_mux", DIV_MIF0_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif0_plldiv_pclk", "dout_mif0_bypassdiv4_aclk", DIV_MIF0_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif0_gate_clks[] __initconst = {
	GATE(0, "mif0_cmu_mif_ipclkport_pclk", "dout_mif0_plldiv_pclk", GAT_MIF0_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_axi2apb_mif_ipclkport_aclk", "dout_mif0_plldiv_pclk", GAT_MIF0_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_dmc_mif_ipclkport_aclk_0", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_dmc_mif_ipclkport_pclk", "dout_mif0_plldiv_pclk", GAT_MIF0_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_lpddr4phy_mif_ipclkport_apbclk", "dout_mif0_plldiv_pclk", GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif0_pll_mux", GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_lpddr4phy_mif_ipclkport_dficlk", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ns_brdg_mif_ipclkport_clk__pmif0__clk_mif0_d", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ns_brdg_mif_ipclkport_clk__pmif0__clk_mif0_p", "dout_mif0_plldiv_pclk", GAT_MIF0_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF0__CLK_MIF0_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ppc_dmc0_mif_ipclkport_clk", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ppc_dmc0_mif_ipclkport_pclk", "dout_mif0_plldiv_pclk", GAT_MIF0_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ppc_dmc1_mif_ipclkport_clk", "dout_mif0_bypassdiv4_aclk", GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_ppc_dmc1_mif_ipclkport_pclk", "dout_mif0_plldiv_pclk", GAT_MIF0_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif0_sysreg_mif_ipclkport_pclk", "dout_mif0_plldiv_pclk", GAT_MIF0_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif0_cmu_info __initconst = {
	.mux_clks		= mif0_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif0_mux_clks),
	.div_clks		= mif0_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif0_div_clks),
	.gate_clks		= mif0_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif0_gate_clks),
	.nr_clk_ids		= MIF0_NR_CLK,
	.clk_regs		= mif0_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif0_clk_regs),
};

static void __init trav_clk_mif0_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif0_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif0, "turbo,trav-clock-mif0", trav_clk_mif0_init);


/* Register Offset definitions for CMU_MIF1 (0x10910000) */
#define PLL_CON0_MIF1_PLL_MUX					0x100
#define DIV_MIF1_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF1_PLLDIV_PCLK					0x1808
#define GAT_MIF1_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF1_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF1_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF1_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF1_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_D	0x2024
#define GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_P	0x2028
#define GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF1_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif1_clk_regs[] __initconst = {
	PLL_CON0_MIF1_PLL_MUX,
	DIV_MIF1_BYPASSDIV4_ACLK,
	DIV_MIF1_PLLDIV_PCLK,
	GAT_MIF1_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF1_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF1_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF1_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF1_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_D,
	GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_P,
	GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF1_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif1_pll_mux_p) = { "fin_pll", "mout_core_mifl_pll_mifl_clk" };

static const struct samsung_mux_clock mif1_mux_clks[] __initconst = {
	MUX(0, "mout_mif1_pll_mux", mout_mif1_pll_mux_p, PLL_CON0_MIF1_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif1_div_clks[] __initconst = {
	DIV(0, "dout_mif1_bypassdiv4_aclk", "mout_mif1_pll_mux", DIV_MIF1_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif1_plldiv_pclk", "dout_mif1_bypassdiv4_aclk", DIV_MIF1_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif1_gate_clks[] __initconst = {
	GATE(0, "mif1_cmu_mif_ipclkport_pclk", "dout_mif1_plldiv_pclk", GAT_MIF1_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_axi2apb_mif_ipclkport_aclk", "dout_mif1_plldiv_pclk", GAT_MIF1_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_dmc_mif_ipclkport_aclk_0", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_dmc_mif_ipclkport_pclk", "dout_mif1_plldiv_pclk", GAT_MIF1_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_lpddr4phy_mif_ipclkport_apbclk", "dout_mif1_plldiv_pclk", GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif1_pll_mux", GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_lpddr4phy_mif_ipclkport_dficlk", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ns_brdg_mif_ipclkport_clk__pmif1__clk_mif1_d", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ns_brdg_mif_ipclkport_clk__pmif1__clk_mif1_p", "dout_mif1_plldiv_pclk", GAT_MIF1_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF1__CLK_MIF1_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ppc_dmc0_mif_ipclkport_clk", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ppc_dmc0_mif_ipclkport_pclk", "dout_mif1_plldiv_pclk", GAT_MIF1_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ppc_dmc1_mif_ipclkport_clk", "dout_mif1_bypassdiv4_aclk", GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_ppc_dmc1_mif_ipclkport_pclk", "dout_mif1_plldiv_pclk", GAT_MIF1_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif1_sysreg_mif_ipclkport_pclk", "dout_mif1_plldiv_pclk", GAT_MIF1_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif1_cmu_info __initconst = {
	.mux_clks		= mif1_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif1_mux_clks),
	.div_clks		= mif1_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif1_div_clks),
	.gate_clks		= mif1_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif1_gate_clks),
	.nr_clk_ids		= MIF1_NR_CLK,
	.clk_regs		= mif1_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif1_clk_regs),
};

static void __init trav_clk_mif1_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif1_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif1, "turbo,trav-clock-mif1", trav_clk_mif1_init);


/* Register Offset definitions for CMU_MIF2 (0x10a10000) */
#define PLL_CON0_MIF2_PLL_MUX					0x100
#define DIV_MIF2_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF2_PLLDIV_PCLK					0x1808
#define GAT_MIF2_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF2_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF2_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF2_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF2_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_D	0x2024
#define GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_P	0x2028
#define GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF2_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif2_clk_regs[] __initconst = {
	PLL_CON0_MIF2_PLL_MUX,
	DIV_MIF2_BYPASSDIV4_ACLK,
	DIV_MIF2_PLLDIV_PCLK,
	GAT_MIF2_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF2_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF2_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF2_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF2_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_D,
	GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_P,
	GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF2_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif2_pll_mux_p) = { "fin_pll", "mout_core_mifl_pll_mifl_clk" };

static const struct samsung_mux_clock mif2_mux_clks[] __initconst = {
	MUX(0, "mout_mif2_pll_mux", mout_mif2_pll_mux_p, PLL_CON0_MIF2_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif2_div_clks[] __initconst = {
	DIV(0, "dout_mif2_bypassdiv4_aclk", "mout_mif2_pll_mux", DIV_MIF2_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif2_plldiv_pclk", "dout_mif2_bypassdiv4_aclk", DIV_MIF2_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif2_gate_clks[] __initconst = {
	GATE(0, "mif2_cmu_mif_ipclkport_pclk", "dout_mif2_plldiv_pclk", GAT_MIF2_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_axi2apb_mif_ipclkport_aclk", "dout_mif2_plldiv_pclk", GAT_MIF2_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_dmc_mif_ipclkport_aclk_0", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_dmc_mif_ipclkport_pclk", "dout_mif2_plldiv_pclk", GAT_MIF2_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_lpddr4phy_mif_ipclkport_apbclk", "dout_mif2_plldiv_pclk", GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif2_pll_mux", GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_lpddr4phy_mif_ipclkport_dficlk", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ns_brdg_mif_ipclkport_clk__pmif2__clk_mif2_d", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ns_brdg_mif_ipclkport_clk__pmif2__clk_mif2_p", "dout_mif2_plldiv_pclk", GAT_MIF2_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF2__CLK_MIF2_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ppc_dmc0_mif_ipclkport_clk", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ppc_dmc0_mif_ipclkport_pclk", "dout_mif2_plldiv_pclk", GAT_MIF2_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ppc_dmc1_mif_ipclkport_clk", "dout_mif2_bypassdiv4_aclk", GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_ppc_dmc1_mif_ipclkport_pclk", "dout_mif2_plldiv_pclk", GAT_MIF2_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif2_sysreg_mif_ipclkport_pclk", "dout_mif2_plldiv_pclk", GAT_MIF2_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif2_cmu_info __initconst = {
	.mux_clks		= mif2_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif2_mux_clks),
	.div_clks		= mif2_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif2_div_clks),
	.gate_clks		= mif2_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif2_gate_clks),
	.nr_clk_ids		= MIF2_NR_CLK,
	.clk_regs		= mif2_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif2_clk_regs),
};

static void __init trav_clk_mif2_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif2_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif2, "turbo,trav-clock-mif2", trav_clk_mif2_init);


/* Register Offset definitions for CMU_MIF3 (0x10b10000) */
#define PLL_CON0_MIF3_PLL_MUX					0x100
#define DIV_MIF3_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF3_PLLDIV_PCLK					0x1808
#define GAT_MIF3_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF3_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF3_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF3_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF3_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_D	0x2024
#define GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_P	0x2028
#define GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF3_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif3_clk_regs[] __initconst = {
	PLL_CON0_MIF3_PLL_MUX,
	DIV_MIF3_BYPASSDIV4_ACLK,
	DIV_MIF3_PLLDIV_PCLK,
	GAT_MIF3_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF3_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF3_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF3_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF3_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_D,
	GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_P,
	GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF3_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif3_pll_mux_p) = { "fin_pll", "mout_core_mifl_pll_mifl_clk" };

static const struct samsung_mux_clock mif3_mux_clks[] __initconst = {
	MUX(0, "mout_mif3_pll_mux", mout_mif3_pll_mux_p, PLL_CON0_MIF3_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif3_div_clks[] __initconst = {
	DIV(0, "dout_mif3_bypassdiv4_aclk", "mout_mif3_pll_mux", DIV_MIF3_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif3_plldiv_pclk", "dout_mif3_bypassdiv4_aclk", DIV_MIF3_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif3_gate_clks[] __initconst = {
	GATE(0, "mif3_cmu_mif_ipclkport_pclk", "dout_mif3_plldiv_pclk", GAT_MIF3_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_axi2apb_mif_ipclkport_aclk", "dout_mif3_plldiv_pclk", GAT_MIF3_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_dmc_mif_ipclkport_aclk_0", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_dmc_mif_ipclkport_pclk", "dout_mif3_plldiv_pclk", GAT_MIF3_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_lpddr4phy_mif_ipclkport_apbclk", "dout_mif3_plldiv_pclk", GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif3_pll_mux", GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_lpddr4phy_mif_ipclkport_dficlk", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ns_brdg_mif_ipclkport_clk__pmif3__clk_mif3_d", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ns_brdg_mif_ipclkport_clk__pmif3__clk_mif3_p", "dout_mif3_plldiv_pclk", GAT_MIF3_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF3__CLK_MIF3_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ppc_dmc0_mif_ipclkport_clk", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ppc_dmc0_mif_ipclkport_pclk", "dout_mif3_plldiv_pclk", GAT_MIF3_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ppc_dmc1_mif_ipclkport_clk", "dout_mif3_bypassdiv4_aclk", GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_ppc_dmc1_mif_ipclkport_pclk", "dout_mif3_plldiv_pclk", GAT_MIF3_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif3_sysreg_mif_ipclkport_pclk", "dout_mif3_plldiv_pclk", GAT_MIF3_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif3_cmu_info __initconst = {
	.mux_clks		= mif3_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif3_mux_clks),
	.div_clks		= mif3_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif3_div_clks),
	.gate_clks		= mif3_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif3_gate_clks),
	.nr_clk_ids		= MIF3_NR_CLK,
	.clk_regs		= mif3_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif3_clk_regs),
};

static void __init trav_clk_mif3_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif3_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif3, "turbo,trav-clock-mif3", trav_clk_mif3_init);


/* Register Offset definitions for CMU_MIF4 (0x10c10000) */
#define PLL_CON0_MIF4_PLL_MUX					0x100
#define DIV_MIF4_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF4_PLLDIV_PCLK					0x1808
#define GAT_MIF4_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF4_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF4_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF4_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF4_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_D	0x2024
#define GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_P	0x2028
#define GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF4_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif4_clk_regs[] __initconst = {
	PLL_CON0_MIF4_PLL_MUX,
	DIV_MIF4_BYPASSDIV4_ACLK,
	DIV_MIF4_PLLDIV_PCLK,
	GAT_MIF4_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF4_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF4_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF4_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF4_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_D,
	GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_P,
	GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF4_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif4_pll_mux_p) = { "fin_pll", "mout_core_mifr_pll_mifr_clk" };

static const struct samsung_mux_clock mif4_mux_clks[] __initconst = {
	MUX(0, "mout_mif4_pll_mux", mout_mif4_pll_mux_p, PLL_CON0_MIF4_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif4_div_clks[] __initconst = {
	DIV(0, "dout_mif4_bypassdiv4_aclk", "mout_mif4_pll_mux", DIV_MIF4_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif4_plldiv_pclk", "dout_mif4_bypassdiv4_aclk", DIV_MIF4_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif4_gate_clks[] __initconst = {
	GATE(0, "mif4_cmu_mif_ipclkport_pclk", "dout_mif4_plldiv_pclk", GAT_MIF4_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_axi2apb_mif_ipclkport_aclk", "dout_mif4_plldiv_pclk", GAT_MIF4_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_dmc_mif_ipclkport_aclk_0", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_dmc_mif_ipclkport_pclk", "dout_mif4_plldiv_pclk", GAT_MIF4_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_lpddr4phy_mif_ipclkport_apbclk", "dout_mif4_plldiv_pclk", GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif4_pll_mux", GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_lpddr4phy_mif_ipclkport_dficlk", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ns_brdg_mif_ipclkport_clk__pmif4__clk_mif4_d", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ns_brdg_mif_ipclkport_clk__pmif4__clk_mif4_p", "dout_mif4_plldiv_pclk", GAT_MIF4_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF4__CLK_MIF4_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ppc_dmc0_mif_ipclkport_clk", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ppc_dmc0_mif_ipclkport_pclk", "dout_mif4_plldiv_pclk", GAT_MIF4_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ppc_dmc1_mif_ipclkport_clk", "dout_mif4_bypassdiv4_aclk", GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_ppc_dmc1_mif_ipclkport_pclk", "dout_mif4_plldiv_pclk", GAT_MIF4_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif4_sysreg_mif_ipclkport_pclk", "dout_mif4_plldiv_pclk", GAT_MIF4_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif4_cmu_info __initconst = {
	.mux_clks		= mif4_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif4_mux_clks),
	.div_clks		= mif4_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif4_div_clks),
	.gate_clks		= mif4_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif4_gate_clks),
	.nr_clk_ids		= MIF4_NR_CLK,
	.clk_regs		= mif4_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif4_clk_regs),
};

static void __init trav_clk_mif4_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif4_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif4, "turbo,trav-clock-mif4", trav_clk_mif4_init);


/* Register Offset definitions for CMU_MIF5 (0x10d10000) */
#define PLL_CON0_MIF5_PLL_MUX					0x100
#define DIV_MIF5_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF5_PLLDIV_PCLK					0x1808
#define GAT_MIF5_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF5_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF5_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF5_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF5_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_D	0x2024
#define GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_P	0x2028
#define GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF5_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif5_clk_regs[] __initconst = {
	PLL_CON0_MIF5_PLL_MUX,
	DIV_MIF5_BYPASSDIV4_ACLK,
	DIV_MIF5_PLLDIV_PCLK,
	GAT_MIF5_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF5_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF5_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF5_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF5_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_D,
	GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_P,
	GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF5_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif5_pll_mux_p) = { "fin_pll", "mout_core_mifr_pll_mifr_clk" };

static const struct samsung_mux_clock mif5_mux_clks[] __initconst = {
	MUX(0, "mout_mif5_pll_mux", mout_mif5_pll_mux_p, PLL_CON0_MIF5_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif5_div_clks[] __initconst = {
	DIV(0, "dout_mif5_bypassdiv4_aclk", "mout_mif5_pll_mux", DIV_MIF5_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif5_plldiv_pclk", "dout_mif5_bypassdiv4_aclk", DIV_MIF5_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif5_gate_clks[] __initconst = {
	GATE(0, "mif5_cmu_mif_ipclkport_pclk", "dout_mif5_plldiv_pclk", GAT_MIF5_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_axi2apb_mif_ipclkport_aclk", "dout_mif5_plldiv_pclk", GAT_MIF5_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_dmc_mif_ipclkport_aclk_0", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_dmc_mif_ipclkport_pclk", "dout_mif5_plldiv_pclk", GAT_MIF5_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_lpddr4phy_mif_ipclkport_apbclk", "dout_mif5_plldiv_pclk", GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif5_pll_mux", GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_lpddr4phy_mif_ipclkport_dficlk", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ns_brdg_mif_ipclkport_clk__pmif5__clk_mif5_d", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ns_brdg_mif_ipclkport_clk__pmif5__clk_mif5_p", "dout_mif5_plldiv_pclk", GAT_MIF5_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF5__CLK_MIF5_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ppc_dmc0_mif_ipclkport_clk", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ppc_dmc0_mif_ipclkport_pclk", "dout_mif5_plldiv_pclk", GAT_MIF5_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ppc_dmc1_mif_ipclkport_clk", "dout_mif5_bypassdiv4_aclk", GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_ppc_dmc1_mif_ipclkport_pclk", "dout_mif5_plldiv_pclk", GAT_MIF5_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif5_sysreg_mif_ipclkport_pclk", "dout_mif5_plldiv_pclk", GAT_MIF5_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif5_cmu_info __initconst = {
	.mux_clks		= mif5_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif5_mux_clks),
	.div_clks		= mif5_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif5_div_clks),
	.gate_clks		= mif5_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif5_gate_clks),
	.nr_clk_ids		= MIF5_NR_CLK,
	.clk_regs		= mif5_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif5_clk_regs),
};

static void __init trav_clk_mif5_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif5_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif5, "turbo,trav-clock-mif5", trav_clk_mif5_init);


/* Register Offset definitions for CMU_MIF6 (0x10e10000) */
#define PLL_CON0_MIF6_PLL_MUX					0x100
#define DIV_MIF6_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF6_PLLDIV_PCLK					0x1808
#define GAT_MIF6_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF6_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF6_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF6_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF6_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_D	0x2024
#define GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_P	0x2028
#define GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF6_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif6_clk_regs[] __initconst = {
	PLL_CON0_MIF6_PLL_MUX,
	DIV_MIF6_BYPASSDIV4_ACLK,
	DIV_MIF6_PLLDIV_PCLK,
	GAT_MIF6_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF6_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF6_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF6_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF6_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_D,
	GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_P,
	GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF6_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif6_pll_mux_p) = { "fin_pll", "mout_core_mifr_pll_mifr_clk" };

static const struct samsung_mux_clock mif6_mux_clks[] __initconst = {
	MUX(0, "mout_mif6_pll_mux", mout_mif6_pll_mux_p, PLL_CON0_MIF6_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif6_div_clks[] __initconst = {
	DIV(0, "dout_mif6_bypassdiv4_aclk", "mout_mif6_pll_mux", DIV_MIF6_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif6_plldiv_pclk", "dout_mif6_bypassdiv4_aclk", DIV_MIF6_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif6_gate_clks[] __initconst = {
	GATE(0, "mif6_cmu_mif_ipclkport_pclk", "dout_mif6_plldiv_pclk", GAT_MIF6_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_axi2apb_mif_ipclkport_aclk", "dout_mif6_plldiv_pclk", GAT_MIF6_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_dmc_mif_ipclkport_aclk_0", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_dmc_mif_ipclkport_pclk", "dout_mif6_plldiv_pclk", GAT_MIF6_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_lpddr4phy_mif_ipclkport_apbclk", "dout_mif6_plldiv_pclk", GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif6_pll_mux", GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_lpddr4phy_mif_ipclkport_dficlk", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ns_brdg_mif_ipclkport_clk__pmif6__clk_mif6_d", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ns_brdg_mif_ipclkport_clk__pmif6__clk_mif6_p", "dout_mif6_plldiv_pclk", GAT_MIF6_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF6__CLK_MIF6_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ppc_dmc0_mif_ipclkport_clk", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ppc_dmc0_mif_ipclkport_pclk", "dout_mif6_plldiv_pclk", GAT_MIF6_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ppc_dmc1_mif_ipclkport_clk", "dout_mif6_bypassdiv4_aclk", GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_ppc_dmc1_mif_ipclkport_pclk", "dout_mif6_plldiv_pclk", GAT_MIF6_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif6_sysreg_mif_ipclkport_pclk", "dout_mif6_plldiv_pclk", GAT_MIF6_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif6_cmu_info __initconst = {
	.mux_clks		= mif6_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif6_mux_clks),
	.div_clks		= mif6_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif6_div_clks),
	.gate_clks		= mif6_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif6_gate_clks),
	.nr_clk_ids		= MIF6_NR_CLK,
	.clk_regs		= mif6_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif6_clk_regs),
};

static void __init trav_clk_mif6_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif6_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif6, "turbo,trav-clock-mif6", trav_clk_mif6_init);


/* Register Offset definitions for CMU_MIF7 (0x10f10000) */
#define PLL_CON0_MIF7_PLL_MUX					0x100
#define DIV_MIF7_BYPASSDIV4_ACLK				0x1800
#define DIV_MIF7_PLLDIV_PCLK					0x1808
#define GAT_MIF7_CMU_MIF_IPCLKPORT_PCLK				0x2000
#define GAT_MIF7_AXI2APB_MIF_IPCLKPORT_ACLK			0x2004
#define GAT_MIF7_DMC_MIF_IPCLKPORT_ACLK_0			0x2008
#define GAT_MIF7_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK		0x200c
#define GAT_MIF7_DMC_MIF_IPCLKPORT_PCLK				0x2010
#define GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_APBCLK			0x2014
#define GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK		0x2018
#define GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICLK			0x201c
#define GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK		0x2020
#define GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_D	0x2024
#define GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_P	0x2028
#define GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_CLK			0x202c
#define GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_PCLK			0x2030
#define GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_CLK			0x2034
#define GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_PCLK			0x2038
#define GAT_MIF7_SYSREG_MIF_IPCLKPORT_PCLK			0x203c

static const unsigned long mif7_clk_regs[] __initconst = {
	PLL_CON0_MIF7_PLL_MUX,
	DIV_MIF7_BYPASSDIV4_ACLK,
	DIV_MIF7_PLLDIV_PCLK,
	GAT_MIF7_CMU_MIF_IPCLKPORT_PCLK,
	GAT_MIF7_AXI2APB_MIF_IPCLKPORT_ACLK,
	GAT_MIF7_DMC_MIF_IPCLKPORT_ACLK_0,
	GAT_MIF7_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK,
	GAT_MIF7_DMC_MIF_IPCLKPORT_PCLK,
	GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_APBCLK,
	GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK,
	GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICLK,
	GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK,
	GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_D,
	GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_P,
	GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_CLK,
	GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_PCLK,
	GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_CLK,
	GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_PCLK,
	GAT_MIF7_SYSREG_MIF_IPCLKPORT_PCLK,
};

PNAME(mout_mif7_pll_mux_p) = { "fin_pll", "mout_core_mifr_pll_mifr_clk" };

static const struct samsung_mux_clock mif7_mux_clks[] __initconst = {
	MUX(0, "mout_mif7_pll_mux", mout_mif7_pll_mux_p, PLL_CON0_MIF7_PLL_MUX, 4, 1),
};

static const struct samsung_div_clock mif7_div_clks[] __initconst = {
	DIV(0, "dout_mif7_bypassdiv4_aclk", "mout_mif7_pll_mux", DIV_MIF7_BYPASSDIV4_ACLK, 0, 2),
	DIV(0, "dout_mif7_plldiv_pclk", "dout_mif7_bypassdiv4_aclk", DIV_MIF7_PLLDIV_PCLK, 0, 4),
};

static const struct samsung_gate_clock mif7_gate_clks[] __initconst = {
	GATE(0, "mif7_cmu_mif_ipclkport_pclk", "dout_mif7_plldiv_pclk", GAT_MIF7_CMU_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_axi2apb_mif_ipclkport_aclk", "dout_mif7_plldiv_pclk", GAT_MIF7_AXI2APB_MIF_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_dmc_mif_ipclkport_aclk_0", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_DMC_MIF_IPCLKPORT_ACLK_0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_dmc_mif_ipclkport_core_ddrc_core_clk", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_DMC_MIF_IPCLKPORT_CORE_DDRC_CORE_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_dmc_mif_ipclkport_pclk", "dout_mif7_plldiv_pclk", GAT_MIF7_DMC_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_lpddr4phy_mif_ipclkport_apbclk", "dout_mif7_plldiv_pclk", GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_APBCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_lpddr4phy_mif_ipclkport_bypasspclk", "mout_mif7_pll_mux", GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_BYPASSPCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_lpddr4phy_mif_ipclkport_dficlk", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_lpddr4phy_mif_ipclkport_dfictlk", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_LPDDR4PHY_MIF_IPCLKPORT_DFICTLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ns_brdg_mif_ipclkport_clk__pmif7__clk_mif7_d", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ns_brdg_mif_ipclkport_clk__pmif7__clk_mif7_p", "dout_mif7_plldiv_pclk", GAT_MIF7_NS_BRDG_MIF_IPCLKPORT_CLK__PMIF7__CLK_MIF7_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ppc_dmc0_mif_ipclkport_clk", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ppc_dmc0_mif_ipclkport_pclk", "dout_mif7_plldiv_pclk", GAT_MIF7_PPC_DMC0_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ppc_dmc1_mif_ipclkport_clk", "dout_mif7_bypassdiv4_aclk", GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_ppc_dmc1_mif_ipclkport_pclk", "dout_mif7_plldiv_pclk", GAT_MIF7_PPC_DMC1_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mif7_sysreg_mif_ipclkport_pclk", "dout_mif7_plldiv_pclk", GAT_MIF7_SYSREG_MIF_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info mif7_cmu_info __initconst = {
	.mux_clks		= mif7_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mif7_mux_clks),
	.div_clks		= mif7_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mif7_div_clks),
	.gate_clks		= mif7_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mif7_gate_clks),
	.nr_clk_ids		= MIF7_NR_CLK,
	.clk_regs		= mif7_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mif7_clk_regs),
};

static void __init trav_clk_mif7_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mif7_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mif7, "turbo,trav-clock-mif7", trav_clk_mif7_init);


/* Register Offset definitions for CMU_MFC (0x12810000) */
#define PLL_LOCKTIME_PLL_MFC					0x0
#define PLL_CON0_PLL_MFC					0x100
#define MUX_MFC_BUSD						0x1000
#define MUX_MFC_BUSP						0x1008
#define DIV_MFC_BUSD_DIV4					0x1800
#define GAT_MFC_CMU_MFC_IPCLKPORT_PCLK				0x2000
#define GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKM			0x2004
#define GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKS			0x2008
#define GAT_MFC_AXI2APB_MFC_IPCLKPORT_ACLK			0x200c
#define GAT_MFC_MFC_IPCLKPORT_ACLK				0x2010
#define GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_D	0x2018
#define GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_P	0x201c
#define GAT_MFC_PPMU_MFCD0_IPCLKPORT_ACLK			0x2028
#define GAT_MFC_PPMU_MFCD0_IPCLKPORT_PCLK			0x202c
#define GAT_MFC_PPMU_MFCD1_IPCLKPORT_ACLK			0x2030
#define GAT_MFC_PPMU_MFCD1_IPCLKPORT_PCLK			0x2034
#define GAT_MFC_SYSREG_MFC_IPCLKPORT_PCLK			0x2038
#define GAT_MFC_TBU_MFCD0_IPCLKPORT_CLK				0x203c
#define GAT_MFC_TBU_MFCD1_IPCLKPORT_CLK				0x2040
#define GAT_MFC_BUSD_DIV4_GATE					0x2044
#define GAT_MFC_BUSD_GATE					0x2048

static const unsigned long mfc_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_MFC,
	PLL_CON0_PLL_MFC,
	MUX_MFC_BUSD,
	MUX_MFC_BUSP,
	DIV_MFC_BUSD_DIV4,
	GAT_MFC_CMU_MFC_IPCLKPORT_PCLK,
	GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKM,
	GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKS,
	GAT_MFC_AXI2APB_MFC_IPCLKPORT_ACLK,
	GAT_MFC_MFC_IPCLKPORT_ACLK,
	GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_D,
	GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_P,
	GAT_MFC_PPMU_MFCD0_IPCLKPORT_ACLK,
	GAT_MFC_PPMU_MFCD0_IPCLKPORT_PCLK,
	GAT_MFC_PPMU_MFCD1_IPCLKPORT_ACLK,
	GAT_MFC_PPMU_MFCD1_IPCLKPORT_PCLK,
	GAT_MFC_SYSREG_MFC_IPCLKPORT_PCLK,
	GAT_MFC_TBU_MFCD0_IPCLKPORT_CLK,
	GAT_MFC_TBU_MFCD1_IPCLKPORT_CLK,
	GAT_MFC_BUSD_DIV4_GATE,
	GAT_MFC_BUSD_GATE,
};

static const struct samsung_pll_rate_table pll_mfc_rate_table[] __initconst = {
	PLL_35XX_RATE(666000000, 111, 4, 0),
};

static const struct samsung_pll_clock mfc_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_mfc", "fin_pll", PLL_LOCKTIME_PLL_MFC, PLL_CON0_PLL_MFC, pll_mfc_rate_table),
};

PNAME(mout_mfc_pll_p) = { "fin_pll", "fout_pll_mfc" };
PNAME(mout_mfc_busp_p) = { "fin_pll", "dout_mfc_busd_div4" };
PNAME(mout_mfc_busd_p) = { "fin_pll", "mfc_busd_gate" };

static const struct samsung_mux_clock mfc_mux_clks[] __initconst = {
	MUX(0, "mout_mfc_pll", mout_mfc_pll_p, PLL_CON0_PLL_MFC, 4, 1),
	MUX(0, "mout_mfc_busp", mout_mfc_busp_p, MUX_MFC_BUSP, 0, 1),
	MUX(0, "mout_mfc_busd", mout_mfc_busd_p, MUX_MFC_BUSD, 0, 1),
};

static const struct samsung_div_clock mfc_div_clks[] __initconst = {
	DIV(0, "dout_mfc_busd_div4", "mfc_busd_div4_gate", DIV_MFC_BUSD_DIV4, 0, 4),
};

static const struct samsung_gate_clock mfc_gate_clks[] __initconst = {
	GATE(0, "mfc_cmu_mfc_ipclkport_pclk", "mout_mfc_busp", GAT_MFC_CMU_MFC_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_as_p_mfc_ipclkport_pclkm", "mout_mfc_busd", GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_as_p_mfc_ipclkport_pclks", "mout_mfc_busp", GAT_MFC_AS_P_MFC_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_axi2apb_mfc_ipclkport_aclk", "mout_mfc_busp", GAT_MFC_AXI2APB_MFC_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(MFC_MFC_IPCLKPORT_ACLK, "mfc_mfc_ipclkport_aclk", "mout_mfc_busd", GAT_MFC_MFC_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ns_brdg_mfc_ipclkport_clk__pmfc__clk_mfc_d", "mout_mfc_busd", GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ns_brdg_mfc_ipclkport_clk__pmfc__clk_mfc_p", "mout_mfc_busp", GAT_MFC_NS_BRDG_MFC_IPCLKPORT_CLK__PMFC__CLK_MFC_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ppmu_mfcd0_ipclkport_aclk", "mout_mfc_busd", GAT_MFC_PPMU_MFCD0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ppmu_mfcd0_ipclkport_pclk", "mout_mfc_busp", GAT_MFC_PPMU_MFCD0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ppmu_mfcd1_ipclkport_aclk", "mout_mfc_busd", GAT_MFC_PPMU_MFCD1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_ppmu_mfcd1_ipclkport_pclk", "mout_mfc_busp", GAT_MFC_PPMU_MFCD1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_sysreg_mfc_ipclkport_pclk", "mout_mfc_busp", GAT_MFC_SYSREG_MFC_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_tbu_mfcd0_ipclkport_clk", "mout_mfc_busd", GAT_MFC_TBU_MFCD0_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_tbu_mfcd1_ipclkport_clk", "mout_mfc_busd", GAT_MFC_TBU_MFCD1_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_busd_div4_gate", "mout_mfc_pll", GAT_MFC_BUSD_DIV4_GATE, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "mfc_busd_gate", "mout_mfc_pll", GAT_MFC_BUSD_GATE, 21, CLK_IGNORE_UNUSED | CLK_IS_CRITICAL, 0),
};

static const struct samsung_cmu_info mfc_cmu_info __initconst = {
	.pll_clks		= mfc_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(mfc_pll_clks),
	.mux_clks		= mfc_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(mfc_mux_clks),
	.div_clks		= mfc_div_clks,
	.nr_div_clks		= ARRAY_SIZE(mfc_div_clks),
	.gate_clks		= mfc_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(mfc_gate_clks),
	.nr_clk_ids		= MFC_NR_CLK,
	.clk_regs		= mfc_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(mfc_clk_regs),
};

static void __init trav_clk_mfc_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &mfc_cmu_info);
}

CLK_OF_DECLARE(trav_clk_mfc, "turbo,trav-clock-mfc", trav_clk_mfc_init);


/* Register Offset definitions for CMU_ISP (0x12010000) */
#define PLL_LOCKTIME_PLL_ISP				0x0
#define PLL_CON0_ISP_TCUCLK_MUX				0x100
#define PLL_CON0_PLL_ISP				0x120
#define DIV_ISP_ACLK					0x1800
#define DIV_ISP_PCLK					0x1804
#define GAT_ISP_CMU_ISP_IPCLKPORT_PCLK			0x2000
#define GAT_ISP_AXI2APB_ISP_IPCLKPORT_ACLK		0x2004
#define GAT_ISP_BUS_P_ISP_IPCLKPORT_MAINCLK		0x2008
#define GAT_ISP_BUS_P_ISP_IPCLKPORT_TCUCLK		0x200c
#define GAT_ISP_IPCLKPORT_ACLK				0x2010
#define GAT_ISP_IPCLKPORT_PCLK				0x2014
#define GAT_ISP_IPCLKPORT_VCLK				0x2018
#define GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_D		0x201c
#define GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_P		0x2020
#define GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_TCU	0x2024
#define GAT_ISP_SYSREG_ISP_IPCLKPORT_PCLK		0x2028
#define GAT_ISP_TBU0_ISP_IPCLKPORT_ACLK			0x202c
#define GAT_ISP_TBU1_ISP_IPCLKPORT_ACLK			0x2030
#define GAT_ISP_TCU_ISP_IPCLKPORT_ACLK			0x2034

static const unsigned long isp_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_ISP,
	PLL_CON0_ISP_TCUCLK_MUX,
	PLL_CON0_PLL_ISP,
	DIV_ISP_ACLK,
	DIV_ISP_PCLK,
	GAT_ISP_CMU_ISP_IPCLKPORT_PCLK,
	GAT_ISP_AXI2APB_ISP_IPCLKPORT_ACLK,
	GAT_ISP_BUS_P_ISP_IPCLKPORT_MAINCLK,
	GAT_ISP_BUS_P_ISP_IPCLKPORT_TCUCLK,
	GAT_ISP_IPCLKPORT_ACLK,
	GAT_ISP_IPCLKPORT_PCLK,
	GAT_ISP_IPCLKPORT_VCLK,
	GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_D,
	GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_P,
	GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_TCU,
	GAT_ISP_SYSREG_ISP_IPCLKPORT_PCLK,
	GAT_ISP_TBU0_ISP_IPCLKPORT_ACLK,
	GAT_ISP_TBU1_ISP_IPCLKPORT_ACLK,
	GAT_ISP_TCU_ISP_IPCLKPORT_ACLK,
};

static const struct samsung_pll_rate_table pll_isp_rate_table[] __initconst = {
	PLL_35XX_RATE(1200000000, 150, 3, 0),
};

static const struct samsung_pll_clock isp_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_isp", "fin_pll", PLL_LOCKTIME_PLL_ISP, PLL_CON0_PLL_ISP, pll_isp_rate_table),
};

PNAME(mout_isp_pll_p) = { "fin_pll", "fout_pll_isp" };
PNAME(mout_isp_tcuclk_mux_p) = { "fin_pll", "dout_cmu_isp_tcuclk" };

static const struct samsung_mux_clock isp_mux_clks[] __initconst = {
	MUX(0, "mout_isp_pll", mout_isp_pll_p, PLL_CON0_PLL_ISP, 4, 1),
	MUX(0, "mout_isp_tcuclk_mux", mout_isp_tcuclk_mux_p, PLL_CON0_ISP_TCUCLK_MUX, 4, 1),
};

static const struct samsung_div_clock isp_div_clks[] __initconst = {
	DIV(0, "dout_isp_aclk", "mout_isp_pll", DIV_ISP_ACLK, 0, 4),
	DIV(0, "dout_isp_pclk", "mout_isp_pll", DIV_ISP_PCLK, 0, 4),
};

static const struct samsung_gate_clock isp_gate_clks[] __initconst = {
	GATE(0, "isp_cmu_isp_ipclkport_pclk", "dout_isp_pclk", GAT_ISP_CMU_ISP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_axi2apb_isp_ipclkport_aclk", "dout_isp_pclk", GAT_ISP_AXI2APB_ISP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_bus_p_isp_ipclkport_mainclk", "dout_isp_pclk", GAT_ISP_BUS_P_ISP_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_bus_p_isp_ipclkport_tcuclk", "mout_isp_tcuclk_mux", GAT_ISP_BUS_P_ISP_IPCLKPORT_TCUCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ipclkport_aclk", "dout_isp_aclk", GAT_ISP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ipclkport_pclk", "dout_isp_pclk", GAT_ISP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ipclkport_vclk", "mout_isp_pll", GAT_ISP_IPCLKPORT_VCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ns_brdg_isp_ipclkport_clk__psoc_isp__clk_isp_d", "dout_isp_aclk", GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ns_brdg_isp_ipclkport_clk__psoc_isp__clk_isp_p", "dout_isp_pclk", GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_ns_brdg_isp_ipclkport_clk__psoc_isp__clk_isp_tcu", "mout_isp_tcuclk_mux", GAT_ISP_NS_BRDG_ISP_IPCLKPORT_CLK__PSOC_ISP__CLK_ISP_TCU, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_sysreg_isp_ipclkport_pclk", "dout_isp_pclk", GAT_ISP_SYSREG_ISP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_tbu0_isp_ipclkport_aclk", "dout_isp_aclk", GAT_ISP_TBU0_ISP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_tbu1_isp_ipclkport_aclk", "dout_isp_aclk", GAT_ISP_TBU1_ISP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "isp_tcu_isp_ipclkport_aclk", "mout_isp_tcuclk_mux", GAT_ISP_TCU_ISP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info isp_cmu_info __initconst = {
	.pll_clks		= isp_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(isp_pll_clks),
	.mux_clks		= isp_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(isp_mux_clks),
	.div_clks		= isp_div_clks,
	.nr_div_clks		= ARRAY_SIZE(isp_div_clks),
	.gate_clks		= isp_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(isp_gate_clks),
	.nr_clk_ids		= ISP_NR_CLK,
	.clk_regs		= isp_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(isp_clk_regs),
};

static void __init trav_clk_isp_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &isp_cmu_info);
}

CLK_OF_DECLARE(trav_clk_isp, "turbo,trav-clock-isp", trav_clk_isp_init);


/* Register Offset definitions for CMU_GPU (0x14410000) */
#define PLL_LOCKTIME_PLL_GPU					0x0
#define PLL_CON0_GPU_CLK_SWITCH_MUX				0x120
#define PLL_CON0_PLL_GPU					0x140
#define MUX_CLK_GPU_BUSD					0x1000
#define DIV_CLK_DROOP_DETECTOR_GPU				0x1800
#define DIV_CLK_GPU_BUSACLK					0x1804
#define DIV_CLK_GPU_BUSP					0x1808
#define DIV_CLK_GPU_DD_BUSP0					0x180c
#define DIV_CLK_GPU_DD_BUSP1					0x1810
#define DIV_CLK_GPU_HPM						0x1814
#define DIV_CLK_GPU						0x1818
#define GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_CLK			0x2004
#define GAT_GPU_DD_GPU0_IPCLKPORT_CK_IN				0x2008
#define GAT_GPU_DD_GPU1_IPCLKPORT_CK_IN				0x200c
#define GAT_GPU_DD_GPU2_IPCLKPORT_CK_IN				0x2010
#define GAT_GPU_DD_GPU3_IPCLKPORT_CK_IN				0x2014
#define GAT_GPU_GPU_CMU_GPU_IPCLKPORT_PCLK			0x2018
#define GAT_GPU_GPU_IPCLKPORT_CLK				0x201c
#define GAT_GPU_HPM0_GPU_IPCLKPORT_HPM_TARGETCLK_C		0x2020
#define GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKM			0x2024
#define GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKS			0x2028
#define GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKM		0x202c
#define GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKS		0x2030
#define GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKM		0x2034
#define GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKS		0x2038
#define GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKM		0x203c
#define GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKS		0x2040
#define GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKM		0x2044
#define GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKS		0x2048
#define GAT_GPU_AXI2APB_GPU_IPCLKPORT_ACLK			0x204c
#define GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_PCLK			0x2050
#define GAT_GPU_BUS_P_GPU_IPCLKPORT_MAINCLK			0x2054
#define GAT_GPU_BUS_P_GPU_IPCLKPORT_PERICLK			0x2058
#define GAT_GPU_DD_GPU0_IPCLKPORT_PCLK				0x205c
#define GAT_GPU_DD_GPU1_IPCLKPORT_PCLK				0x2060
#define GAT_GPU_DD_GPU2_IPCLKPORT_PCLK				0x2064
#define GAT_GPU_DD_GPU3_IPCLKPORT_PCLK				0x2068
#define GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_D	0x206c
#define GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_P	0x2070
#define GAT_GPU_SYSREG_GPU_IPCLKPORT_PCLK			0x2074

static const unsigned long gpu_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_GPU,
	PLL_CON0_GPU_CLK_SWITCH_MUX,
	PLL_CON0_PLL_GPU,
	MUX_CLK_GPU_BUSD,
	DIV_CLK_DROOP_DETECTOR_GPU,
	DIV_CLK_GPU_BUSACLK,
	DIV_CLK_GPU_BUSP,
	DIV_CLK_GPU_DD_BUSP0,
	DIV_CLK_GPU_DD_BUSP1,
	DIV_CLK_GPU_HPM,
	DIV_CLK_GPU,
	GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_CLK,
	GAT_GPU_DD_GPU0_IPCLKPORT_CK_IN,
	GAT_GPU_DD_GPU1_IPCLKPORT_CK_IN,
	GAT_GPU_DD_GPU2_IPCLKPORT_CK_IN,
	GAT_GPU_DD_GPU3_IPCLKPORT_CK_IN,
	GAT_GPU_GPU_CMU_GPU_IPCLKPORT_PCLK,
	GAT_GPU_GPU_IPCLKPORT_CLK,
	GAT_GPU_HPM0_GPU_IPCLKPORT_HPM_TARGETCLK_C,
	GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKM,
	GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKS,
	GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKM,
	GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKS,
	GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKM,
	GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKS,
	GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKM,
	GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKS,
	GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKM,
	GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKS,
	GAT_GPU_AXI2APB_GPU_IPCLKPORT_ACLK,
	GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_PCLK,
	GAT_GPU_BUS_P_GPU_IPCLKPORT_MAINCLK,
	GAT_GPU_BUS_P_GPU_IPCLKPORT_PERICLK,
	GAT_GPU_DD_GPU0_IPCLKPORT_PCLK,
	GAT_GPU_DD_GPU1_IPCLKPORT_PCLK,
	GAT_GPU_DD_GPU2_IPCLKPORT_PCLK,
	GAT_GPU_DD_GPU3_IPCLKPORT_PCLK,
	GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_D,
	GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_P,
	GAT_GPU_SYSREG_GPU_IPCLKPORT_PCLK,
};

static const struct samsung_pll_rate_table pll_gpu_rate_table[] __initconst = {
	PLL_35XX_RATE(700000000, 175, 3, 1),
	PLL_35XX_RATE(900000000, 225, 3, 1),
	PLL_35XX_RATE(1000000000, 250, 3, 1),
};

static const struct samsung_pll_clock gpu_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_gpu", "fin_pll", PLL_LOCKTIME_PLL_GPU, PLL_CON0_PLL_GPU, pll_gpu_rate_table),
};

PNAME(mout_gpu_pll_p) = { "fin_pll", "fout_pll_gpu" };
PNAME(mout_gpu_clk_gpu_busd_p) = { "mout_gpu_pll", "mout_gpu_clk_switch_mux" };
PNAME(mout_gpu_clk_switch_mux_p) = { "fin_pll", "dout_cmu_gpu_main_switch" };

static const struct samsung_mux_clock gpu_mux_clks[] __initconst = {
	MUX(MOUT_GPU_CLK_PLL, "mout_gpu_pll", mout_gpu_pll_p, PLL_CON0_PLL_GPU, 4, 1),
	MUX(MOUT_GPU_CLK_GPU_BUSD, "mout_gpu_clk_gpu_busd", mout_gpu_clk_gpu_busd_p, MUX_CLK_GPU_BUSD, 0, 1),
	MUX(MOUT_GPU_CLK_SWITCH_USER, "mout_gpu_clk_switch_mux", mout_gpu_clk_switch_mux_p, PLL_CON0_GPU_CLK_SWITCH_MUX, 4, 1),
};

static const struct samsung_div_clock gpu_div_clks[] __initconst = {
	DIV(0, "dout_gpu_clk_droop_detector_gpu", "mout_gpu_clk_gpu_busd", DIV_CLK_DROOP_DETECTOR_GPU, 0, 4),
	DIV(0, "dout_gpu_clk_gpu_busaclk", "mout_gpu_clk_gpu_busd", DIV_CLK_GPU_BUSACLK, 0, 4),
	DIV(0, "dout_gpu_clk_gpu_busp", "mout_gpu_clk_gpu_busd", DIV_CLK_GPU_BUSP, 0, 4),
	DIV(0, "dout_gpu_clk_gpu_dd_busp0", "mout_gpu_clk_gpu_busd", DIV_CLK_GPU_DD_BUSP0, 0, 4),
	DIV(0, "dout_gpu_clk_gpu_dd_busp1", "mout_gpu_clk_gpu_busd", DIV_CLK_GPU_DD_BUSP1, 0, 4),
	DIV(0, "dout_gpu_clk_gpu_hpm", "mout_gpu_clk_gpu_busd", DIV_CLK_GPU_HPM, 0, 4),
};

static const struct samsung_gate_clock gpu_gate_clks[] __initconst = {
	GATE(0, "gpu_dd_gpu0_ipclkport_ck_in", "dout_gpu_clk_droop_detector_gpu", GAT_GPU_DD_GPU0_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu1_ipclkport_ck_in", "dout_gpu_clk_droop_detector_gpu", GAT_GPU_DD_GPU1_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu2_ipclkport_ck_in", "dout_gpu_clk_droop_detector_gpu", GAT_GPU_DD_GPU2_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu3_ipclkport_ck_in", "dout_gpu_clk_droop_detector_gpu", GAT_GPU_DD_GPU3_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_busif_hpmgpu_ipclkport_clk", "fin_pll", GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_gpu_cmu_gpu_ipclkport_pclk", "dout_gpu_clk_gpu_busp", GAT_GPU_GPU_CMU_GPU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(CLK_GPU_GPU_IPCLKPORT_CLK, "gpu_gpu_ipclkport_clk", "dout_gpu_clk_gpu", GAT_GPU_GPU_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_hpm0_gpu_ipclkport_hpm_targetclk_c", "dout_gpu_clk_gpu_hpm", GAT_GPU_HPM0_GPU_IPCLKPORT_HPM_TARGETCLK_C, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_ad_axi_gpu_ipclkport_aclkm", "dout_gpu_clk_gpu", GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_ad_axi_gpu_ipclkport_aclks", "dout_gpu_clk_gpu_busaclk", GAT_GPU_AD_AXI_GPU_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu0_ipclkport_pclkm", "dout_gpu_clk_gpu_dd_busp0", GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu0_ipclkport_pclks", "dout_gpu_clk_gpu_busp", GAT_GPU_APB_ASYNC_TOP_GPU0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu1_ipclkport_pclkm", "dout_gpu_clk_gpu_dd_busp0", GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu1_ipclkport_pclks", "dout_gpu_clk_gpu_busp", GAT_GPU_APB_ASYNC_TOP_GPU1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu2_ipclkport_pclkm", "dout_gpu_clk_gpu_dd_busp1", GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu2_ipclkport_pclks", "dout_gpu_clk_gpu_busp", GAT_GPU_APB_ASYNC_TOP_GPU2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu3_ipclkport_pclkm", "dout_gpu_clk_gpu_dd_busp1", GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_apb_async_top_gpu3_ipclkport_pclks", "dout_gpu_clk_gpu_busp", GAT_GPU_APB_ASYNC_TOP_GPU3_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_axi2apb_gpu_ipclkport_aclk", "dout_gpu_clk_gpu_busp", GAT_GPU_AXI2APB_GPU_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_busif_hpmgpu_ipclkport_pclk", "dout_gpu_clk_gpu_busp", GAT_GPU_BUSIF_HPMGPU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_bus_p_gpu_ipclkport_mainclk", "dout_gpu_clk_gpu_busaclk", GAT_GPU_BUS_P_GPU_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_bus_p_gpu_ipclkport_periclk", "dout_gpu_clk_gpu_busp", GAT_GPU_BUS_P_GPU_IPCLKPORT_PERICLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu0_ipclkport_pclk", "dout_gpu_clk_gpu_dd_busp0", GAT_GPU_DD_GPU0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu1_ipclkport_pclk", "dout_gpu_clk_gpu_dd_busp0", GAT_GPU_DD_GPU1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu2_ipclkport_pclk", "dout_gpu_clk_gpu_dd_busp1", GAT_GPU_DD_GPU2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_dd_gpu3_ipclkport_pclk", "dout_gpu_clk_gpu_dd_busp1", GAT_GPU_DD_GPU3_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_ns_brdg_gpu_ipclkport_clk__pgpu__clk_gpu_d", "dout_gpu_clk_gpu", GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_ns_brdg_gpu_ipclkport_clk__pgpu__clk_gpu_p", "dout_gpu_clk_gpu_busaclk", GAT_GPU_NS_BRDG_GPU_IPCLKPORT_CLK__PGPU__CLK_GPU_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "gpu_sysreg_gpu_ipclkport_pclk", "dout_gpu_clk_gpu_busp", GAT_GPU_SYSREG_GPU_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_fixed_factor_clock gpu_fixed_factor_clks[] __initconst = {
	FFACTOR(0, "dout_gpu_clk_gpu", "mout_gpu_clk_gpu_busd", 1, 1, 0),
};

static const struct samsung_cmu_info gpu_cmu_info __initconst = {
	.pll_clks		= gpu_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(gpu_pll_clks),
	.mux_clks		= gpu_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(gpu_mux_clks),
	.div_clks		= gpu_div_clks,
	.nr_div_clks		= ARRAY_SIZE(gpu_div_clks),
	.gate_clks		= gpu_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(gpu_gate_clks),
	.fixed_factor_clks	= gpu_fixed_factor_clks,
	.nr_fixed_factor_clks	= ARRAY_SIZE(gpu_fixed_factor_clks),
	.nr_clk_ids		= GPU_NR_CLK,
	.clk_regs		= gpu_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(gpu_clk_regs),
};

static void __init trav_clk_gpu_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &gpu_cmu_info);
}

CLK_OF_DECLARE(trav_clk_gpu, "turbo,trav-clock-gpu", trav_clk_gpu_init);


/* Register Offset definitions for CMU_CAM_CSI (0x12610000) */
#define PLL_LOCKTIME_PLL_CAM_CSI		0x0
#define PLL_CON0_PLL_CAM_CSI			0x100
#define DIV_CAM_CSI0_ACLK			0x1800
#define DIV_CAM_CSI1_ACLK			0x1804
#define DIV_CAM_CSI2_ACLK			0x1808
#define DIV_CAM_CSI_BUSD			0x180c
#define DIV_CAM_CSI_BUSP			0x1810
#define GAT_CAM_CSI_CMU_CAM_CSI_IPCLKPORT_PCLK	0x2000
#define GAT_CAM_AXI2APB_CAM_CSI_IPCLKPORT_ACLK	0x2004
#define GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI0	0x2008
#define GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI1	0x200c
#define GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI2	0x2010
#define GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_SOC_NOC	0x2014
#define GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__NOC		0x2018
#define GAT_CAM_CSI0_0_IPCLKPORT_I_ACLK		0x201c
#define GAT_CAM_CSI0_0_IPCLKPORT_I_PCLK		0x2020
#define GAT_CAM_CSI0_1_IPCLKPORT_I_ACLK		0x2024
#define GAT_CAM_CSI0_1_IPCLKPORT_I_PCLK		0x2028
#define GAT_CAM_CSI0_2_IPCLKPORT_I_ACLK		0x202c
#define GAT_CAM_CSI0_2_IPCLKPORT_I_PCLK		0x2030
#define GAT_CAM_CSI0_3_IPCLKPORT_I_ACLK		0x2034
#define GAT_CAM_CSI0_3_IPCLKPORT_I_PCLK		0x2038
#define GAT_CAM_CSI1_0_IPCLKPORT_I_ACLK		0x203c
#define GAT_CAM_CSI1_0_IPCLKPORT_I_PCLK		0x2040
#define GAT_CAM_CSI1_1_IPCLKPORT_I_ACLK		0x2044
#define GAT_CAM_CSI1_1_IPCLKPORT_I_PCLK		0x2048
#define GAT_CAM_CSI1_2_IPCLKPORT_I_ACLK		0x204c
#define GAT_CAM_CSI1_2_IPCLKPORT_I_PCLK		0x2050
#define GAT_CAM_CSI1_3_IPCLKPORT_I_ACLK		0x2054
#define GAT_CAM_CSI1_3_IPCLKPORT_I_PCLK		0x2058
#define GAT_CAM_CSI2_0_IPCLKPORT_I_ACLK		0x205c
#define GAT_CAM_CSI2_0_IPCLKPORT_I_PCLK		0x2060
#define GAT_CAM_CSI2_1_IPCLKPORT_I_ACLK		0x2064
#define GAT_CAM_CSI2_1_IPCLKPORT_I_PCLK		0x2068
#define GAT_CAM_CSI2_2_IPCLKPORT_I_ACLK		0x206c
#define GAT_CAM_CSI2_2_IPCLKPORT_I_PCLK		0x2070
#define GAT_CAM_CSI2_3_IPCLKPORT_I_ACLK		0x2074
#define GAT_CAM_CSI2_3_IPCLKPORT_I_PCLK		0x2078
#define GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_D	0x207c
#define GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_P	0x2080
#define GAT_CAM_SYSREG_CAM_CSI_IPCLKPORT_PCLK	0x2084
#define GAT_CAM_TBU_CAM_CSI_IPCLKPORT_ACLK	0x2088

static const unsigned long cam_csi_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CAM_CSI,
	PLL_CON0_PLL_CAM_CSI,
	DIV_CAM_CSI0_ACLK,
	DIV_CAM_CSI1_ACLK,
	DIV_CAM_CSI2_ACLK,
	DIV_CAM_CSI_BUSD,
	DIV_CAM_CSI_BUSP,
	GAT_CAM_CSI_CMU_CAM_CSI_IPCLKPORT_PCLK,
	GAT_CAM_AXI2APB_CAM_CSI_IPCLKPORT_ACLK,
	GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI0,
	GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI1,
	GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI2,
	GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_SOC_NOC,
	GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__NOC,
	GAT_CAM_CSI0_0_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI0_0_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI0_1_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI0_1_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI0_2_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI0_2_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI0_3_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI0_3_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI1_0_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI1_0_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI1_1_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI1_1_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI1_2_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI1_2_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI1_3_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI1_3_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI2_0_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI2_0_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI2_1_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI2_1_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI2_2_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI2_2_IPCLKPORT_I_PCLK,
	GAT_CAM_CSI2_3_IPCLKPORT_I_ACLK,
	GAT_CAM_CSI2_3_IPCLKPORT_I_PCLK,
	GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_D,
	GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_P,
	GAT_CAM_SYSREG_CAM_CSI_IPCLKPORT_PCLK,
	GAT_CAM_TBU_CAM_CSI_IPCLKPORT_ACLK,
};

static const struct samsung_pll_rate_table pll_cam_csi_rate_table[] __initconst = {
	PLL_35XX_RATE(1066000000, 533, 12, 0),
};

static const struct samsung_pll_clock cam_csi_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_cam_csi", "fin_pll", PLL_LOCKTIME_PLL_CAM_CSI, PLL_CON0_PLL_CAM_CSI, pll_cam_csi_rate_table),
};

PNAME(mout_cam_csi_pll_p) = { "fin_pll", "fout_pll_cam_csi" };

static const struct samsung_mux_clock cam_csi_mux_clks[] __initconst = {
	MUX(0, "mout_cam_csi_pll", mout_cam_csi_pll_p, PLL_CON0_PLL_CAM_CSI, 4, 1),
};

static const struct samsung_div_clock cam_csi_div_clks[] __initconst = {
	DIV(0, "dout_cam_csi0_aclk", "mout_cam_csi_pll", DIV_CAM_CSI0_ACLK, 0, 4),
	DIV(0, "dout_cam_csi1_aclk", "mout_cam_csi_pll", DIV_CAM_CSI1_ACLK, 0, 4),
	DIV(0, "dout_cam_csi2_aclk", "mout_cam_csi_pll", DIV_CAM_CSI2_ACLK, 0, 4),
	DIV(0, "dout_cam_csi_busd", "mout_cam_csi_pll", DIV_CAM_CSI_BUSD, 0, 4),
	DIV(0, "dout_cam_csi_busp", "mout_cam_csi_pll", DIV_CAM_CSI_BUSP, 0, 4),
};

static const struct samsung_gate_clock cam_csi_gate_clks[] __initconst = {
	GATE(0, "cam_csi_cmu_cam_csi_ipclkport_pclk", "dout_cam_csi_busp", GAT_CAM_CSI_CMU_CAM_CSI_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_axi2apb_cam_csi_ipclkport_aclk", "dout_cam_csi_busp", GAT_CAM_AXI2APB_CAM_CSI_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi_bus_d_cam_csi_ipclkport_clk__system__clk_csi0", "dout_cam_csi0_aclk", GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI0, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi_bus_d_cam_csi_ipclkport_clk__system__clk_csi1", "dout_cam_csi1_aclk", GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI1, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi_bus_d_cam_csi_ipclkport_clk__system__clk_csi2", "dout_cam_csi2_aclk", GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_CSI2, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi_bus_d_cam_csi_ipclkport_clk__system__clk_soc_noc", "dout_cam_csi_busd", GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__CLK_SOC_NOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi_bus_d_cam_csi_ipclkport_clk__system__noc", "dout_cam_csi_busd", GAT_CAM_CSI_BUS_D_CAM_CSI_IPCLKPORT_CLK__SYSTEM__NOC, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_0_ipclkport_i_aclk", "dout_cam_csi0_aclk", GAT_CAM_CSI0_0_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_0_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI0_0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_1_ipclkport_i_aclk", "dout_cam_csi0_aclk", GAT_CAM_CSI0_1_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_1_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI0_1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_2_ipclkport_i_aclk", "dout_cam_csi0_aclk", GAT_CAM_CSI0_2_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_2_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI0_2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_3_ipclkport_i_aclk", "dout_cam_csi0_aclk", GAT_CAM_CSI0_3_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi0_3_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI0_3_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_0_ipclkport_i_aclk", "dout_cam_csi1_aclk", GAT_CAM_CSI1_0_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_0_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI1_0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_1_ipclkport_i_aclk", "dout_cam_csi1_aclk", GAT_CAM_CSI1_1_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_1_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI1_1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_2_ipclkport_i_aclk", "dout_cam_csi1_aclk", GAT_CAM_CSI1_2_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_2_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI1_2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_3_ipclkport_i_aclk", "dout_cam_csi1_aclk", GAT_CAM_CSI1_3_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi1_3_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI1_3_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_0_ipclkport_i_aclk", "dout_cam_csi2_aclk", GAT_CAM_CSI2_0_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_0_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI2_0_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_1_ipclkport_i_aclk", "dout_cam_csi2_aclk", GAT_CAM_CSI2_1_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_1_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI2_1_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_2_ipclkport_i_aclk", "dout_cam_csi2_aclk", GAT_CAM_CSI2_2_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_2_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI2_2_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_3_ipclkport_i_aclk", "dout_cam_csi2_aclk", GAT_CAM_CSI2_3_IPCLKPORT_I_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_csi2_3_ipclkport_i_pclk", "dout_cam_csi_busp", GAT_CAM_CSI2_3_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_ns_brdg_cam_csi_ipclkport_clk__psoc_cam_csi__clk_cam_csi_d", "dout_cam_csi_busd", GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_ns_brdg_cam_csi_ipclkport_clk__psoc_cam_csi__clk_cam_csi_p", "dout_cam_csi_busp", GAT_CAM_NS_BRDG_CAM_CSI_IPCLKPORT_CLK__PSOC_CAM_CSI__CLK_CAM_CSI_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_sysreg_cam_csi_ipclkport_pclk", "dout_cam_csi_busp", GAT_CAM_SYSREG_CAM_CSI_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_tbu_cam_csi_ipclkport_aclk", "dout_cam_csi_busd", GAT_CAM_TBU_CAM_CSI_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info cam_csi_cmu_info __initconst = {
	.pll_clks		= cam_csi_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(cam_csi_pll_clks),
	.mux_clks		= cam_csi_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(cam_csi_mux_clks),
	.div_clks		= cam_csi_div_clks,
	.nr_div_clks		= ARRAY_SIZE(cam_csi_div_clks),
	.gate_clks		= cam_csi_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(cam_csi_gate_clks),
	.nr_clk_ids		= CAM_CSI_NR_CLK,
	.clk_regs		= cam_csi_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(cam_csi_clk_regs),
};

static void __init trav_clk_cam_csi_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &cam_csi_cmu_info);
}

CLK_OF_DECLARE(trav_clk_cam_csi, "turbo,trav-clock-cam_csi", trav_clk_cam_csi_init);


/*
 * Register Offset definitions for CMU_TRIP0 (0x13c10000) and
 * CMU_TRIP1 (0x13e10000)
 */
#define PLL_LOCKTIME_PLL_TRIP					0x0
#define PLL_CON0_PLL_TRIP					0x100
#define PLL_CON0_TRIP_SWITCH_MUX				0x120
#define CLK_CON_MUX_ECLK_ACLK_MUX				0x1000
#define CLK_CON_MUX_TRIP_SWITCH_PLL_MUX				0x1004
#define CLK_CON_DIV_CLK_TRIP_DROOP_DETECTOR			0x1800
#define CLK_CON_DIV_CLK_TRIP_HPM_TARGET				0x1804
#define CLK_CON_DIV_DIV_CLK_TRIP				0x1808
#define CLK_CON_DIV_TRIP_ACLK					0x180c
#define CLK_CON_DIV_TRIP_PCLK					0x1810
#define CLK_CON_DIV_TRIP_PCLK_DD				0x1814
#define CLK_CON_BUF_BOUT_OSCCLK_TRIP				0x2000
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_CLK \
								0x2004
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_CK_IN \
								0x2008
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_HPM_TRIP_IPCLKPORT_I_HPM_TARGETCLK_C \
								0x200c
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_LHS_ACEL_D_TRIP_IPCLKPORT_I_CLK \
								0x2010
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP \
								0x2014
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_CMU_TRIP_IPCLKPORT_PCLK \
								0x2018
#define CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_IPCLKPORT_ECLK	0x201c
#define CLK_CON_GAT_CLK_DD_GATE					0x2020
#define CLK_CON_GAT_CLK_HPM_TARGET_GATE				0x2024
#define CLK_CON_GAT_CLK_SHORTSTOP_TRIP				0x2028
#define CLK_CON_GAT_GATE_TRIP_PCLK_DD				0x202c
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKM \
								0x2030
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKS \
								0x2034
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_AXI2APB_TRIP_IPCLKPORT_ACLK \
								0x2038
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_PCLK \
								0x203c
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_PCLK \
								0x2040
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP_P \
								0x2044
#define CLK_CON_GAT_GOUT_BLK_TRIP_UID_SYSREG_TRIP_IPCLKPORT_PCLK \
								0x2048
#define CLK_CON_GAT_TRIP_PCLK_GATE \
								0x204c

static const unsigned long trip0_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_TRIP,
	PLL_CON0_PLL_TRIP,
	PLL_CON0_TRIP_SWITCH_MUX,
	CLK_CON_MUX_ECLK_ACLK_MUX,
	CLK_CON_MUX_TRIP_SWITCH_PLL_MUX,
	CLK_CON_DIV_CLK_TRIP_DROOP_DETECTOR,
	CLK_CON_DIV_CLK_TRIP_HPM_TARGET,
	CLK_CON_DIV_DIV_CLK_TRIP,
	CLK_CON_DIV_TRIP_ACLK,
	CLK_CON_DIV_TRIP_PCLK,
	CLK_CON_DIV_TRIP_PCLK_DD,
	CLK_CON_BUF_BOUT_OSCCLK_TRIP,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_CLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_CK_IN,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_HPM_TRIP_IPCLKPORT_I_HPM_TARGETCLK_C,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_LHS_ACEL_D_TRIP_IPCLKPORT_I_CLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_CMU_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_IPCLKPORT_ECLK,
	CLK_CON_GAT_CLK_DD_GATE,
	CLK_CON_GAT_CLK_HPM_TARGET_GATE,
	CLK_CON_GAT_CLK_SHORTSTOP_TRIP,
	CLK_CON_GAT_GATE_TRIP_PCLK_DD,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKM,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKS,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AXI2APB_TRIP_IPCLKPORT_ACLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP_P,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_SYSREG_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_TRIP_PCLK_GATE,
};

/* Clock definition for CMU_TRIP0 (0x13c10000) */
static const struct samsung_pll_rate_table pll_trip0_rate_table[] __initconst = {
	PLL_35XX_RATE(2000000000, 250, 3, 0),
	PLL_35XX_RATE(1800000000, 225, 3, 0),
	PLL_35XX_RATE(1700000000, 425, 3, 1),
	PLL_35XX_RATE(1350000000, 225, 2, 1),
	PLL_35XX_RATE(106250000, 425, 3, 5),	/* Idle for 1.7 GHz */
	PLL_35XX_RATE(84375000, 225, 2, 5),	/* Idle for 1.35 GHz */
	PLL_35XX_RATE(62500000, 250, 3, 5),	/* Idle for 2.0 GHz */
	PLL_35XX_RATE(56250000, 225, 3, 5),	/* Idle for 1.8 GHz */
};

static const struct samsung_pll_clock trip0_pll_clks[] __initconst = {
	PLL(pll_142xx, CLK_TRIP_CLK, "fout_pll_trip0", "fin_pll", PLL_LOCKTIME_PLL_TRIP, PLL_CON0_PLL_TRIP, pll_trip0_rate_table),
};

PNAME(mout_trip0_pll_p) = { "fin_pll", "fout_pll_trip0" };
PNAME(mout_trip0_switch_mux_p) = { "fin_pll", "dout_cmu_trip0_switchclk" };
PNAME(mout_trip0_switch_pll_mux_p) = { "mout_trip0_pll", "mout_trip0_switch_mux" };
PNAME(mout_trip0_eclk_aclk_mux_p) = { "trip0_eclk_in", "dout_trip0_aclk" };

/*
 * 0x100 PLL_TRIP
 * 0x120 TRIP_SWITCH_MUX
 * 0x1004 TRIP_SWITCH_PLL_MUX
 * 0x1000 ECLK_ACLK_MUX
 */
static const struct samsung_mux_clock trip0_mux_clks[] __initconst = {
	MUX(MOUT_TRIP_PLL, "mout_trip0_pll", mout_trip0_pll_p, PLL_CON0_PLL_TRIP, 4, 1), /* 0x100 */
	MUX(MOUT_TRIP_SWITCH_MUX, "mout_trip0_switch_mux", mout_trip0_switch_mux_p, PLL_CON0_TRIP_SWITCH_MUX, 4, 1), /* 0x120 */
	MUX(MOUT_TRIP_SWITCH_PLL_MUX, "mout_trip0_switch_pll_mux", mout_trip0_switch_pll_mux_p, CLK_CON_MUX_TRIP_SWITCH_PLL_MUX, 0, 1), /* 0x1004 */
	MUX(0, "mout_trip0_eclk_aclk_mux", mout_trip0_eclk_aclk_mux_p, CLK_CON_MUX_ECLK_ACLK_MUX, 0, 1),  /* 0x1000 */
};

static const struct samsung_div_clock trip0_div_clks[] __initconst = {
	DIV(0, "dout_div_trip0_clk", "mout_trip0_switch_pll_mux", CLK_CON_DIV_DIV_CLK_TRIP, 0, 4),   /* 0x1808 */
	DIV(0, "dout_trip0_aclk", "dout_div_trip0_clk", CLK_CON_DIV_TRIP_ACLK, 0, 4), /* 0x180c */
	DIV(0, "dout_trip0_pclk", "mout_trip0_switch_pll_mux", CLK_CON_DIV_TRIP_PCLK, 0, 4), /* 0x1810 */
	DIV(0, "dout_trip0_pclk_dd", "mout_trip0_switch_pll_mux", CLK_CON_DIV_TRIP_PCLK_DD, 0, 4), /* 0x1814 */
	DIV(0, "dout_clk_trip0_droop_detector", "mout_trip0_switch_pll_mux", CLK_CON_DIV_CLK_TRIP_DROOP_DETECTOR, 0, 4), /* 0x1800 */
	DIV(0, "dout_clk_trip0_hpm_target", "mout_trip0_switch_pll_mux", CLK_CON_DIV_CLK_TRIP_HPM_TARGET, 0, 4), /* 0x1804 */
};

static const struct samsung_gate_clock trip0_gate_clks[] __initconst = {
	GATE(0, "bout_oscclk_trip0", "fin_pll", CLK_CON_BUF_BOUT_OSCCLK_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2000 */
	GATE(0, "trip0_busif_hpm_clk", "fin_pll", CLK_CON_GAT_CLK_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2004 */
	GATE(0, "trip0_droop_detector_ck_in", "dout_clk_trip0_droop_detector", CLK_CON_GAT_CLK_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0), /* 0x2008 */
	GATE(0, "trip0_lhs_acel_d_trip_i_clk", "mout_trip0_eclk_aclk_mux", CLK_CON_GAT_CLK_BLK_TRIP_UID_LHS_ACEL_D_TRIP_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2010 */
	GATE(0, "trip0_ns_brdg_trip_clk_pacc0_clk_trip0", "mout_trip0_eclk_aclk_mux", CLK_CON_GAT_CLK_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2014 */
	GATE(0, "trip0_cmu_trip_pclk", "dout_trip0_pclk", CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_CMU_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2018 */
	GATE(0, "trip0_trip_eclk", "dout_div_trip0_clk", CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_IPCLKPORT_ECLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x201c */
	GATE(0, "trip0_clk_dd", "dout_clk_trip0_droop_detector", CLK_CON_GAT_CLK_DD_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x2020 */
	GATE(0, "trip0_clk_hpm_target", "dout_clk_trip0_hpm_target", CLK_CON_GAT_CLK_HPM_TARGET_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x2024 */
	GATE(0, "trip0_clk_shortstop_trip", "fin_pll", CLK_CON_GAT_CLK_SHORTSTOP_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2028 */
	GATE(0, "trip0_trip_pclk_dd", "dout_trip0_pclk_dd", CLK_CON_GAT_GATE_TRIP_PCLK_DD, 21, CLK_IGNORE_UNUSED, 0), /* 0x202c */
	GATE(0, "trip0_ad_apb_dd_trip_pcklm", "dout_trip0_pclk_dd", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0), /* 0x2030 */
	GATE(0, "trip0_ad_apb_dd_trip_pckls", "dout_trip0_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0), /* 0x2034 */
	GATE(0, "trip0_axi2apb_trip_aclk", "dout_trip0_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AXI2APB_TRIP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2038 */
	GATE(0, "trip0_busif_hpm_trip_pclk", "dout_trip0_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x203c */
	GATE(0, "trip0_droop_detector_trip_pclk", "dout_trip0_pclk_dd", CLK_CON_GAT_GOUT_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2040 */
	GATE(0, "trip0_ns_brdg_trip_clk_pacc0_clk_trip0_p", "dout_trip0_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP_P, 21, CLK_IGNORE_UNUSED, 0), /* 0x2044 */
	GATE(0, "trip0_sysreg_trip_pclk", "dout_trip0_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_SYSREG_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2048 */
	GATE(0, "trip0_trip_pclk", "dout_trip0_pclk", CLK_CON_GAT_TRIP_PCLK_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x204c */
};

static const struct samsung_cmu_info trip0_cmu_info __initconst = {
	.pll_clks		= trip0_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(trip0_pll_clks),
	.mux_clks		= trip0_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(trip0_mux_clks),
	.div_clks		= trip0_div_clks,
	.nr_div_clks		= ARRAY_SIZE(trip0_div_clks),
	.gate_clks		= trip0_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(trip0_gate_clks),
	.nr_clk_ids		= TRIP_NR_CLK,
	.clk_regs		= trip0_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(trip0_clk_regs),
};

static void __init trav_clk_trip0_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &trip0_cmu_info);
}

CLK_OF_DECLARE(trav_clk_trip0, "turbo,trav-clock-trip0", trav_clk_trip0_init);

/* Clock definition for CMU_TRIP1 (0x13e10000) */
static const unsigned long trip1_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_TRIP,
	PLL_CON0_PLL_TRIP,
	PLL_CON0_TRIP_SWITCH_MUX,
	CLK_CON_MUX_ECLK_ACLK_MUX,
	CLK_CON_MUX_TRIP_SWITCH_PLL_MUX,
	CLK_CON_DIV_CLK_TRIP_DROOP_DETECTOR,
	CLK_CON_DIV_CLK_TRIP_HPM_TARGET,
	CLK_CON_DIV_DIV_CLK_TRIP,
	CLK_CON_DIV_TRIP_ACLK,
	CLK_CON_DIV_TRIP_PCLK,
	CLK_CON_DIV_TRIP_PCLK_DD,
	CLK_CON_BUF_BOUT_OSCCLK_TRIP,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_CLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_CK_IN,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_HPM_TRIP_IPCLKPORT_I_HPM_TARGETCLK_C,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_LHS_ACEL_D_TRIP_IPCLKPORT_I_CLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_CMU_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_IPCLKPORT_ECLK,
	CLK_CON_GAT_CLK_DD_GATE,
	CLK_CON_GAT_CLK_HPM_TARGET_GATE,
	CLK_CON_GAT_CLK_SHORTSTOP_TRIP,
	CLK_CON_GAT_GATE_TRIP_PCLK_DD,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKM,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKS,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_AXI2APB_TRIP_IPCLKPORT_ACLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP_P,
	CLK_CON_GAT_GOUT_BLK_TRIP_UID_SYSREG_TRIP_IPCLKPORT_PCLK,
	CLK_CON_GAT_TRIP_PCLK_GATE,
};

static const struct samsung_pll_rate_table pll_trip1_rate_table[] __initconst = {
	PLL_35XX_RATE(2000000000, 250, 3, 0),
	PLL_35XX_RATE(1800000000, 225, 3, 0),
	PLL_35XX_RATE(1700000000, 425, 3, 1),
	PLL_35XX_RATE(1350000000, 225, 2, 1),
	PLL_35XX_RATE(106250000, 425, 3, 5),	/* Idle for 1.7 GHz */
	PLL_35XX_RATE(84375000, 225, 2, 5),	/* Idle for 1.35 GHz */
	PLL_35XX_RATE(62500000, 250, 3, 5),	/* Idle for 2.0 GHz */
	PLL_35XX_RATE(56250000, 225, 3, 5),	/* Idle for 1.8 GHz */
};

static const struct samsung_pll_clock trip1_pll_clks[] __initconst = {
	PLL(pll_142xx, CLK_TRIP_CLK, "fout_pll_trip1", "fin_pll", PLL_LOCKTIME_PLL_TRIP, PLL_CON0_PLL_TRIP, pll_trip1_rate_table),
};

PNAME(mout_trip1_pll_p) = { "fin_pll", "fout_pll_trip1" };
PNAME(mout_trip1_switch_mux_p) = { "fin_pll", "dout_cmu_trip1_switchclk" };
PNAME(mout_trip1_switch_pll_mux_p) = { "mout_trip1_pll", "mout_trip1_switch_mux" };
PNAME(mout_trip1_eclk_aclk_mux_p) = { "trip1_eclk_in", "dout_trip1_aclk" };

/*
 * 0x100 PLL_TRIP
 * 0x120 TRIP_SWITCH_MUX
 * 0x1004 TRIP_SWITCH_PLL_MUX
 * 0x1000 ECLK_ACLK_MUX
 */
static const struct samsung_mux_clock trip1_mux_clks[] __initconst = {
	MUX(MOUT_TRIP_PLL, "mout_trip1_pll", mout_trip1_pll_p, PLL_CON0_PLL_TRIP, 4, 1), /* 0x100 */
	MUX(MOUT_TRIP_SWITCH_MUX, "mout_trip1_switch_mux", mout_trip1_switch_mux_p, PLL_CON0_TRIP_SWITCH_MUX, 4, 1), /* 0x120 */
	MUX(MOUT_TRIP_SWITCH_PLL_MUX, "mout_trip1_switch_pll_mux", mout_trip1_switch_pll_mux_p, CLK_CON_MUX_TRIP_SWITCH_PLL_MUX, 0, 1), /* 0x1004 */
	MUX(0, "mout_trip1_eclk_aclk_mux", mout_trip1_eclk_aclk_mux_p, CLK_CON_MUX_ECLK_ACLK_MUX, 0, 1),  /* 0x1000 */
};

static const struct samsung_div_clock trip1_div_clks[] __initconst = {
	DIV(0, "dout_div_trip1_clk", "mout_trip1_switch_pll_mux", CLK_CON_DIV_DIV_CLK_TRIP, 0, 4),   /* 0x1808 */
	DIV(0, "dout_trip1_aclk", "dout_div_trip1_clk", CLK_CON_DIV_TRIP_ACLK, 0, 4), /* 0x180c */
	DIV(0, "dout_trip1_pclk", "mout_trip1_switch_pll_mux", CLK_CON_DIV_TRIP_PCLK, 0, 4), /* 0x1810 */
	DIV(0, "dout_trip1_pclk_dd", "mout_trip1_switch_pll_mux", CLK_CON_DIV_TRIP_PCLK_DD, 0, 4), /* 0x1814 */
	DIV(0, "dout_clk_trip1_droop_detector", "mout_trip1_switch_pll_mux", CLK_CON_DIV_CLK_TRIP_DROOP_DETECTOR, 0, 4), /* 0x1800 */
	DIV(0, "dout_clk_trip1_hpm_target", "mout_trip1_switch_pll_mux", CLK_CON_DIV_CLK_TRIP_HPM_TARGET, 0, 4), /* 0x1804 */
};

static const struct samsung_gate_clock trip1_gate_clks[] __initconst = {
	GATE(0, "bout_oscclk_trip1", "fin_pll", CLK_CON_BUF_BOUT_OSCCLK_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2000 */
	GATE(0, "trip1_busif_hpm_clk", "fin_pll", CLK_CON_GAT_CLK_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2004 */
	GATE(0, "trip1_droop_detector_ck_in", "dout_clk_trip1_droop_detector", CLK_CON_GAT_CLK_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_CK_IN, 21, CLK_IGNORE_UNUSED, 0), /* 0x2008 */
	GATE(0, "trip1_lhs_acel_d_trip_i_clk", "mout_trip1_eclk_aclk_mux", CLK_CON_GAT_CLK_BLK_TRIP_UID_LHS_ACEL_D_TRIP_IPCLKPORT_I_CLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2010 */
	GATE(0, "trip1_ns_brdg_trip_clk_pacc0_clk_trip1", "mout_trip1_eclk_aclk_mux", CLK_CON_GAT_CLK_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2014 */
	GATE(0, "trip1_cmu_trip_pclk", "dout_trip1_pclk", CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_CMU_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2018 */
	GATE(0, "trip1_trip_eclk", "dout_div_trip1_clk", CLK_CON_GAT_CLK_BLK_TRIP_UID_TRIP_IPCLKPORT_ECLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x201c */
	GATE(0, "trip1_clk_dd", "dout_clk_trip1_droop_detector", CLK_CON_GAT_CLK_DD_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x2020 */
	GATE(0, "trip1_clk_hpm_target", "dout_clk_trip1_hpm_target", CLK_CON_GAT_CLK_HPM_TARGET_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x2024 */
	GATE(0, "trip1_clk_shortstop_trip", "fin_pll", CLK_CON_GAT_CLK_SHORTSTOP_TRIP, 21, CLK_IGNORE_UNUSED, 0), /* 0x2028 */
	GATE(0, "trip1_trip_pclk_dd", "dout_trip1_pclk_dd", CLK_CON_GAT_GATE_TRIP_PCLK_DD, 21, CLK_IGNORE_UNUSED, 0), /* 0x202c */
	GATE(0, "trip1_ad_apb_dd_trip_pcklm", "dout_trip1_pclk_dd", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0), /* 0x2030 */
	GATE(0, "trip1_ad_apb_dd_trip_pckls", "dout_trip1_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AD_APB_DD_TRIP_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0), /* 0x2034 */
	GATE(0, "trip1_axi2apb_trip_aclk", "dout_trip1_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_AXI2APB_TRIP_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2038 */
	GATE(0, "trip1_busif_hpm_trip_pclk", "dout_trip1_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_BUSIF_HPM_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x203c */
	GATE(0, "trip1_droop_detector_trip_pclk", "dout_trip1_pclk_dd", CLK_CON_GAT_GOUT_BLK_TRIP_UID_DROOP_DETECTOR_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2040 */
	GATE(0, "trip1_ns_brdg_trip_clk_pacc0_clk_trip1_p", "dout_trip1_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_NS_BRDG_TRIP_IPCLKPORT_CLK__PACC0__CLK_TRIP_P, 21, CLK_IGNORE_UNUSED, 0), /* 0x2044 */
	GATE(0, "trip1_sysreg_trip_pclk", "dout_trip1_pclk", CLK_CON_GAT_GOUT_BLK_TRIP_UID_SYSREG_TRIP_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0), /* 0x2048 */
	GATE(0, "trip1_trip_pclk", "dout_trip1_pclk", CLK_CON_GAT_TRIP_PCLK_GATE, 21, CLK_IGNORE_UNUSED, 0), /* 0x204c */
};

static const struct samsung_cmu_info trip1_cmu_info __initconst = {
	.pll_clks		= trip1_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(trip1_pll_clks),
	.mux_clks		= trip1_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(trip1_mux_clks),
	.div_clks		= trip1_div_clks,
	.nr_div_clks		= ARRAY_SIZE(trip1_div_clks),
	.gate_clks		= trip1_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(trip1_gate_clks),
	.nr_clk_ids		= TRIP_NR_CLK,
	.clk_regs		= trip1_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(trip1_clk_regs),
};

static void __init trav_clk_trip1_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &trip1_cmu_info);
}

CLK_OF_DECLARE(trav_clk_trip1, "turbo,trav-clock-trip1", trav_clk_trip1_init);


/* Register Offset definitions for CMU_CAM_DPRX0 (0x14c10000) */
#define PLL_LOCKTIME_PLL_CAM_DPRX				0x0
#define PLL_CON0_BLK_DPRX0_DSC_CLK_MUX				0x100
#define PLL_CON0_DPRX_TOPCMUSWITCHCLK_MUX			0x120
#define PLL_CON0_PLL_CAM_DPRX					0x140
#define MUX_DPRX_SWITCH_MUX					0x1000
#define DIV_CLK_BLK_DPRX0_BUSD					0x1800
#define DIV_CLK_BLK_DPRX0_BUSP					0x1804
#define DIV_CLK_DPRX2_DSC_CLK					0x1808
#define DIV_CLK_DPRX2_RXCLK					0x1814
#define DIV_CLK_DPRX3_DSC_CLK					0x1818
#define DIV_CLK_DPRX3_RXCLK					0x181c
#define DIV_HDCP_CLK						0x1820
#define GAT_CAM_DPRX0_CAM_DPRX0_CMU_DPRX0_IPCLKPORT_PCLK	0x2000
#define GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_REF_CLK		0x2004
#define GAT_CAM_DPRX0_ADS_AXI_P_CAM_DPRX_IPCLKPORT_ACLKS	0x2008
#define GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKM		0x200c
#define GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKS		0x2010
#define GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKM		0x2014
#define GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKS		0x2018
#define GAT_CAM_DPRX0_AXI2APB_CAM_DPRX0_IPCLKPORT_ACLK		0x201c
#define GAT_CAM_DPRX0_BUS_D_CAM_DPRX_0_IPCLKPORT_MAINCLK	0x2020
#define GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK	0x2024
#define GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK_R	0x2028
#define GAT_CAM_DPRX0_DPRX2_IPCLKPORT_ACLK			0x202c
#define GAT_CAM_DPRX0_DPRX2_IPCLKPORT_I_DSC_CLK			0x2030
#define GAT_CAM_DPRX0_DPRX2_IPCLKPORT_PCLK			0x2034
#define GAT_CAM_DPRX0_DPRX2_IPCLKPORT_RXCLK			0x2038
#define GAT_CAM_DPRX0_DPRX3_IPCLKPORT_ACLK			0x203c
#define GAT_CAM_DPRX0_DPRX3_IPCLKPORT_I_DSC_CLK			0x2040
#define GAT_CAM_DPRX0_DPRX3_IPCLKPORT_PCLK			0x2044
#define GAT_CAM_DPRX0_DPRX3_IPCLKPORT_RXCLK			0x2048
#define GAT_CAM_DPRX0_HDCP_IPCLKPORT_CLK			0x204c
#define GAT_CAM_DPRX0_HDCP_IPCLKPORT_I_PCLK			0x2050
#define GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_PCLK		0x2054
#define GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_D	0x2058
#define GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_P	0x205c
#define GAT_CAM_DPRX0_SYSREG_CAM_DPRX0_IPCLKPORT_PCLK		0x2060
#define GAT_CAM_DPRX0_TBU_CAM_DPRX_0_IPCLKPORT_ACLK		0x2064

static const unsigned long cam_dprx0_clk_regs[] __initconst = {
	PLL_LOCKTIME_PLL_CAM_DPRX,
	PLL_CON0_BLK_DPRX0_DSC_CLK_MUX,
	PLL_CON0_DPRX_TOPCMUSWITCHCLK_MUX,
	PLL_CON0_PLL_CAM_DPRX,
	MUX_DPRX_SWITCH_MUX,
	DIV_CLK_BLK_DPRX0_BUSD,
	DIV_CLK_BLK_DPRX0_BUSP,
	DIV_CLK_DPRX2_DSC_CLK,
	DIV_CLK_DPRX2_RXCLK,
	DIV_CLK_DPRX3_DSC_CLK,
	DIV_CLK_DPRX3_RXCLK,
	DIV_HDCP_CLK,
	GAT_CAM_DPRX0_CAM_DPRX0_CMU_DPRX0_IPCLKPORT_PCLK,
	GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_REF_CLK,
	GAT_CAM_DPRX0_ADS_AXI_P_CAM_DPRX_IPCLKPORT_ACLKS,
	GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKM,
	GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKS,
	GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKM,
	GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKS,
	GAT_CAM_DPRX0_AXI2APB_CAM_DPRX0_IPCLKPORT_ACLK,
	GAT_CAM_DPRX0_BUS_D_CAM_DPRX_0_IPCLKPORT_MAINCLK,
	GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK,
	GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK_R,
	GAT_CAM_DPRX0_DPRX2_IPCLKPORT_ACLK,
	GAT_CAM_DPRX0_DPRX2_IPCLKPORT_I_DSC_CLK,
	GAT_CAM_DPRX0_DPRX2_IPCLKPORT_PCLK,
	GAT_CAM_DPRX0_DPRX2_IPCLKPORT_RXCLK,
	GAT_CAM_DPRX0_DPRX3_IPCLKPORT_ACLK,
	GAT_CAM_DPRX0_DPRX3_IPCLKPORT_I_DSC_CLK,
	GAT_CAM_DPRX0_DPRX3_IPCLKPORT_PCLK,
	GAT_CAM_DPRX0_DPRX3_IPCLKPORT_RXCLK,
	GAT_CAM_DPRX0_HDCP_IPCLKPORT_CLK,
	GAT_CAM_DPRX0_HDCP_IPCLKPORT_I_PCLK,
	GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_PCLK,
	GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_D,
	GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_P,
	GAT_CAM_DPRX0_SYSREG_CAM_DPRX0_IPCLKPORT_PCLK,
	GAT_CAM_DPRX0_TBU_CAM_DPRX_0_IPCLKPORT_ACLK,
};

static const struct samsung_pll_rate_table pll_cam_dprx_rate_table[] __initconst = {
	PLL_35XX_RATE(1640000000, 205, 3, 0),
};

static const struct samsung_pll_clock cam_dprx0_pll_clks[] __initconst = {
	PLL(pll_142xx, 0, "fout_pll_cam_dprx", "fin_pll", PLL_LOCKTIME_PLL_CAM_DPRX, PLL_CON0_PLL_CAM_DPRX, pll_cam_dprx_rate_table),
};

PNAME(mout_cam_dprx0_pll_p) = { "fin_pll", "fout_pll_cam_dprx" };
PNAME(mout_cam_dprx0_dsc_clk_mux_p) = { "fin_pll", "dout_cmu_dprx_dsc_clk" };
PNAME(mout_cam_dprx0_topcmuswitchclk_mux_p) = { "fin_pll", "dout_cmu_dprx_switchclk" };
PNAME(mout_cam_dprx0_switch_mux_p) = { "mout_cam_dprx_pll", "mout_cam_dprx0_topcmuswitchclk_mux" };

static const struct samsung_mux_clock cam_dprx0_mux_clks[] __initconst = {
	MUX(MOUT_CAM_DPRX0_PLL, "mout_cam_dprx0_pll", mout_cam_dprx0_pll_p, PLL_CON0_PLL_CAM_DPRX, 4, 1),
	MUX(0, "mout_cam_dprx0_dsc_clk_mux", mout_cam_dprx0_dsc_clk_mux_p, PLL_CON0_BLK_DPRX0_DSC_CLK_MUX, 4, 1),
	MUX(0, "mout_cam_dprx0_topcmuswitchclk_mux", mout_cam_dprx0_topcmuswitchclk_mux_p, PLL_CON0_DPRX_TOPCMUSWITCHCLK_MUX, 4, 1),
	MUX(MOUT_CAM_DPRX0_SWITCH_MUX, "mout_cam_dprx0_switch_mux", mout_cam_dprx0_switch_mux_p, MUX_DPRX_SWITCH_MUX, 0, 1),
};

static const struct samsung_div_clock cam_dprx0_div_clks[] __initconst = {
	DIV(0, "dout_dprx0_clk_blk_dprx0_busd", "mout_cam_dprx0_switch_mux", DIV_CLK_BLK_DPRX0_BUSD, 0, 4),
	DIV(0, "dout_dprx0_clk_blk_dprx0_busp", "dout_dprx0_clk_blk_dprx0_busd", DIV_CLK_BLK_DPRX0_BUSP, 0, 4),
	DIV(0, "dout_dprx0_clk_dprx2_dsc_clk", "mout_cam_dprx0_dsc_clk_mux", DIV_CLK_DPRX2_DSC_CLK, 0, 4),
	DIV(0, "dout_dprx0_clk_dprx2_rxclk", "mout_cam_dprx0_pll", DIV_CLK_DPRX2_RXCLK, 0, 4),
	DIV(0, "dout_dprx0_clk_dprx3_dsc_clk", "mout_cam_dprx0_dsc_clk_mux", DIV_CLK_DPRX3_DSC_CLK, 0, 4),
	DIV(0, "dout_dprx0_clk_dprx3_rxclk", "mout_cam_dprx0_pll", DIV_CLK_DPRX3_RXCLK, 0, 4),
	DIV(0, "dout_dprx0_hdcp_clk", "fin_pll", DIV_HDCP_CLK, 0, 4),
};

static const struct samsung_gate_clock cam_dprx0_gate_clks[] __initconst = {
	GATE(CAM_DPRX0_CMU_DPRX0_IPCLKPORT_PCLK, "cam_dprx0_cam_dprx0_cmu_dprx0_ipclkport_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_CAM_DPRX0_CMU_DPRX0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(ADS_AXI_P_CAM_DPRX_IPCLKPORT_ACLKS, "cam_dprx0_ads_axi_p_cam_dprx_ipclkport_aclks", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_ADS_AXI_P_CAM_DPRX_IPCLKPORT_ACLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(AXI2APB_CAM_DPRX0_IPCLKPORT_ACLK, "cam_dprx0_axi2apb_cam_dprx0_ipclkport_aclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_AXI2APB_CAM_DPRX0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_D_CAM_DPRX_0_IPCLKPORT_MAINCLK, "cam_dprx0_bus_d_cam_dprx_0_ipclkport_mainclk", "dout_dprx0_clk_blk_dprx0_busd", GAT_CAM_DPRX0_BUS_D_CAM_DPRX_0_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK, "cam_dprx0_bus_p_cam_dprx_0_ipclkport_mainclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK_R, "cam_dprx0_bus_p_cam_dprx_0_ipclkport_mainclk_r", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_BUS_P_CAM_DPRX_0_IPCLKPORT_MAINCLK_R, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX2_IPCLKPORT_ACLK, "cam_dprx0_dprx2_ipclkport_aclk", "dout_dprx0_clk_blk_dprx0_busd", GAT_CAM_DPRX0_DPRX2_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX2_IPCLKPORT_I_DSC_CLK, "cam_dprx0_dprx2_ipclkport_i_dsc_clk", "dout_dprx0_clk_dprx2_dsc_clk", GAT_CAM_DPRX0_DPRX2_IPCLKPORT_I_DSC_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX2_IPCLKPORT_PCLK, "cam_dprx0_dprx2_ipclkport_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_DPRX2_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX2_IPCLKPORT_RXCLK, "cam_dprx0_dprx2_ipclkport_rxclk", "dout_dprx0_clk_dprx2_rxclk", GAT_CAM_DPRX0_DPRX2_IPCLKPORT_RXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX3_IPCLKPORT_ACLK, "cam_dprx0_dprx3_ipclkport_aclk", "dout_dprx0_clk_blk_dprx0_busd", GAT_CAM_DPRX0_DPRX3_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX3_IPCLKPORT_I_DSC_CLK, "cam_dprx0_dprx3_ipclkport_i_dsc_clk", "dout_dprx0_clk_dprx3_dsc_clk", GAT_CAM_DPRX0_DPRX3_IPCLKPORT_I_DSC_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX3_IPCLKPORT_PCLK, "cam_dprx0_dprx3_ipclkport_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_DPRX3_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX3_IPCLKPORT_RXCLK,"cam_dprx0_dprx3_ipclkport_rxclk", "dout_dprx0_clk_dprx3_rxclk", GAT_CAM_DPRX0_DPRX3_IPCLKPORT_RXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_dprx0_hdcp_ipclkport_clk", "dout_dprx0_hdcp_clk", GAT_CAM_DPRX0_HDCP_IPCLKPORT_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_dprx0_hdcp_ipclkport_i_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_HDCP_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_D, "cam_dprx0_ns_brdg_cam_dprx_0_ipclkport_clk__psoc_cam_dprx__clk_cam_dprx_d", "dout_dprx0_clk_blk_dprx0_busd", GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_P, "cam_dprx0_ns_brdg_cam_dprx_0_ipclkport_clk__psoc_cam_dprx__clk_cam_dprx_p", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_NS_BRDG_CAM_DPRX_0_IPCLKPORT_CLK__PSOC_CAM_DPRX__CLK_CAM_DPRX_P, 21, CLK_IGNORE_UNUSED, 0),
	GATE(SYSREG_CAM_DPRX0_IPCLKPORT_PCLK, "cam_dprx0_sysreg_cam_dprx0_ipclkport_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_SYSREG_CAM_DPRX0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(TBU_CAM_DPRX_0_IPCLKPORT_ACLK, "cam_dprx0_tbu_cam_dprx_0_ipclkport_aclk", "dout_dprx0_clk_blk_dprx0_busd", GAT_CAM_DPRX0_TBU_CAM_DPRX_0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX2_IPCLKPORT_PCLKS, "cam_dprx0_apb_async_dprx2_ipclkport_pclks", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX2_IPCLKPORT_PCLKM, "cam_dprx0_apb_async_dprx2_ipclkport_pclkm", "dout_dprx0_clk_dprx2_rxclk", GAT_CAM_DPRX0_APB_ASYNC_DPRX2_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX3_IPCLKPORT_PCLKS, "cam_dprx0_apb_async_dprx3_ipclkport_pclks", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX3_IPCLKPORT_PCLKM, "cam_dprx0_apb_async_dprx3_ipclkport_pclkm", "dout_dprx0_clk_dprx3_rxclk", GAT_CAM_DPRX0_APB_ASYNC_DPRX3_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_dprx0_hdcp_sram_ipclkport_i_ref_clk", "fin_pll", GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_REF_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(0, "cam_dprx0_hdcp_sram_ipclkport_i_pclk", "dout_dprx0_clk_blk_dprx0_busp", GAT_CAM_DPRX0_HDCP_SRAM_IPCLKPORT_I_PCLK, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info cam_dprx0_cmu_info __initconst = {
	.pll_clks		= cam_dprx0_pll_clks,
	.nr_pll_clks		= ARRAY_SIZE(cam_dprx0_pll_clks),
	.mux_clks		= cam_dprx0_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(cam_dprx0_mux_clks),
	.div_clks		= cam_dprx0_div_clks,
	.nr_div_clks		= ARRAY_SIZE(cam_dprx0_div_clks),
	.gate_clks		= cam_dprx0_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(cam_dprx0_gate_clks),
	.nr_clk_ids		= CAM_DPRX0_NR_CLK,
	.clk_regs		= cam_dprx0_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(cam_dprx0_clk_regs),
};

static void __init trav_clk_cam_dprx0_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &cam_dprx0_cmu_info);
}

CLK_OF_DECLARE(trav_clk_cam_dprx0, "turbo,trav-clock-cam_dprx0", trav_clk_cam_dprx0_init);


/* Register Offset definitions for CMU_CAM_DPRX1 (0x14c10000) (TBD:CHANGE the base)*/
#define PLL_CON0_BLK_DPRX1_DSC_CLK_MUX				0x100
#define PLL_CON0_DPRX1_BUSD_MUX					0x120
#define PLL_CON0_DPRX1_RXCLK_MUX				0x140
#define DIV_CLK_BLK_DPRX1_BUSD					0x1800
#define DIV_CLK_BLK_DPRX1_BUSP					0x1804
#define DIV_CLK_DPRX0_DSC_CLK					0x1808
#define DIV_CLK_DPRX0_RXCLK					0x180c
#define DIV_CLK_DPRX1_DSC_CLK					0x1810
#define DIV_CLK_DPRX1_RXCLK					0x1814
#define GAT_CAM_DPRX1_CAM_DPRX1_CMU_DPRX1_IPCLKPORT_PCLK	0x2000
#define GAT_CAM_DPRX1_ADM_AXI_P_CAM_DPRX_IPCLKPORT_ACLKM	0x2008
#define GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKM		0x200c
#define GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKS		0x2010
#define GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKM		0x2014
#define GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKS		0x2018
#define GAT_CAM_DPRX1_AXI2APB_CAM_DPRX1_IPCLKPORT_ACLK		0x201c
#define GAT_CAM_DPRX1_BUS_D_CAM_DPRX_1_IPCLKPORT_MAINCLK	0x2020
#define GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK	0x2024
#define GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK_R	0x2028
#define GAT_CAM_DPRX1_DPRX0_IPCLKPORT_ACLK			0x202c
#define GAT_CAM_DPRX1_DPRX0_IPCLKPORT_I_DSC_CLK			0x2030
#define GAT_CAM_DPRX1_DPRX0_IPCLKPORT_PCLK			0x2034
#define GAT_CAM_DPRX1_DPRX0_IPCLKPORT_RXCLK			0x2038
#define GAT_CAM_DPRX1_DPRX1_IPCLKPORT_ACLK			0x203c
#define GAT_CAM_DPRX1_DPRX1_IPCLKPORT_I_DSC_CLK			0x2040
#define GAT_CAM_DPRX1_DPRX1_IPCLKPORT_PCLK			0x2044
#define GAT_CAM_DPRX1_DPRX1_IPCLKPORT_RXCLK			0x2048
#define GAT_CAM_DPRX1_NS_BRDG_CAM_DPRX_1_IPCLKPORT_CLK__PSOC_CAM_DPRX1__CLK_CAM_DPRX_D	0x204c
#define GAT_CAM_DPRX1_SYSREG_CAM_DPRX1_IPCLKPORT_PCLK		0x2050
#define GAT_CAM_DPRX1_TBU_CAM_DPRX_1_IPCLKPORT_ACLK		0x2054

static const unsigned long cam_dprx1_clk_regs[] __initconst = {
	PLL_CON0_BLK_DPRX1_DSC_CLK_MUX,
	PLL_CON0_DPRX1_BUSD_MUX,
	PLL_CON0_DPRX1_RXCLK_MUX,
	DIV_CLK_BLK_DPRX1_BUSD,
	DIV_CLK_BLK_DPRX1_BUSP,
	DIV_CLK_DPRX0_DSC_CLK,
	DIV_CLK_DPRX0_RXCLK,
	DIV_CLK_DPRX1_DSC_CLK,
	DIV_CLK_DPRX1_RXCLK,
	GAT_CAM_DPRX1_CAM_DPRX1_CMU_DPRX1_IPCLKPORT_PCLK,
	GAT_CAM_DPRX1_ADM_AXI_P_CAM_DPRX_IPCLKPORT_ACLKM,
	GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKM,
	GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKS,
	GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKM,
	GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKS,
	GAT_CAM_DPRX1_AXI2APB_CAM_DPRX1_IPCLKPORT_ACLK,
	GAT_CAM_DPRX1_BUS_D_CAM_DPRX_1_IPCLKPORT_MAINCLK,
	GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK,
	GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK_R,
	GAT_CAM_DPRX1_DPRX0_IPCLKPORT_ACLK,
	GAT_CAM_DPRX1_DPRX0_IPCLKPORT_I_DSC_CLK,
	GAT_CAM_DPRX1_DPRX0_IPCLKPORT_PCLK,
	GAT_CAM_DPRX1_DPRX0_IPCLKPORT_RXCLK,
	GAT_CAM_DPRX1_DPRX1_IPCLKPORT_ACLK,
	GAT_CAM_DPRX1_DPRX1_IPCLKPORT_I_DSC_CLK,
	GAT_CAM_DPRX1_DPRX1_IPCLKPORT_PCLK,
	GAT_CAM_DPRX1_DPRX1_IPCLKPORT_RXCLK,
	GAT_CAM_DPRX1_NS_BRDG_CAM_DPRX_1_IPCLKPORT_CLK__PSOC_CAM_DPRX1__CLK_CAM_DPRX_D,
	GAT_CAM_DPRX1_SYSREG_CAM_DPRX1_IPCLKPORT_PCLK,
	GAT_CAM_DPRX1_TBU_CAM_DPRX_1_IPCLKPORT_ACLK,
};

PNAME(mout_cam_dprx1_busd_mux_p) = { "fin_pll", "mout_cam_dprx0_switch_mux" };
PNAME(mout_cam_dprx1_dsc_clk_mux_p) = { "fin_pll", "dout_cmu_dprx_dsc_clk" };
PNAME(mout_cam_dprx1_rxclk_mux_p) = { "fin_pll", "mout_cam_dprx0_pll" };

static const struct samsung_mux_clock cam_dprx1_mux_clks[] __initconst = {
	MUX(0, "mout_cam_dprx1_busd_mux", mout_cam_dprx1_busd_mux_p, PLL_CON0_DPRX1_BUSD_MUX, 4, 1),
	MUX(0, "mout_cam_dprx1_dsc_clk_mux", mout_cam_dprx1_dsc_clk_mux_p, PLL_CON0_BLK_DPRX1_DSC_CLK_MUX, 4, 1),
	MUX(0, "mout_cam_dprx1_rxclk_mux", mout_cam_dprx1_rxclk_mux_p, PLL_CON0_DPRX1_RXCLK_MUX, 4, 1),
};

static const struct samsung_div_clock cam_dprx1_div_clks[] __initconst = {
	DIV(0, "dout_dprx1_clk_blk_dprx1_busd", "mout_cam_dprx1_busd_mux", DIV_CLK_BLK_DPRX1_BUSD, 0, 4),
	DIV(0, "dout_dprx1_clk_blk_dprx1_busp", "mout_cam_dprx1_busd_mux", DIV_CLK_BLK_DPRX1_BUSP, 0, 4),
	DIV(0, "dout_dprx1_clk_dprx0_dsc_clk", "mout_cam_dprx1_dsc_clk_mux", DIV_CLK_DPRX0_DSC_CLK, 0, 4),
	DIV(0, "dout_dprx1_clk_dprx0_rxclk", "mout_cam_dprx1_rxclk_mux", DIV_CLK_DPRX0_RXCLK, 0, 4),
	DIV(0, "dout_dprx1_clk_dprx1_dsc_clk", "mout_cam_dprx1_dsc_clk_mux", DIV_CLK_DPRX1_DSC_CLK, 0, 4),
	DIV(0, "dout_dprx1_clk_dprx1_rxclk", "mout_cam_dprx1_rxclk_mux", DIV_CLK_DPRX1_RXCLK, 0, 4),
};

static const struct samsung_gate_clock cam_dprx1_gate_clks[] __initconst = {
	GATE(CAM_DPRX1_CMU_DPRX1_IPCLKPORT_PCLK, "cam_dprx1_cam_dprx1_cmu_dprx1_ipclkport_pclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_CAM_DPRX1_CMU_DPRX1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(ADM_AXI_P_CAM_DPRX_IPCLKPORT_ACLKM, "cam_dprx1_adm_axi_p_cam_dprx_ipclkport_aclkm", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_ADM_AXI_P_CAM_DPRX_IPCLKPORT_ACLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(AXI2APB_CAM_DPRX1_IPCLKPORT_ACLK, "cam_dprx1_axi2apb_cam_dprx1_ipclkport_aclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_AXI2APB_CAM_DPRX1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_D_CAM_DPRX_1_IPCLKPORT_MAINCLK, "cam_dprx1_bus_d_cam_dprx_1_ipclkport_mainclk", "dout_dprx1_clk_blk_dprx1_busd", GAT_CAM_DPRX1_BUS_D_CAM_DPRX_1_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK, "cam_dprx1_bus_p_cam_dprx_1_ipclkport_mainclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK_R, "cam_dprx1_bus_p_cam_dprx_1_ipclkport_mainclk_r", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_BUS_P_CAM_DPRX_1_IPCLKPORT_MAINCLK_R, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX0_IPCLKPORT_ACLK, "cam_dprx1_dprx0_ipclkport_aclk", "dout_dprx1_clk_blk_dprx1_busd", GAT_CAM_DPRX1_DPRX0_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX0_IPCLKPORT_I_DSC_CLK, "cam_dprx1_dprx0_ipclkport_i_dsc_clk", "dout_dprx1_clk_dprx0_dsc_clk", GAT_CAM_DPRX1_DPRX0_IPCLKPORT_I_DSC_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX0_IPCLKPORT_PCLK, "cam_dprx1_dprx0_ipclkport_pclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_DPRX0_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX0_IPCLKPORT_RXCLK, "cam_dprx1_dprx0_ipclkport_rxclk", "dout_dprx1_clk_dprx0_rxclk", GAT_CAM_DPRX1_DPRX0_IPCLKPORT_RXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX1_IPCLKPORT_ACLK, "cam_dprx1_dprx1_ipclkport_aclk", "dout_dprx1_clk_blk_dprx1_busd", GAT_CAM_DPRX1_DPRX1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX1_IPCLKPORT_I_DSC_CLK, "cam_dprx1_dprx1_ipclkport_i_dsc_clk", "dout_dprx1_clk_dprx1_dsc_clk", GAT_CAM_DPRX1_DPRX1_IPCLKPORT_I_DSC_CLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX1_IPCLKPORT_PCLK, "cam_dprx1_dprx1_ipclkport_pclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_DPRX1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(DPRX1_IPCLKPORT_RXCLK, "cam_dprx1_dprx1_ipclkport_rxclk", "dout_dprx1_clk_dprx1_rxclk", GAT_CAM_DPRX1_DPRX1_IPCLKPORT_RXCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(NS_BRDG_CAM_DPRX_1_IPCLKPORT_CLK__PSOC_CAM_DPRX1__CLK_CAM_DPRX_D, "cam_dprx1_ns_brdg_cam_dprx_1_ipclkport_clk__psoc_cam_dprx1__clk_cam_dprx_d", "dout_dprx1_clk_blk_dprx1_busd", GAT_CAM_DPRX1_NS_BRDG_CAM_DPRX_1_IPCLKPORT_CLK__PSOC_CAM_DPRX1__CLK_CAM_DPRX_D, 21, CLK_IGNORE_UNUSED, 0),
	GATE(SYSREG_CAM_DPRX1_IPCLKPORT_PCLK, "cam_dprx1_sysreg_cam_dprx1_ipclkport_pclk", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_SYSREG_CAM_DPRX1_IPCLKPORT_PCLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(TBU_CAM_DPRX_1_IPCLKPORT_ACLK, "cam_dprx1_tbu_cam_dprx_1_ipclkport_aclk", "dout_dprx1_clk_blk_dprx1_busd", GAT_CAM_DPRX1_TBU_CAM_DPRX_1_IPCLKPORT_ACLK, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX0_IPCLKPORT_PCLKS, "cam_dprx1_apb_async_dprx0_ipclkport_pclks", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX0_IPCLKPORT_PCLKM, "cam_dprx1_apb_async_dprx0_ipclkport_pclkm", "dout_dprx1_clk_dprx0_rxclk", GAT_CAM_DPRX1_APB_ASYNC_DPRX0_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX1_IPCLKPORT_PCLKS, "cam_dprx1_apb_async_dprx1_ipclkport_pclks", "dout_dprx1_clk_blk_dprx1_busp", GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKS, 21, CLK_IGNORE_UNUSED, 0),
	GATE(APB_ASYNC_DPRX1_IPCLKPORT_PCLKM, "cam_dprx1_apb_async_dprx1_ipclkport_pclkm", "dout_dprx1_clk_dprx1_rxclk", GAT_CAM_DPRX1_APB_ASYNC_DPRX1_IPCLKPORT_PCLKM, 21, CLK_IGNORE_UNUSED, 0),
};

static const struct samsung_cmu_info cam_dprx1_cmu_info __initconst = {
	.mux_clks		= cam_dprx1_mux_clks,
	.nr_mux_clks		= ARRAY_SIZE(cam_dprx1_mux_clks),
	.div_clks		= cam_dprx1_div_clks,
	.nr_div_clks		= ARRAY_SIZE(cam_dprx1_div_clks),
	.gate_clks		= cam_dprx1_gate_clks,
	.nr_gate_clks		= ARRAY_SIZE(cam_dprx1_gate_clks),
	.nr_clk_ids		= CAM_DPRX1_NR_CLK,
	.clk_regs		= cam_dprx1_clk_regs,
	.nr_clk_regs		= ARRAY_SIZE(cam_dprx1_clk_regs),
};

static void __init trav_clk_cam_dprx1_init(struct device_node *np)
{
	samsung_cmu_register_one(np, &cam_dprx1_cmu_info);
}

CLK_OF_DECLARE(trav_clk_cam_dprx1, "turbo,trav-clock-cam_dprx1", trav_clk_cam_dprx1_init);
