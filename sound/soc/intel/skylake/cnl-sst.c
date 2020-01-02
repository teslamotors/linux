/*
 * cnl-sst.c - HDA DSP library functions for CNL platform
 *
 * Copyright (C) 2015-16, Intel Corporation.
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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <asm/cacheflush.h>

#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "../common/sst-ipc.h"
#include "cnl-sst-dsp.h"
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-fwlog.h"
#include "skl.h"

#define FW_ROM_INIT_DONE                0x1
#define CNL_IPC_PURGE_FW		0x01004000
#define CNL_ROM_INIT_HIPCIDA_TIMEOUT    500
#define CNL_ROM_INIT_DONE_TIMEOUT	500
#define CNL_FW_ROM_BASEFW_ENTERED_TIMEOUT	500
#define CNL_FW_ROM_BASEFW_ENTERED	0x5
#define CNL_D0I3_DELAY 10000

/* Intel HD Audio SRAM Window 0*/
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

static void cnl_set_dsp_D0i3(struct work_struct *work);

void cnl_ipc_int_enable(struct sst_dsp *ctx)
{
	sst_dsp_shim_update_bits(ctx, CNL_ADSP_REG_ADSPIC,
		CNL_ADSPIC_IPC, CNL_ADSPIC_IPC);
}

void cnl_ipc_int_disable(struct sst_dsp *ctx)
{
	sst_dsp_shim_update_bits_unlocked(ctx, CNL_ADSP_REG_ADSPIC,
		CNL_ADSPIC_IPC, 0);
}

void cnl_ipc_op_int_enable(struct sst_dsp *ctx)
{
	/* enable IPC DONE interrupt */
	sst_dsp_shim_update_bits(ctx, CNL_ADSP_REG_HIPCCTL,
		CNL_ADSP_REG_HIPCCTL_DONE, CNL_ADSP_REG_HIPCCTL_DONE);

	/* Enable IPC BUSY interrupt */
	sst_dsp_shim_update_bits(ctx, CNL_ADSP_REG_HIPCCTL,
		CNL_ADSP_REG_HIPCCTL_BUSY, CNL_ADSP_REG_HIPCCTL_BUSY);
}

bool cnl_ipc_int_status(struct sst_dsp *ctx)
{
	return sst_dsp_shim_read_unlocked(ctx,
		CNL_ADSP_REG_ADSPIS) & CNL_ADSPIS_IPC;
}

void cnl_ipc_free(struct sst_generic_ipc *ipc)
{
	/* Disable IPC DONE interrupt */
	sst_dsp_shim_update_bits(ipc->dsp, CNL_ADSP_REG_HIPCCTL,
		CNL_ADSP_REG_HIPCCTL_DONE, 0);

	/* Disable IPC BUSY interrupt */
	sst_dsp_shim_update_bits(ipc->dsp, CNL_ADSP_REG_HIPCCTL,
		CNL_ADSP_REG_HIPCCTL_BUSY, 0);

	sst_ipc_fini(ipc);
}

#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)

#define CNL_IMR_MEMSIZE					0x400000  /*4MB*/
#define HDA_ADSP_REG_ADSPCS_IMR_CACHED_TLB_START	0x100
#define HDA_ADSP_REG_ADSPCS_IMR_UNCACHED_TLB_START	0x200
#define HDA_ADSP_REG_ADSPCS_IMR_SIZE	0x8
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

