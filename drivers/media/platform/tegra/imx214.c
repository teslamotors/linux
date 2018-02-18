/*
 * imx214.c - imx214 sensor driver
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All Rights Reserved.
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
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <media/camera_common.h>
#include <media/imx214.h>

#include "nvc_utilities.h"
#include "imx214_mode_tbls.h"

struct imx214_info {
	struct miscdevice		miscdev_info;
	int				mode;
	struct imx214_power_rail	power;
	struct imx214_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct imx214_platform_data	*pdata;
	struct regmap			*regmap;
	struct mutex			imx214_camera_lock;
	struct dentry			*debugdir;
	atomic_t			in_use;
	struct camera_common_eeprom_data eeprom[IMX214_EEPROM_NUM_BLOCKS];
	u8 eeprom_buf[IMX214_EEPROM_SIZE];
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static inline void imx214_get_frame_length_regs(struct imx214_reg *regs,
				u32 frame_length)
{
	regs->addr = IMX214_FRAME_LENGTH_ADDR_MSB;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX214_FRAME_LENGTH_ADDR_LSB;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void imx214_get_coarse_time_regs(struct imx214_reg *regs,
				u32 coarse_time)
{
	regs->addr = IMX214_COARSE_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX214_COARSE_TIME_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx214_get_coarse_time_short_regs(struct imx214_reg *regs,
				u32 coarse_time)
{
	regs->addr = IMX214_COARSE_TIME_SHORT_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX214_COARSE_TIME_SHORT_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx214_get_gain_reg(struct imx214_reg *regs,
				u16 gain)
{
	regs->addr = IMX214_GAIN_ADDR_MSB;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = IMX214_GAIN_ADDR_LSB;
	(regs + 1)->val = (gain) & 0xff;
}

static inline void imx214_get_gain_short_reg(struct imx214_reg *regs,
				u16 gain)
{
	regs->addr = IMX214_GAIN_SHORT_ADDR_MSB;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = IMX214_GAIN_SHORT_ADDR_LSB;
	(regs + 1)->val = (gain) & 0xff;
}

static inline int imx214_read_reg(struct imx214_info *info,
				u16 addr, u8 *val)
{
	return regmap_read(info->regmap, addr, (unsigned int *) val);
}

static int imx214_write_reg(struct imx214_info *info, u16 addr, u8 val)
{
	int err;

	err = regmap_write(info->regmap, addr, val);

	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int imx214_write_table(struct imx214_info *info,
				const struct imx214_reg table[],
				const struct imx214_reg override_list[],
				int num_override_regs)
{
	int err;
	const struct imx214_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != IMX214_TABLE_END; next++) {
		if (next->addr == IMX214_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = imx214_write_reg(info, next->addr, val);
		if (err) {
			pr_err("%s:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static int imx214_set_mode(struct imx214_info *info, struct imx214_mode *mode)
{
	int sensor_mode;
	int err;
	struct imx214_reg reg_list[12];

	pr_info("[IMX214] sensor %s:%d ++\n", __func__, __LINE__);
	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u, hdr %d\n",
			 __func__, mode->xres, mode->yres, mode->frame_length,
			 mode->coarse_time, mode->gain, mode->hdr_en);

	if (mode->hdr_en == true) {
		if (mode->xres == 4096 && mode->yres == 2304) {
			sensor_mode = IMX214_MODE_4096X2304_HDR;
		} else if (mode->xres == 4096 && mode->yres == 3072) {
			sensor_mode = IMX214_MODE_4096X3072_HDR;
		} else if (mode->xres == 3840 && mode->yres == 2160) {
			sensor_mode = IMX214_MODE_3840X2160_HDR;
		} else if (mode->xres == 1920 && mode->yres == 1080) {
			sensor_mode = IMX214_MODE_1920X1080_HDR;
		} else if (mode->xres == 1280 && mode->yres == 720) {
			sensor_mode = IMX214_MODE_1280X720_120FPS_HDR;
		} else {
			pr_err("%s: invalid resolution supplied to set mode %d %d\n",
				 __func__, mode->xres, mode->yres);
			return -EINVAL;
		}
	} else {
		if (mode->xres == 4096 && mode->yres == 2304) {
			sensor_mode = IMX214_MODE_4096X2304;
		} else if (mode->xres == 4096 && mode->yres == 3072) {
			sensor_mode = IMX214_MODE_4096X3072;
		} else if (mode->xres == 3840 && mode->yres == 2160) {
			sensor_mode = IMX214_MODE_3840X2160;
		} else if (mode->xres == 2048 && mode->yres == 1536) {
			sensor_mode = IMX214_MODE_2048X1536;
		} else if (mode->xres == 2048 && mode->yres == 1152) {
			sensor_mode = IMX214_MODE_2048X1152;
		} else if (mode->xres == 1920 && mode->yres == 1080) {
			sensor_mode = IMX214_MODE_1920X1080;
		} else if (mode->xres == 1280 && mode->yres == 720) {
			sensor_mode = IMX214_MODE_1280X720_120FPS;
		} else {
			pr_err("%s: invalid resolution supplied to set mode %d %d\n",
				 __func__, mode->xres, mode->yres);
			return -EINVAL;
		}
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	imx214_get_frame_length_regs(reg_list, mode->frame_length);
	imx214_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	imx214_get_gain_reg(reg_list + 4, mode->gain);
	/* if HDR is enabled */
	if (mode->hdr_en == 1) {
		imx214_get_gain_short_reg(reg_list + 6, mode->gain);
		imx214_get_coarse_time_short_regs(
			reg_list + 8, mode->coarse_time_short);
	}

	err = imx214_write_table(info,
				mode_table[IMX214_MODE_COMMON],
				reg_list, mode->hdr_en ? 10 : 6);
	if (err)
		return err;
	err = imx214_write_table(info,
				mode_table[sensor_mode],
				reg_list, mode->hdr_en ? 10 : 6);
	if (err)
		return err;

	info->mode = sensor_mode;
	pr_info("[IMX214]: stream on.\n");
	return 0;
}

