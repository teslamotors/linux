// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/init_task.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#include <linux/sched.h>
#else
#include <uapi/linux/sched/types.h>
#endif
#include <linux/module.h>
#include <linux/fs.h>

#include "ipu.h"
#include "ipu-psys.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"
#define CREATE_TRACE_POINTS
#define IPU_PG_KCMD_TRACE
#include "ipu-trace-event.h"

static bool early_pg_transfer;
module_param(early_pg_transfer, bool, 0664);
MODULE_PARM_DESC(early_pg_transfer,
		 "Copy PGs back to user after resource allocation");

struct ipu_trace_block psys_trace_blocks[] = {
	{
	 .offset = TRACE_REG_PS_TRACE_UNIT_BASE,
	 .type = IPU_TRACE_BLOCK_TUN,
	},
	{
	 .offset = TRACE_REG_PS_SPC_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPP0_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPP1_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP0_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP1_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP2_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_ISP3_EVQ_BASE,
	 .type = IPU_TRACE_BLOCK_TM,
	},
	{
	 .offset = TRACE_REG_PS_SPC_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_SPP0_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_SPP1_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_MMU_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISL_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP0_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP1_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP2_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_ISP3_GPC_BASE,
	 .type = IPU_TRACE_BLOCK_GPC,
	},
	{
	 .offset = TRACE_REG_PS_GPREG_TRACE_TIMER_RST_N,
	 .type = IPU_TRACE_TIMER_RST,
	},
	{
	 .type = IPU_TRACE_BLOCK_END,
	}
};

static void set_sp_info_bits(void *base)
{
	int i;

	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
		   base + IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_CMEM_MASTER(i));
	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_XMEM_MASTER(i));
}

static void set_isp_info_bits(void *base)
{
	int i;

	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
		   base + IPU_REG_PSYS_INFO_SEG_0_CONFIG_ICACHE_MASTER);

	for (i = 0; i < 4; i++)
		writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
			   base + IPU_REG_PSYS_INFO_SEG_DATA_MASTER(i));
}

void ipu_psys_setup_hw(struct ipu_psys *psys)
{
	void __iomem *base = psys->pdata->base;
	void __iomem *spc_regs_base =
	    base + psys->pdata->ipdata->hw_variant.spc_offset;
	void *psys_iommu0_ctrl = base +
	    psys->pdata->ipdata->hw_variant.mmu_hw[0].offset +
		IPU_PSYS_MMU0_CTRL_OFFSET;
	const u8 *thd = psys->pdata->ipdata->hw_variant.cdc_fifo_threshold;
	u32 irqs;
	unsigned int i;

	/* Configure PSYS info bits */
	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY, psys_iommu0_ctrl);

	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPC_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPP0_STATUS_CTRL);
	set_sp_info_bits(spc_regs_base + IPU_PSYS_REG_SPP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP0_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP1_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP2_STATUS_CTRL);
	set_isp_info_bits(spc_regs_base + IPU_PSYS_REG_ISP3_STATUS_CTRL);

	/* Enable FW interrupt #0 */
	writel(0, base + IPU_REG_PSYS_GPDEV_FWIRQ(0));
	irqs = IPU_PSYS_GPDEV_IRQ_FWIRQ(0);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_EDGE);
	/*
	 * With pulse setting, driver misses interrupts. IUNIT integration
	 * HAS(v1.26) suggests to use pulse, but this seem to be error in
	 * documentation.
	 */
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_MASK);
	writel(irqs, base + IPU_REG_PSYS_GPDEV_IRQ_ENABLE);

	/* Write CDC FIFO threshold values for psys */
	for (i = 0; i < psys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(thd[i], base + IPU_REG_PSYS_CDC_THRESHOLD(i));
}

/*
 * Called to free up all resources associated with a kcmd.
 * After this the kcmd doesn't anymore exist in the driver.
 */