#endif
static int cnl_prepare_fw(struct sst_dsp *ctx, const void *fwdata,
		u32 fwsize)
{

	int ret, i, stream_tag;
	u32 reg;
	u32 pages;

#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
	ret = cnl_fpga_alloc_imr(ctx);
	if (ret < 0)
		return ret;
#endif
	dev_dbg(ctx->dev, "Starting to prepare host dma fwsize=0x%x\n", fwsize);
	stream_tag = ctx->dsp_ops.prepare(ctx->dev, 0x40, fwsize, &ctx->dmab);
	if (stream_tag <= 0) {
		dev_err(ctx->dev, "DMA prepare failed: 0x%x\n", stream_tag);
		return stream_tag;
	}

	ctx->dsp_ops.stream_tag = stream_tag;
	pages = (fwsize + PAGE_SIZE - 1) >> PAGE_SHIFT;
	set_memory_uc((unsigned long)ctx->dmab.area, pages);
	memcpy(ctx->dmab.area, fwdata, fwsize);

	/* purge FW request */
	sst_dsp_shim_write(ctx, CNL_ADSP_REG_HIPCIDR,
			CNL_ADSP_REG_HIPCIDR_BUSY |
			CNL_IPC_PURGE_FW | (stream_tag - 1));

	ret = cnl_dsp_enable_core(ctx, SKL_DSP_CORE_MASK(0));
	if (ret < 0) {
		dev_err(ctx->dev, "Boot dsp core failed ret: %d\n", ret);
		ret = -EIO;
		goto base_fw_load_failed;
	}

	for (i = CNL_ROM_INIT_HIPCIDA_TIMEOUT; i > 0; --i) {
		reg = sst_dsp_shim_read(ctx, CNL_ADSP_REG_HIPCIDA);

		if (reg & CNL_ADSP_REG_HIPCIDA_DONE) {
			sst_dsp_shim_update_bits_forced(ctx,
					CNL_ADSP_REG_HIPCIDA,
					CNL_ADSP_REG_HIPCIDA_DONE,
					CNL_ADSP_REG_HIPCIDA_DONE);
			break;
		}

		mdelay(1);
	}

	if (!i) {
		dev_err(ctx->dev, "HIPCIDA done timeout: 0x%x\n", reg);
		sst_dsp_shim_update_bits(ctx, CNL_ADSP_REG_HIPCIDA,
				CNL_ADSP_REG_HIPCIDA_DONE,
				CNL_ADSP_REG_HIPCIDA_DONE);
	}

	/* enable interrupt */
	cnl_ipc_int_enable(ctx);
	cnl_ipc_op_int_enable(ctx);

	/* poll for ROM init done */
	for (i = CNL_ROM_INIT_DONE_TIMEOUT; i > 0; --i) {
		if (FW_ROM_INIT_DONE ==
			(sst_dsp_shim_read(ctx, CNL_ADSP_FW_STATUS) &
				CNL_FW_STS_MASK)) {
				dev_dbg(ctx->dev, "ROM loaded\n");
			break;
		}
		mdelay(1);
	}
	if (!i) {
		dev_err(ctx->dev, "ROM init done timeout: 0x%x\n", reg);
		ret = -EIO;
		goto base_fw_load_failed;
	}

	return 0;
base_fw_load_failed:
	cnl_dsp_disable_core(ctx, SKL_DSP_CORE_MASK(0));
	ctx->dsp_ops.cleanup(ctx->dev, &ctx->dmab, stream_tag);
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_CNL_FPGA)
	cnl_fpga_free_imr(ctx);
#endif
	return ret;
}

static int sst_transfer_fw_host_dma(struct sst_dsp *ctx)
{
	int ret = 0;

	ctx->dsp_ops.trigger(ctx->dev, true, ctx->dsp_ops.stream_tag);
	ret = sst_dsp_register_poll(ctx, CNL_ADSP_FW_STATUS, CNL_FW_STS_MASK,
			CNL_FW_ROM_BASEFW_ENTERED,
			CNL_FW_ROM_BASEFW_ENTERED_TIMEOUT,
			"Firmware boot");

	ctx->dsp_ops.trigger(ctx->dev, false, ctx->dsp_ops.stream_tag);
	ctx->dsp_ops.cleanup(ctx->dev, &ctx->dmab, ctx->dsp_ops.stream_tag);
	return ret;
}

