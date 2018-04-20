// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <asm/cacheflush.h>

#include <linux/kernel.h>
#include <linux/delay.h>
#include "ipu-platform-regs.h"
#include "ipu-fw-isys.h"
#include "ipu-fw-com.h"
#include "ipu-isys.h"

#define IPU_FW_UNSUPPORTED_DATA_TYPE	0
static const uint32_t
extracted_bits_per_pixel_per_mipi_data_type[N_IPU_FW_ISYS_MIPI_DATA_TYPE] = {

	64,	/* [0x00]   IPU_FW_ISYS_MIPI_DATA_TYPE_FRAME_START_CODE */
	64,	/* [0x01]   IPU_FW_ISYS_MIPI_DATA_TYPE_FRAME_END_CODE */
	64,	/* [0x02]   IPU_FW_ISYS_MIPI_DATA_TYPE_LINE_START_CODE */
	64,	/* [0x03]   IPU_FW_ISYS_MIPI_DATA_TYPE_LINE_END_CODE */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x04] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x05] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x06] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x07] */
	64,	/* [0x08]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT1 */
	64,	/* [0x09]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT2 */
	64,	/* [0x0A]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT3 */
	64,	/* [0x0B]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT4 */
	64,	/* [0x0C]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT5 */
	64,	/* [0x0D]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT6 */
	64,	/* [0x0E]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT7 */
	64,	/* [0x0F]   IPU_FW_ISYS_MIPI_DATA_TYPE_GENERIC_SHORT8 */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x10] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x11] */
	8,	/* [0x12]    IPU_FW_ISYS_MIPI_DATA_TYPE_EMBEDDED */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x13] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x14] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x15] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x16] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x17] */
	12,	/* [0x18]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV420_8 */
	15,	/* [0x19]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV420_10 */
	12,	/* [0x1A]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV420_8_LEGACY */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x1B] */
	12,	/* [0x1C]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV420_8_SHIFT */
	15,	/* [0x1D]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV420_10_SHIFT */
	16,	/* [0x1E]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV422_8 */
	20,	/* [0x1F]   IPU_FW_ISYS_MIPI_DATA_TYPE_YUV422_10 */
	16,	/* [0x20]   IPU_FW_ISYS_MIPI_DATA_TYPE_RGB_444 */
	16,	/* [0x21]   IPU_FW_ISYS_MIPI_DATA_TYPE_RGB_555 */
	16,	/* [0x22]   IPU_FW_ISYS_MIPI_DATA_TYPE_RGB_565 */
	18,	/* [0x23]   IPU_FW_ISYS_MIPI_DATA_TYPE_RGB_666 */
	24,	/* [0x24]   IPU_FW_ISYS_MIPI_DATA_TYPE_RGB_888 */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x25] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x26] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x27] */
	6,	/* [0x28]    IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_6 */
	7,	/* [0x29]    IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_7 */
	8,	/* [0x2A]    IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_8 */
	10,	/* [0x2B]   IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_10 */
	12,	/* [0x2C]   IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_12 */
	14,	/* [0x2D]   IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_14 */
	16,	/* [0x2E]   IPU_FW_ISYS_MIPI_DATA_TYPE_RAW_16 */
	8,	/* [0x2F]    IPU_FW_ISYS_MIPI_DATA_TYPE_BINARY_8 */
	8,	/* [0x30]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF1 */
	8,	/* [0x31]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF2 */
	8,	/* [0x32]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF3 */
	8,	/* [0x33]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF4 */
	8,	/* [0x34]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF5 */
	8,	/* [0x35]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF6 */
	8,	/* [0x36]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF7 */
	8,	/* [0x37]    IPU_FW_ISYS_MIPI_DATA_TYPE_USER_DEF8 */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x38] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x39] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x3A] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x3B] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x3C] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x3D] */
	IPU_FW_UNSUPPORTED_DATA_TYPE,	/* [0x3E] */
	IPU_FW_UNSUPPORTED_DATA_TYPE	/* [0x3F] */
};

const char send_msg_types[N_IPU_FW_ISYS_SEND_TYPE][32] = {
	"STREAM_OPEN",
	"STREAM_START",
	"STREAM_START_AND_CAPTURE",
	"STREAM_CAPTURE",
	"STREAM_STOP",
	"STREAM_FLUSH",
	"STREAM_CLOSE"
};


void ipu_fw_isys_set_params(struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg)
{
	unsigned int i;
	unsigned int idx;

	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		idx = stream_cfg->input_pins[i].dt;
		stream_cfg->input_pins[i].bits_per_pix =
		    extracted_bits_per_pixel_per_mipi_data_type[idx];
		stream_cfg->input_pins[i].mapped_dt =
		    N_IPU_FW_ISYS_MIPI_DATA_TYPE;
	}
}

