/*
 * cnl-sst.c - DSP library functions for CNL platform
 *
 * Copyright (C) 2016-17, Intel Corporation.
 *
 * Author: Guneshwor Singh <guneshwor.o.singh@intel.com>
 *
 * Modified from:
 *	HDA DSP library functions for SKL platform
 *	Copyright (C) 2014-15, Intel Corporation.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/sdw_bus.h>
#include <linux/sdw/sdw_cnl.h>
#include <asm/set_memory.h>

#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "../common/sst-ipc.h"
#include "cnl-sst-dsp.h"
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-fwlog.h"

#define CNL_FW_ROM_INIT		0x1
#define CNL_FW_INIT		0x5
#define CNL_IPC_PURGE		0x01004000
#define CNL_INIT_TIMEOUT	300
#define CNL_BASEFW_TIMEOUT	3000

#define CNL_ADSP_SRAM0_BASE	0x80000

/* Trace Buffer Window */
#define CNL_ADSP_SRAM2_BASE     0x0C0000
#define CNL_ADSP_W2_SIZE        0x2000
#define CNL_ADSP_WP_DSP0        (CNL_ADSP_SRAM0_BASE+0x30)
#define CNL_ADSP_WP_DSP1        (CNL_ADSP_SRAM0_BASE+0x34)
#define CNL_ADSP_WP_DSP2        (CNL_ADSP_SRAM0_BASE+0x38)
#define CNL_ADSP_WP_DSP3        (CNL_ADSP_SRAM0_BASE+0x3C)

/* Firmware status window */
#define CNL_ADSP_FW_STATUS	CNL_ADSP_SRAM0_BASE
#define CNL_ADSP_ERROR_CODE	(CNL_ADSP_FW_STATUS + 0x4)

#define CNL_INSTANCE_ID		0
#define CNL_BASE_FW_MODULE_ID	0
#define CNL_ADSP_FW_HDR_OFFSET	0x2000
#define CNL_ROM_CTRL_DMA_ID	0x9

#define CNL_IMR_MEMSIZE					0x400000  /*4MB*/
#define HDA_ADSP_REG_ADSPCS_IMR_CACHED_TLB_START	0x100
#define HDA_ADSP_REG_ADSPCS_IMR_UNCACHED_TLB_START	0x200
#define HDA_ADSP_REG_ADSPCS_IMR_SIZE	0x8

#ifndef writeq
static inline void writeq(u64 val, void __iomem *addr)
{
	writel(((u32) (val)), addr);
	writel(((u32) (val >> 32)), addr + 4);
}
#endif

