/*
 * ov10823.c - ov10823 sensor driver
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/camera_common.h>
#include <media/ov10823.h>

#include "ov10823_mode_tbls.h"

#define OV10823_MAX_COARSE_DIFF	8

#define OV10823_GAIN_SHIFT		8
#define OV10823_MIN_GAIN		(1 << OV10823_GAIN_SHIFT)
#define OV10823_MAX_GAIN \
	((15 << OV10823_GAIN_SHIFT) | (1 << (OV10823_GAIN_SHIFT - 1)))
#define OV10823_MIN_FRAME_LENGTH		(0x09E0)
#define OV10823_MAX_FRAME_LENGTH		(0x7FFF)
#define OV10823_MIN_EXPOSURE_COARSE		(0x8)
#define OV10823_MAX_EXPOSURE_COARSE	\
	(OV10823_MAX_FRAME_LENGTH-OV10823_MAX_COARSE_DIFF)

#define OV10823_DEFAULT_GAIN		OV10823_MIN_GAIN
#define OV10823_DEFAULT_FRAME_LENGTH		OV10823_MIN_FRAME_LENGTH
#define OV10823_DEFAULT_EXPOSURE_COARSE	\
	(OV10823_DEFAULT_FRAME_LENGTH-OV10823_MAX_COARSE_DIFF)

#define OV10823_DEFAULT_MODE		OV10823_MODE_4336X2440
#define OV10823_DEFAULT_WIDTH		4336
#define OV10823_DEFAULT_HEIGHT		2440
#define OV10823_DEFAULT_DATAFMT		V4L2_MBUS_FMT_SRGGB10_1X10
#define OV10823_DEFAULT_CLK_FREQ	26000000

#define REGW_SID	0x300c
#define DEFAULT_ADDR	0x10
#define MASTER_ADDR	0x22

static bool assigned;

struct ov10823 {
	struct mutex			ov10823_camera_lock;
	struct camera_common_power_rail	power;
	int				num_ctrls;
	u32				cam0_addr;
	u32				cam1_addr;
	u32				cam2_addr;
	int				cam0_sid_gpio;
	int				cam1_sid_gpio;
	int				cam2_sid_gpio;
	int				mcu_boot_gpio;
	int				mcu_reset_gpio;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct i2c_client		*i2c_client;
	struct v4l2_subdev		*subdev;
	struct media_pad		pad;

	s32				group_hold_prev;
	bool			group_hold_en;
	struct regmap			*regmap;
	struct camera_common_data	*s_data;
	struct camera_common_pdata	*pdata;
	struct v4l2_ctrl		*ctrls[];
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int ov10823_change_i2c_addr(struct ov10823 *priv, u8 new_i2c_addr)
{
	int ret = -ENODEV;
	struct i2c_msg msg;
	unsigned char data[3];

	int retry = 0;

	dev_dbg(&priv->i2c_client->dev, "%s changing addr from %x to %x",
		__func__, DEFAULT_ADDR, new_i2c_addr);

	data[0] = (REGW_SID >> 8) & 0xff;
	data[1] = REGW_SID & 0xff;
	data[2] = ((new_i2c_addr) << 1) & 0xff;

	msg.addr = DEFAULT_ADDR;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		ret = i2c_transfer(priv->i2c_client->adapter, &msg, 1);

		if (ret == 1)
			return 0;

		retry++;

		dev_dbg(&priv->i2c_client->dev, "%s i2c transfer failed %d\n",
			__func__, retry);

		usleep_range(3000, 3100);
	} while (retry <= 3);

	return ret;
}

u16 ov10823_to_gain(u32 rep, int shift)
{
	u16 gain;
	int gain_int;
	int gain_dec;
	int min_int = (1 << shift);

	if (rep < OV10823_MIN_GAIN)
		rep = OV10823_MIN_GAIN;
	else if (rep > OV10823_MAX_GAIN)
		rep = OV10823_MAX_GAIN;

	/* shift indicates number of least significant bits
	 * used for decimal representation of gain */
	gain_int = (int)(rep >> shift);
	gain_dec = (int)(rep & ~(0xffff << shift));

	/* derived from formulat gain = (x * 16 + 0.5) */
	gain = ((gain_int * min_int + gain_dec) * 32 + min_int) / (2 * min_int);

	return gain;
}

