/*
 * ar0261.c - ar0261 sensor driver
 *
 * Copyright (c) 2014, NVIDIA Corporation. All Rights Reserved.
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
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <media/ar0261.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/soc_camera.h>
#include <mach/io_dpd.h>

struct ar0261_reg {
	u16 addr;
	u16 val;
};

struct ar0261_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct ar0261_info {
	struct v4l2_subdev		subdev;
	const struct ar0261_datafmt	*fmt;

	struct ar0261_power_rail power;
	struct ar0261_sensordata sensor_data;
	struct i2c_client *i2c_client;
	struct clk *mclk;
	struct regmap *regmap;
	int mode;
};

static const struct ar0261_datafmt ar0261_colour_fmts[] = {
	{V4L2_MBUS_FMT_SRGGB10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SRGGB8_1X8, V4L2_COLORSPACE_SRGB},
};

static struct ar0261_info *to_ar0261(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct ar0261_info, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct ar0261_datafmt *ar0261_find_datafmt(
		enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ar0261_colour_fmts); i++)
		if (ar0261_colour_fmts[i].code == code)
			return ar0261_colour_fmts + i;

	return NULL;
}


#define AR0261_TABLE_WAIT_MS 0
#define AR0261_TABLE_END 1

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.cache_type = REGCACHE_RBTREE,
};

static struct ar0261_reg mode_1920x1080[] = {
	{0x301A, 0x0019},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x301A, 0x0218},
	{0x31B0, 0x0062},
	{0x31B2, 0x0046},
	{0x31B4, 0x3248},
	{0x31B6, 0x22A6},
	{0x31B8, 0x1832},
	{0x31BA, 0x1052},
	{0x31BC, 0x0408},
	{0x31AE, 0x0201},
	{AR0261_TABLE_WAIT_MS, 1},
	{0x3044, 0x0590},
	{0x3EE6, 0x60AD},
	{0x3EDC, 0xDBFA},
	{0x301A, 0x0218},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x3D00, 0x0481},
	{0x3D02, 0xFFFF},
	{0x3D04, 0xFFFF},
	{0x3D06, 0xFFFF},
	{0x3D08, 0x6600},
	{0x3D0A, 0x0311},
	{0x3D0C, 0x8C67},
	{0x3D0E, 0x0808},
	{0x3D10, 0x4380},
	{0x3D12, 0x4343},
	{0x3D14, 0x8043},
	{0x3D16, 0x4330},
	{0x3D18, 0x0543},
	{0x3D1A, 0x4381},
	{0x3D1C, 0x4C85},
	{0x3D1E, 0x2022},
	{0x3D20, 0x8020},
	{0x3D22, 0xA093},
	{0x3D24, 0x5A8A},
	{0x3D26, 0x4C81},
	{0x3D28, 0x5981},
	{0x3D2A, 0x1E00},
	{0x3D2C, 0x5F83},
	{0x3D2E, 0x5C80},
	{0x3D30, 0x5C81},
	{0x3D32, 0x5F58},
	{0x3D34, 0x6880},
	{0x3D36, 0x1060},
	{0x3D38, 0x8541},
	{0x3D3A, 0xB350},
	{0x3D3C, 0x5F10},
	{0x3D3E, 0x6050},
	{0x3D40, 0x5780},
	{0x3D42, 0x6880},
	{0x3D44, 0x2220},
	{0x3D46, 0x805D},
	{0x3D48, 0x8140},
	{0x3D4A, 0x864B},
	{0x3D4C, 0x8524},
	{0x3D4E, 0x08A0},
	{0x3D50, 0x55B8},
	{0x3D52, 0x429C},
	{0x3D54, 0x4281},
	{0x3D56, 0x4081},
	{0x3D58, 0x2808},
	{0x3D5A, 0x2810},
	{0x3D5C, 0x5727},
	{0x3D5E, 0x1069},
	{0x3D60, 0x4B52},
	{0x3D62, 0x8265},
	{0x3D64, 0x8A65},
	{0x3D66, 0xA95E},
	{0x3D68, 0x5080},
	{0x3D6A, 0x5250},
	{0x3D6C, 0x6080},
	{0x3D6E, 0x6922},
	{0x3D70, 0x2080},
	{0x3D72, 0x5D80},
	{0x3D74, 0x4080},
	{0x3D76, 0x5681},
	{0x3D78, 0x5781},
	{0x3D7A, 0x4B86},
	{0x3D7C, 0x2408},
	{0x3D7E, 0x9345},
	{0x3D80, 0x8144},
	{0x3D82, 0x4481},
	{0x3D84, 0x4586},
	{0x3D86, 0x4E80},
	{0x3D88, 0x4FCD},
	{0x3D8A, 0x4685},
	{0x3D8C, 0x0006},
	{0x3D8E, 0x8143},
	{0x3D90, 0x4380},
	{0x3D92, 0x4343},
	{0x3D94, 0x8043},
	{0x3D96, 0x4380},
	{0x3D98, 0x4343},
	{0x3D9A, 0x8043},
	{0x3D9C, 0x4380},
	{0x3D9E, 0x4343},
	{0x3DA0, 0x8648},
	{0x3DA2, 0x4880},
	{0x3DA4, 0x6B6B},
	{0x3DA6, 0x814C},
	{0x3DA8, 0x864D},
	{0x3DAA, 0xA442},
	{0x3DAC, 0x8641},
	{0x3DAE, 0x804D},
	{0x3DB0, 0x864C},
	{0x3DB2, 0x8A45},
	{0x3DB4, 0x8144},
	{0x3DB6, 0x4481},
	{0x3DB8, 0x4583},
	{0x3DBA, 0x46B7},
	{0x3DBC, 0x7386},
	{0x3DBE, 0x4685},
	{0x3DC0, 0x0006},
	{0x3DC2, 0x8143},
	{0x3DC4, 0x4380},
	{0x3DC6, 0x4343},
	{0x3DC8, 0x8043},
	{0x3DCA, 0x4380},
	{0x3DCC, 0x4343},
	{0x3DCE, 0x8043},
	{0x3DD0, 0x4380},
	{0x3DD2, 0x4343},
	{0x3DD4, 0x8648},
	{0x3DD6, 0x4880},
	{0x3DD8, 0x6A6A},
	{0x3DDA, 0x814C},
	{0x3DDC, 0x864D},
	{0x3DDE, 0xA442},
	{0x3DE0, 0x8641},
	{0x3DE2, 0x804D},
	{0x3DE4, 0x864C},
	{0x3DE6, 0x8A45},
	{0x3DE8, 0x8144},
	{0x3DEA, 0x4481},
	{0x3DEC, 0x4583},
	{0x3DEE, 0x4686},
	{0x3DF0, 0x73FF},
	{0x3DF2, 0xD358},
	{0x3DF4, 0x835B},
	{0x3DF6, 0x825A},
	{0x3DF8, 0x8153},
	{0x3DFA, 0x5467},
	{0x3DFC, 0x6363},
	{0x3DFE, 0x2640},
	{0x3E00, 0x6470},
	{0x3E02, 0xFFFF},
	{0x3E04, 0xFFFF},
	{0x3E06, 0xFFED},
	{0x3E08, 0x4580},
	{0x3E0A, 0x4384},
	{0x3E0C, 0x4380},
	{0x3E0E, 0x0280},
	{0x3E10, 0x8402},
	{0x3E12, 0x8080},
	{0x3E14, 0x6A84},
	{0x3E16, 0x6A80},
	{0x3E18, 0x4484},
	{0x3E1A, 0x4480},
	{0x3E1C, 0x4578},
	{0x3E1E, 0x8270},
	{0x3E20, 0x0000},
	{0x3E22, 0x0000},
	{0x3E24, 0x0000},
	{0x3E26, 0x0000},
	{0x3E28, 0x0000},
	{0x3E2A, 0x0000},
	{0x3E2C, 0x0000},
	{0x3E2E, 0x0000},
	{0x3E30, 0x0000},
	{0x3E32, 0x0000},
	{0x3E34, 0x0000},
	{0x3E36, 0x0000},
	{0x3E38, 0x0000},
	{0x3E3A, 0x0000},
	{0x3E3C, 0x0000},
	{0x3E3E, 0x0000},
	{0x3E40, 0x0000},
	{0x3E42, 0x0000},
	{0x3E44, 0x0000},
	{0x3E46, 0x0000},
	{0x3E48, 0x0000},
	{0x3E4A, 0x0000},
	{0x3E4C, 0x0000},
	{0x3E4E, 0x0000},
	{0x3E50, 0x0000},
	{0x3E52, 0x0000},
	{0x3E54, 0x0000},
	{0x3E56, 0x0000},
	{0x3E58, 0x0000},
	{0x3E5A, 0x0000},
	{0x3E5C, 0x0000},
	{0x3E5E, 0x0000},
	{0x3E60, 0x0000},
	{0x3E62, 0x0000},
	{0x3E64, 0x0000},
	{0x3E66, 0x0000},
	{0x3E68, 0x0000},
	{0x3E6A, 0x0000},
	{0x3E6C, 0x0000},
	{0x3E6E, 0x0000},
	{0x3E70, 0x0000},
	{0x3E72, 0x0000},
	{0x3E74, 0x0000},
	{0x3E76, 0x0000},
	{0x3E78, 0x0000},
	{0x3E7A, 0x0000},
	{0x3E7C, 0x0000},
	{0x3E7E, 0x0000},
	{0x3E80, 0x0000},
	{0x3E82, 0x0000},
	{0x3E84, 0x0000},
	{0x3E86, 0x0000},
	{0x3E88, 0x0000},
	{0x3E8A, 0x0000},
	{0x3E8C, 0x0000},
	{0x3E8E, 0x0000},
	{0x3E90, 0x0000},
	{0x3E92, 0x0000},
	{0x3E94, 0x0000},
	{0x3E96, 0x0000},
	{0x3E98, 0x0000},
	{0x3E9A, 0x0000},
	{0x3E9C, 0x0000},
	{0x3E9E, 0x0000},
	{0x3EA0, 0x0000},
	{0x3EA2, 0x0000},
	{0x3EA4, 0x0000},
	{0x3EA6, 0x0000},
	{0x3EA8, 0x0000},
	{0x3EAA, 0x0000},
	{0x3EAC, 0x0000},
	{0x3EAE, 0x0000},
	{0x3EB0, 0x0000},
	{0x3EB2, 0x0000},
	{0x3EB4, 0x0000},
	{0x3EB6, 0x0000},
	{0x3EB8, 0x0000},
	{0x3EBA, 0x0000},
	{0x3EBC, 0x0000},
	{0x3EBE, 0x0000},
	{0x3EC0, 0x0000},
	{0x3EC2, 0x0000},
	{0x3EC4, 0x0000},
	{0x3EC6, 0x0000},
	{0x3EC8, 0x0000},
	{0x3ECA, 0x0000},
	{0x301A, 0x021C},
	{0x0342, 0x10CC},
	{0x0340, 0x04A4},
	{0x0202, 0x0496},
	{0x0312, 0x045D},
	{0x31AE, 0x0201},
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, 0x0202},
	{0x0306, 0x4040},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0344, 0x0008},
	{0x0348, 0x0787},
	{0x0346, 0x0008},
	{0x034A, 0x043F},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x0041},
	{0x0104, 0x0001},
	{0x3ECC, 0x008F},
	{0x3ECE, 0xA8F0},
	{0x3ED0, 0xFFFF},
	{0x3ED6, 0x7193},
	{0x3ED8, 0x8A11},
	{0x30D2, 0x0020},
	{0x30D4, 0x0040},
	{0x3180, 0x80FF},
	{0x0104, 0x0000},
	{0x301A, 0x001C},
	{AR0261_TABLE_END, 0x0000}
};

enum {
	AR0261_MODE_1920X1080,
};


static struct ar0261_reg tp_colorbar[] = {
	{0x301A, 0x0019},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x301A, 0x0218},
	{0x31B0, 0x0062},
	{0x31B2, 0x0046},
	{0x31B4, 0x3248},
	{0x31B6, 0x22A6},
	{0x31B8, 0x1832},
	{0x31BA, 0x1052},
	{0x31BC, 0x0408},
	{0x31AE, 0x0201},
	{AR0261_TABLE_WAIT_MS, 1},
	{0x3044, 0x0590},
	{0x3EE6, 0x60AD},
	{0x3EDC, 0xDBFA},
	{0x301A, 0x0218},
	{AR0261_TABLE_WAIT_MS, 10},
	{0x3D00, 0x0481},
	{0x3D02, 0xFFFF},
	{0x3D04, 0xFFFF},
	{0x3D06, 0xFFFF},
	{0x3D08, 0x6600},
	{0x3D0A, 0x0311},
	{0x3D0C, 0x8C67},
	{0x3D0E, 0x0808},
	{0x3D10, 0x4380},
	{0x3D12, 0x4343},
	{0x3D14, 0x8043},
	{0x3D16, 0x4330},
	{0x3D18, 0x0543},
	{0x3D1A, 0x4381},
	{0x3D1C, 0x4C85},
	{0x3D1E, 0x2022},
	{0x3D20, 0x8020},
	{0x3D22, 0xA093},
	{0x3D24, 0x5A8A},
	{0x3D26, 0x4C81},
	{0x3D28, 0x5981},
	{0x3D2A, 0x1E00},
	{0x3D2C, 0x5F83},
	{0x3D2E, 0x5C80},
	{0x3D30, 0x5C81},
	{0x3D32, 0x5F58},
	{0x3D34, 0x6880},
	{0x3D36, 0x1060},
	{0x3D38, 0x8541},
	{0x3D3A, 0xB350},
	{0x3D3C, 0x5F10},
	{0x3D3E, 0x6050},
	{0x3D40, 0x5780},
	{0x3D42, 0x6880},
	{0x3D44, 0x2220},
	{0x3D46, 0x805D},
	{0x3D48, 0x8140},
	{0x3D4A, 0x864B},
	{0x3D4C, 0x8524},
	{0x3D4E, 0x08A0},
	{0x3D50, 0x55B8},
	{0x3D52, 0x429C},
	{0x3D54, 0x4281},
	{0x3D56, 0x4081},
	{0x3D58, 0x2808},
	{0x3D5A, 0x2810},
	{0x3D5C, 0x5727},
	{0x3D5E, 0x1069},
	{0x3D60, 0x4B52},
	{0x3D62, 0x8265},
	{0x3D64, 0x8A65},
	{0x3D66, 0xA95E},
	{0x3D68, 0x5080},
	{0x3D6A, 0x5250},
	{0x3D6C, 0x6080},
	{0x3D6E, 0x6922},
	{0x3D70, 0x2080},
	{0x3D72, 0x5D80},
	{0x3D74, 0x4080},
	{0x3D76, 0x5681},
	{0x3D78, 0x5781},
	{0x3D7A, 0x4B86},
	{0x3D7C, 0x2408},
	{0x3D7E, 0x9345},
	{0x3D80, 0x8144},
	{0x3D82, 0x4481},
	{0x3D84, 0x4586},
	{0x3D86, 0x4E80},
	{0x3D88, 0x4FCD},
	{0x3D8A, 0x4685},
	{0x3D8C, 0x0006},
	{0x3D8E, 0x8143},
	{0x3D90, 0x4380},
	{0x3D92, 0x4343},
	{0x3D94, 0x8043},
	{0x3D96, 0x4380},
	{0x3D98, 0x4343},
	{0x3D9A, 0x8043},
	{0x3D9C, 0x4380},
	{0x3D9E, 0x4343},
	{0x3DA0, 0x8648},
	{0x3DA2, 0x4880},
	{0x3DA4, 0x6B6B},
	{0x3DA6, 0x814C},
	{0x3DA8, 0x864D},
	{0x3DAA, 0xA442},
	{0x3DAC, 0x8641},
	{0x3DAE, 0x804D},
	{0x3DB0, 0x864C},
	{0x3DB2, 0x8A45},
	{0x3DB4, 0x8144},
	{0x3DB6, 0x4481},
	{0x3DB8, 0x4583},
	{0x3DBA, 0x46B7},
	{0x3DBC, 0x7386},
	{0x3DBE, 0x4685},
	{0x3DC0, 0x0006},
	{0x3DC2, 0x8143},
	{0x3DC4, 0x4380},
	{0x3DC6, 0x4343},
	{0x3DC8, 0x8043},
	{0x3DCA, 0x4380},
	{0x3DCC, 0x4343},
	{0x3DCE, 0x8043},
	{0x3DD0, 0x4380},
	{0x3DD2, 0x4343},
	{0x3DD4, 0x8648},
	{0x3DD6, 0x4880},
	{0x3DD8, 0x6A6A},
	{0x3DDA, 0x814C},
	{0x3DDC, 0x864D},
	{0x3DDE, 0xA442},
	{0x3DE0, 0x8641},
	{0x3DE2, 0x804D},
	{0x3DE4, 0x864C},
	{0x3DE6, 0x8A45},
	{0x3DE8, 0x8144},
	{0x3DEA, 0x4481},
	{0x3DEC, 0x4583},
	{0x3DEE, 0x4686},
	{0x3DF0, 0x73FF},
	{0x3DF2, 0xD358},
	{0x3DF4, 0x835B},
	{0x3DF6, 0x825A},
	{0x3DF8, 0x8153},
	{0x3DFA, 0x5467},
	{0x3DFC, 0x6363},
	{0x3DFE, 0x2640},
	{0x3E00, 0x6470},
	{0x3E02, 0xFFFF},
	{0x3E04, 0xFFFF},
	{0x3E06, 0xFFED},
	{0x3E08, 0x4580},
	{0x3E0A, 0x4384},
	{0x3E0C, 0x4380},
	{0x3E0E, 0x0280},
	{0x3E10, 0x8402},
	{0x3E12, 0x8080},
	{0x3E14, 0x6A84},
	{0x3E16, 0x6A80},
	{0x3E18, 0x4484},
	{0x3E1A, 0x4480},
	{0x3E1C, 0x4578},
	{0x3E1E, 0x8270},
	{0x3E20, 0x0000},
	{0x3E22, 0x0000},
	{0x3E24, 0x0000},
	{0x3E26, 0x0000},
	{0x3E28, 0x0000},
	{0x3E2A, 0x0000},
	{0x3E2C, 0x0000},
	{0x3E2E, 0x0000},
	{0x3E30, 0x0000},
	{0x3E32, 0x0000},
	{0x3E34, 0x0000},
	{0x3E36, 0x0000},
	{0x3E38, 0x0000},
	{0x3E3A, 0x0000},
	{0x3E3C, 0x0000},
	{0x3E3E, 0x0000},
	{0x3E40, 0x0000},
	{0x3E42, 0x0000},
	{0x3E44, 0x0000},
	{0x3E46, 0x0000},
	{0x3E48, 0x0000},
	{0x3E4A, 0x0000},
	{0x3E4C, 0x0000},
	{0x3E4E, 0x0000},
	{0x3E50, 0x0000},
	{0x3E52, 0x0000},
	{0x3E54, 0x0000},
	{0x3E56, 0x0000},
	{0x3E58, 0x0000},
	{0x3E5A, 0x0000},
	{0x3E5C, 0x0000},
	{0x3E5E, 0x0000},
	{0x3E60, 0x0000},
	{0x3E62, 0x0000},
	{0x3E64, 0x0000},
	{0x3E66, 0x0000},
	{0x3E68, 0x0000},
	{0x3E6A, 0x0000},
	{0x3E6C, 0x0000},
	{0x3E6E, 0x0000},
	{0x3E70, 0x0000},
	{0x3E72, 0x0000},
	{0x3E74, 0x0000},
	{0x3E76, 0x0000},
	{0x3E78, 0x0000},
	{0x3E7A, 0x0000},
	{0x3E7C, 0x0000},
	{0x3E7E, 0x0000},
	{0x3E80, 0x0000},
	{0x3E82, 0x0000},
	{0x3E84, 0x0000},
	{0x3E86, 0x0000},
	{0x3E88, 0x0000},
	{0x3E8A, 0x0000},
	{0x3E8C, 0x0000},
	{0x3E8E, 0x0000},
	{0x3E90, 0x0000},
	{0x3E92, 0x0000},
	{0x3E94, 0x0000},
	{0x3E96, 0x0000},
	{0x3E98, 0x0000},
	{0x3E9A, 0x0000},
	{0x3E9C, 0x0000},
	{0x3E9E, 0x0000},
	{0x3EA0, 0x0000},
	{0x3EA2, 0x0000},
	{0x3EA4, 0x0000},
	{0x3EA6, 0x0000},
	{0x3EA8, 0x0000},
	{0x3EAA, 0x0000},
	{0x3EAC, 0x0000},
	{0x3EAE, 0x0000},
	{0x3EB0, 0x0000},
	{0x3EB2, 0x0000},
	{0x3EB4, 0x0000},
	{0x3EB6, 0x0000},
	{0x3EB8, 0x0000},
	{0x3EBA, 0x0000},
	{0x3EBC, 0x0000},
	{0x3EBE, 0x0000},
	{0x3EC0, 0x0000},
	{0x3EC2, 0x0000},
	{0x3EC4, 0x0000},
	{0x3EC6, 0x0000},
	{0x3EC8, 0x0000},
	{0x3ECA, 0x0000},
	{0x301A, 0x021C},
	{0x0342, 0x10CC},
	{0x0340, 0x04A4},
	{0x0202, 0x0496},
	{0x0312, 0x045D},
	{0x31AE, 0x0201},
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, 0x0202},
	{0x0306, 0x4040},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x0344, 0x0008},
	{0x0348, 0x0787},
	{0x0346, 0x0008},
	{0x034A, 0x043F},
	{0x034C, 0x0780},
	{0x034E, 0x0438},
	{0x3040, 0x0041},
	{0x0104, 0x0001},
	{0x3ECC, 0x008F},
	{0x3ECE, 0xA8F0},
	{0x3ED0, 0xFFFF},
	{0x3ED6, 0x7193},
	{0x3ED8, 0x8A11},
	{0x30D2, 0x0020},
	{0x30D4, 0x0040},
	{0x3180, 0x80FF},
	{0x0104, 0x0000},
	{0x3044, 0x0000},
	{0x30CA, 0x0001},
	{0x30D4, 0x0000},
	{0x31E0, 0x0000},
	{0x301A, 0x0000},
	{0x301E, 0x0000},
	{0x3070, 0x0002},
	{0x301A, 0x001C},
	{AR0261_TABLE_END, 0x0000}
};

static struct ar0261_reg *mode_table[] = {
	[AR0261_MODE_1920X1080] = mode_1920x1080,
};


static int test_mode;
module_param(test_mode, int, 0644);

static const struct v4l2_frmsize_discrete ar0261_frmsizes[] = {
	{1920, 1080},
};

#define AR0261_MODE	AR0261_MODE_1920X1080
#define AR0261_WIDTH	1920
#define AR0261_HEIGHT	1080

static int ar0261_find_mode(u32 width, u32 height)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ar0261_frmsizes); i++) {
		if (width == ar0261_frmsizes[i].width &&
		    height == ar0261_frmsizes[i].height)
			return i;
	}

	pr_err("ar0261: %dx%d is not supported\n", width, height);
	return AR0261_MODE_1920X1080;
}
static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000+500);
}

static int ar0261_read_reg(struct ar0261_info *info, u16 addr, u16 *val)
{
	int err;
	unsigned char data[2];

	err = regmap_raw_read(info->regmap, addr, data, sizeof(data));
	if (!err)
		*val = (u16)data[0] << 8 | data[1];

	return err;
}

static inline int ar0261_read_reg8(struct ar0261_info *info, u16 reg, u8 *val)
{
	int err;
	u32 _val;

	err = regmap_read(info->regmap, reg, &_val);
	if (!err)
		*val = _val & 0xFF;

	return err;
}

static int ar0261_write_reg(struct ar0261_info *info, u16 addr, u16 val)
{
	int err;
	unsigned char data[2];

	data[0] = (u8) (val >> 8);
	data[1] = (u8) (val & 0xff);
	err = regmap_raw_write(info->regmap, addr, data, sizeof(data));
	if (err)
		dev_err(&info->i2c_client->dev,
			"%s:i2c write failed, %x = %x\n", __func__, addr, val);

	return err;
}

static inline int ar0261_write_reg8(struct ar0261_info *info, u16 reg, u8 val)
{
	return regmap_write(info->regmap, reg, val);
}

static int
ar0261_write_table(struct ar0261_info *info,
			 const struct ar0261_reg table[])
{
	const struct ar0261_reg *next;
	int err = 0;
	u16 val;

	for (next = table; next->addr != AR0261_TABLE_END; next++) {

		if (next->addr == AR0261_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		err = ar0261_write_reg(info, next->addr, val);
		if (err)
			break;
	}

	return err;
}

static void ar0261_mclk_disable(struct ar0261_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ar0261_mclk_enable(struct ar0261_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
			__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static struct tegra_io_dpd csie_io = {
	.name			= "CSIE",
	.io_dpd_reg_index	= 1,
	.io_dpd_bit		= 12,
};

#define CAM_RSTN 219 /* TEGRA_GPIO_PBB3 */
#define CAM_AF_PWDN 223 /* TEGRA_GPIO_PBB7 */
#define CAM2_PWDN 222 /* TEGRA_GPIO_PBB6 */

