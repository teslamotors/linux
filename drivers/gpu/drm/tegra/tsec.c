/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/host1x.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/tsec.h>

#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
#include <linux/clk/tegra.h>
#include <linux/platform/tegra/mc.h>
#else
#include <soc/tegra/pmc.h>
#endif

#include "tsec.h"
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "tsec_t186.h"
#endif

#define TSEC_OS_START_OFFSET 256
#define TSEC_AUTOSUSPEND_DELAY 500

#define TSEC_BO_CREATE(drm, size, flag, bo) \
	bo = tegra_bo_create(drm, size, flag); \
	if (IS_ERR(bo)) { \
		err = PTR_ERR(bo); \
		goto exit; \
	} \

#define hdcp_align(var) (((unsigned long)((u8 *)hdcp_context->var \
			+ HDCP_ALIGNMENT_256 - 1)) & ~HDCP_ALIGNMENT_256);

#define hdcp_align_dma(var) (((unsigned long)(hdcp_context->var \
			+ HDCP_ALIGNMENT_256 - 1)) & ~HDCP_ALIGNMENT_256);

/* TODO: use proper API's */
#define OPCODE_INCR(offset, count) ((1 << 28) | (offset << 16) | count)
#define OPCODE_NONINCR(offset, count) ((2 << 28) | (offset << 16) | count)

#define HOST1X_UCLASS_INCR_SYNCPT 0x0
#define HOST1X_UCLASS_INCR_SYNCPT_COND_OP_DONE_V 0x00000001

static struct tsec *gtsec;
static struct hdcp_context_priv_bo_t hdcp_context_bo;

static int tsec_boot(struct tsec *tsec);

static int tsec_runtime_resume(struct device *dev)
{
	struct tsec *tsec = dev_get_drvdata(dev);
	int err;

	err = clk_prepare_enable(tsec->clk);

	err = tsec_boot(tsec);
	if (err < 0)
		clk_disable_unprepare(tsec->clk);

	return err;
}

static int tsec_runtime_suspend(struct device *dev)
{
	struct tsec *tsec = dev_get_drvdata(dev);

	clk_disable_unprepare(tsec->clk);
	if (tsec->rst)
		reset_control_assert(tsec->rst);

	tsec->booted = false;

	return 0;
}

int drm_tsec_hdcp_create_context(struct hdcp_context_t *hdcp_context)
{
	int err = 0;

	if (!hdcp_context || !gtsec) {
		err = -EINVAL;
		goto exit;
	}

	memset(&hdcp_context_bo, 0, sizeof(struct hdcp_context_priv_bo_t));

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_SCRATCH_BUFFER_SIZE, 0,
		       hdcp_context_bo.scratch);
	hdcp_context->cpuvaddr_scratch = hdcp_context_bo.scratch->vaddr;
	hdcp_context->dma_handle_scratch = hdcp_context_bo.scratch->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_DCP_KPUB_SIZE_ALIGNED, 0,
		       hdcp_context_bo.dcp_kpub);
	hdcp_context->cpuvaddr_dcp_kpub = hdcp_context_bo.dcp_kpub->vaddr;
	hdcp_context->dma_handle_dcp_kpub = hdcp_context_bo.dcp_kpub->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_SRM_SIZE_ALIGNED, 0,
		       hdcp_context_bo.srm);
	hdcp_context->cpuvaddr_srm = hdcp_context_bo.srm->vaddr;
	hdcp_context->dma_handle_srm = hdcp_context_bo.srm->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_CERT_SIZE + HDCP_ALIGNMENT_256 - 1, 0,
		       hdcp_context_bo.cert);
	hdcp_context->cpuvaddr_cert = hdcp_context_bo.cert->vaddr;
	hdcp_context->dma_handle_cert = hdcp_context_bo.cert->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_MTHD_BUF_SIZE, 0,
		       hdcp_context_bo.mthd_buf);
	hdcp_context->cpuvaddr_mthd_buf = hdcp_context_bo.mthd_buf->vaddr;
	hdcp_context->dma_handle_mthd_buf = hdcp_context_bo.mthd_buf->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_RCVR_ID_LIST_SIZE, 0,
		       hdcp_context_bo.rcvr_id_list);
	hdcp_context->cpuvaddr_rcvr_id_list =
			hdcp_context_bo.rcvr_id_list->vaddr;
	hdcp_context->dma_handle_rcvr_id_list = hdcp_context_bo.rcvr_id_list->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_CONTENT_BUF_SIZE, 0,
		       hdcp_context_bo.input_buf);
	hdcp_context->cpuvaddr_input_buf = hdcp_context_bo.input_buf->vaddr;
	hdcp_context->dma_handle_input_buf = hdcp_context_bo.input_buf->paddr;

	TSEC_BO_CREATE(gtsec->tsec_drm, HDCP_CONTENT_BUF_SIZE, 0,
		       hdcp_context_bo.output_buf);
	hdcp_context->cpuvaddr_output_buf = hdcp_context_bo.output_buf->vaddr;
	hdcp_context->dma_handle_output_buf = hdcp_context_bo.output_buf->paddr;

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

