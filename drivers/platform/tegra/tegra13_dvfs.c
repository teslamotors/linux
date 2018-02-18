/*
 * drivers/platform/tegra/tegra13_dvfs.c
 *
 * Copyright (c) 2012-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/pm_qos.h>
#include <linux/tegra-fuse.h>
#include <linux/delay.h>

#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include "board.h"
#include <linux/platform/tegra/tegra_cl_dvfs.h>
#include "tegra_core_sysfs_limits.h"
#include "pm.h"
#include "tegra_simon.h"

static bool tegra_dvfs_cpu_disabled;
static bool tegra_dvfs_core_disabled;
static bool tegra_dvfs_gpu_disabled;

#define KHZ 1000
#define MHZ 1000000

#define VDD_SAFE_STEP			100

/*
 * Effectivly no offsets. We still need DFLL tuning adjustment according to
 * SiMon grade, but SiMon call-backs are enabled only if offsets are installed.
 * So, just make them "0".
 */
static int cpu_vmin_offsets[2];
static int gpu_vmin_offsets[2];

static int vdd_core_vmin_trips_table[MAX_THERMAL_LIMITS] = { 20, };
static int vdd_core_therm_floors_table[MAX_THERMAL_LIMITS] = { 950, };

static int vdd_core_vmax_trips_table[MAX_THERMAL_LIMITS] = { 62, 72, 82, };
static int vdd_core_therm_caps_table[MAX_THERMAL_LIMITS] = { 1130, 1100, 1060, };

#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
static int vdd_cpu_vmax_trips_table[MAX_THERMAL_LIMITS] = { 62,   72,   82, };
static int vdd_cpu_therm_caps_table[MAX_THERMAL_LIMITS] = { 1230, 1210, 1180, };

static struct tegra_cooling_device cpu_vmax_cdev = {
	.cdev_type = "cpu_hot",
};
#endif

static struct tegra_cooling_device cpu_vmin_cdev = {
	.cdev_type = "cpu_cold",
};

static struct tegra_cooling_device core_vmax_cdev = {
	.cdev_type = "core_hot",
};

static struct tegra_cooling_device core_vmin_cdev = {
	.cdev_type = "core_cold",
};

static struct tegra_cooling_device gpu_vmin_cdev = {
	.cdev_type = "gpu_cold",
};

static struct tegra_cooling_device gpu_vts_cdev = {
	.cdev_type = "gpu_scaling",
};

static struct dvfs_rail tegra13_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.version = "p4v18",
	.max_millivolts = 1300,
	.min_millivolts = 680,
	.simon_domain = TEGRA_SIMON_DOMAIN_CPU,
	.step = VDD_SAFE_STEP,
	.jmp_to_zero = true,
	.vmin_cdev = &cpu_vmin_cdev,
#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
	.vmax_cdev = &cpu_vmax_cdev,
#endif
	.alignment = {
		.step_uv = 10000, /* 10mV */
	},
	.stats = {
		.bin_uV = 10000, /* 10mV */
	}
};

static struct dvfs_rail tegra13_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.version = "p4v11",
	.max_millivolts = 1400,
	.min_millivolts = 800,
	.step = VDD_SAFE_STEP,
	.step_up = 1400,
	.vmin_cdev = &core_vmin_cdev,
	.vmax_cdev = &core_vmax_cdev,
};

static struct dvfs_rail tegra13_dvfs_rail_vdd_gpu = {
	.reg_id = "vdd_gpu",
	.version = "p4_v10",
	.max_millivolts = 1350,
	.min_millivolts = 650,
	.simon_domain = TEGRA_SIMON_DOMAIN_GPU,
	.step = VDD_SAFE_STEP,
	.step_up = 1350,
	.in_band_pm = true,
	.vts_cdev = &gpu_vts_cdev,
	.vmin_cdev = &gpu_vmin_cdev,
	.alignment = {
		.step_uv = 10000, /* 10mV */
	},
	.stats = {
		.bin_uV = 10000, /* 10mV */
	}
};

static struct dvfs_rail *tegra13_dvfs_rails[] = {
	&tegra13_dvfs_rail_vdd_cpu,
	&tegra13_dvfs_rail_vdd_core,
	&tegra13_dvfs_rail_vdd_gpu,
};

static int tegra13_get_core_floor_mv(int cpu_mv)
{
	if (cpu_mv < 800)
		return 800;
	if (cpu_mv <= 900)
		return 830;
	if (cpu_mv <= 1000)
		return 870;
	if (cpu_mv <= 1100)
		return 900;
	if (cpu_mv <= 1200)
		return 940;
	return 970;
}

/* vdd_core must be >= min_level as a function of vdd_cpu */
static int tegra13_dvfs_rel_vdd_cpu_vdd_core(struct dvfs_rail *vdd_cpu,
	struct dvfs_rail *vdd_core)
{
	int core_mv;
	int cpu_mv = max(vdd_cpu->new_millivolts, vdd_cpu->millivolts);

	if (tegra_dvfs_rail_is_dfll_mode(vdd_cpu)) {
		/* 30mV thermal floor slack in dfll mode */
		int cpu_floor_mv = tegra_dvfs_rail_get_thermal_floor(vdd_cpu);
		cpu_mv = max(cpu_mv, cpu_floor_mv + 30);
	}

	core_mv = tegra13_get_core_floor_mv(cpu_mv);
	core_mv = max(vdd_core->new_millivolts, core_mv);

	if (vdd_cpu->resolving_to && (core_mv < vdd_core->millivolts))
		udelay(100);	/* let vdd_cpu discharging settle */
	return core_mv;
}

static struct dvfs_relationship tegra13_dvfs_relationships[] = {
	{
		.from = &tegra13_dvfs_rail_vdd_cpu,
		.to = &tegra13_dvfs_rail_vdd_core,
		.solve = tegra13_dvfs_rel_vdd_cpu_vdd_core,
		.solved_at_nominal = true,
	},
};

void __init tegra13x_vdd_cpu_align(int step_uv, int offset_uv)
{
	tegra13_dvfs_rail_vdd_cpu.alignment.step_uv = step_uv;
	tegra13_dvfs_rail_vdd_cpu.alignment.offset_uv = offset_uv;
}

/* CPU DVFS tables */
static unsigned long cpu_max_freq[] = {
/* speedo_id	0	 1	  2	   3      */
		2499000, 2499000,
};

