/*
 *  skl-message.c - HDA DSP interface for FW registration, Pipe and Module
 *  configurations
 *
 *  Copyright (C) 2015 Intel Corp
 *  Author:Rafal Redzimski <rafal.f.redzimski@intel.com>
 *	   Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/pci.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include "skl-sst-dsp.h"
#include "cnl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl.h"
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-topology.h"
#include "skl-tplg-interface.h"
#include "skl-fwlog.h"

#define ASRC_MODE_UPLINK   2
#define ASRC_MODE_DOWNLINK  1

static int skl_alloc_dma_buf(struct device *dev,
		struct snd_dma_buffer *dmab, size_t size)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);

	if (!bus)
		return -ENODEV;

	return  bus->io_ops->dma_alloc_pages(bus, SNDRV_DMA_TYPE_DEV, size, dmab);
}

static int skl_free_dma_buf(struct device *dev, struct snd_dma_buffer *dmab)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);

	if (!bus)
		return -ENODEV;

	bus->io_ops->dma_free_pages(bus, dmab);

	return 0;
}

#define ENABLE_LOGS		6
#define FW_LOGGING_AGING_TIMER_PERIOD 100
#define FW_LOG_FIFO_FULL_TIMER_PERIOD 100

/* set firmware logging state via IPC */
int skl_dsp_enable_logging(struct sst_generic_ipc *ipc, int core, int enable)
{
	struct skl_log_state_msg log_msg;
	struct skl_ipc_large_config_msg msg = {0};
	int ret = 0;

	log_msg.aging_timer_period = FW_LOGGING_AGING_TIMER_PERIOD;
	log_msg.fifo_full_timer_period = FW_LOG_FIFO_FULL_TIMER_PERIOD;

	log_msg.core_mask = (1 << core);
	log_msg.logs_core[core].enable = enable;
	log_msg.logs_core[core].priority = ipc->dsp->trace_wind.log_priority;

	msg.large_param_id = ENABLE_LOGS;
	msg.param_data_size = sizeof(log_msg);

	ret = skl_ipc_set_large_config(ipc, &msg, (u32 *)&log_msg);

	return ret;
}

#define SYSTEM_TIME		20

/* set system time to DSP via IPC */
int skl_dsp_set_system_time(struct skl_sst *skl_sst)
{
	struct sst_generic_ipc *ipc = &skl_sst->ipc;
	struct SystemTime sys_time_msg;
	struct skl_ipc_large_config_msg msg = {0};
	struct timeval tv;
	u64 sys_time;
	u64 mask = 0x00000000FFFFFFFF;
	int ret;

	do_gettimeofday(&tv);

	/* DSP firmware expects UTC time in micro seconds */
	sys_time = tv.tv_sec*1000*1000 + tv.tv_usec;
	sys_time_msg.val_l = sys_time & mask;
	sys_time_msg.val_u = (sys_time & (~mask)) >> 32;

	msg.large_param_id = SYSTEM_TIME;
	msg.param_data_size = sizeof(sys_time_msg);

	ret = skl_ipc_set_large_config(ipc, &msg, (u32 *)&sys_time_msg);
	return ret;
}

#define NOTIFICATION_PARAM_ID 3
#define NOTIFICATION_MASK 0xf

/* disable notfication for underruns/overruns from firmware module */
static void skl_dsp_enable_notification(struct skl_sst *ctx, bool enable)
{
	struct notification_mask mask;
	struct skl_ipc_large_config_msg	msg = {0};

	mask.notify = NOTIFICATION_MASK;
	mask.enable = enable;

	msg.large_param_id = NOTIFICATION_PARAM_ID;
	msg.param_data_size = sizeof(mask);

	skl_ipc_set_large_config(&ctx->ipc, &msg, (u32 *)&mask);
}

static int skl_dsp_setup_spib(struct device *dev, unsigned int size,
				int stream_tag, int enable)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct hdac_stream *stream = snd_hdac_get_hdac_stream(bus,
					SNDRV_PCM_STREAM_PLAYBACK, stream_tag);
	struct hdac_ext_stream *estream;

	if (!stream)
		return -EINVAL;

	estream = stream_to_hdac_ext_stream(stream);
	/* enable/disable SPIB for this hdac stream */
	snd_hdac_ext_stream_spbcap_enable(ebus, enable, stream->index);

	/* set the spib value */
	snd_hdac_ext_stream_set_spib(ebus, estream, size);

	return 0;
}

static int skl_dsp_prepare(struct device *dev, unsigned int format,
				unsigned int size, struct snd_dma_buffer *dmab)
{
	int ret;
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct hdac_ext_stream *estream;
	struct hdac_stream *stream;
	struct snd_pcm_substream substream;

	if (!bus)
		return -ENODEV;

	memset(&substream, 0, sizeof(substream));
	substream.stream = SNDRV_PCM_STREAM_PLAYBACK;

	estream = snd_hdac_ext_stream_assign(ebus, &substream,
					HDAC_EXT_STREAM_TYPE_HOST);
	if (!estream)
		return -ENODEV;

	stream = hdac_stream(estream);
	/* assign decouple host dma channel */
	ret = snd_hdac_dsp_prepare(stream, format, size, dmab);
	if (ret < 0)
		return ret;

	skl_dsp_setup_spib(dev, size, stream->stream_tag, true);
	return stream->stream_tag;
}

static int skl_dsp_trigger(struct device *dev, bool start, int stream_tag)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_stream *stream;
	struct hdac_bus *bus = ebus_to_hbus(ebus);

	if (!bus)
		return -ENODEV;

	stream = snd_hdac_get_hdac_stream(bus,
					SNDRV_PCM_STREAM_PLAYBACK, stream_tag);
	if (!stream)
		return -EINVAL;

	snd_hdac_dsp_trigger(stream, start);

	return 0;
}

static int skl_dsp_cleanup(struct device *dev, struct snd_dma_buffer *dmab,
								int stream_tag)
{
	struct hdac_ext_bus *ebus = dev_get_drvdata(dev);
	struct hdac_stream *stream;
	struct hdac_ext_stream *estream;
	struct hdac_bus *bus = ebus_to_hbus(ebus);

	if (!bus)
		return -ENODEV;

	stream = snd_hdac_get_hdac_stream(bus,
					SNDRV_PCM_STREAM_PLAYBACK, stream_tag);
	if (!stream)
		return -EINVAL;

	estream = stream_to_hdac_ext_stream(stream);
	skl_dsp_setup_spib(dev, 0, stream_tag, false);
	snd_hdac_ext_stream_release(estream, HDAC_EXT_STREAM_TYPE_HOST);

	snd_hdac_dsp_cleanup(stream, dmab);

	return 0;
}

static struct skl_dsp_loader_ops skl_get_loader_ops(void)
{
	struct skl_dsp_loader_ops loader_ops;

	memset(&loader_ops, 0, sizeof(struct skl_dsp_loader_ops));

	loader_ops.alloc_dma_buf = skl_alloc_dma_buf;
	loader_ops.free_dma_buf = skl_free_dma_buf;

	return loader_ops;
};

static struct skl_dsp_loader_ops bxt_get_loader_ops(void)
{
	struct skl_dsp_loader_ops loader_ops;

	memset(&loader_ops, 0, sizeof(struct skl_dsp_loader_ops));

	loader_ops.alloc_dma_buf = skl_alloc_dma_buf;
	loader_ops.free_dma_buf = skl_free_dma_buf;
	loader_ops.prepare = skl_dsp_prepare;
	loader_ops.trigger = skl_dsp_trigger;
	loader_ops.cleanup = skl_dsp_cleanup;

	return loader_ops;
};

