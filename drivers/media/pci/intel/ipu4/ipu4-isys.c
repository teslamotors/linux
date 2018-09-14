// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/module.h>

#include "ipu.h"
#include "ipu-platform-regs.h"
#include "ipu-platform-buttress-regs.h"
#include "ipu-trace.h"
#include "ipu-isys.h"
#include "ipu-isys-video.h"
#include "ipu-isys-tpg.h"

#ifndef V4L2_PIX_FMT_SBGGR14V32
/*
 * Non-vectorized 14bit definitions have been upstreamed.
 * To keep various versions of the ipu4 builds compileable use local
 * definitions when global one's doesn't exists.
 */
#define V4L2_PIX_FMT_SBGGR14V32         v4l2_fourcc('b', 'V', '0', 'M')
#define V4L2_PIX_FMT_SGBRG14V32         v4l2_fourcc('b', 'V', '0', 'N')
#define V4L2_PIX_FMT_SGRBG14V32         v4l2_fourcc('b', 'V', '0', 'O')
#define V4L2_PIX_FMT_SRGGB14V32         v4l2_fourcc('b', 'V', '0', 'P')
#endif

const struct ipu_isys_pixelformat ipu_isys_pfmts[] = {
	/* YUV vector format */
	{V4L2_PIX_FMT_YUYV420_V32, 24, 24, 0, MEDIA_BUS_FMT_YUYV12_1X24,
	 IPU_FW_ISYS_FRAME_FORMAT_YUV420_16},
	/* Raw bayer vector formats. */
	{V4L2_PIX_FMT_SBGGR14V32, 16, 14, 0, MEDIA_BUS_FMT_SBGGR14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG14V32, 16, 14, 0, MEDIA_BUS_FMT_SGBRG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG14V32, 16, 14, 0, MEDIA_BUS_FMT_SGRBG14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB14V32, 16, 14, 0, MEDIA_BUS_FMT_SRGGB14_1X14,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR12V32, 16, 12, 0, MEDIA_BUS_FMT_SBGGR12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG12V32, 16, 12, 0, MEDIA_BUS_FMT_SGBRG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG12V32, 16, 12, 0, MEDIA_BUS_FMT_SGRBG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB12V32, 16, 12, 0, MEDIA_BUS_FMT_SRGGB12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR10V32, 16, 10, 0, MEDIA_BUS_FMT_SBGGR10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG10V32, 16, 10, 0, MEDIA_BUS_FMT_SGBRG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG10V32, 16, 10, 0, MEDIA_BUS_FMT_SGRBG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB10V32, 16, 10, 0, MEDIA_BUS_FMT_SRGGB10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR8_16V32, 16, 8, 0, MEDIA_BUS_FMT_SBGGR8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG8_16V32, 16, 8, 0, MEDIA_BUS_FMT_SGBRG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG8_16V32, 16, 8, 0, MEDIA_BUS_FMT_SGRBG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB8_16V32, 16, 8, 0, MEDIA_BUS_FMT_SRGGB8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_FMT_IPU_ISYS_META, 8, 8, 0, MEDIA_BUS_FMT_FIXED,
	 IPU_FW_ISYS_MIPI_DATA_TYPE_EMBEDDED},
	{}
};

struct ipu_trace_block isys_trace_blocks[] = {
	{
	 .offset = TRACE_REG_IS_TRACE_UNIT_BASE,
	 .type = IPU_TRACE_BLOCK_TUN,
	},
	{
	 .offset = TRACE_REG_IS_SP_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_IS_SP_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_IS_ISL_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_IS_MMU_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_CSI2_TM_BASE,
	 .type = IPU_TRACE_CSI2,
	},
	{
	 .offset = TRACE_REG_CSI2_3PH_TM_BASE,
	 .type = IPU_TRACE_CSI2_3PH,
	},
	{
	 /* Note! this covers all 9 blocks */
	 .offset = TRACE_REG_CSI2_SIG2SIO_GR_BASE(0),
	 .type = IPU_TRACE_SIG2CIOS,
	},
	{
	 /* Note! this covers all 9 blocks */
	 .offset = TRACE_REG_CSI2_PH3_SIG2SIO_GR_BASE(0),
	 .type = IPU_TRACE_SIG2CIOS,
	},
	{
	 .offset = TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N,
	 .type = IPU_TRACE_TIMER_RST,
	},
	{
	 .type = IPU_TRACE_BLOCK_END,
	}
};