void
ipu_fw_isys_dump_stream_cfg(struct device *dev,
			    struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg)
{
	unsigned int i;

	dev_dbg(dev, "---------------------------\n");
	dev_dbg(dev, "IPU_FW_ISYS_STREAM_CFG_DATA\n");
	dev_dbg(dev, "---------------------------\n");

	dev_dbg(dev, "Source %d\n", stream_cfg->src);
	dev_dbg(dev, "VC %d\n", stream_cfg->vc);
	dev_dbg(dev, "Nof input pins %d\n", stream_cfg->nof_input_pins);
	dev_dbg(dev, "Nof output pins %d\n", stream_cfg->nof_output_pins);

	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		dev_dbg(dev, "Input pin %d\n", i);
		dev_dbg(dev, "Mipi data type 0x%0x\n",
			stream_cfg->input_pins[i].dt);
		dev_dbg(dev, "Mipi store mode %d\n",
			stream_cfg->input_pins[i].mipi_store_mode);
		dev_dbg(dev, "Bits per pixel %d\n",
			stream_cfg->input_pins[i].bits_per_pix);
		dev_dbg(dev, "Mapped data type 0x%0x\n",
			stream_cfg->input_pins[i].mapped_dt);
		dev_dbg(dev, "Input res width %d\n",
			stream_cfg->input_pins[i].input_res.width);
		dev_dbg(dev, "Input res height %d\n",
			stream_cfg->input_pins[i].input_res.height);
	}

	for (i = 0; i < N_IPU_FW_ISYS_CROPPING_LOCATION; i++) {
		dev_dbg(dev, "Crop info %d\n", i);
		dev_dbg(dev, "Crop.top_offset %d\n",
			stream_cfg->crop[i].top_offset);
		dev_dbg(dev, "Crop.left_offset %d\n",
			stream_cfg->crop[i].left_offset);
		dev_dbg(dev, "Crop.bottom_offset %d\n",
			stream_cfg->crop[i].bottom_offset);
		dev_dbg(dev, "Crop.right_offset %d\n",
			stream_cfg->crop[i].right_offset);
		dev_dbg(dev, "----------------\n");
	}

	for (i = 0; i < stream_cfg->nof_output_pins; i++) {
		dev_dbg(dev, "Output pin %d\n", i);
		dev_dbg(dev, "Output input pin id %d\n",
			stream_cfg->output_pins[i].input_pin_id);
		dev_dbg(dev, "Output res width %d\n",
			stream_cfg->output_pins[i].output_res.width);
		dev_dbg(dev, "Output res height %d\n",
			stream_cfg->output_pins[i].output_res.height);
		dev_dbg(dev, "Stride %d\n", stream_cfg->output_pins[i].stride);
		dev_dbg(dev, "Pin type %d\n", stream_cfg->output_pins[i].pt);
		dev_dbg(dev, "Ft %d\n", stream_cfg->output_pins[i].ft);
		dev_dbg(dev, "Watermar in lines %d\n",
			stream_cfg->output_pins[i].watermark_in_lines);
		dev_dbg(dev, "Send irq %d\n",
			stream_cfg->output_pins[i].send_irq);
		dev_dbg(dev, "Reserve compression %d\n",
			stream_cfg->output_pins[i].reserve_compression);
		dev_dbg(dev, "----------------\n");
	}

	dev_dbg(dev, "Isl_use %d\n", stream_cfg->isl_use);
	switch (stream_cfg->isl_use) {
	case IPU_FW_ISYS_USE_SINGLE_ISA:
		dev_dbg(dev, "ISA cfg:\n");
		dev_dbg(dev, "blc_enabled %d\n", stream_cfg->isa_cfg.cfg.blc);
		dev_dbg(dev, "lsc_enabled %d\n", stream_cfg->isa_cfg.cfg.lsc);
		dev_dbg(dev, "dpc_enabled %d\n", stream_cfg->isa_cfg.cfg.dpc);
		dev_dbg(dev, "downscaler_enabled %d\n",
			stream_cfg->isa_cfg.cfg.downscaler);
		dev_dbg(dev, "awb_enabled %d\n", stream_cfg->isa_cfg.cfg.awb);
		dev_dbg(dev, "af_enabled %d\n", stream_cfg->isa_cfg.cfg.af);
		dev_dbg(dev, "ae_enabled %d\n", stream_cfg->isa_cfg.cfg.ae);
		break;
	case IPU_FW_ISYS_USE_SINGLE_DUAL_ISL:
	case IPU_FW_ISYS_USE_NO_ISL_NO_ISA:
	default:
		break;
	}
}

void ipu_fw_isys_dump_frame_buff_set(struct device *dev,
				     struct ipu_fw_isys_frame_buff_set_abi *buf,
				     unsigned int outputs)
{
	unsigned int i;

	dev_dbg(dev, "--------------------------\n");
	dev_dbg(dev, "IPU_FW_ISYS_FRAME_BUFF_SET\n");
	dev_dbg(dev, "--------------------------\n");

	for (i = 0; i < outputs; i++) {
		dev_dbg(dev, "Output pin %d\n", i);
		dev_dbg(dev, "out_buf_id %llu\n",
			buf->output_pins[i].out_buf_id);
		dev_dbg(dev, "addr 0x%x\n", buf->output_pins[i].addr);
		dev_dbg(dev, "compress %u\n", buf->output_pins[i].compress);

		dev_dbg(dev, "----------------\n");
	}

	dev_dbg(dev, "process_group_light.addr 0x%x\n",
		buf->process_group_light.addr);
	dev_dbg(dev, "process_group_light.param_buf_id %llu\n",
		buf->process_group_light.param_buf_id);
	dev_dbg(dev, "send_irq_sof 0x%x\n", buf->send_irq_sof);
	dev_dbg(dev, "send_irq_eof 0x%x\n", buf->send_irq_eof);
	dev_dbg(dev, "send_resp_sof 0x%x\n", buf->send_resp_sof);
	dev_dbg(dev, "send_resp_eof 0x%x\n", buf->send_resp_eof);
#if defined(CONFIG_VIDEO_INTEL_IPU4) || defined(CONFIG_VIDEO_INTEL_IPU4P)
	dev_dbg(dev, "send_irq_capture_ack 0x%x\n", buf->send_irq_capture_ack);
	dev_dbg(dev, "send_irq_capture_done 0x%x\n", buf->send_irq_capture_done);
#endif
#ifdef IPU_OTF_SUPPORT
	dev_dbg(dev, "frame_counter 0x%x\n", buf->frame_counter);
#endif
}
