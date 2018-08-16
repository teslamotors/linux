// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 - 2018 Intel Corporation

#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN V4L2_CID_GAIN
#endif

#define OV8856_REG_VALUE_08BIT		1
#define OV8856_REG_VALUE_16BIT		2
#define OV8856_REG_VALUE_24BIT		3

#define OV8856_REG_MODE_SELECT		0x0100
#define OV8856_MODE_STANDBY		0x00
#define OV8856_MODE_STREAMING		0x01

#define OV8856_REG_SOFTWARE_RST	0x0103
#define OV8856_SOFTWARE_RST		0x01

/* Chip ID */
#define OV8856_REG_CHIP_ID		0x300a
#define OV8856_CHIP_ID			0x00885a

/* V_TIMING internal */
#define OV8856_REG_VTS			0x380e
#define OV8856_VTS_30FPS		0x09b2 /* 30 fps */
#define OV8856_VTS_60FPS		0x04da /* 60 fps */
#define OV8856_VTS_MAX			0x7fff

/* HBLANK control - read only */
#define OV8856_PPL_270MHZ		1648
#define OV8856_PPL_540MHZ		3296

/* Exposure control */
#define OV8856_REG_EXPOSURE		0x3500
#define OV8856_EXPOSURE_MIN		4
#define OV8856_EXPOSURE_STEP		1
#define OV8856_EXPOSURE_DEFAULT	0x640

/* Analog gain control */
#define OV8856_REG_ANALOG_GAIN		0x3508
#define OV8856_ANA_GAIN_MIN		0
#define OV8856_ANA_GAIN_MAX		0x1fff
#define OV8856_ANA_GAIN_STEP		1
#define OV8856_ANA_GAIN_DEFAULT	0x80

/* Digital gain control */
#define OV8856_REG_B_MWB_GAIN		0x5019
#define OV8856_REG_G_MWB_GAIN		0x501b
#define OV8856_REG_R_MWB_GAIN		0x501d
#define OV8856_DGTL_GAIN_MIN		0
#define OV8856_DGTL_GAIN_MAX		16384	/* Max = 16 X */
#define OV8856_DGTL_GAIN_DEFAULT	1024	/* Default gain = 1 X */
#define OV8856_DGTL_GAIN_STEP		1	/* Each step = 1/1024 */

/* Test Pattern Control */
#define OV8856_REG_TEST_PATTERN	0x4503
#define OV8856_TEST_PATTERN_ENABLE	BIT(7)
#define OV8856_TEST_PATTERN_MASK	0xfc

/* Number of frames to skip */
#define OV8856_NUM_OF_SKIP_FRAMES	2

struct ov8856_reg {
	u16 address;
	u8 val;
};

struct ov8856_reg_list {
	u32 num_of_regs;
	const struct ov8856_reg *regs;
};

/* Link frequency config */
struct ov8856_link_freq_config {
	u32 pixels_per_line;

	/* PLL registers for this link frequency */
	struct ov8856_reg_list reg_list;
};

/* Mode : resolution and related config&values */
struct ov8856_mode {
	/* Frame width */
	u32 width;
	/* Frame height */
	u32 height;

	/* V-timing */
	u32 vts_def;
	u32 vts_min;

	/* Index of Link frequency config to be used */
	u32 link_freq_index;
	/* Default register values */
	struct ov8856_reg_list reg_list;
};

