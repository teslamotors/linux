/*
 * ov10823.c - ov10823 sensor driver
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
#include <media/ov10823.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "nvc_utilities.h"

#include "ov10823_tables.h"

#define MAX_BUFFER_SIZE 32

/* Frame length: R0x380e~R0x380f */
#define OV10823_NUM_BYTES_FL (2)
/* Coarse time: R0x3500~R0x3502 */
#define OV10823_NUM_BYTES_CT (3)
/* Gain: R0x350A~R0x350B */
#define OV10823_NUM_BYTES_GAIN (2)
/* FSYNC: R0x3002 and other 6 registers */
#define OV10823_NUM_FSYNC (7)
/* Num of regs in override list */
#define OV10823_NUM_BYTES_OVERRIDES (OV10823_NUM_BYTES_FL + \
		OV10823_NUM_BYTES_CT + \
		OV10823_NUM_BYTES_GAIN + \
		OV10823_NUM_FSYNC)

#define OV10823_HAS_SENSOR_ID	(0)
#define OV10823_SUPPORT_EEPROM	(0)

struct ov10823_info {
	struct miscdevice		miscdev_info;
	int				mode;
	bool				fsync_master;
	struct ov10823_power_rail	power;
	struct ov10823_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct ov10823_platform_data	*pdata;
	struct clk			*mclk;
	struct regmap			*regmap;
	struct mutex			ov10823_camera_lock;
	struct dentry			*debugdir;
	atomic_t			in_use;
#if OV10823_SUPPORT_EEPROM
	struct ov10823_eeprom_data eeprom[ov10823_EEPROM_NUM_BLOCKS];
	u8 eeprom_buf[ov10823_EEPROM_SIZE];
#endif
	char devname[16];
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int ov10823_change_i2c_addr(struct i2c_client *client,
	u8 default_i2c_addr, u16 regw_sid, u8 new_i2c_addr)
{
	int ret = -ENODEV;
	struct i2c_msg msg;
	unsigned char data[3];

	int retry = 0;

	dev_dbg(&client->dev, "%s changing addr from %x to %x", __func__,
		default_i2c_addr, new_i2c_addr);

	data[0] = (regw_sid >> 8) & 0xff;
	data[1] = regw_sid & 0xff;
	data[2] = ((new_i2c_addr) << 1) & 0xff;

	msg.addr = default_i2c_addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);

		if (ret == 1)
			return 0;

		retry++;

		dev_dbg(&client->dev, "%s i2c transfer failed, retrying %x\n",
			__func__, default_i2c_addr);

		usleep_range(3000, 3100);
	} while (retry <= 3);

	dev_dbg(&client->dev, "%s %x write done\n", __func__, default_i2c_addr);

	return ret;
}

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000+500);
}

