/*
 * drivers/video/tegra/dc/dp.h
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_DP_H__
#define __DRIVER_VIDEO_TEGRA_DC_DP_H__

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/tegra_prod.h>

#include "sor.h"
#include "dc_priv.h"
#include "dpaux_regs.h"
#include "hpd.h"
#include "../../../../arch/arm/mach-tegra/iomap.h"
#include "dp_lt.h"

#ifdef CONFIG_DEBUG_FS
#include "dp_debug.h"
extern struct tegra_dp_test_settings default_dp_test_settings;
#endif

#define DP_AUX_MAX_BYTES 16
#define DP_AUX_TIMEOUT_MS 1000
#define DP_DPCP_RETRY_SLEEP_NS 400
#define TEGRA_NVHDCP_MAX_DEVS 127
#define DP_AUX_DEFER_MAX_TRIES 7
#define DP_AUX_TIMEOUT_MAX_TRIES 2
#define DP_POWER_ON_MAX_TRIES 3
#define DP_CLOCK_RECOVERY_MAX_TRIES 7
#define DP_CLOCK_RECOVERY_TOT_TRIES 15

/*
 * After hpd irq event, source must start
 * reading dpdc offset 200h-205h within
 * 100ms of rising edge hpd
 */
#define HPD_IRQ_EVENT_TIMEOUT_MS 70

/* the +10ms is the time for power rail going up from 10-90% or
   90%-10% on powerdown */
/* Time from power-rail is turned on and aux/12c-over-aux is available */
#define EDP_PWR_ON_TO_AUX_TIME_MS	    (200+10)
/* Time from power-rail is turned on and MainLink is available for LT */
#define EDP_PWR_ON_TO_ML_TIME_MS	    (200+10)
/* Time from turning off power to turn-it on again (does not include post
   poweron time) */
#define EDP_PWR_OFF_TO_ON_TIME_MS	    (500+10)

/*
 * Receiver capability fields extend from 0 - 0x11fh.
 * By default we read only more useful fields(offsets 0 - 0xb) as
 * required by CTS.
 */
#define DP_DPCD_SINK_CAP_SIZE (0xc)

struct tegra_dc_dp_data {
	struct tegra_dc *dc;
	struct tegra_dc_sor_data *sor;

	u32 irq;

	struct resource *res;
	struct resource *aux_base_res;
	void __iomem *aux_base;
	struct clk *dpaux_clk;
	struct clk *parent_clk; /* pll_dp clock */
	struct reset_control *dpaux_rst;

	u8 revision;

	struct tegra_dc_mode *mode;
	struct tegra_dc_dp_link_config link_cfg;
	struct tegra_dc_dp_link_config max_link_cfg;

	struct tegra_dp_lt_data lt_data;

	bool enabled; /* Controller ready. LT not yet initiated. */
	bool suspended;

	u8 edid_src;
	struct tegra_hpd_data hpd_data;
#ifdef CONFIG_SWITCH
	struct switch_dev audio_switch;
#endif

	struct delayed_work irq_evt_dwork;

	struct tegra_dphdcp *dphdcp;

	struct completion aux_tx;
	struct completion hpd_plug;

	struct tegra_dp_out *pdata;

	struct mutex dpaux_lock;

	struct tegra_prod *prod_list;

	struct tegra_prod *dpaux_prod_list;

	u8 sink_cap[DP_DPCD_SINK_CAP_SIZE];
	bool sink_cap_valid;
	u8 sink_cnt_cp_ready;

	u16 dpaux_i2c_dbg_addr;
	u32 dpaux_i2c_dbg_num_bytes;

	const char *debug_dir_name;

#ifdef CONFIG_DEBUG_FS
	struct tegra_dp_test_settings test_settings;
};

extern const struct file_operations test_settings_fops;
#else
};
#endif

enum {
	VSC_RGB = 0,
	VSC_YUV444 = 1,
	VSC_YUV422 = 2,
	VSC_YUV420 = 3,
	VSC_YONLY = 4,
	VSC_RAW = 5,
};

enum {
	VSC_VESA_RANGE = 0,
	VSC_CEA_RANGE = 1,
};

enum {
	VSC_6BPC = 0,
	VSC_8BPC = 1,
	VSC_10BPC = 2,
	VSC_12BPC = 3,
	VSC_16BPC = 4,
};

int tegra_dp_dpcd_write_field(struct tegra_dc_dp_data *dp, u32 cmd,
	u8 mask, u8 data);
int tegra_dc_dpaux_read(struct tegra_dc_dp_data *dp, u32 cmd, u32 addr,
	u8 *data, u32 *size, u32 *aux_stat);
