/*
 * Tegra TSEC Module Support
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>         /* for kzalloc */
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/clk/tegra.h>
#include <asm/byteorder.h>      /* for parsing ucode image wrt endianness */
#include <linux/delay.h>	/* for udelay */
#include <linux/scatterlist.h>
#include <linux/stop_machine.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/tegra_pm_domains.h>

#include <mach/hardware.h>

#include "dev.h"
#include "tsec.h"
#include "flcn/flcn.h"
#include "flcn/hw_flcn.h"
#include "hw_tsec.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "chip_support.h"
#include "nvhost_intr.h"
#include "t124/t124.h"
#include "t210/t210.h"
#include "nvhost_job.h"
#include "nvhost_channel.h"
#include "nvhost_cdma.h"
#include "host1x/host1x01_hardware.h"
#include "class_ids.h"
#include "tsec_methods.h"
#include "tsec_drv.h"
#include "nvhost_vm.h"

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "t186/t186.h"
#endif

#define TSEC_IDLE_TIMEOUT_DEFAULT	10000	/* 10 milliseconds */
#define TSEC_IDLE_CHECK_PERIOD		10	/* 10 usec */
#define TSEC_KEY_LENGTH			16
#define TSEC_RESERVE			256
#define TSEC_KEY_OFFSET			(TSEC_RESERVE - TSEC_KEY_LENGTH)

#define TSEC_OS_START_OFFSET    256

#define TSEC_CARVEOUT_ADDR_OFFSET	0
#define TSEC_CARVEOUT_SIZE_OFFSET	8

#define hdcp_align(var)	(((unsigned long)((u8 *)hdcp_context->var \
			+ HDCP_ALIGNMENT_256 - 1)) & ~HDCP_ALIGNMENT_256);

#define hdcp_align_dma(var) (((unsigned long)(hdcp_context->var \
			+ HDCP_ALIGNMENT_256 - 1)) & ~HDCP_ALIGNMENT_256);

#define FW_NAME_SIZE 32

static int nvhost_tsec_init_sw(struct platform_device *dev);

/* The key value in ascii hex */
static u8 otf_key[TSEC_KEY_LENGTH];

/* Pointer to this device */
static struct platform_device *tsec;

