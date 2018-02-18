/*
 * imx219.c - imx219 sensor driver
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <dt-bindings/gpio/tegra-gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/camera_common.h>
#include <media/soc_camera.h>
#include <media/imx219.h>

#include "imx219_mode_tbls.h"

#define IMX219_MAX_COARSE_DIFF		4

#define IMX219_GAIN_SHIFT		8
#define IMX219_MIN_GAIN		(1 << IMX219_GAIN_SHIFT)
#define IMX219_MAX_GAIN		(16 << IMX219_GAIN_SHIFT)
#define IMX219_MIN_FRAME_LENGTH	(0x9C3)
#define IMX219_MAX_FRAME_LENGTH	(0xFFFF)
#define IMX219_MIN_EXPOSURE_COARSE	(0x0001)
#define IMX219_MAX_EXPOSURE_COARSE	\
	(IMX219_MAX_FRAME_LENGTH-IMX219_MAX_COARSE_DIFF)

#define IMX219_DEFAULT_GAIN		IMX219_MIN_GAIN
#define IMX219_DEFAULT_FRAME_LENGTH	(0x09C3)
#define IMX219_DEFAULT_EXPOSURE_COARSE	\
	(IMX219_DEFAULT_FRAME_LENGTH-IMX219_MAX_COARSE_DIFF)

#define IMX219_DEFAULT_MODE	IMX219_MODE_3280x2464
#define IMX219_DEFAULT_WIDTH	3280
#define IMX219_DEFAULT_HEIGHT	2464
#define IMX219_DEFAULT_DATAFMT	V4L2_MBUS_FMT_SRGGB10_1X10
#define IMX219_DEFAULT_CLK_FREQ	24000000

struct imx219 {
	struct camera_common_power_rail	power;
	int				num_ctrls;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct i2c_client		*i2c_client;
	struct v4l2_subdev		*subdev;
	struct media_pad		pad;
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

static int imx219_g_volatile_ctrl(struct v4l2_ctrl *ctrl);
static int imx219_s_ctrl(struct v4l2_ctrl *ctrl);

static const struct v4l2_ctrl_ops imx219_ctrl_ops = {
	.g_volatile_ctrl = imx219_g_volatile_ctrl,
	.s_ctrl	= imx219_s_ctrl,
};

static struct v4l2_ctrl_config ctrl_config_list[] = {
/* Do not change the name field for the controls! */
	{
		.ops = &imx219_ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX219_MIN_GAIN,
		.max = IMX219_MAX_GAIN,
		.def = IMX219_DEFAULT_GAIN,
		.step = 1,
	},
	{
		.ops = &imx219_ctrl_ops,
		.id = V4L2_CID_FRAME_LENGTH,
		.name = "Frame Length",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX219_MIN_FRAME_LENGTH,
		.max = IMX219_MAX_FRAME_LENGTH,
		.def = IMX219_DEFAULT_FRAME_LENGTH,
		.step = 1,
	},
	{
		.ops = &imx219_ctrl_ops,
		.id = V4L2_CID_COARSE_TIME,
		.name = "Coarse Time",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = IMX219_MIN_EXPOSURE_COARSE,
		.max = IMX219_MAX_EXPOSURE_COARSE,
		.def = IMX219_DEFAULT_EXPOSURE_COARSE,
		.step = 1,
	},
	{
		.ops = &imx219_ctrl_ops,
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
		.ops = &imx219_ctrl_ops,
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
		.ops = &imx219_ctrl_ops,
		.id = V4L2_CID_FUSE_ID,
		.name = "Fuse ID",
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
		.min = 0,
		.max = IMX219_FUSE_ID_STR_SIZE,
		.step = 2,
	},
};