static inline void ov10823_get_frame_length_regs(struct ov10823_reg *regs,
				u32 frame_length)
{
	/* 2 registers for FL, i.e., 2-byte FL */
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void ov10823_get_coarse_time_regs(struct ov10823_reg *regs,
				u32 coarse_time)
{
	/* 3 registers for CT, i.e., 3-byte CT */
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;
}

static inline void ov10823_get_gain_reg(struct ov10823_reg *regs,
				u16 gain)
{
	/* 2 register for gain, i.e., 2-byte gain */
	regs->addr = 0x350a;
	regs->val = (gain >> 8) & 0xff;
	(regs + 1)->addr = 0x350b;
	(regs + 1)->val = gain & 0xff;
}

static inline void ov10823_fsync_master(struct ov10823_reg *regs,
	u32 frame_length)
{
	/* 7 register for frame sync */
	regs->addr = 0x3002;
	regs->val = 0x88;
	(regs + 1)->addr = 0x3009;
	(regs + 1)->val = 0x06;
	(regs + 2)->addr = 0x3823;
	(regs + 2)->val = 0x00;
	(regs + 3)->addr = 0x3826;
	(regs + 3)->val = 0x00;
	(regs + 4)->addr = 0x3827;
	(regs + 4)->val = 0x00;
	/* R380e-R380f minus 4 */
	(regs + 5)->addr = 0x3830;
	(regs + 5)->val = ((frame_length - 4) >> 8) & 0xff;
	(regs + 6)->addr = 0x3831;
	(regs + 6)->val = (frame_length - 4) & 0xff;
}

static inline void ov10823_fsync_slave(struct ov10823_reg *regs,
	u32 frame_length)
{
	/* 7 register for frame sync */
	regs->addr = 0x3002;
	regs->val = 0x80;
	(regs + 1)->addr = 0x3009;
	(regs + 1)->val = 0x02;
	(regs + 2)->addr = 0x3823;
	(regs + 2)->val = 0x30;
	/* In slave mode, R0x3826/R0x3827 need */
	/* to match master's R3830-R3831 */
	/* i.e., match R380e-R380f minus 4 */
	(regs + 3)->addr = 0x3826;
	(regs + 3)->val = ((frame_length - 4) >> 8) & 0xff;
	(regs + 4)->addr = 0x3827;
	(regs + 4)->val = (frame_length - 4) & 0xff;
	(regs + 5)->addr = 0x3830;
	(regs + 5)->val = 0x00;
	(regs + 6)->addr = 0x3831;
	(regs + 6)->val = 0x00;
}

static inline int ov10823_read_reg(struct ov10823_info *info,
				u16 addr, u8 *val)
{
	return regmap_read(info->regmap, addr, (unsigned int *) val);
}

static int ov10823_write_reg(struct ov10823_info *info, u16 addr, u8 val)
{
	int err;

	err = regmap_write(info->regmap, addr, val);

	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int ov10823_write_table(struct ov10823_info *info,
				const struct ov10823_reg table[],
				const struct ov10823_reg override_list[],
				int num_override_regs)
{
	int err;
	const struct ov10823_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != OV10823_TABLE_END; next++) {
		if (next->addr == OV10823_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ov10823_write_reg(info, next->addr, val);
		if (err) {
			pr_err("%s:ov10823_write_table:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static int ov10823_camera_sid_first_cam(struct ov10823_info *info,
	u8 cam1sid_polarity)
{
	if (info->pdata->cam_sids_high_to_low) {
		if (info->pdata->cam1sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam1sid_gpio,
				cam1sid_polarity);
			msleep_range(1);
		}
	} else {
		if (info->pdata->cam1sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam1sid_gpio,
				!cam1sid_polarity);
			msleep_range(1);
		}
	}
	return 0;
}

static int ov10823_camera_sid_second_cam(struct ov10823_info *info,
	u8 cam2sid_polarity)
{
	if (info->pdata->cam_sids_high_to_low) {
		if (info->pdata->cam2sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam2sid_gpio,
				cam2sid_polarity);
			msleep_range(1);
		}
	} else {
		if (info->pdata->cam2sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam2sid_gpio,
				!cam2sid_polarity);
			msleep_range(1);
		}
	}
	return 0;
}

static int ov10823_camera_sid_third_cam(struct ov10823_info *info,
	u8 cam3sid_polarity)
{
	if (info->pdata->cam_sids_high_to_low) {
		if (info->pdata->cam3sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam3sid_gpio,
				cam3sid_polarity);
			msleep_range(1);
		}
	} else {
		if (info->pdata->cam3sid_gpio > 0) {
			usleep_range(1, 2);
			gpio_set_value(info->pdata->cam3sid_gpio,
				!cam3sid_polarity);
			msleep_range(1);
		}
	}
	return 0;
}

static int ov10823_grouphold_on_single(struct ov10823_info *info)
{
	return ov10823_write_reg(info, 0x3208, 0x00);
}

static int ov10823_stream_single(struct ov10823_info *info)
{
	return ov10823_write_reg(info, 0x0100, 0x01);
}

static int ov10823_sleep_single(struct ov10823_info *info)
{
	return ov10823_write_reg(info, 0x0100, 0x00);
}

static int ov10823_grouphold_on(struct ov10823_info *info)
{
	int err = 0;

	err |= ov10823_grouphold_on_single(info);

	return err;
}

static int ov10823_grouphold_off_single(struct ov10823_info *info)
{
	int ret;
	ret = ov10823_write_reg(info, 0x3208, 0x10);
	if (ret)
		return ret;
	return ov10823_write_reg(info, 0x3208, 0xA0);
}

static int ov10823_grouphold_off(struct ov10823_info *info)
{
	int err = 0;

	err |= ov10823_grouphold_off_single(info);

	return err;
}

