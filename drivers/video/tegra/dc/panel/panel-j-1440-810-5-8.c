/*
 * arch/arm/mach-tegra/panel-j-1440-810-5-8.c
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>
#include <linux/pwm_backlight.h>

#include <mach/dc.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "board-panel.h"

#include "gpio-names.h"

#define DSI_PANEL_EN_GPIO	TEGRA_GPIO_PQ2

#define DSI_PANEL_RESET		1

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

static struct tegra_dsi_out dsi_j_1440_810_5_8_pdata;

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;

static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *avdd_lcd_3v0;
static struct regulator *dvdd_lcd_3v3;

static u16 en_panel_rst;
static u16 en_panel;

static struct tegra_dc_sd_settings dsi_j_1440_810_5_8_sd_settings = {
	.enable = 0, /* disabled by default. */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 1,
	.use_vid_luma = false,
	.phase_in_adjustments = 0,
	.k_limit_enable = true,
	.k_limit = 180,
	.sd_window_enable = false,
	.soft_clipping_enable = true,
	/* Low soft clipping threshold to compensate for aggressive k_limit */
	.soft_clipping_threshold = 128,
	.smooth_k_enable = true,
	.smooth_k_incr = 16,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Gammas: R: 2.2 G: 2.2 B: 2.2 */
	/* Default BL TF */
	.bltf = {
			{
				{57, 65, 73, 82},
				{92, 103, 114, 125},
				{138, 150, 164, 178},
				{193, 208, 224, 241},
			},
		},
	/* Default LUT */
	.lut = {
			{
				{255, 255, 255},
				{199, 199, 199},
				{153, 153, 153},
				{116, 116, 116},
				{85, 85, 85},
				{59, 59, 59},
				{36, 36, 36},
				{17, 17, 17},
				{0, 0, 0},
			},
		},
	.sd_brightness = &sd_brightness,
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_j_1440_810_5_8_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,  1,  2,  4,  5,  6,  7,  9,
		10,  11,  12,  14,  15,  16,  18,  19,
		21,  23,  25,  27,  29,  31,  33,  35,
		37,  40,  42,  45,  47,  50,  53,  56,
		59,  62,  65,  69,  72,  75,  79,  83,
		87,  90,  94,  99,  103,  107,  111,  116,
		121,  125,  130,  135,  140,  145,  151,  156,
		161,  167,  173,  178,  184,  190,  197,  203,
		209,  216,  222,  229,  236,  243,  250,  257,
		264,  272,  279,  287,  295,  303,  311,  319,
		327,  336,  344,  353,  362,  371,  380,  389,
		398,  408,  417,  427,  437,  447,  457,  467,
		477,  488,  498,  509,  520,  531,  542,  553,
		565,  576,  588,  600,  612,  624,  636,  649,
		661,  674,  687,  699,  713,  726,  739,  753,
		766,  780,  794,  808,  822,  837,  851,  866,
		881,  896,  911,  926,  941,  957,  973,  989,
		1005,  1021,  1037,  1053,  1070,  1087,  1104,  1121,
		1138,  1155,  1173,  1190,  1208,  1226,  1244,  1263,
		1281,  1300,  1318,  1337,  1356,  1376,  1395,  1415,
		1434,  1454,  1474,  1494,  1515,  1535,  1556,  1577,
		1598,  1619,  1640,  1662,  1683,  1705,  1727,  1749,
		1771,  1794,  1816,  1839,  1862,  1885,  1909,  1932,
		1956,  1979,  2003,  2027,  2052,  2076,  2101,  2126,
		2151,  2176,  2201,  2227,  2252,  2278,  2304,  2330,
		2357,  2383,  2410,  2437,  2464,  2491,  2518,  2546,
		2573,  2601,  2629,  2658,  2686,  2715,  2744,  2773,
		2802,  2831,  2860,  2890,  2920,  2950,  2980,  3011,
		3041,  3072,  3103,  3134,  3165,  3197,  3228,  3260,
		3292,  3325,  3357,  3390,  3422,  3455,  3488,  3522,
		3555,  3589,  3623,  3657,  3691,  3725,  3760,  3795,
		3830,  3865,  3900,  3936,  3972,  4008,  4044,  4080,
	},
	/* csc */
	{
		0x0FE, 0x005, 0x3FB,
		0x000, 0x0CD, 0x000,
		0x3FF, 0x001, 0x0BD,
	},
	/* lut2 maps linear space to sRGB*/
	{
		0,  2,  3,  5,  7,  9,  10,  12,
		14,  15,  16,  17,  17,  18,  18,  19,
		19,  20,  20,  21,  21,  22,  22,  23,
		23,  24,  24,  25,  25,  26,  26,  27,
		28,  28,  29,  29,  30,  30,  31,  31,
		32,  32,  32,  33,  33,  33,  33,  34,
		34,  34,  34,  35,  35,  35,  36,  36,
		36,  36,  37,  37,  37,  37,  38,  38,
		38,  38,  39,  39,  39,  40,  40,  40,
		40,  41,  41,  41,  41,  42,  42,  42,
		43,  43,  43,  43,  44,  44,  44,  44,
		45,  45,  45,  45,  46,  46,  46,  47,
		47,  47,  47,  48,  48,  48,  48,  48,
		49,  49,  49,  49,  49,  49,  50,  50,
		50,  50,  50,  50,  51,  51,  51,  51,
		51,  51,  52,  52,  52,  52,  52,  52,
		53,  53,  53,  53,  53,  53,  54,  54,
		54,  54,  54,  54,  54,  55,  55,  55,
		55,  55,  55,  56,  56,  56,  56,  56,
		56,  57,  57,  57,  57,  57,  57,  58,
		58,  58,  58,  58,  58,  59,  59,  59,
		59,  59,  59,  60,  60,  60,  60,  60,
		60,  61,  61,  61,  61,  61,  61,  62,
		62,  62,  62,  62,  62,  63,  63,  63,
		63,  63,  63,  64,  64,  64,  64,  64,
		64,  64,  65,  65,  65,  65,  65,  65,
		65,  66,  66,  66,  66,  66,  66,  66,
		66,  67,  67,  67,  67,  67,  67,  67,
		68,  68,  68,  68,  68,  68,  68,  69,
		69,  69,  69,  69,  69,  69,  70,  70,
		70,  70,  70,  70,  70,  70,  71,  71,
		71,  71,  71,  71,  71,  72,  72,  72,
		72,  72,  72,  72,  73,  73,  73,  73,
		73,  73,  73,  74,  74,  74,  74,  74,
		74,  74,  74,  75,  75,  75,  75,  75,
		75,  75,  76,  76,  76,  76,  76,  76,
		76,  77,  77,  77,  77,  77,  77,  77,
		78,  78,  78,  78,  78,  78,  78,  78,
		79,  79,  79,  79,  79,  79,  79,  80,
		80,  80,  80,  80,  80,  80,  80,  81,
		81,  81,  81,  81,  81,  81,  81,  81,
		81,  82,  82,  82,  82,  82,  82,  82,
		82,  82,  83,  83,  83,  83,  83,  83,
		83,  83,  83,  83,  84,  84,  84,  84,
		84,  84,  84,  84,  84,  85,  85,  85,
		85,  85,  85,  85,  85,  85,  85,  86,
		86,  86,  86,  86,  86,  86,  86,  86,
		87,  87,  87,  87,  87,  87,  87,  87,
		87,  87,  88,  88,  88,  88,  88,  88,
		88,  88,  88,  89,  89,  89,  89,  89,
		89,  89,  89,  89,  89,  90,  90,  90,
		90,  90,  90,  90,  90,  90,  91,  91,
		91,  91,  91,  91,  91,  91,  91,  91,
		92,  92,  92,  92,  92,  92,  92,  92,
		92,  93,  93,  93,  93,  93,  93,  93,
		93,  93,  93,  94,  94,  94,  94,  94,
		94,  94,  94,  94,  94,  95,  95,  95,
		95,  95,  95,  95,  95,  95,  96,  96,
		96,  96,  96,  96,  96,  96,  96,  96,
		96,  97,  97,  97,  97,  97,  97,  97,
		97,  97,  97,  97,  97,  98,  98,  98,
		98,  98,  98,  98,  98,  98,  98,  98,
		98,  99,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99,  99,  99,  100,  100,
		100,  101,  101,  102,  103,  103,  104,  105,
		105,  106,  107,  107,  108,  108,  109,  110,
		110,  111,  112,  112,  113,  113,  114,  114,
		115,  116,  116,  117,  117,  118,  118,  119,
		119,  120,  120,  121,  122,  122,  123,  123,
		124,  124,  125,  125,  126,  126,  127,  128,
		128,  129,  129,  130,  130,  130,  131,  131,
		132,  132,  133,  133,  134,  134,  135,  135,
		136,  136,  137,  137,  138,  138,  139,  139,
		140,  140,  141,  141,  142,  142,  143,  143,
		144,  144,  145,  145,  146,  146,  147,  147,
		147,  148,  148,  149,  149,  150,  150,  151,
		151,  152,  152,  153,  153,  153,  154,  154,
		155,  155,  156,  156,  157,  157,  158,  158,
		159,  159,  159,  160,  160,  161,  161,  162,
		162,  163,  163,  164,  164,  164,  165,  165,
		166,  166,  167,  167,  168,  168,  169,  169,
		169,  170,  170,  171,  171,  172,  172,  173,
		173,  174,  174,  174,  175,  175,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		180,  180,  180,  181,  181,  181,  182,  182,
		182,  183,  183,  184,  184,  184,  185,  185,
		185,  186,  186,  187,  187,  187,  188,  188,
		188,  189,  189,  190,  190,  190,  191,  191,
		191,  192,  192,  192,  193,  193,  193,  194,
		194,  194,  194,  195,  195,  195,  195,  196,
		196,  196,  197,  197,  197,  197,  198,  198,
		198,  199,  199,  199,  199,  200,  200,  200,
		200,  201,  201,  201,  202,  202,  202,  202,
		203,  203,  203,  204,  204,  204,  204,  205,
		205,  205,  206,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  210,  211,  211,  211,
		211,  212,  212,  212,  212,  212,  213,  213,
		213,  213,  214,  214,  214,  214,  215,  215,
		215,  215,  216,  216,  216,  216,  217,  217,
		217,  217,  218,  218,  218,  218,  219,  219,
		219,  219,  220,  220,  220,  220,  221,  221,
		221,  221,  222,  222,  222,  222,  223,  223,
		223,  223,  224,  224,  224,  224,  225,  225,
		225,  226,  226,  226,  226,  227,  227,  227,
		227,  228,  228,  228,  228,  229,  229,  229,
		230,  230,  230,  230,  231,  231,  231,  231,
		232,  232,  232,  233,  233,  233,  233,  234,
		234,  234,  234,  235,  235,  235,  236,  236,
		236,  236,  237,  237,  237,  237,  238,  238,
		238,  238,  239,  239,  239,  240,  240,  240,
		240,  241,  241,  241,  241,  241,  242,  242,
		242,  242,  242,  243,  243,  243,  243,  243,
		244,  244,  244,  244,  245,  245,  245,  245,
		245,  246,  246,  246,  246,  246,  247,  247,
		247,  247,  247,  248,  248,  248,  248,  249,
		249,  249,  249,  249,  250,  250,  250,  250,
		250,  251,  251,  251,  251,  252,  252,  252,
		252,  252,  253,  253,  253,  253,  253,  254,
		254,  254,  254,  254,  255,  255,  255,  255,
	},
};
#endif

