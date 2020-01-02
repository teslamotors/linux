/*
 * TRAV CSIS camera interface driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#include "trav-csis.h"

#define CSIS_MODULE_NAME	"csis"

#define CSIS_MODULE_VERSION	"0.1.0"

MODULE_DESCRIPTION("TRAV CSIS Driver");
MODULE_AUTHOR("Sathyakam M, <sathya@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(CSIS_MODULE_VERSION);

static unsigned video_nr = -1;
module_param(video_nr, uint, 0644);
MODULE_PARM_DESC(video_nr, "videoX start number, -1 is autodetect");

static unsigned debug;
module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "activities debug info");

#define csis_dbg(level, dev, fmt, arg...)   \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ##arg)

#define csis_warn(dev, fmt, arg...) \
	v4l2_warn(&dev->v4l2_dev, fmt, ##arg)

#define csis_info(dev, fmt, arg...) \
	v4l2_info(&dev->v4l2_dev, fmt, ##arg)

#define csis_err(dev, fmt, arg...)  \
	v4l2_err(&dev->v4l2_dev, fmt, ##arg)

#define csis_ctx_dbg(level, ctx, fmt, arg...)        \
	v4l2_dbg(level, debug, &ctx->v4l2_dev, fmt, ##arg)

#define csis_ctx_info(ctx, fmt, arg...)      \
	v4l2_info(&ctx->v4l2_dev, fmt, ##arg)

#define csis_ctx_err(ctx, fmt, arg...)       \
	v4l2_err(&ctx->v4l2_dev, fmt, ##arg)

#define bytes_per_line(width, bpp) (ALIGN(((width * bpp) >> 3), 16))

static void __iomem *csis_sys_reg;
static bool csis_pll_init_not_done = true;

const static struct csis_fmt csis_formats[] = {
	{
		.name		= "RGB565",
		.fourcc		= V4L2_PIX_FMT_RGB565,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB565_1X16,
		.depth		= 16,
	},
	{
		.name		= "RGB666",
		.fourcc		= V4L2_PIX_FMT_BGR666,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB666_1X18,
		.depth		= 18,
	},
	{
		.name		= "RGB888-24",
		.fourcc		= V4L2_PIX_FMT_RGB24,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB888_1X24,
		.depth		= 24,
	},
	{
		.name		= "RGB888-32",
		.fourcc		= V4L2_PIX_FMT_RGB32,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code           = MEDIA_BUS_FMT_RGB888_1X32_PADHI,
		.depth		= 32,
	},
	{
		.name		= "XRGB888",
		.fourcc         = V4L2_PIX_FMT_XRGB32,
		.colorspace     = V4L2_COLORSPACE_SRGB,
		.code           = MEDIA_BUS_FMT_RGB888_1X32_PADHI,
		.depth          = 32,
	}, {
		.name		= "UYVY-16",
		.fourcc		= V4L2_PIX_FMT_UYVY,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_UYVY8_2X8,
		.depth		= 16,
	}, {
		.name		= "YUV422-8",
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_VYUY8_2X8,
		.depth		= 16,
	}, {
		.name		= "SBGGR8",
		.fourcc         = V4L2_PIX_FMT_SBGGR8,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SBGGR8_1X8,
		.depth          = 8,
	}, {
		.name		= "SGBRG8",
		.fourcc         = V4L2_PIX_FMT_SGBRG8,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGBRG8_1X8,
		.depth          = 8,
	}, {
		.name		= "SGRBG8",
		.fourcc         = V4L2_PIX_FMT_SGRBG8,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGRBG8_1X8,
		.depth          = 8,
	}, {
		.name		= "SRGGB8",
		.fourcc         = V4L2_PIX_FMT_SRGGB8,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SRGGB8_1X8,
		.depth          = 8,
	}, {
		.name		= "SBGGR10",
		.fourcc         = V4L2_PIX_FMT_SBGGR10,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SBGGR10_1X10,
		.depth          = 10,
	}, {
		.name		= "SGBRG10",
		.fourcc         = V4L2_PIX_FMT_SGBRG10,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGBRG10_1X10,
		.depth          = 10,
	}, {
		.name		= "SGRBG10",
		.fourcc         = V4L2_PIX_FMT_SGRBG10,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGRBG10_1X10,
		.depth          = 10,
	}, {
		.name		= "SRGGB10",
		.fourcc         = V4L2_PIX_FMT_SRGGB10,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SRGGB10_1X10,
		.depth          = 10,
	}, {
		.name		= "SBGGR12",
		.fourcc         = V4L2_PIX_FMT_SBGGR12,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SBGGR12_1X12,
		.depth          = 12,
	}, {
		.name		= "SGBRG12",
		.fourcc         = V4L2_PIX_FMT_SGBRG12,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGBRG12_1X12,
		.depth          = 12,
	}, {
		.name		= "SGRBG12",
		.fourcc         = V4L2_PIX_FMT_SGRBG12,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SGRBG12_1X12,
		.depth          = 12,
	}, {
		.name		= "SRGGB12",
		.fourcc         = V4L2_PIX_FMT_SRGGB12,
		.colorspace     = V4L2_COLORSPACE_RAW,
		.code           = MEDIA_BUS_FMT_SRGGB12_1X12,
		.depth          = 12,
	}, {
		.name		= "JPEG",
		.fourcc         = V4L2_PIX_FMT_JPEG,
		.colorspace     = V4L2_COLORSPACE_JPEG,
		.code           = MEDIA_BUS_FMT_JPEG_1X8,
		.depth          = 16,
	},
};

/* buffer for one video frame */
struct csis_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer  vb;
	struct list_head        list;
	const struct csis_fmt   *fmt;
	unsigned long           sequence_num;
};

struct csis_dmaqueue {
	struct list_head	active;
};

struct csis_dev {
	struct platform_device *pdev;
	struct csis_ctx *ctx[CSIS_NUM_CH];

	struct v4l2_device v4l2_dev;
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_subdev	*sensor[CSIS_NUM_CH];

	struct v4l2_async_notifier	notifier;
	struct v4l2_async_subdev	asd[CSIS_NUM_CH];
	struct v4l2_async_subdev	*asd_list[CSIS_NUM_CH];

	struct mutex		mutex_csis_dma_reg;
	unsigned int		id;
	unsigned long	sequence;
	unsigned int		nb_data_lane;
	void __iomem	*base;
	void __iomem	*dma_base;
	void __iomem	*pll_base;
	void __iomem	*sysreg_base;
	struct resource *res;
	struct resource *res_dma;
	struct resource *res_pll;
	struct resource *res_phy;
	struct resource *res_sysreg;
	int		no_of_active_stream;
	int		stream_enaled[CSIS_NUM_CH];

	int		irq;
	bool		csis_ip_is_on;
	bool		stream_enabled[CSIS_NUM_CH];
};

struct csis_ctx {
	struct csis_dev *dev;


	spinlock_t		slock;
	/* VB2 Queue lokc */
	struct mutex		mutex;
	struct mutex		mutex_buf;

	/* input number */
	unsigned int input;

	struct video_device vdev;

	struct v4l2_fwnode_endpoint endpoint;
	struct v4l2_device v4l2_dev;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_subdev *subdev;

	struct v4l2_fract timesperframe;
	struct vb2_queue vb_vidq;

	unsigned int virtual_channel;

	const struct csis_fmt		*fmt;
	/* Used to store current pixel format */
	struct v4l2_format              v_fmt;
	/* Used to store current mbus frame format */
	struct v4l2_mbus_framefmt       m_fmt;
	const struct csis_fmt *active_fmt[ARRAY_SIZE(csis_formats)];
	unsigned int num_active_fmt;

	struct csis_dmaqueue vidq;
	struct csis_buffer *frame[CSIS_NUM_DMA_OUT_CH];
	dma_addr_t frame_addr[CSIS_NUM_DMA_OUT_CH];

	struct csis_buffer *cur_frame, *next_frame;
	unsigned int num_reqbufs;

	uint8_t prev_dma_ptr;
	uint8_t current_dma_ptr;
	uint8_t number_of_ready_bufs;

	uint32_t prev_frame_counter;
	uint32_t current_frame_counter;

	uint8_t dma_error;
};

/* Utility function to display fourcc */
static char *fourcc_to_str(u32 fmt)
{
	static char code[5];
	code[0] = (unsigned char)(fmt & 0xff);
	code[1] = (unsigned char)((fmt >> 8) & 0xff);
	code[2] = (unsigned char)((fmt >> 16) & 0xff);
	code[3] = (unsigned char)((fmt >> 24) & 0xff);
	code[4] = '\0';
	return code;
}

/* timeperframe: min/max and default */
static const struct v4l2_fract
	csis_tpf_default = {.numerator = 1001,       .denominator = 30000};

static void csis_irq_worker(struct csis_ctx *ctx);

static inline unsigned int csis_read(struct csis_dev *dev, unsigned int offset)
{
	unsigned int val = 0;

	val = readl(dev->base + offset);
	return val;
}

static inline void csis_write(struct csis_dev *dev, unsigned int val, unsigned int offset)
{
	writel(val, (dev->base + offset));
}

static inline unsigned int csis_dma_read(struct csis_dev *dev, unsigned int offset)
{
	unsigned int val = 0;

	val = readl(dev->dma_base + offset);
	return val;
}

static inline void csis_dma_write(struct csis_dev *dev, unsigned int val, unsigned int offset)
{
	writel(val, dev->dma_base + offset);
}

static inline unsigned int csis_pll_read(struct csis_dev *dev, unsigned int offset)
{
	unsigned int val = 0;

	val = readl(dev->pll_base + offset);
	return val;
}

static inline void csis_pll_write(struct csis_dev *dev, unsigned int val, unsigned int offset)
{
	writel(val, dev->pll_base + offset);
}

static inline unsigned int csis_sysreg_read(struct csis_dev *dev, unsigned int offset)
{
	unsigned int val = 0;

	val = readl(dev->sysreg_base + offset);
	return val;
}

static inline void csis_sysreg_write(struct csis_dev *dev, unsigned int val, unsigned int offset)
{
	writel(val, (dev->sysreg_base + offset));
}


static void csis_clear_vid_irqs(struct csis_dev *dev)
{
	unsigned int val = 0;

	val = csis_read(dev, CSIS_INT_SRC0);
	csis_write(dev, val, CSIS_INT_SRC0);
	val = csis_read(dev, CSIS_INT_SRC1);
	csis_write(dev, val, CSIS_INT_SRC1);
}

static void csis_disable_vid_irqs(struct csis_dev *dev)
{
	csis_dbg(1, dev, "%s\n", __func__);

	csis_write(dev, 0x0000000000, CSIS_INT_MSK0);
	csis_write(dev, 0x000000, CSIS_INT_MSK1);
}

static inline unsigned int pix_fmt_to_bpc(unsigned int pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return CSIS_DMA_VID_BITS_WIDTH_8;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		return CSIS_DMA_VID_BITS_WIDTH_10;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		return CSIS_DMA_VID_BITS_WIDTH_12;
	case V4L2_PIX_FMT_SBGGR16:
		return CSIS_DMA_VID_BITS_WIDTH_16;
	default:
		return CSIS_DMA_VID_BITS_WIDTH_8;
	}
}

