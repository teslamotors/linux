/*
 * ov2710.c - ov2710 sensor driver
 *
 * Copyright (c) 2011, NVIDIA, All Rights Reserved.
 *
 * Contributors:
 *      erik lilliebjerg <elilliebjerg@nvidia.com>
 *
 * Leverage OV5650.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/ov2710.h>

struct ov2710_reg {
	u16 addr;
	u16 val;
};

struct ov2710_info {
	int mode;
	struct i2c_client *i2c_client;
	struct ov2710_platform_data *pdata;
};

#define OV2710_TABLE_WAIT_MS 0
#define OV2710_TABLE_END 1
#define OV2710_MAX_RETRIES 3

#if 0
static struct ov2710_reg mode_start[] = {
	{0x3008, 0x82}, /* reset registers pg 72 */
	{OV2710_TABLE_WAIT_MS, 5},
	{0x3008, 0x42}, /* register power down pg 72 */
	{OV2710_TABLE_WAIT_MS, 5},
	{0x3103, 0x93}, /* power up system clock from PLL page 77 */
	{0x3017, 0xff}, /* PAD output enable page 100 */
	{0x3018, 0xfc}, /* PAD output enable page 100 */

	{0x3600, 0x50}, /* analog pg 108 */
	{0x3601, 0x0d}, /* analog pg 108 */
	{0x3604, 0x50}, /* analog pg 108 */
	{0x3605, 0x04}, /* analog pg 108 */
	{0x3606, 0x3f}, /* analog pg 108 */
	{0x3612, 0x1a}, /* analog pg 108 */
	{0x3630, 0x22}, /* analog pg 108 */
	{0x3631, 0x22}, /* analog pg 108 */
	{0x3702, 0x3a}, /* analog pg 108 */
	{0x3704, 0x18}, /* analog pg 108 */
	{0x3705, 0xda}, /* analog pg 108 */
	{0x3706, 0x41}, /* analog pg 108 */
	{0x370a, 0x80}, /* analog pg 108 */
	{0x370b, 0x40}, /* analog pg 108 */
	{0x370e, 0x00}, /* analog pg 108 */
	{0x3710, 0x28}, /* analog pg 108 */
	{0x3712, 0x13}, /* analog pg 108 */
	{0x3830, 0x50}, /* manual exposure gain bit [0] */
	{0x3a18, 0x00}, /* AEC gain ceiling bit 8 pg 114 */
	{0x3a19, 0xf8}, /* AEC gain ceiling pg 114 */
	{0x3a00, 0x38}, /* AEC control 0 debug mode band low
			   limit mode band func pg 112 */

	{0x3603, 0xa7}, /* analog pg 108 */
	{0x3615, 0x50}, /* analog pg 108 */
	{0x3620, 0x56}, /* analog pg 108 */
	{0x3810, 0x00}, /* TIMING HVOFFS both are zero pg 80 */
	{0x3836, 0x00}, /* TIMING HVPAD both are zero pg 82 */
	{0x3a1a, 0x06}, /* DIFF MAX an AEC register??? pg 114 */
	{0x4000, 0x01}, /* BLC enabled pg 120 */
	{0x401c, 0x48}, /* reserved pg 120 */
	{0x401d, 0x28}, /* BLC control pg 120 */
	{0x5000, 0x00}, /* ISP control00 features are disabled. pg 132 */
	{0x5001, 0x00}, /* ISP control01 awb disabled. pg 132 */
	{0x5002, 0x00}, /* ISP control02 debug mode disabled pg 132 */
	{0x503d, 0x00}, /* ISP control3D features disabled pg 133 */
	{0x5046, 0x00}, /* ISP control isp disable awbg disable pg 133 */

	{0x300f, 0x8f}, /* PLL control00 R_SELD5 [7:6] div by 4 R_DIVL [2]
			   two lane div 1 SELD2P5 [1:0] div 2.5 pg 99 */
	{0x3010, 0x10}, /* PLL control01 DIVM [3:0] DIVS [7:4] div 1 pg 99 */
	{0x3011, 0x14}, /* PLL control02 R_DIVP [5:0] div 20 pg 99 */
	{0x3012, 0x02}, /* PLL CTR 03, default */
	{0x3815, 0x82}, /* PCLK to SCLK ratio bit[4:0] is set to 2 pg 81 */
	{0x3503, 0x33}, /* AEC auto AGC auto gain has no latch delay. pg 38 */
	/*	{FAST_SETMODE_START, 0}, */
	{0x3613, 0x44}, /* analog pg 108 */
	{OV2710_TABLE_END, 0x0},
};
#endif

