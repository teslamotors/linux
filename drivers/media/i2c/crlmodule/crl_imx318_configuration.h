/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation
 *
 * Author: Jouni Ukkonen <jouni.ukkonen@intel.com>
 *
 */
#ifndef __CRLMODULE_IMX318_CONFIGURATION_H_
#define __CRLMODULE_IMX318_CONFIGURATION_H_

#include "crlmodule-nvm.h"
#include "crlmodule-sensor-ds.h"

static const struct crl_register_write_rep imx318_pll_1164mbps[] = {
	{ 0x0136, CRL_REG_LEN_08BIT, 0x18 }, /* 24 Mhz */
	{ 0x0137, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0111, CRL_REG_LEN_08BIT, 0x02 }, /* 2 = DPHY, 3 = CPHY */
	{ 0x0112, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0113, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0114, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x0309, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x030B, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x030E, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030F, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x0820, CRL_REG_LEN_08BIT, 0x12 },
	{ 0x0821, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x0822, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0823, CRL_REG_LEN_08BIT, 0x00 },
};

static const struct crl_register_write_rep imx318_pll_8_1164mbps[] = {
	{ 0x0136, CRL_REG_LEN_08BIT, 0x18 }, /* 24 Mhz */
	{ 0x0137, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0111, CRL_REG_LEN_08BIT, 0x02 }, /* 2 = DPHY, 3 = CPHY */
	{ 0x0112, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0113, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0114, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x0309, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x030B, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x030E, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030F, CRL_REG_LEN_08BIT, 0x84 },
	{ 0x0820, CRL_REG_LEN_08BIT, 0x12 },
	{ 0x0821, CRL_REG_LEN_08BIT, 0x30 },
	{ 0x0822, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0823, CRL_REG_LEN_08BIT, 0x00 },
};

static const struct crl_register_write_rep imx318_pll_1920mbps[] = {
	{ 0x0136, CRL_REG_LEN_08BIT, 0x18 }, /* 24 Mhz */
	{ 0x0137, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0111, CRL_REG_LEN_08BIT, 0x02 }, /* 2 = DPHY, 3 = CPHY */
	{ 0x0112, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0113, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0114, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x0309, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x030B, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x030E, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030F, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x0820, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x0821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0822, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0823, CRL_REG_LEN_08BIT, 0x00 },
};

static const struct crl_register_write_rep imx318_pll_8_1920mbps[] = {
	{ 0x0136, CRL_REG_LEN_08BIT, 0x18 }, /* 24 Mhz */
	{ 0x0137, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0111, CRL_REG_LEN_08BIT, 0x02 }, /* 2 = DPHY, 3 = CPHY */
	{ 0x0112, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0113, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x0114, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x0301, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x0303, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0305, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0306, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0307, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x0309, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x030B, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030D, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x030E, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x030F, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x0820, CRL_REG_LEN_08BIT, 0x1E },
	{ 0x0821, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0822, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0823, CRL_REG_LEN_08BIT, 0x00 },
};


