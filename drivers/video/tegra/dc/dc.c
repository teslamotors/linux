/*
 * drivers/video/tegra/dc/dc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/tegra-pm.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <linux/gpio.h>
#include <linux/nvhost.h>
#include <linux/clk/tegra.h>
#include <video/tegrafb.h>
#include <drm/drm_fixed.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/tegra_pm_domains.h>
#include <linux/uaccess.h>
#include <linux/ote_protocol.h>
#include <linux/tegra-timer.h>

#define CREATE_TRACE_POINTS
#include <trace/events/display.h>
EXPORT_TRACEPOINT_SYMBOL(display_writel);
EXPORT_TRACEPOINT_SYMBOL(display_readl);

#include <mach/dc.h>
#include <mach/fb.h>
#include <linux/nvhost.h>
#include <linux/nvhost_ioctl.h>

#include <linux/platform/tegra/latency_allowance.h>
#include <linux/platform/tegra/mc.h>
#include <soc/tegra/tegra_bpmp.h>

#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"
#include "dc_shared_isr.h"
#include "dev.h"
#include "nvhost_sync.h"
#include "nvsd.h"
#include "nvsd2.h"
#include "dpaux.h"
#include "nvsr.h"

#ifdef CONFIG_ADF_TEGRA
#include "tegra_adf.h"
#endif

#include "edid.h"

#ifdef CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT
#include "fake_panel.h"
#include "null_or.h"
#endif /*CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT*/


/* HACK! This needs to come from DT */
#include "../../../../arch/arm/mach-tegra/iomap.h"

#define TEGRA_CRC_LATCHED_DELAY		34

#define DC_COM_PIN_OUTPUT_POLARITY1_INIT_VAL	0x01000000
#define DC_COM_PIN_OUTPUT_POLARITY3_INIT_VAL	0x0

#define MAX_VRR_V_FRONT_PORCH			0x1000

#ifndef CONFIG_TEGRA_NVDISPLAY
static struct of_device_id tegra_disa_pd[] = {
	{ .compatible = "nvidia,tegra186-disa-pd", },
	{ .compatible = "nvidia,tegra210-disa-pd", },
	{ .compatible = "nvidia,tegra132-disa-pd", },
	{},
};

static struct of_device_id tegra_disb_pd[] = {
	{ .compatible = "nvidia,tegra186-disb-pd", },
	{ .compatible = "nvidia,tegra210-disb-pd", },
	{ .compatible = "nvidia,tegra132-disb-pd", },
	{},
};
#endif

struct fb_videomode tegra_dc_vga_mode = {
	.refresh = 60,
	.xres = 640,
	.yres = 480,
	.pixclock = KHZ2PICOS(25200),
	.hsync_len = 96,	/* h_sync_width */
	.vsync_len = 2,		/* v_sync_width */
	.left_margin = 48,	/* h_back_porch */
	.upper_margin = 33,	/* v_back_porch */
	.right_margin = 16,	/* h_front_porch */
	.lower_margin = 10,	/* v_front_porch */
	.vmode = 0,
	.sync = 0,
};

/* needs to be big enough to be index by largest supported out->type */
static struct tegra_dc_mode override_disp_mode[TEGRA_DC_OUT_NULL + 1];

static void _tegra_dc_controller_disable(struct tegra_dc *dc);
static void tegra_dc_disable_irq_ops(struct tegra_dc *dc, bool from_irq);
static void tegra_dc_sor_instance(struct tegra_dc *dc, int out_type);
static int _tegra_dc_config_frame_end_intr(struct tegra_dc *dc, bool enable);

static int tegra_dc_set_out(struct tegra_dc *dc, struct tegra_dc_out *out);
#ifdef PM
static int tegra_dc_suspend(struct platform_device *ndev, pm_message_t state);
static int tegra_dc_resume(struct platform_device *ndev);
#endif

static struct tegra_dc *tegra_dcs[TEGRA_MAX_DC];

#ifdef CONFIG_TEGRA_NVDISPLAY
static struct tegra_dc_win	tegra_dc_windows[DC_N_WINDOWS];
#endif


static DEFINE_MUTEX(tegra_dc_lock);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static DEFINE_MUTEX(shared_lock);
#endif

static struct device_dma_parameters tegra_dc_dma_parameters = {
	.max_segment_size = UINT_MAX,
};

static const struct {
	bool h;
	bool v;
} can_filter[] = {
	/* Window A has no filtering */
	{ false, false },
	/* Window B has both H and V filtering */
	{ true,  true  },
	/* Window C has only H filtering */
	{ false, true  },
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu default_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x100, 0x0,   0x0,
		0x0,   0x100, 0x0,
		0x0,   0x0,   0x100,
	},
	/* lut2 maps linear space to sRGB*/
	{
		0,    1,    2,    2,    3,    4,    5,    6,
		6,    7,    8,    9,    10,   10,   11,   12,
		13,   13,   14,   15,   15,   16,   16,   17,
		18,   18,   19,   19,   20,   20,   21,   21,
		22,   22,   23,   23,   23,   24,   24,   25,
		25,   25,   26,   26,   27,   27,   27,   28,
		28,   29,   29,   29,   30,   30,   30,   31,
		31,   31,   32,   32,   32,   33,   33,   33,
		34,   34,   34,   34,   35,   35,   35,   36,
		36,   36,   37,   37,   37,   37,   38,   38,
		38,   38,   39,   39,   39,   40,   40,   40,
		40,   41,   41,   41,   41,   42,   42,   42,
		42,   43,   43,   43,   43,   43,   44,   44,
		44,   44,   45,   45,   45,   45,   46,   46,
		46,   46,   46,   47,   47,   47,   47,   48,
		48,   48,   48,   48,   49,   49,   49,   49,
		49,   50,   50,   50,   50,   50,   51,   51,
		51,   51,   51,   52,   52,   52,   52,   52,
		53,   53,   53,   53,   53,   54,   54,   54,
		54,   54,   55,   55,   55,   55,   55,   55,
		56,   56,   56,   56,   56,   57,   57,   57,
		57,   57,   57,   58,   58,   58,   58,   58,
		58,   59,   59,   59,   59,   59,   59,   60,
		60,   60,   60,   60,   60,   61,   61,   61,
		61,   61,   61,   62,   62,   62,   62,   62,
		62,   63,   63,   63,   63,   63,   63,   64,
		64,   64,   64,   64,   64,   64,   65,   65,
		65,   65,   65,   65,   66,   66,   66,   66,
		66,   66,   66,   67,   67,   67,   67,   67,
		67,   67,   68,   68,   68,   68,   68,   68,
		68,   69,   69,   69,   69,   69,   69,   69,
		70,   70,   70,   70,   70,   70,   70,   71,
		71,   71,   71,   71,   71,   71,   72,   72,
		72,   72,   72,   72,   72,   72,   73,   73,
		73,   73,   73,   73,   73,   74,   74,   74,
		74,   74,   74,   74,   74,   75,   75,   75,
		75,   75,   75,   75,   75,   76,   76,   76,
		76,   76,   76,   76,   77,   77,   77,   77,
		77,   77,   77,   77,   78,   78,   78,   78,
		78,   78,   78,   78,   78,   79,   79,   79,
		79,   79,   79,   79,   79,   80,   80,   80,
		80,   80,   80,   80,   80,   81,   81,   81,
		81,   81,   81,   81,   81,   81,   82,   82,
		82,   82,   82,   82,   82,   82,   83,   83,
		83,   83,   83,   83,   83,   83,   83,   84,
		84,   84,   84,   84,   84,   84,   84,   84,
		85,   85,   85,   85,   85,   85,   85,   85,
		85,   86,   86,   86,   86,   86,   86,   86,
		86,   86,   87,   87,   87,   87,   87,   87,
		87,   87,   87,   88,   88,   88,   88,   88,
		88,   88,   88,   88,   88,   89,   89,   89,
		89,   89,   89,   89,   89,   89,   90,   90,
		90,   90,   90,   90,   90,   90,   90,   90,
		91,   91,   91,   91,   91,   91,   91,   91,
		91,   91,   92,   92,   92,   92,   92,   92,
		92,   92,   92,   92,   93,   93,   93,   93,
		93,   93,   93,   93,   93,   93,   94,   94,
		94,   94,   94,   94,   94,   94,   94,   94,
		95,   95,   95,   95,   95,   95,   95,   95,
		95,   95,   96,   96,   96,   96,   96,   96,
		96,   96,   96,   96,   96,   97,   97,   97,
		97,   97,   97,   97,   97,   97,   97,   98,
		98,   98,   98,   98,   98,   98,   98,   98,
		98,   98,   99,   99,   99,   99,   99,   99,
		99,   100,  101,  101,  102,  103,  103,  104,
		105,  105,  106,  107,  107,  108,  109,  109,
		110,  111,  111,  112,  113,  113,  114,  115,
		115,  116,  116,  117,  118,  118,  119,  119,
		120,  120,  121,  122,  122,  123,  123,  124,
		124,  125,  126,  126,  127,  127,  128,  128,
		129,  129,  130,  130,  131,  131,  132,  132,
		133,  133,  134,  134,  135,  135,  136,  136,
		137,  137,  138,  138,  139,  139,  140,  140,
		141,  141,  142,  142,  143,  143,  144,  144,
		145,  145,  145,  146,  146,  147,  147,  148,
		148,  149,  149,  150,  150,  150,  151,  151,
		152,  152,  153,  153,  153,  154,  154,  155,
		155,  156,  156,  156,  157,  157,  158,  158,
		158,  159,  159,  160,  160,  160,  161,  161,
		162,  162,  162,  163,  163,  164,  164,  164,
		165,  165,  166,  166,  166,  167,  167,  167,
		168,  168,  169,  169,  169,  170,  170,  170,
		171,  171,  172,  172,  172,  173,  173,  173,
		174,  174,  174,  175,  175,  176,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		179,  180,  180,  180,  181,  181,  182,  182,
		182,  183,  183,  183,  184,  184,  184,  185,
		185,  185,  186,  186,  186,  187,  187,  187,
		188,  188,  188,  189,  189,  189,  189,  190,
		190,  190,  191,  191,  191,  192,  192,  192,
		193,  193,  193,  194,  194,  194,  195,  195,
		195,  196,  196,  196,  196,  197,  197,  197,
		198,  198,  198,  199,  199,  199,  200,  200,
		200,  200,  201,  201,  201,  202,  202,  202,
		202,  203,  203,  203,  204,  204,  204,  205,
		205,  205,  205,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  211,  211,  211,  211,
		212,  212,  212,  213,  213,  213,  213,  214,
		214,  214,  214,  215,  215,  215,  216,  216,
		216,  216,  217,  217,  217,  217,  218,  218,
		218,  219,  219,  219,  219,  220,  220,  220,
		220,  221,  221,  221,  221,  222,  222,  222,
		223,  223,  223,  223,  224,  224,  224,  224,
		225,  225,  225,  225,  226,  226,  226,  226,
		227,  227,  227,  227,  228,  228,  228,  228,
		229,  229,  229,  229,  230,  230,  230,  230,
		231,  231,  231,  231,  232,  232,  232,  232,
		233,  233,  233,  233,  234,  234,  234,  234,
		235,  235,  235,  235,  236,  236,  236,  236,
		237,  237,  237,  237,  238,  238,  238,  238,
		239,  239,  239,  239,  240,  240,  240,  240,
		240,  241,  241,  241,  241,  242,  242,  242,
		242,  243,  243,  243,  243,  244,  244,  244,
		244,  244,  245,  245,  245,  245,  246,  246,
		246,  246,  247,  247,  247,  247,  247,  248,
		248,  248,  248,  249,  249,  249,  249,  249,
		250,  250,  250,  250,  251,  251,  251,  251,
		251,  252,  252,  252,  252,  253,  253,  253,
		253,  253,  254,  254,  254,  254,  255,  255,
	},
};

static struct tegra_dc_cmu default_limited_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x100, 0x000, 0x000,
		0x000, 0x100, 0x000,
		0x000, 0x000, 0x100,
	},
	/*
	 * lut2 maps linear space back to sRGB, where
	 * the output range is [16...235] (limited).
	 */
	{
		16,  17,  18,  18,  19,  19,  20,  21,
		21,  22,  23,  24,  25,  25,  25,  26,
		27,  27,  28,  28,  29,  30,  30,  31,
		31,  31,  31,  32,  32,  33,  33,  34,
		34,  35,  35,  36,  36,  37,  37,  37,
		37,  37,  38,  38,  38,  39,  39,  39,
		40,  40,  41,  41,  41,  42,  42,  42,
		43,  43,  43,  43,  43,  43,  44,  44,
		44,  44,  45,  45,  45,  46,  46,  46,
		47,  47,  47,  47,  48,  48,  48,  49,
		49,  49,  49,  49,  49,  49,  49,  50,
		50,  50,  50,  51,  51,  51,  51,  52,
		52,  52,  52,  53,  53,  53,  53,  54,
		54,  54,  54,  55,  55,  55,  55,  55,
		56,  56,  56,  56,  56,  56,  56,  56,
		56,  57,  57,  57,  57,  57,  58,  58,
		58,  58,  58,  59,  59,  59,  59,  59,
		60,  60,  60,  60,  60,  61,  61,  61,
		61,  61,  62,  62,  62,  62,  62,  62,
		62,  62,  62,  62,  63,  63,  63,  63,
		63,  63,  64,  64,  64,  64,  64,  64,
		65,  65,  65,  65,  65,  66,  66,  66,
		66,  66,  66,  67,  67,  67,  67,  67,
		67,  68,  68,  68,  68,  68,  68,  68,
		68,  68,  68,  68,  68,  69,  69,  69,
		69,  69,  69,  69,  70,  70,  70,  70,
		70,  70,  71,  71,  71,  71,  71,  71,
		72,  72,  72,  72,  72,  72,  72,  73,
		73,  73,  73,  73,  73,  73,  74,  74,
		74,  74,  74,  74,  74,  74,  74,  74,
		74,  74,  74,  74,  75,  75,  75,  75,
		75,  75,  75,  76,  76,  76,  76,  76,
		76,  76,  77,  77,  77,  77,  77,  77,
		77,  78,  78,  78,  78,  78,  78,  78,
		78,  79,  79,  79,  79,  79,  79,  79,
		80,  80,  80,  80,  80,  80,  80,  80,
		80,  80,  80,  80,  80,  80,  80,  80,
		81,  81,  81,  81,  81,  81,  81,  81,
		82,  82,  82,  82,  82,  82,  82,  82,
		83,  83,  83,  83,  83,  83,  83,  83,
		84,  84,  84,  84,  84,  84,  84,  84,
		84,  85,  85,  85,  85,  85,  85,  85,
		85,  86,  86,  86,  86,  86,  86,  86,
		86,  86,  86,  86,  86,  86,  86,  86,
		86,  86,  87,  87,  87,  87,  87,  87,
		87,  87,  87,  88,  88,  88,  88,  88,
		88,  88,  88,  88,  89,  89,  89,  89,
		89,  89,  89,  89,  89,  90,  90,  90,
		90,  90,  90,  90,  90,  90,  91,  91,
		91,  91,  91,  91,  91,  91,  91,  91,
		92,  92,  92,  92,  92,  92,  92,  92,
		92,  92,  92,  92,  92,  92,  92,  92,
		92,  92,  92,  93,  93,  93,  93,  93,
		93,  93,  93,  93,  94,  94,  94,  94,
		94,  94,  94,  94,  94,  94,  95,  95,
		95,  95,  95,  95,  95,  95,  95,  95,
		96,  96,  96,  96,  96,  96,  96,  96,
		96,  96,  97,  97,  97,  97,  97,  97,
		97,  97,  97,  97,  97,  98,  98,  98,
		98,  98,  98,  98,  98,  98,  98,  98,
		98,  98,  98,  98,  98,  98,  98,  98,
		98,  98,  99,  99,  99,  99,  99,  99,
		99,  99,  99,  99, 100, 100, 100, 100,
		100, 100, 100, 100, 100, 100, 100, 101,
		101, 102, 103, 103, 104, 104, 105, 105,
		106, 107, 107, 108, 109, 109, 110, 110,
		110, 111, 111, 112, 113, 113, 114, 115,
		115, 116, 116, 116, 117, 117, 118, 118,
		119, 120, 120, 121, 121, 122, 122, 122,
		122, 123, 124, 124, 125, 125, 126, 126,
		127, 127, 128, 128, 129, 129, 129, 129,
		130, 130, 131, 131, 132, 132, 133, 133,
		134, 134, 135, 135, 135, 135, 136, 136,
		137, 137, 138, 138, 139, 139, 140, 140,
		141, 141, 141, 141, 141, 142, 142, 143,
		143, 144, 144, 144, 145, 145, 146, 146,
		147, 147, 147, 147, 147, 148, 148, 149,
		149, 149, 150, 150, 151, 151, 151, 152,
		152, 153, 153, 153, 153, 153, 154, 154,
		154, 155, 155, 156, 156, 156, 157, 157,
		158, 158, 158, 159, 159, 159, 159, 159,
		160, 160, 160, 161, 161, 162, 162, 162,
		163, 163, 163, 164, 164, 165, 165, 165,
		165, 165, 165, 166, 166, 166, 167, 167,
		167, 168, 168, 169, 169, 169, 170, 170,
		170, 171, 171, 171, 171, 171, 171, 172,
		172, 172, 173, 173, 173, 174, 174, 174,
		175, 175, 175, 176, 176, 176, 177, 177,
		177, 177, 177, 177, 178, 178, 178, 179,
		179, 179, 180, 180, 180, 181, 181, 181,
		181, 182, 182, 182, 183, 183, 183, 183,
		183, 183, 184, 184, 184, 185, 185, 185,
		185, 186, 186, 186, 187, 187, 187, 188,
		188, 188, 188, 189, 189, 189, 189, 189,
		189, 190, 190, 190, 190, 191, 191, 191,
		192, 192, 192, 193, 193, 193, 193, 194,
		194, 194, 195, 195, 195, 195, 195, 195,
		195, 196, 196, 196, 196, 197, 197, 197,
		197, 198, 198, 198, 199, 199, 199, 199,
		200, 200, 200, 201, 201, 201, 201, 202,
		202, 202, 202, 202, 202, 202, 203, 203,
		203, 203, 204, 204, 204, 204, 205, 205,
		205, 205, 206, 206, 206, 207, 207, 207,
		207, 208, 208, 208, 208, 208, 208, 208,
		208, 209, 209, 209, 209, 210, 210, 210,
		210, 211, 211, 211, 211, 212, 212, 212,
		212, 213, 213, 213, 213, 214, 214, 214,
		214, 214, 214, 214, 214, 215, 215, 215,
		215, 216, 216, 216, 216, 217, 217, 217,
		217, 218, 218, 218, 218, 219, 219, 219,
		219, 220, 220, 220, 220, 220, 220, 220,
		220, 221, 221, 221, 221, 221, 222, 222,
		222, 222, 223, 223, 223, 223, 224, 224,
		224, 224, 225, 225, 225, 225, 225, 226,
		226, 226, 226, 226, 226, 226, 226, 227,
		227, 227, 227, 227, 228, 228, 228, 228,
		229, 229, 229, 229, 230, 230, 230, 230,
		230, 231, 231, 231, 231, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 233, 233,
		233, 233, 233, 234, 234, 234, 234, 235
	},
};
#elif defined(CONFIG_TEGRA_DC_CMU_V2)
static struct tegra_dc_cmu default_cmu = {
	{},
};
static struct tegra_dc_cmu default_limited_cmu = {
	{},
};
#endif

#define DSC_MAX_RC_BUF_THRESH_REGS	4
static int dsc_rc_buf_thresh_regs[DSC_MAX_RC_BUF_THRESH_REGS] = {
	DC_COM_DSC_RC_BUF_THRESH_0,
	DC_COM_DSC_RC_BUF_THRESH_1,
	DC_COM_DSC_RC_BUF_THRESH_2,
	DC_COM_DSC_RC_BUF_THRESH_3,
};

/*
 * Always set the first two values to 0. This is to ensure that RC threshold
 * values are programmed in the correct registers.
 */
static int dsc_rc_buf_thresh[] = {
	0, 0, 14, 28, 42, 56, 70, 84, 98, 105, 112, 119, 121,
	123, 125, 126,
};

#define DSC_MAX_RC_RANGE_CFG_REGS	8
static int dsc_rc_range_config[DSC_MAX_RC_RANGE_CFG_REGS] = {
	DC_COM_DSC_RC_RANGE_CFG_0,
	DC_COM_DSC_RC_RANGE_CFG_1,
	DC_COM_DSC_RC_RANGE_CFG_2,
	DC_COM_DSC_RC_RANGE_CFG_3,
	DC_COM_DSC_RC_RANGE_CFG_4,
	DC_COM_DSC_RC_RANGE_CFG_5,
	DC_COM_DSC_RC_RANGE_CFG_6,
	DC_COM_DSC_RC_RANGE_CFG_7,
};

static int dsc_rc_ranges_8bpp_8bpc[16][3] = {
	{0, 4, 2},
	{0, 4, 0},
	{1, 5, 0},
	{1, 6, -2},
	{3, 7, -4},
	{3, 7, -6},
	{3, 7, -8},
	{3, 8, -8},
	{3, 9, -8},
	{3, 10, -10},
	{5, 11, -10},
	{5, 12, -12},
	{5, 13, -12},
	{7, 13, -12},
	{13, 15, -12},
	{0, 0, 0},
};
void tegra_dc_clk_enable(struct tegra_dc *dc)
{
	tegra_disp_clk_prepare_enable(dc->clk);
#ifdef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);
#endif
}

void tegra_dc_clk_disable(struct tegra_dc *dc)
{
	tegra_disp_clk_disable_unprepare(dc->clk);
#ifdef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_set_rate(dc->clk, 0);
#endif
}

static void tegra_dc_sor_instance(struct tegra_dc *dc, int out_type)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	/* check the dc_or_node to set the instance */
	/* Fake dp always take SOR0 */
	if ((!strcmp(dc_or_node_names[dc->ndev->id], "/host1x/sor")) ||
		(out_type == TEGRA_DC_OUT_FAKE_DP))
		dc->sor_instance = 0;
	else if (!strcmp(dc_or_node_names[dc->ndev->id], "/host1x/sor1"))
		dc->sor_instance = 1;
	else
		dc->sor_instance = -1;
#else
	dc->sor_instance = dc->ndev->id;

	if (out_type == TEGRA_DC_OUT_HDMI)
		dc->sor_instance = 1;

#endif
}

void tegra_dc_get(struct tegra_dc *dc)
{
	int enable_count = atomic_inc_return(&dc->enable_count);

	BUG_ON(enable_count  < 1);
	if (enable_count == 1) {
		tegra_dc_io_start(dc);

		/* extra reference to dc clk */
		tegra_disp_clk_prepare_enable(dc->clk);
	}
}
EXPORT_SYMBOL(tegra_dc_get);

void tegra_dc_put(struct tegra_dc *dc)
{
	if (WARN_ONCE(atomic_read(&dc->enable_count) == 0,
		"unbalanced clock calls"))
		return;
	if (atomic_dec_return(&dc->enable_count) == 0) {
		/* balance extra dc clk reference */
		tegra_disp_clk_disable_unprepare(dc->clk);

		tegra_dc_io_end(dc);
	}
}
EXPORT_SYMBOL(tegra_dc_put);

unsigned tegra_dc_out_flags_from_dev(struct device *dev)
{
	struct platform_device *ndev = NULL;
	struct tegra_dc *dc = NULL;

	if (dev)
		ndev = to_platform_device(dev);
	if (ndev)
		dc = platform_get_drvdata(ndev);
	if (dc)
		return dc->out->flags;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_out_flags_from_dev);

bool tegra_dc_initialized(struct device *dev)
{
	struct platform_device *ndev = NULL;
	struct tegra_dc *dc = NULL;

	if (dev)
		ndev = to_platform_device(dev);
	if (ndev)
		dc = platform_get_drvdata(ndev);
	if (dc)
		return dc->initialized;
	else
		return false;
}
EXPORT_SYMBOL(tegra_dc_initialized);

void tegra_dc_hold_dc_out(struct tegra_dc *dc)
{
	if (1 == atomic_inc_return(&dc->holding)) {
		tegra_dc_get(dc);
		if (dc->out_ops && dc->out_ops->hold)
			dc->out_ops->hold(dc);
	}
}

void tegra_dc_release_dc_out(struct tegra_dc *dc)
{
	if (0 == atomic_dec_return(&dc->holding)) {
		if (dc->out_ops && dc->out_ops->release)
			dc->out_ops->release(dc);
		tegra_dc_put(dc);
	}
}

#define DUMP_REG(a) do {			\
	snprintf(buff, sizeof(buff), "%-32s\t%03x\t%08lx\n",  \
		 #a, a, tegra_dc_readl(dc, a));		      \
	print(data, buff);				      \
	} while (0)

