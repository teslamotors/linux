/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/camera_common.h>

#include "tegra_camera_dev_mfi.h"

#define LC898212_ACTUATOR_RANGE	1023
#define LC898212_POS_LOW_DEFAULT	(0)
#define LC898212_POS_HIGH_DEFAULT	(1023)
#define LC898212_FOCUS_MACRO	(896)
#define LC898212_FOCUS_INFINITY	(128)
#define LC898212_PWR_DEV_OFF          (0)
#define LC898212_PWR_DEV_ON           (1)

#define SETTLETIME_MS	(15)
#define FOCAL_LENGTH	(47300)
#define MAX_APERTURE	(22000)
#define FNUMBER		(22000)
#define	LC898212_MOVE_TIME_VALUE	(0x43)

#define LC898212_MAX_RETRIES (3)

#define LC898212_WAIT_REPEAT 0xFE
#define LC898212_TABLE_END   0xFF

#define LC898212_REG_ACCESS	0x1
#define LC898212_RAM_ACCESS	0x2

#define AF_MIN 0
#define AF_MAX 1023
#define AF_RANGE (AF_MAX - AF_MIN + 1)

#define LC898212_RZ			0x04
#define LC898212_ADOFFSET	0x3C
#define LC898212_EQENBL		0x87

/**
* standard + custom controls
* custom controls not added for this focuser yet
*/
#define NUM_FOCUS_STD_CTRLS	1
#define NUM_FOCUS_CUSTOM_CTRLS	0
#define NUM_FOCUS_CTRLS (NUM_FOCUS_STD_CTRLS + NUM_FOCUS_CUSTOM_CTRLS)

static int lc898212_s_ctrl(struct v4l2_ctrl *ctrl);

struct lc898212_reg {
	u8 type;
	u8 addr;
	u16 val;
};

struct lc898212 {
	struct	i2c_client			*i2c_client;
	struct	v4l2_subdev			*subdev;
	struct	media_pad			pad;

	struct	v4l2_ctrl_handler		ctrl_handler;
	struct	regulator			*regulator;
	struct	camera_common_focuser_data	*s_data;
	struct	regmap				*regmap8;
	struct	regmap				*regmap16;
	int					numctrls;
	struct camera_mfi_dev			*cmfi_dev16;
	bool					support_mfi;
	struct	v4l2_ctrl			*ctrls[];
};

static struct lc898212_reg lc898212_init_setting[] = {
	{LC898212_REG_ACCESS, 0x80, 0x34},
	{LC898212_REG_ACCESS, 0x81, 0xA0},
	{LC898212_REG_ACCESS, 0x84, 0xE0},
	{LC898212_REG_ACCESS, 0x87, 0x05},
	{LC898212_REG_ACCESS, 0xA4, 0x24},
	{LC898212_RAM_ACCESS, 0x3A, 0x0000},
	{LC898212_RAM_ACCESS, 0x04, 0x0000},
	{LC898212_RAM_ACCESS, 0x02, 0x0000},
	{LC898212_RAM_ACCESS, 0x18, 0x0000},