static const struct skl_dsp_ops dsp_ops[] = {
{.id = 0x9d70, .loader_ops = skl_get_loader_ops, .init_hw = skl_sst_dsp_init_hw,
		.init_fw = skl_sst_dsp_init_fw, .cleanup = skl_sst_dsp_cleanup,
		.min_fw_ver = {9, 21, 0, 3173}},
{.id = 0x0a98, .loader_ops = bxt_get_loader_ops, .init_hw = bxt_sst_dsp_init_hw,
		.init_fw = bxt_sst_dsp_init_fw, .cleanup = bxt_sst_dsp_cleanup,
		.min_fw_ver = {9, 21, 0, 3173}},
{.id = 0x5a98, .loader_ops = bxt_get_loader_ops, .init_hw = bxt_sst_dsp_init_hw,
		.init_fw = bxt_sst_dsp_init_fw, .cleanup = bxt_sst_dsp_cleanup,
		.min_fw_ver = {9, 22, 1, 3132}},
{.id = 0x9df0, .loader_ops = bxt_get_loader_ops, .init_hw = cnl_sst_dsp_init_hw,
		.init_fw = cnl_sst_dsp_init_fw, .cleanup = cnl_sst_dsp_cleanup,
		.min_fw_ver = {9, 21, 0, 3173}},
{.id = 0x1a98, .loader_ops = bxt_get_loader_ops, .init_hw = bxt_sst_dsp_init_hw,
		.init_fw = bxt_sst_dsp_init_fw, .cleanup = bxt_sst_dsp_cleanup,
		.min_fw_ver = {9, 21, 0, 3173}},
{}
};

static int skl_get_dsp_ops(int pci_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dsp_ops); i++) {
		if (dsp_ops[i].id == pci_id)
			return i;
	}

	return -EINVAL;
}

int skl_validate_fw_version(struct skl_sst *skl)
{
	struct skl_fw_version *fw_version = &skl->fw_property.version;
	const struct skl_dsp_ops *ops = skl->dsp_ops;

	dev_dbg(skl->dev,"ADSP FW Version: %d.%d.%d.%d\n",
	fw_version->major, fw_version->minor,
	fw_version->hotfix, fw_version->build);


	if (ops->min_fw_ver.major == fw_version->major &&
		ops->min_fw_ver.minor == fw_version->minor &&
		ops->min_fw_ver.build <= fw_version->build)

		return 0;

	dev_err(skl->dev, "Incorrect ADSP FW version = %d.%d.%d.%d,\
		minimum supported FW version = %d.%d.%d.%d\n",
		fw_version->major, fw_version->minor, fw_version->hotfix,
		fw_version->build,ops->min_fw_ver.major, ops->min_fw_ver.minor,
		ops->min_fw_ver.hotfix, ops->min_fw_ver.build);

	return -EINVAL;
}

int skl_init_dsp_hw(struct skl *skl, u32 num_ssp)
{
	void __iomem *mmio_base;
	struct hdac_ext_bus *ebus = &skl->ebus;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl_dsp_loader_ops loader_ops;
	int irq = bus->irq;
	int ret, index;
	struct dsp_init *d;

	/* enable ppcap interrupt */
	snd_hdac_ext_bus_ppcap_enable(&skl->ebus, true);
	snd_hdac_ext_bus_ppcap_int_enable(&skl->ebus, true);

	/* read the BAR of the ADSP MMIO */
	mmio_base = pci_ioremap_bar(skl->pci, 4);
	if (mmio_base == NULL) {
		dev_err(bus->dev, "ioremap error\n");
		ret = -ENXIO;
		goto dsp_init_exit;
	}

	index  = skl_get_dsp_ops(skl->pci->device);
	if (index  < 0) {
		ret = -EINVAL;
		goto dsp_init_exit;
	}

	loader_ops = dsp_ops[index].loader_ops();
	d = kzalloc(sizeof(struct dsp_init), GFP_KERNEL);
	d->mmio_base = mmio_base;
	d->irq = irq;
	d->dsp_ops = loader_ops;
	d->num_ssp = num_ssp;
	ret = dsp_ops[index].init_hw(bus->dev, &skl->skl_sst, d);
	skl->skl_sst->validate_fw_version = skl_validate_fw_version;

	INIT_LIST_HEAD(&skl->skl_sst->notify_kctls);
	kfree(d);

	skl->skl_sst->dsp_ops = &dsp_ops[index];

dsp_init_exit:
	dev_dbg(bus->dev, "dsp registration status=%d\n", ret);

	return ret;
}

int skl_init_dsp_fw(struct skl *skl)
{
	struct hdac_ext_bus *ebus = &skl->ebus;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	int ret, index;

	pm_runtime_get_sync(bus->dev);

	index  = skl_get_dsp_ops(skl->pci->device);
	if (index  < 0) {
		ret = -EINVAL;
		pm_runtime_put_sync(bus->dev);
		return ret;
	}

	ret = dsp_ops[index].init_fw(bus->dev, skl->skl_sst);

	if (ret < 0) {
		dev_err(bus->dev, "Firmware or Library load failed\n");
		pm_runtime_put_sync(bus->dev);
		return ret;
	}

	skl_dsp_enable_notification(skl->skl_sst, false);
	/* Set DMA clock controls */
	skl_dsp_set_dma_clk_controls(skl->skl_sst);

	dev_dbg(bus->dev, "dsp registration status=%d\n", ret);
	return ret;
}

int skl_free_dsp(struct skl *skl)
{
	struct hdac_ext_bus *ebus = &skl->ebus;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl_sst *ctx =  skl->skl_sst;
	struct skl_fw_property_info fw_property = skl->skl_sst->fw_property;
	struct skl_scheduler_config sch_config = fw_property.scheduler_config;

	/* disable  ppcap interrupt */
	snd_hdac_ext_bus_ppcap_int_enable(&skl->ebus, false);

	ctx->dsp_ops->cleanup(bus->dev, ctx);

	if (ctx->dsp->addr.lpe)
		iounmap(ctx->dsp->addr.lpe);

	kfree(fw_property.dma_config);
	kfree(sch_config.sys_tick_cfg);

	return 0;
}

int skl_suspend_dsp(struct skl *skl)
{
	struct skl_sst *ctx = skl->skl_sst;
	int ret;

	/* if ppcap is not supported return 0 */
	if (!skl->ebus.ppcap)
		return 0;

	/* Do not proceed to DSP suspend if DSP 1st boot is yet to be done */
	if (ctx->is_first_boot == true)
		goto disable_ppcap;

	if (ctx->fw_loaded) {
		ret = skl_dsp_sleep(ctx->dsp);
		if (ret < 0)
			return ret;
	}

disable_ppcap:
	/* disable ppcap interrupt */
	snd_hdac_ext_bus_ppcap_int_enable(&skl->ebus, false);
	snd_hdac_ext_bus_ppcap_enable(&skl->ebus, false);

	return 0;
}

int skl_resume_dsp(struct skl *skl)
{
	struct skl_sst *ctx = skl->skl_sst;
	int ret = 0;

	/* if ppcap is not supported return 0 */
	if (!skl->ebus.ppcap)
		return 0;

	/* enable ppcap interrupt */
	snd_hdac_ext_bus_ppcap_enable(&skl->ebus, true);
	snd_hdac_ext_bus_ppcap_int_enable(&skl->ebus, true);

	/* just return if DSP 1st boot is yet to be done */
	if (ctx->is_first_boot == true)
		return 0;

	ret = skl_dsp_wake(ctx->dsp);
	if (ret < 0)
		return ret;
	/*
	 * It is correct for skl_dsp_wake() to return success without
	 * loading the FW if it gets called before first boot, say
	 * because of runtime PM resume. In this case, call the
	 * notification enable function only if FW is loaded since
	 * this function sends an IPC */
	if (ctx->fw_loaded)
		skl_dsp_enable_notification(skl->skl_sst, false);

	return ret;
}


int skl_load_modules(struct skl_sst *ctx, struct skl_module_cfg *mcfg)
{
	if (ctx->dsp->fw_ops.load_mod)
		return ctx->dsp->fw_ops.load_mod(ctx->dsp, mcfg->id.module_id,
								mcfg->guid);
	return 0;
}

