/*
 * drivers/video/tegra/dc/dp_lt.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DP_LT_H__
#define __DRIVERS_VIDEO_TEGRA_DC_DP_LT_H__

#define CR_RETRY_LIMIT 5
#define CE_RETRY_LIMIT 5
#define LT_TIMEOUT_MS 10000
#define HPD_DROP_TIMEOUT_MS 1500

enum {
	STATE_RESET,
	STATE_FAST_LT,
	STATE_CLOCK_RECOVERY,
	STATE_CHANNEL_EQUALIZATION,
	STATE_DONE_FAIL,
	STATE_DONE_PASS,
	STATE_REDUCE_BIT_RATE,
	STATE_COUNT,
};

static const char * const tegra_dp_lt_state_names[] = {
	"Reset",
	"fast link training",
	"clock recovery",
	"channel equalization",
	"link training fail/disable",
	"link training pass",
	"reduce bit rate",
};

struct tegra_dp_lt_data {
	struct tegra_dc_dp_data *dp;
	int shutdown;
	int state;
	int tps;
	int pending_evt; /* pending link training request */
	struct mutex lock;
	struct delayed_work dwork;
	bool force_disable; /* not sticky */
	bool force_trigger; /* not sticky */
	struct completion lt_complete;

	u8 no_aux_handshake;
	u8 tps3_supported;
	u8 aux_rd_interval;

	bool lt_config_valid;
	u32 drive_current[4]; /* voltage swing */
	u32 pre_emphasis[4]; /* post cursor1 */
	u32 post_cursor2[4];
	u32 tx_pu;
	u32 n_lanes;
	u32 link_bw;

	u32 cr_retry;
	u32 ce_retry;
};

/* CTS approved list. Do not alter. */
static const u8 tegra_dp_link_config_priority[][2] = {
	/* link bandwidth, lane count */
	{SOR_LINK_SPEED_G5_4, 4}, /* 21.6Gbps */
	{SOR_LINK_SPEED_G2_7, 4}, /* 10.8Gbps */
	{SOR_LINK_SPEED_G1_62, 4}, /* 6.48Gbps */
	{SOR_LINK_SPEED_G5_4, 2}, /* 10.8Gbps */
	{SOR_LINK_SPEED_G2_7, 2}, /* 5.4Gbps */
	{SOR_LINK_SPEED_G1_62, 2}, /* 3.24Gbps */
	{SOR_LINK_SPEED_G5_4, 1}, /* 5.4Gbps */
	{SOR_LINK_SPEED_G2_7, 1}, /* 2.7Gbps */
	{SOR_LINK_SPEED_G1_62, 1}, /* 1.62Gbps */
};

#ifndef CONFIG_TEGRA_NVDISPLAY
static const u32 tegra_dp_vs_regs[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x13, 0x19, 0x1e, 0x28}, /* voltage swing: L0 */
		{0x1e, 0x25, 0x2d}, /* L1 */
		{0x28, 0x32}, /* L2 */
		{0x3c}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x12, 0x17, 0x1b, 0x25},
		{0x1c, 0x23, 0x2a},
		{0x25, 0x2f},
		{0x39},
	},

	/* postcursor2 L2 */
	{
		{0x12, 0x16, 0x1a, 0x22},
		{0x1b, 0x20, 0x27},
		{0x24, 0x2d},
		{0x36},
	},

	/* postcursor2 L3 */
	{
		{0x11, 0x14, 0x17, 0x1f},
		{0x19, 0x1e, 0x24},
		{0x22, 0x2a},
		{0x32},
	},
};
#else
static const u32 tegra_dp_vs_regs[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x13, 0x19, 0x1e, 0x28}, /* voltage swing: L0 */
		{0x1e, 0x25, 0x2d}, /* L1 */
		{0x28, 0x32}, /* L2 */
		{0x39}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x12, 0x17, 0x1b, 0x25},
		{0x1c, 0x23, 0x2a},
		{0x25, 0x2f},
		{0x37},
	},

	/* postcursor2 L2 */
	{
		{0x12, 0x16, 0x1a, 0x22},
		{0x1b, 0x20, 0x27},
		{0x24, 0x2d},
		{0x35},
	},

	/* postcursor2 L3 */
	{
		{0x11, 0x14, 0x17, 0x1f},
		{0x19, 0x1e, 0x24},
		{0x22, 0x2a},
		{0x32},
	},
};
#endif