static int ar0261_power_on(struct ar0261_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	/* disable CSIE IOs DPD mode to turn on front camera for ardbeg */
	tegra_io_dpd_disable(&csie_io);

	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM_AF_PWDN, 1);

	err = regulator_enable(pw->vcmvdd);
	if (unlikely(err))
		goto ar0261_vcm_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto ar0261_dvdd_fail;

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0261_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0261_iovdd_fail;

	usleep_range(1, 2);
	gpio_set_value(CAM2_PWDN, 1);

	gpio_set_value(CAM_RSTN, 1);

	return 0;

ar0261_iovdd_fail:
	regulator_disable(pw->dvdd);

ar0261_dvdd_fail:
	regulator_disable(pw->avdd);

ar0261_avdd_fail:
	regulator_disable(pw->vcmvdd);

ar0261_vcm_fail:
	/* put CSIE IOs into DPD mode to save additional power for ardbeg */
	tegra_io_dpd_enable(&csie_io);
	pr_info("%s: failed!\n", __func__);

	return -ENODEV;
}

static int ar0261_power_off(struct ar0261_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd ||
					!pw->vcmvdd))) {
		/* put CSIE IOs into DPD mode to
		 * save additional power for ardbeg
		 */
		tegra_io_dpd_enable(&csie_io);
		return -EFAULT;
	}

	gpio_set_value(CAM_RSTN, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);
	regulator_disable(pw->vcmvdd);
	/* put CSIE IOs into DPD mode to save additional power for ardbeg */
	tegra_io_dpd_enable(&csie_io);
	return 0;
}


