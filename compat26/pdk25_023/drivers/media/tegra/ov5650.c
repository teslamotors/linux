/*
 * ov5650.c - ov5650 sensor driver
 *
 * Copyright (C) 2011 Google Inc.
 *
 * Contributors:
 *      Rebecca Schultz Zavin <rebecca@android.com>
 *
 * Leverage OV9640.c
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
#include <media/ov5650.h>
#include <media/tegra_camera.h>

struct ov5650_reg {
	u16 addr;
	u16 val;
};

static struct ov5650_reg *last_mode;

struct ov5650_sensor {
	struct i2c_client *i2c_client;
	struct ov5650_platform_data *pdata;
};

struct ov5650_info {
	int mode;
	enum StereoCameraMode camera_mode;
	struct ov5650_sensor left;
	struct ov5650_sensor right;
};

static struct ov5650_info *stereo_ov5650_info;

#define OV5650_TABLE_WAIT_MS 0
#define OV5650_TABLE_END 1
#define OV5650_MAX_RETRIES 3

static struct ov5650_reg tp_none_seq[] = {
	{0x5046, 0x00}, /* isp_off */
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg tp_cbars_seq[] = {
	{0x503D, 0xC0},
	{0x503E, 0x00},
	{0x5046, 0x01}, /* isp_on */
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg tp_checker_seq[] = {
	{0x503D, 0xC0},
	{0x503E, 0x0A},
	{0x5046, 0x01}, /* isp_on */
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg *test_pattern_modes[] = {
	tp_none_seq,
	tp_cbars_seq,
	tp_checker_seq,
};

static struct ov5650_reg reset_seq[] = {
	{0x3008, 0x82}, /* reset registers pg 72 */
	{OV5650_TABLE_WAIT_MS, 5},
	{0x3008, 0x42}, /* register power down pg 72 */
	{OV5650_TABLE_WAIT_MS, 5},
	{OV5650_TABLE_END, 0x0000},
};

static struct ov5650_reg mode_start[] = {
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
	{OV5650_TABLE_END, 0x0},
};

static struct ov5650_reg mode_2592x1944[] = {
	{0x3621, 0x2f}, /* analog horizontal binning/sampling not enabled.
			   pg 108 */
	{0x3632, 0x55}, /* analog pg 108 */
	{0x3703, 0xe6}, /* analog pg 108 */
	{0x370c, 0xa0}, /* analog pg 108 */
	{0x370d, 0x04}, /* analog pg 108 */
	{0x3713, 0x2f}, /* analog pg 108 */
	{0x3800, 0x02}, /* HREF start point higher 4 bits [3:0] pg 108 */
	{0x3801, 0x58}, /* HREF start point lower  8 bits [7:0] pg 108 */
	{0x3802, 0x00}, /* VREF start point higher 4 bits [3:0] pg 108 */
	{0x3803, 0x0c}, /* VREF start point [7:0] pg 108 */
	{0x3804, 0x0a}, /* HREF width  higher 4 bits [3:0] pg 108 */
	{0x3805, 0x20}, /* HREF width  lower  8 bits [7:0] pg 108 */
	{0x3806, 0x07}, /* VREF height higher 4 bits [3:0] pg 109 */
	{0x3807, 0xa0}, /* VREF height lower  8 bits [7:0] pg 109 */
	{0x3808, 0x0a}, /* DVP horizontal output size higher 4 bits [3:0]
			   pg 109 */
	{0x3809, 0x20}, /* DVP horizontal output size lower  8 bits [7:0]
			   pg 109 */
	{0x380a, 0x07}, /* DVP vertical   output size higher 4 bits [3:0]
			   pg 109 */
	{0x380b, 0xa0}, /* DVP vertical   output size lower  8 bits [7:0]
			   pg 109 */
	{0x380c, 0x0c}, /* total horizontal size higher 5 bits [4:0] pg 109,
			   line length */
	{0x380d, 0xb4}, /* total horizontal size lower  8 bits [7:0] pg 109,
			   line length */
	{0x380e, 0x07}, /* total vertical   size higher 5 bits [4:0] pg 109,
			   frame length */
	{0x380f, 0xb0}, /* total vertical   size lower  8 bits [7:0] pg 109,
			   frame length */
	{0x3818, 0xc0}, /* timing control reg18 mirror & dkhf pg 110 */
	{0x381a, 0x3c}, /* HS mirror adjustment pg 110 */
	{0x3a0d, 0x06}, /* b60 max pg 113 */
	{0x3c01, 0x00}, /* 5060HZ_CTRL01 pg 116 */
	{0x3007, 0x3f}, /* clock enable03 pg 98 */
	{0x5059, 0x80}, /* => NOT found */
	{0x3003, 0x03}, /* reset MIPI and DVP pg 97 */
	{0x3500, 0x00}, /* long exp 1/3 in unit of 1/16 line, pg 38 */
	{0x3501, 0x7a}, /* long exp 2/3 in unit of 1/16 line, pg 38,
			   note frame length start with 0x7b0,
			   and SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF=3 */
	{0x3502, 0xd0}, /* long exp 3/3 in unit of 1/16 line, pg 38.
			   Two lines of integration time. */
	{0x350a, 0x00}, /* gain output to sensor, pg 38 */
	{0x350b, 0x00}, /* gain output to sensor, pg 38 */
	{0x4801, 0x0f}, /* MIPI control01 pg 125 */
	{0x300e, 0x0c}, /* SC_MIPI_SC_CTRL0 pg 73 */
	{0x4803, 0x50}, /* MIPI CTRL3 pg 91 */
	{0x4800, 0x34}, /* MIPI CTRl0 idle and short line pg 89 */
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_1296x972[] = {
	{0x3621, 0xaf}, /* analog horizontal binning/sampling enabled.
			   pg 108 */
	{0x3632, 0x5a}, /* analog pg 108 */
	{0x3703, 0xb0}, /* analog pg 108 */
	{0x370c, 0xc5}, /* analog pg 108 */
	{0x370d, 0x42}, /* analog pg 108 */
	{0x3713, 0x2f}, /* analog pg 108 */
	{0x3800, 0x03}, /* HREF start point higher 4 bits [3:0] pg 108 */
	{0x3801, 0x3c}, /* HREF start point lower  8 bits [7:0] pg 108 */
	{0x3802, 0x00}, /* VREF start point higher 4 bits [3:0] pg 108 */
	{0x3803, 0x06}, /* VREF start point [7:0] pg 108 */
	{0x3804, 0x05}, /* HREF width  higher 4 bits [3:0] pg 108 */
	{0x3805, 0x10}, /* HREF width  lower  8 bits [7:0] pg 108 */
	{0x3806, 0x03}, /* VREF height higher 4 bits [3:0] pg 109 */
	{0x3807, 0xd0}, /* VREF height lower  8 bits [7:0] pg 109 */
	{0x3808, 0x05}, /* DVP horizontal output size higher 4 bits [3:0]
			   pg 109 */
	{0x3809, 0x10}, /* DVP horizontal output size lower  8 bits [7:0]
			   pg 109 */
	{0x380a, 0x03}, /* DVP vertical   output size higher 4 bits [3:0]
			   pg 109 */
	{0x380b, 0xd0}, /* DVP vertical   output size lower  8 bits [7:0]
			   pg 109 */
	{0x380c, 0x08}, /* total horizontal size higher 5 bits [4:0]
			   pg 109, line length */
	{0x380d, 0xa8}, /* total horizontal size lower  8 bits [7:0] pg 109,
			   line length */
	{0x380e, 0x05}, /* total vertical   size higher 5 bits [4:0] pg 109,
			   frame length */
	{0x380f, 0xa4}, /* total horizontal size lower  8 bits [7:0] pg 109,
			   frame length */
	{0x3818, 0xc1}, /* timing control reg18 mirror & dkhf pg 110 */
	{0x381a, 0x00}, /* HS mirror adjustment pg 110 */
	{0x3a0d, 0x08}, /* b60 max pg 113 */
	{0x3c01, 0x00}, /* 5060HZ_CTRL01 pg 116 */
	{0x3007, 0x3b}, /* clock enable03 pg 98 */
	{0x5059, 0x80}, /* => NOT found. added */
	{0x3003, 0x03}, /* reset MIPI and DVP pg 97 */
	{0x3500, 0x00}, /* long exp 1/3 in unit of 1/16 line, pg 38,
			   note frame length is from 0x5a4,
			   and SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF=3 */
	{0x3501, 0x5a}, /* long exp 2/3 in unit of 1/16 line, pg 38 */
	{0x3502, 0x10}, /* long exp 3/3 in unit of 1/16 line, pg 38 */
	{0x350a, 0x00}, /* gain output to sensor, pg 38 */
	{0x350b, 0x10}, /* gain output to sensor, pg 38 */
	{0x4801, 0x0f}, /* MIPI control01 pg 125 */
	{0x300e, 0x0c}, /* SC_MIPI_SC_CTRL0 pg 73 */
	{0x4803, 0x50}, /* MIPI CTRL3 pg 91 */
	{0x4800, 0x34}, /* MIPI CTRl0 idle and short line pg 89 */
	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_2080x1164[] = {
	{0x3103, 0x93}, /* power up system clock from PLL page 77 */
	{0x3007, 0x3b}, /* clock enable03 pg 98 */
	{0x3017, 0xff}, /* PAD output enable page 100 */
	{0x3018, 0xfc}, /* PAD output enable page 100 */

	{0x3600, 0x54}, /* analog pg 108 */
	{0x3601, 0x05}, /* analog pg 108 */
	{0x3603, 0xa7}, /* analog pg 108 */
	{0x3604, 0x40}, /* analog pg 108 */
	{0x3605, 0x04}, /* analog pg 108 */
	{0x3606, 0x3f}, /* analog pg 108 */
	{0x3612, 0x1a}, /* analog pg 108 */
	{0x3613, 0x44}, /* analog pg 108 */
	{0x3615, 0x52}, /* analog pg 108 */
	{0x3620, 0x56}, /* analog pg 108 */
	{0x3623, 0x01}, /* analog pg 108 */
	{0x3630, 0x22}, /* analog pg 108 */
	{0x3631, 0x36}, /* analog pg 108 */
	{0x3632, 0x5f}, /* analog pg 108 */
	{0x3633, 0x24}, /* analog pg 108 */

	{0x3702, 0x3a}, /* analog pg 108 */
	{0x3704, 0x18}, /* analog pg 108 */
	{0x3706, 0x41}, /* analog pg 108 */
	{0x370b, 0x40}, /* analog pg 108 */
	{0x370e, 0x00}, /* analog pg 108 */
	{0x3710, 0x28}, /* analog pg 108 */
	{0x3711, 0x24},
	{0x3712, 0x13}, /* analog pg 108 */

	{0x3810, 0x00}, /* TIMING HVOFFS both are zero pg 80 */
	{0x3815, 0x82}, /* PCLK to SCLK ratio bit[4:0] is set to 2 pg 81 */
	{0x3830, 0x50}, /* manual exposure gain bit [0] */
	{0x3836, 0x00}, /* TIMING HVPAD both are zero pg 82 */

	{0x3a1a, 0x06}, /* DIFF MAX an AEC register??? pg 114 */
	{0x3a18, 0x00}, /* AEC gain ceiling bit 8 pg 114 */
	{0x3a19, 0xf8}, /* AEC gain ceiling pg 114 */
	{0x3a00, 0x38}, /* AEC control 0 debug mode band
			low limit mode band func pg 112 */
	{0x3a0d, 0x06}, /* b60 max pg 113 */
	{0x3c01, 0x34}, /* 5060HZ_CTRL01 pg 116 */

	{0x401f, 0x03}, /* BLC enabled pg 120 */
	{0x4000, 0x05}, /* BLC enabled pg 120 */
	{0x401d, 0x08}, /* reserved pg 120 */
	{0x4001, 0x02}, /* BLC control pg 120 */

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
	{0x3503, 0x33}, /* AEC auto AGC auto gain has delay of 2 frames.
						pg 38 */

	{0x3621, 0x2f}, /* analog horizontal binning/sampling not enabled.
						pg 108 */
	{0x3703, 0xe6}, /* analog pg 108 */
	{0x370c, 0x00}, /* analog pg 108 */
	{0x370d, 0x04}, /* analog pg 108 */
	{0x3713, 0x22}, /* analog pg 108 */
	{0x3714, 0x27},
	{0x3705, 0xda},
	{0x370a, 0x80},

	{0x3800, 0x02}, /* HREF start point higher 4 bits [3:0] pg 108 */
	{0x3801, 0x12}, /* HREF start point lower  8 bits [7:0] pg 108 */
	{0x3802, 0x00}, /* VREF start point higher 4 bits [3:0] pg 108 */
	{0x3803, 0x0a}, /* VREF start point [7:0] pg 108 */
	{0x3804, 0x08}, /* HREF width  higher 4 bits [3:0] pg 108 */
	{0x3805, 0x20}, /* HREF width  lower  8 bits [7:0] pg 108 */
	{0x3806, 0x04}, /* VREF height higher 4 bits [3:0] pg 109 */
	{0x3807, 0x92}, /* VREF height lower  8 bits [7:0] pg 109 */
	{0x3808, 0x08}, /* DVP horizontal output size higher 4 bits [3:0]
							pg 109 */
	{0x3809, 0x20}, /* DVP horizontal output size lower  8 bits [7:0]
							pg 109 */
	{0x380a, 0x04}, /* DVP vertical   output size higher 4 bits [3:0]
							pg 109 */
	{0x380b, 0x92}, /* DVP vertical   output size lower  8 bits [7:0]
							pg 109 */
	{0x380c, 0x0a}, /* total horizontal size higher 5 bits [4:0] pg 109,
							line length */
	{0x380d, 0x96}, /* total horizontal size lower  8 bits [7:0] pg 109,
							line length */
	{0x380e, 0x04}, /* total vertical size higher 5 bits [4:0] pg 109,
							frame length */
	{0x380f, 0x9e}, /* total vertical   size lower  8 bits [7:0] pg 109,
							frame length */
	{0x3818, 0xc0}, /* timing control reg18 mirror & dkhf pg 110 */
	{0x381a, 0x3c}, /* HS mirror adjustment pg 110 */
	{0x381c, 0x31},
	{0x381d, 0x8e},
	{0x381e, 0x04},
	{0x381f, 0x92},
	{0x3820, 0x04},
	{0x3821, 0x19},
	{0x3824, 0x01},
	{0x3827, 0x0a},
	{0x401c, 0x46},

	{0x3003, 0x03}, /* reset MIPI and DVP pg 97 */
	{0x3500, 0x00}, /* long exp 1/3 in unit of 1/16 line, pg 38 */
	{0x3501, 0x49}, /* long exp 2/3 in unit of 1/16 line, pg 38 */
	{0x3502, 0xa0}, /* long exp 3/3 in unit of 1/16 line, pg 38 */
	{0x350a, 0x00}, /* gain output to sensor, pg 38 */
	{0x350b, 0x00}, /* gain output to sensor, pg 38 */
	{0x4801, 0x0f}, /* MIPI control01 pg 125 */
	{0x300e, 0x0c}, /* SC_MIPI_SC_CTRL0 pg 73 */
	{0x4803, 0x50}, /* MIPI CTRL3 pg 91 */
	{0x4800, 0x34}, /* MIPI CTRl0 idle and short line pg 89 */

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_1920x1080[] = {
	{0x3103, 0x93}, // power up system clock from PLL page 77
	{0x3007, 0x3b}, // clock enable03 pg 98
	{0x3017, 0xff}, // PAD output enable page 100
	{0x3018, 0xfc}, // PAD output enable page 100

	{0x3600, 0x54}, // analog pg 108
	{0x3601, 0x05}, // analog pg 108
	{0x3603, 0xa7}, // analog pg 108
	{0x3604, 0x40}, // analog pg 108
	{0x3605, 0x04}, // analog pg 108
	{0x3606, 0x3f}, // analog pg 108
	{0x3612, 0x1a}, // analog pg 108
	{0x3613, 0x44}, // analog pg 108
	{0x3615, 0x52}, // analog pg 108
	{0x3620, 0x56}, // analog pg 108
	{0x3623, 0x01}, // analog pg 108
	{0x3630, 0x22}, // analog pg 108
	{0x3631, 0x36}, // analog pg 108
	{0x3632, 0x5f}, // analog pg 108
	{0x3633, 0x24}, // analog pg 108

	{0x3702, 0x3a}, // analog pg 108
	{0x3704, 0x18}, // analog pg 108
	{0x3706, 0x41}, // analog pg 108
	{0x370b, 0x40}, // analog pg 108
	{0x370e, 0x00}, // analog pg 108
	{0x3710, 0x28}, // analog pg 108
	{0x3711, 0x24},
	{0x3712, 0x13}, // analog pg 10

	{0x3810, 0x00}, // TIMING HVOFFS both are zero pg 80
	{0x3815, 0x82}, // PCLK to SCLK ratio bit[4:0] is set to 2 pg 81

	{0x3830, 0x50}, // manual exposure gain bit [0]
	{0x3836, 0x00}, // TIMING HVPAD both are zero pg 82

	{0x3a1a, 0x06}, // DIFF MAX an AEC register??? pg 114
	{0x3a18, 0x00}, // AEC gain ceiling bit 8 pg 114
	{0x3a19, 0xf8}, // AEC gain ceiling pg 114
	{0x3a00, 0x38}, // AEC control 0 debug mode band low limit mode band func pg 112
	{0x3a0d, 0x06}, // b60 max pg 113
	{0x3c01, 0x34}, // 5060HZ_CTRL01 pg 116

	{0x401f, 0x03}, // BLC enabled pg 120
	{0x4000, 0x05}, // BLC enabled pg 120
	{0x401d, 0x08}, // reserved pg 120
	{0x4001, 0x02}, // BLC control pg 120

	{0x5000, 0x00}, // ISP control00 features are disabled. pg 132
	{0x5001, 0x00}, // ISP control01 awb disabled. pg 132
	{0x5002, 0x00}, // ISP control02 debug mode disabled pg 132
	{0x503d, 0x00}, // ISP control3D features disabled pg 133
	{0x5046, 0x00}, // ISP control isp disable awbg disable pg 133

	{0x300f, 0x8f}, // PLL control00 R_SELD5 [7:6] div by 4 R_DIVL [2] two lane div 1 SELD2P5 [1:0] div 2.5 pg 99
	{0x3010, 0x10}, // PLL control01 DIVM [3:0] DIVS [7:4] div 1 pg 99
	{0x3011, 0x14}, // PLL control02 R_DIVP [5:0] div 20 pg 99
	{0x3012, 0x02}, // PLL CTR 03, default
	{0x3503, 0x33}, // AEC auto AGC auto gain has delay of 2 frames. pg 38

	{0x3621, 0x2f}, // analog horizontal binning/sampling not enabled. pg 108
	{0x3703, 0xe6}, // analog pg 108
	{0x370c, 0x00}, // analog pg 108
	{0x370d, 0x04}, // analog pg 108
	{0x3713, 0x22}, // analog pg 108
	{0x3714, 0x27},
	{0x3705, 0xda},
	{0x370a, 0x80},

	{0x3800, 0x02}, // HREF start point higher 4 bits [3:0] pg 108
	{0x3801, 0x94}, // HREF start point lower  8 bits [7:0] pg 108
	{0x3802, 0x00}, // VREF start point higher 4 bits [3:0] pg 108
	{0x3803, 0x0c}, // VREF start point [7:0] pg 108
	{0x3804, 0x07}, // HREF width  higher 4 bits [3:0] pg 108
	{0x3805, 0x80}, // HREF width  lower  8 bits [7:0] pg 108
	{0x3806, 0x04}, // VREF height higher 4 bits [3:0] pg 109
	{0x3807, 0x40}, // VREF height lower  8 bits [7:0] pg 109
	{0x3808, 0x07}, // DVP horizontal output size higher 4 bits [3:0] pg 109
	{0x3809, 0x80}, // DVP horizontal output size lower  8 bits [7:0] pg 109
	{0x380a, 0x04}, // DVP vertical   output size higher 4 bits [3:0] pg 109
	{0x380b, 0x40}, // DVP vertical   output size lower  8 bits [7:0] pg 109
	{0x380c, 0x0a}, // total horizontal size higher 5 bits [4:0] pg 109, line length
	{0x380d, 0x84}, // total horizontal size lower  8 bits [7:0] pg 109, line length
	{0x380e, 0x04}, // total vertical   size higher 5 bits [4:0] pg 109, frame length
	{0x380f, 0xa4}, // total vertical   size lower  8 bits [7:0] pg 109, frame length
	{0x3818, 0xc0}, // timing control reg18 mirror & dkhf pg 110
	{0x381a, 0x3c}, // HS mirror adjustment pg 110
	{0x381c, 0x31},
	{0x381d, 0xa4},
	{0x381e, 0x04},
	{0x381f, 0x60},
	{0x3820, 0x03},
	{0x3821, 0x1a},
	{0x3824, 0x01},
	{0x3827, 0x0a},
	{0x401c, 0x46},

	{0x3003, 0x03}, // reset MIPI and DVP pg 97
	{0x3500, 0x00}, // long exp 1/3 in unit of 1/16 line, pg 38
	{0x3501, 0x49}, // long exp 2/3 in unit of 1/16 line, pg 38
	{0x3502, 0xa0}, // long exp 3/3 in unit of 1/16 line, pg 38
	{0x350a, 0x00}, // gain output to sensor, pg 38
	{0x350b, 0x00}, // gain output to sensor, pg 38
	{0x4801, 0x0f}, // MIPI control01 pg 125
	{0x300e, 0x0c}, // SC_MIPI_SC_CTRL0 pg 73
	{0x4803, 0x50}, // MIPI CTRL3 pg 91
	{0x4800, 0x34}, // MIPI CTRl0 idle and short line pg 89

	{OV5650_TABLE_END, 0x0000}
};


static struct ov5650_reg mode_1264x704[] = {
	{0x3600, 0x54}, /* analog pg 108 */
	{0x3601, 0x05}, /* analog pg 108 */
	{0x3604, 0x40}, /* analog pg 108 */
	{0x3705, 0xdb}, /* analog pg 108 */
	{0x370a, 0x81}, /* analog pg 108 */
	{0x3615, 0x52}, /* analog pg 108 */
	{0x3810, 0x40}, /* TIMING HVOFFS both are zero pg 80 */
	{0x3836, 0x41}, /* TIMING HVPAD both are zero pg 82 */
	{0x4000, 0x05}, /* BLC enabled pg 120 */
	{0x401c, 0x42}, /* reserved pg 120 */
	{0x5046, 0x09}, /* ISP control isp disable awbg disable pg 133 */
	{0x3010, 0x00}, /* PLL control01 DIVM [3:0] DIVS [7:4] div 1 pg 99 */
	{0x3503, 0x00}, /* AEC auto AGC auto gain has no latch delay. pg 38 */
	{0x3613, 0xc4}, /* analog pg 108 */

	{0x3621, 0xaf}, /* analog horizontal binning/sampling enabled.
			   pg 108 */
	{0x3632, 0x55}, /* analog pg 108 */
	{0x3703, 0x9a}, /* analog pg 108 */
	{0x370c, 0x00}, /* analog pg 108 */
	{0x370d, 0x42}, /* analog pg 108 */
	{0x3713, 0x22}, /* analog pg 108 */
	{0x3800, 0x02}, /* HREF start point higher 4 bits [3:0] pg 108 */
	{0x3801, 0x54}, /* HREF start point lower  8 bits [7:0] pg 108 */
	{0x3802, 0x00}, /* VREF start point higher 4 bits [3:0] pg 108 */
	{0x3803, 0x0c}, /* VREF start point [7:0] pg 108 */
	{0x3804, 0x05}, /* HREF width  higher 4 bits [3:0] pg 108 */
	{0x3805, 0x00}, /* HREF width  lower  8 bits [7:0] pg 108 */
	{0x3806, 0x02}, /* VREF height higher 4 bits [3:0] pg 109 */
	{0x3807, 0xd0}, /* VREF height lower  8 bits [7:0] pg 109 */
	{0x3808, 0x05}, /* DVP horizontal output size higher 4 bits [3:0]
			   pg 109 */
	{0x3809, 0x00}, /* DVP horizontal output size lower  8 bits [7:0]
			   pg 109 */
	{0x380a, 0x02}, /* DVP vertical   output size higher 4 bits [3:0]
			   pg 109 */
	{0x380b, 0xd0}, /* DVP vertical   output size lower  8 bits [7:0]
			   pg 109 */
	{0x380c, 0x08}, /* total horizontal size higher 5 bits [4:0] pg 109,
			   line length */
	{0x380d, 0x72}, /* total horizontal size lower  8 bits [7:0] pg 109,
			   line length */
	{0x380e, 0x02}, /* total vertical size higher 5 bits [4:0] pg 109,
			   frame length */
	{0x380f, 0xe4}, /* total vertical size lower  8 bits [7:0] pg 109,
			   frame length */
	{0x3818, 0xc1}, /* timing control reg18 mirror & dkhf pg 110 */
	{0x381a, 0x3c}, /* HS mirror adjustment pg 110 */
	{0x3a0d, 0x06}, /* b60 max pg 113 */
	{0x3c01, 0x34}, /* 5060HZ_CTRL01 pg 116 */
	{0x3007, 0x3b}, /* clock enable03 pg 98 */
	{0x5059, 0x80}, /* => NOT found */
	{0x3003, 0x03}, /* reset MIPI and DVP pg 97 */
	{0x3500, 0x04}, /* long exp 1/3 in unit of 1/16 line, pg 38 */
	{0x3501, 0xa5}, /* long exp 2/3 in unit of 1/16 line, pg 38,
			   note frame length start with 0x7b0,
			   and SENSOR_BAYER_DEFAULT_MAX_COARSE_DIFF=3 */
	{0x3502, 0x10}, /* long exp 3/3 in unit of 1/16 line, pg 38.
			   Two lines of integration time. */
	{0x350a, 0x00}, /* gain output to sensor, pg 38 */
	{0x350b, 0x00}, /* gain output to sensor, pg 38 */
	{0x4801, 0x0f}, /* MIPI control01 pg 125 */
	{0x300e, 0x0c}, /* SC_MIPI_SC_CTRL0 pg 73 */
	{0x4803, 0x50}, /* MIPI CTRL3 pg 91 */
	{0x4800, 0x24}, /* MIPI CTRl0 idle and short line pg 89 */
	{0x300f, 0x8b}, /* PLL control00 R_SELD5 [7:6] div by 4 R_DIVL [2]
			   two lane div 1 SELD2P5 [1:0] div 2.5 pg 99 */

	{0x3711, 0x24},
	{0x3713, 0x92},
	{0x3714, 0x17},
	{0x381c, 0x10},
	{0x381d, 0x82},
	{0x381e, 0x05},
	{0x381f, 0xc0},
	{0x3821, 0x20},
	{0x3824, 0x23},
	{0x3825, 0x2c},
	{0x3826, 0x00},
	{0x3827, 0x0c},
	{0x3623, 0x01},
	{0x3633, 0x24},
	{0x3632, 0x5f},
	{0x401f, 0x03},

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_320x240[] = {
	{0x3103, 0x93},
	{0x3b07, 0x0c},
	{0x3017, 0xff},
	{0x3018, 0xfc},
	{0x3706, 0x41},
	{0x3613, 0xc4},
	{0x370d, 0x42},
	{0x3703, 0x9a},
	{0x3630, 0x22},
	{0x3605, 0x04},
	{0x3606, 0x3f},
	{0x3712, 0x13},
	{0x370e, 0x00},
	{0x370b, 0x40},
	{0x3600, 0x54},
	{0x3601, 0x05},
	{0x3713, 0x22},
	{0x3714, 0x27},
	{0x3631, 0x22},
	{0x3612, 0x1a},
	{0x3604, 0x40},
	{0x3705, 0xdc},
	{0x370a, 0x83},
	{0x370c, 0xc8},
	{0x3710, 0x28},
	{0x3702, 0x3a},
	{0x3704, 0x18},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3a00, 0x38},
	{0x3800, 0x02},
	{0x3801, 0x54},
	{0x3803, 0x0c},
	{0x380c, 0x0c},
	{0x380d, 0xb4},
	{0x380e, 0x07},
	{0x380f, 0xb0},
	{0x3830, 0x50},
	{0x3a08, 0x12},
	{0x3a09, 0x70},
	{0x3a0a, 0x0f},
	{0x3a0b, 0x60},
	{0x3a0d, 0x06},
	{0x3a0e, 0x06},
	{0x3a13, 0x54},
	{0x3815, 0x82},
	{0x5059, 0x80},
	{0x3615, 0x52},
	{0x505a, 0x0a},
	{0x505b, 0x2e},
	{0x3713, 0x92},
	{0x3714, 0x17},
	{0x3803, 0x0a},
	{0x3804, 0x05},
	{0x3805, 0x00},
	{0x3806, 0x01},
	{0x3807, 0x00},
	{0x3808, 0x01},
	{0x3809, 0x40},
	{0x380a, 0x01},
	{0x380b, 0x00},
	{0x380c, 0x0a}, /* total horizontal size higher 5 bits [4:0] pg 109,
			   line length */
	{0x380d, 0x04}, /* total horizontal size lower  8 bits [7:0] pg 109,
			   line length */
	{0x380e, 0x01}, /* total vertical size higher 5 bits [4:0] pg 109,
			   frame length */
	{0x380f, 0x38}, /* total vertical size lower  8 bits [7:0] pg 109,
			   frame length */
	{0x3815, 0x81},
	{0x3824, 0x23},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x370d, 0xc2},
	{0x3a08, 0x17},
	{0x3a09, 0x64},
	{0x3a0a, 0x13},
	{0x3a0b, 0x80},
	{0x3a00, 0x58},
	{0x3a1a, 0x06},
	{0x3503, 0x33},
	{0x3623, 0x01},
	{0x3633, 0x24},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c07, 0x07},
	{0x3c09, 0xc2},
	{0x4000, 0x05},
	{0x401d, 0x28},
	{0x4001, 0x02},
	{0x401c, 0x42},
	{0x5046, 0x09},
	{0x3810, 0x40},
	{0x3836, 0x41},
	{0x505f, 0x04},
	{0x5000, 0x06},
	{0x5001, 0x00},
	{0x5002, 0x02},
	{0x503d, 0x00},
	{0x5901, 0x08},
	{0x585a, 0x01},
	{0x585b, 0x2c},
	{0x585c, 0x01},
	{0x585d, 0x93},
	{0x585e, 0x01},
	{0x585f, 0x90},
	{0x5860, 0x01},
	{0x5861, 0x0d},
	{0x5180, 0xc0},
	{0x5184, 0x00},
	{0x470a, 0x00},
	{0x470b, 0x00},
	{0x470c, 0x00},
	{0x300f, 0x8e},
	{0x3603, 0xa7},
	{0x3632, 0x55},
	{0x3620, 0x56},
	{0x3621, 0xaf},
	{0x3818, 0xc3},
	{0x3631, 0x36},
	{0x3632, 0x5f},
	{0x3711, 0x24},
	{0x401f, 0x03},

	{0x3011, 0x14},
	{0x3007, 0x3B},
	{0x300f, 0x8f},  //0x8f:2-lane, 0x8b:1-lane
	{0x4801, 0x0f},
	{0x3003, 0x03},
	{0x300e, 0x0c},
	{0x3010, 0x15},  //120/60/30fps:0x15/0x35/0x75
	{0x4803, 0x50},
	{0x4800, 0x24},  //bit[5]=0 as CSI continuous clock
	{0x4837, 0x40},  //120/60/30fps:0x10/0x20/0x40
	{0x3815, 0x82},

	{OV5650_TABLE_END, 0x0000}
};

static struct ov5650_reg mode_end[] = {
	{0x3212, 0x00}, /* SRM_GROUP_ACCESS (group hold begin) */
	{0x3003, 0x01}, /* reset DVP pg 97 */
	{0x3212, 0x10}, /* SRM_GROUP_ACCESS (group hold end) */
	{0x3212, 0xa0}, /* SRM_GROUP_ACCESS (group hold launch) */
	{0x3008, 0x02}, /* SYSTEM_CTRL0 mipi suspend mask pg 98 */

	/*	{FAST_SETMODE_END, 0}, */
	{OV5650_TABLE_END, 0x0000}
};

enum {
	OV5650_MODE_2592x1944,
	OV5650_MODE_1296x972,
	OV5650_MODE_2080x1164,
	OV5650_MODE_1920x1080,
	OV5650_MODE_1264x704,
	OV5650_MODE_320x240,
	OV5650_MODE_INVALID
};

static struct ov5650_reg *mode_table[] = {
	[OV5650_MODE_2592x1944] = mode_2592x1944,
	[OV5650_MODE_1296x972]  = mode_1296x972,
	[OV5650_MODE_2080x1164] = mode_2080x1164,
	[OV5650_MODE_1920x1080] = mode_1920x1080,
	[OV5650_MODE_1264x704]  = mode_1264x704,
	[OV5650_MODE_320x240]   = mode_320x240
};

/* 2 regs to program frame length */
static inline void ov5650_get_frame_length_regs(struct ov5650_reg *regs,
						u32 frame_length)
{
	regs->addr = 0x380e;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x380f;
	(regs + 1)->val = (frame_length) & 0xff;
}

/* 3 regs to program coarse time */
static inline void ov5650_get_coarse_time_regs(struct ov5650_reg *regs,
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
static inline void ov5650_get_gain_reg(struct ov5650_reg *regs, u16 gain)
{
	regs->addr = 0x350b;
	regs->val = gain;
}

static int ov5650_read_reg(struct i2c_client *client, u16 addr, u8 *val)
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

	if (err != 1)
		return -EINVAL;

	*val = data[2];

	return 0;
}

static int ov5650_read_reg_helper(struct ov5650_info *info,
					u16 addr, u8 *val)
{
	int ret;
	switch (info->camera_mode) {
	case Main:
	case StereoCameraMode_Left:
		ret = ov5650_read_reg(info->left.i2c_client, addr, val);
		break;
	case StereoCameraMode_Stereo:
		ret = ov5650_read_reg(info->left.i2c_client, addr, val);
		if (ret)
			break;
		ret = ov5650_read_reg(info->right.i2c_client, addr, val);
		break;
	case StereoCameraMode_Right:
		ret = ov5650_read_reg(info->right.i2c_client, addr, val);
		break;
	default:
		return -1;
	}
	return ret;
}

static int ov5650_write_reg(struct i2c_client *client, u16 addr, u8 val)
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
		pr_err("ov5650: i2c transfer failed, retrying %x %x\n",
			addr, val);
		usleep_range(3000, 3250);
	} while (retry <= OV5650_MAX_RETRIES);

	return err;
}

static int ov5650_write_reg_helper(struct ov5650_info *info,
					u16 addr, u8 val)
{
	int ret;
	switch (info->camera_mode) {
	case Main:
	case StereoCameraMode_Left:
		ret = ov5650_write_reg(info->left.i2c_client, addr, val);
		break;
	case StereoCameraMode_Stereo:
		ret = ov5650_write_reg(info->left.i2c_client, addr, val);
		if (ret)
			break;
		ret = ov5650_write_reg(info->right.i2c_client, addr, val);
		break;
	case StereoCameraMode_Right:
		ret = ov5650_write_reg(info->right.i2c_client, addr, val);
		break;
	default:
		return -1;
	}
	return ret;
}

static int ov5650_write_table(struct ov5650_info *info,
				const struct ov5650_reg table[],
				const struct ov5650_reg override_list[],
				int num_override_regs)
{
	int err;
	const struct ov5650_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != OV5650_TABLE_END; next++) {
		if (next->addr == OV5650_TABLE_WAIT_MS) {
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

		err = ov5650_write_reg_helper(info, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static int ov5650_set_mode(struct ov5650_info *info, struct ov5650_mode *mode)
{
	int sensor_mode;
	int err;
	struct ov5650_reg reg_list[6];

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
		__func__, mode->xres, mode->yres, mode->frame_length,
		mode->coarse_time, mode->gain);
	if (mode->xres == 2592 && mode->yres == 1944)
		sensor_mode = OV5650_MODE_2592x1944;
	else if (mode->xres == 1296 && mode->yres == 972)
		sensor_mode = OV5650_MODE_1296x972;
	else if (mode->xres == 2080 && mode->yres == 1164)
		sensor_mode = OV5650_MODE_2080x1164;
	else if (mode->xres == 1920 && mode->yres == 1080)
		sensor_mode = OV5650_MODE_1920x1080;
	else if (mode->xres == 1264 && mode->yres == 704)
		sensor_mode = OV5650_MODE_1264x704;
	else if (mode->xres == 320 && mode->yres == 240)
		sensor_mode = OV5650_MODE_320x240;
	else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	ov5650_get_frame_length_regs(reg_list, mode->frame_length);
	ov5650_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	ov5650_get_gain_reg(reg_list + 5, mode->gain);

	/* Check what condition reset and mode start sequences are */
	/* needed. For switching between certain modes, these are  */
	/* not required. Skipping them saves time for I2C access   */
	if ((info->mode == OV5650_MODE_INVALID) ||
		((last_mode != mode_2592x1944) &&
		(last_mode != mode_1296x972)) ||
		((mode_table[sensor_mode] != mode_2592x1944) &&
		(mode_table[sensor_mode] != mode_1296x972))) {
		err = ov5650_write_table(info, reset_seq, NULL, 0);
		if (err)
			return err;

		err = ov5650_write_table(info, mode_start, NULL, 0);
		if (err)
			return err;
	}

	last_mode = mode_table[sensor_mode];

	err = ov5650_write_table(info, mode_table[sensor_mode],
		reg_list, 6);
	if (err)
		return err;

	err = ov5650_write_table(info, mode_end, NULL, 0);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int ov5650_set_frame_length(struct ov5650_info *info, u32 frame_length)
{
	struct ov5650_reg reg_list[2];
	int i = 0;
	int ret;

	ov5650_get_frame_length_regs(reg_list, frame_length);

	for (i = 0; i < 2; i++)	{
		ret = ov5650_write_reg_helper(info, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

static int ov5650_set_coarse_time(struct ov5650_info *info, u32 coarse_time)
{
	int ret;

	struct ov5650_reg reg_list[3];
	int i = 0;

	ov5650_get_coarse_time_regs(reg_list, coarse_time);

	ret = ov5650_write_reg_helper(info, 0x3212, 0x01);
	if (ret)
		return ret;

	for (i = 0; i < 3; i++)	{
		ret = ov5650_write_reg_helper(info, reg_list[i].addr,
			reg_list[i].val);
		if (ret)
			return ret;
	}

	ret = ov5650_write_reg_helper(info, 0x3212, 0x11);
	if (ret)
		return ret;

	ret = ov5650_write_reg_helper(info, 0x3212, 0xa1);
	if (ret)
		return ret;

	return 0;
}

static int ov5650_set_gain(struct ov5650_info *info, u16 gain)
{
	int ret;
	struct ov5650_reg reg_list;

	ov5650_get_gain_reg(&reg_list, gain);

	ret = ov5650_write_reg_helper(info, reg_list.addr, reg_list.val);

	return ret;
}

static int ov5650_set_binning(struct ov5650_info *info, u8 enable)
{
	s32 ret;
	u8  array_ctrl_reg, analog_ctrl_reg, timing_reg;
	u32 val;

	if (info->mode == OV5650_MODE_2592x1944
	 || info->mode == OV5650_MODE_2080x1164
	 || info->mode >= OV5650_MODE_INVALID) {
		return -EINVAL;
	}

	ov5650_read_reg_helper(info, OV5650_ARRAY_CONTROL_01, &array_ctrl_reg);
	ov5650_read_reg_helper(info, OV5650_ANALOG_CONTROL_D, &analog_ctrl_reg);
	ov5650_read_reg_helper(info, OV5650_TIMING_TC_REG_18, &timing_reg);

	/* Group 3 begin (pg.78) */
	ret = ov5650_write_reg_helper(info,
			OV5650_SRM_GRUP_ACCESS,
			OV5650_GROUP_ID(3));
	if (ret < 0)
		return -EIO;

	if (!enable) {
		/* 2x2 subsampling
		 * ----------------
		 * address | value
		 * --------+-------
		 *  0x3621 | 0xEF
		 *  0x370D | 0x02
		 *  0x3818 | 0xC1
		 * ----------------
		 */
	ret = ov5650_write_reg_helper(info,
		OV5650_ARRAY_CONTROL_01,
		array_ctrl_reg |
		(OV5650_H_BINNING_BIT | OV5650_H_SUBSAMPLING_BIT));

	if (ret < 0)
		goto exit;

	ret = ov5650_write_reg_helper(info,
		OV5650_ANALOG_CONTROL_D,
		analog_ctrl_reg & ~OV5650_V_BINNING_BIT);

	if (ret < 0)
		goto exit;

	ret = ov5650_write_reg_helper(info,
		OV5650_TIMING_TC_REG_18,
		timing_reg | OV5650_V_SUBSAMPLING_BIT);

	if (ret < 0)
		goto exit;

	if (info->mode == OV5650_MODE_1296x972)
		val = 0x1A2;
	else
		/* FIXME: this value is not verified yet. */
		val = 0x1A8;

	ret = ov5650_write_reg_helper(info,
		OV5650_TIMING_CONTROL_HS_HIGH,
		(val >> 8));

	if (ret < 0)
		goto exit;

	ret = ov5650_write_reg_helper(info,
		OV5650_TIMING_CONTROL_HS_LOW,
		(val & 0xFF));
	} else {
		/* 2x2 binning
		 * ----------------
		 * address | value
		 * --------+-------
		 *  0x3621 | 0xAF
		 *  0x370D | 0x42
		 *  0x3818 | 0xC1
		 * ----------------
		 */
		ret = ov5650_write_reg_helper(info,
			OV5650_ARRAY_CONTROL_01,
			(array_ctrl_reg | OV5650_H_BINNING_BIT)
			& ~OV5650_H_SUBSAMPLING_BIT);

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_ANALOG_CONTROL_D,
			analog_ctrl_reg | OV5650_V_BINNING_BIT);

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_TC_REG_18,
			timing_reg | OV5650_V_SUBSAMPLING_BIT);

		if (ret < 0)
			goto exit;

		if (info->mode == OV5650_MODE_1296x972)
			val = 0x33C;
		else
			val = 0x254;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_HIGH,
			(val >> 8));

		if (ret < 0)
			goto exit;

		ret = ov5650_write_reg_helper(info,
			OV5650_TIMING_CONTROL_HS_LOW,
			(val & 0xFF));
	}

exit:
	/* Group 3 end (pg.78) */
	ret = ov5650_write_reg_helper(info,
		OV5650_SRM_GRUP_ACCESS,
		(OV5650_GROUP_HOLD_END_BIT | OV5650_GROUP_ID(3)));

	/* Group3 launch (pg.78) */
	ret |= ov5650_write_reg_helper(info,
		OV5650_SRM_GRUP_ACCESS,
		(OV5650_GROUP_HOLD_BIT | OV5650_GROUP_LAUNCH_BIT | OV5650_GROUP_ID(3)));

	return ret;
}

static int ov5650_test_pattern(struct ov5650_info *info,
			       enum ov5650_test_pattern pattern)
{
	if (pattern >= ARRAY_SIZE(test_pattern_modes))
		return -EINVAL;

	return ov5650_write_table(info,
				  test_pattern_modes[pattern],
				  NULL, 0);
}

static int set_power_helper(struct ov5650_platform_data *pdata,
				int powerLevel)
{
	if (pdata) {
		if (powerLevel && pdata->power_on) {
			pdata->power_on();
		} else if (pdata->power_off) {
			pdata->power_off();
			stereo_ov5650_info->mode = OV5650_MODE_INVALID;
		}
	}
	return 0;
}

static int ov5650_set_power(int powerLevel)
{
	pr_info("%s: powerLevel=%d camera mode=%d\n", __func__, powerLevel,
			stereo_ov5650_info->camera_mode);

	if (StereoCameraMode_Left & stereo_ov5650_info->camera_mode)
		set_power_helper(stereo_ov5650_info->left.pdata, powerLevel);

	if (StereoCameraMode_Right & stereo_ov5650_info->camera_mode)
		set_power_helper(stereo_ov5650_info->right.pdata, powerLevel);

	return 0;
}

static long ov5650_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct ov5650_info *info = file->private_data;

	switch (cmd) {
	case OV5650_IOCTL_SET_CAMERA_MODE:
	{
		if (info->camera_mode != arg) {
			err = ov5650_set_power(0);
			if (err) {
				pr_info("%s %d\n", __func__, __LINE__);
				return err;
			}
			info->camera_mode = arg;
			err = ov5650_set_power(1);
			if (err)
				return err;
		}
		return 0;
	}
	case OV5650_IOCTL_SYNC_SENSORS:
		if (info->right.pdata->synchronize_sensors)
			info->right.pdata->synchronize_sensors();
		return 0;
	case OV5650_IOCTL_SET_MODE:
	{
		struct ov5650_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov5650_mode))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}

		return ov5650_set_mode(info, &mode);
	}
	case OV5650_IOCTL_SET_FRAME_LENGTH:
		return ov5650_set_frame_length(info, (u32)arg);
	case OV5650_IOCTL_SET_COARSE_TIME:
		return ov5650_set_coarse_time(info, (u32)arg);
	case OV5650_IOCTL_SET_GAIN:
		return ov5650_set_gain(info, (u16)arg);
	case OV5650_IOCTL_SET_BINNING:
		return ov5650_set_binning(info, (u8)arg);
	case OV5650_IOCTL_GET_STATUS:
	{
		u16 status = 0;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}
	case OV5650_IOCTL_TEST_PATTERN:
	{
		err = ov5650_test_pattern(info, (enum ov5650_test_pattern) arg);
		if (err)
			pr_err("%s %d %d\n", __func__, __LINE__, err);
		return err;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static int ov5650_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	file->private_data = stereo_ov5650_info;
	ov5650_set_power(1);
	return 0;
}

int ov5650_release(struct inode *inode, struct file *file)
{
	ov5650_set_power(0);
	file->private_data = NULL;
	return 0;
}


static const struct file_operations ov5650_fileops = {
	.owner = THIS_MODULE,
	.open = ov5650_open,
	.unlocked_ioctl = ov5650_ioctl,
	.release = ov5650_release,
};

static struct miscdevice ov5650_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov5650",
	.fops = &ov5650_fileops,
};

static int ov5650_probe_common(void)
{
	int err;

	if (!stereo_ov5650_info) {
		stereo_ov5650_info = kzalloc(sizeof(struct ov5650_info),
					GFP_KERNEL);
		if (!stereo_ov5650_info) {
			pr_err("ov5650: Unable to allocate memory!\n");
			return -ENOMEM;
		}

		err = misc_register(&ov5650_device);
		if (err) {
			pr_err("ov5650: Unable to register misc device!\n");
			kfree(stereo_ov5650_info);
			return err;
		}
	}
	return 0;
}

static int ov5650_remove_common(struct i2c_client *client)
{
	if (stereo_ov5650_info->left.i2c_client ||
		stereo_ov5650_info->right.i2c_client)
		return 0;

	misc_deregister(&ov5650_device);
	kfree(stereo_ov5650_info);
	stereo_ov5650_info = NULL;

	return 0;
}

static int left_ov5650_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	pr_info("%s: probing sensor.\n", __func__);

	err = ov5650_probe_common();
	if (err)
		return err;

	stereo_ov5650_info->left.pdata = client->dev.platform_data;
	stereo_ov5650_info->left.i2c_client = client;

	return 0;
}