static int ov10823_g_volatile_ctrl(struct v4l2_ctrl *ctrl);
static int ov10823_s_ctrl(struct v4l2_ctrl *ctrl);

static const struct v4l2_ctrl_ops ov10823_ctrl_ops = {
	.g_volatile_ctrl = ov10823_g_volatile_ctrl,
	.s_ctrl		= ov10823_s_ctrl,
};

static struct v4l2_ctrl_config ctrl_config_list[] = {
/* Do not change the name field for the controls! */
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = OV10823_MIN_GAIN,
		.max = OV10823_MAX_GAIN,
		.def = OV10823_DEFAULT_GAIN,
		.step = 1,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_FRAME_LENGTH,
		.name = "Frame Length",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = OV10823_MIN_FRAME_LENGTH,
		.max = OV10823_MAX_FRAME_LENGTH,
		.def = OV10823_DEFAULT_FRAME_LENGTH,
		.step = 1,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_COARSE_TIME,
		.name = "Coarse Time",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = OV10823_MIN_EXPOSURE_COARSE,
		.max = OV10823_MAX_EXPOSURE_COARSE,
		.def = OV10823_DEFAULT_EXPOSURE_COARSE,
		.step = 1,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_GROUP_HOLD,
		.name = "Group Hold",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.min = 0,
		.max = ARRAY_SIZE(switch_ctrl_qmenu) - 1,
		.menu_skip_mask = 0,
		.def = 0,
		.qmenu_int = switch_ctrl_qmenu,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_HDR_EN,
		.name = "HDR enable",
		.type = V4L2_CTRL_TYPE_INTEGER_MENU,
		.min = 0,
		.max = ARRAY_SIZE(switch_ctrl_qmenu) - 1,
		.menu_skip_mask = 0,
		.def = 0,
		.qmenu_int = switch_ctrl_qmenu,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_OTP_DATA,
		.name = "OTP Data",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = OV10823_OTP_STR_SIZE,
		.step = 2,
	},
	{
		.ops = &ov10823_ctrl_ops,
		.id = V4L2_CID_FUSE_ID,
		.name = "Fuse ID",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = OV10823_FUSE_ID_STR_SIZE,
		.step = 2,
	},
};

static inline void ov10823_get_frame_length_regs(ov10823_reg *regs,
				u16 frame_length, bool master)
{
	/* 2 registers for FL, i.e., 2-byte FL */
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
	if (master) {
		(regs + 2)->addr = 0x3830;
		(regs + 2)->val = ((frame_length - 4) >> 8) & 0xff;
		(regs + 3)->addr = 0x3831;
		(regs + 3)->val = (frame_length - 4) & 0xff;
	} else {
		(regs + 2)->addr = 0x3826;
		(regs + 2)->val = ((frame_length - 4) >> 8) & 0xff;
		(regs + 3)->addr = 0x3827;
		(regs + 3)->val = (frame_length - 4) & 0xff;
	}
	(regs + 4)->addr = OV10823_TABLE_END;
	(regs + 4)->val = 0;
}

static inline void ov10823_get_coarse_time_regs(ov10823_reg *regs,
				u16 coarse_time)
{
	/* 3 registers for CT, i.e., 3-byte CT */
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;
	(regs + 3)->addr = OV10823_TABLE_END;
	(regs + 3)->val = 0;
}

static inline void ov10823_get_gain_reg(ov10823_reg *regs,
				u16 gain)
{
	/* 2 register for gain, i.e., 2-byte gain */
	regs->addr = 0x350a;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = 0x350b;
	(regs + 1)->val = (gain) & 0xff;
	(regs + 2)->addr = OV10823_TABLE_END;
	(regs + 2)->val = 0;
}

static inline int ov10823_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;

	return regmap_read(priv->regmap, addr, (unsigned int *) val);
}