/* TODO: Need to be moved into respective CMU module */
static void csis_set_pll_clock_values(struct csis_dev *dev)
{
	unsigned int pll_con0 = 0x0, pll_busy = 0x0, val = 0x0;

	if (!csis_pll_init_not_done)
		return;

	/* PLL clock */
	csis_pll_write(dev, PLL_CON0_LOCKTIME_VAL, PLL_LOCKTIME_PLL_CAM_CSI);

	pll_con0 |= PLL_CON0_LOCKTIME_EN;
	pll_con0 |= PLL_CON0_DIV_P_VAL;
	pll_con0 |= PLL_CON0_DIV_M_VAL;
	pll_con0 &= PLL_CON0_DIV_S_VAL;
	csis_pll_write(dev, pll_con0, PLL_CON0_PLL_CAM_CSI);

	/* Wait for PLL to be stable */
	do {
		mdelay(5);
		pll_con0 = csis_pll_read(dev, PLL_CON0_PLL_CAM_CSI);
		pll_busy = ((pll_con0 >> 29) & 0x01);
	} while (!pll_busy);

	pll_con0 |= PLL_CON0_MUX_SEL;
	csis_pll_write(dev, pll_con0, PLL_CON0_PLL_CAM_CSI);
	mdelay(2);
	pll_con0 = csis_pll_read(dev, PLL_CON0_PLL_CAM_CSI);

	val = csis_pll_read(dev, CLK_CON_DIV_DIV_CAM_CSI_BUSD);
	val |= PLL_CON_BUSD_DIV_RATIO;
	csis_pll_write(dev, val, CLK_CON_DIV_DIV_CAM_CSI_BUSD);

	val = csis_pll_read(dev, CLK_CON_DIV_DIV_CAM_CSI0_ACLK);
	val |= PLL_CON_CSI0_ACLK_DIV_RATIO;
	csis_pll_write(dev, val, CLK_CON_DIV_DIV_CAM_CSI0_ACLK);

	val = csis_pll_read(dev, CLK_CON_DIV_DIV_CAM_CSI_BUSP);
	val |= PLL_CON_BUSP_DIV_RATIO;
	csis_pll_write(dev, val, CLK_CON_DIV_DIV_CAM_CSI_BUSP);

	val = csis_pll_read(dev, CLK_CON_DIV_DIV_CAM_CSI1_ACLK);
	val |= PLL_CON_CSI1_ACLK_DIV_RATIO;
	csis_pll_write(dev, val, CLK_CON_DIV_DIV_CAM_CSI1_ACLK);

	val = csis_pll_read(dev, CLK_CON_DIV_DIV_CAM_CSI2_ACLK);
	val |= PLL_CON_CSI2_ACLK_DIV_RATIO;
	csis_pll_write(dev, val, CLK_CON_DIV_DIV_CAM_CSI2_ACLK);

	csis_pll_init_not_done = false;
}

static void csis_sw_reset(struct csis_dev *dev)
{
	unsigned int val = (0x01 << 1);

	do {
		csis_write(dev, val, CSIS_CMN_CTRL);
		val = csis_read(dev, CSIS_CMN_CTRL);
	} while ((val >> 1) & 0x01);
}

static void csis_set_num_of_datalane(struct csis_dev *dev, unsigned int nb_data_lane)
{
	unsigned int val;

	dev->nb_data_lane = nb_data_lane;
	val = csis_read(dev, CSIS_CMN_CTRL);

	val &= ~(0x3 << 8);

	if (nb_data_lane == DATALANE1)
		val |= (0x0 << 8);
	else if (nb_data_lane == DATALANE2)
		val |= (0x1 << 8);
	else if (nb_data_lane == DATALANE3)
		val |= (0x2 << 8);
	else if (nb_data_lane == DATALANE4)
		val |= (0x3 << 8);

	csis_write(dev, val, CSIS_CMN_CTRL);
}

static void csis_enable(struct csis_dev *dev)
{
	unsigned int val;

	val = csis_read(dev, CSIS_CMN_CTRL);
	val |= 1;
	csis_write(dev, val, CSIS_CMN_CTRL);
}

static void csis_disable(struct csis_dev *dev)
{
	unsigned int val;

	val = csis_read(dev, CSIS_CMN_CTRL);
	val &= ~(1);
	csis_write(dev, val, CSIS_CMN_CTRL);
}

//TODO: Care should be taken reset the DPHY only when there is no streaming
//going on
static void csis_dphy_reset_release(struct csis_dev *dev)
{
	unsigned int val = 0x0;

	/* Sys reg value 0 => Reset, 1 => Reset release*/
	csis_sysreg_write(dev, val, SW_RESETN_DPHY);

	do {
		csis_sysreg_write(dev, 0x07, SW_RESETN_DPHY);
		mdelay(2);
		val = csis_sysreg_read(dev, SW_RESETN_DPHY);
	} while (val == 0);
}

static void csis_mipi_dphy_init(struct csis_dev *dev)
{
	unsigned int val = 0;

	csis_dphy_reset_release(dev);

	val = csis_read(dev, DPHY_SCTRL_H);

	//Enable Rx Skew calibration
	val |= (0x01 << 1);

	//Set Rx Skew Calibratin to Max Code Control
	val |= (0x24 << 2);

	csis_write(dev, val, DPHY_SCTRL_H);
}

static void csis_set_dphy_on(struct csis_dev *dev, unsigned int nb_data_lane)
{
	unsigned int val;

	val = csis_read(dev, DPHY_CMN_CTRL);
	val &= ~(0xF);

	if (nb_data_lane == DATALANE1) {
		csis_dbg(1, dev, "[CSIS]Data lane 1\n");
		val |= (DPHYON_CLK | DPHYON_DATA0);
	} else if (nb_data_lane == DATALANE2) {
		csis_dbg(1, dev, "[CSIS]Data lane 2\n");
		val |= (DPHYON_CLK | DPHYON_DATA0 | DPHYON_DATA1);
	} else if (nb_data_lane == DATALANE3) {
		csis_dbg(1, dev, "[CSIS]Data lane 3\n");
		val |= (DPHYON_CLK | DPHYON_DATA0 | DPHYON_DATA1 | DPHYON_DATA2);
	} else if (nb_data_lane == DATALANE4) {
		csis_dbg(1, dev, "[CSIS]Data lane 4\n");
		val |= (DPHYON_CLK | DPHYON_DATA0 | DPHYON_DATA1 | DPHYON_DATA2 | DPHYON_DATA3);
	}

	val |= (0x01 << 21); //Enable DPHY Slave byte clock
	csis_write(dev, val, DPHY_CMN_CTRL); // D-Phy On.
}

static void csis_set_data_align(struct csis_ctx *ctx, MIPI_DATA_ALIGN data_align)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, vc = ctx->virtual_channel;
	unsigned int reg = ISP_CONFIG_CH0 + (vc * 0x10);

	val = csis_read(dev, reg);

	//Set Parallel bit for 32 bit or normal alignment
	if (data_align == MIPI_32BIT_ALIGN) {
		val |= (0x1 << 11);
	} else {
		val &= ~(0x1 << 11);
	}

	csis_write(dev, val, reg);
}

static void csis_set_img_fmt(struct csis_ctx *ctx, unsigned int vc, const struct csis_fmt *fmt)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, fourcc = fmt->fourcc;

	switch (vc) {
	case VC_CH0:
		val = csis_read(dev, ISP_CONFIG_CH0);
		break;
	case VC_CH1:
		val = csis_read(dev, ISP_CONFIG_CH1);
		break;
	case VC_CH2:
		val = csis_read(dev, ISP_CONFIG_CH2);
		break;
	case VC_CH3:
		val = csis_read(dev, ISP_CONFIG_CH3);
		break;
	default:
		BUG();
		break;
	}

	val &= ~(0xFC);

	if (fourcc == V4L2_PIX_FMT_RGB565) {

		/* RGB565 */
		val |= (0x22 << 2);

	} else if (fourcc == V4L2_PIX_FMT_BGR666) {

		/* RGB666 */
		val |= (0x23 << 2);

	} else if ((fourcc == V4L2_COLORSPACE_SRGB) \
		|| (fourcc == V4L2_PIX_FMT_XRGB32) \
		|| (fourcc == V4L2_PIX_FMT_RGB24) \
		|| (fourcc == V4L2_PIX_FMT_RGB32)) {

		/* RGB888 */
		val |= (0x24 << 2);

	} else if ((fourcc == V4L2_PIX_FMT_YUYV) \
		|| (fourcc == V4L2_PIX_FMT_UYVY)) {

		/* YUYV-16/YUV422-8, UYVY-16 / YUV 422 */
		val |= (0x1E << 2);
        val &= ~(1<<11);

	} else if ((fourcc == V4L2_PIX_FMT_SGBRG8) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG8) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB8)) {

		/* SGBRG8 / RAW8*/
		val |= (0x2A << 2);

	} else if ((fourcc == V4L2_PIX_FMT_SBGGR10) \
		|| (fourcc == V4L2_PIX_FMT_SGBRG10) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG10) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB10)) {

		/* SBGGR10, SGBRG10, SGRBG10, SRGGB10 / RAW10 */
		val |= (0x2B << 2);

	} else if ((fourcc == V4L2_PIX_FMT_SBGGR12) \
		|| (fourcc == V4L2_PIX_FMT_SGBRG12) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG12) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB12)) {

		/*Output data non CSI format*/
		val &= ~(1<<11);

		/* SRGGB12, SGRBG12, SGBRG12, SBGGR12 / RAW-12 */
		val |= (0x2C << 2);

	} else if (fourcc == V4L2_PIX_FMT_JPEG) {

		/* JPEG */
		val |= (0x30 << 2);
		csis_set_data_align(ctx, MIPI_32BIT_ALIGN);
	}

	// Set Virtual Channel ID
	switch (vc) {
	case VC_CH0:
		val |= 0x0;
		csis_write(dev, val, ISP_CONFIG_CH0);
		break;
	case VC_CH1:
		val |= 0x1;
		csis_write(dev, val, ISP_CONFIG_CH1);
		break;
	case VC_CH2:
		val |= 0x2;
		csis_write(dev, val, ISP_CONFIG_CH2);
		break;
	case VC_CH3:
		val |= 0x3;
		csis_write(dev, val, ISP_CONFIG_CH3);
		break;
	}
}

static void csis_set_resolution(struct csis_ctx *ctx, unsigned int vc, u32 width, u32 height)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val = 0;

	val = (width << 0) | (height << 16);

	switch (vc) {
	case VC_CH0:
		csis_write(dev, val, ISP_RESOL_CH0);
		break;
	case VC_CH1:
		csis_write(dev, val, ISP_RESOL_CH1);
		break;
	case VC_CH2:
		csis_write(dev, val, ISP_RESOL_CH2);
		break;
	case VC_CH3:
		csis_write(dev, val, ISP_RESOL_CH3);
		break;
	}
}