int tsec_hdcp_create_context(struct hdcp_context_t *hdcp_context)
{
	int err = 0;
	DEFINE_DMA_ATTRS(attrs);
	if (!hdcp_context) {
		err = -EINVAL;
		goto exit;
	}
	hdcp_context->cpuvaddr_scratch = dma_alloc_attrs(&tsec->dev,
					HDCP_SCRATCH_BUFFER_SIZE,
					&hdcp_context->dma_handle_scratch,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_scratch) {
		err = -ENOMEM;
		goto exit;
	}
	hdcp_context->cpuvaddr_dcp_kpub = dma_alloc_attrs(&tsec->dev,
					HDCP_DCP_KPUB_SIZE_ALIGNED,
					&hdcp_context->dma_handle_dcp_kpub,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_dcp_kpub) {
		err = -ENOMEM;
		goto exit;
	}
	if ((unsigned int)hdcp_context->dma_handle_dcp_kpub &
	(HDCP_ALIGNMENT_256 - 1)) {
		hdcp_context->cpuvaddr_dcp_kpub_aligned =
				(u32 *)hdcp_align(cpuvaddr_dcp_kpub);
		hdcp_context->dma_handle_dcp_kpub_aligned =
				(dma_addr_t)hdcp_align_dma(dma_handle_dcp_kpub);
	} else {
		hdcp_context->cpuvaddr_dcp_kpub_aligned =
				hdcp_context->cpuvaddr_dcp_kpub;
		hdcp_context->dma_handle_dcp_kpub_aligned =
				hdcp_context->dma_handle_dcp_kpub;
	}

	hdcp_context->cpuvaddr_srm = dma_alloc_attrs(&tsec->dev,
					HDCP_SRM_SIZE_ALIGNED,
					&hdcp_context->dma_handle_srm,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_srm) {
		err = -ENOMEM;
		goto exit;
	}

	hdcp_context->cpuvaddr_cert = dma_alloc_attrs(&tsec->dev,
				HDCP_CERT_SIZE + HDCP_ALIGNMENT_256 - 1,
				&hdcp_context->dma_handle_cert,
				GFP_KERNEL,
				&attrs);

	if (!hdcp_context->cpuvaddr_cert) {
		err = -ENOMEM;
		goto exit;
	}

	if ((unsigned int)hdcp_context->dma_handle_cert &
	(HDCP_ALIGNMENT_256 - 1)) {
		hdcp_context->cpuvaddr_cert_aligned =
				(u32 *)hdcp_align(cpuvaddr_cert);
		hdcp_context->dma_handle_cert_aligned =
				(dma_addr_t)hdcp_align_dma(dma_handle_cert);
	} else {
		hdcp_context->cpuvaddr_cert_aligned =
				hdcp_context->cpuvaddr_cert;
		hdcp_context->dma_handle_cert_aligned =
				hdcp_context->dma_handle_cert;
	}

	hdcp_context->cpuvaddr_mthd_buf = dma_alloc_attrs(&tsec->dev,
					HDCP_MTHD_BUF_SIZE,
					&hdcp_context->dma_handle_mthd_buf,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_mthd_buf) {
		err = -ENOMEM;
		goto exit;
	}

	if ((unsigned int)hdcp_context->dma_handle_mthd_buf &
	(HDCP_ALIGNMENT_256 - 1)) {
		hdcp_context->cpuvaddr_mthd_buf_aligned =
			(u32 *)hdcp_align(cpuvaddr_mthd_buf);
		hdcp_context->dma_handle_mthd_buf_aligned =
			(dma_addr_t)hdcp_align_dma(dma_handle_mthd_buf);
	} else {
		hdcp_context->cpuvaddr_mthd_buf_aligned =
				hdcp_context->cpuvaddr_mthd_buf;
		hdcp_context->dma_handle_mthd_buf_aligned =
				hdcp_context->dma_handle_mthd_buf;
	}

	hdcp_context->cpuvaddr_rcvr_id_list = dma_alloc_attrs(&tsec->dev,
					HDCP_RCVR_ID_LIST_SIZE,
					&hdcp_context->dma_handle_rcvr_id_list,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_rcvr_id_list) {
		err = -ENOMEM;
		goto exit;
	}

	hdcp_context->cpuvaddr_input_buf = dma_alloc_attrs(&tsec->dev,
					HDCP_CONTENT_BUF_SIZE,
					&hdcp_context->dma_handle_input_buf,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_input_buf) {
		err = -ENOMEM;
		goto exit;
	}

	hdcp_context->cpuvaddr_output_buf = dma_alloc_attrs(&tsec->dev,
					HDCP_CONTENT_BUF_SIZE,
					&hdcp_context->dma_handle_output_buf,
					GFP_KERNEL,
					&attrs);
	if (!hdcp_context->cpuvaddr_output_buf) {
		err = -ENOMEM;
		goto exit;
	}
exit:
	return err;
}