int skl_unload_modules(struct skl_sst *ctx, struct skl_module_cfg *mcfg)
{
	if (ctx->dsp->fw_ops.unload_mod)
		return ctx->dsp->fw_ops.unload_mod(ctx->dsp, mcfg->id.module_id);

	return 0;
}
enum skl_bitdepth skl_get_bit_depth(int params)
{
	switch (params) {
	case 8:
		return SKL_DEPTH_8BIT;

	case 16:
		return SKL_DEPTH_16BIT;

	case 24:
		return SKL_DEPTH_24BIT;

	case 32:
		return SKL_DEPTH_32BIT;

	default:
		return SKL_DEPTH_INVALID;

	}
}

static struct
skl_module_fmt *skl_get_pin_format(struct skl_module_cfg *mconfig,
					u8 pin_direction, u8 pin_idx)
{
	struct skl_module *module = mconfig->module;
	u8 fmt_idx = mconfig->fmt_idx;
	struct skl_module_intf *intf = &module->formats[fmt_idx];
	struct skl_module_fmt *pin_fmt;

	if (pin_direction == INPUT_PIN)
		pin_fmt = &intf->input[pin_idx].pin_fmt;
	else
		pin_fmt = &intf->output[pin_idx].pin_fmt;

	return pin_fmt;
}

/*
 * Each module in DSP expects a base module configuration, which consists of
 * PCM format information, which we calculate in driver and resource values
 * which are read from widget information passed through topology binary
 * This is send when we create a module with INIT_INSTANCE IPC msg
 */
static void skl_set_base_module_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_base_cfg *base_cfg)
{
	struct skl_module *module = mconfig->module;
	u8 res_idx = mconfig->res_idx;
	struct skl_module_res *res = &module->resources[res_idx];
	struct  skl_module_fmt *format;

	format = skl_get_pin_format(mconfig, INPUT_PIN, 0);

	base_cfg->audio_fmt.number_of_channels = format->channels;

	base_cfg->audio_fmt.s_freq = format->s_freq;
	base_cfg->audio_fmt.bit_depth = format->bit_depth;
	base_cfg->audio_fmt.valid_bit_depth = format->valid_bit_depth;
	base_cfg->audio_fmt.ch_cfg = format->ch_cfg;
	base_cfg->audio_fmt.sample_type = format->sample_type;

	dev_dbg(ctx->dev, "bit_depth=%x valid_bd=%x ch_config=%x\n",
			format->bit_depth, format->valid_bit_depth,
			format->ch_cfg);

	base_cfg->audio_fmt.channel_map = format->ch_map;

	base_cfg->audio_fmt.interleaving = format->interleaving_style;

	base_cfg->cps = res->cpc;
	base_cfg->ibs = res->ibs;
	base_cfg->obs = res->obs;
	base_cfg->is_pages = res->is_pages;
}

static void skl_fill_module_pin_format(struct skl_pin_format *pin_fmt,
				struct skl_module_pin_resources *res,
				struct skl_module_fmt *intf_fmt)
{
	pin_fmt->pin_index = res->pin_index;
	pin_fmt->buff_size = res->buf_size;

	pin_fmt->audio_fmt.s_freq = intf_fmt->s_freq;
	pin_fmt->audio_fmt.bit_depth = intf_fmt->bit_depth;
	pin_fmt->audio_fmt.channel_map = intf_fmt->ch_map;
	pin_fmt->audio_fmt.ch_cfg = intf_fmt->ch_cfg;
	pin_fmt->audio_fmt.interleaving = intf_fmt->interleaving_style;
	pin_fmt->audio_fmt.number_of_channels = intf_fmt->channels;
	pin_fmt->audio_fmt.valid_bit_depth = intf_fmt->valid_bit_depth;
	pin_fmt->audio_fmt.sample_type = intf_fmt->sample_type;
}

/* Non-infrastructure modules are capable of supporting heterogenous
 * pins. These modules needs to be initialized with Extended base
 * module config IPC.
 */
static int skl_set_ext_algo_format(struct skl_sst *ctx,
				struct skl_module_cfg *mconfig,
				void *param_data)
{
	struct skl_module *mod_data = mconfig->module;
	struct skl_module_res *res = &mod_data->resources[mconfig->res_idx];
	struct skl_module_intf *intf = &mod_data->formats[mconfig->fmt_idx];
	struct skl_pin_format *pin_fmt;
	struct skl_module_pin_resources *pin_res;
	struct skl_module_fmt *intf_fmt;
	struct skl_base_cfg_ext *ext_cfg;
	int i;

	ext_cfg = (struct skl_base_cfg_ext *)param_data;
	skl_set_base_module_format(ctx, mconfig, &ext_cfg->base_cfg);

	if (res->nr_input_pins != intf->nr_input_fmt) {
		dev_err(ctx->dev, "res and intf ip pins mismatch: %d, %d %d",
				mconfig->id.module_id, res->nr_input_pins,
				intf->nr_input_fmt);
		return -EINVAL;
	}
	if (res->nr_output_pins != intf->nr_output_fmt) {
		dev_err(ctx->dev, "res and intf op pins mismatch: %d, %d %d",
				mconfig->id.module_id, res->nr_output_pins,
				intf->nr_output_fmt);
		return -EINVAL;
	}

	ext_cfg->nr_input_pins = res->nr_input_pins;
	ext_cfg->nr_output_pins = res->nr_output_pins;
	pin_fmt = (struct skl_pin_format *)&ext_cfg->pin_formats;

	for (i = 0; i < res->nr_input_pins; i++, pin_fmt++) {
		pin_res = &res->input[i];
		intf_fmt = &intf->input[i].pin_fmt;

		skl_fill_module_pin_format(pin_fmt, pin_res, intf_fmt);
	}

	for (i = 0; i < res->nr_output_pins; i++, pin_fmt++) {
		pin_res = &res->output[i];
		intf_fmt = &intf->output[i].pin_fmt;

		skl_fill_module_pin_format(pin_fmt, pin_res, intf_fmt);
	}

	if (mconfig->formats_config[SKL_PARAM_INIT].caps_size != 0) {
		memcpy(ext_cfg->params,
			mconfig->formats_config[SKL_PARAM_INIT].caps,
			mconfig->formats_config[SKL_PARAM_INIT].caps_size);
		ext_cfg->priv_param_length =
			mconfig->formats_config[SKL_PARAM_INIT].caps_size;
	}

	return 0;
}

/*
 * Algo module are DSP pre processing modules. Algo
 * module take base module configuration and params
 */
static int skl_set_base_algo_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_algo_cfg *algo_mcfg)
{
	struct skl_base_cfg *base_cfg = (struct skl_base_cfg *)algo_mcfg;

	skl_set_base_module_format(ctx, mconfig, base_cfg);

	if (mconfig->formats_config[SKL_PARAM_INIT].caps_size == 0)
		return 0;

	memcpy(algo_mcfg->params,
			mconfig->formats_config[SKL_PARAM_INIT].caps,
			mconfig->formats_config[SKL_PARAM_INIT].caps_size);

	return 0;
}

static int skl_set_algo_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			void *param_data)
{
	struct skl_module *mod_data = mconfig->module;
	struct skl_module_res *res = &mod_data->resources[mconfig->res_idx];
	u8 module_pins;
	int ret;

	/*
	 * Non-infrastructure modules might support heterogenous
	 * pins. If module instance has pin specific resources
	 * then compose extended message.
	 */
	module_pins = res->nr_input_pins + res->nr_output_pins;
	if (module_pins)
		ret = skl_set_ext_algo_format(ctx, mconfig, param_data);
	else
		ret = skl_set_base_algo_format(ctx, mconfig, param_data);

	return ret;
}

/*
 * Copies copier capabilities into copier module and updates copier module
 * config size.
 */