static inline void imx219_get_frame_length_regs(struct reg_8 *regs,
				u16 frame_length)
{
	regs->addr = IMX219_FRAME_LENGTH_ADDR_MSB;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX219_FRAME_LENGTH_ADDR_LSB;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void imx219_get_coarse_time_regs(struct reg_8 *regs,
				u16 coarse_time)
{
	regs->addr = IMX219_COARSE_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX219_COARSE_TIME_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx219_get_gain_reg(struct reg_8 *regs,
				u8 gain)
{
	regs->addr = IMX219_GAIN_ADDR;
	regs->val = gain & 0xff;
}

static int test_mode;
module_param(test_mode, int, 0644);

static inline int imx219_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	struct imx219 *priv = (struct imx219 *)s_data->priv;

	return regmap_read(priv->regmap, addr, (unsigned int *) val);
}

static int imx219_write_reg(struct camera_common_data *s_data, u16 addr, u8 val)
{
	int err;
	struct imx219 *priv = (struct imx219 *)s_data->priv;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int imx219_write_table(struct imx219 *priv,
			      const struct reg_8 table[])
{
	return regmap_util_write_table_8(priv->regmap,
					 table,
					 NULL, 0,
					 IMX219_TABLE_WAIT_MS,
					 IMX219_TABLE_END);
}

static void imx219_mclk_disable(struct camera_common_power_rail *pw)
{
	clk_disable_unprepare(pw->mclk);
}

static void imx219_mclk_enable(struct camera_common_power_rail *pw)
{
	clk_set_rate(pw->mclk, IMX219_DEFAULT_CLK_FREQ);
	clk_prepare_enable(pw->mclk);
}

static int imx219_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct imx219 *priv = (struct imx219 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power on\n", __func__);

	imx219_mclk_enable(pw);

	if (gpio_cansleep(pw->reset_gpio))
		gpio_set_value_cansleep(pw->reset_gpio, 0);
	else
		gpio_set_value(pw->reset_gpio, 0);
	usleep_range(10, 20);

	if (pw->avdd)
		err = regulator_enable(pw->avdd);
	if (err)
		goto imx219_avdd_fail;

	if (pw->iovdd)
		err = regulator_enable(pw->iovdd);
	if (err)
		goto imx219_iovdd_fail;

	if (pw->dvdd)
		err = regulator_enable(pw->dvdd);
	if (err)
		goto imx219_dvdd_fail;

	usleep_range(1, 2);
	if (gpio_cansleep(pw->reset_gpio))
		gpio_set_value_cansleep(pw->reset_gpio, 1);
	else
		gpio_set_value(pw->reset_gpio, 1);
	usleep_range(300, 310);

	pw->state = SWITCH_ON;
	return 0;

imx219_dvdd_fail:
	regulator_disable(pw->iovdd);

imx219_iovdd_fail:
	regulator_disable(pw->avdd);

imx219_avdd_fail:
	imx219_mclk_disable(pw);
	return -ENODEV;
}

static int imx219_power_off(struct camera_common_data *s_data)
{
	struct imx219 *priv = (struct imx219 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power off\n", __func__);

	usleep_range(1, 2);
	if (gpio_cansleep(pw->reset_gpio))
		gpio_set_value_cansleep(pw->reset_gpio, 0);
	else
		gpio_set_value(pw->reset_gpio, 0);
	usleep_range(1, 2);

	if (pw->dvdd)
		regulator_disable(pw->dvdd);
	if (pw->iovdd)
		regulator_disable(pw->iovdd);
	if (pw->avdd)
		regulator_disable(pw->avdd);

	imx219_mclk_disable(pw);
	pw->state = SWITCH_OFF;
	return 0;
}

static int imx219_power_put(struct imx219 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;

	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;

	return 0;
}

static int imx219_power_get(struct imx219 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;
	struct camera_common_pdata *pdata = priv->pdata;
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

	/* ananlog 2.7v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->avdd, pdata->regulators.avdd);
	/* digital 1.2v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->dvdd, pdata->regulators.dvdd);
	/* IO 1.8v */
	err |= camera_common_regulator_get(priv->i2c_client,
			&pw->iovdd, pdata->regulators.iovdd);

	if (!err)
		pw->reset_gpio = pdata->reset_gpio;

	pw->state = SWITCH_OFF;
	return err;
}

static int imx219_set_gain(struct imx219 *priv, s32 val);
static int imx219_set_frame_length(struct imx219 *priv, s32 val);
static int imx219_set_coarse_time(struct imx219 *priv, s32 val);

static int imx219_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx219 *priv = (struct imx219 *)s_data->priv;
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!enable)
		return 0;

	err = imx219_write_table(priv, mode_table[s_data->mode]);
	if (err)
		goto exit;

	return 0;
exit:
	dev_dbg(&client->dev, "%s: error setting stream\n", __func__);
	return err;
}

static struct v4l2_subdev_video_ops imx219_subdev_video_ops = {
	.s_stream	= imx219_s_stream,
	.s_mbus_fmt	= camera_common_s_fmt,
	.g_mbus_fmt	= camera_common_g_fmt,
	.try_mbus_fmt	= camera_common_try_fmt,
	.enum_mbus_fmt	= camera_common_enum_fmt,
	.g_mbus_config	= camera_common_g_mbus_config,
};

static struct v4l2_subdev_core_ops imx219_subdev_core_ops = {
	.s_power	= camera_common_s_power,
};

static struct v4l2_subdev_ops imx219_subdev_ops = {
	.core	= &imx219_subdev_core_ops,
	.video	= &imx219_subdev_video_ops,
};

static struct of_device_id imx219_of_match[] = {
	{ .compatible = "nvidia,imx219", },
	{ },
};

static struct camera_common_sensor_ops imx219_common_ops = {
	.power_on = imx219_power_on,
	.power_off = imx219_power_off,
	.write_reg = imx219_write_reg,
	.read_reg = imx219_read_reg,
};

static int imx219_set_group_hold(struct imx219 *priv, s32 val)
{
	/* IMX219 does not support group hold */
	return 0;
}

static int imx219_set_gain(struct imx219 *priv, s32 val)
{
	struct reg_8 reg;
	int err;
	u8 gain;
	int i = 0;

	/* translate value */
	gain = 256 - (256 * (1 << IMX219_GAIN_SHIFT) / val);
	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, gain);

	imx219_get_gain_reg(&reg, gain);

	err = imx219_write_reg(priv->s_data, reg.addr, reg.val);
	if (err)
		goto fail;

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: GAIN control error\n", __func__);
	return err;
}