static void csis_set_wrapper_timing(struct csis_ctx *ctx, unsigned int vc, unsigned char l_intv)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val;

	switch (vc) {
	case VC_CH0:
		val = csis_read(dev, ISP_SYNC_CH0);
		break;
	case VC_CH1:
		val = csis_read(dev, ISP_SYNC_CH1);
		break;
	case VC_CH2:
		val = csis_read(dev, ISP_SYNC_CH2);
		break;
	case VC_CH3:
		val = csis_read(dev, ISP_SYNC_CH3);
		break;
	default:
		BUG();
		break;
	}

	val &= ~(0x00FFFFFF);

	val |= (l_intv << 18);

	switch (vc) {
	case VC_CH0:
		csis_write(dev, val, ISP_SYNC_CH0);
		break;
	case VC_CH1:
		csis_write(dev, val, ISP_SYNC_CH1);
		break;
	case VC_CH2:
		csis_write(dev, val, ISP_SYNC_CH2);
		break;
	case VC_CH3:
		csis_write(dev, val, ISP_SYNC_CH3);
		break;
	}
}

static void csis_set_hs_settle(struct csis_dev *dev, unsigned char hs_settle)
{
	unsigned int val;

	val = csis_read(dev, DPHY_CMN_CTRL);
	val &= ~(0xFF000000);
	val |= (hs_settle << 24);

	csis_write(dev, val, DPHY_CMN_CTRL);
	val = csis_read(dev, DPHY_CMN_CTRL);
}

static void csis_setclk_settle_ctl(struct csis_dev *dev, unsigned char eClkSettle)
{
	unsigned int val;

	val = csis_read(dev, DPHY_CMN_CTRL);

	val &= ~(0x00C00000);
	val |= (eClkSettle << 22);
	csis_write(dev, val, DPHY_CMN_CTRL);
	val = csis_read(dev, DPHY_CMN_CTRL);
}

static void csis_set_clock_lane_delay(struct csis_dev *dev, unsigned char uValue)
{
	unsigned int val;

	val = csis_read(dev, DPHY_SCTRL_L);

	val &= ~(0xC);
	val |= (uValue << 2);
	csis_write(dev, val, DPHY_SCTRL_L);
}

static void csis_update_shadow_ctx(struct csis_ctx *ctx)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val = 0, vc = ctx->virtual_channel;

	// User have to assert software reset when camera module is turned off.
	val = csis_read(dev, CSIS_CMN_CTRL);

	val |= (0x1 << (16 + vc));
	csis_write(dev, val, CSIS_CMN_CTRL);
}

static void csis_set_clkgate_trail(struct csis_ctx *ctx, unsigned short clkgate_tail)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, vc = ctx->virtual_channel;
	val = csis_read(dev, CSIS_CLK_CTRL);
	val |= (clkgate_tail << (vc + 16));
	csis_write(dev, val, CSIS_CLK_CTRL);
}

static void csis_set_wclk_src_clkgate_en(struct csis_ctx *ctx, bool clk_gate_en)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, vc = ctx->virtual_channel;

	val = csis_read(dev, CSIS_CLK_CTRL);
	val &= ~(0xF0);

	if (clk_gate_en)
		val |= (0x1 << (vc + 4));
	else
		val &= (~(0x1 << (vc + 4)));

	csis_write(dev, val, CSIS_CLK_CTRL);
}

static void csis_set_interleave_mode(struct csis_dev *dev, CSIS_INTERLEAVE intrlv_en)
{
	unsigned int val;

	val = csis_read(dev, CSIS_CMN_CTRL);
	val = val & ~(0x3 << 10);
	val |= (intrlv_en << 10);

	csis_write(dev, val, CSIS_CMN_CTRL);
}

static void csis_enable_deskew_logic(struct csis_dev *dev, bool enable)
{
	unsigned int val;

	val = csis_read(dev, CSIS_CMN_CTRL);

	if (enable)
		val |= (1 << 12);
	else
		val &= (~(1 << 12));

	csis_write(dev, val, CSIS_CMN_CTRL);
}

static void csis_set_vc_passing(struct csis_ctx *ctx)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val;
	unsigned int vc = ctx->virtual_channel;

	val = csis_read(dev, VC_PASSING);

	val |= (vc << 8);
	val |= (0x01 << 7);
	csis_write(dev, val, VC_PASSING);
}

static void csis_dma_enable(struct csis_ctx *ctx, bool en_dma)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int vc = ctx->virtual_channel;
	unsigned int val;

	if (vc == VC_CH0) {
		val = csis_dma_read(dev, DMA0_CTRL);
		val = val   & ~(0x1 << 0);
		val |= (!en_dma << 0);
		csis_dma_write(dev, val, DMA0_CTRL);
	} else if (vc == VC_CH1) {
		val = csis_dma_read(dev, DMA1_CTRL);
		val = val   & ~(0x1 << 0);
		val |= (!en_dma << 0);
		csis_dma_write(dev, val, DMA1_CTRL);
	} else if (vc == VC_CH2) {
		val = csis_dma_read(dev, DMA2_CTRL);
		val = val   & ~(0x1 << 0);
		val |= (!en_dma << 0);
		csis_dma_write(dev, val, DMA2_CTRL);
	} else {
		val = csis_dma_read(dev, DMA3_CTRL);
		val = val   & ~(0x1 << 0);
		val |= (!en_dma << 0);
		csis_dma_write(dev, val, DMA3_CTRL);
	}
}

static void csis_setpack12(struct csis_ctx *ctx, unsigned int vc, unsigned char en_pack12)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val;

	if (vc == VC_CH0) {
		val = csis_dma_read(dev, DMA0_FMT);
		val = val   & ~(0x2 << 16);
		val |= (en_pack12 << 16);
		csis_dma_write(dev, val, DMA0_FMT);
	} else if (vc == VC_CH1) {
		val = csis_dma_read(dev, DMA1_FMT);
		val = val   & ~(0x2 << 16);
		val |= (en_pack12 << 16);
		csis_dma_write(dev, val, DMA1_FMT);
	} else if (vc == VC_CH2) {
		val = csis_dma_read(dev, DMA2_FMT);
		val = val   & ~(0x2 << 16);
		val |= (en_pack12 << 16);
		csis_dma_write(dev, val, DMA2_FMT);
	} else {
		val = csis_dma_read(dev, DMA3_FMT);
		val = val   & ~(0x2 << 16);
		val |= (en_pack12 << 16);
	csis_dma_write(dev, val, DMA3_FMT);
	}
}

static void csis_set_dma_dump(struct csis_ctx *ctx, unsigned int vc, bool set_dump)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val;

	if (vc == VC_CH0) {
		val = csis_dma_read(dev,  DMA0_FMT);
		val = val   & ~(0x1 << 13);
		if (set_dump)
			val |= (1 << 13);
		csis_dma_write(dev, val, DMA0_FMT);
	} else if (vc == VC_CH1) {
		val = csis_dma_read(dev, DMA1_FMT);
		val = val   & ~(0x1 << 13);
		if (set_dump)
			val |= (1 << 13);
		csis_dma_write(dev, val, DMA1_FMT);
	} else if (vc == VC_CH2) {
		val = csis_dma_read(dev, DMA2_FMT);
		val = val   & ~(0x1 << 13);
		if (set_dump)
			val |= (1 << 13);
		csis_dma_write(dev, val, DMA2_FMT);
	} else {
		val = csis_dma_read(dev, DMA3_FMT);
		val = val   & ~(0x1 << 13);
		if (set_dump)
			val |= (1 << 13);
		csis_dma_write(dev, val, DMA3_FMT);
	}
}

void csis_set_dma_dimention(struct csis_ctx *ctx, unsigned int vc, bool set_dim)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val;

	if (vc == VC_CH0) {
		val = csis_dma_read(dev,  DMA0_FMT);
		val = val   & ~(0x1 << 15);
		val |= (set_dim << 15);
		csis_dma_write(dev, val, DMA0_FMT);
	} else if (vc == VC_CH1) {
		val = csis_dma_read(dev, DMA1_FMT);
		val = val   & ~(0x1 << 15);
		val |= (set_dim << 15);
		csis_dma_write(dev, val, DMA1_FMT);
	} else if (vc == VC_CH2) {
		val = csis_dma_read(dev, DMA2_FMT);
		val = val   & ~(0x1 << 15);
		val |= (set_dim << 15);
		csis_dma_write(dev, val, DMA2_FMT);
	} else {
		val = csis_dma_read(dev, DMA3_FMT);
		val = val   & ~(0x1 << 15);
		val |= (set_dim << 15);
		csis_dma_write(dev, val, DMA3_FMT);
	}
}

static void csis_set_dma_format(struct csis_ctx *ctx, const struct csis_fmt *fmt)
{
	unsigned int fourcc = fmt->fourcc;

	if ((fourcc == V4L2_PIX_FMT_SBGGR10) \
		|| (fourcc == V4L2_PIX_FMT_SGBRG10) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG10) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB10)) {
		csis_setpack12(ctx, ctx->virtual_channel, 1);
	} else if ((fourcc == V4L2_PIX_FMT_SBGGR12) \
		|| (fourcc == V4L2_PIX_FMT_SBGGR12) \
		|| (fourcc == V4L2_PIX_FMT_SGBRG12) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG12) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB12)) {
		csis_setpack12(ctx, ctx->virtual_channel, 2);
	} else
		csis_setpack12(ctx, ctx->virtual_channel, 0);

	csis_set_dma_dump(ctx, ctx->virtual_channel, false);
	csis_set_dma_dimention(ctx, ctx->virtual_channel, false);
}

void csis_set_2ppc(struct csis_ctx *ctx, unsigned int vc, const struct csis_fmt *fmt)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, fourcc = fmt->fourcc;

	if (vc == VC_CH0) {
		val = csis_read(dev, ISP_CONFIG_CH0);
	} else if (vc == VC_CH1) {
		val = csis_read(dev, ISP_CONFIG_CH1);
	} else if (vc == VC_CH2) {
		val = csis_read(dev, ISP_CONFIG_CH2);
	} else if (vc == VC_CH3) {
		val = csis_read(dev, ISP_CONFIG_CH3);
	} else {
		BUG();
	}

	if ((fourcc == V4L2_PIX_FMT_SGBRG8) || (fourcc == V4L2_PIX_FMT_SGRBG8) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB8) || (fourcc == V4L2_PIX_FMT_SBGGR10) \
		|| (fourcc == V4L2_PIX_FMT_SGBRG10) || (fourcc == V4L2_PIX_FMT_SGRBG10) \
		|| (fourcc == V4L2_PIX_FMT_SRGGB10) || (fourcc == V4L2_PIX_FMT_SBGGR12) \
		|| (fourcc == V4L2_PIX_FMT_SBGGR12) || (fourcc == V4L2_PIX_FMT_SGBRG12) \
		|| (fourcc == V4L2_PIX_FMT_SGRBG12) || (fourcc == V4L2_PIX_FMT_SRGGB12) \
		|| (fourcc == V4L2_PIX_FMT_YUYV) || (fourcc == V4L2_PIX_FMT_UYVY)) {

		val |= (1 << 12);

	} else {
		val &= ~(1 << 12);
	}

	if (vc == VC_CH0) {
		csis_write(dev, val, ISP_CONFIG_CH0);
	} else if (vc == VC_CH1) {
		csis_write(dev, val, ISP_CONFIG_CH1);
	} else if (vc == VC_CH2) {
		csis_write(dev, val, ISP_CONFIG_CH2);
	} else if (vc == VC_CH3) {
		csis_write(dev, val, ISP_CONFIG_CH3);
	}
}

