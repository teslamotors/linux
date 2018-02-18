/*
 * drivers/video/tegra/dc/edid.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION, All rights reserved.
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


#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#include "edid.h"
#include "dc_priv.h"

struct tegra_edid_pvt {
	struct kref			refcnt;
	struct tegra_edid_hdmi_eld	eld;
	bool				support_stereo;
	bool				support_underscan;
	bool				support_audio;
	bool				scdc_present;
	bool				db420_present;
	bool				hfvsdb_present;
	bool				support_yuv422;
	bool				support_yuv444;
	bool				rgb_quant_selectable;
	bool				yuv_quant_selectable;
	int			        hdmi_vic_len;
	u8			        hdmi_vic[7];
	u16			color_depth_flag;
	u16			max_tmds_char_rate_hf_mhz;
	u16			max_tmds_char_rate_hllc_mhz;
	u16			colorimetry;
	u16			min_vrr_fps;
	bool			hdr_present;
	u8			hdr_pckt_len;
	u8			hdr_eotf;
	u8			hdr_static_metadata;
	u16			hdr_desired_max_luma;
	u16			hdr_desired_max_frame_avg_luma;
	u16			hdr_desired_min_luma;
	u32			quirks;
	/* Note: dc_edid must remain the last member */
	struct tegra_dc_edid		dc_edid;
};

/* 720p 60Hz EDID */
static const char default_720p_edid[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x4c, 0x2d, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x2c, 0x0e, 0x01, 0x03, 0x80, 0x59, 0x32, 0x8c,
	0x0a, 0xe2, 0xbd, 0xa1, 0x5b, 0x4a, 0x98, 0x24,
	0x15, 0x47, 0x4a, 0x20, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1d,
	0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28,
	0x55, 0x00, 0x75, 0xf2, 0x31, 0x00, 0x00, 0x1e,
	0x01, 0x1d, 0x00, 0xbc, 0x52, 0xd0, 0x1e, 0x20,
	0xb8, 0x28, 0x55, 0x40, 0x75, 0xf2, 0x31, 0x00,
	0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x32,
	0x3d, 0x0f, 0x2e, 0x08, 0x00, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x53, 0x41, 0x4d, 0x53, 0x55, 0x4e, 0x47,
	0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x81,
	0x02, 0x03, 0x19, 0x71, 0x46, 0x84, 0x13, 0x05,
	0x14, 0x03, 0x12, 0x23, 0x09, 0x07, 0x07, 0x83,
	0x01, 0x00, 0x00, 0x65, 0x03, 0x0c, 0x00, 0x10,
	0x00, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16,
	0x20, 0x58, 0x2c, 0x25, 0x00, 0x75, 0xf2, 0x31,
	0x00, 0x00, 0x9e, 0x01, 0x1d, 0x80, 0xd0, 0x72,
	0x1c, 0x16, 0x20, 0x10, 0x2c, 0x25, 0x80, 0x75,
	0xf2, 0x31, 0x00, 0x00, 0x9e, 0x8c, 0x0a, 0xd0,
	0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96,
	0x00, 0x75, 0xf2, 0x31, 0x00, 0x00, 0x18, 0x8c,
	0x0a, 0xd0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0c,
	0x40, 0x55, 0x00, 0x75, 0xf2, 0x31, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca,
};

#ifdef DEBUG
static char tegra_edid_dump_buff[16 * 1024];

static void tegra_edid_dump(struct tegra_edid *edid)
{
	struct seq_file s;
	int i;
	char c;

	memset(&s, 0x0, sizeof(s));

	s.buf = tegra_edid_dump_buff;
	s.size = sizeof(tegra_edid_dump_buff);
	s.private = edid;

	tegra_edid_show(&s, NULL);

	i = 0;
	while (i < s.count ) {
		if ((s.count - i) > 256) {
			c = s.buf[i + 256];
			s.buf[i + 256] = 0;
			printk("%s", s.buf + i);
			s.buf[i + 256] = c;
		} else {
			printk("%s", s.buf + i);
		}
		i += 256;
	}
}
#else
static void tegra_edid_dump(struct tegra_edid *edid)
{
}
#endif


