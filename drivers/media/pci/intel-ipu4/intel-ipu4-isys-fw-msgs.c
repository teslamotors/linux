/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <asm/cacheflush.h>

#include <linux/kernel.h>
#include <linux/delay.h>
#include "intel-ipu4-regs.h"
#include "intel-ipu4-isys-fw-msgs.h"
#include "intel-ipu4-fw-com.h"
#include "intel-ipu4-isys.h"

#include "intel-ipu4-isys-fw-tables.h"

void intel_ipu4_isys_set_fw_params(
	struct ipu_fw_isys_stream_cfg_data_abi *stream_cfg)
{
	unsigned int i;
	unsigned int idx;

	for (i = 0; i < stream_cfg->nof_input_pins; i++) {
		idx = stream_cfg->input_pins[i].dt;
		stream_cfg->input_pins[i].bits_per_pix =
			extracted_bits_per_pixel_per_mipi_data_type[idx];
	}
}

const char send_msg_types[N_IPU_FW_ISYS_SEND_TYPE][32] = {
	"STREAM_OPEN",
	"STREAM_START",
	"STREAM_START_AND_CAPTURE",
	"STREAM_CAPTURE",
	"STREAM_STOP",
	"STREAM_FLUSH",
	"STREAM_CLOSE"
};

static int handle_proxy_response(struct intel_ipu4_isys *isys,
				 unsigned int req_id)
{
	struct ipu_fw_isys_proxy_resp_info_abi *resp;
	int rval = -EIO;

	resp = (struct ipu_fw_isys_proxy_resp_info_abi *)
		intel_ipu4_recv_get_token(isys->fwcom, ISYS_PROXY_INDEX);
	if (!resp)
		return 1;

	dev_dbg(&isys->adev->dev,
		"Proxy response: id 0x%x, error %d, details %d\n",
		resp->request_id, resp->error_info.error,
		resp->error_info.error_details);

	if (req_id == resp->request_id)
		rval = 0;

	intel_ipu4_recv_put_token(isys->fwcom, ISYS_PROXY_INDEX);
	return rval;
}

/* Simple blocking proxy send function */
static int intel_ipu4_isys_send_proxy_token(struct intel_ipu4_isys *isys,
				     unsigned int req_id,
				     unsigned int index,
				     unsigned int offset,
				     u32 value)
{
	struct intel_ipu4_fw_com_context *ctx = isys->fwcom;
	struct ipu_fw_proxy_send_queue_token *token;
	unsigned int timeout = 1000;
	int rval = -EBUSY;

	dev_dbg(&isys->adev->dev,
		"proxy send token: req_id 0x%x, index %d, offset 0x%x, value 0x%x\n",
		req_id, index, offset, value);

	mutex_lock(&isys->mutex);
	token = intel_ipu4_send_get_token(ctx, ISYS_PROXY_INDEX);
	if (!token)
		goto leave;

	token->request_id = req_id;
	token->region_index = index;
	token->offset = offset;
	token->value = value;
	intel_ipu4_send_put_token(ctx, ISYS_PROXY_INDEX);

	/* Currently proxy doesn't support irq based service. Poll */
	do {
		usleep_range(100, 110);
		rval = handle_proxy_response(isys, req_id);
		if (!rval)
			break;
		if (rval == -EIO) {
			dev_err(&isys->adev->dev,
				"Proxy response received with unexpected id\n");
			break;
		}
		timeout--;
	} while (rval && timeout);

	if (!timeout)
		dev_err(&isys->adev->dev, "Proxy response timed out\n");
leave:
	mutex_unlock(&isys->mutex);
	return rval;
}

static int intel_ipu4_isys_abi_complex_cmd(struct intel_ipu4_isys *isys,
				    const unsigned int stream_handle,
				    void *cpu_mapped_buf,
				    dma_addr_t dma_mapped_buf,
				    size_t size,
				    enum ipu_fw_isys_send_type send_type)
{
	struct intel_ipu4_fw_com_context *ctx = isys->fwcom;
	struct ipu_fw_send_queue_token *token;

	if (send_type >= N_IPU_FW_ISYS_SEND_TYPE)
		return -EINVAL;