static int ov10823_write_reg(struct camera_common_data *s_data,
		u16 addr, u8 val)
{
	int err;
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int ov10823_write_table(struct ov10823 *priv,
				const ov10823_reg table[])
{
	return regmap_util_write_table_8(priv->regmap,
					 table,
					 NULL, 0,
					 OV10823_TABLE_WAIT_MS,
					 OV10823_TABLE_END);
}

static int ov10823_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power on\n", __func__);

	if (priv->pdata->power_on) {
		err = priv->pdata->power_on(pw);
		if (err)
			pr_err("%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	usleep_range(5350, 5360);
	pw->state = SWITCH_ON;
	return 0;
}

static int ov10823_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power off\n", __func__);
	ov10823_write_table(priv, mode_table[OV10823_MODE_STOP_STREAM]);

	if (priv->pdata->power_off) {
		err = priv->pdata->power_off(pw);
		if (err)
			pr_err("%s failed.\n", __func__);
		else
			goto power_off_done;
	}

	return err;

power_off_done:
	pw->state = SWITCH_OFF;
	return 0;
}

static int ov10823_power_put(struct ov10823 *priv)
{
	return 0;
}

static int ov10823_power_get(struct ov10823 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;
	const char *mclk_name;
	int err = 0;

	mclk_name = priv->pdata->mclk_name ?
		    priv->pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(&priv->i2c_client->dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(&priv->i2c_client->dev,
			"unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	pw->state = SWITCH_OFF;
	return err;
}

static int ov10823_set_gain(struct ov10823 *priv, s32 val);
static int ov10823_set_frame_length(struct ov10823 *priv, s32 val);
static int ov10823_set_coarse_time(struct ov10823 *priv, s32 val);

static int ov10823_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;
	struct v4l2_control control;
	int err;
	int master = 0;

	if (!enable) {
		dev_dbg(&client->dev, "%s: stream off\n", __func__);
		return ov10823_write_table(priv,
			mode_table[OV10823_MODE_STOP_STREAM]);
		}

	if (client->addr != MASTER_ADDR)
		master = 1;

	dev_dbg(&client->dev, "%s: write mode table %d\n",
		__func__, s_data->mode);
	err = ov10823_write_table(priv, mode_table[s_data->mode]);
	dev_dbg(&client->dev, "%s: write fsync table %d\n", __func__, master);
	err = ov10823_write_table(priv, fsync_table[master]);
	if (err)
		goto exit;

	/* write list of override regs for the asking frame length,
	 * coarse integration time, and gain. Failures to write
	 * overrides are non-fatal. */
	control.id = V4L2_CID_GAIN;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= ov10823_set_gain(priv, control.value);
	if (err)
		dev_dbg(&client->dev, "%s: error gain override\n", __func__);

	control.id = V4L2_CID_FRAME_LENGTH;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= ov10823_set_frame_length(priv, control.value);
	if (err)
		dev_dbg(&client->dev,
			"%s: error frame length override\n", __func__);

	control.id = V4L2_CID_COARSE_TIME;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= ov10823_set_coarse_time(priv, control.value);
	if (err)
		dev_dbg(&client->dev,
			"%s: error coarse time override\n", __func__);

	dev_dbg(&client->dev, "%s: stream on\n", __func__);
	err = ov10823_write_table(priv, mode_table[OV10823_MODE_START_STREAM]);
	if (err)
		goto exit;

	return 0;
exit:
	dev_dbg(&client->dev, "%s: error setting stream\n", __func__);
	return err;
}

static struct v4l2_subdev_video_ops ov10823_subdev_video_ops = {
	.s_stream	= ov10823_s_stream,
	.s_mbus_fmt	= camera_common_s_fmt,
	.g_mbus_fmt	= camera_common_g_fmt,
	.try_mbus_fmt	= camera_common_try_fmt,
	.enum_mbus_fmt	= camera_common_enum_fmt,
	.g_mbus_config	= camera_common_g_mbus_config,
};

static struct v4l2_subdev_core_ops ov10823_subdev_core_ops = {
	.s_power	= camera_common_s_power,
};

static struct v4l2_subdev_ops ov10823_subdev_ops = {
	.core	= &ov10823_subdev_core_ops,
	.video	= &ov10823_subdev_video_ops,
};

static struct of_device_id ov10823_of_match[] = {
	{ .compatible = "nvidia,ov10823", },
	{ },
};

static struct camera_common_sensor_ops ov10823_common_ops = {
	.power_on = ov10823_power_on,
	.power_off = ov10823_power_off,
	.write_reg = ov10823_write_reg,
	.read_reg = ov10823_read_reg,
};

static int ov10823_set_group_hold(struct ov10823 *priv)
{
	int err;
	int gh_prev = switch_ctrl_qmenu[priv->group_hold_prev];

	if (priv->group_hold_en == true && gh_prev == SWITCH_OFF) {
		/* group hold start */
		err = ov10823_write_reg(priv->s_data,
				       OV10823_GROUP_HOLD_ADDR, 0x00);
		if (err)
			goto fail;
		priv->group_hold_prev = 1;
	} else if (priv->group_hold_en == false && gh_prev == SWITCH_ON) {
		/* group hold end */
		err = ov10823_write_reg(priv->s_data,
				       OV10823_GROUP_HOLD_ADDR, 0x10);
		/* quick launch */
		err |= ov10823_write_reg(priv->s_data,
				       OV10823_GROUP_HOLD_ADDR, 0xA0);
		if (err)
			goto fail;
		priv->group_hold_prev = 0;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: Group hold control error\n", __func__);
	return err;
}

static int ov10823_set_gain(struct ov10823 *priv, s32 val)
{
	ov10823_reg reg_list[3];
	int err;
	u16 gain;

	/* max_gain 15.5x ---> 0x350A=0x00, 0x350B=0xF8 */
	/* min_gain 1.0x  ---> 0x350A=0x00, 0x350B=0x10 */
	/* translate value */
	gain = ov10823_to_gain((u32)val, OV10823_GAIN_SHIFT);

	dev_dbg(&priv->i2c_client->dev,
		 "%s: gain: %d\n", __func__, gain);

	ov10823_get_gain_reg(reg_list, gain);
	ov10823_set_group_hold(priv);
	err = ov10823_write_table(priv, reg_list);
	if (err)
		goto fail;

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: GAIN control error\n", __func__);
	return err;
}

static int ov10823_set_frame_length(struct ov10823 *priv, s32 val)
{
	ov10823_reg reg_list[5];
	int err;
	u16 frame_length;
	bool master = false;

	master = &priv->i2c_client->dev == MASTER_ADDR;

	frame_length = (u16)val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: frame_length: %d\n", __func__, frame_length);

	ov10823_get_frame_length_regs(reg_list, frame_length, master);
	ov10823_set_group_hold(priv);
	err = ov10823_write_table(priv, reg_list);
	if (err)
		goto fail;

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: FRAME_LENGTH control error\n", __func__);
	return err;
}

static int ov10823_set_coarse_time(struct ov10823 *priv, s32 val)
{
	ov10823_reg reg_list[4];
	int err;
	u16 coarse_time;

	coarse_time = (u16)val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: coarse_time: %d\n", __func__, coarse_time);

	ov10823_get_coarse_time_regs(reg_list, coarse_time);
	ov10823_set_group_hold(priv);
	err = ov10823_write_table(priv, reg_list);
	if (err)
		goto fail;

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: COARSE_TIME control error\n", __func__);
	return err;
}

static int ov10823_read_otp(struct ov10823 *priv, u8 *buf,
		u16 addr, int size)
{
	int err;

	err = ov10823_write_reg(priv->s_data, OV10823_ISP_CTRL_ADDR, 0x00);
	if (err)
		return err;
	/* Start streaming before write or read */
	err = ov10823_write_reg(priv->s_data, 0x0100, 0x01);
	if (err)
		return err;
	msleep(20);

	/* By default otp loading works in auto mode, but we can switch to
	   manual mode through OV10823_OTP_MODE_CTRL_ADDR[6] and the start
	   addr and end addr of manual mode can be configured by registers
	   accordingly
	 */

	/* Loading enable
	   1: manual mode
	   0: auto mode */
	err = ov10823_write_reg(priv->s_data, OV10823_OTP_LOAD_CTRL_ADDR, 0x01);
	if (err)
		return err;

	msleep(20);
	err = regmap_bulk_read(priv->regmap, addr, buf, size);
	if (err)
		return err;

	return 0;
}

static int ov10823_otp_setup(struct ov10823 *priv)
{
	int err;
	int i;
	struct v4l2_ctrl *ctrl;
	u8 otp_buf[OV10823_OTP_SIZE];

	err = camera_common_s_power(priv->subdev, true);
	if (err)
		return -ENODEV;

	ov10823_read_otp(priv, &otp_buf[0],
				   OV10823_OTP_SRAM_START_ADDR,
				   OV10823_OTP_SIZE);

	ctrl = v4l2_ctrl_find(&priv->ctrl_handler, V4L2_CID_OTP_DATA);
	if (!ctrl) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return -EINVAL;
	}

	for (i = 0; i < OV10823_OTP_SIZE; i++)
		sprintf(&ctrl->p_new.p_char[i*2], "%02x",
			otp_buf[i]);
	ctrl->p_cur.p_char = ctrl->p_new.p_char;

	err = camera_common_s_power(priv->subdev, false);
	if (err)
		return -ENODEV;

	return 0;
}

static int ov10823_fuse_id_setup(struct ov10823 *priv)
{
	int err;
	int i;
	struct v4l2_ctrl *ctrl;
	u8 fuse_id[OV10823_FUSE_ID_SIZE];

	err = camera_common_s_power(priv->subdev, true);
	if (err)
		return -ENODEV;

	ov10823_read_otp(priv, &fuse_id[0],
				   OV10823_FUSE_ID_OTP_BASE_ADDR,
				   OV10823_FUSE_ID_SIZE);

	ctrl = v4l2_ctrl_find(&priv->ctrl_handler, V4L2_CID_FUSE_ID);
	if (!ctrl) {
		dev_err(&priv->i2c_client->dev,
			"could not find device ctrl.\n");
		return -EINVAL;
	}

	for (i = 0; i < OV10823_FUSE_ID_SIZE; i++)
		sprintf(&ctrl->p_new.p_char[i*2], "%02x",
			fuse_id[i]);
	ctrl->p_cur.p_char = ctrl->p_new.p_char;

	err = camera_common_s_power(priv->subdev, false);
	if (err)
		return -ENODEV;

	return 0;
}

static int ov10823_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov10823 *priv =
		container_of(ctrl->handler, struct ov10823, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	default:
			pr_err("%s: unknown ctrl id.\n", __func__);
			return -EINVAL;
	}

	return err;
}

static int ov10823_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov10823 *priv =
		container_of(ctrl->handler, struct ov10823, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		err = ov10823_set_gain(priv, ctrl->val);
		break;
	case V4L2_CID_FRAME_LENGTH:
		err = ov10823_set_frame_length(priv, ctrl->val);
		break;
	case V4L2_CID_COARSE_TIME:
		err = ov10823_set_coarse_time(priv, ctrl->val);
		break;
	case V4L2_CID_GROUP_HOLD:
		if (switch_ctrl_qmenu[ctrl->val] == SWITCH_ON) {
			priv->group_hold_en = true;
		} else {
			priv->group_hold_en = false;
			err = ov10823_set_group_hold(priv);
		}
		break;
	case V4L2_CID_HDR_EN:
		break;
	default:
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}

	return err;
}