int tegra_edid_i2c_adap_change_rate(struct i2c_adapter *i2c_adap, int rate)
{
	const int MIN_RATE = 5000, MAX_RATE = 4000000;
	int err = 0, cur_rate = 0;
	if (rate < MIN_RATE || rate > MAX_RATE) {
		pr_warn("Cannot change the i2c_ddc rate, the rate:%d cannot"
"be below minimum rate:%d or above maximum rate:%d", rate, MIN_RATE, MAX_RATE);
		return -1;
	}

	if (i2c_adap) {
		cur_rate = i2c_get_adapter_bus_clk_rate(i2c_adap);
		if (cur_rate == rate)
			return 0;

		err = i2c_set_adapter_bus_clk_rate(i2c_adap, rate);
		if (err)
			pr_warn("Could not change i2c_ddc sclk rate\n");
		else
			pr_warn("Switching i2c_ddc sclk rate: from %d, "
"to %d\n", cur_rate, rate);
	} else {
		pr_warn("ddc i2c adapter NULL\n");
		err = -1;
	}
	return err;
}

static int tegra_edid_i2c_divide_rate(struct tegra_edid *edid)
{
	struct i2c_adapter *i2c_adap = i2c_get_adapter(edid->dc->out->ddc_bus);
	int new_rate = 0, old_rate = 0, err = 0;

	if (i2c_adap) {
		old_rate = i2c_get_adapter_bus_clk_rate(i2c_adap);
		new_rate = old_rate >> 1;
		err = tegra_edid_i2c_adap_change_rate(i2c_adap, new_rate);
	} else
		err = -1;
	return err;
}

int tegra_edid_read_block(struct tegra_edid *edid, int block, u8 *data)
{
	u8 block_buf[] = {block >> 1};
	u8 cmd_buf[] = {(block % 0x2) * 128};
	u8 i;
	u8 last_checksum = 0;
	size_t attempt_cnt = 0;
	struct i2c_msg msg[] = {
		{
			.addr = 0x30,
			.flags = 0,
			.len = 1,
			.buf = block_buf,
		},
		{
			.addr = 0x50,
			.flags = 0,
			.len = 1,
			.buf = cmd_buf,
		},
		{
			.addr = 0x50,
			.flags = I2C_M_RD,
			.len = 128,
			.buf = data,
		}};
	struct i2c_msg *m;
	int msg_len;
	if (block > 1) {
		msg_len = 3;
		m = msg;
	} else {
		msg_len = 2;
		m = &msg[1];
	}

	do {
		u8 checksum = 0;
		int status = edid->i2c_ops.i2c_transfer(edid->dc, m, msg_len);

		if (status < 0)
			return status;

		if (status != msg_len)
			return -EIO;

		/* fix base block header if corrupted */
		if (!block) {
			for (i = 0; i < EDID_BASE_HEADER_SIZE; i++) {
				if (data[i] != edid_base_header[i])
					data[i] = edid_base_header[i];
			}
		}

		for (i = 0; i < 128; i++)
			checksum += data[i];
		if (checksum != 0) {
			/*
			 * It is completely possible that the sink that we are
			 * reading has a bad EDID checksum (specifically, some
			 * of the older TVs). These TVs have the modes, etc
			 * programmed in their EDID correctly, but just have
			 * a bad checksum. It then becomes hard to distinguish
			 * between an i2c failure vs bad EDID.
			 * To get around this, read the EDID multiple times.
			 * If the calculated checksum is the exact same
			 * multiple number of times, just print a
			 * warning and ignore.
			 */

			if (attempt_cnt == 0)
				last_checksum = checksum;

			/* On different checksum remainder, lower i2c speed */
			if (last_checksum != checksum) {
				pr_warn("%s: checksum failed and did not match consecutive reads. Previous remainder was %u. New remainder is %u. Failed at attempt %zu\n",
					__func__, last_checksum, checksum, attempt_cnt);
				if (tegra_edid_i2c_divide_rate(edid)) {
					pr_warn("Cannot halve i2c speed giving"
"up on trying to change the i2c speed for EDID read\n");
					return -EIO;
				} else {
					attempt_cnt = 0;
					continue;
				}
			}
			usleep_range(TEGRA_EDID_MIN_RETRY_DELAY_US, TEGRA_EDID_MAX_RETRY_DELAY_US);
		}
	} while (last_checksum != 0 && ++attempt_cnt < TEGRA_EDID_MAX_RETRY);

	/*
	 * Re-calculate the checksum since the standard EDID parser doesn't
	 * like the bad checksum
	 */
	if (last_checksum != 0) {
		u8 checksum = 0;

		edid->errors |= EDID_ERRORS_CHECKSUM_CORRUPTED;

		for (i = 0; i < 127; i++)
			checksum += data[i];

		checksum = (u8)(256 - checksum);
		data[127] = checksum;

		pr_warn("%s: remainder is %u for the last %d attempts. Assuming bad sink EDID and ignoring. New checksum is %u\n",
				__func__, last_checksum, TEGRA_EDID_MAX_RETRY,
				checksum);
	}

	return 0;
}