static int left_ov5650_remove(struct i2c_client *client)
{
	if (stereo_ov5650_info) {
		stereo_ov5650_info->left.i2c_client = NULL;
		ov5650_remove_common(client);
	}
	return 0;
}

static const struct i2c_device_id left_ov5650_id[] = {
	{ "ov5650", 0 },
	{ "ov5650L", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, left_ov5650_id);

static struct i2c_driver left_ov5650_i2c_driver = {
	.driver = {
		.name = "ov5650",
		.owner = THIS_MODULE,
	},
	.probe = left_ov5650_probe,
	.remove = left_ov5650_remove,
	.id_table = left_ov5650_id,
};

static int right_ov5650_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	pr_info("%s: probing sensor.\n", __func__);

	err = ov5650_probe_common();
	if (err)
		return err;

	stereo_ov5650_info->right.pdata = client->dev.platform_data;
	stereo_ov5650_info->right.i2c_client = client;

	return 0;
}

static int right_ov5650_remove(struct i2c_client *client)
{
	if (stereo_ov5650_info) {
		stereo_ov5650_info->right.i2c_client = NULL;
		ov5650_remove_common(client);
	}
	return 0;
}

static const struct i2c_device_id right_ov5650_id[] = {
	{ "ov5650R", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, right_ov5650_id);

static struct i2c_driver right_ov5650_i2c_driver = {
	.driver = {
		.name = "ov5650R",
		.owner = THIS_MODULE,
	},
	.probe = right_ov5650_probe,
	.remove = right_ov5650_remove,
	.id_table = right_ov5650_id,
};

static int __init ov5650_init(void)
{
	int ret;
	pr_info("ov5650 sensor driver loading\n");
	ret = i2c_add_driver(&left_ov5650_i2c_driver);
	if (ret)
		return ret;
	return i2c_add_driver(&right_ov5650_i2c_driver);
}

static void __exit ov5650_exit(void)
{
	i2c_del_driver(&right_ov5650_i2c_driver);
	i2c_del_driver(&left_ov5650_i2c_driver);
}

module_init(ov5650_init);
module_exit(ov5650_exit);