#ifdef CONFIG_VIDEO_INTEL_IPU4
void isys_setup_hw(struct ipu_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	const u8 *thd = isys->pdata->ipdata->hw_variant.cdc_fifo_threshold;
	u32 irqs;
	unsigned int i;

	/* Enable irqs for all MIPI busses */
	irqs = IPU_ISYS_UNISPART_IRQ_CSI2(0) |
	    IPU_ISYS_UNISPART_IRQ_CSI2(1) |
	    IPU_ISYS_UNISPART_IRQ_CSI2(2) |
	    IPU_ISYS_UNISPART_IRQ_CSI2(3) |
	    IPU_ISYS_UNISPART_IRQ_CSI2(4) | IPU_ISYS_UNISPART_IRQ_CSI2(5);

	irqs |= IPU_ISYS_UNISPART_IRQ_SW;

	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_EDGE);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_MASK);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_ENABLE);

	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);

	/* Write CDC FIFO threshold values for isys */
	for (i = 0; i < isys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(thd[i], base + IPU_REG_ISYS_CDC_THRESHOLD(i));
}
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4P
static void ipu4p_isys_irq_cfg(struct ipu_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	int i, j;
	struct {
		u32 base;
		u32 mask;
	} irq_config[] = {
		{IPU_REG_ISYS_UNISPART_IRQ_EDGE, 0x400018},
		{IPU_REG_ISYS_ISA_ACC_IRQ_CTRL_BASE, 0x0},
		{IPU_REG_ISYS_A_IRQ_CTRL_BASE, 0x0},
		{IPU_REG_ISYS_SIP0_IRQ_CTRL_BASE, 0xf},
		{IPU_REG_ISYS_SIP1_IRQ_CTRL_BASE, 0xf},
	};
	unsigned int offsets[4] = {
		0x0, 0x4, 0x10, 0x14
	};

	for (i = 0; i < ARRAY_SIZE(irq_config); i++) {
		for (j = 0; j < ARRAY_SIZE(offsets); j++)
			writel(irq_config[i].mask,
				   base + irq_config[i].base + offsets[j]);
		writel(0xffffffff, base + irq_config[i].base + 0xc);
	}

	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);
}

static void ipu4p_isys_bb_cfg(struct ipu_isys *isys)
{
	void __iomem *isp_base = isys->adev->isp->base;
	unsigned int i, val;
	unsigned int bbconfig[4][3] = {
		{4, 15, 0xf},
		{6, 15, 0x15},
		{12, 15, 0xf},
		{14, 15, 0x15},
	};

	/* Config building block */
	for (i = 0; i < 4; i++) {
		unsigned int bb = bbconfig[i][0];
		unsigned int crc = bbconfig[i][1];
		unsigned int afe = bbconfig[i][2];

		val = readl(isp_base + BUTTRESS_REG_CPHYX_DLL_OVRD(bb));
		val &= ~0x7e;
		val |= crc << 1;
		val |= 1;
		writel(val, isp_base + BUTTRESS_REG_CPHYX_DLL_OVRD(bb));
		val = readl(isp_base + BUTTRESS_REG_DPHYX_DLL_OVRD(bb));
		val |= 1;
		writel(val, isp_base + BUTTRESS_REG_DPHYX_DLL_OVRD(bb));
		val &= ~1;
		writel(val, isp_base + BUTTRESS_REG_DPHYX_DLL_OVRD(bb));
		val = afe | (2 << 29);
		writel(val, isp_base + BUTTRESS_REG_BBX_AFE_CONFIG(bb));
	}
}

static void ipu4p_isys_port_cfg(struct ipu_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	void __iomem *isp_base = isys->adev->isp->base;

	/* Port config */
	writel(0x3895, base + IPU_GPOFFSET + 0x14);
	writel(0x3895, base + IPU_COMBO_GPOFFSET + 0x14);
	writel((0x100 << 1) | (0x100 << 10) | (0x100 << 19), isp_base +
		   BUTTRESS_REG_CSI_BSCAN_EXCLUDE);
}