static int tegra_edid_parse_ext_block(const u8 *raw, int idx,
			       struct tegra_edid_pvt *edid)
{
	const u8 *ptr;
	u8 tmp;
	u8 code;
	int len;
	int i;
	bool basic_audio = false;

	if (!edid) {
		pr_err("%s: invalid argument\n", __func__);
		return -EINVAL;
	}
	ptr = &raw[0];

	/* If CEA 861 block get info for eld struct */
	if (ptr) {
		if (*ptr <= 3)
			edid->eld.eld_ver = 0x02;
		edid->eld.cea_edid_ver = ptr[1];

		/* check for basic audio support in CEA 861 block */
		if(raw[3] & (1<<6)) {
			/* For basic audio, set spk_alloc to Left+Right.
			 * If there is a Speaker Alloc block this will
			 * get over written with that value */
			basic_audio = true;
			edid->support_audio = 1;
		}
	}

	if (raw[3] & 0x80)
		edid->support_underscan = 1;
	else
		edid->support_underscan = 0;

	if (raw[3] & (1<<5))
		edid->support_yuv444 = 1;
	else
		edid->support_yuv444 = 0;

	if (raw[3] & (1<<4))
		edid->support_yuv422 = 1;
	else
		edid->support_yuv422 = 0;
	ptr = &raw[4];