static const struct crl_register_write_rep imx318_powerup_regset[] = {
	{ 0x3067, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4600, CRL_REG_LEN_08BIT, 0x1B },
	{ 0x46C2, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4877, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x487B, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x487F, CRL_REG_LEN_08BIT, 0x3B },
	{ 0x4883, CRL_REG_LEN_08BIT, 0xB4 },
	{ 0x4C6F, CRL_REG_LEN_08BIT, 0x5E },
	{ 0x5113, CRL_REG_LEN_08BIT, 0xF4 },
	{ 0x5115, CRL_REG_LEN_08BIT, 0xF6 },
	{ 0x5125, CRL_REG_LEN_08BIT, 0xF4 },
	{ 0x5127, CRL_REG_LEN_08BIT, 0xF8 },
	{ 0x51CF, CRL_REG_LEN_08BIT, 0xF4 },
	{ 0x51E9, CRL_REG_LEN_08BIT, 0xF4 },
	{ 0x5483, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x5485, CRL_REG_LEN_08BIT, 0x7C },
	{ 0x5495, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x5497, CRL_REG_LEN_08BIT, 0x7F },
	{ 0x5515, CRL_REG_LEN_08BIT, 0xC3 },
	{ 0x5517, CRL_REG_LEN_08BIT, 0xC7 },
	{ 0x552B, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x5535, CRL_REG_LEN_08BIT, 0x7A },
	{ 0x5A35, CRL_REG_LEN_08BIT, 0x1B },
	{ 0x5C13, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x5D89, CRL_REG_LEN_08BIT, 0xB1 },
	{ 0x5D8B, CRL_REG_LEN_08BIT, 0x2C },
	{ 0x5D8D, CRL_REG_LEN_08BIT, 0x61 },
	{ 0x5D8F, CRL_REG_LEN_08BIT, 0xE1 },
	{ 0x5D91, CRL_REG_LEN_08BIT, 0x4D },
	{ 0x5D93, CRL_REG_LEN_08BIT, 0xB4 },
	{ 0x5D95, CRL_REG_LEN_08BIT, 0x41 },
	{ 0x5D97, CRL_REG_LEN_08BIT, 0x96 },
	{ 0x5D99, CRL_REG_LEN_08BIT, 0x37 },
	{ 0x5D9B, CRL_REG_LEN_08BIT, 0x81 },
	{ 0x5D9D, CRL_REG_LEN_08BIT, 0x31 },
	{ 0x5D9F, CRL_REG_LEN_08BIT, 0x71 },
	{ 0x5DA1, CRL_REG_LEN_08BIT, 0x2B },
	{ 0x5DA3, CRL_REG_LEN_08BIT, 0x64 },
	{ 0x5DA5, CRL_REG_LEN_08BIT, 0x27 },
	{ 0x5DA7, CRL_REG_LEN_08BIT, 0x5A },
	{ 0x6009, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x613A, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x613C, CRL_REG_LEN_08BIT, 0x23 },
	{ 0x6142, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x6143, CRL_REG_LEN_08BIT, 0x62 },
	{ 0x6144, CRL_REG_LEN_08BIT, 0x89 },
	{ 0x6145, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x6146, CRL_REG_LEN_08BIT, 0x24 },
	{ 0x6147, CRL_REG_LEN_08BIT, 0x28 },
	{ 0x6148, CRL_REG_LEN_08BIT, 0x90 },
	{ 0x6149, CRL_REG_LEN_08BIT, 0xA2 },
	{ 0x614A, CRL_REG_LEN_08BIT, 0x40 },
	{ 0x614B, CRL_REG_LEN_08BIT, 0x8A },
	{ 0x614C, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x614D, CRL_REG_LEN_08BIT, 0x12 },
	{ 0x614E, CRL_REG_LEN_08BIT, 0x2C },
	{ 0x614F, CRL_REG_LEN_08BIT, 0x98 },
	{ 0x6150, CRL_REG_LEN_08BIT, 0xA2 },
	{ 0x615D, CRL_REG_LEN_08BIT, 0x37 },
	{ 0x615E, CRL_REG_LEN_08BIT, 0xE6 },
	{ 0x615F, CRL_REG_LEN_08BIT, 0x4B },
	{ 0x616C, CRL_REG_LEN_08BIT, 0x41 },
	{ 0x616D, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x616E, CRL_REG_LEN_08BIT, 0x48 },
	{ 0x616F, CRL_REG_LEN_08BIT, 0xC5 },
	{ 0x6174, CRL_REG_LEN_08BIT, 0xB9 },
	{ 0x6175, CRL_REG_LEN_08BIT, 0x42 },
	{ 0x6176, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x6177, CRL_REG_LEN_08BIT, 0xC3 },
	{ 0x6178, CRL_REG_LEN_08BIT, 0x81 },
	{ 0x6179, CRL_REG_LEN_08BIT, 0x78 },
	{ 0x6182, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x6A5F, CRL_REG_LEN_08BIT, 0x03 },
	{ 0x9302, CRL_REG_LEN_08BIT, 0xFF },
};

static const struct crl_register_write_rep imx318_mode_full[] = {
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x6F },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x034B, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0220, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0221, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0222, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0900, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0901, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0902, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0x65 },
	{ 0x3011, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x30FC, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30FD, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3194, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x31A0, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4711, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6669, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x0408, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0409, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040C, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x040D, CRL_REG_LEN_08BIT, 0x70 },
	{ 0x040E, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x040F, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x70 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0x10 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3033, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3035, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3037, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3039, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x303B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306E, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x306F, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x6636, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6637, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3066, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7B63, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x56FB, CRL_REG_LEN_08BIT, 0x50 },
	{ 0x56FF, CRL_REG_LEN_08BIT, 0x50 },
	{ 0x9323, CRL_REG_LEN_08BIT, 0x10 },
};


