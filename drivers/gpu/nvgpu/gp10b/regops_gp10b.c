/*
 * Tegra GK20A GPU Debugger Driver Register Ops
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>
#include <linux/err.h>
#include <linux/bsearch.h>
#include <uapi/linux/nvgpu.h>

#include "gk20a/gk20a.h"
#include "gk20a/dbg_gpu_gk20a.h"
#include "gk20a/regops_gk20a.h"
#include "regops_gp10b.h"

static const struct regop_offset_range gp10b_global_whitelist_ranges[] = {
	{ 0x000004f0,   1},
	{ 0x00001a00,   3},
	{ 0x00002800, 128},
	{ 0x00009400,   1},
	{ 0x00009410,   1},
	{ 0x00009480,   1},
	{ 0x00020200,  24},
	{ 0x00021c00,   4},
	{ 0x00021c14,   3},
	{ 0x00021c24,   1},
	{ 0x00021c2c,  69},
	{ 0x00021d44,   1},
	{ 0x00021d4c,   1},
	{ 0x00021d54,   1},
	{ 0x00021d5c,   1},
	{ 0x00021d64,   2},
	{ 0x00021d70,  16},
	{ 0x00022430,   7},
	{ 0x00022450,   1},
	{ 0x0002245c,   1},
	{ 0x00070000,   5},
	{ 0x000884e0,   1},
	{ 0x0008e00c,   1},
	{ 0x00100c18,   3},
	{ 0x00100c84,   1},
	{ 0x00104038,   1},
	{ 0x0010a0a8,   1},
	{ 0x0010a4f0,   1},
	{ 0x0010e490,   1},
	{ 0x0013cc14,   1},
	{ 0x00140028,   1},
	{ 0x00140280,   1},
	{ 0x001402a0,   1},
	{ 0x00140350,   1},
	{ 0x00140480,   1},
	{ 0x001404a0,   1},
	{ 0x00140550,   1},
	{ 0x00142028,   1},
	{ 0x00142280,   1},
	{ 0x001422a0,   1},
	{ 0x00142350,   1},
	{ 0x00142480,   1},
	{ 0x001424a0,   1},
	{ 0x00142550,   1},
	{ 0x0017e028,   1},
	{ 0x0017e280,   1},
	{ 0x0017e294,   1},
	{ 0x0017e29c,   2},
	{ 0x0017e2ac,   1},
	{ 0x0017e350,   1},
	{ 0x0017e39c,   1},
	{ 0x0017e480,   1},
	{ 0x0017e4a0,   1},
	{ 0x0017e550,   1},
	{ 0x00180040,  41},
	{ 0x001800ec,  10},
	{ 0x00180240,  41},
	{ 0x001802ec,  10},
	{ 0x00180440,  41},
	{ 0x001804ec,  10},
	{ 0x00180640,  41},
	{ 0x001806ec,  10},
	{ 0x00180840,  41},
	{ 0x001808ec,  10},
	{ 0x00180a40,  41},
	{ 0x00180aec,  10},
	{ 0x00180c40,  41},
	{ 0x00180cec,  10},
	{ 0x00180e40,  41},
	{ 0x00180eec,  10},
	{ 0x001a0040,  41},
	{ 0x001a00ec,  10},
	{ 0x001a0240,  41},
	{ 0x001a02ec,  10},
	{ 0x001a0440,  41},
	{ 0x001a04ec,  10},
	{ 0x001a0640,  41},
	{ 0x001a06ec,  10},
	{ 0x001a0840,  41},
	{ 0x001a08ec,  10},
	{ 0x001a0a40,  41},
	{ 0x001a0aec,  10},
	{ 0x001a0c40,  41},
	{ 0x001a0cec,  10},
	{ 0x001a0e40,  41},
	{ 0x001a0eec,  10},
	{ 0x001b0040,  41},
	{ 0x001b00ec,  10},
	{ 0x001b0240,  41},
	{ 0x001b02ec,  10},
	{ 0x001b0440,  41},
	{ 0x001b04ec,  10},
	{ 0x001b0640,  41},
	{ 0x001b06ec,  10},
	{ 0x001b0840,  41},
	{ 0x001b08ec,  10},
	{ 0x001b0a40,  41},
	{ 0x001b0aec,  10},
	{ 0x001b0c40,  41},
	{ 0x001b0cec,  10},
	{ 0x001b0e40,  41},
	{ 0x001b0eec,  10},
	{ 0x001b4000,   1},
	{ 0x001b4008,   1},
	{ 0x001b4010,   3},
	{ 0x001b4020,   3},
	{ 0x001b4030,   3},
	{ 0x001b4040,   3},
	{ 0x001b4050,   3},
	{ 0x001b4060,   4},
	{ 0x001b4074,   7},
	{ 0x001b4094,   3},
	{ 0x001b40a4,   1},
	{ 0x001b4100,   6},
	{ 0x001b4124,   2},
	{ 0x001b8000,   1},
	{ 0x001b8008,   1},
	{ 0x001b8010,   3},
	{ 0x001bc000,   1},
	{ 0x001bc008,   1},
	{ 0x001bc010,   3},
	{ 0x001be000,   1},
	{ 0x001be008,   1},
	{ 0x001be010,   3},
	{ 0x00400500,   1},
	{ 0x0040415c,   1},
	{ 0x00404468,   1},
	{ 0x00404498,   1},
	{ 0x00405800,   1},
	{ 0x00405840,   2},
	{ 0x00405850,   1},
	{ 0x00405908,   1},
	{ 0x00405b40,   1},
	{ 0x00405b50,   1},
	{ 0x00406024,   5},
	{ 0x00407010,   1},
	{ 0x00407808,   1},
	{ 0x0040803c,   1},
	{ 0x00408804,   1},
	{ 0x0040880c,   1},
	{ 0x00408900,   2},
	{ 0x00408910,   1},
	{ 0x00408944,   1},
	{ 0x00408984,   1},
	{ 0x004090a8,   1},
	{ 0x004098a0,   1},
	{ 0x00409b00,   1},
	{ 0x0041000c,   1},
	{ 0x00410110,   1},
	{ 0x00410184,   1},
	{ 0x0041040c,   1},
	{ 0x00410510,   1},
	{ 0x00410584,   1},
	{ 0x00418000,   1},
	{ 0x00418008,   1},
	{ 0x00418380,   2},
	{ 0x00418400,   2},
	{ 0x004184a0,   1},
	{ 0x00418604,   1},
	{ 0x00418680,   1},
	{ 0x00418704,   1},
	{ 0x00418714,   1},
	{ 0x00418800,   1},
	{ 0x0041881c,   1},
	{ 0x00418830,   1},
	{ 0x00418884,   1},
	{ 0x004188b0,   1},
	{ 0x004188c8,   3},
	{ 0x004188fc,   1},
	{ 0x00418b04,   1},
	{ 0x00418c04,   1},
	{ 0x00418c10,   8},
	{ 0x00418c88,   1},
	{ 0x00418d00,   1},
	{ 0x00418e00,   1},
	{ 0x00418e08,   1},
	{ 0x00418e34,   1},
	{ 0x00418e40,   4},
	{ 0x00418e58,  16},
	{ 0x00418f08,   1},
	{ 0x00419000,   1},
	{ 0x0041900c,   1},
	{ 0x00419018,   1},
	{ 0x00419854,   1},
	{ 0x00419864,   1},
	{ 0x00419a04,   2},
	{ 0x00419a14,   1},
	{ 0x00419ab0,   1},
	{ 0x00419ab8,   3},
	{ 0x00419c0c,   1},
	{ 0x00419c8c,   2},
	{ 0x00419d00,   1},
	{ 0x00419d08,   2},
	{ 0x00419e00,  11},
	{ 0x00419e34,   2},
	{ 0x00419e44,  11},
	{ 0x00419e74,  10},
	{ 0x00419ea4,   1},
	{ 0x00419eac,   2},
	{ 0x00419ee8,   1},
	{ 0x00419ef0,  28},
	{ 0x00419f70,   1},
	{ 0x00419f78,   2},
	{ 0x00419f98,   2},
	{ 0x00419fdc,   1},
	{ 0x0041a02c,   2},
	{ 0x0041a0a0,   1},
	{ 0x0041a0a8,   1},
	{ 0x0041a890,   2},
	{ 0x0041a8a0,   3},
	{ 0x0041a8b0,   2},
	{ 0x0041b014,   1},
	{ 0x0041b0a0,   1},
	{ 0x0041b0cc,   1},
	{ 0x0041b1dc,   1},
	{ 0x0041be0c,   3},
	{ 0x0041bea0,   1},
	{ 0x0041becc,   1},
	{ 0x0041bfdc,   1},
	{ 0x0041c054,   1},
	{ 0x0041c2b0,   1},
	{ 0x0041c2b8,   3},
	{ 0x0041c40c,   1},
	{ 0x0041c48c,   2},
	{ 0x0041c500,   1},
	{ 0x0041c508,   2},
	{ 0x0041c600,  11},
	{ 0x0041c634,   2},
	{ 0x0041c644,  11},
	{ 0x0041c674,  10},
	{ 0x0041c6a4,   1},
	{ 0x0041c6ac,   2},
	{ 0x0041c6e8,   1},
	{ 0x0041c6f0,  28},
	{ 0x0041c770,   1},
	{ 0x0041c778,   2},
	{ 0x0041c798,   2},
	{ 0x0041c7dc,   1},
	{ 0x0041c854,   1},
	{ 0x0041cab0,   1},
	{ 0x0041cab8,   3},
	{ 0x0041cc0c,   1},
	{ 0x0041cc8c,   2},
	{ 0x0041cd00,   1},
	{ 0x0041cd08,   2},
	{ 0x0041ce00,  11},
	{ 0x0041ce34,   2},
	{ 0x0041ce44,  11},
	{ 0x0041ce74,  10},
	{ 0x0041cea4,   1},
	{ 0x0041ceac,   2},
	{ 0x0041cee8,   1},
	{ 0x0041cef0,  28},
	{ 0x0041cf70,   1},
	{ 0x0041cf78,   2},
	{ 0x0041cf98,   2},
	{ 0x0041cfdc,   1},
	{ 0x00500384,   1},
	{ 0x005004a0,   1},
	{ 0x00500604,   1},
	{ 0x00500680,   1},
	{ 0x00500714,   1},
	{ 0x0050081c,   1},
	{ 0x00500884,   1},
	{ 0x005008b0,   1},
	{ 0x005008c8,   3},
	{ 0x005008fc,   1},
	{ 0x00500b04,   1},
	{ 0x00500c04,   1},
	{ 0x00500c10,   8},
	{ 0x00500c88,   1},
	{ 0x00500d00,   1},
	{ 0x00500e08,   1},
	{ 0x00500f08,   1},
	{ 0x00501000,   1},
	{ 0x0050100c,   1},
	{ 0x00501018,   1},
	{ 0x00501854,   1},
	{ 0x00501ab0,   1},
	{ 0x00501ab8,   3},
	{ 0x00501c0c,   1},
	{ 0x00501c8c,   2},
	{ 0x00501d00,   1},
	{ 0x00501d08,   2},
	{ 0x00501e00,  11},
	{ 0x00501e34,   2},
	{ 0x00501e44,  11},
	{ 0x00501e74,  10},
	{ 0x00501ea4,   1},
	{ 0x00501eac,   2},
	{ 0x00501ee8,   1},
	{ 0x00501ef0,  28},
	{ 0x00501f70,   1},
	{ 0x00501f78,   2},
	{ 0x00501f98,   2},
	{ 0x00501fdc,   1},
	{ 0x0050202c,   2},
	{ 0x005020a0,   1},
	{ 0x005020a8,   1},
	{ 0x00502890,   2},
	{ 0x005028a0,   3},
	{ 0x005028b0,   2},
	{ 0x00503014,   1},
	{ 0x005030a0,   1},
	{ 0x005030cc,   1},
	{ 0x005031dc,   1},
	{ 0x00503e14,   1},
	{ 0x00503ea0,   1},
	{ 0x00503ecc,   1},
	{ 0x00503fdc,   1},
	{ 0x00504054,   1},
	{ 0x005042b0,   1},
	{ 0x005042b8,   3},
	{ 0x0050440c,   1},
	{ 0x0050448c,   2},
	{ 0x00504500,   1},
	{ 0x00504508,   2},
	{ 0x00504600,  11},
	{ 0x00504634,   2},
	{ 0x00504644,  11},
	{ 0x00504674,  10},
	{ 0x005046a4,   1},
	{ 0x005046ac,   2},
	{ 0x005046e8,   1},
	{ 0x005046f0,  28},
	{ 0x00504770,   1},
	{ 0x00504778,   2},
	{ 0x00504798,   2},
	{ 0x005047dc,   1},
	{ 0x00504854,   1},
	{ 0x00504ab0,   1},
	{ 0x00504ab8,   3},
	{ 0x00504c0c,   1},
	{ 0x00504c8c,   2},
	{ 0x00504d00,   1},
	{ 0x00504d08,   2},
	{ 0x00504e00,  11},
	{ 0x00504e34,   2},
	{ 0x00504e44,  11},
	{ 0x00504e74,  10},
	{ 0x00504ea4,   1},
	{ 0x00504eac,   2},
	{ 0x00504ee8,   1},
	{ 0x00504ef0,  28},
	{ 0x00504f70,   1},
	{ 0x00504f78,   2},
	{ 0x00504f98,   2},
	{ 0x00504fdc,   1},
	{ 0x00900100,   1},
	{ 0x009a0100,   1},
};

static const u32 gp10b_global_whitelist_ranges_count =
	ARRAY_SIZE(gp10b_global_whitelist_ranges);

/* context */