#ifndef CONFIG_TEGRA_NVDISPLAY
void reg_dump(struct tegra_dc *dc, void *data,
		       void (* print)(void *data, const char *str))
{
	int i;
	char buff[256];
	const char winname[] = "ABCDHT";
	/* for above, see also: DC_CMD_DISPLAY_WINDOW_HEADER and DC_N_WINDOWS */
	unsigned long cmd_state;

	/* If gated, quietly return. */
	if (!tegra_powergate_is_powered(dc->powergate_id))
		return;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	cmd_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, WRITE_MUX_ACTIVE | READ_MUX_ACTIVE,
		DC_CMD_STATE_ACCESS);

	DUMP_REG(DC_CMD_DISPLAY_COMMAND_OPTION0);
	DUMP_REG(DC_CMD_DISPLAY_COMMAND);
	DUMP_REG(DC_CMD_SIGNAL_RAISE);
	DUMP_REG(DC_CMD_INT_STATUS);
	DUMP_REG(DC_CMD_INT_MASK);
	DUMP_REG(DC_CMD_INT_ENABLE);
	DUMP_REG(DC_CMD_INT_TYPE);
	DUMP_REG(DC_CMD_INT_POLARITY);
	DUMP_REG(DC_CMD_SIGNAL_RAISE1);
	DUMP_REG(DC_CMD_SIGNAL_RAISE2);
	DUMP_REG(DC_CMD_SIGNAL_RAISE3);
	DUMP_REG(DC_CMD_STATE_ACCESS);
	DUMP_REG(DC_CMD_STATE_CONTROL);
	DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
	DUMP_REG(DC_CMD_REG_ACT_CONTROL);

	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS1);
	DUMP_REG(DC_DISP_DISP_WIN_OPTIONS);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY_TIMER);
	DUMP_REG(DC_DISP_DISP_TIMING_OPTIONS);
	DUMP_REG(DC_DISP_REF_TO_SYNC);
	DUMP_REG(DC_DISP_SYNC_WIDTH);
	DUMP_REG(DC_DISP_BACK_PORCH);
	DUMP_REG(DC_DISP_DISP_ACTIVE);
	DUMP_REG(DC_DISP_FRONT_PORCH);
	DUMP_REG(DC_DISP_H_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_D);
	DUMP_REG(DC_DISP_V_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE3_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE3_POSITION_A);
	DUMP_REG(DC_DISP_M0_CONTROL);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_DISP_DI_CONTROL);
	DUMP_REG(DC_DISP_PP_CONTROL);
	DUMP_REG(DC_DISP_PP_SELECT_A);
	DUMP_REG(DC_DISP_PP_SELECT_B);
	DUMP_REG(DC_DISP_PP_SELECT_C);
	DUMP_REG(DC_DISP_PP_SELECT_D);
	DUMP_REG(DC_DISP_DISP_CLOCK_CONTROL);
	DUMP_REG(DC_DISP_DISP_INTERFACE_CONTROL);
	DUMP_REG(DC_DISP_DISP_COLOR_CONTROL);
	DUMP_REG(DC_DISP_SHIFT_CLOCK_OPTIONS);
	DUMP_REG(DC_DISP_DATA_ENABLE_OPTIONS);
	DUMP_REG(DC_DISP_SERIAL_INTERFACE_OPTIONS);
	DUMP_REG(DC_DISP_LCD_SPI_OPTIONS);
#if !defined(CONFIG_TEGRA_DC_BLENDER_GEN2)
	DUMP_REG(DC_DISP_BORDER_COLOR);
#endif
	DUMP_REG(DC_DISP_COLOR_KEY0_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY0_UPPER);
	DUMP_REG(DC_DISP_COLOR_KEY1_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY1_UPPER);
	DUMP_REG(DC_DISP_CURSOR_FOREGROUND);
	DUMP_REG(DC_DISP_CURSOR_BACKGROUND);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR_NS);
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
	DUMP_REG(DC_DISP_CURSOR_START_ADDR_HI);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR_HI_NS);
#endif
	DUMP_REG(DC_DISP_CURSOR_POSITION);
	DUMP_REG(DC_DISP_CURSOR_POSITION_NS);
	DUMP_REG(DC_DISP_INIT_SEQ_CONTROL);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_A);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_B);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_C);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_D);
	DUMP_REG(DC_DISP_DC_MCCIF_FIFOCTRL);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0A_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0B_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0C_HYST);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	DUMP_REG(DC_DISP_MCCIF_DISPLAY1B_HYST);
#endif
	DUMP_REG(DC_DISP_DAC_CRT_CTRL);
	DUMP_REG(DC_DISP_DISP_MISC_CONTROL);
#if defined(CONFIG_TEGRA_DC_INTERLACE)
	DUMP_REG(DC_DISP_INTERLACE_CONTROL);
	DUMP_REG(DC_DISP_INTERLACE_FIELD2_REF_TO_SYNC);
	DUMP_REG(DC_DISP_INTERLACE_FIELD2_SYNC_WIDTH);
	DUMP_REG(DC_DISP_INTERLACE_FIELD2_BACK_PORCH);
	DUMP_REG(DC_DISP_INTERLACE_FIELD2_FRONT_PORCH);
	DUMP_REG(DC_DISP_INTERLACE_FIELD2_DISP_ACTIVE);
#endif

	DUMP_REG(DC_CMD_DISPLAY_POWER_CONTROL);
	DUMP_REG(DC_COM_PIN_OUTPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_POLARITY2);
	DUMP_REG(DC_COM_PIN_OUTPUT_DATA2);
	DUMP_REG(DC_COM_PIN_INPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_SELECT5);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_COM_PM1_CONTROL);
	DUMP_REG(DC_COM_PM1_DUTY_CYCLE);
	DUMP_REG(DC_DISP_SD_CONTROL);

#ifdef CONFIG_TEGRA_DC_CMU
	DUMP_REG(DC_COM_CMU_CSC_KRR);
	DUMP_REG(DC_COM_CMU_CSC_KGR);
	DUMP_REG(DC_COM_CMU_CSC_KBR);
	DUMP_REG(DC_COM_CMU_CSC_KRG);
	DUMP_REG(DC_COM_CMU_CSC_KGG);
	DUMP_REG(DC_COM_CMU_CSC_KBG);
	DUMP_REG(DC_COM_CMU_CSC_KRB);
	DUMP_REG(DC_COM_CMU_CSC_KGB);
	DUMP_REG(DC_COM_CMU_CSC_KBB);
#endif

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		print(data, "\n");
		snprintf(buff, sizeof(buff), "WINDOW %c:\n", winname[i]);
		print(data, buff);

		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_WIN_WIN_OPTIONS);
		DUMP_REG(DC_WIN_BYTE_SWAP);
		DUMP_REG(DC_WIN_BUFFER_CONTROL);
		DUMP_REG(DC_WIN_COLOR_DEPTH);
		DUMP_REG(DC_WIN_POSITION);
		DUMP_REG(DC_WIN_SIZE);
		DUMP_REG(DC_WIN_PRESCALED_SIZE);
		DUMP_REG(DC_WIN_H_INITIAL_DDA);
		DUMP_REG(DC_WIN_V_INITIAL_DDA);
		DUMP_REG(DC_WIN_DDA_INCREMENT);
		DUMP_REG(DC_WIN_LINE_STRIDE);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
		DUMP_REG(DC_WIN_BUF_STRIDE);
		DUMP_REG(DC_WIN_UV_BUF_STRIDE);
#endif
		DUMP_REG(DC_WIN_BLEND_NOKEY);
		DUMP_REG(DC_WIN_BLEND_1WIN);
		DUMP_REG(DC_WIN_BLEND_2WIN_X);
		DUMP_REG(DC_WIN_BLEND_2WIN_Y);
		DUMP_REG(DC_WIN_BLEND_3WIN_XY);
		DUMP_REG(DC_WIN_GLOBAL_ALPHA);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC)
		DUMP_REG(DC_WINBUF_BLEND_LAYER_CONTROL);
#endif
		DUMP_REG(DC_WINBUF_START_ADDR);
		DUMP_REG(DC_WINBUF_START_ADDR_U);
		DUMP_REG(DC_WINBUF_START_ADDR_V);
		DUMP_REG(DC_WINBUF_ADDR_H_OFFSET);
		DUMP_REG(DC_WINBUF_ADDR_V_OFFSET);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC)
		DUMP_REG(DC_WINBUF_START_ADDR_HI);
		DUMP_REG(DC_WINBUF_START_ADDR_HI_U);
		DUMP_REG(DC_WINBUF_START_ADDR_HI_V);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2_U);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2_V);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2_HI);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2_HI_U);
		DUMP_REG(DC_WINBUF_START_ADDR_FIELD2_HI_V);
		DUMP_REG(DC_WINBUF_ADDR_H_OFFSET_FIELD2);
		DUMP_REG(DC_WINBUF_ADDR_V_OFFSET_FIELD2);
#endif
		DUMP_REG(DC_WINBUF_UFLOW_STATUS);
		DUMP_REG(DC_WIN_CSC_YOF);
		DUMP_REG(DC_WIN_CSC_KYRGB);
		DUMP_REG(DC_WIN_CSC_KUR);
		DUMP_REG(DC_WIN_CSC_KVR);
		DUMP_REG(DC_WIN_CSC_KUG);
		DUMP_REG(DC_WIN_CSC_KVG);
		DUMP_REG(DC_WIN_CSC_KUB);
		DUMP_REG(DC_WIN_CSC_KVB);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		DUMP_REG(DC_WINBUF_CDE_CONTROL);
		DUMP_REG(DC_WINBUF_CDE_COMPTAG_BASE_0);
		DUMP_REG(DC_WINBUF_CDE_COMPTAG_BASEHI_0);
		DUMP_REG(DC_WINBUF_CDE_ZBC_COLOR_0);
		DUMP_REG(DC_WINBUF_CDE_SURFACE_OFFSET_0);
		DUMP_REG(DC_WINBUF_CDE_CTB_ENTRY_0);
		DUMP_REG(DC_WINBUF_CDE_CG_SW_OVR);
		DUMP_REG(DC_WINBUF_CDE_PM_CONTROL);
		DUMP_REG(DC_WINBUF_CDE_PM_COUNTER);
#endif
	}

	tegra_dc_writel(dc, cmd_state, DC_CMD_STATE_ACCESS);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}
#endif	/* !CONFIG_TEGRA_NVDISPLAY */

#undef DUMP_REG

#ifdef DEBUG
static void dump_regs_print(void *data, const char *str)
{
	struct tegra_dc *dc = data;
	dev_dbg(&dc->ndev->dev, "%s", str);
}

void dump_regs(struct tegra_dc *dc)
{
	reg_dump(dc, dc, dump_regs_print);
}
#else /* !DEBUG */

void dump_regs(struct tegra_dc *dc) {}

#endif /* DEBUG */

#ifdef CONFIG_DEBUG_FS

static void dbg_regs_print(void *data, const char *str)
{
	struct seq_file *s = data;

	seq_printf(s, "%s", str);
}

#undef DUMP_REG

static int dbg_dc_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	reg_dump(dc, s, dbg_regs_print);

	return 0;
}


static int dbg_dc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open		= dbg_dc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_mode_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	struct tegra_dc_mode *m;

	mutex_lock(&dc->lock);
	m = &dc->mode;
	seq_printf(s,
		"pclk: %d\n"
		"h_ref_to_sync: %d\n"
		"v_ref_to_sync: %d\n"
		"h_sync_width: %d\n"
		"v_sync_width: %d\n"
		"h_back_porch: %d\n"
		"v_back_porch: %d\n"
		"h_active: %d\n"
		"v_active: %d\n"
		"h_front_porch: %d\n"
		"v_front_porch: %d\n"
		"flags: 0x%x\n"
		"stereo_mode: %d\n"
		"avi_m: 0x%x\n"
		"vmode: 0x%x\n",
		m->pclk, m->h_ref_to_sync, m->v_ref_to_sync,
		m->h_sync_width, m->v_sync_width,
		m->h_back_porch, m->v_back_porch,
		m->h_active, m->v_active,
		m->h_front_porch, m->v_front_porch,
		m->flags, m->stereo_mode, m->avi_m, m->vmode);
	mutex_unlock(&dc->lock);
	return 0;
}

static int dbg_dc_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_mode_show, inode->i_private);
}

static const struct file_operations mode_fops = {
	.open		= dbg_dc_mode_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_stats_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	mutex_lock(&dc->lock);
	seq_printf(s,
		"underflows: %llu\n"
		"underflows_a: %llu\n"
		"underflows_b: %llu\n"
		"underflows_c: %llu\n",
		dc->stats.underflows,
		dc->stats.underflows_a,
		dc->stats.underflows_b,
		dc->stats.underflows_c);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
	seq_printf(s,
		"underflows_d: %llu\n"
		"underflows_h: %llu\n"
		"underflows_t: %llu\n",
		dc->stats.underflows_d,
		dc->stats.underflows_h,
		dc->stats.underflows_t);
#endif
#if defined(CONFIG_TEGRA_NVDISPLAY)
	seq_printf(s,
		"underflow_frames: %llu\n",
		dc->stats.underflow_frames);
#endif
	mutex_unlock(&dc->lock);

	return 0;
}

static int dbg_dc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_stats_show, inode->i_private);
}

static int dbg_dc_event_inject_show(struct seq_file *s, void *unused)
{
	return 0;
}

static ssize_t dbg_dc_event_inject_write(struct file *file,
	const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data; /* single_open() initialized */
	struct tegra_dc *dc = m ? m->private : NULL;
	long event;
	int ret;

	if (!dc)
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &event);
	if (ret < 0)
		return ret;

	/*
	 * ADF has two seperate events for hotplug connect and disconnect.
	 * We map event 0x0, and 0x1 for them accordingly.  For DC_EXT,
	 * both events map to HOTPLUG.
	 */
#ifdef CONFIG_ADF_TEGRA
	if (event == 0x0)
		tegra_adf_process_hotplug_connected(dc->adf, NULL);
	else if (event == 0x1)
		tegra_adf_process_hotplug_disconnected(dc->adf);
	else if (event == 0x2)
		tegra_adf_process_bandwidth_renegotiate(dc->adf, 0);
	else {
		dev_err(&dc->ndev->dev, "Unknown event 0x%lx\n", event);
		return -EINVAL; /* unknown event number */
	}
#endif
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	if (event == 0x0 || event == 0x1) /* TEGRA_DC_EXT_EVENT_HOTPLUG */
		tegra_dc_ext_process_hotplug(dc->ndev->id);
	else if (event == 0x2) /* TEGRA_DC_EXT_EVENT_BANDWIDTH_DEC */
		tegra_dc_ext_process_bandwidth_renegotiate(
				dc->ndev->id, NULL);
	else {
		dev_err(&dc->ndev->dev, "Unknown event 0x%lx\n", event);
		return -EINVAL; /* unknown event number */
	}
#endif
	return len;
}

/* Update the strings as dc.h get updated for new output types*/
static const char * const dc_outtype_strings[] = {
	"TEGRA_DC_OUT_RGB",
	"TEGRA_DC_OUT_HDMI",
	"TEGRA_DC_OUT_DSI",
	"TEGRA_DC_OUT_DP",
	"TEGRA_DC_OUT_LVDS",
	"TEGRA_DC_OUT_NVSR_DP",
	"TEGRA_DC_OUT_FAKE_DP",
	"TEGRA_DC_OUT_FAKE_DSIA",
	"TEGRA_DC_OUT_FAKE_DSIB",
	"TEGRA_DC_OUT_FAKE_DSI_GANGED",
	"TEGRA_DC_OUT_NULL",
	"TEGRA_DC_OUT_UNKNOWN"
};

static int dbg_dc_outtype_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	mutex_lock(&dc->lock);
	seq_puts(s, "\n");
	seq_printf(s,
		"\tDC OUTPUT: \t%s (%d)\n",
		dc_outtype_strings[dc->out->type], dc->out->type);
	seq_puts(s, "\n");
	mutex_unlock(&dc->lock);
	return 0;
}

/* Add specific variable related to each output type.
 * Save and reuse on changing the output type
 */
#if defined(CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT)
struct tegra_dc_out_info {
	struct tegra_dc_out_ops *out_ops;
	void *out_data;
	struct tegra_dc_out out;
	struct tegra_dc_mode mode;
	int fblistindex;
};

static struct tegra_dc_out_info dbg_dc_out_info[TEGRA_DC_OUT_MAX];
/* array for saving the out_type for each head */
static int  boot_out_type[] = {-1, -1, -1};

static int is_invalid_dc_out(struct tegra_dc *dc, long dc_outtype)
{
	if ((dc_outtype != boot_out_type[dc->ndev->id]) &&
		(dc_outtype != TEGRA_DC_OUT_FAKE_DP) &&
		(dc_outtype != TEGRA_DC_OUT_FAKE_DSIA) &&
		(dc_outtype != TEGRA_DC_OUT_FAKE_DSIB) &&
		(dc_outtype != TEGRA_DC_OUT_FAKE_DSI_GANGED) &&
		(dc_outtype != TEGRA_DC_OUT_NULL)) {
		dev_err(&dc->ndev->dev,
			"Request 0x%lx is unsupported target out_type\n",
			 dc_outtype);
		dev_err(&dc->ndev->dev,
			"boot_out_type[%d] is 0x%x\n",
			 dc->ndev->id, boot_out_type[dc->ndev->id]);
		return -EINVAL;
	}

	return 0;
}

static int is_valid_dsi_out(struct tegra_dc *dc, long dc_outtype)
{
	if (((dc_outtype >= TEGRA_DC_OUT_FAKE_DSIA) &&
		(dc_outtype <= TEGRA_DC_OUT_FAKE_DSI_GANGED)) ||
		(dc_outtype == TEGRA_DC_OUT_DSI))
			return 1;

	return 0;
}


static int is_valid_fake_support(struct tegra_dc *dc, long dc_outtype)
{
	if ((dc_outtype == TEGRA_DC_OUT_FAKE_DP) ||
		(dc_outtype == TEGRA_DC_OUT_FAKE_DSIA) ||
		(dc_outtype == TEGRA_DC_OUT_FAKE_DSIB) ||
		(dc_outtype == TEGRA_DC_OUT_FAKE_DSI_GANGED) ||
		(dc_outtype == TEGRA_DC_OUT_NULL))
		return 1;

	return 0;
}

static int set_avdd(struct tegra_dc *dc, long cur_out, long new_out)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	/* T210 macro_clk is failing SOR access
	 * if avdd_lcd is not enabled
	 */
	bool is_enable = false;
	struct tegra_dc_out *dc_out =
		&dbg_dc_out_info[boot_out_type[dc->ndev->id]].out;

	/* cur is fake and new is fake - skip */
	if (is_valid_fake_support(dc, cur_out) &&
		is_valid_fake_support(dc, new_out))
		return 0;

	/* cur is valid and new is fake - enable */
	if (!is_valid_fake_support(dc, cur_out) &&
		is_valid_fake_support(dc, new_out))
		is_enable = true;

	if (is_enable) {
		if (dc_out && dc_out->enable)
			dc_out->enable(&dc->ndev->dev);
	} else {
		if (dc_out && dc_out->disable)
			dc_out->disable(&dc->ndev->dev);
	}
#endif
	return 0;
}
static ssize_t dbg_dc_out_type_set(struct file *file,
	const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data; /* single_open() initialized */
	struct tegra_dc *dc = m ? m->private : NULL;
	long cur_dc_out;
	long out_type;
	int ret = 0;
	bool  allocate = false;

	if (!dc)
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &out_type);
	if (ret < 0)
		return ret;

	if (!dc->pdata->default_out)
		return -EINVAL;

	/* check out type is out of range then skip */
	if (out_type < TEGRA_DC_OUT_RGB ||
		out_type >= TEGRA_DC_OUT_MAX) {
		dev_err(&dc->ndev->dev, "Unknown out_type 0x%lx\n", out_type);
		return -EINVAL;
	}

	WARN_ON((sizeof(boot_out_type) / sizeof(int)) != TEGRA_MAX_DC);

	if (boot_out_type[dc->ndev->id] == -1)
		boot_out_type[dc->ndev->id] = dc->pdata->default_out->type;

	cur_dc_out = dc->pdata->default_out->type;

	/* Nothing to do if new outtype is same as old
	 * Allow to switch between booted out type and fake panel out
	 */
	if ((cur_dc_out == out_type) || is_invalid_dc_out(dc, out_type))
		return -EINVAL;

	/* disable the dc and output controllers */
	if (dc->enabled)
		tegra_dc_disable(dc);

	/* If output is already created - save it */
	if (dc->out_data) {
		dbg_dc_out_info[cur_dc_out].out_data = dc->out_data;
		dbg_dc_out_info[cur_dc_out].out_ops  = dc->out_ops;
		memcpy(&dbg_dc_out_info[cur_dc_out].out, dc->out,
					sizeof(struct tegra_dc_out));
		dbg_dc_out_info[cur_dc_out].mode = dc->mode;

		if (is_valid_dsi_out(dc, cur_dc_out) &&
			dbg_dc_out_info[cur_dc_out].out_data)
			tegra_dc_destroy_dsi_resources(dc, cur_dc_out);

		if (!is_valid_fake_support(dc, cur_dc_out))
			dbg_dc_out_info[cur_dc_out].fblistindex =
						tegra_fb_update_modelist(dc, 0);

		set_avdd(dc, cur_dc_out, out_type);
	}

	/* If output already created - reuse it */
	if (dbg_dc_out_info[out_type].out_data) {
		mutex_lock(&dc->lp_lock);
		mutex_lock(&dc->lock);

		/* Change the out type */
		dc->pdata->default_out->type = out_type;
		dc->out_ops = dbg_dc_out_info[out_type].out_ops;
		dc->out_data = dbg_dc_out_info[out_type].out_data;
		memcpy(dc->out, &dbg_dc_out_info[out_type].out,
						sizeof(struct tegra_dc_out));
		dc->mode = dbg_dc_out_info[out_type].mode;

		/* Re-init the resources that are destroyed for dsi */
		if (is_valid_dsi_out(dc, out_type))
			ret = tegra_dc_reinit_dsi_resources(dc, out_type);

		if (!is_valid_fake_support(dc, out_type))
			tegra_fb_update_modelist(dc,
					dbg_dc_out_info[out_type].fblistindex);

		mutex_unlock(&dc->lock);
		mutex_unlock(&dc->lp_lock);

		if (ret) {
			dev_err(&dc->ndev->dev, "Failed to reinit!!!\n");
			return -EINVAL;
		}

	} else {
		/* Change the out type */
		dc->pdata->default_out->type = out_type;

		/* create new - now restricted to fake_dp only */
		if (out_type == TEGRA_DC_OUT_FAKE_DP) {

			/* set to default bpp */
			if (!dc->pdata->default_out->depth)
				dc->pdata->default_out->depth = 24;

			/* DP and Fake_Dp use same data
			*  Reuse if already created */
			if (!dbg_dc_out_info[TEGRA_DC_OUT_DP].out_data) {
				allocate = true;
				tegra_dc_init_fakedp_panel(dc);
			}
		} else if ((out_type >= TEGRA_DC_OUT_FAKE_DSIA) &&
				(out_type <= TEGRA_DC_OUT_FAKE_DSI_GANGED)) {
			/* DSI and fake DSI use same data
			 * create new if not created yet
			 */
			if (!dc->pdata->default_out->depth)
				dc->pdata->default_out->depth = 18;

			allocate = true;
			tegra_dc_init_fakedsi_panel(dc, out_type);

		} else if (out_type == TEGRA_DC_OUT_NULL) {
			if (!dbg_dc_out_info[TEGRA_DC_OUT_NULL].out_data) {
				allocate = true;
				tegra_dc_init_null_or(dc);
			}
		} else {
			/* set  back to existing one */
			dc->pdata->default_out->type = cur_dc_out;
			dev_err(&dc->ndev->dev, "Unknown type is asked\n");
			goto by_pass;
		}

		if (allocate) {
			ret = tegra_dc_set_out(dc, dc->pdata->default_out);
				if (ret < 0) {
					dev_err(&dc->ndev->dev,
					"Failed to initialize DC out ops\n");
					return -EINVAL;
				}
		}

		dbg_dc_out_info[out_type].out_ops = dc->out_ops;
		dbg_dc_out_info[out_type].out_data = dc->out_data;
		memcpy(&dbg_dc_out_info[out_type].out, dc->out,
						sizeof(struct tegra_dc_out));

	}

by_pass:
	/*enable the dc and output controllers */
	if (!dc->enabled)
		tegra_dc_enable(dc);

	return len;
}
#else
static ssize_t dbg_dc_out_type_set(struct file *file,
	const char __user *addr, size_t len, loff_t *pos)
{
	return -EINVAL;
}
#endif /*CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT*/

static const struct file_operations stats_fops = {
	.open		= dbg_dc_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_event_inject_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_event_inject_show, inode->i_private);
}

static const struct file_operations event_inject_fops = {
	.open		= dbg_dc_event_inject_open,
	.read		= seq_read,
	.write		= dbg_dc_event_inject_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_outtype_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_outtype_show, inode->i_private);
}

static const struct file_operations outtype_fops = {
	.open		= dbg_dc_outtype_open,
	.read		= seq_read,
	.write		= dbg_dc_out_type_set,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_edid_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	struct tegra_edid *edid = dc->edid;
	struct tegra_dc_edid *data;
	u8 *buf;
	int i;

	if (WARN_ON(!dc || !dc->out || !dc->edid))
		return -EINVAL;

	data = tegra_edid_get_data(edid);
	if (!data) {
		seq_puts(s, "No EDID\n");
		return 0;
	}

	buf = data->buf;

	for (i = 0; i < data->len; i++) {
#ifdef DEBUG
		if (i % 16 == 0)
			seq_printf(s, "edid[%03x] =", i);
#endif

		seq_printf(s, " %02x", buf[i]);

		if (i % 16 == 15)
			seq_puts(s, "\n");

	}

	tegra_edid_put_data(data);

	return 0;
}

static int dbg_edid_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_edid_show, inode->i_private);
}

static ssize_t dbg_edid_write(struct file *file,
const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc *dc = m ? m->private : NULL;
	int ret;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	dc->vedid = false;

	kfree(dc->vedid_data);
	dc->vedid_data = NULL;

	if (len < 128) /* invalid edid, turn off vedid */
		return 1;

	dc->vedid_data = kmalloc(sizeof(char) * len, GFP_KERNEL);
	if (!dc->vedid_data) {
		dev_err(&dc->ndev->dev, "no memory for edid\n");
		return 0; /* dc->vedid is false */
	}

	ret = copy_from_user(dc->vedid_data, addr, len);
	if (ret < 0) {
		dev_err(&dc->ndev->dev, "error copying edid\n");
		kfree(dc->vedid_data);
		dc->vedid_data = NULL;
		return ret; /* dc->vedid is false */
	}

	dc->vedid = true;

	return len;
}

