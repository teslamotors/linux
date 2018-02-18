/*
 * general p state infrastructure
 *
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
 */
#ifndef _ctrlclk_h_
#define _ctrlclk_h_

#include "ctrlboardobj.h"
#include "ctrlclkavfs.h"
#include "ctrlvolt.h"

#define CTRL_CLK_CLK_DELTA_MAX_VOLT_RAILS 4

/* valid clock domain values */
#define CTRL_CLK_DOMAIN_MCLK                                (0x00000010)
#define CTRL_CLK_DOMAIN_DISPCLK                             (0x00000040)
#define CTRL_CLK_DOMAIN_GPC2CLK                             (0x00010000)
#define CTRL_CLK_DOMAIN_XBAR2CLK                            (0x00040000)
#define CTRL_CLK_DOMAIN_SYS2CLK                             (0x00080000)
#define CTRL_CLK_DOMAIN_HUB2CLK                             (0x00100000)
#define CTRL_CLK_DOMAIN_PWRCLK                              (0x00800000)
#define CTRL_CLK_DOMAIN_NVDCLK                              (0x01000000)
#define CTRL_CLK_DOMAIN_PCIEGENCLK                          (0x02000000)

#define CTRL_CLK_DOMAIN_GPCCLK                              (0x10000000)
#define CTRL_CLK_DOMAIN_XBARCLK                             (0x20000000)
#define CTRL_CLK_DOMAIN_SYSCLK                              (0x40000000)
#define CTRL_CLK_DOMAIN_HUBCLK                              (0x80000000)

#define CTRL_CLK_CLK_DOMAIN_TYPE_3X                                  0x01
#define CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED                            0x02
#define CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG                             0x03
#define CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER                           0x04
#define CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE                            0x05

#define CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_INVALID 0xFF
#define CTRL_CLK_CLK_DOMAIN_INDEX_INVALID                           0xFF

#define CTRL_CLK_CLK_PROG_TYPE_1X                              0x00
#define CTRL_CLK_CLK_PROG_TYPE_1X_MASTER                       0x01
#define CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO                 0x02
#define CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_TABLE                 0x03
#define CTRL_CLK_CLK_PROG_TYPE_UNKNOWN                         255

/*!
 * Enumeration of CLK_PROG source types.
 */
#define CTRL_CLK_PROG_1X_SOURCE_PLL                            0x00
#define CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE                     0x01
#define CTRL_CLK_PROG_1X_SOURCE_FLL                            0x02
#define CTRL_CLK_PROG_1X_SOURCE_INVALID                        255

#define CTRL_CLK_CLK_PROG_1X_MASTER_VF_ENTRY_MAX_ENTRIES 4
#define CTRL_CLK_PROG_1X_MASTER_MAX_SLAVE_ENTRIES 6

#define CTRL_CLK_CLK_VF_POINT_IDX_INVALID                      255

#define CTRL_CLK_CLK_VF_POINT_TYPE_FREQ                         0x00
#define CTRL_CLK_CLK_VF_POINT_TYPE_VOLT                         0x01
#define CTRL_CLK_CLK_VF_POINT_TYPE_UNKNOWN                      255

struct ctrl_clk_clk_prog_1x_master_source_fll {
	u32 base_vfsmooth_volt_uv;
	u32 max_vf_ramprate;
	u32 max_freq_stepsize_mhz;
};

union ctrl_clk_clk_prog_1x_master_source_data {
	struct ctrl_clk_clk_prog_1x_master_source_fll fll;
};

struct ctrl_clk_clk_vf_point_info_freq {
	u16 freq_mhz;
};

struct ctrl_clk_clk_vf_point_info_volt {
	u32  sourceVoltageuV;
	u8  vfGainVfeEquIdx;
	u8  clkDomainIdx;
};

struct ctrl_clk_clk_prog_1x_master_vf_entry {
	u8 vfe_idx;
	u8 gain_vfe_idx;
	u8 vf_point_idx_first;
	u8 vf_point_idx_last;
};

struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry {
	u8 clk_dom_idx;
	u8 ratio;
};

struct ctrl_clk_clk_prog_1x_master_table_slave_entry {
	u8 clk_dom_idx;
	u16 freq_mhz;
};

struct ctrl_clk_clk_prog_1x_source_pll {
	u8 pll_idx;
	u8 freq_step_size_mhz;
};

struct ctrl_clk_clk_delta {
	int freq_delta_khz;
	int volt_deltauv[CTRL_CLK_CLK_DELTA_MAX_VOLT_RAILS];

};

union ctrl_clk_clk_prog_1x_source_data {
	struct ctrl_clk_clk_prog_1x_source_pll pll;
};

struct ctrl_clk_vf_pair {
	u16 freq_mhz;
	u32 voltage_uv;
};

struct ctrl_clk_clk_domain_list_item {
	u32  clk_domain;
	u32  clk_freq_khz;
	u32  clk_flags;
	u8   current_regime_id;
	u8   target_regime_id;
};

#define CTRL_CLK_VF_PAIR_FREQ_MHZ_GET(pvfpair)                          \
	((pvfpair)->freq_mhz)

#define CTRL_CLK_VF_PAIR_VOLTAGE_UV_GET(pvfpair)                        \
	((pvfpair)->voltage_uv)

#define CTRL_CLK_VF_PAIR_FREQ_MHZ_SET(pvfpair, _freqmhz)                \
	(((pvfpair)->freq_mhz) = (_freqmhz))

#define CTRL_CLK_VF_PAIR_FREQ_MHZ_SET(pvfpair, _freqmhz)                \
	(((pvfpair)->freq_mhz) = (_freqmhz))


#define CTRL_CLK_VF_PAIR_VOLTAGE_UV_SET(pvfpair, _voltageuv)	        \
	(((pvfpair)->voltage_uv) = (_voltageuv))

#endif