void isys_setup_hw(struct ipu_isys *isys)
{
	ipu4p_isys_irq_cfg(isys);
	ipu4p_isys_port_cfg(isys);
	ipu4p_isys_bb_cfg(isys);
}
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4
irqreturn_t isys_isr(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status;

	spin_lock(&isys->power_lock);
	if (!isys->power) {
		spin_unlock(&isys->power_lock);
		return IRQ_NONE;
	}

	status = readl(isys->pdata->base +
			   IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	do {
		writel(status, isys->pdata->base +
			   IPU_REG_ISYS_UNISPART_IRQ_CLEAR);

		if (isys->isr_csi2_bits & status) {
			unsigned int i;

			for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
				if (IPU_ISYS_UNISPART_IRQ_CSI2(i) & status)
					ipu_isys_csi2_isr(&isys->csi2[i]);
			}
		}

		writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);

		/*
		 * Handle a single FW event per checking the CSI-2
		 * receiver SOF status. This is done in order to avoid
		 * the case where events arrive to the event queue and
		 * one of them is a SOF event which then could be
		 * handled before the SOF interrupt. This would pose
		 * issues in sequence numbering which is based on SOF
		 * interrupts, always assumed to arrive before FW SOF
		 * events.
		 */
		if (status & IPU_ISYS_UNISPART_IRQ_SW && !isys_isr_one(adev))
			status = IPU_ISYS_UNISPART_IRQ_SW;
		else
			status = 0;

		status |= readl(isys->pdata->base +
				    IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (status & (isys->isr_csi2_bits
			   | IPU_ISYS_UNISPART_IRQ_SW) &&
		 !isys->adev->isp->flr_done);
	spin_unlock(&isys->power_lock);

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4P
irqreturn_t isys_isr(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status;
	unsigned int i;
	u32 sip0_status, sip1_status;
	struct {
		u32 *status;
		u32 mask;
	} csi2_irq_mask[] = {
		{&sip0_status, IPU_ISYS_CSI2_D_IRQ_MASK},
		{&sip1_status, IPU_ISYS_CSI2_A_IRQ_MASK},
		{&sip1_status, IPU_ISYS_CSI2_B_IRQ_MASK},
		{&sip1_status, IPU_ISYS_CSI2_C_IRQ_MASK},
		{&sip1_status, IPU_ISYS_CSI2_D_IRQ_MASK},
	};

	spin_lock(&isys->power_lock);
	if (!isys->power) {
		spin_unlock(&isys->power_lock);
		return IRQ_NONE;
	}

	/* read unis sw irq */
	status = readl(isys->pdata->base +
			   IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	dev_dbg(&adev->dev, "isys irq status - unis sw irq = 0x%x", status);

	do {
		/* clear unis sw irqs */
		writel(status, isys->pdata->base +
			   IPU_REG_ISYS_UNISPART_IRQ_CLEAR);

		/* read and clear sip irq status */
		sip0_status = readl(isys->pdata->base +
					IPU_REG_ISYS_SIP0_IRQ_CTRL_STATUS);
		sip1_status = readl(isys->pdata->base +
					IPU_REG_ISYS_SIP1_IRQ_CTRL_STATUS);
		dev_dbg(&adev->dev, "isys irq status - sip0 = 0x%x sip1 = 0x%x",
			sip0_status, sip1_status);
		writel(sip0_status, isys->pdata->base +
			   IPU_REG_ISYS_SIP0_IRQ_CTRL_CLEAR);
		writel(sip1_status, isys->pdata->base +
			   IPU_REG_ISYS_SIP1_IRQ_CTRL_CLEAR);

		for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
			if (*csi2_irq_mask[i].status & csi2_irq_mask[i].mask)
				ipu_isys_csi2_isr(&isys->csi2[i]);
		}

		writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);

		/*
		 * Handle a single FW event per checking the CSI-2
		 * receiver SOF status. This is done in order to avoid
		 * the case where events arrive to the event queue and
		 * one of them is a SOF event which then could be
		 * handled before the SOF interrupt. This would pose
		 * issues in sequence numbering which is based on SOF
		 * interrupts, always assumed to arrive before FW SOF
		 * events.
		 */
		if (status & IPU_ISYS_UNISPART_IRQ_SW && !isys_isr_one(adev))
			status = IPU_ISYS_UNISPART_IRQ_SW;
		else
			status = 0;

		status |= readl(isys->pdata->base +
				    IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (status & (isys->isr_csi2_bits
			   | IPU_ISYS_UNISPART_IRQ_SW) &&
		 !isys->adev->isp->flr_done);
	spin_unlock(&isys->power_lock);

	return IRQ_HANDLED;
}
#endif

int tpg_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ipu_isys_tpg *tpg = to_ipu_isys_tpg(sd);
	__u32 code = tpg->asd.ffmt[TPG_PAD_SOURCE][0].code;
	unsigned int bpp = ipu_isys_mbus_code_to_bpp(code);

	/*
	 * MIPI_GEN block is CSI2 FB. Need to enable/disable TPG selection
	 * register to control the TPG streaming.
	 */
	if (tpg->sel)
		writel(enable ? 1 : 0, tpg->sel);

	if (!enable) {
		writel(0, tpg->base + MIPI_GEN_REG_COM_ENABLE);
		return 0;
	}

	writel(MIPI_GEN_COM_DTYPE_RAW(bpp),
		   tpg->base + MIPI_GEN_REG_COM_DTYPE);
	writel(ipu_isys_mbus_code_to_mipi(code),
		   tpg->base + MIPI_GEN_REG_COM_VTYPE);
	writel(0, tpg->base + MIPI_GEN_REG_COM_VCHAN);

	writel(0, tpg->base + MIPI_GEN_REG_SYNG_NOF_FRAMES);

	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE][0].width *
				bpp, BITS_PER_BYTE),
		   tpg->base + MIPI_GEN_REG_COM_WCOUNT);
	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE][0].width,
				MIPI_GEN_PPC),
		   tpg->base + MIPI_GEN_REG_SYNG_NOF_PIXELS);
	writel(tpg->asd.ffmt[TPG_PAD_SOURCE][0].height,
		   tpg->base + MIPI_GEN_REG_SYNG_NOF_LINES);

	writel(0, tpg->base + MIPI_GEN_REG_TPG_MODE);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_HCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_VCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_XYCNT_MASK);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_HCNT_DELTA);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_VCNT_DELTA);

	v4l2_ctrl_handler_setup(&tpg->asd.ctrl_handler);

	writel(2, tpg->base + MIPI_GEN_REG_COM_ENABLE);
	return 0;
}