/* runcontrol */
static const u32 gp10b_runcontrol_whitelist[] = {
};
static const u32 gp10b_runcontrol_whitelist_count =
	ARRAY_SIZE(gp10b_runcontrol_whitelist);

static const struct regop_offset_range gp10b_runcontrol_whitelist_ranges[] = {
};
static const u32 gp10b_runcontrol_whitelist_ranges_count =
	ARRAY_SIZE(gp10b_runcontrol_whitelist_ranges);


/* quad ctl */
static const u32 gp10b_qctl_whitelist[] = {
};
static const u32 gp10b_qctl_whitelist_count =
	ARRAY_SIZE(gp10b_qctl_whitelist);

static const struct regop_offset_range gp10b_qctl_whitelist_ranges[] = {
};
static const u32 gp10b_qctl_whitelist_ranges_count =
	ARRAY_SIZE(gp10b_qctl_whitelist_ranges);

static const struct regop_offset_range *gp10b_get_global_whitelist_ranges(void)
{
	return gp10b_global_whitelist_ranges;
}

static int gp10b_get_global_whitelist_ranges_count(void)
{
	return gp10b_global_whitelist_ranges_count;
}

static const struct regop_offset_range *gp10b_get_context_whitelist_ranges(void)
{
	return gp10b_global_whitelist_ranges;
}