static void ov10823_i2c_addr_recovery(struct ov10823_info *info)
{
	int err = 0;

	/* LOW SID SEL */
	if (info->pdata->cam_change_i2c_addr &&
		info->pdata->cam_sids_high_to_low) {

		ov10823_camera_sid_first_cam(info, 1);
		ov10823_camera_sid_second_cam(info, 1);
		ov10823_camera_sid_third_cam(info, 1);

		ov10823_camera_sid_first_cam(info, 0);
		err = ov10823_change_i2c_addr(info->i2c_client,
			(u8)info->pdata->default_i2c_sid_low,
			(u16)info->pdata->regw_sid_low,
			(u8)info->pdata->cam1i2c_addr);
		if (err < 0) {
			dev_dbg(&info->i2c_client->dev,
				"[ov10823] can't change i2c address cam #1\n");
		}

		ov10823_camera_sid_second_cam(info, 0);
		err = ov10823_change_i2c_addr(info->i2c_client,
			(u8)info->pdata->default_i2c_sid_low,
			(u16)info->pdata->regw_sid_low,
			(u8)info->pdata->cam2i2c_addr);
		if (err < 0) {
			dev_dbg(&info->i2c_client->dev,
				"[ov10823] can't change i2c address cam #2\n");
		}

		ov10823_camera_sid_third_cam(info, 0);
		err = ov10823_change_i2c_addr(info->i2c_client,
			(u8)info->pdata->default_i2c_sid_low,
			(u16)info->pdata->regw_sid_low,
			(u8)info->pdata->cam3i2c_addr);
		if (err < 0) {
			dev_dbg(&info->i2c_client->dev,
				"[ov10823] can't change i2c address cam #3\n");
		}
	}
}

static int ov10823_init_seq(struct ov10823_info *info,
	struct ov10823_mode *mode,
	int sensor_mode)
{
	int err = 0;
	struct ov10823_reg reg_list[OV10823_NUM_BYTES_OVERRIDES];

	if (info->pdata->cam_skip_sw_reset) {
		dev_info(&info->i2c_client->dev,
			"[ov10823] skip sw reset along with stop streaming\n");
		ov10823_sleep_single(info);
	} else {
		/* sw reset */
		dev_info(&info->i2c_client->dev,
			"[ov10823] sw reset\n");
		err = ov10823_write_table(info, sw_reset_table[0],
				reg_list, OV10823_NUM_BYTES_OVERRIDES);
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain. */
	ov10823_get_frame_length_regs(reg_list, mode->frame_length);
	ov10823_get_coarse_time_regs(reg_list + OV10823_NUM_BYTES_FL,
				mode->coarse_time);
	ov10823_get_gain_reg(reg_list + OV10823_NUM_BYTES_FL +
				OV10823_NUM_BYTES_CT, mode->gain);
	/* fsync mode */
	if (mode->fsync_master)
		ov10823_fsync_master(reg_list + OV10823_NUM_BYTES_FL +
				OV10823_NUM_BYTES_CT + OV10823_NUM_BYTES_GAIN,
				mode->frame_length);
	else
		ov10823_fsync_slave(reg_list + OV10823_NUM_BYTES_FL +
				OV10823_NUM_BYTES_CT + OV10823_NUM_BYTES_GAIN,
				mode->frame_length);

	err = ov10823_write_table(info, mode_table[sensor_mode],
				reg_list, OV10823_NUM_BYTES_OVERRIDES);
	return err;
}

static int ov10823_set_mode(struct ov10823_info *info,
	struct ov10823_mode *mode)
{
	int sensor_mode;
	int err;

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
			 __func__, mode->xres, mode->yres, mode->frame_length,
			 mode->coarse_time, mode->gain);

	if (info->pdata->cam_use_26mhz_mclk) {
		/* 26Mhz MCLK setting */
		if ((mode->xres == 4336 && mode->yres == 2440) ||
			(mode->xres == 4336 && mode->yres == 2432)) {
			sensor_mode = OV10823_MODE_4336x2440_26MhzMCLK;
			pr_info("ov10823_set_mode 4336x2440\n");
		} else if (mode->xres == 3000 && mode->yres == 2440) {
			sensor_mode = OV10823_MODE_3000x2440_26MhzMCLK;
			pr_info("ov10823_set_mode 3000x2440\n");
		} else if (mode->xres == 2168 && mode->yres == 1220) {
			sensor_mode = OV10823_MODE_2168x1220_26MhzMCLK;
			pr_info("ov10823_set_mode 2168x1220\n");
		} else {
			pr_err("There is no this resolution no support %dX%d!!",
				mode->xres, mode->yres);
			return -EINVAL;
		}
	} else {
		/* default is 24Mhz MCLK */
		if ((mode->xres == 4336 && mode->yres == 2440) ||
			(mode->xres == 4336 && mode->yres == 2432)) {
			sensor_mode = OV10823_MODE_4336x2440_24MhzMCLK;
			pr_info("ov10823_set_mode 4336x2440\n");
		} else if (mode->xres == 3000 && mode->yres == 2440) {
			sensor_mode = OV10823_MODE_3000x2440_24MhzMCLK;
			pr_info("ov10823_set_mode 3000x2440\n");
		} else if (mode->xres == 2168 && mode->yres == 1220) {
			sensor_mode = OV10823_MODE_2168x1220_24MhzMCLK;
			pr_info("ov10823_set_mode 2168x1220\n");
		} else {
			pr_err("There is no this resolution no support %dX%d!",
				mode->xres, mode->yres);
			return -EINVAL;
		}
	}

	err = ov10823_init_seq(info, mode, sensor_mode);
	if (err)
		return err;

	err = ov10823_stream_single(info);
	if (err)
		return err;

	pr_info("[ov10823.(%d)]: stream on.\n", info->pdata->num);

	info->mode = sensor_mode;
	info->fsync_master = mode->fsync_master ? true : false;

	return 0;
}