/* Needed for presilicon platform based on FPGA */
static int cnl_fpga_alloc_imr(struct sst_dsp *ctx)
{
	u32 pages;
	u32 fw_size = CNL_IMR_MEMSIZE;
	int ret;

	ret = ctx->dsp_ops.alloc_dma_buf(ctx->dev, &ctx->dsp_fw_buf, fw_size);

	if (ret < 0) {
		dev_err(ctx->dev, "Alloc buffer for base fw failed: %x\n", ret);
		return ret;
	}

	pages = (fw_size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	dev_dbg(ctx->dev, "sst_cnl_fpga_alloc_imr pages=0x%x\n", pages);
	set_memory_uc((unsigned long)ctx->dsp_fw_buf.area, pages);

	writeq(virt_to_phys(ctx->dsp_fw_buf.area) + 1,
		 ctx->addr.shim + HDA_ADSP_REG_ADSPCS_IMR_CACHED_TLB_START);
	writeq(virt_to_phys(ctx->dsp_fw_buf.area) + 1,
		 ctx->addr.shim + HDA_ADSP_REG_ADSPCS_IMR_UNCACHED_TLB_START);

	writel(CNL_IMR_MEMSIZE, ctx->addr.shim
	       + HDA_ADSP_REG_ADSPCS_IMR_CACHED_TLB_START
	       + HDA_ADSP_REG_ADSPCS_IMR_SIZE);
	writel(CNL_IMR_MEMSIZE, ctx->addr.shim
	       + HDA_ADSP_REG_ADSPCS_IMR_UNCACHED_TLB_START
	       + HDA_ADSP_REG_ADSPCS_IMR_SIZE);

	memset(ctx->dsp_fw_buf.area, 0, fw_size);

	return 0;
}

static inline void cnl_fpga_free_imr(struct sst_dsp *ctx)
{
	ctx->dsp_ops.free_dma_buf(ctx->dev, &ctx->dsp_fw_buf);
}

static int cnl_prepare_fw(struct sst_dsp *ctx, const void *fwdata, u32 fwsize)
{

	int ret, stream_tag;

	ret = cnl_fpga_alloc_imr(ctx);
	if (ret < 0)
		return ret;

	stream_tag = ctx->dsp_ops.prepare(ctx->dev, 0x40, fwsize, &ctx->dmab,
						SNDRV_PCM_STREAM_PLAYBACK);
	if (stream_tag <= 0) {
		dev_err(ctx->dev, "dma prepare failed: 0%#x\n", stream_tag);
		return stream_tag;
	}

	ctx->dsp_ops.stream_tag = stream_tag;
	memcpy(ctx->dmab.area, fwdata, fwsize);

	/* purge FW request */
	sst_dsp_shim_write(ctx, CNL_ADSP_REG_HIPCIDR,
			   CNL_ADSP_REG_HIPCIDR_BUSY | (CNL_IPC_PURGE |
			   ((stream_tag - 1) << CNL_ROM_CTRL_DMA_ID)));

	ret = cnl_dsp_enable_core(ctx, SKL_DSP_CORE0_MASK);
	if (ret < 0) {
		dev_err(ctx->dev, "dsp boot core failed ret: %d\n", ret);
		ret = -EIO;
		goto base_fw_load_failed;
	}


	for (ret = CNL_BASEFW_TIMEOUT;
	     ret > 0 && IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA); --ret) {
		u32 reg = sst_dsp_shim_read(ctx, CNL_ADSP_REG_HIPCIDA);

		if (reg & CNL_ADSP_REG_HIPCIDA_DONE) {
			sst_dsp_shim_update_bits_forced(ctx,
					CNL_ADSP_REG_HIPCIDA,
					CNL_ADSP_REG_HIPCIDA_DONE,
					CNL_ADSP_REG_HIPCIDA_DONE);
			break;
		}

		mdelay(1);
	}
	/* enable interrupt */
	cnl_ipc_int_enable(ctx);
	cnl_ipc_op_int_enable(ctx);

	ret = sst_dsp_register_poll(ctx, CNL_ADSP_FW_STATUS, CNL_FW_STS_MASK,
				    CNL_FW_ROM_INIT, CNL_INIT_TIMEOUT,
				    "rom load");
	if (ret < 0) {
		dev_err(ctx->dev, "rom init timeout, ret: %d\n", ret);
		goto base_fw_load_failed;
	}

	return 0;

base_fw_load_failed:
	ctx->dsp_ops.cleanup(ctx->dev, &ctx->dmab, stream_tag,
						SNDRV_PCM_STREAM_PLAYBACK);
	cnl_dsp_disable_core(ctx, SKL_DSP_CORE0_MASK);
	cnl_fpga_free_imr(ctx);

	return ret;
}

static int sst_transfer_fw_host_dma(struct sst_dsp *ctx)
{
	int ret;

	ctx->dsp_ops.trigger(ctx->dev, true, ctx->dsp_ops.stream_tag,
						SNDRV_PCM_STREAM_PLAYBACK);
	ret = sst_dsp_register_poll(ctx, CNL_ADSP_FW_STATUS, CNL_FW_STS_MASK,
				    CNL_FW_INIT, CNL_BASEFW_TIMEOUT,
				    "firmware boot");

	ctx->dsp_ops.trigger(ctx->dev, false, ctx->dsp_ops.stream_tag,
						SNDRV_PCM_STREAM_PLAYBACK);
	ctx->dsp_ops.cleanup(ctx->dev, &ctx->dmab, ctx->dsp_ops.stream_tag,
						SNDRV_PCM_STREAM_PLAYBACK);

	return ret;
}

