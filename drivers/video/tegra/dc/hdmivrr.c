/*
 * drivers/video/tegra/dc/hdmivrr.c
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/ote_protocol.h>

#include <mach/dc.h>
#include <mach/fb.h>

#include "dc_priv.h"
#include "edid.h"
#include "hdmi2.0.h"
#include "sor.h"
#include "sor_regs.h"
#include "hdmi_reg.h"
#include "hdmivrr.h"

#define HDMIVRR_POLL_MS 50
#define HDMIVRR_POLL_TIMEOUT_MS 1000

#define HDMIVRR_CHLNG_SRC_MON 0
#define HDMIVRR_CHLNG_SRC_DRV 1

#define NS_IN_MS (1000 * 1000)

static int hdmivrr_i2c_read(struct tegra_hdmi *hdmi, size_t len, void *data)
{
	int status;
	struct tegra_dc *dc = hdmi->dc;

	struct i2c_msg msg[] = {
		{
			.addr = 0x37,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	if (dc->vedid)
		goto skip_hdmivrr_i2c;

	tegra_dc_ddc_enable(dc, true);

	status = i2c_transfer(hdmi->ddcci_i2c_client->adapter,
				msg, ARRAY_SIZE(msg));
	if (status != 1)
		status = -EIO;
	else
		status = 0;

	tegra_dc_ddc_enable(dc, false);

	return status;

skip_hdmivrr_i2c:
	return -EIO;
}

static int hdmivrr_i2c_write(struct tegra_hdmi *hdmi,
					size_t len, const void *data)
{
	int status;
	u8 buf[len];
	struct tegra_dc *dc = hdmi->dc;

	struct i2c_msg msg[] = {
		{
			.addr = 0x37,
			.flags = 0,
			.len = len,
			.buf = buf,
		},
	};

	if (dc->vedid)
		goto skip_hdmivrr_i2c;

	memcpy(buf, data, len);

	tegra_dc_ddc_enable(dc, true);

	status = i2c_transfer(hdmi->ddcci_i2c_client->adapter,
				msg, ARRAY_SIZE(msg));

	if (status != 1)
		status = -EIO;
	else
		status = 0;

	tegra_dc_ddc_enable(dc, false);
	return status;

skip_hdmivrr_i2c:
	return -EIO;
}

static void hdmivrr_calc_checksum(char *buf, int len)
{
	int i;
	char cksum = 0x6e;

	for (i = 0; i < len-1; i++)
		cksum ^= buf[i];
	buf[len-1] = cksum;
}

static bool hdmivrr_verify_checksum(char *buf, int len)
{
	int i;
	char cksum = 0x50;

	for (i = 0; i < len-1; i++)
		cksum ^= buf[i];

	return buf[len-1] == cksum;
}

static int hdmivrr_set_vcp(struct tegra_hdmi *hdmi, u8 reg, u16 val)
{
	int status;
	char buf[SET_VCP_LEN] = {0x51, 0x84, 0x3, 0x00, 0x00, 0x00, 0x00};

	buf[SET_VCP_VCP_OFF] = reg;
	buf[SET_VCP_SH_OFF] = (val >> 8) & 0xff;
	buf[SET_VCP_SL_OFF] = val & 0xff;
	hdmivrr_calc_checksum(buf, SET_VCP_LEN);

	status = hdmivrr_i2c_write(hdmi, SET_VCP_LEN, buf);

	return status;
}

static int hdmivrr_get_vcp(struct tegra_hdmi *hdmi,
					u8 reg, u16 *val)
{
	int status;
	char wr_buf[GET_VCP_WR_LEN] = {0x51, 0x82, 0x01, 0x00, 0x00};
	char rd_buf[GET_VCP_RD_LEN];
	char ret_code;

	wr_buf[GET_VCP_WR_VCP_OFF] = reg;
	hdmivrr_calc_checksum(wr_buf, GET_VCP_WR_LEN);

	status = hdmivrr_i2c_write(hdmi, GET_VCP_WR_LEN, wr_buf);
	if (status)
		return status;

	status = hdmivrr_i2c_read(hdmi, GET_VCP_RD_LEN, rd_buf);
	ret_code = rd_buf[GET_VCP_RES_CODE_OFF];
	if (ret_code)
		return ret_code;

	if (!hdmivrr_verify_checksum(rd_buf, GET_VCP_RD_LEN))
		return -EINVAL;
	*val = rd_buf[GET_VCP_SH_OFF] << 8 | rd_buf[GET_VCP_SL_OFF];

	return status;
}

static int hdmivrr_table_write(struct tegra_hdmi *hdmi,
					u8 reg, u8 len, u8 *val)
{
	int status;
	char buf[TABLE_WRITE_LEN] = {0x51, 0x00, 0xe7, 0x00, 0x00, 0x00};
	u8 tlen;

	buf[TABLE_WRITE_VCP_OFF] = reg;
	buf[TABLE_WRITE_LEN_OFF] = 0x80 | (len + 4);
	tlen = len + TABLE_WRITE_HEADER_LEN + 1;
	memcpy(buf + TABLE_WRITE_DATA_OFF, val, len);
	hdmivrr_calc_checksum(buf, tlen);

	status = hdmivrr_i2c_write(hdmi, tlen, buf);

	return status;
}

static int hdmivrr_table_read(struct tegra_hdmi *hdmi,
					u8 reg, u8 len, u8 *val)
{
	int status;
	int tlen;
	char wr_buf[TABLE_READ_WR_LEN] = {0x51, 0x84, 0xe2,
						0x00, 0x00, 0x00, 0x00};
	char rd_buf[TABLE_READ_RD_LEN];

	wr_buf[TABLE_READ_WR_VCP_OFF] = reg;
	hdmivrr_calc_checksum(wr_buf, TABLE_READ_WR_LEN);
	status = hdmivrr_i2c_write(hdmi, TABLE_READ_WR_LEN, wr_buf);
	if (status)
		return status;

	tlen = len + TABLE_READ_RD_HEADER_LEN + 1;
	status = hdmivrr_i2c_read(hdmi, tlen, rd_buf);

	if (!hdmivrr_verify_checksum(rd_buf, tlen))
		return -EINVAL;

	memcpy(val, rd_buf + TABLE_READ_RD_DATA_OFF, len);

	return status;
}

static int hdmivrr_poll_vcp(struct tegra_hdmi *hdmi, u8 reg,
		u16 exp_val, u32 poll_interval_ms, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u16 reg_val = 0;
	int status;

	do {
		status = hdmivrr_get_vcp(hdmi, reg, &reg_val);
		if (status)
			return status;

		if (reg_val != exp_val)
			msleep(poll_interval_ms);
		else
			return 0;
	} while (time_after(timeout_jf, jiffies));

	dev_err(&hdmi->dc->ndev->dev, "VRR VCP poll timeout\n");

	return -ETIMEDOUT;
}

static int hdmivrr_wait_for_vcp_ready(struct tegra_hdmi *hdmi)
{
	return hdmivrr_poll_vcp(hdmi, VCP_AUX_STAT, 0,
		HDMIVRR_POLL_MS, HDMIVRR_POLL_TIMEOUT_MS);
}

static int hdmivrr_dpcd_read(struct tegra_hdmi *hdmi,
					u32 reg, u8 len, u8 *val)
{
	int status;

	status = hdmivrr_wait_for_vcp_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_ADDR_H, (reg>>16) & 0xffff);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_ADDR_L, reg & 0xffff);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_LENGTH, len);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_STAT, VCP_AUX_STAT_RD);
	if (status)
		return status;

	status = hdmivrr_wait_for_vcp_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_table_read(hdmi, VCP_AUX_BUF, len, val);

	return status;
}

static int hdmivrr_dpcd_write(struct tegra_hdmi *hdmi,
					u32 reg, u8 len, u8 *val)
{
	int status;

	status = hdmivrr_wait_for_vcp_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_ADDR_H, (reg>>16) & 0xffff);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_ADDR_L, reg & 0xffff);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_LENGTH, len);
	if (status)
		return status;

	status = hdmivrr_table_write(hdmi, VCP_AUX_BUF, len, val);
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_AUX_STAT, VCP_AUX_STAT_WR);
	if (status)
		return status;

	status = hdmivrr_wait_for_vcp_ready(hdmi);
	if (status)
		return status;

	return status;
}

static int hdmivrr_dpcd_read_u8(struct tegra_hdmi *hdmi, u32 reg, u8 *val)
{
	int status;

	status = hdmivrr_dpcd_read(hdmi, reg, 1, val);

	return status;
}

static int hdmivrr_dpcd_write_u8(struct tegra_hdmi *hdmi, u32 reg, u8 val)
{
	int status;

	status = hdmivrr_dpcd_write(hdmi, reg, 1, &val);

	return status;
}

static int hdmivrr_dpcd_read_u32(struct tegra_hdmi *hdmi, u32 reg, u32 *val)
{
	int status;
	u8 buf[4];

	status = hdmivrr_dpcd_read(hdmi, reg, 4, buf);

	*val = buf[0]<<24 | buf[1] << 16 | buf[2] << 8 | buf[3];

	return status;
}

__maybe_unused
static int hdmivrr_dpcd_write_u32(struct tegra_hdmi *hdmi, u32 reg, u32 val)
{
	int status;
	u8 buf[4];

	buf[0] = (val >> 24) & 0xff;
	buf[1] = (val >> 16) & 0xff;
	buf[2] = (val >> 8)  & 0xff;
	buf[3] = (val >> 0)  & 0xff;
	status = hdmivrr_dpcd_write(hdmi, reg, 4, buf);

	return status;
}

__maybe_unused
static int tegra_hdmivrr_is_vrr_capable(struct tegra_hdmi *hdmi)
{
	int status;
	u16 fw_ver;

	status = hdmivrr_get_vcp(hdmi, VCP_NV_FW_VER, &fw_ver);
	if (status)
		return status;

	if (fw_ver < NV_FW_MIN_VER)
		return -EINVAL;

	return status;
}

__maybe_unused
static int tegra_hdmivrr_page_init(struct tegra_hdmi *hdmi)
{
	int status;
	u16 page;

	status = hdmivrr_get_vcp(hdmi, VCP_MAGIC1, &page);
	if (status)
		return status;

	if (page == (('A' << 8) | 'U'))
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_MAGIC0, ('N'<<8) | 'V');
	if (status)
		return status;

	status = hdmivrr_set_vcp(hdmi, VCP_MAGIC1, ('A'<<8) | 'U');

	return status;
}

static int tegra_hdmivrr_auth_setup(struct tegra_hdmi *hdmi,
						struct tegra_vrr *vrr)
{
	int status = 0;
	u8 oui[3] = {0x00, 0x04, 0x4b};
	u8 buf[3];
	u32 val32;
	u8 val8;

	status = hdmivrr_dpcd_write(hdmi, DPAUX_SOURCE_OUI, 3, oui);
	if (status)
		return status;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_SOURCE_OUI, 3, buf);
	if (status)
		return status;

	if (buf[0] != oui[0] || buf[1] != oui[1] || buf[2] != oui[2])
		return -EINVAL;

	status = hdmivrr_dpcd_read_u32(hdmi, DPAUX_AUTH_MAGIC, &val32);
	if (status)
		return status;

	if (val32 != AUTH_MAGIC_NUM)
		return -EINVAL;

	status = hdmivrr_dpcd_read_u8(hdmi, DPAUX_AUTH_PROTOCOL, &val8);
	if (status)
		return status;

	if (val8 != AUTH_PROTOCOL_VALID)
		return -EINVAL;

	status = hdmivrr_dpcd_read_u8(hdmi, DPAUX_AUTH_KEYNUM, &vrr->keynum);
	if (status)
		return status;

	if (vrr->keynum != AUTH_KEYNUM_VALUE)
		return -EINVAL;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_SERIALNUM, 9, vrr->serial);

	return status;
}

static int tegra_hdmivrr_auth_sts_ready(struct tegra_hdmi *hdmi)
{
	unsigned long timeout_jf = jiffies +
		msecs_to_jiffies(HDMIVRR_POLL_TIMEOUT_MS);
	int status = 0;
	u8 auth_stat;

	do {
		status = hdmivrr_dpcd_read_u8(hdmi,
					DPAUX_AUTH_STATUS, &auth_stat);
		if (status)
			return status;

		if (auth_stat != AUTH_STATUS_READY)
			msleep(HDMIVRR_POLL_MS);
		else
			return 0;
	} while (time_after(timeout_jf, jiffies));

	return -ETIMEDOUT;
}

static void tegra_hdmivrr_mac(struct tegra_hdmi *hdmi, struct tegra_vrr *vrr)
{
#if defined(CONFIG_TRUSTED_LITTLE_KERNEL) || defined(CONFIG_OTE_TRUSTY)
	if (te_is_secos_dev_enabled())
		te_authenticate_vrr((u8 *)&vrr->keynum, 75);
#endif
}

static int tegra_hdmivrr_driver_unlock(struct tegra_hdmi *hdmi,
						struct tegra_vrr *vrr)
{
	int status = 0;
	u8 digest[32];

	tegra_hdmivrr_auth_setup(hdmi, vrr);
	if (status)
		return status;

	hdmivrr_dpcd_write_u8(hdmi, DPAUX_AUTH_CMD, AUTH_CMD_RESET);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	hdmivrr_dpcd_write_u8(hdmi, DPAUX_AUTH_CMD, AUTH_CMD_MONAUTH);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	vrr->challenge_src = HDMIVRR_CHLNG_SRC_DRV;
	tegra_hdmivrr_mac(hdmi, vrr);

	status = hdmivrr_dpcd_write(hdmi, DPAUX_AUTH_CHALLENGE1, 16,
							vrr->challenge);
	if (status)
		return status;

	status = hdmivrr_dpcd_write(hdmi, DPAUX_AUTH_CHALLENGE2, 16,
							&vrr->challenge[16]);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_AUTH_DIGEST1, 16, digest);
	if (status)
		return status;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_AUTH_DIGEST2, 16, &digest[16]);
	if (status)
		return status;

	if (memcmp(vrr->digest, digest, sizeof(digest))) {
		dev_err(&hdmi->dc->ndev->dev, "VRR driver unlock digest mismatch\n");
		return -EINVAL;
	}

	dev_info(&hdmi->dc->ndev->dev, "VRR driver unlock succeeded\n");

	return status;
}

static int tegra_hdmivrr_monitor_unlock(struct tegra_hdmi *hdmi,
						struct tegra_vrr *vrr)
{
	int status = 0;
	u8 val8;

	tegra_hdmivrr_auth_setup(hdmi, vrr);
	if (status)
		return status;

	status = hdmivrr_dpcd_read_u8(hdmi, DPAUX_LOCK_STATUS, &val8);
	if (status)
		return status;

	if (val8 == LOCK_STATUS_UNLOCKED) {
		dev_info(&hdmi->dc->ndev->dev, "VRR monitor unlocked\n");
		return status;
	}

	hdmivrr_dpcd_write_u8(hdmi, DPAUX_AUTH_CMD, AUTH_CMD_RESET);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	hdmivrr_dpcd_write_u8(hdmi, DPAUX_AUTH_CMD, AUTH_CMD_DRVAUTH);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_AUTH_CHALLENGE1, 16,
							vrr->challenge);
	if (status)
		return status;

	status = hdmivrr_dpcd_read(hdmi, DPAUX_AUTH_CHALLENGE2, 16,
							&vrr->challenge[16]);
	if (status)
		return status;

	vrr->challenge_src = HDMIVRR_CHLNG_SRC_MON;
	tegra_hdmivrr_mac(hdmi, vrr);

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_dpcd_write(hdmi, DPAUX_AUTH_DIGEST1, 16, vrr->digest);
	if (status)
		return status;

	status = hdmivrr_dpcd_write(hdmi, DPAUX_AUTH_DIGEST2, 16,
							&vrr->digest[16]);
	if (status)
		return status;

	tegra_hdmivrr_auth_sts_ready(hdmi);
	if (status)
		return status;

	status = hdmivrr_dpcd_read_u8(hdmi, DPAUX_LOCK_STATUS, &val8);
	if (status)
		return status;

	if (val8 != LOCK_STATUS_UNLOCKED) {
		dev_err(&hdmi->dc->ndev->dev, "VRR monitor unlock digest refused\n");
		return -EINVAL;
	}

	dev_info(&hdmi->dc->ndev->dev, "VRR monitor unlock succeeded\n");

	return status;
}

__maybe_unused
static int tegra_hdmivrr_authentication(struct tegra_hdmi *hdmi,
						struct tegra_vrr *vrr)
{
	int status = 0;

	status = tegra_hdmivrr_driver_unlock(hdmi, vrr);
	if (status) {
		dev_err(&hdmi->dc->ndev->dev, "VRR driver unlock failed\n");
		return status;
	}

	status = tegra_hdmivrr_monitor_unlock(hdmi, vrr);
	if (status) {
		dev_err(&hdmi->dc->ndev->dev, "VRR monitor unlock failed\n");
		return status;
	}

	return status;
}

int tegra_hdmi_vrr_init(struct tegra_hdmi *hdmi)
{
	struct tegra_dc *dc;
	struct tegra_vrr *vrr;
	struct i2c_adapter *i2c_adap;
	int err = 0;
	struct i2c_board_info i2c_dev_info = {
		.type = "tegra_hdmi_ddcci",
		.addr = 0x37,
	};

	if (!hdmi || !hdmi->dc || !hdmi->dc->out || !hdmi->dc->out->vrr) {
		err = -EINVAL;
		goto fail;
	}

	dc = hdmi->dc;
	vrr = dc->out->vrr;

	i2c_adap = i2c_get_adapter(dc->out->ddc_bus);
	if (!i2c_adap) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't get adpater for ddcci bus %d\n",
			dc->out->ddc_bus);
		err = -EBUSY;
		goto fail;
	}

	hdmi->ddcci_i2c_client = i2c_new_device(i2c_adap, &i2c_dev_info);
	i2c_put_adapter(i2c_adap);
	if (!hdmi->ddcci_i2c_client) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't create ddcci i2c device\n");
		err = -EBUSY;
		goto fail;
	}

fail:
	return err;
}

static bool tegra_hdmivrr_fb_mode_is_compatible(struct tegra_hdmi *hdmi,
	struct fb_videomode *m)
{
	struct fb_videomode m_tmp;

	if ((m->vmode & FB_VMODE_IS_DETAILED) ||
		!(m->vmode & FB_VMODE_IS_CEA))
		return false;

	/* Currently HDMI VRR is supported only in 1920x1080p 60Hz mode.
	 * 1920x1080p 60Hz CEA mode index is 16. Strip out vmode flags
	 * that we don't expect to find in this CEA mode before comparison,
	 * to ignore non-standard flags added during hotplug.
	 *
	 * Note fb_mode_is_equal() doesn't check for pixclock;
	 * use fb_mode_is_equal_tolerance() for that purpose */
	BUG_ON(tegra_hdmi_get_cea_modedb_size(hdmi) <= 16);

	m_tmp = *m;
	m_tmp.vmode &= cea_modes[16].vmode;
	return fb_mode_is_equal(&m_tmp, &cea_modes[16]) &&
		fb_mode_is_equal_tolerance(&m_tmp, &cea_modes[16], 0);
}