void csis_set_dma_clk(struct csis_dev *dev)
{
	unsigned int val = 0x0;

	val = csis_dma_read(dev, DMA_CLK_CTRL);
	val &= (~(0x01));
	val |= (DMA_CLK_GATE_TRAIL << 1);
	csis_dma_write(dev, val, DMA_CLK_CTRL);
}

static int csis_ip_configure(struct csis_dev *dev)
{
	int ret = 0;
	unsigned int i;
	unsigned int num_data_lane = CSIS_NUM_DATA_LANE;

	csis_sw_reset(dev);
	csis_mipi_dphy_init(dev);

	for (i = 0; i < CSIS_NUM_CH; i++) {

		if (dev->ctx[i]) {
			csis_set_vc_passing(dev->ctx[i]);
			num_data_lane = dev->ctx[i]->endpoint.bus.mipi_csi2.num_data_lanes;
		}
	}

	csis_set_interleave_mode(dev, VC_DT_BOTH);
	csis_enable_deskew_logic(dev, true);
	csis_set_hs_settle(dev, S_HSSETTLECTL_VAL);
	csis_setclk_settle_ctl(dev, S_CLKSETTLECTL_VAL);
	csis_set_clock_lane_delay(dev, 0x00);
	csis_set_num_of_datalane(dev, num_data_lane);

	for (i = 0; i < CSIS_NUM_CH; i++) {

		if (dev->ctx[i]) {
			csis_set_wclk_src_clkgate_en(dev->ctx[i], true);
			csis_set_clkgate_trail(dev->ctx[i], CLKGATE_TAIL_VAL);
		}
	}

	csis_set_dphy_on(dev, num_data_lane);

	for (i = 0; i < CSIS_NUM_CH; i++) {

		if (dev->ctx[i]) {
			csis_dma_enable(dev->ctx[i], false);
		}
	}

	csis_set_dma_clk(dev);
	csis_clear_vid_irqs(dev);

	return ret;
}

const static struct csis_fmt *find_format_by_pix(struct csis_ctx *ctx,
						unsigned int pixelformat)
{
	const struct csis_fmt *fmt;
	unsigned int i;

	for (i = 0; i < ctx->num_active_fmt; i++) {
		fmt = ctx->active_fmt[i];
		if (fmt->fourcc == pixelformat)
			return fmt;
	}

	return NULL;
}

const static struct csis_fmt *find_format_by_code(struct csis_ctx *ctx,
						unsigned int pixelformat)
{
	const struct csis_fmt *fmt;
	unsigned int i;

	for (i = 0; i < ctx->num_active_fmt; i++) {
		fmt = ctx->active_fmt[i];
		if (fmt->code == pixelformat)
			return fmt;
	}

	return NULL;
}

static inline struct csis_dev *notifier_to_dev(struct v4l2_async_notifier *n)
{
	return container_of(n, struct csis_dev, notifier);
}

static inline int get_field(unsigned int value, unsigned int mask)
{
	return (value & mask) >> __ffs(mask);
}

static inline void set_field(unsigned int *valp, unsigned int field, unsigned int mask)
{
	unsigned int val = *valp;

	val &= ~mask;
	val |= (field << __ffs(mask)) & mask;
	*valp = val;
}

static inline int csis_runtime_get(struct csis_dev *dev)
{
	int r;

	r = pm_runtime_get_sync(&dev->pdev->dev);

	return r;
}

static inline void csis_runtime_put(struct csis_dev *dev)
{
	pm_runtime_put_sync(&dev->pdev->dev);
}

static void csis_enable_irqs_for_ctx(struct csis_ctx *ctx)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, vc = ctx->virtual_channel;
	unsigned int err_lost_fe = 0, err_lost_fs = 0, err_sot_hs = 0;
	unsigned int dma_otf_ovrlap = 0, dma_frm_end = 0;

	val = csis_read(dev, CSIS_INT_MSK0);

	err_sot_hs = 1 << (vc + 16);
	err_lost_fs = 1 << (vc + 12);
	err_lost_fe = 1 << (vc + 8);

	val |= (err_sot_hs | err_lost_fs | err_lost_fe |
		0x1F);
	csis_write(dev, val, CSIS_INT_MSK0);

	val = csis_read(dev, CSIS_INT_MSK1);

	dma_otf_ovrlap = 1 << (vc + 14);
	dma_frm_end = 1 << (vc + 8);

	val |= (dma_otf_ovrlap | dma_frm_end);
	val |= (1 << 13) | (1 << 12);
	csis_write(dev, val, CSIS_INT_MSK1);
}

static void csis_disable_irqs_for_ctx(struct csis_ctx *ctx)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int val, vc = ctx->virtual_channel;
	unsigned int err_lost_fe = 0, err_lost_fs = 0, err_sot_hs = 0;
	unsigned int dma_otf_ovrlap = 0, dma_frm_end = 0;

	val = csis_read(dev, CSIS_INT_MSK0);

	err_sot_hs = ~(1 << (vc + 16));
	err_lost_fs = ~(1 << (vc + 12));
	err_lost_fe = ~(1 << (vc + 8));

	val &= (err_sot_hs & err_lost_fs & err_lost_fe);
	val &= (~(0x1F));
	csis_write(dev, val, CSIS_INT_MSK0);

	val = csis_read(dev, CSIS_INT_MSK1);

	dma_otf_ovrlap = ~(1 << (vc + 14));
	dma_frm_end = ~(1 << (vc + 8));

	val &= (dma_otf_ovrlap & dma_frm_end);
	val &= ~(1 << 13);
	val &= ~(1 << 12);
	csis_write(dev, val, CSIS_INT_MSK1);
}

static void csis_dma_set_vid_base_addr(struct csis_ctx *ctx, int frm_no, unsigned long addr)
{
	struct csis_dev *dev = ctx->dev;
	unsigned int vc = ctx->virtual_channel;
	unsigned int offset = 0;

	BUG_ON(frm_no >= CSIS_NUM_DMA_OUT_CH);

	if (vc == VC_CH0) {
		offset = DMA0_ADDR1;
	} else if (vc == VC_CH1) {
		offset = DMA1_ADDR1;
	} else if (vc == VC_CH2) {
		offset = DMA2_ADDR1;
	} else if (vc == VC_CH3) {
		offset = DMA3_ADDR1;
	}

	offset = offset + (frm_no * 4);
	mutex_lock(&dev->mutex_csis_dma_reg);
	csis_dma_write(dev, addr, offset);
	mutex_unlock(&dev->mutex_csis_dma_reg);
}


static void csis_dma_set_vid_frame_num(struct csis_ctx *ctx)
{
#ifdef CSIS_HW
	csis_dma_write(dev, val, DMA0_OUT_ADDR);
	csis_dma_write(dev, val, DMA1_OUT_ADDR);
	csis_dma_write(dev, val, DMA2_OUT_ADDR);
	csis_dma_write(dev, val, DMA3_OUT_ADDR);
	csis_dma_write(dev, val, DMA4_OUT_ADDR);
	csis_dma_write(dev, val, DMA5_OUT_ADDR);
	csis_dma_write(dev, val, DMA6_OUT_ADDR);
	csis_dma_write(dev, val, DMA7_OUT_ADDR);
#endif
}

static irqreturn_t csis_irq_handler(int irq_csis, void *data)
{
	struct csis_dev *dev;
	struct csis_ctx *ctx = NULL;
	int vc;
	unsigned int int_src0 = 0x0, int_src1 = 0x0, err_over = 0x0, val1 = 0;
	unsigned int err_sot = 0x0, err_lost_fs = 0x0, err_lost_fe = 0x0;
	unsigned int err_wrong_cfg = 0x0, err_ecc = 0x0, err_crc = 0x0, err_id = 0x0;
	unsigned int dma_frame_end = 0x0;
	unsigned int dma_frame_end_vc = 0x0;
	unsigned int dma_error_vc = 0x0;
	unsigned int dma_otf_overlap = 0x0, dma_abort_done = 0x0, dma_error = 0x0;
	unsigned int dma_err_code = 0x0, dma_trx = 0x0, dma_fifo = 0x0;
	unsigned int int0_err = 0x0, int1_err = 0x0;

	dev = data;

	int_src0 = csis_read(dev, CSIS_INT_SRC0);
	int_src1 = csis_read(dev, CSIS_INT_SRC1);
	dma_err_code = csis_dma_read(dev, DMA_ERR_CODE);

	int0_err = (int_src0 & ((1 << 20) - 1));
	int1_err = (int_src1 >> 12);
	err_sot = ((int_src0 >> 16) & 0x0F);
	err_lost_fs = ((int_src0 >> 12) & 0x0F);
	err_lost_fe = ((int_src0 >> 8) & 0x0F);
	err_over = ((int_src0 >> 4) & 0x01);
	err_wrong_cfg = ((int_src0 >> 3) & 0x01);
	err_ecc = ((int_src0 >> 2) & 0x01);
	err_crc = ((int_src0 >> 1) & 0x01);
	err_id = int_src0 & 0x01;

	dma_otf_overlap = (int_src1 >> 14) & 0x0F;
	dma_abort_done = (int_src1 >> 13) & 0x01;
	dma_error = (int_src1 >> 12) & 0x01;
	dma_frame_end = (int_src1 >> 8) & 0x0F;

	if (dma_frame_end || dma_error) {

		for (vc = 0; vc < CSIS_NUM_CH; vc++) {

			dma_frame_end_vc = (dma_frame_end >> vc) & 0x01;
			if (dma_error)
				dma_error_vc = int_src1 & (CSIS_DMA_CH0_MASK << vc);

			ctx = dev->ctx[vc];

			if (ctx) {
				if (dma_frame_end_vc || dma_error_vc) {
					ctx->dma_error = dma_error_vc;
					csis_irq_worker(ctx);
				}
			}
		}
	}

	/* Check any errors */
	if (int0_err || int1_err) {

		if (int0_err) {

			if (err_wrong_cfg) {
				csis_err(dev, "Wrong ISP configuration! %x\n", int_src0);
				val1 = csis_read(dev, ISP_CONFIG_CH0);
				csis_err(dev, "ISP_CONFIG_CH0 %x\n", val1);
				val1 = csis_read(dev, ISP_CONFIG_CH1);
				csis_err(dev, "ISP_CONFIG_CH1 %x\n", val1);
				val1 = csis_read(dev, ISP_CONFIG_CH2);
				csis_err(dev, "ISP_CONFIG_CH2 %x\n", val1);
				val1 = csis_read(dev, ISP_CONFIG_CH3);
				csis_err(dev, "ISP_CONFIG_CH3 %x\n", val1);
			}

			if (err_sot) {
				csis_err(dev, "SoT Error %x\n", err_sot);
			}
			if (err_lost_fs) {
				csis_err(dev, "Lost Frame Start %x\n", err_lost_fs);
			}

			if (err_lost_fe) {
				csis_err(dev, "Lost Frame End %x\n", err_lost_fe);
			}

			if (err_over) {
				csis_err(dev, "Overflow Error %x\n", err_over);
			}

			if (err_ecc) {
				csis_err(dev, "ECC Error %x\n", err_ecc);
			}

			if (err_crc) {
				csis_err(dev, "CRC Error %x\n", err_crc);
			}

			if (err_id) {
				csis_ctx_err(ctx, "Wrong ID %x\n", err_id);
			}
		}

		if (int1_err) {
				csis_err(dev, "DMA Error! %x Error code %x Trx status %x Fifo status %x\n", int_src1, dma_err_code, dma_trx, dma_fifo);
		}
	}

	/* Clear the interrupt 0,1 status */
	csis_write(dev, int_src0, CSIS_INT_SRC0);
	csis_write(dev, int_src1, CSIS_INT_SRC1);

	return IRQ_HANDLED;
}