static int cnl_load_base_firmware(struct sst_dsp *ctx)
{
	struct firmware stripped_fw;
	struct skl_sst *cnl = ctx->thread_context;
	struct skl_fw_property_info fw_property;
	int ret, i;

	fw_property.memory_reclaimed = -1;
	if (!ctx->fw) {
		ret = request_firmware(&ctx->fw, ctx->fw_name, ctx->dev);
		if (ret < 0) {
			dev_err(ctx->dev, "request firmware failed: %d\n", ret);
			return ret;
		}
	}

	/* parse uuids if first boot */
	if (cnl->is_first_boot) {
		ret = snd_skl_parse_uuids(ctx, ctx->fw, CNL_ADSP_FW_HDR_OFFSET, 0);
		if (ret < 0)
			goto load_base_firmware_failed;
	}

	stripped_fw.data = ctx->fw->data;
	stripped_fw.size = ctx->fw->size;
	skl_dsp_strip_extended_manifest(&stripped_fw);

	ret = -ENOEXEC;
	for (i = 0; i < SST_FW_INIT_RETRY && ret < 0; i++) {
		ret = cnl_prepare_fw(ctx, stripped_fw.data, stripped_fw.size);
		if (ret < 0) {
			dev_dbg(ctx->dev, "prepare firmware failed: %d\n", ret);
			continue;
		}

		dev_dbg(ctx->dev, "ROM loaded successfully on iteration %d.\n", i);

		ret = sst_transfer_fw_host_dma(ctx);
		if (ret < 0) {
			dev_dbg(ctx->dev, "transfer firmware failed: %d\n", ret);
			cnl_dsp_disable_core(ctx, SKL_DSP_CORE0_MASK);
		}
	}

	if (ret < 0)
		goto load_base_firmware_failed;
	dev_dbg(ctx->dev, "Firmware download successful.\n");

	ret = wait_event_timeout(cnl->boot_wait, cnl->boot_complete,
				 msecs_to_jiffies(SKL_IPC_BOOT_MSECS));
	if (ret == 0) {
		dev_err(ctx->dev, "FW ready timed-out\n");
		cnl_dsp_disable_core(ctx, SKL_DSP_CORE0_MASK);
		ret = -EIO;
		goto load_base_firmware_failed;
	}

	cnl->fw_loaded = true;

	ret = skl_get_firmware_configuration(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "fwconfig ipc failed !\n");
		ret = -EIO;
		goto load_base_firmware_failed;
	}

	ret = skl_validate_fw_version(cnl);
	if (ret < 0)
		goto load_base_firmware_failed;

	fw_property = cnl->fw_property;
	if (fw_property.memory_reclaimed <= 0) {
		dev_err(ctx->dev, "Memory reclaim not enabled:%d\n",
			fw_property.memory_reclaimed);
		ret = -EIO;
		goto load_base_firmware_failed;
	}

	ret = skl_get_hardware_configuration(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "hwconfig ipc failed !\n");
		ret = -EIO;
		goto load_base_firmware_failed;
	}

	/* Update dsp core count retrieved from hw config IPC */
	cnl->cores.count = cnl->hw_property.dsp_cores;

	return 0;

load_base_firmware_failed:
	dev_err(ctx->dev, "Firmware load failed: %d.\n", ret);
	release_firmware(ctx->fw);
	ctx->fw = NULL;

	return ret;
}