static int cnl_load_base_firmware(struct sst_dsp *ctx)
{
	int ret = 0;
	struct skl_sst *cnl = ctx->thread_context;
	struct skl_ext_manifest_header *hdr;
	u32 size;
	const void *data;

	cnl->boot_complete = false;

	ret = request_firmware(&ctx->fw, "dsp_fw_release.bin", ctx->dev);
	if (ret < 0) {
		dev_err(ctx->dev, "Request firmware failed: 0x%x\n", ret);
		goto cnl_load_base_firmware_failed;
	}

	size = ctx->fw->size;
	data = ctx->fw->data;
	hdr = (struct skl_ext_manifest_header *)ctx->fw->data;
	if (hdr->ext_manifest_id == SKL_EXT_MANIFEST_MAGIC_HEADER_ID) {
		dev_dbg(ctx->dev, "Found Extended manifest in FW Binary\n");
		if (hdr->ext_manifest_len >= ctx->fw->size) {
			ret = -EINVAL;
			goto cnl_load_base_firmware_failed;
		}
		size = ctx->fw->size - hdr->ext_manifest_len;
		data = (u8 *)ctx->fw->data + hdr->ext_manifest_len;
	}

	ret = cnl_prepare_fw(ctx, data, size);
	if (ret < 0) {
		dev_err(ctx->dev, "Prepare firmware failed: 0x%x\n", ret);
		goto cnl_load_base_firmware_failed;
	}

	ret = sst_transfer_fw_host_dma(ctx);
	if (ret < 0) {
		dev_err(ctx->dev, "Transfer firmware failed: 0x%x\n", ret);
		goto cnl_load_base_firmware_failed;
	} else {
		dev_dbg(ctx->dev, "Firmware download successful\n");
		ret = wait_event_timeout(cnl->boot_wait, cnl->boot_complete,
					msecs_to_jiffies(SKL_IPC_BOOT_MSECS));
		if (ret == 0) {
			dev_err(ctx->dev, "DSP boot failed, FW Ready timed-out\n");
			cnl_dsp_disable_core(ctx, SKL_DSP_CORE_MASK(0));
			ret = -EIO;
		} else {

			skl_dsp_init_core_state(ctx);
			ret = 0;
		}
	}

cnl_load_base_firmware_failed:
	release_firmware(ctx->fw);
	return ret;
}

static void cnl_set_dsp_D0i3(struct work_struct *work)
{
	int ret;
	struct skl_ipc_d0ix_msg msg;
	struct skl_sst *cnl = container_of(work,
			struct skl_sst, d0i3_data.d0i3_work.work);
	struct sst_dsp *ctx = cnl->dsp;

	dev_dbg(ctx->dev, "In %s:\n", __func__);

	/* D0i3 entry allowed only if core 0 alone is running */
	if (SKL_DSP_CORE0_MASK != cnl_dsp_get_enabled_cores(ctx)) {
		dev_warn(ctx->dev,
				"D0i3 allowed when only core0 running:Exit\n");
		return;
	}

	msg.instance_id = 0;
	msg.module_id = 0;
	msg.wake = 1;
	msg.streaming = 1;

	ret =  skl_ipc_set_d0ix(&cnl->ipc, &msg);

	if (ret < 0) {
		dev_err(ctx->dev, "Failed to set DSP to D0i3 state\n");
		return;
	}
	/* Set Vendor specific register D0I3C.I3 to enable D0i3*/
	if (cnl->update_d0i3c)
		cnl->update_d0i3c(cnl->dev, true);

	ctx->core_info.core_state[SKL_DSP_CORE0_ID] = SKL_DSP_RUNNING_D0I3;
}