exit:
	return err;
}

int drm_tsec_hdcp_free_context(struct hdcp_context_t *hdcp_context)
{
	int err = 0;

	if (!hdcp_context) {
		err = -EINVAL;
		goto exit;
	}

	tegra_bo_put(&hdcp_context_bo.scratch->base);
	tegra_bo_put(&hdcp_context_bo.dcp_kpub->base);
	tegra_bo_put(&hdcp_context_bo.srm->base);
	tegra_bo_put(&hdcp_context_bo.cert->base);
	tegra_bo_put(&hdcp_context_bo.mthd_buf->base);
	tegra_bo_put(&hdcp_context_bo.rcvr_id_list->base);
	tegra_bo_put(&hdcp_context_bo.input_buf->base);
	tegra_bo_put(&hdcp_context_bo.output_buf->base);
exit:
	return err;
}

static void tsec_write_method(u32 *buf, u32 mid, u32 data, u32 *offset)
{
	int i = 0;
	buf[i++] = OPCODE_INCR(NV_PSEC_THI_METHOD0 >> 2, 1);
	buf[i++] = mid >> 2;
	buf[i++] = OPCODE_INCR(NV_PSEC_THI_METHOD1 >> 2, 1);
	buf[i++] = data;
	*offset = *offset + 4;
}

static void write_method(u32 *buf, u32 op1, u32 op2, u32 *offset)
{
	int i = 0;
	buf[i++] = op1;
	buf[i++] = op2;
	*offset = *offset + 2;
}

static void tsec_execute_method(struct tegra_bo *bo, u32 opcode_len,
				u32 syncpt_id, u32 syncpt_incrs)
{

	struct host1x_job *job = NULL;
	struct host1x_syncpt *sp;
	struct host1x *host1x = dev_get_drvdata(gtsec->dev->parent);
	struct host1x_channel *channel = NULL;
	int err = 0;
	int fence = 0;

	err = pm_runtime_get_sync(gtsec->dev);
	if (err < 0)
		return;

	channel = host1x_channel_get(gtsec->channel);

	job = host1x_job_alloc(channel, 1, 0, 0, 1);
	if (!job)
		return;

	sp = host1x_syncpt_get(host1x, syncpt_id);
	job->num_syncpts = 1;
	job->syncpts[0].id = syncpt_id;
	job->syncpts[0].incrs = syncpt_incrs;

	host1x_job_add_client_gather_address(job, &bo->base, opcode_len,
					     HOST1X_CLASS_TSEC,
					     bo->paddr);

	err = host1x_job_submit(job);
	if (err)
		goto exit;

	host1x_channel_put(gtsec->channel);

	host1x_syncpt_wait(sp, job->syncpts[0].end, (u32)MAX_SCHEDULE_TIMEOUT,
			   &fence);

exit:
	host1x_job_put(job);
	job = NULL;
	return;
}