static tegra_dc_bl_output dsi_j_1440_810_5_8_bl_response_curve = {
	0, 1, 2, 3, 4, 5, 6, 8, 9, 10,
	11, 12, 13, 14, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 70,
	71, 72, 73, 74, 74, 75, 76, 77,
	78, 79, 80, 81, 82, 83, 84, 85,
	86, 87, 88, 89, 90, 91, 92, 93,
	94, 95, 96, 97, 98, 99, 99, 100,
	101, 103, 104, 105, 106, 107, 108,
	109, 110, 111, 112, 113, 114, 114,
	115, 116, 117, 118, 119, 120, 121,
	122, 123, 124, 125, 126, 127, 128,
	129, 130, 131, 132, 133, 134, 135,
	136, 137, 138, 139, 140, 141, 142,
	143, 144, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 154, 155, 156,
	157, 158, 159, 160, 161, 162, 163,
	164, 165, 166, 168, 169, 170, 171,
	172, 173, 174, 175, 176, 177, 178,
	179, 180, 181, 182, 183, 184, 185,
	186, 187, 188, 189, 190, 191, 192,
	193, 194, 195, 196, 197, 198, 199,
	201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214,
	215, 216, 217, 218, 219, 220, 221,
	223, 224, 225, 226, 227, 228, 229,
	230, 231, 232, 233, 234, 235, 236,
	237, 238, 239, 241, 242, 243, 244,
	245, 246, 247, 248, 249, 250, 251,
	252, 253, 255
};