static struct cpu_cvb_dvfs cpu_cvb_dvfs_table[] = {
	/* A01 DVFS table */
	{
		.speedo_id = 0,
		.process_id = -1,
		.dfll_tune_data  = {
			.tune0		= 0x00FF2FFF,
			.tune0_high_mv	= 0x00FF40E5,
			.tune1		= 0x000000FF,
			.droop_rate_min = 1000000,
			.tune_high_min_millivolts = 960,
			.min_millivolts = 800,
			.tune_high_margin_mv = 30,
		},
		.max_mv = 1260,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll: c0,     c1,   c2  pll:  c0,   c1,    c2 */
			{510000,        {1413914, -39055, 488}, {880000, 0, 0}},
			{612000,        {1491617, -40975, 488}, {920000, 0, 0}},
			{714000,        {1571360, -42895, 488}, {960000, 0, 0}},
			{816000,        {1653143, -44815, 488}, {1000000, 0, 0}},
			{918000,        {1736966, -46725, 488}, {1050000, 0, 0}},
			{1020000,       {1822828, -48645, 488}, {1090000, 0, 0}},
			{1122000,       {1910731, -50565, 488}, {1130000, 0, 0}},
			{1224000,       {2000673, -52485, 488}, {1170000, 0, 0}},
			{      0 , 	{      0,      0,   0}, {      0, 0, 0}},
		},
		.vmin_trips_table = { 15, },
		.therm_floors_table = { 900, },
	},
	/* A02 DVFS table */
	{
		.speedo_id = 1,
		.process_id = -1,
		.dfll_tune_data  = {
			.tune1		= 0x00000095,
			.droop_rate_min = 1000000,
			.min_millivolts = 800,
			.tune_high_margin_mv = 30,
		},
		.max_mv = 1260,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.thermal_scale = 10,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f       dfll: c0,     c1,   c2  pll:  c0,   c1,    c2 */
			{204000,        {1225091, -39915,  743}, {800000,  0, 0} },
			{306000,        {1263591, -41215,  743}, {800000,  0, 0} },
			{408000,        {1303202, -42515,  743}, {840000,  0, 0} },
			{510000,        {1343922, -43815,  743}, {880000,  0, 0} },
			{612000,        {1385753, -45115,  743}, {920000,  0, 0} },
			{714000,        {1428693, -46415,  743}, {960000,  0, 0} },
			{816000,        {1472743, -47715,  743}, {1000000, 0, 0} },
			{918000,        {1517903, -49015,  743}, {1050000, 0, 0} },
			{1020000,       {1564174, -50315,  743}, {1090000, 0, 0} },
			{1122000,       {1611553, -51615,  743}, {1130000, 0, 0} },
			{1224000,       {1660043, -52915,  743}, {1170000, 0, 0} },
			{1326000,       {1709643, -54215,  743}, {1210000, 0, 0} },
			{1428000,       {1760353, -55515,  743}, {1260000, 0, 0} },
			{1530000,       {1812172, -56815,  743}, {1300000, 0, 0} },
			{1632000,       {1865102, -58115,  743}, {1300000, 0, 0} },
			{1734000,       {1919141, -59425,  743}, {1300000, 0, 0} },
			{1836000,       {1974291, -60725,  743}, {1300000, 0, 0} },
			{1938000,       {2030550, -62025,  743}, {1300000, 0, 0} },
			{2014500,       {2073190, -62985,  743}, {1300000, 0, 0} },
			{2091000,       {2117020, -63975,  743}, {1300000, 0, 0} },
			{2193000,       {2176054, -65275,  743}, {1300000, 0, 0} },
			{2295000,       {2236198, -66575,  743}, {1300000, 0, 0} },
			{2397000,       {2297452, -67875,  743}, {1300000, 0, 0} },
			{2499000,       {2359816, -69175,  743}, {1300000, 0, 0} },
			{      0,       {      0,      0,    0}, {      0, 0, 0} },
		},
		.cvb_vmin =  {  0, { 2877000, -174300, 3600, -357,  -339,  53}, },
		.vmin_trips_table = { 15, 30, 50, 70, 120, },
		.therm_floors_table = { 890, 800, 800, 800, 800, },
	},
};

static int cpu_vmin[MAX_THERMAL_RANGES];
static int cpu_millivolts[MAX_DVFS_FREQS];
static int cpu_dfll_millivolts[MAX_DVFS_FREQS];

static struct dvfs cpu_dvfs = {
	.clk_name	= "cpu_g",
	.millivolts	= cpu_millivolts,
	.dfll_millivolts = cpu_dfll_millivolts,
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra13_dvfs_rail_vdd_cpu,
};

/* Core DVFS tables */
static const int core_millivolts[MAX_DVFS_FREQS] = {
	800, 850, 900, 950, 1000, 1050, 1100, 1150};

#define CORE_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...) \
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra13_dvfs_rail_vdd_core,	\
	}

#define OVRRD_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...) \
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.can_override	= true,				\
		.dvfs_rail	= &tegra13_dvfs_rail_vdd_core,	\
	}

static struct dvfs core_dvfs_table[] = {
	/* Core voltages (mV):		         800,    850,    900,	 950,    1000,	1050,    1100,	 1150 */
	/* Clock limits for internal blocks, PLLs */

        CORE_DVFS("emc",        -1, -1, 1, KHZ, 264000, 348000, 384000, 384000, 528000, 528000, 1066000, 1200000),

        CORE_DVFS("sbus",       0, 0, 1, KHZ,   120000, 180000, 228000, 264000, 312000, 348000, 372000, 372000),
        CORE_DVFS("sbus",       0, 1, 1, KHZ,   120000, 204000, 252000, 288000, 324000, 360000, 372000, 372000),

        CORE_DVFS("vic03",      0, 0, 1, KHZ,   180000, 240000, 324000, 420000, 492000, 576000, 648000, 720000),
        CORE_DVFS("vic03",      0, 1, 1, KHZ,   180000, 336000, 420000, 504000, 600000, 684000, 720000, 720000),

        CORE_DVFS("tsec",       0, 0, 1, KHZ,   180000, 240000, 324000, 420000, 492000, 576000, 648000, 720000),
        CORE_DVFS("tsec",       0, 1, 1, KHZ,   180000, 336000, 420000, 504000, 600000, 684000, 720000, 720000),

        CORE_DVFS("msenc",      0, 0, 1, KHZ,    84000, 168000, 216000, 276000, 324000, 372000, 420000, 456000),
        CORE_DVFS("msenc",      0, 1, 1, KHZ,   120000, 228000, 276000, 348000, 396000, 444000, 456000, 456000),

        CORE_DVFS("se",         0, 0, 1, KHZ,    84000, 168000, 216000, 276000, 324000, 372000, 420000, 456000),
        CORE_DVFS("se",         0, 1, 1, KHZ,   120000, 228000, 276000, 348000, 396000, 444000, 456000, 456000),

        CORE_DVFS("vde",        0, 0, 1, KHZ,    84000, 168000, 216000, 276000, 324000, 372000, 420000, 456000),
        CORE_DVFS("vde",        0, 1, 1, KHZ,   120000, 228000, 276000, 348000, 396000, 444000, 456000, 456000),

        CORE_DVFS("host1x",     0, 0, 1, KHZ,   108000, 156000, 204000, 240000, 348000, 372000, 408000, 408000),
        CORE_DVFS("host1x",     0, 1, 1, KHZ,   108000, 156000, 204000, 252000, 348000, 384000, 408000, 408000),

        CORE_DVFS("vi",         0, 0, 1, KHZ,        1, 324000, 420000, 516000, 600000, 600000, 600000, 600000),
        CORE_DVFS("vi",         0, 1, 1, KHZ,        1, 420000, 480000, 600000, 600000, 600000, 600000, 600000),

        CORE_DVFS("isp",        0, 0, 1, KHZ,        1, 324000, 420000, 516000, 600000, 600000, 600000, 600000),
        CORE_DVFS("isp",        0, 1, 1, KHZ,        1, 420000, 480000, 600000, 600000, 600000, 600000, 600000),

#ifdef CONFIG_TEGRA_DUAL_CBUS
        CORE_DVFS("c2bus",      0, 0, 1, KHZ,    84000, 168000, 216000, 276000, 324000, 372000, 420000, 456000),
        CORE_DVFS("c2bus",      0, 1, 1, KHZ,   120000, 228000, 276000, 348000, 396000, 444000, 456000, 456000),

        CORE_DVFS("c3bus",      0, 0, 1, KHZ,   180000, 240000, 324000, 420000, 492000, 576000, 648000, 720000),
        CORE_DVFS("c3bus",      0, 1, 1, KHZ,   180000, 336000, 420000, 504000, 600000, 684000, 720000, 720000),
#else
	CORE_DVFS("cbus",      -1, -1, 1, KHZ,  120000, 144000, 168000, 168000, 216000, 216000, 372000, 372000),
#endif