void ipu_psys_kcmd_free(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys *psys;
	unsigned long flags;

	if (!kcmd)
		return;

	psys = kcmd->fh->psys;

	if (!list_empty(&kcmd->list))
		list_del(&kcmd->list);

	spin_lock_irqsave(&psys->pgs_lock, flags);
	if (kcmd->kpg)
		kcmd->kpg->pg_size = 0;
	spin_unlock_irqrestore(&psys->pgs_lock, flags);

	mutex_lock(&kcmd->fh->bs_mutex);
	if (kcmd->kbuf_set)
		kcmd->kbuf_set->buf_set_size = 0;
	mutex_unlock(&kcmd->fh->bs_mutex);

	kfree(kcmd->pg_manifest);
	kfree(kcmd->kbufs);
	kfree(kcmd->buffers);
	kfree(kcmd);
}

struct ipu_psys_kcmd *ipu_psys_copy_cmd(struct ipu_psys_command *cmd,
					struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	struct ipu_psys_kbuffer *kpgbuf;
	unsigned int i;
	int ret, prevfd = 0;

	if (cmd->bufcount > IPU_MAX_PSYS_CMD_BUFFERS)
		return NULL;

	if (!cmd->pg_manifest_size ||
	    cmd->pg_manifest_size > KMALLOC_MAX_CACHE_SIZE)
		return NULL;

	kcmd = kzalloc(sizeof(*kcmd), GFP_KERNEL);
	if (!kcmd)
		return NULL;

	kcmd->state = KCMD_STATE_NEW;
	kcmd->fh = fh;
	INIT_LIST_HEAD(&kcmd->list);
	INIT_LIST_HEAD(&kcmd->started_list);

	mutex_lock(&fh->mutex);
	kpgbuf = ipu_psys_lookup_kbuffer(fh, cmd->pg);
	mutex_unlock(&fh->mutex);
	if (!kpgbuf || !kpgbuf->sgt)
		goto error;

	kcmd->pg_user = kpgbuf->kaddr;
	kcmd->kpg = __get_pg_buf(psys, kpgbuf->len);
	if (!kcmd->kpg)
		goto error;

	memcpy(kcmd->kpg->pg, kcmd->pg_user, kcmd->kpg->pg_size);

	kcmd->pg_manifest = kzalloc(cmd->pg_manifest_size, GFP_KERNEL);
	if (!kcmd->pg_manifest)
		goto error;

	ret = copy_from_user(kcmd->pg_manifest, cmd->pg_manifest,
			     cmd->pg_manifest_size);
	if (ret)
		goto error;

	kcmd->pg_manifest_size = cmd->pg_manifest_size;

	kcmd->user_token = cmd->user_token;
	kcmd->issue_id = cmd->issue_id;
	kcmd->priority = cmd->priority;
	if (kcmd->priority >= IPU_PSYS_CMD_PRIORITY_NUM)
		goto error;

	kcmd->nbuffers = ipu_fw_psys_pg_get_terminal_count(kcmd);
	kcmd->buffers = kcalloc(kcmd->nbuffers, sizeof(*kcmd->buffers),
				GFP_KERNEL);
	if (!kcmd->buffers)
		goto error;

	kcmd->kbufs = kcalloc(kcmd->nbuffers, sizeof(kcmd->kbufs[0]),
			      GFP_KERNEL);
	if (!kcmd->kbufs)
		goto error;


	if (!cmd->bufcount || kcmd->nbuffers > cmd->bufcount)
		goto error;

	ret = copy_from_user(kcmd->buffers, cmd->buffers,
			     kcmd->nbuffers * sizeof(*kcmd->buffers));
	if (ret)
		goto error;

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ipu_fw_psys_terminal *terminal;

		terminal = ipu_fw_psys_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;


		mutex_lock(&fh->mutex);
		kcmd->kbufs[i] = ipu_psys_lookup_kbuffer(fh,
						 kcmd->buffers[i].base.fd);
		mutex_unlock(&fh->mutex);
		if (!kcmd->kbufs[i] || !kcmd->kbufs[i]->sgt ||
		    kcmd->kbufs[i]->len < kcmd->buffers[i].bytes_used)
			goto error;
		if ((kcmd->kbufs[i]->flags &
		     IPU_BUFFER_FLAG_NO_FLUSH) ||
		    (kcmd->buffers[i].flags &
		     IPU_BUFFER_FLAG_NO_FLUSH) ||
		    prevfd == kcmd->buffers[i].base.fd)
			continue;

		prevfd = kcmd->buffers[i].base.fd;
		dma_sync_sg_for_device(&psys->adev->dev,
				       kcmd->kbufs[i]->sgt->sgl,
				       kcmd->kbufs[i]->sgt->orig_nents,
				       DMA_BIDIRECTIONAL);
	}


	return kcmd;