static const struct file_operations edid_fops = {
	.open		= dbg_edid_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= dbg_edid_write,
	.release	= single_release,
};

static int dbg_hotplug_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	rmb();
	seq_put_decimal_ll(s, '\0', dc->out->hotplug_state);
	seq_putc(s, '\n');
	return 0;
}

static int dbg_hotplug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_hotplug_show, inode->i_private);
}

static ssize_t dbg_hotplug_write(struct file *file, const char __user *addr,
	size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data; /* single_open() initialized */
	struct tegra_dc *dc = m ? m->private : NULL;
	int ret;
	long new_state;
	int hotplug_state;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_state);
	if (ret < 0)
		return ret;

	mutex_lock(&dc->lock);
	rmb();
	hotplug_state = dc->out->hotplug_state;
	if (hotplug_state == 0 && new_state != 0
			&& dc->hotplug_supported) {
		/* was 0, now -1 or 1.
		 * we are overriding the hpd GPIO, so ignore the interrupt. */
		int gpio_irq = gpio_to_irq(dc->out->hotplug_gpio);

		disable_irq(gpio_irq);
	} else if (hotplug_state != 0 && new_state == 0
			&& dc->hotplug_supported) {
		/* was -1 or 1, and now 0
		 * restore the interrupt for hpd GPIO. */
		int gpio_irq = gpio_to_irq(dc->out->hotplug_gpio);

		enable_irq(gpio_irq);
	}

	dc->out->hotplug_state = new_state;
	wmb();

	/* retrigger the hotplug */
	if (dc->out_ops->detect)
		dc->connected = dc->out_ops->detect(dc);
	mutex_unlock(&dc->lock);

	return len;
}

static const struct file_operations dbg_hotplug_fops = {
	.open		= dbg_hotplug_open,
	.read		= seq_read,
	.write		= dbg_hotplug_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_vrr_enable_show(struct seq_file *m, void *unused)
{
	struct tegra_vrr *vrr = m->private;

	if (!vrr) return -EINVAL;

	seq_printf(m, "vrr enable state: %d\n", vrr->enable);

	return 0;
}

static int dbg_vrr_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_vrr_enable_show, inode->i_private);
}

static const struct file_operations dbg_vrr_enable_ops = {
	.open = dbg_vrr_enable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_vrr_dcb_show(struct seq_file *m, void *unused)
{
	struct tegra_vrr *vrr = m->private;

	if (!vrr)
		return -EINVAL;

	seq_printf(m, "vrr dc balance: %d\n", vrr->dcb);

	return 0;
}

static int dbg_vrr_dcb_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_vrr_dcb_show, inode->i_private);
}

static const struct file_operations dbg_vrr_dcb_ops = {
	.open = dbg_vrr_dcb_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_vrr_db_tolerance_show(struct seq_file *m, void *unused)
{
	struct tegra_vrr *vrr = m->private;

	if (!vrr)
		return -EINVAL;

	seq_printf(m, "vrr db tolerance: %d\n", vrr->db_tolerance);

	return 0;
}

static ssize_t dbg_vrr_db_tolerance_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_vrr *vrr = m->private;
	long   new_value;
	int    ret;

	if (!vrr)
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_value);
	if (ret < 0)
		return ret;

	vrr->db_tolerance = new_value;

	return len;
}

static int dbg_vrr_db_tolerance_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_vrr_db_tolerance_show, inode->i_private);
}

static const struct file_operations dbg_vrr_db_tolerance_ops = {
	.open = dbg_vrr_db_tolerance_open,
	.read = seq_read,
	.write = dbg_vrr_db_tolerance_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_vrr_frame_avg_pct_show(struct seq_file *m, void *unused)
{
	struct tegra_vrr *vrr = m->private;

	if (!vrr)
		return -EINVAL;

	seq_printf(m, "vrr frame average percent: %d\n", vrr->frame_avg_pct);

	return 0;
}

static ssize_t dbg_vrr_frame_avg_pct_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_vrr *vrr = m->private;
	long   new_pct;
	int    ret;

	if (!vrr)
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_pct);
	if (ret < 0)
		return ret;

	vrr->frame_avg_pct = new_pct;

	return len;
}

static int dbg_vrr_frame_avg_pct_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_vrr_frame_avg_pct_show, inode->i_private);
}

static const struct file_operations dbg_vrr_frame_avg_pct_ops = {
	.open = dbg_vrr_frame_avg_pct_open,
	.read = seq_read,
	.write = dbg_vrr_frame_avg_pct_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_vrr_fluct_avg_pct_show(struct seq_file *m, void *unused)
{
	struct tegra_vrr *vrr = m->private;

	if (!vrr)
		return -EINVAL;

	seq_printf(m, "vrr fluct average percent: %d\n", vrr->fluct_avg_pct);

	return 0;
}

static ssize_t dbg_vrr_fluct_avg_pct_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_vrr *vrr = m->private;
	long   new_pct;
	int    ret;

	if (!vrr)
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_pct);
	if (ret < 0)
		return ret;

	vrr->fluct_avg_pct = new_pct;

	return len;
}

static int dbg_vrr_fluct_avg_pct_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_vrr_fluct_avg_pct_show, inode->i_private);
}

static const struct file_operations dbg_vrr_fluct_avg_pct_ops = {
	.open = dbg_vrr_fluct_avg_pct_open,
	.read = seq_read,
	.write = dbg_vrr_fluct_avg_pct_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_tegrahw_type_show(struct seq_file *m, void *unused)
{
	struct tegra_dc *dc = m->private;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	/* All platforms other than real silicon are taken
		as simulation */
	seq_printf(m,
		"real_silicon: %d\n",
		tegra_platform_is_silicon());

	return 0;
}

static int dbg_tegrahw_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_tegrahw_type_show, inode->i_private);
}

static const struct file_operations dbg_tegrahw_type_ops = {
	.open = dbg_tegrahw_type_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t dbg_background_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc *dc = m->private;
	unsigned long background;
	u32 old_state;

	if (!dc)
		return -EINVAL;

	if (kstrtoul_from_user(addr, len, 0, &background) < 0)
		return -EINVAL;

	if (!dc->enabled)
		return -EBUSY;

	tegra_dc_get(dc);
	mutex_lock(&dc->lock);
	old_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	/* write active version */
	tegra_dc_writel(dc, WRITE_MUX_ACTIVE | READ_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);
	tegra_dc_readl(dc, DC_CMD_STATE_ACCESS); /* flush */
	tegra_dc_writel(dc, background, DC_DISP_BLEND_BACKGROUND_COLOR);
	/* write assembly version */
	tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);
	tegra_dc_readl(dc, DC_CMD_STATE_ACCESS); /* flush */
	tegra_dc_writel(dc, background, DC_DISP_BLEND_BACKGROUND_COLOR);
	/* cycle the values through assemby -> arm -> active */
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_readl(dc, DC_CMD_STATE_CONTROL); /* flush */
	tegra_dc_writel(dc, NC_HOST_TRIG | GENERAL_ACT_REQ,
			DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, old_state, DC_CMD_STATE_ACCESS);
	tegra_dc_readl(dc, DC_CMD_STATE_ACCESS); /* flush */
	mutex_unlock(&dc->lock);
	tegra_dc_put(dc);

	return len;
}

static int dbg_background_show(struct seq_file *m, void *unused)
{
	struct tegra_dc *dc = m->private;
	u32 old_state;
	u32 background;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	if (!dc->enabled)
		return -EBUSY;

	tegra_dc_get(dc);
	mutex_lock(&dc->lock);
	old_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, WRITE_MUX_ACTIVE | READ_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);
	background = tegra_dc_readl(dc, DC_DISP_BLEND_BACKGROUND_COLOR);
	tegra_dc_writel(dc, old_state, DC_CMD_STATE_ACCESS);
	mutex_unlock(&dc->lock);
	tegra_dc_put(dc);

	seq_printf(m, "%#x\n", (unsigned)background);

	return 0;
}

static int dbg_background_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_background_show, inode->i_private);
}

static const struct file_operations dbg_background_ops = {
	.open = dbg_background_open,
	.write = dbg_background_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* toggly the enable/disable for any windows with 1 bit set */
static ssize_t dbg_window_toggle_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc *dc = m->private;
	unsigned long windows;
	int i;
	u32 status;
	int retries;

	if (!dc)
		return -EINVAL;

	if (kstrtoul_from_user(addr, len, 0, &windows) < 0)
		return -EINVAL;

	if (!dc->enabled)
		return 0;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	/* limit the request only to valid windows */
	windows &= dc->valid_windows;
	for_each_set_bit(i, &windows, DC_N_WINDOWS) {
		u32 val;
		/* select the assembly registers for window i */
		tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
				DC_CMD_STATE_ACCESS);
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		/* toggle the enable bit */
		val = tegra_dc_readl(dc, DC_WIN_WIN_OPTIONS);
		val ^= WIN_ENABLE;
		dev_dbg(&dc->ndev->dev, "%s window #%d\n",
			(val & WIN_ENABLE) ? "enabling" : "disabling", i);
		tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);

		/* post the update */
		tegra_dc_writel(dc, WIN_A_UPDATE << i, DC_CMD_STATE_CONTROL);
		retries = 8;
		do {
			status = tegra_dc_readl(dc, DC_CMD_STATE_CONTROL);
			retries--;
		} while (retries && (status & (WIN_A_UPDATE << i)));
		tegra_dc_writel(dc, WIN_A_ACT_REQ << i, DC_CMD_STATE_CONTROL);

	}
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return len;
}

/* reading shows the enabled windows */
static int dbg_window_toggle_show(struct seq_file *m, void *unused)
{
	struct tegra_dc *dc = m->private;
	int i;
	unsigned long windows;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	/* limit the request only to valid windows */
	windows = 0;
	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		u32 val;

		/* select the active registers for window i */
		tegra_dc_writel(dc, WRITE_MUX_ACTIVE | READ_MUX_ACTIVE,
				DC_CMD_STATE_ACCESS);
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);

		/* add i to a bitmap if WIN_ENABLE is set */
		val = tegra_dc_readl(dc, DC_WIN_WIN_OPTIONS);
		if (val & WIN_ENABLE)
			set_bit(i, &windows);

	}
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	seq_printf(m, "%#lx %#lx\n", dc->valid_windows, windows);

	return 0;
}

static int dbg_window_toggle_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_window_toggle_show, inode->i_private);
}

static const struct file_operations dbg_window_toggle_ops = {
	.open = dbg_window_toggle_open,
	.write = dbg_window_toggle_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_dc_cmu_lut1_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	u32 val;
	int i;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	/* Disable CMU while reading LUTs */
	val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, val & ~CMU_ENABLE, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	_tegra_dc_wait_for_frame_end(dc,
		div_s64(dc->frametime_ns, 1000000ll) * 2);

	for (i = 0; i < 256; i++) {
		tegra_dc_writel(dc, LUT1_READ_EN | LUT1_READ_ADDR(i),
			DC_COM_CMU_LUT1_READ);

		seq_printf(s, "%lu\n",
			LUT1_READ_DATA(tegra_dc_readl(dc, DC_COM_CMU_LUT1)));
	}
	tegra_dc_writel(dc, 0, DC_COM_CMU_LUT1_READ);

	tegra_dc_writel(dc, val, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
	return 0;
}

static int dbg_dc_cmu_lut1_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_cmu_lut1_show, inode->i_private);
}

static const struct file_operations cmu_lut1_fops = {
	.open		= dbg_dc_cmu_lut1_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_cmu_lut2_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	u32 val;
	int i;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	/* Disable CMU while reading LUTs */
	val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, val & ~CMU_ENABLE, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	_tegra_dc_wait_for_frame_end(dc,
		div_s64(dc->frametime_ns, 1000000ll) * 2);

	for (i = 0; i < 960; i++) {
		tegra_dc_writel(dc, LUT2_READ_EN | LUT2_READ_ADDR(i),
			DC_COM_CMU_LUT2_READ);

		seq_printf(s, "%lu\n",
			LUT2_READ_DATA(tegra_dc_readl(dc, DC_COM_CMU_LUT2)));
	}
	tegra_dc_writel(dc, 0, DC_COM_CMU_LUT2_READ);

	tegra_dc_writel(dc, val, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
	return 0;
}

static int dbg_dc_cmu_lut2_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_cmu_lut2_show, inode->i_private);
}

static const struct file_operations cmu_lut2_fops = {
	.open		= dbg_dc_cmu_lut2_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_measure_refresh_show(struct seq_file *m, void *unused)
{
	struct tegra_dc *dc = m->private;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	seq_puts(m, "Write capture time in seconds to this node.\n");
	seq_puts(m, "Results will show up in dmesg.\n");

	return 0;
}

static ssize_t dbg_measure_refresh_write(struct file *file,
		const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_dc *dc = m->private;
	s32 seconds;
	u32 fe_count;
	int ret;
	fixed20_12 refresh_rate;
	fixed20_12 seconds_fixed;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	ret = kstrtoint_from_user(addr, len, 10, &seconds);
	if (ret < 0 || seconds < 1) {
		dev_info(&dc->ndev->dev,
				"specify integer number of seconds greater than 0\n");
		return -EINVAL;
	}

	dev_info(&dc->ndev->dev, "measuring for %d seconds\n", seconds);

	mutex_lock(&dc->lock);
	_tegra_dc_config_frame_end_intr(dc, true);
	dc->dbg_fe_count = 0;
	mutex_unlock(&dc->lock);

	msleep(1000 * seconds);

	mutex_lock(&dc->lock);
	_tegra_dc_config_frame_end_intr(dc, false);
	fe_count = dc->dbg_fe_count;
	mutex_unlock(&dc->lock);

	refresh_rate.full = dfixed_const(fe_count);
	seconds_fixed.full = dfixed_const(seconds);
	refresh_rate.full = dfixed_div(refresh_rate, seconds_fixed);

	/* Print fixed point 20.12 in decimal, truncating the 12-bit fractional
	   part to 2 decimal points */
	dev_info(&dc->ndev->dev, "refresh rate: %d.%dHz\n",
		dfixed_trunc(refresh_rate),
		dfixed_frac(refresh_rate) * 100 / 4096);

	return len;
}

static int dbg_measure_refresh_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_measure_refresh_show, inode->i_private);
}

static const struct file_operations dbg_measure_refresh_ops = {
	.open = dbg_measure_refresh_open,
	.read = seq_read,
	.write = dbg_measure_refresh_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void tegra_dc_remove_debugfs(struct tegra_dc *dc)
{
	if (dc->debugdir)
		debugfs_remove_recursive(dc->debugdir);
	dc->debugdir = NULL;
}

#ifdef CONFIG_TEGRA_NVDISPLAY
/*
 * ihub_win_num specifies the window number. A value of -1 should be used if
 * the property you want to read isn't window specific.
 */
static int ihub_win_num;

static int dbg_ihub_win_num_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	seq_put_decimal_ll(s, '\0', ihub_win_num);
	seq_putc(s, '\n');
	return 0;
}

static int dbg_ihub_win_num_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_ihub_win_num_show, inode->i_private);
}

static ssize_t dbg_ihub_win_num_write(struct file *file,
	const char __user *addr, size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data; /* single_open() initialized */
	struct tegra_dc *dc = m ? m->private : NULL;
	int ret;
	long new_win_num;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_win_num);
	if (ret < 0)
		return ret;

	mutex_lock(&dc->lock);
	ihub_win_num = new_win_num;
	mutex_unlock(&dc->lock);

	return len;
}

static const struct file_operations dbg_ihub_win_num_ops = {
	.open = dbg_ihub_win_num_open,
	.read = seq_read,
	.write = dbg_ihub_win_num_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dbg_ihub_mempool_size_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	seq_put_decimal_ll(s, '\0', tegra_nvdisp_ihub_read(dc, -1, 0));
	seq_putc(s, '\n');
	return 0;
}

static int dbg_ihub_mempool_size_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_ihub_mempool_size_show, inode->i_private);
}

static ssize_t dbg_ihub_mempool_size_write(struct file *file,
	const char __user *addr, size_t len, loff_t *pos)
{
	return -EINVAL; /* read-only property */
}