        CORE_DVFS("c4bus",      0, 0, 1, KHZ,        1, 324000, 420000, 516000, 600000, 600000, 600000, 600000),
        CORE_DVFS("c4bus",      0, 1, 1, KHZ,        1, 420000, 480000, 600000, 600000, 600000, 600000, 600000),

	CORE_DVFS("pll_m",  -1, -1, 1, KHZ,   800000,  800000, 1066000, 1066000, 1066000, 1066000, 1200000, 1200000),
	CORE_DVFS("pll_c",  -1, -1, 1, KHZ,   800000,  800000, 1066000, 1066000, 1066000, 1066000, 1066000, 1066000),
	CORE_DVFS("pll_c2", -1, -1, 1, KHZ,   800000,  800000, 1066000, 1066000, 1066000, 1066000, 1066000, 1066000),
	CORE_DVFS("pll_c3", -1, -1, 1, KHZ,   800000,  800000, 1066000, 1066000, 1066000, 1066000, 1066000, 1066000),

	/* Core voltages (mV):		         800,    850,    900,	 950,    1000,	1050,    1100,	 1150 */
	/* Clock limits for I/O peripherals */

	CORE_DVFS("dsia",   -1, -1, 1, KHZ,   402000, 500000, 750000, 750000,  750000, 750000, 750000, 750000),
	CORE_DVFS("dsib",   -1, -1, 1, KHZ,   402000, 500000, 750000, 750000,  750000, 750000, 750000, 750000),
	CORE_DVFS("dsialp",   -1, -1, 1, KHZ, 102000, 102000, 102000, 102000,  156000, 156000, 156000, 156000),
	CORE_DVFS("dsiblp",   -1, -1, 1, KHZ, 102000, 102000, 102000, 102000,  156000, 156000, 156000, 156000),

	CORE_DVFS("hdmi",   -1, -1, 1, KHZ,        1, 148500, 148500, 297000,  297000, 297000, 297000, 297000),
	/* FIXME: Finalize these values for NOR after qual */
	CORE_DVFS("nor",    -1, -1, 1, KHZ,   102000, 102000, 102000, 102000,  102000, 102000, 102000, 102000),

	CORE_DVFS("pciex",  -1,  -1, 1, KHZ,       1, 250000, 250000, 500000,  500000, 500000, 500000, 500000),
	CORE_DVFS("mselect", -1,  -1, 1, KHZ,  102000, 102000, 204000, 204000,  204000, 204000, 408000, 408000),

	/* Core voltages (mV):		         	800,    850,    900,	 950,    1000,	1050,    1100,	 1150 */
	/* xusb clocks */
	CORE_DVFS("xusb_falcon_src", -1, -1, 1, KHZ,  	  1, 336000, 336000, 336000, 336000, 336000 ,  336000,  336000),
	CORE_DVFS("xusb_host_src",   -1, -1, 1, KHZ,  	  1, 112000, 112000, 112000, 112000, 112000 ,  112000,  112000),
	CORE_DVFS("xusb_dev_src",    -1, -1, 1, KHZ,  	  1,  58300,  58300,  58300, 112000, 112000 ,  112000,  112000),
	CORE_DVFS("xusb_ss_src",     -1, -1, 1, KHZ,  	  1, 120000, 120000, 120000, 120000, 120000 ,  120000,  120000),
	CORE_DVFS("xusb_fs_src",     -1, -1, 1, KHZ,  	  1,  48000,  48000,  48000,  48000,  48000 ,   48000,   48000),
	CORE_DVFS("xusb_hs_src",     -1, -1, 1, KHZ,  	  1,  60000,  60000,  60000,  60000,  60000 ,   60000,   60000),

	CORE_DVFS("hda",    	     -1, -1, 1, KHZ,  	  1, 108000, 108000, 108000, 108000, 108000 ,  108000,  108000),
	CORE_DVFS("hda2codec_2x",    -1, -1, 1, KHZ,  	  1,  48000,  48000,  48000,  48000,  48000 ,   48000,   48000),

        CORE_DVFS("sor0",             0, -1, 1, KHZ,  162500, 270000, 540000, 540000, 540000,  540000,  540000,  540000),

	OVRRD_DVFS("sdmmc1", -1, -1, 1, KHZ,       	  1,      1,  82000,  82000,  136000, 136000, 136000, 204000),
	OVRRD_DVFS("sdmmc3", -1, -1, 1, KHZ,          	  1,      1,  82000,  82000,  136000, 136000, 136000, 204000),
	OVRRD_DVFS("sdmmc4", -1, -1, 1, KHZ,       	  1,      1,  82000,  82000,  136000, 136000, 136000, 200000),
};

/*
 *
 * Display peak voltage aggregation into override range floor is deferred until
 * actual pixel clock for the particular platform is known. This would allow to
 * extend sdmmc tuning range on the platforms that do not excercise maximum
 * display clock capabilities specified in DVFS table.
 *
 */
#define DEFER_DVFS(_clk_name, _speedo_id, _process_id, _auto, _mult, _freqs...) \
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= _process_id,			\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.defer_override = true,				\
		.dvfs_rail	= &tegra13_dvfs_rail_vdd_core,	\
	}

static struct dvfs disp_dvfs_table[] = {
	/*
	 * The clock rate for the display controllers that determines the
	 * necessary core voltage depends on a divider that is internal
	 * to the display block.  Disable auto-dvfs on the display clocks,
	 * and let the display driver call tegra_dvfs_set_rate manually
	 */
	/* Core voltages (mV)			  800,    850,    900,    950,    1000,   1050,   1100,   1150 */
	DEFER_DVFS("disp1",       0,  0, 0, KHZ,       1, 240000, 282000, 330000, 388000, 408000, 456000, 490000),
	DEFER_DVFS("disp1",       0,  1, 0, KHZ,  192000, 247000, 306000, 342000, 400000, 432000, 474000, 535000),

	DEFER_DVFS("disp2",       0,  0, 0, KHZ,       1, 240000, 282000, 330000, 388000, 408000, 456000, 490000),
	DEFER_DVFS("disp2",       0,  1, 0, KHZ,  192000, 247000, 306000, 342000, 400000, 432000, 474000, 535000),
};

/* Alternative display dvfs table: applicable if only one window B is active */
static struct dvfs disp_alt_dvfs_table[] = {
	/* Core voltages (mV):		          800,    850,    900,	  950,    1000,	  1050,   1100,	  1150 */
	DEFER_DVFS("disp1",       0,  0, 0, KHZ,        1, 272000, 330000, 400000, 456000, 490000, 490000, 490000),
	DEFER_DVFS("disp1",       0,  1, 0, KHZ,   216000, 280000, 342000, 408000, 480000, 506000, 535000, 535000),

	DEFER_DVFS("disp2",       0,  0, 0, KHZ,        1, 272000, 330000, 400000, 456000, 490000, 490000, 490000),
	DEFER_DVFS("disp2",       0,  1, 0, KHZ,   216000, 280000, 342000, 408000, 480000, 506000, 535000, 535000),
};

static int resolve_core_override(int min_override_mv)
{
	/* nothing to do -- always resolved */
	return 0;
}