static int dsi_j_1440_810_5_8_bl_notify(struct device *dev, int brightness)
{
	struct backlight_device *bl = NULL;
	struct pwm_bl_data *pb = NULL;
	int cur_sd_brightness = atomic_read(&sd_brightness);
	bl = (struct backlight_device *)dev_get_drvdata(dev);
	pb = (struct pwm_bl_data *)dev_get_drvdata(&bl->dev);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else if (pb->bl_measured)
		brightness = pb->bl_measured[brightness];

	return brightness;
}

static int dsi_j_1440_810_5_8_check_fb(struct device *dev, struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct platform_pwm_backlight_data dsi_j_1440_810_5_8_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 77,
	.pwm_period_ns	= 29334,
	.bl_measured    = dsi_j_1440_810_5_8_bl_response_curve,
	.notify		= dsi_j_1440_810_5_8_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= dsi_j_1440_810_5_8_check_fb,
};

static struct platform_device __maybe_unused dsi_j_1440_810_5_8_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &dsi_j_1440_810_5_8_bl_data,
	},
};

static struct platform_device __maybe_unused *loki_bl_device[] __initdata = {
	&dsi_j_1440_810_5_8_bl_device,
};

static struct tegra_dc_mode dsi_j_1440_810_5_8_modes[] = {
	{
		.pclk = 82833000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 48,
		.v_sync_width = 4,
		.h_back_porch = 48,
		.v_back_porch = 8,
		.h_active = 1440,
		.v_active = 810,
		.h_front_porch = 128,
		.v_front_porch = 8,
	},
};