static const struct crl_register_write_rep imx318_mode_uhd[] = {
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x6F },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x034B, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0220, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0221, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0222, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0900, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0901, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0902, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0x65 },
	{ 0x3011, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x30FC, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30FD, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3194, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A0, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4711, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6669, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x16 },
	{ 0x0408, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0409, CRL_REG_LEN_08BIT, 0x68 },
	{ 0x040A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040B, CRL_REG_LEN_08BIT, 0x3A },
	{ 0x040C, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x040D, CRL_REG_LEN_08BIT, 0xA0 },
	{ 0x040E, CRL_REG_LEN_08BIT, 0x0B },
	{ 0x040F, CRL_REG_LEN_08BIT, 0x9C },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x08 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0x70 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3033, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3035, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3037, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3039, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x303B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306E, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x306F, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x6636, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6637, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3066, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7B63, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x56FB, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x56FF, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x9323, CRL_REG_LEN_08BIT, 0x16 },
};

static const struct crl_register_write_rep imx318_mode_1080[] = {
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x6F },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x034B, CRL_REG_LEN_08BIT, 0x0F },
	{ 0x0220, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0221, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0222, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0900, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0901, CRL_REG_LEN_08BIT, 0x22 },
	{ 0x0902, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0x65 },
	{ 0x3011, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x30FC, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30FD, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3194, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A0, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4711, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6669, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x16 },
	{ 0x0408, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0409, CRL_REG_LEN_08BIT, 0x34 },
	{ 0x040A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040B, CRL_REG_LEN_08BIT, 0x1C },
	{ 0x040C, CRL_REG_LEN_08BIT, 0x0A },
	{ 0x040D, CRL_REG_LEN_08BIT, 0x50 },
	{ 0x040E, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x040F, CRL_REG_LEN_08BIT, 0xCE },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x07 },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x80 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0x38 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3033, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3035, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3037, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3039, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x303B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306E, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x306F, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x6636, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6637, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3066, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7B63, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x56FB, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x56FF, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x9323, CRL_REG_LEN_08BIT, 0x16 },
};

static const struct crl_register_write_rep imx318_mode_720[] = {
	{ 0x0344, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0345, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0346, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0347, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x0348, CRL_REG_LEN_08BIT, 0x15 },
	{ 0x0349, CRL_REG_LEN_08BIT, 0x6F },
	{ 0x034A, CRL_REG_LEN_08BIT, 0x0E },
	{ 0x034B, CRL_REG_LEN_08BIT, 0x13 },
	{ 0x0220, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0221, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0222, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0381, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0383, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0385, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0387, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0900, CRL_REG_LEN_08BIT, 0x01 },
	{ 0x0901, CRL_REG_LEN_08BIT, 0x44 },
	{ 0x0902, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x3010, CRL_REG_LEN_08BIT, 0x65 },
	{ 0x3011, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x30FC, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x30FD, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3194, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A0, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x31A1, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x4711, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6669, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0401, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x0404, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0405, CRL_REG_LEN_08BIT, 0x11 },
	{ 0x0408, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x0409, CRL_REG_LEN_08BIT, 0x04 },
	{ 0x040A, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x040B, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x040C, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x040D, CRL_REG_LEN_08BIT, 0x52 },
	{ 0x040E, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x040F, CRL_REG_LEN_08BIT, 0xFE },
	{ 0x034C, CRL_REG_LEN_08BIT, 0x05 },
	{ 0x034D, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x034E, CRL_REG_LEN_08BIT, 0x02 },
	{ 0x034F, CRL_REG_LEN_08BIT, 0xD0 },
	{ 0x3031, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3033, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3035, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3037, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x3039, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x303B, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306C, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x306E, CRL_REG_LEN_08BIT, 0x0D },
	{ 0x306F, CRL_REG_LEN_08BIT, 0x56 },
	{ 0x6636, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x6637, CRL_REG_LEN_08BIT, 0x14 },
	{ 0x3066, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x7B63, CRL_REG_LEN_08BIT, 0x00 },
	{ 0x56FB, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x56FF, CRL_REG_LEN_08BIT, 0x33 },
	{ 0x9323, CRL_REG_LEN_08BIT, 0x16 },
};

static struct crl_register_write_rep imx318_streamon_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x01 }
};

static struct crl_register_write_rep imx318_streamoff_regs[] = {
	{ 0x0100, CRL_REG_LEN_08BIT, 0x00 }
};

static struct crl_register_write_rep imx318_data_fmt_width10[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0a0a }
};

static struct crl_register_write_rep imx318_data_fmt_width8[] = {
	{ 0x0112, CRL_REG_LEN_16BIT, 0x0808 }
};