	while (ptr < &raw[idx]) {
		tmp = *ptr;
		len = tmp & 0x1f;

		/* HDMI Specification v1.4a, section 8.3.2:
		 * see Table 8-16 for HDMI VSDB format.
		 * data blocks have tags in top 3 bits:
		 * tag code 2: video data block
		 * tag code 3: vendor specific data block
		 */
		code = (tmp >> 5) & 0x7;
		switch (code) {
		case CEA_DATA_BLOCK_AUDIO:
		{
			int sad_n = edid->eld.sad_count * 3;
			edid->eld.sad_count += len / 3;
			pr_debug("%s: incrementing eld.sad_count by %d to %d\n",
				 __func__, len / 3, edid->eld.sad_count);
			edid->eld.conn_type = 0x00;
			edid->eld.support_hdcp = 0x00;
			for (i = 0; (i < len) && (sad_n < ELD_MAX_SAD_BYTES);
			     i++, sad_n++)
				edid->eld.sad[sad_n] = ptr[i + 1];
			len++;
			ptr += len; /* adding the header */
			/* Got an audio data block so enable audio */
			if (basic_audio == true)
				edid->eld.spk_alloc = 1;
			break;
		}
		/* case 2 is commented out for now */
		case CEA_DATA_BLOCK_VENDOR:
		{
			int j = 0;
			u16 temp = 0;
			/* OUI for hdmi licensing, LLC */
			if ((ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {
				edid->eld.port_id[0] = ptr[4];
				edid->eld.port_id[1] = ptr[5];
				temp = ptr[6];
				edid->color_depth_flag = (temp << 5) &
							TEGRA_DC_RGB_MASK;
				if (edid->support_yuv422 && (temp & 0x08))
					edid->color_depth_flag |= (temp >> 1) &
							TEGRA_DC_Y422_MASK;
				if (edid->support_yuv444 && (temp & 0x08))
					edid->color_depth_flag |= (temp << 2) &
							TEGRA_DC_Y444_MASK;
				if (len >= 7)
					edid->max_tmds_char_rate_hllc_mhz =
								ptr[7] * 5;
				edid->max_tmds_char_rate_hllc_mhz =
					edid->max_tmds_char_rate_hllc_mhz ? :
					165; /* for <=165MHz field may be 0 */
			}

			/* OUI for hdmi forum */
			if ((ptr[1] == 0xd8) &&
				(ptr[2] == 0x5d) &&
				(ptr[3] == 0xc4)) {
				edid->hfvsdb_present = true;
				edid->color_depth_flag |= ptr[7] &
							TEGRA_DC_Y420_MASK;
				edid->max_tmds_char_rate_hf_mhz = ptr[5] * 5;
				edid->scdc_present = (ptr[6] >> 7) & 0x1;
			}

			/* OUI for Nvidia */
			if ((ptr[1] == 0x4b) &&
				(ptr[2] == 0x04) &&
				(ptr[3] == 0)) {
				/* version 1.0 vrr capabilities */
				if (ptr[4] == 1)
					edid->min_vrr_fps = ptr[5];
			}

			if ((len >= 8) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {
				j = 8;
				tmp = ptr[j++];
				/* HDMI_Video_present? */
				if (tmp & 0x20) {
					/* Latency_Fields_present? */
					if (tmp & 0x80)
						j += 2;
					/* I_Latency_Fields_present? */
					if (tmp & 0x40)
						j += 2;
					/* 3D_present? */
					if (j <= len && (ptr[j] & 0x80))
						edid->support_stereo = 1;
					/* HDMI_VIC_LEN */
					if (++j <= len && (ptr[j] & 0xe0)) {
						int k = 0;
						edid->hdmi_vic_len = ptr[j] >> 5;
						for (k = 0; k < edid->hdmi_vic_len; k++)
						    edid->hdmi_vic[k] = ptr[j+k+1];
					}
				}
			}
			if ((len > 5) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {

				edid->eld.support_ai = (ptr[6] & 0x80);
			}

			if ((len > 9) &&
				(ptr[1] == 0x03) &&
				(ptr[2] == 0x0c) &&
				(ptr[3] == 0)) {

				edid->eld.aud_synch_delay = ptr[10];
			}
			len++;
			ptr += len; /* adding the header */
			break;
		}
		case CEA_DATA_BLOCK_SPEAKER_ALLOC:
		{
			edid->eld.spk_alloc = ptr[1];
			len++;
			ptr += len; /* adding the header */
			break;
		}
		case CEA_DATA_BLOCK_EXT:
		{
			u8 ext_db = ptr[1];

			switch (ext_db) {
			case CEA_DATA_BLOCK_EXT_VCDB:
				edid->rgb_quant_selectable = ptr[2] & 0x40;
				edid->yuv_quant_selectable = ptr[2] & 0x80;
				break;
			case CEA_DATA_BLOCK_EXT_Y420VDB: /* fall through */
			case CEA_DATA_BLOCK_EXT_Y420CMDB:
				edid->db420_present = true;
				break;
			case CEA_DATA_BLOCK_EXT_CDB:
				edid->colorimetry = ptr[2];
				break;
			case CEA_DATA_BLOCK_EXT_HDR:
				edid->hdr_pckt_len = ptr[0] & 0x0f;
				edid->hdr_present = true;
				edid->hdr_eotf = ptr[2];
				edid->hdr_static_metadata = ptr[3];
				if (edid->hdr_pckt_len > 5) {
					edid->hdr_desired_max_luma = ptr[4];
					edid->hdr_desired_max_frame_avg_luma =
									ptr[5];
					edid->hdr_desired_min_luma = ptr[6];
				} else if (edid->hdr_pckt_len > 4) {
					edid->hdr_desired_max_luma = ptr[4];
					edid->hdr_desired_max_frame_avg_luma =
									ptr[5];
				} else
					edid->hdr_desired_max_luma = ptr[4];
				break;
			};

			len++;
			ptr += len;
			break;
		}
		default:
			len++; /* len does not include header */
			ptr += len;
			break;
		}
	}

	return 0;
}

static int tegra_edid_mode_support_stereo(struct fb_videomode *mode)
{
	if (!mode)
		return 0;

	if (mode->xres == 1280 &&
		mode->yres == 720 &&
		((mode->refresh == 60) || (mode->refresh == 50)))
		return 1;

	if (mode->xres == 1920 && mode->yres == 1080 && mode->refresh == 24)
		return 1;

	return 0;
}

static void data_release(struct kref *ref)
{
	struct tegra_edid_pvt *data =
		container_of(ref, struct tegra_edid_pvt, refcnt);
	vfree(data);
}

u16 tegra_edid_get_cd_flag(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return -EFAULT;
	}

	return edid->data->color_depth_flag;
}

u16 tegra_edid_get_ex_hdr_cap(struct tegra_edid *edid)
{
	u16 ret = 0;
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return -EFAULT;
	}

	if (edid->data->hdr_present)
		ret |= FB_CAP_HDR;

	return ret;
}

u16 tegra_edid_get_quant_cap(struct tegra_edid *edid)
{
	u16 ret = 0;

	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return 0;
	}

	if (edid->data->rgb_quant_selectable)
		ret |= FB_CAP_RGB_QUANT_SELECTABLE;

	if (edid->data->yuv_quant_selectable)
		ret |= FB_CAP_YUV_QUANT_SELECTABLE;

	return ret;
}

/* hdmi spec mandates sink to specify correct max_tmds_clk only for >165MHz */
u16 tegra_edid_get_max_clk_rate(struct tegra_edid *edid)
{
	u16 tmds_hf, tmds_llc;

	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return -EFAULT;
	}

