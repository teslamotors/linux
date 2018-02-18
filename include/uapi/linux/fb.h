#ifndef _UAPI_LINUX_FB_H
#define _UAPI_LINUX_FB_H

#include <linux/types.h>
#include <linux/i2c.h>

/* Definitions of frame buffers						*/

#define FB_MAX			32	/* sufficient for now */

/* ioctls
   0x46 is 'F'								*/
#define FBIOGET_VSCREENINFO	0x4600
#define FBIOPUT_VSCREENINFO	0x4601
#define FBIOGET_FSCREENINFO	0x4602
#define FBIOGETCMAP		0x4604
#define FBIOPUTCMAP		0x4605
#define FBIOPAN_DISPLAY		0x4606
#ifndef __KERNEL__
#define FBIO_CURSOR            _IOWR('F', 0x08, struct fb_cursor)
#endif
/* 0x4607-0x460B are defined below */
/* #define FBIOGET_MONITORSPEC	0x460C */
/* #define FBIOPUT_MONITORSPEC	0x460D */
/* #define FBIOSWITCH_MONIBIT	0x460E */
#define FBIOGET_CON2FBMAP	0x460F
#define FBIOPUT_CON2FBMAP	0x4610
#define FBIOBLANK		0x4611		/* arg: 0 or vesa level + 1 */
#define FBIOGET_VBLANK		_IOR('F', 0x12, struct fb_vblank)
#define FBIO_ALLOC              0x4613
#define FBIO_FREE               0x4614
#define FBIOGET_GLYPH           0x4615
#define FBIOGET_HWCINFO         0x4616
#define FBIOPUT_MODEINFO        0x4617
#define FBIOGET_DISPINFO        0x4618
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, __u32)

#define FB_TYPE_PACKED_PIXELS		0	/* Packed Pixels	*/
#define FB_TYPE_PLANES			1	/* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES	2	/* Interleaved planes	*/
#define FB_TYPE_TEXT			3	/* Text/attributes	*/
#define FB_TYPE_VGA_PLANES		4	/* EGA/VGA planes	*/
#define FB_TYPE_FOURCC			5	/* Type identified by a V4L2 FOURCC */

#define FB_AUX_TEXT_MDA		0	/* Monochrome text */
#define FB_AUX_TEXT_CGA		1	/* CGA/EGA/VGA Color text */
#define FB_AUX_TEXT_S3_MMIO	2	/* S3 MMIO fasttext */
#define FB_AUX_TEXT_MGA_STEP16	3	/* MGA Millenium I: text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_MGA_STEP8	4	/* other MGAs:      text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_GROUP	8	/* 8-15: SVGA tileblit compatible modes */
#define FB_AUX_TEXT_SVGA_MASK	7	/* lower three bits says step */
#define FB_AUX_TEXT_SVGA_STEP2	8	/* SVGA text mode:  text, attr */
#define FB_AUX_TEXT_SVGA_STEP4	9	/* SVGA text mode:  text, attr,  2 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP8	10	/* SVGA text mode:  text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP16	11	/* SVGA text mode:  text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_SVGA_LAST	15	/* reserved up to 15 */

#define FB_AUX_VGA_PLANES_VGA4		0	/* 16 color planes (EGA/VGA) */
#define FB_AUX_VGA_PLANES_CFB4		1	/* CFB4 in planes (VGA) */
#define FB_AUX_VGA_PLANES_CFB8		2	/* CFB8 in planes (VGA) */

#define FB_VISUAL_MONO01		0	/* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10		1	/* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR		2	/* True color	*/
#define FB_VISUAL_PSEUDOCOLOR		3	/* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR		4	/* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR	5	/* Pseudo color readonly */
#define FB_VISUAL_FOURCC		6	/* Visual identified by a V4L2 FOURCC */