error:
	ipu_psys_kcmd_free(kcmd);

	dev_dbg(&psys->adev->dev, "failed to copy cmd\n");

	return NULL;
}

static void ipu_psys_kcmd_run(struct ipu_psys *psys)
{
	struct ipu_psys_kcmd *kcmd = list_first_entry(&psys->started_kcmds_list,
						      struct ipu_psys_kcmd,
						      started_list);
	int ret;

	ret = ipu_psys_move_resources(&psys->adev->dev,
				      &kcmd->resource_alloc,
				      &psys->resource_pool_started,
				      &psys->resource_pool_running);
	if (!ret) {
		psys->started_kcmds--;
		psys->active_kcmds++;
		kcmd->state = KCMD_STATE_RUNNING;
		list_del(&kcmd->started_list);
		kcmd->watchdog.expires = jiffies +
		    msecs_to_jiffies(psys->timeout);
		add_timer(&kcmd->watchdog);
		return;
	}

	if (ret != -ENOSPC || !psys->active_kcmds) {
		dev_err(&psys->adev->dev,
			"kcmd %p failed to alloc resources (running (%d, psys->active_kcmds = %d))\n",
			kcmd, ret, psys->active_kcmds);
		ipu_psys_kcmd_abort(psys, kcmd, ret);
		return;
	}
}

/*
 * Move kcmd into completed state (due to running finished or failure).
 * Fill up the event struct and notify waiters.
 */
void ipu_psys_kcmd_complete(struct ipu_psys *psys,
				   struct ipu_psys_kcmd *kcmd, int error)
{
	struct ipu_psys_fh *fh = kcmd->fh;

	trace_ipu_pg_kcmd(__func__, kcmd->user_token, kcmd->issue_id,
			  kcmd->priority,
			  ipu_fw_psys_pg_get_id(kcmd),
			  ipu_fw_psys_pg_load_cycles(kcmd),
			  ipu_fw_psys_pg_init_cycles(kcmd),
			  ipu_fw_psys_pg_processing_cycles(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUNNING:
		if (try_to_del_timer_sync(&kcmd->watchdog) < 0) {
			dev_err(&psys->adev->dev,
				"could not cancel kcmd timer\n");
			return;
		}
		/* Fall through on purpose */
	case KCMD_STATE_RUN_PREPARED:
		ipu_psys_free_resources(&kcmd->resource_alloc,
					&psys->resource_pool_running);
		if (psys->started_kcmds)
			ipu_psys_kcmd_run(psys);
		if (kcmd->state == KCMD_STATE_RUNNING)
			psys->active_kcmds--;
		break;
	case KCMD_STATE_STARTED:
		psys->started_kcmds--;
		list_del(&kcmd->started_list);
		/* Fall through on purpose */
	case KCMD_STATE_START_PREPARED:
		ipu_psys_free_resources(&kcmd->resource_alloc,
					&psys->resource_pool_started);
		break;
	default:
		break;
	}

	kcmd->ev.type = IPU_PSYS_EVENT_TYPE_CMD_COMPLETE;
	kcmd->ev.user_token = kcmd->user_token;
	kcmd->ev.issue_id = kcmd->issue_id;
	kcmd->ev.error = error;

	if (kcmd->constraint.min_freq)
		ipu_buttress_remove_psys_constraint(psys->adev->isp,
						    &kcmd->constraint);

	if (!early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg) {
		struct ipu_psys_kbuffer *kbuf;

		kbuf = ipu_psys_lookup_kbuffer_by_kaddr(kcmd->fh,
							kcmd->pg_user);

		if (kbuf && kbuf->valid)
			memcpy(kcmd->pg_user,
			       kcmd->kpg->pg, kcmd->kpg->pg_size);
		else
			dev_dbg(&psys->adev->dev,
				"Skipping already unmapped buffer\n");
	}

	if (kcmd->state == KCMD_STATE_RUNNING ||
	    kcmd->state == KCMD_STATE_STARTED) {
		pm_runtime_mark_last_busy(&psys->adev->dev);
		pm_runtime_put_autosuspend(&psys->adev->dev);
	}

	kcmd->state = KCMD_STATE_COMPLETE;

	wake_up_interruptible(&fh->wait);
}

/*
 * Move kcmd into completed state. If kcmd is currently running,
 * abort it.
 */
int ipu_psys_kcmd_abort(struct ipu_psys *psys,
			struct ipu_psys_kcmd *kcmd, int error)
{
	int ret = 0;