/* 4224x3136 needs 1080Mbps/lane, 4 lanes */
static const struct ov8856_reg mipi_data_rate_1080mbps[] = {
	/* PLL1 registers */
	{0x0302, 0x26 }, /* 3c for 19.2Mhz input clk */
	{0x0303, 0x00 }, /* 01 for 19.2Mhz input clk */
	{0x030d, 0x26 }, /* 1e for 19.2Mhz input clk */
	{0x031e, 0x0c },
	{0x3000, 0x00 },
	{0x300e, 0x00 },
	{0x3010, 0x00 },
	{0x3015, 0x84 },
	{0x3018, 0x72 },
	{0x3033, 0x24 },
	{0x3500, 0x00 },
	{0x3501, 0x9a },
	{0x3502, 0x20 },
	{0x3503, 0x00 },
	{0x3505, 0x83 },
	{0x3508, 0x00 },
	{0x3509, 0x80 },
	{0x350c, 0x00 },
	{0x350d, 0x80 },
	{0x350e, 0x04 },
	{0x350f, 0x00 },
	{0x3510, 0x00 },
	{0x3511, 0x02 },
	{0x3512, 0x00 },
	{0x3600, 0x72 },
	{0x3601, 0x40 },
	{0x3602, 0x30 },
	{0x3610, 0xc5 },
	{0x3611, 0x58 },
	{0x3612, 0x5c },
	{0x3613, 0x5a },
	{0x3614, 0x60 },
	{0x3628, 0xff },
	{0x3629, 0xff },
	{0x362a, 0xff },
	{0x3633, 0x10 },
	{0x3634, 0x10 },
	{0x3635, 0x10 },
	{0x3636, 0x10 },
	{0x3663, 0x08 },
	{0x3669, 0x34 },
	{0x366e, 0x10 },
	{0x3706, 0x86 },
	{0x370b, 0x7e },
	{0x3714, 0x23 },
	{0x3730, 0x12 },
	{0x3733, 0x10 },
	{0x3764, 0x00 },
	{0x3765, 0x00 },
	{0x3769, 0x62 },
	{0x376a, 0x2a },
	{0x376b, 0x30 },
	{0x3780, 0x00 },
	{0x3781, 0x24 },
	{0x3782, 0x00 },
	{0x3783, 0x23 },
	{0x3798, 0x2f },
	{0x37a1, 0x60 },
	{0x37a8, 0x6a },
	{0x37ab, 0x3f },
	{0x37c2, 0x04 },
	{0x37c3, 0xf1 },
	{0x37c9, 0x80 },
	{0x37cb, 0x03 },
	{0x37cc, 0x0a },
	{0x37cd, 0x16 },
	{0x37ce, 0x1f },
	{0x3814, 0x01 },
	{0x3815, 0x01 },
	{0x3816, 0x00 },
	{0x3817, 0x00 },
	{0x3818, 0x00 },
	{0x3819, 0x00 },
	{0x3820, 0x80 },
	{0x3821, 0x46 },
	{0x382a, 0x01 },
	{0x382b, 0x01 },
	{0x3830, 0x06 },
	{0x3836, 0x02 },
	{0x3862, 0x04 },
	{0x3863, 0x08 },
	{0x3cc0, 0x33 },
	{0x3d85, 0x17 },
	{0x3d8c, 0x73 },
	{0x3d8d, 0xde },
	{0x4001, 0xe0 },
	{0x4003, 0x40 },
	{0x4008, 0x00 },
	{0x4009, 0x0b },
	{0x400a, 0x00 },
	{0x400b, 0x84 },
	{0x400f, 0x80 },
	{0x4010, 0xf0 },
	{0x4011, 0xff },
	{0x4012, 0x02 },
	{0x4013, 0x01 },
	{0x4014, 0x01 },
	{0x4015, 0x01 },
	{0x4042, 0x00 },
	{0x4043, 0x80 },
	{0x4044, 0x00 },
	{0x4045, 0x80 },
	{0x4046, 0x00 },
	{0x4047, 0x80 },
	{0x4048, 0x00 },
	{0x4049, 0x80 },
	{0x4041, 0x03 },
	{0x404c, 0x20 },
	{0x404d, 0x00 },
	{0x404e, 0x20 },
	{0x4203, 0x80 },
	{0x4307, 0x30 },
	{0x4317, 0x00 },
	{0x4503, 0x08 },
	{0x4601, 0x80 },
	{0x4816, 0x53 },
	{0x481b, 0x58 },
	{0x481f, 0x27 },
	{0x482a, 0x3C }, /* set sensor TX to 60*UI + 145ns */
	{0x4818, 0x00 }, /* set sensor TX to 60*UI + 145ns */
	{0x4819, 0x91 }, /* set sensor TX to 60*UI + 145ns */
	{0x4837, 0x15 }, /* 16 for 19.2Mhz input clk */
	{0x5000, 0x77 },
	{0x5001, 0x0a },
	{0x5004, 0x06 },
	{0x502e, 0x03 },
	{0x5030, 0x41 },
	{0x5795, 0x02 },
	{0x5796, 0x20 },
	{0x5797, 0x20 },
	{0x5798, 0xd5 },
	{0x5799, 0xd5 },
	{0x579a, 0x00 },
	{0x579b, 0x50 },
	{0x579c, 0x00 },
	{0x579d, 0x2c },
	{0x579e, 0x0c },
	{0x579f, 0x40 },
	{0x57a0, 0x09 },
	{0x57a1, 0x40 },
	{0x5780, 0x14 },
	{0x5781, 0x0f },
	{0x5782, 0x44 },
	{0x5783, 0x02 },
	{0x5784, 0x01 },
	{0x5785, 0x01 },
	{0x5786, 0x00 },
	{0x5787, 0x04 },
	{0x5788, 0x02 },
	{0x5789, 0x0f },
	{0x578a, 0xfd },
	{0x578b, 0xf5 },
	{0x578c, 0xf5 },
	{0x578d, 0x03 },
	{0x578e, 0x08 },
	{0x578f, 0x0c },
	{0x5790, 0x08 },
	{0x5791, 0x04 },
	{0x5792, 0x00 },
	{0x5793, 0x52 },
	{0x5794, 0xa3 },
	{0x59f8, 0x3d },
	{0x5a08, 0x02 },
	{0x5b00, 0x02 },
	{0x5b01, 0x10 },
	{0x5b02, 0x03 },
	{0x5b03, 0xcf },
	{0x5b05, 0x6c },
	{0x5e00, 0x00 },
};

/*
 * 2112x1568, 2112x1188, 1056x784 need 540Mbps/lane,
 * 4 lanes
 */