static int imx214_get_status(struct imx214_info *info, u8 *dev_status)
{
	*dev_status = 0;
	return 0;
}

static int imx214_set_frame_length(struct imx214_info *info, u32 frame_length,
				bool group_hold)
{
	struct imx214_reg reg_list[2];
	int i = 0;
	int ret;

	imx214_get_frame_length_regs(reg_list, frame_length);

	if (group_hold) {
		ret = imx214_write_reg(info, 0x0104, 0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = imx214_write_reg(info, 0x0104, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int imx214_set_coarse_time(struct imx214_info *info, u32 coarse_time,
				bool group_hold)
{
	int ret;

	struct imx214_reg reg_list[2];
	int i = 0;

	imx214_get_coarse_time_regs(reg_list, coarse_time);

	if (group_hold) {
		ret = imx214_write_reg(info, 0x104, 0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = imx214_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int imx214_set_gain(struct imx214_info *info, u16 gain,
				bool group_hold)
{
	int ret, i;
	struct imx214_reg reg_list[2];
	struct imx214_reg reg_list_short[2];

	imx214_get_gain_reg(reg_list, gain);
	imx214_get_gain_short_reg(reg_list_short, gain);

	if (group_hold) {
		ret = imx214_write_reg(info, 0x104, 0x1);
		if (ret)
			return ret;
	}

	/* writing long gain */
	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}
	/* writing short gain */
	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list_short[i].addr,
			 reg_list_short[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = imx214_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int imx214_set_hdr_coarse_time(struct imx214_info *info,
				struct imx214_hdr *values)
{
	struct imx214_reg reg_list[2];
	struct imx214_reg reg_list_short[2];
	int ret, i = 0;

	/* get long and short coarse time registers */
	imx214_get_coarse_time_regs(reg_list, values->coarse_time_long);
	imx214_get_coarse_time_short_regs(reg_list_short,
			values->coarse_time_short);

	/* set to direct mode */
	ret = imx214_write_reg(info, 0x238, 0x1);
	if (ret)
		return ret;
	/* set group hold */
	ret = imx214_write_reg(info, 0x104, 0x1);
	if (ret)
		return ret;
	/* writing long exposure */
	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}
	/* writing short exposure */
	for (i = 0; i < 2; i++) {
		ret = imx214_write_reg(info, reg_list_short[i].addr,
			 reg_list_short[i].val);
		if (ret)
			return ret;
	}
	ret = imx214_write_reg(info, 0x104, 0x0);
	if (ret)
		return ret;

	return 0;
}

static int imx214_set_group_hold(struct imx214_info *info,

				struct imx214_ae *ae)
{
	int ret;
	int count = 0;
	bool group_hold_enabled = false;
	struct imx214_hdr values;

	values.coarse_time_long = ae->coarse_time;
	values.coarse_time_short = ae->coarse_time_short;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		group_hold_enabled = true;

	if (group_hold_enabled) {
		ret = imx214_write_reg(info, 0x104, 0x1);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		imx214_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		imx214_set_hdr_coarse_time(info, &values);
	if (ae->frame_length_enable)
		imx214_set_frame_length(info, ae->frame_length, false);

	if (group_hold_enabled) {
		ret = imx214_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static void imx214_mclk_disable(
	struct device *dev, struct imx214_power_rail *pw)
{
	dev_dbg(dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(pw->mclk);
}

static int imx214_mclk_enable(
	struct device *dev, struct imx214_power_rail *pw)
{
	unsigned long mclk_init_rate = 24000000;
	int err;

	dev_info(dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(pw->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(pw->mclk);
	return err;
}

static int imx214_power_on(struct imx214_info *info)
{
	int err = 0;
	struct imx214_power_rail *pw = &info->power;

	err = imx214_mclk_enable(&info->i2c_client->dev, pw);
	if (err < 0)
		return err;

	if (info->pdata->power_on)
		return info->pdata->power_on(pw);

	if (pw->ext_reg1)
		err = regulator_enable(pw->ext_reg1);
	if (unlikely(err))
		goto imx214_ext_reg1_fail;

	if (pw->ext_reg2)
		err = regulator_enable(pw->ext_reg2);
	if (unlikely(err))
		goto imx214_ext_reg2_fail;

	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 0);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 1);
	if (pw->cam1_gpio)
		gpio_set_value(pw->cam1_gpio, 0);
	usleep_range(10, 20);

	if (pw->avdd)
		err = regulator_enable(pw->avdd);
	if (err)
		goto imx214_avdd_fail;

	if (pw->iovdd)
		err = regulator_enable(pw->iovdd);
	if (err)
		goto imx214_iovdd_fail;

	usleep_range(1, 2);
	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 1);
	if (pw->cam1_gpio)
		gpio_set_value(pw->cam1_gpio, 1);

	usleep_range(300, 310);

	return err;


imx214_iovdd_fail:
	regulator_disable(pw->avdd);

imx214_avdd_fail:
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

imx214_ext_reg2_fail:
	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 0);

imx214_ext_reg1_fail:
	imx214_mclk_disable(&info->i2c_client->dev, pw);
	pr_err("%s failed.\n", __func__);

	return -ENODEV;
}

static int imx214_power_off(struct imx214_info *info)
{
	int err = 0;
	struct imx214_power_rail *pw = &info->power;

	if (info->pdata->power_off) {
		err = info->pdata->power_off(&info->power);
		if (err < 0) {
			pr_err("%s failed.\n", __func__);
			return err;
		} else {
			goto power_off_done;
		}
	}

	usleep_range(1, 2);
	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 0);
	if (pw->af_gpio)
		gpio_set_value(pw->af_gpio, 0);
	if (pw->cam1_gpio)
		gpio_set_value(pw->cam1_gpio, 0);
	usleep_range(1, 2);

	if (pw->iovdd)
		regulator_disable(pw->iovdd);
	if (pw->avdd)
		regulator_disable(pw->avdd);

	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

power_off_done:
	imx214_mclk_disable(&info->i2c_client->dev, pw);

	return 0;
}

static int imx214_get_sensor_id(struct imx214_info *info)
{
	int err = 0;
	u8 status;

	if (info->sensor_data.fuse_id_size)
		return 0;

	err = imx214_power_on(info);
	if (err)
		return -ENODEV;

	err = imx214_write_reg(info, IMX214_OTP_PAGE_NUM_ADDR,
			 IMX214_FUSE_ID_OTP_PAGE);
	if (err)
		return err;
	err = imx214_write_reg(info, IMX214_OTP_CTRL_ADDR, 0x01);
	if (err)
		return err;
	err = imx214_read_reg(info, IMX214_OTP_STATUS_ADDR, &status);
	if (err)
		return err;
	if (status == IMX214_OTP_STATUS_IN_PROGRESS) {
		dev_err(&info->i2c_client->dev, "another OTP read in progress\n");
		return err;
	}

	err = regmap_bulk_read(info->regmap, IMX214_FUSE_ID_OTP_ROW_ADDR,
			&info->sensor_data.fuse_id[0], IMX214_FUSE_ID_SIZE);
	if (err)
		return err;

	err = imx214_read_reg(info, IMX214_OTP_STATUS_ADDR, &status);
	if (err)
		return err;
	if (status == IMX214_OTP_STATUS_READ_FAIL) {
		dev_err(&info->i2c_client->dev, "fuse id read error\n");
		return err;
	}

	info->sensor_data.fuse_id_size = IMX214_FUSE_ID_SIZE;

	imx214_power_off(info);
	return err;
}

static int imx214_eeprom_device_release(struct imx214_info *info)
{
	int i;

	for (i = 0; i < IMX214_EEPROM_NUM_BLOCKS; i++) {
		if (info->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(info->eeprom[i].i2c_client);
			info->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static int imx214_eeprom_device_init(struct imx214_info *info)
{
	char *dev_name = "eeprom_imx214";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
	};
	int i;
	int err;

	for (i = 0; i < IMX214_EEPROM_NUM_BLOCKS; i++) {
		info->eeprom[i].adap = i2c_get_adapter(
				info->i2c_client->adapter->nr);
		memset(&info->eeprom[i].brd, 0, sizeof(info->eeprom[i].brd));
		strncpy(info->eeprom[i].brd.type, dev_name,
				sizeof(info->eeprom[i].brd.type));
		info->eeprom[i].brd.addr = IMX214_EEPROM_ADDRESS + i;
		info->eeprom[i].i2c_client = i2c_new_device(
				info->eeprom[i].adap, &info->eeprom[i].brd);

		info->eeprom[i].regmap = devm_regmap_init_i2c(
			info->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(info->eeprom[i].regmap)) {
			err = PTR_ERR(info->eeprom[i].regmap);
			imx214_eeprom_device_release(info);
			return err;
		}
	}

	return 0;
}

static int imx214_read_eeprom(struct imx214_info *info,
				u8 reg, u16 length, u8 *buf)
{
	return regmap_raw_read(info->eeprom[0].regmap, reg, &buf[reg], length);
}

static int imx214_write_eeprom(struct imx214_info *info,
				u16 addr, u8 val)
{
	return regmap_write(info->eeprom[addr >> 8].regmap, addr & 0xFF, val);
}

static long imx214_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct imx214_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(IMX214_IOCTL_SET_POWER):
		if (arg)
			err = imx214_power_on(info);
		else
			err = imx214_power_off(info);
		if (err)
			return -ENODEV;
		break;
	case _IOC_NR(IMX214_IOCTL_SET_MODE):
	{
		struct imx214_mode mode;
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct imx214_mode))) {
			pr_err("%s:Failed to get mode from user.\n", __func__);
			return -EFAULT;
		}
		return imx214_set_mode(info, &mode);
	}
	case _IOC_NR(IMX214_IOCTL_SET_FRAME_LENGTH):
		return imx214_set_frame_length(info, (u32)arg, true);
	case _IOC_NR(IMX214_IOCTL_SET_COARSE_TIME):
		return imx214_set_coarse_time(info, (u32)arg, true);
	case _IOC_NR(IMX214_IOCTL_SET_GAIN):
		return imx214_set_gain(info, (u16)arg, true);
	case _IOC_NR(IMX214_IOCTL_GET_STATUS):
	{
		u8 status;

		err = imx214_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			pr_err("%s:Failed to copy status to user\n", __func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(IMX214_IOCTL_GET_SENSORDATA):
	{
		err = imx214_get_sensor_id(info);

		if (err) {
			pr_err("%s:Failed to get fuse id info.\n", __func__);
			return err;
		}
		if (copy_to_user((void __user *)arg, &info->sensor_data,
				sizeof(struct imx214_sensordata))) {
			pr_info("%s:Failed to copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(IMX214_IOCTL_SET_GROUP_HOLD):
	{
		struct imx214_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
			sizeof(struct imx214_ae))) {
			pr_info("%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return imx214_set_group_hold(info, &ae);
	}
	case _IOC_NR(IMX214_IOCTL_SET_HDR_COARSE_TIME):
	{
		struct imx214_hdr values;

		dev_dbg(&info->i2c_client->dev,
				"IMX214_IOCTL_SET_HDR_COARSE_TIME\n");
		if (copy_from_user(&values,
			(const void __user *)arg,
			sizeof(struct imx214_hdr))) {
				err = -EFAULT;
				break;
		}
		err = imx214_set_hdr_coarse_time(info, &values);
		break;
	}
	case _IOC_NR(NVC_IOCTL_GET_EEPROM_DATA):
	{
		imx214_read_eeprom(info,
			0,
			IMX214_EEPROM_SIZE,
			info->eeprom_buf);

		if (copy_to_user((void __user *)arg,
			info->eeprom_buf, IMX214_EEPROM_SIZE)) {
			dev_err(&info->i2c_client->dev,
				"%s:Failed to copy status to user\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}

	case _IOC_NR(NVC_IOCTL_SET_EEPROM_DATA):
	{
		int i;
		if (copy_from_user(info->eeprom_buf,
			(const void __user *)arg, IMX214_EEPROM_SIZE)) {
			dev_err(&info->i2c_client->dev,
					"%s:Failed to read from user buffer\n",
					__func__);
			return -EFAULT;
		}
		for (i = 0; i < IMX214_EEPROM_SIZE; i++) {
			imx214_write_eeprom(info,
				i,
				info->eeprom_buf[i]);
			msleep(20);
		}
		return 0;
	}

	default:
		pr_err("%s:unknown cmd.\n", __func__);
		err = -EINVAL;
	}

	return err;
}

static int imx214_debugfs_show(struct seq_file *s, void *unused)
{
	struct imx214_info *dev = s->private;

	dev_dbg(&dev->i2c_client->dev, "%s: ++\n", __func__);

	mutex_lock(&dev->imx214_camera_lock);
	mutex_unlock(&dev->imx214_camera_lock);

	return 0;
}

static ssize_t imx214_debugfs_write(struct file *file, char const __user *buf,
				size_t count, loff_t *offset)
{
	struct imx214_info *dev =
			((struct seq_file *)file->private_data)->private;
	struct i2c_client *i2c_client = dev->i2c_client;
	int ret = 0;
	char buffer[MAX_BUFFER_SIZE];
	u32 address;
	u32 data;
	u8 readback;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	if (copy_from_user(&buffer, buf, sizeof(buffer)))
		goto debugfs_write_fail;

	if (sscanf(buffer, "0x%x 0x%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buffer, "0X%x 0X%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buffer, "%d %d", &address, &data) == 2)
		goto set_attr;

	if (sscanf(buffer, "0x%x 0x%x", &address, &data) == 1)
		goto read;
	if (sscanf(buffer, "0X%x 0X%x", &address, &data) == 1)
		goto read;
	if (sscanf(buffer, "%d %d", &address, &data) == 1)
		goto read;

	dev_err(&i2c_client->dev, "SYNTAX ERROR: %s\n", buffer);
	return -EFAULT;

set_attr:
	dev_info(&i2c_client->dev,
			"new address = %x, data = %x\n", address, data);
	ret |= imx214_write_reg(dev, address, data);
read:
	ret |= imx214_read_reg(dev, address, &readback);
	dev_dbg(&i2c_client->dev,
			"wrote to address 0x%x with value 0x%x\n",
			address, readback);

	if (ret)
		goto debugfs_write_fail;

	return count;

debugfs_write_fail:
	dev_err(&i2c_client->dev,
			"%s: test pattern write failed\n", __func__);
	return -EFAULT;
}

static int imx214_debugfs_open(struct inode *inode, struct file *file)
{
	struct imx214_info *dev = inode->i_private;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	return single_open(file, imx214_debugfs_show, inode->i_private);
}

static const struct file_operations imx214_debugfs_fops = {
	.open		= imx214_debugfs_open,
	.read		= seq_read,
	.write		= imx214_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void imx214_remove_debugfs(struct imx214_info *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	debugfs_remove_recursive(dev->debugdir);
	dev->debugdir = NULL;
}

static void imx214_create_debugfs(struct imx214_info *dev)
{
	struct dentry *ret;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s\n", __func__);

	dev->debugdir =
		debugfs_create_dir(dev->miscdev_info.this_device->kobj.name,
							NULL);
	if (!dev->debugdir)
		goto remove_debugfs;

	ret = debugfs_create_file("d",
				S_IWUSR | S_IRUGO,
				dev->debugdir, dev,
				&imx214_debugfs_fops);
	if (!ret)
		goto remove_debugfs;

	return;
remove_debugfs:
	dev_err(&i2c_client->dev, "couldn't create debugfs\n");
	imx214_remove_debugfs(dev);
}

static int imx214_get_extra_regulators(struct imx214_info *info,
				struct imx214_power_rail *pw)
{
	if (!pw->ext_reg1) {
		pw->ext_reg1 = devm_regulator_get(&info->i2c_client->dev,
						"imx214_reg1");
		if (WARN_ON(IS_ERR(pw->ext_reg1))) {
			pr_err("%s: can't get regulator imx214_reg1: %ld\n",
				__func__, PTR_ERR(pw->ext_reg1));
			pw->ext_reg1 = NULL;
			return -ENODEV;
		}
	}

	if (!pw->ext_reg2) {
		pw->ext_reg2 = devm_regulator_get(&info->i2c_client->dev,
						"imx214_reg2");
		if (WARN_ON(IS_ERR(pw->ext_reg2))) {
			pr_err("%s: can't get regulator imx214_reg2: %ld\n",
				__func__, PTR_ERR(pw->ext_reg2));
			pw->ext_reg2 = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int imx214_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct imx214_info *info;

	info = container_of(miscdev, struct imx214_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		pr_info("%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	return 0;
}

static int imx214_release(struct inode *inode, struct file *file)
{
	struct imx214_info *info = file->private_data;

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int imx214_power_put(struct imx214_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->ext_reg1))
		regulator_put(pw->ext_reg1);

	if (likely(pw->ext_reg2))
		regulator_put(pw->ext_reg2);

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;
	pw->ext_reg1 = NULL;
	pw->ext_reg2 = NULL;

	return 0;
}

static int imx214_regulator_get(struct imx214_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = devm_regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %p\n",
			__func__, vreg_name, reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int imx214_power_get(struct imx214_info *info)
{
	struct imx214_power_rail *pw = &info->power;
	struct imx214_platform_data *pdata = info->pdata;
	const char *mclk_name;
	int err = 0;

	mclk_name = pdata->mclk_name ? pdata->mclk_name : "default_mclk";
	pw->mclk = devm_clk_get(&info->i2c_client->dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(&info->i2c_client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(pw->mclk);
	}

	err |= imx214_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */
	err |= imx214_regulator_get(info, &pw->dvdd, "vdig"); /* digital 1.2v */
	err |= imx214_regulator_get(info, &pw->iovdd, "vif"); /* IO 1.8v */

	if (pdata->ext_reg)
		err |= imx214_get_extra_regulators(info, pw);

	if (!err) {
		pw->reset_gpio = pdata->reset_gpio;
		pw->af_gpio = pdata->af_gpio;
		pw->cam1_gpio = pdata->cam1_gpio;
	}

	return err;
}

static const struct file_operations imx214_fileops = {
	.owner = THIS_MODULE,
	.open = imx214_open,
	.unlocked_ioctl = imx214_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = imx214_ioctl,
#endif
	.release = imx214_release,
};

static struct miscdevice imx214_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imx214",
	.fops = &imx214_fileops,
};

static struct imx214_platform_data *imx214_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct imx214_platform_data *pdata;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	of_property_read_string(np, "clocks", &pdata->mclk_name);
	pdata->cam1_gpio = of_get_named_gpio(np, "cam1-gpios", 0);
	pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	pdata->af_gpio = of_get_named_gpio(np, "af-gpios", 0);

	pdata->ext_reg = of_property_read_bool(np, "nvidia,ext_reg");

	return pdata;
}

static int imx214_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct imx214_info *info;
	int err;

	pr_info("[IMX214]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
			sizeof(struct imx214_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	if (client->dev.of_node)
		info->pdata = imx214_parse_dt(client);
	else
		info->pdata = client->dev.platform_data;

	if (!info->pdata) {
		pr_err("[IMX214]:%s:Unable to get platform data\n", __func__);
		return -EFAULT;
	}

	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = -1;

	imx214_power_get(info);

	memcpy(&info->miscdev_info,
		&imx214_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		pr_err("%s:Unable to register misc device!\n", __func__);
		goto imx214_probe_fail;
	}

	i2c_set_clientdata(client, info);

	/* eeprom interface */
	err = imx214_eeprom_device_init(info);
	if (err) {
		dev_err(&client->dev,
			"Failed to allocate eeprom register map: %d\n", err);
		return err;
	}

	/* create debugfs interface */
	imx214_create_debugfs(info);
	return 0;

imx214_probe_fail:
	imx214_power_put(&info->power);

	return err;
}

static int imx214_remove(struct i2c_client *client)
{
	struct imx214_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&imx214_device);

	imx214_power_put(&info->power);

	imx214_remove_debugfs(info);
	imx214_eeprom_device_release(info);
	return 0;
}

static const struct i2c_device_id imx214_id[] = {
	{ "imx214", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx214_id);

static struct i2c_driver imx214_i2c_driver = {
	.driver = {
		.name = "imx214",
		.owner = THIS_MODULE,
	},
	.probe = imx214_probe,
	.remove = imx214_remove,
	.id_table = imx214_id,
};

module_i2c_driver(imx214_i2c_driver);