#define FB_ACCEL_NONE		0	/* no hardware accelerator	*/
#define FB_ACCEL_ATARIBLITT	1	/* Atari Blitter		*/
#define FB_ACCEL_AMIGABLITT	2	/* Amiga Blitter                */
#define FB_ACCEL_S3_TRIO64	3	/* Cybervision64 (S3 Trio64)    */
#define FB_ACCEL_NCR_77C32BLT	4	/* RetinaZ3 (NCR 77C32BLT)      */
#define FB_ACCEL_S3_VIRGE	5	/* Cybervision64/3D (S3 ViRGE)	*/
#define FB_ACCEL_ATI_MACH64GX	6	/* ATI Mach 64GX family		*/
#define FB_ACCEL_DEC_TGA	7	/* DEC 21030 TGA		*/
#define FB_ACCEL_ATI_MACH64CT	8	/* ATI Mach 64CT family		*/
#define FB_ACCEL_ATI_MACH64VT	9	/* ATI Mach 64CT family VT class */
#define FB_ACCEL_ATI_MACH64GT	10	/* ATI Mach 64CT family GT class */
#define FB_ACCEL_SUN_CREATOR	11	/* Sun Creator/Creator3D	*/
#define FB_ACCEL_SUN_CGSIX	12	/* Sun cg6			*/
#define FB_ACCEL_SUN_LEO	13	/* Sun leo/zx			*/
#define FB_ACCEL_IMS_TWINTURBO	14	/* IMS Twin Turbo		*/
#define FB_ACCEL_3DLABS_PERMEDIA2 15	/* 3Dlabs Permedia 2		*/
#define FB_ACCEL_MATROX_MGA2064W 16	/* Matrox MGA2064W (Millenium)	*/
#define FB_ACCEL_MATROX_MGA1064SG 17	/* Matrox MGA1064SG (Mystique)	*/
#define FB_ACCEL_MATROX_MGA2164W 18	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGA2164W_AGP 19	/* Matrox MGA2164W (Millenium II) */
#define FB_ACCEL_MATROX_MGAG100	20	/* Matrox G100 (Productiva G100) */
#define FB_ACCEL_MATROX_MGAG200	21	/* Matrox G200 (Myst, Mill, ...) */
#define FB_ACCEL_SUN_CG14	22	/* Sun cgfourteen		 */
#define FB_ACCEL_SUN_BWTWO	23	/* Sun bwtwo			*/
#define FB_ACCEL_SUN_CGTHREE	24	/* Sun cgthree			*/
#define FB_ACCEL_SUN_TCX	25	/* Sun tcx			*/
#define FB_ACCEL_MATROX_MGAG400	26	/* Matrox G400			*/
#define FB_ACCEL_NV3		27	/* nVidia RIVA 128              */
#define FB_ACCEL_NV4		28	/* nVidia RIVA TNT		*/
#define FB_ACCEL_NV5		29	/* nVidia RIVA TNT2		*/
#define FB_ACCEL_CT_6555x	30	/* C&T 6555x			*/
#define FB_ACCEL_3DFX_BANSHEE	31	/* 3Dfx Banshee			*/
#define FB_ACCEL_ATI_RAGE128	32	/* ATI Rage128 family		*/
#define FB_ACCEL_IGS_CYBER2000	33	/* CyberPro 2000		*/
#define FB_ACCEL_IGS_CYBER2010	34	/* CyberPro 2010		*/
#define FB_ACCEL_IGS_CYBER5000	35	/* CyberPro 5000		*/
#define FB_ACCEL_SIS_GLAMOUR    36	/* SiS 300/630/540              */
#define FB_ACCEL_3DLABS_PERMEDIA3 37	/* 3Dlabs Permedia 3		*/
#define FB_ACCEL_ATI_RADEON	38	/* ATI Radeon family		*/
#define FB_ACCEL_I810           39      /* Intel 810/815                */
#define FB_ACCEL_SIS_GLAMOUR_2  40	/* SiS 315, 650, 740		*/
#define FB_ACCEL_SIS_XABRE      41	/* SiS 330 ("Xabre")		*/
#define FB_ACCEL_I830           42      /* Intel 830M/845G/85x/865G     */
#define FB_ACCEL_NV_10          43      /* nVidia Arch 10               */
#define FB_ACCEL_NV_20          44      /* nVidia Arch 20               */
#define FB_ACCEL_NV_30          45      /* nVidia Arch 30               */
#define FB_ACCEL_NV_40          46      /* nVidia Arch 40               */
#define FB_ACCEL_XGI_VOLARI_V	47	/* XGI Volari V3XT, V5, V8      */
#define FB_ACCEL_XGI_VOLARI_Z	48	/* XGI Volari Z7                */
#define FB_ACCEL_OMAP1610	49	/* TI OMAP16xx                  */
#define FB_ACCEL_TRIDENT_TGUI	50	/* Trident TGUI			*/
#define FB_ACCEL_TRIDENT_3DIMAGE 51	/* Trident 3DImage		*/
#define FB_ACCEL_TRIDENT_BLADE3D 52	/* Trident Blade3D		*/
#define FB_ACCEL_TRIDENT_BLADEXP 53	/* Trident BladeXP		*/
#define FB_ACCEL_CIRRUS_ALPINE   53	/* Cirrus Logic 543x/544x/5480	*/
#define FB_ACCEL_NEOMAGIC_NM2070 90	/* NeoMagic NM2070              */
#define FB_ACCEL_NEOMAGIC_NM2090 91	/* NeoMagic NM2090              */
#define FB_ACCEL_NEOMAGIC_NM2093 92	/* NeoMagic NM2093              */
#define FB_ACCEL_NEOMAGIC_NM2097 93	/* NeoMagic NM2097              */
#define FB_ACCEL_NEOMAGIC_NM2160 94	/* NeoMagic NM2160              */
#define FB_ACCEL_NEOMAGIC_NM2200 95	/* NeoMagic NM2200              */
#define FB_ACCEL_NEOMAGIC_NM2230 96	/* NeoMagic NM2230              */
#define FB_ACCEL_NEOMAGIC_NM2360 97	/* NeoMagic NM2360              */
#define FB_ACCEL_NEOMAGIC_NM2380 98	/* NeoMagic NM2380              */
#define FB_ACCEL_PXA3XX		 99	/* PXA3xx			*/