static const struct ov8856_reg mipi_data_rate_540mbps[] = {
	{0x0302, 0x43 }, /* 3c for 19.2Mhz input clk */
	{0x0303, 0x00 }, /* 01 for 19.2Mhz input clk */
	{0x030d, 0x26 }, /* 1e for 19.2Mhz input clk */
	{0x031e, 0x0c },
	{0x3000, 0x00 },
	{0x300e, 0x00 },
	{0x3010, 0x00 },
	{0x3015, 0x84 },
	{0x3018, 0x32 },
	{0x3033, 0x24 },
	{0x3500, 0x00 },
	{0x3501, 0x9a },
	{0x3502, 0x20 },
	{0x3503, 0x00 },
	{0x3505, 0x83 },
	{0x3508, 0x00 },
	{0x3509, 0x80 },
	{0x350c, 0x00 },
	{0x350d, 0x80 },
	{0x350e, 0x04 },
	{0x350f, 0x00 },
	{0x3510, 0x00 },
	{0x3511, 0x02 },
	{0x3512, 0x00 },
	{0x3600, 0x72 },
	{0x3601, 0x40 },
	{0x3602, 0x30 },
	{0x3610, 0xc5 },
	{0x3611, 0x58 },
	{0x3612, 0x5c },
	{0x3613, 0x5a },
	{0x3614, 0x60 },
	{0x3628, 0xff },
	{0x3629, 0xff },
	{0x362a, 0xff },
	{0x3633, 0x10 },
	{0x3634, 0x10 },
	{0x3635, 0x10 },
	{0x3636, 0x10 },
	{0x3663, 0x08 },
	{0x3669, 0x34 },
	{0x366e, 0x10 },
	{0x3706, 0x86 },
	{0x370b, 0x7e },
	{0x3714, 0x23 },
	{0x3730, 0x12 },
	{0x3733, 0x10 },
	{0x3764, 0x00 },
	{0x3765, 0x00 },
	{0x3769, 0x62 },
	{0x376a, 0x2a },
	{0x376b, 0x30 },
	{0x3780, 0x00 },
	{0x3781, 0x24 },
	{0x3782, 0x00 },
	{0x3783, 0x23 },
	{0x3798, 0x2f },
	{0x37a1, 0x60 },
	{0x37a8, 0x6a },
	{0x37ab, 0x3f },
	{0x37c2, 0x04 },
	{0x37c3, 0xf1 },
	{0x37c9, 0x80 },
	{0x37cb, 0x03 },
	{0x37cc, 0x0a },
	{0x37cd, 0x16 },
	{0x37ce, 0x1f },
	{0x3814, 0x01 },
	{0x3815, 0x01 },
	{0x3816, 0x00 },
	{0x3817, 0x00 },
	{0x3818, 0x00 },
	{0x3819, 0x00 },
	{0x3820, 0x80 },
	{0x3821, 0x46 },
	{0x382a, 0x01 },
	{0x382b, 0x01 },
	{0x3830, 0x06 },
	{0x3836, 0x02 },
	{0x3862, 0x04 },
	{0x3863, 0x08 },
	{0x3cc0, 0x33 },
	{0x3d85, 0x17 },
	{0x3d8c, 0x73 },
	{0x3d8d, 0xde },
	{0x4001, 0xe0 },
	{0x4003, 0x40 },
	{0x4008, 0x00 },
	{0x4009, 0x0b },
	{0x400a, 0x00 },
	{0x400b, 0x84 },
	{0x400f, 0x80 },
	{0x4010, 0xf0 },
	{0x4011, 0xff },
	{0x4012, 0x02 },
	{0x4013, 0x01 },
	{0x4014, 0x01 },
	{0x4015, 0x01 },
	{0x4042, 0x00 },
	{0x4043, 0x80 },
	{0x4044, 0x00 },
	{0x4045, 0x80 },
	{0x4046, 0x00 },
	{0x4047, 0x80 },
	{0x4048, 0x00 },
	{0x4049, 0x80 },
	{0x4041, 0x03 },
	{0x404c, 0x20 },
	{0x404d, 0x00 },
	{0x404e, 0x20 },
	{0x4203, 0x80 },
	{0x4307, 0x30 },
	{0x4317, 0x00 },
	{0x4503, 0x08 },
	{0x4601, 0x80 },
	{0x4816, 0x53 },
	{0x481b, 0x58 },
	{0x481f, 0x27 },
	{0x482a, 0x06 }, /* set sensor TX to 60*UI + 145ns */
	{0x4818, 0x00 }, /* set sensor TX to 60*UI + 145ns */
	{0x4819, 0x70 }, /* set sensor TX to 60*UI + 145ns */
	{0x4837, 0x0C },
	{0x5000, 0x77 },
	{0x5001, 0x0a },
	{0x5004, 0x06 },
	{0x502e, 0x03 },
	{0x5030, 0x41 },
	{0x5795, 0x02 },
	{0x5796, 0x20 },
	{0x5797, 0x20 },
	{0x5798, 0xd5 },
	{0x5799, 0xd5 },
	{0x579a, 0x00 },
	{0x579b, 0x50 },
	{0x579c, 0x00 },
	{0x579d, 0x2c },
	{0x579e, 0x0c },
	{0x579f, 0x40 },
	{0x57a0, 0x09 },
	{0x57a1, 0x40 },
	{0x5780, 0x14 },
	{0x5781, 0x0f },
	{0x5782, 0x44 },
	{0x5783, 0x02 },
	{0x5784, 0x01 },
	{0x5785, 0x01 },
	{0x5786, 0x00 },
	{0x5787, 0x04 },
	{0x5788, 0x02 },
	{0x5789, 0x0f },
	{0x578a, 0xfd },
	{0x578b, 0xf5 },
	{0x578c, 0xf5 },
	{0x578d, 0x03 },
	{0x578e, 0x08 },
	{0x578f, 0x0c },
	{0x5790, 0x08 },
	{0x5791, 0x04 },
	{0x5792, 0x00 },
	{0x5793, 0x52 },
	{0x5794, 0xa3 },
	{0x59f8, 0x3d },
	{0x5a08, 0x02 },
	{0x5b00, 0x02 },
	{0x5b01, 0x10 },
	{0x5b02, 0x03 },
	{0x5b03, 0xcf },
	{0x5b05, 0x6c },
	{0x5e00, 0x00 },
};

