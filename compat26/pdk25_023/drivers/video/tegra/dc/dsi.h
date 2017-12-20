/*
  * drivers/video/tegra/dc/dsi.h
  *
  * Copyright (c) 2011, NVIDIA Corporation.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_DSI_H__
#define __DRIVERS_VIDEO_TEGRA_DC_DSI_H__

/* source of video data */
enum{
	TEGRA_DSI_VIDEO_DRIVEN_BY_DC,
	TEGRA_DSI_VIDEO_DRIVEN_BY_HOST,
};

/* Max number of data lanes supported */
#define MAX_DSI_DATA_LANES	2
/* Default Peripheral reset timeout */
#define DSI_PR_TO_VALUE		0x2000

/* DCS commands for command mode */
#define DSI_ENTER_PARTIAL_MODE	0x12
#define DSI_SET_PIXEL_FORMAT	0x3A
#define DSI_AREA_COLOR_MODE 	0x4C
#define DSI_SET_PARTIAL_AREA	0x30
#define DSI_SET_PAGE_ADDRESS	0x2B
#define DSI_SET_ADDRESS_MODE	0x36
#define DSI_SET_COLUMN_ADDRESS	0x2A
#define DSI_WRITE_MEMORY_START	0x2C
#define DSI_WRITE_MEMORY_CONTINUE	0x3C
#define DSI_MAX_COMMAND_DELAY_USEC	250000
#define DSI_COMMAND_DELAY_STEPS_USEC	10

/* DSI return packet types */
#define GEN_LONG_RD_RES 0x1A
#define DCS_LONG_RD_RES 0x1C
#define GEN_1_BYTE_SHORT_RD_RES 0x11
#define DCS_1_BYTE_SHORT_RD_RES 0x21
#define GEN_2_BYTE_SHORT_RD_RES 0x12
#define DCS_2_BYTE_SHORT_RD_RES 0x22
#define ACK_ERR_RES 0x02

/* End of Transmit command for HS mode */
#define DSI_CMD_HS_EOT_PACKAGE          0x000F0F08

/* Delay required after issuing the trigger*/
#define DSI_COMMAND_COMPLETION_DELAY_USEC   5

#define DSI_DELAY_FOR_READ_FIFO 5

/* Dsi virtual channel bit position, refer to the DSI specs */
#define DSI_VIR_CHANNEL_BIT_POSITION	6

/* DSI packet commands from Host to peripherals */
enum {
	dsi_command_v_sync_start = 0x01,
	dsi_command_v_sync_end = 0x11,
	dsi_command_h_sync_start = 0x21,
	dsi_command_h_sync_end = 0x31,
	dsi_command_end_of_transaction = 0x08,
	dsi_command_blanking = 0x19,
	dsi_command_null_packet = 0x09,
	dsi_command_h_active_length_16bpp = 0x0E,
	dsi_command_h_active_length_18bpp = 0x1E,
	dsi_command_h_active_length_18bpp_np = 0x2E,
	dsi_command_h_active_length_24bpp = 0x3E,
	dsi_command_h_sync_active = dsi_command_blanking,
	dsi_command_h_back_porch = dsi_command_blanking,
	dsi_command_h_front_porch = dsi_command_blanking,
	dsi_command_writ_no_param = 0x05,
	dsi_command_long_write = 0x39,
	dsi_command_max_return_pkt_size = 0x37,
	dsi_command_generic_read_request_with_2_param = 0x24,
	dsi_command_dcs_read_with_no_params = 0x06,
};

/* Maximum polling time for reading the dsi status register */
#define DSI_STATUS_POLLING_DURATION_USEC    100000
#define DSI_STATUS_POLLING_DELAY_USEC       100

/*
  * Horizontal Sync Blank Packet Over head
  * DSI_overhead = size_of(HS packet header)
  *             + size_of(BLANK packet header) + size_of(checksum)
  * DSI_overhead = 4 + 4 + 2 = 10
  */
#define DSI_HSYNC_BLNK_PKT_OVERHEAD  10

/*
 * Horizontal Front Porch Packet Overhead
 * DSI_overhead = size_of(checksum)
 *            + size_of(BLANK packet header) + size_of(checksum)
 * DSI_overhead = 2 + 4 + 2 = 8
 */
#define DSI_HFRONT_PORCH_PKT_OVERHEAD 8