static int imx219_set_frame_length(struct imx219 *priv, s32 val)
{
	struct reg_8 reg_list[2];
	int err;
	u16 frame_length = (u16) val;
	int i = 0;

	frame_length = val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, frame_length);

	imx219_get_frame_length_regs(reg_list, frame_length);

	for (i = 0; i < 2; i++) {
		err = imx219_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: FRAME_LENGTH control error\n", __func__);
	return err;
}

static int imx219_set_coarse_time(struct imx219 *priv, s32 val)
{
	struct reg_8 reg_list[2];
	int err;
	u16 coarse_time = (u16) val;
	int i = 0;

	coarse_time = val;

	dev_dbg(&priv->i2c_client->dev,
		 "%s: val: %d\n", __func__, coarse_time);

	imx219_get_coarse_time_regs(reg_list, coarse_time);

	for (i = 0; i < 2; i++) {
		err = imx219_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		 "%s: COARSE_TIME control error\n", __func__);
	return err;
}

static int imx219_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx219 *priv =
		container_of(ctrl->handler, struct imx219, ctrl_handler);
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

static int imx219_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx219 *priv =
		container_of(ctrl->handler, struct imx219, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		err = imx219_set_gain(priv, ctrl->val);
		break;
	case V4L2_CID_FRAME_LENGTH:
		err = imx219_set_frame_length(priv, ctrl->val);
		break;
	case V4L2_CID_COARSE_TIME:
		err = imx219_set_coarse_time(priv, ctrl->val);
		break;
	case V4L2_CID_GROUP_HOLD:
		err = imx219_set_group_hold(priv, ctrl->val);
		break;
	case V4L2_CID_HDR_EN:
		break;
	case V4L2_CID_FUSE_ID:
		break;
	default:
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}

	return err;
}

static int imx219_ctrls_init(struct imx219 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	struct v4l2_ctrl *ctrl;
	int num_ctrls;
	int err;
	int i;

	dev_info(&client->dev, "%s++\n", __func__);

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
			if (!ctrl->p_new.p_char)
				return -ENOMEM;
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

	return 0;

error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return err;
}

MODULE_DEVICE_TABLE(of, imx219_of_match);