static int __subdev_get_format(struct csis_ctx *ctx,
				struct v4l2_mbus_framefmt *frmfmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &sd_fmt.format;
	int ret;

	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sd_fmt.pad = 0;

	ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
			       pad, get_fmt, NULL, &sd_fmt);

	if (ret)
		return ret;

	*frmfmt = *mbus_fmt;

	csis_ctx_dbg(1, ctx, "%s %dx%d code:%04X\n", __func__, frmfmt->width,
		     frmfmt->height, frmfmt->code);

	return 0;
}

static int __subdev_set_format(struct csis_ctx *ctx,
				struct v4l2_mbus_framefmt *frmfmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &sd_fmt.format;
	int ret;

	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sd_fmt.pad = 0;
	*mbus_fmt = *frmfmt;

	ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
					pad, set_fmt, NULL, &sd_fmt);

	csis_ctx_dbg(3, ctx, "__subdev_set_format ret %d\n", ret);

	if (ret)
		return ret;

	*frmfmt = *mbus_fmt;

	return 0;
}

static int csis_format_size(struct csis_ctx *ctx,
				const struct csis_fmt *fmt,
				struct v4l2_format *f)
{
	csis_ctx_dbg(3, ctx, "%s\n", __func__);
	if (!fmt) {
		csis_ctx_dbg(3, ctx, "No format provided\n");
		return -EINVAL;
	}

	v4l_bound_align_image(&f->fmt.pix.width, CSIS_WMIN, CSIS_WMAX,
				CSIS_WALIGN, &f->fmt.pix.height, CSIS_HMIN,
				CSIS_HMAX, CSIS_HALIGN, CSIS_SALIGN);

	f->fmt.pix.bytesperline = bytes_per_line(f->fmt.pix.width,
						fmt->depth);

	f->fmt.pix.sizeimage = f->fmt.pix.height *
				f->fmt.pix.bytesperline;

	csis_set_resolution(ctx, ctx->virtual_channel, f->fmt.pix.width,
			    f->fmt.pix.height);

	csis_ctx_dbg(3, ctx, "%s: fourcc %s width %d height %d bpl %d img size\
		     %d\n", __func__, fourcc_to_str(f->fmt.pix.pixelformat),
		     f->fmt.pix.width, f->fmt.pix.height,
		     f->fmt.pix.bytesperline, f->fmt.pix.sizeimage);

	return 0;
}

/**
 * Video device ioctls
**/
static int csis_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct csis_ctx *ctx = video_drvdata(file);

	strlcpy(cap->driver, CSIS_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, CSIS_MODULE_NAME, sizeof(cap->card));

	snprintf(cap->bus_info, sizeof(cap->bus_info),
			"platform:%s", ctx->v4l2_dev.name);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int csis_enum_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_fmtdesc *f)
{
	struct csis_ctx *ctx = video_drvdata(file);
	const struct csis_fmt *fmt = NULL;

	if (f->index >= ctx->num_active_fmt)
		return -EINVAL;

	fmt = ctx->active_fmt[f->index];

	f->pixelformat = fmt->fourcc;
	f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int csis_try_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct csis_ctx *ctx = video_drvdata(file);
	const struct csis_fmt *fmt;
	struct v4l2_subdev_frame_size_enum fse;
	int ret, found;

	fmt = find_format_by_pix(ctx, f->fmt.pix.pixelformat);

	if (!fmt) {
		csis_ctx_dbg(3, ctx, "Fourcc format (0x%08x) not found\n", f->fmt.pix.pixelformat);

		/* Just get the first one enumerated */
		fmt = ctx->active_fmt[0];
		f->fmt.pix.pixelformat = fmt->fourcc;
		f->fmt.pix.colorspace = fmt->colorspace;
	}

	f->fmt.pix.field = ctx->v_fmt.fmt.pix.field;

	/* check for / find a valid width, height */
	ret = 0;
	found = false;
	fse.pad = 0;
	fse.code = fmt->code;
	fse.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	for (fse.index = 0; ; fse.index++) {
		ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
					pad, enum_frame_size, NULL, &fse);

		if (ret)
			break;

		if (((f->fmt.pix.width == fse.max_width) &&
			(f->fmt.pix.height == fse.max_height)) ||
			((f->fmt.pix.width <= fse.max_width) &&
			((f->fmt.pix.height >= fse.min_height) &&
			(f->fmt.pix.height <= fse.min_height)))) {
				found = true;
				break;
		}
	}

	if (!found) {
		csis_ctx_dbg(3, ctx, "%s resolution %dx%d not found. Defaulting to %dx%d\n",
			     __func__, f->fmt.pix.width, f->fmt.pix.height, ctx->v_fmt.fmt.pix.width, ctx->v_fmt.fmt.pix.height);
		csis_ctx_dbg(3, ctx, "pixel format %x bpl %d size %d colorspace %x\n", ctx->v_fmt.fmt.pix.pixelformat,
			     ctx->v_fmt.fmt.pix.bytesperline, ctx->v_fmt.fmt.pix.sizeimage, ctx->v_fmt.fmt.pix.colorspace);
		/* use existing values as default */
		f->fmt.pix.width = ctx->v_fmt.fmt.pix.width;
		f->fmt.pix.height =  ctx->v_fmt.fmt.pix.height;
	}

	return csis_format_size(ctx, fmt, f);
}

static int csis_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct csis_ctx *ctx = video_drvdata(file);
	struct vb2_queue *q = &ctx->vb_vidq;
	const struct csis_fmt *fmt;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;

	if (vb2_is_busy(q)) {
		csis_ctx_dbg(3, ctx, "%s device busy: %d\n", __func__, q->num_buffers);
		return -EBUSY;
	}

	ret = csis_try_fmt_vid_cap(file, priv, f);

	if (ret < 0) {
		csis_ctx_err(ctx, "%x try format failed\n", f->fmt.pix.pixelformat);
		return ret;
	}

	fmt = find_format_by_pix(ctx, f->fmt.pix.pixelformat);

	if (!fmt) {
		csis_ctx_err(ctx, "Fourcc format (0x%08x) not found\n",
					f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	v4l2_fill_mbus_format(&mbus_fmt, &f->fmt.pix, fmt->code);

	ret = __subdev_set_format(ctx, &mbus_fmt);

	if (ret) {
		csis_ctx_err(ctx, "%x not supported by subdev\n", f->fmt.pix.pixelformat);
		return ret;
	}

	if (mbus_fmt.code != fmt->code) {
		csis_ctx_dbg(3, ctx,
			"%s changed format! This should not happen.\n",
			__func__);
		return -EINVAL;
	}

	v4l2_fill_pix_format(&ctx->v_fmt.fmt.pix, &mbus_fmt);
	ctx->v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ctx->v_fmt.fmt.pix.pixelformat = fmt->fourcc;
	ctx->v_fmt.fmt.pix.colorspace = fmt->colorspace;
	ctx->fmt = fmt;
	ctx->m_fmt = mbus_fmt;

	csis_set_img_fmt(ctx, ctx->virtual_channel, fmt);
	csis_format_size(ctx, fmt, &ctx->v_fmt);

	csis_set_2ppc(ctx, ctx->virtual_channel, fmt);
	csis_set_wrapper_timing(ctx, ctx->virtual_channel, HSYNC_LINTV);
	csis_set_dma_format(ctx, fmt);
	csis_update_shadow_ctx(ctx);

	*f = ctx->v_fmt;

	return 0;
}

static int csis_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct csis_ctx *ctx = video_drvdata(file);

	csis_info(ctx->dev, "%s\n", __func__);

	*f = ctx->v_fmt;

	return 0;
}

static int csis_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	struct csis_ctx *ctx = video_drvdata(file);
	const struct csis_fmt *fmt;
	struct v4l2_subdev_frame_size_enum fse;
	int ret;

	fmt = find_format_by_pix(ctx, fsize->pixel_format);
	if (!fmt) {
		csis_ctx_dbg(3, ctx, "Invalid pixel code: %x\n",
				fsize->pixel_format);
		return -EINVAL;
	}

	fse.index = fsize->index;
	fse.pad = 0;
	fse.code = fmt->code;

	ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
					pad, enum_frame_size, NULL, &fse);

	if (ret)
		return ret;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = fse.max_width;
	fsize->discrete.height = fse.max_height;

	return 0;
}

static int csis_enum_frameintervals(struct file *file, void *priv,
					struct v4l2_frmivalenum *fival)
{
	struct csis_ctx *ctx = video_drvdata(file);
	const struct csis_fmt *fmt;
	struct v4l2_subdev_frame_interval_enum fie = {
		.index = fival->index,
		.width = fival->width,
		.height = fival->height,
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	int ret;

	fmt = find_format_by_pix(ctx, fival->pixel_format);
	if (!fmt)
		return -EINVAL;

	fie.code = fmt->code;
	ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
				pad, enum_frame_interval, NULL, &fie);

	if (ret)
		return ret;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete = fie.interval;

	return 0;
}

static int csis_ioctl_reqbufs(struct file *file, void *priv,
					struct v4l2_requestbuffers *p)
{
	struct csis_ctx *ctx = video_drvdata(file);

	if (p->count < (CSIS_NUM_DMA_OUT_CH + 1)) {
		csis_ctx_err(ctx, "Minimum %d buffers need to be provided!\n", CSIS_NUM_DMA_OUT_CH + 1);
		return -EINVAL;
	}

	ctx->num_reqbufs = p->count;
	return vb2_ioctl_reqbufs(file, priv, p);
}

static int csis_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index >= CSIS_NUM_INPUT)
		return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	snprintf(inp->name, sizeof(inp->name), "Camera %u\n", inp->index);
	return 0;
}

static int csis_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct csis_ctx *ctx = video_drvdata(file);

	*i = ctx->input;

	return 0;
}

static int csis_s_input(struct file *file, void *priv, unsigned int i)
{
	struct csis_ctx *ctx = video_drvdata(file);

	if (i >= CSIS_NUM_INPUT)
		return -EINVAL;

	ctx->input = i;

	return 0;
}