static struct ov2710_reg mode_1920x1080[] = {
	{0x3103, 0x93},
	{0x3008, 0x82},
	{0x3017, 0x7f},
	{0x3018, 0xfc},
	{0x3706, 0x61},
	{0x3712, 0x0c},
	{0x3630, 0x6d},
	{0x3801, 0xb4},

	{0x3621, 0x04},
	{0x3604, 0x60},
	{0x3603, 0xa7},
	{0x3631, 0x26},
	{0x3600, 0x04},
	{0x3620, 0x37},
	{0x3623, 0x00},
	{0x3702, 0x9e},
	{0x3703, 0x5c},
	{0x3704, 0x40},
	{0x370d, 0x0f},
	{0x3713, 0x9f},
	{0x3714, 0x4c},
	{0x3710, 0x9e},
	{0x3801, 0xc4},
	{0x3605, 0x05},
	{0x3606, 0x3f},
	{0x302d, 0x90},
	{0x370b, 0x40},
	{0x3716, 0x31},
	{0x380d, 0x74},
	{0x5181, 0x20},
	{0x518f, 0x00},
	{0x4301, 0xff},
	{0x4303, 0x00},
	{0x3a00, 0x78},
	{0x3a18, 0x00}, /* AEC gain ceiling bit 8 pg 51 */
	{0x3a19, 0xf8}, /* AEC gain ceiling pg 51 */
	{0x300f, 0x88},
	{0x3011, 0x28},
	{0x3a1a, 0x06},
	{0x3a18, 0x00},
	{0x3a19, 0x7a},
	{0x3a13, 0x54},
	{0x382e, 0x0f},
	{0x381a, 0x1a},
	{0x401d, 0x02},
	{0x5688, 0x03},
	{0x5684, 0x07},
	{0x5685, 0xa0},
	{0x5686, 0x04},
	{0x5687, 0x43},
	{0x3011, 0x0a},
	{0x300f, 0x8a},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x300e, 0x04},
	{0x4801, 0x0f},
	{0x300f, 0xc3},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},

	{0x350c, 0xff}, /* peak VTS reg, set to highest limit */
	{0x350d, 0xff}, /* peak VTS reg, set to highest limit */
	{0x3406, 0x01}, /* AWB manual, 0-auto, 1-manual */
	{0x3503, 0x07}, /* enable manual gain and manual exposure */
	{0x3500, 0x00}, /* write default to AEC PK EXPO */
	{0x3501, 0x00}, /* write default to AEC PK EXPO */
	{0x3502, 0x02}, /* write default to AEC PK EXPO */
	{0x350a, 0x00}, /* write default to manual gain reg */
	{0x350b, 0x10}, /* write default to manual gain reg */

	{OV2710_TABLE_END, 0x0000}
};

static struct ov2710_reg mode_1280x720[] = {
	{0x3008, 0x82},
	{OV2710_TABLE_WAIT_MS, 5},
	{0x3008, 0x02},
	{OV2710_TABLE_WAIT_MS, 5},
	{0x3103, 0x93},
	{0x3017, 0x7f},
	{0x3018, 0xfc},