	if (kcmd->state == KCMD_STATE_COMPLETE)
		return 0;

	if ((kcmd->state == KCMD_STATE_RUNNING ||
	     kcmd->state == KCMD_STATE_STARTED)) {
		ret = ipu_fw_psys_pg_abort(kcmd);
		if (ret) {
			dev_err(&psys->adev->dev, "failed to abort kcmd!\n");
			goto out;
		}
	}

out:
	ipu_psys_kcmd_complete(psys, kcmd, ret);

	return ret;
}

/*
 * Submit kcmd into psys queue. If running fails, complete the kcmd
 * with an error.
 */
static int ipu_psys_kcmd_start(struct ipu_psys *psys,
			       struct ipu_psys_kcmd *kcmd)
{
	/*
	 * Found a runnable PG. Move queue to the list tail for round-robin
	 * scheduling and run the PG. Start the watchdog timer if the PG was
	 * started successfully. Enable PSYS power if requested.
	 */
	int ret;

	if (psys->adev->isp->flr_done) {
		ipu_psys_kcmd_complete(psys, kcmd, -EIO);
		return -EIO;
	}

	ret = pm_runtime_get_sync(&psys->adev->dev);
	if (ret < 0) {
		dev_err(&psys->adev->dev, "failed to power on PSYS\n");
		ipu_psys_kcmd_complete(psys, kcmd, -EIO);
		pm_runtime_put_noidle(&psys->adev->dev);
		return ret;
	}

	if (early_pg_transfer && kcmd->pg_user && kcmd->kpg->pg)
		memcpy(kcmd->pg_user, kcmd->kpg->pg, kcmd->kpg->pg_size);