	dev_dbg(&isys->adev->dev,
		"send_token: %s\n", send_msg_types[send_type]);

	/*
	 * Time to flush cache in case we have some payload. Not all messages
	 * have that
	 */
	if (cpu_mapped_buf)
		clflush_cache_range(cpu_mapped_buf, size);

	token = intel_ipu4_send_get_token(ctx, stream_handle + ISYS_MSG_INDEX);
	if (!token)
		return -EBUSY;

	token->payload = dma_mapped_buf;
	token->buf_handle = (uint64_t)cpu_mapped_buf;
	token->send_type = send_type;
	intel_ipu4_send_put_token(ctx, stream_handle + ISYS_MSG_INDEX);

	return 0;
}

static int intel_ipu4_isys_abi_simple_cmd(struct intel_ipu4_isys *isys,
				   const unsigned int stream_handle,
				   enum ipu_fw_isys_send_type send_type)
{
	return intel_ipu4_isys_abi_complex_cmd(isys, stream_handle, NULL, 0, 0,
					       send_type);
}

static int intel_ipu4_isys_abi_fw_close(struct intel_ipu4_isys *isys)
{
	struct device *dev = &isys->adev->dev;
	int timeout = INTEL_IPU4_ISYS_TURNOFF_TIMEOUT;
	int rval;

	/*
	 * Stop the isys fw. Actual close takes
	 * some time as the FW must stop its actions including code fetch
	 * to SP icache.
	*/
	rval = intel_ipu4_fw_com_close(isys->fwcom);
	if (rval)
		dev_err(dev, "Device close failure: %d\n", rval);

	/* release probably fails if the close failed. Let's try still */
	do {
		usleep_range(INTEL_IPU4_ISYS_TURNOFF_DELAY_US,
			2 * INTEL_IPU4_ISYS_TURNOFF_DELAY_US);
		rval = intel_ipu4_fw_com_release(isys->fwcom, 0);
		timeout--;
	} while (rval != 0 && timeout);

	if (!rval)
		isys->fwcom = NULL; /* No further actions needed */
	else
		dev_err(dev, "Device release time out %d\n", rval);
	return rval;
}

static void intel_ipu4_isys_abi_fw_cleanup(struct intel_ipu4_isys *isys)
{
	intel_ipu4_fw_com_release(isys->fwcom, 1);
	isys->fwcom = NULL;
}

static void start_sp(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = isys->pdata->base +
		isys->pdata->ipdata->hw_variant.spc_offset;
	u32 val = 0;

	val |= INTEL_IPU4_ISYS_SPC_STATUS_START |
		INTEL_IPU4_ISYS_SPC_STATUS_RUN |
		INTEL_IPU4_ISYS_SPC_STATUS_CTRL_ICACHE_INVALIDATE;
	val |= isys->icache_prefetch ?
		INTEL_IPU4_ISYS_SPC_STATUS_ICACHE_PREFETCH : 0;
	writel(val, spc_regs_base + INTEL_IPU4_ISYS_REG_SPC_STATUS_CTRL);
}

static int query_sp(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *spc_regs_base = isys->pdata->base +
		isys->pdata->ipdata->hw_variant.spc_offset;
	u32 val =
		readl(spc_regs_base + INTEL_IPU4_ISYS_REG_SPC_STATUS_CTRL);

	/* return true when READY == 1, START == 0 */
	val &= INTEL_IPU4_ISYS_SPC_STATUS_READY |
		INTEL_IPU4_ISYS_SPC_STATUS_START;

	return val == INTEL_IPU4_ISYS_SPC_STATUS_READY;
}