/* GPU DVFS tables */
static unsigned long gpu_max_freq[] = {
/* speedo_id	0	1	2	*/
		804000, 852000, 918000,
};
static struct gpu_cvb_dvfs gpu_cvb_dvfs_table[] = {
	{
		.speedo_id =  -1,
		.process_id = -1,
		.max_mv = 1200,
		.freqs_mult = KHZ,
		.speedo_scale = 100,
		.thermal_scale = 10,
		.voltage_scale = 1000,
		.cvb_table = {
			/*f        dfll  pll:   c0,     c1,   c2,   c3,      c4,   c5 */
			{   72000, {  }, { 1209886, -36468,  515,   417, -13123,  203}, },
			{  108000, {  }, { 1130804, -27659,  296,   298, -10834,  221}, },
			{  180000, {  }, { 1162871, -27110,  247,   238, -10681,  268}, },
			{  252000, {  }, { 1220458, -28654,  247,   179, -10376,  298}, },
			{  324000, {  }, { 1280953, -30204,  247,   119,  -9766,  304}, },
			{  396000, {  }, { 1344547, -31777,  247,   119,  -8545,  292}, },
			{  468000, {  }, { 1420168, -34227,  269,    60,  -7172,  256}, },
			{  540000, {  }, { 1490757, -35955,  274,    60,  -5188,  197}, },
			{  612000, {  }, { 1599112, -42583,  398,     0,  -1831,  119}, },
			{  648000, {  }, { 1366986, -16459, -274,     0,  -3204,   72}, },
			{  684000, {  }, { 1391884, -17078, -274,   -60,  -1526,   30}, },
			{  708000, {  }, { 1415522, -17497, -274,   -60,   -458,    0}, },
			{  756000, {  }, { 1464061, -18331, -274,  -119,   1831,  -72}, },
			{  804000, {  }, { 1524225, -20064, -254,  -119,   4272, -155}, },
			{  852000, {  }, { 1608418, -21643, -269,     0,    763,  -48}, },
			{  900000, {  }, { 1706383, -25155, -209,     0,    305,    0}, },
			{  918000, {  }, { 1729600, -26289, -194,     0,    763,    0}, },
			{  954000, {  }, { 1880996, -35353,   14,  -179,   4120,   24}, },
			{  984000, {  }, { 1890996, -35353,   14,  -179,   4120,   24}, },
			{ 1008000, {  }, { 2015834, -44439,  271,  -596,   4730, 1222}, },
			{       0, {  }, { }, },
		},
		.cvb_vmin =  {  0, {  }, { 1180000, -18900,    0,     0,  -6110,    0}, },
		.vmin_trips_table = { 15, },
		.therm_floors_table = { 900, },
		.vts_trips_table = { -10, 10, 30, 50, 70, },
	}
};

static int gpu_vmin[MAX_THERMAL_RANGES];
static int gpu_peak_millivolts[MAX_DVFS_FREQS];
static int gpu_millivolts[MAX_THERMAL_RANGES][MAX_DVFS_FREQS];
static int gpu_millivolts_offs[MAX_THERMAL_RANGES][MAX_DVFS_FREQS];
static struct dvfs gpu_dvfs = {
	.clk_name	= "gbus",
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra13_dvfs_rail_vdd_gpu,
};

static struct notifier_block gpu_simon_grade_nb;

int tegra_dvfs_disable_core_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra13_dvfs_rail_vdd_core);

	return 0;
}

int tegra_dvfs_disable_cpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra13_dvfs_rail_vdd_cpu);

	return 0;
}

int tegra_dvfs_disable_gpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_gpu_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_gpu);
	else
		tegra_dvfs_rail_enable(&tegra13_dvfs_rail_vdd_gpu);

	return 0;
}