static int cnl_set_dsp_D0(struct sst_dsp *ctx, unsigned int core_id)
{
	struct skl_sst *cnl = ctx->thread_context;
	unsigned int core_mask = SKL_DSP_CORE_MASK(core_id);
	struct skl_ipc_dxstate_info dx;
	int ret;

	if (!cnl->fw_loaded) {
		cnl->boot_complete = false;
		ret = cnl_load_base_firmware(ctx);
		if (ret < 0) {
			dev_err(ctx->dev, "fw reload failed: %d\n", ret);
			return ret;
		}

		cnl->cores.state[core_id] = SKL_DSP_RUNNING;
		return ret;
	}

	ret = cnl_dsp_enable_core(ctx, core_mask);
	if (ret < 0) {
		dev_err(ctx->dev, "enable dsp core %d failed: %d\n",
			core_id, ret);
		goto err;
	}

	if (core_id == SKL_DSP_CORE0_ID) {
		/* enable interrupt */
		cnl_ipc_int_enable(ctx);
		cnl_sdw_int_enable(ctx, true);
		cnl_ipc_op_int_enable(ctx);
		cnl->boot_complete = false;

		ret = wait_event_timeout(cnl->boot_wait, cnl->boot_complete,
					 msecs_to_jiffies(SKL_IPC_BOOT_MSECS));
		if (ret == 0) {
			dev_err(ctx->dev,
				"dsp boot timeout, status=%#x error=%#x\n",
				sst_dsp_shim_read(ctx, CNL_ADSP_FW_STATUS),
				sst_dsp_shim_read(ctx, CNL_ADSP_ERROR_CODE));
			goto err;
		}
	} else {
		dx.core_mask = core_mask;
		dx.dx_mask = core_mask;

		ret = skl_ipc_set_dx(&cnl->ipc, CNL_INSTANCE_ID,
				     CNL_BASE_FW_MODULE_ID, &dx);
		if (ret < 0) {
			dev_err(ctx->dev, "set_dx failed, core: %d ret: %d\n",
				core_id, ret);
			goto err;
		}
	}
	cnl->cores.state[core_id] = SKL_DSP_RUNNING;

	return 0;
err:
	cnl_dsp_disable_core(ctx, core_mask);

	return ret;
}

static int cnl_set_dsp_D3(struct sst_dsp *ctx, unsigned int core_id)
{
	struct skl_sst *cnl = ctx->thread_context;
	unsigned int core_mask = SKL_DSP_CORE_MASK(core_id);
	struct skl_ipc_dxstate_info dx;
	int ret;

	dx.core_mask = core_mask;
	dx.dx_mask = SKL_IPC_D3_MASK;

	ret = skl_ipc_set_dx(&cnl->ipc, CNL_INSTANCE_ID,
			     CNL_BASE_FW_MODULE_ID, &dx);
	if (ret < 0) {
		dev_err(ctx->dev,
			"dsp core %d to d3 failed; continue reset\n",
			core_id);
		cnl->fw_loaded = false;
	}

	/* disable interrupts if core 0 */
	if (core_id == SKL_DSP_CORE0_ID) {
		skl_ipc_op_int_disable(ctx);
		skl_ipc_int_disable(ctx);
	}

	ret = cnl_dsp_disable_core(ctx, core_mask);
	if (ret < 0) {
		dev_err(ctx->dev, "disable dsp core %d failed: %d\n",
			core_id, ret);
		return ret;
	}

	cnl->cores.state[core_id] = SKL_DSP_RESET;

	return ret;
}

static unsigned int cnl_get_errno(struct sst_dsp *ctx)
{
	return sst_dsp_shim_read(ctx, CNL_ADSP_ERROR_CODE);
}

static const struct skl_dsp_fw_ops cnl_fw_ops = {
	.set_state_D0 = cnl_set_dsp_D0,
	.set_state_D3 = cnl_set_dsp_D3,
	.set_state_D0i3 = bxt_schedule_dsp_D0i3,
	.set_state_D0i0 = bxt_set_dsp_D0i0,
	.load_fw = cnl_load_base_firmware,
	.get_fw_errcode = cnl_get_errno,
	.load_library = bxt_load_library,
};

static struct sst_ops cnl_ops = {
	.irq_handler = cnl_dsp_sst_interrupt,
	.write = sst_shim32_write,
	.read = sst_shim32_read,
	.ram_read = sst_memcpy_fromio_32,
	.ram_write = sst_memcpy_toio_32,
	.free = cnl_dsp_free,
};