static const struct v4l2_ioctl_ops csis_ioctl_ops = {
	.vidioc_querycap      = csis_querycap,

	.vidioc_enum_fmt_vid_cap  = csis_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = csis_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = csis_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = csis_g_fmt_vid_cap,

	.vidioc_enum_framesizes   = csis_enum_framesizes,
	.vidioc_enum_frameintervals = csis_enum_frameintervals,

	.vidioc_reqbufs       = csis_ioctl_reqbufs,
	.vidioc_querybuf      = vb2_ioctl_querybuf,
	.vidioc_qbuf          = vb2_ioctl_qbuf,
	.vidioc_expbuf	      = vb2_ioctl_expbuf,
	.vidioc_dqbuf         = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf   = vb2_ioctl_prepare_buf,
	.vidioc_create_bufs   = vb2_ioctl_create_bufs,

	.vidioc_streamon      = vb2_ioctl_streamon,
	.vidioc_streamoff     = vb2_ioctl_streamoff,

	.vidioc_enum_input    = csis_enum_input,
	.vidioc_g_input       = csis_g_input,
	.vidioc_s_input       = csis_s_input,

	.vidioc_log_status    = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

/**
 * V4L2 File operations
**/

static const struct v4l2_file_operations csis_fops = {
	.owner		= THIS_MODULE,
	.open           = v4l2_fh_open,
	.release        = vb2_fop_release,
	.read           = vb2_fop_read,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = vb2_fop_mmap,
};

static struct video_device csis_videodev = {
	.name		= CSIS_MODULE_NAME,
	.fops		= &csis_fops,
	.ioctl_ops	= &csis_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static void add_to_ring_buffer(struct csis_ctx *ctx, struct csis_buffer *buf, uint8_t addr)
{
	ctx->frame[addr] = buf;
	ctx->frame_addr[addr] = vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, 0);

	csis_dma_set_vid_base_addr(ctx, addr, ctx->frame_addr[addr]);
}

/**
 * Videobuf operations
**/
static int csis_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct csis_ctx *ctx = vb2_get_drv_priv(vq);
	unsigned size = ctx->v_fmt.fmt.pix.sizeimage;

	if (*nplanes) {
		if (sizes[0] < size)
			return -EINVAL;
		size = sizes[0];
	}

	*nplanes = 1;
	sizes[0] = size;

	csis_ctx_dbg(3, ctx, "nbuffers %d size %d\n", *nbuffers, sizes[0]);

	return 0;
}

static int csis_buffer_prepare(struct vb2_buffer *vb)
{
	struct csis_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct csis_buffer *buf = container_of(vb, struct csis_buffer,
							vb.vb2_buf);
	unsigned long size;

	if (WARN_ON(!ctx->fmt))
		return -EINVAL;

	size = ctx->v_fmt.fmt.pix.sizeimage;
	if (vb2_plane_size(vb, 0) < size) {
		csis_ctx_err(ctx, "Data will not fit into plane (%lu < %lu)\n",
				vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	csis_ctx_dbg(2, ctx, "%s size %lu\n", __func__, size);
	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, size);

	return 0;
}

static void csis_buffer_queue(struct vb2_buffer *vb)
{
	struct csis_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct csis_buffer *buf = container_of(vb, struct csis_buffer, vb.vb2_buf);
	struct csis_dmaqueue *vidq = &ctx->vidq;

	mutex_lock(&ctx->mutex_buf);
	list_add_tail(&buf->list, &vidq->active);
	buf->sequence_num = ctx->dev->sequence++;
	mutex_unlock(&ctx->mutex_buf);
}

static int csis_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct csis_ctx *ctx = vb2_get_drv_priv(q);
	struct csis_dmaqueue *vidq = &ctx->vidq;
	struct csis_buffer *buf, *tmp;
    u64 t_stamp;
	uint8_t i;
	int ret;

	for (i = 0; i < CSIS_NUM_DMA_OUT_CH; i++) {

		mutex_lock(&ctx->mutex_buf);
		if (list_empty(&vidq->active)) {
			mutex_unlock(&ctx->mutex_buf);
			csis_ctx_err(ctx, "Active buffer queue could not fill DMA buffer addresses!\n");
			return -EIO;
		}

		buf = list_entry(vidq->active.next, struct csis_buffer, list);
		list_del(&buf->list);
		add_to_ring_buffer(ctx, buf, i);
		mutex_unlock(&ctx->mutex_buf);
	}

	csis_dma_set_vid_frame_num(ctx);

	/* save last frame counter and dma pointer location just before enabling dma */
	ctx->prev_dma_ptr = (csis_dma_read(ctx->dev, DMA0_ACT_CTRL + ctx->virtual_channel*0x100) & 0x01C) >> 2;
	ctx->prev_frame_counter = csis_read(ctx->dev, FRM_CNT_CH0 + ctx->virtual_channel*4);

	csis_clear_vid_irqs(ctx->dev);
	csis_dma_enable(ctx, true);

	ret = v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel], video, s_stream, 1);
	if (ret) {
		csis_ctx_err(ctx, "Failed to enable streaming in subdev\n");
		csis_runtime_put(ctx->dev);
		goto err;
	}

	csis_enable_irqs_for_ctx(ctx);
	csis_enable(ctx->dev);
	csis_ctx_dbg(0, ctx, "%s vc %d\n", __func__, ctx->virtual_channel);

	return 0;

err:
    t_stamp = ktime_get_ns();
	list_for_each_entry_safe(buf, tmp, &vidq->active, list) {
		list_del(&buf->list);
        buf->vb.vb2_buf.timestamp = t_stamp;
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	return ret;
}

