/*
 * tegra210_sfc_alt.h - Definitions for Tegra210 SFC driver
 *
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA210_SFC_ALT_H__
#define __TEGRA210_SFC_ALT_H__

/*
 * SFC_AXBAR_RX registers are with respect to AXBAR.
 * The data is coming from AXBAR to SFC for playback.
 */
#define TEGRA210_SFC_AXBAR_RX_STATUS			0x0c
#define TEGRA210_SFC_AXBAR_RX_INT_STATUS		0x10
#define TEGRA210_SFC_AXBAR_RX_INT_MASK			0x14
#define TEGRA210_SFC_AXBAR_RX_INT_SET			0x18
#define TEGRA210_SFC_AXBAR_RX_INT_CLEAR			0x1c
#define TEGRA210_SFC_AXBAR_RX_CIF_CTRL			0x20
#define TEGRA210_SFC_AXBAR_RX_FREQ				0x24
#define TEGRA210_SFC_AXBAR_RX_CYA				0x28
#define TEGRA210_SFC_AXBAR_RX_DBG				0x2c

/*
 * SFC_AXBAR_TX registers are with respect to AXBAR.
 * The data is going out of SFC for playback.
 */
#define TEGRA210_SFC_AXBAR_TX_STATUS			0x4c
#define TEGRA210_SFC_AXBAR_TX_INT_STATUS		0x50
#define TEGRA210_SFC_AXBAR_TX_INT_MASK			0x54
#define TEGRA210_SFC_AXBAR_TX_INT_SET			0x58
#define TEGRA210_SFC_AXBAR_TX_INT_CLEAR			0x5c
#define TEGRA210_SFC_AXBAR_TX_CIF_CTRL			0x60
#define TEGRA210_SFC_AXBAR_TX_FREQ				0x64
#define TEGRA210_SFC_AXBAR_TX_CYA				0x68
#define TEGRA210_SFC_AXBAR_TX_DBG				0x6c

/* Register offsets from TEGRA210_SFC*_BASE */
#define TEGRA210_SFC_ENABLE						0x80
#define TEGRA210_SFC_SOFT_RESET					0x84
#define TEGRA210_SFC_CG							0x88
#define TEGRA210_SFC_STATUS						0x8c
#define TEGRA210_SFC_INT_STATUS					0x90
#define TEGRA210_SFC_CYA						0x94
#define TEGRA210_SFC_DBG						0xac
#define TEGRA210_SFC_COEF_RAM					0xbc
#define TEGRA210_SFC_AHUBRAMCTL_SFC_CTRL		0xc0
#define TEGRA210_SFC_AHUBRAMCTL_SFC_DATA		0xc4

/* Fields in TEGRA210_SFC_ENABLE */
#define TEGRA210_SFC_EN_SHIFT				0
#define TEGRA210_SFC_EN						(1 << TEGRA210_SFC_EN_SHIFT)

#define TEGRA210_SFC_BITS_8					1
#define TEGRA210_SFC_BITS_12				2
#define TEGRA210_SFC_BITS_16				3
#define TEGRA210_SFC_BITS_20				4
#define TEGRA210_SFC_BITS_24				5
#define TEGRA210_SFC_BITS_28				6
#define TEGRA210_SFC_BITS_32				7
#define TEGRA210_SFC_FS8					0
#define TEGRA210_SFC_FS11_025				1
#define TEGRA210_SFC_FS16					2
#define TEGRA210_SFC_FS22_05				3
#define TEGRA210_SFC_FS24					4
#define TEGRA210_SFC_FS32					5
#define TEGRA210_SFC_FS44_1					6
#define TEGRA210_SFC_FS48					7
#define TEGRA210_SFC_FS64					8
#define TEGRA210_SFC_FS88_2					9
#define TEGRA210_SFC_FS96					10
#define TEGRA210_SFC_FS176_4				11
#define TEGRA210_SFC_FS192					12

/* Fields in TEGRA210_SFC_COEF_RAM */
#define TEGRA210_SFC_COEF_RAM_COEF_RAM_EN	BIT(0)

#define TEGRA210_SFC_SOFT_RESET_EN              BIT(0)

/* SRC coefficients */
#define TEGRA210_SFC_COEF_RAM_DEPTH		64

struct tegra210_sfc_soc_data {
	void (*set_audio_cif)(struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *conf);
};