static int ar0261_get_sensor_id(struct ar0261_info *info)
{
	u16 chip_version = 0;
	int retry_num = 10, ret = 0;

	ar0261_mclk_enable(info);
	ar0261_power_on(&info->power);

	/*
	 * We have to poll reading until read out something,
	 * ignore those I2C failures
	 */
	while (!chip_version && --retry_num > 0)
		ar0261_read_reg(info, 0x0, &chip_version);

	pr_info("[ar0261]: chip version 0x%04x\n", chip_version);
	if (chip_version != 0x0052) {
		pr_err("[ar0261]: failed to read sensor id\n");
		ret = -ENODEV;
	}

	ar0261_mclk_disable(info);
	ar0261_power_off(&info->power);

	return ret;
}

static int ar0261_power_put(struct ar0261_power_rail *pw)
{
	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->vcmvdd))
		regulator_put(pw->vcmvdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->vcmvdd = NULL;

	return 0;
}

static int ar0261_regulator_get(struct ar0261_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int ar0261_power_get(struct ar0261_info *info)
{
	struct ar0261_power_rail *pw = &info->power;

	ar0261_regulator_get(info, &pw->dvdd, "vdig"); /* digital 1.2v */
	ar0261_regulator_get(info, &pw->avdd, "vana"); /* analog 2.7v */
	ar0261_regulator_get(info, &pw->iovdd, "vif"); /* interface 1.8v */
	ar0261_regulator_get(info, &pw->vcmvdd, "avdd_af1_cam");

	return 0;
}

static int ar0261_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar0261_info *info = to_ar0261(client);
	int mode = ar0261_find_mode(mf->width, mf->height);

	mf->width = ar0261_frmsizes[mode].width;
	mf->height = ar0261_frmsizes[mode].height;

	if (mf->code != V4L2_MBUS_FMT_SRGGB8_1X8 &&
	    mf->code != V4L2_MBUS_FMT_SRGGB10_1X10)
		mf->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	info->mode = mode;

	return 0;
}

