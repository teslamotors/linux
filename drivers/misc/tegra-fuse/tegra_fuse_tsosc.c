/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/tegra-fuse.h>

#include "fuse.h"

/*
 * SOC_THERM TSENSOR fuse patterns for these chip families:-
 *   TEGRA_CHIPID_TEGRA12, TEGRA_CHIPID_TEGRA13, TEGRA_CHIPID_TEGRA21
 */

#define FUSE_TSENSOR_COMMON		0x280
#define FUSE_SPARE_REALIGNMENT_REG_0	0x2fc

/*
 * T210: Layout of bits in FUSE_TSENSOR_COMMON:
 *    3                   2                   1                   0
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |       BASE_FT       |      BASE_CP      | SHFT_FT | SHIFT_CP  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * In chips prior to T210, this fuse was incorrectly sized as 26 bits,
 * and didn't hold SHIFT_CP in [31:26]. Therefore these missing six bits
 * were obtained via the FUSE_SPARE_REALIGNMENT_REG register [5:0].
 *
 * T12x, T13x, etc: FUSE_TSENSOR_COMMON:
 *    3                   2                   1                   0
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |-----------| SHFT_FT |       BASE_FT       |      BASE_CP      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * FUSE_SPARE_REALIGNMENT_REG:
 *    3                   2                   1                   0
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |---------------------------------------------------| SHIFT_CP  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#define FUSE_SHIFT_CP_POS	0
#define FUSE_SHIFT_CP_BITS	6
#define FUSE_SHIFT_CP_MASK	((1 << FUSE_SHIFT_CP_BITS) - 1)

#define FUSE_SHIFT_FT_POS	((chip_id == TEGRA_CHIPID_TEGRA21) ? 6 : 21)
#define FUSE_SHIFT_FT_BITS	5
#define FUSE_SHIFT_FT_MASK	((1 << FUSE_SHIFT_FT_BITS) - 1)

#define FUSE_BASE_CP_POS	((chip_id == TEGRA_CHIPID_TEGRA21) ? 11 : 0)
#define FUSE_BASE_CP_BITS	10
#define FUSE_BASE_CP_MASK	((1 << FUSE_BASE_CP_BITS) - 1)

#define FUSE_BASE_FT_POS	((chip_id == TEGRA_CHIPID_TEGRA21) ? 21 : 10)
#define FUSE_BASE_FT_BITS	11
#define FUSE_BASE_FT_MASK	((1 << FUSE_BASE_FT_BITS) - 1)

static enum tegra_chipid chip_id;
static int check_cp;
static int check_ft;
static int ret_base_cp;
static u32 _base_cp;
static s32 _shift_cp;
static int ret_base_ft;
static u32 _base_ft;
static s32 _shift_ft;

/* see Table 12 of T210_SOC_THERM IAS */
static int tsensor_calib_offset[] = {
	[0]	= 0x198,
	[1]	= 0x184,
	[2]	= 0x188,
	[3]	= 0x22c,
	[4]	= 0x254,
	[5]	= 0x258,
	[6]	= 0x25c,
	[7]	= 0x260,
	[8]	= 0x280, /* FUSE_TSENSOR_COMMON */
	[9]	= 0x2d4,
	[10]	= 0x31c,
	[11]	= 0x31c, /* FUSE_TSENSOR10_CALIB_AUX */
};

/*
 * Check CP fuse revision. Return value (depending on chip) is as below:
 *   Any: ERROR:      -ve:	Negative return value
 *
 *  T12x: CP/FT:	1:	Old style CP/FT fuse
 *        CP1/CP2:	0:	New style CP1/CP2 fuse (default)
 *
 *  T13x: Old pattern:	2:	Old ATE CP1/CP2 fuse (rev upto 0.8)
 *        Mid pattern:	1:	Mid ATE CP1/CP2 fuse (rev 0.9 - 0.11)
 *        New pattern:	0:	New ATE CP1/CP2 fuse (rev 0.12 onwards)
 *
 *  T21x: CP1/CP2:	2:	Same as 0 but with TSOSCs altered in A02
 *        CP1/CP2:	1:	Old ATE CP1/CP2 fuse (rev upto 00.2)
 *        CP1/CP2:	0:	New ATE CP1/CP2 fuse (rev 00.3 onwards)
 */
static int fuse_cp_rev_check(void)
{
	u32 rev, rev_major, rev_minor, chip_rev;

	rev = tegra_fuse_readl(FUSE_CP_REV);
	rev_minor = rev & 0x1f;
	rev_major = (rev >> 5) & 0x3f;
	pr_debug("%s: CP rev %d.%d\n", __func__, rev_major, rev_minor);

	if (!chip_id)
		chip_id = tegra_get_chipid();

	if (chip_id == TEGRA_CHIPID_TEGRA12) {
		/* CP rev < 00.4 is unsupported */
		if ((rev_major == 0) && (rev_minor < 4))
			return -EINVAL;
		/* CP rev < 00.8 is CP/FT (old style) */
		else if ((rev_major == 0) && (rev_minor < 8))
			return 1;
		/* default new CP1/CP2 fuse */
		else
			return 0;
	}

	if (chip_id == TEGRA_CHIPID_TEGRA13) {
		/* CP rev <= 00.8 is old ATE pattern */
		if ((rev_major == 0) && (rev_minor <= 8))
			return 2;
		/* CP 00.8 > rev >= 00.11 is mid ATE pattern */
		else if ((rev_major == 0) && (rev_minor <= 11))
			return 1;
		/* default new ATE pattern */
		else
			return 0;
	}

	if (chip_id == TEGRA_CHIPID_TEGRA21) {
		chip_rev = tegra_chip_get_revision();

		/* CP rev <= 00.2 is old ATE pattern */
		if ((rev_major == 0) && (rev_minor <= 2))
			rev = 1; /* should be -EINVAL to skip using TSOSC */
		else if (chip_rev < TEGRA_REVISION_A02)
			rev = 0;	/* consider A01, A01Q */
		else
			rev = 2;	/* chips A02+ */

		return rev;
	}

	return -EINVAL;
}