static int ov10823_get_status(struct ov10823_info *info, u8 *dev_status)
{
	*dev_status = 0;
	return 0;
}

static int ov10823_set_fsync(struct ov10823_info *info,
	u32 frame_length, bool group_hold)
{
	struct ov10823_reg reg_list[OV10823_NUM_FSYNC];
	int i = 0;
	int ret;

	if (info->fsync_master)
		ov10823_fsync_master(reg_list, frame_length);
	else
		ov10823_fsync_slave(reg_list, frame_length);

	if (group_hold) {
		ret = ov10823_grouphold_on_single(info);
		if (ret)
			return ret;
	}

	for (i = 0; i < OV10823_NUM_FSYNC; i++) {
		ret = ov10823_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = ov10823_grouphold_off_single(info);
		if (ret)
			return ret;
	}

	return 0;
}

static int ov10823_set_frame_length_single(struct ov10823_info *info,
	u32 frame_length, bool group_hold)
{
	struct ov10823_reg reg_list[OV10823_NUM_BYTES_FL];
	int i = 0;
	int ret;

	ov10823_get_frame_length_regs(reg_list, frame_length);

	if (group_hold) {
		ret = ov10823_grouphold_on_single(info);
		if (ret)
			return ret;
	}

	for (i = 0; i < OV10823_NUM_BYTES_FL; i++) {
		ret = ov10823_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = ov10823_grouphold_off_single(info);
		if (ret)
			return ret;
	}

	return 0;
}

static int ov10823_set_frame_length(struct ov10823_info *info, u32 frame_length,
				bool group_hold)
{
	int ret = 0;

	ret |= ov10823_set_frame_length_single(info, frame_length, group_hold);

	return ret;
}

static int ov10823_set_coarse_time_single(struct ov10823_info *info,
	u32 coarse_time, bool group_hold)
{
	int ret;

	struct ov10823_reg reg_list[OV10823_NUM_BYTES_CT];
	int i = 0;

	ov10823_get_coarse_time_regs(reg_list, coarse_time);

	if (group_hold) {
		ret = ov10823_grouphold_on_single(info);
		if (ret)
			return ret;
	}

	for (i = 0; i < OV10823_NUM_BYTES_CT; i++) {
		ret = ov10823_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = ov10823_grouphold_off_single(info);
		if (ret)
			return ret;
	}
	return 0;
}

static int ov10823_set_coarse_time(struct ov10823_info *info, u32 coarse_time,
				bool group_hold)
{
	int ret = 0;

	ret |= ov10823_set_coarse_time_single(info, coarse_time, group_hold);

	return ret;
}

