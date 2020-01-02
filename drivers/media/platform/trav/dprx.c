/*
 * TRAV DPRX camera interface driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

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
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "dprx-regs.h"
#include "dprx-dma-regs.h"

#define DPRX_MODULE_NAME "dprx"

#define MAX_WIDTH 1920
#define MAX_HEIGHT 1200

#define DPRX_VERSION "0.1.0"
#define AUX_WAIT_TIME 100
#define PAYLOAD_TBL_SZ 63

#define DPRX_NUM_CLOCK 16

MODULE_DESCRIPTION("TRAV DPRX driver");
MODULE_AUTHOR("Ajay Kumar, <ajaykumar.rs@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DPRX_VERSION);

static unsigned int video_nr = 20;
module_param(video_nr, uint, 0644);
MODULE_PARM_DESC(video_nr, "videoX start number, -1 is autodetect");

static unsigned int debug;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

#define dprx_dbg(level, dprxdev, fmt, arg...)	\
		v4l2_dbg(level, debug, &dprxdev->v4l2_dev, fmt, ##arg)
#define dprx_info(dprxdev, fmt, arg...)	\
		v4l2_info(&dprxdev->v4l2_dev, fmt, ##arg)
#define dprx_err(dprxdev, fmt, arg...)	\
		v4l2_err(&dprxdev->v4l2_dev, fmt, ##arg)

#define ch_dbg(level, ch, fmt, arg...)	\
		v4l2_dbg(level, debug, &ch->v4l2_dev, fmt, ##arg)
#define ch_info(ch, fmt, arg...)	\
		v4l2_info(&ch->v4l2_dev, fmt, ##arg)
#define ch_err(ch, fmt, arg...)	\
		v4l2_err(&ch->v4l2_dev, fmt, ##arg)

#define bytes_per_line(width, bpp) (ALIGN(((width * bpp) >> 3), 16))

#define V4L2_CID_USER_DPRX_NO_OF_LANE	(V4L2_CID_USER_TRAV_DPRX_BASE + 0)
#define V4L2_CID_USER_DPRX_LANE_SPEED	(V4L2_CID_USER_TRAV_DPRX_BASE + 1)
#define V4L2_CID_USER_DPRX_TPS_PATTERN	(V4L2_CID_USER_TRAV_DPRX_BASE + 2)
#define V4L2_CID_USER_DPRX_ENABLE_HDCP	(V4L2_CID_USER_TRAV_DPRX_BASE + 3)
#define V4L2_CID_USER_DPRX_ENABLE_DSC	(V4L2_CID_USER_TRAV_DPRX_BASE + 4)
#define V4L2_CID_USER_DPRX_MST_CONFIG	(V4L2_CID_USER_TRAV_DPRX_BASE + 5)
#define V4L2_CID_USER_DPRX_POWER_STATE	(V4L2_CID_USER_TRAV_DPRX_BASE + 6)

struct dpcd {
	__u32 operation;
	__u32 dpcd_addr;
	__u32 dpcd_length;
	__u8 dpcd_val[16];
};

struct dp_crc {
	__u32 crc[8];
};

struct dp_payload_id {
	__u8 payload_id[DPRX_NUM_MST];
	__u8 xval[DPRX_NUM_MST];
	__u8 yval[DPRX_NUM_MST];
	__u8 payload_id_table[PAYLOAD_TBL_SZ];
};

#define VIDIOC_SUBDEV_PRIV_DPCD_ACCESS		_IOWR('V', 101, struct dpcd)
#define VIDIOC_SUBDEV_PRIV_DPRX_CRC		_IOWR('V', 102, struct dp_crc)
#define VIDIOC_SUBDEV_PRIV_SET_PAYLOAD_ID	_IOWR('V', 103, struct dp_payload_id)
/* ------------------------------------------------------------------
 *	Basic structures
 * ------------------------------------------------------------------
 */

struct dprx_fmt {
	u32	fourcc;
	u32	colorspace;
	u32	code;
	u8	depth;
	u8	packed;
};

static struct dprx_fmt dprx_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_XRGB32,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB888_1X32_PADHI,
		.depth		= 32,
	}, {
		.fourcc		= V4L2_PIX_FMT_XRGB30,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB30_1X32,
		.depth		= 32,
	}, {
		.fourcc		= V4L2_PIX_FMT_XRGB36,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.code		= MEDIA_BUS_FMT_RGB36_1X64,
		.depth		= 64,
	}, {
		.fourcc		= V4L2_PIX_FMT_SBGGR8,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SBGGR8_1X8,
		.depth		= 8,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGBRG8,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGBRG8_1X8,
		.depth		= 8,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGRBG8,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGRBG8_1X8,
		.depth		= 8,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SRGGB8,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SRGGB8_1X8,
		.depth		= 8,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SBGGR10,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
		.depth		= 10,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGBRG10,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGBRG10_1X10,
		.depth		= 10,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGRBG10,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGRBG10_1X10,
		.depth		= 10,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SRGGB10,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SRGGB10_1X10,
		.depth		= 10,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SBGGR12,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SBGGR12_1X12,
		.depth		= 12,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGBRG12,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGBRG12_1X12,
		.depth		= 12,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGRBG12,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGRBG12_1X12,
		.depth		= 12,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SRGGB12,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SRGGB12_1X12,
		.depth		= 12,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SBGGR14,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SBGGR14_1X14,
		.depth		= 14,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGBRG14,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGBRG14_1X14,
		.depth		= 14,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGRBG14,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGRBG14_1X14,
		.depth		= 14,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SRGGB14,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SRGGB14_1X14,
		.depth		= 14,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SBGGR16,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SBGGR16_1X16,
		.depth		= 16,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGBRG16,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGBRG16_1X16,
		.depth		= 16,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SGRBG16,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SGRBG16_1X16,
		.depth		= 16,
		.packed		= 1,
	}, {
		.fourcc		= V4L2_PIX_FMT_SRGGB16,
		.colorspace	= V4L2_COLORSPACE_RAW,
		.code		= MEDIA_BUS_FMT_SRGGB16_1X16,
		.depth		= 16,
		.packed		= 1,
	},
};

/*  Print Four-character-code (FOURCC) */
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

/* buffer for one video frame */
struct dprx_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;
	const struct dprx_fmt	*fmt;
	unsigned long		sequence_num;
};

struct dprx_dmaqueue {
	struct list_head	active;
};

/*
 * dprx driver structure.
 */
struct dprx_dev {
	int				id;
	struct resource			*res_link;
	struct resource			*res_dma;
	struct platform_device		*pdev;
	void __iomem			*base;
	void __iomem			*dma_base;
	int				link_irq;
	int				dma_irq;

	struct v4l2_device		v4l2_dev;
	struct v4l2_ctrl_handler	ctrl_handler;

	/* subdev for overall IP control */
	struct v4l2_subdev		subdev;
	/* subdev pointer for each sensor control */
	struct v4l2_subdev		*sensor[DPRX_NUM_MST];

	struct v4l2_async_notifier	notifier;
	struct v4l2_async_subdev	asd[DPRX_NUM_MST];
	struct v4l2_async_subdev	*asd_list[DPRX_NUM_MST];

	struct mutex			mutex_dprx_dma_reg;

	struct dprx_channel		*ch[DPRX_NUM_MST];

	spinlock_t			dma_en_vid_lock;

	struct clk			*dp_clk[DPRX_NUM_CLOCK];
	unsigned long			sequence;
	int				dprx_num_clock;
	int				stream_enabled[8];
	int				payload_id[DPRX_NUM_MST];
	int				xval[DPRX_NUM_MST];
	int				yval[DPRX_NUM_MST];
	int				payload_id_table[PAYLOAD_TBL_SZ];
	int				lane_count;
	int				ls_rate, ls_sel;

	link_rate			link_rate;
	tps_pattern			tps_pattern;

	bool				dprx_ip_is_on;
	bool				enhance_mode;
	bool				mst_en;
	bool				fec_en;
	bool				dsc_en;
	bool				hdcp_en;
};

typedef enum {
	DMA_OFF,
	DMA_ON,
	DMA_OFF_BY_INTR,
} DMA_STATE;

/*
 * There is one dprx_channel structure for each stream.
 */
struct dprx_channel {
	struct v4l2_device		v4l2_dev;
	struct video_device		vdev;
	struct v4l2_fwnode_endpoint	endpoint;
	struct v4l2_fh			fh;
	struct vb2_queue		vb_vidq;

	struct dprx_dev			*dev;
	/* Used to store current pixel format */
	struct v4l2_format		v_fmt;
	/* Used to store current mbus frame format */
	struct v4l2_mbus_framefmt	m_fmt;

	/* local queue to store queued buffers */
	struct dprx_dmaqueue		vidq;

	/* video capture */
	const struct dprx_fmt		*fmt;

	/* Current subdev enumerated format */
	struct dprx_fmt			*active_fmt[ARRAY_SIZE(dprx_formats)];

	/* v4l2_ioctl mutex */
	struct mutex			mutex;

	spinlock_t			slock;

	struct dprx_buffer		*frame[DPRX_NUM_FRAME_ADDR];
	unsigned long			frame_paddr[DPRX_NUM_FRAME_ADDR];
	unsigned long			frame_vaddr[DPRX_NUM_FRAME_ADDR];

	unsigned int			seq_count;
	unsigned int			virtual_channel;
	int				num_active_fmt;
	int				ring_add_ptr;
	int				ring_del_ptr;
	int				runtime_ring_cnt;
	u32				crc;

	atomic_t			end_irq_worker;
	atomic_t			dma_on;
	int				irq_counter;
	int				buf_counter;
};

static inline void get_link_rate(struct dprx_dev *dev)
{
	switch (dev->link_rate) {
	case LS_810:
		dev->ls_sel = 3;
		dev->ls_rate = 30;
		break;
	case LS_540:
		dev->ls_sel = 2;
		dev->ls_rate = 20;
		break;
	case LS_270:
		dev->ls_sel = 1;
		dev->ls_rate = 10;
		break;
	case LS_162:
		dev->ls_sel = 0;
		dev->ls_rate = 6;
		break;
	}
}

void dprx_rst_aux(struct dprx_dev *dev)
{
	u32 val;

	val = dprx_cfg_read(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_RST_CTRL_1);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_RST_CTRL_1,
						val | DPRX_RST_AUX_CH_RESET);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_RST_CTRL_1,
						val & (~DPRX_RST_AUX_CH_RESET));
}