static const struct ov8856_reg mode_3264x2448_regs[] = {
	{0x3800, 0x00 },
	{0x3801, 0x00 },
	{0x3802, 0x00 },
	{0x3803, 0x0c },
	{0x3804, 0x0c },
	{0x3805, 0xdf },
	{0x3806, 0x09 },
	{0x3807, 0xa3 },
	{0x3808, 0x0c },
	{0x3809, 0xc0 },
	{0x380a, 0x09 },
	{0x380b, 0x90 },
	{0x380c, 0x07 },
	{0x380d, 0x8c },
	{0x380e, 0x09 },
	{0x380f, 0xb2 },
	{0x3810, 0x00 },
	{0x3811, 0x10 },
	{0x3812, 0x00 },
	{0x3813, 0x03 },
};

static const struct ov8856_reg mode_3280x2464_regs[] = {
	{0x3800, 0x00 },
	{0x3801, 0x00 },
	{0x3802, 0x00 },
	{0x3803, 0x04 },
	{0x3804, 0x0c },
	{0x3805, 0xdf },
	{0x3806, 0x09 },
	{0x3807, 0xa7 },
	{0x3808, 0x0c },
	{0x3809, 0xd0 },
	{0x380a, 0x09 },
	{0x380b, 0xa0 },
	{0x380c, 0x07 },
	{0x380d, 0x8c },
	{0x380e, 0x09 },
	{0x380f, 0xc2 },
	{0x3810, 0x00 },
	{0x3811, 0x08 },
	{0x3812, 0x00 },
	{0x3813, 0x03 },
};

static const struct ov8856_reg mode_3280x1848_regs[] = {
	{0x3800, 0x00 },
	{0x3801, 0x00 },
	{0x3802, 0x01 },
	{0x3803, 0x38 },
	{0x3804, 0x0c },
	{0x3805, 0xdf },
	{0x3806, 0x09 },
	{0x3807, 0x77 },
	{0x3808, 0x0c },
	{0x3809, 0xd0 },
	{0x380a, 0x07 },
	{0x380b, 0x38 },
	{0x380c, 0x07 },
	{0x380d, 0x8c },
	{0x380e, 0x09 },
	{0x380f, 0xc2 },
	{0x3810, 0x00 },
	{0x3811, 0x08 },
	{0x3812, 0x00 },
	{0x3813, 0x03 },
};

static const struct ov8856_reg mode_1940x1096_regs[] = {
	/* 60 FPS */
	{0x3800, 0x02 },
	{0x3801, 0x8e },
	{0x3802, 0x03 },
	{0x3803, 0xc6 },
	{0x3804, 0x0a },
	{0x3805, 0x49 },
	{0x3806, 0x08 },
	{0x3807, 0x92 },
	{0x3808, 0x07 },
	{0x3809, 0x94 },
	{0x380a, 0x04 },
	{0x380b, 0x48 },
	{0x380c, 0x07 },
	{0x380d, 0x8c },
	{0x380e, 0x04 },
	{0x380f, 0xda },
	{0x3810, 0x00 },
	{0x3811, 0x10 },
	{0x3812, 0x00 },
	{0x3813, 0x03 },
};

static const struct ov8856_reg mode_2112x1186_regs[] = {
	/* 60 FPS */
	{0x3800, 0x02 },
	{0x3801, 0x40 },
	{0x3802, 0x02 },
	{0x3803, 0x82 },
	{0x3804, 0x0a },
	{0x3805, 0x9f },
	{0x3806, 0x07 },
	{0x3807, 0x2d },
	{0x3808, 0x08 },
	{0x3809, 0x40 },
	{0x380a, 0x04 },
	{0x380b, 0xa4 },
	{0x380c, 0x07 },
	{0x380d, 0x8c },
	{0x380e, 0x04 },
	{0x380f, 0xda },
	{0x3810, 0x00 },
	{0x3811, 0x10 },
	{0x3812, 0x00 },
	{0x3813, 0x03 },
};