#define CNL_IPC_GLB_NOTIFY_RSP_SHIFT	29
#define CNL_IPC_GLB_NOTIFY_RSP_MASK	0x1
#define CNL_IPC_GLB_NOTIFY_RSP_TYPE(x)	(((x) >> CNL_IPC_GLB_NOTIFY_RSP_SHIFT) \
					& CNL_IPC_GLB_NOTIFY_RSP_MASK)

static irqreturn_t cnl_dsp_irq_thread_handler(int irq, void *context)
{
	struct sst_dsp *dsp = context;
	struct skl_sst *cnl = sst_dsp_get_thread_context(dsp);
	struct sst_generic_ipc *ipc = &cnl->ipc;
	struct skl_ipc_header header = {0};
	u32 hipcida, hipctdr, hipctdd;
	int ipc_irq = 0;

	/* here we handle ipc interrupts only */
	if (!(dsp->intr_status & CNL_ADSPIS_IPC))
		return IRQ_NONE;

	hipcida = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCIDA);
	hipctdr = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCTDR);

	/* reply message from dsp */
	if (hipcida & CNL_ADSP_REG_HIPCIDA_DONE) {
		sst_dsp_shim_update_bits(dsp, CNL_ADSP_REG_HIPCCTL,
			CNL_ADSP_REG_HIPCCTL_DONE, 0);

		/* clear done bit - tell dsp operation is complete */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCIDA,
			CNL_ADSP_REG_HIPCIDA_DONE, CNL_ADSP_REG_HIPCIDA_DONE);

		ipc_irq = 1;

		/* unmask done interrupt */
		sst_dsp_shim_update_bits(dsp, CNL_ADSP_REG_HIPCCTL,
			CNL_ADSP_REG_HIPCCTL_DONE, CNL_ADSP_REG_HIPCCTL_DONE);
	}

	/* new message from dsp */
	if (hipctdr & CNL_ADSP_REG_HIPCTDR_BUSY) {
		hipctdd = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCTDD);
		header.primary = hipctdr;
		header.extension = hipctdd;
		dev_dbg(dsp->dev, "IPC irq: Firmware respond primary:%x",
						header.primary);
		dev_dbg(dsp->dev, "IPC irq: Firmware respond extension:%x",
						header.extension);

		if (CNL_IPC_GLB_NOTIFY_RSP_TYPE(header.primary)) {
			/* Handle Immediate reply from DSP Core */
			skl_ipc_process_reply(ipc, header);
		} else {
			dev_dbg(dsp->dev, "IPC irq: Notification from firmware\n");
			skl_ipc_process_notification(ipc, header);
		}
		/* clear busy interrupt */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCTDR,
			CNL_ADSP_REG_HIPCTDR_BUSY, CNL_ADSP_REG_HIPCTDR_BUSY);

		/* set done bit to ack dsp */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCTDA,
			CNL_ADSP_REG_HIPCTDA_DONE, CNL_ADSP_REG_HIPCTDA_DONE);
		ipc_irq = 1;
	}

	if (ipc_irq == 0)
		return IRQ_NONE;

	cnl_ipc_int_enable(dsp);

	/* continue to send any remaining messages */
	schedule_work(&ipc->kwork);

	return IRQ_HANDLED;
}

static struct sst_dsp_device cnl_dev = {
	.thread = cnl_dsp_irq_thread_handler,
	.ops = &cnl_ops,
};

static void cnl_ipc_tx_msg(struct sst_generic_ipc *ipc, struct ipc_message *msg)
{
	struct skl_ipc_header *header = (struct skl_ipc_header *)(&msg->header);

	if (msg->tx_size)
		sst_dsp_outbox_write(ipc->dsp, msg->tx_data, msg->tx_size);
	sst_dsp_shim_write_unlocked(ipc->dsp, CNL_ADSP_REG_HIPCIDD,
				    header->extension);
	sst_dsp_shim_write_unlocked(ipc->dsp, CNL_ADSP_REG_HIPCIDR,
				header->primary | CNL_ADSP_REG_HIPCIDR_BUSY);
}

static bool cnl_ipc_is_dsp_busy(struct sst_dsp *dsp)
{
	u32 hipcidr;

	hipcidr = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCIDR);

	return (hipcidr & CNL_ADSP_REG_HIPCIDR_BUSY);
}