static int ov10823_set_gain_single(struct ov10823_info *info, u16 gain,
				bool group_hold)
{
	int ret, i;
	struct ov10823_reg reg_list[OV10823_NUM_BYTES_GAIN];

	ov10823_get_gain_reg(reg_list, gain);

	if (group_hold) {
		ret = ov10823_grouphold_on_single(info);
		if (ret)
			return ret;
	}

	/* writing 1-byte gain */
	for (i = 0; i < OV10823_NUM_BYTES_GAIN; i++) {
		ret = ov10823_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = ov10823_grouphold_off_single(info);
		if (ret)
			return ret;
	}
	return 0;
}

static int ov10823_set_gain(struct ov10823_info *info, u16 gain,
				bool group_hold)
{
	int ret = 0;

	ret |= ov10823_set_gain_single(info, gain, group_hold);

	return ret;
}

static int ov10823_set_group_hold(struct ov10823_info *info,

				struct ov10823_grouphold *ae)
{
	int ret;
	int count = 0;
	bool group_hold_enabled = false;

	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		group_hold_enabled = true;

	if (group_hold_enabled) {
		/* need to program all sensors */
		ret = ov10823_grouphold_on(info);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		ov10823_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		ov10823_set_coarse_time(info, ae->coarse_time, false);
	if (ae->frame_length_enable) {
		ov10823_set_frame_length(info, ae->frame_length, false);
		/* update frame sync in */
		ov10823_set_fsync(info, ae->frame_length, false);
	}

	if (group_hold_enabled) {
		/* need to program all sensors */
		ret = ov10823_grouphold_off(info);
		if (ret)
			return ret;
	}

	return 0;
}

static int ov10823_get_sensor_id(struct ov10823_info *info)
{
	int ret = 0;
#if OV10823_HAS_SENSOR_ID
	int i;
	u8 bak = 0;

	pr_info("%s\n", __func__);
	if (info->sensor_data.fuse_id_size)
		return 0;

	/* select bank 31 */
	ov10823_write_reg(info, 0x3d84, 31);
	for (i = 0; i < 8; i++) {
		ret |= ov10823_read_reg(info, 0x300A + i, &bak);
		info->sensor_data.fuse_id[i] = bak;
	}

	if (!ret)
		info->sensor_data.fuse_id_size = 2;

#endif
	return ret;
}

static void ov10823_mclk_disable(struct ov10823_info *info)
{
	if (!info->pdata->cam_use_osc_for_mclk) {
		dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
		clk_disable_unprepare(info->mclk);
	}
}

static int ov10823_mclk_enable(struct ov10823_info *info)
{
	int err = 0;
	unsigned long mclk_init_rate;

	if (!info->pdata->cam_use_osc_for_mclk) {
		mclk_init_rate = 24000000;

		dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
			__func__, mclk_init_rate);

		err = clk_set_rate(info->mclk, mclk_init_rate);
		if (!err)
			err = clk_prepare_enable(info->mclk);
	}
	return err;
}