static const char * const ov8856_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Configurations for supported link frequencies */
#define OV8856_NUM_OF_LINK_FREQS	2
#define OV8856_LINK_FREQ_540MHZ	540000000ULL
#define OV8856_LINK_FREQ_270MHZ	270000000ULL
#define OV8856_LINK_FREQ_INDEX_0	0
#define OV8856_LINK_FREQ_INDEX_1	1

/* Menu items for LINK_FREQ V4L2 control */
static const s64 link_freq_menu_items[OV8856_NUM_OF_LINK_FREQS] = {
	OV8856_LINK_FREQ_540MHZ,
	OV8856_LINK_FREQ_270MHZ
};

/* Link frequency configs */
static const struct ov8856_link_freq_config
link_freq_configs[OV8856_NUM_OF_LINK_FREQS] = {
	{
		.pixels_per_line = OV8856_PPL_540MHZ,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mipi_data_rate_1080mbps),
			.regs = mipi_data_rate_1080mbps,
		}
	},
	{
		.pixels_per_line = OV8856_PPL_270MHZ,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mipi_data_rate_540mbps),
			.regs = mipi_data_rate_540mbps,
		}
	}
};

/* Mode configs */
static const struct ov8856_mode supported_modes[] = {
	{
		.width = 3264,
		.height = 2448,
		.vts_def = OV8856_VTS_30FPS,
		.vts_min = OV8856_VTS_30FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3264x2448_regs),
			.regs = mode_3264x2448_regs,
		},
		.link_freq_index = OV8856_LINK_FREQ_INDEX_0,
	},
	{
		.width = 3280,
		.height = 2464,
		.vts_def = 0x9c2,
		.vts_min = 0x9c2,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3280x2464_regs),
			.regs = mode_3280x2464_regs,
		},
		.link_freq_index = OV8856_LINK_FREQ_INDEX_0,
	},
	{
		.width = 3280,
		.height = 1848,
		.vts_def = OV8856_VTS_30FPS,
		.vts_min = OV8856_VTS_30FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3280x1848_regs),
			.regs = mode_3280x1848_regs,
		},
		.link_freq_index = OV8856_LINK_FREQ_INDEX_0,
	},
	{
		.width = 1940,
		.height = 1096,
		.vts_def = OV8856_VTS_60FPS,
		.vts_min = OV8856_VTS_60FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1940x1096_regs),
			.regs = mode_1940x1096_regs,
		},
		.link_freq_index = OV8856_LINK_FREQ_INDEX_1,
	},
	{
		.width = 2112,
		.height = 1186,
		.vts_def = OV8856_VTS_60FPS,
		.vts_min = OV8856_VTS_60FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_2112x1186_regs),
			.regs = mode_2112x1186_regs,
		},
		.link_freq_index = OV8856_LINK_FREQ_INDEX_1,
	},
};

struct ov8856 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_ctrl_handler ctrl_handler;
	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;

	/* Current mode */
	const struct ov8856_mode *cur_mode;

	/* Mutex for serialized access */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;
};

#define to_ov8856(_sd)	container_of(_sd, struct ov8856, sd)