int tsec_hdcp_free_context(struct hdcp_context_t *hdcp_context)
{
	int err = 0;
	DEFINE_DMA_ATTRS(attrs);
	if (!hdcp_context) {
		err = -EINVAL;
		goto exit;
	}
	if (hdcp_context->cpuvaddr_scratch) {
		dma_free_attrs(&tsec->dev,
			HDCP_SCRATCH_BUFFER_SIZE,
			hdcp_context->cpuvaddr_scratch,
			hdcp_context->dma_handle_scratch,
			&attrs);
		hdcp_context->cpuvaddr_scratch = NULL;
	}
	if (hdcp_context->cpuvaddr_dcp_kpub) {
		dma_free_attrs(&tsec->dev,
			HDCP_DCP_KPUB_SIZE_ALIGNED,
			hdcp_context->cpuvaddr_dcp_kpub,
			hdcp_context->dma_handle_dcp_kpub,
			&attrs);
		hdcp_context->cpuvaddr_dcp_kpub = NULL;
	}
	if (hdcp_context->cpuvaddr_srm) {
		dma_free_attrs(&tsec->dev,
			HDCP_SRM_SIZE_ALIGNED,
			hdcp_context->cpuvaddr_srm,
			hdcp_context->dma_handle_srm,
			&attrs);
		hdcp_context->cpuvaddr_srm = NULL;
	}

	if (hdcp_context->cpuvaddr_cert) {
		dma_free_attrs(&tsec->dev,
			HDCP_CERT_SIZE_ALIGNED,
			hdcp_context->cpuvaddr_cert,
			hdcp_context->dma_handle_cert,
			&attrs);
		hdcp_context->cpuvaddr_cert = NULL;
	}

	if (hdcp_context->cpuvaddr_mthd_buf) {
		dma_free_attrs(&tsec->dev,
			HDCP_MTHD_BUF_SIZE,
			hdcp_context->cpuvaddr_mthd_buf,
			hdcp_context->dma_handle_mthd_buf,
			&attrs);
		hdcp_context->cpuvaddr_mthd_buf = NULL;
	}

	if (hdcp_context->cpuvaddr_rcvr_id_list) {
		dma_free_attrs(&tsec->dev,
			HDCP_RCVR_ID_LIST_SIZE,
			hdcp_context->cpuvaddr_rcvr_id_list,
			hdcp_context->dma_handle_rcvr_id_list,
			&attrs);
		hdcp_context->cpuvaddr_rcvr_id_list = NULL;
	}

	if (hdcp_context->cpuvaddr_input_buf) {
		dma_free_attrs(&tsec->dev,
			HDCP_CONTENT_BUF_SIZE,
			hdcp_context->cpuvaddr_input_buf,
			hdcp_context->dma_handle_input_buf,
			&attrs);
		hdcp_context->cpuvaddr_input_buf = NULL;
	}

	if (hdcp_context->cpuvaddr_output_buf) {
		dma_free_attrs(&tsec->dev,
			HDCP_CONTENT_BUF_SIZE,
			hdcp_context->cpuvaddr_output_buf,
			hdcp_context->dma_handle_output_buf,
			&attrs);
		hdcp_context->cpuvaddr_output_buf = NULL;
	}
exit:
	return err;

}

static void tsec_execute_method(dma_addr_t dma_handle,
	u32 *cpuvaddr,
	u32 opcode_len,
	u32 syncpt_id,
	u32 syncpt_incrs)
{
	struct nvhost_channel *channel = NULL;
	struct nvhost_job *job = NULL;
	int err = 0;
	struct nvhost_device_data *pdata = platform_get_drvdata(tsec);

	err = nvhost_channel_map(pdata, &channel, pdata);
	if (err) {
		nvhost_err(&tsec->dev, "Channel map failed\n");
		return;
	}

	job = nvhost_job_alloc(channel, 1, 0, 0, 1);
	if (!job) {
		nvhost_err(&tsec->dev, "failed to allocate job\n");
		return;
	}

	channel->syncpts[0] = syncpt_id;
	job->sp->id = syncpt_id;
	job->sp->incrs = syncpt_incrs;
	job->num_syncpts = 1;

	err = nvhost_job_add_client_gather_address(job, opcode_len,
				NV_TSEC_CLASS_ID, dma_handle);
	if (err) {
		nvhost_err(&tsec->dev, "failed to add gather\n");
		goto exit;
	}

	err = nvhost_channel_submit(job);
	if (err) {
		nvhost_err(&tsec->dev, "submit failed\n");
		goto exit;
	}

	/* submit takes a reference on the job structure; we can drop the
	 * local reference now */
	nvhost_putchannel(channel, 1);

	nvhost_syncpt_wait_timeout_ext(tsec, job->sp->id,
			job->sp->fence,
			(u32)MAX_SCHEDULE_TIMEOUT,
			NULL, NULL);

exit:
	nvhost_job_put(job);
	job = NULL;
	return;
}

static void tsec_write_mthd(u32 *buf, u32 mid, u32 data, u32 *offset)
{
	int i = 0;
	buf[i++] = nvhost_opcode_incr(NV_PSEC_THI_METHOD0>>2, 1);
	buf[i++] = mid>>2;
	buf[i++] = nvhost_opcode_incr(NV_PSEC_THI_METHOD1>>2, 1);
	buf[i++] = data;
	*offset = *offset + 4;
}

static void write_mthd(u32 *buf, u32 op1, u32 op2, u32 *offset)
{
	int i = 0;
	buf[i++] = op1;
	buf[i++] = op2;
	*offset = *offset + 2;
}