static int ov10823_ctrls_init(struct ov10823 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	struct v4l2_ctrl *ctrl;
	int num_ctrls;
	int err;
	int i;

	dev_dbg(&client->dev, "%s++\n", __func__);

	num_ctrls = ARRAY_SIZE(ctrl_config_list);
	v4l2_ctrl_handler_init(&priv->ctrl_handler, num_ctrls);

	for (i = 0; i < num_ctrls; i++) {
		ctrl = v4l2_ctrl_new_custom(&priv->ctrl_handler,
			&ctrl_config_list[i], NULL);
		if (ctrl == NULL) {
			dev_err(&client->dev, "Failed to init %s ctrl\n",
				ctrl_config_list[i].name);
			continue;
		}

		if (ctrl_config_list[i].type == V4L2_CTRL_TYPE_STRING &&
			ctrl_config_list[i].flags & V4L2_CTRL_FLAG_READ_ONLY) {
			ctrl->p_new.p_char = devm_kzalloc(&client->dev,
				ctrl_config_list[i].max + 1, GFP_KERNEL);
		}
		priv->ctrls[i] = ctrl;
	}

	priv->num_ctrls = num_ctrls;
	priv->subdev->ctrl_handler = &priv->ctrl_handler;
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls\n",
			priv->ctrl_handler.error);
		err = priv->ctrl_handler.error;
		goto error;
	}

	err = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (err) {
		dev_err(&client->dev,
			"Error %d setting default controls\n", err);
		goto error;
	}

	err = ov10823_otp_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"Error %d reading otp data\n", err);
		goto error;
	}

	err = ov10823_fuse_id_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"Error %d reading fuse id data\n", err);
		goto error;
	}

	return 0;