#define FB_ACCEL_SAVAGE4        0x80	/* S3 Savage4                   */
#define FB_ACCEL_SAVAGE3D       0x81	/* S3 Savage3D                  */
#define FB_ACCEL_SAVAGE3D_MV    0x82	/* S3 Savage3D-MV               */
#define FB_ACCEL_SAVAGE2000     0x83	/* S3 Savage2000                */
#define FB_ACCEL_SAVAGE_MX_MV   0x84	/* S3 Savage/MX-MV              */
#define FB_ACCEL_SAVAGE_MX      0x85	/* S3 Savage/MX                 */
#define FB_ACCEL_SAVAGE_IX_MV   0x86	/* S3 Savage/IX-MV              */
#define FB_ACCEL_SAVAGE_IX      0x87	/* S3 Savage/IX                 */
#define FB_ACCEL_PROSAVAGE_PM   0x88	/* S3 ProSavage PM133           */
#define FB_ACCEL_PROSAVAGE_KM   0x89	/* S3 ProSavage KM133           */
#define FB_ACCEL_S3TWISTER_P    0x8a	/* S3 Twister                   */
#define FB_ACCEL_S3TWISTER_K    0x8b	/* S3 TwisterK                  */
#define FB_ACCEL_SUPERSAVAGE    0x8c    /* S3 Supersavage               */
#define FB_ACCEL_PROSAVAGE_DDR  0x8d	/* S3 ProSavage DDR             */
#define FB_ACCEL_PROSAVAGE_DDRK 0x8e	/* S3 ProSavage DDR-K           */

#define FB_ACCEL_PUV3_UNIGFX	0xa0	/* PKUnity-v3 Unigfx		*/

#define FB_CAP_FOURCC		0x0001	/* Device supports FOURCC-based formats */
#define FB_CAP_Y420_DC_30	0x0002	/* YCbCr 4:2:0 deep color 30bpp */
#define FB_CAP_Y420_DC_36	0x0004	/* YCbCr 4:2:0 deep color 36bpp */
#define FB_CAP_Y420_DC_48	0x0008	/* YCbCr 4:2:0 deep color 48bpp */
#define FB_CAP_DC_Y420_MASK		(FB_CAP_Y420_DC_30 | \
				FB_CAP_Y420_DC_36 | FB_CAP_Y420_DC_48)