void tsec_send_method(struct hdcp_context_t *hdcp_context,
	u32 method, u32 flags)
{
	struct platform_device *host_dev =
		to_platform_device(tsec->dev.parent);
	struct nvhost_master *host = nvhost_get_private_data(host_dev);
	u32 opcode_len = 0;
	u32 *cpuvaddr = NULL;
	u32 id = 0;
	dma_addr_t dma_handle = 0;
	DEFINE_DMA_ATTRS(attrs);
	u32 increment_opcode;

	id = nvhost_get_syncpt_host_managed(tsec, 0, "tsec_hdcp");
	if (!id) {
		nvhost_err(&tsec->dev, "failed to get sync point\n");
		return;
	}

	cpuvaddr = dma_alloc_attrs(tsec->dev.parent, HDCP_MTHD_BUF_SIZE,
			&dma_handle, GFP_KERNEL,
			&attrs);
	if (!cpuvaddr) {
		nvhost_err(&tsec->dev, "Failed to allocate memory\n");
		return;
	}
	memset(cpuvaddr, 0x0, HDCP_MTHD_BUF_SIZE);

	tsec_write_mthd(&cpuvaddr[opcode_len],
		SET_APPLICATION_ID,
		SET_APPLICATION_ID_ID_HDCP,
		&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_SB)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_SCRATCH_BUFFER,
			hdcp_context->dma_handle_scratch>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_DCP_KPUB)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_DCP_KPUB,
			hdcp_context->dma_handle_dcp_kpub_aligned>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_SRM)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_SRM,
			hdcp_context->dma_handle_srm>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_CERT)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_CERT_RX,
			hdcp_context->dma_handle_cert_aligned>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_RECV_ID_LIST)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_RECEIVER_ID_LIST,
			hdcp_context->dma_handle_rcvr_id_list>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_INPUT_BUFFER)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_ENC_INPUT_BUFFER,
			hdcp_context->dma_handle_input_buf>>8,
			&opcode_len);
	if (flags & HDCP_MTHD_FLAGS_OUTPUT_BUFFER)
		tsec_write_mthd(&cpuvaddr[opcode_len],
			HDCP_SET_ENC_OUTPUT_BUFFER,
			hdcp_context->dma_handle_output_buf>>8,
			&opcode_len);
	tsec_write_mthd(&cpuvaddr[opcode_len],
			method, hdcp_context->dma_handle_mthd_buf_aligned>>8,
			&opcode_len);
	tsec_write_mthd(&cpuvaddr[opcode_len],
			EXECUTE,
			0x100,
			&opcode_len);

	/* check the width of the syncpoint field */
	if (host->info.nb_hw_pts > 255)
		increment_opcode =
			host1x_uclass_incr_syncpt_cond_op_done_v() << 10;
	else
		increment_opcode =
			host1x_uclass_incr_syncpt_cond_op_done_v() << 8;

	increment_opcode |= id;

	write_mthd(&cpuvaddr[opcode_len],
		   nvhost_opcode_nonincr(host1x_uclass_incr_syncpt_r(), 1),
		   increment_opcode, &opcode_len);

	tsec_execute_method(dma_handle, cpuvaddr, opcode_len, id, 1);

	nvhost_syncpt_put_ref_ext(tsec, id);

	dma_free_attrs(tsec->dev.parent,
		HDCP_MTHD_BUF_SIZE, cpuvaddr,
		dma_handle, &attrs);
}



/* caller is responsible for freeing */
static char *tsec_get_fw_name(struct platform_device *dev)
{
	char *fw_name;
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* note size here is a little over...*/
	fw_name = kzalloc(FW_NAME_SIZE, GFP_KERNEL);
	if (!fw_name)
		return NULL;

	decode_tsec_ver(pdata->version, &maj, &min);
	if (maj == 1) {
		/* there are no minor versions so far for maj==1 */
		snprintf(fw_name, FW_NAME_SIZE, "nvhost_tsec.fw");
	} else {
		kfree(fw_name);
		return NULL;
	}

	dev_dbg(&dev->dev, "fw name:%s\n", fw_name);

	return fw_name;
}