static struct camera_common_pdata *imx219_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int gpio, err;

	match = of_match_device(imx219_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(&client->dev,
			   sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err) {
		dev_err(&client->dev, "mclk not in DT\n");
		goto error;
	}

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		dev_err(&client->dev, "reset gpios not in DT, use %d\n", gpio);
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	err = of_property_read_string(np, "avdd-reg",
			&board_priv_pdata->regulators.avdd);
	if (err) {
		dev_err(&client->dev, "avdd-reg not in DT\n");
		goto error;
	}
	err = of_property_read_string(np, "dvdd-reg",
			&board_priv_pdata->regulators.dvdd);
	if (err) {
		dev_err(&client->dev, "dvdd-reg not in DT\n");
		goto error;
	}
	err = of_property_read_string(np, "iovdd-reg",
			&board_priv_pdata->regulators.iovdd);
	if (err) {
		dev_err(&client->dev, "iovdd-reg not in DT\n");
		goto error;
	}

	return board_priv_pdata;

error:
	devm_kfree(&client->dev, board_priv_pdata);
	return NULL;
}

static int imx219_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops imx219_subdev_internal_ops = {
	.open = imx219_open,
};

static const struct media_entity_operations imx219_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int imx219_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct camera_common_data *common_data;
	struct device_node *node = client->dev.of_node;
	struct imx219 *priv;
	char node_name[10];
	int err;
	int gpio;
	struct device_node *np = client->dev.of_node;

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	common_data = devm_kzalloc(&client->dev,
			    sizeof(struct camera_common_data), GFP_KERNEL);
	if (!common_data)
		return -ENOMEM;

	priv = devm_kzalloc(&client->dev,
			    sizeof(struct imx219) + sizeof(struct v4l2_ctrl *) *
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

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0)
		return gpio;

	priv->pdata = imx219_parse_dt(client);
	if (!priv->pdata) {
		dev_err(&client->dev, "unable to get platform data\n");
		return -EFAULT;
	}

	common_data->ops		= &imx219_common_ops;
	common_data->ctrl_handler	= &priv->ctrl_handler;
	common_data->i2c_client		= client;
	common_data->frmfmt		= &imx219_frmfmt[0];
	common_data->colorfmt		= camera_common_find_datafmt(
					  IMX219_DEFAULT_DATAFMT);
	common_data->power		= &priv->power;
	common_data->ctrls		= priv->ctrls;
	common_data->priv		= (void *)priv;
	common_data->numctrls		= ARRAY_SIZE(ctrl_config_list);
	common_data->numfmts		= ARRAY_SIZE(imx219_frmfmt);
	common_data->def_mode		= IMX219_DEFAULT_MODE;
	common_data->def_width		= IMX219_DEFAULT_WIDTH;
	common_data->def_height		= IMX219_DEFAULT_HEIGHT;
	common_data->def_clk_freq	= IMX219_DEFAULT_CLK_FREQ;

	priv->i2c_client		= client;
	priv->s_data			= common_data;
	priv->subdev			= &common_data->subdev;
	priv->subdev->dev		= &client->dev;

	err = imx219_power_get(priv);
	if (err)
		return err;

	camera_common_create_debugfs(common_data, "imx219");

	v4l2_i2c_subdev_init(&common_data->subdev, client, &imx219_subdev_ops);

	err = imx219_ctrls_init(priv);
	if (err)
		return err;

	priv->subdev->internal_ops = &imx219_subdev_internal_ops;
	priv->subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

#if defined(CONFIG_MEDIA_CONTROLLER)
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	priv->subdev->entity.ops = &imx219_media_ops;
	err = media_entity_init(&priv->subdev->entity, 1, &priv->pad, 0);
	if (err < 0) {
		dev_err(&client->dev, "unable to init media entity\n");
		return err;
	}
#endif

	err = v4l2_async_register_subdev(priv->subdev);
	if (err)
		return err;

	dev_dbg(&client->dev, "Detected IMX219 sensor\n");

	return 0;
}

static int
imx219_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(client);
	struct imx219 *priv = (struct imx219 *)s_data->priv;

	v4l2_async_unregister_subdev(priv->subdev);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif

	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	imx219_power_put(priv);
	camera_common_remove_debugfs(s_data);

	return 0;
}

static const struct i2c_device_id imx219_id[] = {
	{ "imx219", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx219_id);

static struct i2c_driver imx219_i2c_driver = {
	.driver = {
		.name = "imx219",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(imx219_of_match),
	},
	.probe = imx219_probe,
	.remove = imx219_remove,
	.id_table = imx219_id,
};

module_i2c_driver(imx219_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for Sony IMX219");
MODULE_AUTHOR("Bryan Wu <pengw@nvidia.com>");
MODULE_LICENSE("GPL v2");