struct tegra210_sfc {
	int srate_in;
	int srate_out;
	int format_in;
	int format_out;
	struct regmap *regmap;
	struct snd_pcm_hw_params in_hw_params;
	struct snd_pcm_hw_params out_hw_params;
	int stereo_conv_input;
	int mono_conv_output;
	const struct tegra210_sfc_soc_data *soc_data;
};

/* coeff RAM tables required for SFC */

static u32 coef_8to16[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00006102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002 /* output gain */
};

static u32 coef_8to24[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x0000a105, /* header */
	0x000005e1, /* input gain */
	0x00dca92f, 0xff45647a, 0x0046b59c,
	0x00429d1e, 0xff4fec62, 0x00516d30,
	0xffdea779, 0xff5e08ba, 0x0060185e,
	0xffafbab2, 0xff698d5a, 0x006ce3ae,
	0xff9a82d2, 0xff704674, 0x007633c5,
	0xff923433, 0xff721128, 0x007cff42,
	0x00000003 /* output gain */
};

static u32 coef_8to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x0156105, /* interpolation + IIR filter */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000002, /* ouptut gain */
	0x0021a102, /* interpolation + IIR filter */
	0x00000e00, /* input gain */
	0x00e2e000, 0xff6e1a00, 0x002aaa00,
	0x00610a00, 0xff5dda00, 0x003ccc00,
	0x00163a00, 0xff3c0400, 0x00633200,
	0x00000003, /* Output gain */
	0x00000204, /* Farrow filter */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000
};

static u32 coef_8to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00156105, /* interpolation + IIR Filter */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000002, /* ouptut gain */
	0x0000a102, /* interpolation + IIR filter */
	0x00000e00, /* input gain */
	0x00e2e000, 0xff6e1a00, 0x002aaa00,
	0x00610a00, 0xff5dda00, 0x003ccc00,
	0x00163a00, 0xff3c0400, 0x00633200,
	0x00000003 /* output gain */
};

static u32 coef_11to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d727,  /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00006102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002 /* output gain */
};

static u32 coef_11to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102,  /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00186102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002, /* output gain */
	0x00246102, /* header */
	0x0000010a, /* input gain */
	0x00c93dc4, 0xff26f5f6, 0x005d1041,
	0x001002c4, 0xff245b76, 0x00666002,
	0xffc30a45, 0xff1baecd, 0x00765921,
	0x00000002, /* output gain */
	0x00005204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000
};

static u32 coef_16to8[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_16to24[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x0015a105, /* header */
	0x00000292, /* input gain */
	0x00e4320a, 0xff41d2d9, 0x004911ac,
	0x005dd9e3, 0xff4c7d80, 0x0052103e,
	0xfff8ebef, 0xff5b6fab, 0x005f0a0d,
	0xffc4b414, 0xff68582c, 0x006b38e5,
	0xffabb861, 0xff704bec, 0x0074de52,
	0xffa19f4c, 0xff729059, 0x007c7e90,
	0x00000003, /* output gain */
	0x00005105, /* header */
	0x00000292, /* input gain */
	0x00e4320a, 0xff41d2d9, 0x004911ac,
	0x005dd9e3, 0xff4c7d80, 0x0052103e,
	0xfff8ebef, 0xff5b6fab, 0x005f0a0d,
	0xffc4b414, 0xff68582c, 0x006b38e5,
	0xffabb861, 0xff704bec, 0x0074de52,
	0xffa19f4c, 0xff729059, 0x007c7e90,
	0x00000001 /* output gain */
};

static u32 coef_16to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00156105, /* interpolation + IIR filter */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000002, /* output gain */
	0x0021a102, /* interpolation + IIR filter */
	0x00000e00, /* input gain */
	0x00e2e000, 0xff6e1a00, 0x002aaa00,
	0x00610a00, 0xff5dda00, 0x003ccc00,
	0x00163a00, 0xff3c0400, 0x00633200,
	0x00000003, /* output gain */
	0x002c0204, /* Farrow Filter */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005101, /* IIR Filter + Decimator */
	0x0000203c, /* input gain */
	0x00f52d35, 0xff2e2162, 0x005a21e0,
	0x00c6f0f0, 0xff2ecd69, 0x006fa78d,
	0x00000001 /* output gain */
};

static u32 coef_16to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x0000a105, /* interpolation + IIR Filter */
	0x00000784, /* input gain */
	0x00cc516e, 0xff2c9639, 0x005ad5b3,
	0x0013ad0d, 0xff3d4799, 0x0063ce75,
	0xffb6f398, 0xff5138d1, 0x006e9e1f,
	0xff9186e5, 0xff5f96a4, 0x0076a86e,
	0xff82089c, 0xff676b81, 0x007b9f8a,
	0xff7c48a5, 0xff6a31e7, 0x007ebb7b,
	0x00000003 /* output gain */
};