error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return err;
}

MODULE_DEVICE_TABLE(of, ov10823_of_match);

static struct camera_common_pdata *ov10823_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int gpio;
	int err;

	match = of_match_device(ov10823_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(&client->dev,
			   sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(np, "mclk",
				      &board_priv_pdata->mclk_name);
	if (err) {
		dev_err(&client->dev, "mclk not in DT\n");
		goto error;
	}

	gpio = of_get_named_gpio(np, "pwdn-gpios", 0);
	if (gpio < 0) {
		dev_dbg(&client->dev, "pwdn gpios not in DT\n");
		gpio = 0;
	}
	board_priv_pdata->pwdn_gpio = (unsigned int)gpio;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		dev_dbg(&client->dev, "reset gpios not in DT\n");
		gpio = 0;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	return board_priv_pdata;

error:
	devm_kfree(&client->dev, board_priv_pdata);
	return NULL;
}

static void ov10823_i2c_addr_assign(struct ov10823 *priv)
{
	int err = 0;

	gpio_set_value(priv->mcu_boot_gpio, 0);
	gpio_set_value(priv->mcu_reset_gpio, 0);
	msleep_range(1);
	gpio_set_value(priv->mcu_reset_gpio, 1);

	gpio_set_value(priv->cam0_sid_gpio, 0);
	gpio_set_value(priv->cam1_sid_gpio, 1);
	gpio_set_value(priv->cam2_sid_gpio, 1);


	msleep_range(300);
	err = ov10823_change_i2c_addr(priv, priv->cam0_addr);
	if (err < 0) {
		dev_dbg(&priv->i2c_client->dev,
			"[ov10823] can't change i2c address cam #0\n");
	}

	gpio_set_value(priv->cam1_sid_gpio, 0);
	msleep_range(1);
	err = ov10823_change_i2c_addr(priv, priv->cam1_addr);
	if (err < 0) {
		dev_dbg(&priv->i2c_client->dev,
			"[ov10823] can't change i2c address cam #1\n");
	}

	gpio_set_value(priv->cam2_sid_gpio, 0);
	msleep_range(1);
	err = ov10823_change_i2c_addr(priv, priv->cam2_addr);
	if (err < 0) {
		dev_dbg(&priv->i2c_client->dev,
			"[ov10823] can't change i2c address cam #2\n");
	}

	assigned = true;
}
static int ov10823_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops ov10823_subdev_internal_ops = {
	.open = ov10823_open,
};

static const struct media_entity_operations ov10823_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int ov10823_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct camera_common_data *common_data;
	struct ov10823 *priv;
	char dev_name[10];
	int err;

	pr_info("[OV10823]: probing v4l2 sensor.\n");

	common_data = devm_kzalloc(&client->dev,
			    sizeof(struct camera_common_data), GFP_KERNEL);

	priv = devm_kzalloc(&client->dev,
			sizeof(struct ov10823) + sizeof(struct v4l2_ctrl *) *
			ARRAY_SIZE(ctrl_config_list),
			GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	priv->pdata = ov10823_parse_dt(client);
	if (!priv->pdata) {
		dev_err(&client->dev, "unable to get platform data\n");
		return -EFAULT;
	}

	common_data->ops		= &ov10823_common_ops;
	common_data->ctrl_handler	= &priv->ctrl_handler;
	common_data->i2c_client		= client;
	common_data->frmfmt		= &ov10823_frmfmt[0];
	common_data->colorfmt		= camera_common_find_datafmt(
					  OV10823_DEFAULT_DATAFMT);
	common_data->power		= &priv->power;
	common_data->ctrls		= priv->ctrls;
	common_data->priv		= (void *)priv;
	common_data->numctrls		= ARRAY_SIZE(ctrl_config_list);
	common_data->numfmts		= ARRAY_SIZE(ov10823_frmfmt);
	common_data->def_mode		= OV10823_DEFAULT_MODE;
	common_data->def_width		= OV10823_DEFAULT_WIDTH;
	common_data->def_height		= OV10823_DEFAULT_HEIGHT;
	common_data->def_clk_freq	= OV10823_DEFAULT_CLK_FREQ;

	priv->i2c_client		= client;
	priv->s_data			= common_data;
	priv->subdev			= &common_data->subdev;
	priv->subdev->dev		= &client->dev;
	priv->group_hold_prev		= 0;

	err = ov10823_power_get(priv);
	if (err)
		return err;

	if (!assigned) {
		of_property_read_u32(np, "cam0-i2c-addr", &priv->cam0_addr);
		of_property_read_u32(np, "cam1-i2c-addr", &priv->cam1_addr);
		of_property_read_u32(np, "cam2-i2c-addr", &priv->cam2_addr);

		priv->cam0_sid_gpio =
			of_get_named_gpio(np, "cam0-sid-gpios", 0);
		priv->cam1_sid_gpio =
			of_get_named_gpio(np, "cam1-sid-gpios", 0);
		priv->cam2_sid_gpio =
			of_get_named_gpio(np, "cam2-sid-gpios", 0);
		priv->mcu_boot_gpio =
			of_get_named_gpio(np, "mcu-boot-gpios", 0);
		priv->mcu_reset_gpio =
			of_get_named_gpio(np, "mcu-reset-gpios", 0);

		ov10823_i2c_addr_assign(priv);
	}

	err = camera_common_parse_ports(client, common_data);
	if (err) {
		dev_err(&client->dev, "Failed to find port info\n");
		return err;
	}
	sprintf(dev_name, "ov10823_%c", common_data->csi_port + 'a');
	dev_dbg(&client->dev, "%s: name %s\n", __func__, dev_name);
	camera_common_create_debugfs(common_data, dev_name);

	v4l2_i2c_subdev_init(&common_data->subdev, client,
			     &ov10823_subdev_ops);

	err = ov10823_ctrls_init(priv);
	if (err)
		return err;

	priv->subdev->internal_ops = &ov10823_subdev_internal_ops;
	priv->subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

#if defined(CONFIG_MEDIA_CONTROLLER)
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	priv->subdev->entity.ops = &ov10823_media_ops;
	err = media_entity_init(&priv->subdev->entity, 1, &priv->pad, 0);
	if (err < 0) {
		dev_err(&client->dev, "unable to init media entity\n");
		return err;
	}
#endif

	err = v4l2_async_register_subdev(priv->subdev);
	if (err)
		return err;

	dev_dbg(&client->dev, "Detected OV10823 sensor\n");

	return 0;
}

static int
ov10823_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct ov10823 *priv = (struct ov10823 *)s_data->priv;

	v4l2_async_unregister_subdev(priv->subdev);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	ov10823_power_put(priv);
	camera_common_remove_debugfs(s_data);

	return 0;
}

static const struct i2c_device_id ov10823_id[] = {
	{ "ov10823", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ov10823_id);

static struct i2c_driver ov10823_i2c_driver = {
	.driver = {
		.name = "ov10823",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ov10823_of_match),
	},
	.probe = ov10823_probe,
	.remove = ov10823_remove,
	.id_table = ov10823_id,
};

module_i2c_driver(ov10823_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for Omnivison OV10823");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");