#define FB_CAP_Y422_DC_30	0x0010	/* YCbCr 4:2:2 deep color 30bpp */
#define FB_CAP_Y422_DC_36	0x0020	/* YCbCr 4:2:2 deep color 36bpp */
#define FB_CAP_Y422_DC_48	0x0040	/* YCbCr 4:2:2 deep color 48bpp */
#define FB_CAP_DC_Y422_MASK		(FB_CAP_Y422_DC_30 | \
				FB_CAP_Y422_DC_36 | FB_CAP_Y422_DC_48)

#define FB_CAP_Y444_DC_30	0x0080	/* YCbCr 4:4:4 deep color 30bpp */
#define FB_CAP_Y444_DC_36	0x0100	/* YCbCr 4:4:4 deep color 36bpp */
#define FB_CAP_Y444_DC_48	0x0200	/* YCbCr 4:4:4 deep color 48bpp */
#define FB_CAP_DC_Y444_MASK		(FB_CAP_Y444_DC_30 | \
				FB_CAP_Y444_DC_36 | FB_CAP_Y444_DC_48)

#define FB_CAP_RGB_DC_30	0x0400	/* RGB 4:4:4 deep color 30bpp */
#define FB_CAP_RGB_DC_36	0x0800	/* RGB 4:4:4 deep color 36bpp */
#define FB_CAP_RGB_DC_48	0x1000	/* RGB 4:4:4 deep color 48bpp */
#define FB_CAP_DC_RGB_MASK		(FB_CAP_RGB_DC_30 | \
				FB_CAP_RGB_DC_36 | FB_CAP_RGB_DC_48)

#define FB_CAP_DC_MASK		(FB_CAP_DC_Y420_MASK | FB_CAP_DC_Y422_MASK | \
				FB_CAP_DC_Y444_MASK | FB_CAP_DC_RGB_MASK)

#define FB_CAP_Y422		0x2000	/* YCbCr 4:2:2 support */
#define FB_CAP_Y444		0x4000	/* YCbCr 4:4:4 support */
#define FB_CAP_HDR		0x8000	/* Device supports HDR*/
/* Device supports selectable RGB range */
#define FB_CAP_RGB_QUANT_SELECTABLE		0x10000
/* Device supports selectable YUV range */
#define FB_CAP_YUV_QUANT_SELECTABLE		0x20000

#define FB_COL_XVYCC601		0x1
#define FB_COL_XVYCC709		0x2
#define FB_COL_SYCC601		0x4
#define FB_COL_ADOBE_YCC601	0x8
#define FB_COL_ADOBE_RGB	0x10
#define FB_COL_BT2020_CYCC	0x20
#define FB_COL_BT2020_YCC	0x40
#define FB_COL_BT2020_RGB	0x80

struct fb_fix_screeninfo {
	char id[16];			/* identification string eg "TT Builtin" */
	unsigned long smem_start;	/* Start of frame buffer mem */
					/* (physical address) */
	__u32 smem_len;			/* Length of frame buffer mem */
	__u32 type;			/* see FB_TYPE_*		*/
	__u32 type_aux;			/* Interleave for interleaved Planes */
	__u32 visual;			/* see FB_VISUAL_*		*/
	__u16 xpanstep;			/* zero if no hardware panning  */
	__u16 ypanstep;			/* zero if no hardware panning  */
	__u16 ywrapstep;		/* zero if no hardware ywrap    */
	__u32 line_length;		/* length of a line in bytes    */
	unsigned long mmio_start;	/* Start of Memory Mapped I/O   */
					/* (physical address) */
	__u32 mmio_len;			/* Length of Memory Mapped I/O  */
	__u32 accel;			/* Indicate to driver which	*/
					/*  specific chip/card we have	*/
	__u16 capabilities;		/* see FB_CAP_*			*/
	__u16 max_clk_rate;	/* max supported clock rate on link in Mhz */
	__u16 colorimetry;		/* see FB_COL_* */
};

/* Interpretation of offset for color fields: All offsets are from the right,
 * inside a "pixel" value, which is exactly 'bits_per_pixel' wide (means: you
 * can use the offset as right argument to <<). A pixel afterwards is a bit
 * stream and is written to video memory as that unmodified.
 *
 * For pseudocolor: offset and length should be the same for all color
 * components. Offset specifies the position of the least significant bit
 * of the pallette index in a pixel value. Length indicates the number
 * of available palette entries (i.e. # of entries = 1 << length).
 */
struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */
					/* right */
};

#define FB_NONSTD_HAM		1	/* Hold-And-Modify (HAM)        */
#define FB_NONSTD_REV_PIX_IN_B	2	/* order of pixels in each byte is reversed */

#define FB_ACTIVATE_NOW		0	/* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN	1	/* activate on next open	*/
#define FB_ACTIVATE_TEST	2	/* don't set, round up impossible */
#define FB_ACTIVATE_MASK       15
					/* values			*/
#define FB_ACTIVATE_VBL	       16	/* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL     32	/* change colormap on vbl	*/
#define FB_ACTIVATE_ALL	       64	/* change all VCs on this fb	*/
#define FB_ACTIVATE_FORCE     128	/* force apply even when no change*/
#define FB_ACTIVATE_INV_MODE  256       /* invalidate videomode */

#define FB_ACCELF_TEXT		1	/* (OBSOLETE) see fb_info.flags and vc_mode */

#define FB_SYNC_HOR_HIGH_ACT	1	/* horizontal sync high active	*/
#define FB_SYNC_VERT_HIGH_ACT	2	/* vertical sync high active	*/
#define FB_SYNC_EXT		4	/* external sync		*/
#define FB_SYNC_COMP_HIGH_ACT	8	/* composite sync high active   */
#define FB_SYNC_BROADCAST	16	/* broadcast video timings      */
					/* vtotal = 144d/288n/576i => PAL  */
					/* vtotal = 121d/242n/484i => NTSC */
#define FB_SYNC_ON_GREEN	32	/* sync on green */

#define FB_VMODE_NONINTERLACED  0x0000	/* non interlaced */
#define FB_VMODE_INTERLACED	0x0001	/* interlaced	*/
#define FB_VMODE_DOUBLE		0x0002	/* double scan */
#define FB_VMODE_ODD_FLD_FIRST	0x0004	/* interlaced: top line first */

#define FB_VMODE_SCAN_MASK	(FB_VMODE_NONINTERLACED | \
				 FB_VMODE_INTERLACED | FB_VMODE_DOUBLE | \
				 FB_VMODE_ODD_FLD_FIRST)

#define FB_VMODE_Y420		0x0008	/* YCrCb 4:2:0 also supported or
					 * select YCrCb 4:2:0 if setting mode
					 */
#define FB_VMODE_Y420_ONLY	0x0010	/* YCrCb 4:2:0 only supported */
#define FB_VMODE_Y422		0x0020	/* select YCrCb 4:2:2 if setting mode */
#define FB_VMODE_Y444		0x0040	/* select YCrCb 4:4:4 if setting mode */

#define FB_VMODE_Y24		0x0100	/* select 8 bits per component */
#define FB_VMODE_Y30		0x0200	/* select 10 bits per component */
#define FB_VMODE_Y36		0x0400	/* select 12 bits per component */
#define FB_VMODE_Y48		0x0800	/* select 16 bits per component */

#define FB_VMODE_SET_YUV_MASK	(FB_VMODE_Y420 | FB_VMODE_Y422 | \
				FB_VMODE_Y444 | FB_VMODE_Y24 | FB_VMODE_Y30 | \
				FB_VMODE_Y36 | FB_VMODE_Y48)

#define FB_VMODE_YUV_MASK	(FB_VMODE_Y420_ONLY | FB_VMODE_SET_YUV_MASK)

/* extended colorimetry */
#define FB_VMODE_EC_ENABLE	0x1000
#define FB_VMODE_EC_SHIFT	13
#define FB_VMODE_EC_MASK	(0xf << FB_VMODE_EC_SHIFT)
#define FB_VMODE_EC_XVYCC601	((0x0 << FB_VMODE_EC_SHIFT) & FB_VMODE_EC_MASK)
#define FB_VMODE_EC_XVYCC709	((0x1 << FB_VMODE_EC_SHIFT) & FB_VMODE_EC_MASK)
#define FB_VMODE_EC_SYCC601	((0x2 << FB_VMODE_EC_SHIFT) & FB_VMODE_EC_MASK)
#define FB_VMODE_EC_ADOBE_YCC601	((0x3 << FB_VMODE_EC_SHIFT) & \
					FB_VMODE_EC_MASK)