static const struct file_operations dbg_ihub_mempool_size_ops = {
	.open = dbg_ihub_mempool_size_open,
	.read = seq_read,
	.write = dbg_ihub_mempool_size_write,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif

static void tegra_dc_create_debugfs(struct tegra_dc *dc)
{
	struct dentry *retval, *vrrdir;
#ifdef CONFIG_TEGRA_NVDISPLAY
	struct dentry *ihubdir;
#endif
	char   devname[50];

	snprintf(devname, sizeof(devname), "tegradc.%d", dc->ctrl_num);
	dc->debugdir = debugfs_create_dir(devname, NULL);
	if (!dc->debugdir)
		goto remove_out;

	retval = debugfs_create_file("regs", S_IRUGO, dc->debugdir, dc,
		&regs_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("mode", S_IRUGO, dc->debugdir, dc,
		&mode_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("stats", S_IRUGO, dc->debugdir, dc,
		&stats_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("event_inject", S_IRUGO, dc->debugdir, dc,
		&event_inject_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("out_type", S_IRUGO, dc->debugdir, dc,
		&outtype_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("edid", S_IRUGO, dc->debugdir, dc,
		&edid_fops);
	if (!retval)
		goto remove_out;

	if (dc->out_ops->detect) {
		/* only create the file if hotplug is supported */
		retval = debugfs_create_file("hotplug", S_IRUGO, dc->debugdir,
			dc, &dbg_hotplug_fops);
		if (!retval)
			goto remove_out;
	}

	vrrdir = debugfs_create_dir("vrr",  dc->debugdir);
	if (!vrrdir)
		goto remove_out;

	retval = debugfs_create_file("enable", S_IRUGO, vrrdir,
				dc->out->vrr, &dbg_vrr_enable_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("dcb", S_IRUGO, vrrdir,
				dc->out->vrr, &dbg_vrr_dcb_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("frame_avg_pct", S_IRUGO, vrrdir,
				dc->out->vrr, &dbg_vrr_frame_avg_pct_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("fluct_avg_pct", S_IRUGO, vrrdir,
				dc->out->vrr, &dbg_vrr_fluct_avg_pct_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("tegrahw_type", S_IRUGO, dc->debugdir,
				dc, &dbg_tegrahw_type_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("background", S_IRUGO, dc->debugdir,
				dc, &dbg_background_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("window_toggle", S_IRUGO, dc->debugdir,
				dc, &dbg_window_toggle_ops);
	if (!retval)
		goto remove_out;

#ifdef CONFIG_TEGRA_NVDISPLAY
	ihubdir = debugfs_create_dir("ihub", dc->debugdir);
	if (!ihubdir)
		goto remove_out;

	retval = debugfs_create_file("win_num", S_IRUGO, ihubdir,
				dc, &dbg_ihub_win_num_ops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("mempool_size", S_IRUGO, ihubdir,
				dc, &dbg_ihub_mempool_size_ops);
	if (!retval)
		goto remove_out;
#endif

	retval = debugfs_create_file("cmu_lut1", S_IRUGO, dc->debugdir, dc,
		&cmu_lut1_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("cmu_lut2", S_IRUGO, dc->debugdir, dc,
		&cmu_lut2_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("measure_refresh", S_IRUGO, dc->debugdir,
				dc, &dbg_measure_refresh_ops);
	if (!retval)
		goto remove_out;

	return;
remove_out:
	dev_err(&dc->ndev->dev, "could not create debugfs\n");
	tegra_dc_remove_debugfs(dc);
}

#else /* !CONFIG_DEBUGFS */
static inline void tegra_dc_create_debugfs(struct tegra_dc *dc) { };
static inline void tegra_dc_remove_debugfs(struct tegra_dc *dc) { };
#endif /* CONFIG_DEBUGFS */

s32 tegra_dc_calc_v_front_porch(struct tegra_dc_mode *mode,
				int desired_fps)
{
	int vfp = 0;

	if (desired_fps > 0) {
		int line = mode->h_sync_width + mode->h_back_porch +
			mode->h_active + mode->h_front_porch;
		int lines_per_frame = mode->pclk / line / desired_fps;
		vfp = lines_per_frame - mode->v_sync_width -
			mode->v_active - mode->v_back_porch;
	}

	return vfp;
}

static void tegra_dc_setup_vrr(struct tegra_dc *dc)
{
	int lines_per_frame_max, lines_per_frame_min;

	struct tegra_dc_mode *m;
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr) return;

	m = &dc->out->modes[dc->out->n_modes-1];
	vrr->v_front_porch = m->v_front_porch;
	vrr->v_back_porch = m->v_back_porch;
	vrr->pclk = m->pclk;

	if (vrr->vrr_min_fps > 0)
		vrr->v_front_porch_max = tegra_dc_calc_v_front_porch(m,
				vrr->vrr_min_fps);

	vrr->vrr_max_fps =
		(s32)div_s64(NSEC_PER_SEC, dc->frametime_ns);

	vrr->v_front_porch_min = m->v_front_porch;

	vrr->line_width = m->h_sync_width + m->h_back_porch +
			m->h_active + m->h_front_porch;
	vrr->lines_per_frame_common = m->v_sync_width +
			m->v_back_porch + m->v_active;
	lines_per_frame_max = vrr->lines_per_frame_common +
			vrr->v_front_porch_max;
	lines_per_frame_min = vrr->lines_per_frame_common +
			vrr->v_front_porch_min;

	if (lines_per_frame_max < 2*lines_per_frame_min) {
		pr_err("max fps is less than 2 times min fps.\n");
		return;
	}

	vrr->frame_len_max = vrr->line_width * lines_per_frame_max /
					(m->pclk / 1000000);
	vrr->frame_len_min = vrr->line_width * lines_per_frame_min /
					(m->pclk / 1000000);
	vrr->vfp_extend = vrr->v_front_porch_max;
	vrr->vfp_shrink = vrr->v_front_porch_min;

	vrr->frame_type = 0;
	vrr->frame_delta_us = 0;

	vrr->max_adj_pct = 50;
	vrr->max_flip_pct = 20;
	vrr->max_dcb = 20000;
	vrr->max_inc_pct = 5;

	vrr->dcb = 0;
	vrr->frame_avg_pct = 75;
	vrr->fluct_avg_pct = 75;
	vrr->db_tolerance = 5000;
}

unsigned long tegra_dc_poll_register(struct tegra_dc *dc, u32 reg, u32 mask,
		u32 exp_val, u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u32 reg_val = 0;

	if (tegra_platform_is_linsim())
		return 0;

	do {
		usleep_range(poll_interval_us, poll_interval_us << 1);
/*		usleep_range(1000, 1500);*/
		reg_val = tegra_dc_readl(dc, reg);
	} while (((reg_val & mask) != exp_val) &&
		time_after(timeout_jf, jiffies));

	if ((reg_val & mask) == exp_val)
		return 0;       /* success */
	dev_err(&dc->ndev->dev,
		"dc_poll_register 0x%x: timeout\n", reg);
	return jiffies - timeout_jf + 1;
}


void tegra_dc_enable_general_act(struct tegra_dc *dc)
{
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	if (tegra_dc_poll_register(dc, DC_CMD_STATE_CONTROL,
		GENERAL_ACT_REQ, 0, 1,
		TEGRA_DC_POLL_TIMEOUT_MS))
		dev_err(&dc->ndev->dev,
			"dc timeout waiting for DC to stop\n");
}


static int tegra_dc_set_next(struct tegra_dc *dc)
{
	int i;
	int ret = -EBUSY;

	mutex_lock(&tegra_dc_lock);

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		if (tegra_dcs[i] == NULL) {
			tegra_dcs[i] = dc;
			ret = i;
			break;
		}
	}

	mutex_unlock(&tegra_dc_lock);

	return ret;
}

static int tegra_dc_set_idx(struct tegra_dc *dc, int index)
{
	int ret = 0;

	mutex_lock(&tegra_dc_lock);
	if (index >= TEGRA_MAX_DC) {
		ret = -EINVAL;
		goto out;
	}

	if (dc != NULL && tegra_dcs[index] != NULL) {
		ret = -EBUSY;
		goto out;
	}

	tegra_dcs[index] = dc;

out:
	mutex_unlock(&tegra_dc_lock);

	return ret;
}

/*
 * If index == -1, set dc at next available index. This is to be called only
 * when registering dc in DT case. For non DT case & when removing the device
 * (dc == NULL), index should be accordingly.
 */
static int tegra_dc_set(struct tegra_dc *dc, int index)
{
	if ((index == -1) && (dc != NULL)) /* DT register case */
		return tegra_dc_set_next(dc);
	else /* non DT, unregister case */
		return tegra_dc_set_idx(dc, index);
}

unsigned int tegra_dc_has_multiple_dc(void)
{
	unsigned int idx;
	unsigned int cnt = 0;
	struct tegra_dc *dc;

	mutex_lock(&tegra_dc_lock);
	for (idx = 0; idx < TEGRA_MAX_DC; idx++)
		cnt += ((dc = tegra_dcs[idx]) != NULL && dc->enabled) ? 1 : 0;
	mutex_unlock(&tegra_dc_lock);

	return (cnt > 1);
}

/* get the stride size of a window.
 * return: stride size in bytes for window win. or 0 if unavailble. */
int tegra_dc_get_stride(struct tegra_dc *dc, unsigned win)
{
	u32 stride;

	if (!dc->enabled)
		return 0;
	BUG_ON(win > DC_N_WINDOWS);
	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
#ifdef CONFIG_TEGRA_NVDISPLAY
	stride = tegra_nvdisp_get_linestride(dc, win);
#else
	tegra_dc_writel(dc, WINDOW_A_SELECT << win,
		DC_CMD_DISPLAY_WINDOW_HEADER);

	stride = tegra_dc_readl(dc, DC_WIN_LINE_STRIDE);
#endif
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
	return GET_LINE_STRIDE(stride);
}
EXPORT_SYMBOL(tegra_dc_get_stride);

struct tegra_dc *tegra_dc_get_dc(unsigned idx)
{
	if (idx < TEGRA_MAX_DC)
		return tegra_dcs[idx];
	else
		return NULL;
}
EXPORT_SYMBOL(tegra_dc_get_dc);

struct tegra_dc_win *tegra_dc_get_window(struct tegra_dc *dc, unsigned win)
{
	if (win >= DC_N_WINDOWS || !test_bit(win, &dc->valid_windows))
		return NULL;

#ifdef CONFIG_TEGRA_NVDISPLAY
	return &tegra_dc_windows[win];
#else
	return &dc->windows[win];
#endif
}
EXPORT_SYMBOL(tegra_dc_get_window);

bool tegra_dc_get_connected(struct tegra_dc *dc)
{
	return dc->connected;
}
EXPORT_SYMBOL(tegra_dc_get_connected);

bool tegra_dc_hpd(struct tegra_dc *dc)
{
	int hpd = false;
	int hotplug_state;

	if (WARN_ON(!dc || !dc->out))
		return false;

	rmb();
	hotplug_state = dc->out->hotplug_state;

	if (hotplug_state != TEGRA_HPD_STATE_NORMAL) {
		if (hotplug_state == TEGRA_HPD_STATE_FORCE_ASSERT)
			return true;
		if (hotplug_state == TEGRA_HPD_STATE_FORCE_DEASSERT)
			return false;
	}

	if (!dc->hotplug_supported)
		return true;

	if (dc->out_ops && dc->out_ops->hpd_state)
		hpd = dc->out_ops->hpd_state(dc);

	if (dc->out->hotplug_report)
		dc->out->hotplug_report(hpd);

	return hpd;
}
EXPORT_SYMBOL(tegra_dc_hpd);

#ifndef CONFIG_TEGRA_NVDISPLAY
static void tegra_dc_set_scaling_filter(struct tegra_dc *dc)
{
	unsigned i;
	unsigned v0 = 128;
	unsigned v1 = 0;

	/* linear horizontal and vertical filters */
	for (i = 0; i < 16; i++) {
		tegra_dc_writel(dc, (v1 << 16) | (v0 << 8),
				DC_WIN_H_FILTER_P(i));

		tegra_dc_writel(dc, v0,
				DC_WIN_V_FILTER_P(i));
		v0 -= 8;
		v1 += 8;
	}
}
#endif

static int _tegra_dc_config_frame_end_intr(struct tegra_dc *dc, bool enable)
{
	tegra_dc_get(dc);
	if (enable) {
		atomic_inc(&dc->frame_end_ref);
		tegra_dc_unmask_interrupt(dc, FRAME_END_INT);
	} else if (!atomic_dec_return(&dc->frame_end_ref))
		tegra_dc_mask_interrupt(dc, FRAME_END_INT);
	tegra_dc_put(dc);

	return 0;
}

#if defined(CONFIG_TEGRA_DC_CMU) || defined(CONFIG_TEGRA_DC_CMU_V2)
static struct tegra_dc_cmu *tegra_dc_get_cmu(struct tegra_dc *dc)
{
	if (dc->out->type == TEGRA_DC_OUT_FAKE_DP ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSIA ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSIB ||
		dc->out->type == TEGRA_DC_OUT_FAKE_DSI_GANGED ||
		dc->out->type == TEGRA_DC_OUT_NULL) {
#if defined(CONFIG_TEGRA_NVDISPLAY)
		tegra_nvdisp_get_default_cmu(&default_cmu);
#endif
		return &default_cmu;
	}
	if (dc->pdata->cmu && !dc->pdata->default_clr_space)
		return dc->pdata->cmu;
	else if (dc->pdata->cmu_adbRGB && dc->pdata->default_clr_space)
		return dc->pdata->cmu_adbRGB;
	else if (dc->out->type == TEGRA_DC_OUT_HDMI) {
#if defined(CONFIG_TEGRA_NVDISPLAY)
		tegra_nvdisp_get_default_cmu(&default_limited_cmu);
#endif
		return &default_limited_cmu;
	} else {
#if defined(CONFIG_TEGRA_NVDISPLAY)
		tegra_nvdisp_get_default_cmu(&default_cmu);
#endif
		return &default_cmu;
	}
}

void tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable)
{
	dc->cmu_enabled = cmu_enable;
#if defined(CONFIG_TEGRA_NVDISPLAY)
	dc->pdata->cmu_enable = cmu_enable;
	tegra_dc_cache_cmu(dc, tegra_dc_get_cmu(dc));
	tegra_nvdisp_update_cmu(dc, &dc->cmu);
#else
	tegra_dc_update_cmu(dc, tegra_dc_get_cmu(dc));
#endif
}
EXPORT_SYMBOL(tegra_dc_cmu_enable);
#else
#define tegra_dc_cmu_enable(dc, cmu_enable)
#endif

#ifdef CONFIG_TEGRA_DC_CMU
static void tegra_dc_cache_cmu(struct tegra_dc *dc,
				struct tegra_dc_cmu *src_cmu)
{
	if (&dc->cmu != src_cmu) /* ignore if it would require memmove() */
		memcpy(&dc->cmu, src_cmu, sizeof(*src_cmu));
	dc->cmu_dirty = true;
}

static void tegra_dc_set_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	u32 val;
	u32 i;

	for (i = 0; i < 256; i++) {
		val = LUT1_ADDR(i) | LUT1_DATA(cmu->lut1[i]);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT1);
	}

	tegra_dc_writel(dc, cmu->csc.krr, DC_COM_CMU_CSC_KRR);
	tegra_dc_writel(dc, cmu->csc.kgr, DC_COM_CMU_CSC_KGR);
	tegra_dc_writel(dc, cmu->csc.kbr, DC_COM_CMU_CSC_KBR);
	tegra_dc_writel(dc, cmu->csc.krg, DC_COM_CMU_CSC_KRG);
	tegra_dc_writel(dc, cmu->csc.kgg, DC_COM_CMU_CSC_KGG);
	tegra_dc_writel(dc, cmu->csc.kbg, DC_COM_CMU_CSC_KBG);
	tegra_dc_writel(dc, cmu->csc.krb, DC_COM_CMU_CSC_KRB);
	tegra_dc_writel(dc, cmu->csc.kgb, DC_COM_CMU_CSC_KGB);
	tegra_dc_writel(dc, cmu->csc.kbb, DC_COM_CMU_CSC_KBB);

	for (i = 0; i < 960; i++) {
		val = LUT2_ADDR(i) | LUT1_DATA(cmu->lut2[i]);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT2);
	}

	dc->cmu_dirty = false;
}

static void _tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	u32 val;

	if (!dc->cmu_enabled)
		return;

	tegra_dc_cache_cmu(dc, cmu);

	if (dc->cmu_dirty) {
		/* Disable CMU to avoid programming it while it is in use */
		val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
		if (val & CMU_ENABLE) {
			val &= ~CMU_ENABLE;
			tegra_dc_writel(dc, val,
					DC_DISP_DISP_COLOR_CONTROL);
			val = GENERAL_ACT_REQ;
			tegra_dc_writel(dc, val, DC_CMD_STATE_CONTROL);
			/*TODO: Sync up with vsync */
			mdelay(20);
		}
		dev_dbg(&dc->ndev->dev, "updating CMU cmu_dirty=%d\n",
			dc->cmu_dirty);

		tegra_dc_set_cmu(dc, &dc->cmu);
	}
}

void _tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable)
{
	dc->cmu_enabled = cmu_enable;
	_tegra_dc_update_cmu(dc, tegra_dc_get_cmu(dc));
	tegra_dc_set_color_control(dc);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
}
EXPORT_SYMBOL(_tegra_dc_cmu_enable);

int tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}

	tegra_dc_get(dc);

	_tegra_dc_update_cmu(dc, cmu);
	tegra_dc_set_color_control(dc);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_cmu);

static int _tegra_dc_update_cmu_aligned(struct tegra_dc *dc,
				struct tegra_dc_cmu *cmu,
				bool force)
{
	memcpy(&dc->cmu_shadow, cmu, sizeof(dc->cmu));
	dc->cmu_shadow_dirty = true;
	dc->cmu_shadow_force_update = dc->cmu_shadow_force_update || force;
	_tegra_dc_config_frame_end_intr(dc, true);

	return 0;
}

int tegra_dc_update_cmu_aligned(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	int ret;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}
	ret = _tegra_dc_update_cmu_aligned(dc, cmu, false);
	mutex_unlock(&dc->lock);

	return ret;
}

EXPORT_SYMBOL(tegra_dc_update_cmu_aligned);
#else
#define tegra_dc_cache_cmu(dc, src_cmu)
#define tegra_dc_set_cmu(dc, cmu)
#define tegra_dc_update_cmu(dc, cmu)
#define _tegra_dc_enable_cmu(dc, cmu)
#define tegra_dc_update_cmu_aligned(dc, cmu)
#endif

int tegra_dc_set_hdr(struct tegra_dc *dc, struct tegra_dc_hdr *hdr,
						bool cache_dirty)
{
	int ret;

	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}
	trace_hdr_data_update(dc, hdr);
	if (cache_dirty) {
		dc->hdr.eotf = hdr->eotf;
		dc->hdr.static_metadata_id = hdr->static_metadata_id;
		memcpy(dc->hdr.static_metadata, hdr->static_metadata,
					sizeof(dc->hdr.static_metadata));
	} else if (dc->hdr.enabled == hdr->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}
	dc->hdr.enabled = hdr->enabled;
	dc->hdr_cache_dirty = true;
	if (!dc->hdr.enabled)
		memset(&dc->hdr, 0, sizeof(dc->hdr));
	ret = _tegra_dc_config_frame_end_intr(dc, true);

	mutex_unlock(&dc->lock);

	return ret;
}
EXPORT_SYMBOL(tegra_dc_set_hdr);

/* disable_irq() blocks until handler completes, calling this function while
 * holding dc->lock can deadlock. */
static inline void disable_dc_irq(const struct tegra_dc *dc)
{
	disable_irq(dc->irq);
}

u32 tegra_dc_get_syncpt_id(struct tegra_dc *dc, int i)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, i);
	BUG_ON(!win);
	return win->syncpt.id;
}
EXPORT_SYMBOL(tegra_dc_get_syncpt_id);

static u32 tegra_dc_incr_syncpt_max_locked(struct tegra_dc *dc, int i)
{
	u32 max;
	struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

	BUG_ON(!win);
	max = nvhost_syncpt_incr_max_ext(dc->ndev,
		win->syncpt.id, ((dc->enabled) ? 1 : 0));
	win->syncpt.max = max;

	return max;
}

u32 tegra_dc_incr_syncpt_max(struct tegra_dc *dc, int i)
{
	u32 max;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	max = tegra_dc_incr_syncpt_max_locked(dc, i);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return max;
}

void tegra_dc_incr_syncpt_min(struct tegra_dc *dc, int i, u32 val)
{
	struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

	BUG_ON(!win);
	mutex_lock(&dc->lock);

	tegra_dc_get(dc);
	while (win->syncpt.min < val) {
		win->syncpt.min++;
		nvhost_syncpt_cpu_incr_ext(dc->ndev, win->syncpt.id);
		}
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}

struct sync_fence *tegra_dc_create_fence(struct tegra_dc *dc, int i, u32 val)
{
	struct nvhost_ctrl_sync_fence_info syncpt;
	u32 id = tegra_dc_get_syncpt_id(dc, i);

	syncpt.id = id;
	syncpt.thresh = val;
	return nvhost_sync_create_fence(
			to_platform_device(dc->ndev->dev.parent),
			&syncpt, 1, dev_name(&dc->ndev->dev));
}

void
tegra_dc_config_pwm(struct tegra_dc *dc, struct tegra_dc_pwm_params *cfg)
{
	unsigned int ctrl;
	unsigned long out_sel;
	unsigned long cmd_state;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_get(dc);

	ctrl = ((cfg->period << PM_PERIOD_SHIFT) |
		(cfg->clk_div << PM_CLK_DIVIDER_SHIFT) |
		cfg->clk_select);

	/* The new value should be effected immediately */
	cmd_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, (cmd_state | (1 << 2)), DC_CMD_STATE_ACCESS);

	switch (cfg->which_pwm) {
	case TEGRA_PWM_PM0:
		/* Select the LM0 on PM0 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 0);
		out_sel |= (3 << 0);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM0_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM0_DUTY_CYCLE);
		break;
	case TEGRA_PWM_PM1:
		/* Select the LM1 on PM1 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 4);
		out_sel |= (3 << 4);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM1_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM1_DUTY_CYCLE);
		break;
	default:
		dev_err(&dc->ndev->dev, "Error: Need which_pwm\n");
		break;
	}
	tegra_dc_writel(dc, cmd_state, DC_CMD_STATE_ACCESS);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}
EXPORT_SYMBOL(tegra_dc_config_pwm);

void tegra_dc_set_out_pin_polars(struct tegra_dc *dc,
				const struct tegra_dc_out_pin *pins,
				const unsigned int n_pins)
{
	unsigned int i;

	int name;
	int pol;

	u32 pol1, pol3;

	u32 set1, unset1;
	u32 set3, unset3;

	set1 = set3 = unset1 = unset3 = 0;

	for (i = 0; i < n_pins; i++) {
		name = (pins + i)->name;
		pol  = (pins + i)->pol;

		/* set polarity by name */
		switch (name) {
		case TEGRA_DC_OUT_PIN_DATA_ENABLE:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set3 |= LSPI_OUTPUT_POLARITY_LOW;
			else
				unset3 |= LSPI_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_H_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LHS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LHS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_V_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LVS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LVS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_PIXEL_CLOCK:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LSC0_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LSC0_OUTPUT_POLARITY_LOW;
			break;
		default:
			printk("Invalid argument in function %s\n",
			       __FUNCTION__);
			break;
		}
	}

	pol1 = DC_COM_PIN_OUTPUT_POLARITY1_INIT_VAL;
	pol3 = DC_COM_PIN_OUTPUT_POLARITY3_INIT_VAL;

	pol1 |= set1;
	pol1 &= ~unset1;

	pol3 |= set3;
	pol3 &= ~unset3;

	tegra_dc_writel(dc, pol1, DC_COM_PIN_OUTPUT_POLARITY1);
	tegra_dc_writel(dc, pol3, DC_COM_PIN_OUTPUT_POLARITY3);
}

static struct tegra_dc_mode *tegra_dc_get_override_mode(struct tegra_dc *dc)
{
	unsigned long refresh;

	if (dc->out->type == TEGRA_DC_OUT_HDMI &&
			tegra_is_bl_display_initialized(dc->ndev->id)) {

		/* For seamless HDMI, read mode parameters from bootloader
		 * set DC configuration
		 */
		u32 val = 0;
		struct tegra_dc_mode *mode = &override_disp_mode[dc->out->type];
#ifdef CONFIG_TEGRA_NVDISPLAY
		struct clk *parent_clk = tegra_disp_clk_get(&dc->ndev->dev,
				dc->out->parent_clk ? : "plld2");
#else
		struct clk *parent_clk = clk_get_sys(NULL,
				dc->out->parent_clk ? : "pll_d2");
#endif
		memset(mode, 0, sizeof(struct tegra_dc_mode));
		mode->pclk = clk_get_rate(parent_clk);
		mode->rated_pclk = 0;

		tegra_dc_get(dc);
		val = tegra_dc_readl(dc, DC_DISP_REF_TO_SYNC);
		mode->h_ref_to_sync = val & 0xffff;
		mode->v_ref_to_sync = (val >> 16) & 0xffff;

		val = tegra_dc_readl(dc, DC_DISP_SYNC_WIDTH);
		mode->h_sync_width = val & 0xffff;
		mode->v_sync_width = (val >> 16) & 0xffff;

		val = tegra_dc_readl(dc, DC_DISP_BACK_PORCH);
		mode->h_back_porch = val & 0xffff;
		mode->v_back_porch = (val >> 16) & 0xffff;

		val = tegra_dc_readl(dc, DC_DISP_FRONT_PORCH);
		mode->h_front_porch = val & 0xffff;
		mode->v_front_porch = (val >> 16) & 0xffff;

		val = tegra_dc_readl(dc, DC_DISP_DISP_ACTIVE);
		mode->h_active = val & 0xffff;
		mode->v_active = (val >> 16) & 0xffff;

		/* Check the freq setup by the BL, 59.94 or 60Hz
		 * If 59.94, vmode needs to be FB_VMODE_1000DIV1001
		 * for seamless
		 */
		refresh = tegra_dc_calc_refresh(mode);
		if (refresh % 1000)
			mode->vmode |= FB_VMODE_1000DIV1001;

#ifdef CONFIG_TEGRA_DC_CMU
		/*
		 * Implicit contract between BL and us. If CMU is enabled,
		 * assume limited range. This sort of works because we know
		 * BL doesn't support YUV
		 */
		val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
		if (val & CMU_ENABLE)
			mode->vmode |= FB_VMODE_LIMITED_RANGE;
#endif

		tegra_dc_put(dc);
	}

	if (dc->out->type == TEGRA_DC_OUT_RGB  ||
		dc->out->type == TEGRA_DC_OUT_HDMI ||
		dc->out->type == TEGRA_DC_OUT_DP ||
		dc->out->type == TEGRA_DC_OUT_DSI  ||
		dc->out->type == TEGRA_DC_OUT_NULL)
		return override_disp_mode[dc->out->type].pclk ?
			&override_disp_mode[dc->out->type] : NULL;
	else
		return NULL;
}

static int tegra_dc_set_out(struct tegra_dc *dc, struct tegra_dc_out *out)
{
	struct tegra_dc_mode *mode;
	int err = 0;

	dc->out = out;

	if (dc->out->type == TEGRA_DC_OUT_HDMI &&
			tegra_is_bl_display_initialized(dc->ndev->id)) {
		/*
		 * Bootloader enables clk and host1x in seamless
		 * usecase. Below extra reference accounts for it
		 */
		tegra_dc_get(dc);
	}
/*
 * This config enables seamless feature only for
 * android usecase as a WAR for improper DSI initialization
 * in bootloader for L4T usecase.
 * Bug 200122858
 */
#ifdef CONFIG_ANDROID
	/*
	 * Seamless supporting panels can work in seamless mode
	 * only if BL initializes DC/DSI. If not, panel should
	 * go with complete initialization.
	 */
	if (dc->out->type == TEGRA_DC_OUT_DSI &&
			!tegra_is_bl_display_initialized(dc->ndev->id)) {
		dc->initialized = false;
	} else if (dc->out->type == TEGRA_DC_OUT_DSI &&
			tegra_is_bl_display_initialized(dc->ndev->id)) {
		/*
		 * In case of dsi->csi loopback support, force re-initialize
		 * all DSI controllers. So, set dc->initialized to false.
		 */
		if (dc->out->dsi->dsi_csi_loopback)
			dc->initialized = false;
		else
			dc->initialized = true;
	}
#endif
	mode = tegra_dc_get_override_mode(dc);

	if (mode) {
		tegra_dc_set_mode(dc, mode);

		/*
		 * Bootloader should and should only pass disp_params if
		 * it has initialized display controller.  Whenever we see
		 * override modes, we should skip things cause display resets.
		 */
		dev_info(&dc->ndev->dev, "Bootloader disp_param detected. "
				"Detected mode: %dx%d (on %dx%dmm) pclk=%d\n",
				dc->mode.h_active, dc->mode.v_active,
				dc->out->h_size, dc->out->v_size,
				dc->mode.pclk);
		dc->initialized = true;
	} else if (out->n_modes > 0) {
		/* For VRR panels, default mode is first in the list,
		 * and native panel mode is the last.
		 * Initialization must occur using the native panel mode. */
		if (dc->out->vrr) {
			tegra_dc_set_mode(dc,
				&dc->out->modes[dc->out->n_modes-1]);
			tegra_dc_setup_vrr(dc);
		} else
			tegra_dc_set_mode(dc, &dc->out->modes[0]);
	}
	tegra_dc_sor_instance(dc, out->type);

	switch (out->type) {
	case TEGRA_DC_OUT_RGB:
		dc->out_ops = &tegra_dc_rgb_ops;
		break;

	case TEGRA_DC_OUT_HDMI:
#if	defined(CONFIG_TEGRA_HDMI2_0)
		dc->out_ops = &tegra_dc_hdmi2_0_ops;
#elif defined(CONFIG_TEGRA_HDMI)
		dc->out_ops = &tegra_dc_hdmi_ops;
#endif
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
		if (tegra_bonded_out_dev(BOND_OUT_SOR1)) {
			dev_info(&dc->ndev->dev,
				"SOR1 instance is bonded out\n");
			dc->out_ops = NULL;
			err = -ENODEV;
		}
#endif
		break;

	case TEGRA_DC_OUT_DSI:
	case TEGRA_DC_OUT_FAKE_DSIA:
	case TEGRA_DC_OUT_FAKE_DSIB:
	case TEGRA_DC_OUT_FAKE_DSI_GANGED:
		dc->out_ops = &tegra_dc_dsi_ops;
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
		if (tegra_bonded_out_dev(BOND_OUT_DSI) ||
			tegra_bonded_out_dev(BOND_OUT_DSIB)) {
			dev_info(&dc->ndev->dev,
				"DSI instance is bonded out\n");
			dc->out_ops = NULL;
			err = -ENODEV;
		}
#endif
		break;

#ifdef CONFIG_TEGRA_DP
	case TEGRA_DC_OUT_FAKE_DP:
	case TEGRA_DC_OUT_DP:
		dc->out_ops = &tegra_dc_dp_ops;
		break;
#ifdef CONFIG_TEGRA_NVSR
	case TEGRA_DC_OUT_NVSR_DP:
		dc->out_ops = &tegra_dc_nvsr_ops;
		break;
#endif
#endif
#ifdef CONFIG_TEGRA_LVDS
	case TEGRA_DC_OUT_LVDS:
		dc->out_ops = &tegra_dc_lvds_ops;
		break;
#endif
#ifdef CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT
	case TEGRA_DC_OUT_NULL:
		dc->out_ops = &tegra_dc_null_ops;
		break;
#endif /*CONFIG_TEGRA_DC_FAKE_PANEL_SUPPORT*/

	default:
		dc->out_ops = NULL;
		break;
	}

#ifdef CONFIG_TEGRA_DC_CMU
	tegra_dc_cache_cmu(dc, tegra_dc_get_cmu(dc));
#endif

	if (dc->out_ops && dc->out_ops->init) {
		err = dc->out_ops->init(dc);
		if (err < 0) {
			dc->out = NULL;
			dc->out_ops = NULL;
			dev_err(&dc->ndev->dev,
				"Error: out->type:%d out_ops->init() failed\n",
				out->type);
			return err;
		}
	}

	return err;
}

int tegra_dc_get_head(const struct tegra_dc *dc)
{
	if (dc)
		return dc->ctrl_num;
	return -EINVAL;
}

/* returns on error: -EINVAL
 * on success: TEGRA_DC_OUT_RGB, TEGRA_DC_OUT_HDMI, ... */
int tegra_dc_get_out(const struct tegra_dc *dc)
{
	if (dc && dc->out)
		return dc->out->type;
	return -EINVAL;
}

bool tegra_dc_is_ext_dp_panel(const struct tegra_dc *dc)
{
	if (dc && dc->out)
		return dc->out->is_ext_dp_panel;
	return false;
}

unsigned tegra_dc_get_out_height(const struct tegra_dc *dc)
{
	unsigned height = 0;

	if (dc->out) {
		if (dc->out->height)
			height = dc->out->height;
		else if (dc->out->h_size && dc->out->v_size)
			height = dc->out->v_size;
	}

	return height;
}
EXPORT_SYMBOL(tegra_dc_get_out_height);

unsigned tegra_dc_get_out_width(const struct tegra_dc *dc)
{
	unsigned width = 0;

	if (dc->out) {
		if (dc->out->width)
			width = dc->out->width;
		else if (dc->out->h_size && dc->out->v_size)
			width = dc->out->h_size;
	}

	return width;
}
EXPORT_SYMBOL(tegra_dc_get_out_width);

unsigned tegra_dc_get_out_max_pixclock(const struct tegra_dc *dc)
{
	if (dc && dc->out)
		return dc->out->max_pixclock;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_max_pixclock);

void tegra_dc_enable_crc(struct tegra_dc *dc)
{
	u32 val;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	val = CRC_ALWAYS_ENABLE | CRC_INPUT_DATA_ACTIVE_DATA |
		CRC_ENABLE_ENABLE;
	tegra_dc_writel(dc, val, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	/* Register a client of frame_end interrupt */
	tegra_dc_config_frame_end_intr(dc, true);
}

void tegra_dc_disable_crc(struct tegra_dc *dc)
{
	/* Unregister a client of frame_end interrupt */
	tegra_dc_config_frame_end_intr(dc, false);

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	tegra_dc_writel(dc, 0x0, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}

u32 tegra_dc_read_checksum_latched(struct tegra_dc *dc)
{
	int crc = 0;

	if (!dc) {
		pr_err("Failed to get dc: NULL parameter.\n");
		goto crc_error;
	}

	/* If gated quitely return */
	if (!tegra_dc_is_powered(dc))
		return 0;

	reinit_completion(&dc->crc_complete);
	if (dc->crc_pending &&
	    wait_for_completion_interruptible(&dc->crc_complete)) {
		pr_err("CRC read interrupted.\n");
		goto crc_error;
	}

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);
	crc = tegra_dc_readl(dc, DC_COM_CRC_CHECKSUM_LATCHED);
	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
crc_error:
	return crc;
}
EXPORT_SYMBOL(tegra_dc_read_checksum_latched);

bool tegra_dc_windows_are_dirty(struct tegra_dc *dc, u32 win_act_req_mask)
{
	u32 val;

	if (tegra_platform_is_linsim())
		return false;

	val = tegra_dc_readl(dc, DC_CMD_STATE_CONTROL);
	if (val & (win_act_req_mask))
		return true;

	return false;
}

static inline void __maybe_unused
enable_dc_irq(const struct tegra_dc *dc)
{
	if (tegra_platform_is_fpga())
		/* Always disable DC interrupts on FPGA. */
		disable_irq(dc->irq);
	else
		enable_irq(dc->irq);
}

/* assumes dc->lock is already taken. */
static void _tegra_dc_vsync_enable(struct tegra_dc *dc)
{
	int vsync_irq;

	if (test_bit(V_BLANK_USER, &dc->vblank_ref_count))
		return; /* already set, nothing needs to be done */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		vsync_irq = MSF_INT;
	else
		vsync_irq = V_BLANK_INT;
	tegra_dc_hold_dc_out(dc);
	set_bit(V_BLANK_USER, &dc->vblank_ref_count);
	tegra_dc_unmask_interrupt(dc, vsync_irq);
}

int tegra_dc_vsync_enable(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);
	if (dc->enabled) {
		_tegra_dc_vsync_enable(dc);
		mutex_unlock(&dc->lock);
		return 0;
	}
	mutex_unlock(&dc->lock);
	return 1;
}

