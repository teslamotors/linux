/*
 * Copyright (c) 2014--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include "libintel-ipu4.h"

EXPORT_SYMBOL_GPL(ia_css_isys_device_open);
EXPORT_SYMBOL_GPL(ia_css_isys_device_open_ready);
EXPORT_SYMBOL_GPL(ia_css_isys_device_close);
EXPORT_SYMBOL_GPL(ia_css_isys_device_release);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_open);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_close);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_start);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_stop);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_flush);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_capture_indication);
EXPORT_SYMBOL_GPL(ia_css_isys_stream_handle_response);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_entry_get_address_lo);
EXPORT_SYMBOL_GPL(ia_css_pkg_dir_get_entry);


void csslib_dump_isys_stream_cfg(struct device *dev, struct ia_css_isys_stream_cfg_data *stream_cfg)
{
	int i;

	dev_dbg(dev, "---------------------------\n");
	dev_dbg(dev, "IA_CSS_ISYS_STREAM_CFG_DATA\n");
	dev_dbg(dev, "---------------------------\n");

	dev_dbg(dev, "Source %d\n", stream_cfg->src);
	dev_dbg(dev, "VC %d\n", stream_cfg->vc);
	dev_dbg(dev, "Nof input pins %d\n", stream_cfg->nof_input_pins);
	dev_dbg(dev, "Nof output pins %d\n", stream_cfg->nof_output_pins);

	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		dev_dbg(dev, "Input pin %d\n", i);
		dev_dbg(dev, "Mipi data type %d\n", stream_cfg->input_pins[i].dt);
		dev_dbg(dev, "Input res width %d\n", stream_cfg->input_pins[i].input_res.width);
		dev_dbg(dev, "Input res height %d\n", stream_cfg->input_pins[i].input_res.height);
	}

	for (i = 0; i < N_IA_CSS_ISYS_CROPPING_LOCATION; i++) {
		dev_dbg(dev, "Crop info %d\n", i);
		dev_dbg(dev, "Crop.top_offset %d\n", stream_cfg->crop[i].top_offset);
		dev_dbg(dev, "Crop.left_offset %d\n", stream_cfg->crop[i].left_offset);
		dev_dbg(dev, "Crop.bottom_offset %d\n", stream_cfg->crop[i].bottom_offset);
		dev_dbg(dev, "Crop.right_offset %d\n", stream_cfg->crop[i].right_offset);
		dev_dbg(dev, "----------------\n");
	}

	for (i = 0; i < stream_cfg->nof_output_pins; i++) {
		dev_dbg(dev, "Output pin %d\n", i);

		dev_dbg(dev, "Output input pin id %d\n", stream_cfg->output_pins[i].input_pin_id);

		dev_dbg(dev, "Output res width %d\n", stream_cfg->output_pins[i].output_res.width);
		dev_dbg(dev, "Output res height %d\n", stream_cfg->output_pins[i].output_res.height);

		dev_dbg(dev, "Stride type %d\n", stream_cfg->output_pins[i].stride);
		dev_dbg(dev, "Pin type %d\n", stream_cfg->output_pins[i].pt);
		dev_dbg(dev, "Ft %d\n", stream_cfg->output_pins[i].ft);

		dev_dbg(dev, "Watermar in lines %d\n", stream_cfg->output_pins[i].watermark_in_lines);
		dev_dbg(dev, "Send irq %d\n", stream_cfg->output_pins[i].send_irq);
		dev_dbg(dev, "----------------\n");
	}

	dev_dbg(dev, "Isl_use %d\n", stream_cfg->isl_use);
	switch (stream_cfg->isl_use) {
	case IA_CSS_ISYS_USE_SINGLE_ISA:
		dev_dbg(dev, "ISA cfg:\n");
		dev_dbg(dev, "blc_enabled %d\n", stream_cfg->isa_cfg.blc_enabled);
		dev_dbg(dev, "lsc_enabled %d\n", stream_cfg->isa_cfg.lsc_enabled);
		dev_dbg(dev, "dpc_enabled %d\n", stream_cfg->isa_cfg.dpc_enabled);
		dev_dbg(dev, "downscaler_enabled %d\n", stream_cfg->isa_cfg.downscaler_enabled);
		dev_dbg(dev, "awb_enabled %d\n", stream_cfg->isa_cfg.awb_enabled);
		dev_dbg(dev, "af_enabled %d\n", stream_cfg->isa_cfg.af_enabled);
		dev_dbg(dev, "ae_enabled %d\n", stream_cfg->isa_cfg.ae_enabled);
		break;
	case IA_CSS_ISYS_USE_SINGLE_DUAL_ISL:
	case IA_CSS_ISYS_USE_NO_ISL_NO_ISA:
	default:
		break;
	}

}
EXPORT_SYMBOL_GPL(csslib_dump_isys_stream_cfg);

void csslib_dump_isys_frame_buff_set(struct device *dev,
				     struct ia_css_isys_frame_buff_set *buf,
				     unsigned int outputs)
{
	int i;

	dev_dbg(dev, "--------------------------\n");
	dev_dbg(dev, "IA_CSS_ISYS_FRAME_BUFF_SET\n");
	dev_dbg(dev, "--------------------------\n");

	for (i = 0; i < outputs; i++) {
		dev_dbg(dev, "Output pin %d\n", i);
		dev_dbg(dev, "out_buf_id %llu\n", buf->output_pins[i].out_buf_id);
		dev_dbg(dev, "addr 0x%x\n", buf->output_pins[i].addr);

		dev_dbg(dev, "----------------\n");
	}

#ifdef PARAMETER_INTERFACE_V2
	dev_dbg(dev, "process_group_light.addr 0x%x\n", buf->process_group_light.addr);
	dev_dbg(dev, "process_group_light.param_buf_id %llu\n", buf->process_group_light.param_buf_id);
#else
	dev_dbg(dev, "blc_param.param_buf_id %llu\n", buf->blc_param.param_buf_id);
	dev_dbg(dev, "blc_param.addr 0x%x\n", buf->blc_param.addr);

	dev_dbg(dev, "lsc_param.param_buf_id %llu\n", buf->lsc_param.param_buf_id);
	dev_dbg(dev, "lsc_param.addr 0x%x\n", buf->lsc_param.addr);

	dev_dbg(dev, "dpc_param.param_buf_id %llu\n", buf->dpc_param.param_buf_id);
	dev_dbg(dev, "dpc_param.addr 0x%x\n", buf->dpc_param.addr);

	dev_dbg(dev, "ids_param.param_buf_id %llu\n", buf->ids_param.param_buf_id);
	dev_dbg(dev, "ids_param.addr 0x%x\n", buf->ids_param.addr);

	dev_dbg(dev, "awb_param.param_buf_id %llu\n", buf->awb_param.param_buf_id);
	dev_dbg(dev, "awb_param.addr 0x%x\n", buf->awb_param.addr);

	dev_dbg(dev, "af_param.param_buf_id %llu\n", buf->af_param.param_buf_id);
	dev_dbg(dev, "af_param.addr 0x%x\n", buf->af_param.addr);

	dev_dbg(dev, "ae_param.param_buf_id %llu\n", buf->ae_param.param_buf_id);
	dev_dbg(dev, "ae_param.addr 0x%x\n", buf->ae_param.addr);
#endif

	dev_dbg(dev, "send_irq_sof 0x%x\n", buf->send_irq_sof);
	dev_dbg(dev, "send_irq_eof 0x%x\n", buf->send_irq_eof);
}
EXPORT_SYMBOL_GPL(csslib_dump_isys_frame_buff_set);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 library");