static void csis_stop_streaming(struct vb2_queue *q)
{
	struct csis_ctx *ctx = vb2_get_drv_priv(q);
	struct csis_dmaqueue *vidq = &ctx->vidq;
	struct csis_buffer *buf, *tmp;
	unsigned int timeout_cnt = 0;
    u64 t_stamp;
	int i;

	csis_disable_irqs_for_ctx(ctx);
	csis_dma_enable(ctx, false);

	/* Wait for DMA Operation to finish */
	while ((csis_dma_read(ctx->dev, DMA0_ACT_CTRL + ctx->virtual_channel*0x100) & 0x1) == 0x0) {
		if (timeout_cnt > 50) {
			csis_warn(ctx->dev, "DMA did not finish in 500ms.\n");
			break;
		}
		usleep_range(10000, 20000); /* Wait min 10ms, max 20ms */
		timeout_cnt++;
	}

	/*
	 * If still DMA operation exists after disabled irq, it will
	 * update dma_done part in interrupt source register. For next
	 * streaming session, this could be interpreted as current session's
	 * first frame done. To prevent this incorrect dma_done receiving,
	 * clearing interrupt source register here.
	 */
	csis_clear_vid_irqs(ctx->dev);
	csis_ctx_dbg(3, ctx, "%s vc %d\n", __func__, ctx->virtual_channel);

	if (v4l2_subdev_call(ctx->dev->sensor[ctx->virtual_channel],
				video, s_stream, 0))
		csis_ctx_err(ctx, "Failed to disable streaming in subdev\n");

	/* Release all active buffers */
	mutex_lock(&ctx->mutex_buf);
	t_stamp = ktime_get_ns();
    list_for_each_entry_safe(buf, tmp, &vidq->active, list) {
		list_del(&buf->list);
        buf->vb.vb2_buf.timestamp = t_stamp;
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	mutex_unlock(&ctx->mutex_buf);

	for (i = 0; i < CSIS_NUM_DMA_OUT_CH; i++) {
		buf = ctx->frame[i];
		if (buf) {
            buf->vb.vb2_buf.timestamp = t_stamp;
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		}
	}
}

static const struct vb2_ops csis_video_ops = {
	.queue_setup		= csis_queue_setup,
	.buf_prepare		= csis_buffer_prepare,
	.buf_queue		= csis_buffer_queue,
	.start_streaming	= csis_start_streaming,
	.stop_streaming		= csis_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};


static void csis_irq_worker(struct csis_ctx *ctx)
{
	struct csis_buffer *buf_from;
	struct csis_buffer *buf_to;
	struct csis_dmaqueue *vidq = &ctx->vidq;
	uint8_t i;

	ctx->current_dma_ptr = (csis_dma_read(ctx->dev, DMA0_ACT_CTRL + ctx->virtual_channel*0x100) & 0x01C) >> 2;
	ctx->current_frame_counter = csis_read(ctx->dev, FRM_CNT_CH0 + ctx->virtual_channel*4);

	if (ctx->dma_error) {
		ctx->prev_dma_ptr = ctx->current_dma_ptr;
		goto update_prev_counters;
	}

	if (ctx->current_dma_ptr >= ctx->prev_dma_ptr)
		ctx->number_of_ready_bufs = ctx->current_dma_ptr - ctx->prev_dma_ptr;
	else
		ctx->number_of_ready_bufs = CSIS_NUM_DMA_OUT_CH - ctx->prev_dma_ptr + ctx->current_dma_ptr;

	if ((ctx->current_frame_counter - ctx->prev_frame_counter) >= (CSIS_NUM_DMA_OUT_CH - 1)) {
		/*
		 * In case of more than 8 frames delay, set how may recent frames wanted to be ready
		 * When interrupt happens, 7 frames could be ready, for being cautions set only
		 * 6 of the DMA buffers is ready to dequeue because at the time of dequeuing here,
		 * DMA might have started for next buffer. Address change could be dangerous.
		 */
		ctx->number_of_ready_bufs = (CSIS_NUM_DMA_OUT_CH - 1) - 1;

		ctx->prev_dma_ptr = ((ctx->current_dma_ptr + 1) + 1) % CSIS_NUM_DMA_OUT_CH;

		csis_ctx_err(ctx, "Interrupt delayed more than 8 frames!\n");
	}

	if (ctx->number_of_ready_bufs == 0) {
		csis_ctx_err(ctx, "Interrupt burst number_of_ready_bufs: %d\n", ctx->number_of_ready_bufs);
		return;
	} else {
		if (ctx->number_of_ready_bufs > 1) {
			/* Interrupt has been missed. Do not populate DMA_ACT_CTRL pointer.
			 * Notify buffers ready until (DMA_ACT_CTRL - 1) pointer. Because, the delayed
			 * interrupt might be arrived in DMA active time.
			 */
			ctx->number_of_ready_bufs--;
			csis_ctx_dbg(1, ctx, "interrupt got delayed %d frames\n", ctx->number_of_ready_bufs);
		}
		csis_ctx_dbg(2, ctx, "number_of_ready_bufs: %d\n", ctx->number_of_ready_bufs);
	}

	for (i = 0; i < ctx->number_of_ready_bufs; i++) {
		ctx->prev_dma_ptr = (ctx->prev_dma_ptr + 1) % CSIS_NUM_DMA_OUT_CH;

		mutex_lock(&ctx->mutex_buf);

		/* Before dequeuing buffer from DMA at least one buffer should be ready in vb2_queue */
		if (list_empty(&vidq->active)) {
			mutex_unlock(&ctx->mutex_buf);
			csis_ctx_dbg(1, ctx, "No available buffer in vb2 queue\n");
			ctx->prev_dma_ptr = ctx->current_dma_ptr;
			goto update_prev_counters;
		} else {
			buf_from = list_entry(vidq->active.next, struct csis_buffer, list);
			list_del(&buf_from->list);
		}

		mutex_unlock(&ctx->mutex_buf);

		buf_to = ctx->frame[ctx->prev_dma_ptr];

		add_to_ring_buffer(ctx, buf_from, ctx->prev_dma_ptr);

		if (buf_to) {
			buf_to->vb.vb2_buf.timestamp = ktime_get_ns();
			vb2_buffer_done(&buf_to->vb.vb2_buf, VB2_BUF_STATE_DONE);
		} else {
			csis_ctx_err(ctx, "DMA buffer pointer is not valid\n");
		}

	}

update_prev_counters:
	ctx->prev_frame_counter = ctx->current_frame_counter;
}

static int csis_ip_s_stream(struct v4l2_subdev *sd, int on)
{
	struct csis_dev *dev = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (on) {
		if (!dev->csis_ip_is_on) {
			csis_runtime_get(dev);
			ret = csis_ip_configure(dev);

			if (ret) {
				v4l2_err(&dev->v4l2_dev, "Failed to configure CSIS IP\n");
			}
			dev->csis_ip_is_on = true;
		}
	} else {
		if (dev->csis_ip_is_on) {
			disable_irq(dev->irq);
			csis_runtime_put(dev);
			dev->csis_ip_is_on = false;
		}
	}

	return ret;
}

static int csis_ip_s_power(struct v4l2_subdev *sd, int on)
{
	// TODO:
	return 0;
}

static const struct v4l2_subdev_core_ops csis_ip_core_ops = {
	.s_power = csis_ip_s_power,
};

static const struct v4l2_subdev_video_ops csis_ip_video_ops = {
	.s_stream = csis_ip_s_stream,
};

static const struct v4l2_subdev_ops csis_ip_subdev_ops = {
	.core = &csis_ip_core_ops,
	.video = &csis_ip_video_ops,
};

static int csis_ip_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct csis_dev	*dev = container_of(ctrl->handler, struct csis_dev, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_USER_CSIS_NO_OF_LANE:
		csis_set_num_of_datalane(dev, ctrl->val);
		csis_set_dphy_on(dev, ctrl->val);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops csis_ip_ctrl_ops = {
	.s_ctrl	= csis_ip_s_ctrl,
};

static const struct v4l2_ctrl_config csis_ip_set_nb_lane = {
	.ops	= &csis_ip_ctrl_ops,
	.id	= V4L2_CID_USER_CSIS_NO_OF_LANE,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "CSIS no of lanes",
	.min	= 1,
	.step	= 1,
	.max	= 4,
	.def	= 1,
};

static int csis_ip_power_s_ctrl(struct v4l2_ctrl *ctrl)
{
	// TODO:
	return 0;
}

static const struct v4l2_ctrl_ops csis_ip_power_state_ctrl_ops = {
	.s_ctrl	= csis_ip_power_s_ctrl,
};

static const struct v4l2_ctrl_config csis_ip_power_state_ctrl = {
	.ops	= &csis_ip_power_state_ctrl_ops,
	.id	= V4L2_CID_USER_CSIS_POWER_STATE,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.name	= "CSIS power state: on/of",
	.step	= 1,
	.def	= 0,
};

static int csis_ip_mst_config_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct csis_dev *dev = container_of(ctrl->handler,
						struct csis_dev, ctrl_handler);
	int i;

	for (i = 0; i < CSIS_NUM_CH; i++) {
		if (ctrl->val & (1 << i)) {
			dev->stream_enabled[i] = 1;
		}
	}

	return 0;
}

static const struct v4l2_ctrl_ops csis_ip_mst_config_ctrl_ops = {
	.s_ctrl	= csis_ip_mst_config_s_ctrl,
};

static const struct v4l2_ctrl_config csis_ip_mst_config = {
	.ops	= &csis_ip_mst_config_ctrl_ops,
	.id	= V4L2_CID_USER_CSIS_MST_CONFIG,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "CSIS CH to enable",
	.step	= 1,
	.def	= 0,
	.min	= 0,
	.max	= 255,
};

static int csis_async_complete(struct v4l2_async_notifier *notifier)
{
	struct csis_dev *dev = notifier_to_dev(notifier);
	int ret;

	ret = v4l2_device_register_subdev_nodes(&dev->v4l2_dev);

	if (ret) {
		v4l2_err(&dev->v4l2_dev,
				"Failed to register subdev nodes: %d\n", ret);
	}

	return ret;
}

static int csis_complete_ctx(struct csis_ctx *ctx)
{
	struct video_device *vdev;
	struct vb2_queue *q;
	int ret;

	ctx->timesperframe = csis_tpf_default;

	/* initialize locks */
	spin_lock_init(&ctx->slock);
	mutex_init(&ctx->mutex);
	mutex_init(&ctx->mutex_buf);

	/* initialize vb2_queue */
	q = &ctx->vb_vidq;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ | VB2_USERPTR;
	q->drv_priv = ctx;
	q->buf_struct_size = sizeof(struct csis_buffer);
	q->ops = &csis_video_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &ctx->mutex;
	q->min_buffers_needed = CSIS_NUM_MIN_CH;
	q->dev = &ctx->dev->pdev->dev;
	dma_set_coherent_mask(&ctx->dev->pdev->dev, DMA_BIT_MASK(32));

	ret = vb2_queue_init(q);
	if (ret)
		return ret;

	/* initialize video DMA queue */
	INIT_LIST_HEAD(&ctx->vidq.active);

	vdev = &ctx->vdev;
	*vdev = csis_videodev;
	vdev->v4l2_dev = &ctx->v4l2_dev;
	vdev->queue = q;
	video_set_drvdata(vdev, ctx);

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, video_nr);
	if (ret)
		return ret;

	v4l2_info(&ctx->v4l2_dev, "V4L2 device registered as %s\n",
					video_device_node_name(vdev));

	return ret;
}

static int csis_async_bound(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	struct csis_dev *dev = notifier_to_dev(notifier);
	struct csis_ctx *ctx = NULL;
	const struct csis_fmt *fmt;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_mbus_framefmt mbus_fmt;
	int i, j, k, ret;

	for (i = 0; i < CSIS_NUM_CH; i++) {
		if (dev->asd[i].match.fwnode.fwnode ==
		    of_fwnode_handle(subdev->dev->of_node)) {
			dev->sensor[i] = subdev;
			break;
		}
	}

	if (i == CSIS_NUM_CH) {
		csis_ctx_err(ctx, "No matching sensor node for found!\n");
		return -ENODEV;
	}

	ctx = dev->ctx[i];

	v4l2_set_subdev_hostdata(subdev, ctx);

	v4l2_info(&dev->v4l2_dev, "Hooked sensor subdevice: %s to parent\n", subdev->name);

	/* Enumerate subdevice formates and enable matching csis formats */
	ctx->num_active_fmt = 0;

	for (i = 0, j = 0; ret != -EINVAL; ++j) {
		memset(&mbus_code, 0, sizeof(mbus_code));
		mbus_code.index = j;
		ret = v4l2_subdev_call(subdev, pad, enum_mbus_code,
						NULL, &mbus_code);

		if (ret)
			continue;

		for (k = 0; k < ARRAY_SIZE(csis_formats); k++) {
			fmt = &csis_formats[k];

			if (mbus_code.code == fmt->code) {
				ctx->active_fmt[i] = fmt;
				ctx->num_active_fmt = ++i;
				break;
			}
		}
	}

	if (0 == i) {
		csis_ctx_err(ctx, "No matching format found by subdev %s\n",
					subdev->name);
	}

	ret = csis_complete_ctx(ctx);

	if (ret) {
		csis_ctx_err(ctx, "Failed to register video device for\
			     csis%d-%d\n", dev->id, ctx->virtual_channel);
		return ret;
	}

	ret = __subdev_get_format(ctx, &mbus_fmt);

	if (ret)
		return ret;

	fmt = find_format_by_code(ctx, mbus_fmt.code);

	if (!fmt) {
		csis_ctx_dbg(3, ctx, "mubs code (0x%08X) format not found\n",
			     mbus_fmt.code);
		return -EINVAL;
	}

	/* Save current subdev format */
	v4l2_fill_pix_format(&ctx->v_fmt.fmt.pix, &mbus_fmt);
	ctx->v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ctx->v_fmt.fmt.pix.pixelformat  = fmt->fourcc;
	ctx->v_fmt.fmt.pix.colorspace = fmt->colorspace;
	csis_format_size(ctx, fmt, &ctx->v_fmt);
	ctx->fmt = fmt;
	ctx->m_fmt = mbus_fmt;

	return 0;
}

static struct device_node *
of_get_next_port(const struct device_node *parent,
		struct device_node *prev)
{
	struct device_node *port = NULL;

	if (!parent)
		return NULL;

	if (!prev) {
		struct device_node *ports;
		/*
		 * It's the first csisl, we have to find a port subnode
		 * within this node or within an optional 'ports' node.
		 */
		ports = of_get_child_by_name(parent, "ports");
		if (ports)
			parent = ports;

		port = of_get_child_by_name(parent, "port");

		/* release the 'ports' node */
		of_node_put(ports);
	} else {
		struct device_node *ports;

		ports = of_get_parent(prev);
		if (!ports)
			return NULL;

		do {
			port = of_get_next_child(ports, prev);
			if (!port) {
				of_node_put(ports);
				return NULL;
			}
			prev = port;
		} while (of_node_cmp(port->name, "port") != 0);
	}

	return port;
}

static struct device_node *
of_get_next_endpoint(const struct device_node *parent,
		     struct device_node *prev)
{
	struct device_node *ep = NULL;

	if (!parent)
		return NULL;

	do {
		ep = of_get_next_child(parent, prev);
		if (!ep)
			return NULL;
		prev = ep;
	} while (of_node_cmp(ep->name, "endpoint") != 0);

	return ep;
}