int tegra_dc_dpaux_write(struct tegra_dc_dp_data *dp, u32 cmd, u32 addr,
	u8 *data, u32 *size, u32 *aux_stat);
void tegra_dc_dp_pre_disable_link(struct tegra_dc_dp_data *dp);
void tegra_dc_dp_disable_link(struct tegra_dc_dp_data *dp, bool powerdown);
void tegra_dc_dp_enable_link(struct tegra_dc_dp_data *dp);

int tegra_dc_dpaux_read_chunk_locked(struct tegra_dc_dp_data *dp,
	u32 cmd, u32 addr, u8 *data, u32 *size, u32 *aux_stat);
int tegra_dc_dpaux_write_chunk_locked(struct tegra_dc_dp_data *dp,
	u32 cmd, u32 addr, u8 *data, u32 *size, u32 *aux_stat);
void tegra_dp_update_link_config(struct tegra_dc_dp_data *dp);
int tegra_dc_dp_dpcd_read(struct tegra_dc_dp_data *dp, u32 cmd, u8 *data_ptr);
int tegra_dc_dp_dpcd_write(struct tegra_dc_dp_data *dp, u32 cmd, u8 data);
void tegra_dp_tpg(struct tegra_dc_dp_data *dp, u32 tp, u32 n_lanes);
bool tegra_dc_dp_calc_config(struct tegra_dc_dp_data *dp,
				const struct tegra_dc_mode *mode,
				struct tegra_dc_dp_link_config *cfg);