static int cnl_schedule_dsp_D0i3(struct sst_dsp *ctx)
{
	struct skl_sst *skl = ctx->thread_context;
	struct skl_d0i3_data *d0i3_data = &skl->d0i3_data;

	if ((d0i3_data->d0i3_stream_count > 0) &&
			(d0i3_data->non_d0i3_stream_count == 0))
		schedule_delayed_work(&d0i3_data->d0i3_work,
				msecs_to_jiffies(CNL_D0I3_DELAY));

	return 0;
}

static int cnl_set_dsp_D0i0(struct sst_dsp *ctx)
{
	int ret;
	struct skl_ipc_d0ix_msg msg;
	struct skl_sst *cnl = ctx->thread_context;

	dev_dbg(ctx->dev, "In %s:\n", __func__);

	/* First Cancel any pending attempt to put DSP to D0i3 */
	cancel_delayed_work_sync(&cnl->d0i3_data.d0i3_work);

	/* If DSP is currently in D0i3, bring it to D0i0 */
	if (ctx->core_info.core_state[SKL_DSP_CORE0_ID] != SKL_DSP_RUNNING_D0I3)
		return 0;

	dev_dbg(ctx->dev, "Set DSP to D0i0\n");

	msg.instance_id = 0;
	msg.module_id = 0;
	msg.streaming = 1;
	msg.wake = 0;

	/* Clear Vendor specific register D0I3C.I3 to disable D0i3*/
	if (cnl->update_d0i3c)
		cnl->update_d0i3c(cnl->dev, false);

	ret = skl_ipc_set_d0ix(&cnl->ipc, &msg);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to set DSP to D0i0\n");
		return ret;
	}

	ctx->core_info.core_state[SKL_DSP_CORE0_ID] = SKL_DSP_RUNNING;

	return 0;
}

static int cnl_set_dsp_D0(struct sst_dsp *ctx, unsigned int core_id)
{
	int ret = 0;
	struct skl_sst *cnl = ctx->thread_context;
	unsigned core_mask = SKL_DSP_CORE_MASK(core_id);
	struct skl_ipc_dxstate_info dx;


	/* Re-download FW and library if needed. e.g. on coming back from S3 */
	if (cnl->fw_loaded == false) {
		dev_dbg(ctx->dev, "Re-downloading FW on Resume\n");
		return cnl_sst_dsp_init_fw(ctx->dev, cnl);
	}

	ret = cnl_dsp_enable_core(ctx, core_mask);
	if (ret < 0) {
		dev_err(ctx->dev, "enable DSP core %d failed: %d\n",
			core_id, ret);
		return ret;
	}

	if (core_id == 0) {
		cnl->boot_complete = false;
		/* enable interrupt */
		cnl_ipc_int_enable(ctx);
		cnl_ipc_op_int_enable(ctx);

		ret = wait_event_timeout(cnl->boot_wait, cnl->boot_complete,
					msecs_to_jiffies(SKL_IPC_BOOT_MSECS));
		if (ret == 0) {
			dev_err(ctx->dev,
				"DSP boot timeout: Status=%#x Error=%#x\n",
				sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_FW_STATUS),
				sst_dsp_shim_read_unlocked(ctx, CNL_ADSP_ERROR_CODE));
			goto err;
		}
	} else {
		dx.core_mask = core_mask;
		dx.dx_mask = core_mask;

		ret = skl_ipc_set_dx(&cnl->ipc,
				     CNL_INSTANCE_ID,
				     CNL_BASE_FW_MODULE_ID,
				     &dx);
		if (ret < 0) {
			dev_err(ctx->dev, "Failed to set DSP core %d to D0\n",
								core_id);
			goto err;
		}
	}

	ctx->core_info.core_state[core_id] = SKL_DSP_RUNNING;
	return 0;
err:
	cnl_dsp_disable_core(ctx, core_mask);
	return ret;
}