/* Read registers up to 4 at a time */
static int ov8856_read_reg(struct ov8856 *ov8856, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	int ret;
	u32 data_be = 0;
	u16 reg_addr_be = cpu_to_be16(reg);

	if (len > 4)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

/* Write registers up to 4 at a time */
static int ov8856_write_reg(struct ov8856 *ov8856, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	int buf_i, val_i;
	u8 buf[6], *val_p;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val = cpu_to_be32(val);
	val_p = (u8 *)&val;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/* Write a list of registers */
static int ov8856_write_regs(struct ov8856 *ov8856,
			     const struct ov8856_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	int ret;
	u32 i;

	for (i = 0; i < len; i++) {
		ret = ov8856_write_reg(ov8856, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(
				&client->dev,
				"Failed to write reg 0x%4.4x. error = %d\n",
				regs[i].address, ret);

			return ret;
		}
	}

	return 0;
}

static int ov8856_write_reg_list(struct ov8856 *ov8856,
				 const struct ov8856_reg_list *r_list)
{
	return ov8856_write_regs(ov8856, r_list->regs, r_list->num_of_regs);
}

/* Open sub-device */
static int ov8856_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	struct v4l2_mbus_framefmt *try_fmt = v4l2_subdev_get_try_format(sd,
									fh->pad,
									0);

	mutex_lock(&ov8856->mutex);

	/* Initialize try_fmt */
	try_fmt->width = ov8856->cur_mode->width;
	try_fmt->height = ov8856->cur_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	/* No crop or compose */
	mutex_unlock(&ov8856->mutex);

	return 0;
}

static int ov8856_update_digital_gain(struct ov8856 *ov8856, u32 d_gain)
{
	int ret;

	ret = ov8856_write_reg(ov8856, OV8856_REG_B_MWB_GAIN,
			       OV8856_REG_VALUE_16BIT, d_gain);
	if (ret)
		return ret;

	ret = ov8856_write_reg(ov8856, OV8856_REG_G_MWB_GAIN,
			       OV8856_REG_VALUE_16BIT, d_gain);
	if (ret)
		return ret;

	ret = ov8856_write_reg(ov8856, OV8856_REG_R_MWB_GAIN,
			       OV8856_REG_VALUE_16BIT, d_gain);

	return ret;
}

static int ov8856_enable_test_pattern(struct ov8856 *ov8856, u32 pattern)
{
	int ret;
	u32 val;

	ret = ov8856_read_reg(ov8856, OV8856_REG_TEST_PATTERN,
			      OV8856_REG_VALUE_08BIT, &val);
	if (ret)
		return ret;

	if (pattern) {
		val &= OV8856_TEST_PATTERN_MASK;
		val |= (pattern - 1) | OV8856_TEST_PATTERN_ENABLE;
	} else {
		val &= ~OV8856_TEST_PATTERN_ENABLE;
	}

	return ov8856_write_reg(ov8856, OV8856_REG_TEST_PATTERN,
				OV8856_REG_VALUE_08BIT, val);
}

static int ov8856_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov8856 *ov8856 = container_of(ctrl->handler,
					     struct ov8856, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	s64 max;
	int ret;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ov8856->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(ov8856->exposure,
					 ov8856->exposure->minimum,
					 max, ov8856->exposure->step, max);
		break;
	};

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) <= 0)
		return 0;

	ret = 0;
	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ov8856_write_reg(ov8856, OV8856_REG_ANALOG_GAIN,
				       OV8856_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = ov8856_update_digital_gain(ov8856, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = ov8856_write_reg(ov8856, OV8856_REG_EXPOSURE,
				       OV8856_REG_VALUE_24BIT,
				       ctrl->val << 4);
		break;
	case V4L2_CID_VBLANK:
		/* Update VTS that meets expected vertical blanking */
		ret = ov8856_write_reg(ov8856, OV8856_REG_VTS,
				       OV8856_REG_VALUE_16BIT,
				       ov8856->cur_mode->height + ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ov8856_enable_test_pattern(ov8856, ctrl->val);
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		break;
	};

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov8856_ctrl_ops = {
	.s_ctrl = ov8856_set_ctrl,
};

static int ov8856_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	/* Only one bayer order(GRBG) is supported */
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov8856_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static void ov8856_update_pad_format(const struct ov8856_mode *mode,
				     struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->format.field = V4L2_FIELD_NONE;
}

static int ov8856_do_get_pad_format(struct ov8856 *ov8856,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt;
	struct v4l2_subdev *sd = &ov8856->sd;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		fmt->format = *framefmt;
	} else {
		ov8856_update_pad_format(ov8856->cur_mode, fmt);
	}

	return 0;
}

static int ov8856_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	int ret;

	mutex_lock(&ov8856->mutex);
	ret = ov8856_do_get_pad_format(ov8856, cfg, fmt);
	mutex_unlock(&ov8856->mutex);

	return ret;
}

/*
 * Calculate resolution distance
 */