bool dprx_wait_aux_finished(struct dprx_dev *dev, int time)
{
	u32 val;
	u8 cnt = 0;

	val = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_REG_AUX_CTRL_1);
	while (val & DPRX_AUX_OP_EN) {
		mdelay(time);
		cnt++;
		if (cnt == 100) {
			dprx_rst_aux(dev);
			return false;
		}
		val = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_REG_AUX_CTRL_1);
	}

	/* AUX transaction completed. Check if transaction was successful */
	val = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_AUX_STATUS_0);
	if (!val) {
		return true;
	} else {
		dprx_err(dev, "AUX transaction error status:%d\n", val);
		return false;
	}

	return true;
}

bool dprx_dpcd_read(struct dprx_dev *dev, u32 uDPCDaddr, u8 uCount, u8 *pDPCDbuf)
{
	u32 reg;
	u8 i;

	printk("dprx_dpcd_read: addr: %x len: %d\n", uDPCDaddr, uCount);

	/* clear AUX buffer */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_BUF_CLR,
							DPRX_AUX_BUF_CLR);

	/* set read count & read command*/
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_CMD_LEN,
					(((uCount - 1) & 0x0F) << 4) | 0x09);

	/* set AUX address 15:0 */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_0,
							uDPCDaddr & 0xff);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_1,
						(uDPCDaddr >> 8) & 0xff);

	/* set address 19:16 */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_2,
						((uDPCDaddr >> 12) & 0x0f));

	/* Enable AUX */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_CTRL_1,
							DPRX_AUX_OP_EN);

	if (dprx_wait_aux_finished(dev, AUX_WAIT_TIME) == false)
		return false;

	printk("data read:\n");
	for (i = 0; i < uCount; i++) {
		reg = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_REG_AUX_BUFFER(i));
		pDPCDbuf[i] = (u8)reg;
		printk("0x%x\n", pDPCDbuf[i]);
	}

	return true;
}

bool dprx_dpcd_write(struct dprx_dev *dev, u32 uDPCDaddr, u8 uCount, u8 *pDPCDbuf)
{
	u8 i;

	printk("dprx_dpcd_write: addr: %x len: %d\n", uDPCDaddr, uCount);
	/* clear AUX buffer */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_BUF_CLR,
							DPRX_AUX_BUF_CLR);

	printk("data to write:\n");
	/* write data to AUX buffer */
	for (i = 0; i < uCount; i++) {
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
			DPRX_ADDR_REG_AUX_BUFFER(i), pDPCDbuf[i]);
		printk("0x%x\n", pDPCDbuf[i]);
	}

	/* set write count & write command*/
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_CMD_LEN,
					(((uCount - 1) & 0x0F) << 4) | 0x08);

	/* set AUX address 15:0 */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_0,
							uDPCDaddr & 0xff);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_1,
						(uDPCDaddr >> 8) & 0xff);

	/* set AUX address 19:16 */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_ADDR_2,
						((uDPCDaddr >> 12) & 0x0f));

	/* Enable Aux */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_REG_AUX_CTRL_1,
								DPRX_AUX_OP_EN);

	return dprx_wait_aux_finished(dev, AUX_WAIT_TIME);

}

bool dprx_link_training_process(struct dprx_dev *dev)
{
	u32 val, i;
	u8 cr_lock, cr_lock2, sym_lock;
	u8 aux_buf[1] = {0};
	u8 lane0_1_status;
	u8 lane2_3_status;
	u8 lane_align_status;
	u8 vol_sw_lane0, vol_sw_lane1, vol_sw_lane2, vol_sw_lane3;
	u8 pre_em_lane0, pre_em_lane1, pre_em_lane2, pre_em_lane3;

#if 0
	/* Check HPD Status, LT valid once HPD state is High */
	val = dprx_cfg_read(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_DP_HPD_DEBUG);
	if (!(val & DPRX_HOT_PLUG_DETECT))
		return false;
#endif
	/* Wait for DPTX to be ready to receive Training commands */
	do {
		if (!dprx_dpcd_read(dev, DPCD_ADDR_VENDOR_SPECIFIC, 1, aux_buf))
			dprx_err(dev, "AUX DPCD read failure\n");
	} while (aux_buf[0] != 1);

	/* Select DP_PLL_SPEED & Link Bandwidth */
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
				DPRX_ADDR_SERDES_CTRL1, dev->ls_sel);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_LINK_BW_SET, dev->ls_rate);
	/* Set Link Bandwidth at DPTX side */
	aux_buf[0] = dev->ls_rate;
	dprx_dpcd_write(dev, DPCD_ADDR_LINK_BW_SET, 1, aux_buf);

	/* Set Lane Count and Enhanced Frame at DPRX */
	if (dev->lane_count == 1)
		val = DPRX_ENHANCED_FRAME_EN | DPRX_LANE_COUNT_SET(1);
	else if (dev->lane_count == 2)
		val = DPRX_ENHANCED_FRAME_EN | DPRX_LANE_COUNT_SET(2);
	else /* if (dev->lane_count == 4) */
		val = DPRX_ENHANCED_FRAME_EN | DPRX_LANE_COUNT_SET(4);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_LANE_COUNT_SET, val);

	/* Set Lane Count and Enhanced Frame at DPTX */
	aux_buf[0] = val;
	dprx_dpcd_write(dev, DPCD_ADDR_LANE_COUNT_SET, 1, aux_buf);

	/* Link Training Start */
	/* Write the DPCD addr 0x102 dptx and set dptx to send the CR/TPS1
	   pattern. SCRAMBLING disabled during TPS1 */
	aux_buf[0] = DPRX_SCRAMBLING_DISABLE | LT_PATTERN_1;
	dprx_dpcd_write(dev, DPCD_ADDR_TRAINING_PATTERN_SET, 1, aux_buf);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_TRAINING_PATTERN_SET,
				DPRX_SCRAMBLING_DISABLE | LT_PATTERN_1);

	cr_lock = 0;
	vol_sw_lane0 = 0;
	vol_sw_lane1 = 0;
	vol_sw_lane2 = 0;
	vol_sw_lane3 = 0;

	pre_em_lane0 = 0;
	pre_em_lane1 = 0;
	pre_em_lane2 = 0;
	pre_em_lane3 = 0;

	for (i = 0; i < 5; i++) {
		mdelay(1);
		lane0_1_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE0_1_STATUS);
		lane2_3_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE2_3_STATUS);
		lane_align_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE_ALIGN_STATUS);

		if (dev->lane_count == 1) {
			cr_lock = (lane0_1_status & DPRX_LANE0_CR_DONE);
		} else if (dev->lane_count == 2) {
			cr_lock = (lane0_1_status & (DPRX_LANE0_CR_DONE |
						     DPRX_LANE1_CR_DONE));
		} else if (dev->lane_count == 4) {
			cr_lock = (lane0_1_status & (DPRX_LANE0_CR_DONE |
						     DPRX_LANE1_CR_DONE)) &&
				  (lane2_3_status & (DPRX_LANE2_CR_DONE |
						     DPRX_LANE3_CR_DONE));
		}

		if (cr_lock)
			break;
		else {
		/* Otherwise, Adjust Pre-emp and Drive Current and repeat */
		}
	}

	if (!cr_lock) {
		dprx_err(dev, "Link training:Clock Recovery fail\n");
		return false;
	}

	dprx_info(dev, "Link training:Clock Recovery successful\n");

	cr_lock2 = 0;
	sym_lock = 0;

	/* Check EQ status */

	switch (dev->ls_rate) {
	case 6:
		val = DPRX_SCRAMBLING_DISABLE | LT_PATTERN_2;
		break;
	case 10:
		val = DPRX_SCRAMBLING_DISABLE | LT_PATTERN_2;
		break;
	case 20:
		val = DPRX_SCRAMBLING_DISABLE | LT_PATTERN_3;
		break;
	case 30:
		/* SCRAMBLING needed only for during TPS4 */
		val = LT_PATTERN_4;
		break;
	}
	/* Write the DPCD addr 0x102 to dpcd in dptx and set dptx to send the EQ
	   pattern */
	aux_buf[0] = val;
	dprx_dpcd_write(dev, DPCD_ADDR_TRAINING_PATTERN_SET, 1, aux_buf);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_TRAINING_PATTERN_SET, val);

	for (i = 0; i < 5; i++) {
		mdelay(1);

		lane0_1_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE0_1_STATUS);
		lane2_3_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE2_3_STATUS);
		lane_align_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE_ALIGN_STATUS);

		if (dev->lane_count == 1) {
			cr_lock2 = (lane0_1_status & DPRX_LANE0_CR_DONE);
		} else if (dev->lane_count == 2) {
			cr_lock2 = (lane0_1_status & (DPRX_LANE0_CR_DONE |
						      DPRX_LANE1_CR_DONE));
		} else if (dev->lane_count == 4) {
			cr_lock2 = (lane0_1_status & (DPRX_LANE0_CR_DONE |
						      DPRX_LANE1_CR_DONE)) &&
				   (lane2_3_status & (DPRX_LANE3_CR_DONE |
						      DPRX_LANE2_CR_DONE));
		}

		if (!(cr_lock2 > 0))
			break;

		if (dev->lane_count == 1) {
			if ((lane0_1_status & (DPRX_LANE0_CR_DONE |
					       DPRX_LANE0_CHANNEL_EQ_DONE |
					       DPRX_LANE0_SYMBOL_LOCKED)) &&
				(lane_align_status & DPRX_INTERLANE_ALIGN_DONE))
				sym_lock = 1;
		} else if (dev->lane_count == 2) {
			if ((lane0_1_status & (DPRX_LANE0_CR_DONE |
					       DPRX_LANE0_CHANNEL_EQ_DONE |
					       DPRX_LANE0_SYMBOL_LOCKED |
					       DPRX_LANE1_CR_DONE |
					       DPRX_LANE1_CHANNEL_EQ_DONE |
					       DPRX_LANE1_SYMBOL_LOCKED)) &&
				(lane_align_status & DPRX_INTERLANE_ALIGN_DONE))
				sym_lock = 1;
		} else if (dev->lane_count == 4) {
			if ((lane0_1_status & (DPRX_LANE1_CR_DONE |
					       DPRX_LANE1_CHANNEL_EQ_DONE |
					       DPRX_LANE1_SYMBOL_LOCKED |
					       DPRX_LANE0_CR_DONE |
					       DPRX_LANE0_CHANNEL_EQ_DONE |
					       DPRX_LANE0_SYMBOL_LOCKED)) &&
			    (lane2_3_status & (DPRX_LANE3_CR_DONE |
					       DPRX_LANE3_CHANNEL_EQ_DONE |
					       DPRX_LANE3_SYMBOL_LOCKED |
					       DPRX_LANE2_CR_DONE |
					       DPRX_LANE2_CHANNEL_EQ_DONE |
					       DPRX_LANE2_SYMBOL_LOCKED)) &&
				(lane_align_status & DPRX_INTERLANE_ALIGN_DONE))
				sym_lock = 1;
		}

		if (sym_lock)
			break;
		else {
		/* Otherwise, Adjust Pre-emp and Drive Current and repeat */
		}
	}

	/* Disable Training Pattern at DPRX */
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_TRAINING_PATTERN_SET, 0);
	/* Disable Training Pattern at DPTX */
	aux_buf[0] = 0;
	dprx_dpcd_write(dev, DPCD_ADDR_TRAINING_PATTERN_SET, 1, aux_buf);

	if (sym_lock) {
		dprx_info(dev, "Link training:EQ and Symbol lock successful\n");
		return true;
	} else {
		dprx_err(dev, "Link training:EQ and Symbol fail\n");
		return false;
	}
}

