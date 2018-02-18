/*
 * drivers/video/tegra/dc/edid.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_DC_EDID_H
#define __DRIVERS_VIDEO_TEGRA_DC_EDID_H

#include <linux/i2c.h>
#include <linux/wait.h>
#include <mach/dc.h>

#define TEGRA_DC_Y420_30	1	/* YCbCr 4:2:0 deep color 30bpp */
#define TEGRA_DC_Y420_36	2	/* YCbCr 4:2:0 deep color 36bpp */
#define TEGRA_DC_Y420_48	4	/* YCbCr 4:2:0 deep color 48bpp */
#define TEGRA_DC_Y420_MASK	(TEGRA_DC_Y420_30 | \
				TEGRA_DC_Y420_36 | TEGRA_DC_Y420_48)

#define TEGRA_DC_Y422_30	8	/* YCbCr 4:2:2 deep color 30bpp */
#define TEGRA_DC_Y422_36	16	/* YCbCr 4:2:2 deep color 36bpp */
#define TEGRA_DC_Y422_48	32	/* YCbCr 4:2:2 deep color 48bpp */
#define TEGRA_DC_Y422_MASK	(TEGRA_DC_Y422_30 | \
				TEGRA_DC_Y422_36 | TEGRA_DC_Y422_48)

#define TEGRA_DC_Y444_30	64	/* YCbCr 4:4:4 deep color 30bpp */
#define TEGRA_DC_Y444_36	128	/* YCbCr 4:4:4 deep color 36bpp */
#define TEGRA_DC_Y444_48	256	/* YCbCr 4:4:4 deep color 48bpp */
#define TEGRA_DC_Y444_MASK	(TEGRA_DC_Y444_30 | \
				TEGRA_DC_Y444_36 | TEGRA_DC_Y444_48)

#define TEGRA_DC_RGB_30	512		/* RGB 4:4:4 deep color 30bpp */
#define TEGRA_DC_RGB_36	1024	/* RGB 4:4:4 deep color 36bpp */
#define TEGRA_DC_RGB_48	2048	/* RGB 4:4:4 deep color 48bpp */
#define TEGRA_DC_RGB_MASK	(TEGRA_DC_RGB_30 | \
				TEGRA_DC_RGB_36 | TEGRA_DC_RGB_48)

#define TEGRA_DC_MASK	(TEGRA_DC_Y420_MASK | \
				TEGRA_DC_Y422_MASK | TEGRA_DC_Y444_MASK | \
				TEGRA_DC_RGB_MASK)

#define TEGRA_EDID_MAX_RETRY 5
#define TEGRA_EDID_MIN_RETRY_DELAY_US 200
#define TEGRA_EDID_MAX_RETRY_DELAY_US (TEGRA_EDID_MIN_RETRY_DELAY_US + 200)

enum {
	CEA_DATA_BLOCK_RSVD0,
	CEA_DATA_BLOCK_AUDIO,
	CEA_DATA_BLOCK_VIDEO,
	CEA_DATA_BLOCK_VENDOR,
	CEA_DATA_BLOCK_SPEAKER_ALLOC,
	CEA_DATA_BLOCK_VESA_DISP_TRANS_CHAR,
	CEA_DATA_BLOCK_RSVD1,
	CEA_DATA_BLOCK_EXT,
	CEA_DATA_BLOCK_MAX_CNT,
};

enum {
	/* video blocks */
	CEA_DATA_BLOCK_EXT_VCDB = 0, /* video capability data block */
	CEA_DATA_BLOCK_EXT_VSVDB = 1, /* vendor specific video data block */
	CEA_DATA_BLOCK_EXT_VESA_DDDB = 2, /* VESA display device data block */
	CEA_DATA_BLOCK_EXT_VESA_VTBE = 3, /* VESA video timing block ext */
	CEA_DATA_BLOCK_EXT_HDMI_VDB = 4, /* rsvd for HDMI video data block */
	CEA_DATA_BLOCK_EXT_CDB = 5, /* colorimetry data block */
	CEA_DATA_BLOCK_EXT_HDR = 6, /* HDR data block */
	/* 7-12 rsvd for other video related blocks */
	CEA_DATA_BLOCK_EXT_VFPDB = 13, /* video format preference data block */
	CEA_DATA_BLOCK_EXT_Y420VDB = 14, /* YCbCr 4:2:0 video data block */
	CEA_DATA_BLOCK_EXT_Y420CMDB = 15, /* YCbCr 4:2:0 cap map data block */