/*
 * Horizontal Back Porch Packet
 * DSI_overhead = size_of(HE packet header)
 *            + size_of(BLANK packet header) + size_of(checksum)
 *            + size_of(RGB packet header)
 * DSI_overhead = 4 + 4 + 2 + 4 = 14
 */
#define DSI_HBACK_PORCH_PKT_OVERHEAD  14

/* Additional Hs TX timeout margin */
#define DSI_HTX_TO_MARGIN   720

#define DSI_CYCLE_COUNTER_VALUE     512

#define DSI_LRXH_TO_VALUE   0x2000

/* Turn around timeout terminal count */
#define DSI_TA_TO_VALUE     0x2000

/* Turn around timeout tally */
#define DSI_TA_TALLY_VALUE      0x0
/* LP Rx timeout tally */
#define DSI_LRXH_TALLY_VALUE    0x0
/* HS Tx Timeout tally */
#define DSI_HTX_TALLY_VALUE     0x0

/* DSI Power control settle time 10 micro seconds */
#define DSI_POWER_CONTROL_SETTLE_TIME_US    10

#define DSI_HOST_FIFO_DEPTH     64
#define DSI_VIDEO_FIFO_DEPTH    480
#define DSI_READ_FIFO_DEPTH	(32 << 2)

#define NUMOF_BIT_PER_BYTE			8
#define DEFAULT_LP_CMD_MODE_CLK_KHZ		10000
#define DEFAULT_MAX_DSI_PHY_CLK_KHZ		(500*1000)
#define DEFAULT_PANEL_RESET_TIMEOUT		2
#define DEFAULT_PANEL_BUFFER_BYTE		512

/*
 * TODO: are DSI_HOST_DSI_CONTROL_CRC_RESET(RESET_CRC) and
 * DSI_HOST_DSI_CONTROL_HOST_TX_TRIG_SRC(IMMEDIATE) required for everyone?
 */
#define HOST_DSI_CTRL_COMMON 	\
			(DSI_HOST_DSI_CONTROL_PHY_CLK_DIV(DSI_PHY_CLK_DIV1) | \
			DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(NORMAL) | \
			DSI_HOST_DSI_CONTROL_PERIPH_RESET(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_RAW_DATA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_PKT_BTA(TEGRA_DSI_DISABLE) | \
			DSI_HOST_DSI_CONTROL_CS_ENABLE(TEGRA_DSI_ENABLE) | \
			DSI_HOST_DSI_CONTROL_ECC_ENABLE(TEGRA_DSI_ENABLE) | \
			DSI_HOST_DSI_CONTROL_PKT_WR_FIFO_SEL(HOST_ONLY))

#define HOST_DSI_CTRL_HOST_DRIVEN \
			(DSI_HOST_DSI_CONTROL_CRC_RESET(RESET_CRC) | \
			DSI_HOST_DSI_CONTROL_HOST_TX_TRIG_SRC(IMMEDIATE))

#define HOST_DSI_CTRL_DC_DRIVEN 0

#define DSI_CTRL_HOST_DRIVEN	(DSI_CONTROL_VID_ENABLE(TEGRA_DSI_DISABLE) | \
				DSI_CONTROL_HOST_ENABLE(TEGRA_DSI_ENABLE))

#define DSI_CTRL_DC_DRIVEN 	(DSI_CONTROL_VID_TX_TRIG_SRC(SOL) | \
				DSI_CONTROL_VID_ENABLE(TEGRA_DSI_ENABLE) | \
				DSI_CONTROL_HOST_ENABLE(TEGRA_DSI_DISABLE))

#define DSI_CTRL_CMD_MODE 	(DSI_CONTROL_VID_DCS_ENABLE(TEGRA_DSI_ENABLE))

#define DSI_CTRL_VIDEO_MODE 	(DSI_CONTROL_VID_DCS_ENABLE(TEGRA_DSI_DISABLE))


enum {
	CMD_VS 	= 0x01,
	CMD_VE		= 0x11,

	CMD_HS		= 0x21,
	CMD_HE		= 0x31,

	CMD_EOT 	= 0x08,
	CMD_NULL 	= 0x09,
	CMD_SHORTW      = 0x15,
	CMD_BLNK 	= 0x19,
	CMD_LONGW	= 0x39,