int dprx_training(struct dprx_dev *dev, int training_en)
{
	u8 lane0_1_status;
	u8 lane2_3_status;
	int ok = true;

#ifdef AUX_CH_PRESENT
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG,
				DPRX_ADDR_DP_HPD_DEGLITCH_L, 0xCF);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG,
				DPRX_ADDR_DP_HPD_DEGLITCH_H, 0x03);

	/* Check HPD */
	while (1) {
		tmp = dprx_cfg_read(dev,
				SLAVE_ID_TOP_REG, DPRX_ADDR_DP_HPD_DEBUG);
		if (tmp & DPRX_HOT_PLUG_DETECT)
			break;
		udelay(10);
	}
#endif

	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
				DPRX_ADDR_SERDES_CTRL1, dev->ls_sel);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_LINK_BW_SET, dev->ls_rate);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_LANE_COUNT_SET,
				(DPRX_ENHANCED_FRAME_EN) |
				DPRX_LANE_COUNT_SET(dev->lane_count));

	if (!training_en) {
		lane0_1_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE0_1_STATUS);
		lane2_3_status = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE2_3_STATUS);
		dprx_info(dev, "lane0_1_status: %x lane2_3_status: %x\n",
						lane0_1_status, lane2_3_status);
		return ok;
	} else {
		dprx_info(dev, "Doing Dprx Link Training........\n");
		ok = dprx_link_training_process(dev);
	}
	return ok;
}

static void config_x_y_per_stream(struct dprx_dev *dev, int x, int y, int n)
{
	u8 aux_buf[1];

	aux_buf[0] = x;
	dprx_dpcd_write(dev, DPCD_ADDR_RATE_GOVERNING_X(n), 1, aux_buf);
	aux_buf[0] = y;
	dprx_dpcd_write(dev, DPCD_ADDR_RATE_GOVERNING_Y(n), 1, aux_buf);
}

static void config_tx_slot_num(struct dprx_dev *dev)
{
	int i, id, start, length, val;
	u8 aux_buf[1];

	start = 1;
	length = 0;
	id = dev->payload_id_table[0];

	i = 0;
	while (i < 63) {
		val = dev->payload_id_table[i];
		if (id != val) {
			aux_buf[0] = id;
			dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_ALLOCATE_SET, 1, aux_buf);
			aux_buf[0] = start;
			dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_START_TIME_SLOT, 1, aux_buf);
			aux_buf[0] = length;
			dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_TIME_SLOT_COUNT, 1, aux_buf);
			dprx_dpcd_read(dev, DPCD_ADDR_PAYLOAD_TABLE_UPDATE_STATUS, 1, aux_buf);
			start = i + 1;
			length = 0;
			id = val;
		}
		length++;
		i++;
	}
}

static void config_rx_slot_num(struct dprx_dev *dev)
{
	int i;

	/* Assign payload id number for each stream */
	for (i = 0; i < DPRX_NUM_MST; i++) {
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_STRM_PL_ID(i), dev->payload_id[i]);
		dprx_info(dev, "%s: %x\n", __func__, dprx_cfg_read(dev,
				SLAVE_ID_CONTROL_REG, DPRX_ADDR_STRM_PL_ID(i)));
	}

	/* Update time slot count per paylaod id */
	for (i = 0; i < PAYLOAD_TBL_SZ; i++) {
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_MST_PL_ID(i), dev->payload_id_table[i]);
		dprx_info(dev, "%s: %x\n", __func__, dprx_cfg_read(dev,
				SLAVE_ID_CONTROL_REG, DPRX_ADDR_MST_PL_ID(i)));
	}
}

static void dprx_config(struct dprx_dev *dev, int mst_enable)
{
	int i;
	u32 val = 0;
	u8 aux_buf[16];

	if (mst_enable) {
		printk("Clear old VC payload table from driver\n");
		aux_buf[0] = 0x0;
		dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_ALLOCATE_SET, 1, aux_buf);
		aux_buf[0] = 0x0;
		dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_START_TIME_SLOT, 1, aux_buf);
		aux_buf[0] = 0x3f;
		dprx_dpcd_write(dev, DPCD_ADDR_PAYLOAD_TIME_SLOT_COUNT, 1, aux_buf);

		printk("Read back to check if clear is fine\n");
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(0), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(16), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(32), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(48), 15, aux_buf);

		config_tx_slot_num(dev);

		for (i = 0; i < 8; i++)
			config_x_y_per_stream(dev, dev->xval[i], dev->yval[i], i);

		printk("After setting VC payload table from driver\n");
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(0), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(16), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(32), 16, aux_buf);
		dprx_dpcd_read(dev, DPCD_ADDR_VC_PAYLOAD_ID(48), 15, aux_buf);

		/* Turn off ENHANCED mode for MST */
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_LANE_COUNT_SET,
				DPRX_LANE_COUNT_SET(dev->lane_count));

		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_MSTM_CTRL, DPRX_MST_EN);
		config_rx_slot_num(dev);
	}

	for (i = 0; i < DPRX_NUM_MST; i++)
		val = val | (dev->stream_enabled[i] << i);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_STRM_SEL, val);
}

static void dprx_ip_interrupt_init(struct dprx_dev *dev)
{
	u32 enabled_streams = 0, val, i;
	u8 slave_id[8] = {
				SLAVE_ID_MAINLINK_REG_0,
				SLAVE_ID_MAINLINK_REG_1,
				SLAVE_ID_MAINLINK_REG_2,
				SLAVE_ID_MAINLINK_REG_3,
				SLAVE_ID_MAINLINK_REG_4,
				SLAVE_ID_MAINLINK_REG_5,
				SLAVE_ID_MAINLINK_REG_6,
				SLAVE_ID_MAINLINK_REG_7,
			};

	for (i = 0; i < DPRX_NUM_MST; i++)
		enabled_streams = enabled_streams | (dev->stream_enabled[i] << i);

	/* Enable interrupts for each enabled stream */
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_INTR_1_MASK, ~(enabled_streams));

	/* Clear residual interrupts if any from boot stage for DPRX MST */
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_INTR_1, enabled_streams);

	for (i = 0; i < DPRX_NUM_MST; i++) {
		if (dev->stream_enabled[i]) {
			/* Unmask FRAME ERROR detection interrupt */
			val = ~(DPRX_VID_FRAME_ERR_INTR);
			dprx_cfg_write(dev, slave_id[i],
					DPRX_ADDR_MAINLINK_INTR1_MASK, val);

			/* Clear any pending FRAME ERROR detection interrupt */
			dprx_cfg_write(dev, slave_id[i],
			DPRX_ADDR_MAINLINK_INTR1, DPRX_VID_FRAME_ERR_INTR);
		}
	}
}

static int dprx_ip_configure(struct dprx_dev *dev)
{
	int ret = 0;
	u32 rdata, val;

	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_RST_CTRL_0,
				DPRX_RST_SW_RESET | DPRX_RST_LOGIC_RESET);

	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_RST_SEL, 0);

#ifdef EMULATOR
	/* To take DPRX LINK FSM to post link training state */
	val = dprx_cfg_read(dev, SLAVE_ID_SERDES_REG,
						DPRX_ADDR_DP_CHn_CTRL0(0));
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
			DPRX_ADDR_DP_CHn_CTRL0(0), val | (DPRX_DP_CH_LANE_OK));
	val = dprx_cfg_read(dev, SLAVE_ID_SERDES_REG,
						DPRX_ADDR_DP_CHn_CTRL0(1));
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
			DPRX_ADDR_DP_CHn_CTRL0(1), val | (DPRX_DP_CH_LANE_OK));
	val = dprx_cfg_read(dev, SLAVE_ID_SERDES_REG,
						DPRX_ADDR_DP_CHn_CTRL0(2));
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
			DPRX_ADDR_DP_CHn_CTRL0(2), val | (DPRX_DP_CH_LANE_OK));
	val = dprx_cfg_read(dev, SLAVE_ID_SERDES_REG,
						DPRX_ADDR_DP_CHn_CTRL0(3));
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG,
			DPRX_ADDR_DP_CHn_CTRL0(3), val | (DPRX_DP_CH_LANE_OK));
	dprx_cfg_write(dev, SLAVE_ID_SERDES_REG, DPRX_ADDR_SERDES_CTRL0,
			DPRX_RESET_STATE | DPRX_BYPASS_PLL_LOCK |
			DPRX_BYPASS_SIGNAL_DET | DPRX_BYPASS_STATE);
#endif

	dprx_ip_interrupt_init(dev);

	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_MISC_CTRL, 0x3);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_MISC_CTRL, 0x2);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_MISC_CTRL, 0x1);
	dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_MISC_CTRL, 0x0);

#ifdef EMULATOR
	/* DP XTOR doesn't send data format bpc properly. Explicitly over riding
	 * incoming MISC0 and MISC 1 packet from incoming packets, and choose
	 * from DPRX IP SFR instead.
	 */
	dprx_cfg_write(dev, SLAVE_ID_MAINLINK_REG_0,
						DPRX_ADDR_CFG_MISC0, 0x20);
	dprx_cfg_write(dev, SLAVE_ID_MAINLINK_REG_0, DPRX_ADDR_CFG_MISC1, 0x0);
	dprx_cfg_write(dev, SLAVE_ID_MAINLINK_REG_0,
						DPRX_ADDR_VID_PARA_SEL, 0x3);
#endif

	if (dev->lane_count == 4)
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_LANE_USE_AS, 0xE4);