	/* Filter Setting */
	{LC898212_RAM_ACCESS, 0x40, 0x4030},
	{LC898212_RAM_ACCESS, 0x42, 0x7150},
	{LC898212_RAM_ACCESS, 0x44, 0x8F90},
	{LC898212_RAM_ACCESS, 0x46, 0x61B0},
	{LC898212_RAM_ACCESS, 0x48, 0x7FF0},
	{LC898212_RAM_ACCESS, 0x4A, 0x3930},
	{LC898212_RAM_ACCESS, 0x4C, 0x4030},
	{LC898212_RAM_ACCESS, 0x4E, 0x8010},
	{LC898212_RAM_ACCESS, 0x50, 0x04F0},
	{LC898212_RAM_ACCESS, 0x52, 0x7610},
	{LC898212_RAM_ACCESS, 0x54, 0x2030},
	{LC898212_RAM_ACCESS, 0x56, 0x0000},
	{LC898212_RAM_ACCESS, 0x58, 0x7FF0},
	{LC898212_RAM_ACCESS, 0x5A, 0x0680},
	{LC898212_RAM_ACCESS, 0x5C, 0x72F0},
	{LC898212_RAM_ACCESS, 0x5E, 0x7F70},
	{LC898212_RAM_ACCESS, 0x60, 0x7ED0},
	{LC898212_RAM_ACCESS, 0x62, 0x7FF0},
	{LC898212_RAM_ACCESS, 0x64, 0x0000},
	{LC898212_RAM_ACCESS, 0x66, 0x0000},
	{LC898212_RAM_ACCESS, 0x68, 0x5130},
	{LC898212_RAM_ACCESS, 0x6A, 0x72F0},
	{LC898212_RAM_ACCESS, 0x6C, 0x8010},
	{LC898212_RAM_ACCESS, 0x6E, 0x0000},
	{LC898212_RAM_ACCESS, 0x70, 0x0000},
	{LC898212_RAM_ACCESS, 0x72, 0x18E0},
	{LC898212_RAM_ACCESS, 0x74, 0x4E30},
	{LC898212_RAM_ACCESS, 0x30, 0x0000},
	{LC898212_RAM_ACCESS, 0x76, 0x0C50},
	{LC898212_RAM_ACCESS, 0x78, 0x4000},

	{LC898212_REG_ACCESS, 0x86, 0x60},
	{LC898212_REG_ACCESS, 0x88, 0x70},
	{LC898212_RAM_ACCESS, 0x28, 0x8020},
	{LC898212_RAM_ACCESS, 0x4C, 0x4000},
	{LC898212_REG_ACCESS, 0x83, 0x2C},
	{LC898212_REG_ACCESS, 0x85, 0xC0},

	/* Repeat to read the register until the value turns 0x00 */
	{LC898212_REG_ACCESS, LC898212_WAIT_REPEAT, 0x85},

	{LC898212_REG_ACCESS, 0x84, 0xE3},
	{LC898212_REG_ACCESS, 0x97, 0x00},
	{LC898212_REG_ACCESS, 0x98, 0x42},
	{LC898212_REG_ACCESS, 0x99, 0x00},
	{LC898212_REG_ACCESS, 0x9A, 0x00},

	{LC898212_REG_ACCESS, LC898212_TABLE_END, 0x00}
};

static struct of_device_id lc898212_of_match[] = {
	{ .compatible = "nvidia,lc898212", },
	{ },
};
MODULE_DEVICE_TABLE(of, lc898212_of_match);

static int lc898212_set_position(struct lc898212 *priv, u32 position)
{
	int ret = 0;
	s16 new_pos = 0;
	struct camera_common_focuser_data *s_data = priv->s_data;
	struct nv_focuser_config *cfg = &s_data->config;

	dev_dbg(&s_data->i2c_client->dev, "%s++\n", __func__);
	if (position < cfg->pos_actual_low ||
		position > cfg->pos_actual_high) {
		dev_dbg(&priv->i2c_client->dev,
			"%s: position(%d) out of bound([%d, %d])\n",
			__func__, position, cfg->pos_actual_low,
			cfg->pos_actual_high);
		if (position < cfg->pos_actual_low)
			position = cfg->pos_actual_low;
		if (position > cfg->pos_actual_high)
			position = cfg->pos_actual_high;
	}

	/* unsigned 10 bit to signed 16 bit */
	new_pos = ((s16) position - AF_RANGE / 2) * 64;

	if (priv->support_mfi) {
		ret = tegra_camera_dev_mfi_clear(priv->cmfi_dev16);
		if (ret)
			return ret;
		ret = tegra_camera_dev_mfi_wr_add(priv->cmfi_dev16,
				LC898212_RZ,
				(u16) new_pos);
	} else {
		ret = regmap_write(priv->regmap16, LC898212_RZ, (u16) new_pos);
	}

	dev_dbg(&s_data->i2c_client->dev, "%s--\n", __func__);
	return ret;
}

/*
 * V4l2 controls
 */

static const struct v4l2_ctrl_ops lc898212_ctrl_ops = {
	.s_ctrl		= lc898212_s_ctrl,
};