static void skl_copy_copier_caps(struct skl_module_cfg *mconfig,
				struct skl_cpr_cfg *cpr_mconfig)
{
	if (mconfig->formats_config[SKL_PARAM_INIT].caps_size == 0)
		return;

	memcpy(cpr_mconfig->gtw_cfg.config_data,
			mconfig->formats_config[SKL_PARAM_INIT].caps,
			mconfig->formats_config[SKL_PARAM_INIT].caps_size);

	cpr_mconfig->gtw_cfg.config_length =
			(mconfig->formats_config[SKL_PARAM_INIT].caps_size) / 4;
}

#define SKL_NON_GATEWAY_CPR_NODE_ID 0xFFFFFFFF
/*
 * Calculate the gatewat settings required for copier module, type of
 * gateway and index of gateway to use
 */
static void skl_setup_cpr_gateway_cfg(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_cpr_cfg *cpr_mconfig)
{
	union skl_connector_node_id node_id = {0};
	union skl_ssp_dma_node ssp_node  = {0};
	struct skl_pipe_params *params = mconfig->pipe->p_params;
	struct skl_module_res *res;

	switch (mconfig->dev_type) {
	case SKL_DEVICE_BT:
		node_id.node.dma_type =
			(SKL_CONN_SOURCE == mconfig->hw_conn_type) ?
			SKL_DMA_I2S_LINK_OUTPUT_CLASS :
			SKL_DMA_I2S_LINK_INPUT_CLASS;
		node_id.node.vindex = params->host_dma_id +
					(mconfig->vbus_id << 3);
		break;

	case SKL_DEVICE_I2S:
		node_id.node.dma_type =
			(SKL_CONN_SOURCE == mconfig->hw_conn_type) ?
			SKL_DMA_I2S_LINK_OUTPUT_CLASS :
			SKL_DMA_I2S_LINK_INPUT_CLASS;
		ssp_node.dma_node.time_slot_index = mconfig->time_slot;
		ssp_node.dma_node.i2s_instance = mconfig->vbus_id;
		node_id.node.vindex = ssp_node.val;
		break;

	case SKL_DEVICE_DMIC:
		node_id.node.dma_type = SKL_DMA_DMIC_LINK_INPUT_CLASS;
		node_id.node.vindex = mconfig->vbus_id +
					 (mconfig->time_slot);
		break;

	case SKL_DEVICE_HDALINK:
		node_id.node.dma_type =
			(SKL_CONN_SOURCE == mconfig->hw_conn_type) ?
			SKL_DMA_HDA_LINK_OUTPUT_CLASS :
			SKL_DMA_HDA_LINK_INPUT_CLASS;
		node_id.node.vindex = params->link_dma_id;
		break;

	case SKL_DEVICE_HDAHOST:
		node_id.node.dma_type =
			(SKL_CONN_SOURCE == mconfig->hw_conn_type) ?
			SKL_DMA_HDA_HOST_OUTPUT_CLASS :
			SKL_DMA_HDA_HOST_INPUT_CLASS;
		node_id.node.vindex = params->host_dma_id;
		break;

	default:
		cpr_mconfig->gtw_cfg.node_id = SKL_NON_GATEWAY_CPR_NODE_ID;
		cpr_mconfig->cpr_feature_mask = mconfig->fast_mode;
		return;
	}

	cpr_mconfig->gtw_cfg.node_id = node_id.val;
	res = &mconfig->module->resources[mconfig->res_idx];
	cpr_mconfig->gtw_cfg.dma_buffer_size = res->dma_buffer_size;
	cpr_mconfig->cpr_feature_mask = mconfig->fast_mode;
	cpr_mconfig->gtw_cfg.config_length  = 0;

	skl_copy_copier_caps(mconfig, cpr_mconfig);
}

static u32 skl_get_i2s_node_id(u32 instance, u8 dev_type,
				u32 dir, u32 time_slot)
{
	union skl_connector_node_id node_id = {0};
	union skl_ssp_dma_node ssp_node  = {0};

	node_id.node.dma_type = (dir == SNDRV_PCM_STREAM_PLAYBACK) ?
					SKL_DMA_I2S_LINK_OUTPUT_CLASS :
					SKL_DMA_I2S_LINK_INPUT_CLASS;
	ssp_node.dma_node.time_slot_index = time_slot;
	ssp_node.dma_node.i2s_instance = instance;
	node_id.node.vindex = ssp_node.val;

	return node_id.val;
}

#define DMA_CONTROL_ID 5

static int skl_dsp_set_dma_control(struct skl_sst *ctx, u32 *caps,
				u32 caps_size, u32 node_id, u32 blob_size)
{
	struct skl_dma_control *dma_ctrl;
	struct skl_ipc_large_config_msg msg = {0};
	int err = 0;

	/*
	 * if blob size is same as capablity size, then no dma control
	 * present so return
	 */
	if (caps_size == blob_size) {
		dev_dbg(ctx->dev, "No dma control included\n");
		return 0;
	}
	msg.large_param_id = DMA_CONTROL_ID;
	msg.param_data_size = sizeof(struct skl_dma_control) + caps_size;
	msg.module_id = 0;
	msg.instance_id = 0;

	dma_ctrl = kzalloc(msg.param_data_size, GFP_KERNEL);
	if (dma_ctrl == NULL)
		return -ENOMEM;

	dma_ctrl->node_id = node_id;

	/* size in dwords */
	dma_ctrl->config_length = blob_size / 4;

	memcpy(dma_ctrl->config_data, caps, caps_size);

	err = skl_ipc_set_large_config(&ctx->ipc, &msg, (u32 *)dma_ctrl);

	kfree(dma_ctrl);

	return err;
}

int skl_dsp_set_dma_clk_controls(struct skl_sst *ctx)
{
	struct skl_dfw_dma_ctrl_hdr *hdr;
	struct nhlt_specific_cfg *cfg = NULL;
	struct skl_manifest *minfo = &ctx->manifest;
	struct skl *skl = get_skl_ctx(ctx->dev);
	struct hdac_ext_bus *ebus = &skl->ebus;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	u8 *frame_ptr, *dma_ctrl_config;
	void *i2s_config;
	u32 sz_remaining, i2s_config_size;
	u32 node_id;
	int err;

	frame_ptr = (u8 *)&minfo->dma_ctrl + (2 * sizeof(u32));
	sz_remaining = minfo->dma_ctrl.size;

	if (sz_remaining) {
		while (sz_remaining != 0) {
			hdr = (struct skl_dfw_dma_ctrl_hdr *)frame_ptr;

			/* get nhlt specific config info */
			cfg = skl_get_nhlt_specific_cfg(skl, hdr->vbus_id,
						NHLT_LINK_SSP, hdr->fmt,
						hdr->chan, hdr->freq,
						hdr->direction);

			if (cfg && hdr->data_size) {

				i2s_config_size = cfg->size + hdr->data_size;

				i2s_config =
					kzalloc(i2s_config_size, GFP_KERNEL);
				if (i2s_config == NULL) {
					pm_runtime_put_sync(bus->dev);
					return -ENOMEM;
				}

				/* copy blob */
				memcpy(i2s_config, cfg->caps, cfg->size);

				/* copy additional dma controls informatioin */
				dma_ctrl_config = (u8 *)i2s_config + cfg->size;
				memcpy(dma_ctrl_config, hdr->data,
							hdr->data_size);

				print_hex_dump(KERN_DEBUG,
					"Blob + DMA Control Info:",
					DUMP_PREFIX_OFFSET, 8, 4, i2s_config,
					i2s_config_size, false);

				/* get node id */
				node_id = skl_get_i2s_node_id(hdr->vbus_id,
								SKL_DEVICE_I2S,
								hdr->direction,
								hdr->tdm_slot);

				err = skl_dsp_set_dma_control(ctx,
						(u32 *)i2s_config,
						i2s_config_size, node_id, cfg->size);
				if (err < 0) {
					dev_err(ctx->dev,
					"Failed to set dma controls: %d\n", err);
					kfree(i2s_config);
					pm_runtime_put_sync(bus->dev);
					return err;
				}

				kfree(i2s_config);

			} else {
				dev_err(ctx->dev,
				"Failed to load NHLT specific Configuration\n");
				pm_runtime_put_sync(bus->dev);
				return -EIO;
			}

			sz_remaining -= sizeof(struct skl_dfw_dma_ctrl_hdr) +
							hdr->data_size;
			frame_ptr += sizeof(struct skl_dfw_dma_ctrl_hdr) +
							hdr->data_size;
		}
	} else {
		pm_runtime_put_sync(bus->dev);
	}

	return 0;
}