void drm_tsec_send_method(struct hdcp_context_t *hdcp_context,
		      u32 method, u32 flags)
{
	u32 opcode_len = 0;
	u32 *vaddr;
	u32 id = 0;
	u32 increment_opcode;
	struct tegra_bo *bo;

	id = host1x_syncpt_id(gtsec->client.base.syncpts[0]);
	if (!id)
		return;

	bo = tegra_bo_create(gtsec->tsec_drm, HDCP_MTHD_BUF_SIZE, 0);
	if (IS_ERR(bo))
		return;

	vaddr = bo->vaddr;

	tsec_write_method(&vaddr[opcode_len],
		SET_APPLICATION_ID,
		SET_APPLICATION_ID_ID_HDCP,
		&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_SB)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_SCRATCH_BUFFER,
			hdcp_context->dma_handle_scratch >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_DCP_KPUB)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_DCP_KPUB,
			hdcp_context->dma_handle_dcp_kpub_aligned >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_SRM)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_SRM,
			hdcp_context->dma_handle_srm >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_CERT)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_CERT_RX,
			hdcp_context->dma_handle_cert_aligned >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_RECV_ID_LIST)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_RECEIVER_ID_LIST,
			hdcp_context->dma_handle_rcvr_id_list >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_INPUT_BUFFER)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_ENC_INPUT_BUFFER,
			hdcp_context->dma_handle_input_buf >> 8,
			&opcode_len);

	if (flags & HDCP_MTHD_FLAGS_OUTPUT_BUFFER)
		tsec_write_method(&vaddr[opcode_len],
			HDCP_SET_ENC_OUTPUT_BUFFER,
			hdcp_context->dma_handle_output_buf >> 8,
			&opcode_len);

	tsec_write_method(&vaddr[opcode_len],
			method, hdcp_context->dma_handle_mthd_buf_aligned >> 8,
			&opcode_len);

	tsec_write_method(&vaddr[opcode_len],
			EXECUTE,
			0x100,
			&opcode_len);

	increment_opcode = HOST1X_UCLASS_INCR_SYNCPT_COND_OP_DONE_V << 8;
	increment_opcode |= id;

	write_method(&vaddr[opcode_len],
		    OPCODE_NONINCR(HOST1X_UCLASS_INCR_SYNCPT, 1),
		   increment_opcode, &opcode_len);

	tsec_execute_method(bo, opcode_len, id, 1);

	tegra_bo_put(&bo->base);
}

static int tsec_boot(struct tsec *tsec)
{
	int err = 0;
	struct tegra_drm_client *client = &tsec->client;

	if (tsec->booted)
		return 0;

	if (!tsec->falcon.firmware.valid) {
		err = falcon_read_firmware(&tsec->falcon,
					   tsec->config->ucode_name);
		if (err < 0)
			return err;
	}

	/* ensure that the engine is in sane state */
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	if (tsec->rst) {
		reset_control_assert(tsec->rst);
		usleep_range(10, 100);
		reset_control_deassert(tsec->rst);
	} else {
		tegra_mc_flush(true);
		tegra_periph_reset_assert(tsec->clk);
		usleep_range(10, 100);
		tegra_periph_reset_deassert(tsec->clk);
		tegra_mc_flush_done(true);
	}
#else
	if (tsec->rst) {
		reset_control_assert(tsec->rst);
		usleep_range(10, 100);
		reset_control_deassert(tsec->rst);
	}
#endif

	if (client->ops->load_regs)
		client->ops->load_regs(client);

	err = falcon_boot(&tsec->falcon);
	if (err < 0)
		return err;

	/*TODO: read kfuse */
	err = falcon_wait_idle(&tsec->falcon);
	if (err < 0) {
		dev_err(tsec->dev,
			"failed to set application ID and FCE base\n");
		return err;
	}

	tsec->booted = true;

	return 0;
}