static int tsec_load_kfuse(struct platform_device *pdev)
{
	u32 val;
	u32 timeout;

	if (tegra_platform_is_linsim())
		return 0;

	val = host1x_readl(pdev, tsec_tegra_ctl_r());
	val &= ~tsec_tegra_ctl_tkfi_kfuse_m();
	host1x_writel(pdev, tsec_tegra_ctl_r(), val);

	val = host1x_readl(pdev, tsec_scp_ctl_pkey_r());
	val |= tsec_scp_ctl_pkey_request_reload_s();
	host1x_writel(pdev, tsec_scp_ctl_pkey_r(), val);

	timeout = TSEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, TSEC_IDLE_CHECK_PERIOD, timeout);
		u32 w = host1x_readl(pdev, tsec_scp_ctl_pkey_r());

		if (w & tsec_scp_ctl_pkey_loaded_m())
			break;
		udelay(TSEC_IDLE_CHECK_PERIOD);
		timeout -= check;
	} while (timeout || !tegra_platform_is_silicon());

	val = host1x_readl(pdev, tsec_tegra_ctl_r());
	val |= tsec_tegra_ctl_tkfi_kfuse_m();
	host1x_writel(pdev, tsec_tegra_ctl_r(), val);

	if (timeout)
		return 0;
	else
		return -1;
}

int nvhost_tsec_finalize_poweron(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	u32 timeout;
	u32 offset;
	int err = 0;
	struct flcn *m;

	err = nvhost_tsec_init_sw(dev);
	if (err)
		return err;

	m = get_flcn(dev);

	if (m->is_booted)
		return 0;

	err = flcn_wait_mem_scrubbing(dev);
	if (err)
		return err;

	/* load transcfg configuration if defined */
	if (pdata->transcfg_addr)
		host1x_writel(dev, pdata->transcfg_addr, pdata->transcfg_val);

	host1x_writel(dev, flcn_dmactl_r(), 0);
	host1x_writel(dev, flcn_dmatrfbase_r(),
		(m->dma_addr + m->os.bin_data_offset) >> 8);

	for (offset = 0; offset < m->os.data_size; offset += 256)
		flcn_dma_pa_to_internal_256b(dev,
					   m->os.data_offset + offset,
					   offset, false);

	flcn_dma_pa_to_internal_256b(dev,
				     m->os.code_offset+TSEC_OS_START_OFFSET,
				     TSEC_OS_START_OFFSET, true);


	/* boot tsec */
	host1x_writel(dev, flcn_bootvec_r(),
			     flcn_bootvec_vec_f(TSEC_OS_START_OFFSET));
	host1x_writel(dev, flcn_cpuctl_r(),
			flcn_cpuctl_startcpu_true_f());

	timeout = 0; /* default */

	err = flcn_wait_idle(dev, &timeout);
	if (err != 0) {
		dev_err(&dev->dev, "boot failed due to timeout");
		return err;
	}

	/* setup tsec interrupts and enable interface */
	host1x_writel(dev, flcn_irqmset_r(),
			(flcn_irqmset_ext_f(0xff) |
				flcn_irqmset_swgen1_set_f() |
				flcn_irqmset_swgen0_set_f() |
				flcn_irqmset_exterr_set_f() |
				flcn_irqmset_halt_set_f()   |
				flcn_irqmset_wdtmr_set_f()));

	host1x_writel(dev, flcn_itfen_r(),
			(flcn_itfen_mthden_enable_f() |
				flcn_itfen_ctxen_enable_f()));

	err = tsec_load_kfuse(dev);
	if (err)
		return err;
	m->is_booted = true;

	return err;
}