	ret = ipu_fw_psys_pg_start(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	ipu_fw_psys_pg_dump(psys, kcmd, "run");

	/*
	 * Starting from scci_master_20151228_1800, pg start api is split into
	 * two different calls, making driver responsible to flush pg between
	 * start and disown library calls.
	 */
	clflush_cache_range(kcmd->kpg->pg, kcmd->kpg->pg_size);
	ret = ipu_fw_psys_pg_disown(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to start kcmd!\n");
		goto error;
	}

	trace_ipu_pg_kcmd(__func__, kcmd->user_token, kcmd->issue_id,
			  kcmd->priority,
			  ipu_fw_psys_pg_get_id(kcmd),
			  ipu_fw_psys_pg_load_cycles(kcmd),
			  ipu_fw_psys_pg_init_cycles(kcmd),
			  ipu_fw_psys_pg_processing_cycles(kcmd));

	switch (kcmd->state) {
	case KCMD_STATE_RUN_PREPARED:
		kcmd->state = KCMD_STATE_RUNNING;
		psys->active_kcmds++;
		kcmd->watchdog.expires = jiffies +
		    msecs_to_jiffies(psys->timeout);
		add_timer(&kcmd->watchdog);
		break;
	case KCMD_STATE_START_PREPARED:
		kcmd->state = KCMD_STATE_STARTED;
		psys->started_kcmds++;
		list_add_tail(&kcmd->started_list, &psys->started_kcmds_list);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		goto error;
	}
	return 0;

error:
	dev_err(&psys->adev->dev, "failed to start process group\n");
	ipu_psys_kcmd_complete(psys, kcmd, -EIO);
	return ret;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 2)
static void ipu_psys_watchdog(unsigned long data)
{
	struct ipu_psys_kcmd *kcmd = (struct ipu_psys_kcmd *)data;
#else
static void ipu_psys_watchdog(struct timer_list *t)
{
	struct ipu_psys_kcmd *kcmd = from_timer(kcmd, t, watchdog);
#endif
	struct ipu_psys *psys = kcmd->fh->psys;

	queue_work(IPU_PSYS_WORK_QUEUE, &psys->watchdog_work);
}

static int ipu_psys_config_legacy_pg(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys *psys = kcmd->fh->psys;
	unsigned int i;
	int ret;

	ret = ipu_fw_psys_pg_set_ipu_vaddress(kcmd, kcmd->kpg->pg_dma_addr);
	if (ret) {
		ret = -EIO;
		goto error;
	}

	for (i = 0; i < kcmd->nbuffers; i++) {
		struct ipu_fw_psys_terminal *terminal;
		u32 buffer;

		terminal = ipu_fw_psys_pg_get_terminal(kcmd, i);
		if (!terminal)
			continue;

		buffer = (u32) kcmd->kbufs[i]->dma_addr +
		    kcmd->buffers[i].data_offset;

		ret = ipu_fw_psys_terminal_set(terminal, i, kcmd,
					       buffer, kcmd->kbufs[i]->len);
		if (ret == -EAGAIN)
			continue;

		if (ret) {
			dev_err(&psys->adev->dev, "Unable to set terminal\n");
			goto error;
		}
	}

	ipu_fw_psys_pg_set_token(kcmd, (uintptr_t) kcmd);

	ret = ipu_fw_psys_pg_submit(kcmd);
	if (ret) {
		dev_err(&psys->adev->dev, "failed to submit kcmd!\n");
		goto error;
	}

	return 0;

error:
	dev_err(&psys->adev->dev, "failed to config legacy pg\n");
	return ret;
}

static bool ipu_psys_kcmd_is_valid(struct ipu_psys *psys,
				   struct ipu_psys_kcmd *kcmd)
{
	struct ipu_psys_fh *fh;
	struct ipu_psys_kcmd *kcmd0;
	int p;

	list_for_each_entry(fh, &psys->fhs, list) {
		mutex_lock(&fh->mutex);
		for (p = 0; p < IPU_PSYS_CMD_PRIORITY_NUM; p++) {
			list_for_each_entry(kcmd0, &fh->kcmds[p], list) {
				if (kcmd0 == kcmd) {
					mutex_unlock(&fh->mutex);
					return true;
				}
			}
		}
		mutex_unlock(&fh->mutex);
	}

	return false;
}

int ipu_psys_kcmd_queue(struct ipu_psys *psys, struct ipu_psys_kcmd *kcmd)
{
	int ret;

	if (kcmd->state != KCMD_STATE_NEW) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (!psys->started_kcmds) {
		ret = ipu_psys_allocate_resources(&psys->adev->dev,
						  kcmd->kpg->pg,
						  kcmd->pg_manifest,
						  &kcmd->resource_alloc,
						  &psys->resource_pool_running);
		if (!ret) {
			if (kcmd->state == KCMD_STATE_NEW)
				kcmd->state = KCMD_STATE_RUN_PREPARED;
			return ipu_psys_kcmd_start(psys, kcmd);
		}

		if (ret != -ENOSPC || !psys->active_kcmds) {
			dev_err(&psys->adev->dev,
				"kcmd %p failed to alloc resources (running)\n",
				kcmd);
			ipu_psys_kcmd_complete(psys, kcmd, ret);
			/* kcmd_complete doesn't handle PM for KCMD_STATE_NEW */
			pm_runtime_put(&psys->adev->dev);
			return -EINVAL;
		}
	}

	ret = ipu_psys_allocate_resources(&psys->adev->dev,
					  kcmd->kpg->pg,
					  kcmd->pg_manifest,
					  &kcmd->resource_alloc,
					  &psys->resource_pool_started);
	if (!ret) {
		kcmd->state = KCMD_STATE_START_PREPARED;
		return ipu_psys_kcmd_start(psys, kcmd);
	}

	if (ret != -ENOSPC || !psys->started_kcmds) {
		dev_err(&psys->adev->dev,
			"kcmd %p failed to alloc resources (started)\n", kcmd);
		ipu_psys_kcmd_complete(psys, kcmd, ret);
		/* kcmd_complete doesn't handle PM for KCMD_STATE_NEW */
		pm_runtime_put(&psys->adev->dev);
		ret = -EINVAL;
	}
	return ret;
}

int ipu_psys_kcmd_new(struct ipu_psys_command *cmd, struct ipu_psys_fh *fh)
{
	struct ipu_psys *psys = fh->psys;
	struct ipu_psys_kcmd *kcmd;
	size_t pg_size;
	int ret;

	if (psys->adev->isp->flr_done)
		return -EIO;

	kcmd = ipu_psys_copy_cmd(cmd, fh);
	if (!kcmd)
		return -EINVAL;

	ipu_psys_resource_alloc_init(&kcmd->resource_alloc);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 2)
	init_timer(&kcmd->watchdog);
	kcmd->watchdog.data = (unsigned long)kcmd;
	kcmd->watchdog.function = &ipu_psys_watchdog;
#else
	timer_setup(&kcmd->watchdog, ipu_psys_watchdog, 0);
#endif

	if (cmd->min_psys_freq) {
		kcmd->constraint.min_freq = cmd->min_psys_freq;
		ipu_buttress_add_psys_constraint(psys->adev->isp,
						 &kcmd->constraint);
	}

	pg_size = ipu_fw_psys_pg_get_size(kcmd);
	if (pg_size > kcmd->kpg->pg_size) {
		dev_dbg(&psys->adev->dev, "pg size mismatch %zu %zu\n",
			pg_size, kcmd->kpg->pg_size);
		ret = -EINVAL;
		goto error;
	}

	ret = ipu_psys_config_legacy_pg(kcmd);
	if (ret)
		goto error;

	mutex_lock(&fh->mutex);
	list_add_tail(&kcmd->list, &fh->kcmds[cmd->priority]);
	if (!fh->new_kcmd_tail[cmd->priority] && kcmd->state == KCMD_STATE_NEW) {
		fh->new_kcmd_tail[cmd->priority] = kcmd;
		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);
	}
	mutex_unlock(&fh->mutex);