static int intel_ipu4_isys_abi_fw_init(struct intel_ipu4_isys *isys,
				       unsigned int num_streams)
{
	int retry = INTEL_IPU4_ISYS_OPEN_RETRY;
	int num_in_message_queues = clamp_t(
			unsigned int, num_streams, 1,
			INTEL_IPU4_ISYS_NUM_STREAMS_B0);
	int num_out_message_queues = 1;

	struct ia_css_syscom_queue_config
	      input_queue_cfg[INTEL_IPU4_STREAM_ID_MAX + ISYS_NBR_PROXY_QUEUES];
	struct ia_css_syscom_queue_config
		output_queue_cfg[1 + ISYS_NBR_PROXY_QUEUES];

	struct intel_ipu4_fw_com_cfg fwcom = {
		.num_input_queues =
		 num_in_message_queues + ISYS_NBR_PROXY_QUEUES,
		.num_output_queues =
		 num_out_message_queues + ISYS_NBR_PROXY_QUEUES,
		.input = input_queue_cfg,
		.output = output_queue_cfg,
		.cell_start = start_sp,
		.cell_ready = query_sp,
	};

	struct ipu_fw_isys_fw_config isys_fw_cfg = {
		.num_send_queues[ISYS_MSG_INDEX] = num_in_message_queues,
		.num_recv_queues[ISYS_MSG_INDEX] = num_out_message_queues,
		.num_send_queues[ISYS_PROXY_INDEX] = ISYS_NBR_PROXY_QUEUES,
		.num_recv_queues[ISYS_PROXY_INDEX] = ISYS_NBR_PROXY_QUEUES,
	};
	struct device *dev = &isys->adev->dev;
	int rval, i;

	/*
	 * SRAM partitioning. Initially equal partitioning is set
	 * TODO: Fine tune the partitining based on the stream pixel load
	 */
	for (i = 0; i < INTEL_IPU4_NOF_SRAM_BLOCKS_MAX; i++) {
		if (i < num_in_message_queues)
			isys_fw_cfg.buffer_partition.num_gda_pages[i] =
				(INTEL_IPU4_DEVICE_GDA_NR_PAGES *
				 INTEL_IPU4_DEVICE_GDA_VIRT_FACTOR) /
				num_in_message_queues;
		else
			isys_fw_cfg.buffer_partition.num_gda_pages[i] = 0;
	}

	fwcom.dmem_addr = isys->pdata->ipdata->hw_variant.dmem_offset;
	fwcom.specific_addr = &isys_fw_cfg;
	fwcom.specific_size = sizeof(isys_fw_cfg);

	/* FW assumes proxy interface at fwcom queue 0 */
	for (i = 0; i < ISYS_NBR_PROXY_QUEUES; i++) {
		input_queue_cfg[i].token_size =
			sizeof(struct ipu_fw_proxy_send_queue_token);
		input_queue_cfg[i].queue_size =
			INTEL_IPU4_ISYS_SIZE_PROXY_SEND_QUEUE;
	}
	/* Note! i continues from previous value */
	for (; i < fwcom.num_input_queues; i++) {
		input_queue_cfg[i].token_size =
			sizeof(struct ipu_fw_send_queue_token);
		input_queue_cfg[i].queue_size = INTEL_IPU4_ISYS_SIZE_SEND_QUEUE;
	}

	for (i = 0; i < ISYS_NBR_PROXY_QUEUES; i++) {
		output_queue_cfg[i].token_size =
			sizeof(struct ipu_fw_proxy_resp_queue_token);
		output_queue_cfg[i].queue_size =
			INTEL_IPU4_ISYS_SIZE_PROXY_RECV_QUEUE;
	}
	/* Note! i continues from previous value */
	for (; i < fwcom.num_output_queues; i++) {
		output_queue_cfg[i].token_size =
			sizeof(struct ipu_fw_resp_queue_token);
		output_queue_cfg[i].queue_size =
			INTEL_IPU4_ISYS_SIZE_RECV_QUEUE;
	}

	isys->fwcom = intel_ipu4_fw_com_prepare(&fwcom, isys->adev,
						isys->pdata->base);
	if (!isys->fwcom) {
		dev_err(dev, "isys fw com prepare failed\n");
		return -EIO;
	}

	intel_ipu4_fw_com_open(isys->fwcom);

	do {
		usleep_range(INTEL_IPU4_ISYS_OPEN_TIMEOUT_US,
			     INTEL_IPU4_ISYS_OPEN_TIMEOUT_US + 10);
		rval = intel_ipu4_fw_com_ready(isys->fwcom);
		if (!rval)
			break;
	} while (retry > 0);

	if (!retry && rval) {
		dev_err(dev, "isys port open ready failed %d\n", rval);
		intel_ipu4_isys_abi_fw_close(isys);
	}

	return rval;
}