static int
ov8856_get_resolution_dist(const struct ov8856_mode *mode,
			   struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

/*
 * Find the closest supported resolution to the requested resolution
 */
static const struct ov8856_mode *
ov8856_find_best_fit(struct ov8856 *ov8856,
		     struct v4l2_subdev_format *fmt)
{
	int i, dist, cur_best_fit = 0, cur_best_fit_dist = -1;
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = ov8856_get_resolution_dist(&supported_modes[i],
						  framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int
ov8856_set_pad_format(struct v4l2_subdev *sd,
		      struct v4l2_subdev_pad_config *cfg,
		      struct v4l2_subdev_format *fmt)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	const struct ov8856_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	s32 vblank_def;
	s32 vblank_min;
	s64 h_blank;
	s64 pixel_rate;

	mutex_lock(&ov8856->mutex);

	/* Only one raw bayer(GRBG) order is supported */
	if (fmt->format.code != MEDIA_BUS_FMT_SGRBG10_1X10)
		fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;

	mode = ov8856_find_best_fit(ov8856, fmt);
	ov8856_update_pad_format(mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else {
		ov8856->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(ov8856->link_freq, mode->link_freq_index);
		pixel_rate =
			(link_freq_menu_items[mode->link_freq_index] * 2 * 4) /
			10;
		__v4l2_ctrl_s_ctrl_int64(ov8856->pixel_rate, pixel_rate);
		/* Update limits and set FPS to default */
		vblank_def = ov8856->cur_mode->vts_def -
			ov8856->cur_mode->height;
		vblank_min = ov8856->cur_mode->vts_min -
			ov8856->cur_mode->height;
		__v4l2_ctrl_modify_range(
			ov8856->vblank, vblank_min,
			OV8856_VTS_MAX - ov8856->cur_mode->height, 1,
			vblank_def);
		__v4l2_ctrl_s_ctrl(ov8856->vblank, vblank_def);
		h_blank =
			link_freq_configs[mode->link_freq_index].pixels_per_line
			- ov8856->cur_mode->width;
		__v4l2_ctrl_modify_range(ov8856->hblank, h_blank,
					 h_blank, 1, h_blank);
	}

	mutex_unlock(&ov8856->mutex);

	return 0;
}

static int ov8856_get_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	*frames = OV8856_NUM_OF_SKIP_FRAMES;

	return 0;
}

/* Start streaming */
static int ov8856_start_streaming(struct ov8856 *ov8856)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	const struct ov8856_reg_list *reg_list;
	int ret, link_freq_index;

	/* Get out of from software reset */
	ret = ov8856_write_reg(ov8856, OV8856_REG_SOFTWARE_RST,
			       OV8856_REG_VALUE_08BIT, OV8856_SOFTWARE_RST);
	if (ret) {
		dev_err(&client->dev, "%s failed to set powerup registers\n",
			__func__);
		return ret;
	}

	/* Setup PLL */
	link_freq_index = ov8856->cur_mode->link_freq_index;
	reg_list = &link_freq_configs[link_freq_index].reg_list;
	ret = ov8856_write_reg_list(ov8856, reg_list);
	if (ret) {
		dev_err(&client->dev, "%s failed to set plls\n", __func__);
		return ret;
	}

	/* Apply default values of current mode */
	reg_list = &ov8856->cur_mode->reg_list;
	ret = ov8856_write_reg_list(ov8856, reg_list);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(ov8856->sd.ctrl_handler);
	if (ret)
		return ret;

	return ov8856_write_reg(ov8856, OV8856_REG_MODE_SELECT,
				OV8856_REG_VALUE_08BIT,
				OV8856_MODE_STREAMING);
}

/* Stop streaming */
static int ov8856_stop_streaming(struct ov8856 *ov8856)
{
	return ov8856_write_reg(ov8856, OV8856_REG_MODE_SELECT,
				OV8856_REG_VALUE_08BIT, OV8856_MODE_STANDBY);
}

static int ov8856_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&ov8856->mutex);
	if (ov8856->streaming == enable) {
		mutex_unlock(&ov8856->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto err_unlock;
		}

		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = ov8856_start_streaming(ov8856);
		if (ret)
			goto err_rpm_put;
	} else {
		ov8856_stop_streaming(ov8856);
		pm_runtime_put(&client->dev);
	}

	ov8856->streaming = enable;
	mutex_unlock(&ov8856->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
err_unlock:
	mutex_unlock(&ov8856->mutex);

	return ret;
}

static int __maybe_unused ov8856_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);

	if (ov8856->streaming)
		ov8856_stop_streaming(ov8856);

	return 0;
}

static int __maybe_unused ov8856_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);
	int ret;

	if (ov8856->streaming) {
		ret = ov8856_start_streaming(ov8856);
		if (ret)
			goto error;
	}

	return 0;

error:
	ov8856_stop_streaming(ov8856);
	ov8856->streaming = 0;
	return ret;
}

/* Verify chip ID */
static int ov8856_identify_module(struct ov8856 *ov8856)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	int ret;
	u32 val;

	ret = ov8856_read_reg(ov8856, OV8856_REG_CHIP_ID,
			      OV8856_REG_VALUE_24BIT, &val);
	if (ret)
		return ret;

	if (val != OV8856_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x\n",
			OV8856_CHIP_ID, val);
		return -EIO;
	}
	return 0;
}

static const struct v4l2_subdev_video_ops ov8856_video_ops = {
	.s_stream = ov8856_set_stream,
};

static const struct v4l2_subdev_pad_ops ov8856_pad_ops = {
	.enum_mbus_code = ov8856_enum_mbus_code,
	.get_fmt = ov8856_get_pad_format,
	.set_fmt = ov8856_set_pad_format,
	.enum_frame_size = ov8856_enum_frame_size,
};

static const struct v4l2_subdev_sensor_ops ov8856_sensor_ops = {
	.g_skip_frames = ov8856_get_skip_frames,
};

static const struct v4l2_subdev_ops ov8856_subdev_ops = {
	.video = &ov8856_video_ops,
	.pad = &ov8856_pad_ops,
	.sensor = &ov8856_sensor_ops,
};

static const struct media_entity_operations ov8856_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_internal_ops ov8856_internal_ops = {
	.open = ov8856_open,
};