static int dsi_j_1440_810_5_8_reg_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	avdd_lcd_3v0 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd_3v0)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0);
		avdd_lcd_3v0 = NULL;
		goto fail;
	}
	dvdd_lcd_3v3 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR(dvdd_lcd_3v3)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(dvdd_lcd_3v3);
		dvdd_lcd_3v3 = NULL;
		goto fail;
	}

	if (machine_is_dalmore()) {
		vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
		if (IS_ERR(vdd_lcd_bl)) {
			pr_err("vdd_lcd_bl regulator get failed\n");
			err = PTR_ERR(vdd_lcd_bl);
			vdd_lcd_bl = NULL;
			goto fail;
		}
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_j_1440_810_5_8_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(dsi_j_1440_810_5_8_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(DSI_PANEL_EN_GPIO, "panel en");
	if (err < 0) {
		pr_err("panel en gpio request failed\n");
		goto fail;
	}

	err = gpio_request(dsi_j_1440_810_5_8_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel backlight pwm gpio request failed\n");
		return err;
	}
	gpio_free(dsi_j_1440_810_5_8_pdata.dsi_panel_bl_pwm_gpio);

	gpio_requested = true;
	return 0;
fail:
	return err;
}

static struct tegra_dsi_cmd dsi_j_1440_810_5_8_init_cmd[] = {
	/* panel exit_sleep_mode sequence */
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_SEND_FRAME(5),
	DSI_DLY_MS(20),

	/* panel set_display_on sequence */
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_j_1440_810_5_8_suspend_cmd[] = {
	/* panel set_display_off sequence */
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, 0x0),

	/* panel enter_sleep_mode sequence*/
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, 0x0),
	DSI_DLY_MS(80),
};