#define FB_VMODE_EC_ADOBE_RGB	((0x4 << FB_VMODE_EC_SHIFT) & FB_VMODE_EC_MASK)
#define FB_VMODE_EC_BT2020_CYCC	((0x5 << FB_VMODE_EC_SHIFT) & \
					FB_VMODE_EC_MASK)
#define FB_VMODE_EC_BT2020_YCC_RGB	((0x6 << FB_VMODE_EC_SHIFT) & \
					FB_VMODE_EC_MASK)

#define FB_VMODE_1000DIV1001	0x020000
#define FB_VMODE_IS_DETAILED	0x200000
#define FB_VMODE_IS_CEA		0x400000
#define FB_VMODE_IS_HDMI_EXT	0x800000
#define FB_VMODE_ADJUSTED	0x000080 /* adj to meet timing restrictions */
#define FB_VMODE_VRR	0x08000000

#define FB_VMODE_LIMITED_RANGE	0x10000000

#define FB_VMODE_MASK		0x18e3ffff

#define FB_VMODE_YWRAP		0x40000  /* ywrap instead of panning */
#define FB_VMODE_SMOOTH_XPAN	0x80000  /* smooth xpan possible (internally used) */
#define FB_VMODE_CONUPDATE	0x100000 /* don't update x/yoffset */

#define FB_FLAG_RATIO_4_3	64
#define FB_FLAG_RATIO_16_9	128
#define FB_FLAG_RATIO_64_27	512
#define FB_FLAG_RATIO_256_135	1024
#define FB_FLAG_RATIO_MASK	(FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9 |\
				FB_FLAG_RATIO_64_27 | FB_FLAG_RATIO_256_135)
#define FB_FLAG_PIXEL_REPEAT	256

/*
 * Stereo modes
 */
#define FB_VMODE_STEREO_NONE        0x00000000  /* not stereo */
#define FB_VMODE_STEREO_FRAME_PACK  0x01000000  /* frame packing */
#define FB_VMODE_STEREO_TOP_BOTTOM  0x02000000  /* top-bottom */
#define FB_VMODE_STEREO_LEFT_RIGHT  0x04000000  /* left-right */
#define FB_VMODE_STEREO_MASK        0x07000000

/*
 * Display rotation support
 */
#define FB_ROTATE_UR      0
#define FB_ROTATE_CW      1
#define FB_ROTATE_UD      2
#define FB_ROTATE_CCW     3

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

struct fb_var_screeninfo {
	__u32 xres;			/* visible resolution		*/
	__u32 yres;
	__u32 xres_virtual;		/* virtual resolution		*/
	__u32 yres_virtual;
	__u32 xoffset;			/* offset from virtual to visible */
	__u32 yoffset;			/* resolution			*/

	__u32 bits_per_pixel;		/* guess what			*/
	__u32 grayscale;		/* 0 = color, 1 = grayscale,	*/
					/* >1 = FOURCC			*/
	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/

	__u32 nonstd;			/* != 0 Non standard pixel format */

	__u32 activate;			/* see FB_ACTIVATE_*		*/

	__u32 height;			/* height of picture in mm    */
	__u32 width;			/* width of picture in mm     */

	__u32 accel_flags;		/* (OBSOLETE) see fb_info.flags */

	/* Timing: All values in pixclocks, except pixclock (of course) */
	__u32 pixclock;			/* pixel clock in ps (pico seconds) */
	__u32 left_margin;		/* time from sync to picture	*/
	__u32 right_margin;		/* time from picture to sync	*/
	__u32 upper_margin;		/* time from sync to picture	*/
	__u32 lower_margin;
	__u32 hsync_len;		/* length of horizontal sync	*/
	__u32 vsync_len;		/* length of vertical sync	*/
	__u32 sync;			/* see FB_SYNC_*		*/
	__u32 vmode;			/* see FB_VMODE_*		*/
	__u32 rotate;			/* angle we rotate counter clockwise */
	__u32 colorspace;		/* colorspace for FOURCC-based modes */
	__u32 reserved[4];		/* Reserved for future compatibility */
};