#if OV10823_SUPPORT_EEPROM
static int ov10823_eeprom_device_release(struct ov10823_info *info)
{
	int i;

	for (i = 0; i < ov10823_EEPROM_NUM_BLOCKS; i++) {
		if (info->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(info->eeprom[i].i2c_client);
			info->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static int ov10823_eeprom_device_init(struct ov10823_info *info)
{
	char *dev_name = "eeprom_ov10823";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
	};
	int i;
	int err;

	for (i = 0; i < ov10823_EEPROM_NUM_BLOCKS; i++) {
		info->eeprom[i].adap = i2c_get_adapter(
				info->i2c_client->adapter->nr);
		memset(&info->eeprom[i].brd, 0, sizeof(info->eeprom[i].brd));
		strncpy(info->eeprom[i].brd.type, dev_name,
				sizeof(info->eeprom[i].brd.type));
		info->eeprom[i].brd.addr = ov10823_EEPROM_ADDRESS + i;
		info->eeprom[i].i2c_client = i2c_new_device(
				info->eeprom[i].adap, &info->eeprom[i].brd);

		info->eeprom[i].regmap = devm_regmap_init_i2c(
			info->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(info->eeprom[i].regmap)) {
			err = PTR_ERR(info->eeprom[i].regmap);
			ov10823_eeprom_device_release(info);
			return err;
		}
	}

	return 0;
}

static int ov10823_read_eeprom(struct ov10823_info *info,
				u8 reg, u16 length, u8 *buf)
{
	return regmap_raw_read(info->eeprom[0].regmap, reg, &buf[reg], length);
}

static int ov10823_write_eeprom(struct ov10823_info *info,
				u16 addr, u8 val)
{
	return regmap_write(info->eeprom[addr >> 8].regmap, addr & 0xFF, val);
}
#endif

static long OV10823_IOCTL(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct ov10823_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OV10823_IOCTL_SET_POWER):
		if (!info->pdata)
			break;
		if (arg && info->pdata->power_on) {
			err = ov10823_mclk_enable(info);
			if (!err)
				err = info->pdata->power_on(&info->power);
			if (err < 0)
				ov10823_mclk_disable(info);
		}
		if (!arg && info->pdata->power_off) {
			info->pdata->power_off(&info->power);
			ov10823_mclk_disable(info);
		}
		break;
	case _IOC_NR(OV10823_IOCTL_SET_MODE):
	{
		struct ov10823_mode mode;
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct ov10823_mode))) {
			pr_err("%s:Failed to get mode from user.\n", __func__);
			return -EFAULT;
		}
		return ov10823_set_mode(info, &mode);
	}
	case _IOC_NR(OV10823_IOCTL_SET_FRAME_LENGTH):
		err |= ov10823_set_frame_length(info, (u32)arg, true);
		/* update frame sync in */
		err |= ov10823_set_fsync(info, (u32)arg, true);
		return err;
	case _IOC_NR(OV10823_IOCTL_SET_COARSE_TIME):
		return ov10823_set_coarse_time(info, (u32)arg, true);
	case _IOC_NR(OV10823_IOCTL_SET_GAIN):
		return ov10823_set_gain(info, (u16)arg, true);
	case _IOC_NR(OV10823_IOCTL_GET_STATUS):
	{
		u8 status;

		err = ov10823_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			pr_err("%s:Failed to copy status to user\n", __func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(OV10823_IOCTL_GET_SENSORDATA):
	{
		err = ov10823_get_sensor_id(info);

		if (err) {
			pr_err("%s:Failed to get fuse id info.\n", __func__);
			return err;
		}
		if (copy_to_user((void __user *)arg, &info->sensor_data,
				sizeof(struct ov10823_sensordata))) {
			pr_info("%s:Failed to copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(OV10823_IOCTL_SET_GROUP_HOLD):
	{
		struct ov10823_grouphold grouphold;
		if (copy_from_user(&grouphold, (const void __user *)arg,
			sizeof(struct ov10823_grouphold))) {
			pr_info("%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return ov10823_set_group_hold(info, &grouphold);
	}
#if OV10823_SUPPORT_EEPROM
	/* there is actually one of them really in use
		NVC_IOCTL_GET_EEPROM_DATA
	or
		OV10823_IOCTL_GET_SENSORDATA (legacy?)
	*/
	case _IOC_NR(NVC_IOCTL_GET_EEPROM_DATA):
	{
		ov10823_read_eeprom(info,
			0,
			ov10823_EEPROM_SIZE,
			info->eeprom_buf);

		if (copy_to_user((void __user *)arg,
			info->eeprom_buf, ov10823_EEPROM_SIZE)) {
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
			(const void __user *)arg, ov10823_EEPROM_SIZE)) {
			dev_err(&info->i2c_client->dev,
					"%s:Failed to read from user buffer\n",
					__func__);
			return -EFAULT;
		}
		for (i = 0; i < ov10823_EEPROM_SIZE; i++) {
			ov10823_write_eeprom(info,
				i,
				info->eeprom_buf[i]);
			msleep(20);
		}
		return 0;
	}
#endif

	default:
		pr_err("%s:unknown cmd.\n", __func__);
		err = -EINVAL;
	}

	return err;
}

static int ov10823_debugfs_show(struct seq_file *s, void *unused)
{
	struct ov10823_info *dev = s->private;

	dev_dbg(&dev->i2c_client->dev, "%s: ++\n", __func__);

	mutex_lock(&dev->ov10823_camera_lock);
	mutex_unlock(&dev->ov10823_camera_lock);

	return 0;
}

static ssize_t ov10823_debugfs_write(struct file *file, char const __user *buf,
				size_t count, loff_t *offset)
{
	struct ov10823_info *dev =
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

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 2)
		goto set_attr;
	if (sscanf(buf, "%d %d", &address, &data) == 2)
		goto set_attr;

	if (sscanf(buf, "0x%x 0x%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "0X%x 0X%x", &address, &data) == 1)
		goto read;
	if (sscanf(buf, "%d %d", &address, &data) == 1)
		goto read;

	dev_err(&i2c_client->dev, "SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	dev_info(&i2c_client->dev,
			"new address = %x, data = %x\n", address, data);
	ret |= ov10823_write_reg(dev, address, data);
read:
	ret |= ov10823_read_reg(dev, address, &readback);
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

static int ov10823_debugfs_open(struct inode *inode, struct file *file)
{
	struct ov10823_info *dev = inode->i_private;
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	return single_open(file, ov10823_debugfs_show, inode->i_private);
}

static const struct file_operations ov10823_debugfs_fops = {
	.open		= ov10823_debugfs_open,
	.read		= seq_read,
	.write		= ov10823_debugfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void ov10823_remove_debugfs(struct ov10823_info *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;

	dev_dbg(&i2c_client->dev, "%s: ++\n", __func__);

	debugfs_remove_recursive(dev->debugdir);
	dev->debugdir = NULL;
}

static void ov10823_create_debugfs(struct ov10823_info *dev)
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
				&ov10823_debugfs_fops);
	if (!ret)
		goto remove_debugfs;

	return;
remove_debugfs:
	dev_err(&i2c_client->dev, "couldn't create debugfs\n");
	ov10823_remove_debugfs(dev);
}

static int ov10823_power_on(struct ov10823_power_rail *pw)
{
	struct ov10823_info *info = container_of(pw,
		struct ov10823_info, power);

	if (info->pdata->reset_gpio > 0) {
		gpio_set_value(info->pdata->reset_gpio, 0);
		usleep_range(1, 2);
		gpio_set_value(info->pdata->reset_gpio, 1);
	}

	/* not a faulty call */
	if (info->pdata->cam_i2c_recovery)
		ov10823_i2c_addr_recovery(info);

	return 1;
}

static int ov10823_power_off(struct ov10823_power_rail *pw)
{
	struct ov10823_info *info = container_of(pw,
		struct ov10823_info, power);

	if (info->pdata->reset_gpio > 0)
		gpio_set_value(info->pdata->reset_gpio, 0);

	return 0;
}

static int ov10823_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct ov10823_info *info;
	info = dev_get_drvdata(miscdev->parent);

	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		pr_info("%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	return 0;
}

static int ov10823_release(struct inode *inode, struct file *file)
{
	struct ov10823_info *info = file->private_data;

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));

	return 0;
}

static int ov10823_power_put(struct ov10823_power_rail *pw)
{
	return 0;
}

static int ov10823_power_get(struct ov10823_info *info)
{
	return 0;
}

static const struct file_operations ov10823_fileops = {
	.owner = THIS_MODULE,
	.open = ov10823_open,
	.unlocked_ioctl = OV10823_IOCTL,
#ifdef CONFIG_COMPAT
	.compat_ioctl = OV10823_IOCTL,
#endif
	.release = ov10823_release,
};

static struct ov10823_platform_data *ov10823_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct ov10823_platform_data *pdata;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata),
			GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	of_property_read_string(np, "dev_name", &pdata->dev_name);
	of_property_read_u32(np, "num", &pdata->num);
	of_property_read_string(np, "clocks", &pdata->mclk_name);
	pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);

	/* MCLK */
	pdata->cam_use_26mhz_mclk = of_property_read_bool(np,
		"cam,cam-use-26mhz-mclk");

	/* SID */
	pdata->cam1sid_gpio = of_get_named_gpio(np, "cam1sid-gpios", 0);
	pdata->cam2sid_gpio = of_get_named_gpio(np, "cam2sid-gpios", 0);
	pdata->cam3sid_gpio = of_get_named_gpio(np, "cam3sid-gpios", 0);

	/* settings to be used by changing i2c addr */
	of_property_read_u32(np, "default_i2c_sid_low",
		&pdata->default_i2c_sid_low);
	of_property_read_u32(np, "default_i2c_sid_high",
		&pdata->default_i2c_sid_high);
	of_property_read_u32(np, "regw_sid_low", &pdata->regw_sid_low);
	of_property_read_u32(np, "regw_sid_high", &pdata->regw_sid_high);
	of_property_read_u32(np, "cam1i2c_addr", &pdata->cam1i2c_addr);
	of_property_read_u32(np, "cam2i2c_addr", &pdata->cam2i2c_addr);
	of_property_read_u32(np, "cam3i2c_addr", &pdata->cam3i2c_addr);

	/* changing i2c addr */
	pdata->cam_change_i2c_addr = of_property_read_bool(np,
		"cam,cam-change-i2c-addr");

	/* skip sw reset */
	pdata->cam_skip_sw_reset = of_property_read_bool(np,
		"cam,cam-skip-sw-reset");

	/* i2c address recovery */
	pdata->cam_i2c_recovery = of_property_read_bool(np,
		"cam,cam-i2c-recovery");

	/* SID low select */
	pdata->cam_sids_high_to_low = of_property_read_bool(np,
		"cam,cam-sids-high-to-low");

	/* use osc as mclk source */
	pdata->cam_use_osc_for_mclk = of_property_read_bool(np,
		"cam,cam-use-osc-for-mclk");

	pdata->power_on = ov10823_power_on;
	pdata->power_off = ov10823_power_off;

	return pdata;
}