	tmds_hf = edid->data->max_tmds_char_rate_hf_mhz;
	tmds_llc = edid->data->max_tmds_char_rate_hllc_mhz;

	if (tmds_hf || tmds_llc)
		return tmds_hf ? : tmds_llc;

	return 0;
}

bool tegra_edid_is_scdc_present(struct tegra_edid *edid)
{
	if (tegra_platform_is_linsim()) {
		return false;
	}
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return false;
	}

	if (edid->data->scdc_present &&
		!tegra_edid_is_hfvsdb_present(edid)) {
		pr_warn("scdc presence incorrectly parsed\n");
		dump_stack();
	}

	return edid->data->scdc_present;
}

bool tegra_edid_is_hfvsdb_present(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return false;
	}

	return edid->data->hfvsdb_present;
}

bool tegra_edid_is_420db_present(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return false;
	}

	return edid->data->db420_present;
}

u32 tegra_edid_get_quirks(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return false;
	}

	return edid->data->quirks;
}

u16 tegra_edid_get_ex_colorimetry(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return 0;
	}

	return edid->data->colorimetry;
}

bool tegra_edid_support_yuv422(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return 0;
	}

	return edid->data->support_yuv422;
}

bool tegra_edid_support_yuv444(struct tegra_edid *edid)
{
	if (!edid || !edid->data) {
		pr_warn("edid invalid\n");
		return 0;
	}

	return edid->data->support_yuv444;
}