static int cnl_ipc_init(struct device *dev, struct skl_sst *cnl)
{
	struct sst_generic_ipc *ipc;
	int err;

	ipc = &cnl->ipc;
	ipc->dsp = cnl->dsp;
	ipc->dev = dev;

	ipc->tx_data_max_size = CNL_ADSP_W1_SZ;
	ipc->rx_data_max_size = CNL_ADSP_W0_UP_SZ;

	err = sst_ipc_init(ipc);
	if (err)
		return err;

	/*
	 * overriding tx_msg and is_dsp_busy since
	 * ipc registers are different for cnl
	 */
	ipc->ops.tx_msg = cnl_ipc_tx_msg;
	ipc->ops.tx_data_copy = skl_ipc_tx_data_copy;
	ipc->ops.is_dsp_busy = cnl_ipc_is_dsp_busy;

	return 0;
}

#if IS_ENABLED(CONFIG_SND_SOC_RT700)
static int skl_register_sdw_masters(struct device *dev, struct skl_sst *dsp,
			void __iomem *mmio_base, int irq, void *ptr)
{
	struct sdw_master_capabilities *m_cap, *map_data;
	struct sdw_mstr_dp0_capabilities *dp0_cap;
	struct sdw_mstr_dpn_capabilities *dpn_cap;
	struct sdw_master *master;
	struct cnl_sdw_data *p_data;
	struct cnl_bra_operation *p_ptr = ptr;
	int ret = 0, i, j, k, wl = 0;
	/* TODO: This number 4 should come from ACPI */
	dsp->num_sdw_controllers = 4;
	master = devm_kzalloc(dev,
			(sizeof(*master) * dsp->num_sdw_controllers),
			GFP_KERNEL);
	if (!master) {
			return -ENOMEM;
			dsp->num_sdw_controllers = 0;
	}
	dsp->mstr = master;

	dsp->bra_pipe_data = devm_kzalloc(dev,
				(sizeof(*dsp->bra_pipe_data) *
				dsp->num_sdw_controllers),
				GFP_KERNEL);

	/* TODO This should come from ACPI */
	for (i = 0; i < dsp->num_sdw_controllers; i++) {
		p_data = devm_kzalloc(dev, sizeof(*p_data), GFP_KERNEL);
		if (!p_data)
			return -ENOMEM;

		/* PCI Device is parent of the SoundWire master device */
		/* TODO: All these hardcoding should come from ACPI */
		master[i].dev.parent = dev;
		master[i].dev.platform_data = p_data;
		m_cap = &master[i].mstr_capabilities;
		dp0_cap = &m_cap->sdw_dp0_cap;
		map_data = kzalloc(sizeof(*m_cap), GFP_KERNEL);
		if (!map_data)
			return -ENOMEM;
		/* This function retrieves the data for SoundWire controller */
		cnl_sdw_get_master_caps(dev, map_data);
		master[i].nr = i;
		master[i].timeout = -1;
		master[i].retries = CNL_SDW_MAX_CMD_RETRIES;
		m_cap->base_clk_freq = map_data->base_clk_freq;
		/* TODO: Frequency is not read correctly in ACPI code */
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
		m_cap->base_clk_freq = 9600000;
#else
		m_cap->base_clk_freq = 12000000;
#endif
		strcpy(master[i].name, "cnl_sdw_mstr");
		m_cap->highphy_capable = false;
		m_cap->monitor_handover_supported = false;
		m_cap->sdw_dp0_supported = 1;
		m_cap->num_data_ports = CNL_SDW_MAX_PORTS;
		dp0_cap->max_word_length = 32;
		dp0_cap->min_word_length = 1;
		dp0_cap->num_word_length = 0;
		dp0_cap->word_length_buffer = NULL;
		dp0_cap->bra_max_data_per_frame = 0;
		m_cap->sdw_dpn_cap = kzalloc(((sizeof(*dpn_cap)) *
					CNL_SDW_MAX_PORTS), GFP_KERNEL);
		if (!m_cap->sdw_dpn_cap)
			return -ENOMEM;
		for (j = 0; j < m_cap->num_data_ports; j++) {
			dpn_cap = &m_cap->sdw_dpn_cap[j];
			map_data->sdw_dpn_cap = kzalloc(sizeof(*dpn_cap),
								GFP_KERNEL);
			if (!map_data->sdw_dpn_cap)
				return -ENOMEM;
			/*
			 * This function retrieves the data
			 * for SoundWire devices.
			 */
			cnl_sdw_get_master_dev_caps(dev, map_data, j);
			/* Both Tx and Rx */
			/* WORKAROUND: hard code for SDW1 */
			if (j == 1)
				dpn_cap->dpn_type = SDW_FULL_DP;
			else
				dpn_cap->dpn_type =
					map_data->sdw_dpn_cap->dpn_type;
			dpn_cap->port_direction = 0x3;
			dpn_cap->port_number = j;
			dpn_cap->max_word_length = 32;
			dpn_cap->min_word_length = 1;
			dpn_cap->num_word_length = 4;

			dpn_cap->word_length_buffer =
					kzalloc(((sizeof(unsigned int)) *
					dpn_cap->num_word_length), GFP_KERNEL);
			if (!dpn_cap->word_length_buffer)
				return -ENOMEM;
			for (k = 0; k < dpn_cap->num_word_length; k++)
				dpn_cap->word_length_buffer[k] = wl = wl + 8;
			wl = 0;
			dpn_cap->dpn_type = SDW_FULL_DP;
			dpn_cap->min_ch_num = 1;
			dpn_cap->max_ch_num = 8;
			dpn_cap->num_ch_supported = 0;
			dpn_cap->ch_supported =  NULL;
			/* IP supports all, but we are going to support only
			 * isochronous
			 */
			dpn_cap->port_mode_mask =
				SDW_PORT_FLOW_MODE_ISOCHRONOUS;
			dpn_cap->block_packing_mode_mask =
				SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT |
				SDW_PORT_BLK_PKG_MODE_BLK_PER_CH;
			kfree(map_data->sdw_dpn_cap);
		}
		kfree(map_data);
		master[i].link_sync_mask = 0x0;
		switch (i) {
		case 0:
			p_data->sdw_regs = mmio_base + CNL_SDW_LINK_0_BASE;
			break;
		case 1:
			p_data->sdw_regs = mmio_base + CNL_SDW_LINK_1_BASE;
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
			master[i].link_sync_mask = 0x2;
#endif
			break;
		case 2:
			p_data->sdw_regs = mmio_base + CNL_SDW_LINK_2_BASE;
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
			master[i].link_sync_mask = 0x4;
#endif
			break;
		case 3:
			p_data->sdw_regs = mmio_base + CNL_SDW_LINK_3_BASE;
			break;
		default:
			return -EINVAL;
		}
		p_data->sdw_shim = mmio_base + CNL_SDW_SHIM_BASE;
		p_data->alh_base = mmio_base + CNL_ALH_BASE;
		p_data->inst_id = i;
		p_data->irq = irq;

		p_data->bra_data = kzalloc((sizeof(struct cnl_sdw_bra_cfg)),
					GFP_KERNEL);
		p_data->bra_data->drv_data = dsp;
		p_data->bra_data->bra_ops = p_ptr;
		ret = sdw_add_master_controller(&master[i]);
		if (ret) {
			dev_err(dev, "Failed to register soundwire master\n");
			return ret;
		}
	}
	/* Enable the global soundwire interrupts */
	cnl_sdw_int_enable(dsp->dsp, 1);
	return 0;
}
#endif

