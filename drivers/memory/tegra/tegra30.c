/*
 * Copyright (C) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/mm.h>

#include <dt-bindings/memory/tegra30-mc.h>

#include "mc.h"

static const struct tegra_mc_client tegra30_mc_clients[] = {
	{
		.id = 0x00,
		.name = "ptcr",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_PTC,
		.smmu = {
			.reg = 0x228,
			.bit = 0,
		},
		.la = {
			.reg = 0x34c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x00,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x01,
		.name = "display0a",
		.fifo_size = 128,
		.swgroup = TEGRA_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 1,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x02,
		.name = "display0ab",
		.fifo_size = 128,
		.swgroup = TEGRA_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 2,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x03,
		.name = "display0b",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 3,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x04,
		.name = "display0bb",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 4,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x05,
		.name = "display0c",
		.fifo_size = 128,
		.swgroup = TEGRA_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 5,
		},
		.la = {
			.reg = 0x2ec,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x06,
		.name = "display0cb",
		.fifo_size = 128,
		.swgroup = TEGRA_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 6,
		},
		.la = {
			.reg = 0x2f8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x07,
		.name = "display1b",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 7,
		},
		.la = {
			.reg = 0x2ec,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x08,
		.name = "display1bb",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 8,
		},
		.la = {
			.reg = 0x2f8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x09,
		.name = "eppup",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_EPP,
		.smmu = {
			.reg = 0x228,
			.bit = 9,
		},
		.la = {
			.reg = 0x300,
			.shift = 0,
			.mask = 0xff,
			.def = 0x17,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x0a,
		.name = "g2pr",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 10,
		},
		.la = {
			.reg = 0x308,
			.shift = 0,
			.mask = 0xff,
			.def = 0x09,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x0b,
		.name = "g2sr",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 11,
		},
		.la = {
			.reg = 0x308,
			.shift = 16,
			.mask = 0xff,
			.def = 0x09,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x0c,
		.name = "mpeunifbr",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 12,
		},
		.la = {
			.reg = 0x328,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x0d,
		.name = "viruv",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_VI,
		.smmu = {
			.reg = 0x228,
			.bit = 13,
		},
		.la = {
			.reg = 0x364,
			.shift = 0,
			.mask = 0xff,
			.def = 0x2c,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x0e,
		.name = "afir",
		.fifo_size = 32,
		.swgroup = TEGRA_SWGROUP_AFI,
		.smmu = {
			.reg = 0x228,
			.bit = 14,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x0f,
		.name = "avpcarm7r",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_AVPC,
		.smmu = {
			.reg = 0x228,
			.bit = 15,
		},
		.la = {
			.reg = 0x2e4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x10,
		.name = "displayhc",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 16,
		},
		.la = {
			.reg = 0x2f0,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x11,
		.name = "displayhcb",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 17,
		},
		.la = {
			.reg = 0x2fc,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x12,
		.name = "fdcdrd",
		.fifo_size = 48,
		.fdc = true,
		.swgroup = TEGRA_SWGROUP_NV,
		.smmu = {
			.reg = 0x228,
			.bit = 18,
		},
		.la = {
			.reg = 0x334,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x13,
		.name = "fdcdrd2",
		.fifo_size = 48,
		.fdc = true,
		.swgroup = TEGRA_SWGROUP_NV2,
		.smmu = {
			.reg = 0x228,
			.bit = 19,
		},
		.la = {
			.reg = 0x33c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x14,
		.name = "g2dr",
		.fifo_size = 48,
		.swgroup = TEGRA_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 20,
		},
		.la = {
			.reg = 0x30c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x15,
		.name = "hdar",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_HDA,
		.smmu = {
			.reg = 0x228,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x16,
		.name = "host1xdmar",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_HC,
		.smmu = {
			.reg = 0x228,
			.bit = 22,
		},
		.la = {
			.reg = 0x310,
			.shift = 0,
			.mask = 0xff,
			.def = 0x05,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x17,
		.name = "host1xr",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_HC,
		.smmu = {
			.reg = 0x228,
			.bit = 23,
		},
		.la = {
			.reg = 0x310,
			.shift = 16,
			.mask = 0xff,
			.def = 0x50,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x18,
		.name = "idxsrd",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_NV,
		.smmu = {
			.reg = 0x228,
			.bit = 24,
		},
		.la = {
			.reg = 0x334,
			.shift = 16,
			.mask = 0xff,
			.def = 0x13,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x19,
		.name = "idxsrd2",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_NV2,
		.smmu = {
			.reg = 0x228,
			.bit = 25,
		},
		.la = {
			.reg = 0x33c,
			.shift = 16,
			.mask = 0xff,
			.def = 0x13,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1a,
		.name = "mpe_ipred",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 26,
		},
		.la = {
			.reg = 0x328,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1b,
		.name = "mpeamemrd",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 27,
		},
		.la = {
			.reg = 0x32c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x42,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1c,
		.name = "mpecsrd",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 28,
		},
		.la = {
			.reg = 0x32c,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1d,
		.name = "ppcsahbdmar",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 29,
		},
		.la = {
			.reg = 0x344,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1e,
		.name = "ppcsahbslvr",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 30,
		},
		.la = {
			.reg = 0x344,
			.shift = 16,
			.mask = 0xff,
			.def = 0x12,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x1f,
		.name = "satar",
		.fifo_size = 32,
		.swgroup = TEGRA_SWGROUP_SATA,
		.smmu = {
			.reg = 0x228,
			.bit = 31,
		},
		.la = {
			.reg = 0x350,
			.shift = 0,
			.mask = 0xff,
			.def = 0x33,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x20,
		.name = "texsrd",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_NV,
		.smmu = {
			.reg = 0x22c,
			.bit = 0,
		},
		.la = {
			.reg = 0x338,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x21,
		.name = "texsrd2",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_NV2,
		.smmu = {
			.reg = 0x22c,
			.bit = 1,
		},
		.la = {
			.reg = 0x340,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x22,
		.name = "vdebsevr",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 2,
		},
		.la = {
			.reg = 0x354,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x23,
		.name = "vdember",
		.fifo_size = 4,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 3,
		},
		.la = {
			.reg = 0x354,
			.shift = 16,
			.mask = 0xff,
			.def = 0xd0,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x24,
		.name = "vdemcer",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 4,
		},
		.la = {
			.reg = 0x358,
			.shift = 0,
			.mask = 0xff,
			.def = 0x2a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x25,
		.name = "vdetper",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 5,
		},
		.la = {
			.reg = 0x358,
			.shift = 16,
			.mask = 0xff,
			.def = 0x74,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x26,
		.name = "mpcorelpr",
		.fifo_size = 14,
		.swgroup = TEGRA_SWGROUP_MPCORELP,
		.la = {
			.reg = 0x324,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x27,
		.name = "mpcorer",
		.fifo_size = 14,
		.swgroup = TEGRA_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x28,
		.name = "eppu",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 8,
		},
		.la = {
			.reg = 0x300,
			.shift = 16,
			.mask = 0xff,
			.def = 0x6c,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x29,
		.name = "eppv",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 9,
		},
		.la = {
			.reg = 0x304,
			.shift = 0,
			.mask = 0xff,
			.def = 0x6c,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x2a,
		.name = "eppy",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 10,
		},
		.la = {
			.reg = 0x304,
			.shift = 16,
			.mask = 0xff,
			.def = 0x6c,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x2b,
		.name = "mpeunifbw",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x22c,
			.bit = 11,
		},
		.la = {
			.reg = 0x330,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x2c,
		.name = "viwsb",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 12,
		},
		.la = {
			.reg = 0x364,
			.shift = 16,
			.mask = 0xff,
			.def = 0x12,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x2d,
		.name = "viwu",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 13,
		},
		.la = {
			.reg = 0x368,
			.shift = 0,
			.mask = 0xff,
			.def = 0xb2,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x2e,
		.name = "viwv",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 14,
		},
		.la = {
			.reg = 0x368,
			.shift = 16,
			.mask = 0xff,
			.def = 0xb2,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x2f,
		.name = "viwy",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 15,
		},
		.la = {
			.reg = 0x36c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x12,
			.expiry_ns = 1050,
		},
	}, {
		.id = 0x30,
		.name = "g2dw",
		.fifo_size = 128,
		.swgroup = TEGRA_SWGROUP_G2,
		.smmu = {
			.reg = 0x22c,
			.bit = 16,
		},
		.la = {
			.reg = 0x30c,
			.shift = 16,
			.mask = 0xff,
			.def = 0x9,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x31,
		.name = "afiw",
		.fifo_size = 32,
		.swgroup = TEGRA_SWGROUP_AFI,
		.smmu = {
			.reg = 0x22c,
			.bit = 17,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0c,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x32,
		.name = "avpcarm7w",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_AVPC,
		.smmu = {
			.reg = 0x22c,
			.bit = 18,
		},
		.la = {
			.reg = 0x2e4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x33,
		.name = "fdcdwr",
		.fifo_size = 48,
		.fdc = true,
		.swgroup = TEGRA_SWGROUP_NV,
		.smmu = {
			.reg = 0x22c,
			.bit = 19,
		},
		.la = {
			.reg = 0x338,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x34,
		.name = "fdcdwr2",
		.fifo_size = 48,
		.fdc = true,
		.swgroup = TEGRA_SWGROUP_NV2,
		.smmu = {
			.reg = 0x22c,
			.bit = 20,
		},
		.la = {
			.reg = 0x340,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0a,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x35,
		.name = "hdaw",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_HDA,
		.smmu = {
			.reg = 0x22c,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x36,
		.name = "host1xw",
		.fifo_size = 32,
		.swgroup = TEGRA_SWGROUP_HC,
		.smmu = {
			.reg = 0x22c,
			.bit = 22,
		},
		.la = {
			.reg = 0x314,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x37,
		.name = "ispw",
		.fifo_size = 64,
		.swgroup = TEGRA_SWGROUP_ISP,
		.smmu = {
			.reg = 0x22c,
			.bit = 23,
		},
		.la = {
			.reg = 0x31c,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x38,
		.name = "mpcorelpw",
		.fifo_size = 24,
		.swgroup = TEGRA_SWGROUP_MPCORELP,
		.la = {
			.reg = 0x324,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x39,
		.name = "mpcorew",
		.fifo_size = 24,
		.swgroup = TEGRA_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3a,
		.name = "mpecswr",
		.fifo_size = 8,
		.swgroup = TEGRA_SWGROUP_MPE,
		.smmu = {
			.reg = 0x22c,
			.bit = 26,
		},
		.la = {
			.reg = 0x330,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3b,
		.name = "ppcsahbdmaw",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 27,
		},
		.la = {
			.reg = 0x348,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3c,
		.name = "ppcsahbslvw",
		.fifo_size = 4,
		.swgroup = TEGRA_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 28,
		},
		.la = {
			.reg = 0x348,
			.shift = 16,
			.mask = 0xff,
			.def = 0x06,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3d,
		.name = "sataw",
		.fifo_size = 32,
		.swgroup = TEGRA_SWGROUP_SATA,
		.smmu = {
			.reg = 0x22c,
			.bit = 29,
		},
		.la = {
			.reg = 0x350,
			.shift = 16,
			.mask = 0xff,
			.def = 0x33,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3e,
		.name = "vdebsevw",
		.fifo_size = 4,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 30,
		},
		.la = {
			.reg = 0x35c,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x3f,
		.name = "vdedbgw",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 31,
		},
		.la = {
			.reg = 0x35c,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x40,
		.name = "vdembew",
		.fifo_size = 2,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x230,
			.bit = 0,
		},
		.la = {
			.reg = 0x360,
			.shift = 0,
			.mask = 0xff,
			.def = 0x42,
			.expiry_ns = 150,
		},
	}, {
		.id = 0x41,
		.name = "vdetpmw",
		.fifo_size = 16,
		.swgroup = TEGRA_SWGROUP_VDE,
		.smmu = {
			.reg = 0x230,
			.bit = 1,
		},
		.la = {
			.reg = 0x360,
			.shift = 16,
			.mask = 0xff,
			.def = 0x2a,
		},
	},
};

static const struct tegra_smmu_swgroup tegra30_swgroups[] = {
	{ .name = "dc",   .swgroup = TEGRA_SWGROUP_DC,   .reg = 0x240 },
	{ .name = "dcb",  .swgroup = TEGRA_SWGROUP_DCB,  .reg = 0x244 },
	{ .name = "epp",  .swgroup = TEGRA_SWGROUP_EPP,  .reg = 0x248 },
	{ .name = "g2",   .swgroup = TEGRA_SWGROUP_G2,   .reg = 0x24c },
	{ .name = "mpe",  .swgroup = TEGRA_SWGROUP_MPE,  .reg = 0x264 },
	{ .name = "vi",   .swgroup = TEGRA_SWGROUP_VI,   .reg = 0x280 },
	{ .name = "afi",  .swgroup = TEGRA_SWGROUP_AFI,  .reg = 0x238 },
	{ .name = "avpc", .swgroup = TEGRA_SWGROUP_AVPC, .reg = 0x23c },
	{ .name = "nv",   .swgroup = TEGRA_SWGROUP_NV,   .reg = 0x268 },
	{ .name = "nv2",  .swgroup = TEGRA_SWGROUP_NV2,  .reg = 0x26c },
	{ .name = "hda",  .swgroup = TEGRA_SWGROUP_HDA,  .reg = 0x254 },
	{ .name = "hc",   .swgroup = TEGRA_SWGROUP_HC,   .reg = 0x250 },
	{ .name = "ppcs", .swgroup = TEGRA_SWGROUP_PPCS, .reg = 0x270 },
	{ .name = "sata", .swgroup = TEGRA_SWGROUP_SATA, .reg = 0x278 },
	{ .name = "vde",  .swgroup = TEGRA_SWGROUP_VDE,  .reg = 0x27c },
	{ .name = "isp",  .swgroup = TEGRA_SWGROUP_ISP,  .reg = 0x258 },
};

static const struct tegra_smmu_soc tegra30_smmu_soc = {
	.clients = tegra30_mc_clients,
	.num_clients = ARRAY_SIZE(tegra30_mc_clients),
	.swgroups = tegra30_swgroups,
	.num_swgroups = ARRAY_SIZE(tegra30_swgroups),
	.supports_round_robin_arbitration = false,
	.supports_request_limit = false,
	.num_tlb_lines = 16,
	.num_asids = 4,
};

const struct tegra_mc_soc tegra30_mc_soc = {
	.clients = tegra30_mc_clients,
	.num_clients = ARRAY_SIZE(tegra30_mc_clients),
	.num_address_bits = 32,
	.atom_size = 16,
	.atom_size_fdc = 32,
	.client_id_mask = 0x7f,
	.smmu = &tegra30_smmu_soc,
	.intmask = MC_INT_INVALID_SMMU_PAGE | MC_INT_SECURITY_VIOLATION |
		   MC_INT_DECERR_EMEM,
};