static void *tsec_falcon_alloc(struct falcon *falcon, size_t size,
			       dma_addr_t *iova)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	return dma_alloc_writecombine(falcon->dev, size, iova,
					   GFP_KERNEL | __GFP_NOWARN);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_alloc(tegra, size, iova);
#endif
}

static void tsec_falcon_free(struct falcon *falcon, size_t size,
			    dma_addr_t iova, void *va)
{
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
	dma_free_writecombine(falcon->dev, size, va, iova);
#else
	struct tegra_drm *tegra = falcon->data;

	return tegra_drm_free(tegra, size, va, iova);
#endif
}

static const struct falcon_ops tsec_falcon_ops = {
	.alloc = tsec_falcon_alloc,
	.free = tsec_falcon_free
};

static int tsec_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct tsec *tsec = to_tsec(drm);
	int err;

	if (tegra->domain) {
		err = iommu_attach_device(tegra->domain, tsec->dev);
		if (err < 0) {
			dev_err(tsec->dev, "failed to attach to domain: %d\n",
				err);
			return err;
		}

		tsec->domain = tegra->domain;
	}

	tsec->falcon.firmware.os_start_offset = TSEC_OS_START_OFFSET;
	tsec->falcon.dev = tsec->dev;
	tsec->falcon.regs = tsec->regs;
	tsec->falcon.data = tegra;
	tsec->falcon.ops = &tsec_falcon_ops;
	err = falcon_init(&tsec->falcon);
	if (err < 0)
		goto detach_device;

	tsec->channel = host1x_channel_request(client->dev);
	if (!tsec->channel) {
		err = -ENOMEM;
		goto exit_falcon;
	}

	client->syncpts[0] = host1x_syncpt_request(client->dev, 0);
	if (!client->syncpts[0]) {
		err = -ENOMEM;
		goto free_channel;
	}

	err = tegra_drm_register_client(tegra, drm);
	if (err < 0)
		goto free_syncpt;

	if (!tsec->tsec_drm)
		tsec->tsec_drm = dev;

	return 0;

free_syncpt:
	host1x_syncpt_free(client->syncpts[0]);
free_channel:
	host1x_channel_free(tsec->channel);
exit_falcon:
	falcon_exit(&tsec->falcon);
detach_device:
	if (tegra->domain)
		iommu_detach_device(tegra->domain, tsec->dev);

	return err;
}

static int tsec_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = dev->dev_private;
	struct tsec *tsec = to_tsec(drm);
	int err;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	host1x_syncpt_free(client->syncpts[0]);
	host1x_channel_free(tsec->channel);

	if (tsec->booted) {
#ifdef CONFIG_DRM_TEGRA_DOWNSTREAM
		if (tsec->rst) {
			reset_control_assert(tsec->rst);
			usleep_range(10, 100);
			reset_control_deassert(tsec->rst);
		} else {
			tegra_mc_flush(true);
			tegra_periph_reset_assert(tsec->clk);
			usleep_range(10, 100);
			tegra_periph_reset_deassert(tsec->clk);
			tegra_mc_flush_done(true);
		}
#else
		if (tsec->rst) {
			reset_control_assert(tsec->rst);
			usleep_range(10, 100);
			reset_control_deassert(tsec->rst);
		}
#endif
	}

	falcon_exit(&tsec->falcon);

	if (tsec->domain) {
		iommu_detach_device(tsec->domain, tsec->dev);
		tsec->domain = NULL;
	}

	return 0;
}

static const struct host1x_client_ops tsec_client_ops = {
	.init = tsec_init,
	.exit = tsec_exit,
};