#ifdef EMULATOR
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_RCD_PN_CONVERTE, 0x10);
#endif

	rdata = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_LANE_ALIGN_STATUS);

	rdata = dprx_cfg_read(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_LANE_COUNT_SET);
	rdata = rdata & ~(0x1f << 0);

	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
			DPRX_ADDR_LANE_COUNT_SET, rdata | dev->lane_count);

	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_EDP_CONFIG_SET, 0x0);
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, 0x270, 0x0);

	if (dev->dsc_en)
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_DSC_ENABLE, 0xF);

	if (dev->fec_en) {
		val = DPRX_FEC_LANE_SELECT_0 |
			DPRX_CORRECTED_BLOCK_ERROR_COUNT | DPRX_FEC_READY;
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
						DPRX_ADDR_FEC_CONFIG_0, val);
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
				DPRX_ADDR_FEC_CONFIG_1, DPRX_STOP_CIPHER_EN);
	}

	get_link_rate(dev);
	ret = dprx_training(dev, 1);
	if (!ret) {
		dprx_err(dev, "Link training failed\n");
		return -EAGAIN;
	}

	dprx_config(dev, dev->mst_en);

	return 0;
}

static void dprx_ip_off(struct dprx_dev *dev)
{
	dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG, DPRX_ADDR_STRM_SEL, 0);

	if (dev->mst_en)
		dprx_cfg_write(dev, SLAVE_ID_CONTROL_REG,
					DPRX_ADDR_MSTM_CTRL, 0);

}

static const struct dprx_fmt *find_format_by_pix(struct dprx_channel *ch,
						u32 pixelformat)
{
	const struct dprx_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ch->num_active_fmt; k++) {
		fmt = ch->active_fmt[k];
		if (fmt->fourcc == pixelformat)
			return fmt;
	}

	return NULL;
}

static struct dprx_fmt *find_format_by_code(struct dprx_channel *ch,
						 u32 code)
{
	struct dprx_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ch->num_active_fmt; k++) {
		fmt = ch->active_fmt[k];
		if (fmt->code == code)
			return fmt;
	}

	return NULL;
}

static inline struct dprx_dev *notifier_to_dev(struct v4l2_async_notifier *n)
{
	return container_of(n, struct dprx_dev, notifier);
}

/*
 * Load HDCP firmware and key
 */
static void dprx_load_hdcp_firmware(struct dprx_dev *dev)
{
}

static inline int dprx_runtime_get(struct dprx_dev *dev)
{
	int r;

	r = pm_runtime_get_sync(&dev->pdev->dev);

	return r;
}

static inline void dprx_runtime_put(struct dprx_dev *dev)
{
	pm_runtime_put_sync(&dev->pdev->dev);
}

inline u32 pix_fmt_to_bpc(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return DPRX_DMA_VID_BITS_WIDTH_8;
	case V4L2_PIX_FMT_XRGB30:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		return DPRX_DMA_VID_BITS_WIDTH_10;
	case V4L2_PIX_FMT_XRGB36:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		return DPRX_DMA_VID_BITS_WIDTH_12;
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		return DPRX_DMA_VID_BITS_WIDTH_14;
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		return DPRX_DMA_VID_BITS_WIDTH_16;
	default:
		return DPRX_DMA_VID_BITS_WIDTH_8;
	}
}

static void dprx_dma_set_fmt_vid_for_each_ch(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	int ch_no = ch->virtual_channel;

	u32 colorspace = ch->v_fmt.fmt.pix.colorspace;
	u32 pixelformat = ch->v_fmt.fmt.pix.pixelformat;
	u32 width = ch->v_fmt.fmt.pix.width;
	u32 height = ch->v_fmt.fmt.pix.height;
	u32 packed = ch->v_fmt.fmt.pix.priv;
	u32 cp, bpc;

	if (colorspace == V4L2_COLORSPACE_SRGB)
		cp = DPRX_DMA_VID_FMT_STRM_RGB;
	else
		cp = DPRX_DMA_VID_FMT_STRM_RAW;

	bpc = pix_fmt_to_bpc(pixelformat);

	mutex_lock(&dev->mutex_dprx_dma_reg);
	writel(cp, dev->dma_base + DPRX_DMA_VID_FMT_STRM(ch_no));

	writel(bpc, dev->dma_base + DPRX_DMA_VID_BITS_STRM(ch_no));

	writel(packed, dev->dma_base + DPRX_DMA_VID_MODE_STRM(ch_no));

	writel(height, dev->dma_base + DPRX_DMA_VID_HEIGHT_STRM(ch_no));

	writel(width, dev->dma_base + DPRX_DMA_VID_WIDTH_STRM(ch_no));
	mutex_unlock(&dev->mutex_dprx_dma_reg);
}

static void dprx_dma_set_vid_base_addr_for_each_ch(struct dprx_channel *ch, int frm_no, unsigned long addr)
{
	struct dprx_dev *dev = ch->dev;
	int ch_no = ch->virtual_channel;

	/* No need to protect this register write operation with locks since
	 * the callers of this function have already taken care of locking
	 */
	writel(addr, dev->dma_base + DPRX_DMA_YBASE_STRM(ch_no, frm_no));
}


static void dprx_dma_set_vid_frame_num_for_each_ch(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	int ch_no = ch->virtual_channel;

	mutex_lock(&dev->mutex_dprx_dma_reg);
	writel(DPRX_NUM_FRAME_ADDR - 1, dev->dma_base +
					DPRX_DMA_FRAME_NUM_VID_STRM(ch_no));
	mutex_unlock(&dev->mutex_dprx_dma_reg);
}

static void dprx_dma_enable_interrupts_for_ch(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	u32 val;

	mutex_lock(&dev->mutex_dprx_dma_reg);

#ifdef CRC_CHECK
	/* Unmask interrupts at DPRX_DMA level */
	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_START));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_START));
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_DONE));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_DONE));

	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_ERR));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_ERR));

#ifdef CRC_CHECK
	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_START));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_START));
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_DONE));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_DONE));

	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_ERR));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_ERR));

#ifdef CRC_CHECK
	/* Clear residual interrupts for DPRX_DMA, if any */
	val = readl(dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_START));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_START));
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_DONE));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_DONE));

	val = readl(dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_ERR));
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_ERR));

	mutex_unlock(&dev->mutex_dprx_dma_reg);
}

static void dprx_dma_enable_and_update(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	unsigned long flags = 0;
	u32 val;

	mutex_lock(&dev->mutex_dprx_dma_reg);
	/* Reset the VID channel */
	val = readl(dev->dma_base + DPRX_DMA_SOFT_RESET_VID);
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_SOFT_RESET_VID);

	writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
	writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);

	/* Reset release for the VID channel */
	val = readl(dev->dma_base + DPRX_DMA_SOFT_RESET_VID);
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_SOFT_RESET_VID);

	writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
	writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	mutex_unlock(&dev->mutex_dprx_dma_reg);

	spin_lock_irqsave(&dev->dma_en_vid_lock, flags);
	val = readl(dev->dma_base + DPRX_DMA_DMA_EN_VID);
	val |= (1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_DMA_EN_VID);

	writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
	writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	spin_unlock_irqrestore(&dev->dma_en_vid_lock, flags);
}

static void dprx_dma_disable_interrupts_for_ch(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	u32 val;

	mutex_lock(&dev->mutex_dprx_dma_reg);

#ifdef CRC_CHECK
	/* Mask interrupts at DPRX_DMA level */
	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_START));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_START));
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_DONE));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_DONE));

	val = readl(dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_ERR));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_MSK_VID(FRAME_ERR));

#ifdef CRC_CHECK
	/* Disable interrupt source for DPRX_DMA */
	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_START));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_START));
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_DONE));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_DONE));

	val = readl(dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_ERR));
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_INT_SRC_VID(FRAME_ERR));

	mutex_unlock(&dev->mutex_dprx_dma_reg);
}

static void dprx_dma_disable_and_update(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	unsigned long flags = 0;
	u32 val;

	spin_lock_irqsave(&dev->dma_en_vid_lock, flags);
	val = readl(dev->dma_base + DPRX_DMA_DMA_EN_VID);
	val &= ~(1 << ch->virtual_channel);
	writel(val, dev->dma_base + DPRX_DMA_DMA_EN_VID);

	writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
	writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	spin_unlock_irqrestore(&dev->dma_en_vid_lock, flags);
}

static void add_to_ring_buffer(struct dprx_channel *ch, struct dprx_buffer *buf)
{
	ch->frame[ch->ring_add_ptr] = buf;
	ch->frame_paddr[ch->ring_add_ptr] = vb2_dma_contig_plane_dma_addr(&buf->vb.vb2_buf, 0);

	dprx_dma_set_vid_base_addr_for_each_ch(ch, ch->ring_add_ptr, ch->frame_paddr[ch->ring_add_ptr]);

	ch->ring_add_ptr++;
	ch->ring_add_ptr = ch->ring_add_ptr % DPRX_NUM_FRAME_ADDR;
	ch->runtime_ring_cnt++;
}

irqreturn_t dprx_link_irq(int irq_dprx, void *data)
{
	struct dprx_dev *dev = (struct dprx_dev *)data;
	u32 val1, val2, i;
	u8 slave_id[8] = {
				SLAVE_ID_MAINLINK_REG_0,
				SLAVE_ID_MAINLINK_REG_1,
				SLAVE_ID_MAINLINK_REG_2,
				SLAVE_ID_MAINLINK_REG_3,
				SLAVE_ID_MAINLINK_REG_4,
				SLAVE_ID_MAINLINK_REG_5,
				SLAVE_ID_MAINLINK_REG_6,
				SLAVE_ID_MAINLINK_REG_7,
			};

	val1 = dprx_cfg_read(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_INTR_1);
	if (val1) {
		for (i = 0; i < DPRX_NUM_MST; i++) {
			/* Find out which stream caused interrupt */
			if (val1 & (1 << i)) {
				val2 = dprx_cfg_read(dev, slave_id[i],
							DPRX_ADDR_MAINLINK_INTR1);
				/* to find the type of interrupt */
				if (val2) {
					if (val2 & DPRX_VID_FRAME_ERR_INTR)
						dprx_err(dev, "FRAME ERROR originated "\
								"from DPRX LINK\n");
					dprx_cfg_write(dev, slave_id[i],
							DPRX_ADDR_MAINLINK_INTR1, val2);
				}
			}
		}

		dprx_cfg_write(dev, SLAVE_ID_TOP_REG, DPRX_ADDR_INTR_1, val1);
	}

	return IRQ_HANDLED;
}