static int cnl_set_dsp_D3(struct sst_dsp *ctx, unsigned int core_id)
{
	int ret;
	struct skl_ipc_dxstate_info dx;
	struct skl_sst *cnl = ctx->thread_context;
	unsigned core_mask = SKL_DSP_CORE_MASK(core_id);

	dx.core_mask = core_mask;
	dx.dx_mask = SKL_IPC_D3_MASK;
	ret = skl_ipc_set_dx(&cnl->ipc,
			     CNL_INSTANCE_ID,
			     CNL_BASE_FW_MODULE_ID,
			     &dx);
	if (ret < 0)
		dev_err(ctx->dev,
			"Failed to set DSP core %d to D3; continue reset\n",
			core_id);

	ret = cnl_dsp_disable_core(ctx, core_mask);
	if (ret < 0) {
		dev_err(ctx->dev, "Disable DSP core %d failed: %d\n",
			core_id, ret);
		return -EIO;
	}

	ctx->core_info.core_state[core_id] = SKL_DSP_RESET;

	return ret;
}

static unsigned int cnl_get_errorcode(struct sst_dsp *ctx)
{
	 return sst_dsp_shim_read(ctx, CNL_ADSP_ERROR_CODE);
}

static int cnl_load_library(struct sst_dsp *ctx)
{
	struct snd_dma_buffer dmab;
	struct skl_sst *cnl = ctx->thread_context;
	struct skl_manifest *minfo = &cnl->manifest;
	const struct firmware *fw = NULL;
	struct skl_ext_manifest_header *hdr;
	u32 size;
	const void *data;
	int ret = 0, i, dma_id, stream_tag;

	for (i = 1; i < minfo->lib_count; i++) {
		fw = NULL;
		ret = request_firmware(&fw, minfo->lib[i].name, ctx->dev);
		if (ret < 0) {
			dev_err(ctx->dev, "Request firmware failed %d for library: %s\n", ret, minfo->lib[i].name);
			goto load_library_failed;
		}

		size = fw->size;
		data = fw->data;
		hdr = (struct skl_ext_manifest_header *)fw->data;
		if (hdr->ext_manifest_id == SKL_EXT_MANIFEST_MAGIC_HEADER_ID) {
			dev_dbg(ctx->dev, "Found extended manifest in lib binary\n");
			/* Check for extended manifest is not greater than FW size*/
			if (hdr->ext_manifest_len >= fw->size) {
				ret = -EINVAL;
				goto load_library_failed;
			}

			size = fw->size - hdr->ext_manifest_len;
			data = (u8 *)fw->data + hdr->ext_manifest_len;
		}

		dev_dbg(ctx->dev, "Preparing host dma for lib %s with size:%d\n", minfo->lib[i].name, size);
		stream_tag = ctx->dsp_ops.prepare(ctx->dev, 0x40, size,
						&dmab);
		if (stream_tag <= 0) {
			dev_err(ctx->dev, "DMA prepare for lib download failed\n");
			ret = stream_tag;
			goto load_library_failed;
		}
		dma_id = stream_tag - 1;
		memcpy(dmab.area, data, size);

		/* make sure Library is flushed to DDR */
		clflush_cache_range(dmab.area, size);

		ctx->dsp_ops.trigger(ctx->dev, true, stream_tag);
		ret = skl_sst_ipc_load_library(&cnl->ipc, dma_id, i);
		if (ret < 0) {
			dev_err(ctx->dev, "Load lib %s failed: %d\n", minfo->lib[i].name, ret);
			dev_dbg(ctx->dev, "Error code=0x%x: FW status=0x%x\n",
				sst_dsp_shim_read(ctx, CNL_ADSP_ERROR_CODE),
				sst_dsp_shim_read(ctx, CNL_ADSP_FW_STATUS));
		}

		ctx->dsp_ops.trigger(ctx->dev, false, stream_tag);
		ctx->dsp_ops.cleanup(ctx->dev, &dmab, stream_tag);
		release_firmware(fw);
	}

	return 0;

load_library_failed:
	release_firmware(fw);
	return ret;
}