	CMD_RGB	= 0x00,
	CMD_RGB_16BPP	= 0x0E,
	CMD_RGB_18BPP	= 0x1E,
	CMD_RGB_18BPPNP = 0x2E,
	CMD_RGB_24BPP	= 0x3E,
};

#define PKT_ID0(id)		DSI_PKT_SEQ_0_LO_PKT_00_ID(id) | \
				DSI_PKT_SEQ_1_LO_PKT_10_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN0(len)	DSI_PKT_SEQ_0_LO_PKT_00_SIZE(len)

#define PKT_ID1(id)		DSI_PKT_SEQ_0_LO_PKT_01_ID(id) | \
				DSI_PKT_SEQ_1_LO_PKT_11_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN1(len)	DSI_PKT_SEQ_0_LO_PKT_01_SIZE(len)

#define PKT_ID2(id)		DSI_PKT_SEQ_0_LO_PKT_02_ID(id) | \
				DSI_PKT_SEQ_1_LO_PKT_12_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN2(len)	DSI_PKT_SEQ_0_LO_PKT_02_SIZE(len)

#define PKT_ID3(id)		DSI_PKT_SEQ_0_HI_PKT_03_ID(id) | \
				DSI_PKT_SEQ_1_HI_PKT_13_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN3(len)	DSI_PKT_SEQ_0_HI_PKT_03_SIZE(len)

#define PKT_ID4(id)		DSI_PKT_SEQ_0_HI_PKT_04_ID(id) | \
				DSI_PKT_SEQ_1_HI_PKT_14_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN4(len)	DSI_PKT_SEQ_0_HI_PKT_04_SIZE(len)

#define PKT_ID5(id)		DSI_PKT_SEQ_0_HI_PKT_05_ID(id) | \
				DSI_PKT_SEQ_1_HI_PKT_15_EN(TEGRA_DSI_ENABLE)
#define PKT_LEN5(len)	DSI_PKT_SEQ_0_HI_PKT_05_SIZE(len)

#define PKT_LP		DSI_PKT_SEQ_0_LO_SEQ_0_FORCE_LP(TEGRA_DSI_ENABLE)

#define NUMOF_PKT_SEQ	12


/* Macros for calculating the phy timings */
#define T_HSEXIT_DEFAULT(clkns)		(100 / ((clkns) * 8) + 1)
#define T_HSTRAIL_DEFAULT(clkns)	(3 + max((8 * (clkns)), \
					(60 + 4 * (clkns))) / ((clkns) * 8) + 1)
#define T_HSPREPR_ORG(clkns)		((65 + 5 * (clkns)) / ((clkns) * 8))
#define T_HSPREPR_DEFAULT(clkns)	((T_HSPREPR_ORG(clkns) == 0) ? \
					         1 : T_HSPREPR_ORG(clkns))
#define T_DATZERO_DEFAULT(clkns)	((145 + 5 * (clkns)) / ((clkns) * 8) +1)

#define T_CLKTRAIL_DEFAULT(clkns)	(60 / ((clkns) * 8) + 1)
#define T_CLKPOST_DEFAULT(clkns)	((60 + 52 * (clkns)) / ((clkns) * 8) +1)
#define T_CLKZERO_DEFAULT(clkns)	(170 / ((clkns) * 8) + 1)
#define T_TLPX_ORG(clkns)		(50 / ((clkns) * 8) + 1)
#define T_TLPX_DEFAULT(clkns)		((T_TLPX_ORG(clkns) == 0) ? \
							1 : T_TLPX_ORG(clkns))

#define T_CLKPRE_DEFAULT(clkns)		1
#define T_CLKPREPARE_DEFAULT(clkns)	4

/* Minimum ULPM wakeup time as per the spec is 1msec */
#define T_WAKEUP_DEFAULT(clkns)		(2*1000*1000 / (clkns))

#define DSI_CYCLE_COUNTER_VALUE		512

/* Defines the DSI phy timing parameters */
struct dsi_phy_timing_inclk
{
	unsigned	t_hsdexit;
	unsigned	t_hstrail;
	unsigned	t_hsprepr;
	unsigned	t_datzero;

	unsigned 	t_clktrail;
	unsigned 	t_clkpost;
	unsigned 	t_clkzero;
	unsigned 	t_tlpx;

	unsigned	t_clkpre;
        unsigned        t_clkprepare;
	unsigned	t_wakeup;

	unsigned	t_taget;
	unsigned	t_tasure;
	unsigned	t_tago;

};

#endif