static int tsec_setup_ucode_image(struct platform_device *dev,
		u32 *ucode_ptr,
		const struct firmware *ucode_fw)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct flcn *m = get_flcn(dev);
	/* image data is little endian. */
	struct ucode_v1_flcn ucode;
	int err;
	u32 reserved_offset;
	u32 tsec_key_offset;
	u32 tsec_carveout_addr_off;
	u32 tsec_carveout_size_off;

	ucode.bin_header = (struct ucode_bin_header_v1_flcn *)ucode_ptr;

	err = flcn_setup_ucode_image(dev, ucode_ptr, ucode_fw);
	if (err)
		return err;

	/* make space for reserved area - we need 20 bytes, but we move 256
	 * bytes because firmware needs to be 256 byte aligned */
	reserved_offset = ucode.bin_header->os_bin_data_offset;
	memmove(((void *)ucode_ptr) + reserved_offset + TSEC_RESERVE,
			((void *)ucode_ptr) + reserved_offset,
			ucode.bin_header->os_bin_size);
	ucode.bin_header->os_bin_data_offset += TSEC_RESERVE;

	/*  clear 256 bytes before ucode os code */
	memset(((void *)ucode_ptr) + reserved_offset, 0, TSEC_RESERVE);

	/* Copy key to be the 16 bytes before the firmware */
	tsec_key_offset = reserved_offset + TSEC_KEY_OFFSET;
	memcpy(((void *)ucode_ptr) + tsec_key_offset, otf_key, TSEC_KEY_LENGTH);

	/* Copy tsec carveout address and size before the firmware */
	tsec_carveout_addr_off = reserved_offset + TSEC_CARVEOUT_ADDR_OFFSET;
	tsec_carveout_size_off = reserved_offset + TSEC_CARVEOUT_SIZE_OFFSET;

	*((dma_addr_t *)(((void *)ucode_ptr) + tsec_carveout_addr_off)) =
		pdata->carveout_addr;
	*((dma_addr_t *)(((void *)ucode_ptr) + tsec_carveout_size_off)) =
		pdata->carveout_size;

	m->os.size = ucode.bin_header->os_bin_size;
	m->os.reserved_offset = reserved_offset;
	m->os.bin_data_offset = ucode.bin_header->os_bin_data_offset;

	return 0;
}

static int tsec_read_ucode(struct platform_device *dev, const char *fw_name)
{
	struct flcn *m = get_flcn(dev);
	const struct firmware *ucode_fw;
	int err;
	DEFINE_DMA_ATTRS(attrs);

	m->dma_addr = 0;
	m->mapped = NULL;

	ucode_fw = nvhost_client_request_firmware(dev, fw_name);
	if (!ucode_fw) {
		dev_err(&dev->dev, "failed to get tsec firmware\n");
		err = -ENOENT;
		return err;
	}

	m->size = ucode_fw->size;
	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);

	m->mapped = dma_alloc_attrs(&dev->dev,
				m->size, &m->dma_addr,
				GFP_KERNEL, &attrs);
	if (!m->mapped) {
		dev_err(&dev->dev, "dma memory allocation failed");
		err = -ENOMEM;
		goto clean_up;
	}

	err = tsec_setup_ucode_image(dev, m->mapped, ucode_fw);
	if (err) {
		dev_err(&dev->dev, "failed to parse firmware image\n");
		goto clean_up;
	}

	m->valid = true;

	release_firmware(ucode_fw);

	return 0;

clean_up:
	if (m->mapped) {
		dma_free_attrs(&dev->dev,
			m->size, m->mapped,
			m->dma_addr, &attrs);
		m->mapped = NULL;
		m->dma_addr = 0;
	}
	release_firmware(ucode_fw);
	return err;
}

static int nvhost_tsec_init_sw(struct platform_device *dev)
{
	int err = 0;
	struct flcn *m = get_flcn(dev);
	char *fw_name;

	if (m)
		return 0;

	fw_name = tsec_get_fw_name(dev);
	if (!fw_name) {
		dev_err(&dev->dev, "couldn't determine firmware name");
		return -EINVAL;
	}

	m = kzalloc(sizeof(struct flcn), GFP_KERNEL);
	if (!m) {
		dev_err(&dev->dev, "couldn't alloc ucode");
		kfree(fw_name);
		return -ENOMEM;
	}
	set_flcn(dev, m);
	m->is_booted = false;

	err = tsec_read_ucode(dev, fw_name);
	kfree(fw_name);
	fw_name = NULL;

	if (err || !m->valid) {
		dev_err(&dev->dev, "ucode not valid");
		goto clean_up;
	}

	return 0;

clean_up:
	dev_err(&dev->dev, "failed");
	return err;
}

int nvhost_tsec_prepare_poweroff(struct platform_device *dev)
{
	struct flcn *m = get_flcn(dev);
	if (m)
		m->is_booted = false;

	return 0;
}


static struct of_device_id tegra_tsec_of_match[] = {
#ifdef TEGRA_12X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra124-tsec",
		.data = (struct nvhost_device_data *)&t124_tsec_info },