int tegra_edid_get_monspecs(struct tegra_edid *edid, struct fb_monspecs *specs)
{
	int i;
	int j;
	int ret;
	int extension_blocks;
	struct tegra_edid_pvt *new_data, *old_data;
	u8 checksum = 0;
	u8 *data;
	bool use_fallback = false;

	new_data = vzalloc(SZ_32K + sizeof(struct tegra_edid_pvt));
	if (!new_data)
		return -ENOMEM;

	kref_init(&new_data->refcnt);

	if (edid->errors & EDID_ERRORS_READ_FAILED)
		use_fallback = true;

	edid->errors = 0;

	data = new_data->dc_edid.buf;

	if (edid->dc->vedid) {
		memcpy(data, edid->dc->vedid_data, 128);
		/* checksum new edid */
		for (i = 0; i < 128; i++)
			checksum += data[i];
		if (checksum != 0) {
			pr_err("%s: checksum failed\n", __func__);
			ret = -EINVAL;
			goto fail;
		}
	} else if (use_fallback) {
		memcpy(data, default_720p_edid, 128);
		/* no checksum test needed */
	} else {
		ret = tegra_edid_read_block(edid, 0, data);
		if (ret)
			goto fail;
	}

	memset(specs, 0x0, sizeof(struct fb_monspecs));
	memset(&new_data->eld, 0x0, sizeof(new_data->eld));
	fb_edid_to_monspecs(data, specs);
	if (specs->modedb == NULL) {
		ret = -EINVAL;
		goto fail;
	}
	memcpy(new_data->eld.monitor_name, specs->monitor, sizeof(specs->monitor));
	new_data->eld.mnl = strlen(new_data->eld.monitor_name) + 1;
	new_data->eld.product_id[0] = data[0x8];
	new_data->eld.product_id[1] = data[0x9];
	new_data->eld.manufacture_id[0] = data[0xA];
	new_data->eld.manufacture_id[1] = data[0xB];
	new_data->quirks = tegra_edid_lookup_quirks(specs->manufacturer,
		specs->model, specs->monitor);

	extension_blocks = data[0x7e];

	/* verify only one extension block (to stay in bounds) */
	if (use_fallback && extension_blocks != 1) {
		pr_err("%s: fallback edid parsing failed\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	for (i = 1; i <= extension_blocks; i++) {
		if (edid->dc->vedid) {
			memcpy(data + i * 128,
				edid->dc->vedid_data + i * 128, 128);
			for (j = 0; j < 128; j++)
				checksum += data[i * 128 + j];
			if (checksum != 0) {
				pr_err("%s: checksum failed\n", __func__);
				ret = -EINVAL;
				goto fail;
			}
		} else if (use_fallback) {
			/* only one extension block, verified above */
			memcpy(data + i * 128,
				default_720p_edid + i * 128, 128);
		} else {
			ret = tegra_edid_read_block(edid, i, data + i * 128);
			if (ret < 0)
				goto fail;
		}

		if (data[i * 128] == 0x2) {
			fb_edid_add_monspecs(data + i * 128, specs);

			tegra_edid_parse_ext_block(data + i * 128,
					data[i * 128 + 2], new_data);

			if (new_data->support_stereo) {
				for (j = 0; j < specs->modedb_len; j++) {
					if (tegra_edid_mode_support_stereo(
						&specs->modedb[j]))
						specs->modedb[j].vmode |=
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
						FB_VMODE_STEREO_FRAME_PACK;
#else
						FB_VMODE_STEREO_LEFT_RIGHT;
#endif
				}
			}

			if (new_data->hdmi_vic_len > 0) {
				int k;
				int l = specs->modedb_len;
				struct fb_videomode *m;
				m = kzalloc((specs->modedb_len +
				    new_data->hdmi_vic_len) *
				    sizeof(struct fb_videomode), GFP_KERNEL);
				if (!m)
					break;
				memcpy(m, specs->modedb, specs->modedb_len *
					sizeof(struct fb_videomode));
				for (k = 0; k < new_data->hdmi_vic_len; k++) {
				    unsigned vic = new_data->hdmi_vic[k];
				    if (vic >= HDMI_EXT_MODEDB_SIZE) {
				        pr_warning("Unsupported HDMI VIC %d, ignoring\n", vic);
				        continue;
				    }
				    memcpy(&m[l], &hdmi_ext_modes[vic],
						sizeof(m[l]));
				    m[l].vmode |= FB_VMODE_IS_HDMI_EXT;
				    l++;
				}
				kfree(specs->modedb);
				specs->modedb = m;
				specs->modedb_len = specs->modedb_len +
							new_data->hdmi_vic_len;
			}
		}
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_ARCH_TEGRA_18x_SOC)
	/* T210 and T186 supports fractional divider and hence can support the * 1000 / 1001 modes.
	   For now, only enable support for 24, 30 and 60 Hz modes. */
	{
		const int max_modes = 50;
		struct fb_videomode *frac_modes, *m;
		int frac_n = 0;
		frac_modes = kzalloc(sizeof(struct fb_videomode) * max_modes,
				     GFP_KERNEL);

		if (frac_modes) {
			for (j = 0; j < specs->modedb_len; ++j) {
				int rate = tegra_dc_calc_fb_refresh(&specs->modedb[j]);
				/*
				 * 1000/1001 modes are only supported on CEA
				 * SVDs or on HDMI EXT
				 * */
				bool supported = ((specs->modedb[j].vmode &
						   FB_VMODE_IS_CEA) &&
						  !(specs->modedb[j].vmode &
						    FB_VMODE_IS_DETAILED)) ||
						 specs->modedb[j].vmode &
						 FB_VMODE_IS_HDMI_EXT;
				if (supported && (rate == 24000 ||
				     rate == 30000 ||
				    (rate > (60000 - 20) && rate < (60000 + 20))) &&
				    frac_n < max_modes) {
					memcpy(&frac_modes[frac_n], &specs->modedb[j], sizeof(struct fb_videomode));
					frac_modes[frac_n].pixclock = frac_modes[frac_n].pixclock * 1001 / 1000;
					frac_modes[frac_n].vmode |= FB_VMODE_1000DIV1001;
					frac_n++;
				}
			}

			if (frac_n == max_modes)
				pr_warn("Hit fractional mode limit %d!\n", frac_n);

			m = kzalloc((specs->modedb_len + frac_n) * sizeof(struct fb_videomode), GFP_KERNEL);
			if (m) {
				memcpy(m, specs->modedb, specs->modedb_len * sizeof(struct fb_videomode));
				memcpy(&m[specs->modedb_len], frac_modes, frac_n * sizeof(struct fb_videomode));
				kfree(specs->modedb);
				specs->modedb = m;
				specs->modedb_len += frac_n;
			}

			kfree(frac_modes);
		}
	}
#endif

	for (j = 0; j < specs->modedb_len; j++) {
		if (!new_data->rgb_quant_selectable &&
		    !(specs->modedb[j].vmode & FB_VMODE_SET_YUV_MASK))
			specs->modedb[j].vmode |= FB_VMODE_LIMITED_RANGE;

		if (!new_data->yuv_quant_selectable &&
		    (specs->modedb[j].vmode & FB_VMODE_SET_YUV_MASK))
			specs->modedb[j].vmode |= FB_VMODE_LIMITED_RANGE;
	}

	if (use_fallback)
		edid->errors |= EDID_ERRORS_USING_FALLBACK;

	new_data->dc_edid.len = i * 128;

	mutex_lock(&edid->lock);
	old_data = edid->data;
	edid->data = new_data;
	mutex_unlock(&edid->lock);

	if (old_data)
		kref_put(&old_data->refcnt, data_release);

	tegra_edid_dump(edid);
	return 0;

fail:
	vfree(new_data);
	return ret;
}

int tegra_edid_audio_supported(struct tegra_edid *edid)
{
	if ((!edid) || (!edid->data))
		return 0;

	return edid->data->support_audio;
}

int tegra_edid_underscan_supported(struct tegra_edid *edid)
{
	if ((!edid) || (!edid->data))
		return 0;

	return edid->data->support_underscan;
}

int tegra_edid_get_eld(struct tegra_edid *edid, struct tegra_edid_hdmi_eld *elddata)
{
	if (!elddata || !edid->data)
		return -EFAULT;

	memcpy(elddata,&edid->data->eld,sizeof(struct tegra_edid_hdmi_eld));

	return 0;
}

struct tegra_edid *tegra_edid_create(struct tegra_dc *dc,
	i2c_transfer_func_t i2c_func)
{
	struct tegra_edid *edid;

	edid = kzalloc(sizeof(struct tegra_edid), GFP_KERNEL);
	if (!edid)
		return ERR_PTR(-ENOMEM);

	mutex_init(&edid->lock);
	edid->i2c_ops.i2c_transfer = i2c_func;
	edid->dc = dc;

	return edid;
}

void tegra_edid_destroy(struct tegra_edid *edid)
{
	if (edid->data)
		kref_put(&edid->data->refcnt, data_release);
	kfree(edid);
}

struct tegra_dc_edid *tegra_edid_get_data(struct tegra_edid *edid)
{
	struct tegra_edid_pvt *data;

	mutex_lock(&edid->lock);
	data = edid->data;
	if (data)
		kref_get(&data->refcnt);
	mutex_unlock(&edid->lock);

	return data ? &data->dc_edid : NULL;
}

void tegra_edid_put_data(struct tegra_dc_edid *data)
{
	struct tegra_edid_pvt *pvt;

	if (!data)
		return;

	pvt = container_of(data, struct tegra_edid_pvt, dc_edid);

	kref_put(&pvt->refcnt, data_release);
}

int tegra_dc_edid_blob(struct tegra_dc *dc, struct i2c_msg *msgs, int num)
{
	struct i2c_msg *pmsg;
	int i;
	int status = 0;
	u32 len = 0;
	struct device_node *np_panel = NULL;

	np_panel = tegra_get_panel_node_out_type_check(dc,
		dc->pdata->default_out->type);

	if (!np_panel || !of_device_is_available(np_panel))
		return -ENOENT;

	for (i = 0; i < num; ++i) {
		pmsg = &msgs[i];

		if (pmsg->flags & I2C_M_RD) { /* Read */
			len = pmsg->len;
			status = of_property_read_u8_array(np_panel,
				"nvidia,edid", pmsg->buf, len);

			if (status) {
				dev_err(&dc->ndev->dev,
					"Failed to read EDID blob from DT"
					" addr:%d, size:%d\n",
					pmsg->addr, len);
				return status;
			}
		}
	}
	of_node_put(np_panel);
	return i;
}

struct tegra_dc_edid *tegra_dc_get_edid(struct tegra_dc *dc)
{
	if (!dc->edid)
		return ERR_PTR(-ENODEV);

	return tegra_edid_get_data(dc->edid);
}
EXPORT_SYMBOL(tegra_dc_get_edid);

void tegra_dc_put_edid(struct tegra_dc_edid *edid)
{
	tegra_edid_put_data(edid);
}
EXPORT_SYMBOL(tegra_dc_put_edid);

static const struct i2c_device_id tegra_edid_id[] = {
        { "tegra_edid", 0 },
        { }
};

MODULE_DEVICE_TABLE(i2c, tegra_edid_id);

static struct i2c_driver tegra_edid_driver = {
        .id_table = tegra_edid_id,
        .driver = {
                .name = "tegra_edid",
        },
};

static int __init tegra_edid_init(void)
{
        return i2c_add_driver(&tegra_edid_driver);
}

static void __exit tegra_edid_exit(void)
{
        i2c_del_driver(&tegra_edid_driver);
}

module_init(tegra_edid_init);
module_exit(tegra_edid_exit);
