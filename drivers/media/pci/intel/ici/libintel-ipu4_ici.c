// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2014 - 2018 Intel Corporation

#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include "ici/ici-isys.h"
#include "ipu-wrapper.h"
#include <ia_css_isysapi.h>

#include "ipu-platform.h"

#define ipu_lib_call_notrace_unlocked(func, isys, ...)		\
	({								\
		int rval;						\
									\
		rval = -ia_css_isys_##func((isys)->fwcom, ##__VA_ARGS__); \
									\
		rval;							\
	})

#define ipu_lib_call_notrace(func, isys, ...)		\
	({							\
		int rval;					\
								\
		mutex_lock(&(isys)->lib_mutex);			\
								\
		rval = ipu_lib_call_notrace_unlocked(	\
			func, isys, ##__VA_ARGS__);		\
								\
		mutex_unlock(&(isys)->lib_mutex);		\
								\
		rval;						\
	})

#define ipu_lib_call(func, isys, ...)				\
	({								\
		int rval;						\
		dev_dbg(&(isys)->adev->dev, "hostlib: libcall %s\n", #func); \
		rval = ipu_lib_call_notrace(func, isys, ##__VA_ARGS__); \
									\
		rval;							\
	})

static int wrapper_init_done;

int ipu_fw_isys_close(struct ici_isys *isys)
{
	struct device *dev = &isys->adev->dev;
	int timeout = IPU_ISYS_TURNOFF_TIMEOUT;
	int rval;
	unsigned long flags;

	/*
	 * Ask library to stop the isys fw. Actual close takes
	 * some time as the FW must stop its actions including code fetch
	 * to SP icache.
	 */
	spin_lock_irqsave(&isys->power_lock, flags);
	rval = ipu_lib_call(device_close, isys);
	spin_unlock_irqrestore(&isys->power_lock, flags);
	if (rval)
		dev_err(dev, "Device close failure: %d\n", rval);

	/* release probably fails if the close failed. Let's try still */
	do {
		usleep_range(IPU_ISYS_TURNOFF_DELAY_US,
				2 * IPU_ISYS_TURNOFF_DELAY_US);
		rval = ipu_lib_call_notrace(device_release, isys, 0);
		timeout--;
	} while (rval != 0 && timeout);

	/* Spin lock to wait the interrupt handler to be finished */
	spin_lock_irqsave(&isys->power_lock, flags);
	if (!rval)
		isys->fwcom = NULL; /* No further actions needed */
	else
		dev_err(dev, "Device release time out %d\n", rval);
	spin_unlock_irqrestore(&isys->power_lock, flags);
	return rval;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_close);

int ipu_fw_isys_init(struct ici_isys *isys,
					unsigned int num_streams)
{
	int retry = IPU_ISYS_OPEN_RETRY;
	unsigned int i;

	struct ia_css_isys_device_cfg_data isys_cfg = {
		.driver_sys = {
			.ssid = ISYS_SSID,
			.mmid = ISYS_MMID,
			.num_send_queues = clamp_t(
				unsigned int, num_streams, 1,
				IPU_ISYS_NUM_STREAMS),
			.num_recv_queues = IPU_ISYS_NUM_RECV_QUEUE,
			.send_queue_size = IPU_ISYS_SIZE_SEND_QUEUE,
			.recv_queue_size = IPU_ISYS_SIZE_RECV_QUEUE,
			.icache_prefetch = isys->icache_prefetch,
		},
	};
	struct device *dev = &isys->adev->dev;
	int rval;

	if (!wrapper_init_done) {
		wrapper_init_done = true;
		ipu_wrapper_init(ISYS_MMID, &isys->adev->dev,
				 isys->pdata->base);
	}

	/*
	 * SRAM partitioning. Initially equal partitioning is set
	 * TODO: Fine tune the partitining based on the stream pixel load
	 */
	for (i = 0; i < min(IPU_NOF_SRAM_BLOCKS_MAX, NOF_SRAM_BLOCKS_MAX); i++) {
		if (i < isys_cfg.driver_sys.num_send_queues)
			isys_cfg.buffer_partition.num_gda_pages[i] =
				(IPU_DEVICE_GDA_NR_PAGES *
				 IPU_DEVICE_GDA_VIRT_FACTOR) /
				isys_cfg.driver_sys.num_send_queues;
		else
			isys_cfg.buffer_partition.num_gda_pages[i] = 0;
	}

	rval = -ia_css_isys_device_open(&isys->fwcom, &isys_cfg);
	if (rval < 0) {
		dev_err(dev, "isys device open failed %d\n", rval);
		return rval;
	}

	do {
		usleep_range(IPU_ISYS_OPEN_TIMEOUT_US,
			     IPU_ISYS_OPEN_TIMEOUT_US + 10);
		rval = ipu_lib_call(device_open_ready, isys);
		if (!rval)
			break;
		retry--;
	} while (retry > 0);

	if (!retry && rval) {
		dev_err(dev, "isys device open ready failed %d\n", rval);
		ipu_fw_isys_close(isys);
	}

	return rval;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_init);

void ipu_fw_isys_cleanup(struct ici_isys *isys)
{
	ipu_lib_call(device_release, isys, 1);
	isys->fwcom = NULL;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_cleanup);

struct ipu_fw_isys_resp_info_abi *ipu_fw_isys_get_resp(
	void *context, unsigned int queue,
	struct ipu_fw_isys_resp_info_abi *response)
{
	struct ia_css_isys_resp_info apiresp;
	int rval;

	rval = -ia_css_isys_stream_handle_response(context, &apiresp);
	if (rval < 0)
		return NULL;

	response->buf_id = 0;
	response->type = apiresp.type;
	response->timestamp[0] = apiresp.timestamp[0];
	response->timestamp[1] = apiresp.timestamp[1];
	response->stream_handle = apiresp.stream_handle;
	response->error_info.error = apiresp.error;
	response->error_info.error_details = apiresp.error_details;
	response->pin.out_buf_id = apiresp.pin.out_buf_id;
	response->pin.addr = apiresp.pin.addr;
	response->pin_id = apiresp.pin_id;
	response->process_group_light.param_buf_id =
		apiresp.process_group_light.param_buf_id;
	response->process_group_light.addr =
		apiresp.process_group_light.addr;
	response->acc_id = apiresp.acc_id;
#ifdef IPU_OTF_SUPPORT
	response->frame_counter = apiresp.frame_counter;
	response->written_direct = apiresp.written_direct;
#endif

	return response;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_get_resp);

void ipu_fw_isys_put_resp(void *context, unsigned int queue)
{
	/* Nothing to do here really */
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_put_resp);

int ipu_fw_isys_simple_cmd(struct ici_isys *isys,
			   const unsigned int stream_handle,
			   enum ipu_fw_isys_send_type send_type)
{
	int rval = -1;

	switch (send_type) {
	case IPU_FW_ISYS_SEND_TYPE_STREAM_START:
		rval = ipu_lib_call(stream_start, isys, stream_handle,
					   NULL);
		break;
	case IPU_FW_ISYS_SEND_TYPE_STREAM_FLUSH:
		rval = ipu_lib_call(stream_flush, isys, stream_handle);
		break;
	case IPU_FW_ISYS_SEND_TYPE_STREAM_STOP:
		rval = ipu_lib_call(stream_stop, isys, stream_handle);
		break;
	case IPU_FW_ISYS_SEND_TYPE_STREAM_CLOSE:
		rval = ipu_lib_call(stream_close, isys, stream_handle);
		break;
	default:
		WARN_ON(1);
	}

	return rval;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_simple_cmd);

static void resolution_abi_to_api(const struct ipu_fw_isys_resolution_abi *abi,
				  struct ia_css_isys_resolution *api)
{
	api->width = abi->width;
	api->height = abi->height;
}

static void output_pin_payload_abi_to_api(
	struct ipu_fw_isys_output_pin_payload_abi *abi,
	struct ia_css_isys_output_pin_payload *api)
{
	api->out_buf_id = abi->out_buf_id;
	api->addr = abi->addr;
}

static void output_pin_info_abi_to_api(
	struct ipu_fw_isys_output_pin_info_abi *abi,
	struct ia_css_isys_output_pin_info *api)
{
	api->input_pin_id = abi->input_pin_id;
	resolution_abi_to_api(&abi->output_res, &api->output_res);
	api->stride = abi->stride;
	api->pt = abi->pt;
	api->watermark_in_lines = abi->watermark_in_lines;
	api->payload_buf_size = abi->payload_buf_size;
	api->send_irq = abi->send_irq;
	api->ft = abi->ft;
#ifdef IPU_OTF_SUPPORT
	api->link_id = abi->link_id;
#endif
	api->reserve_compression = abi->reserve_compression;
}

static void param_pin_abi_to_api(struct ipu_fw_isys_param_pin_abi *abi,
				 struct ia_css_isys_param_pin *api)
{
	api->param_buf_id = abi->param_buf_id;
	api->addr = abi->addr;
}

static void input_pin_info_abi_to_api(
	struct ipu_fw_isys_input_pin_info_abi *abi,
	struct ia_css_isys_input_pin_info *api)
{
	resolution_abi_to_api(&abi->input_res, &api->input_res);
	api->dt = abi->dt;
	api->mipi_store_mode = abi->mipi_store_mode;
	api->mapped_dt = abi->mapped_dt;
}

static void isa_cfg_abi_to_api(const struct ipu_fw_isys_isa_cfg_abi *abi,
			       struct ia_css_isys_isa_cfg *api)
{
	unsigned int i;

	for (i = 0; i < min(N_IPU_FW_ISYS_RESOLUTION_INFO,
			    N_IA_CSS_ISYS_RESOLUTION_INFO); i++)
		resolution_abi_to_api(&abi->isa_res[i], &api->isa_res[i]);

	api->blc_enabled = abi->cfg.blc;
	api->lsc_enabled = abi->cfg.lsc;
	api->dpc_enabled = abi->cfg.dpc;
	api->downscaler_enabled = abi->cfg.downscaler;
	api->awb_enabled = abi->cfg.awb;
	api->af_enabled = abi->cfg.af;
	api->ae_enabled = abi->cfg.ae;
	api->paf_type = abi->cfg.paf;
	api->send_irq_stats_ready = abi->cfg.send_irq_stats_ready;
	api->send_resp_stats_ready = abi->cfg.send_irq_stats_ready;
}

static void cropping_abi_to_api(struct ipu_fw_isys_cropping_abi *abi,
				struct ia_css_isys_cropping *api)
{
	api->top_offset = abi->top_offset;
	api->left_offset = abi->left_offset;
	api->bottom_offset = abi->bottom_offset;
	api->right_offset = abi->right_offset;
}

static void stream_cfg_abi_to_api(struct ipu_fw_isys_stream_cfg_data_abi *abi,
				  struct ia_css_isys_stream_cfg_data *api)
{
	unsigned int i;

	api->src = abi->src;
	api->vc = abi->vc;
	api->isl_use = abi->isl_use;
	api->compfmt = abi->compfmt;
	isa_cfg_abi_to_api(&abi->isa_cfg, &api->isa_cfg);
	for (i = 0; i < min(N_IPU_FW_ISYS_CROPPING_LOCATION,
			    N_IA_CSS_ISYS_CROPPING_LOCATION); i++)
		cropping_abi_to_api(&abi->crop[i], &api->crop[i]);

	api->send_irq_sof_discarded = abi->send_irq_sof_discarded;
	api->send_irq_eof_discarded = abi->send_irq_eof_discarded;
	api->send_resp_sof_discarded = abi->send_irq_sof_discarded;
	api->send_resp_eof_discarded = abi->send_irq_eof_discarded;
	api->nof_input_pins = abi->nof_input_pins;
	api->nof_output_pins = abi->nof_output_pins;
	for (i = 0; i < abi->nof_input_pins; i++)
		input_pin_info_abi_to_api(&abi->input_pins[i],
					  &api->input_pins[i]);

	for (i = 0; i < abi->nof_output_pins; i++)
		output_pin_info_abi_to_api(&abi->output_pins[i],
					   &api->output_pins[i]);
}

static void frame_buff_set_abi_to_api(
	struct ipu_fw_isys_frame_buff_set_abi *abi,
	struct ia_css_isys_frame_buff_set *api)
{
	int i;

	for (i = 0; i < min(IPU_MAX_OPINS, MAX_OPINS); i++)
		output_pin_payload_abi_to_api(&abi->output_pins[i],
					      &api->output_pins[i]);

	param_pin_abi_to_api(&abi->process_group_light,
			     &api->process_group_light);

	api->send_irq_sof = abi->send_irq_sof;
	api->send_irq_eof = abi->send_irq_eof;
}

int ipu_fw_isys_complex_cmd(struct ici_isys *isys,
					   const unsigned int stream_handle,
					   void *cpu_mapped_buf,
					   dma_addr_t dma_mapped_buf,
					   size_t size,
					   enum ipu_fw_isys_send_type send_type)
{
	union {
		struct ia_css_isys_stream_cfg_data stream_cfg;
		struct ia_css_isys_frame_buff_set buf;
	} param;
	int rval = -1;

	memset(&param, 0, sizeof(param));

	switch (send_type) {
	case IPU_FW_ISYS_SEND_TYPE_STREAM_CAPTURE:
		frame_buff_set_abi_to_api(cpu_mapped_buf, &param.buf);
		rval = ipu_lib_call(stream_capture_indication,
				    isys, stream_handle, &param.buf);
		break;
	case IPU_FW_ISYS_SEND_TYPE_STREAM_OPEN:
		stream_cfg_abi_to_api(cpu_mapped_buf, &param.stream_cfg);
		rval = ipu_lib_call(stream_open, isys, stream_handle,
				    &param.stream_cfg);
		break;
	case IPU_FW_ISYS_SEND_TYPE_STREAM_START_AND_CAPTURE:
		frame_buff_set_abi_to_api(cpu_mapped_buf, &param.buf);
		rval = ipu_lib_call(stream_start, isys, stream_handle,
				    &param.buf);
		break;
	default:
		WARN_ON(1);
	}

	return rval;
}
EXPORT_SYMBOL_GPL(ipu_fw_isys_complex_cmd);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu library");
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

