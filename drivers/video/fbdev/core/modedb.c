/*
 *  linux/drivers/video/modedb.c -- Standard video mode database management
 *
 *	Copyright (C) 1999 Geert Uytterhoeven
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/kernel.h>

#undef DEBUG

#define name_matches(v, s, l) \
    ((v).name && !strncmp((s), (v).name, (l)) && strlen((v).name) == (l))
#define res_matches(v, x, y) \
    ((v).xres == (x) && (v).yres == (y))

#ifdef DEBUG
#define DPRINTK(fmt, args...)	printk("modedb %s: " fmt, __func__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 *  Standard video mode definitions (taken from XFree86)
 */

static const struct fb_videomode modedb[] = {

	/* 640x400 @ 70 Hz, 31.5 kHz hsync */
	{ NULL, 70, 640, 400, 39721, 40, 24, 39, 9, 96, 2, 0,
		FB_VMODE_NONINTERLACED },

	/* 640x480 @ 60 Hz, 31.5 kHz hsync */
	{ NULL, 60, 640, 480, 39721, 40, 24, 32, 11, 96, 2,	0,
		FB_VMODE_NONINTERLACED },

	/* 800x600 @ 56 Hz, 35.15 kHz hsync */
	{ NULL, 56, 800, 600, 27777, 128, 24, 22, 1, 72, 2,	0,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 87 Hz interlaced, 35.5 kHz hsync */
	{ NULL, 87, 1024, 768, 22271, 56, 24, 33, 8, 160, 8, 0,
		FB_VMODE_INTERLACED },

	/* 640x400 @ 85 Hz, 37.86 kHz hsync */
	{ NULL, 85, 640, 400, 31746, 96, 32, 41, 1, 64, 3,
		FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED },

	/* 640x480 @ 72 Hz, 36.5 kHz hsync */
	{ NULL, 72, 640, 480, 31746, 144, 40, 30, 8, 40, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 640x480 @ 75 Hz, 37.50 kHz hsync */
	{ NULL, 75, 640, 480, 31746, 120, 16, 16, 1, 64, 3,	0,
		FB_VMODE_NONINTERLACED },

	/* 800x600 @ 60 Hz, 37.8 kHz hsync */
	{ NULL, 60, 800, 600, 25000, 88, 40, 23, 1, 128, 4,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 640x480 @ 85 Hz, 43.27 kHz hsync */
	{ NULL, 85, 640, 480, 27777, 80, 56, 25, 1, 56, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 89 Hz interlaced, 44 kHz hsync */
	{ NULL, 89, 1152, 864, 15384, 96, 16, 110, 1, 216, 10, 0,
		FB_VMODE_INTERLACED },
	/* 800x600 @ 72 Hz, 48.0 kHz hsync */
	{ NULL, 72, 800, 600, 20000, 64, 56, 23, 37, 120, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 60 Hz, 48.4 kHz hsync */
	{ NULL, 60, 1024, 768, 15384, 168, 8, 29, 3, 144, 6, 0,
		FB_VMODE_NONINTERLACED },

	/* 640x480 @ 100 Hz, 53.01 kHz hsync */
	{ NULL, 100, 640, 480, 21834, 96, 32, 36, 8, 96, 6,	0,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 60 Hz, 53.5 kHz hsync */
	{ NULL, 60, 1152, 864, 11123, 208, 64, 16, 4, 256, 8, 0,
		FB_VMODE_NONINTERLACED },

	/* 800x600 @ 85 Hz, 55.84 kHz hsync */
	{ NULL, 85, 800, 600, 16460, 160, 64, 36, 16, 64, 5, 0,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 70 Hz, 56.5 kHz hsync */
	{ NULL, 70, 1024, 768, 13333, 144, 24, 29, 3, 136, 6, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 87 Hz interlaced, 51 kHz hsync */
	{ NULL, 87, 1280, 1024, 12500, 56, 16, 128, 1, 216, 12,	0,
		FB_VMODE_INTERLACED },

	/* 800x600 @ 100 Hz, 64.02 kHz hsync */
	{ NULL, 100, 800, 600, 14357, 160, 64, 30, 4, 64, 6, 0,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 76 Hz, 62.5 kHz hsync */
	{ NULL, 76, 1024, 768, 11764, 208, 8, 36, 16, 120, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 70 Hz, 62.4 kHz hsync */
	{ NULL, 70, 1152, 864, 10869, 106, 56, 20, 1, 160, 10, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 61 Hz, 64.2 kHz hsync */
	{ NULL, 61, 1280, 1024, 9090, 200, 48, 26, 1, 184, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1400x1050 @ 60Hz, 63.9 kHz hsync */
	{ NULL, 60, 1400, 1050, 9259, 136, 40, 13, 1, 112, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1400x1050 @ 75,107 Hz, 82,392 kHz +hsync +vsync*/
	{ NULL, 75, 1400, 1050, 7190, 120, 56, 23, 10, 112, 13,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1400x1050 @ 60 Hz, ? kHz +hsync +vsync*/
	{ NULL, 60, 1400, 1050, 9259, 128, 40, 12, 0, 112, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 85 Hz, 70.24 kHz hsync */
	{ NULL, 85, 1024, 768, 10111, 192, 32, 34, 14, 160, 6, 0,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 78 Hz, 70.8 kHz hsync */
	{ NULL, 78, 1152, 864, 9090, 228, 88, 32, 0, 84, 12, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 70 Hz, 74.59 kHz hsync */
	{ NULL, 70, 1280, 1024, 7905, 224, 32, 28, 8, 160, 8, 0,
		FB_VMODE_NONINTERLACED },

	/* 1600x1200 @ 60Hz, 75.00 kHz hsync */
	{ NULL, 60, 1600, 1200, 6172, 304, 64, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 84 Hz, 76.0 kHz hsync */
	{ NULL, 84, 1152, 864, 7407, 184, 312, 32, 0, 128, 12, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 74 Hz, 78.85 kHz hsync */
	{ NULL, 74, 1280, 1024, 7407, 256, 32, 34, 3, 144, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1024x768 @ 100Hz, 80.21 kHz hsync */
	{ NULL, 100, 1024, 768, 8658, 192, 32, 21, 3, 192, 10, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 76 Hz, 81.13 kHz hsync */
	{ NULL, 76, 1280, 1024, 7407, 248, 32, 34, 3, 104, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1600x1200 @ 70 Hz, 87.50 kHz hsync */
	{ NULL, 70, 1600, 1200, 5291, 304, 64, 46, 1, 192, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 1152x864 @ 100 Hz, 89.62 kHz hsync */
	{ NULL, 100, 1152, 864, 7264, 224, 32, 17, 2, 128, 19, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 85 Hz, 91.15 kHz hsync */
	{ NULL, 85, 1280, 1024, 6349, 224, 64, 44, 1, 160, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1600x1200 @ 75 Hz, 93.75 kHz hsync */
	{ NULL, 75, 1600, 1200, 4938, 304, 64, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1680x1050 @ 60 Hz, 65.191 kHz hsync */
	{ NULL, 60, 1680, 1050, 6848, 280, 104, 30, 3, 176, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1600x1200 @ 85 Hz, 105.77 kHz hsync */
	{ NULL, 85, 1600, 1200, 4545, 272, 16, 37, 4, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1280x1024 @ 100 Hz, 107.16 kHz hsync */
	{ NULL, 100, 1280, 1024, 5502, 256, 32, 26, 7, 128, 15, 0,
		FB_VMODE_NONINTERLACED },

	/* 1800x1440 @ 64Hz, 96.15 kHz hsync  */
	{ NULL, 64, 1800, 1440, 4347, 304, 96, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1800x1440 @ 70Hz, 104.52 kHz hsync  */
	{ NULL, 70, 1800, 1440, 4000, 304, 96, 46, 1, 192, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 512x384 @ 78 Hz, 31.50 kHz hsync */
	{ NULL, 78, 512, 384, 49603, 48, 16, 16, 1, 64, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 512x384 @ 85 Hz, 34.38 kHz hsync */
	{ NULL, 85, 512, 384, 45454, 48, 16, 16, 1, 64, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 320x200 @ 70 Hz, 31.5 kHz hsync, 8:5 aspect ratio */
	{ NULL, 70, 320, 200, 79440, 16, 16, 20, 4, 48, 1, 0,
		FB_VMODE_DOUBLE },

	/* 320x240 @ 60 Hz, 31.5 kHz hsync, 4:3 aspect ratio */
	{ NULL, 60, 320, 240, 79440, 16, 16, 16, 5, 48, 1, 0,
		FB_VMODE_DOUBLE },

	/* 320x240 @ 72 Hz, 36.5 kHz hsync */
	{ NULL, 72, 320, 240, 63492, 16, 16, 16, 4, 48, 2, 0,
		FB_VMODE_DOUBLE },

	/* 400x300 @ 56 Hz, 35.2 kHz hsync, 4:3 aspect ratio */
	{ NULL, 56, 400, 300, 55555, 64, 16, 10, 1, 32, 1, 0,
		FB_VMODE_DOUBLE },

	/* 400x300 @ 60 Hz, 37.8 kHz hsync */
	{ NULL, 60, 400, 300, 50000, 48, 16, 11, 1, 64, 2, 0,
		FB_VMODE_DOUBLE },

	/* 400x300 @ 72 Hz, 48.0 kHz hsync */
	{ NULL, 72, 400, 300, 40000, 32, 24, 11, 19, 64, 3,	0,
		FB_VMODE_DOUBLE },

	/* 480x300 @ 56 Hz, 35.2 kHz hsync, 8:5 aspect ratio */
	{ NULL, 56, 480, 300, 46176, 80, 16, 10, 1, 40, 1, 0,
		FB_VMODE_DOUBLE },

	/* 480x300 @ 60 Hz, 37.8 kHz hsync */
	{ NULL, 60, 480, 300, 41858, 56, 16, 11, 1, 80, 2, 0,
		FB_VMODE_DOUBLE },

	/* 480x300 @ 63 Hz, 39.6 kHz hsync */
	{ NULL, 63, 480, 300, 40000, 56, 16, 11, 1, 80, 2, 0,
		FB_VMODE_DOUBLE },

	/* 480x300 @ 72 Hz, 48.0 kHz hsync */
	{ NULL, 72, 480, 300, 33386, 40, 24, 11, 19, 80, 3, 0,
		FB_VMODE_DOUBLE },

	/* 1920x1200 @ 60 Hz, 74.5 Khz hsync */
	{ NULL, 60, 1920, 1200, 5177, 128, 336, 1, 38, 208, 3,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1152x768, 60 Hz, PowerBook G4 Titanium I and II */
	{ NULL, 60, 1152, 768, 14047, 158, 26, 29, 3, 136, 6,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED },

	/* 1366x768, 60 Hz, 47.403 kHz hsync, WXGA 16:9 aspect ratio */
	{ NULL, 60, 1366, 768, 13806, 120, 10, 14, 3, 32, 5, 0,
		FB_VMODE_NONINTERLACED },

	/* 1280x800, 60 Hz, 47.403 kHz hsync, WXGA 16:10 aspect ratio */
	{ NULL, 60, 1280, 800, 12048, 200, 64, 24, 1, 136, 3, 0,
		FB_VMODE_NONINTERLACED },

	/* 720x576i @ 50 Hz, 15.625 kHz hsync (PAL RGB) */
	{ NULL, 50, 720, 576, 74074, 64, 16, 39, 5, 64, 5, 0,
		FB_VMODE_INTERLACED },

	/* 800x520i @ 50 Hz, 15.625 kHz hsync (PAL RGB) */
	{ NULL, 50, 800, 520, 58823, 144, 64, 72, 28, 80, 5, 0,
		FB_VMODE_INTERLACED },

	/* 864x480 @ 60 Hz, 35.15 kHz hsync */
	{ NULL, 60, 864, 480, 27777, 1, 1, 1, 1, 0, 0,
		0, FB_VMODE_NONINTERLACED },
};

#ifdef CONFIG_FB_MODE_HELPERS
const struct fb_videomode cea_modes[CEA_MODEDB_SIZE] = {
	{},
	/* 1: 640x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 640, .yres = 480, .pixclock = 39721,
	 .left_margin = 48, .right_margin = 16,
	 .upper_margin = 33, .lower_margin = 10,
	 .hsync_len = 96, .vsync_len = 2,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 2: 720x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 720, .yres = 480, .pixclock = 37037,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 3: 720x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 720, .yres = 480, .pixclock = 37037,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 4: 1280x720p @ 59.94Hz/60Hz */
	{.refresh = 60, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 110,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 5: 1920x1080i @ 59.94Hz/60Hz */
	{.refresh = 60, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 15, .lower_margin = 2,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_INTERLACED},
	/* 6: 720(1440)x480i @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 480, .pixclock = 37037,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 7: 720(1440)x480i @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 480, .pixclock = 37037,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 8: 720(1440)x240p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 240, .pixclock = 37037,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 5,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 9: 720(1440)x240p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 240, .pixclock = 37037,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 5,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 10: 2880x480i @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 480, .pixclock = 18518,
	 .left_margin = 228, .right_margin = 76,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 248, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 11: 2880x480i @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 480, .pixclock = 18518,
	 .left_margin = 228, .right_margin = 76,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 248, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 12: 2880x240p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 240, .pixclock = 18518,
	 .left_margin = 228, .right_margin = 76,
	 .upper_margin = 15, .lower_margin = 5,
	 .hsync_len = 248, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 13: 2880x240p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 240, .pixclock = 18518,
	 .left_margin = 228, .right_margin = 76,
	 .upper_margin = 15, .lower_margin = 5,
	 .hsync_len = 248, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 14: 1440x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 480, .pixclock = 18518,
	 .left_margin = 120, .right_margin = 32,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 124, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 15: 1440x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 1440, .yres = 480, .pixclock = 18518,
	 .left_margin = 120, .right_margin = 32,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 124, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 16: 1920x1080p @ 59.94Hz/60Hz */
	{.refresh = 60, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 17: 720x576p @ 50Hz */
	{.refresh = 50, .xres = 720, .yres = 576, .pixclock = 37037,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 18: 720x576p @ 50Hz */
	{.refresh = 50, .xres = 720, .yres = 576, .pixclock = 37037,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 19: 1280x720p @ 50Hz */
	{.refresh = 50, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 440,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 20: 1920x1080i @ 50Hz */
	{.refresh = 50, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 15, .lower_margin = 2,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_INTERLACED},
	/* 21: 720(1440)x576i @ 50Hz */
	{.refresh = 50, .xres = 1440, .yres = 576, .pixclock = 37037,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 22: 720(1440)x576i @ 50Hz */
	{.refresh = 50, .xres = 1440, .yres = 576, .pixclock = 37037,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 23: 720(1440)x288p @ 50Hz */
	{.refresh = 49, .xres = 1440, .yres = 288, .pixclock = 37037,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 4,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 24: 720(1440)x288p @ 50Hz */
	{.refresh = 49, .xres = 1440, .yres = 288, .pixclock = 37037,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 4,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 25: 2880x576i @ 50Hz */
	{.refresh = 50, .xres = 2880, .yres = 576, .pixclock = 18518,
	 .left_margin = 276, .right_margin = 48,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 252, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 26: 2880x576i @ 50Hz */
	{.refresh = 50, .xres = 2880, .yres = 576, .pixclock = 18518,
	 .left_margin = 276, .right_margin = 48,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 252, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 27: 2880x288p @ 50Hz */
	{.refresh = 49, .xres = 2880, .yres = 288, .pixclock = 18518,
	 .left_margin = 276, .right_margin = 48,
	 .upper_margin = 19, .lower_margin = 4,
	 .hsync_len = 252, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 28: 2880x288p @ 50Hz */
	{.refresh = 49, .xres = 2880, .yres = 288, .pixclock = 18518,
	 .left_margin = 276, .right_margin = 48,
	 .upper_margin = 19, .lower_margin = 4,
	 .hsync_len = 252, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 29: 1440x576p @ 50Hz */
	{.refresh = 50, .xres = 1440, .yres = 576, .pixclock = 18518,
	 .left_margin = 136, .right_margin = 24,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 128, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 30: 1440x576p @ 50Hz */
	{.refresh = 50, .xres = 1440, .yres = 576, .pixclock = 18518,
	 .left_margin = 136, .right_margin = 24,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 128, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 31: 1920x1080p @ 50Hz */
	{.refresh = 50, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 32: 1920x1080p @ 23.97Hz/24Hz */
	{.refresh = 24, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 638,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 33: 1920x1080p @ 25Hz */
	{.refresh = 25, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 34: 1920x1080p @ 29.97Hz/30Hz */
	{.refresh = 30, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 35: 2880x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 480, .pixclock = 9259,
	 .left_margin = 240, .right_margin = 64,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 248, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 36: 2880x480p @ 59.94Hz/60Hz */
	{.refresh = 59, .xres = 2880, .yres = 480, .pixclock = 9259,
	 .left_margin = 240, .right_margin = 64,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 248, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 37: 2880x576p @ 50Hz */
	{.refresh = 50, .xres = 2880, .yres = 576, .pixclock = 9259,
	 .left_margin = 272, .right_margin = 48,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 256, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 38: 2880x576p @ 50Hz */
	{.refresh = 50, .xres = 2880, .yres = 576, .pixclock = 9259,
	 .left_margin = 272, .right_margin = 48,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 256, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 39: 1920x1080i @ 50Hz */
	{.refresh = 50, .xres = 1920, .yres = 1080, .pixclock = 13888,
	 .left_margin = 184, .right_margin = 32,
	 .upper_margin = 57, .lower_margin = 2,
	 .hsync_len = 168, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_INTERLACED},
	/* 40: 1920x1080i @ 100Hz */
	{.refresh = 100, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 15, .lower_margin = 2,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_INTERLACED},
	/* 41: 1280x720p @ 100Hz */
	{.refresh = 100, .xres = 1280, .yres = 720, .pixclock = 6734,
	 .left_margin = 220, .right_margin = 440,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 42: 720x576p @ 100Hz */
	{.refresh = 100, .xres = 720, .yres = 576, .pixclock = 18518,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 43: 720x576p @ 100Hz */
	{.refresh = 100, .xres = 720, .yres = 576, .pixclock = 18518,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 44: 720(1440)x576i @ 100Hz */
	{.refresh = 100, .xres = 1440, .yres = 576, .pixclock = 18518,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 45: 720(1440)x576i @ 100Hz */
	{.refresh = 100, .xres = 1440, .yres = 576, .pixclock = 18518,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 46: 1920x1080i @ 119.88/120Hz */
	{.refresh = 120, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 15, .lower_margin = 2,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_INTERLACED},
	/* 47: 1280x720p @ 119.88/120Hz */
	{.refresh = 120, .xres = 1280, .yres = 720, .pixclock = 6734,
	 .left_margin = 220, .right_margin = 110,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 48: 720x480p @ 119.88/120Hz */
	{.refresh = 119, .xres = 720, .yres = 480, .pixclock = 18518,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 49: 720x480p @ 119.88/120Hz */
	{.refresh = 119, .xres = 720, .yres = 480, .pixclock = 18518,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 50: 720(1440)x480i @ 119.88/120Hz */
	{.refresh = 119, .xres = 1440, .yres = 480, .pixclock = 18518,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 51: 720(1440)x480i @ 119.88/120Hz */
	{.refresh = 119, .xres = 1440, .yres = 480, .pixclock = 18518,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 52: 720x576p @ 200Hz */
	{.refresh = 200, .xres = 720, .yres = 576, .pixclock = 9259,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 53: 720x576p @ 200Hz */
	{.refresh = 200, .xres = 720, .yres = 576, .pixclock = 9259,
	 .left_margin = 68, .right_margin = 12,
	 .upper_margin = 39, .lower_margin = 5,
	 .hsync_len = 64, .vsync_len = 5,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 54: 720(1440)x576i @ 200Hz */
	{.refresh = 200, .xres = 1440, .yres = 576, .pixclock = 9259,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 55: 720(1440)x576i @ 200Hz */
	{.refresh = 200, .xres = 1440, .yres = 576, .pixclock = 9259,
	 .left_margin = 138, .right_margin = 24,
	 .upper_margin = 19, .lower_margin = 2,
	 .hsync_len = 126, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 56: 720x480p @ 239.76/240Hz */
	{.refresh = 239, .xres = 720, .yres = 480, .pixclock = 9259,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 57: 720x480p @ 239.76/240Hz */
	{.refresh = 239, .xres = 720, .yres = 480, .pixclock = 9259,
	 .left_margin = 60, .right_margin = 16,
	 .upper_margin = 30, .lower_margin = 9,
	 .hsync_len = 62, .vsync_len = 6,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 58: 720(1440)x480i @ 239.76/240Hz */
	{.refresh = 239, .xres = 1440, .yres = 480, .pixclock = 9259,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_4_3 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 59: 720(1440)x480i @ 239.76/240Hz */
	{.refresh = 239, .xres = 1440, .yres = 480, .pixclock = 9259,
	 .left_margin = 114, .right_margin = 38,
	 .upper_margin = 15, .lower_margin = 4,
	 .hsync_len = 124, .vsync_len = 3,
	 .sync = 0,
	 .flag = FB_FLAG_RATIO_16_9 | FB_FLAG_PIXEL_REPEAT,
	 .vmode = FB_VMODE_INTERLACED},
	/* 60: 1280x720p @ 23.97Hz/24Hz */
	{.refresh = 24, .xres = 1280, .yres = 720, .pixclock = 16835,
	 .left_margin = 220, .right_margin = 1760,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 61: 1280x720p @ 25Hz */
	{.refresh = 25, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 2420,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 62: 1280x720p @ 29.97Hz/30Hz */
	{.refresh = 30, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 1760,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 63: 1920x1080p @ 119.88/120Hz */
	{.refresh = 120, .xres = 1920, .yres = 1080, .pixclock = 3367,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 64: 1920x1080p @ 100Hz */
	{.refresh = 100, .xres = 1920, .yres = 1080, .pixclock = 3367,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	 /* 65: 1280x720p @ 24Hz */
	{.refresh = 24, .xres = 1280, .yres = 720, .pixclock = 16835,
	 .left_margin = 220, .right_margin = 1760,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 66: 1280x720p @ 25Hz */
	{.refresh = 25, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 2420,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 67: 1280x720p @ 30Hz */
	{.refresh = 30, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 1760,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 68: 1280x720p @ 50Hz */
	{.refresh = 50, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 440,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 69: 1280x720p @ 60Hz */
	{.refresh = 60, .xres = 1280, .yres = 720, .pixclock = 13468,
	 .left_margin = 220, .right_margin = 110,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 70: 1280x720p @ 100Hz */
	{.refresh = 100, .xres = 1280, .yres = 720, .pixclock = 6734,
	 .left_margin = 220, .right_margin = 440,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 71: 1280x720p @ 120Hz */
	{.refresh = 120, .xres = 1280, .yres = 720, .pixclock = 6734,
	 .left_margin = 220, .right_margin = 110,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 72: 1920x1080p @ 24Hz */
	{.refresh = 24, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 638,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 73: 1920x1080p @ 25Hz */
	{.refresh = 25, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 74: 1920x1080p @ 30Hz */
	{.refresh = 30, .xres = 1920, .yres = 1080, .pixclock = 13468,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 75: 1920x1080p @ 50Hz */
	{.refresh = 50, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 76: 1920x1080p @ 60Hz */
	{.refresh = 60, .xres = 1920, .yres = 1080, .pixclock = 6734,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 77: 1920x1080p @ 100Hz */
	{.refresh = 100, .xres = 1920, .yres = 1080, .pixclock = 3367,
	 .left_margin = 148, .right_margin = 528,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 78: 1920x1080p @ 120Hz */
	{.refresh = 120, .xres = 1920, .yres = 1080, .pixclock = 3367,
	 .left_margin = 148, .right_margin = 88,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 79: 1680x720p @ 24Hz */
	{.refresh = 24, .xres = 1680, .yres = 720, .pixclock = 16835,
	 .left_margin = 220, .right_margin = 1360,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 80: 1680x720p @ 25Hz */
	{.refresh = 25, .xres = 1680, .yres = 720, .pixclock = 16835,
	 .left_margin = 220, .right_margin = 1228,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 81: 1680x720p @ 30Hz */
	{.refresh = 30, .xres = 1680, .yres = 720, .pixclock = 16835,
	 .left_margin = 220, .right_margin = 700,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 82: 1680x720p @ 50Hz */
	{.refresh = 50, .xres = 1680, .yres = 720, .pixclock = 12121,
	 .left_margin = 220, .right_margin = 260,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 83: 1680x720p @ 60Hz */
	{.refresh = 60, .xres = 1680, .yres = 720, .pixclock = 10101,
	 .left_margin = 220, .right_margin = 260,
	 .upper_margin = 20, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 84: 1680x720p @ 100Hz */
	{.refresh = 100, .xres = 1680, .yres = 720, .pixclock = 6060,
	 .left_margin = 220, .right_margin = 60,
	 .upper_margin = 95, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 85: 1680x720p @ 120Hz */
	{.refresh = 120, .xres = 1680, .yres = 720, .pixclock = 5050,
	 .left_margin = 220, .right_margin = 60,
	 .upper_margin = 95, .lower_margin = 5,
	 .hsync_len = 40, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 86: 2560x1080p @ 24Hz */
	{.refresh = 24, .xres = 2560, .yres = 1080, .pixclock = 10101,
	 .left_margin = 148, .right_margin = 998,
	 .upper_margin = 11, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 87: 2560x1080p @ 25Hz */
	{.refresh = 25, .xres = 2560, .yres = 1080, .pixclock = 11111,
	 .left_margin = 148, .right_margin = 448,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 88: 2560x1080p @ 30Hz */
	{.refresh = 30, .xres = 2560, .yres = 1080, .pixclock = 8417,
	 .left_margin = 148, .right_margin = 768,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 89: 2560x1080p @ 50Hz */
	{.refresh = 50, .xres = 2560, .yres = 1080, .pixclock = 5387,
	 .left_margin = 148, .right_margin = 548,
	 .upper_margin = 36, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 90: 2560x1080p @ 60Hz */
	{.refresh = 60, .xres = 2560, .yres = 1080, .pixclock = 5050,
	 .left_margin = 148, .right_margin = 248,
	 .upper_margin = 11, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 91: 2560x1080p @ 100Hz */
	{.refresh = 100, .xres = 2560, .yres = 1080, .pixclock = 2693,
	 .left_margin = 148, .right_margin = 218,
	 .upper_margin = 161, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 92: 2560x1080p @ 120Hz */
	{.refresh = 120, .xres = 2560, .yres = 1080, .pixclock = 2020,
	 .left_margin = 148, .right_margin = 548,
	 .upper_margin = 161, .lower_margin = 4,
	 .hsync_len = 44, .vsync_len = 5,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 93: 3840x2160p @ 24Hz */
	{.refresh = 24, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1276,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 94: 3840x2160p @ 25Hz */
	{.refresh = 25, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1056,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 95: 3840x2160p @ 30Hz */
	{.refresh = 30, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 176,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 96: 3840x2160p @ 50Hz */
	{.refresh = 50, .xres = 3840, .yres = 2160, .pixclock = 1683,
	 .left_margin = 296, .right_margin = 1056,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 97: 3840x2160p @ 60Hz */
	{.refresh = 60, .xres = 3840, .yres = 2160, .pixclock = 1683,
	 .left_margin = 296, .right_margin = 176,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 98: 4096x2160p @ 24Hz */
	{.refresh = 24, .xres = 4096, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1020,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_256_135,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 99: 4096x2160p @ 25Hz */
	{.refresh = 25, .xres = 4096, .yres = 2160, .pixclock = 3367,
	 .left_margin = 128, .right_margin = 968,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_256_135,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 100: 4096x2160p @ 30Hz */
	{.refresh = 30, .xres = 4096, .yres = 2160, .pixclock = 3367,
	 .left_margin = 128, .right_margin = 88,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_256_135,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 101: 4096x2160p @ 50Hz */
	{.refresh = 50, .xres = 4096, .yres = 2160, .pixclock = 1683,
	 .left_margin = 128, .right_margin = 968,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_256_135,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 102: 4096x2160p @ 60Hz */
	{.refresh = 60, .xres = 4096, .yres = 2160, .pixclock = 1683,
	 .left_margin = 128, .right_margin = 88,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_256_135,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 103: 3840x2160p @ 24Hz */
	{.refresh = 24, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1276,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 104: 3840x2160p @ 25Hz */
	{.refresh = 25, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1056,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 105: 3840x2160p @ 30Hz */
	{.refresh = 30, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 176,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 106: 3840x2160p @ 50Hz */
	{.refresh = 50, .xres = 3840, .yres = 2160, .pixclock = 1683,
	 .left_margin = 296, .right_margin = 1056,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* 107: 3840x2160p @ 60Hz */
	{.refresh = 60, .xres = 3840, .yres = 2160, .pixclock = 1683,
	 .left_margin = 296, .right_margin = 176,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_64_27,
	 .vmode = FB_VMODE_NONINTERLACED},
};
EXPORT_SYMBOL(cea_modes);

const struct fb_videomode hdmi_ext_modes[HDMI_EXT_MODEDB_SIZE] = {
	{},
	/* HDMI_VIC 0x01: 3840x2160p @ 29.97/30Hz */
	{.refresh = 30, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 176,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* HDMI_VIC 0x02: 3840x2160p @ 25Hz */
	{.refresh = 25, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1056,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* HDMI_VIC 0x03: 3840x2160p @ 23.98/24Hz */
	{.refresh = 24, .xres = 3840, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1276,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
	/* HDMI_VIC 0x04: 4096x2160p @ 24Hz */
	{.refresh = 24, .xres = 4096, .yres = 2160, .pixclock = 3367,
	 .left_margin = 296, .right_margin = 1020,
	 .upper_margin = 72, .lower_margin = 8,
	 .hsync_len = 88, .vsync_len = 10,
	 .sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 .flag = FB_FLAG_RATIO_16_9,
	 .vmode = FB_VMODE_NONINTERLACED},
};
EXPORT_SYMBOL(hdmi_ext_modes);

const struct fb_videomode vesa_modes[VESA_MODEDB_SIZE] = {
	/* 0 640x350-85 VESA */
	{ NULL, 85, 640, 350, 31746, 96, 32, 60, 32, 64, 3,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 1 640x400-85 VESA */
	{ NULL, 85, 640, 400, 31746, 96, 32, 41, 1, 64, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 2 720x400-85 VESA */
	{ NULL, 85, 720, 400, 28169, 108, 36, 42, 1, 72, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 3 640x480-60 VESA */
	{ NULL, 60, 640, 480, 39722, 48, 16, 33, 10, 96, 2,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 4 640x480-72 VESA */
	{ NULL, 72, 640, 480, 31746, 128, 24, 29, 9, 40, 2,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 5 640x480-75 VESA */
	{ NULL, 75, 640, 480, 31746, 120, 16, 16, 1, 64, 3,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 6 640x480-85 VESA */
	{ NULL, 85, 640, 480, 27777, 80, 56, 25, 1, 56, 3,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 7 800x600-56 VESA */
	{ NULL, 56, 800, 600, 27777, 128, 24, 22, 1, 72, 2,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 8 800x600-60 VESA */
	{ NULL, 60, 800, 600, 25000, 88, 40, 23, 1, 128, 4,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 9 800x600-72 VESA */
	{ NULL, 72, 800, 600, 20000, 64, 56, 23, 37, 120, 6,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 10 800x600-75 VESA */
	{ NULL, 75, 800, 600, 20202, 160, 16, 21, 1, 80, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 11 800x600-85 VESA */
	{ NULL, 85, 800, 600, 17777, 152, 32, 27, 1, 64, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 12 800x600-120 VESA */
	{ NULL, 120, 800, 600, 13651, 80, 48, 29, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 13 848x480-60 VESA */
	{ NULL, 60, 848, 480, 29629, 112, 16, 23, 6, 112, 8,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 14 1024x768-43 VESA */
	{ NULL, 43, 1024, 768, 22271, 56, 8, 45, 0, 176, 4,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 15 1024x768-60 VESA */
	{ NULL, 60, 1024, 768, 15384, 160, 24, 29, 3, 136, 6,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 16 1024x768-70 VESA */
	{ NULL, 70, 1024, 768, 13333, 144, 24, 29, 3, 136, 6,
	  0, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 17 1024x768-75 VESA */
	{ NULL, 75, 1024, 768, 12698, 176, 16, 28, 1, 96, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 18 1024x768-85 VESA */
	{ NULL, 85, 1024, 768, 10582, 208, 48, 36, 1, 96, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 19 1024x768-120 VESA */
	{ NULL, 120, 1024, 768, 8658, 80, 48, 38, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 20 1152x864-75 VESA */
	{ NULL, 75, 1152, 864, 9259, 256, 64, 32, 1, 128, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 21 1280x768-60 VESA */
	{ NULL, 60, 1280, 768, 14652, 80, 48, 12, 3, 32, 7,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 22 1280x768-60 VESA */
	{ NULL, 60, 1280, 768, 12578, 192, 64, 20, 3, 128, 7,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 23 1280x768-75 VESA */
	{ NULL, 75, 1280, 768, 9779, 208, 80, 27, 3, 128, 7,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 24 1280x768-85 VESA */
	{ NULL, 85, 1280, 768, 8510, 216, 80, 31, 3, 136, 7,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 25 1280x768-120 VESA */
	{ NULL, 120, 1280, 768, 7130, 80, 48, 35, 3, 32, 7,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 26 1280x800-60 VESA */
	{ NULL, 60, 1280, 800, 14084, 80, 48, 14, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 27 1280x800-60 VESA */
	{ NULL, 60, 1280, 800, 11976, 200, 72, 22, 3, 128, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 28 1280x800-75 VESA */
	{ NULL, 75, 1280, 800, 9389, 208, 80, 29, 3, 128, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 29 1280x800-85 VESA */
	{ NULL, 85, 1280, 800, 8163, 216, 80, 34, 3, 136, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 30 1280x800-120 VESA */
	{ NULL, 120, 1280, 800, 6837, 80, 48, 38, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 31 1280x960-60 VESA */
	{ NULL, 60, 1280, 960, 9259, 312, 96, 36, 1, 112, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 32 1280x960-85 VESA */
	{ NULL, 85, 1280, 960, 6734, 224, 64, 47, 1, 160, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 33 1280x960-120 VESA */
	{ NULL, 120, 1280, 960, 5698, 80, 48, 50, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 34 1280x1024-60 VESA */
	{ NULL, 60, 1280, 1024, 9262, 248, 48, 38, 1, 112, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 35 1280x1024-75 VESA */
	{ NULL, 75, 1280, 1024, 7407, 248, 16, 38, 1, 144, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 36 1280x1024-85 VESA */
	{ NULL, 85, 1280, 1024, 6349, 224, 64, 44, 1, 160, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 37 1280x1024-120 VESA */
	{ NULL, 120, 1280, 1024, 5340, 80, 48, 50, 3, 32, 7,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 38 1360x768-60 VESA */
	{ NULL, 60, 1360, 768, 11695, 256, 64, 18, 3, 112, 6,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 39 1360x768-120 VESA */
	{ NULL, 120, 1360, 768, 6745, 80, 48, 37, 3, 32, 5,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 40 1400x1050-60 VESA */
	{ NULL, 60, 1400, 1050, 9900, 80, 48, 23, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 41 1400x1050-60 VESA */
	{ NULL, 60, 1400, 1050, 8213, 232, 88, 32, 3, 144, 4,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 42 1400x1050-75 VESA */
	{ NULL, 75, 1400, 1050, 6410, 248, 104, 42, 3, 144, 4,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 43 1400x1050-85 VESA */
	{ NULL, 85, 1400, 1050, 5571, 256, 104, 48, 3, 152, 4,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 44 1400x1050-120 VESA */
	{ NULL, 120, 1400, 1050, 4807, 80, 48, -7, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 45 1440x900-60 VESA */
	{ NULL, 60, 1440, 900, 11267, 80, 48, 17, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 46 1440x900-60 VESA */
	{ NULL, 60, 1440, 900, 9389, 232, 80, 25, 3, 152, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 47 1440x900-75 VESA */
	{ NULL, 75, 1440, 900, 7312, 248, 96, 33, 3, 152, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 48 1440x900-85 VESA */
	{ NULL, 85, 1440, 900, 6369, 256, 104, 39, 3, 152, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 49 1440x900-120 VESA */
	{ NULL, 120, 1440, 900, 5471, 80, 48, 44, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 50 1600x1200-60 VESA */
	{ NULL, 60, 1600, 1200, 6172, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 51 1600x1200-65 VESA */
	{ NULL, 65, 1600, 1200, 5698, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 52 1600x1200-70 VESA */
	{ NULL, 70, 1600, 1200, 5291, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 53 1600x1200-75 VESA */
	{ NULL, 75, 1600, 1200, 4938, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 54 1600x1200-85 VESA */
	{ NULL, 85, 1600, 1200, 4357, 304, 64, 46, 1, 192, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 55 1600x1200-120 VESA */
	{ NULL, 120, 1600, 1200, 3727, 80, 48, 64, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 56 1680x1050-60 VESA */
	{ NULL, 60, 1680, 1050, 8403, 80, 48, 21, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 57 1680x1050-60 VESA */
	{ NULL, 60, 1680, 1050, 6837, 280, 104, 30, 3, 176, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 58 1680x1050-75 VESA */
	{ NULL, 75, 1680, 1050, 5347, 296, 120, 40, 3, 176, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 59 1680x1050-85 VESA */
	{ NULL, 85, 1680, 1050, 4656, 304, 128, 46, 3, 176, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 60 1680x1050-120 VESA */
	{ NULL, 120, 1680, 1050, 4073, 80, 48, 53, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 61 1792x1344-60 VESA */
	{ NULL, 60, 1792, 1344, 4884, 328, 128, 46, 1, 200, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 62 1792x1344-75 VESA */
	{ NULL, 75, 1792, 1344, 3831, 352, 96, 69, 1, 216, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 63 1792x1344-120 VESA */
	{ NULL, 120, 1792, 1344, 3000, 80, 48, 72, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 64 1856x1392-60 VESA */
	{ NULL, 60, 1856, 1392, 4581, 352, 96, 43, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 65 1856x1392-75 VESA */
	{ NULL, 75, 1856, 1392, 3472, 352, 128, 104, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 66 1856x1392-120 VESA */
	{ NULL, 120, 1856, 1392, 2805, 80, 48, 75, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 67 1920x1200-60 VESA */
	{ NULL, 60, 1920, 1200, 6493, 80, 48, 26, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 68 1920x1200-60 VESA */
	{ NULL, 60, 1920, 1200, 5174, 336, 136, 36, 3, 200, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 69 1920x1200-75 VESA */
	{ NULL, 75, 1920, 1200, 4077, 344, 136, 46, 3, 208, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 70 1920x1200-85 VESA */
	{ NULL, 85, 1920, 1200, 3555, 352, 144, 53, 3, 208, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 71 1920x1200-120 VESA */
	{ NULL, 120, 1920, 1200, 3154, 80, 48, 62, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 72 1920x1440-60 VESA */
	{ NULL, 60, 1920, 1440, 4273, 344, 128, 56, 1, 208, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 73 1920x1440-75 VESA */
	{ NULL, 75, 1920, 1440, 3367, 352, 144, 56, 1, 224, 3,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 74 1920x1440-120 VESA */
	{ NULL, 120, 1920, 1440, 2628, 80, 48, 78, 3, 32, 4,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 75 2560x1600-60 VESA */
	{ NULL, 60, 2560, 1600, 3724, 80, 48, 37, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 76 2560x1600-60 VESA */
	{ NULL, 60, 2560, 1600, 2869, 472, 192, 49, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 77 2560x1600-75 VESA */
	{ NULL, 75, 2560, 1600, 2256, 488, 208, 63, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 78 2560x1600-85 VESA */
	{ NULL, 85, 2560, 1600, 1979, 488, 208, 73, 3, 280, 6,
	  FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 79 2560x1600-120 VESA */
	{ NULL, 120, 2560, 1600, 1809, 80, 48, 85, 3, 32, 6,
	  FB_SYNC_HOR_HIGH_ACT, FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 80 1366x768-60 VESA */
	{ NULL, 60, 1366, 768, 11695, 213, 70, 24, 3, 143, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 81 1920x1080-60 VESA */
	{ NULL, 60, 1920, 1080, 6734, 148, 88, 36, 4, 44, 5,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 82 1600x900-60 VESA */
	{ NULL, 60, 1600, 900, 9259, 96, 24, 96, 1, 80, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 83 2048x1152-60 VESA */
	{ NULL, 60, 2048, 1152, 6172, 96, 26, 44, 1, 80, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 84 1280x720-60 VESA */
	{ NULL, 60, 1280, 720, 13468, 220, 110, 20, 5, 40, 5,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
	/* 85 1366x768-60 VESA */
	{ NULL, 60, 1366, 768, 13888, 64, 14, 28, 1, 56, 3,
	  FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	  FB_VMODE_NONINTERLACED, FB_MODE_IS_VESA },
};
EXPORT_SYMBOL(vesa_modes);

const struct dmt_videomode dmt_modes[DMT_SIZE] = {
	{ 0x01, 0x0000, 0x000000, &vesa_modes[0] },
	{ 0x02, 0x3119, 0x000000, &vesa_modes[1] },
	{ 0x03, 0x0000, 0x000000, &vesa_modes[2] },
	{ 0x04, 0x3140, 0x000000, &vesa_modes[3] },
	{ 0x05, 0x314c, 0x000000, &vesa_modes[4] },
	{ 0x06, 0x314f, 0x000000, &vesa_modes[5] },
	{ 0x07, 0x3159, 0x000000, &vesa_modes[6] },
	{ 0x08, 0x0000, 0x000000, &vesa_modes[7] },
	{ 0x09, 0x4540, 0x000000, &vesa_modes[8] },
	{ 0x0a, 0x454c, 0x000000, &vesa_modes[9] },
	{ 0x0b, 0x454f, 0x000000, &vesa_modes[10] },
	{ 0x0c, 0x4559, 0x000000, &vesa_modes[11] },
	{ 0x0d, 0x0000, 0x000000, &vesa_modes[12] },
	{ 0x0e, 0x0000, 0x000000, &vesa_modes[13] },
	{ 0x0f, 0x0000, 0x000000, &vesa_modes[14] },
	{ 0x10, 0x6140, 0x000000, &vesa_modes[15] },
	{ 0x11, 0x614a, 0x000000, &vesa_modes[16] },
	{ 0x12, 0x614f, 0x000000, &vesa_modes[17] },
	{ 0x13, 0x6159, 0x000000, &vesa_modes[18] },
	{ 0x14, 0x0000, 0x000000, &vesa_modes[19] },
	{ 0x15, 0x714f, 0x000000, &vesa_modes[20] },
	{ 0x16, 0x0000, 0x7f1c21, &vesa_modes[21] },
	{ 0x17, 0x0000, 0x7f1c28, &vesa_modes[22] },
	{ 0x18, 0x0000, 0x7f1c44, &vesa_modes[23] },
	{ 0x19, 0x0000, 0x7f1c62, &vesa_modes[24] },
	{ 0x1a, 0x0000, 0x000000, &vesa_modes[25] },
	{ 0x1b, 0x0000, 0x8f1821, &vesa_modes[26] },
	{ 0x1c, 0x8100, 0x8f1828, &vesa_modes[27] },
	{ 0x1d, 0x810f, 0x8f1844, &vesa_modes[28] },
	{ 0x1e, 0x8119, 0x8f1862, &vesa_modes[29] },
	{ 0x1f, 0x0000, 0x000000, &vesa_modes[30] },
	{ 0x20, 0x8140, 0x000000, &vesa_modes[31] },
	{ 0x21, 0x8159, 0x000000, &vesa_modes[32] },
	{ 0x22, 0x0000, 0x000000, &vesa_modes[33] },
	{ 0x23, 0x8180, 0x000000, &vesa_modes[34] },
	{ 0x24, 0x818f, 0x000000, &vesa_modes[35] },
	{ 0x25, 0x8199, 0x000000, &vesa_modes[36] },
	{ 0x26, 0x0000, 0x000000, &vesa_modes[37] },
	{ 0x27, 0x0000, 0x000000, &vesa_modes[38] },
	{ 0x28, 0x0000, 0x000000, &vesa_modes[39] },
	{ 0x29, 0x0000, 0x0c2021, &vesa_modes[40] },
	{ 0x2a, 0x9040, 0x0c2028, &vesa_modes[41] },
	{ 0x2b, 0x904f, 0x0c2044, &vesa_modes[42] },
	{ 0x2c, 0x9059, 0x0c2062, &vesa_modes[43] },
	{ 0x2d, 0x0000, 0x000000, &vesa_modes[44] },
	{ 0x2e, 0x0000, 0xc11821, &vesa_modes[45] },
	{ 0x2f, 0x9500, 0xc11828, &vesa_modes[46] },
	{ 0x30, 0x950f, 0xc11844, &vesa_modes[47] },
	{ 0x31, 0x9519, 0xc11868, &vesa_modes[48] },
	{ 0x32, 0x0000, 0x000000, &vesa_modes[49] },
	{ 0x33, 0xa940, 0x000000, &vesa_modes[50] },
	{ 0x34, 0xa945, 0x000000, &vesa_modes[51] },
	{ 0x35, 0xa94a, 0x000000, &vesa_modes[52] },
	{ 0x36, 0xa94f, 0x000000, &vesa_modes[53] },
	{ 0x37, 0xa959, 0x000000, &vesa_modes[54] },
	{ 0x38, 0x0000, 0x000000, &vesa_modes[55] },
	{ 0x39, 0x0000, 0x0c2821, &vesa_modes[56] },
	{ 0x3a, 0xb300, 0x0c2828, &vesa_modes[57] },
	{ 0x3b, 0xb30f, 0x0c2844, &vesa_modes[58] },
	{ 0x3c, 0xb319, 0x0c2868, &vesa_modes[59] },
	{ 0x3d, 0x0000, 0x000000, &vesa_modes[60] },
	{ 0x3e, 0xc140, 0x000000, &vesa_modes[61] },
	{ 0x3f, 0xc14f, 0x000000, &vesa_modes[62] },
	{ 0x40, 0x0000, 0x000000, &vesa_modes[63] },
	{ 0x41, 0xc940, 0x000000, &vesa_modes[64] },
	{ 0x42, 0xc94f, 0x000000, &vesa_modes[65] },
	{ 0x43, 0x0000, 0x000000, &vesa_modes[66] },
	{ 0x44, 0x0000, 0x572821, &vesa_modes[67] },
	{ 0x45, 0xd100, 0x572828, &vesa_modes[68] },
	{ 0x46, 0xd10f, 0x572844, &vesa_modes[69] },
	{ 0x47, 0xd119, 0x572862, &vesa_modes[70] },
	{ 0x48, 0x0000, 0x000000, &vesa_modes[71] },
	{ 0x49, 0xd140, 0x000000, &vesa_modes[72] },
	{ 0x4a, 0xd14f, 0x000000, &vesa_modes[73] },
	{ 0x4b, 0x0000, 0x000000, &vesa_modes[74] },
	{ 0x4c, 0x0000, 0x1f3821, &vesa_modes[75] },
	{ 0x4d, 0x0000, 0x1f3828, &vesa_modes[76] },
	{ 0x4e, 0x0000, 0x1f3844, &vesa_modes[77] },
	{ 0x4f, 0x0000, 0x1f3862, &vesa_modes[78] },
	{ 0x50, 0x0000, 0x000000, &vesa_modes[79] },
	{ 0x51, 0x0000, 0x000000, &vesa_modes[80] },
	{ 0x52, 0xd1c0, 0x000000, &vesa_modes[81] },
	{ 0x53, 0xa9c0, 0x000000, &vesa_modes[82] },
	{ 0x54, 0xe1c0, 0x000000, &vesa_modes[83] },
	{ 0x55, 0x81c0, 0x000000, &vesa_modes[84] },
	{ 0x56, 0x0000, 0x000000, &vesa_modes[85] },
	{ 0x57, 0x0000, 0x000000, &vesa_modes[86] },
	{ 0x58, 0x0000, 0x000000, &vesa_modes[87] },
};
EXPORT_SYMBOL(dmt_modes);
#endif /* CONFIG_FB_MODE_HELPERS */

/**
 *	fb_try_mode - test a video mode
 *	@var: frame buffer user defined part of display
 *	@info: frame buffer info structure
 *	@mode: frame buffer video mode structure
 *	@bpp: color depth in bits per pixel
 *
 *	Tries a video mode to test it's validity for device @info.
 *
 *	Returns 1 on success.
 *
 */

static int fb_try_mode(struct fb_var_screeninfo *var, struct fb_info *info,
		       const struct fb_videomode *mode, unsigned int bpp)
{
	int err = 0;

	DPRINTK("Trying mode %s %dx%d-%d@%d\n",
		mode->name ? mode->name : "noname",
		mode->xres, mode->yres, bpp, mode->refresh);
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres;
	var->xoffset = 0;
	var->yoffset = 0;
	var->bits_per_pixel = bpp;
	var->activate |= FB_ACTIVATE_TEST;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode;
	if (info->fbops->fb_check_var)
		err = info->fbops->fb_check_var(var, info);
	var->activate &= ~FB_ACTIVATE_TEST;
	return err;
}

/**
 *     fb_find_mode - finds a valid video mode
 *     @var: frame buffer user defined part of display
 *     @info: frame buffer info structure
 *     @mode_option: string video mode to find
 *     @db: video mode database
 *     @dbsize: size of @db
 *     @default_mode: default video mode to fall back to
 *     @default_bpp: default color depth in bits per pixel
 *
 *     Finds a suitable video mode, starting with the specified mode
 *     in @mode_option with fallback to @default_mode.  If
 *     @default_mode fails, all modes in the video mode database will
 *     be tried.
 *
 *     Valid mode specifiers for @mode_option:
 *
 *     <xres>x<yres>[M][R][-<bpp>][@<refresh>][i][m] or
 *     <name>[-<bpp>][@<refresh>]
 *
 *     with <xres>, <yres>, <bpp> and <refresh> decimal numbers and
 *     <name> a string.
 *
 *      If 'M' is present after yres (and before refresh/bpp if present),
 *      the function will compute the timings using VESA(tm) Coordinated
 *      Video Timings (CVT).  If 'R' is present after 'M', will compute with
 *      reduced blanking (for flatpanels).  If 'i' is present, compute
 *      interlaced mode.  If 'm' is present, add margins equal to 1.8%
 *      of xres rounded down to 8 pixels, and 1.8% of yres. The char
 *      'i' and 'm' must be after 'M' and 'R'. Example:
 *
 *      1024x768MR-8@60m - Reduced blank with margins at 60Hz.
 *
 *     NOTE: The passed struct @var is _not_ cleared!  This allows you
 *     to supply values for e.g. the grayscale and accel_flags fields.
 *
 *     Returns zero for failure, 1 if using specified @mode_option,
 *     2 if using specified @mode_option with an ignored refresh rate,
 *     3 if default mode is used, 4 if fall back to any valid mode.
 *
 */

int fb_find_mode(struct fb_var_screeninfo *var,
		 struct fb_info *info, const char *mode_option,
		 const struct fb_videomode *db, unsigned int dbsize,
		 const struct fb_videomode *default_mode,
		 unsigned int default_bpp)
{
	int i;

	/* Set up defaults */
	if (!db) {
		db = modedb;
		dbsize = ARRAY_SIZE(modedb);
	}

	if (!default_mode)
		default_mode = &db[0];

	if (!default_bpp)
		default_bpp = 8;

	/* Did the user specify a video mode? */
	if (!mode_option)
		mode_option = fb_mode_option;
	if (mode_option) {
		const char *name = mode_option;
		unsigned int namelen = strlen(name);
		int res_specified = 0, bpp_specified = 0, refresh_specified = 0;
		unsigned int xres = 0, yres = 0, bpp = default_bpp, refresh = 0;
		int yres_specified = 0, cvt = 0, rb = 0, interlace = 0;
		int margins = 0;
		u32 best, diff, tdiff;

		for (i = namelen-1; i >= 0; i--) {
			switch (name[i]) {
			case '@':
				namelen = i;
				if (!refresh_specified && !bpp_specified &&
				    !yres_specified) {
					refresh = simple_strtol(&name[i+1], NULL,
								10);
					refresh_specified = 1;
					if (cvt || rb)
						cvt = 0;
				} else
					goto done;
				break;
			case '-':
				namelen = i;
				if (!bpp_specified && !yres_specified) {
					bpp = simple_strtol(&name[i+1], NULL,
							    10);
					bpp_specified = 1;
					if (cvt || rb)
						cvt = 0;
				} else
					goto done;
				break;
			case 'x':
				if (!yres_specified) {
					yres = simple_strtol(&name[i+1], NULL,
							     10);
					yres_specified = 1;
				} else
					goto done;
				break;
			case '0' ... '9':
				break;
			case 'M':
				if (!yres_specified)
					cvt = 1;
				break;
			case 'R':
				if (!cvt)
					rb = 1;
				break;
			case 'm':
				if (!cvt)
					margins = 1;
				break;
			case 'i':
				if (!cvt)
					interlace = 1;
				break;
			default:
				goto done;
			}
		}
		if (i < 0 && yres_specified) {
			xres = simple_strtol(name, NULL, 10);
			res_specified = 1;
		}
done:
		if (cvt) {
			struct fb_videomode cvt_mode;
			int ret;

			DPRINTK("CVT mode %dx%d@%dHz%s%s%s\n", xres, yres,
				(refresh) ? refresh : 60,
				(rb) ? " reduced blanking" : "",
				(margins) ? " with margins" : "",
				(interlace) ? " interlaced" : "");

			memset(&cvt_mode, 0, sizeof(cvt_mode));
			cvt_mode.xres = xres;
			cvt_mode.yres = yres;
			cvt_mode.refresh = (refresh) ? refresh : 60;

			if (interlace)
				cvt_mode.vmode |= FB_VMODE_INTERLACED;
			else
				cvt_mode.vmode &= ~FB_VMODE_INTERLACED;

			ret = fb_find_mode_cvt(&cvt_mode, margins, rb);

			if (!ret && !fb_try_mode(var, info, &cvt_mode, bpp)) {
				DPRINTK("modedb CVT: CVT mode ok\n");
				return 1;
			}

			DPRINTK("CVT mode invalid, getting mode from database\n");
		}

		DPRINTK("Trying specified video mode%s %ix%i\n",
			refresh_specified ? "" : " (ignoring refresh rate)",
			xres, yres);

		if (!refresh_specified) {
			/*
			 * If the caller has provided a custom mode database and
			 * a valid monspecs structure, we look for the mode with
			 * the highest refresh rate.  Otherwise we play it safe
			 * it and try to find a mode with a refresh rate closest
			 * to the standard 60 Hz.
			 */
			if (db != modedb &&
			    info->monspecs.vfmin && info->monspecs.vfmax &&
			    info->monspecs.hfmin && info->monspecs.hfmax &&
			    info->monspecs.dclkmax) {
				refresh = 1000;
			} else {
				refresh = 60;
			}
		}

		diff = -1;
		best = -1;
		for (i = 0; i < dbsize; i++) {
			if ((name_matches(db[i], name, namelen) ||
			     (res_specified && res_matches(db[i], xres, yres))) &&
			    !fb_try_mode(var, info, &db[i], bpp)) {
				if (refresh_specified && db[i].refresh == refresh)
					return 1;

				if (abs(db[i].refresh - refresh) < diff) {
					diff = abs(db[i].refresh - refresh);
					best = i;
				}
			}
		}
		if (best != -1) {
			fb_try_mode(var, info, &db[best], bpp);
			return (refresh_specified) ? 2 : 1;
		}

		diff = 2 * (xres + yres);
		best = -1;
		DPRINTK("Trying best-fit modes\n");
		for (i = 0; i < dbsize; i++) {
			DPRINTK("Trying %ix%i\n", db[i].xres, db[i].yres);
			if (!fb_try_mode(var, info, &db[i], bpp)) {
				tdiff = abs(db[i].xres - xres) +
					abs(db[i].yres - yres);

				/*
				 * Penalize modes with resolutions smaller
				 * than requested.
				 */
				if (xres > db[i].xres || yres > db[i].yres)
					tdiff += xres + yres;

				if (diff > tdiff) {
					diff = tdiff;
					best = i;
				}
			}
		}
		if (best != -1) {
			fb_try_mode(var, info, &db[best], bpp);
			return 5;
		}
	}

	DPRINTK("Trying default video mode\n");
	if (!fb_try_mode(var, info, default_mode, default_bpp))
		return 3;

	DPRINTK("Trying all modes\n");
	for (i = 0; i < dbsize; i++)
		if (!fb_try_mode(var, info, &db[i], default_bpp))
			return 4;

	DPRINTK("No valid mode found\n");
	return 0;
}

/**
 * fb_var_to_videomode - convert fb_var_screeninfo to fb_videomode
 * @mode: pointer to struct fb_videomode
 * @var: pointer to struct fb_var_screeninfo
 */
void fb_var_to_videomode(struct fb_videomode *mode,
			 const struct fb_var_screeninfo *var)
{
	u32 pixclock, hfreq, htotal, vtotal;

	mode->name = NULL;
	mode->xres = var->xres;
	mode->yres = var->yres;
	mode->pixclock = var->pixclock;
	mode->hsync_len = var->hsync_len;
	mode->vsync_len = var->vsync_len;
	mode->left_margin = var->left_margin;
	mode->right_margin = var->right_margin;
	mode->upper_margin = var->upper_margin;
	mode->lower_margin = var->lower_margin;
	mode->sync = var->sync;
	mode->vmode = var->vmode & (FB_VMODE_MASK | FB_VMODE_STEREO_MASK);
	mode->flag = FB_MODE_IS_FROM_VAR;
	mode->refresh = 0;

	if (!var->pixclock)
		return;

	pixclock = PICOS2KHZ(var->pixclock) * 1000;

	htotal = var->xres + var->right_margin + var->hsync_len +
		var->left_margin;
	vtotal = var->yres + var->lower_margin + var->vsync_len +
		var->upper_margin;

	if (var->vmode & FB_VMODE_INTERLACED)
		vtotal /= 2;
	if (var->vmode & FB_VMODE_DOUBLE)
		vtotal *= 2;

	hfreq = pixclock/htotal;
	mode->refresh = hfreq/vtotal;
}

/**
 * fb_videomode_to_var - convert fb_videomode to fb_var_screeninfo
 * @var: pointer to struct fb_var_screeninfo
 * @mode: pointer to struct fb_videomode
 */
void fb_videomode_to_var(struct fb_var_screeninfo *var,
			 const struct fb_videomode *mode)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres;
	var->xoffset = 0;
	var->yoffset = 0;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode & (FB_VMODE_MASK | FB_VMODE_STEREO_MASK);
}

/**
 * fb_mode_is_equal - compare 2 videomodes, with the second one possibly
 * generated by a modeset operation. In this case, use special comparison
 * for the vmode flags. Do not compare special vmode flags related to
 * colorimetry and bits per component. Use adequate rules for yuv420 flags.
 * @mode1: first videomode
 * @mode2: second videomode, possibly from a modeset
 *
 * RETURNS:
 * 1 if equal, 0 if not
 */
int fb_mode_is_equal(const struct fb_videomode *mode1,
		     const struct fb_videomode *mode2)
{
	/* Compare all the other information first */
	if (mode1->xres         != mode2->xres ||
	    mode1->yres         != mode2->yres ||
	    mode1->refresh      != mode2->refresh ||
	    mode1->hsync_len    != mode2->hsync_len ||
	    mode1->vsync_len    != mode2->vsync_len ||
	    mode1->left_margin  != mode2->left_margin ||
	    mode1->right_margin != mode2->right_margin ||
	    mode1->upper_margin != mode2->upper_margin ||
	    mode1->lower_margin != mode2->lower_margin ||
	    mode1->sync         != mode2->sync)
		return 0;

	/* If bitfields already match, skip further testing */
	if (mode1->vmode == mode2->vmode)
		return 1;

	/* If it is not from a modeset, use traditional comparison */
	if (!(mode2->flag & FB_MODE_IS_FROM_VAR))
		return (mode1->vmode == mode2->vmode);

	/* Compare stereo and scanning properties, they should match */
	if ((mode1->vmode & (FB_VMODE_STEREO_MASK | FB_VMODE_SCAN_MASK)) !=
	    (mode2->vmode & (FB_VMODE_STEREO_MASK | FB_VMODE_SCAN_MASK)))
		return 0;

	/*
	 * Compare only FB_VMODE_Y420 and FB_VMODE_Y420_ONLY, as they are the
	 * only flags not provided through fb_fix_screeninfo to userspace.
	 */
	if ((mode1->vmode & (FB_VMODE_Y420 | FB_VMODE_Y420_ONLY)) ==
	    (mode2->vmode & (FB_VMODE_Y420 | FB_VMODE_Y420_ONLY)))
		return 1;

	/*
	 *Special case when comparing an RGB or YUV420 mode from set (mode2)
	 * to a mode capable of both, YUV420 and RGB (mode1). Neither mode1
	 * nor mode2 may have the FB_VMODE_Y420_ONLY flag set.
	 */
	if (!(mode1->vmode & FB_VMODE_Y420_ONLY) &&
	    !(mode2->vmode & FB_VMODE_Y420_ONLY) &&
	     (mode1->vmode & FB_VMODE_Y420))
		return 1;

	/* In any other case, report the modes do not match */
	return 0;
}

/**
 * fb_mode_is_equal_tolerance - compare 2 videomodes with a tolerance in
 * pixclock. Similar to fb_mode_is_equal
 *
 * RETURNS:
 * 1 if equal, 0 if not
 */
int fb_mode_is_equal_tolerance(const struct fb_videomode *mode1,
			       const struct fb_videomode *mode2,
			       unsigned int tolerance)
{
	/*
	 * Note: this function intentionally doesn't check refresh and flags.
	 * refresh is an optional field and always has +1/-1 rounding errors
	 */

	if (mode1->xres             == mode2->xres &&
		mode1->yres         == mode2->yres &&
		mode2->pixclock * (1000-tolerance) / 1000 <= mode1->pixclock &&
		mode1->pixclock <= mode2->pixclock * (1000+tolerance) / 1000 &&
		mode1->hsync_len    == mode2->hsync_len &&
		mode1->vsync_len    == mode2->vsync_len &&
		mode1->left_margin  == mode2->left_margin &&
		mode1->right_margin == mode2->right_margin &&
		mode1->upper_margin == mode2->upper_margin &&
		mode1->lower_margin == mode2->lower_margin &&
		mode1->sync         == mode2->sync &&
		mode1->vmode        == mode2->vmode)
		return 1;
	else
		return 0;
}

/**
 * fb_var_is_equal - compare two struct fb_var_screeninfo for equality
 * @var1: first variable screen info
 * @var2: second variable screen info
 *
 * This is stricter than strictly necessary, but it is assumed that this will
 * only be called on screen info structures derived from the same mode and
 * will therefore have the exact same flags. Flexibility is sacrificed for
 * simplicity.
 *
 * Returns:
 * True if both screen info structures match, false otherwise.
 */
static bool fb_var_is_equal(const struct fb_var_screeninfo *var1,
			    const struct fb_var_screeninfo *var2)
{
	if (var1->xres != var2->xres ||
	    var1->yres != var2->yres ||
	    var1->pixclock != var2->pixclock ||
	    var1->left_margin != var2->left_margin ||
	    var1->right_margin != var2->right_margin ||
	    var1->upper_margin != var2->upper_margin ||
	    var1->lower_margin != var2->lower_margin ||
	    var1->hsync_len != var2->hsync_len ||
	    var1->vsync_len != var2->vsync_len)
		return false;

	if (var1->vmode == var2->vmode)
		return true;

	/*
	 * This could probably be less strict, similar to what's done in the
	 * fb_mode_is_equal() function, but given the assumption that both
	 * screen info structures are derived from the same mode, the above
	 * checks should be good enough.
	 */

	return false;
}

/**
 * fb_find_best_mode - find best matching videomode
 * @var: pointer to struct fb_var_screeninfo
 * @head: pointer to struct list_head of modelist
 *
 * RETURNS:
 * struct fb_videomode, NULL if none found
 *
 * IMPORTANT:
 * This function assumes that all modelist entries in
 * info->modelist are valid.
 *
 * NOTES:
 * Finds best matching videomode which has an equal or greater dimension than
 * var->xres and var->yres.  If more than 1 videomode is found, will return
 * the videomode with the highest refresh rate
 */
const struct fb_videomode *fb_find_best_mode(const struct fb_var_screeninfo *var,
					     struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_videomode *mode, *best = NULL;
	u32 diff = -1;

	list_for_each(pos, head) {
		u32 d;

		modelist = list_entry(pos, struct fb_modelist, list);
		mode = &modelist->mode;

		if (mode->xres >= var->xres && mode->yres >= var->yres) {
			d = (mode->xres - var->xres) +
				(mode->yres - var->yres);
			if (diff > d) {
				diff = d;
				best = mode;
			} else if (diff == d && best &&
				   mode->refresh > best->refresh)
				best = mode;
		}
	}
	return best;
}

/**
 * fb_find_nearest_mode - find closest videomode
 *
 * @mode: pointer to struct fb_videomode
 * @head: pointer to modelist
 *
 * Finds best matching videomode, smaller or greater in dimension.
 * If more than 1 videomode is found, will return the videomode with
 * the closest refresh rate.
 */
const struct fb_videomode *fb_find_nearest_mode(const struct fb_videomode *mode,
					        struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_videomode *cmode, *best = NULL;
	u32 diff = -1, diff_refresh = -1;

	list_for_each(pos, head) {
		u32 d;

		modelist = list_entry(pos, struct fb_modelist, list);
		cmode = &modelist->mode;

		d = abs(cmode->xres - mode->xres) +
			abs(cmode->yres - mode->yres);
		if (diff > d) {
			diff = d;
			diff_refresh = abs(cmode->refresh - mode->refresh);
			best = cmode;
		} else if (diff == d) {
			d = abs(cmode->refresh - mode->refresh);
			if (diff_refresh > d) {
				diff_refresh = d;
				best = cmode;
			}
		}
	}

	return best;
}

/**
 * fb_match_mode - find a videomode which exactly matches the timings in var
 * @var: pointer to struct fb_var_screeninfo
 * @head: pointer to struct list_head of modelist
 *
 * RETURNS:
 * struct fb_videomode, NULL if none found
 */
const struct fb_videomode *fb_match_mode(const struct fb_var_screeninfo *var,
					 struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_var_screeninfo v;

	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		fb_videomode_to_var(&v, &modelist->mode);
		if (fb_var_is_equal(var, &v))
			return &modelist->mode;
	}
	return NULL;
}

/**
 * fb_add_videomode - adds videomode entry to modelist
 * @mode: videomode to add
 * @head: struct list_head of modelist
 *
 * NOTES:
 * Will only add unmatched mode entries
 */
int fb_add_videomode(const struct fb_videomode *mode, struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_videomode *m;
	int found = 0;

	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
		if (fb_mode_is_equal(m, mode)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		modelist = kmalloc(sizeof(struct fb_modelist),
						  GFP_KERNEL);

		if (!modelist)
			return -ENOMEM;
		modelist->mode = *mode;
		list_add_tail(&modelist->list, head);
	}
	return 0;
}

/**
 * fb_delete_videomode - removed videomode entry from modelist
 * @mode: videomode to remove
 * @head: struct list_head of modelist
 *
 * NOTES:
 * Will remove all matching mode entries
 */
void fb_delete_videomode(const struct fb_videomode *mode,
			 struct list_head *head)
{
	struct list_head *pos, *n;
	struct fb_modelist *modelist;
	struct fb_videomode *m;

	list_for_each_safe(pos, n, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
		if (fb_mode_is_equal(m, mode)) {
			list_del(pos);
			kfree(pos);
		}
	}
}

/**
 * fb_destroy_modelist - destroy modelist
 * @head: struct list_head of modelist
 */
void fb_destroy_modelist(struct list_head *head)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, head) {
		list_del(pos);
		kfree(pos);
	}
}
EXPORT_SYMBOL_GPL(fb_destroy_modelist);

/**
 * fb_videomode_to_modelist - convert mode array to mode list
 * @modedb: array of struct fb_videomode
 * @num: number of entries in array
 * @head: struct list_head of modelist
 */
void fb_videomode_to_modelist(const struct fb_videomode *modedb, int num,
			      struct list_head *head)
{
	int i;

	INIT_LIST_HEAD(head);

	for (i = 0; i < num; i++) {
		if (fb_add_videomode(&modedb[i], head))
			return;
	}
}

const struct fb_videomode *fb_find_best_display(const struct fb_monspecs *specs,
					        struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	const struct fb_videomode *m, *m1 = NULL, *md = NULL, *best = NULL;
	int first = 0;

	if (!head->prev || !head->next || list_empty(head))
		goto finished;

	/* get the first detailed mode and the very first mode */
	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;

		if (!first) {
			m1 = m;
			first = 1;
		}

		if (m->flag & FB_MODE_IS_FIRST) {
 			md = m;
			break;
		}
	}

	/* first detailed timing is preferred */
	if (specs->misc & FB_MISC_1ST_DETAIL) {
		best = md;
		goto finished;
	}

	/* find best mode based on display width and height */
	if (specs->max_x && specs->max_y) {
		struct fb_var_screeninfo var;

		memset(&var, 0, sizeof(struct fb_var_screeninfo));
		var.xres = (specs->max_x * 7200)/254;
		var.yres = (specs->max_y * 7200)/254;
		m = fb_find_best_mode(&var, head);
		if (m) {
			best = m;
			goto finished;
		}
	}

	/* use first detailed mode */
	if (md) {
		best = md;
		goto finished;
	}

	/* last resort, use the very first mode */
	best = m1;
finished:
	return best;
}
EXPORT_SYMBOL(fb_find_best_display);

EXPORT_SYMBOL(fb_videomode_to_var);
EXPORT_SYMBOL(fb_var_to_videomode);
EXPORT_SYMBOL(fb_mode_is_equal);
EXPORT_SYMBOL(fb_add_videomode);
EXPORT_SYMBOL(fb_match_mode);
EXPORT_SYMBOL(fb_find_best_mode);
EXPORT_SYMBOL(fb_find_nearest_mode);
EXPORT_SYMBOL(fb_videomode_to_modelist);
EXPORT_SYMBOL(fb_find_mode);
EXPORT_SYMBOL(fb_find_mode_cvt);