#endif
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{ .name = "tsec",
		.compatible = "nvidia,tegra210-tsec",
		.data = (struct nvhost_device_data *)&t21_tsec_info },
	{ .name = "tsecb",
		.compatible = "nvidia,tegra210-tsec",
		.data = (struct nvhost_device_data *)&t21_tsecb_info },
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .name = "tsec",
		.compatible = "nvidia,tegra186-tsec",
		.data = (struct nvhost_device_data *)&t18_tsec_info },
	{ .name = "tsecb",
		.compatible = "nvidia,tegra186-tsec",
		.data = (struct nvhost_device_data *)&t18_tsecb_info },
#endif
	{ },
};

static int tsec_probe(struct platform_device *dev)
{
	int err;
	struct device_node *node;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_tsec_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	err = nvhost_check_bondout(pdata->bond_out_id);
	if (err) {
		dev_err(&dev->dev, "No TSEC unit present. err:%d", err);
		return err;
	}

	pdata->pdev = dev;
	mutex_init(&pdata->lock);
	platform_set_drvdata(dev, pdata);

	node = of_find_node_by_name(dev->dev.of_node, "carveout");
	if (node) {
		DEFINE_DMA_ATTRS(attrs);
		err = of_property_read_u32(node, "carveout_addr",
					(u32 *)&pdata->carveout_addr);
		if (err) {
			dev_err(&dev->dev, "invalid carveout_addr\n");
			return -EINVAL;
		}

		err = of_property_read_u32(node, "carveout_size",
					(u32 *)&pdata->carveout_size);
		if (err) {
			dev_err(&dev->dev, "invalid carveout_size\n");
			return -EINVAL;
		}

		dma_set_attr(DMA_ATTR_SKIP_IOVA_GAP, &attrs);
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		pdata->carveout_addr = dma_map_single_attrs(&dev->dev,
					       __va(pdata->carveout_addr),
					       pdata->carveout_size,
					       DMA_TO_DEVICE,
					       &attrs);
		if (dma_mapping_error(&dev->dev, pdata->carveout_addr)) {
			dev_err(&dev->dev, "mapping to iova failed\n");
			return -EINVAL;
		}
	}

	err = nvhost_client_device_get_resources(dev);
	if (err)
		return err;
	if (!tsec)
		tsec = dev;
	nvhost_module_init(dev);

#ifdef CONFIG_PM_GENERIC_DOMAINS
	err = nvhost_module_add_domain(&pdata->pd, dev);
	if (err)
		return err;
#endif

	err = nvhost_client_device_init(dev);

	return err;
}

static int __exit tsec_remove(struct platform_device *dev)
{
	nvhost_client_device_release(dev);
	return 0;
}

static struct platform_driver tsec_driver = {
	.probe = tsec_probe,
	.remove = __exit_p(tsec_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "tsec",
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = tegra_tsec_of_match,
#endif
	}
};

static int __init tsec_key_setup(char *line)
{
	int i;
	u8 tmp[] = {0, 0, 0};
	pr_debug("tsec otf key: %s\n", line);

	if (strlen(line) != TSEC_KEY_LENGTH*2) {
		pr_warn("invalid tsec key: %s\n", line);
		return 0;
	}

	for (i = 0; i < TSEC_KEY_LENGTH; i++) {
		int err;
		memcpy(tmp, &line[i*2], 2);
		err = kstrtou8(tmp, 16, &otf_key[i]);
		if (err) {
			pr_warn("cannot read tsec otf key: %d", err);
			break;
		}
	}
	return 0;
}
__setup("otf_key=", tsec_key_setup);

static struct of_device_id tegra_tsec_domain_match[] = {
	{.compatible = "nvidia,tegra124-tsec-pd",
	 .data = (struct nvhost_device_data *)&t124_tsec_info},
	{.compatible = "nvidia,tegra132-tsec-pd",
	 .data = (struct nvhost_device_data *)&t124_tsec_info},
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{.compatible = "nvidia,tegra210-tsec-pd",
	 .data = (struct nvhost_device_data *)&t21_tsec_info},
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{.compatible = "nvidia,tegra186-tsec-pd",
	 .data = (struct nvhost_device_data *)&t18_tsec_info},
#endif
	{},
};

static int __init tsec_init(void)
{
	int ret;

	ret = nvhost_domain_init(tegra_tsec_domain_match);
	if (ret)
		return ret;

	return platform_driver_register(&tsec_driver);
}

static void __exit tsec_exit(void)
{
	platform_driver_unregister(&tsec_driver);
}

module_init(tsec_init);
module_exit(tsec_exit);