/* Initialize control handlers */
static int ov8856_init_controls(struct ov8856 *ov8856)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ov8856->sd);
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max;
	s64 vblank_def;
	s64 vblank_min;
	s64 hblank;
	s64 pixel_rate_min;
	s64 pixel_rate_max;
	const struct ov8856_mode *mode;
	int ret;

	ctrl_hdlr = &ov8856->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	mutex_init(&ov8856->mutex);
	ctrl_hdlr->lock = &ov8856->mutex;
	ov8856->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
						   &ov8856_ctrl_ops,
						   V4L2_CID_LINK_FREQ,
						   OV8856_NUM_OF_LINK_FREQS - 1,
						   0, link_freq_menu_items);
	ov8856->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* pixel_rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate_max = (link_freq_menu_items[0] * 2 * 4) / 10;
	pixel_rate_min = (link_freq_menu_items[1] * 2 * 4) / 10;
	/* By default, PIXEL_RATE is read only */
	ov8856->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ov8856_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       pixel_rate_min, pixel_rate_max,
					       1, pixel_rate_max);

	mode = ov8856->cur_mode;
	vblank_def = mode->vts_def - mode->height;
	vblank_min = mode->vts_min - mode->height;
	ov8856->vblank = v4l2_ctrl_new_std(
		ctrl_hdlr, &ov8856_ctrl_ops, V4L2_CID_VBLANK,
		vblank_min, OV8856_VTS_MAX - mode->height, 1,
		vblank_def);

	hblank = link_freq_configs[mode->link_freq_index].pixels_per_line -
		mode->width;
	ov8856->hblank = v4l2_ctrl_new_std(
		ctrl_hdlr, &ov8856_ctrl_ops, V4L2_CID_HBLANK,
		hblank, hblank, 1, hblank);
	ov8856->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	exposure_max = mode->vts_def - 8;
	ov8856->exposure = v4l2_ctrl_new_std(
		ctrl_hdlr, &ov8856_ctrl_ops,
		V4L2_CID_EXPOSURE, OV8856_EXPOSURE_MIN,
		exposure_max, OV8856_EXPOSURE_STEP,
		OV8856_EXPOSURE_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &ov8856_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  OV8856_ANA_GAIN_MIN, OV8856_ANA_GAIN_MAX,
			  OV8856_ANA_GAIN_STEP, OV8856_ANA_GAIN_DEFAULT);

	/* Digital gain */
	v4l2_ctrl_new_std(ctrl_hdlr, &ov8856_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  OV8856_DGTL_GAIN_MIN, OV8856_DGTL_GAIN_MAX,
			  OV8856_DGTL_GAIN_STEP, OV8856_DGTL_GAIN_DEFAULT);

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ov8856_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov8856_test_pattern_menu) - 1,
				     0, 0, ov8856_test_pattern_menu);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ov8856->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&ov8856->mutex);

	return ret;
}

static void ov8856_free_controls(struct ov8856 *ov8856)
{
	v4l2_ctrl_handler_free(ov8856->sd.ctrl_handler);
	mutex_destroy(&ov8856->mutex);
}

static int ov8856_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ov8856 *ov8856;
	int ret;

	dev_info(&client->dev, "ov8856_probe. %d-%04x\n",
		 client->adapter->nr, client->addr);
	ov8856 = devm_kzalloc(&client->dev, sizeof(*ov8856), GFP_KERNEL);
	if (!ov8856)
		return -ENOMEM;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&ov8856->sd, client, &ov8856_subdev_ops);

	/* Check module identity */
	ret = ov8856_identify_module(ov8856);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d\n", ret);
		return ret;
	}

	/* Set default mode to max resolution */
	ov8856->cur_mode = &supported_modes[0];

	ret = ov8856_init_controls(ov8856);
	if (ret)
		return ret;

	/* Initialize subdev */
	ov8856->sd.internal_ops = &ov8856_internal_ops;
	ov8856->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov8856->sd.entity.ops = &ov8856_subdev_entity_ops;
	ov8856->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	ov8856->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ov8856->sd.entity, 1, &ov8856->pad);
	if (ret) {
		dev_err(&client->dev, "%s failed:%d\n", __func__, ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev(&ov8856->sd);
	if (ret < 0)
		goto error_media_entity;

	/*
	 * Device is already turned on by i2c-core with ACPI domain PM.
	 * Enable runtime PM and turn off the device.
	 */
	pm_runtime_get_noresume(&client->dev);
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_put(&client->dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&ov8856->sd.entity);

error_handler_free:
	ov8856_free_controls(ov8856);
	dev_err(&client->dev, "%s failed:%d\n", __func__, ret);

	return ret;
}

static int ov8856_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	ov8856_free_controls(ov8856);

	/*
	 * Disable runtime PM but keep the device turned on.
	 * i2c-core with ACPI domain PM will turn off the device.
	 */
	pm_runtime_get_sync(&client->dev);
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	return 0;
}

static const struct i2c_device_id ov8856_id_table[] = {
	{"ov8856", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov8856_id_table);

static const struct dev_pm_ops ov8856_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov8856_suspend, ov8856_resume)
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id ov8856_acpi_ids[] = {
	{"OVTID858"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(acpi, ov8856_acpi_ids);
#endif

static struct i2c_driver ov8856_i2c_driver = {
	.driver = {
		.name = "ov8856",
		.owner = THIS_MODULE,
		.pm = &ov8856_pm_ops,
		/*.acpi_match_table = ACPI_PTR(ov8856_acpi_ids),*/
	},
	.probe = ov8856_probe,
	.remove = ov8856_remove,
	.id_table = ov8856_id_table,
};

module_i2c_driver(ov8856_i2c_driver);

MODULE_AUTHOR("Pu, Yuning <yuning.pu@intel.com>");
MODULE_AUTHOR("Qiu, Tianshu <tian.shu.qiu@intel.com>");
MODULE_DESCRIPTION("Omnivision ov8856 sensor driver");
MODULE_LICENSE("GPL v2");