static u32 coef_22to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00006102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002 /* output gain */
};

static u32 coef_22to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00186102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002, /* output gain */
	0x00005204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000
};

static u32 coef_24to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00186102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002, /* output gain */
	0x00230204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x00001685, /* input gain */
	0x00f53ae9, 0xff52f196, 0x003e3e08,
	0x00b9f857, 0xff5d8985, 0x0050070a,
	0x008c3e86, 0xff6053f0, 0x006d98ef,
	0x00000001 /* output gain */
};

static u32 coef_24to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00006102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002 /* output gain */
};

static u32 coef_32to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x0018a102, /* header */
	0x000005d6, /* input gain */
	0x00c6543e, 0xff342935, 0x0052f116,
	0x000a1d78, 0xff3330c0, 0x005f88a3,
	0xffbee7c0, 0xff2b5ba5, 0x0073eb26,
	0x00000003, /* output gain */
	0x00235204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x0000015f, /* input gain */
	0x00a7909c, 0xff241c71, 0x005f5e00,
	0xffca77f4, 0xff20dd50, 0x006855eb,
	0xff86c552, 0xff18137a, 0x00773648,
	0x00000001 /* output gain */
};

static u32 coef_32to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x0015a105, /* header */
	0x00000292, /* input gain */
	0x00e4320a, 0xff41d2d9, 0x004911ac,
	0x005dd9e3, 0xff4c7d80, 0x0052103e,
	0xfff8ebef, 0xff5b6fab, 0x005f0a0d,
	0xffc4b414, 0xff68582c, 0x006b38e5,
	0xffabb861, 0xff704bec, 0x0074de52,
	0xffa19f4c, 0xff729059, 0x007c7e90,
	0x00000003, /* output gain */
	0x00005105, /* header */
	0x00000292, /* input gain */
	0x00e4320a, 0xff41d2d9, 0x004911ac,
	0x005dd9e3, 0xff4c7d80, 0x0052103e,
	0xfff8ebef, 0xff5b6fab, 0x005f0a0d,
	0xffc4b414, 0xff68582c, 0x006b38e5,
	0xffabb861, 0xff704bec, 0x0074de52,
	0xffa19f4c, 0xff729059, 0x007c7e90,
	0x00000001 /* output gain */
};

static u32 coef_44to8[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00120104, /* IIR Filter */
	0x00000af2, /* input gain */
	0x0057eebe, 0xff1e9863, 0x00652604,
	0xff7206ea, 0xff22ad7e, 0x006d47e1,
	0xff42a4d7, 0xff26e722, 0x0075fd83,
	0xff352f66, 0xff29312b, 0x007b986b,
	0xff310a07, 0xff296f51, 0x007eca7c,
	0x00000001, /* output gain */
	0x001d9204, /* Farrow Filter + decimation */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005105, /* IIR Filter + Decimator */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000001 /* output gain */
};

static u32 coef_44to16[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00126104, /* IIR Filter + interpolation */
	0x00000af2, /* input gain */
	0x0057eebe, 0xff1e9863, 0x00652604,
	0xff7206ea, 0xff22ad7e, 0x006d47e1,
	0xff42a4d7, 0xff26e722, 0x0075fd83,
	0xff352f66, 0xff29312b, 0x007b986b,
	0xff310a07, 0xff296f51, 0x007eca7c,
	0x00000002, /* output gain */
	0x001d9204, /* Farrow Filter + Decimation */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005105, /* IIR Filter + Decimator */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000001 /* output gain */
};

static u32 coef_48to24[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_44to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102,/* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00186102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002, /* output gain */
	0x00235204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x0001d029, /* input gain */
	0x00f2a98b, 0xff92aa71, 0x001fcd16,
	0x00ae9004, 0xffb85140, 0x0041813a,
	0x007f8ed1, 0xffd585fc, 0x006a69e6,
	0x00000001 /* output gain */
};