/* assumes dc->lock is already taken. */
static void _tegra_dc_vsync_disable(struct tegra_dc *dc)
{
	int vsync_irq;

	if (!test_bit(V_BLANK_USER, &dc->vblank_ref_count))
		return; /* already clear, nothing needs to be done */
	if (dc->out->type == TEGRA_DC_OUT_DSI)
		vsync_irq = MSF_INT;
	else
		vsync_irq = V_BLANK_INT;
	clear_bit(V_BLANK_USER, &dc->vblank_ref_count);
	if (!dc->vblank_ref_count)
		tegra_dc_mask_interrupt(dc, vsync_irq);
	tegra_dc_release_dc_out(dc);
}

void tegra_dc_vsync_disable(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);
	_tegra_dc_vsync_disable(dc);
	mutex_unlock(&dc->lock);
}

bool tegra_dc_has_vsync(struct tegra_dc *dc)
{
	return true;
}

/* assumes dc->lock is already taken. */
static void _tegra_dc_user_vsync_enable(struct tegra_dc *dc, bool enable)
{
	if (enable) {
		dc->out->user_needs_vblank++;
		init_completion(&dc->out->user_vblank_comp);
		_tegra_dc_vsync_enable(dc);
	} else {
		_tegra_dc_vsync_disable(dc);
		if (dc->out->user_needs_vblank > 0)
			dc->out->user_needs_vblank--;
	}
}

int tegra_dc_wait_for_vsync(struct tegra_dc *dc)
{
	unsigned long timeout_ms;
	unsigned long refresh; /* in 1000th Hz */
	int ret;

	mutex_lock(&dc->lp_lock);
	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		ret = -ENOTTY;
		goto out;
	}
	refresh = tegra_dc_calc_refresh(&dc->mode);
	/* time out if waiting took more than 2 frames */
	timeout_ms = DIV_ROUND_UP(2 * 1000000, refresh);
	_tegra_dc_user_vsync_enable(dc, true);
	mutex_unlock(&dc->lock);
	ret = wait_for_completion_interruptible_timeout(
		&dc->out->user_vblank_comp, msecs_to_jiffies(timeout_ms));
	mutex_lock(&dc->lock);
	_tegra_dc_user_vsync_enable(dc, false);
out:
	mutex_unlock(&dc->lock);
	mutex_unlock(&dc->lp_lock);
	return ret;
}

int _tegra_dc_wait_for_frame_end(struct tegra_dc *dc,
	u32 timeout_ms)
{
	int ret;

	reinit_completion(&dc->frame_end_complete);

	tegra_dc_get(dc);

	tegra_dc_flush_interrupt(dc, FRAME_END_INT);
	/* unmask frame end interrupt */
	_tegra_dc_config_frame_end_intr(dc, true);

	ret = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(timeout_ms));

	_tegra_dc_config_frame_end_intr(dc, false);

	tegra_dc_put(dc);

	return ret;
}

#if defined(CONFIG_TEGRA_NVSD) || defined(CONFIG_TEGRA_NVDISPLAY)
static void tegra_dc_prism_update_backlight(struct tegra_dc *dc)
{
	/* Do the actual brightness update outside of the mutex dc->lock */
	if (dc->out->sd_settings && !dc->out->sd_settings->bl_device &&
		dc->out->sd_settings->bl_device_name) {
		char *bl_device_name =
			dc->out->sd_settings->bl_device_name;
		dc->out->sd_settings->bl_device =
			get_backlight_device_by_name(bl_device_name);
	}

	if (dc->out->sd_settings && dc->out->sd_settings->bl_device) {
		struct backlight_device *bl = dc->out->sd_settings->bl_device;
		backlight_update_status(bl);
	}
}
#endif

void tegra_dc_set_act_vfp(struct tegra_dc *dc, int vfp)
{
	WARN_ON(!mutex_is_locked(&dc->lock));
	WARN_ON(vfp < dc->mode.v_ref_to_sync + 1);
	/* It's very unlikely that active vfp will need to
	 * be changed outside of vrr context */
	WARN_ON(!dc->out->vrr || !dc->out->vrr->capability);

	tegra_dc_writel(dc, WRITE_MUX_ACTIVE | READ_MUX_ACTIVE,
			DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, dc->mode.h_front_porch |
			(vfp << 16), DC_DISP_FRONT_PORCH);
	tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
			DC_CMD_STATE_ACCESS);
}

static void tegra_dc_vrr_extend_vfp(struct tegra_dc *dc)
{
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr || !vrr->capability)
		return;

	if (!vrr->enable)
		return;

	tegra_dc_set_act_vfp(dc, MAX_VRR_V_FRONT_PORCH);
}

int tegra_dc_get_v_count(struct tegra_dc *dc)
{
	u32     value;

	value = tegra_dc_readl(dc, DC_DISP_DISPLAY_DBG_TIMING);
	return (value & DBG_V_COUNT_MASK) >> DBG_V_COUNT_SHIFT;
}

static void tegra_dc_vrr_get_ts(struct tegra_dc *dc)
{
	struct timespec time_now;
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr || !vrr->capability ||
		(!vrr->enable && !vrr->lastenable))
		return;

	getnstimeofday(&time_now);
	vrr->fe_time_us = (s64)time_now.tv_sec * 1000000 +
				time_now.tv_nsec / 1000;
	vrr->v_count = tegra_dc_get_v_count(dc);
}

static void tegra_dc_vrr_sec(struct tegra_dc *dc)
{
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr || !vrr->capability)
		return;

	if (!vrr->enable && !vrr->fe_intr_req)
		return;

#ifdef CONFIG_TEGRA_NVDISPLAY
	cancel_delayed_work_sync(&dc->vrr_work);
#endif

	/* Decrement frame end interrupt refcount previously
	   requested by secure library */
	if (vrr->fe_intr_req) {
		_tegra_dc_config_frame_end_intr(dc, false);
		vrr->fe_intr_req = 0;
	}

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL) || defined(CONFIG_OTE_TRUSTY)
	if (te_is_secos_dev_enabled())
		te_vrr_sec();
#endif

	/* Increment frame end interrupt refcount requested
	   by secure library */
	if (vrr->fe_intr_req)
		_tegra_dc_config_frame_end_intr(dc, true);

#ifdef CONFIG_TEGRA_NVDISPLAY
	if (vrr->insert_frame)
		schedule_delayed_work(&dc->vrr_work,
			msecs_to_jiffies(vrr->insert_frame/1000));
#endif
}

static void tegra_dc_vblank(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work, struct tegra_dc, vblank_work);
#if defined(CONFIG_TEGRA_NVSD) || defined(CONFIG_TEGRA_NVDISPLAY)
	bool nvsd_updated = false;
#endif
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_get(dc);

	/* Clear the V_BLANK_FLIP bit of vblank ref-count if update is clean. */
	if (!tegra_dc_windows_are_dirty(dc, WIN_ALL_ACT_REQ))
		clear_bit(V_BLANK_FLIP, &dc->vblank_ref_count);

#ifdef CONFIG_TEGRA_NVDISPLAY
	/*
	 * If this HEAD has a pending COMMON_ACT_REQ, the COMMON channel state
	 * should be promoted on vblank.
	 */
	tegra_nvdisp_handle_common_channel_promotion(dc);

	if (dc->out->sd_settings) {
		if (dc->out->sd_settings->enable) {
			if ((dc->out->sd_settings->update_sd) ||
					(dc->out->sd_settings->phase_in_steps)) {
				tegra_dc_mask_interrupt(dc, SMARTDIM_INT);
				nvsd_updated = tegra_sd_update_brightness(dc);
				dc->out->sd_settings->update_sd = false;
				tegra_dc_unmask_interrupt(dc, SMARTDIM_INT);
			}
		}
	}
#endif
#ifdef CONFIG_TEGRA_NVSD
	/* Update the SD brightness */
	if (dc->out->sd_settings && !dc->out->sd_settings->use_vpulse2) {
		nvsd_updated = nvsd_update_brightness(dc);
		/* Ref-count vblank if nvsd is on-going. Otherwise, clean the
		 * V_BLANK_NVSD bit of vblank ref-count. */
		if (nvsd_updated) {
			set_bit(V_BLANK_NVSD, &dc->vblank_ref_count);
			tegra_dc_unmask_interrupt(dc, V_BLANK_INT);
		} else {
			clear_bit(V_BLANK_NVSD, &dc->vblank_ref_count);
		}
	}

	/* Mask vblank interrupt if ref-count is zero. */
	if (!dc->vblank_ref_count)
		tegra_dc_mask_interrupt(dc, V_BLANK_INT);
#endif /* CONFIG_TEGRA_NVSD */

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

#if defined(CONFIG_TEGRA_NVSD) || defined(CONFIG_TEGRA_NVDISPLAY)
	/* Do the actual brightness update outside of the mutex dc->lock */
	if (nvsd_updated)
		tegra_dc_prism_update_backlight(dc);
#endif
}

#define CSC_UPDATE_IF_CHANGED(entry, ENTRY) do { \
		if (cmu_active->csc.entry != cmu_shadow->csc.entry || \
			dc->cmu_shadow_force_update) { \
			cmu_active->csc.entry = cmu_shadow->csc.entry; \
			tegra_dc_writel(dc, \
				cmu_active->csc.entry, \
				DC_COM_CMU_CSC_##ENTRY); \
		} \
	} while (0)

static void _tegra_dc_handle_hdr(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_get(dc);

	if (dc->out_ops->set_hdr)
		dc->out_ops->set_hdr(dc);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	return;
}

static void tegra_dc_frame_end(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work,
		struct tegra_dc, frame_end_work);
#ifdef CONFIG_TEGRA_DC_CMU
	u32 val;
	u32 i;

	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_get(dc);

	if (dc->cmu_shadow_dirty) {
		struct tegra_dc_cmu *cmu_active = &dc->cmu;
		struct tegra_dc_cmu *cmu_shadow = &dc->cmu_shadow;

		for (i = 0; i < 256; i++) {
			if (cmu_active->lut1[i] != cmu_shadow->lut1[i] ||
				dc->cmu_shadow_force_update) {
				cmu_active->lut1[i] = cmu_shadow->lut1[i];
				val = LUT1_ADDR(i) |
					LUT1_DATA(cmu_shadow->lut1[i]);
				tegra_dc_writel(dc, val, DC_COM_CMU_LUT1);
			}
		}

		CSC_UPDATE_IF_CHANGED(krr, KRR);
		CSC_UPDATE_IF_CHANGED(kgr, KGR);
		CSC_UPDATE_IF_CHANGED(kbr, KBR);
		CSC_UPDATE_IF_CHANGED(krg, KRG);
		CSC_UPDATE_IF_CHANGED(kgg, KGG);
		CSC_UPDATE_IF_CHANGED(kbg, KBG);
		CSC_UPDATE_IF_CHANGED(krb, KRB);
		CSC_UPDATE_IF_CHANGED(kgb, KGB);
		CSC_UPDATE_IF_CHANGED(kbb, KBB);

		for (i = 0; i < 960; i++)
			if (cmu_active->lut2[i] != cmu_shadow->lut2[i] ||
				dc->cmu_shadow_force_update) {
				cmu_active->lut2[i] = cmu_shadow->lut2[i];
				val = LUT2_ADDR(i) |
					LUT2_DATA(cmu_active->lut2[i]);
				tegra_dc_writel(dc, val, DC_COM_CMU_LUT2);
			}

		dc->cmu_shadow_dirty = false;
		dc->cmu_shadow_force_update = false;
		_tegra_dc_config_frame_end_intr(dc, false);
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
#endif
	if (dc->hdr_cache_dirty) {
		_tegra_dc_handle_hdr(dc);
		_tegra_dc_config_frame_end_intr(dc, false);
		dc->hdr_cache_dirty = false;
	}
	return;
}

static void tegra_dc_one_shot_worker(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(
		to_delayed_work(work), struct tegra_dc, one_shot_work);
	mutex_lock(&dc->lock);

	/* memory client has gone idle */
	tegra_dc_clear_bandwidth(dc);

	if (dc->out_ops && dc->out_ops->idle) {
		tegra_dc_io_start(dc);
		dc->out_ops->idle(dc);
		tegra_dc_io_end(dc);
	}

	mutex_unlock(&dc->lock);
}

#if !defined(CONFIG_TEGRA_NVDISPLAY)
/* return an arbitrarily large number if count overflow occurs.
 * make it a nice base-10 number to show up in stats output */
static u64 tegra_dc_underflow_count(struct tegra_dc *dc, unsigned reg)
{
	unsigned count = tegra_dc_readl(dc, reg);

	tegra_dc_writel(dc, 0, reg);
	return ((count & 0x80000000) == 0) ? count : 10000000000ll;
}
#endif

static void tegra_dc_underflow_handler(struct tegra_dc *dc)
{
#if !defined(CONFIG_TEGRA_NVDISPLAY)

	const u32 masks[] = {
		WIN_A_UF_INT,
		WIN_B_UF_INT,
		WIN_C_UF_INT,
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
		WIN_D_UF_INT,
		HC_UF_INT,
		WIN_T_UF_INT,
#endif
	};
	int i;

	dc->stats.underflows++;
	if (dc->underflow_mask & WIN_A_UF_INT)
		dc->stats.underflows_a += tegra_dc_underflow_count(dc,
			DC_WINBUF_AD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_B_UF_INT)
		dc->stats.underflows_b += tegra_dc_underflow_count(dc,
			DC_WINBUF_BD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_C_UF_INT)
		dc->stats.underflows_c += tegra_dc_underflow_count(dc,
			DC_WINBUF_CD_UFLOW_STATUS);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (dc->underflow_mask & HC_UF_INT)
		dc->stats.underflows_h += tegra_dc_underflow_count(dc,
			DC_WINBUF_HD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_D_UF_INT)
		dc->stats.underflows_d += tegra_dc_underflow_count(dc,
			DC_WINBUF_DD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_T_UF_INT)
		dc->stats.underflows_t += tegra_dc_underflow_count(dc,
			DC_WINBUF_TD_UFLOW_STATUS);
#endif

	/* Check for any underflow reset conditions */
	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);
		if (WARN_ONCE(i >= ARRAY_SIZE(masks),
			"underflow stats unsupported"))
			break; /* bail if the table above is missing entries */
		if (!masks[i])
			continue; /* skip empty entries */

		if (dc->underflow_mask & masks[i]) {
			win->underflows++;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
			if (i < 3 && win->underflows > 4) {
				schedule_work(&dc->reset_work);
				/* reset counter */
				win->underflows = 0;
				trace_display_reset(dc);
			}
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
			if (i < 3 && win->underflows > 4) {
				trace_display_reset(dc);
				tegra_dc_writel(dc, UF_LINE_FLUSH,
						DC_DISP_DISP_MISC_CONTROL);
				tegra_dc_writel(dc, GENERAL_ACT_REQ,
						DC_CMD_STATE_CONTROL);

				tegra_dc_writel(dc, 0,
						DC_DISP_DISP_MISC_CONTROL);
				tegra_dc_writel(dc, GENERAL_ACT_REQ,
						DC_CMD_STATE_CONTROL);
			}
#endif
		} else {
			win->underflows = 0;
		}
	}

#else
	tegra_nvdisp_underflow_handler(dc);
#endif /* CONFIG_TEGRA_NVDISPLAY */

	/* Clear the underflow mask now that we've checked it. */
	tegra_dc_writel(dc, dc->underflow_mask, DC_CMD_INT_STATUS);
	dc->underflow_mask = 0;
	tegra_dc_unmask_interrupt(dc, ALL_UF_INT());
	trace_underflow(dc);
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static void tegra_dc_vpulse2(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work, struct tegra_dc, vpulse2_work);
#ifdef CONFIG_TEGRA_NVSD
	bool nvsd_updated = false;
#endif

	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_get(dc);

	/* Clear the V_PULSE2_FLIP if no update */
	if (!tegra_dc_windows_are_dirty(dc, WIN_ALL_ACT_REQ))
		clear_bit(V_PULSE2_FLIP, &dc->vpulse2_ref_count);

#ifdef CONFIG_TEGRA_NVSD
	/* Update the SD brightness */
	if (dc->out->sd_settings && dc->out->sd_settings->use_vpulse2) {
		nvsd_updated = nvsd_update_brightness(dc);

		if (nvsd_updated) {
			set_bit(V_PULSE2_NVSD, &dc->vpulse2_ref_count);
			tegra_dc_unmask_interrupt(dc, V_PULSE2_INT);
		} else {
			clear_bit(V_PULSE2_NVSD, &dc->vpulse2_ref_count);
		}
	}

	/* Mask vpulse2 interrupt if ref-count is zero. */
	if (!dc->vpulse2_ref_count)
		tegra_dc_mask_interrupt(dc, V_PULSE2_INT);
#endif /* CONFIG_TEGRA_NVSD */

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

#ifdef CONFIG_TEGRA_NVSD
	/* Do the actual brightness update outside of the mutex dc->lock */
	if (nvsd_updated)
		tegra_dc_prism_update_backlight(dc);
#endif
}
#endif

static void tegra_dc_process_vblank(struct tegra_dc *dc, ktime_t timestamp)
{
	/* pending user vblank, so wakeup */
	if (dc->out->user_needs_vblank) {
		dc->out->user_needs_vblank = false;
		complete(&dc->out->user_vblank_comp);
	}
	if (test_bit(V_BLANK_USER, &dc->vblank_ref_count)) {
#ifdef CONFIG_ADF_TEGRA
		tegra_adf_process_vblank(dc->adf, timestamp);
#endif
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
		tegra_dc_ext_process_vblank(dc->ndev->id, timestamp);
#endif
	}
}

int tegra_dc_config_frame_end_intr(struct tegra_dc *dc, bool enable)
{
	int ret;

	mutex_lock(&dc->lock);
	ret = _tegra_dc_config_frame_end_intr(dc, enable);
	mutex_unlock(&dc->lock);

	return ret;
}