void release_frame(struct dprx_channel *ch, enum vb2_buffer_state state)
{
	struct dprx_buffer *buf;
	struct dprx_dmaqueue *vidq = &ch->vidq;
	struct dprx_dev *dev = ch->dev;
	u32 val;

	if (atomic_read(&ch->end_irq_worker) == 0)
		return;

	spin_lock(&ch->slock);
	buf = ch->frame[ch->ring_del_ptr];
	if (buf) {
		ch->irq_counter++;
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		ch->frame[ch->ring_del_ptr] = NULL;
		ch->ring_del_ptr = (ch->ring_del_ptr + 1) % DPRX_NUM_FRAME_ADDR;
		ch->runtime_ring_cnt--;
	}
	/* If Ring is empty, turn off the DMA before it reaches to same buffer
	 * address by round robin
	 */
	if (!ch->runtime_ring_cnt) {
		spin_lock(&dev->dma_en_vid_lock);
		val = readl(dev->dma_base + DPRX_DMA_DMA_EN_VID);
		val &= ~(1 << ch->virtual_channel);
		writel(val, dev->dma_base + DPRX_DMA_DMA_EN_VID);

		writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
		writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
		atomic_set(&ch->dma_on, DMA_OFF_BY_INTR);
		spin_unlock(&dev->dma_en_vid_lock);
	}

	if (!list_empty(&vidq->active)) {
		/* Add next buffer from active queue to ring */
		buf = list_entry(vidq->active.next, struct dprx_buffer, list);
		list_del(&buf->list);
	} else {
		/* If active queue is empty, just return */
		spin_unlock(&ch->slock);
		return;
	}
	add_to_ring_buffer(ch, buf);
	/* Update the new address to IP */
	writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
	writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	spin_unlock(&ch->slock);

	spin_lock(&dev->dma_en_vid_lock);
	if (atomic_read(&ch->dma_on) == DMA_OFF_BY_INTR) {
		atomic_set(&ch->dma_on, DMA_ON);
		val = readl(dev->dma_base + DPRX_DMA_DMA_EN_VID);
		val |= (1 << ch->virtual_channel);
		writel(val, dev->dma_base + DPRX_DMA_DMA_EN_VID);

		writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
		writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	}
	spin_unlock(&dev->dma_en_vid_lock);
}

#ifdef CRC_CHECK
void check_crc(struct dprx_channel *ch)
{
	struct dprx_dev *dev = ch->dev;
	u32 val;
	u8 slave_id[8] = {
				SLAVE_ID_MAINLINK_REG_0,
				SLAVE_ID_MAINLINK_REG_1,
				SLAVE_ID_MAINLINK_REG_2,
				SLAVE_ID_MAINLINK_REG_3,
				SLAVE_ID_MAINLINK_REG_4,
				SLAVE_ID_MAINLINK_REG_5,
				SLAVE_ID_MAINLINK_REG_6,
				SLAVE_ID_MAINLINK_REG_7,
			};
	val = dprx_cfg_read(dev, slave_id[ch->virtual_channel],
						DPRX_ADDR_TEST_CRC_R_CR_L);
	val |= (dprx_cfg_read(dev, slave_id[ch->virtual_channel],
					DPRX_ADDR_TEST_CRC_R_CR_H)) << 8;
	if (val != ch->crc)
		ch_err(ch, "CRC error: frame:%d CRC at DPRX:%x\n", ch->irq_counter, val);
}
#endif

irqreturn_t dprx_dma_irq(int irq_dprx, void *data)
{
	struct dprx_dev *dev = (struct dprx_dev *)data;
	int i;
	u32 val;

#ifdef CRC_CHECK
	val = readl(dev->dma_base + DPRX_DMA_INT_VID_STATUS(FRAME_START));
	if (val) {
		for (i = 0; i < DPRX_NUM_MST; i++) {
			if ((dev->stream_enabled[i]) && (val & (1 << i)))
				check_crc(dev->ch[i]);
		}
		writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_START));
	}
#endif

	val = readl(dev->dma_base + DPRX_DMA_INT_VID_STATUS(FRAME_ERR));
	if (val) {
		dprx_err(dev, "DPRX DMA reports FRAME_ERROR\n");
		for (i = 0; i < DPRX_NUM_MST; i++) {
			if ((dev->stream_enabled[i]) && (val & (1 << i)))
				release_frame(dev->ch[i], VB2_BUF_STATE_ERROR);
		}
		writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_ERR));
	}

	val = readl(dev->dma_base + DPRX_DMA_INT_VID_STATUS(FRAME_DONE));
	if (val) {
		for (i = 0; i < DPRX_NUM_MST; i++) {
			if ((dev->stream_enabled[i]) && (val & (1 << i)))
				release_frame(dev->ch[i], VB2_BUF_STATE_DONE);
		}
		writel(val, dev->dma_base + DPRX_DMA_INT_CLR_VID(FRAME_DONE));
	}

	return IRQ_HANDLED;
}

/*
 * video ioctls
 */
static int dprx_querycap(struct file *file, void *priv,
			struct v4l2_capability *cap)
{
	struct dprx_channel *ch = video_drvdata(file);

	strlcpy(cap->driver, DPRX_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, DPRX_MODULE_NAME, sizeof(cap->card));

	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", ch->v4l2_dev.name);
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int dprx_enum_fmt_vid_cap(struct file *file, void  *priv,
				struct v4l2_fmtdesc *f)
{
	struct dprx_channel *ch = video_drvdata(file);
	const struct dprx_fmt *fmt = NULL;

	if (f->index >= ch->num_active_fmt)
		return -EINVAL;

	fmt = ch->active_fmt[f->index];

	f->pixelformat = fmt->fourcc;
	f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return 0;
}

static int __subdev_get_format(struct dprx_channel *ch,
			       struct v4l2_mbus_framefmt *fmt, int vc)
{
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &sd_fmt.format;
	int ret;

	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sd_fmt.pad = 0;

	ret = v4l2_subdev_call(ch->dev->sensor[vc],
						pad, get_fmt, NULL, &sd_fmt);
	if (ret)
		return ret;

	*fmt = *mbus_fmt;

	ch_dbg(1, ch, "%s %dx%d code:%04X\n", __func__,
		fmt->width, fmt->height, fmt->code);

	return 0;
}

static int __subdev_set_format(struct dprx_channel *ch,
			       struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct v4l2_mbus_framefmt *mbus_fmt = &sd_fmt.format;
	int ret;

	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sd_fmt.pad = 0;
	*mbus_fmt = *fmt;

	ret = v4l2_subdev_call(ch->dev->sensor[ch->virtual_channel],
						pad, set_fmt, NULL, &sd_fmt);
	if (ret)
		return ret;

	ch_dbg(1, ch, "%s %dx%d code:%04X\n", __func__,
		fmt->width, fmt->height, fmt->code);

	return 0;
}

static int dprx_format_size(struct dprx_channel *ch,
				const struct dprx_fmt *fmt,
				struct v4l2_format *f)
{
	if (!fmt) {
		ch_dbg(3, ch, "No dprx_fmt provided!\n");
		return -EINVAL;
	}

	f->fmt.pix.bytesperline = bytes_per_line(f->fmt.pix.width, fmt->depth);

	/* Store packed/unpacked information in priv field */
	f->fmt.pix.priv = fmt->packed;
	f->fmt.pix.sizeimage = f->fmt.pix.height *
			       f->fmt.pix.bytesperline;

	ch_info(ch, "fourcc: %s size: %dx%d bpl:%d img_size:%d packed: %d\n",
		fourcc_to_str(f->fmt.pix.pixelformat),
		f->fmt.pix.width, f->fmt.pix.height,
		f->fmt.pix.bytesperline, f->fmt.pix.sizeimage, f->fmt.pix.priv);

	return 0;
}

static int dprx_g_fmt_vid_cap(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct dprx_channel *ch = video_drvdata(file);

	*f = ch->v_fmt;

	return 0;
}

static int dprx_try_fmt_vid_cap(struct file *file, void *priv,
			       struct v4l2_format *f)
{
	struct dprx_channel *ch = video_drvdata(file);
	const struct dprx_fmt *fmt;
	struct v4l2_subdev_frame_size_enum fse;
	int ret, found;

	fmt = find_format_by_pix(ch, f->fmt.pix.pixelformat);
	if (!fmt) {
		ch_dbg(3, ch, "Fourcc format (0x%08x) not found.\n",
			f->fmt.pix.pixelformat);

		/* Just get the first one enumerated */
		fmt = ch->active_fmt[0];
		f->fmt.pix.pixelformat = fmt->fourcc;
	}

	f->fmt.pix.field = ch->v_fmt.fmt.pix.field;

	/* check for/find a valid width/height */
	ret = 0;
	found = false;
	fse.pad = 0;
	fse.code = fmt->code;
	fse.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	for (fse.index = 0; ; fse.index++) {
		ret = v4l2_subdev_call(ch->dev->sensor[ch->virtual_channel],
					pad, enum_frame_size, NULL, &fse);
		if (ret)
			break;

		if ((f->fmt.pix.width == fse.max_width) &&
		    (f->fmt.pix.height == fse.max_height)) {
			found = true;
			break;
		} else if ((f->fmt.pix.width >= fse.min_width) &&
			 (f->fmt.pix.width <= fse.max_width) &&
			 (f->fmt.pix.height >= fse.min_height) &&
			 (f->fmt.pix.height <= fse.max_height)) {
			found = true;
			break;
		}
	}

	if (!found) {
		/* use existing values as default */
		f->fmt.pix.width = ch->v_fmt.fmt.pix.width;
		f->fmt.pix.height =  ch->v_fmt.fmt.pix.height;
	}