static int ov10823_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ov10823_info *info;
	int err;
	const char *mclk_name;

	pr_info("[ov10823]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
			sizeof(struct ov10823_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	if (client->dev.of_node)
		info->pdata = ov10823_parse_dt(client);
	else
		info->pdata = client->dev.platform_data;

	if (!info->pdata) {
		pr_err("[ov10823]:%s:Unable to get platform data\n", __func__);
		return -EFAULT;
	}

	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);

	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = -1;

	mclk_name = info->pdata->mclk_name ?
		info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	ov10823_power_get(info);

	if (info->pdata->dev_name != NULL)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "ov10823", sizeof(info->devname) - 1);
	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
			 info->devname, info->pdata->num);

	info->miscdev_info.name = info->devname;
	info->miscdev_info.fops = &ov10823_fileops;
	info->miscdev_info.minor = MISC_DYNAMIC_MINOR;
	info->miscdev_info.parent = &client->dev;

	err = misc_register(&info->miscdev_info);
	if (err) {
		pr_err("%s:Unable to register misc device!\n", __func__);
		goto ov10823_probe_fail;
	}

	i2c_set_clientdata(client, info);

#if OV10823_SUPPORT_EEPROM
	/* eeprom interface */
	err = ov10823_eeprom_device_init(info);
	if (err) {
		dev_err(&client->dev,
			"Failed to allocate eeprom register map: %d\n", err);
		return err;
	}