static int ar0261_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar0261_info *info = to_ar0261(client);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	/* MIPI CSI could have changed the format, double-check */
	if (!ar0261_find_datafmt(mf->code))
		return -EINVAL;

	ar0261_try_fmt(sd, mf);

	info->fmt = ar0261_find_datafmt(mf->code);

	return 0;
}

static int ar0261_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar0261_info *info = to_ar0261(client);

	const struct ar0261_datafmt *fmt = info->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= AR0261_WIDTH;
	mf->height	= AR0261_HEIGHT;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ar0261_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct v4l2_rect *rect = &a->c;

	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rect->top	= 0;
	rect->left	= 0;
	rect->width	= AR0261_WIDTH;
	rect->height	= AR0261_HEIGHT;

	return 0;
}

static int ar0261_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= AR0261_WIDTH;
	a->bounds.height		= AR0261_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int ar0261_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if ((unsigned int)index >= ARRAY_SIZE(ar0261_colour_fmts))
		return -EINVAL;

	*code = ar0261_colour_fmts[index].code;
	return 0;
}

static int ar0261_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar0261_info *info = to_ar0261(client);

	if (!enable)
		return 0;

	if (test_mode)
		return ar0261_write_table(info, tp_colorbar);

	return ar0261_write_table(info, mode_table[info->mode]);
}