static u32 coef_48to8[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c9102, /* IIR Filter + Decimator */
	0x00000e00, /* input gain */
	0x00e2e000, 0xff6e1a00, 0x002aaa00,
	0x00610a00, 0xff5dda00, 0x003ccc00,
	0x00163a00, 0xff3c0400, 0x00633200,
	0x00000001, /* output gain */
	0x00005105, /* IIR Filter + Decimator */
	0x0000d649, /* input gain */
	0x00e87afb, 0xff5f69d0, 0x003df3cf,
	0x007ce488, 0xff99a5c8, 0x0056a6a0,
	0x00344928, 0xffcba3e5, 0x006be470,
	0x00137aa7, 0xffe60276, 0x00773410,
	0x0005fa2a, 0xfff1ac11, 0x007c795b,
	0x00012d36, 0xfff5eca2, 0x007f10ef,
	0x00000001 /* ouptut gain */
};

static u32 coef_48to16[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00009105, /* IIR Filter + Decimator */
	0x00000784, /* input gain */
	0x00cc516e, 0xff2c9639, 0x005ad5b3,
	0x0013ad0d, 0xff3d4799, 0x0063ce75,
	0xffb6f398, 0xff5138d1, 0x006e9e1f,
	0xff9186e5, 0xff5f96a4, 0x0076a86e,
	0xff82089c, 0xff676b81, 0x007b9f8a,
	0xff7c48a5, 0xff6a31e7, 0x007ebb7b,
	0x00000001 /* output gain */
};

static u32 coef_48to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d029, /* input gain */
	0x00f2a98b, 0xff92aa71, 0x001fcd16,
	0x00ae9004, 0xffb85140, 0x0041813a,
	0x007f8ed1, 0xffd585fc, 0x006a69e6,
	0x00000002, /* output gain */
	0x001b6103, /* header */
	0x000001e0, /* input gain */
	0x00de44c0, 0xff380b7f, 0x004ffc73,
	0x00494b44, 0xff3d493a, 0x005908bf,
	0xffe9a3c8, 0xff425647, 0x006745f7,
	0xffc42d61, 0xff40a6c7, 0x00776709,
	0x00000002, /* output gain */
	0x00265204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_48to96[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00006102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002 /* output gain */
};

static u32 coef_48to192[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000002, /* output gain */
	0x00006102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000002 /* output gain */
};

static u32 coef_88to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_88to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x00001685, /* input gain */
	0x00f53ae9, 0xff52f196, 0x003e3e08,
	0x00b9f857, 0xff5d8985, 0x0050070a,
	0x008c3e86, 0xff6053f0, 0x006d98ef,
	0x00000002, /* output gain */
	0x00175204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_96to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000f6103, /* header */
	0x000001e0, /* input gain */
	0x00de44c0, 0xff380b7f, 0x004ffc73,
	0x00494b44, 0xff3d493a, 0x005908bf,
	0xffe9a3c8, 0xff425647, 0x006745f7,
	0xffc42d61, 0xff40a6c7, 0x00776709,
	0x00000002, /* output gain */
	0x001a5204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_96to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_176to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c5102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000001, /* output gain */
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_176to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c0102, /* header */
	0x00001685, /* input gain */
	0x00f53ae9, 0xff52f196, 0x003e3e08,
	0x00b9f857, 0xff5d8985, 0x0050070a,
	0x008c3e86, 0xff6053f0, 0x006d98ef,
	0x00000001, /* output gain */
	0x00175204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00005102,/* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_192to44[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c6102, /* header */
	0x000000af, /* input gain */
	0x00c65663, 0xff23d2ce, 0x005f97d6,
	0x00086ad6, 0xff20ec4f, 0x00683201,
	0xffbbbef6, 0xff184447, 0x00770963,
	0x00000002, /* output gain */
	0x00175204, /* farrow */
	0x000aaaab,
	0xffaaaaab,
	0xfffaaaab,
	0x00555555,
	0xff600000,
	0xfff55555,
	0x00155555,
	0x00055555,
	0xffeaaaab,
	0x00200000,
	0x00235102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000001, /* output gain */
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

static u32 coef_192to48[TEGRA210_SFC_COEF_RAM_DEPTH] = {
	0x000c5102, /* header */
	0x000013d9, /* input gain */
	0x00ebd477, 0xff4ce383, 0x0042049d,
	0x0089c278, 0xff54414d, 0x00531ded,
	0x004a5e07, 0xff53cf41, 0x006efbdc,
	0x00000001, /* output gain */
	0x00005102, /* header */
	0x0001d727, /* input gain */
	0x00fc2fc7, 0xff9bb27b, 0x001c564c,
	0x00e55557, 0xffcadd5b, 0x003d80ba,
	0x00d13397, 0xfff232f8, 0x00683337,
	0x00000001 /* output gain */
};

#endif