int tsec_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct tsec *tsec = to_tsec(client);

	context->channel = host1x_channel_get(tsec->channel);
	if (!context->channel)
		return -ENOMEM;

	return 0;
}

void tsec_close_channel(struct tegra_drm_context *context)
{
	host1x_channel_put(context->channel);
}

static const struct tegra_drm_client_ops tsec_ops = {
	.open_channel = tsec_open_channel,
	.close_channel = tsec_close_channel,
	.submit = tegra_drm_submit,
};

static const struct tsec_config tsec_t210_config = {
	.ucode_name = "tegra21x/nvhost_tsec.fw",
	.drm_client_ops = &tsec_ops,
	.class_id = HOST1X_CLASS_TSEC,
};

static const struct tsec_config tsecb_t210_config = {
	.ucode_name = "tegra21x/nvhost_tsec.fw",
	.drm_client_ops = &tsec_ops,
	.class_id = HOST1X_CLASS_TSECB,
};

static const struct of_device_id tsec_match[] = {
	{ .name = "tsec",
		.compatible = "nvidia,tegra210-tsec",
		.data = &tsec_t210_config },
	{ .name = "tsecb",
		.compatible = "nvidia,tegra210-tsec",
		.data = &tsecb_t210_config },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .name = "tsec",
		.compatible = "nvidia,tegra186-tsec",
		.data = &tsec_t186_config },
	{ .name = "tsecb",
		.compatible = "nvidia,tegra186-tsec",
		.data = &tsecb_t186_config },
#endif
	{ },
};

static int tsec_probe(struct platform_device *pdev)
{
	struct tsec_config *tsec_config = NULL;
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct resource *regs;
	const struct of_device_id *match;
	struct tsec *tsec;
	int err;

	match = of_match_device(tsec_match, dev);
	tsec_config = (struct tsec_config *)match->data;

	tsec = devm_kzalloc(dev, sizeof(*tsec), GFP_KERNEL);
	if (!tsec)
		return -ENOMEM;

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "failed to get registers\n");
		return -ENXIO;
	}

	tsec->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(tsec->regs))
		return PTR_ERR(tsec->regs);

	tsec->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(tsec->clk)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(tsec->clk);
	}

	tsec->rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(tsec->rst)) {
		dev_err(dev, "cannot get reset\n");
		tsec->rst = NULL;
	}

	platform_set_drvdata(pdev, tsec);

	INIT_LIST_HEAD(&tsec->client.base.list);
	tsec->client.base.ops = &tsec_client_ops;
	tsec->client.base.dev = dev;
	tsec->client.base.class = tsec_config->class_id;
	tsec->client.base.syncpts = syncpts;
	tsec->client.base.num_syncpts = 1;
	tsec->dev = dev;
	tsec->config = tsec_config;

	INIT_LIST_HEAD(&tsec->client.list);
	tsec->client.ops = tsec->config->drm_client_ops;

	err = host1x_client_register(&tsec->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	pm_runtime_set_autosuspend_delay(dev, TSEC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);
	if (!pm_runtime_enabled(dev)) {
		err = tsec_runtime_resume(dev);
		if (err < 0)
			goto unregister_client;
	}

	if (!gtsec)
		gtsec = tsec;

	/* TODO: map carveout */
	dev_info(dev, "initialized");

	return 0;

unregister_client:
	host1x_client_unregister(&tsec->client.base);

	return err;
}

static int tsec_remove(struct platform_device *pdev)
{
	struct tsec *tsec = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&tsec->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_disable(&pdev->dev);
	else
		tsec_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tsec_pm_ops = {
	SET_RUNTIME_PM_OPS(tsec_runtime_suspend, tsec_runtime_resume, NULL)
};

struct platform_driver tegra_tsec_driver = {
	.driver = {
		.name = "tegra-tsec",
		.of_match_table = tsec_match,
		.pm = &tsec_pm_ops
	},
	.probe = tsec_probe,
	.remove = tsec_remove,
};