void fw_dump_isys_stream_cfg(struct device *dev,
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
		dev_dbg(dev, "Mipi data type %d\n",
			stream_cfg->input_pins[i].dt);
		dev_dbg(dev, "Mipi store mode %d\n",
			stream_cfg->input_pins[i].mipi_store_mode);
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

		dev_dbg(dev, "Stride type %d\n",
			stream_cfg->output_pins[i].stride);
		dev_dbg(dev, "Pin type %d\n", stream_cfg->output_pins[i].pt);
		dev_dbg(dev, "Ft %d\n", stream_cfg->output_pins[i].ft);
		dev_dbg(dev, "Online %d\n", stream_cfg->output_pins[i].online);

		dev_dbg(dev, "Watermar in lines %d\n",
			stream_cfg->output_pins[i].watermark_in_lines);
		dev_dbg(dev, "Send irq %d\n",
			stream_cfg->output_pins[i].send_irq);
		dev_dbg(dev, "----------------\n");
	}

	dev_dbg(dev, "Isl_use %d\n", stream_cfg->isl_use);
	switch (stream_cfg->isl_use) {
	case IPU_FW_ISYS_USE_SINGLE_ISA:
		dev_dbg(dev, "ISA cfg:\n");
		dev_dbg(dev, "blc_enabled %d\n",
			stream_cfg->isa_cfg.cfg.blc);
		dev_dbg(dev, "lsc_enabled %d\n",
			stream_cfg->isa_cfg.cfg.lsc);
		dev_dbg(dev, "dpc_enabled %d\n",
			stream_cfg->isa_cfg.cfg.dpc);
		dev_dbg(dev, "downscaler_enabled %d\n",
			stream_cfg->isa_cfg.cfg.downscaler);
		dev_dbg(dev, "awb_enabled %d\n",
			stream_cfg->isa_cfg.cfg.awb);
		dev_dbg(dev, "af_enabled %d\n", stream_cfg->isa_cfg.cfg.af);
		dev_dbg(dev, "ae_enabled %d\n", stream_cfg->isa_cfg.cfg.ae);
		break;
	case IPU_FW_ISYS_USE_SINGLE_DUAL_ISL:
	case IPU_FW_ISYS_USE_NO_ISL_NO_ISA:
	default:
		break;
	}

}

void fw_dump_isys_frame_buff_set(struct device *dev,
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

		dev_dbg(dev, "----------------\n");
	}

	dev_dbg(dev, "process_group_light.addr 0x%x\n",
		buf->process_group_light.addr);
	dev_dbg(dev, "process_group_light.param_buf_id %llu\n",
		buf->process_group_light.param_buf_id);
	dev_dbg(dev, "send_irq_sof 0x%x\n", buf->send_irq_sof);
	dev_dbg(dev, "send_irq_eof 0x%x\n", buf->send_irq_eof);
}

static struct ipu_fw_isys_resp_info_abi *intel_ipu4_isys_abi_get_resp(
	void *context, unsigned int queue,
	struct ipu_fw_isys_resp_info_abi *response)
{
	return (struct ipu_fw_isys_resp_info_abi *)
		intel_ipu4_recv_get_token(context, ISYS_MSG_INDEX);
}

static void intel_ipu4_isys_abi_put_resp(void *context, unsigned int queue)
{
	intel_ipu4_recv_put_token(context, ISYS_MSG_INDEX);
}

static const struct intel_ipu4_isys_fw_ctrl abi_fw_cntr = {
	.fw_init = intel_ipu4_isys_abi_fw_init,
	.fw_close = intel_ipu4_isys_abi_fw_close,
	.fw_force_clean = intel_ipu4_isys_abi_fw_cleanup,
	.simple_cmd = intel_ipu4_isys_abi_simple_cmd,
	.complex_cmd = intel_ipu4_isys_abi_complex_cmd,
	.get_response = intel_ipu4_isys_abi_get_resp,
	.put_response = intel_ipu4_isys_abi_put_resp,
	.fw_proxy_cmd = intel_ipu4_isys_send_proxy_token,
};

void intel_ipu4_abi_init(struct intel_ipu4_isys *isys)
{
	isys->fwctrl = &abi_fw_cntr;
}