	dev_dbg(&psys->adev->dev,
		"IOC_QCMD: user_token:%llx issue_id:0x%llx pri:%d\n",
		cmd->user_token, cmd->issue_id, cmd->priority);

	return 0;

error:
	ipu_psys_kcmd_free(kcmd);

	return ret;
}

void ipu_psys_handle_events(struct ipu_psys *psys)
{
	struct ipu_psys_kcmd *kcmd = NULL;
	struct ipu_fw_psys_event event;
	bool error;

	do {
		memset(&event, 0, sizeof(event));
		if (!ipu_fw_psys_rcv_event(psys, &event))
			break;

		error = false;
		kcmd = (struct ipu_psys_kcmd *)(unsigned long)event.token;
		error = IS_ERR_OR_NULL(kcmd) ? true : false;

		dev_dbg(&psys->adev->dev, "psys received event status:%d\n",
			event.status);

		if (error) {
			dev_err(&psys->adev->dev,
				"no token received, command unknown\n");
			pm_runtime_put(&psys->adev->dev);
			ipu_psys_reset(psys);
			pm_runtime_get(&psys->adev->dev);
			break;
		}

		if (ipu_psys_kcmd_is_valid(psys, kcmd))
			ipu_psys_kcmd_complete(psys, kcmd,
			       event.status ==
			       IPU_PSYS_EVENT_CMD_COMPLETE ||
			       event.status ==
			       IPU_PSYS_EVENT_FRAGMENT_COMPLETE
			       ? 0 : -EIO);
		/* Kick command scheduler thread */
		atomic_set(&psys->wakeup_sched_thread_count, 1);
		wake_up_interruptible(&psys->sched_cmd_wq);
	} while (1);
}