static void skl_unregister_sdw_masters(struct skl_sst *ctx)
{
	int i;

	/* Disable global soundwire interrupts */
	cnl_sdw_int_enable(ctx->dsp, 0);
	for (i = 0; i < ctx->num_sdw_controllers; i++)
		sdw_del_master_controller(&ctx->mstr[i]);

}

int cnl_sst_dsp_init(struct device *dev, void __iomem *mmio_base, int irq,
		     const char *fw_name, const struct skl_dsp_ops *dsp_ops,
		     struct skl_sst **dsp, void *ptr)
{
	struct skl_sst *cnl;
	struct sst_dsp *sst;
	struct skl_dsp_loader_ops loader_ops;
	u32 dsp_wp[] = {CNL_ADSP_WP_DSP0, CNL_ADSP_WP_DSP1, CNL_ADSP_WP_DSP2,
				CNL_ADSP_WP_DSP3};
	int ret;

	loader_ops = dsp_ops->loader_ops();
	ret = skl_sst_ctx_init(dev, irq, fw_name, loader_ops, dsp, &cnl_dev);
	if (ret < 0) {
		dev_err(dev, "%s: no device\n", __func__);
		return ret;
	}

	cnl = *dsp;
	cnl->dsp_ops = dsp_ops;
	sst = cnl->dsp;
	sst->fw_ops = cnl_fw_ops;
	sst->addr.lpe = mmio_base;
	sst->addr.shim = mmio_base;
	sst->addr.sram0_base = CNL_ADSP_SRAM0_BASE;
	sst->addr.sram1_base = CNL_ADSP_SRAM1_BASE;
	sst->addr.w0_stat_sz = CNL_ADSP_W0_STAT_SZ;
	sst->addr.w0_up_sz = CNL_ADSP_W0_UP_SZ;