static void tegra_dc_one_shot_irq(struct tegra_dc *dc, unsigned long status,
		ktime_t timestamp)
{
	if (status & MSF_INT)
		tegra_dc_process_vblank(dc, timestamp);

	if (status & V_BLANK_INT) {
		/* Sync up windows. */
		tegra_dc_trigger_windows(dc);

		/* Schedule any additional bottom-half vblank actvities. */
		queue_work(system_freezable_wq, &dc->vblank_work);
	}

	if (status & FRAME_END_INT) {
		/* Mark the frame_end as complete. */
		dc->crc_pending = false;
		if (!completion_done(&dc->frame_end_complete))
			complete(&dc->frame_end_complete);
		if (!completion_done(&dc->crc_complete))
			complete(&dc->crc_complete);

		if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
			tegra_dc_put(dc);

		queue_work(system_freezable_wq, &dc->frame_end_work);
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (status & V_PULSE2_INT)
		queue_work(system_freezable_wq, &dc->vpulse2_work);
#endif
}

static void tegra_dc_continuous_irq(struct tegra_dc *dc, unsigned long status,
		ktime_t timestamp)
{
	/* Schedule any additional bottom-half vblank actvities. */
	if (status & V_BLANK_INT) {
#ifdef CONFIG_TEGRA_NVDISPLAY
		if (status & SMARTDIM_INT) {
			if (dc->out->sd_settings)
				dc->out->sd_settings->update_sd = true;
		}
#endif
		queue_work(system_freezable_wq, &dc->vblank_work);
	}
	if (status & (V_BLANK_INT | MSF_INT)) {
		if (dc->out->user_needs_vblank) {
			dc->out->user_needs_vblank = false;
			complete(&dc->out->user_vblank_comp);
		}
		tegra_dc_process_vblank(dc, timestamp);
	}

	if (status & FRAME_END_INT) {
		struct timespec tm;
		ktime_get_ts(&tm);
		dc->frame_end_timestamp = timespec_to_ns(&tm);
		wake_up(&dc->timestamp_wq);

		if (!tegra_dc_windows_are_dirty(dc, WIN_ALL_ACT_REQ)) {
			if (dc->out->type == TEGRA_DC_OUT_DSI) {
				tegra_dc_vrr_get_ts(dc);
				tegra_dc_vrr_sec(dc);
			} else
				tegra_dc_vrr_extend_vfp(dc);
		}

		/* Mark the frame_end as complete. */
		if (!completion_done(&dc->frame_end_complete))
			complete(&dc->frame_end_complete);
		if (!completion_done(&dc->crc_complete))
			complete(&dc->crc_complete);

		tegra_dc_trigger_windows(dc);

		queue_work(system_freezable_wq, &dc->frame_end_work);
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (status & V_PULSE2_INT)
		queue_work(system_freezable_wq, &dc->vpulse2_work);
#endif
}

/* XXX: Not sure if we limit look ahead to 1 frame */
bool tegra_dc_is_within_n_vsync(struct tegra_dc *dc, s64 ts)
{
	BUG_ON(!dc->frametime_ns);
	return ((ts - dc->frame_end_timestamp) < dc->frametime_ns);
}

bool tegra_dc_does_vsync_separate(struct tegra_dc *dc, s64 new_ts, s64 old_ts)
{
	BUG_ON(!dc->frametime_ns);
	return (((new_ts - old_ts) > dc->frametime_ns)
		|| (div_s64((new_ts - dc->frame_end_timestamp), dc->frametime_ns)
			!= div_s64((old_ts - dc->frame_end_timestamp),
				dc->frametime_ns)));
}

static irqreturn_t tegra_dc_irq(int irq, void *ptr)
{
	ktime_t timestamp = ktime_get();
	struct tegra_dc *dc = ptr;
	unsigned long status;
	unsigned long underflow_mask;
	u32 val;
	int need_disable = 0;

#ifndef CONFIG_TEGRA_NVDISPLAY
	if (tegra_platform_is_fpga())
		return IRQ_NONE;
#endif
	mutex_lock(&dc->lock);
	if (!tegra_dc_is_powered(dc)) {
		mutex_unlock(&dc->lock);
		return IRQ_HANDLED;
	}

	tegra_dc_get(dc);

	if (!dc->enabled || !nvhost_module_powered_ext(dc->ndev)) {
		dev_dbg(&dc->ndev->dev, "IRQ when DC not powered!\n");
		status = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
		tegra_dc_writel(dc, status, DC_CMD_INT_STATUS);
		tegra_dc_put(dc);
		mutex_unlock(&dc->lock);
		return IRQ_HANDLED;
	}

	/* clear all status flags except underflow, save those for the worker */
	status = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
	tegra_dc_writel(dc, status & ~ALL_UF_INT(), DC_CMD_INT_STATUS);
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val & ~ALL_UF_INT(), DC_CMD_INT_MASK);

	/*
	 * Overlays can get thier internal state corrupted during and underflow
	 * condition.  The only way to fix this state is to reset the DC.
	 * if we get 4 consecutive frames with underflows, assume we're
	 * hosed and reset.
	 */
	underflow_mask = status & ALL_UF_INT();

	/* Check underflow */
	if (underflow_mask) {
		dc->underflow_mask |= underflow_mask;
		schedule_delayed_work(&dc->underflow_work,
			msecs_to_jiffies(1));
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE ||
		dc->out->flags & TEGRA_DC_OUT_NVSR_MODE)
		tegra_dc_one_shot_irq(dc, status, timestamp);
	else
		tegra_dc_continuous_irq(dc, status, timestamp);

	if (dc->nvsr)
		tegra_dc_nvsr_irq(dc->nvsr, status);

	/* update video mode if it has changed since the last frame */
	if (status & (FRAME_END_INT | V_BLANK_INT))
		if (tegra_dc_update_mode(dc))
			need_disable = 1; /* force display off on error */

	if (status & FRAME_END_INT) {
		dc->dbg_fe_count++;
		if (dc->disp_active_dirty) {
			tegra_dc_writel(dc, dc->mode.h_active |
				(dc->mode.v_active << 16), DC_DISP_DISP_ACTIVE);
			tegra_dc_writel(dc,
				GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

			dc->disp_active_dirty = false;
		}
	}

	if (status & V_BLANK_INT)
		trace_display_vblank(dc->ctrl_num,
			tegra_dc_readl(dc, DC_COM_RG_DPCA) >> 16);

	tegra_dc_put(dc);

#ifdef TEGRA_DC_USR_SHARED_IRQ
	/* user shared display ISR call-back */
	if (dc->isr_usr_cb)
		dc->isr_usr_cb(dc->ctrl_num, status, dc->isr_usr_pdt);
#endif /* TEGRA_DC_USR_SHARED_IRQ */

	mutex_unlock(&dc->lock);

	if (need_disable)
		tegra_dc_disable_irq_ops(dc, true);

	return IRQ_HANDLED;
}

void tegra_dc_set_color_control(struct tegra_dc *dc)
{
	u32 color_control;

	switch (dc->out->depth) {
	case 3:
		color_control = BASE_COLOR_SIZE111;
		break;

	case 6:
		color_control = BASE_COLOR_SIZE222;
		break;

	case 8:
		color_control = BASE_COLOR_SIZE332;
		break;

	case 9:
		color_control = BASE_COLOR_SIZE333;
		break;

	case 12:
		color_control = BASE_COLOR_SIZE444;
		break;

	case 15:
		color_control = BASE_COLOR_SIZE555;
		break;

	case 16:
		color_control = BASE_COLOR_SIZE565;
		break;

	case 18:
		color_control = BASE_COLOR_SIZE666;
		break;

	default:
		color_control = BASE_COLOR_SIZE888;
		break;
	}

	switch (dc->out->dither) {
	case TEGRA_DC_UNDEFINED_DITHER:
	case TEGRA_DC_DISABLE_DITHER:
		color_control |= DITHER_CONTROL_DISABLE;
		break;
	case TEGRA_DC_ORDERED_DITHER:
		color_control |= DITHER_CONTROL_ORDERED;
		break;
#ifdef CONFIG_TEGRA_DC_TEMPORAL_DITHER
	case TEGRA_DC_TEMPORAL_DITHER:
		color_control |= DITHER_CONTROL_TEMPORAL;
		break;
#else
	case TEGRA_DC_ERRDIFF_DITHER:
		/* The line buffer for error-diffusion dither is limited
		 * to 1280 pixels per line. This limits the maximum
		 * horizontal active area size to 1280 pixels when error
		 * diffusion is enabled.
		 */
		BUG_ON(dc->mode.h_active > 1280);
		color_control |= DITHER_CONTROL_ERRDIFF;
		break;
#endif
	default:
		dev_err(&dc->ndev->dev, "Error: Unsupported dithering mode\n");
	}

#ifdef CONFIG_TEGRA_DC_CMU
	if (dc->cmu_enabled)
		color_control |= CMU_ENABLE;
#endif

	tegra_dc_writel(dc, color_control, DC_DISP_DISP_COLOR_CONTROL);
}


/*
 * Due to the limitations in DSC architecture, program DSC block with predefined
 * values.
*/
void tegra_dc_dsc_init(struct tegra_dc *dc)
{
	struct tegra_dc_mode *mode = &dc->mode;
	u32 val;
	u32 slice_width, slice_height, chunk_size, hblank;
	u32 min_rate_buf_size, num_xtra_mux_bits, hrdelay;
	u32 initial_offset, final_offset;
	u32 initial_xmit_delay, initial_dec_delay;
	u32 initial_scale_value, final_scale;
	u32 scale_dec_interval, scale_inc_interval;
	u32 groups_per_line, total_groups, first_line_bpg_offset;
	u32 nfl_bpg_offset, slice_bpg_offset;
	u32 rc_model_size = DSC_DEF_RC_MODEL_SIZE;
	u32 delay_in_slice, output_delay, wrap_output_delay;
	u8 i, j;
	u8 bpp;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	u32 check_flatness;
#endif

	/* Link compression is only supported for DSI panels */
	if ((dc->out->type != TEGRA_DC_OUT_DSI) || !dc->out->dsc_en) {
		dev_info(&dc->ndev->dev,
			"Link compression not supported by the panel\n");
		return;
	}

	dev_info(&dc->ndev->dev, "Configuring DSC\n");
	/*
	 * Slice height and width are in pixel. When the whole picture is one
	 * slice, slice height and width should be equal to picture height or
	 * width.
	*/
	bpp = dc->out->dsc_bpp;
	slice_height = dc->out->slice_height;
	slice_width = (mode->h_active / dc->out->num_of_slices);
	val = DSC_VALID_SLICE_HEIGHT(slice_height) |
		DSC_VALID_SLICE_WIDTH(slice_width);
	tegra_dc_writel(dc, val, DSC_COM_DSC_SLICE_INFO);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/*
	 * Use RC overflow solution 2. Program overflow threshold values and
	 * enable flatness checking.
	 */
	check_flatness = ((3 - (slice_width % 3)) != 2);
	val = DC_DISP_SPARE0_VALID_OVERFLOW_THRES(DC_DISP_DEF_OVERFLOW_THRES) |
		DC_DISP_SPARE0_RC_SOLUTION_MODE(DC_SPARE0_RC_SOLUTION_2) |
		(check_flatness << 1) | 0x1;
	tegra_dc_writel(dc, val, DC_DISP_DISPLAY_SPARE0);
#endif
	/*
	 * Calculate chunk size based on slice width. Enable block prediction
	 * and set compressed bpp rate.
	 */
	chunk_size = DIV_ROUND_UP((slice_width * bpp), 8);
	val = DSC_VALID_BITS_PER_PIXEL(bpp << 4) |
		DSC_VALID_CHUNK_SIZE(chunk_size);
	if (dc->out->en_block_pred)
		val |= DSC_BLOCK_PRED_ENABLE;
	tegra_dc_writel(dc, val, DSC_COM_DSC_COMMON_CTRL);

	/* Set output delay */
	initial_xmit_delay = (4096 / bpp);
	if (slice_height == mode->v_active)
		initial_xmit_delay = 475;
	delay_in_slice = DIV_ROUND_UP(DSC_ENC_FIFO_SIZE * 8 * 3, bpp) +
		slice_width + initial_xmit_delay + DSC_START_PIXEL_POS;
	hblank = mode->h_sync_width + mode->h_front_porch + mode->h_back_porch;
	output_delay = ((delay_in_slice / slice_width) *
		(mode->h_active + hblank)) + (delay_in_slice % slice_width);
	wrap_output_delay = output_delay + 20;
	val = DSC_VALID_OUTPUT_DELAY(output_delay);
	val |= DSC_VALID_WRAP_OUTPUT_DELAY(wrap_output_delay);
	tegra_dc_writel(dc, val, DC_COM_DSC_DELAY);

	/* Set RC flatness info and bpg offset for first line of slice */
	first_line_bpg_offset = (bpp == 8) ? DSC_DEF_8BPP_FIRST_LINE_BPG_OFFS :
		DSC_DEF_12BPP_FIRST_LINE_BPG_OFFS;
	val = DSC_VALID_FLATNESS_MAX_QP(12) | DSC_VALID_FLATNESS_MIN_QP(3) |
		DSC_VALID_FIRST_LINE_BPG_OFFS(first_line_bpg_offset);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_FLATNESS_INFO);


	/* Set RC model offset values to be used at slice start and end */
	initial_offset = (bpp == 8) ? DSC_DEF_8BPP_INITIAL_OFFSET :
		DSC_DEF_12BPP_INITIAL_OFFSET;
	num_xtra_mux_bits = 198 + ((chunk_size * slice_height * 8 - 246) % 48);
	final_offset = rc_model_size - (initial_xmit_delay * bpp) +
		num_xtra_mux_bits;
	val = DSC_VALID_INITIAL_OFFSET(initial_offset) |
		DSC_VALID_FINAL_OFFSET(final_offset);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_OFFSET_INFO);

	/*
	 * DSC_SLICE_BPG_OFFSET:Bpg offset used to enforce slice bit constraint
	 * DSC_NFL_BPG_OFFSET:Non-first line bpg offset to use
	 */
	nfl_bpg_offset = DIV_ROUND_UP((first_line_bpg_offset << 11),
		(slice_height - 1));
	slice_bpg_offset = (rc_model_size - initial_offset +
		num_xtra_mux_bits) * (1 << 11);
	groups_per_line = slice_width / 3;
	total_groups = slice_height * groups_per_line;
	slice_bpg_offset = DIV_ROUND_UP(slice_bpg_offset, total_groups);
	val = DSC_VALID_SLICE_BPG_OFFSET(slice_bpg_offset) |
		DSC_VALID_NFL_BPG_OFFSET(nfl_bpg_offset);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_BPGOFF_INFO);

	/*
	 * INITIAL_DEC_DELAY:Num of pixels to delay the VLD on the decoder
	 * INITIAL_XMIT_DELAY:Num of pixels to delay the initial transmission
	 */
	min_rate_buf_size = rc_model_size - initial_offset +
		(initial_xmit_delay * bpp) +
		(groups_per_line * first_line_bpg_offset);
	hrdelay = DIV_ROUND_UP(min_rate_buf_size, bpp);
	initial_dec_delay = hrdelay - initial_xmit_delay;
	val = DSC_VALID_INITIAL_XMIT_DELAY(initial_xmit_delay) |
		DSC_VALID_INITIAL_DEC_DELAY(initial_dec_delay);
	tegra_dc_writel(dc, val, DSC_COM_DSC_RC_RELAY_INFO);

	/*
	 * SCALE_DECR_INTERVAL:Decrement scale factor every scale_decr_interval
	 * groups.
	 * INITIAL_SCALE_VALUE:Initial value for scale factor
	 * SCALE_INCR_INTERVAL:Increment scale factor every scale_incr_interval
	 * groups.
	 */
	initial_scale_value = (8 * rc_model_size) / (rc_model_size -
		initial_offset);
	scale_dec_interval = groups_per_line / (initial_scale_value - 8);
	val = DSC_VALID_SCALE_DECR_INTERVAL(scale_dec_interval) |
		DSC_VALID_INITIAL_SCALE_VALUE(initial_scale_value);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_SCALE_INFO);

	final_scale = (8 * rc_model_size) / (rc_model_size - final_offset);
	scale_inc_interval = (2048 * final_offset) /
		((final_scale - 9) * (slice_bpg_offset + nfl_bpg_offset));
	val = DSC_VALID_SCALE_INCR_INTERVAL(scale_inc_interval);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_SCALE_INFO_2);

	/* Set the RC parameters */
	val = DSC_VALID_RC_TGT_OFFSET_LO(3) | DSC_VALID_RC_TGT_OFFSET_HI(3) |
		DSC_VALID_RC_EDGE_FACTOR(6) |
		DSC_VALID_RC_QUANT_INCR_LIMIT1(11) |
		DSC_VALID_RC_QUANT_INCR_LIMIT0(11);
	tegra_dc_writel(dc, val, DC_COM_DSC_RC_PARAM_SET);

	for (i = 0, j = 0; j < DSC_MAX_RC_BUF_THRESH_REGS; j++) {
		val = DSC_VALID_RC_BUF_THRESH_0(dsc_rc_buf_thresh[i++]);
		val |= DSC_VALID_RC_BUF_THRESH_1(dsc_rc_buf_thresh[i++]);
		val |= DSC_VALID_RC_BUF_THRESH_2(dsc_rc_buf_thresh[i++]);
		val |= DSC_VALID_RC_BUF_THRESH_3(dsc_rc_buf_thresh[i++]);

		if (dsc_rc_buf_thresh_regs[j] == DC_COM_DSC_RC_BUF_THRESH_0)
			val |= DSC_VALID_RC_MODEL_SIZE(rc_model_size);
		tegra_dc_writel(dc, val, dsc_rc_buf_thresh_regs[j]);
	}

	for (i = 0, j = 0; j < DSC_MAX_RC_RANGE_CFG_REGS; j++) {
		val = DSC_VALID_RC_RANGE_PARAM_LO(
			SET_RC_RANGE_MIN_QP(dsc_rc_ranges_8bpp_8bpc[i][0]) |
			SET_RC_RANGE_MAX_QP(dsc_rc_ranges_8bpp_8bpc[i][1]) |
			SET_RC_RANGE_BPG_OFFSET(dsc_rc_ranges_8bpp_8bpc[i][2]));
		i++;
		val |= DSC_VALID_RC_RANGE_PARAM_HI(
			SET_RC_RANGE_MIN_QP(dsc_rc_ranges_8bpp_8bpc[i][0]) |
			SET_RC_RANGE_MAX_QP(dsc_rc_ranges_8bpp_8bpc[i][1]) |
			SET_RC_RANGE_BPG_OFFSET(dsc_rc_ranges_8bpp_8bpc[i][2]));
		i++;
		tegra_dc_writel(dc, val, dsc_rc_range_config[j]);
	}

	val = tegra_dc_readl(dc, DC_COM_DSC_UNIT_SET);
	val &= ~DSC_LINEBUF_DEPTH_8_BIT;
	val |= DSC_VALID_SLICE_NUM_MINUS1_IN_LINE(dc->out->num_of_slices - 1);
	val |= DSC_CHECK_FLATNESS2;
	val |= DSC_FLATNESS_FIX_EN;
	tegra_dc_writel(dc, val, DC_COM_DSC_UNIT_SET);

	dev_info(&dc->ndev->dev, "DSC configured\n");
}

void tegra_dc_en_dis_dsc(struct tegra_dc *dc, bool enable)
{
	u32 val;
	bool is_enabled = false, set_reg = false;

	if ((dc->out->type != TEGRA_DC_OUT_DSI) || !dc->out->dsc_en)
		return;

	val = tegra_dc_readl(dc, DC_COM_DSC_TOP_CTL);
	if (val & DSC_ENABLE)
		is_enabled = true;

	if (enable && !is_enabled) {
		val |= DSC_ENABLE;
		set_reg = true;
	} else if (!enable && is_enabled) {
		val &= ~DSC_ENABLE;
		set_reg = true;
	}

	if (set_reg) {
		dev_info(&dc->ndev->dev, "Link compression %s\n",
			enable ? "enabled" : "disabled");
		val &= ~DSC_AUTO_RESET;
		tegra_dc_writel(dc, val, DC_COM_DSC_TOP_CTL);
	}
}

#ifndef CONFIG_TEGRA_NVDISPLAY
static void tegra_dc_init_vpulse2_int(struct tegra_dc *dc)
{
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	u32 start, end;
	unsigned long val;

	val = V_PULSE2_H_POSITION(0) | V_PULSE2_LAST(0x1);
	tegra_dc_writel(dc, val, DC_DISP_V_PULSE2_CONTROL);

	start = dc->mode.v_ref_to_sync + dc->mode.v_sync_width +
		dc->mode.v_back_porch +	dc->mode.v_active;
	end = start + 1;
	val = V_PULSE2_START_A(start) + V_PULSE2_END_A(end);
	tegra_dc_writel(dc, val, DC_DISP_V_PULSE2_POSITION_A);

	val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
	val |= V_PULSE2_INT;
	tegra_dc_writel(dc, val , DC_CMD_INT_ENABLE);

	tegra_dc_mask_interrupt(dc, V_PULSE2_INT);
	val = tegra_dc_readl(dc, DC_DISP_DISP_SIGNAL_OPTIONS0);
	val |= V_PULSE_2_ENABLE;
	tegra_dc_writel(dc, val, DC_DISP_DISP_SIGNAL_OPTIONS0);
#endif
}

static int tegra_dc_init(struct tegra_dc *dc)
{
	int i;
	int int_enable;
	u32 val;

	tegra_dc_io_start(dc);
	tegra_dc_writel(dc, 0x00000100, DC_CMD_GENERAL_INCR_SYNCPT_CNTRL);
	tegra_dc_writel(dc, 0x00000100 | dc->vblank_syncpt,
			DC_CMD_CONT_SYNCPT_VSYNC);

	tegra_dc_writel(dc, 0x00004700, DC_CMD_INT_TYPE);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
	tegra_dc_writel(dc, WIN_A_OF_INT | WIN_B_OF_INT | WIN_C_OF_INT |
		WIN_T_UF_INT | WIN_D_UF_INT | HC_UF_INT |
		WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT,
		DC_CMD_INT_POLARITY);
#else
	tegra_dc_writel(dc, WIN_A_OF_INT | WIN_B_OF_INT | WIN_C_OF_INT |
		WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT,
		DC_CMD_INT_POLARITY);
#endif
	tegra_dc_writel(dc, 0x00202020, DC_DISP_MEM_HIGH_PRIORITY);
	tegra_dc_writel(dc, 0x00010101, DC_DISP_MEM_HIGH_PRIORITY_TIMER);
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	tegra_dc_writel(dc, 0x00000000, DC_DISP_DISP_MISC_CONTROL);
#endif
	/* enable interrupts for vblank, frame_end and underflows */
	int_enable = (FRAME_END_INT | V_BLANK_INT | ALL_UF_INT());
	/* for panels with one-shot mode enable tearing effect interrupt */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		int_enable |= MSF_INT;

	tegra_dc_writel(dc, int_enable, DC_CMD_INT_ENABLE);
	tegra_dc_writel(dc, ALL_UF_INT(), DC_CMD_INT_MASK);
	tegra_dc_init_vpulse2_int(dc);

	tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
		DC_CMD_STATE_ACCESS);

#if !defined(CONFIG_TEGRA_DC_BLENDER_GEN2)
	tegra_dc_writel(dc, 0x00000000, DC_DISP_BORDER_COLOR);
#else
	tegra_dc_writel(dc, 0x00000000, DC_DISP_BLEND_BACKGROUND_COLOR);
#endif

#ifdef CONFIG_TEGRA_DC_CMU
	if (dc->is_cmu_set_bl)
		_tegra_dc_update_cmu_aligned(dc, &dc->cmu, true);
	else
		_tegra_dc_update_cmu(dc, &dc->cmu);
	dc->is_cmu_set_bl = false;
#endif
	tegra_dc_set_color_control(dc);
	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_set_csc(dc, &win->csc);
		tegra_dc_set_lut(dc, win);
		tegra_dc_set_scaling_filter(dc);
	}

#ifdef CONFIG_TEGRA_DC_WIN_H
	/* Window H is set to window mode by default for t14x. */
	tegra_dc_writel(dc, WINH_CURS_SELECT(1),
			DC_DISP_BLEND_CURSOR_CONTROL);
#endif

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		struct tegra_dc_win *win = tegra_dc_get_window(dc, i);

		BUG_ON(!win);

		/* refuse to operate on invalid syncpts */
		if (WARN_ON(win->syncpt.id == NVSYNCPT_INVALID))
			continue;

		if (!nvhost_syncpt_read_ext_check(dc->ndev, win->syncpt.id, &val))
			win->syncpt.min = win->syncpt.max = val;
	}

	dc->crc_pending = false;

	trace_display_mode(dc, &dc->mode);

	if (dc->mode.pclk) {
		if (!dc->initialized) {
			if (tegra_dc_program_mode(dc, &dc->mode)) {
				tegra_dc_io_end(dc);
				dev_warn(&dc->ndev->dev,
					"%s: tegra_dc_program_mode failed\n",
					__func__);
				return -EINVAL;
			}
		} else {
			dev_info(&dc->ndev->dev, "DC initialized, "
					"skipping tegra_dc_program_mode.\n");
		}
	}

	/* Initialize SD AFTER the modeset.
	   nvsd_init handles the sd_settings = NULL case. */
	nvsd_init(dc, dc->out->sd_settings);

	tegra_dc_io_end(dc);

	return 0;
}

static bool _tegra_dc_controller_enable(struct tegra_dc *dc)
{
	int failed_init = 0;
	int i;

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC) && !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	struct device_node *np_dpaux;
#endif

	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return false;

	tegra_dc_unpowergate_locked(dc);

	if (dc->out->enable)
		dc->out->enable(&dc->ndev->dev);

	tegra_dc_setup_clk(dc, dc->clk);

	/* dc clk always on for continuous mode */
	if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE))
		tegra_dc_clk_enable(dc);
	else
#ifdef CONFIG_TEGRA_CORE_DVFS
		tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);
#else
		;
#endif

	tegra_dc_get(dc);

	tegra_dc_power_on(dc);

	/* do not accept interrupts during initialization */
	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	enable_dc_irq(dc);

	failed_init = tegra_dc_init(dc);
	if (failed_init) {
		tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);
		disable_irq_nosync(dc->irq);
		tegra_dc_clear_bandwidth(dc);
		if (dc->out && dc->out->disable)
			dc->out->disable(&dc->ndev->dev);
		tegra_dc_put(dc);
		if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE))
			tegra_dc_clk_disable(dc);
		else
#ifdef CONFIG_TEGRA_CORE_DVFS
			tegra_dvfs_set_rate(dc->clk, 0);
#else
			;
#endif
		dev_warn(&dc->ndev->dev,
			"%s: tegra_dc_init failed\n", __func__);
		return false;
	}

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC) && !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (dc->out->type != TEGRA_DC_OUT_DP) {
		int sor_num = tegra_dc_which_sor(dc);
		np_dpaux = of_find_node_by_path(
				sor_num ? DPAUX1_NODE : DPAUX_NODE);
		if (np_dpaux || !dc->ndev->dev.of_node)
			tegra_dpaux_pad_power(dc,
			sor_num ? TEGRA_DPAUX_INSTANCE_1 :
			TEGRA_DPAUX_INSTANCE_0, false);
		of_node_put(np_dpaux);
	}
#endif

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	/* force a full blending update */
	for (i = 0; i < DC_N_WINDOWS; i++)
		dc->blend.z[i] = -1;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_enable(dc->ext);
#endif

	/* initialize cursor to defaults, as driver depends on HW state */
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_START_ADDR);
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_START_ADDR_NS);
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_START_ADDR_HI);
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_START_ADDR_HI_NS);
#endif
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_POSITION);
	tegra_dc_writel(dc, 0, DC_DISP_CURSOR_POSITION_NS);
	tegra_dc_writel(dc, 0xffffff, DC_DISP_CURSOR_FOREGROUND); /* white */
	tegra_dc_writel(dc, 0x000000, DC_DISP_CURSOR_BACKGROUND); /* black */
	tegra_dc_writel(dc, 0, DC_DISP_BLEND_CURSOR_CONTROL);

	trace_display_enable(dc);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
!defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
!defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
#endif
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_dsc_init(dc);

	if (dc->out->postpoweron)
		dc->out->postpoweron(&dc->ndev->dev);

	if (dc->out_ops && dc->out_ops->postpoweron)
		dc->out_ops->postpoweron(dc);

	tegra_log_resume_time();

	tegra_dc_put(dc);

	tegra_disp_clk_prepare_enable(dc->emc_la_clk);

	return true;
}
#endif

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static bool _tegra_dc_controller_reset_enable(struct tegra_dc *dc)
{
	bool ret = true;

	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return false;

	if (dc->out->enable)
		dc->out->enable(&dc->ndev->dev);

	tegra_dc_setup_clk(dc, dc->clk);
	tegra_dc_clk_enable(dc);

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		mutex_lock(&tegra_dcs[1]->lock);
		disable_irq_nosync(tegra_dcs[1]->irq);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		mutex_lock(&tegra_dcs[0]->lock);
		disable_irq_nosync(tegra_dcs[0]->irq);
	}

	msleep(5);
	tegra_periph_reset_assert(dc->clk);
	msleep(2);
	if (tegra_platform_is_silicon()) {
		tegra_periph_reset_deassert(dc->clk);
		msleep(1);
	}

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		enable_dc_irq(tegra_dcs[1]);
		mutex_unlock(&tegra_dcs[1]->lock);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		enable_dc_irq(tegra_dcs[0]);
		mutex_unlock(&tegra_dcs[0]->lock);
	}

	enable_dc_irq(dc);

	if (tegra_dc_init(dc)) {
		dev_err(&dc->ndev->dev, "cannot initialize\n");
		ret = false;
	}

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	if (dc->out->postpoweron)
		dc->out->postpoweron(&dc->ndev->dev);

	/* force a full blending update */
	dc->blend.z[0] = -1;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_enable(dc->ext);
#endif

	if (!ret) {
		dev_err(&dc->ndev->dev, "initialization failed,disabling");
		_tegra_dc_controller_disable(dc);
	}

	trace_display_reset(dc);
	return ret;
}
#endif

static int _tegra_dc_set_default_videomode(struct tegra_dc *dc)
{
	if (dc->mode.pclk == 0) {
		switch (dc->out->type) {
		case TEGRA_DC_OUT_HDMI:
			/* No fallback mode. If no mode info available
			 * from bootloader or device tree,
			 * mode will be set by userspace during unblank.
			 */
			break;
		case TEGRA_DC_OUT_DP:
#ifdef CONFIG_TEGRA_NVDISPLAY
			break;
#endif
		case TEGRA_DC_OUT_NVSR_DP:
		case TEGRA_DC_OUT_FAKE_DP:
		case TEGRA_DC_OUT_NULL:
			return tegra_dc_set_fb_mode(dc, &tegra_dc_vga_mode, 0);

		/* Do nothing for other outputs for now */
		case TEGRA_DC_OUT_RGB:

		case TEGRA_DC_OUT_DSI:

		default:
			return false;
		}
	}

	return false;
}

int tegra_dc_set_default_videomode(struct tegra_dc *dc)
{
	return _tegra_dc_set_default_videomode(dc);
}