static int gp10b_get_context_whitelist_ranges_count(void)
{
	return gp10b_global_whitelist_ranges_count;
}

static const u32 *gp10b_get_runcontrol_whitelist(void)
{
	return gp10b_runcontrol_whitelist;
}

static int gp10b_get_runcontrol_whitelist_count(void)
{
	return gp10b_runcontrol_whitelist_count;
}

static const
struct regop_offset_range *gp10b_get_runcontrol_whitelist_ranges(void)
{
	return gp10b_runcontrol_whitelist_ranges;
}

static int gp10b_get_runcontrol_whitelist_ranges_count(void)
{
	return gp10b_runcontrol_whitelist_ranges_count;
}

static const u32 *gp10b_get_qctl_whitelist(void)
{
	return gp10b_qctl_whitelist;
}

static int gp10b_get_qctl_whitelist_count(void)
{
	return gp10b_qctl_whitelist_count;
}

static const struct regop_offset_range *gp10b_get_qctl_whitelist_ranges(void)
{
	return gp10b_qctl_whitelist_ranges;
}

static int gp10b_get_qctl_whitelist_ranges_count(void)
{
	return gp10b_qctl_whitelist_ranges_count;
}

static int gp10b_apply_smpc_war(struct dbg_session_gk20a *dbg_s)
{
	/* Not needed on gp10b */
	return 0;
}