	return dprx_format_size(ch, fmt, f);
}

static int dprx_s_fmt_vid_cap(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct dprx_channel *ch = video_drvdata(file);
	struct vb2_queue *q = &ch->vb_vidq;
	const struct dprx_fmt *fmt;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;

	if (vb2_is_busy(q)) {
		ch_dbg(3, ch, "%s device busy: %d\n", __func__, q->num_buffers);
		return -EBUSY;
	}

	ret = dprx_try_fmt_vid_cap(file, priv, f);
	if (ret < 0)
		return ret;

	fmt = find_format_by_pix(ch, f->fmt.pix.pixelformat);

	v4l2_fill_mbus_format(&mbus_fmt, &f->fmt.pix, fmt->code);

	ret = __subdev_set_format(ch, &mbus_fmt);
	if (ret)
		return ret;

	/* Just double check nothing has gone wrong */
	if (mbus_fmt.code != fmt->code) {
		ch_dbg(3, ch,
			"%s subdev changed format, should not happen\n",
			__func__);
		return -EINVAL;
	}

	v4l2_fill_pix_format(&ch->v_fmt.fmt.pix, &mbus_fmt);
	ch->v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ch->v_fmt.fmt.pix.pixelformat  = fmt->fourcc;
	ch->v_fmt.fmt.pix.colorspace = fmt->colorspace;
	dprx_format_size(ch, fmt, &ch->v_fmt);
	ch->fmt = fmt;
	ch->m_fmt = mbus_fmt;
	*f = ch->v_fmt;

	return 0;
}

/*
 * Videobuf operations
 */
static int dprx_queue_setup(struct vb2_queue *vq,
			   unsigned int *nbuffers, unsigned int *nplanes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct dprx_channel *ch = vb2_get_drv_priv(vq);
	unsigned int size = ch->v_fmt.fmt.pix.sizeimage;

	if (*nplanes) {
		if (sizes[0] < size)
			return -EINVAL;
		size = sizes[0];
	}

	*nplanes = 1;
	sizes[0] = size;

	ch_dbg(3, ch, "nbuffers=%d, size=%d\n", *nbuffers, sizes[0]);

	return 0;
}

static int dprx_buffer_prepare(struct vb2_buffer *vb)
{
	struct dprx_channel *ch = vb2_get_drv_priv(vb->vb2_queue);
	struct dprx_buffer *buf = container_of(vb, struct dprx_buffer,
					      vb.vb2_buf);
	unsigned long size;

	if (WARN_ON(!ch->fmt))
		return -EINVAL;

	size = ch->v_fmt.fmt.pix.sizeimage;
	if (vb2_plane_size(vb, 0) < size) {
		ch_err(ch,
			"data will not fit into plane (%lu < %lu)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, size);
	return 0;
}

static void dprx_buffer_queue(struct vb2_buffer *vb)
{
	struct dprx_channel *ch = vb2_get_drv_priv(vb->vb2_queue);
	struct dprx_buffer *buf = container_of(vb, struct dprx_buffer,
					      vb.vb2_buf);
	struct dprx_dmaqueue *vidq = &ch->vidq;
	struct dprx_dev *dev = ch->dev;
	unsigned long flags = 0;
	u32 val;

	spin_lock_irqsave(&ch->slock, flags);
	list_add_tail(&buf->list, &vidq->active);
	buf->sequence_num = ch->dev->sequence++;
	ch->buf_counter++;

	if (atomic_read(&ch->end_irq_worker) == 0) {
		spin_unlock_irqrestore(&ch->slock, flags);
		return;
	}

	/* If ring queue is empty, move buffer from active queue to ring */
	if (!ch->runtime_ring_cnt) {
		buf = list_entry(vidq->active.next, struct dprx_buffer, list);
		list_del(&buf->list);
		add_to_ring_buffer(ch, buf);
		/* Update the new address to IP */
		writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
		writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	}
	spin_unlock_irqrestore(&ch->slock, flags);

	/* If DMA is set to off by IRQ handler(list empty), re-init the same */
	spin_lock_irqsave(&dev->dma_en_vid_lock, flags);
	if (atomic_read(&ch->dma_on) == DMA_OFF_BY_INTR) {
		atomic_set(&ch->dma_on, DMA_ON);
		val = readl(dev->dma_base + DPRX_DMA_DMA_EN_VID);
		val |= (1 << ch->virtual_channel);
		writel(val, dev->dma_base + DPRX_DMA_DMA_EN_VID);

		writel(0x1, dev->dma_base + DPRX_DMA_SFR_UP);
		writel(0x0, dev->dma_base + DPRX_DMA_SFR_UP);
	}
	spin_unlock_irqrestore(&dev->dma_en_vid_lock, flags);
}

static void dprx_buffer_finish(struct vb2_buffer *vb)
{
	/* TODO: Any final processing of buffer before returning the buffer */
}

static int dprx_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct dprx_channel *ch = vb2_get_drv_priv(vq);
	struct dprx_dmaqueue *dma_q = &ch->vidq;
	struct dprx_buffer *buf, *tmp;
	unsigned long flags;
	int i = 0, ret = 0;

	spin_lock_irqsave(&ch->slock, flags);
	ch->ring_add_ptr = 0;
	ch->ring_del_ptr = 0;
	ch->runtime_ring_cnt = 0;
	spin_unlock_irqrestore(&ch->slock, flags);

	/* Out of all buffers queued from the application, queue
	 * DPRX_NUM_FRAME_ADDR number of buffers to start with.
	 * As the capture progresses, the remaining addresses
	 * will be utilized to capture the frames.
	 */
	for (i = 0; i < DPRX_NUM_FRAME_ADDR; i++) {
		spin_lock_irqsave(&ch->slock, flags);
		if (list_empty(&dma_q->active)) {
			spin_unlock_irqrestore(&ch->slock, flags);
			ch_err(ch, "Active buffer queue is empty!\n");
			return -EIO;
		}

		buf = list_entry(dma_q->active.next, struct dprx_buffer, list);
		list_del(&buf->list);
		add_to_ring_buffer(ch, buf);
		spin_unlock_irqrestore(&ch->slock, flags);
	}

	dprx_dma_set_fmt_vid_for_each_ch(ch);
	dprx_dma_set_vid_frame_num_for_each_ch(ch);

	ret = v4l2_subdev_call(ch->dev->sensor[ch->virtual_channel],
							video, s_stream, 1);
	if (ret) {
		ch_err(ch, "stream on failed in subdev\n");
		goto err;
	}

	dprx_dma_enable_and_update(ch);
	dprx_dma_enable_interrupts_for_ch(ch);
	atomic_set(&ch->dma_on, DMA_ON);
	atomic_set(&ch->end_irq_worker, 1);

	return ret;

err:
	spin_lock_irqsave(&ch->slock, flags);
	list_for_each_entry_safe(buf, tmp, &dma_q->active, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&ch->slock, flags);

	return ret;
}

static void dprx_stop_streaming(struct vb2_queue *vq)
{
	struct dprx_channel *ch = vb2_get_drv_priv(vq);
	struct dprx_dmaqueue *dma_q = &ch->vidq;
	struct dprx_buffer *buf, *tmp;
	unsigned long flags;
	int i;

	atomic_set(&ch->end_irq_worker, 0);
	dprx_dma_disable_interrupts_for_ch(ch);
	dprx_dma_disable_and_update(ch);
	atomic_set(&ch->dma_on, DMA_OFF);

	if (v4l2_subdev_call(ch->dev->sensor[ch->virtual_channel],
							video, s_stream, 0))
		ch_err(ch, "stream off failed in subdev\n");

	/* Release all active buffers in vidq list */
	spin_lock_irqsave(&ch->slock, flags);
	list_for_each_entry_safe(buf, tmp, &dma_q->active, list) {
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	dprx_info(ch->dev, "strm: %d no of frames:%d\n", ch->virtual_channel, ch->irq_counter);

	ch->irq_counter = 0;
	ch->buf_counter = 0;

	/* Release all active buffers left in the ring */
	for (i = 0; i < DPRX_NUM_FRAME_ADDR; i++) {
		buf = ch->frame[i];
		if (buf) {
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
			ch->frame[i] = NULL;
		}
	}
	ch->ring_add_ptr = 0;
	ch->ring_del_ptr = 0;
	ch->runtime_ring_cnt = 0;
	spin_unlock_irqrestore(&ch->slock, flags);
}

static const struct vb2_ops dprx_video_qops = {
	.queue_setup		= dprx_queue_setup,
	.buf_prepare		= dprx_buffer_prepare,
	.buf_queue		= dprx_buffer_queue,
	.buf_finish		= dprx_buffer_finish,
	.start_streaming	= dprx_start_streaming,
	.stop_streaming		= dprx_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static const struct v4l2_file_operations dprx_fops = {
	.owner		= THIS_MODULE,
	.open           = v4l2_fh_open,
	.release        = vb2_fop_release,
	.read           = vb2_fop_read,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = vb2_fop_mmap,
};

static const struct v4l2_ioctl_ops dprx_ioctl_ops = {
	.vidioc_querycap      = dprx_querycap,

	.vidioc_enum_fmt_vid_cap  = dprx_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = dprx_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = dprx_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = dprx_g_fmt_vid_cap,

	.vidioc_reqbufs       = vb2_ioctl_reqbufs,
	.vidioc_querybuf      = vb2_ioctl_querybuf,
	.vidioc_qbuf          = vb2_ioctl_qbuf,
	.vidioc_expbuf	      = vb2_ioctl_expbuf,
	.vidioc_dqbuf         = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf   = vb2_ioctl_prepare_buf,
	.vidioc_create_bufs   = vb2_ioctl_create_bufs,

	.vidioc_streamon      = vb2_ioctl_streamon,
	.vidioc_streamoff     = vb2_ioctl_streamoff,

	.vidioc_log_status    = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static struct video_device dprx_videodev = {
	.name		= DPRX_MODULE_NAME,
	.fops		= &dprx_fops,
	.ioctl_ops	= &dprx_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static int dprx_ip_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

long dprx_ip_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct dprx_dev *dev = v4l2_get_subdevdata(sd);
	struct dpcd *dpcd_data = arg;
	struct dp_crc *dp_crc = arg;
	struct dp_payload_id *id_table = arg;
	int i;

	switch (cmd) {
	case VIDIOC_SUBDEV_PRIV_DPCD_ACCESS:

		if (!dpcd_data->operation) {
			dprx_dpcd_read(dev, (u32)dpcd_data->dpcd_addr,
				(u32)dpcd_data->dpcd_length, (u8 *)dpcd_data->dpcd_val);
		} else {
			dprx_dpcd_write(dev, (u32)dpcd_data->dpcd_addr,
				(u32)dpcd_data->dpcd_length, (u8 *)dpcd_data->dpcd_val);
		}
	break;
	case VIDIOC_SUBDEV_PRIV_DPRX_CRC:
		for (i = 0; i < 8; i++)
			if (dev->stream_enabled[i]) {
				dev->ch[i]->crc = dp_crc->crc[i];
				dprx_info(dev, "CRC received from DPTX: %x\n", dev->ch[i]->crc);
			}
	break;

	case VIDIOC_SUBDEV_PRIV_SET_PAYLOAD_ID:
		for (i = 0; i < DPRX_NUM_MST; i++) {
			dev->payload_id[i] = id_table->payload_id[i];
			dev->xval[i] = id_table->xval[i];
			dev->yval[i] = id_table->yval[i];
		}
		for (i = 0; i < PAYLOAD_TBL_SZ; i++)
			dev->payload_id_table[i] = id_table->payload_id_table[i];
		dev->mst_en = 1;
	break;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops dprx_ip_core_ops = {
	.s_power	= dprx_ip_s_power,
	.ioctl		= dprx_ip_priv_ioctl,
};

static int dprx_ip_s_stream(struct v4l2_subdev *sd, int on)
{
	struct dprx_dev *dev = v4l2_get_subdevdata(sd);
	int i, j, ret = 0;

	if (on) {
		if (!dev->dprx_ip_is_on) {
			dprx_runtime_get(dev);
			for (i = 0; i < dev->dprx_num_clock; i++) {
				ret = clk_prepare_enable(dev->dp_clk[i]);
				if (ret) {
					dprx_err(dev,
					"Failed to enable dp_clk:%d\n", i);
					/* Disable clocks enabled till now */
					for (j = 0; j < i; j++)
						clk_disable(dev->dp_clk[j]);
					dprx_runtime_put(dev);
					return ret;
				}
			}
			ret = dprx_ip_configure(dev);
			if (ret) {
				v4l2_err(&dev->v4l2_dev,
					"Failed to configure DPRX IP\n");
				for (i = 0; i < dev->dprx_num_clock; i++)
					clk_disable(dev->dp_clk[i]);
				dprx_runtime_put(dev);
				return ret;
			}
			dev->dprx_ip_is_on = true;
		}
	} else {
		if (dev->dprx_ip_is_on) {
			dprx_ip_off(dev);
			for (i = 0; i < dev->dprx_num_clock; i++)
				clk_disable(dev->dp_clk[i]);
			dprx_runtime_put(dev);
			dev->dprx_ip_is_on = false;
		}
	}

	return ret;
}

static const struct v4l2_subdev_video_ops dprx_ip_video_ops = {
	.s_stream = dprx_ip_s_stream,
};

static const struct v4l2_subdev_ops dprx_ip_subdev_ops = {
	.core = &dprx_ip_core_ops,
	.video = &dprx_ip_video_ops,
};

static int dprx_ip_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dprx_dev	*dev = container_of(ctrl->handler,
					struct dprx_dev, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_USER_DPRX_NO_OF_LANE:
		dev->lane_count = ctrl->val;
		break;
	case V4L2_CID_USER_DPRX_LANE_SPEED:
		dev->link_rate = ctrl->val;
		break;
	case V4L2_CID_USER_DPRX_TPS_PATTERN:
		dev->tps_pattern = ctrl->val;
		break;
	case V4L2_CID_USER_DPRX_ENABLE_HDCP:
		dev->hdcp_en = ctrl->val;
		break;
	case V4L2_CID_USER_DPRX_ENABLE_DSC:
		dev->dsc_en = ctrl->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops dprx_ip_ctrl_ops = {
	.s_ctrl	= dprx_ip_s_ctrl,
};

static const struct v4l2_ctrl_config dprx_ip_no_of_lane = {
	.ops	= &dprx_ip_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_NO_OF_LANE,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "DPRX no of lanes",
	.min	= 1,
	.step	= 1,
	.max	= 4,
	.def	= 1,
};

static const struct v4l2_ctrl_config dprx_ip_lane_speed = {
	.ops	= &dprx_ip_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_LANE_SPEED,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "DPRX lane speed",
	.min	= 0,
	.step	= 1,
	.max	= 3,
	.def	= 1,
};

static const struct v4l2_ctrl_config dprx_ip_tps_pattern = {
	.ops	= &dprx_ip_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_TPS_PATTERN,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "DPRX link training pattern",
	.min	= 1,
	.step	= 1,
	.max	= 3,
	.def	= 1,
};

static const struct v4l2_ctrl_config dprx_ip_hdcp_enable = {
	.ops	= &dprx_ip_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_ENABLE_HDCP,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.name	= "DPRX HDCP enable",
	.step	= 1,
	.def	= 0,
};

static const struct v4l2_ctrl_config dprx_ip_dsc_enable = {
	.ops	= &dprx_ip_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_ENABLE_DSC,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.name	= "DPRX DSC enable",
	.step	= 1,
	.def	= 0,
};

static int dprx_ip_power_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dprx_dev	*dev = container_of(ctrl->handler,
					struct dprx_dev, ctrl_handler);
	int ret;

	/* Call DPRX IP EN, DPRX_VID/AUD/SDP EN, SFR_UP */
	ret = dprx_ip_s_stream(&dev->subdev, ctrl->val);

	return ret;
}

static const struct v4l2_ctrl_ops dprx_ip_power_state_ctrl_ops = {
	.s_ctrl	= dprx_ip_power_s_ctrl,
};

static const struct v4l2_ctrl_config dprx_ip_power_state_ctrl = {
	.ops	= &dprx_ip_power_state_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_POWER_STATE,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.name	= "DPRX power state: on/of",
	.step	= 1,
	.def	= 0,
};

static int dprx_ip_mst_config_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct dprx_dev	*dev = container_of(ctrl->handler,
					struct dprx_dev, ctrl_handler);
	int i;

	for (i = 0; i < 8; i++)
		if (ctrl->val & (1 << i))
			dev->stream_enabled[i] = 1;

	for (i = 0; i < 8; i++)
		dprx_info(dev, "stream_enabled[%d]: %d\n", i,
						dev->stream_enabled[i]);

	return 0;
}

static const struct v4l2_ctrl_ops dprx_ip_mst_config_ctrl_ops = {
	.s_ctrl	= dprx_ip_mst_config_s_ctrl,
};

static const struct v4l2_ctrl_config dprx_ip_mst_config = {
	.ops	= &dprx_ip_mst_config_ctrl_ops,
	.id	= V4L2_CID_USER_DPRX_MST_CONFIG,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "DPRX CH to enable",
	.step	= 1,
	.def	= 0,
	.min	= 0,
	.max	= 255,
};

/* -----------------------------------------------------------------
 *	Initialization and module stuff
 * ------------------------------------------------------------------
 */
static int dprx_complete_ch(struct dprx_channel *ch);

static int dprx_async_bound(struct v4l2_async_notifier *notifier,
			   struct v4l2_subdev *subdev,
			   struct v4l2_async_subdev *asd)
{
	struct dprx_dev *dev = notifier_to_dev(notifier);
	struct dprx_channel *ch;
	struct dprx_fmt *fmt;
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	int ret = 0;
	int i, j, k;

	for (i = 0; i < DPRX_NUM_MST; i++) {
		if (dev->asd[i].match.fwnode.fwnode ==
			of_fwnode_handle(subdev->dev->of_node)) {
			dev->sensor[i] = subdev;
			break;
		}
	}

	ch = dev->ch[i];

	v4l2_set_subdev_hostdata(subdev, ch);

	v4l2_info(&dev->v4l2_dev, "Hooked sensor subdevice: %s to parent\n",
								subdev->name);

	/* Enumerate sub device formats and enable all matching dprx formats */
	ch->num_active_fmt = 0;
	for (j = 0, i = 0; ret != -EINVAL; ++j) {
		memset(&mbus_code, 0, sizeof(mbus_code));
		mbus_code.index = j;
		ret = v4l2_subdev_call(subdev, pad, enum_mbus_code,
				       NULL, &mbus_code);
		if (ret)
			continue;

		ch_dbg(2, ch,
			"subdev %s: code: %04x idx: %d\n",
			subdev->name, mbus_code.code, j);

		for (k = 0; k < ARRAY_SIZE(dprx_formats); k++) {
			fmt = &dprx_formats[k];

			if (mbus_code.code == fmt->code) {
				ch->active_fmt[i] = fmt;
				ch_dbg(2, ch,
					"matched fourcc: %s: code: %04x idx: %d\n",
					fourcc_to_str(fmt->fourcc),
					fmt->code, i);
				ch->num_active_fmt = ++i;
			}
		}
	}

	if (i == 0) {
		ch_err(ch, "No suitable format reported by subdev %s\n",
			subdev->name);
		return -EINVAL;
	}

	ret = dprx_complete_ch(ch);
	if (ret) {
		ch_err(ch, "failed to register video dev for MST ch %d\n", i);
		return ret;
	}

	ret = __subdev_get_format(ch, &mbus_fmt, ch->virtual_channel);
	if (ret)
		return ret;

	fmt = find_format_by_code(ch, mbus_fmt.code);
	if (!fmt) {
		ch_dbg(3, ch, "mbus code format (0x%08x) not found.\n",
			mbus_fmt.code);
		return -EINVAL;
	}

	/* Save current subdev format */
	v4l2_fill_pix_format(&ch->v_fmt.fmt.pix, &mbus_fmt);
	ch->v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ch->v_fmt.fmt.pix.pixelformat  = fmt->fourcc;
	ch->v_fmt.fmt.pix.colorspace = fmt->colorspace;
	dprx_format_size(ch, fmt, &ch->v_fmt);
	ch->fmt = fmt;
	ch->m_fmt = mbus_fmt;

	return ret;
}

static int dprx_async_complete(struct v4l2_async_notifier *notifier)
{
	struct v4l2_subdev *sd;
	struct dprx_dev *dev = notifier_to_dev(notifier);
	int ret;

	ret = v4l2_device_register_subdev_nodes(&dev->v4l2_dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register v4l2_device: %d\n",
									ret);
	}

	list_for_each_entry(sd, &dev->v4l2_dev.subdevs, list) {
		if (sd->devnode)
			v4l2_info(&dev->v4l2_dev,
			"V4L2 sub device %s registered as %s\n", sd->name,
					video_device_node_name(sd->devnode));
	}

	return ret;
}

static int dprx_complete_ch(struct dprx_channel *ch)
{
	struct video_device *vfd;
	struct vb2_queue *q;
	int ret;

	/* initialize locks */
	spin_lock_init(&ch->slock);
	mutex_init(&ch->mutex);

	/* initialize queue */
	q = &ch->vb_vidq;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ | VB2_USERPTR;
	q->drv_priv = ch;
	q->buf_struct_size = sizeof(struct dprx_buffer);
	q->ops = &dprx_video_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &ch->mutex;
	q->min_buffers_needed = DPRX_NUM_FRAME_ADDR;
	q->dev = ch->v4l2_dev.dev;
	dma_set_coherent_mask(&ch->dev->pdev->dev, DMA_BIT_MASK(32));

	ret = vb2_queue_init(q);
	if (ret)
		return ret;

	/* init video dma queues */
	INIT_LIST_HEAD(&ch->vidq.active);

	vfd = &ch->vdev;
	*vfd = dprx_videodev;
	vfd->v4l2_dev = &ch->v4l2_dev;
	vfd->queue = q;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &ch->mutex;
	video_set_drvdata(vfd, ch);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
	if (ret < 0)
		return ret;

	v4l2_info(&ch->v4l2_dev, "V4L2 video device registered as %s\n",
		  video_device_node_name(vfd));

	return ret;
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
		 * It's the first dprxl, we have to find a port subnode
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

static int of_dprx_create_channel(struct dprx_channel *ch, int ch_no)
{
	struct platform_device *pdev = ch->dev->pdev;
	struct device_node *ep_node, *port, *remote_ep,
			*sensor_node, *parent;
	struct v4l2_fwnode_endpoint *endpoint;
	struct v4l2_async_subdev *asd;
	u32 regval = 0;
	int ret, index, found_port = 0;

	parent = pdev->dev.of_node;

	endpoint = &ch->endpoint;

	ep_node = NULL;
	port = NULL;
	remote_ep = NULL;
	sensor_node = NULL;
	ret = 0;

	for (index = 0; index < DPRX_NUM_MST; index++) {
		port = of_get_next_port(parent, port);
		if (!port) {
			ret = -ENODEV;
			goto cleanup_exit;
		}

		of_property_read_u32(port, "reg", &regval);
		if ((regval == ch_no) && (index == ch_no)) {
			found_port = 1;
			break;
		}
	}

	if (!found_port) {
		ch_dbg(1, ch, "No port node matches dprx port:%d\n",
			ch_no);
		ret = -ENODEV;
		goto cleanup_exit;
	}

	ep_node = of_get_next_endpoint(port, ep_node);
	if (!ep_node) {
		ch_dbg(3, ch, "can't get next endpoint\n");
	}

	sensor_node = of_graph_get_remote_port_parent(ep_node);
	if (!sensor_node) {
		ch_dbg(3, ch, "can't get remote parent\n");
		ret = -ENODEV;
		goto cleanup_exit;
	}

	remote_ep = of_parse_phandle(ep_node, "remote-endpoint", 0);
	if (!remote_ep) {
		ch_dbg(3, ch, "can't get remote-endpoint\n");
		ret = -ENODEV;
		goto cleanup_exit;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(remote_ep),
							endpoint);
	if (ret) {
		ch_dbg(3, ch, "failed to parse endpoint\n");
		ret = -ENODEV;
		goto cleanup_exit;
	}

	/* Store Virtual Channel number */
	ch->virtual_channel = endpoint->base.id;

	asd = &ch->dev->asd[ch->virtual_channel];
	asd->match_type = V4L2_ASYNC_MATCH_FWNODE;
	asd->match.fwnode.fwnode = of_fwnode_handle(sensor_node);

	ch->dev->asd_list[ch->virtual_channel] = asd;
	ch->dev->notifier.num_subdevs++;

cleanup_exit:
	if (!remote_ep)
		of_node_put(remote_ep);
	if (!sensor_node)
		of_node_put(sensor_node);
	if (!ep_node)
		of_node_put(ep_node);
	if (!port)
		of_node_put(port);

	return ret;
}

static struct dprx_channel *dprx_create_channel(struct dprx_dev *dev, int ch_no)
{
	struct dprx_channel *ch;
	int ret;

	ch = devm_kzalloc(&dev->pdev->dev, sizeof(*ch), GFP_KERNEL);
	if (!ch)
		return NULL;

	/* save the dprx_dev * for future ref */
	ch->dev = dev;

	snprintf(ch->v4l2_dev.name, sizeof(ch->v4l2_dev.name),
		 "%s%d-%03d", DPRX_MODULE_NAME, dev->id, ch_no);
	ret = v4l2_device_register(&dev->pdev->dev, &ch->v4l2_dev);
	if (ret)
		goto err_exit;

	ret = of_dprx_create_channel(ch, ch_no);
	if (ret)
		goto unreg_dev;

	return ch;

unreg_dev:
	v4l2_device_unregister(&ch->v4l2_dev);
err_exit:
	return NULL;
}
static int dprx_probe(struct platform_device *pdev)
{
	struct dprx_dev *dev;
	struct v4l2_subdev *sd;
	struct v4l2_ctrl_handler *handler;
	int i, ret;
	char clk_name[10];

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	sd = &dev->subdev;
	sd->ctrl_handler = handler = &dev->ctrl_handler;

	/* set pseudo v4l2 device name so we can use v4l2_printk */
	strlcpy(dev->v4l2_dev.name, DPRX_MODULE_NAME,
		sizeof(dev->v4l2_dev.name));

	/* save pdev pointer */
	dev->pdev = pdev;
	dev->id = of_alias_get_id(pdev->dev.of_node, "dprx");

	dev->res_link = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->base = devm_ioremap_resource(&pdev->dev, dev->res_link);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	dprx_info(dev, "DPRX LINK ioresource %s at %pa - %pa\n",
	dev->res_link->name, &dev->res_link->start, &dev->res_link->end);

	dev->link_irq = platform_get_irq(pdev, 0);
	dprx_dbg(1, dev, "got irq# %d\n", dev->link_irq);
	ret = devm_request_irq(&pdev->dev, dev->link_irq, dprx_link_irq,
						0, DPRX_MODULE_NAME, dev);
	if (ret)
		return ret;

	dev->res_dma = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	dev->dma_base = devm_ioremap_resource(&pdev->dev, dev->res_dma);
	if (IS_ERR(dev->dma_base))
		return PTR_ERR(dev->dma_base);
	dprx_info(dev, "DPRX DMA ioresource %s at %pa - %pa\n",
		dev->res_dma->name, &dev->res_dma->start, &dev->res_dma->end);

	dev->dma_irq = platform_get_irq(pdev, 1);
	dprx_dbg(1, dev, "got irq# %d\n", dev->dma_irq);
	ret = devm_request_irq(&pdev->dev, dev->dma_irq, dprx_dma_irq,
						0, DPRX_MODULE_NAME, dev);
	if (ret)
		return ret;

	/* BLK_CAM_DPRX0(DPRX2, DPRX3) has one clock extra than
	 * BLK_CAM_DPRX1 (DPRX0, DPRX1)
	 */
	if ((dev->id == 2) || (dev->id == 3))
		dev->dprx_num_clock = DPRX_NUM_CLOCK;
	else
		dev->dprx_num_clock = DPRX_NUM_CLOCK - 1;

	for (i = 0; i < dev->dprx_num_clock; i++) {
		snprintf(clk_name, sizeof(clk_name), "dp_clk%d", i);
		dev->dp_clk[i] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(dev->dp_clk[i])) {
			dev_err(&pdev->dev, "cannot get dp_clk:%d\n", i);
			return -ENOENT;
		}
	}

	platform_set_drvdata(pdev, dev);

	for (i = 0; i < DPRX_NUM_MST; i++)
		dev->ch[i] = dprx_create_channel(dev, i);

	for (i = 0; i < DPRX_NUM_MST; i++)
	if (!dev->ch[i])
		dprx_info(dev, "Not all of the MST ports are configured\n");

	for (i = 0; i < DPRX_NUM_MST; i++)
		dev->stream_enabled[i] = 0;

	dev->sequence = 0;
	mutex_init(&dev->mutex_dprx_dma_reg);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "Failed to register v4l2_device: %d\n",
									ret);
		return ret;
	}

	v4l2_subdev_init(sd, &dprx_ip_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "DPRX-IP.%d", dev->id);

	v4l2_ctrl_handler_init(handler, 7);

	v4l2_ctrl_new_custom(handler, &dprx_ip_no_of_lane, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_lane_speed, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_tps_pattern, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_hdcp_enable, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_dsc_enable, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_mst_config, NULL);
	v4l2_ctrl_new_custom(handler, &dprx_ip_power_state_ctrl, NULL);

	sd->ctrl_handler = handler;

	v4l2_set_subdevdata(sd, dev);

	/* Register subdev to control DPRX_IP part */
	ret = v4l2_device_register_subdev(&dev->v4l2_dev, sd);
	if (ret) {
		v4l2_err(sd->v4l2_dev, "Failed to register %s\n",
							sd->name);
		goto unregister_v4l2_dev;
	}

	dev->notifier.subdevs = dev->asd_list;
	dev->notifier.bound = dprx_async_bound;
	dev->notifier.complete = dprx_async_complete;
	ret = v4l2_async_notifier_register(&dev->v4l2_dev,
					   &dev->notifier);
	if (ret) {
		dprx_err(dev, "Error registering async notifier\n");
		ret = -EINVAL;
		goto unregister_subdev;
	}
	pm_runtime_enable(&pdev->dev);

	dev->dprx_ip_is_on = false;
	ret = dprx_runtime_get(dev);
	if (ret)
		goto runtime_disable;

	/* Load the HDCP key and firmware */
	dprx_load_hdcp_firmware(dev);

	dprx_runtime_put(dev);

	dev->link_rate = 1;
	dev->lane_count = 1;

	spin_lock_init(&dev->dma_en_vid_lock);

	return 0;

runtime_disable:
	pm_runtime_disable(&pdev->dev);
	v4l2_async_notifier_unregister(&dev->notifier);
unregister_subdev:
	v4l2_device_unregister_subdev(sd);
unregister_v4l2_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
	return ret;
}

static int dprx_remove(struct platform_device *pdev)
{
	struct dprx_dev *dev =
		(struct dprx_dev *)platform_get_drvdata(pdev);
	struct dprx_channel *ch;
	int i;

	dprx_dbg(1, dev, "Removing %s\n", DPRX_MODULE_NAME);

	for (i = 0; i < dev->dprx_num_clock; i++)
		if (dev->dp_clk[i])
			clk_disable_unprepare(dev->dp_clk[i]);

	dprx_runtime_get(dev);

	v4l2_async_notifier_unregister(&dev->notifier);

	for (i = 0; i < DPRX_NUM_MST; i++) {
		ch = dev->ch[i];
		if (ch) {
			ch_dbg(1, ch, "unregistering %s\n",
				video_device_node_name(&ch->vdev));
			video_unregister_device(&ch->vdev);
		}
	}

	v4l2_device_unregister_subdev(&dev->subdev);
	v4l2_device_unregister(&dev->v4l2_dev);

	dprx_runtime_put(dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id dprx_of_match[] = {
	{ .compatible = "trav-dprx", },
	{},
};
MODULE_DEVICE_TABLE(of, dprx_of_match);
#endif

static struct platform_driver dprx_pdrv = {
	.probe		= dprx_probe,
	.remove		= dprx_remove,
	.driver		= {
		.name	= DPRX_MODULE_NAME,
		.of_match_table = of_match_ptr(dprx_of_match),
	},
};

module_platform_driver(dprx_pdrv);