static struct crl_arithmetic_ops imx318_vflip_ops[] = {
	{
		.op = CRL_BITWISE_LSHIFT,
		.operand.entity_val = 1,
	},
};

static struct crl_dynamic_register_access imx318_h_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = 0,
		.ops = 0,
		.mask = 0x1,
	},
};

static struct crl_dynamic_register_access imx318_v_flip_regs[] = {
	{
		.address = 0x0101,
		.len = CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE,
		.ops_items = ARRAY_SIZE(imx318_vflip_ops),
		.ops = imx318_vflip_ops,
		.mask = 0x2,
	},
};

static struct crl_dynamic_register_access imx318_ana_gain_global_regs[] = {
	{
		.address = 0x0204,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access imx318_exposure_regs[] = {
	{
		.address = 0x0202,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	}
};

static struct crl_dynamic_register_access imx318_fll_regs[] = {
	{
		.address = 0x0340,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_dynamic_register_access imx318_llp_regs[] = {
	{
		.address = 0x0342,
		.len = CRL_REG_LEN_16BIT,
		.ops_items = 0,
		.ops = 0,
		.mask = 0xffff,
	},
};

static struct crl_sensor_detect_config imx318_sensor_detect_regset[] = {
	{
		.reg = { 0x0019, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 5,
	},
	{
		.reg = { 0x0018, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 5,
	},
	{
		.reg = { 0x0016, CRL_REG_LEN_16BIT, 0x0000ffff },
		.width = 7,
	},
};

static struct crl_pll_configuration imx318_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 582000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 465600000,
		.pixel_rate_pa = 799206000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx318_pll_1164mbps),
		.pll_regs = imx318_pll_1164mbps,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 582000000,
		.bitsperpixel = 8,
		.pixel_rate_csi = 465600000,
		.pixel_rate_pa = 799206000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx318_pll_8_1164mbps),
		.pll_regs = imx318_pll_8_1164mbps,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 960000000,
		.bitsperpixel = 10,
		.pixel_rate_csi = 768000000,
		.pixel_rate_pa = 799206000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx318_pll_1920mbps),
		.pll_regs = imx318_pll_1920mbps,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 960000000,
		.bitsperpixel = 8,
		.pixel_rate_csi = 960000000,
		.pixel_rate_pa = 799206000,
		.csi_lanes = 4,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = ARRAY_SIZE(imx318_pll_8_1920mbps),
		.pll_regs = imx318_pll_8_1920mbps,
	},

};

static struct crl_subdev_rect_rep imx318_full_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 0, 5488, 4112 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 0, 5488, 4112 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 0, 5488, 4112 },
	},
};


static struct crl_subdev_rect_rep imx318_uhd_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 512, 5280, 3088 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 5280, 3088 },
		.out_rect = { 0, 0, 5280, 3088 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 5280, 3088 },
		.out_rect = { 0, 0, 3840, 2160 },
	},
};


static struct crl_subdev_rect_rep imx318_1080_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 512, 5488, 3088 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 5488, 3088 },
		.out_rect = { 0, 0, 2744, 1544 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 2744, 1544 },
		.out_rect = { 0, 0, 1920, 1080 },
	},
};

static struct crl_subdev_rect_rep imx318_720_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect = { 0, 0, 5488, 4112 },
		.out_rect = { 0, 516, 5488, 3088 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect = { 0, 0, 5488, 3088 },
		.out_rect = { 0, 0, 1372, 772 },
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.in_rect = { 0, 0, 1372, 772 },
		.out_rect = { 0, 0, 1280, 720 },
	},
};

static struct crl_mode_rep imx318_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(imx318_full_rects),
		.sd_rects = imx318_full_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 5488,
		.height = 4112,
		.min_llp = 6224,
		.min_fll = 4280,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx318_mode_full),
		.mode_regs = imx318_mode_full,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx318_uhd_rects),
		.sd_rects = imx318_uhd_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 22,
		.width = 3840,
		.height = 2160,
		.min_llp = 6224,
		.min_fll = 3622,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx318_mode_uhd),
		.mode_regs = imx318_mode_uhd,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx318_1080_rects),
		.sd_rects = imx318_1080_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 22,
		.width = 1920,
		.height = 1080,
		.min_llp = 6224,
		.min_fll = 1600,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx318_mode_1080),
		.mode_regs = imx318_mode_1080,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx318_720_rects),
		.sd_rects = imx318_720_rects,
		.binn_hor = 4,
		.binn_vert = 4,
		.scale_m = 17,
		.width = 1280,
		.height = 720,
		.min_llp = 6224,
		.min_fll = 904,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx318_mode_720),
		.mode_regs = imx318_mode_720,
	},
};