/* DPCD definitions */
#define NV_DPCD_REV					(0x00000000)
#define NV_DPCD_REV_MAJOR_SHIFT				(4)
#define NV_DPCD_REV_MAJOR_MASK				(0xf << 4)
#define NV_DPCD_REV_MINOR_SHIFT				(0)
#define NV_DPCD_REV_MINOR_MASK				(0xf)
#define NV_DPCD_MAX_LINK_BANDWIDTH			(0x00000001)
#define NV_DPCD_MAX_LINK_BANDWIDTH_VAL_1_62_GPBS	(0x00000006)
#define NV_DPCD_MAX_LINK_BANDWIDTH_VAL_2_70_GPBS	(0x0000000a)
#define NV_DPCD_MAX_LINK_BANDWIDTH_VAL_5_40_GPBS	(0x00000014)
#define NV_DPCD_MAX_LANE_COUNT				(0x00000002)
#define NV_DPCD_MAX_LANE_COUNT_MASK			(0x1f)
#define NV_DPCD_MAX_LANE_COUNT_LANE_1			(0x00000001)
#define NV_DPCD_MAX_LANE_COUNT_LANE_2			(0x00000002)
#define NV_DPCD_MAX_LANE_COUNT_LANE_4			(0x00000004)
#define NV_DPCD_MAX_LANE_COUNT_TPS3_SUPPORTED_YES	(0x00000001 << 6)
#define NV_DPCD_MAX_LANE_COUNT_ENHANCED_FRAMING_NO	(0x00000000 << 7)
#define NV_DPCD_MAX_LANE_COUNT_ENHANCED_FRAMING_YES	(0x00000001 << 7)
#define NV_DPCD_MAX_DOWNSPREAD				(0x00000003)
#define NV_DPCD_MAX_DOWNSPREAD_VAL_NONE			(0x00000000)
#define NV_DPCD_MAX_DOWNSPREAD_VAL_0_5_PCT		(0x00000001)
#define NV_DPCD_MAX_DOWNSPREAD_NO_AUX_HANDSHAKE_LT_F	(0x00000000 << 6)
#define NV_DPCD_MAX_DOWNSPREAD_NO_AUX_HANDSHAKE_LT_T	(0x00000001 << 6)
#define NV_DPCD_EDP_CONFIG_CAP				(0x0000000D)
#define NV_DPCD_EDP_CONFIG_CAP_ASC_RESET_NO		(0x00000000)
#define NV_DPCD_EDP_CONFIG_CAP_ASC_RESET_YES		(0x00000001)
#define NV_DPCD_EDP_CONFIG_CAP_FRAMING_CHANGE_NO	(0x00000000 << 1)
#define NV_DPCD_EDP_CONFIG_CAP_FRAMING_CHANGE_YES	(0x00000001 << 1)
#define NV_DPCD_EDP_CONFIG_CAP_DISPLAY_CONTROL_CAP_YES	(0x00000001 << 3)
#define NV_DPCD_TRAINING_AUX_RD_INTERVAL		(0x0000000E)
#define NV_DPCD_LINK_BANDWIDTH_SET			(0x00000100)
#define NV_DPCD_LANE_COUNT_SET				(0x00000101)
#define NV_DPCD_LANE_COUNT_SET_MASK			(0x1f)
#define NV_DPCD_LANE_COUNT_SET_ENHANCEDFRAMING_F	(0x00000000 << 7)
#define NV_DPCD_LANE_COUNT_SET_ENHANCEDFRAMING_T	(0x00000001 << 7)
#define NV_DPCD_TRAINING_PATTERN_SET			(0x00000102)
#define NV_DPCD_TRAINING_PATTERN_SET_TPS_MASK		0x3
#define NV_DPCD_TRAINING_PATTERN_SET_TPS_NONE		(0x00000000)
#define NV_DPCD_TRAINING_PATTERN_SET_TPS_TP1		(0x00000001)
#define NV_DPCD_TRAINING_PATTERN_SET_TPS_TP2		(0x00000002)
#define NV_DPCD_TRAINING_PATTERN_SET_TPS_TP3		(0x00000003)
#define NV_DPCD_TRAINING_PATTERN_SET_SC_DISABLED_F	(0x00000000 << 5)
#define NV_DPCD_TRAINING_PATTERN_SET_SC_DISABLED_T	(0x00000001 << 5)
#define NV_DPCD_TRAINING_LANE0_SET			(0x00000103)
#define NV_DPCD_TRAINING_LANE1_SET			(0x00000104)
#define NV_DPCD_TRAINING_LANE2_SET			(0x00000105)
#define NV_DPCD_TRAINING_LANE3_SET			(0x00000106)
#define NV_DPCD_TRAINING_LANEX_SET_DC_SHIFT		0
#define NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_T	(0x00000001 << 2)
#define NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_F	(0x00000000 << 2)
#define NV_DPCD_TRAINING_LANEX_SET_PE_SHIFT		3
#define NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_T	(0x00000001 << 5)
#define NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_F	(0x00000000 << 5)
#define NV_DPCD_DOWNSPREAD_CTRL				(0x00000107)
#define NV_DPCD_DOWNSPREAD_CTRL_SPREAD_AMP_NONE		(0x00000000 << 4)
#define NV_DPCD_DOWNSPREAD_CTRL_SPREAD_AMP_LT_0_5	(0x00000001 << 4)
#define NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET		(0x00000108)
#define NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET_ANSI_8B10B	1
#define NV_DPCD_EDP_CONFIG_SET				(0x0000010A)
#define NV_DPCD_EDP_CONFIG_SET_ASC_RESET_DISABLE	(0x00000000)
#define NV_DPCD_EDP_CONFIG_SET_ASC_RESET_ENABLE		(0x00000001)
#define NV_DPCD_EDP_CONFIG_SET_FRAMING_CHANGE_DISABLE	(0x00000000 << 1)
#define NV_DPCD_EDP_CONFIG_SET_FRAMING_CHANGE_ENABLE	(0x00000001 << 1)
#define NV_DPCD_TRAINING_LANE0_1_SET2			(0x0000010F)
#define NV_DPCD_TRAINING_LANE2_3_SET2			(0x00000110)
#define NV_DPCD_LANEX_SET2_PC2_SHIFT			0
#define NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_T		(0x00000001 << 2)
#define NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_F		(0x00000000 << 2)
#define NV_DPCD_LANEXPLUS1_SET2_PC2_SHIFT		4
#define NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_T	(0x00000001 << 6)
#define NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_F	(0x00000000 << 6)
#define NV_DPCD_SINK_COUNT				(0x00000200)
#define NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR		(0x00000201)
#define NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_NO	(0x00000000 << 1)
#define NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_YES	(0x00000001 << 1)
#define NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_CP_NO		(0x00000000 << 2)
#define NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_CP_YES	(0x00000001 << 2)
#define NV_DPCD_LANE0_1_STATUS				(0x00000202)
#define NV_DPCD_LANE2_3_STATUS				(0x00000203)
#define NV_DPCD_STATUS_LANEX_CR_DONE_SHIFT		0
#define NV_DPCD_STATUS_LANEX_CR_DONE_NO			(0x00000000)
#define NV_DPCD_STATUS_LANEX_CR_DONE_YES		(0x00000001)
#define NV_DPCD_STATUS_LANEX_CHN_EQ_DONE_SHIFT		1
#define NV_DPCD_STATUS_LANEX_CHN_EQ_DONE_NO		(0x00000000 << 1)
#define NV_DPCD_STATUS_LANEX_CHN_EQ_DONE_YES		(0x00000001 << 1)
#define NV_DPCD_STATUS_LANEX_SYMBOL_LOCKED_SHFIT	2
#define NV_DPCD_STATUS_LANEX_SYMBOL_LOCKED_NO		(0x00000000 << 2)
#define NV_DPCD_STATUS_LANEX_SYMBOL_LOCKED_YES		(0x00000001 << 2)
#define NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_SHIFT		4
#define NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_NO		(0x00000000 << 4)
#define NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_YES		(0x00000001 << 4)
#define NV_DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_SHIFT	5
#define NV_DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_NO	(0x00000000 << 5)
#define NV_DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_YES	(0x00000001 << 5)
#define NV_DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_SHIFT	6
#define NV_DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_NO	(0x00000000 << 6)
#define NV_DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_YES	(0x00000001 << 6)
#define NV_DPCD_LANE_ALIGN_STATUS_UPDATED		(0x00000204)
#define NV_DPCD_LANE_ALIGN_STATUS_INTERLANE_ALIGN_DONE_NO	(0x00000000)
#define NV_DPCD_LANE_ALIGN_STATUS_INTERLANE_ALIGN_DONE_YES	(0x00000001)
#define NV_DPCD_LANE0_1_ADJUST_REQ			(0x00000206)
#define NV_DPCD_LANE2_3_ADJUST_REQ			(0x00000207)
#define NV_DPCD_ADJUST_REQ_LANEX_DC_SHIFT		0
#define NV_DPCD_ADJUST_REQ_LANEX_DC_MASK		0x3
#define NV_DPCD_ADJUST_REQ_LANEX_PE_SHIFT		2
#define NV_DPCD_ADJUST_REQ_LANEX_PE_MASK		(0x3 << 2)
#define NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_SHIFT		4
#define NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_MASK		(0x3 << 4)
#define NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_SHIFT		6
#define NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_MASK		(0x3 << 6)
#define NV_DPCD_ADJUST_REQ_POST_CURSOR2			(0x0000020C)
#define NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_MASK	0x3
#define NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_SHIFT(i)	(i*2)
#define NV_DPCD_TEST_REQUEST				(0x00000218)
#define NV_DPCD_TEST_REQUEST_TEST_LT			(1 << 0)
#define NV_DPCD_TEST_REQUEST_TEST_PATTERN		(1 << 1)
#define NV_DPCD_TEST_REQUEST_TEST_EDID_READ		(1 << 2)
#define NV_DPCD_TEST_RESPONSE				(0x00000260)
#define NV_DPCD_TEST_RESPONSE_ACK			(1 << 0)
#define NV_DPCD_TEST_RESPONSE_NACK			(1 << 1)
#define NV_DPCD_TEST_EDID_CHECKSUM_WR		(1 << 2)
#define NV_DPCD_TEST_EDID_CHECKSUM			(0x00000261)
#define NV_DPCD_SOURCE_IEEE_OUI				(0x00000300)
#define NV_IEEE_OUI					(0x00044b)
#define NV_DPCD_SINK_IEEE_OUI				(0x00000400)
#define NV_DPCD_BRANCH_IEEE_OUI				(0x00000500)
#define NV_DPCD_SET_POWER				(0x00000600)
#define NV_DPCD_SET_POWER_VAL_RESERVED			(0x00000000)
#define NV_DPCD_SET_POWER_VAL_D0_NORMAL			(0x00000001)
#define NV_DPCD_SET_POWER_VAL_D3_PWRDWN			(0x00000002)
#define NV_DPCD_FEATURE_ENUM_LIST			(0x00002210)
#define NV_DPCD_FEATURE_ENUM_LIST_VSC_EXT_COLORIMETRY	(1 << 3)
#define NV_DPCD_HDCP_BKSV_OFFSET			(0x00068000)
#define NV_DPCD_HDCP_RPRIME_OFFSET			(0x00068005)
#define NV_DPCD_HDCP_AKSV_OFFSET			(0x00068007)
#define NV_DPCD_HDCP_AN_OFFSET				(0x0006800C)
#define NV_DPCD_HDCP_VPRIME_OFFSET			(0x00068014)
#define NV_DPCD_HDCP_BCAPS_OFFSET			(0x00068028)
#define NV_DPCD_HDCP_BSTATUS_OFFSET			(0x00068029)
#define NV_DPCD_HDCP_BINFO_OFFSET			(0x0006802A)
#define NV_DPCD_HDCP_KSV_FIFO_OFFSET			(0x0006802C)
#define NV_DPCD_HDCP_AINFO_OFFSET			(0x0006803B)
#endif