struct fb_cmap {
	__u32 start;			/* First entry	*/
	__u32 len;			/* Number of entries */
	__u16 *red;			/* Red values	*/
	__u16 *green;
	__u16 *blue;
	__u16 *transp;			/* transparency, can be NULL */
};

struct fb_con2fbmap {
	__u32 console;
	__u32 framebuffer;
};

/* VESA Blanking Levels */
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3


enum {
	/* screen: unblanked, hsync: on,  vsync: on */
	FB_BLANK_UNBLANK       = VESA_NO_BLANKING,

	/* screen: blanked,   hsync: on,  vsync: on */
	FB_BLANK_NORMAL        = VESA_NO_BLANKING + 1,

	/* screen: blanked,   hsync: on,  vsync: off */
	FB_BLANK_VSYNC_SUSPEND = VESA_VSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: on */
	FB_BLANK_HSYNC_SUSPEND = VESA_HSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: off */
	FB_BLANK_POWERDOWN     = VESA_POWERDOWN + 1
};

#define FB_VBLANK_VBLANKING	0x001	/* currently in a vertical blank */
#define FB_VBLANK_HBLANKING	0x002	/* currently in a horizontal blank */
#define FB_VBLANK_HAVE_VBLANK	0x004	/* vertical blanks can be detected */
#define FB_VBLANK_HAVE_HBLANK	0x008	/* horizontal blanks can be detected */
#define FB_VBLANK_HAVE_COUNT	0x010	/* global retrace counter is available */
#define FB_VBLANK_HAVE_VCOUNT	0x020	/* the vcount field is valid */
#define FB_VBLANK_HAVE_HCOUNT	0x040	/* the hcount field is valid */
#define FB_VBLANK_VSYNCING	0x080	/* currently in a vsync */
#define FB_VBLANK_HAVE_VSYNC	0x100	/* verical syncs can be detected */

struct fb_vblank {
	__u32 flags;			/* FB_VBLANK flags */
	__u32 count;			/* counter of retraces since boot */
	__u32 vcount;			/* current scanline position */
	__u32 hcount;			/* current scandot position */
	__u32 reserved[4];		/* reserved for future compatibility */
};

/* Internal HW accel */
#define ROP_COPY 0
#define ROP_XOR  1

struct fb_copyarea {
	__u32 dx;
	__u32 dy;
	__u32 width;
	__u32 height;
	__u32 sx;
	__u32 sy;
};

struct fb_fillrect {
	__u32 dx;	/* screen-relative */
	__u32 dy;
	__u32 width;
	__u32 height;
	__u32 color;
	__u32 rop;
};

struct fb_image {
	__u32 dx;		/* Where to place image */
	__u32 dy;
	__u32 width;		/* Size of image */
	__u32 height;
	__u32 fg_color;		/* Only used when a mono bitmap */
	__u32 bg_color;
	__u8  depth;		/* Depth of the image */
	const char *data;	/* Pointer to image data */
	struct fb_cmap cmap;	/* color map info */
};

/*
 * hardware cursor control
 */

#define FB_CUR_SETIMAGE 0x01
#define FB_CUR_SETPOS   0x02
#define FB_CUR_SETHOT   0x04
#define FB_CUR_SETCMAP  0x08
#define FB_CUR_SETSHAPE 0x10
#define FB_CUR_SETSIZE	0x20
#define FB_CUR_SETALL   0xFF

struct fbcurpos {
	__u16 x, y;
};

struct fb_cursor {
	__u16 set;		/* what to set */
	__u16 enable;		/* cursor on/off */
	__u16 rop;		/* bitop operation */
	const char *mask;	/* cursor mask bits */
	struct fbcurpos hot;	/* cursor hot spot */
	struct fb_image	image;	/* Cursor image */
};

#ifdef CONFIG_FB_BACKLIGHT
/* Settings for the generic backlight code */
#define FB_BACKLIGHT_LEVELS	128
#define FB_BACKLIGHT_MAX	0xFF
#endif


#endif /* _UAPI_LINUX_FB_H */