static struct crl_sensor_subdev_config imx318_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_SCALER,
		.name = "imx318 scaler",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "imx318 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "imx318 pixel array",
	},
};

static struct crl_sensor_limits imx318_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 5488,
	.y_addr_max = 4112,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 6224, /*TBD*/
	.max_line_length_pixels = 32752,
	.scaler_m_min = 16,
	.scaler_m_max = 255,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 3,
};

static struct crl_flip_data imx318_flip_configurations[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	},
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	},
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	},
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	},
};

static struct crl_csi_data_fmt imx318_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 10,
		.regs_items = 1,
		.regs = imx318_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx318_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx318_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 10,
		.regs = imx318_data_fmt_width10,
	},
	{
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx318_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx318_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx318_data_fmt_width8,
	},
	{
		.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
		.regs_items = 1,
		.bits_per_pixel = 8,
		.regs = imx318_data_fmt_width8,
	},
};

static const s64 imx318_op_sys_clock[] =  { 582000000,
						582000000,
						960000000,
						960000000, };

static struct crl_v4l2_ctrl imx318_vl42_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.data.v4l2_int_menu.def = 0,
		.data.v4l2_int_menu.max =
			ARRAY_SIZE(imx318_pll_configurations) - 1,
		.data.v4l2_int_menu.menu = imx318_op_sys_clock,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_PA",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_SCALER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = INT_MAX,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_ANALOGUE_GAIN,
		.name = "V4L2_CID_ANALOGUE_GAIN",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 480,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx318_ana_gain_global_regs),
		.regs = imx318_ana_gain_global_regs,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_EXPOSURE,
		.name = "V4L2_CID_EXPOSURE",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 65500,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx318_exposure_regs),
		.regs = imx318_exposure_regs,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_HFLIP,
		.name = "V4L2_CID_HFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.ctrl = 0,
		.regs_items = ARRAY_SIZE(imx318_h_flip_regs),
		.regs = imx318_h_flip_regs,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_VFLIP,
		.name = "V4L2_CID_VFLIP",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 1,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx318_v_flip_regs),
		.regs = imx318_v_flip_regs,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_FRAME_LENGTH_LINES,
		.name = "Frame length lines",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 160,
		.data.std_data.max = 65535,
		.data.std_data.step = 1,
		.data.std_data.def = 4130,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx318_fll_regs),
		.regs = imx318_fll_regs,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_LINE_LENGTH_PIXELS,
		.name = "Line Length Pixels",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 6024,
		.data.std_data.max = 65520,
		.data.std_data.step = 1,
		.data.std_data.def = 6024,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = ARRAY_SIZE(imx318_llp_regs),
		.regs = imx318_llp_regs,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity imx318_power_items[] = {
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VANA",
		.val = 2800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VDIG",
		.val = 1050000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VIO",
		.val = 1800000,
		.delay = 0,
	},
	{
		.type = CRL_POWER_ETY_REGULATOR_FRAMEWORK,
		.ent_name = "VAF",
		.val = 3000000,
		.delay = 2000,
	},
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
		.delay = 10700,
	},
};


struct crl_sensor_configuration imx318_crl_configuration = {

	.power_items = ARRAY_SIZE(imx318_power_items),
	.power_entities = imx318_power_items,

	.powerup_regs_items = ARRAY_SIZE(imx318_powerup_regset),
	.powerup_regs = imx318_powerup_regset,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(imx318_sensor_detect_regset),
	.id_regs = imx318_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(imx318_sensor_subdevs),
	.subdevs = imx318_sensor_subdevs,

	.sensor_limits = &imx318_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx318_pll_configurations),
	.pll_configs = imx318_pll_configurations,

	.modes_items = ARRAY_SIZE(imx318_modes),
	.modes = imx318_modes,
	.fail_safe_mode_index = 0,

	.streamon_regs_items = ARRAY_SIZE(imx318_streamon_regs),
	.streamon_regs = imx318_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx318_streamoff_regs),
	.streamoff_regs = imx318_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx318_vl42_ctrls),
	.v4l2_ctrl_bank = imx318_vl42_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx318_crl_csi_data_fmt),
	.csi_fmts = imx318_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx318_flip_configurations),
	.flip_data = imx318_flip_configurations,

};

#endif  /* __CRLMODULE_imx318_CONFIGURATION_H_ */