static bool _tegra_dc_enable(struct tegra_dc *dc)
{
	if (dc->mode.pclk == 0)
		return false;

	if (!dc->out)
		return false;

	if (dc->enabled)
		return true;

	dc->shutdown = false;

	if ((dc->out->type == TEGRA_DC_OUT_HDMI ||
		dc->out->type == TEGRA_DC_OUT_DP) &&
		!tegra_dc_hpd(dc))
		return false;

	pm_runtime_get_sync(&dc->ndev->dev);

#ifdef CONFIG_TEGRA_NVDISPLAY
	if (tegra_nvdisp_head_enable(dc)) {
#else
	if (!_tegra_dc_controller_enable(dc)) {
#endif
		pm_runtime_put_sync(&dc->ndev->dev);
		return false;
	}

	return true;
}

void tegra_dc_enable(struct tegra_dc *dc)
{
	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return;

	mutex_lock(&dc->lock);

	if (!dc->enabled)
		dc->enabled = _tegra_dc_enable(dc);

	mutex_unlock(&dc->lock);
	trace_display_mode(dc, &dc->mode);
}

static void tegra_dc_flush_syncpts_window(struct tegra_dc *dc, unsigned win)
{
	struct tegra_dc_win *w = tegra_dc_get_window(dc, win);
	u32 max;

	/* refuse to operate on invalid syncpts */
	if (WARN_ON(w->syncpt.id == NVSYNCPT_INVALID))
		return;

	/* flush any pending syncpt waits */
	max = tegra_dc_incr_syncpt_max_locked(dc, win);
	while (w->syncpt.min < w->syncpt.max) {
		trace_display_syncpt_flush(dc, w->syncpt.id,
			w->syncpt.min, w->syncpt.max);
		w->syncpt.min++;
		nvhost_syncpt_cpu_incr_ext(dc->ndev, w->syncpt.id);
	}
}

void tegra_dc_disable_window(struct tegra_dc *dc, unsigned win)
{
	struct tegra_dc_win *w = tegra_dc_get_window(dc, win);

	/* reset window bandwidth */
	w->bandwidth = 0;
	w->new_bandwidth = 0;

	/* disable windows */
	w->flags &= ~TEGRA_WIN_FLAG_ENABLED;

	/* flush pending syncpts */
	tegra_dc_flush_syncpts_window(dc, win);
}

static void _tegra_dc_controller_disable(struct tegra_dc *dc)
{
	unsigned i;

	tegra_dc_get(dc);

	if (atomic_read(&dc->holding)) {
		/* Force release all refs but the last one */
		atomic_set(&dc->holding, 1);
		tegra_dc_release_dc_out(dc);
	}

	if (dc->out && dc->out->prepoweroff)
		dc->out->prepoweroff();

	if (dc->out_ops && dc->out_ops->vrr_enable &&
		dc->out->vrr && dc->out->vrr->capability) {
		dc->out_ops->vrr_enable(dc, 0);
		/* TODO: Fix properly. Bug 1644102. */
		tegra_dc_set_act_vfp(dc, dc->mode.v_front_porch);
	}

	if (dc->out_ops && dc->out_ops->disable)
		dc->out_ops->disable(dc);

	if (tegra_powergate_is_powered(dc->powergate_id))
		tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	disable_irq_nosync(dc->irq);

	tegra_dc_clear_bandwidth(dc);

	if (dc->out && dc->out->disable)
		dc->out->disable(&dc->ndev->dev);

	for_each_set_bit(i, &dc->valid_windows, DC_N_WINDOWS) {
		tegra_dc_disable_window(dc, i);
	}
	trace_display_disable(dc);

	if (dc->out_ops && dc->out_ops->postpoweroff)
		dc->out_ops->postpoweroff(dc);

#ifdef CONFIG_TEGRA_NVDISPLAY
	/* clear the windows ownership from head*/
	tegra_nvdisp_head_disable(dc);
#endif

	/* clean up tegra_dc_vsync_enable() */
	while (dc->out->user_needs_vblank > 0)
		_tegra_dc_user_vsync_enable(dc, false);

	if (test_bit(V_BLANK_USER, &dc->vblank_ref_count)) {
		tegra_dc_release_dc_out(dc);
		clear_bit(V_BLANK_USER, &dc->vblank_ref_count);
	}

	tegra_dc_put(dc);

#ifndef CONFIG_TEGRA_NVDISPLAY
	/* disable always on dc clk in continuous mode */
	if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE))
		tegra_dc_clk_disable(dc);
	else
#ifdef CONFIG_TEGRA_CORE_DVFS
		tegra_dvfs_set_rate(dc->clk, 0);
#else
		;
#endif

	tegra_disp_clk_disable_unprepare(dc->emc_la_clk);
#endif
}

void tegra_dc_stats_enable(struct tegra_dc *dc, bool enable)
{
#if 0 /* underflow interrupt is already enabled by dc reset worker */
	u32 val;
	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		if (enable)
			val |= (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		else
			val &= ~(WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		tegra_dc_writel(dc, val, DC_CMD_INT_ENABLE);
	}
#endif
}

bool tegra_dc_stats_get(struct tegra_dc *dc)
{
#if 0 /* right now it is always enabled */
	u32 val;
	bool res;

	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		res = !!(val & (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT));
	} else {
		res = false;
	}

	return res;
#endif
	return true;
}

/* blank selected windows by disabling them */
void tegra_dc_blank(struct tegra_dc *dc, unsigned windows)
{
	struct tegra_dc_win *dcwins[DC_N_WINDOWS];
	struct tegra_dc_win blank_win;
	unsigned i;
	unsigned long int blank_windows;
	int nr_win = 0;

	/* YUV420 10bpc variables */
	int yuv_flag = dc->mode.vmode & FB_VMODE_YUV_MASK;
	bool yuv_420_10b_path = false;
	int fb_win_idx = -1;
	int fb_win_pos = -1;

	if (dc->yuv_bypass && yuv_flag == (FB_VMODE_Y420 | FB_VMODE_Y30))
		yuv_420_10b_path = true;

#ifdef CONFIG_TEGRA_NVDISPLAY
	if (dc->shutdown)
		yuv_420_10b_path = false;
#endif

	if (yuv_420_10b_path) {
		u32 active_width = dc->mode.h_active;
		u32 active_height = dc->mode.v_active;

		blank_win = *tegra_fb_get_blank_win(dc->fb);

		/*
		 * 420 10bpc blank frame statically
		 * created for this pixel format
		 */
		blank_win.h.full = dfixed_const(1);
		blank_win.w.full = dfixed_const(active_width);
		blank_win.fmt = TEGRA_WIN_FMT_B8G8R8A8;
		blank_win.out_w = active_width;
		blank_win.out_h = active_height;

		dcwins[0] = &blank_win;
		fb_win_idx = dcwins[0]->idx;
		nr_win++;
	}

	blank_windows = windows & dc->valid_windows;

	if (!blank_windows)
		return;

	for_each_set_bit(i, &blank_windows, DC_N_WINDOWS) {
		dcwins[nr_win] = tegra_dc_get_window(dc, i);
		if (!dcwins[nr_win])
			continue;
		/*
		 * Prevent disabling the YUV410 10bpc window in case
		 * it is also in blank_windows, additionally, prevent
		 * adding it to the list twice.
		 */
		if (fb_win_idx == dcwins[nr_win]->idx) {
			fb_win_pos = i;
			continue;
		}
		dcwins[nr_win++]->flags &= ~TEGRA_WIN_FLAG_ENABLED;
	}

#ifdef CONFIG_TEGRA_NVDISPLAY
	if (dc->shutdown) {
		if ((dc->out->type == TEGRA_DC_OUT_HDMI) ||
			(dc->out->type == TEGRA_DC_OUT_DP))
			if (dc->out_ops && dc->out_ops->shutdown_interface)
				dc->out_ops->shutdown_interface(dc);
	}
#endif

	/* Skip update for linsim */
	if (!tegra_platform_is_linsim()) {
		tegra_dc_update_windows(dcwins, nr_win, NULL, true);
		tegra_dc_sync_windows(dcwins, nr_win);
	}
	tegra_dc_program_bandwidth(dc, true);

	/*
	 * Disable, reset bandwidth and advance pending syncpoints
	 * of all windows. In case the statically created 420 10bpc
	 * is also present in blank_windows, only advance syncpoints.
	 */
	for_each_set_bit(i, &blank_windows, DC_N_WINDOWS) {
		if (fb_win_pos == i) {
			tegra_dc_flush_syncpts_window(dc, i);
			continue;
		}
		tegra_dc_disable_window(dc, i);
	}
}

int tegra_dc_restore(struct tegra_dc *dc)
{
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	return tegra_dc_ext_restore(dc->ext);
#else
	return 0;
#endif
}

static void _tegra_dc_disable(struct tegra_dc *dc)
{
#ifdef CONFIG_TEGRA_DC_CMU
	/* power down resets the registers, setting to true
	 * causes CMU to be restored in tegra_dc_init(). */
	dc->cmu_dirty = true;
#endif
	tegra_dc_get(dc);
	_tegra_dc_controller_disable(dc);
	tegra_dc_put(dc);

	tegra_dc_powergate_locked(dc);

	pm_runtime_put(&dc->ndev->dev);

	tegra_log_suspend_entry_time();
}

void tegra_dc_disable(struct tegra_dc *dc)
{
	dc->shutdown = true;
	tegra_dc_disable_irq_ops(dc, false);
}

static void tegra_dc_disable_irq_ops(struct tegra_dc *dc, bool from_irq)
{
	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return;

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	if (!tegra_dc_ext_disable(dc->ext))
		tegra_dc_blank(dc, BLANK_ALL);
#else
	tegra_dc_blank(dc, BLANK_ALL);
#endif

	if (dc->cursor.enabled)
		tegra_dc_cursor_suspend(dc);

	/* it's important that new underflow work isn't scheduled before the
	 * lock is acquired. */
	cancel_delayed_work_sync(&dc->underflow_work);


	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		mutex_lock(&dc->one_shot_lock);
		cancel_delayed_work_sync(&dc->one_shot_work);
	}

	mutex_lock(&dc->lp_lock);
	mutex_lock(&dc->lock);

	if (dc->enabled) {
		dc->enabled = false;
		dc->blanked = false;

		if (!dc->suspended)
			_tegra_dc_disable(dc);
	}

#ifdef CONFIG_SWITCH
	if (dc->switchdev_registered)
		switch_set_state(&dc->modeset_switch, 0);
#endif
	mutex_unlock(&dc->lock);
	mutex_unlock(&dc->lp_lock);
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		mutex_unlock(&dc->one_shot_lock);
	if (!from_irq)
		synchronize_irq(dc->irq);
	trace_display_mode(dc, &dc->mode);

	/* disable pending clks due to uncompleted frames */
	while (tegra_platform_is_silicon() && atomic_read(&dc->enable_count))
		tegra_dc_put(dc);
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static void tegra_dc_reset_worker(struct work_struct *work)
{
	struct tegra_dc *dc =
		container_of(work, struct tegra_dc, reset_work);

	unsigned long val = 0;

	mutex_lock(&shared_lock);

	dev_warn(&dc->ndev->dev,
		"overlay stuck in underflow state.  resetting.\n");

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_disable(dc->ext);
#endif

	mutex_lock(&dc->lock);

	if (dc->enabled == false)
		goto unlock;

	dc->enabled = false;

	/*
	 * off host read bus
	 */
	val = tegra_dc_readl(dc, DC_CMD_CONT_SYNCPT_VSYNC);
	val &= ~(0x00000100);
	tegra_dc_writel(dc, val, DC_CMD_CONT_SYNCPT_VSYNC);

	/*
	 * set DC to STOP mode
	 */
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);

	msleep(10);

	_tegra_dc_controller_disable(dc);

	/* _tegra_dc_controller_reset_enable deasserts reset */
	_tegra_dc_controller_reset_enable(dc);

	dc->enabled = true;

	/* reopen host read bus */
	val = tegra_dc_readl(dc, DC_CMD_CONT_SYNCPT_VSYNC);
	val &= ~(0x00000100);
	val |= 0x100;
	tegra_dc_writel(dc, val, DC_CMD_CONT_SYNCPT_VSYNC);

unlock:
	mutex_unlock(&dc->lock);
	mutex_unlock(&shared_lock);
	trace_display_reset(dc);
}
#endif

static void tegra_dc_underflow_worker(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(
		to_delayed_work(work), struct tegra_dc, underflow_work);

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	if (dc->enabled)
		tegra_dc_underflow_handler(dc);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);
}

static void (*flip_callback)(void);
static spinlock_t flip_callback_lock;
static bool init_tegra_dc_flip_callback_called;

static int __init init_tegra_dc_flip_callback(void)
{
	spin_lock_init(&flip_callback_lock);
	init_tegra_dc_flip_callback_called = true;
	return 0;
}

pure_initcall(init_tegra_dc_flip_callback);

int tegra_dc_set_flip_callback(void (*callback)(void))
{
	WARN_ON(!init_tegra_dc_flip_callback_called);

	spin_lock(&flip_callback_lock);
	flip_callback = callback;
	spin_unlock(&flip_callback_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_set_flip_callback);

int tegra_dc_unset_flip_callback(void)
{
	spin_lock(&flip_callback_lock);
	flip_callback = NULL;
	spin_unlock(&flip_callback_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_unset_flip_callback);

void tegra_dc_call_flip_callback(void)
{
	spin_lock(&flip_callback_lock);
	if (flip_callback)
		flip_callback();
	spin_unlock(&flip_callback_lock);
}
EXPORT_SYMBOL(tegra_dc_call_flip_callback);

#ifdef CONFIG_SWITCH
static ssize_t switch_modeset_print_mode(struct switch_dev *sdev, char *buf)
{
	struct tegra_dc *dc =
		container_of(sdev, struct tegra_dc, modeset_switch);

	if (!sdev->state)
		return sprintf(buf, "offline\n");

	return sprintf(buf, "%dx%d\n", dc->mode.h_active, dc->mode.v_active);
}
#endif

/* enables pads and clocks to perform DDC/I2C */
int tegra_dc_ddc_enable(struct tegra_dc *dc, bool enabled)
{
	int ret = -ENOSYS;
	if (dc->out_ops) {
		if (enabled && dc->out_ops->ddc_enable)
			ret = dc->out_ops->ddc_enable(dc);
		else if (!enabled && dc->out_ops->ddc_disable)
			ret = dc->out_ops->ddc_disable(dc);
	}
	return ret;
}

#if IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
int tegra_dc_slgc_disp0(struct notifier_block *nb,
	unsigned long unused0, void *unused1)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	struct tegra_dc *dc = container_of(nb, struct tegra_dc, slgc_notifier);
	u32 val;

	tegra_dc_get(dc);

	val = tegra_dc_readl(dc, DC_COM_DSC_TOP_CTL);
	val |= DSC_SLCG_OVERRIDE;
	tegra_dc_writel(dc, val, DC_COM_DSC_TOP_CTL); /* set */
	/* flush the previous write */
	(void)tegra_dc_readl(dc, DC_CMD_DISPLAY_COMMAND);
	val &= ~DSC_SLCG_OVERRIDE;
	tegra_dc_writel(dc, val, DC_COM_DSC_TOP_CTL); /* restore */

	tegra_dc_put(dc);
#endif
	return NOTIFY_OK;
}
#endif

int tegra_dc_update_winmask(struct tegra_dc *dc, unsigned long winmask)
{
	struct tegra_dc *dc_other;
	struct tegra_dc_win *win;
	int i, j, ret = 0;

#ifndef CONFIG_TEGRA_NVDISPLAY
	return -EINVAL;
#endif /* CONFIG_TEGRA_NVDISPLAY */

	/* check that dc is not NULL and do range check */
	if (!dc || (winmask >= (1 << DC_N_WINDOWS)))
		return -EINVAL;

	mutex_lock(&dc->lock);
	if ((!dc->ndev) || (dc->enabled)) {
		ret = -EINVAL;
		goto exit;
	}

	/* check requested=enabled windows NOT owned by other dcs */
	for_each_set_bit(i, &winmask, DC_N_WINDOWS) {
		j = dc->ndev->id;
		win = tegra_dc_get_window(dc, i);
		/* is window already owned by this dc? */
		if (win && win->dc && (win->dc == dc))
			continue;
		/* is window already owned by other dc? */
		for (j = 0; j < TEGRA_MAX_DC; j++) {
			dc_other = tegra_dc_get_dc(j);
			if (!dc_other)
				continue;
			if (!dc_other->pdata) {
				ret = -EINVAL;
				goto exit;
			}
			/* found valid dc, does it own window=i? */
			if ((dc_other->pdata->win_mask >> i) & 0x1) {
				dev_err(&dc->ndev->dev,
					"win[%d] already on fb%d\n", i, j);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	/* attach window happens on device enable call and
	 * detach window happens on device disable call
	 */

	dc->pdata->win_mask = winmask;
	dc->valid_windows = winmask;
	/* cleanup the valid window bits */
	if (!winmask) {
		/* disable the fb win_index */
		tegra_fb_set_win_index(dc, winmask);
		dc->pdata->fb->win = -1;
	}

exit:
	mutex_unlock(&dc->lock);
	return ret;
}

struct clk *tegra_disp_of_clk_get_by_name(struct device_node *np,
						const char *name)
{
#ifdef CONFIG_TEGRA_NVDISPLAY
	if (!tegra_bpmp_running())
		return of_clk_get_by_name(np, "clk32k_in");
#endif
	return of_clk_get_by_name(np, name);
}

struct clk *tegra_disp_clk_get(struct device *dev, const char *id)
{
#ifdef CONFIG_TEGRA_NVDISPLAY
	struct clk *disp_clk;

	if (!tegra_bpmp_running()) {
		return of_clk_get_by_name(dev->of_node, "clk32k_in");
	} else {
		disp_clk = devm_clk_get(dev, id);
		if (IS_ERR_OR_NULL(disp_clk))
			pr_err("Failed to get %s clk\n", id);
		return disp_clk;
	}

#else
	return clk_get(dev, id);
#endif
}

void tegra_disp_clk_put(struct device *dev, struct clk *clk)
{
#ifdef CONFIG_TEGRA_NVDISPLAY
	if (tegra_platform_is_silicon() && tegra_bpmp_running())
		devm_clk_put(dev, clk);
#else
	return clk_put(clk);
#endif
}

static int tegra_dc_probe(struct platform_device *ndev)
{
	struct tegra_dc *dc;
	struct tegra_dc_mode *mode;
	struct tegra_dc_platform_data *dt_pdata = NULL;
	struct clk *clk;
#ifndef CONFIG_TEGRA_ISOMGR
	struct clk *emc_clk;
#elif !defined(CONFIG_TEGRA_NVDISPLAY)
	int isomgr_client_id = -1;
#endif
	struct clk *emc_la_clk;
	struct device_node *np = ndev->dev.of_node;
	struct resource *res;
	struct resource dt_res;
	struct resource *base_res;
	struct resource *fb_mem = NULL;
	char clk_name[16];
	int ret = 0;
	void __iomem *base;
	int irq;
	int i;
#if IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS) && !IS_ENABLED(CONFIG_TEGRA_NVDISPLAY)
	int partition_id_disa, partition_id_disb;
#endif

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	if (tegra_platform_is_linsim()) {
		dev_info(&ndev->dev, "DC instances are not present on linsim\n");
		return -ENODEV;
	}
#endif

	if (!np && !ndev->dev.platform_data) {
		dev_err(&ndev->dev, "no platform data\n");
		return -ENOENT;
	}

	/* Specify parameters for the maximum physical segment size. */
	ndev->dev.dma_parms = &tegra_dc_dma_parameters;

	dc = kzalloc(sizeof(struct tegra_dc), GFP_KERNEL);
	if (!dc) {
		dev_err(&ndev->dev, "can't allocate memory for tegra_dc\n");
		return -ENOMEM;
	}

	if (np) {
#ifdef CONFIG_OF
		irq = of_irq_to_resource(np, 0, NULL);
		if (!irq)
			goto err_free;
#endif

		ret = of_address_to_resource(np, 0, &dt_res);
		if (ret)
			goto err_free;

		ndev->id = tegra_dc_set(dc, -1);
		if (ndev->id < 0) {
			dev_err(&ndev->dev, "can't add dc\n");
			goto err_free;
		}

		dev_info(&ndev->dev, "Display dc.%08x registered with id=%d\n",
				(unsigned int)dt_res.start, ndev->id);
		res = &dt_res;

		dt_pdata = of_dc_parse_platform_data(ndev);
		if (dt_pdata == NULL)
			goto err_free;

#ifdef CONFIG_TEGRA_NVDISPLAY
		dc->ctrl_num = dt_pdata->ctrl_num;
#else
		if (dt_res.start == TEGRA_DISPLAY_BASE)
			dc->ctrl_num = 0;
		else if (dt_res.start == TEGRA_DISPLAY2_BASE)
			dc->ctrl_num = 1;
		else
			goto err_free;
#endif

	} else {

		dc->ctrl_num = ndev->id;

		irq = platform_get_irq_byname(ndev, "irq");
		if (irq <= 0) {
			dev_err(&ndev->dev, "no irq\n");
			ret = -ENOENT;
			goto err_free;
		}

		res = platform_get_resource_byname(ndev,
			IORESOURCE_MEM, "regs");
		if (!res) {
			dev_err(&ndev->dev, "no mem resource\n");
			ret = -ENOENT;
			goto err_free;
		}

		if (tegra_dc_set(dc, ndev->id) < 0) {
			dev_err(&ndev->dev, "can't add dc\n");
			goto err_free;
		}

	}

	base_res = request_mem_region(res->start, resource_size(res),
		ndev->name);
	if (!base_res) {
		dev_err(&ndev->dev, "request_mem_region failed\n");
		ret = -EBUSY;
		goto err_free;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&ndev->dev, "registers can't be mapped\n");
		ret = -EBUSY;
		goto err_release_resource_reg;
	}

#ifndef CONFIG_TEGRA_NVDISPLAY
	for (i = 0; i < DC_N_WINDOWS; i++)
		dc->windows[i].syncpt.id = NVSYNCPT_INVALID;

	if (TEGRA_DISPLAY_BASE == res->start) {
		dc->vblank_syncpt = NVSYNCPT_VBLANK0;
		dc->windows[0].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_a");
		dc->windows[1].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_b");
		dc->windows[2].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_c");
		dc->valid_windows = 0x07;
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
		dc->windows[3].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_d");
		dc->windows[4].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_h");
		dc->valid_windows |= 0x18;
#elif !defined(CONFIG_ARCH_TEGRA_2x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_3x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_11x_SOC)
		dc->windows[3].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp0_d");
		dc->valid_windows |= 0x08;
#endif
#if IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
		partition_id_disa = tegra_pd_get_powergate_id(tegra_disa_pd);
		if (partition_id_disa < 0)
			return -EINVAL;

		dc->powergate_id = partition_id_disa;
#ifdef CONFIG_TEGRA_ISOMGR
		isomgr_client_id = TEGRA_ISO_CLIENT_DISP_0;
#endif
		dc->slgc_notifier.notifier_call = tegra_dc_slgc_disp0;
		slcg_register_notifier(dc->powergate_id,
			&dc->slgc_notifier);
#endif
	} else if (TEGRA_DISPLAY2_BASE == res->start) {
		dc->vblank_syncpt = NVSYNCPT_VBLANK1;
		dc->windows[0].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp1_a");
		dc->windows[1].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp1_b");
		dc->windows[2].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp1_c");
		dc->valid_windows = 0x07;
#ifdef CONFIG_ARCH_TEGRA_14x_SOC
		dc->windows[4].syncpt.id =
			nvhost_get_syncpt_client_managed(ndev, "disp1_h");
		dc->valid_windows |= 0x10;
#endif
#if IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
		partition_id_disb = tegra_pd_get_powergate_id(tegra_disb_pd);
		if (partition_id_disb < 0)
			return -EINVAL;

		dc->powergate_id = partition_id_disb;
#endif
#ifdef CONFIG_TEGRA_ISOMGR
		isomgr_client_id = TEGRA_ISO_CLIENT_DISP_1;
#endif
	} else {
		dev_err(&ndev->dev,
			"Unknown base address %llx: unable to assign syncpt\n",
			(u64)res->start);
	}
#endif	/* !CONFIG_TEGRA_NVDISPLAY */

	if (np) {
		struct resource of_fb_res;
#ifdef CONFIG_TEGRA_NVDISPLAY
		tegra_get_fb_resource(&of_fb_res);
#else
		if (dc->ctrl_num == 0)
			tegra_get_fb_resource(&of_fb_res);
		else /* dc->ctrl_num == 1*/
			tegra_get_fb2_resource(&of_fb_res);
#endif

		fb_mem = kzalloc(sizeof(struct resource), GFP_KERNEL);
		if (fb_mem == NULL) {
			ret = -ENOMEM;
			goto err_iounmap_reg;
		}
		fb_mem->name = "fbmem";
		fb_mem->flags = IORESOURCE_MEM;
		fb_mem->start = (resource_size_t)of_fb_res.start;
		fb_mem->end = (resource_size_t)of_fb_res.end;
	} else {
		fb_mem = platform_get_resource_byname(ndev,
			IORESOURCE_MEM, "fbmem");
		if (fb_mem == NULL) {
			ret = -ENOMEM;
			goto err_iounmap_reg;
		}
	}

#ifdef CONFIG_TEGRA_NVDISPLAY
	snprintf(clk_name, sizeof(clk_name), "nvdisplay_p%u", dc->ctrl_num);
#else
	memset(clk_name, 0, sizeof(clk_name));
#endif
	clk = tegra_disp_clk_get(&ndev->dev, clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&ndev->dev, "can't get clock: %s\n", clk_name);
		ret = -ENOENT;
		goto err_iounmap_reg;
	}

	dc->clk = clk;
	dc->shift_clk_div.mul = dc->shift_clk_div.div = 1;
	/* Initialize one shot work delay, it will be assigned by dsi
	 * according to refresh rate later. */
	dc->one_shot_delay_ms = 40;

	dc->base_res = base_res;
	dc->base = base;
	dc->irq = irq;
	dc->ndev = ndev;
	dc->fb_mem = fb_mem;

	if (!np)
		dc->pdata = ndev->dev.platform_data;
	else
		dc->pdata = dt_pdata;

	dc->bw_kbps = 0;

#ifdef CONFIG_TEGRA_NVDISPLAY
	/* dc variables need to initialized before nvdisp init */
	ret = tegra_nvdisp_init(dc);
	if (ret)
		goto err_iounmap_reg;
#endif

	mutex_init(&dc->lock);
	mutex_init(&dc->one_shot_lock);
	mutex_init(&dc->lp_lock);
	init_completion(&dc->frame_end_complete);
	init_completion(&dc->crc_complete);
	init_waitqueue_head(&dc->wq);
	init_waitqueue_head(&dc->timestamp_wq);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	INIT_WORK(&dc->reset_work, tegra_dc_reset_worker);
#endif
	INIT_WORK(&dc->vblank_work, tegra_dc_vblank);
	dc->vblank_ref_count = 0;
	INIT_WORK(&dc->frame_end_work, tegra_dc_frame_end);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	INIT_WORK(&dc->vpulse2_work, tegra_dc_vpulse2);
#endif
	dc->vpulse2_ref_count = 0;
	INIT_DELAYED_WORK(&dc->underflow_work, tegra_dc_underflow_worker);
	INIT_DELAYED_WORK(&dc->one_shot_work, tegra_dc_one_shot_worker);
#ifdef CONFIG_TEGRA_NVDISPLAY
	INIT_DELAYED_WORK(&dc->vrr_work, tegra_nvdisp_vrr_work);
#endif
	tegra_dc_init_lut_defaults(&dc->fb_lut);

	dc->n_windows = DC_N_WINDOWS;
	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_win *tmp_win = &dc->tmp_wins[i];
#ifdef CONFIG_TEGRA_NVDISPLAY
		struct tegra_dc_win *win = &tegra_dc_windows[i];
#else
		struct tegra_dc_win *win = &dc->windows[i];
		win->dc = dc;
#endif
		if (!test_bit(i, &dc->valid_windows))
			win->flags |= TEGRA_WIN_FLAG_INVALID;
		else {
		win->idx = i;
		tmp_win->idx = i;
		tmp_win->dc = dc;
#if defined(CONFIG_TEGRA_CSC)
		tegra_dc_init_csc_defaults(&win->csc);
#endif
		tegra_dc_init_lut_defaults(&win->lut);
		}
	}

#if defined(CONFIG_TEGRA_CSC_V2)
	if (dc->pdata->cmu)
		dc->default_csc = dc->pdata->cmu->panel_csc;
	else
		tegra_nvdisp_init_csc_defaults(&dc->default_csc);
#endif

	platform_set_drvdata(ndev, dc);

#ifdef CONFIG_SWITCH
	dc->modeset_switch.name = dev_name(&ndev->dev);
	dc->modeset_switch.state = 0;
	dc->modeset_switch.print_state = switch_modeset_print_mode;
	ret = switch_dev_register(&dc->modeset_switch);
	if (ret < 0) {
		dev_err(&ndev->dev,
			"failed to register switch driver ret(%d)\n", ret);
		dc->switchdev_registered = false;
	} else
		dc->switchdev_registered = true;
#endif

	tegra_dc_feature_register(dc);

	if (dc->pdata->default_out) {
		if (dc->pdata->default_out->hotplug_init)
			dc->pdata->default_out->hotplug_init(&dc->ndev->dev);
		ret = tegra_dc_set_out(dc, dc->pdata->default_out);
		if (ret < 0) {
			dev_err(&dc->ndev->dev, "failed to initialize DC out ops\n");
			goto err_put_clk;
		}
	} else {
		dev_err(&ndev->dev,
			"No default output specified.  Leaving output disabled.\n");
	}
	dc->mode_dirty = false; /* ignore changes tegra_dc_set_out has done */
#ifdef CONFIG_TEGRA_NVDISPLAY
	nvdisp_register_backlight_notifier(dc);
#endif

	/* For HDMI|DP, hotplug always supported
	 * For eDP, hotplug is never supported
	 * Else GPIO# determines if hotplug supported
	 */
	if (dc->out->type == TEGRA_DC_OUT_HDMI)
		dc->hotplug_supported = true;
	else if (dc->out->type == TEGRA_DC_OUT_DP)
		dc->hotplug_supported = tegra_dc_is_ext_dp_panel(dc);
	else
		dc->hotplug_supported = dc->out->hotplug_gpio >= 0;

	if ((dc->pdata->flags & TEGRA_DC_FLAG_ENABLED) &&
			dc->out && dc->out->type == TEGRA_DC_OUT_LVDS) {
		struct fb_monspecs specs;
		struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);
		if (!tegra_edid_get_monspecs(lvds->edid, &specs))
			tegra_dc_set_fb_mode(dc, specs.modedb, false);
	}

#ifndef CONFIG_TEGRA_ISOMGR
		/*
		 * The emc is a shared clock, it will be set based on
		 * the requirements for each user on the bus.
		 */
		emc_clk = tegra_disp_clk_get(&ndev->dev, "emc");
		if (IS_ERR_OR_NULL(emc_clk)) {
			dev_err(&ndev->dev, "can't get emc clock\n");
			ret = -ENOENT;
			goto err_put_clk;
		}
		dc->emc_clk = emc_clk;
#endif
		/*
		 * The emc_la clock is being added to set the floor value
		 * for emc depending on the LA calculaions for each window
		 */
#ifdef CONFIG_TEGRA_NVDISPLAY
		emc_la_clk = tegra_disp_clk_get(&ndev->dev, "emc_latency");
#else
		emc_la_clk = tegra_disp_clk_get(&ndev->dev, "emc.la");
#endif
		if (IS_ERR_OR_NULL(emc_la_clk)) {
			dev_err(&ndev->dev, "can't get emc.la clock\n");
			ret = -ENOENT;
			goto err_put_clk;
		}
		dc->emc_la_clk = emc_la_clk;
		clk_set_rate(dc->emc_la_clk, 0);

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	dc->ext = tegra_dc_ext_register(ndev, dc);
	if (IS_ERR_OR_NULL(dc->ext)) {
		dev_warn(&ndev->dev, "Failed to enable Tegra DC extensions.\n");
		dc->ext = NULL;
	}
#endif

	/* interrupt handler must be registered before tegra_fb_register() */
	if (request_threaded_irq(irq, NULL, tegra_dc_irq, IRQF_ONESHOT,
			dev_name(&ndev->dev), dc)) {
		dev_err(&ndev->dev, "request_irq %d failed\n", irq);
		ret = -EBUSY;
		goto err_disable_dc;
	}
	disable_dc_irq(dc);

	tegra_pd_add_device(&ndev->dev);
	pm_runtime_use_autosuspend(&ndev->dev);
	pm_runtime_set_autosuspend_delay(&ndev->dev, 100);
	pm_runtime_enable(&ndev->dev);

#if defined(CONFIG_TEGRA_DC_CMU) || defined(CONFIG_TEGRA_DC_CMU_V2)
	/* if bootloader leaves this head enabled, then skip CMU programming. */
	dc->is_cmu_set_bl = (dc->pdata->flags & TEGRA_DC_FLAG_ENABLED) != 0;
	dc->cmu_enabled = dc->pdata->cmu_enable;
#endif

#if !defined(CONFIG_TEGRA_NVDISPLAY) && defined(CONFIG_TEGRA_ISOMGR)
	if (isomgr_client_id == -1) {
		dc->isomgr_handle = NULL;
	} else {
		dc->isomgr_handle = tegra_isomgr_register(isomgr_client_id,
			tegra_dc_calc_min_bandwidth(dc),
			tegra_dc_bandwidth_renegotiate, dc);
		if (IS_ERR(dc->isomgr_handle)) {
			dev_err(&dc->ndev->dev,
				"could not register isomgr. err=%ld\n",
				PTR_ERR(dc->isomgr_handle));
			ret = -ENOENT;
			goto err_put_clk;
		}
		dc->reserved_bw = tegra_dc_calc_min_bandwidth(dc);
		/*
		 * Use maximum value so we can try to reserve as much as
		 * needed until we are told by isomgr to backoff.
		 */
		dc->available_bw = UINT_MAX;
	}
#endif

	tegra_dc_create_debugfs(dc);

	dev_info(&ndev->dev, "probed\n");

	if (dc->pdata->fb) {
		if (dc->enabled && dc->pdata->fb->bits_per_pixel == -1) {
			unsigned long fmt;
			tegra_dc_writel(dc,
					WINDOW_A_SELECT << dc->pdata->fb->win,
					DC_CMD_DISPLAY_WINDOW_HEADER);

			fmt = tegra_dc_readl(dc, DC_WIN_COLOR_DEPTH);
			dc->pdata->fb->bits_per_pixel =
				tegra_dc_fmt_bpp(fmt);
		}

		mode = tegra_dc_get_override_mode(dc);
		if (mode) {
			dc->pdata->fb->xres = mode->h_active;
			dc->pdata->fb->yres = mode->v_active;
		}

#ifdef CONFIG_ADF_TEGRA
		tegra_dc_io_start(dc);
		dc->adf = tegra_adf_init(ndev, dc, dc->pdata->fb, fb_mem);
		tegra_dc_io_end(dc);

		if (IS_ERR(dc->adf)) {
			tegra_dc_io_start(dc);
			dc->fb = tegra_fb_register(ndev, dc, dc->pdata->fb,
				fb_mem);
			tegra_dc_io_end(dc);
			if (IS_ERR_OR_NULL(dc->fb)) {
				dc->fb = NULL;
				dev_err(&ndev->dev, "failed to register fb\n");
				goto err_remove_debugfs;
			}
		}
#endif
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
		tegra_dc_get(dc);
#ifdef CONFIG_TEGRA_NVDISPLAY
		dc->fb = tegra_nvdisp_fb_register(ndev, dc, dc->pdata->fb,
			fb_mem);
#else
		dc->fb = tegra_fb_register(ndev, dc, dc->pdata->fb, fb_mem,
			NULL);
#endif
		tegra_dc_put(dc);
		if (IS_ERR_OR_NULL(dc->fb)) {
			dc->fb = NULL;
			dev_err(&ndev->dev, "failed to register fb\n");
			goto err_remove_debugfs;
		}
#endif
	}

	if (dc->pdata->flags & TEGRA_DC_FLAG_ENABLED) {
		/* WAR: BL is putting DC in bad state for EDP configuration */
		if (!tegra_platform_is_linsim() &&
			(dc->out->type == TEGRA_DC_OUT_DP ||
				dc->out->type == TEGRA_DC_OUT_NVSR_DP)) {
			tegra_disp_clk_prepare_enable(dc->clk);
			tegra_periph_reset_assert(dc->clk);
			udelay(10);
			tegra_periph_reset_deassert(dc->clk);
			udelay(10);
			tegra_disp_clk_disable_unprepare(dc->clk);
		}

		if (dc->out_ops && dc->out_ops->hotplug_init)
			dc->out_ops->hotplug_init(dc);

		_tegra_dc_set_default_videomode(dc);
		dc->enabled = _tegra_dc_enable(dc);

#if !defined(CONFIG_ARCH_TEGRA_11x_SOC) && \
	!defined(CONFIG_ARCH_TEGRA_14x_SOC) && \
	!defined(CONFIG_TEGRA_NVDISPLAY) && \
	IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
		/* BL or PG init will keep DISA unpowergated after booting.
		 * Adding an extra powergate to balance the refcount
		 * since _tegra_dc_enable() increases the refcount.
		 */
		if (!tegra_platform_is_fpga())
			if (dc->powergate_id == TEGRA_POWERGATE_DISA)
				tegra_dc_powergate_locked(dc);
#endif
	}

	if (dc->out_ops) {
		if (dc->out_ops->detect)
			dc->connected = dc->out_ops->detect(dc);
		else
			dc->connected = true;
	} else
		dc->connected = false;

	/* Powergate display module when it's unconnected. */
	/* detect() function, if presetns, responsible for the powergate */
	if (!tegra_dc_get_connected(dc) &&
			!(dc->out_ops && dc->out_ops->detect))
		tegra_dc_powergate_locked(dc);

	tegra_dc_create_sysfs(&dc->ndev->dev);

	/*
	 * Overriding the display mode only applies for modes set up during
	 * boot. It should not apply for e.g. HDMI hotplug.
	 */
	dc->initialized = false;
#ifdef CONFIG_TEGRA_NVDISPLAY
	if (dc->out->sd_settings) {
		if (dc->out->sd_settings->enable) {
			mutex_lock(&dc->lock);
			tegra_dc_unmask_interrupt(dc, SMARTDIM_INT);
			tegra_sd_stop(dc);
			tegra_sd_init(dc);
			mutex_unlock(&dc->lock);
		}
	}
#endif
	/*
	 * Initialize vedid state. This is placed here
	 * to allow persistence across sw HDMI hotplugs.
	 */
	dc->vedid = false;
	dc->vedid_data = NULL;

	return 0;

err_remove_debugfs:
	tegra_dc_remove_debugfs(dc);
	free_irq(irq, dc);
err_disable_dc:
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	if (dc->ext) {
		tegra_dc_ext_disable(dc->ext);
		tegra_dc_ext_unregister(dc->ext);
	}
#endif
	mutex_lock(&dc->lock);
	if (dc->enabled)
		_tegra_dc_disable(dc);
	dc->enabled = false;
	mutex_unlock(&dc->lock);
#if !defined(CONFIG_TEGRA_NVDISPLAY) && defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_unregister(dc->isomgr_handle);
#elif !defined(CONFIG_TEGRA_ISOMGR)
	tegra_disp_clk_put(&ndev->dev, emc_clk);
#endif
	tegra_disp_clk_put(&ndev->dev, dc->emc_la_clk);
err_put_clk:
#ifdef CONFIG_SWITCH
	if (dc->switchdev_registered)
		switch_dev_unregister(&dc->modeset_switch);
#endif
	tegra_disp_clk_put(&ndev->dev, clk);
err_iounmap_reg:
	iounmap(base);
	if (fb_mem) {
		if (!np)
			release_resource(fb_mem);
		else
			kfree(fb_mem);
	}
err_release_resource_reg:
	release_resource(base_res);
err_free:
	kfree(dc);
	tegra_dc_set(NULL, ndev->id);

	return ret;
}