int tegra_dvfs_disable_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops tegra_dvfs_disable_core_ops = {
	.set = tegra_dvfs_disable_core_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_cpu_ops = {
	.set = tegra_dvfs_disable_cpu_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_gpu_ops = {
	.set = tegra_dvfs_disable_gpu_set,
	.get = tegra_dvfs_disable_get,
};

module_param_cb(disable_core, &tegra_dvfs_disable_core_ops,
	&tegra_dvfs_core_disabled, 0644);
module_param_cb(disable_cpu, &tegra_dvfs_disable_cpu_ops,
	&tegra_dvfs_cpu_disabled, 0644);
module_param_cb(disable_gpu, &tegra_dvfs_disable_gpu_ops,
	&tegra_dvfs_gpu_disabled, 0644);

static bool __init match_dvfs_one(const char *name,
	int dvfs_speedo_id, int dvfs_process_id,
	int speedo_id, int process_id)
{
	if ((dvfs_process_id != -1 && dvfs_process_id != process_id) ||
		(dvfs_speedo_id != -1 && dvfs_speedo_id != speedo_id)) {
		pr_debug("tegra13_dvfs: rejected %s speedo %d, process %d\n",
			 name, dvfs_speedo_id, dvfs_process_id);
		return false;
	}
	return true;
}

static void __init init_alt_dvfs_one(struct dvfs *alt_d)
{
	int ret, i;
	struct clk *c = tegra_get_clock_by_name(alt_d->clk_name);

	if (!c || !c->dvfs) {
		pr_debug("tegra13_dvfs: invalid alt dvfs for %s\n",
			alt_d->clk_name);
		return;
	}

	if ((c->dvfs->speedo_id == alt_d->speedo_id) &&
	    (c->dvfs->process_id == alt_d->process_id)) {
		for (i = 0; i < c->dvfs->num_freqs; i++)
			alt_d->freqs[i] *= alt_d->freqs_mult;
		ret = tegra_dvfs_alt_freqs_install(c->dvfs, alt_d->freqs);
		if (ret)
			pr_err("tegra13_dvfs: failed install alt dvfs on %s\n",
			       c->name);
	}
}

/* cvb_mv = ((c2 * speedo / s_scale + c1) * speedo / s_scale + c0) / v_scale */
static inline int get_cvb_voltage(int speedo, int s_scale,
				  struct cvb_dvfs_parameters *cvb)
{
	/* apply only speedo scale: output mv = cvb_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c2 * speedo, s_scale);
	mv = DIV_ROUND_CLOSEST((mv + cvb->c1) * speedo, s_scale) + cvb->c0;
	return mv;
}

/* cvb_t_mv =
   ((c3 * speedo / s_scale + c4 + c5 * T / t_scale) * T / t_scale) / v_scale */
static inline int get_cvb_t_voltage(int speedo, int s_scale, int t, int t_scale,
				    struct cvb_dvfs_parameters *cvb)
{
	/* apply speedo & temperature scales: output mv = cvb_t_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c3 * speedo, s_scale) + cvb->c4 +
		DIV_ROUND_CLOSEST(cvb->c5 * t, t_scale);
	mv = DIV_ROUND_CLOSEST(mv * t, t_scale);
	return mv;
}

static int round_cvb_voltage(int mv, int v_scale, struct rail_alignment *align)
{
	/* combined: apply voltage scale and round to cvb alignment step */
	int uv;
	int step = (align->step_uv ? : 1000) * v_scale;
	int offset = align->offset_uv * v_scale;

	uv = max(mv * 1000, offset) - offset;
	uv = DIV_ROUND_UP(uv, step) * align->step_uv + align->offset_uv;
	return uv / 1000;
}

static int round_voltage(int mv, struct rail_alignment *align, bool up)
{
	if (align->step_uv) {
		int uv = max(mv * 1000, align->offset_uv) - align->offset_uv;
		uv = (uv + (up ? align->step_uv - 1 : 0)) / align->step_uv;
		return (uv * align->step_uv + align->offset_uv) / 1000;
	}
	return mv;
}

static void __init set_cpu_dfll_tuning_data(struct cpu_cvb_dvfs *d, int speedo)
{
	if (d->speedo_id >= 1) {
		/* Initial (low SiMon grade) settings - same for all parts */
		d->dfll_tune_data.tune0 = 0x8315FF;
		d->dfll_tune_data.tune0_high_mv = 0x8340FF;

		if (speedo <= 2180) {
			/* Toggle SiMOn mask - high grade settings 0x8A....*/
			d->dfll_tune_data.tune0_simon_mask = 0x090000;
			d->dfll_tune_data.tune_high_min_millivolts = 950;
		} else if (speedo < 2336) {
			/* Toggle SiMOn mask - high grade settings 0x87....*/
			d->dfll_tune_data.tune0_simon_mask = 0x040000;
			d->dfll_tune_data.tune_high_min_millivolts = 900;
		} else {
			/* High and low grade settings are the same */
			d->dfll_tune_data.tune_high_min_millivolts = 900;
		}
	}
}

static void __init set_cpu_dfll_vmin_data(
	struct cpu_cvb_dvfs *d, struct dvfs *cpu_dvfs,
	int speedo, struct rail_alignment *align)
{
	int mv, mvj, t, j;
	struct dvfs_rail *rail = &tegra13_dvfs_rail_vdd_cpu;

	/* First install fixed Vmin profile */
	tegra_dvfs_rail_init_vmin_thermal_profile(d->vmin_trips_table,
		d->therm_floors_table, rail, &d->dfll_tune_data);

	if (!rail->therm_mv_floors || !rail->therm_mv_floors_num ||
	    !d->cvb_vmin.cvb_dfll_param.c0)
		return;

	/*
	 * If fixed profile installed successfully, and speedo dependency is
	 * specified, calculate Vmin for each temperature range based on CVB
	 * equations for DFLL mode with the following restrictions:
	 * - keep fixed floor in lowest temperature range
	 * - apply Vmin at left (low) boundary trip-point to the entire range
	 * - don't allow Vmin below fixed floor
	 * - make sure calculated Vmin profile is descending with temperature
	 */
	mv = get_cvb_voltage(
		speedo, d->speedo_scale, &d->cvb_vmin.cvb_dfll_param);

	for (j = rail->therm_mv_floors_num - 1;; j--) {
		cpu_vmin[j] = d->therm_floors_table[j];
		if (j < rail->therm_mv_floors_num - 1)
			cpu_vmin[j] = max(cpu_vmin[j], cpu_vmin[j+1]);
		if (j == 0)
			break;

		/* add Vmin thermal offset */
		t = d->vmin_trips_table[j-1];
		mvj = mv + get_cvb_t_voltage(speedo, d->speedo_scale,
			t, d->thermal_scale, &d->cvb_vmin.cvb_dfll_param);
		mvj = round_cvb_voltage(mvj, d->voltage_scale, align);
		cpu_vmin[j] = max(cpu_vmin[j], mvj);
	}

	/*
	 * Overwrite fixed Vmin profile with CVB Vmin profile. Although CVB
	 * calculations are done using DFLL mode coefficients, resulting limits
	 * can be applied in PLL mode as well (with no actual limitations, since
	 * PLL mode Vmin requirements are higher, and embedded into PLL DVFS
	 * table)
	 */
	tegra_dvfs_rail_init_vmin_thermal_profile(d->vmin_trips_table,
		cpu_vmin, rail, &d->dfll_tune_data);
}


static int __init set_cpu_dvfs_data(unsigned long max_freq,
	struct cpu_cvb_dvfs *d, struct dvfs *cpu_dvfs, int *max_freq_index)
{
	int j, mv, dfll_mv, min_dfll_mv;
	unsigned long fmax_at_vmin = 0;
	unsigned long fmax_pll_mode = 0;
	unsigned long fmin_use_dfll = 0;
	struct cvb_dvfs_table *table = NULL;
	int speedo = tegra_cpu_speedo_value();
	struct dvfs_rail *rail = &tegra13_dvfs_rail_vdd_cpu;
	struct rail_alignment *align = &rail->alignment;

	set_cpu_dfll_tuning_data(d, speedo);

	min_dfll_mv = d->dfll_tune_data.min_millivolts;
	min_dfll_mv =  round_voltage(min_dfll_mv, align, true);
	d->max_mv = round_voltage(d->max_mv, align, false);
	BUG_ON(min_dfll_mv < rail->min_millivolts);

	/*
	 * Use CVB table to fill in CPU dvfs frequencies and voltages. Each
	 * CVB entry specifies CPU frequency and CVB coefficients to calculate
	 * the respective voltage when either DFLL or PLL is used as CPU clock
	 * source.
	 *
	 * Minimum voltage limit is applied only to DFLL source. For PLL source
	 * voltage can go as low as table specifies. Maximum voltage limit is
	 * applied to both sources, but differently: directly clip voltage for
	 * DFLL, and limit maximum frequency for PLL.
	 */
	for (j = 0; j < MAX_DVFS_FREQS; j++) {
		table = &d->cvb_table[j];
		if (!table->freq || (table->freq > max_freq))
			break;

		dfll_mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_dfll_param);
		dfll_mv = round_cvb_voltage(dfll_mv, d->voltage_scale, align);

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);
		mv = round_cvb_voltage(mv, d->voltage_scale, align);

		/*
		 * Check maximum frequency at minimum voltage for dfll source;
		 * round down unless all table entries are above Vmin, then use
		 * the 1st entry as is.
		 */
		dfll_mv = max(dfll_mv, min_dfll_mv);
		if (dfll_mv > min_dfll_mv) {
			if (!j)
				fmax_at_vmin = table->freq;
			if (!fmax_at_vmin)
				fmax_at_vmin = cpu_dvfs->freqs[j - 1];
		}

		/* Clip maximum frequency at maximum voltage for pll source */
		if (mv > d->max_mv) {
			if (!j)
				break;	/* 1st entry already above Vmax */
			if (!fmax_pll_mode)
				fmax_pll_mode = cpu_dvfs->freqs[j - 1];
		}

		/* Minimum rate with pll source voltage above dfll Vmin */
		if ((mv >= min_dfll_mv) && (!fmin_use_dfll))
			fmin_use_dfll = table->freq;

		/* fill in dvfs tables */
		cpu_dvfs->freqs[j] = table->freq;
		cpu_dfll_millivolts[j] = min(dfll_mv, d->max_mv);
		cpu_millivolts[j] = mv;
	}

	/* Table must not be empty, must have at least one entry above Vmin */
	if (!j || !fmax_at_vmin) {
		pr_err("tegra13_dvfs: invalid cpu dvfs table\n");
		return -ENOENT;
	}

	/* In the dfll operating range dfll voltage at any rate should be
	   better (below) than pll voltage */
	if (!fmin_use_dfll || (fmin_use_dfll > fmax_at_vmin)) {
		WARN(1, "tegra13_dvfs: pll voltage is below dfll in the dfll"
			" operating range\n");
		fmin_use_dfll = fmax_at_vmin;
	}

	/* dvfs tables are successfully populated - fill in the rest */
	cpu_dvfs->speedo_id = d->speedo_id;
	cpu_dvfs->process_id = d->process_id;
	cpu_dvfs->freqs_mult = d->freqs_mult;
	cpu_dvfs->dvfs_rail->nominal_millivolts = min(d->max_mv,
		max(cpu_millivolts[j - 1], cpu_dfll_millivolts[j - 1]));
	*max_freq_index = j - 1;

	cpu_dvfs->dfll_data = d->dfll_tune_data;
	cpu_dvfs->dfll_data.max_rate_boost = fmax_pll_mode ?
		(cpu_dvfs->freqs[j - 1] - fmax_pll_mode) * d->freqs_mult : 0;
	cpu_dvfs->dfll_data.out_rate_min = fmax_at_vmin * d->freqs_mult;
	cpu_dvfs->dfll_data.use_dfll_rate_min = fmin_use_dfll * d->freqs_mult;
	cpu_dvfs->dfll_data.min_millivolts = min_dfll_mv;
	cpu_dvfs->dfll_data.is_bypass_down = is_lp_cluster;

	/* Init cpu thermal floors */
	set_cpu_dfll_vmin_data(d, cpu_dvfs, speedo, align);

	/* Init cpu thermal caps */
#ifndef CONFIG_TEGRA_CPU_VOLT_CAP
	tegra_dvfs_rail_init_vmax_thermal_profile(
		vdd_cpu_vmax_trips_table, vdd_cpu_therm_caps_table,
		rail, &d->dfll_tune_data);

#endif
	if (cpu_dvfs->speedo_id == 0)
		return 0;

	/* Init cpu Vmin SiMon offsets */
	tegra_dvfs_rail_init_simon_vmin_offsets(cpu_vmin_offsets,
		ARRAY_SIZE(cpu_vmin_offsets), rail);

	/* check Vmin SiMon offset: ignore SiMon if it pushes too low */
	if (rail->therm_mv_floors && rail->simon_vmin_offsets) {
		mv = rail->therm_mv_floors[rail->therm_mv_floors_num - 1];
		mv += rail->simon_vmin_offsets[rail->simon_vmin_offs_num - 1];
		mv = round_voltage(mv, align, true);
		if (mv < min_dfll_mv) {
			WARN(1, "tegra13_dvfs: cpu simon min %dmV below dfll min %dmV\n",
			     mv, min_dfll_mv);
			rail->simon_vmin_offsets = NULL;
			rail->simon_vmin_offs_num = 0;
		}
	}

	return 0;
}

static int __init set_gpu_dvfs_data(unsigned long max_freq,
	struct gpu_cvb_dvfs *d, struct dvfs *gpu_dvfs, int *max_freq_index)
{
	int i, j, thermal_ranges, simon_offs, mv;
	struct cvb_dvfs_table *table = NULL;
	int speedo = tegra_gpu_speedo_value();
	struct dvfs_rail *rail = &tegra13_dvfs_rail_vdd_gpu;
	struct rail_alignment *align = &rail->alignment;

	d->max_mv = round_voltage(d->max_mv, align, false);

	/* Init gpu Vmin SiMon offsets (Tegra13 has exactly 2 offsests) */
	BUILD_BUG_ON(ARRAY_SIZE(gpu_vmin_offsets) != 2);
	tegra_dvfs_rail_init_simon_vmin_offsets(gpu_vmin_offsets, 2, rail);
	simon_offs = rail->simon_vmin_offsets ? rail->simon_vmin_offsets[1] : 0;

	/*
	 * Init thermal trips, find number of thermal ranges; note that the
	 * first trip-point is used for voltage calculations within the lowest
	 * range, but should not be actually set. Hence, at least 2 trip-points
	 * must be specified.
	 */
	if (tegra_dvfs_rail_init_thermal_dvfs_trips(d->vts_trips_table, rail))
		return -ENOENT;
	thermal_ranges = rail->vts_cdev->trip_temperatures_num;
	rail->vts_cdev->trip_temperatures_num--;

	if (thermal_ranges < 2)
		WARN(1, "tegra13_dvfs: %d gpu trip: thermal dvfs is broken\n",
		     thermal_ranges);

	/*
	 * Use CVB table to calculate Vmin for each temperature range
	 */
	mv = get_cvb_voltage(
		speedo, d->speedo_scale, &d->cvb_vmin.cvb_pll_param);
	for (j = 0; j < thermal_ranges; j++) {
		int mvj = mv;
		int t = rail->vts_cdev->trip_temperatures[j];

		/* add Vmin thermal offset for this trip-point */
		mvj += get_cvb_t_voltage(speedo, d->speedo_scale,
			t, d->thermal_scale, &d->cvb_vmin.cvb_pll_param);
		mvj = round_cvb_voltage(mvj, d->voltage_scale, align);
		if (mvj < rail->min_millivolts) {
			WARN(1, "tegra13_dvfs: gpu Vmin %d below rail min %d\n",
			     mvj, rail->min_millivolts);
			mvj = rail->min_millivolts;
		}

		/* check Vmin SiMon offset: ignore SiMon if it pushes too low */
		if (mvj + simon_offs < rail->min_millivolts) {
			WARN(1, "tegra13_dvfs: gpu simon min %dmV below rail min %dmV\n",
			     mvj + simon_offs, rail->min_millivolts);
			rail->simon_vmin_offsets = NULL;
			rail->simon_vmin_offs_num = 0;
			simon_offs = 0;
		}

		gpu_vmin[j] = mvj;
	}

	/*
	 * Use CVB table to fill in gpu dvfs frequencies and voltages. Each
	 * CVB entry specifies gpu frequency and CVB coefficients to calculate
	 * the respective voltage.
	 */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		table = &d->cvb_table[i];
		if (!table->freq || (table->freq > max_freq))
			break;

		mv = get_cvb_voltage(
			speedo, d->speedo_scale, &table->cvb_pll_param);

		for (j = 0; j < thermal_ranges; j++) {
			int mvj_offs, mvj = mv;
			int t = rail->vts_cdev->trip_temperatures[j];

			/* get thermal offset for this trip-point */
			mvj += get_cvb_t_voltage(speedo, d->speedo_scale,
				t, d->thermal_scale, &table->cvb_pll_param);
			mvj = round_cvb_voltage(mvj, d->voltage_scale, align);

			/* clip to minimum, abort if above maximum */
			mvj_offs = max(mvj, gpu_vmin[j] + simon_offs);
			mvj = max(mvj, gpu_vmin[j]);
			if (mvj > d->max_mv)
				break;

			/* update voltage for adjacent ranges bounded by this
			   trip-point (cvb & dvfs are transpose matrices) */
			gpu_millivolts[j][i] = mvj;
			if (j && (gpu_millivolts[j-1][i] < mvj))
				gpu_millivolts[j-1][i] = mvj;

			gpu_millivolts_offs[j][i] = mvj_offs;
			if (j && (gpu_millivolts_offs[j-1][i] < mvj_offs))
				gpu_millivolts_offs[j-1][i] = mvj_offs;
		}
		/* Make sure all voltages for this frequency are below max */
		if (j < thermal_ranges)
			break;

		/* fill in gpu dvfs tables */
		gpu_dvfs->freqs[i] = table->freq;
	}

	/*
	 * Table must not be empty, must have at least one entry in range, and
	 * must specify monotonically increasing voltage on frequency dependency
	 * in each temperature range.
	 */
	if (!i || tegra_dvfs_init_thermal_dvfs_voltages(&gpu_millivolts[0][0],
		gpu_peak_millivolts, i, thermal_ranges, gpu_dvfs)) {
		pr_err("tegra13_dvfs: invalid gpu dvfs table\n");
		return -ENOENT;
	}

	/* Shift out the 1st trip-point */
	for (j = 1; j < thermal_ranges; j++)
		rail->vts_cdev->trip_temperatures[j - 1] =
		rail->vts_cdev->trip_temperatures[j];

	/* dvfs tables are successfully populated - fill in the gpu dvfs */
	gpu_dvfs->speedo_id = d->speedo_id;
	gpu_dvfs->process_id = d->process_id;
	gpu_dvfs->freqs_mult = d->freqs_mult;
	gpu_dvfs->dvfs_rail->nominal_millivolts = d->max_mv;

	*max_freq_index = i - 1;

	/* Init thermal floors */
	tegra_dvfs_rail_init_vmin_thermal_profile(d->vmin_trips_table,
		d->therm_floors_table, &tegra13_dvfs_rail_vdd_gpu, NULL);

	return 0;
}