static struct skl_dsp_fw_ops cnl_fw_ops = {
	.set_state_D0 = cnl_set_dsp_D0,
	.set_state_D3 = cnl_set_dsp_D3,
	.set_state_D0i3 = cnl_schedule_dsp_D0i3,
	.set_state_D0i0 = cnl_set_dsp_D0i0,
	.load_fw = cnl_load_base_firmware,
	.get_fw_errcode = cnl_get_errorcode,
	.load_library = cnl_load_library,
};

static struct sst_ops cnl_ops = {
	.irq_handler = cnl_dsp_sst_interrupt,
	.write = sst_shim32_write,
	.read = sst_shim32_read,
	.ram_read = sst_memcpy_fromio_32,
	.ram_write = sst_memcpy_toio_32,
	.free = cnl_dsp_free,
};

#define IPC_GLB_NOTIFY_RSP_SHIFT	29
#define IPC_GLB_NOTIFY_RSP_MASK		0x1
#define IPC_GLB_NOTIFY_RSP_TYPE(x)	(((x) >> IPC_GLB_NOTIFY_RSP_SHIFT) \
					& IPC_GLB_NOTIFY_RSP_MASK)

static irqreturn_t cnl_dsp_irq_thread_handler(int irq, void *context)
{
	struct sst_dsp *dsp = context;
	struct skl_sst *cnl = sst_dsp_get_thread_context(dsp);
	struct sst_generic_ipc *ipc = &cnl->ipc;
	struct skl_ipc_header header = {0};
	u32 hipcida, hipctdr, hipctdd;
	int ipc_irq = 0;

	/* Here we handle IPC interrupts only */
	if (!(dsp->intr_status & CNL_ADSPIS_IPC))
		return IRQ_NONE;

	hipcida = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCIDA);
	hipctdr = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCTDR);

	/* reply message from DSP */
	if (hipcida & CNL_ADSP_REG_HIPCIDA_DONE) {
		sst_dsp_shim_update_bits(dsp, CNL_ADSP_REG_HIPCCTL,
			CNL_ADSP_REG_HIPCCTL_DONE, 0);

		/* clear DONE bit - tell DSP we have completed the operation */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCIDA,
			CNL_ADSP_REG_HIPCIDA_DONE, CNL_ADSP_REG_HIPCIDA_DONE);

		ipc_irq = 1;

		/* unmask Done interrupt */
		sst_dsp_shim_update_bits(dsp, CNL_ADSP_REG_HIPCCTL,
			CNL_ADSP_REG_HIPCCTL_DONE, CNL_ADSP_REG_HIPCCTL_DONE);
	}

	/* New message from DSP */
	if (hipctdr & CNL_ADSP_REG_HIPCTDR_BUSY) {
		hipctdd = sst_dsp_shim_read_unlocked(dsp, CNL_ADSP_REG_HIPCTDD);
		header.primary = hipctdr;
		header.extension = hipctdd;
		dev_dbg(dsp->dev, "IPC irq: Firmware respond primary:%x",
						header.primary);
		dev_dbg(dsp->dev, "IPC irq: Firmware respond extension:%x",
						header.extension);

		if (IPC_GLB_NOTIFY_RSP_TYPE(header.primary)) {
			/* Handle Immediate reply from DSP Core */
			skl_ipc_process_reply(ipc, header);
		} else {
			dev_dbg(dsp->dev, "IPC irq: Notification from firmware\n");
			skl_ipc_process_notification(ipc, header);
		}
		/* clear busy interrupt */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCTDR,
			CNL_ADSP_REG_HIPCTDR_BUSY, CNL_ADSP_REG_HIPCTDR_BUSY);

		/* set done bit to ack DSP */
		sst_dsp_shim_update_bits_forced(dsp, CNL_ADSP_REG_HIPCTDA,
			CNL_ADSP_REG_HIPCTDA_DONE, CNL_ADSP_REG_HIPCTDA_DONE);
		ipc_irq = 1;
	}

	if (ipc_irq == 0)
		return IRQ_NONE;

	cnl_ipc_int_enable(dsp);

	/* continue to send any remaining messages... */
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

	/* Overriding tx_msg and is_dsp_busy since IPC registers are changed for CNL */
	ipc->ops.tx_msg = cnl_ipc_tx_msg;
	ipc->ops.tx_data_copy = skl_ipc_tx_data_copy;
	ipc->ops.is_dsp_busy = cnl_ipc_is_dsp_busy;

	return 0;
}

