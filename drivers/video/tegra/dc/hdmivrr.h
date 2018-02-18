/*
 * drivers/video/tegra/dc/hdmivrr.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_HDMIVRR_H
#define __DRIVERS_VIDEO_TEGRA_DC_HDMIVRR_H

#define VCP_MAGIC0			0xe0
#define VCP_MAGIC1			0xe1
#define VCP_AUX_STAT			0xe2
#define VCP_AUX_STAT_IDLE		0x00
#define VCP_AUX_STAT_RD			0x01
#define VCP_AUX_STAT_WR			0x02
#define VCP_AUX_ADDR_H			0xe3
#define VCP_AUX_ADDR_L			0xe4
#define VCP_AUX_LENGTH			0xe5
#define VCP_AUX_BUF			0xe6

#define VCP_TEMP			0xf9
#define VCP_BOARDID			0xfa
#define VCP_DP_STATUS			0xfb
#define VCP_GFX_VER			0xfc
#define VCP_PANEL_VER			0xfd
#define VCP_NV_FW_VER			0xfe
#define NV_FW_MIN_VER			0x400
#define VCP_ERROR			0xff

#define SET_VCP_LEN			0x7
#define SET_VCP_VCP_OFF			0x3
#define SET_VCP_SH_OFF			0x4
#define SET_VCP_SL_OFF			0x5

#define GET_VCP_WR_LEN			0x5
#define GET_VCP_WR_VCP_OFF		0x3
#define GET_VCP_RD_LEN			0xb
#define GET_VCP_RES_CODE_OFF		0x3
#define GET_VCP_RES_CODE_NO_ERR		0x0
#define GET_VCP_RES_CODE_UN_SUP		0x1
#define GET_VCP_SH_OFF			0x8
#define GET_VCP_SL_OFF			0x9

#define TABLE_WRITE_LEN			0x17
#define TABLE_WRITE_HEADER_LEN		0x6
#define TABLE_WRITE_LEN_OFF		0x1
#define TABLE_WRITE_VCP_OFF		0x3
#define TABLE_WRITE_DATA_OFF		0x6

#define TABLE_READ_WR_LEN		0x7
#define TABLE_READ_WR_VCP_OFF		0x3
#define TABLE_READ_RD_LEN		0x16
#define TABLE_READ_RD_HEADER_LEN	0x5
#define TABLE_READ_RD_DATA_OFF		0x5

#define DPAUX_SOURCE_OUI		0x300
#define DPAUX_AUTH_MAGIC		0x310
#define AUTH_MAGIC_NUM			0x56525200
#define DPAUX_AUTH_PROTOCOL		0x314
#define AUTH_PROTOCOL_VALID		0x1
#define DPAUX_AUTH_KEYNUM		0x315
#define AUTH_KEYNUM_VALUE		0x0b
#define DPAUX_SERIALNUM			0x316
#define DPAUX_AUTH_CMD			0x320
#define AUTH_CMD_RESET			0x0
#define AUTH_CMD_MONAUTH		0x1
#define AUTH_CMD_DRVAUTH		0x2
#define DPAUX_AUTH_STATUS		0x321
#define AUTH_STATUS_READY		0xff
#define AUTH_STATUS_BUSY		0x01
#define DPAUX_LOCK_STATUS		0x322
#define LOCK_STATUS_LOCKED		0x00
#define LOCK_STATUS_UNLOCKED		0x01
#define DPAUX_AUTH_CHALLENGE1		0x3c0
#define DPAUX_AUTH_CHALLENGE2		0x3d0
#define DPAUX_AUTH_DIGEST1		0x3e0
#define DPAUX_AUTH_DIGEST2		0x3f0

#ifdef CONFIG_TEGRA_HDMIVRR
int tegra_hdmivrr_setup(struct tegra_hdmi *hdmi);
void tegra_hdmivrr_update_monspecs(struct tegra_dc *dc,
	struct list_head *head);
int tegra_hdmi_vrr_init(struct tegra_hdmi *hdmi);
void te_authenticate_vrr(u8 *buf_ptr, u32 buflen);
void _tegra_hdmivrr_activate(struct tegra_hdmi *hdmi, bool activate);
#else
static inline int tegra_hdmivrr_setup(struct tegra_hdmi *hdmi)
{ return -EPROTONOSUPPORT; }
static inline void tegra_hdmivrr_update_monspecs(struct tegra_dc *dc,
	struct list_head *head) { return; }
static inline int tegra_hdmi_vrr_init(struct tegra_hdmi *hdmi)
{ return -EPROTONOSUPPORT; }
static inline void _tegra_hdmivrr_activate(struct tegra_hdmi *hdmi,
	bool activate)
{ return; }
#endif

#endif