	{0x3706, 0x61},
	{0x3712, 0x0c},
	{0x3630, 0x6d},
	{0x3801, 0xb4},
	{0x3621, 0x04},
	{0x3604, 0x60},
	{0x3603, 0xa7},
	{0x3631, 0x26},
	{0x3600, 0x04},
	{0x3620, 0x37},
	{0x3623, 0x00},
	{0x3702, 0x9e},
	{0x3703, 0x5c},
	{0x3704, 0x40},
	{0x370d, 0x0f},
	{0x3713, 0x9f},
	{0x3714, 0x4c},
	{0x3710, 0x9e},
	{0x3801, 0xc4},
	{0x3605, 0x05},
	{0x3606, 0x3f},
	{0x302d, 0x90},
	{0x370b, 0x40},
	{0x3716, 0x31},
	{0x380d, 0x74},
	{0x5181, 0x20},
	{0x518f, 0x00},
	{0x4301, 0xff},
	{0x4303, 0x00},
	{0x3a00, 0x78},
	{0x3a18, 0x00}, /* AEC gain ceiling bit 8 pg 51 */
	{0x3a19, 0xf8}, /* AEC gain ceiling pg 51 */
	{0x300f, 0x88},
	{0x3011, 0x28},
	{0x3a1a, 0x06},
	{0x3a18, 0x00},
	{0x3a19, 0x7a},
	{0x3a13, 0x54},
	{0x382e, 0x0f},
	{0x381a, 0x1a},
	{0x401d, 0x02},
	{0x381c, 0x10},
	{0x381d, 0xb8},
	{0x381e, 0x02},
	{0x381f, 0xdc},
	{0x3820, 0x0a},
	{0x3821, 0x29},
	{0x3804, 0x05},
	{0x3805, 0x00},
	{0x3806, 0x02},
	{0x3807, 0xd0},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380e, 0x02},
	{0x380f, 0xe8},
	{0x380c, 0x07},
	{0x380d, 0x00},
	{0x5688, 0x03},
	{0x5684, 0x05},
	{0x5685, 0x00},
	{0x5686, 0x02},
	{0x5687, 0xd0},
	{0x3a08, 0x1b},
	{0x3a09, 0xe6},
	{0x3a0a, 0x17},
	{0x3a0b, 0x40},
	{0x3a0e, 0x01},
	{0x3a0d, 0x02},
	{0x3011, 0x0a},
	{0x300f, 0x8a},
	{0x3017, 0x00},
	{0x3018, 0x00},

	{0x4803, 0x50}, /* MIPI CTRL3 pg 91 */
	{0x4800, 0x24}, /* MIPI CTRl0 idle and short line pg 89 */

	{0x300e, 0x04},
	{0x4801, 0x0f},
	{0x300f, 0xc3},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},

	{0x3010, 0x10},
	{0x3a0e, 0x02},
	{0x3a0d, 0x03},
	{0x3a08, 0x0d},
	{0x3a09, 0xf3},
	{0x3a0a, 0x0b},
	{0x3a0b, 0xa0},

	{0x350c, 0xff}, /* peak VTS reg, set to highest limit */
	{0x350d, 0xff}, /* peak VTS reg, set to highest limit */
	{0x3406, 0x01}, /* AWB manual, 0-auto, 1-manual */
	{0x3503, 0x07}, /* enable manual gain and manual exposure */
	{0x3500, 0x00}, /* write default to AEC PK EXPO */
	{0x3501, 0x00}, /* write default to AEC PK EXPO */
	{0x3502, 0x02}, /* write default to AEC PK EXPO */
	{0x350a, 0x00}, /* write default to manual gain reg */
	{0x350b, 0x10}, /* write default to manual gain reg */

	{OV2710_TABLE_END, 0x0000}
};

#if 0
static struct ov2710_reg mode_end[] = {
	{0x3212, 0x00}, /* SRM_GROUP_ACCESS (group hold begin) */
	{0x3003, 0x01}, /* reset DVP pg 97 */
	{0x3212, 0x10}, /* SRM_GROUP_ACCESS (group hold end) */
	{0x3212, 0xa0}, /* SRM_GROUP_ACCESS (group hold launch) */
	{0x3008, 0x02}, /* SYSTEM_CTRL0 mipi suspend mask pg 98 */

	/*	{FAST_SETMODE_END, 0}, */
	{OV2710_TABLE_END, 0x0000}
};
#endif

enum {
	OV2710_MODE_1920x1080,
	OV2710_MODE_1280x720,
};

static struct ov2710_reg *mode_table[] = {
	[OV2710_MODE_1920x1080] = mode_1920x1080,
	[OV2710_MODE_1280x720] = mode_1280x720,
};

/* 2 regs to program frame length */
static inline void ov2710_get_frame_length_regs(struct ov2710_reg *regs,
						u32 frame_length)
{
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
}

/* 3 regs to program coarse time */
static inline void ov2710_get_coarse_time_regs(struct ov2710_reg *regs,
					       u32 coarse_time)
{
	regs->addr = 0x3500;
	regs->val = (coarse_time >> 12) & 0xff;
	(regs + 1)->addr = 0x3501;
	(regs + 1)->val = (coarse_time >> 4) & 0xff;
	(regs + 2)->addr = 0x3502;
	(regs + 2)->val = (coarse_time & 0xf) << 4;
}

/* 1 reg to program gain */
static inline void ov2710_get_gain_reg(struct ov2710_reg *regs, u16 gain)
{
	regs->addr = 0x350b;
	regs->val = gain;
}

static int ov2710_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];

	return 0;
}

static int ov2710_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[3];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("ov2710: i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(3);
	} while (retry <= OV2710_MAX_RETRIES);

	return err;
}