int cnl_sst_dsp_init_hw(struct device *dev, struct skl_sst **dsp,
			struct dsp_init *d)
{
	struct skl_sst *cnl;
	struct sst_dsp *sst;
	u32 dsp_wp[] = {CNL_ADSP_WP_DSP0, CNL_ADSP_WP_DSP1, CNL_ADSP_WP_DSP2,
				CNL_ADSP_WP_DSP3};
	int ret;

	cnl = devm_kzalloc(dev, sizeof(*cnl), GFP_KERNEL);
	if (cnl == NULL)
		return -ENOMEM;

	cnl->dev = dev;
	cnl_dev.thread_context = cnl;

	cnl->dsp = skl_dsp_ctx_init(dev, &cnl_dev, d->irq);
	if (!cnl->dsp) {
		dev_err(cnl->dev, "%s: no device\n", __func__);
		return -ENODEV;
	}

	sst = cnl->dsp;

	sst->dsp_ops = d->dsp_ops;
	sst->fw_ops = cnl_fw_ops;
	sst->addr.lpe = d->mmio_base;
	sst->addr.shim = d->mmio_base;
	sst_dsp_mailbox_init(sst, (CNL_ADSP_SRAM0_BASE + CNL_ADSP_W0_STAT_SZ),
			CNL_ADSP_W0_UP_SZ, CNL_ADSP_SRAM1_BASE, CNL_ADSP_W1_SZ);
	ret = skl_dsp_init_trace_window(sst, dsp_wp, CNL_ADSP_SRAM2_BASE,
					 CNL_ADSP_W2_SIZE, CNL_DSP_CORES);
	if (ret) {
		dev_err(dev, "FW tracing init failed : %x", ret);
		return ret;
	}
	INIT_LIST_HEAD(&sst->module_list);
	ret = cnl_ipc_init(dev, cnl);
	if (ret)
		return ret;

	sst->core_info.cores = 4;
	cnl->is_first_boot = true;

	cnl->boot_complete = false;
	init_waitqueue_head(&cnl->boot_wait);

	INIT_DELAYED_WORK(&cnl->d0i3_data.d0i3_work, cnl_set_dsp_D0i3);

	if (dsp)
		*dsp = cnl;

	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sst_dsp_init_hw);

int cnl_sst_dsp_init_fw(struct device *dev, struct skl_sst *ctx)
{
	int ret;

	ret = ctx->dsp->fw_ops.load_fw(ctx->dsp);
	if (ret < 0) {
		dev_err(dev, "Load base fw failed: %d", ret);
		return ret;
	}
	ctx->fw_loaded = true;

	if (ctx->manifest.lib_count > 1) {
		ret = ctx->dsp->fw_ops.load_library(ctx->dsp);
		if (ret < 0) {
			dev_err(dev, "Load library failed: %d", ret);
			return ret;
		}
	}

	/* First boot successfully done */
	ctx->is_first_boot = false;

	return 0;
}
EXPORT_SYMBOL_GPL(cnl_sst_dsp_init_fw);



void cnl_sst_dsp_cleanup(struct device *dev, struct skl_sst *ctx)
{
	cnl_ipc_free(&ctx->ipc);
	ctx->dsp->ops->free(ctx->dsp);
}
EXPORT_SYMBOL_GPL(cnl_sst_dsp_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Cannonlake IPC driver");