static int lc898212_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct lc898212 *priv =
		container_of(ctrl->handler, struct lc898212, ctrl_handler);
	int err;

	dev_dbg(&priv->s_data->i2c_client->dev, "%s++\n", __func__);
	/* check for power state */
	if (priv->s_data->pwr_dev == LC898212_PWR_DEV_OFF)
		return -ENODEV;

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		err = lc898212_set_position(priv, ctrl->val);
		break;
	default:
		pr_err("%s: unknown v4l2 ctlr id\n", __func__);
		return -EINVAL;
	}

	return err;
}

static int lc898212_ctrls_init(struct camera_common_focuser_data *s_data)
{
	struct lc898212 *priv = (struct lc898212 *)s_data->priv;
	struct i2c_client *client = priv->i2c_client;
	struct v4l2_ctrl *ctrl;
	struct nv_focuser_config *cfg = &s_data->config;
	int min = cfg->pos_actual_low;
	int max = cfg->pos_actual_high;
	int def = priv->s_data->def_position;
	int err = 0;

	v4l2_ctrl_handler_init(&priv->ctrl_handler, priv->numctrls);
	priv->subdev->ctrl_handler = &priv->ctrl_handler;
	err = priv->ctrl_handler.error;
	if (err) {
		dev_err(&client->dev, "Error %d adding controls\n", err);
		goto error;
	}

	/* add std controls */
	ctrl = v4l2_ctrl_new_std(&priv->ctrl_handler, &lc898212_ctrl_ops,
			V4L2_CID_FOCUS_ABSOLUTE, min, max, 1, def);
	if (ctrl == NULL) {
		dev_err(&client->dev, "Error initializing controls\n");
		err = -EINVAL;
		goto error;
	}
	priv->ctrls[0] = ctrl;

	err = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (err) {
		dev_err(&client->dev, "Error setting default controls\n");
		goto error;
	}

	return 0;
error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return err;
}