static void skl_setup_out_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_audio_data_format *out_fmt)
{
	struct  skl_module_fmt *format;

	format = skl_get_pin_format(mconfig, OUTPUT_PIN, 0);

	out_fmt->number_of_channels = (u8)format->channels;
	out_fmt->s_freq = format->s_freq;
	out_fmt->bit_depth = format->bit_depth;
	out_fmt->valid_bit_depth = format->valid_bit_depth;
	out_fmt->ch_cfg = format->ch_cfg;

	out_fmt->channel_map = format->ch_map;
	out_fmt->interleaving = format->interleaving_style;
	out_fmt->sample_type = format->sample_type;

	dev_dbg(ctx->dev, "copier out format chan=%d fre=%d bitdepth=%d\n",
		out_fmt->number_of_channels, format->s_freq, format->bit_depth);
}
#define SKL_ENABLE_ALL_CHANNELS  0xffffffff

static int skl_set_gain_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_gain_module_config *gain_mconfig)
{
	struct skl_gain_data *gain_fmt = &mconfig->gain_data;

	skl_set_base_module_format(ctx, mconfig,
			(struct skl_base_cfg *)gain_mconfig);
	gain_mconfig->gain_cfg.channel_id = SKL_ENABLE_ALL_CHANNELS;
	gain_mconfig->gain_cfg.target_volume = gain_fmt->volume[0];
	gain_mconfig->gain_cfg.ramp_type = gain_fmt->ramp_type;
	gain_mconfig->gain_cfg.ramp_duration = gain_fmt->ramp_duration;

	return 0;
}

/*
 * DSP needs SRC module for frequency conversion, SRC takes base module
 * configuration and the target frequency as extra parameter passed as src
 * config
 */
static void skl_set_src_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_src_module_cfg *src_mconfig)
{
	struct  skl_module_fmt *fmt;

	fmt = skl_get_pin_format(mconfig, OUTPUT_PIN, 0);

	skl_set_base_module_format(ctx, mconfig,
		(struct skl_base_cfg *)src_mconfig);

	src_mconfig->src_cfg = fmt->s_freq;

	if (mconfig->m_type == SKL_MODULE_TYPE_ASRC) {
		if (mconfig->pipe->p_params->stream ==
				SNDRV_PCM_STREAM_PLAYBACK)
			src_mconfig->mode = ASRC_MODE_DOWNLINK;
		else
			src_mconfig->mode = ASRC_MODE_UPLINK;
	}
}

/*
 * DSP needs updown module to do channel conversion. updown module take base
 * module configuration and channel configuration
 * It also take coefficients and now we have defaults applied here
 */
static void skl_set_updown_mixer_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_up_down_mixer_cfg *mixer_mconfig)
{
	struct  skl_module_fmt *fmt;
	int i = 0;

	fmt = skl_get_pin_format(mconfig, OUTPUT_PIN, 0);

	skl_set_base_module_format(ctx,	mconfig,
		(struct skl_base_cfg *)mixer_mconfig);
	mixer_mconfig->out_ch_cfg = fmt->ch_cfg;

	/* Select F/W default coefficient */
	mixer_mconfig->coeff_sel = 0x0;
	mixer_mconfig->ch_map = fmt->ch_map;

	/* User coeff, don't care since we are selecting F/W defaults */
	for (i = 0; i < UP_DOWN_MIXER_MAX_COEFF; i++)
		mixer_mconfig->coeff[i] = 0x0;
}

/*
 * 'copier' is DSP internal module which copies data from Host DMA (HDA host
 * dma) or link (hda link, SSP, PDM)
 * Here we calculate the copier module parameters, like PCM format, output
 * format, gateway settings
 * copier_module_config is sent as input buffer with INIT_INSTANCE IPC msg
 */
static void skl_set_copier_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_cpr_cfg *cpr_mconfig)
{
	struct skl_audio_data_format *out_fmt = &cpr_mconfig->out_fmt;
	struct skl_base_cfg *base_cfg = (struct skl_base_cfg *)cpr_mconfig;

	skl_set_base_module_format(ctx, mconfig, base_cfg);

	skl_setup_out_format(ctx, mconfig, out_fmt);
	skl_setup_cpr_gateway_cfg(ctx, mconfig, cpr_mconfig);
}

static void skl_setup_probe_gateway_cfg(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_probe_cfg *probe_cfg)
{
	union skl_connector_node_id node_id = {0};
	struct skl_module_res *res;
	struct skl_probe_config *pconfig = &ctx->probe_config;

	res = &mconfig->module->resources[mconfig->res_idx];

	pconfig->edma_buffsize = res->dma_buffer_size;

	node_id.node.dma_type = pconfig->edma_type;
	node_id.node.vindex = pconfig->edma_id;
	probe_cfg->prb_cfg.dma_buffer_size = pconfig->edma_buffsize;

	memcpy(&(probe_cfg->prb_cfg.node_id), &node_id, sizeof(u32));
}

static void skl_set_probe_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_probe_cfg *probe_mconfig)
{
	struct skl_base_cfg *base_cfg = (struct skl_base_cfg *)probe_mconfig;

	skl_set_base_module_format(ctx, mconfig, base_cfg);
	skl_setup_probe_gateway_cfg(ctx, mconfig, probe_mconfig);
}

/*
 * Some modules require base and out format configuration like mic
 * select module. These module take base module configuration and
 * out format configuration
 */
static void skl_set_base_outfmt_format(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig,
			struct skl_base_outfmt_cfg *base_outfmt_mcfg)
{
	struct skl_audio_data_format *out_fmt = &base_outfmt_mcfg->out_fmt;
	struct skl_base_cfg *base_cfg = (struct skl_base_cfg *)base_outfmt_mcfg;

	skl_set_base_module_format(ctx, mconfig, base_cfg);

	skl_setup_out_format(ctx, mconfig, out_fmt);
}

static u16 skl_get_module_param_size(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig)
{
	struct skl_module *mod_data = mconfig->module;
	struct skl_module_res *res = &mod_data->resources[mconfig->res_idx];
	u16 param_size, module_pins;

	switch (mconfig->m_type) {
	case SKL_MODULE_TYPE_COPIER:
		param_size = sizeof(struct skl_cpr_cfg);
		param_size += mconfig->formats_config[SKL_PARAM_INIT].caps_size;
		return param_size;

	case SKL_MODULE_TYPE_PROBE:
		return sizeof(struct skl_probe_cfg);

	case SKL_MODULE_TYPE_SRCINT:
	case SKL_MODULE_TYPE_ASRC:
		return sizeof(struct skl_src_module_cfg);

	case SKL_MODULE_TYPE_UPDWMIX:
		return sizeof(struct skl_up_down_mixer_cfg);

	case SKL_MODULE_TYPE_ALGO:
		/*
		 * add ext base cfg size if pin resources are found
		 */
		module_pins = res->nr_input_pins + res->nr_output_pins;
		if (module_pins) {
			param_size = sizeof(struct skl_base_cfg_ext) +
				(sizeof(struct skl_pin_format) * module_pins);
		} else
			param_size = sizeof(struct skl_base_cfg);

		param_size += mconfig->formats_config[SKL_PARAM_INIT].caps_size;
		return param_size;

	case SKL_MODULE_TYPE_GAIN:
		return sizeof(struct skl_gain_module_config);

	case SKL_MODULE_TYPE_MIC_SELECT:
		return sizeof(struct skl_base_outfmt_cfg);

	case SKL_MODULE_TYPE_MIXER:
		return sizeof(struct skl_base_cfg);

	default:
		/*
		 * return size of base cfg ext when no specific module
		 * type is specified
		 */
		module_pins = res->nr_input_pins + res->nr_output_pins;
		param_size =  sizeof(struct skl_base_cfg_ext) +
			(sizeof(struct skl_pin_format) * module_pins);
		param_size += mconfig->formats_config[SKL_PARAM_INIT].caps_size;
		return param_size;
	}

	return 0;
}