/* Both 12x and 13x config enabled for 13x */
#if (defined(CONFIG_ARCH_TEGRA_12x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_13x_SOC))
static const u32 tegra_dp_pe_regs[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x00, 0x09, 0x13, 0x25}, /* voltage swing: L0 */
		{0x00, 0x0f, 0x1e}, /* L1 */
		{0x00, 0x14}, /* L2 */
		{0x00}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x00, 0x0a, 0x14, 0x28},
		{0x00, 0x0f, 0x1e},
		{0x00, 0x14},
		{0x00},
	},

	/* postcursor2 L2 */
	{
		{0x00, 0x0a, 0x14, 0x28},
		{0x00, 0x0f, 0x1e},
		{0x00, 0x14},
		{0x00},
	},

	/* postcursor2 L3 */
	{
		{0x00, 0x0a, 0x14, 0x28},
		{0x00, 0x0f, 0x1e},
		{0x00, 0x14},
		{0x00},
	},
};
#else
static const u32 tegra_dp_pe_regs[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x00, 0x08, 0x12, 0x24}, /* voltage swing: L0 */
		{0x01, 0x0e, 0x1d}, /* L1 */
		{0x01, 0x13}, /* L2 */
		{0x00}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x00, 0x08, 0x12, 0x24},
		{0x00, 0x0e, 0x1d},
		{0x00, 0x13},
		{0x00},
	},

	/* postcursor2 L2 */
	{
		{0x00, 0x08, 0x12, 0x24},
		{0x00, 0x0e, 0x1d},
		{0x00, 0x13},
		{0x00},
	},

	/* postcursor2 L3 */
	{
		{0x00, 0x08, 0x12, 0x24},
		{0x00, 0x0e, 0x1d},
		{0x00, 0x13},
		{0x00},
	},
};
#endif

static const u32 tegra_dp_pc_regs[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x00, 0x00, 0x00, 0x00}, /* voltage swing: L0 */
		{0x00, 0x00, 0x00}, /* L1 */
		{0x00, 0x00}, /* L2 */
		{0x00}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x02, 0x02, 0x04, 0x05},
		{0x02, 0x04, 0x05},
		{0x04, 0x05},
		{0x05},
	},

	/* postcursor2 L2 */
	{
		{0x04, 0x05, 0x08, 0x0b},
		{0x05, 0x09, 0x0b},
		{0x08, 0x0a},
		{0x0b},
	},

	/* postcursor2 L3 */
	{
		{0x05, 0x09, 0x0b, 0x12},
		{0x09, 0x0d, 0x12},
		{0x0b, 0x0f},
		{0x12},
	},
};

static const u32 tegra_dp_tx_pu[][4][4] = {
	/* postcursor2 L0 */
	{
		/* pre-emphasis: L0, L1, L2, L3 */
		{0x20, 0x30, 0x40, 0x60}, /* voltage swing: L0 */
		{0x30, 0x40, 0x60}, /* L1 */
		{0x40, 0x60}, /* L2 */
		{0x60}, /* L3 */
	},

	/* postcursor2 L1 */
	{
		{0x20, 0x20, 0x30, 0x50},
		{0x30, 0x40, 0x50},
		{0x40, 0x50},
		{0x60},
	},

	/* postcursor2 L2 */
	{
		{0x20, 0x20, 0x30, 0x40},
		{0x30, 0x30, 0x40},
		{0x40, 0x50},
		{0x60},
	},

	/* postcursor2 L3 */
	{
		{0x20, 0x20, 0x20, 0x40},
		{0x30, 0x30, 0x40},
		{0x40, 0x40},
		{0x60},
	},
};

static inline int tegra_dp_is_max_vs(u32 pe, u32 vs)
{
	return vs >= DRIVE_CURRENT_L3;
}

static inline int tegra_dp_is_max_pe(u32 pe, u32 vs)
{
	return pe >= PRE_EMPHASIS_L3;
}

static inline int tegra_dp_is_max_pc(u32 pc)
{
	return pc >= POST_CURSOR2_L3;
}

void tegra_dp_lt_init(struct tegra_dp_lt_data *lt_data,
			struct tegra_dc_dp_data *dp);
void tegra_dp_lt_set_pending_evt(struct tegra_dp_lt_data *lt_data);
void tegra_dp_lt_force_disable(struct tegra_dp_lt_data *lt_data);
long tegra_dp_lt_wait_for_completion(struct tegra_dp_lt_data *lt_data,
						unsigned long timeout_ms);
int tegra_dp_get_lt_state(struct tegra_dp_lt_data *lt_data);
bool tegra_dp_get_lt_status(struct tegra_dp_lt_data *lt_data);
#endif