	sst_dsp_mailbox_init(sst, (CNL_ADSP_SRAM0_BASE + CNL_ADSP_W0_STAT_SZ),
			     CNL_ADSP_W0_UP_SZ, CNL_ADSP_SRAM1_BASE,
			     CNL_ADSP_W1_SZ);

	ret = skl_dsp_init_trace_window(sst, dsp_wp, CNL_ADSP_SRAM2_BASE,
					 CNL_ADSP_W2_SIZE, CNL_DSP_CORES);
	if (ret) {
		dev_err(dev, "FW tracing init failed : %x", ret);
		return ret;
	}

	ret = cnl_ipc_init(dev, cnl);
	if (ret) {
		skl_dsp_free(sst);
		return ret;
	}

	cnl->boot_complete = false;
	init_waitqueue_head(&cnl->boot_wait);

	ret = cnl_load_base_firmware(sst);
	if (ret < 0) {
		dev_err(dev, "Load base fw failed: %d", ret);
		return ret;
	}

	/* set the D0i3 check */
	cnl->ipc.ops.check_dsp_lp_on = skl_ipc_check_D0i0;

	INIT_DELAYED_WORK(&cnl->d0i3.work, bxt_set_dsp_D0i3);
	cnl->d0i3.state = SKL_DSP_D0I3_NONE;

#if IS_ENABLED(CONFIG_SND_SOC_RT700)
	ret = skl_register_sdw_masters(dev, cnl, mmio_base, irq, ptr);
	if (ret) {
		dev_err(cnl->dev, "%s SoundWire masters registration failed\n", __func__);
		return ret;
	}
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sst_dsp_init);

int cnl_sst_init_fw(struct device *dev, struct skl_sst *ctx)
{
	struct sst_dsp *sst = ctx->dsp;
	int ret;

	skl_dsp_init_core_state(sst);

	if (ctx->lib_count > 1) {
		ret = sst->fw_ops.load_library(sst, ctx->lib_info,
						ctx->lib_count);
		if (ret) {
			dev_err(dev, "Load Library failed: %#x", ret);
			return ret;
		}
	}
	ctx->is_first_boot = false;

	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sst_init_fw);

void cnl_sst_dsp_cleanup(struct device *dev, struct skl_sst *ctx)
{
	if (ctx->dsp->fw)
		release_firmware(ctx->dsp->fw);

	skl_freeup_uuid_list(ctx);
	skl_unregister_sdw_masters(ctx);
	cnl_ipc_free(&ctx->ipc);

	ctx->dsp->ops->free(ctx->dsp);
	cnl_fpga_free_imr(ctx->dsp);
}
EXPORT_SYMBOL_GPL(cnl_sst_dsp_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Cannonlake IPC driver");