static int gpu_simon_grade_notify_cb(struct notifier_block *nb,
				     unsigned long grade, void *v)
{
	struct dvfs_rail *rail = &tegra13_dvfs_rail_vdd_gpu;
	int curr_domain = (int)((long)v);
	int ret;

	if (curr_domain != rail->simon_domain)
		return NOTIFY_DONE;

	/* Only 2 grades are supported; both voltage tables must be valid */
	ret = tegra_dvfs_replace_voltage_table(&gpu_dvfs,
		grade ? &gpu_millivolts_offs[0][0] : &gpu_millivolts[0][0]);

	if (!WARN_ON(ret == -EINVAL))
		pr_info("tegra_dvfs: set %s simon grade %lu\n",
			rail->reg_id, grade);

	return NOTIFY_OK;
};

static int __init tegra13_register_gpu_simon_notifier(void)
{
	int ret;
	struct dvfs_rail *rail = &tegra13_dvfs_rail_vdd_gpu;

	/* Stay at default if no simon offsets or thermal dvfs is broken */
	if (!gpu_dvfs.therm_dvfs || !rail->simon_vmin_offsets)
		return 0;

	gpu_simon_grade_nb.notifier_call = gpu_simon_grade_notify_cb;

	ret = tegra_register_simon_notifier(&gpu_simon_grade_nb);
	if (ret) {
		pr_err("tegra13_dvfs: failed to register %s simon notifier\n",
		       rail->reg_id);
		return ret;
	}

	pr_info("tegra dvfs: registered %s simon notifier\n", rail->reg_id);
	return 0;
}
late_initcall(tegra13_register_gpu_simon_notifier);