static int ov2710_write_table(struct i2c_client *client,
			      const struct ov2710_reg table[],
			      const struct ov2710_reg override_list[],
			      int num_override_regs)
{
	int err;
	const struct ov2710_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != OV2710_TABLE_END; next++) {
		if (next->addr == OV2710_TABLE_WAIT_MS) {
			msleep(next->val);
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

		err = ov2710_write_reg(client, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static int ov2710_set_mode(struct ov2710_info *info, struct ov2710_mode *mode)
{
	int sensor_mode;
	int err;
	struct ov2710_reg reg_list[6];

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
		__func__, mode->xres, mode->yres, mode->frame_length,
		mode->coarse_time, mode->gain);
	if (mode->xres == 1920 && mode->yres == 1080)
		sensor_mode = OV2710_MODE_1920x1080;
	else if (mode->xres == 1264 && mode->yres == 704)
		sensor_mode = OV2710_MODE_1280x720;
	else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	ov2710_get_frame_length_regs(reg_list, mode->frame_length);
	ov2710_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	ov2710_get_gain_reg(reg_list + 5, mode->gain);

	err = ov2710_write_table(info->i2c_client, mode_table[sensor_mode],
	NULL, 0);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int ov2710_set_frame_length(struct ov2710_info *info, u32 frame_length)
{
	struct ov2710_reg reg_list[2];
	int i = 0;
	int ret;

	ov2710_get_frame_length_regs(reg_list, frame_length);

	for (i = 0; i < 2; i++)	{
		ret = ov2710_write_reg(info->i2c_client, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

static int ov2710_set_coarse_time(struct ov2710_info *info, u32 coarse_time)
{
	int ret;

	struct ov2710_reg reg_list[3];
	int i = 0;

	ov2710_get_coarse_time_regs(reg_list, coarse_time);

	ret = ov2710_write_reg(info->i2c_client, 0x3212, 0x01);
	if (ret)
		return ret;

	for (i = 0; i < 3; i++)	{
		ret = ov2710_write_reg(info->i2c_client, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	ret = ov2710_write_reg(info->i2c_client, 0x3212, 0x11);
	if (ret)
		return ret;

	ret = ov2710_write_reg(info->i2c_client, 0x3212, 0xa1);
	if (ret)
		return ret;

	return 0;
}

static int ov2710_set_gain(struct ov2710_info *info, u16 gain)
{
	int ret;
	struct ov2710_reg reg_list;

	ov2710_get_gain_reg(&reg_list, gain);

	ret = ov2710_write_reg(info->i2c_client, reg_list.addr, reg_list.val);

	return ret;
}

static int ov2710_get_status(struct ov2710_info *info, u8 *status)
{
	int err;

	*status = 0;
	err = ov2710_read_reg(info->i2c_client, 0x002, status);
	return err;
}


static long ov2710_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct ov2710_info *info = file->private_data;

	switch (cmd) {
	case OV2710_IOCTL_SET_MODE:
	{
		struct ov2710_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov2710_mode))) {
			return -EFAULT;
		}

		return ov2710_set_mode(info, &mode);
	}
	case OV2710_IOCTL_SET_FRAME_LENGTH:
		return ov2710_set_frame_length(info, (u32)arg);
	case OV2710_IOCTL_SET_COARSE_TIME:
		return ov2710_set_coarse_time(info, (u32)arg);
	case OV2710_IOCTL_SET_GAIN:
		return ov2710_set_gain(info, (u16)arg);
	case OV2710_IOCTL_GET_STATUS:
	{
		u8 status;

		err = ov2710_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			return -EFAULT;
		}
		return 0;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static struct ov2710_info *info;

static int ov2710_open(struct inode *inode, struct file *file)
{
	u8 status;

	file->private_data = info;
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();
	ov2710_get_status(info, &status);
	return 0;
}

int ov2710_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	file->private_data = NULL;
	return 0;
}


static const struct file_operations ov2710_fileops = {
	.owner = THIS_MODULE,
	.open = ov2710_open,
	.unlocked_ioctl = ov2710_ioctl,
	.release = ov2710_release,
};

static struct miscdevice ov2710_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov2710",
	.fops = &ov2710_fileops,
};

static int ov2710_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;

	pr_info("ov2710: probing sensor.\n");

	info = kzalloc(sizeof(struct ov2710_info), GFP_KERNEL);
	if (!info) {
		pr_err("ov2710: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	err = misc_register(&ov2710_device);
	if (err) {
		pr_err("ov2710: Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;

	i2c_set_clientdata(client, info);
	return 0;
}

static int ov2710_remove(struct i2c_client *client)
{
	struct ov2710_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&ov2710_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov2710_id[] = {
	{ "ov2710", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov2710_id);

static struct i2c_driver ov2710_i2c_driver = {
	.driver = {
		.name = "ov2710",
		.owner = THIS_MODULE,
	},
	.probe = ov2710_probe,
	.remove = ov2710_remove,
	.id_table = ov2710_id,
};

static int __init ov2710_init(void)
{
	pr_info("ov2710 sensor driver loading\n");
	return i2c_add_driver(&ov2710_i2c_driver);
}

static void __exit ov2710_exit(void)
{
	i2c_del_driver(&ov2710_i2c_driver);
}

module_init(ov2710_init);
module_exit(ov2710_exit);