/* If the monitor supports VRR, scan for VRR-capable modes.
 * Make a copy of these modes, mark them as VRR-capable.
 * and add them to the available list of modes. The original modes
 * may still be used on systems that are not VRR-compatible.
 */
void tegra_hdmivrr_update_monspecs(struct tegra_dc *dc,
	struct list_head *head)
{
	struct tegra_vrr *vrr;
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_videomode *m;
	struct fb_videomode m_vrr;
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (!head)
		return;

	vrr = dc->out->vrr;

	if (!vrr)
		return;

#ifdef VRR_AUTHENTICATION_ENABLED
	if (!vrr->capability)
		return;
#endif

	/* Check whether VRR modes were already added */
	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;

		if (m->vmode & FB_VMODE_VRR)
			return;
	}

	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;

		/* VRR modes will be added to the end of the list;
		 * don't add them twice. */
		if (m->vmode & FB_VMODE_VRR)
			break;

		if (tegra_hdmivrr_fb_mode_is_compatible(hdmi, m)) {
			m_vrr = *m;
			m_vrr.vmode |= FB_VMODE_VRR;
			fb_add_videomode(&m_vrr, head);
		}
	}
}

/* Active or deactivate VRR mode on the monitor side */
void _tegra_hdmivrr_activate(struct tegra_hdmi *hdmi, bool activate)
{
	struct tegra_dc *dc = hdmi->dc;
	struct tegra_vrr *vrr  = dc->out->vrr;
	struct fb_videomode *fbmode = tegra_fb_get_mode(dc);
	int frametime_ms = (int)div64_s64(dc->frametime_ns, NS_IN_MS);

	if (!vrr || !fbmode || !(dc->mode.vmode & FB_VMODE_VRR))
		return;

	if (activate) {
		/*
		   Inform VRR monitor to turn on VRR mode by increase
		   vertical backporch by 2.
		   The monitor needs a few frames of standard timing
		   before activating VRR mode.
		   */
		msleep(frametime_ms * 20);
		dc->mode.v_back_porch = fbmode->upper_margin + 2;
	} else
		dc->mode.v_back_porch = fbmode->upper_margin;

	_tegra_dc_set_mode(dc, &dc->mode);
	tegra_dc_update_mode(dc);
	msleep(frametime_ms * 2);
}

int tegra_hdmivrr_setup(struct tegra_hdmi *hdmi)
{
	int status;
	struct tegra_dc *dc;
	struct tegra_vrr *vrr;

	if (!hdmi || !hdmi->dc || !hdmi->dc->out || !hdmi->dc->out->vrr)
		return -EINVAL;

	dc = hdmi->dc;
	vrr = dc->out->vrr;
	/* TODO: Remove this conditional
	 * once we have support for authentication
	 * */
#ifdef VRR_AUTHENTICATION_ENABLED
	status = tegra_hdmivrr_is_vrr_capable(hdmi);
	if (status)
		goto fail;

	status = tegra_hdmivrr_page_init(hdmi);
	if (status)
		goto fail;

	status = tegra_hdmivrr_authentication(hdmi, vrr);
	if (status)
		goto fail;
#endif

	vrr->capability = 1;
	goto exit;
fail:
	vrr->capability = 0;
exit:
	return status;
}