static int __init get_core_nominal_mv_index(int speedo_id)
{
	int i;
	int mv = tegra_core_speedo_mv();
	int core_edp_voltage = get_core_edp();

	/*
	 * Start with nominal level for the chips with this speedo_id. Then,
	 * make sure core nominal voltage is below edp limit for the board
	 * (if edp limit is set).
	 */
	if (!core_edp_voltage)
		core_edp_voltage = 1150;	/* default 1.15V EDP limit */

	mv = min(mv, core_edp_voltage);

	/* Round nominal level down to the nearest core scaling step */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		if ((core_millivolts[i] == 0) || (mv < core_millivolts[i]))
			break;
	}

	if (i == 0) {
		pr_err("tegra13_dvfs: unable to adjust core dvfs table to"
		       " nominal voltage %d\n", mv);
		return -ENOSYS;
	}
	return i - 1;
}

#define INIT_CORE_DVFS_TABLE(table, table_size)				       \
	do {								       \
		for (i = 0; i < (table_size); i++) {			       \
			struct dvfs *d = &(table)[i];			       \
			if (!match_dvfs_one(d->clk_name, d->speedo_id,	       \
				d->process_id, soc_speedo_id, core_process_id))\
				continue;				       \
			tegra_init_dvfs_one(d, core_nominal_mv_index);	       \
		}							       \
	} while (0)

void __init tegra13x_init_dvfs(void)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int cpu_process_id = tegra_cpu_process_id();
	int soc_speedo_id = tegra_soc_speedo_id();
	int core_process_id = tegra_core_process_id();
	int gpu_speedo_id = tegra_gpu_speedo_id();
	int gpu_process_id = tegra_gpu_process_id();

	int i, ret;
	int core_nominal_mv_index;
	int gpu_max_freq_index = 0;
	int cpu_max_freq_index = 0;

#ifndef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_core_disabled = true;
#endif
#ifndef CONFIG_TEGRA_CPU_DVFS
	tegra_dvfs_cpu_disabled = true;
#endif
#ifndef CONFIG_TEGRA_GPU_DVFS
	tegra_dvfs_gpu_disabled = true;
#endif

	/*
	 * Find nominal voltages for core (1st) and cpu rails before rail
	 * init. Nominal voltage index in core scaling ladder can also be
	 * used to determine max dvfs frequencies for all core clocks. In
	 * case of error disable core scaling and set index to 0, so that
	 * core clocks would not exceed rates allowed at minimum voltage.
	 */
	core_nominal_mv_index = get_core_nominal_mv_index(soc_speedo_id);
	if (core_nominal_mv_index < 0) {
		tegra13_dvfs_rail_vdd_core.disabled = true;
		tegra_dvfs_core_disabled = true;
		core_nominal_mv_index = 0;
	}
	tegra13_dvfs_rail_vdd_core.nominal_millivolts =
		core_millivolts[core_nominal_mv_index];

	tegra13_dvfs_rail_vdd_core.resolve_override = resolve_core_override;

	/*
	 * Setup cpu dvfs and dfll tables from cvb data, determine nominal
	 * voltage for cpu rail, and cpu maximum frequency. Note that entire
	 * frequency range is guaranteed only when dfll is used as cpu clock
	 * source. Reaching maximum frequency with pll as cpu clock source
	 * may not be possible within nominal voltage range (dvfs mechanism
	 * would automatically fail frequency request in this case, so that
	 * voltage limit is not violated). Error when cpu dvfs table can not
	 * be constructed must never happen.
	 */
	BUG_ON(cpu_speedo_id >= ARRAY_SIZE(cpu_max_freq));
	for (ret = 0, i = 0; i <  ARRAY_SIZE(cpu_cvb_dvfs_table); i++) {
		struct cpu_cvb_dvfs *d = &cpu_cvb_dvfs_table[i];
		unsigned long max_freq = cpu_max_freq[cpu_speedo_id];
		if (match_dvfs_one("cpu cvb", d->speedo_id, d->process_id,
				   cpu_speedo_id, cpu_process_id)) {
			ret = set_cpu_dvfs_data(max_freq,
				d, &cpu_dvfs, &cpu_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(cpu_cvb_dvfs_table)) || ret);

	/*
	 * Setup gpu dvfs tables from cvb data, determine nominal voltage for
	 * gpu rail, and gpu maximum frequency. Error when gpu dvfs table can
	 * not be constructed must never happen.
	 */
	BUG_ON(gpu_speedo_id >= ARRAY_SIZE(gpu_max_freq));
	for (ret = 0, i = 0; i < ARRAY_SIZE(gpu_cvb_dvfs_table); i++) {
		struct gpu_cvb_dvfs *d = &gpu_cvb_dvfs_table[i];
		unsigned long max_freq = gpu_max_freq[gpu_speedo_id];
		if (match_dvfs_one("gpu cvb", d->speedo_id, d->process_id,
				   gpu_speedo_id, gpu_process_id)) {
			ret = set_gpu_dvfs_data(max_freq,
				d, &gpu_dvfs, &gpu_max_freq_index);
			break;
		}
	}
	BUG_ON((i == ARRAY_SIZE(gpu_cvb_dvfs_table)) || ret);

	/* Init core thermal profile */
	tegra_dvfs_rail_init_vmin_thermal_profile(vdd_core_vmin_trips_table,
		vdd_core_therm_floors_table, &tegra13_dvfs_rail_vdd_core, NULL);
	tegra_dvfs_rail_init_vmax_thermal_profile(vdd_core_vmax_trips_table,
		vdd_core_therm_caps_table, &tegra13_dvfs_rail_vdd_core, NULL);

	/* Init rail structures and dependencies */
	tegra_dvfs_init_rails(tegra13_dvfs_rails,
		ARRAY_SIZE(tegra13_dvfs_rails));
	if ((tegra_revision == TEGRA_REVISION_A01) ||
	    (tegra_revision == TEGRA_REVISION_A02))
		tegra_dvfs_add_relationships(tegra13_dvfs_relationships,
			ARRAY_SIZE(tegra13_dvfs_relationships));

	/* Search core dvfs table for speedo/process matching entries and
	   initialize dvfs-ed clocks */
	if (!tegra_platform_is_linsim()) {
		INIT_CORE_DVFS_TABLE(core_dvfs_table,
				     ARRAY_SIZE(core_dvfs_table));
		INIT_CORE_DVFS_TABLE(disp_dvfs_table,
				     ARRAY_SIZE(disp_dvfs_table));

		for (i = 0; i <  ARRAY_SIZE(disp_alt_dvfs_table); i++)
			init_alt_dvfs_one(&disp_alt_dvfs_table[i]);
	}

	/* Initialize matching gpu dvfs entry already found when nominal
	   voltage was determined */
	tegra_init_dvfs_one(&gpu_dvfs, gpu_max_freq_index);

	/* Initialize matching cpu dvfs entry already found when nominal
	   voltage was determined */
	tegra_init_dvfs_one(&cpu_dvfs, cpu_max_freq_index);

	/* Finally disable dvfs on rails if necessary */
	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_core);
	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_cpu);
	if (tegra_dvfs_gpu_disabled)
		tegra_dvfs_rail_disable(&tegra13_dvfs_rail_vdd_gpu);

	pr_info("tegra dvfs: VDD_CPU nominal %dmV, scaling %s\n",
		tegra13_dvfs_rail_vdd_cpu.nominal_millivolts,
		tegra_dvfs_cpu_disabled ? "disabled" : "enabled");
	pr_info("tegra dvfs: VDD_CORE nominal %dmV, scaling %s\n",
		tegra13_dvfs_rail_vdd_core.nominal_millivolts,
		tegra_dvfs_core_disabled ? "disabled" : "enabled");
	pr_info("tegra dvfs: VDD_GPU nominal %dmV, scaling %s\n",
		tegra13_dvfs_rail_vdd_gpu.nominal_millivolts,
		tegra_dvfs_gpu_disabled ? "disabled" : "enabled");
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
	return 0;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
	return 0;
}