/*
 * DSP firmware supports various modules like copier, SRC, updown etc.
 * These modules required various parameters to be calculated and sent for
 * the module initialization to DSP. By default a generic module needs only
 * base module format configuration
 */

static int skl_set_module_format(struct skl_sst *ctx,
			struct skl_module_cfg *module_config,
			u16 *module_config_size,
			void **param_data)
{
	int ret = 0;
	u16 param_size;

	param_size  = skl_get_module_param_size(ctx, module_config);

	*param_data = kzalloc(param_size, GFP_KERNEL);
	if (NULL == *param_data)
		return -ENOMEM;

	*module_config_size = param_size;

	switch (module_config->m_type) {
	case SKL_MODULE_TYPE_COPIER:
		skl_set_copier_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_PROBE:
		skl_set_probe_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_SRCINT:
	case SKL_MODULE_TYPE_ASRC:
		skl_set_src_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_UPDWMIX:
		skl_set_updown_mixer_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_ALGO:
		ret = skl_set_algo_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_GAIN:
		skl_set_gain_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_MIC_SELECT:
		skl_set_base_outfmt_format(ctx, module_config, *param_data);
		break;

	case SKL_MODULE_TYPE_MIXER:
		skl_set_base_module_format(ctx, module_config, *param_data);
		break;

	default:
		/* Send extended base configuration. If a module accepts
		 * base module config alone, it will ignore extra information.
		 */
		ret = skl_set_ext_algo_format(ctx, module_config, *param_data);
		break;
	}

	dev_dbg(ctx->dev, "Module id = %d, type=%d config size: %d bytes\n",
			module_config->id.module_id, module_config->m_type, param_size);
	return ret;
}

static int skl_get_queue_index(struct skl_module_pin *mpin,
				struct skl_module_inst_id id, int max)
{
	int i;

	for (i = 0; i < max; i++)  {
		if (mpin[i].id.module_id == id.module_id &&
			mpin[i].id.instance_id == id.instance_id)
			return i;
	}

	return -EINVAL;
}

/*
 * Allocates queue for each module.
 * if dynamic, the pin_index is allocated 0 to max_pin.
 * In static, the pin_index is fixed based on module_id and instance id
 */
static int skl_alloc_queue(struct skl_module_pin *mpin,
			struct skl_module_cfg *tgt_cfg, int max)
{
	int i;
	struct skl_module_inst_id id = tgt_cfg->id;
	/*
	 * if pin in dynamic, find first free pin
	 * otherwise find match module and instance id pin as topology will
	 * ensure a unique pin is assigned to this so no need to
	 * allocate/free
	 */
	for (i = 0; i < max; i++)  {
		if (mpin[i].is_dynamic) {
			if (!mpin[i].in_use &&
				mpin[i].pin_state == SKL_PIN_UNBIND) {

				mpin[i].in_use = true;
				mpin[i].id.module_id = id.module_id;
				mpin[i].id.instance_id = id.instance_id;
				mpin[i].tgt_mcfg = tgt_cfg;
				return i;
			}
		} else {
			if (mpin[i].id.module_id == id.module_id &&
				mpin[i].id.instance_id == id.instance_id &&
				mpin[i].pin_state == SKL_PIN_UNBIND) {

				mpin[i].tgt_mcfg = tgt_cfg;
				return i;
			}
		}
	}

	return -EINVAL;
}

static void skl_free_queue(struct skl_module_pin *mpin, int q_index)
{
	if (mpin[q_index].is_dynamic) {
		mpin[q_index].in_use = false;
		mpin[q_index].id.module_id = 0;
		mpin[q_index].id.instance_id = 0;
	}
	mpin[q_index].pin_state = SKL_PIN_UNBIND;
	mpin[q_index].tgt_mcfg = NULL;
}

/* Module state will be set to unint, if all the out pin state is UNBIND */

static void skl_clear_module_state(struct skl_module_pin *mpin, int max,
						struct skl_module_cfg *mcfg)
{
	int i;
	bool found = false;

	for (i = 0; i < max; i++)  {
		if (mpin[i].pin_state == SKL_PIN_UNBIND)
			continue;
		found = true;
		break;
	}

	if (!found)
		mcfg->m_state = SKL_MODULE_INIT_DONE;
	return;
}

/*
 * A module needs to be instanataited in DSP. A mdoule is present in a
 * collection of module referred as a PIPE.
 * We first calculate the module format, based on module type and then
 * invoke the DSP by sending IPC INIT_INSTANCE using ipc helper
 */
int skl_init_module(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig)
{
	u16 module_config_size = 0;
	void *param_data = NULL;
	int ret;
	struct skl_ipc_init_instance_msg msg;

	dev_dbg(ctx->dev, "%s: module_id = %d instance=%d\n", __func__,
		 mconfig->id.module_id, mconfig->id.instance_id);

	if (mconfig->pipe->state != SKL_PIPE_CREATED) {
		dev_err(ctx->dev, "Pipe not created state= %d pipe_id= %d\n",
				 mconfig->pipe->state, mconfig->pipe->ppl_id);
		return -EIO;
	}

	ret = skl_set_module_format(ctx, mconfig,
			&module_config_size, &param_data);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to set module format ret=%d\n", ret);
		kfree(param_data);
		return ret;
	}

	msg.module_id = mconfig->id.module_id;
	msg.instance_id = mconfig->id.instance_id;
	msg.ppl_instance_id = mconfig->pipe->ppl_id;
	msg.param_data_size = module_config_size;
	msg.core_id = mconfig->core_id;
	msg.domain = mconfig->domain;

	ret = skl_ipc_init_instance(&ctx->ipc, &msg, param_data);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to init instance ret=%d\n", ret);
		kfree(param_data);
		return ret;
	}
	mconfig->m_state = SKL_MODULE_INIT_DONE;
	kfree(param_data);
	return ret;
}

int skl_init_probe_module(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig)
{
	u16 module_config_size = 0;
	void *param_data = NULL;
	int ret;
	struct skl_ipc_init_instance_msg msg;

	dev_dbg(ctx->dev, "%s: module_id = %d instance=%d\n", __func__,
		 mconfig->id.module_id, mconfig->id.instance_id);

	ret = snd_skl_get_module_info(ctx, mconfig);
	if(ret < 0)
		return ret;

	ret = skl_set_module_format(ctx, mconfig,
			&module_config_size, &param_data);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to set module format ret=%d\n", ret);
		kfree(param_data);
		return ret;
	}

	msg.module_id = mconfig->id.module_id;
	msg.instance_id = mconfig->id.instance_id;
	msg.ppl_instance_id = -1;
	msg.param_data_size = module_config_size;
	msg.core_id = mconfig->core_id;
	msg.domain = mconfig->domain;

	ret = skl_ipc_init_instance(&ctx->ipc, &msg, param_data);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to init instance ret=%d\n", ret);
		kfree(param_data);
		return ret;
	}
	mconfig->m_state = SKL_MODULE_INIT_DONE;
	kfree(param_data);
	return ret;
}

