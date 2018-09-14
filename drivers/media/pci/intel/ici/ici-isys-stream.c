// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include "./ici/ici-isys.h"
#ifdef ICI_ENABLED

#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/sched/types.h>
#include "isysapi/interface/ia_css_isysapi_fw_types.h"
#include "isysapi/interface/ia_css_isysapi.h"
#include <ia_css_pkg_dir.h>
#include <ia_css_pkg_dir_iunit.h>
#include <ia_css_pkg_dir_types.h>
#include "ipu-trace.h"
#include "ipu-fw-isys.h"
#include "ipu-wrapper.h"
#include "./ici/ici-isys-stream.h"
#include "./ici/ici-isys-csi2.h"
#include "./ici/ici-isys-csi2-be.h"
#include <media/ici.h>

#ifndef IPU4_DEBUG
#define IPU4_DEBUG 1
#endif

#define dev_to_stream(dev) \
	container_of(dev, struct ici_isys_stream, strm_dev)

const struct ici_isys_pixelformat ici_isys_pfmts[] = {
	/* YUV vector format */
	{ ICI_FORMAT_YUYV, 24, 24, ICI_FORMAT_YUYV, IA_CSS_ISYS_FRAME_FORMAT_YUV420_16 },
	/* Raw bayer vector formats. */
	{ ICI_FORMAT_SBGGR12, 16, 12, ICI_FORMAT_SBGGR12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGBRG12, 16, 12, ICI_FORMAT_SGBRG12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGRBG12, 16, 12, ICI_FORMAT_SGRBG12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SRGGB12, 16, 12, ICI_FORMAT_SRGGB12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SBGGR10, 16, 10, ICI_FORMAT_SBGGR10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGBRG10, 16, 10, ICI_FORMAT_SGBRG10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGRBG10, 16, 10, ICI_FORMAT_SGRBG10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SRGGB10, 16, 10, ICI_FORMAT_SRGGB10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SBGGR8, 16, 8, ICI_FORMAT_SBGGR8, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGBRG8, 16, 8, ICI_FORMAT_SGBRG8, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGRBG8, 16, 8, ICI_FORMAT_SGRBG8, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SRGGB8, 16, 8, ICI_FORMAT_SRGGB8, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	/*{ V4L2_FMT_INTEL_IPU4_ISYS_META, 8, 8, MEDIA_BUS_FMT_FIXED, IA_CSS_ISYS_MIPI_DATA_TYPE_EMBEDDED },*/
	{ }
};

const struct ici_isys_pixelformat ici_isys_pfmts_be_soc[] = {
	{ ICI_FORMAT_UYVY, 16, 16, ICI_FORMAT_UYVY, IA_CSS_ISYS_FRAME_FORMAT_UYVY },
	{ ICI_FORMAT_YUYV, 16, 16, ICI_FORMAT_YUYV, IA_CSS_ISYS_FRAME_FORMAT_YUYV },
	{ ICI_FORMAT_RGB565, 32, 32, ICI_FORMAT_RGB565, IA_CSS_ISYS_FRAME_FORMAT_RGBA888 },
	{ ICI_FORMAT_RGB888, 32, 32, ICI_FORMAT_RGB888, IA_CSS_ISYS_FRAME_FORMAT_RGBA888 },
	/* Raw bayer formats. */
	{ ICI_FORMAT_SBGGR12, 16, 12, ICI_FORMAT_SBGGR12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGBRG12, 16, 12, ICI_FORMAT_SGBRG12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGRBG12, 16, 12, ICI_FORMAT_SGRBG12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SRGGB12, 16, 12, ICI_FORMAT_SRGGB12, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SBGGR10, 16, 10, ICI_FORMAT_SBGGR10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGBRG10, 16, 10, ICI_FORMAT_SGBRG10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SGRBG10, 16, 10, ICI_FORMAT_SGRBG10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SRGGB10, 16, 10, ICI_FORMAT_SRGGB10, IA_CSS_ISYS_FRAME_FORMAT_RAW16 },
	{ ICI_FORMAT_SBGGR8, 8, 8, ICI_FORMAT_SBGGR8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
	{ ICI_FORMAT_SGBRG8, 8, 8, ICI_FORMAT_SGBRG8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
	{ ICI_FORMAT_SGRBG8, 8, 8, ICI_FORMAT_SGRBG8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
	{ ICI_FORMAT_SRGGB8, 8, 8, ICI_FORMAT_SRGGB8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
	{ }
};

const struct ici_isys_pixelformat ici_isys_pfmts_packed[] = {
        { ICI_FORMAT_UYVY, 16, 16, ICI_FORMAT_UYVY, IA_CSS_ISYS_FRAME_FORMAT_UYVY },
        { ICI_FORMAT_YUYV, 16, 16, ICI_FORMAT_YUYV, IA_CSS_ISYS_FRAME_FORMAT_YUYV },
        { ICI_FORMAT_RGB565, 16, 16, ICI_FORMAT_RGB565, IA_CSS_ISYS_FRAME_FORMAT_RGB565 },
        { ICI_FORMAT_RGB888, 24, 24, ICI_FORMAT_RGB888, IA_CSS_ISYS_FRAME_FORMAT_RGBA888 },
        { ICI_FORMAT_SBGGR12, 12, 12, ICI_FORMAT_SBGGR12, IA_CSS_ISYS_FRAME_FORMAT_RAW12 },
        { ICI_FORMAT_SGBRG12, 12, 12, ICI_FORMAT_SGBRG12, IA_CSS_ISYS_FRAME_FORMAT_RAW12 },
        { ICI_FORMAT_SGRBG12, 12, 12, ICI_FORMAT_SGRBG12, IA_CSS_ISYS_FRAME_FORMAT_RAW12 },
        { ICI_FORMAT_SRGGB12, 12, 12, ICI_FORMAT_SRGGB12, IA_CSS_ISYS_FRAME_FORMAT_RAW12 },
        { ICI_FORMAT_SBGGR10, 10, 10, ICI_FORMAT_SBGGR10, IA_CSS_ISYS_FRAME_FORMAT_RAW10 },
        { ICI_FORMAT_SGBRG10, 10, 10, ICI_FORMAT_SGBRG10, IA_CSS_ISYS_FRAME_FORMAT_RAW10 },
        { ICI_FORMAT_SGRBG10, 10, 10, ICI_FORMAT_SGRBG10, IA_CSS_ISYS_FRAME_FORMAT_RAW10 },
        { ICI_FORMAT_SRGGB10, 10, 10, ICI_FORMAT_SRGGB10, IA_CSS_ISYS_FRAME_FORMAT_RAW10 },
        { ICI_FORMAT_SBGGR8, 8, 8, ICI_FORMAT_SBGGR8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
        { ICI_FORMAT_SGBRG8, 8, 8, ICI_FORMAT_SGBRG8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
        { ICI_FORMAT_SGRBG8, 8, 8, ICI_FORMAT_SGRBG8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
        { ICI_FORMAT_SRGGB8, 8, 8, ICI_FORMAT_SRGGB8, IA_CSS_ISYS_FRAME_FORMAT_RAW8 },
        { }
};

static int node_set_format(struct ici_isys_node *node,
	struct ici_pad_framefmt* pff)
{
	if (node->node_set_pad_ffmt)
		return node->node_set_pad_ffmt(node, pff);
	return 0;
}

struct pipeline_format_data {
	struct ici_isys_stream *as;
	struct ici_pad_framefmt pff;
};

static int set_pipeline_node_format(void* cb_data,
	struct ici_isys_node* node,
	struct node_pipe* pipe)
{
	int ret;
	struct pipeline_format_data* fmt_data = cb_data;
	struct ici_isys_stream *as = fmt_data->as;
	dev_info(&as->isys->adev->dev,
		"Set ext sd \"%s\" format %d, width %d, height %d\n",
		node->name,
		fmt_data->pff.ffmt.pixelformat,
		fmt_data->pff.ffmt.width,
		fmt_data->pff.ffmt.height);
	fmt_data->pff.pad.pad_idx = 0;
	ret = node_set_format(node, &fmt_data->pff);
	if (ret < 0)
		return ret;
	if (node->nr_pads > 1) {
		fmt_data->pff.pad.pad_idx = 1;
		ret = node_set_format(node, &fmt_data->pff);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int set_pipeline_format(struct ici_isys_stream *as,
	struct ici_framefmt* ff)
{
	struct pipeline_format_data fmt_data = {
		.as = as,
		.pff.pad.pad_idx = 0,
		.pff.ffmt = *ff
	};

	return ici_isys_pipeline_for_each_node(
		set_pipeline_node_format,
		&fmt_data,
		&as->node,
		&as->ip,
		true);
}

struct pipeline_power_data {
	struct ici_isys_stream *as;
	int power;
};

static int pipeline_set_node_power(void* cb_data,
	struct ici_isys_node* node,
	struct node_pipe* pipe)
{
	struct pipeline_power_data* pwr_data = cb_data;
	struct ici_isys_stream *as = pwr_data->as;
	dev_info(&as->isys->adev->dev,
		"Set ext sd \"%s\" power to %d\n",
		node->name, pwr_data->power);
	if (node->node_set_power) {
		int ret = node->node_set_power(node, pwr_data->power);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int pipeline_set_power(struct ici_isys_stream *as,
	int state)
{
	struct pipeline_power_data pwr_data = {
		.as = as,
		.power = state,
	};

	return ici_isys_pipeline_for_each_node(
			pipeline_set_node_power,
			&pwr_data,
			&as->node,
			&as->ip,
			true);
}

static int intel_ipu4_isys_library_close(struct ici_isys *isys)
{
	struct device *dev = &isys->adev->dev;
	int timeout = IPU_ISYS_TURNOFF_TIMEOUT;
	int rval;

	/*
	 * Ask library to stop the isys fw. Actual close takes
	 * some time as the FW must stop its actions including code fetch
	 * to SP icache.
	*/
	rval = ipu_lib_call(device_close, isys);
	if (rval)
		dev_err(dev, "Device close failure: %d\n", rval);

	/* release probably fails if the close failed. Let's try still */
	do {
		usleep_range(IPU_ISYS_TURNOFF_DELAY_US,
			2 * IPU_ISYS_TURNOFF_DELAY_US);
		rval = ipu_lib_call_notrace(device_release, isys, 0);
		timeout--;
	} while (rval != 0 && timeout);

	if (!rval)
		isys->fwcom = NULL; /* No further actions needed */
	else
		dev_err(dev, "Device release time out %d\n", rval);
	return rval;
}

static unsigned int get_comp_format(u32 code)
{
	unsigned int predictor = 0; /* currently hard coded */
	unsigned int udt = ici_isys_format_code_to_mipi(code);
	unsigned int scheme = ici_isys_get_compression_scheme(code);

	/* if data type is not user defined return here */
	if ((udt < ICI_ISYS_MIPI_CSI2_TYPE_USER_DEF(1))
		 || (udt > ICI_ISYS_MIPI_CSI2_TYPE_USER_DEF(8)))
		return 0;

	/*
	 * For each user defined type (1..8) there is configuration bitfield for
	 * decompression.
	 *
	 * | bit 3     | bits 2:0 |
	 * | predictor | scheme   |
	 * compression schemes:
	 * 000 = no compression
	 * 001 = 10 - 6 - 10
	 * 010 = 10 - 7 - 10
	 * 011 = 10 - 8 - 10
	 * 100 = 12 - 6 - 12
	 * 101 = 12 - 7 - 12
	 * 110 = 12 - 8 - 12
	 */

	return ((predictor << 3) | scheme) <<
		 ((udt - ICI_ISYS_MIPI_CSI2_TYPE_USER_DEF(1)) * 4);
}

static void csi_short_packet_prepare_firmware_stream_cfg_ici(
	struct ici_isys_pipeline *ip,
	struct ia_css_isys_stream_cfg_data *cfg)
{
	struct ici_isys_stream *as =
		ici_pipeline_to_stream(ip);
	struct ici_isys_frame_buf_list *buf_list =
		&as->buf_list;
	int input_pin = cfg->nof_input_pins++;
	int output_pin = cfg->nof_output_pins++;
	struct ia_css_isys_input_pin_info *input_info =
		&cfg->input_pins[input_pin];
	struct ia_css_isys_output_pin_info *output_info =
		&cfg->output_pins[output_pin];

	/*
	 * Setting dt as ICI_ISYS_SHORT_PACKET_GENERAL_DT will cause
	 * MIPI receiver to receive all MIPI short packets.
	 */
	input_info->dt = ICI_ISYS_SHORT_PACKET_GENERAL_DT;
	input_info->input_res.width = ICI_ISYS_SHORT_PACKET_WIDTH;
	input_info->input_res.height = buf_list->num_short_packet_lines;

	ip->output_pins[output_pin].pin_ready =
		ici_isys_frame_short_packet_ready;
	ip->output_pins[output_pin].buf_list = buf_list;
	buf_list->short_packet_output_pin = output_pin;

	output_info->input_pin_id = input_pin;
	output_info->output_res.width = ICI_ISYS_SHORT_PACKET_WIDTH;
	output_info->output_res.height = buf_list->num_short_packet_lines;
	output_info->stride = ICI_ISYS_SHORT_PACKET_WIDTH *
			      ICI_ISYS_SHORT_PACKET_UNITSIZE;
	output_info->pt = ICI_ISYS_SHORT_PACKET_PT;
	output_info->ft = ICI_ISYS_SHORT_PACKET_FT;
	output_info->send_irq = 1;
}

static int csi_short_packet_configure_tunit(
    struct ici_isys_pipeline *ip,
    bool enable)
{
	struct ici_isys *isys = ip->isys;
	void __iomem *isys_base = isys->pdata->base;
	void __iomem *tunit_base = isys_base + TRACE_REG_IS_TRACE_UNIT_BASE;
	void __iomem *csi2_tm_base;
	void __iomem *event_mask_reg;
	unsigned int trace_addr;
	int rval;
	int i;

	if (ip->csi2->index >= IPU_ISYS_MAX_CSI2_LEGACY_PORTS) {
		csi2_tm_base = isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE;
		trace_addr = TRACE_REG_CSI2_3PH_TM_TRACE_ADDRESS_VAL;
		event_mask_reg = csi2_tm_base +
			TRACE_REG_CSI2_3PH_TM_TRACE_DDR_EN_REG_IDX_P(
			ip->csi2->index);
	} else {
		csi2_tm_base = isys->pdata->base + TRACE_REG_CSI2_TM_BASE;
		trace_addr = TRACE_REG_CSI2_TM_TRACE_ADDRESS_VAL;
		event_mask_reg = csi2_tm_base +
			TRACE_REG_CSI2_TM_TRACE_DDR_EN_REG_IDX_P(
			ip->csi2->index);
	}

	if (!enable) {
		writel(0, event_mask_reg);
		writel(0, csi2_tm_base +
			TRACE_REG_CSI2_TM_OVERALL_ENABLE_REG_IDX);
		writel(0, tunit_base + TRACE_REG_TUN_DDR_ENABLE);
		return 0;
	}

	/* ring buffer base */
	writel(isys->short_packet_trace_buffer_dma_addr,
        tunit_base + TRACE_REG_TUN_DRAM_BASE_ADDR);

	/* ring buffer end */
	writel(isys->short_packet_trace_buffer_dma_addr +
        IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE -
        IPU_ISYS_SHORT_PACKET_TRACE_MSG_SIZE,
        tunit_base + TRACE_REG_TUN_DRAM_END_ADDR);

	/* Infobits for ddr trace */
	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
        tunit_base + TRACE_REG_TUN_DDR_INFO_VAL);

	/* Remove reset from trace timers */
	writel(TRACE_REG_GPREG_TRACE_TIMER_RST_OFF,
		isys_base + TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N);

	/* Reset CSI2 monitor */
	writel(1, csi2_tm_base + TRACE_REG_CSI2_TM_RESET_REG_IDX);

	/* Set trace address register. */
	writel(trace_addr, csi2_tm_base +
		TRACE_REG_CSI2_TM_TRACE_ADDRESS_REG_IDX);
	writel(TRACE_REG_CSI2_TM_TRACE_HEADER_VAL, csi2_tm_base +
		TRACE_REG_CSI2_TM_TRACE_HEADER_REG_IDX);

	/* Enable DDR trace. */
	writel(1, tunit_base + TRACE_REG_TUN_DDR_ENABLE);

	/* Enable trace for CSI2 port. */
#if 0
	reg_val = readl(event_mask_reg);
	reg_val |= TRACE_REG_CSI2_TM_EVENT_FS(ip->vc);
	writel(reg_val, event_mask_reg);
#else
	for (i = 0; i < IPU_ISYS_MAX_CSI2_LEGACY_PORTS +
	     IPU_ISYS_MAX_CSI2_COMBO_PORTS; i++) {
		void __iomem *event_mask_reg =
			(i < IPU_ISYS_MAX_CSI2_LEGACY_PORTS) ?
			isys->pdata->base + TRACE_REG_CSI2_TM_BASE +
			TRACE_REG_CSI2_TM_TRACE_DDR_EN_REG_IDX_P(i) :
			isys->pdata->base + TRACE_REG_CSI2_3PH_TM_BASE +
			TRACE_REG_CSI2_3PH_TM_TRACE_DDR_EN_REG_IDX_P(i);

		writel(IPU_ISYS_SHORT_PACKET_TRACE_EVENT_MASK,
			event_mask_reg);
	}
#endif
	/* Enable CSI2 receiver monitor */
	writel(1, csi2_tm_base + TRACE_REG_CSI2_TM_OVERALL_ENABLE_REG_IDX);

	rval = ipu_buttress_tsc_read(isys->adev->isp,
		&isys->tsc_timer_base);
	if (rval) {
		dev_err(&isys->adev->dev,
			"Failed to read TSC timer.\n");
		return rval;
	}
	rval = ipu_trace_get_timer(&isys->adev->dev,
		&isys->tunit_timer_base);
	if (rval) {
		dev_err(&isys->adev->dev,
			"Failed to read Tunit timer.\n");
		return rval;
	}

	return 0;
}

static void get_stream_opened(struct ici_isys_stream *as)
{
	unsigned long flags;

	spin_lock_irqsave(&as->isys->lock, flags);
	as->isys->stream_opened++;
	spin_unlock_irqrestore(&as->isys->lock, flags);
}

static void put_stream_opened(struct ici_isys_stream *as)
{
	unsigned long flags;

	spin_lock_irqsave(&as->isys->lock, flags);
	as->isys->stream_opened--;
	spin_unlock_irqrestore(&as->isys->lock, flags);
}

static int get_stream_handle(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	unsigned int stream_handle;
	unsigned long flags;

	spin_lock_irqsave(&as->isys->lock, flags);
	for (stream_handle = 0;
		stream_handle < INTEL_IPU4_ISYS_MAX_STREAMS; stream_handle++)
		if (as->isys->ici_pipes[stream_handle] == NULL)
			break;
	if (stream_handle == INTEL_IPU4_ISYS_MAX_STREAMS) {
		spin_unlock_irqrestore(&as->isys->lock, flags);
		return -EBUSY;
	}
	as->isys->ici_pipes[stream_handle] = ip;
	ip->stream_handle = stream_handle;
	spin_unlock_irqrestore(&as->isys->lock, flags);
	return 0;
}

static void put_stream_handle(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	unsigned long flags;

	spin_lock_irqsave(&as->isys->lock, flags);
	as->isys->ici_pipes[ip->stream_handle] = NULL;
	ip->stream_handle = -1;
	spin_unlock_irqrestore(&as->isys->lock, flags);
}

/* Create stream and start it using the CSS library API. */
static int start_stream_firmware(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	struct device *dev = &as->isys->adev->dev;
	struct ia_css_isys_stream_cfg_data stream_cfg = {
		.src = ip->source,
		.vc = 0,
		.isl_use = ICI_ISL_OFF,
		.nof_input_pins = 1,
	};
	struct ia_css_isys_frame_buff_set css_buf;
	struct ici_pad_framefmt source_fmt = {
		.pad.pad_idx = ip->asd_source_pad_id,
		.ffmt = {0},

	};
	struct ici_isys_node* be_csi2_node = NULL;

	int rval, rvalout, tout, i;

	rval = ip->asd_source->node.node_get_pad_ffmt(
		&ip->asd_source->node, &source_fmt);
	if (rval)
		return rval;
	stream_cfg.compfmt = get_comp_format(source_fmt.ffmt.pixelformat);
	stream_cfg.input_pins[0].input_res.width = source_fmt.ffmt.width;
	stream_cfg.input_pins[0].input_res.height = source_fmt.ffmt.height;
	stream_cfg.input_pins[0].dt =
		ici_isys_format_code_to_mipi(source_fmt.ffmt.pixelformat);

	/*
	 * Only CSI2-BE has the capability to do crop,
	 * so get the crop info from csi2-be.
	 */
	stream_cfg.crop[0].right_offset = source_fmt.ffmt.width;
	stream_cfg.crop[0].bottom_offset = source_fmt.ffmt.height;
	if (ip->csi2_be) {
		struct ici_pad_selection ps = {
			.pad.pad_idx = CSI2_BE_ICI_PAD_SOURCE,
			.rect = {0},
		};
		be_csi2_node = &ip->csi2_be->asd.node;
		if (be_csi2_node->node_get_pad_sel)
			rval = be_csi2_node->node_get_pad_sel(
				be_csi2_node, &ps);
		else
			rval = -ENODEV;
		if (!rval) {
			stream_cfg.crop[0].left_offset = ps.rect.left;
			stream_cfg.crop[0].top_offset = ps.rect.top;
			stream_cfg.crop[0].right_offset = ps.rect.left +
				ps.rect.width;
			stream_cfg.crop[0].bottom_offset = ps.rect.top +
				ps.rect.height;
		}
	}

	as->prepare_firmware_stream_cfg(as, &stream_cfg);

	if (ip->interlaced) {
		if (ip->short_packet_source ==
		    IPU_ISYS_SHORT_PACKET_FROM_RECEIVER)
			csi_short_packet_prepare_firmware_stream_cfg_ici(ip,
				&stream_cfg);
		else
			csi_short_packet_configure_tunit(ip, 1);
	}

//	csslib_dump_isys_stream_cfg(dev, &stream_cfg); //TODO implement corresponding function to dump command input to FW

	ip->nr_output_pins = stream_cfg.nof_output_pins;

	rval = get_stream_handle(as);
	if (rval) {
		dev_dbg(dev, "Can't get stream_handle\n");
		return rval;
	}

	reinit_completion(&ip->stream_open_completion);
/* SKTODO: Debug start */
	printk("SKTODO: My stream open\n");
	printk("ia_css_isys_stream_source src = %d\n", stream_cfg.src);
	printk("ia_css_isys_mipi_vc vc = %d\n", stream_cfg.vc);
	printk("ia_css_isys_isl_use isl_use = %d\n", stream_cfg.isl_use);
	printk("compfmt = %u\n", stream_cfg.compfmt);
	printk("struct ia_css_isys_isa_cfg isa_cfg");
	for ( i = 0 ; i < N_IA_CSS_ISYS_CROPPING_LOCATION ; i++ ) {
		printk("crop[%d].top_offset = %d\n", i, stream_cfg.crop[i].top_offset);
		printk("crop[%d].left_offset = %d\n", i, stream_cfg.crop[i].left_offset);
		printk("crop[%d].bottom_offset = %d\n", i, stream_cfg.crop[i].bottom_offset);
		printk("crop[%d].right_offset = %d\n", i, stream_cfg.crop[i].right_offset);
	}
	printk("send_irq_sof_discarded = %u\n", stream_cfg.send_irq_sof_discarded);
	printk("send_irq_eof_discarded = %u\n", stream_cfg.send_irq_eof_discarded);
	printk("send_resp_sof_discarded = %u\n", stream_cfg.send_resp_sof_discarded);
	printk("send_resp_eof_discarded = %u\n", stream_cfg.send_resp_eof_discarded);
	printk("nof_input_pins = %u\n", stream_cfg.nof_input_pins);
	printk("nof_output_pins = %u\n", stream_cfg.nof_output_pins);
	for (i = 0 ; i < stream_cfg.nof_input_pins ; i++) {
		printk("input_pins[%d].input_res.width = %u\n", i, stream_cfg.input_pins[i].input_res.width);
		printk("input_pins[%d].input_res.height = %u\n", i, stream_cfg.input_pins[i].input_res.height);
		printk("input_pins[%d].dt = %d\n", i, stream_cfg.input_pins[i].dt);
		printk("input_pins[%d].mipi_store_mode = %d\n", i, stream_cfg.input_pins[i].mipi_store_mode);
	}
	for (i = 0 ; i < stream_cfg.nof_output_pins ; i++) {
		printk("output_pins[%d].input_pin_id = %u\n", i, stream_cfg.output_pins[i].input_pin_id);
		printk("output_pins[%d].output_res.width = %u\n", i, stream_cfg.output_pins[i].output_res.width);
		printk("output_pins[%d].output_res.height = %u\n", i, stream_cfg.output_pins[i].output_res.height);
		printk("output_pins[%d].stride = %u\n", i, stream_cfg.output_pins[i].stride);
		printk("output_pins[%d].pt = %d\n", i, stream_cfg.output_pins[i].pt);
		printk("output_pins[%d].ft = %d\n", i, stream_cfg.output_pins[i].ft);
		printk("output_pins[%d].watermark_in_lines = %u\n", i, stream_cfg.output_pins[i].watermark_in_lines);
		printk("output_pins[%d].send_irq = %u\n", i, stream_cfg.output_pins[i].send_irq);
	}
/* SKTODO: Debug end */
	rval = ipu_lib_call(stream_open, as->isys, ip->stream_handle, &stream_cfg);
	if (rval < 0) {
		dev_err(dev, "can't open stream (%d)\n", rval);
		goto out_put_stream_handle;
	}
	get_stream_opened(as);

	tout = wait_for_completion_timeout(&ip->stream_open_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout) {
		dev_err(dev, "stream open time out\n");
		rval = -ETIMEDOUT;
		goto out_put_stream_opened;
	}
	if (ip->error) {
		dev_err(dev, "stream open error: %d\n", ip->error);
		rval = -EIO;
		goto out_put_stream_opened;
	}
	dev_dbg(dev, "start stream: open complete\n");

	rval = ici_isys_frame_buf_add_next(as, &css_buf);
	if (rval) {
		dev_err(dev, "no buffers for streaming (%d)\n", rval);
		goto out_stream_close;
	}
//TODO implement corresponding function to dump command input to FW
//	csslib_dump_isys_frame_buff_set(dev, &css_buf,
//		stream_cfg.nof_output_pins);

	reinit_completion(&ip->stream_start_completion);
	rval = ipu_lib_call(stream_start, as->isys, ip->stream_handle,
				   &css_buf);
	if (rval < 0) {
		dev_err(dev, "can't start streaming (%d)\n", rval);
		goto out_stream_close;
	}

	tout = wait_for_completion_timeout(&ip->stream_start_completion,
					   IPU_LIB_CALL_TIMEOUT_JIFFIES);
	if (!tout) {
		dev_err(dev, "stream start time out\n");
		rval = -ETIMEDOUT;
		goto out_stream_close;
	}
	if (ip->error) {
		dev_err(dev, "stream start error: %d\n", ip->error);
		rval = -EIO;
		goto out_stream_close;
	}
	dev_dbg(dev, "start stream: complete\n");

	return 0;

out_stream_close:
	reinit_completion(&ip->stream_close_completion);

	rvalout = ipu_lib_call(stream_close, as->isys, ip->stream_handle);
	if (rvalout < 0) {
		dev_dbg(dev, "can't close stream (%d)\n", rvalout);
	} else {
		tout = wait_for_completion_timeout(&ip->stream_close_completion,
				IPU_LIB_CALL_TIMEOUT_JIFFIES);
		if (!tout)
			dev_err(dev, "stream close time out\n");
		else if (ip->error)
			dev_err(dev, "stream close error: %d\n", ip->error);
		else
			dev_dbg(dev, "stream close complete\n");
	}

out_put_stream_opened:
	put_stream_opened(as);

out_put_stream_handle:
	put_stream_handle(as);
	return rval;
}

static void stop_streaming_firmware(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	struct device *dev = &as->isys->adev->dev;
	int rval, tout;

	reinit_completion(&ip->stream_stop_completion);
	rval = ipu_lib_call(stream_flush, as->isys, ip->stream_handle);
	if (rval < 0) {
		dev_err(dev, "can't stop stream (%d)\n", rval);
	} else {
		tout = wait_for_completion_timeout(&ip->stream_stop_completion,
				IPU_LIB_CALL_TIMEOUT_JIFFIES);
		if (!tout)
			dev_err(dev, "stream stop time out\n");
		else if (ip->error)
			dev_err(dev, "stream stop error: %d\n", ip->error);
		else
			dev_dbg(dev, "stop stream: complete\n");
	}
	if (ip->interlaced && ip->short_packet_source ==
	    IPU_ISYS_SHORT_PACKET_FROM_TUNIT)
		csi_short_packet_configure_tunit(ip, 0);
}

static void close_streaming_firmware(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	struct device *dev = &as->isys->adev->dev;
	int rval, tout;

	reinit_completion(&ip->stream_close_completion);
	rval = ipu_lib_call(stream_close, as->isys, ip->stream_handle);
	if (rval < 0) {
		dev_err(dev, "can't close stream (%d)\n", rval);
	} else {
		tout = wait_for_completion_timeout(&ip->stream_close_completion,
				IPU_LIB_CALL_TIMEOUT_JIFFIES);
		if (!tout)
			dev_err(dev, "stream close time out\n");
		else if (ip->error)
			dev_err(dev, "stream close error: %d\n", ip->error);
		else
			dev_dbg(dev, "close stream: complete\n");
	}
	put_stream_opened(as);
	put_stream_handle(as);
}

void ici_isys_stream_add_capture_done(
	struct ici_isys_pipeline* ip,
	void (*capture_done)(struct ici_isys_pipeline* ip,
		struct ia_css_isys_resp_info* resp))
{
	unsigned int i;

	/* Different instances may register same function. Add only once */
	for (i = 0; i < ICI_NUM_CAPTURE_DONE ; i++)
		if (ip->capture_done[i] == capture_done)
			return;

	for (i = 0; i < ICI_NUM_CAPTURE_DONE ; i++) {
		if (ip->capture_done[i] == NULL) {
			ip->capture_done[i] = capture_done;
			return;
		}
	}
	/*
	 * Too many call backs registered. Change to INTEL_IPU4_NUM_CAPTURE_DONE
	 * constant probably required.
	 */
	BUG();
}

void ici_isys_prepare_firmware_stream_cfg_default(
	struct ici_isys_stream *as,
	struct ia_css_isys_stream_cfg_data *cfg)
{
	struct ici_isys_pipeline *ip = &as->ip;

	struct ici_isys_frame_buf_list *bl = &as->buf_list;

	struct ia_css_isys_output_pin_info *pin_info;
	int pin = cfg->nof_output_pins++;

	bl->fw_output = pin;
	ip->output_pins[pin].pin_ready = ici_isys_frame_buf_ready;
	ip->output_pins[pin].buf_list = bl;

	pin_info = &cfg->output_pins[pin];
	pin_info->input_pin_id = 0;
	pin_info->output_res.width = as->strm_format.ffmt.width;
	pin_info->output_res.height = as->strm_format.ffmt.height;
	pin_info->stride = as->strm_format.pfmt.plane_fmt[0].bytesperline;
	pin_info->pt = bl->css_pin_type;
	pin_info->ft = as->pfmt->css_pixelformat;
	pin_info->send_irq = 1;
	cfg->vc = ip->vc;
}

static int pipeline_validate_node(void* cb_data,
	struct ici_isys_node* src_node,
	struct node_pipe* pipe)
{
	int rval;
	struct ici_isys_pipeline *ip = cb_data;

	dev_err(&ip->pipeline_dev->dev, "Validating node %s\n",
		src_node->name);
	if (src_node->node_pipeline_validate) {
		rval = src_node->node_pipeline_validate(&ip->pipe,
			src_node);
		if (rval)
			return rval;
	}
	if (pipe) {
		struct ici_isys_node* sink_node =
			pipe->sink_pad->node;
		struct ici_pad_framefmt src_format = {
			.pad.pad_idx = pipe->src_pad->pad_id,
		};
		struct ici_pad_framefmt sink_format = {
			.pad.pad_idx = pipe->sink_pad->pad_id,
		};
		if (src_node->node_get_pad_ffmt) {
			rval = src_node->node_get_pad_ffmt(src_node,
				&src_format);
			if (rval)
				return rval;
		}
		if (sink_node->node_get_pad_ffmt) {
			rval = sink_node->node_get_pad_ffmt(sink_node,
				&sink_format);
			if (rval)
				return rval;
		}
		if (src_format.ffmt.width != sink_format.ffmt.width ||
			src_format.ffmt.height != sink_format.ffmt.height ||
			src_format.ffmt.pixelformat != sink_format.ffmt.pixelformat ||
			src_format.ffmt.field != sink_format.ffmt.field ||
			src_format.ffmt.colorspace != sink_format.ffmt.colorspace) {
			dev_err(&ip->pipeline_dev->dev, "Formats don't match node (%d:%d) -> node (%d:%d)\n",
				src_node->node_id, src_format.pad.pad_idx,
				sink_node->node_id, sink_format.pad.pad_idx);
			return -EINVAL;
		}
	}
	return 0;
}

static int pipeline_validate(
	struct ici_isys_node *node,
	struct ici_isys_pipeline *ip)
{
	return ici_isys_pipeline_for_each_node(
		pipeline_validate_node,
		ip,
		node,
		ip,
		true);
}

struct set_streaming_data {
	struct ici_isys_pipeline *ip;
	bool external;
	int state;
};

static int set_streaming_node(void* cb_data,
	struct ici_isys_node* node,
	struct node_pipe* pipe)
{
	struct set_streaming_data* data = cb_data;
	if (data->external != node->external)
		return 0;

	if (node->node_set_streaming)
		return node->node_set_streaming(node, data->ip,
			data->state);
	return 0;
}


static int set_streaming(struct ici_isys_node *node,
	struct ici_isys_pipeline *ip,
	bool external,
	int state)
{
	struct set_streaming_data data = {
		.ip = ip,
		.external = external,
		.state = state
	};
	return ici_isys_pipeline_for_each_node(
		set_streaming_node,
		&data,
		node,
		ip,
		true);
}

static int ici_isys_set_streaming(
					struct ici_isys_stream *as,
					unsigned int state)
{
	struct ici_isys_pipeline *ip = &as->ip;
	int rval = 0;

	dev_dbg(&as->isys->adev->dev, "set stream (intel_stream%d): %d\n", state,
		as->strm_dev.minor);

	if (!state) {
		stop_streaming_firmware(as);

		/* stop external sub-device now. */
		if (ip->csi2) {
			ici_isys_csi2_wait_last_eof(ip->csi2);
		}

		set_streaming(&as->node, ip, true, 0);
	}

	rval = set_streaming(&as->node, ip, false, state);
	if (rval)
		goto out_stop_streaming;

	if (state) {
		rval = start_stream_firmware(as);
		if (rval) {
			goto out_stop_streaming;
		}
		dev_dbg(&ip->isys->adev->dev, "set stream: source %d, stream_handle %d\n",
				ip->source, ip->stream_handle);

		/* Start external sub-device now. */
		rval = set_streaming(&as->node, ip, true, state);
		if (rval)
			goto out_stop_streaming_firmware;

	} else {
		close_streaming_firmware(as);
	}

	ip->streaming = state;
	return 0;

out_stop_streaming_firmware:
	stop_streaming_firmware(as);

out_stop_streaming:
	set_streaming(&as->node, ip, false, 0);
	return rval;
}

static void stream_buffers(struct ici_isys_stream *as)
{
	struct ici_isys_pipeline *ip = &as->ip;
	struct ia_css_isys_frame_buff_set set = {};
	int rval;

	for (;;) {
		rval = ici_isys_frame_buf_add_next(as, &set);
		if (rval) {
			break;
		}
//TODO implement corresponding function to dump command input to FW
//		csslib_dump_isys_frame_buff_set(&as->isys->adev->dev, &set,
//						ip->nr_output_pins);
		WARN_ON(ipu_lib_call(
				stream_capture_indication, as->isys,
				ip->stream_handle, &set) < 0);
	}
}

static int ici_isys_stream_on(struct file *file, void *fh)
{
	struct ici_isys_stream *as =
		dev_to_stream(file->private_data);
	struct ici_isys_pipeline *ip = &as->ip;
	int rval, i;

	dev_dbg(&as->isys->adev->dev,
		"stream_on: %u\n", as->strm_dev.minor);

	if (ip->streaming) {
		dev_dbg(&as->isys->adev->dev,
			"Already streaming\n");
		return 0;
	}

	ip->csi2 = NULL;
	ip->csi2_be = NULL;
	ip->asd_source = NULL;
	ip->asd_source_pad_id = 0;
	rval = pipeline_validate(&as->node, ip);
	if (rval)
		return rval;

	if (!ip->asd_source) {
		dev_err(&ip->isys->adev->dev, "set stream: Pipeline does not have a source\n");
		return -ENODEV;
	}

	pipeline_set_power(as, 1);

	mutex_lock(&as->isys->stream_mutex);
	ip->source = ip->asd_source->source;

	for (i = 0; i < ICI_NUM_CAPTURE_DONE; i++)
		ip->capture_done[i] = NULL;

	if (ip->interlaced) {
		pr_err("** SKTODO: INTERLACE ENABLED **\n");
	    if (ip->short_packet_source ==
            IPU_ISYS_SHORT_PACKET_FROM_RECEIVER) {
		    rval = ici_isys_frame_buf_short_packet_setup(
			    as, &as->strm_format);
		    if (rval)
			    goto out_requeue;
	    } else {
		    memset(ip->isys->short_packet_trace_buffer, 0,
			    IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE);
            dma_sync_single_for_device(&as->isys->adev->dev,
                as->isys->short_packet_trace_buffer_dma_addr,
                IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
                DMA_BIDIRECTIONAL);
            ip->short_packet_trace_index = 0;
        }
	}

	rval = ici_isys_set_streaming(as, 1);
	if (rval)
		goto out_cleanup_short_packet;

	ip->streaming = 1;

	dev_dbg(&as->isys->adev->dev, "dispatching queued requests\n");
	stream_buffers(as);
	dev_dbg(&as->isys->adev->dev,
		"done dispatching queued requests\n");

	mutex_unlock(&as->isys->stream_mutex);

	return 0;

out_cleanup_short_packet:
	ici_isys_frame_buf_short_packet_destroy(as);

out_requeue:
	ici_isys_frame_buf_stream_cancel(as);
	mutex_unlock(&as->isys->stream_mutex);
	pipeline_set_power(as, 0);
	return rval;
}

static int ici_isys_stream_off(struct file *file, void *fh)
{
	struct ici_isys_stream *as =
		dev_to_stream(file->private_data);
	struct ici_isys_pipeline *ip = &as->ip;

	mutex_lock(&as->isys->stream_mutex);
	if (ip->streaming)
		ici_isys_set_streaming(as, 0);

	ip->streaming = 0;
	ici_isys_frame_buf_short_packet_destroy(as);
	mutex_unlock(&as->isys->stream_mutex);

	ici_isys_frame_buf_stream_cancel(as);
	pipeline_set_power(as, 0);
	return 0;
}

const struct ici_isys_pixelformat
*ici_isys_get_pixelformat(
	struct ici_isys_stream *as, unsigned int pixelformat)
{
	const struct ici_isys_pixelformat *pfmt;
	unsigned pad;
	const unsigned *supported_codes;

	pad = as->pad.pad_id;
	supported_codes = as->asd->supported_codes[pad];

	for (pfmt = as->pfmts; pfmt->bpp; pfmt++) {
		unsigned int i;

		if (pfmt->code != pixelformat)
			continue;

		for (i = 0; supported_codes[i]; i++) {
			if (pfmt->code == supported_codes[i])
				return pfmt;
		}
	}

	/* Not found. Get the default, i.e. the first defined one. */
	for (pfmt = as->pfmts; pfmt->bpp; pfmt++) {
		if (pfmt->code == *supported_codes)
			return pfmt;
	}

	BUG();
}

const struct ici_isys_pixelformat
*ici_isys_video_try_fmt_vid_mplane_default(
	struct ici_isys_stream *as,
	struct ici_stream_format *mpix)
{
	const struct ici_isys_pixelformat *pfmt =
		ici_isys_get_pixelformat(as, mpix->ffmt.pixelformat);

	mpix->ffmt.pixelformat = pfmt->pixelformat;
	mpix->pfmt.num_planes = 1;

	if (!as->packed)
		mpix->pfmt.plane_fmt[0].bytesperline =
			 mpix->ffmt.width * DIV_ROUND_UP(pfmt->bpp,
			 BITS_PER_BYTE);
	else
		mpix->pfmt.plane_fmt[0].bytesperline = DIV_ROUND_UP(
			as->line_header_length + as->line_footer_length
			+ (unsigned int)mpix->ffmt.width * pfmt->bpp,
			BITS_PER_BYTE);

	mpix->pfmt.plane_fmt[0].bytesperline =
		ALIGN(mpix->pfmt.plane_fmt[0].bytesperline,
		as->isys->line_align);
	mpix->pfmt.plane_fmt[0].bpp = pfmt->bpp;

	 /*
	 * (height + 1) * bytesperline due to a hardware issue: the DMA unit
	 * is a power of two, and a line should be transferred as few units
	 * as possible. The result is that up to line length more data than
	 * the image size may be transferred to memory after the image.
	 * Another limition is the GDA allocation unit size. For low
	 * resolution it gives a bigger number. Use larger one to avoid
	 * memory corruption.
	 */
	mpix->pfmt.plane_fmt[0].sizeimage =
		max(max(mpix->pfmt.plane_fmt[0].sizeimage,
			mpix->pfmt.plane_fmt[0].bytesperline *
			mpix->ffmt.height +
			max(mpix->pfmt.plane_fmt[0].bytesperline,
			as->isys->pdata->ipdata->isys_dma_overshoot)),
			1U);

	if (mpix->ffmt.field == ICI_FIELD_ANY)
		mpix->ffmt.field = ICI_FIELD_NONE;

	return pfmt;
}

static int ici_s_fmt_vid_cap_mplane(
	struct ici_isys_stream *as,
	struct ici_stream_format *f)
{
	if (as->ip.streaming)
		return -EBUSY;

	as->pfmt = as->try_fmt_vid_mplane(as, f);
	as->strm_format = *f;

	return 0;
}

/**
 * Returns true if device does not support real interrupts and
 * polling must be used.
 */
static int ici_poll_for_events(
	struct ici_isys_stream *as)
{
//	return is_intel_ipu_hw_fpga();
	return 0;
}

static void ipu_cleanup_fw_msg_bufs(struct ici_isys *isys)
{
        struct isys_fw_msgs *fwmsg, *fwmsg0;
        unsigned long flags;

        spin_lock_irqsave(&isys->listlock, flags);
        list_for_each_entry_safe(fwmsg, fwmsg0, &isys->framebuflist_fw, head)
                list_move(&fwmsg->head, &isys->framebuflist);
        spin_unlock_irqrestore(&isys->listlock, flags);
}

static int stream_fop_open(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev =
		inode_to_intel_ipu_stream_device(inode);
	struct ici_isys_stream* as = dev_to_stream(strm_dev);
	struct ici_isys *isys = as->isys;
	struct ipu_bus_device *adev =
		to_ipu_bus_device(&isys->adev->dev);
	struct ipu_device *isp = adev->isp;
	int rval;
	DEBUGK("%s: stream open (%p)\n", __func__, as);

	mutex_lock(&isys->mutex);
	if (isys->reset_needed) {
		mutex_unlock(&isys->mutex);
		dev_warn(&isys->adev->dev, "isys power cycle required\n");
		return -EIO;
	}
	mutex_unlock(&isys->mutex);

	rval = ipu_buttress_authenticate(isp);
	if (rval) {
		dev_err(&isys->adev->dev, "FW authentication failed\n");
		return rval;
	}

	rval = pm_runtime_get_sync(&isys->adev->dev);
	if (rval < 0) {
		pm_runtime_put_noidle(&isys->adev->dev);
		return rval;
	}

	mutex_lock(&isys->mutex);

	ipu_configure_spc(adev->isp,
				 &isys->pdata->ipdata->hw_variant,
				 IA_CSS_PKG_DIR_ISYS_INDEX,
				 isys->pdata->base, isys->pkg_dir,
				 isys->pkg_dir_dma_addr);


	if (isys->ici_stream_opened++) {
		/* Already open */
		mutex_unlock(&isys->mutex);
		return 0;
	}

        ipu_cleanup_fw_msg_bufs(isys);

	if (isys->fwcom) {
		/*
		 * Something went wrong in previous shutdown. As we are now
		 * restarting isys we can safely delete old context.
		 */
		ipu_fw_isys_cleanup(isys);
		isys->fwcom = NULL;
	}

	if (ici_poll_for_events(as)) {
		static const struct sched_param param = {
			.sched_priority = MAX_USER_RT_PRIO/2,
		};

		isys->isr_thread = kthread_run(
			intel_ipu4_isys_isr_run_ici, as->isys,
			IPU_ISYS_ENTITY_PREFIX);

		if (IS_ERR(isys->isr_thread)) {
			rval = PTR_ERR(isys->isr_thread);
			goto out_intel_ipu4_pipeline_pm_use;
		}

		sched_setscheduler(isys->isr_thread, SCHED_FIFO, &param);
	}

	rval = ipu_fw_isys_init(as->isys, IPU_ISYS_NUM_STREAMS);
	if (rval < 0)
		goto out_lib_init;

	mutex_unlock(&isys->mutex);

	return 0;

out_lib_init:
	if (ici_poll_for_events(as))
		kthread_stop(isys->isr_thread);

out_intel_ipu4_pipeline_pm_use:
	isys->ici_stream_opened--;
	mutex_unlock(&isys->mutex);
	pm_runtime_put(&isys->adev->dev);

	return rval;
}

static int stream_fop_release(struct inode *inode, struct file *file)
{
	struct ici_stream_device *strm_dev =
		inode_to_intel_ipu_stream_device(inode);
	struct ici_isys_stream* as = dev_to_stream(strm_dev);
	int ret = 0;
	DEBUGK("%s: stream release (%p)\n", __func__, as);

	if (as->ip.streaming) {
		ici_isys_stream_off(file, NULL);
	}

	mutex_lock(&as->isys->mutex);

	if (!--as->isys->ici_stream_opened) {
		if (ici_poll_for_events(as))
			kthread_stop(as->isys->isr_thread);

		intel_ipu4_isys_library_close(as->isys);
		if (as->isys->fwcom) {
			as->isys->reset_needed = true;
			ret = -EIO;
		}
	}

	mutex_unlock(&as->isys->mutex);

	pm_runtime_put(&as->isys->adev->dev);
	return ret;
}

static unsigned int stream_fop_poll(struct file *file,
	struct poll_table_struct *poll)
{
	struct ici_isys_stream *as =
		dev_to_stream(file->private_data);
	struct ici_isys *isys = as->isys;
	unsigned int res = 0;

	dev_dbg(&isys->adev->dev, "stream_fop_poll\n");

	poll_wait(file, &as->buf_list.wait, poll);

	if (!list_empty(&as->buf_list.putbuf_list))
		res = POLLIN;

	dev_dbg(&isys->adev->dev, "stream_fop_poll res %u\n", res);

	return res;
}

static int ici_isys_set_format(struct file *file, void *fh,
	struct ici_stream_format *sf)
{
	int rval;
	struct ici_isys_stream *as =
		dev_to_stream(file->private_data);
	struct ici_isys *isys = as->isys;

	DEBUGK("%s: ici stream set format (%p)\n \
		width: %u, height: %u, pixelformat: %u, field: %u, colorspace: %u\n",
			__func__, as,
			sf->ffmt.width,
			sf->ffmt.height,
			sf->ffmt.pixelformat,
			sf->ffmt.field,
			sf->ffmt.colorspace);

	if (sf->ffmt.field == ICI_FIELD_ALTERNATE) {
		DEBUGK("Interlaced enabled\n");
		as->ip.interlaced = true;
		as->ip.short_packet_source = 1;
	} else {
		as->ip.interlaced = false;
	}

	rval = ici_s_fmt_vid_cap_mplane(as, sf);
	if (rval) {
		dev_err(&isys->adev->dev, "failed to set format (vid_cap) %d\n", rval);
		return rval;
	}
	if (sf->pfmt.num_planes != 1) {
		dev_err(&isys->adev->dev, "Invalid num of planes %d\n",
			sf->pfmt.num_planes);
		return rval;
	}
	if (!sf->pfmt.plane_fmt[0].sizeimage) {
		dev_err(&isys->adev->dev, "Zero image size for plane 0\n");
		return rval;
	}

	rval = set_pipeline_format(as, &sf->ffmt);
	if (rval) {
		dev_err(&isys->adev->dev,
			"failed to set format on pipeline %d\n", rval);
		return rval;
	}
	return 0;
}

static int ici_isys_getbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	int rval = 0;
	struct ici_isys_stream *as = dev_to_stream(
		file->private_data);
	struct ici_isys *isys = as->isys;

	//DEBUGK("%s: ici stream getbuf (%p)\n", __func__, as);
	rval = ici_isys_get_buf(as, user_frame_info);
	if(rval) {
		dev_err(&isys->adev->dev, "failed to get buffer %d\n", rval);
		return rval;
	}

	mutex_lock(&as->isys->stream_mutex);
	if (as->ip.streaming) {
		stream_buffers(as);
	}
	mutex_unlock(&as->isys->stream_mutex);
	return 0;
}

static int ici_isys_getbuf_virt(struct file *file, void *fh,
	struct ici_frame_buf_wrapper *user_frame_buf, struct page **pages)
{
	int rval = 0;
	struct ici_isys_stream *as = dev_to_stream(
		file->private_data);
	struct ici_isys *isys = as->isys;

	rval = ici_isys_get_buf_virt(as, user_frame_buf, pages);
	if (rval) {
		dev_err(&isys->adev->dev, "failed to get buffer %d\n", rval);
		return rval;
	}

	mutex_lock(&as->isys->stream_mutex);
	if (as->ip.streaming) {
		stream_buffers(as);
	}
	mutex_unlock(&as->isys->stream_mutex);
	return 0;
}

static int ici_isys_putbuf(struct file *file, void *fh,
	struct ici_frame_info *user_frame_info)
{
	int rval = 0;
	struct ici_isys_stream *as = dev_to_stream(file->private_data);
	struct ici_isys *isys = as->isys;
	//DEBUGK("%s: ici stream putbuf (%p)\n", __func__, as);
	rval = ici_isys_put_buf(as, user_frame_info,
		file->f_flags);
	if(rval) {
		dev_err(&isys->adev->dev, "failed to put buffer %d\n", rval);
		return rval;
	}
	return 0;
}

static int ici_isys_stream_get_ffmt(
	struct ici_isys_node* node,
	struct ici_pad_framefmt* pff)
{
	struct ici_isys_stream *as = node->sd;
	if (pff->pad.pad_idx != 0) {
		dev_err(&as->isys->adev->dev,
			"Stream only has pad 0\n");
		return -EINVAL;
	}
	pff->ffmt = as->strm_format.ffmt;
	return 0;
}

static const struct ici_ioctl_ops ioctl_ops_mplane_ici = {
	.ici_set_format = ici_isys_set_format,
	.ici_stream_on = ici_isys_stream_on,
	.ici_stream_off = ici_isys_stream_off,
	.ici_get_buf = ici_isys_getbuf,
	.ici_get_buf_virt = ici_isys_getbuf_virt,
	.ici_put_buf = ici_isys_putbuf,
};

static const struct file_operations ipu4_isys_ici_stream_fops = {
	.owner = THIS_MODULE,
	.poll = stream_fop_poll,
	.open = stream_fop_open,
	.release = stream_fop_release,
};

int ici_isys_stream_init(
				struct ici_isys_stream *as,
				struct ici_isys_subdev *asd,
				struct ici_isys_node *node,
				unsigned int pad,
				unsigned long pad_flags)
{
	int rval;
	char name[ICI_MAX_NODE_NAME];

	mutex_init(&as->mutex);
	init_completion(&as->ip.stream_open_completion);
	init_completion(&as->ip.stream_close_completion);
	init_completion(&as->ip.stream_start_completion);
	init_completion(&as->ip.stream_stop_completion);
	init_completion(&as->ip.capture_ack_completion);
	as->ip.isys = as->isys;
	as->ip.pipeline_dev = &as->isys->pipeline_dev;
	as->asd = asd;

	as->strm_dev.ipu_ioctl_ops = &ioctl_ops_mplane_ici;

	ici_isys_frame_buf_init(&as->buf_list);

	as->pad.flags = pad_flags | ICI_PAD_FLAGS_MUST_CONNECT;
	snprintf(name, sizeof(name),
		 "%s Stream", asd->node.name);
	rval = ici_isys_pipeline_node_init(as->isys,
		&as->node, name, 1, &as->pad);
	if (rval)
		goto out_init_fail;

	if (__ici_isys_subdev_get_ffmt(asd, pad))
		as->strm_format.ffmt =
			*__ici_isys_subdev_get_ffmt(asd, pad);

	as->node.sd = as;
	as->node.pipe = &as->ip.pipe;
	as->node.node_get_pad_ffmt =
		ici_isys_stream_get_ffmt;

	asd->node.pipe = &as->ip.pipe;
	/*asd->node.ops = &entity_ops;*/
	as->strm_dev.fops = &ipu4_isys_ici_stream_fops;

	as->strm_dev.frame_buf_list = &as->buf_list;
	as->strm_dev.mutex = &as->mutex;
	as->strm_dev.dev_parent = &as->isys->adev->dev;
	dev_set_drvdata(&as->strm_dev.dev, as);

	mutex_lock(&as->mutex);

	rval = stream_device_register(&as->strm_dev);
	if (rval)
		goto out_mutex_unlock;

	if (pad_flags & ICI_PAD_FLAGS_SINK)
		rval = node_pad_create_link(
			node, pad, &as->node, 0, 0);
	else if (pad_flags & ICI_PAD_FLAGS_SOURCE)
		rval = node_pad_create_link(
			&as->node, 0, node, pad, 0);

	if (rval) {
		printk(KERN_WARNING "can't create link\n");
		goto out_mutex_unlock;
	}

	mutex_unlock(&as->mutex);

	return rval;

out_mutex_unlock:
	mutex_unlock(&as->mutex);
	node_pads_cleanup(&as->asd->node);
	//intel_ipu4_isys_framebuf_cleanup(&as->buf_list);
out_init_fail:
	mutex_destroy(&as->mutex);

	return rval;
}

void ici_isys_stream_cleanup(struct ici_isys_stream *as)
{
	list_del(&as->node.node_entry);
	stream_device_unregister(&as->strm_dev);
	node_pads_cleanup(&as->asd->node);
	mutex_destroy(&as->mutex);
	//intel_ipu4_isys_framebuf_cleanup(&as->buf_list);
}

#endif //ICI_ENABLED