void gp10b_init_regops(struct gpu_ops *gops)
{
	gops->regops.get_global_whitelist_ranges =
		gp10b_get_global_whitelist_ranges;
	gops->regops.get_global_whitelist_ranges_count =
		gp10b_get_global_whitelist_ranges_count;

	gops->regops.get_context_whitelist_ranges =
		gp10b_get_context_whitelist_ranges;
	gops->regops.get_context_whitelist_ranges_count =
		gp10b_get_context_whitelist_ranges_count;

	gops->regops.get_runcontrol_whitelist =
		gp10b_get_runcontrol_whitelist;
	gops->regops.get_runcontrol_whitelist_count =
		gp10b_get_runcontrol_whitelist_count;

	gops->regops.get_runcontrol_whitelist_ranges =
		gp10b_get_runcontrol_whitelist_ranges;
	gops->regops.get_runcontrol_whitelist_ranges_count =
		gp10b_get_runcontrol_whitelist_ranges_count;

	gops->regops.get_qctl_whitelist =
		gp10b_get_qctl_whitelist;
	gops->regops.get_qctl_whitelist_count =
		gp10b_get_qctl_whitelist_count;

	gops->regops.get_qctl_whitelist_ranges =
		gp10b_get_qctl_whitelist_ranges;
	gops->regops.get_qctl_whitelist_ranges_count =
		gp10b_get_qctl_whitelist_ranges_count;

	gops->regops.apply_smpc_war =
		gp10b_apply_smpc_war;
}