int skl_uninit_probe_module(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig)
{
	u16 module_config_size = 0;
	int ret;
	struct skl_ipc_init_instance_msg msg;

	dev_dbg(ctx->dev, "%s: module_id = %d instance=%d\n", __func__,
		 mconfig->id.module_id, mconfig->id.instance_id);

	msg.module_id = mconfig->id.module_id;
	msg.instance_id = mconfig->id.instance_id;
	msg.ppl_instance_id = -1;
	msg.param_data_size = module_config_size;
	msg.core_id = mconfig->core_id;
	msg.domain = mconfig->domain;

	ret = skl_ipc_delete_instance(&ctx->ipc, &msg);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to delete instance ret=%d\n", ret);
		return ret;
	}
	mconfig->m_state = SKL_MODULE_UNINIT;

	return ret;
}

static void skl_dump_bind_info(struct skl_sst *ctx, struct skl_module_cfg
	*src_module, struct skl_module_cfg *dst_module)
{
	dev_dbg(ctx->dev, "%s: src module_id = %d  src_instance=%d\n",
		__func__, src_module->id.module_id, src_module->id.instance_id);
	dev_dbg(ctx->dev, "%s: dst_module=%d dst_instacne=%d\n", __func__,
		 dst_module->id.module_id, dst_module->id.instance_id);

	dev_dbg(ctx->dev, "src_module state = %d dst module state = %d\n",
		src_module->m_state, dst_module->m_state);
}

int skl_probe_point_disconnect_ext(struct skl_sst *ctx,
				struct snd_soc_dapm_widget *w)
{
	struct skl_ipc_large_config_msg msg;
	struct skl_probe_config *pconfig = &ctx->probe_config;
	struct skl_module_cfg *mcfg;
	u32 probe_point[NO_OF_EXTRACTOR] = {0};
	int store_prb_pt_index[NO_OF_EXTRACTOR] = {0};
	int n = 0, i;
	int ret = 0;
	int no_of_extractor = pconfig->no_extractor;

	dev_dbg(ctx->dev, "Disconnecting extractor probe points\n");
	mcfg = w->priv;
	msg.module_id = mcfg->id.module_id;
	msg.instance_id = mcfg->id.instance_id;
	msg.large_param_id = SKL_PROBE_DISCONNECT;

	for (i = 0; i < no_of_extractor; i++) {
		if (pconfig->eprobe[i].state == SKL_PROBE_STATE_EXT_CONNECTED) {
			probe_point[n] = pconfig->eprobe[i].probe_point_id;
			store_prb_pt_index[i] = 1;
			n++;
		}
	}
	if (n == 0)
		return ret;

	msg.param_data_size = n * sizeof(u32);
	dev_dbg(ctx->dev, "setting module params size=%d\n", msg.param_data_size);
	ret = skl_ipc_set_large_config(&ctx->ipc, &msg, probe_point);
	if (ret < 0)
		return -EINVAL;

	for (i = 0; i < pconfig->no_extractor; i++) {
		if (store_prb_pt_index[i]) {
			pconfig->eprobe[i].state = SKL_PROBE_STATE_EXT_NONE;
			dev_dbg(ctx->dev, "eprobe[%d].state %d\n", i, pconfig->eprobe[i].state);
		}
	}

	return ret;
}

int skl_probe_point_disconnect_inj(struct skl_sst *ctx,
				struct snd_soc_dapm_widget *w, int index)
{
	struct skl_ipc_large_config_msg msg;
	struct skl_probe_config *pconfig = &ctx->probe_config;
	struct skl_module_cfg *mcfg;
	u32 probe_point = 0;
	int ret = 0;

	if (pconfig->iprobe[index].state == SKL_PROBE_STATE_INJ_CONNECTED) {
		dev_dbg(ctx->dev, "Disconnecting injector probe point\n");
		mcfg = w->priv;
		msg.module_id = mcfg->id.module_id;
		msg.instance_id = mcfg->id.instance_id;
		msg.large_param_id = SKL_PROBE_DISCONNECT;
		probe_point = pconfig->iprobe[index].probe_point_id;
		msg.param_data_size = sizeof(u32);

		dev_dbg(ctx->dev, "setting module params size=%d\n", msg.param_data_size);
		ret = skl_ipc_set_large_config(&ctx->ipc, &msg, &probe_point);
		if (ret < 0)
			return -EINVAL;

		pconfig->iprobe[index].state = SKL_PROBE_STATE_INJ_DISCONNECTED;
		dev_dbg(ctx->dev, "iprobe[%d].state %d\n", index, pconfig->iprobe[index].state);
	}

	return ret;

}
/*
 * On module freeup, we need to unbind the module with modules
 * it is already bind.
 * Find the pin allocated and unbind then using bind_unbind IPC
 */
int skl_unbind_modules(struct skl_sst *ctx,
			struct skl_module_cfg *src_mcfg,
			struct skl_module_cfg *dst_mcfg)
{
	struct skl_ipc_bind_unbind_msg msg;
	struct skl_module_inst_id src_id = src_mcfg->id;
	struct skl_module_inst_id dst_id = dst_mcfg->id;
	int in_max, out_max, ret;
	int src_index, dst_index, src_pin_state, dst_pin_state;

	if (!dst_mcfg->module || !src_mcfg->module)
		return -EINVAL;

	in_max = dst_mcfg->module->max_input_pins;
	out_max = src_mcfg->module->max_output_pins;

	skl_dump_bind_info(ctx, src_mcfg, dst_mcfg);

	/* get src queue index */
	src_index = skl_get_queue_index(src_mcfg->m_out_pin, dst_id, out_max);
	if (src_index < 0)
		return 0;

	msg.src_queue = src_index;

	/* get dst queue index */
	dst_index  = skl_get_queue_index(dst_mcfg->m_in_pin, src_id, in_max);
	if (dst_index < 0)
		return 0;

	msg.dst_queue = dst_index;

	src_pin_state = src_mcfg->m_out_pin[src_index].pin_state;
	dst_pin_state = dst_mcfg->m_in_pin[dst_index].pin_state;

	if (src_pin_state != SKL_PIN_BIND_DONE ||
		dst_pin_state != SKL_PIN_BIND_DONE)
		return 0;

	msg.module_id = src_mcfg->id.module_id;
	msg.instance_id = src_mcfg->id.instance_id;
	msg.dst_module_id = dst_mcfg->id.module_id;
	msg.dst_instance_id = dst_mcfg->id.instance_id;
	msg.bind = false;

	ret = skl_ipc_bind_unbind(&ctx->ipc, &msg);
	if (!ret) {
		/* free queue only if unbind is success */
		skl_free_queue(src_mcfg->m_out_pin, src_index);
		skl_free_queue(dst_mcfg->m_in_pin, dst_index);

		/*
		 * check only if src module bind state, bind is
		 * always from src -> sink
		 */
		skl_clear_module_state(src_mcfg->m_out_pin, out_max, src_mcfg);
	}

	return ret;
}

/*
 * Once a module is instantiated it need to be 'bind' with other modules in
 * the pipeline. For binding we need to find the module pins which are bind
 * together
 * This function finds the pins and then sends bund_unbind IPC message to
 * DSP using IPC helper
 */
int skl_bind_modules(struct skl_sst *ctx,
			struct skl_module_cfg *src_mcfg,
			struct skl_module_cfg *dst_mcfg)
{
	struct skl_ipc_bind_unbind_msg msg;
	int in_max, out_max, ret;
	int src_index, dst_index;

	if (!src_mcfg->module || !dst_mcfg->module)
		return -EINVAL;

	in_max = dst_mcfg->module->max_input_pins;
	out_max = src_mcfg->module->max_output_pins;

	skl_dump_bind_info(ctx, src_mcfg, dst_mcfg);

	if (src_mcfg->m_state < SKL_MODULE_INIT_DONE ||
		dst_mcfg->m_state < SKL_MODULE_INIT_DONE)
		return 0;