static int tegra_dc_remove(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	struct device_node *np = ndev->dev.of_node;

	tegra_dc_remove_sysfs(&dc->ndev->dev);
	tegra_dc_remove_debugfs(dc);

	if (dc->fb) {
		tegra_fb_unregister(dc->fb);
		if (dc->fb_mem) {
			if (!np)
				release_resource(dc->fb_mem);
			else
				kfree(dc->fb_mem);
		}
	}

#ifdef CONFIG_ADF_TEGRA
	if (dc->adf)
		tegra_adf_unregister(dc->adf);
#endif
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	if (dc->ext) {
		tegra_dc_ext_disable(dc->ext);
		tegra_dc_ext_unregister(dc->ext);
	}
#endif

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		mutex_lock(&dc->one_shot_lock);
		cancel_delayed_work_sync(&dc->one_shot_work);
	}
	mutex_lock(&dc->lock);
	if (dc->enabled)
		_tegra_dc_disable(dc);
	dc->enabled = false;
	mutex_unlock(&dc->lock);
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		mutex_unlock(&dc->one_shot_lock);
	synchronize_irq(dc->irq); /* wait for IRQ handlers to finish */

#ifdef CONFIG_SWITCH
	if (dc->switchdev_registered)
		switch_dev_unregister(&dc->modeset_switch);
#endif
	free_irq(dc->irq, dc);
#if defined(CONFIG_TEGRA_NVDISPLAY) && defined(CONFIG_TEGRA_ISOMGR)
	tegra_nvdisp_isomgr_unregister();
#elif defined(CONFIG_TEGRA_ISOMGR)
	if (dc->isomgr_handle) {
		tegra_isomgr_unregister(dc->isomgr_handle);
		dc->isomgr_handle = NULL;
	}
#else
	tegra_disp_clk_put(&ndev->dev, dc->emc_clk);
#endif
	tegra_disp_clk_put(&ndev->dev, dc->emc_la_clk);

	tegra_disp_clk_put(&ndev->dev, dc->clk);
	iounmap(dc->base);
	if (dc->fb_mem)
		release_resource(dc->base_res);
	kfree(dc);
	tegra_dc_set(NULL, ndev->id);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_dc_suspend(struct platform_device *ndev, pm_message_t state)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);
	int ret = 0;

	trace_display_suspend(dc);
	dev_info(&ndev->dev, "suspend\n");

#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_disable(dc->ext);
#endif

	tegra_dc_cursor_suspend(dc);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		mutex_lock(&dc->one_shot_lock);
		cancel_delayed_work_sync(&dc->one_shot_work);
	}
	mutex_lock(&dc->lock);
	ret = tegra_dc_io_start(dc);

	if (dc->out_ops && dc->out_ops->suspend)
		dc->out_ops->suspend(dc);

	if (dc->enabled) {
		_tegra_dc_disable(dc);

		dc->suspended = true;
	}

	if (dc->out && dc->out->postsuspend) {
		dc->out->postsuspend();
		/* avoid resume event due to voltage falling on interfaces that
		 * support hotplug wake. And only do this if a panel is
		 * connected, if we are already disconnected, then no phantom
		 * hotplug can occur by disabling the voltage.
		 */
		if ((dc->out->flags & TEGRA_DC_OUT_HOTPLUG_WAKE_LP0)
			&& tegra_dc_get_connected(dc))
			msleep(100);
	}

	if (!ret)
		tegra_dc_io_end(dc);

	mutex_unlock(&dc->lock);
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		mutex_unlock(&dc->one_shot_lock);
	synchronize_irq(dc->irq); /* wait for IRQ handlers to finish */

	return 0;
}

static int tegra_dc_resume(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	trace_display_resume(dc);
	dev_info(&ndev->dev, "resume\n");

	mutex_lock(&dc->lock);
	dc->suspended = false;

	/* To pan the fb on resume */
	tegra_fb_pan_display_reset(dc->fb);

	if (dc->enabled) {
		dc->enabled = false;
		_tegra_dc_set_default_videomode(dc);
		dc->enabled = _tegra_dc_enable(dc);
	}

	if (dc->out && dc->out->hotplug_init)
		dc->out->hotplug_init(&ndev->dev);

	if (dc->out_ops && dc->out_ops->resume)
		dc->out_ops->resume(dc);

	mutex_unlock(&dc->lock);
	tegra_dc_cursor_resume(dc);

	return 0;
}

#endif /* CONFIG_PM */

static void tegra_dc_shutdown(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	if (WARN_ON(!dc || !dc->out || !dc->out_ops))
		return;

	if (!dc->enabled)
		return;

	kfree(dc->vedid_data);
	dc->vedid_data = NULL;
	dc->vedid = false;


	/* Let dc clients know about shutdown event before calling disable */
	if (dc->out_ops && dc->out_ops->shutdown)
		dc->out_ops->shutdown(dc);

	tegra_dc_disable(dc);
}

static int suspend_set(const char *val, struct kernel_param *kp)
{
	if (!strcmp(val, "dump"))
		dump_regs(tegra_dcs[0]);
#ifdef CONFIG_PM
	else if (!strcmp(val, "suspend"))
		tegra_dc_suspend(tegra_dcs[0]->ndev, PMSG_SUSPEND);
	else if (!strcmp(val, "resume"))
		tegra_dc_resume(tegra_dcs[0]->ndev);
#endif

	return 0;
}

static int suspend_get(char *buffer, struct kernel_param *kp)
{
	return 0;
}

static int suspend;

module_param_call(suspend, suspend_set, suspend_get, &suspend, 0644);


#ifdef CONFIG_OF
static struct of_device_id tegra_display_of_match[] = {
	{.compatible = "nvidia,tegra114-dc", },
	{.compatible = "nvidia,tegra124-dc", },
	{.compatible = "nvidia,tegra210-dc", },
	{.compatible = "nvidia,tegra186-dc", },
	{ },
};
#endif

static struct platform_driver tegra_dc_driver = {
	.driver = {
		.name = "tegradc",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table =
			of_match_ptr(tegra_display_of_match),
#endif
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = tegra_dc_probe,
	.remove = tegra_dc_remove,
#ifdef CONFIG_PM
	.suspend = tegra_dc_suspend,
	.resume = tegra_dc_resume,
#endif
	.shutdown = tegra_dc_shutdown,
};

#ifndef MODULE
static int __init parse_disp_params(char *options, struct tegra_dc_mode *mode)
{
	int i, params[11];
	char *p;

	memset(params, 0, ARRAY_SIZE(params));
	for (i = 0; i < ARRAY_SIZE(params); i++) {
		if ((p = strsep(&options, ",")) != NULL) {
			if (*p)
				params[i] = simple_strtoul(p, &p, 10);
		} else
			return -EINVAL;
	}

	if ((mode->pclk = params[0]) == 0)
		return -EINVAL;

	mode->h_active      = params[1];
	mode->v_active      = params[2];
	mode->h_ref_to_sync = params[3];
	mode->v_ref_to_sync = params[4];
	mode->h_sync_width  = params[5];
	mode->v_sync_width  = params[6];
	mode->h_back_porch  = params[7];
	mode->v_back_porch  = params[8];
	mode->h_front_porch = params[9];
	mode->v_front_porch = params[10];

	return 0;
}

static int __init tegra_dc_mode_override(char *str)
{
	char *p = str, *options;

	if (!p || !*p)
		return -EINVAL;

	p = strstr(str, "hdmi:");
	if (p) {
		p += 5;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_HDMI]))
			return -EINVAL;
	}

	p = strstr(str, "rgb:");
	if (p) {
		p += 4;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_RGB]))
			return -EINVAL;
	}

	p = strstr(str, "dsi:");
	if (p) {
		p += 4;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_DSI]))
			return -EINVAL;
	}

	p = strstr(str, "null:");
	if (p) {
		p += 5;
		options = strsep(&p, ";");
		if (parse_disp_params(options,
				&override_disp_mode[TEGRA_DC_OUT_NULL]))
			return -EINVAL;
	}

	return 0;
}

__setup("disp_params=", tegra_dc_mode_override);
#endif


#ifdef TEGRA_DC_USR_SHARED_IRQ

static struct tegra_dc  *tegra_dc_hwidx2dc(int dcid)
{
	struct tegra_dc  *dc;
	int              i;

	for (i = 0; i < TEGRA_MAX_DC; i++) {
		dc = tegra_dc_get_dc(i);
		if (dc && (dcid == dc->ctrl_num))
			return dc;
	}

	return NULL;
}


/*
 * get number of Tegra display heads
 * o inputs: none
 * o outputs:
 *  - return: number of Tegra display heads
 */
int  tegra_dc_get_numof_dispheads(void)
{
	return TEGRA_MAX_DC;
}
EXPORT_SYMBOL(tegra_dc_get_numof_dispheads);


/*
 * get Tegra display head status
 * o inputs:
 *  - dcid: display head HW index (0 to TEGRA_MAX_DC-1)
 *  - pSts: pointer to the head status structure to be returned
 * o outputs:
 *  - return: error number
 *   . 0: registration successful without an error
 *   . !0: registration failed with an error
 *  - *pSts: head status
 * o notes:
 */
int  tegra_dc_get_disphead_sts(int dcid, struct tegra_dc_head_status *pSts)
{
	struct tegra_dc  *dc = tegra_dc_hwidx2dc(dcid);

	if (dc) {
		pSts->magic = TEGRA_DC_HEAD_STATUS_MAGIC1;
		pSts->irqnum = dc->irq;
		pSts->init = dc->initialized ? 1 : 0;
		pSts->connected = dc->connected ? 1 : 0;
		pSts->active = dc->enabled ? 1 : 0;
		return 0;
	} else {
		return -ENODEV;
	}
}
EXPORT_SYMBOL(tegra_dc_get_disphead_sts);


/*
 * to register the Tegra display ISR user call-back routine
 * o inputs:
 *  - dcid: display head HW index (0 to TEGRA_MAX_DC-1)
 *  - usr_isr_cb: function pointer to the user call-back routine
 *  - usr_isr_pdt: user call-back private data
 * o outputs:
 *  - return: error code
 *   . 0: registration successful without an error
 *   . !0: registration failed with an error
 * o notes: will overwrite the old CB always
 */
int  tegra_dc_register_isr_usr_cb(int dcid,
	int (*usr_isr_cb)(int dcid, unsigned long irq_sts, void *usr_isr_pdt),
	void *usr_isr_pdt)
{
	struct tegra_dc  *dc = tegra_dc_hwidx2dc(dcid);

	/* register usr ISR */
	if (dc && usr_isr_cb) {
		if (dc->isr_usr_cb) {
			dev_warn(&dc->ndev->dev,
				"%s DC%d: overwriting ISR USR CB:%p PDT:%p\n",
				 __func__, dcid,
				 dc->isr_usr_cb, dc->isr_usr_pdt);
		}
		mutex_lock(&dc->lock);
		/* always replace the old ISR */
		dc->isr_usr_cb  = usr_isr_cb;
		dc->isr_usr_pdt = usr_isr_pdt;
		mutex_unlock(&dc->lock);
		dev_info(&dc->ndev->dev,
			"DC%d: ISR USR CB:%p PDT:%p registered\n",
			dcid, usr_isr_cb, usr_isr_pdt);
		return 0;
	} else {
		return dc ? -EINVAL : -ENODEV;
	}
}
EXPORT_SYMBOL(tegra_dc_register_isr_usr_cb);


/*
 * to unregister the Tegra display ISR user call-back routine
 * o inputs:
 *  - dcid: display head HW index (0 to TEGRA_MAX_DC-1)
 *  - usr_isr_cb: registered user call-back. ignored.
 *  - usr_isr_pdt: registered user call-back private data. ignored.
 * o outputs:
 *  - return: error code
 *   . 0: unregistration successful
 *   . !0: unregistration failed with an error
 * o notes: will unregister the current CB always
 */
int  tegra_dc_unregister_isr_usr_cb(int dcid,
	int (*usr_isr_cb)(int dcid, unsigned long irq_sts, void *usr_isr_pdt),
	void *usr_isr_pdt)
{
	struct tegra_dc  *dc = tegra_dc_hwidx2dc(dcid);

	/* unregister USR ISR CB */
	if (dc) {
		mutex_lock(&dc->lock);
		dc->isr_usr_cb = NULL;
		dc->isr_usr_pdt = NULL;
		mutex_unlock(&dc->lock);
		dev_info(&dc->ndev->dev,
			"DC%d: USR ISR CB unregistered\n", dcid);
		return 0;
	} else {
		return -ENODEV;
	}
}
EXPORT_SYMBOL(tegra_dc_unregister_isr_usr_cb);

#endif /* TEGRA_DC_USR_SHARED_IRQ */


static int __init tegra_dc_module_init(void)
{
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	int ret = tegra_dc_ext_module_init();
	if (ret)
		return ret;
#endif
	return platform_driver_register(&tegra_dc_driver);
}

static void __exit tegra_dc_module_exit(void)
{
	platform_driver_unregister(&tegra_dc_driver);
#ifdef CONFIG_TEGRA_DC_EXTENSIONS
	tegra_dc_ext_module_exit();
#endif
}

module_exit(tegra_dc_module_exit);
module_init(tegra_dc_module_init);