/**
 * Parse the device node for local (csis port) and remote endpoint
 * (sensor node) properties.
 * Fill the sensor node properties into V4L2 endpoint descriptor
 * for later use
**/
static int of_create_csis_instance(struct csis_ctx *ctx, int inst)
{
	struct platform_device *pdev = ctx->dev->pdev;
	struct device_node *parent_node = NULL, *port = NULL, *ep_node = NULL,
				*remote_ep = NULL, *sensor_node = NULL;
	struct v4l2_fwnode_endpoint *endpoint;
	struct v4l2_async_subdev *asd;
	int ret, index, vc_index;
	unsigned int regval = 0x0;
	bool found_port = false;

	parent_node = pdev->dev.of_node;

	endpoint = &ctx->endpoint;

	for (vc_index = 0; vc_index < CSIS_NUM_VC; vc_index++) {
		port = of_get_next_port(parent_node, port);

		if (NULL == port) {
			ret = -ENODEV;
			goto cleanup_exit;
		}

		of_property_read_u32(port, "reg", &regval);

		for (index = 0; index < CSIS_NUM_CH; index++) {
			if ((regval == inst) && (regval == index)) {
				found_port = true;
				break;
			}
		}
	}

	if (!found_port) {
		csis_ctx_dbg(1, ctx, "No port node matches csis port: %d\n", inst);
		ret = -ENODEV;
		goto cleanup_exit;
	}

	ep_node = of_get_next_endpoint(port, ep_node);
	if (NULL == ep_node) {
		csis_ctx_dbg(3, ctx, "Cant get endpoint: %p\n", port);
	}

	sensor_node = of_graph_get_remote_port_parent(ep_node);
	if (NULL == sensor_node) {
		csis_ctx_dbg(3, ctx, "Cant get sensor node %p\n", sensor_node);
		ret = -ENODEV;
		goto cleanup_exit;
	}

	remote_ep = of_parse_phandle(ep_node, "remote-endpoint", 0);
	if (NULL == remote_ep) {
		csis_ctx_dbg(3, ctx, "Cant get remote enpoint %p\n", remote_ep);
		ret = -ENODEV;
		goto cleanup_exit;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(remote_ep),
					 endpoint);
	if (ret) {
		csis_ctx_dbg(3, ctx, "Failed to parse enpoint %p\n", remote_ep);
		ret = -ENODEV;
		goto cleanup_exit;
	}
/*
	if (endpoint->bus_type != V4L2_MBUS_CSI2) {
		csis_ctx_err(ctx, "Port:%d sub-device %s is not a  device\n",
				inst, sensor_node->name);
		goto cleanup_exit;
	}
*/

	/* Store virtual channel id */
	ctx->virtual_channel = index;

	asd = &ctx->dev->asd[ctx->virtual_channel];
	asd->match_type = V4L2_ASYNC_MATCH_FWNODE;
	asd->match.fwnode.fwnode = of_fwnode_handle(sensor_node);

	ctx->dev->asd_list[ctx->dev->notifier.num_subdevs] = asd;
	ctx->dev->notifier.num_subdevs++;

cleanup_exit:

	if (NULL == remote_ep)
		of_node_put(remote_ep);

	if (NULL == sensor_node)
		of_node_put(sensor_node);

	if (NULL == ep_node)
		of_node_put(ep_node);

	if (NULL == port)
		of_node_put(port);

	return ret;
}

static struct csis_ctx *csis_create_instance(struct csis_dev *dev, int inst)
{
	struct csis_ctx *ctx;
	struct v4l2_ctrl_handler *ctrl_handler;
	int ret;

	ctx = devm_kzalloc(&dev->pdev->dev, sizeof(*ctx), GFP_KERNEL);

	if (NULL == ctx)
		return NULL;

	ctx->dev = dev;

	ret = of_create_csis_instance(ctx, inst);

	if (ret) {
		goto free_ctx;
	}

	snprintf(ctx->v4l2_dev.name, sizeof(ctx->v4l2_dev.name),
			"%s%d-%03d", CSIS_MODULE_NAME, dev->id,
			ctx->virtual_channel);

	ret = v4l2_device_register(&dev->pdev->dev, &ctx->v4l2_dev);

	if (ret) {
		csis_ctx_err(ctx, "Unable to register device %s\n",
			     ctx->v4l2_dev.name);
		goto free_ctx;
	}

	csis_ctx_dbg(3, ctx, "Registered device %s\n", ctx->v4l2_dev.name);

	ctrl_handler = &ctx->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_handler, 4);

	if (ret) {
		csis_ctx_err(ctx, "Unable to register ctrl handler\n");
		goto unreg_dev;
	}

	return ctx;

unreg_dev:
	v4l2_device_unregister(&ctx->v4l2_dev);

free_ctx:
	devm_kfree(&dev->pdev->dev, ctx);

	return NULL;
}

static int csis_probe(struct platform_device *pdev)
{
	struct csis_dev *dev;
	struct v4l2_subdev *sd;
	struct v4l2_ctrl_handler *ctrl_handler;
	int i, ret;
	unsigned int irq;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);

	if (NULL == dev)
		return -ENOMEM;

	/* save pdev pointer */
	dev->pdev = pdev;
	dev->id = of_alias_get_id(pdev->dev.of_node, "csis");

	/* Get Register and DMA resources, IRQ */
	dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == dev->res) {
		csis_err(dev, "%s Unable to get Base resource!\n", __func__);
		ret = -ENODEV;
		goto free_dev;
	}
	dev->base = devm_ioremap_resource(&pdev->dev, dev->res);

	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	dev->res_dma = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (NULL == dev->res_dma) {
		csis_err(dev, "%s Unable to get DMA Base resource!\n", __func__);
		ret = -ENODEV;
		goto free_dev;
	}
	dev->dma_base = devm_ioremap_resource(&pdev->dev, dev->res_dma);

	if (IS_ERR(dev->dma_base)) {
		ret = PTR_ERR(dev->dma_base);
		goto free_dev;
	}

	if (csis_pll_init_not_done) {
		dev->res_pll = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (NULL == dev->res_pll) {
			csis_err(dev, "%s Unable to get PLL Base resource!\n", __func__);
			ret = -ENODEV;
			goto free_dev;
		}
		dev->pll_base = devm_ioremap_resource(&pdev->dev, dev->res_pll);

		if (IS_ERR(dev->pll_base)) {
			ret = PTR_ERR(dev->pll_base);
			goto free_dev;
		}

	}

	if (NULL == csis_sys_reg) {
		dev->res_sysreg = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		if (NULL == dev->res_sysreg) {
			csis_err(dev, "%s Unable to get DMA Base resource!\n", __func__);
			ret = -ENODEV;
			goto free_dev;
		}
		dev->sysreg_base = devm_ioremap_resource(&pdev->dev, dev->res_sysreg);
		csis_sys_reg = dev->sysreg_base;
	} else {
		dev->sysreg_base = csis_sys_reg;
	}

	/* set pseudo v4l2 device name for use in printk */
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
			 "%s%d", CSIS_MODULE_NAME, dev->id);

	irq = platform_get_irq(pdev, 0);
	csis_dbg(1, dev, "Requested and got irq %d\n", irq);
	ret = devm_request_irq(&pdev->dev, irq,
		csis_irq_handler, 0, dev->v4l2_dev.name, dev);

	if (ret)
		goto free_dev;

	platform_set_drvdata(pdev, dev);

	csis_set_pll_clock_values(dev);

	mutex_init(&dev->mutex_csis_dma_reg);

	dev->no_of_active_stream = 0;
	dev->sequence = 0;

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);

	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev,
				"Failed register v4l2_device: %d\n", ret);
		goto free_dev;
	}

	sd = &dev->subdev;
	ctrl_handler = sd->ctrl_handler = &dev->ctrl_handler;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_subdev_init(sd, &csis_ip_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "CSIS-IP %d", dev->id);

	/* FIXME: fix registration of subdev operations */
	v4l2_ctrl_handler_init(ctrl_handler, 2);
	v4l2_ctrl_new_custom(ctrl_handler, &csis_ip_set_nb_lane, NULL);
	v4l2_ctrl_new_custom(ctrl_handler, &csis_ip_mst_config, NULL);
	dev->subdev.ctrl_handler = ctrl_handler;

	ret = v4l2_device_register_subdev(&dev->v4l2_dev, sd);

	if (ret < 0) {
		v4l2_err(sd->v4l2_dev,
				"Failed to register subdev: %d\n", ret);
		goto free_dev;
	}

	for (i = 0; i < CSIS_NUM_CH; i++) {

		dev->ctx[i] = csis_create_instance(dev, i);
		dev->stream_enabled[i] = 0;
	}

	dev->notifier.subdevs = dev->asd_list;
	dev->notifier.bound = csis_async_bound;
	dev->notifier.complete = csis_async_complete;

	ret = v4l2_async_notifier_register(&dev->v4l2_dev, &dev->notifier);

	if (ret < 0) {
		csis_err(dev, "Error registering async notifer\n");
		ret = -EINVAL;
		goto unregister_device;
	}

	dev->csis_ip_is_on = false;
	pm_runtime_enable(&pdev->dev);
	ret = csis_runtime_get(dev);

	if (ret)
		goto runtime_disable;

	csis_runtime_put(dev);

	v4l2_set_subdevdata(sd, dev);
	csis_ip_s_stream(sd, 1);

	return 0;

runtime_disable:
	pm_runtime_disable(&pdev->dev);

unregister_device:
	v4l2_async_notifier_unregister(&dev->notifier);
	v4l2_device_unregister_subdev(sd);

free_dev:
	devm_kfree(&pdev->dev, dev);

	return ret;
}

static int csis_remove(struct platform_device *pdev)
{
	struct csis_dev *dev =
		(struct csis_dev *)platform_get_drvdata(pdev);
	struct csis_ctx *ctx;
	int i;

	csis_dbg(1, dev, "Removing %s\n", CSIS_MODULE_NAME);

	csis_disable_vid_irqs(dev);
	csis_disable(dev);
	csis_runtime_get(dev);

	v4l2_async_notifier_unregister(&dev->notifier);

	for (i = 0; i < CSIS_NUM_CH; i++) {
		ctx = dev->ctx[i];
		if (ctx) {
			csis_ctx_dbg(1, ctx, "unregistering %s\n",
				video_device_node_name(&ctx->vdev));
			/* Right now this is not required */
			/* csis_dphy_disable(ctx); */
			v4l2_ctrl_handler_free(&ctx->ctrl_handler);
			video_unregister_device(&ctx->vdev);
		}
	}

	v4l2_device_unregister(&dev->v4l2_dev);

	csis_runtime_put(dev);
	pm_runtime_disable(&pdev->dev);
	csis_pll_init_not_done = true;
	csis_sys_reg = NULL;

	return 0;
}

static void csis_shutdown(struct platform_device *pdev)
{
	struct csis_dev *dev =
		(struct csis_dev *)platform_get_drvdata(pdev);

	csis_disable_vid_irqs(dev);
	csis_disable(dev);
}

#ifdef CONFIG_PM
static int csis_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct csis_dev *dev = (struct csis_dev *)platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &dev->subdev;

	if (dev->pll_base != NULL)
		csis_pll_init_not_done = true;
	ret = csis_ip_s_stream(sd, 0);

	return ret;
}

static int csis_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct csis_dev *dev = (struct csis_dev *)platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &dev->subdev;

	if (dev->pll_base != NULL)
		csis_set_pll_clock_values(dev);
	ret = csis_ip_s_stream(sd, 1);

	return ret;
}
#endif

#if	defined(CONFIG_OF)
static struct of_device_id csis_of_match[] = {
	{.compatible = "turbo,trav-csis", },
	{},
};
MODULE_DEVICE_TABLE(of, csis_of_match);
#endif

static struct platform_driver csis_pdrv = {
	.probe		= csis_probe,
	.remove		= csis_remove,
	.shutdown	= csis_shutdown,
#ifdef CONFIG_PM
	.suspend        = csis_suspend,
	.resume         = csis_resume,
#endif
	.driver		= {
		.name	= CSIS_MODULE_NAME,
		.of_match_table	= of_match_ptr(csis_of_match),
	},
};

module_platform_driver(csis_pdrv);