	src_index = skl_alloc_queue(src_mcfg->m_out_pin, dst_mcfg, out_max);
	if (src_index < 0)
		return -EINVAL;

	msg.src_queue = src_index;
	dst_index = skl_alloc_queue(dst_mcfg->m_in_pin, src_mcfg, in_max);
	if (dst_index < 0) {
		skl_free_queue(src_mcfg->m_out_pin, src_index);
		return -EINVAL;
	}

	msg.dst_queue = dst_index;

	dev_dbg(ctx->dev, "src queue = %d dst queue =%d\n",
			 msg.src_queue, msg.dst_queue);

	msg.module_id = src_mcfg->id.module_id;
	msg.instance_id = src_mcfg->id.instance_id;
	msg.dst_module_id = dst_mcfg->id.module_id;
	msg.dst_instance_id = dst_mcfg->id.instance_id;
	msg.bind = true;

	ret = skl_ipc_bind_unbind(&ctx->ipc, &msg);

	if (!ret) {
		src_mcfg->m_state = SKL_MODULE_BIND_DONE;
		src_mcfg->m_out_pin[src_index].pin_state = SKL_PIN_BIND_DONE;
		dst_mcfg->m_in_pin[dst_index].pin_state = SKL_PIN_BIND_DONE;
	} else {
		/* error case , if IPC fails, clear the queue index */
		skl_free_queue(src_mcfg->m_out_pin, src_index);
		skl_free_queue(dst_mcfg->m_in_pin, dst_index);
	}

	return ret;
}

static int skl_set_pipe_state(struct skl_sst *ctx, struct skl_pipe *pipe,
	enum skl_ipc_pipeline_state state)
{
	dev_dbg(ctx->dev, "%s: pipe_satate = %d\n", __func__, state);

	return skl_ipc_set_pipeline_state(&ctx->ipc, pipe->ppl_id, state);
}

/*
 * A pipeline is a collection of modules. Before a module in instantiated a
 * pipeline needs to be created for it.
 * This function creates pipeline, by sending create pipeline IPC messages
 * to FW
 */
int skl_create_pipeline(struct skl_sst *ctx, struct skl_pipe *pipe)
{
	int ret;

	dev_dbg(ctx->dev, "%s: pipe_id = %d\n", __func__, pipe->ppl_id);

	ret = skl_ipc_create_pipeline(&ctx->ipc, pipe->memory_pages,
				pipe->pipe_priority, pipe->ppl_id,
				pipe->lp_mode);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to create pipeline\n");
		return ret;
	}

	pipe->state = SKL_PIPE_CREATED;

	return 0;
}

/*
 * A pipeline needs to be deleted on cleanup. If a pipeline is running, then
 * pause the pipeline first and then delete it. There is also case in which
 * pipeline needs to be reset before deletion, so always reset as it doesn't
 * change anything in other cases.
 * The pipe delete is done by sending delete pipeline IPC. DSP will stop the
 * DMA engines and releases resources
 */
int skl_delete_pipe(struct skl_sst *ctx, struct skl_pipe *pipe)
{
	int ret;

	dev_dbg(ctx->dev, "%s: pipe = %d\n", __func__, pipe->ppl_id);

	/* If pipe was not created in FW, do not try to delete it */
	if (pipe->state < SKL_PIPE_CREATED)
		return 0;

	/* If pipe is started, do stop the pipe in FW. */
	if (pipe->state > SKL_PIPE_STARTED) {
		ret = skl_set_pipe_state(ctx, pipe, PPL_PAUSED);
		if (ret < 0) {
			dev_err(ctx->dev, "Failed to stop pipeline\n");
			return ret;
		}

		pipe->state = SKL_PIPE_PAUSED;
	}

	/* reset pipe state before deletion */
	ret = skl_set_pipe_state(ctx, pipe, PPL_RESET);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to reset pipe ret=%d\n", ret);
		return ret;
	}

	pipe->state = SKL_PIPE_RESET;

	ret = skl_ipc_delete_pipeline(&ctx->ipc, pipe->ppl_id);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to delete pipeline\n");
		return ret;
	}
	pipe->state = SKL_PIPE_INVALID;
	skl_update_topology_change_event_data(ctx,
			SKL_TPLG_CHG_NOTIF_PIPELINE_STOP);

	return ret;
}

/*
 * A pipeline is also a scheduling entity in DSP which can be run, stopped
 * For processing data the pipe need to be run by sending IPC set pipe state
 * to DSP
 */
int skl_run_pipe(struct skl_sst *ctx, struct skl_pipe *pipe)
{
	int ret;

	dev_dbg(ctx->dev, "%s: pipe = %d\n", __func__, pipe->ppl_id);

	/* If pipe was not created in FW, do not try to pause or delete */
	if (pipe->state < SKL_PIPE_CREATED)
		return 0;

	/* Pipe has to be paused before it is started */
	ret = skl_set_pipe_state(ctx, pipe, PPL_PAUSED);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to pause pipe\n");
		return ret;
	}

	pipe->state = SKL_PIPE_PAUSED;

	ret = skl_set_pipe_state(ctx, pipe, PPL_RUNNING);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to start pipe\n");
		return ret;
	}

	pipe->state = SKL_PIPE_STARTED;

	skl_update_topology_change_event_data(ctx,
		SKL_TPLG_CHG_NOTIF_PIPELINE_START);
	return 0;
}

/*
 * Stop the pipeline by sending set pipe state IPC
 * DSP doesnt implement stop so we always send pause message
 */
int skl_stop_pipe(struct skl_sst *ctx, struct skl_pipe *pipe)
{
	int ret;

	dev_dbg(ctx->dev, "In %s pipe=%d\n", __func__, pipe->ppl_id);

	/* If pipe was not created in FW, do not try to pause or delete */
	if (pipe->state < SKL_PIPE_PAUSED)
		return 0;

	ret = skl_set_pipe_state(ctx, pipe, PPL_PAUSED);
	if (ret < 0) {
		dev_dbg(ctx->dev, "Failed to stop pipe\n");
		return ret;
	}

	pipe->state = SKL_PIPE_PAUSED;

	return 0;
}

/*
 * Reset the pipeline by sending set pipe state IPC this will reset the DMA
 * from the DSP side
 */
int skl_reset_pipe(struct skl_sst *ctx, struct skl_pipe *pipe)
{
	int ret;

	/* If pipe was not created in FW, do not try to pause or delete */
	if (pipe->state < SKL_PIPE_PAUSED)
		return 0;

	ret = skl_set_pipe_state(ctx, pipe, PPL_RESET);
	if (ret < 0) {
		dev_dbg(ctx->dev, "Failed to reset pipe ret=%d\n", ret);
		return ret;
	}

	pipe->state = SKL_PIPE_RESET;

	return 0;
}

/* Algo parameter set helper function */
int skl_set_module_params(struct skl_sst *ctx, u32 *params, int size,
				u32 param_id, struct skl_module_cfg *mcfg)
{
	struct skl_ipc_large_config_msg msg;

	msg.module_id = mcfg->id.module_id;
	msg.instance_id = mcfg->id.instance_id;
	msg.param_data_size = size;
	msg.large_param_id = param_id;

	dev_dbg(ctx->dev, "setting module params size=%d\n", size);
	return skl_ipc_set_large_config(&ctx->ipc, &msg, params);
}

int skl_get_module_params(struct skl_sst *ctx, u32 *params, int size,
			  u32 param_id, struct skl_module_cfg *mcfg)
{
	struct skl_ipc_large_config_msg msg;

	msg.module_id = mcfg->id.module_id;
	msg.instance_id = mcfg->id.instance_id;
	msg.param_data_size = size;
	msg.large_param_id = param_id;

	dev_dbg(ctx->dev, "getting module params size=%d\n", size);
	return skl_ipc_get_large_config(&ctx->ipc, &msg, params, NULL, 0);
}