static int dsi_j_1440_810_5_8_enable(struct device *dev)
{
	int err = 0;

	err = dsi_j_1440_810_5_8_reg_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}
	err = tegra_panel_gpio_get_dt("j,1440-810-5-8", &panel_of);
	if (err < 0) {
		/* try to request gpios from board file */
		err = dsi_j_1440_810_5_8_gpio_get();
		if (err < 0) {
			pr_err("dsi gpio request failed\n");
			goto fail;
		}
	}
	/* If panel rst gpio is specified in device tree,
	 * use that.
	 */
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];
	else
		en_panel_rst =
			dsi_j_1440_810_5_8_pdata.dsi_panel_rst_gpio;

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_PANEL_EN]))
		en_panel = panel_of.panel_gpio[TEGRA_GPIO_PANEL_EN];
	else
		en_panel = DSI_PANEL_EN_GPIO;

	if (!tegra_dc_initialized(dev)) {
		gpio_direction_output(en_panel_rst, 0);
		gpio_direction_output(en_panel, 0);
	}

	if (avdd_lcd_3v0) {
		err = regulator_enable(avdd_lcd_3v0);
		if (err < 0) {
			pr_err("avdd_lcd_3v0 regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0, 3100000, 3100000);
	}
	usleep_range(3000, 5000);

	if (dvdd_lcd_3v3) {
		err = regulator_enable(dvdd_lcd_3v3);
		if (err < 0) {
			pr_err("dvdd_lcd_3v3 regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);


	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	if (!tegra_dc_initialized(dev)) {
		gpio_direction_output(en_panel_rst, 1);
		msleep(20);
		gpio_set_value(en_panel, 1);
		msleep(20);
	}

	return 0;
fail:
	return err;
}

static int dsi_j_1440_810_5_8_postpoweron(struct device *dev)
{
	msleep(80);
	return 0;
}

static struct tegra_dsi_out dsi_j_1440_810_5_8_pdata = {
	.n_data_lanes = 4,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.dsi_init_cmd = dsi_j_1440_810_5_8_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_j_1440_810_5_8_init_cmd),
	.dsi_suspend_cmd = dsi_j_1440_810_5_8_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_j_1440_810_5_8_suspend_cmd),
	.ulpm_not_supported = true,
};

static int dsi_j_1440_810_5_8_disable(struct device *dev)
{
	/* Delay b/w DSI data/clk disable and panel reset */
	usleep_range(3000, 5000);
	gpio_direction_output(en_panel_rst, 0);
	usleep_range(3000, 5000);

	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (dvdd_lcd_3v3)
		regulator_disable(dvdd_lcd_3v3);

	if (avdd_lcd_3v0)
		regulator_disable(avdd_lcd_3v0);

	return 0;
}

static int dsi_j_1440_810_5_8_postsuspend(void)
{
	/* TODO */
	return 0;
}

static int dsi_j_1440_810_5_8_register_bl_dev(void)
{
	int err = 0;
	struct device_node *dc1_node = NULL;
	struct device_node *dc2_node = NULL;
	struct device_node *pwm_bl_node = NULL;

	find_dc_node(&dc1_node, &dc2_node);
	pwm_bl_node = of_find_compatible_node(NULL, NULL,
		"pwm-backlight");

	if (!of_have_populated_dt() || !dc1_node ||
		!of_device_is_available(dc1_node) ||
		!pwm_bl_node ||
		!of_device_is_available(pwm_bl_node)) {
		err = platform_device_register(&dsi_j_1440_810_5_8_bl_device);
		if (err) {
			pr_err("disp1 bl device registration failed");
			of_node_put(pwm_bl_node);
			return err;
		}
	}
	of_node_put(pwm_bl_node);
	return err;
}

static void dsi_j_1440_810_5_8_set_disp_device
	(struct platform_device *loki_display_device)
{
	disp_device = loki_display_device;
}

static void dsi_j_1440_810_5_8_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_j_1440_810_5_8_pdata;
	dc->modes = dsi_j_1440_810_5_8_modes;
	dc->n_modes = ARRAY_SIZE(dsi_j_1440_810_5_8_modes);
	dc->enable = dsi_j_1440_810_5_8_enable;
	dc->disable = dsi_j_1440_810_5_8_disable;
	dc->postsuspend = dsi_j_1440_810_5_8_postsuspend;
	dc->postpoweron = dsi_j_1440_810_5_8_postpoweron;
	dc->width = 130;
	dc->height = 74;
	dc->flags = DC_CTRL_MODE;
}

static void dsi_j_1440_810_5_8_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_j_1440_810_5_8_modes[0].h_active;
	fb->yres = dsi_j_1440_810_5_8_modes[0].v_active;
}

static void dsi_j_1440_810_5_8_sd_settings_init
	(struct tegra_dc_sd_settings *settings)
{
	*settings = dsi_j_1440_810_5_8_sd_settings;
	settings->bl_device_name = "pwm-backlight";
}

#ifdef CONFIG_TEGRA_DC_CMU
static void dsi_j_1440_810_5_8_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_j_1440_810_5_8_cmu;
}
#endif

static struct pwm_bl_data_dt_ops dsi_j_1440_810_5_8_pwm_bl_ops = {
	.notify = dsi_j_1440_810_5_8_bl_notify,
	.check_fb = dsi_j_1440_810_5_8_check_fb,
	.blnode_compatible = "j,1440-810-5-8-bl",
};

struct tegra_panel_ops dsi_j_1440_810_5_8_ops = {
	.enable = dsi_j_1440_810_5_8_enable,
	.disable = dsi_j_1440_810_5_8_disable,
	.postsuspend = dsi_j_1440_810_5_8_postsuspend,
	.postpoweron = dsi_j_1440_810_5_8_postpoweron,
	.pwm_bl_ops = &dsi_j_1440_810_5_8_pwm_bl_ops,
};

struct tegra_panel __initdata dsi_j_1440_810_5_8 = {
	.init_sd_settings = dsi_j_1440_810_5_8_sd_settings_init,
	.init_dc_out = dsi_j_1440_810_5_8_dc_out_init,
	.init_fb_data = dsi_j_1440_810_5_8_fb_data_init,
	.set_disp_device = dsi_j_1440_810_5_8_set_disp_device,
	.register_bl_dev = dsi_j_1440_810_5_8_register_bl_dev,
#ifdef CONFIG_TEGRA_DC_CMU
	.init_cmu_data = dsi_j_1440_810_5_8_cmu_init,
#endif
};
EXPORT_SYMBOL(dsi_j_1440_810_5_8);