#ifdef CONFIG_TEGRA_CORE_VOLT_CAP
/* Core voltage and bus cap object and tables */
static struct kobject *cap_kobj;
static struct kobject *gpu_kobj;
static struct kobject *emc_kobj;

static struct core_dvfs_cap_table tegra13_core_cap_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ .cap_name = "cap.vcore.c2bus" },
	{ .cap_name = "cap.vcore.c3bus" },
#else
	{ .cap_name = "cap.vcore.cbus" },
#endif
	{ .cap_name = "cap.vcore.sclk" },
	{ .cap_name = "cap.vcore.emc" },
	{ .cap_name = "cap.vcore.host1x" },
	{ .cap_name = "cap.vcore.mselect" },
};

static struct core_bus_limit_table tegra13_gpu_cap_syfs = {
	.limit_clk_name = "cap.profile.gbus",
	.refcnt_attr = {.attr = {.name = "gpu_cap_state", .mode = 0644} },
	.level_attr  = {.attr = {.name = "gpu_cap_rate", .mode = 0644} },
	.pm_qos_class = PM_QOS_GPU_FREQ_MAX,
};

static struct core_bus_limit_table tegra13_gpu_floor_sysfs = {
	.limit_clk_name = "floor.profile.gbus",
	.refcnt_attr = {.attr = {.name = "gpu_floor_state", .mode = 0644} },
	.level_attr  = {.attr = {.name = "gpu_floor_rate", .mode = 0644} },
	.pm_qos_class = PM_QOS_GPU_FREQ_MIN,
};

static struct core_bus_rates_table tegra13_gpu_rates_sysfs = {
	.bus_clk_name = "gbus",
	.rate_attr = {.attr = {.name = "gpu_rate", .mode = 0444} },
	.available_rates_attr = {
		.attr = {.name = "gpu_available_rates", .mode = 0444} },
};

static struct core_bus_rates_table tegra13_emc_rates_sysfs = {
	.bus_clk_name = "emc",
	.rate_attr = {.attr = {.name = "emc_rate", .mode = 0444} },
	.available_rates_attr = {
		.attr = {.name = "emc_available_rates", .mode = 0444} },
};

static int __init tegra13_dvfs_init_core_cap(void)
{
	int ret;
	const int hack_core_millivolts = 0;

	cap_kobj = kobject_create_and_add("tegra_cap", kernel_kobj);
	if (!cap_kobj) {
		pr_err("tegra13_dvfs: failed to create sysfs cap object\n");
		return 0;
	}

	/* FIXME: skip core cap init b/c it's too slow on QT */
	if (tegra_platform_is_qt())
		ret = tegra_init_core_cap(
			tegra13_core_cap_table, ARRAY_SIZE(tegra13_core_cap_table),
			&hack_core_millivolts, 1, cap_kobj);
	else
		ret = tegra_init_core_cap(
			tegra13_core_cap_table, ARRAY_SIZE(tegra13_core_cap_table),
			core_millivolts, ARRAY_SIZE(core_millivolts), cap_kobj);

	if (ret) {
		pr_err("tegra13_dvfs: failed to init core cap interface (%d)\n",
		       ret);
		kobject_del(cap_kobj);
		return 0;
	}

	/* core cap must be initialized for vmax cdev operations */
	tegra13_dvfs_rail_vdd_core.apply_vmax_cap =
		tegra_dvfs_therm_vmax_core_cap_apply;
	tegra_dvfs_rail_register_vmax_cdev(&tegra13_dvfs_rail_vdd_core);

	tegra_core_cap_debug_init();
	pr_info("tegra dvfs: tegra sysfs cap interface is initialized\n");

	gpu_kobj = kobject_create_and_add("tegra_gpu", kernel_kobj);
	if (!gpu_kobj) {
		pr_err("tegra13_dvfs: failed to create sysfs gpu object\n");
		return 0;
	}

	ret = tegra_init_shared_bus_cap(&tegra13_gpu_cap_syfs,
					1, gpu_kobj);
	if (ret) {
		pr_err("tegra13_dvfs: failed to init gpu cap interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	ret = tegra_init_shared_bus_floor(&tegra13_gpu_floor_sysfs,
					  1, gpu_kobj);
	if (ret) {
		pr_err("tegra13_dvfs: failed to init gpu floor interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	ret = tegra_init_sysfs_shared_bus_rate(&tegra13_gpu_rates_sysfs,
					       1, gpu_kobj);
	if (ret) {
		pr_err("tegra13_dvfs: failed to init gpu rates interface (%d)\n",
		       ret);
		kobject_del(gpu_kobj);
		return 0;
	}

	emc_kobj = kobject_create_and_add("tegra_emc", kernel_kobj);
	if (!emc_kobj) {
		pr_err("tegra13_dvfs: failed to create sysfs emc object\n");
		return 0;
	}

	ret = tegra_init_sysfs_shared_bus_rate(&tegra13_emc_rates_sysfs,
					       1, emc_kobj);
	if (ret) {
		pr_err("tegra13_dvfs: failed to init emc rates interface (%d)\n",
		       ret);
		kobject_del(emc_kobj);
		return 0;
	}
	pr_info("tegra dvfs: tegra sysfs gpu & emc interface is initialized\n");

	return 0;
}
late_initcall(tegra13_dvfs_init_core_cap);
#endif