static int ar0261_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ar0261_info *info = to_ar0261(client);
	int err;

	if (on) {
		err = ar0261_mclk_enable(info);
		if (!err)
			err = ar0261_power_on(&info->power);
		if (err < 0)
			ar0261_mclk_disable(info);
		return err;
	} else if (!on) {
		ar0261_power_off(&info->power);
		ar0261_mclk_disable(info);
		return 0;
	} else
		return -EINVAL;
}

static int ar0261_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK;

	return 0;
}

static struct v4l2_subdev_video_ops ar0261_subdev_video_ops = {
	.s_stream	= ar0261_s_stream,
	.s_mbus_fmt	= ar0261_s_fmt,
	.g_mbus_fmt	= ar0261_g_fmt,
	.try_mbus_fmt	= ar0261_try_fmt,
	.enum_mbus_fmt	= ar0261_enum_fmt,
	.g_crop		= ar0261_g_crop,
	.cropcap	= ar0261_cropcap,
	.g_mbus_config	= ar0261_g_mbus_config,
};

static struct v4l2_subdev_core_ops ar0261_subdev_core_ops = {
	.s_power	= ar0261_s_power,
};

static struct v4l2_subdev_ops ar0261_subdev_ops = {
	.core	= &ar0261_subdev_core_ops,
	.video	= &ar0261_subdev_video_ops,
};