/*
 * Check FT fuse revision.
 *    return value: see comment under fuse_cp_rev_check_do()
 */
static int fuse_ft_rev_check(void)
{
	u32 rev, rev_major, rev_minor;

	if (!chip_id)
		chip_id = tegra_get_chipid();

	/* T13x and T21x do not use FT */
	if ((chip_id == TEGRA_CHIPID_TEGRA13) ||
	    (chip_id == TEGRA_CHIPID_TEGRA21))
		return 1;

	/* T12x */
	if (check_cp < 0)
		return check_cp;
	if (check_cp == 0)
		return -ENODEV; /* No FT rev in CP1/CP2 mode */

	rev = tegra_fuse_readl(FUSE_FT_REV);
	rev_minor = rev & 0x1f;
	rev_major = (rev >> 5) & 0x3f;
	pr_debug("%s: FT rev %d.%d\n", __func__, rev_major, rev_minor);

	/* FT rev < 00.5 is unsupported */
	if ((rev_major == 0) && (rev_minor < 5))
		return -EINVAL;

	return 0;
}

static int fuse_calib_base_get_cp_raw(u32 *base_cp, s32 *shifted_cp)
{
	s32 cp;
	u32 val;

	if (check_cp < 0)
		return check_cp;

	val = tegra_fuse_readl(FUSE_TSENSOR_COMMON);
	if (!val)
		return -EINVAL;

	*base_cp = (val >> FUSE_BASE_CP_POS) & FUSE_BASE_CP_MASK;
	if (*base_cp == 0) {
		pr_err("soctherm: ERROR: Improper calib_base CP fuse.\n");
		*base_cp = -EINVAL; /* cache the error value */
		return -EINVAL;
	}

	if ((chip_id == TEGRA_CHIPID_TEGRA12) ||
	    (chip_id == TEGRA_CHIPID_TEGRA13))
		val = tegra_fuse_readl(FUSE_SPARE_REALIGNMENT_REG_0);

	cp = (val >> FUSE_SHIFT_CP_POS) & FUSE_SHIFT_CP_MASK;
	*shifted_cp = ((s32)cp << (32 - FUSE_SHIFT_CP_BITS))
				>> (32 - FUSE_SHIFT_CP_BITS);

	return check_cp; /* return tri-state: 0, 1, or -ve */
}

static int fuse_calib_base_get_ft_raw(u32 *base_ft, s32 *shifted_ft)
{
	s32 ft;
	u32 val;

	if (check_cp < 0)
		return check_cp;
	/* when check_cp is 1, check_ft must be valid */
	if (check_cp != 0 && check_ft < 0)
		return -EINVAL;

	val = tegra_fuse_readl(FUSE_TSENSOR_COMMON);
	if (!val)
		return -EINVAL;

	*base_ft = (val >> FUSE_BASE_FT_POS) & FUSE_BASE_FT_MASK;
	if (*base_ft == 0) {
		pr_err("soctherm: ERROR: Improper calib_base FT fuse.\n");
		*base_ft = -EINVAL; /* cache the error value */
		return -EINVAL;
	}

	ft = (val >> FUSE_SHIFT_FT_POS) & FUSE_SHIFT_FT_MASK;
	*shifted_ft = ((s32)ft << (32 - FUSE_SHIFT_FT_BITS))
				>> (32 - FUSE_SHIFT_FT_BITS);

	return check_cp;
}

int tegra_fuse_get_tsensor_calib(int index, u32 *calib)
{
	u32 value;

	if (index < 0 || index >= ARRAY_SIZE(tsensor_calib_offset))
		return -EINVAL;
	value = tegra_fuse_readl(tsensor_calib_offset[index]);
	if (calib)
		*calib = value;
	return 0;
}

/*
 * Returns CP or CP1 fuse dep on CP/FT or CP1/CP2 style fusing
 *    return value: see comment under fuse_cp_rev_check()
 */
int tegra_fuse_calib_base_get_cp(u32 *base_cp, s32 *shifted_cp)
{
	if (base_cp)
		*base_cp = _base_cp;
	if (shifted_cp)
		*shifted_cp = _shift_cp;

	return ret_base_cp; /* return tri-state: 0, 1, or -ve */
}

/*
 * Returns FT or CP2 fuse dep on CP/FT or CP1/CP2 style fusing
 *    return value: see comment under fuse_cp_rev_check()
 */
int tegra_fuse_calib_base_get_ft(u32 *base_ft, s32 *shifted_ft)
{
	if (base_ft)
		*base_ft = _base_ft;
	if (shifted_ft)
		*shifted_ft = _shift_ft;

	return ret_base_ft; /* return tri-state: 0, 1, or -ve */
}

int tegra_fuse_tsosc_init(void)
{
	check_cp = fuse_cp_rev_check();
	check_ft = fuse_ft_rev_check();
	ret_base_cp = fuse_calib_base_get_cp_raw(&_base_cp, &_shift_cp);
	ret_base_ft = fuse_calib_base_get_ft_raw(&_base_ft, &_shift_ft);

	pr_debug("%s: BASE_CP %u  SHIFTED_CP %d\n", __func__,
		_base_cp, _shift_cp);

	pr_debug("%s: BASE_FT %u  SHIFTED_FT %d\n", __func__,
		_base_ft, _shift_ft);

	return check_cp;
}