#endif

	/* create debugfs interface */
	ov10823_create_debugfs(info);
	return 0;

ov10823_probe_fail:
	ov10823_power_put(&info->power);

	return err;
}

static int ov10823_remove(struct i2c_client *client)
{
	struct ov10823_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&info->miscdev_info);

	ov10823_power_put(&info->power);

	ov10823_remove_debugfs(info);

#if OV10823_SUPPORT_EEPROM
	ov10823_eeprom_device_release(info);
#endif
	return 0;
}

static const struct i2c_device_id ov10823_id[] = {
	{ "ov10823", 0 },
	{ "ov10823.1", 0 },
	{ "ov10823.2", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov10823_id);

static struct i2c_driver ov10823_i2c_driver = {
	.driver = {
		.name = "ov10823",
		.owner = THIS_MODULE,
	},
	.probe = ov10823_probe,
	.remove = ov10823_remove,
	.id_table = ov10823_id,
};


static int __init
ov10823_init(void)
{
	pr_info("[ov10823] sensor driver loading\n");
	return i2c_add_driver(&ov10823_i2c_driver);
}

static void __exit
ov10823_exit(void)
{
	i2c_del_driver(&ov10823_i2c_driver);
}

module_init(ov10823_init);
module_exit(ov10823_exit);