static int
ar0261_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ar0261_info *info;
	int ret;

	pr_info("[ar0261]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
		sizeof(struct ar0261_info), GFP_KERNEL);
	if (!info) {
		pr_err("[ar0261]:%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}
	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	info->i2c_client = client;
	info->mode = 0;
	info->fmt = &ar0261_colour_fmts[0];

	info->mclk = devm_clk_get(&client->dev, "mclk2");
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock mclk2\n",
			__func__);
		return PTR_ERR(info->mclk);
	}

	i2c_set_clientdata(client, info);

	ar0261_power_get(info);
	ret = ar0261_get_sensor_id(info);
	if (ret < 0) {
		pr_err("[ar0261]: fail to read out sensor ID.\n");
		return ret;
	}

	v4l2_i2c_subdev_init(&info->subdev, client, &ar0261_subdev_ops);

	pr_info("[ar0261]: probing sensor is done.\n");

	return 0;
}

static int
ar0261_remove(struct i2c_client *client)
{
	struct soc_camera_subdev_desc *ssdd;
	struct ar0261_info *info;

	ssdd = soc_camera_i2c_to_desc(client);
	if (ssdd->free_bus)
		ssdd->free_bus(ssdd);

	info = i2c_get_clientdata(client);
	ar0261_power_put(&info->power);
	return 0;
}

static const struct i2c_device_id ar0261_id[] = {
	{ "ar0261_v4l2", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ar0261_id);

static struct i2c_driver ar0261_i2c_driver = {
	.driver = {
		.name = "ar0261_v4l2",
		.owner = THIS_MODULE,
	},
	.probe = ar0261_probe,
	.remove = ar0261_remove,
	.id_table = ar0261_id,
};

static int __init
ar0261_init(void)
{
	pr_info("[ar0261]: sensor driver loading\n");
	return i2c_add_driver(&ar0261_i2c_driver);
}

static void __exit
ar0261_exit(void)
{
	i2c_del_driver(&ar0261_i2c_driver);
}

module_init(ar0261_init);
module_exit(ar0261_exit);

MODULE_DESCRIPTION("SoC Camera driver for Aptina AR0261");
MODULE_AUTHOR("Bryan Wu <pengw@nvidia.com>");
MODULE_LICENSE("GPL v2");