static int lc898212_write_table(struct lc898212 *priv,
				 const struct lc898212_reg table[])
{
	int err;
	const struct lc898212_reg *next;
	u16 val;

	for (next = table; next->addr != LC898212_TABLE_END; next++) {
		val = next->val;

		if (next->addr == LC898212_WAIT_REPEAT) {
			u8 data = 0;
			u8 count = 0;

			err = regmap_read(priv->regmap8, val,
				(unsigned int *) &data);
			if (err) {
				pr_err("%s: regmap_read: %d\n", __func__, err);
				return err;
			}
			while (data != 0) {
				if (count >= 10)
					return -EFAULT; /* focuser not ready */

				usleep_range(10, 20);
				err = regmap_read(priv->regmap8, val,
					(unsigned int *) &data);
				if (err) {
					pr_err("%s: regmap_read: %d\n",
						__func__, err);
					return err;
				}
				count++;
			}
			continue;
		}

		if (next->type == LC898212_RAM_ACCESS)
			err = regmap_write(priv->regmap16, next->addr, val);
		else
			err = regmap_write(priv->regmap8, next->addr, val);
		if (err) {
			pr_err("%s:lc898212_write_table:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static unsigned int convert_signed16b_to_unsigned10b(s16 data)
{
	return (u16)(data / 64 + (AF_RANGE>>1));
}

static int lc898212_init(struct lc898212 *priv)
{
	int err;
	u16 data;

	err = lc898212_write_table(priv, lc898212_init_setting);

	err |= regmap_read(priv->regmap16, LC898212_ADOFFSET,
					(unsigned int *) &data);
	priv->s_data->def_position =
			convert_signed16b_to_unsigned10b((s16)data);
	err |= regmap_write(priv->regmap16, LC898212_RZ, data);

	/* Servo On */
	err |= regmap_write(priv->regmap8, LC898212_EQENBL, 0x85);

	return err;
}

static int lc898212_load_config(struct camera_common_focuser_data *s_data)
{
	struct nv_focuser_config *cfg = &s_data->config;

	/* load default configuration */
	/* TODO: parse these values from DT */

	cfg->focal_length = FOCAL_LENGTH;
	cfg->fnumber = FNUMBER;
	cfg->max_aperture = MAX_APERTURE;
	cfg->range_ends_reversed = 0;

	cfg->pos_working_low = LC898212_FOCUS_INFINITY;
	cfg->pos_working_high = LC898212_FOCUS_MACRO;
	cfg->pos_actual_low = LC898212_POS_LOW_DEFAULT;
	cfg->pos_actual_high = LC898212_POS_HIGH_DEFAULT;

	cfg->num_focuser_sets = 1;
	cfg->focuser_set[0].macro = LC898212_FOCUS_MACRO;
	cfg->focuser_set[0].hyper = LC898212_FOCUS_INFINITY;
	cfg->focuser_set[0].inf = LC898212_FOCUS_INFINITY;
	cfg->focuser_set[0].settle_time = SETTLETIME_MS;

	return 0;
}

static int lc898212_power_off(struct camera_common_focuser_data *s_data)
{
	struct lc898212 *priv = (struct lc898212 *)s_data->priv;

	dev_dbg(&s_data->i2c_client->dev, "%s++\n", __func__);
	if (priv->regulator)
		regulator_disable(priv->regulator);
	s_data->pwr_dev = LC898212_PWR_DEV_OFF;

	return 0;
}

static int lc898212_power_on(struct camera_common_focuser_data *s_data)
{
	int err = 0;
	struct lc898212 *priv = (struct lc898212 *)s_data->priv;

	dev_dbg(&s_data->i2c_client->dev, "%s++\n", __func__);
	if (priv->regulator) {
		err = regulator_enable(priv->regulator);
		if (err) {
			dev_err(&s_data->i2c_client->dev,
				"%s:regulator enabled failed\n", __func__);
			return err;
		}
	}

	err = lc898212_init(priv);
	if (err)
		return err;

	s_data->pwr_dev = LC898212_PWR_DEV_ON;

	return 0;
}

static struct camera_common_focuser_ops lc898212_ops = {
	.power_on = lc898212_power_on,
	.power_off = lc898212_power_off,
	.load_config = lc898212_load_config,
	.ctrls_init = lc898212_ctrls_init,
};

#if 0
static int lc898212_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_focuser_data *s_data =
		to_camera_common_focuser_data(client);
	struct lc898212 *priv = (struct lc898212 *)s_data->priv;
	struct v4l2_control control;
	int err = 0;

	dev_dbg(&client->dev, "%s++\n", __func__);
	err = lc898212_init(priv);
	if (err)
		return err;

	/* write override registers for focus position */
	control.id = V4L2_CID_FOCUS_ABSOLUTE;
	err = v4l2_g_ctrl(&priv->ctrl_handler, &control);
	err |= lc898212_set_position(priv, control.value);
	if (err)
		dev_dbg(&client->dev, "%s:warning focus pos %d set failed\n",
			__func__, control.value);

	dev_dbg(&client->dev, "%s--\n", __func__);
	return 0;
}

static struct v4l2_subdev_video_ops lc898212_subdev_video_ops = {
	.s_stream	= lc898212_s_stream,
};

#endif

static int lc898212_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);
	return 0;
}

static struct v4l2_subdev_core_ops lc898212_subdev_core_ops = {
	.s_power	= camera_common_focuser_s_power,
};

static struct v4l2_subdev_ops lc898212_subdev_ops = {
	.core	= &lc898212_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops lc898212_subdev_internal_ops = {
	.open = lc898212_open,
};

static const struct media_entity_operations lc898212_media_ops = {
#ifdef CONFIG_MEDIA_CONTROLLER
	.link_validate = v4l2_subdev_link_validate,
#endif
};

static int lc898212_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct lc898212 *priv;
	struct camera_common_focuser_data *common_data;
	char dev_id[20];
	const char *p_mfi_str;

	static struct regmap_config lc898212_regmap_config8 = {
		.reg_bits = 8,
		.val_bits = 8,
	};
	static struct regmap_config lc898212_regmap_config16 = {
		.reg_bits = 8,
		.val_bits = 16,
	};

	pr_info("[lc898212]: probing sensor\n");

	common_data = devm_kzalloc(&client->dev,
		sizeof(struct camera_common_focuser_data), GFP_KERNEL);
	if (!common_data)
		return -ENOMEM;

	priv = devm_kzalloc(&client->dev, (sizeof(struct lc898212) +
		sizeof(struct v4l2_ctrl *) * NUM_FOCUS_CTRLS), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regulator = devm_regulator_get(&client->dev, "vvcm");
	if (IS_ERR(priv->regulator)) {
		dev_err(&client->dev, "unable to get regulator %s\n",
			dev_name(&client->dev));
		priv->regulator = NULL;
	}

	priv->regmap8 = devm_regmap_init_i2c(client,
		&lc898212_regmap_config8);
	if (IS_ERR(priv->regmap8)) {
		err = PTR_ERR(priv->regmap8);
		dev_err(&client->dev,
			"Failed to allocate register map 8: %d\n", err);
		goto ERROR_RET;
	}
	priv->regmap16 = devm_regmap_init_i2c(client,
		&lc898212_regmap_config16);
	if (IS_ERR(priv->regmap16)) {
		err = PTR_ERR(priv->regmap16);
		dev_err(&client->dev,
			"Failed to allocate register map 16: %d\n", err);
		goto ERROR_RET;
	}

	common_data->ops = &lc898212_ops;
	common_data->ctrl_handler = &priv->ctrl_handler;
	common_data->i2c_client = client;
	common_data->ctrls = priv->ctrls;
	common_data->priv = (void *)priv;
	priv->numctrls = NUM_FOCUS_CTRLS;
	priv->i2c_client		= client;
	priv->s_data			= common_data;
	priv->subdev			= &common_data->subdev;
	priv->subdev->dev		= &client->dev;

	if (client->dev.of_node) {
		err = of_property_read_string(client->dev.of_node,
					"support_mfi",
					&p_mfi_str);
		if (err < 0) {
			dev_err(&client->dev,
					"%s unable to read MFI property\n",
					__func__);
			goto ERROR_RET;
		}

		priv->support_mfi = !strcmp(p_mfi_str, "true") ? true : false;

		if (priv->support_mfi) {
			strcat(dev_id, "lc898212");
			err = tegra_camera_dev_mfi_add_regmap(&priv->cmfi_dev16,
				strcat(dev_id, &dev_name(&client->dev)[0]),
				priv->regmap16);
			if (err < 0) {
				dev_err(&client->dev,
					"%s unable to add to mfi regmap\n",
					__func__);
				goto ERROR_RET;
			}
		}
	}

	err = camera_common_focuser_init(common_data);
	if (err) {
		dev_err(&client->dev, "unable to initialize focuser\n");
		goto ERROR_RET;
	}

	v4l2_i2c_subdev_init(priv->subdev, client, &lc898212_subdev_ops);

	priv->subdev->internal_ops = &lc898212_subdev_internal_ops;
	priv->subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

#if defined(CONFIG_MEDIA_CONTROLLER)
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev->entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	priv->subdev->entity.ops = &lc898212_media_ops;
	err = media_entity_init(&priv->subdev->entity, 1, &priv->pad, 0);
	if (err < 0) {
		dev_err(&client->dev, "unable to init media entity\n");
		goto ERROR_RET;
	}
#endif

	err = v4l2_async_register_subdev(priv->subdev);
	if (err)
		goto ERROR_RET;

	dev_dbg(&client->dev, "Detected lc898212 sensor\n");
	return 0;

ERROR_RET:
	dev_err(&client->dev, "lc898212: probing sensor failed!!\n");
	return err;
}

static int lc898212_remove(struct i2c_client *client)
{
	struct camera_common_focuser_data *s_data =
			to_camera_common_focuser_data(client);
	struct lc898212 *priv = (struct lc898212 *)s_data->priv;

	v4l2_async_unregister_subdev(priv->subdev);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&priv->ctrl_handler);

	return 0;
}

static const struct i2c_device_id lc898212_id[] = {
	{ "lc898212", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, lc898212_id);

static struct i2c_driver lc898212_i2c_driver = {
	.driver = {
		.name = "lc898212",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lc898212_of_match),
	},
	.probe = lc898212_probe,
	.remove = lc898212_remove,
	.id_table = lc898212_id,
};

module_i2c_driver(lc898212_i2c_driver);

MODULE_DESCRIPTION("I2C driver for LC898212");
MODULE_AUTHOR("Bhanu Murthy V <bmurthyv@nvidia.com>");
MODULE_LICENSE("GPL v2");