	/* audio blocks */
	CEA_DATA_BLOCK_EXT_CEA_MAF = 16, /* rsvd CEA misc audio fields */
	CEA_DATA_BLOCK_EXT_VSADB = 17, /* vendor specific audio data block */
	CEA_DATA_BLOCK_EXT_HDMI_ADB = 18, /* rsvd HDMI audio data block */
	/* 19-31 rsvd for other audio related blocks */

	CEA_DATA_BLOCK_EXT_IDB = 32, /* infoframe data block */
	/* 33-255 rsvd */
};

#define ELD_MAX_MNL	16
#define ELD_MAX_SAD	16
#define ELD_MAX_SAD_BYTES (ELD_MAX_SAD * 3)

struct tegra_edid_pvt;

#define EDID_BASE_HEADER_SIZE 8
static const unsigned char edid_base_header[EDID_BASE_HEADER_SIZE] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
};

typedef int (*i2c_transfer_func_t)(struct tegra_dc *dc, struct i2c_msg *msgs,
	int num);

struct tegra_dc_i2c_ops {
	i2c_transfer_func_t i2c_transfer;
};

enum {
	EDID_SRC_PANEL,
	EDID_SRC_DT,
};

/* Flag panel edid checksum is corrupted.
 * SW fixes checksum before passing on the
 * edid block to parser. For now just represent checksum
 * corruption on any of the edid blocks.
 */
#define EDID_ERRORS_CHECKSUM_CORRUPTED	0x01

/* Flag edid read failed after all retries. */
#define EDID_ERRORS_READ_FAILED		0x02

/* Flag fallback edid is in use. */
#define EDID_ERRORS_USING_FALLBACK	0x04

#define TEGRA_EDID_QUIRK_NONE      (0)
/* TV doesn't support YUV420, but declares support */
#define TEGRA_EDID_QUIRK_NO_YUV (1 << 0)

struct tegra_edid {
	struct tegra_edid_pvt	*data;

	struct mutex		lock;
	struct tegra_dc_i2c_ops i2c_ops;
	struct tegra_dc		*dc;

	/* Bitmap to flag EDID reading / parsing error conditions. */
	u8 errors;
};

/*
 * ELD: EDID Like Data
 */
struct tegra_edid_hdmi_eld {
	u8	baseline_len;
	u8	eld_ver;
	u8	cea_edid_ver;
	char	monitor_name[ELD_MAX_MNL + 1];
	u8	mnl;
	u8	manufacture_id[2];
	u8	product_id[2];
	u8	port_id[8];
	u8	support_hdcp;
	u8	support_ai;
	u8	conn_type;
	u8	aud_synch_delay;
	u8	spk_alloc;
	u8	sad_count;
	u8	sad[ELD_MAX_SAD_BYTES];
};

struct tegra_edid *tegra_edid_create(struct tegra_dc *dc,
	i2c_transfer_func_t func);
void tegra_edid_destroy(struct tegra_edid *edid);
int tegra_edid_get_monspecs(struct tegra_edid *edid,
				struct fb_monspecs *specs);
u16 tegra_edid_get_cd_flag(struct tegra_edid *edid);
u16 tegra_edid_get_ex_hdr_cap(struct tegra_edid *edid);
u16 tegra_edid_get_quant_cap(struct tegra_edid *edid);
u16 tegra_edid_get_max_clk_rate(struct tegra_edid *edid);
bool tegra_edid_is_scdc_present(struct tegra_edid *edid);
bool tegra_edid_is_420db_present(struct tegra_edid *edid);
bool tegra_edid_is_hfvsdb_present(struct tegra_edid *edid);
bool tegra_edid_support_yuv422(struct tegra_edid *edid);
bool tegra_edid_support_yuv444(struct tegra_edid *edid);
u16 tegra_edid_get_ex_colorimetry(struct tegra_edid *edid);
int tegra_edid_get_eld(struct tegra_edid *edid, struct tegra_edid_hdmi_eld *elddata);
u32 tegra_edid_get_quirks(struct tegra_edid *edid);
u32 tegra_edid_lookup_quirks(const char *manufacturer, u32 model,
	const char *monitor_name);
struct tegra_dc_edid *tegra_edid_get_data(struct tegra_edid *edid);
void tegra_edid_put_data(struct tegra_dc_edid *data);
int tegra_dc_edid_blob(struct tegra_dc *dc, struct i2c_msg *msgs, int num);

int tegra_edid_underscan_supported(struct tegra_edid *edid);
int tegra_edid_i2c_adap_change_rate(struct i2c_adapter *i2c_adap, int rate);
int tegra_edid_read_block(struct tegra_edid *edid, int block, u8 *data);
int tegra_edid_audio_supported(struct tegra_edid *edid);
#endif
