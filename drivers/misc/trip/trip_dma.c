/*
 * Copyright (C) 2017-2018 Tesla, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_opp.h>
#include <linux/devfreq_cooling.h>

#include "trip_ioctl.h"
#include "trip_reg.h"
#include "trip_fw_fmt.h"
#include "trip_fw.h"
#include "trip_test.h"
#include "trip.h"

#define CREATE_TRACE_POINTS
#include <trace/events/trip.h>

/* mailbox to read ASV info */
int __attribute__((weak)) mail_box_read_asv_trip(void)
{
	return 0;
}

static inline u32 trip_net_readl(struct tripdev *td, unsigned int n,
					unsigned int reg)
{
	return readl(td->td_regs + reg + (n * TRIP_R_NET_OFFSET));
}

static inline void trip_net_writel(struct tripdev *td, u32 val,
					unsigned int n, unsigned int reg)
{
	writel(val, td->td_regs + reg + (n * TRIP_R_NET_OFFSET));
}

static inline u64 trip_readq(struct tripdev *td, unsigned int reg)
{
	return readq(td->td_regs + reg);
}

static inline void trip_writeq(struct tripdev *td, u64 val, unsigned int reg)
{
	writeq(val, td->td_regs + reg);
}

static inline u32 trip_readl(struct tripdev *td, unsigned int reg)
{
	return readl(td->td_regs + reg);
}

static inline void trip_writel(struct tripdev *td, u32 val, unsigned int reg)
{
	writel(val, td->td_regs + reg);
}

/* Set up watchdog for initialization, which is atypically large. */
void trip_init_wdt_memtest(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	trip_writel(td, TRIP_R_WDT_PROGRESS_CTL_DMA_IN_EN_BIT |
			TRIP_R_WDT_PROGRESS_CTL_DMA_OUT_EN_BIT |
			TRIP_R_WDT_PROGRESS_CTL_COMP_EN_BIT |
			0x1fffffff, TRIP_R_WDT_PROGRESS_CTL);
	trip_writel(td, TRIP_R_WDT_LAYER_CTL_EN_BIT |
			0x1fffffff, TRIP_R_WDT_LAYER_CTL);
	trip_writel(td, TRIP_R_WDT_NETWORK_CTL_EN_BIT |
			0x1fffffff, TRIP_R_WDT_NETWORK_CTL);
}

/* Mask parity, used during sram tests (access is single-threaded.) */
void trip_enable_parity(struct platform_device *pdev, bool enabled)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	uint32_t val;

	/* read-only register, assume all currently enabled */
	if (enabled)
		val = ~0;
	else
		val = ~(TRIP_R_ERROR_INTERRUPT_ENABLE_PARITY_ERR);
	trip_writel(td, val, TRIP_R_ERROR_INTERRUPT_ENABLE);
}

/* Setup descriptor for uncompressed DMA from DRAM to SRAM */
static void trip_desc_dma_read_input(u64 *buf, u64 dram_addr, u32 sram_addr,
					u32 len)
{
	buf[0] = (TRIP_ENG_CMD_DMA_READ_INPUT << TRIP_DMA_DESC_CMD_SHIFT) |
		(((u64) (sram_addr >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_SRAM_ADDR_SHIFT) &
			TRIP_DMA_DESC_SRAM_ADDR_MASK) |
		((dram_addr << TRIP_DMA_DESC_DRAM_ADDR_SHIFT) &
			TRIP_DMA_DESC_DRAM_ADDR_MASK);
	buf[1] = (((u64) (len >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_BLOCK_LEN_SHIFT) &
			TRIP_DMA_DESC_BLOCK_LEN_MASK) |
		(((u64) (len >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_BUF_SZ_SHIFT) &
			TRIP_DMA_DESC_BUF_SZ_MASK);
	buf[2] = 0;
	buf[3] = 0;
}

/* Setup descriptor for uncompressed DMA from DRAM to SRAM */
static void trip_desc_dma_write_output(u64 *buf, u32 sram_addr, u64 dram_addr,
					u32 len)
{
	buf[0] = (TRIP_ENG_CMD_DMA_WRITE_OUTPUT << TRIP_DMA_DESC_CMD_SHIFT) |
		TRIP_DMA_DESC_INPUT_DEP_NONE |
		(((u64) (sram_addr >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_SRAM_ADDR_SHIFT) &
			TRIP_DMA_DESC_SRAM_ADDR_MASK) |
		((dram_addr << TRIP_DMA_DESC_DRAM_ADDR_SHIFT) &
			TRIP_DMA_DESC_DRAM_ADDR_MASK);
	buf[1] = (((u64) (len >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_BLOCK_LEN_SHIFT) &
			TRIP_DMA_DESC_BLOCK_LEN_MASK) |
		(((u64) (len >> TRIP_DMA_ALIGNMENT_SHIFT) <<
			TRIP_DMA_DESC_BUF_SZ_SHIFT) &
			TRIP_DMA_DESC_BUF_SZ_MASK);
	buf[2] = 0;
	buf[3] = 0;
}


/* Setup descriptor for stop command */
static void trip_desc_stop(u64 *buf)
{
	buf[0] = (TRIP_ENG_CMD_STOP << TRIP_DMA_DESC_CMD_SHIFT);
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
}

/* Copy provided buffer to sram at given u64-aligned offset, using PIO. */
static int trip_copy_to_sram(struct platform_device *pdev, const u64 *buf,
				u32 len, u32 offset)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	if (((offset & 7) != 0) || ((len & 7) != 0))
		return -EINVAL;

	__iowmb();
	while (len) {
		writeq_relaxed(*buf++, td->td_sram + offset);
		offset += sizeof(u64);
		len -= sizeof(u64);
	}

	return 0;
}

/*
 * Copy provided buffer to sram at given u64-aligned offset, using PIO.
 * Then verifies data was written successfully.
 */
int trip_copy_to_sram_verified(struct platform_device *pdev,
					const u64 *buf, u32 len, u32 offset)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	u32 count;

	if (((offset & 7) != 0) || ((len & 7) != 0))
		return -EINVAL;

	__iowmb();
	for (count = 0; count < (len / sizeof(uint64_t)); count++) {
		writeq_relaxed(*(buf + count), td->td_sram + offset +
				count * sizeof(uint64_t));
	}

	for (count = 0; count < (len / sizeof(uint64_t)); count++) {
		u32 cur_ofs = count * sizeof(uint64_t);
		u64 val = readq(td->td_sram + offset + cur_ofs);
		u64 expected = *(buf + count);

		if (val != expected) {
			dev_err(&pdev->dev, "sram miscompare @%x: read %llx, expected %llx\n",
					cur_ofs, val, expected);
			return -EIO;
		}
	}

	return 0;
}

static int trip_net_start(struct tripdev *td, int n,
			  u32 sram_addr, dma_addr_t data_base,
			  dma_addr_t weight_base, dma_addr_t output_base)
{
	if ((sram_addr & TRIP_R_NW_CONTROL_CODE_ADDR_MASK) != sram_addr)
		return -EIO;

	/* set input data base register if provided, zero if omitted */
	trip_net_writel(td, TRIP_BASE_ADDR(data_base),
			n, TRIP_R_INPUT_DATA_BASE);

	/* set input weight base register if provided, zero if omitted */
	trip_net_writel(td, TRIP_BASE_ADDR(weight_base),
			n, TRIP_R_INPUT_WEIGHT_BASE);

	/* set output base register if provided, zero if omitted */
	trip_net_writel(td, TRIP_BASE_ADDR(output_base),
			n, TRIP_R_OUTPUT_BASE);

	trip_net_writel(td, TRIP_R_NW_CONTROL_INT_EN_BIT | sram_addr,
			n, TRIP_R_NW_CONTROL);
	trace_trip_net_start(td->id, n, sram_addr);

	return 0;
}

/*
 * Launches one request on hardware.
 * Must be called with td->serial_lock held.
 */
static void trip_start_req_serial(struct tripdev *td, struct trip_req *req)
{
		/* Must be called with td->serial_lock held */
		list_del(&req->list);
		list_add_tail(&req->list, &td->started_reqs);

		/* kick off the current request */
		req->state = TRIP_REQ_PENDING;
		trip_net_start(td, req->tn->idx, req->sram_addr,
				req->data_base, req->weight_base,
				req->output_base);
}

void trip_submit_lock(struct tripdev *td)
{
	if (READ_ONCE(td->serialized)) {
		mutex_lock(&td->submit_mutex);

		/*
		 * There is a narrow race where we observe td->serialized true
		 * but then end up blocking on submit_mutex being held by
		 * another thread clearing td->serialized.  Handle this case
		 * by dropping the mutex again here.
		 */
		if (unlikely(!READ_ONCE(td->serialized)))
			mutex_unlock(&td->submit_mutex);
	}
}

void trip_submit_unlock(struct tripdev *td)
{
	if (READ_ONCE(td->serialized))
		mutex_unlock(&td->submit_mutex);
}

/*
 * Attempt to start requests if none are scheduled on HW.
 * Must be called with td->serial_lock held
 */
static void trip_kick_reqs_serial(struct tripdev *td)
{
	struct trip_req *req, *first_req = NULL;

	if (list_empty(&td->started_reqs)) {
		first_req = list_first_entry_or_null(&td->waiting_reqs,
					struct trip_req, list);

		/* move to started_reqs and launch on HW */
		if (first_req != NULL)
			trip_start_req_serial(td, first_req);
	} else {
		if (list_is_singular(&td->started_reqs)) {
			first_req = list_first_entry(&td->started_reqs,
					struct trip_req, list);
		} else {
			/* Nothing to do at this point, bail. */
			return;
		}
	}

	/* Try to kick off a second request where possible */
	if (first_req != NULL) {
		/*
		 * Can start a second request if we have one, but only if
		 * second is not enqueued for the same network number.  This
		 * also enforces serialization since we only allow serial
		 * requests on the same network number and we always enqueue
		 * them sequentially.
		 */
		req = list_first_entry_or_null(&td->waiting_reqs,
						struct trip_req, list);
		if (req != NULL && first_req->tn != req->tn)
			trip_start_req_serial(td, req);
	} else {
		wake_up_all(&td->idle_wq);
	}
}

static void trip_add_req_serial(struct tripdev *td, struct trip_req *req)
{
	unsigned long flags;

	spin_lock_irqsave(&td->serial_lock, flags);
	list_add_tail(&req->list, &td->waiting_reqs);
	trip_kick_reqs_serial(td);
	spin_unlock_irqrestore(&td->serial_lock, flags);
}

static void trip_net_add_req(struct tripdev *td, struct trip_net *tn,
				struct trip_req *req)
{
	unsigned long flags;
	int was_empty;

	spin_lock_irqsave(&tn->net_lock, flags);

	/*
	 * There is some small chance we've lost a race and ended up here
	 * during switch to serialized mode. This is hard to prevent without
	 * introducing a single point of contention but we can detect
	 * and resolve here as a special case readily enough.
	 */
	if (unlikely(td->serialized)) {
		/* Drop spinlock and pick up mutex, then retry as serial */
		spin_unlock_irqrestore(&tn->net_lock, flags);
		mutex_lock(&td->submit_mutex);

		/*
		 * Pick up network lock again, just in case we were really
		 * unlucky and lost another race.  We have taken care to
		 * grab mutex unconditionally so can't lose any further.
		 */
		spin_lock_irqsave(&tn->net_lock, flags);
		if (likely(td->serialized)) {
			spin_unlock_irqrestore(&tn->net_lock, flags);
			return trip_add_req_serial(td, req);
		}
		mutex_unlock(&td->submit_mutex);
	}

	was_empty = list_empty(&tn->pending_reqs);
	list_add_tail(&req->list, &tn->pending_reqs);

	if (was_empty) {
		/* kick off the current request */
		req->state = TRIP_REQ_PENDING;
		trip_net_start(td, req->tn->idx, req->sram_addr,
				req->data_base, req->weight_base,
				req->output_base);
	}
	spin_unlock_irqrestore(&tn->net_lock, flags);
}

void trip_submit_req(struct tripdev *td, struct trip_req *req, int n)
{
	INIT_LIST_HEAD(&req->list);
	req->tn = &td->net[n];
	req->state = TRIP_REQ_NEW;

	if (td->serialized)
		trip_add_req_serial(td, req);
	else
		trip_net_add_req(td, req->tn, req);
}

struct trip_req *trip_net_submit(struct tripdev *td, int n,
			  u32 sram_addr, dma_addr_t data_base,
			  dma_addr_t weight_base, dma_addr_t output_base)
{
	struct trip_req *req;

	if (n < 0 || n >= TRIP_NET_MAX)
		return ERR_PTR(-EINVAL);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return ERR_PTR(-ENOMEM);

	req->sram_addr = sram_addr;
	req->data_base = data_base;
	req->weight_base = weight_base;
	req->output_base = output_base;

	init_completion(&req->completion);

	trip_submit_req(td, req, n);

	return req;
}

int trip_set_serialized_mode(struct tripdev *td, bool serialized)
{
	int n, ret = 0;
	bool was_serialized;

	/*
	 * Allow switching modes only when trip is idle, so we don't end up
	 * losing track of queued requests or fail to handle them properly.
	 *
	 * This is fiddly because we want to ensure there are no requests
	 * outstanding and prevent new ones from showing up, but in the
	 * non-serialized case we don't serialize submissions on a single
	 * mutex.  So we have to grab all of the queue locks to prevent
	 * any new work from getting queued while we're setting the mode.
	 *
	 * We still grab the submit mutex here even when not in serialized
	 * mode, which serves to serialize any concurrent attempts to set
	 * the serialization mode.
	 */
	mutex_lock(&td->submit_mutex);

	was_serialized = td->serialized;
	if (!was_serialized) {
		/* Grab all the per-network locks in order. */
		for (n = 0; n < TRIP_NET_MAX; n++)
			spin_lock_irq(&td->net[n].net_lock);

		/* No new reqs can appear now, so check all at once. */
		for (n = 0; n < TRIP_NET_MAX; n++) {
			if (!list_empty(&td->net[n].pending_reqs))
				ret = -EBUSY;
		}
	} else {
		spin_lock_irq(&td->serial_lock);
		if (!list_empty(&td->started_reqs) ||
				!list_empty(&td->waiting_reqs))
			ret = -EBUSY;
	}

	if (ret == 0) {

		/* If we get here, safe to change due to no reqs queued.
		 *
		 * There is still one last race where a task may have skipped
		 * acquiring submit_mutex due to observing !td->serialized
		 * but has not yet picked up the net_lock.  We handle this
		 * as a special case in trip_net_add_req() in order to
		 * keep the non-serialized mode free of single points of
		 * contention.
		 */
		td->serialized = serialized;
		dev_info(&td->pdev->dev, "serialization mode: %s\n",
			(td->serialized) ? "enabled" : "disabled");
	}

	if (!was_serialized) {
		/* Release locks in reverse order. */
		for (n = TRIP_NET_MAX - 1; n >= 0; n--)
			spin_unlock_irq(&td->net[n].net_lock);
	} else {
		spin_unlock_irq(&td->serial_lock);
	}

	mutex_unlock(&td->submit_mutex);

	return ret;
}

int trip_check_status(u32 hw_status, u32 *xfer_status)
{
	u32 err_type;

	if ((hw_status & TRIP_R_NW_STATUS_ERROR_BIT) == 0) {
		if ((hw_status & TRIP_R_NW_STATUS_COMPLETE_BIT) != 0) {
			*xfer_status = TRIP_IOCTL_STATUS_OK;
			return 0;
		}
		*xfer_status = TRIP_IOCTL_STATUS_ERR_TIMEOUT;
		return -EIO;
	}

	/* map error response to ioctl range */
	err_type = (hw_status & TRIP_R_NW_STATUS_ERROR_TYPE_MASK) >>
		TRIP_R_NW_STATUS_ERROR_TYPE_SHIFT;
	*xfer_status = TRIP_IOCTL_STATUS_ERR_BAD_COMMAND +
		err_type - TRIP_R_NW_STATUS_ERROR_BADCMD;

	return -EIO;
}

#ifdef TRIP_DMA_TEST_AT_BOOT
#ifdef TRIP_DISABLE_WDT
/* Forcibly disable watchdogs, mainly left here for dev use cases. */
void trip_disable_wdt(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	trip_writel(td, 0, TRIP_R_WDT_PROGRESS_CTL);
	trip_writel(td, 0, TRIP_R_WDT_LAYER_CTL);
	trip_writel(td, 0, TRIP_R_WDT_NETWORK_CTL);
}
#else /* TRIP_DISABLE_WDT */
void trip_disable_wdt(struct platform_device *pdev)
{


}
#endif /* TRIP_DISABLE_WDT */

/* Perform one basic synchronous DMA operation, more useful for simulation. */
static int trip_dma_test(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct trip_req *req;
	u64 dma_desc[8] = { 0 };
	const u64 dram_len = PAGE_SIZE;
	u64 *dram_buf;
	dma_addr_t dram_addr;
	int ret;
	uint32_t status;

	/* Hook to allow disabling wdt for testing */
	trip_disable_wdt(pdev);

	dram_buf = kzalloc(dram_len, GFP_KERNEL);
	if (!dram_buf)
		return -ENOMEM;

	/* Send some data to SRAM */
	memset(dram_buf, 0x42, dram_len);

	dram_addr = dma_map_single(&pdev->dev, dram_buf, dram_len,
					DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, dram_addr)) {
		dev_err(&pdev->dev, "dma_mapping error\n");
		kfree(dram_buf);
		return -ENOMEM;
	}

	trip_desc_dma_read_input(&dma_desc[0], 0, 0x100, dram_len);
	trip_desc_stop(&dma_desc[4]);

	trip_copy_to_sram(pdev, dma_desc, sizeof(dma_desc), 0x0);

	dev_info(&pdev->dev, "completions: %llu\n", td->net[0].completions);

	trip_dump_sram(pdev, 0, 16);
	pm_runtime_get_sync(&td->pdev->dev);	/* prevent idle */
	req = trip_net_submit(td, 0, 0x0, dram_addr, 0, 0);
	if (IS_ERR(req)) {
		pm_runtime_put(&td->pdev->dev);
		dma_unmap_single(&pdev->dev, dram_addr, dram_len,
				 DMA_TO_DEVICE);
		kfree(dram_buf);
		return PTR_ERR(req);
	}
	wait_for_completion(&req->completion);
	pm_runtime_put(&td->pdev->dev);
	ret = trip_check_status(req->nw_status, &status);
	if (ret != 0) {
		dev_err(&pdev->dev, "dma to device failed %d, 0x%08x\n",
					status, req->nw_status);
	}
	kfree(req);

	dma_unmap_single(&pdev->dev, dram_addr, dram_len, DMA_TO_DEVICE);

	trip_dump_sram(pdev, 0x3ff8, 16);
	trip_dump_sram(pdev, 0x4ff8, 16);

	dev_info(&pdev->dev, "completions: %llu\n", td->net[0].completions);

	/* And back again */
	memset(dram_buf, 0x76, dram_len);

	dram_addr = dma_map_single(&pdev->dev, dram_buf, dram_len,
					DMA_FROM_DEVICE);
	if (dma_mapping_error(&pdev->dev, dram_addr)) {
		dev_err(&pdev->dev, "dma_mapping error\n");
		kfree(dram_buf);
		return -ENOMEM;
	}

	trip_desc_dma_write_output(&dma_desc[0], 0x100, 0, dram_len);
	trip_desc_stop(&dma_desc[4]);

	trip_copy_to_sram(pdev, dma_desc, sizeof(dma_desc), 0x0);

	pm_runtime_get_sync(&td->pdev->dev);	/* prevent idle */
	req = trip_net_submit(td, 0, 0x0, 0, 0, dram_addr);
	if (IS_ERR(req)) {
		pm_runtime_put(&td->pdev->dev);
		dma_unmap_single(&pdev->dev, dram_addr, dram_len,
				 DMA_FROM_DEVICE);
		kfree(dram_buf);
		return PTR_ERR(req);
	}
	wait_for_completion(&req->completion);
	pm_runtime_put(&td->pdev->dev);
	ret = trip_check_status(req->nw_status, &status);
	if (ret != 0) {
		dev_err(&pdev->dev, "dma from device failed %d, 0x%08x\n",
					status, req->nw_status);
	}
	kfree(req);

	dev_info(&pdev->dev, "completions: %llu\n", td->net[0].completions);

	dma_unmap_single(&pdev->dev, dram_addr, dram_len, DMA_FROM_DEVICE);

	dev_info(&pdev->dev, "dram_buf: %llx\n", *dram_buf);

	kfree(dram_buf);

	return 0;
}
#else	/* TRIP_DMA_TEST_AT_BOOT */
static int trip_dma_test(struct platform_device *pdev)
{
	return 0;
}
#endif	/* TRIP_DMA_TEST_AT_BOOT */

/* Now done in coreboot as a dependency to clock and trustzone setup */
#ifdef TRIP_RELEASE_RESET
#define TRAV_PMU_RESET_SEQUENCER	0x11400504
#define TRAV_PMU_RESET_VALUE		0x4c

/* hack to pull trip out of reset, PoR seems to be to have M7 handle this */
static int trip_release_reset(struct platform_device *pdev)
{
	void __iomem *addr;

	addr = ioremap(TRAV_PMU_RESET_SEQUENCER, sizeof(u32));
	if (!addr) {
		dev_err(&pdev->dev, "Failed to map trip PMU reset reg.\n");
		return -ENOMEM;
	}
	/* releases reset for both TRIP0 and TRIP1, but sim lacks TRIP1. */
	writel(TRAV_PMU_RESET_VALUE, addr);
	iounmap(addr);
	return 0;
}
#else /* TRIP_RELEASE_RESET */
static int trip_release_reset(struct platform_device *pdev)
{
	return 0;
}
#endif /* TRIP_RELEASE_RESET */

static irqreturn_t trip_net_comp_isr(int irq, void *net_instance)
{
	struct trip_net *tn = net_instance;
	struct tripdev *td = tn->ptd;
	struct trip_req *req;
	spinlock_t *q_lock = (td->serialized) ? &td->serial_lock
					      : &tn->net_lock;
	unsigned long flags;
	u32 pending;

	pending = trip_readl(td, TRIP_R_INTERRUPT_PENDING);

	pending = pending & TRIP_R_INTERRUPT_PENDING_COMPLETION(tn->idx);
	if (pending) {
		spin_lock_irqsave(q_lock, flags);
		tn->completions++;

		/* mark first request as completed */
		if (td->serialized) {
			req = list_first_entry_or_null(&td->started_reqs,
					struct trip_req, list);
		} else {
			req = list_first_entry_or_null(&tn->pending_reqs,
					struct trip_req, list);
		}
		if (req != NULL) {
			/* verify TRIP_REQ_PENDING? */
			req->state = TRIP_REQ_DONE;
			req->nw_status = trip_net_readl(td, req->tn->idx,
							TRIP_R_NW_STATUS);
			trace_trip_net_done(td->id, req->tn->idx,
						req->nw_status &
						TRIP_R_NW_STATUS_OUT);
			list_del(&req->list);

			if (req->callback != NULL)
				req->callback(req, req->cb_data);
			else
				complete_all(&req->completion);

		} else {
			/* Flag spurious completion? */
		}

		trip_writel(td, pending, TRIP_R_INTERRUPT_PENDING);

		/* submit next request(s), if possible */
		if (td->serialized) {
			trip_kick_reqs_serial(td);
		} else {
			if (!list_empty(&tn->pending_reqs)) {
				req = list_first_entry(&tn->pending_reqs,
							struct trip_req, list);
				req->state = TRIP_REQ_PENDING;
				trip_net_start(td, req->tn->idx,
						req->sram_addr,
						req->data_base,
						req->weight_base,
						req->output_base);
			} else {
				wake_up_all(&tn->idle_wq);
			}
		}

		spin_unlock_irqrestore(q_lock, flags);
	}

	return IRQ_RETVAL(pending != 0);
}

int trip_sg_handle_dma(struct platform_device *pdev,
				struct trip_ioc_xfer *xfer,
				struct sg_table *sgt,
				int nr_dma,
				int dma_to_sram)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct scatterlist *sg;
	u64 dma_desc[8];
	unsigned int i, ret = 0;
	u32 sram_addr = xfer->sram_addr;

	/* Ideally IOMMU lets us do the whole transfer in one pass. */
	dev_dbg(&pdev->dev, "spare_net: %d  addr: 0x%08x\n", xfer->spare_net,
		xfer->spare_sram_addr);
	for_each_sg(sgt->sgl, sg, nr_dma, i) {
		struct trip_req *req;

		if (dma_to_sram) {
			/*
			 * Looks wrong, but 'read input' descriptor is named
			 * from TRIP device's perspective
			 */
			trip_desc_dma_read_input(&dma_desc[0],
						 0, sram_addr,
						 sg_dma_len(sg));
		} else {
			trip_desc_dma_write_output(&dma_desc[0],
						   sram_addr, 0,
						   sg_dma_len(sg));
		}
		trip_desc_stop(&dma_desc[4]);

		dev_dbg(&pdev->dev, "sram: 0x%08x dram addr: 0x%llx  len: 0x%08x\n",
			sram_addr, (uint64_t)(sg_dma_address(sg)),
			sg_dma_len(sg));
		trip_copy_to_sram(pdev, dma_desc, sizeof(dma_desc),
				  xfer->spare_sram_addr);

		pm_runtime_get_sync(&td->pdev->dev);	/* prevent idle */
		req = trip_net_submit(td, xfer->spare_net,
					xfer->spare_sram_addr,
					(dma_to_sram) ? sg_dma_address(sg) : 0,
					0,
					(dma_to_sram) ? 0 : sg_dma_address(sg));
		if (IS_ERR(req)) {
			pm_runtime_put(&td->pdev->dev);
			return PTR_ERR(req);
		}

		wait_for_completion(&req->completion);
		pm_runtime_put(&td->pdev->dev);
		ret = trip_check_status(req->nw_status, &xfer->status);
		kfree(req);
		if (ret != 0)
			break;

		sram_addr += sg_dma_len(sg);
	}

	return ret;
}

static void trip_reg_setup(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	/* APHW-996: AXI protocol check is not enabled by default. */
	trip_writel(td, TRIP_R_AXI_CFG_PROTO_CHK_EN_BIT,
		TRIP_R_AXI_CFG);
}

int trip_wdt_setup(struct platform_device *pdev, uint32_t network)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	if ((network & TRIP_R_WDT_NETWORK_CTL_MAX_DELAY_MASK) != network) {
		dev_err(&pdev->dev, "invalid wdt value\n");
		return -EINVAL;
	}

	/* Progress and layer watchdogs not needed for normal operation. */
	trip_writel(td, 0, TRIP_R_WDT_PROGRESS_CTL);
	trip_writel(td, 0, TRIP_R_WDT_LAYER_CTL);

	trip_writel(td, network | TRIP_R_WDT_NETWORK_CTL_EN_BIT,
		TRIP_R_WDT_NETWORK_CTL);

	return 0;
}

static const unsigned int trip_ro_ranges[] = {
	/* 0    1    2    3    4  */
	113, 117, 127, 138, 148, 152,
};
#define MAX_ASV_GROUP (ARRAY_SIZE(trip_ro_ranges) - 1)

static int get_asv_group(void)
{
	int asv_val;
	int asv_group = 0;
	int i;

	asv_val = mail_box_read_asv_trip();

	for (i = 0; i < MAX_ASV_GROUP; i++) {
		if ((asv_val >= trip_ro_ranges[i]) &&
				(asv_val < trip_ro_ranges[i + 1])) {
			asv_group = i;
			break;
		}
}

	pr_info("trip asv: val: %d, group: %d\n", asv_val, asv_group);

	return asv_group;
}

int trip_set_clk_rate(struct platform_device *pdev,
				unsigned long rate)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	int ret, change_ret;

	/* If already at target rate, skip setting. */
	if (td->clk_rate == rate)
		return 0;

	dev_info(&pdev->dev, "changing clk rate from %lu to %lu.\n",
			td->clk_rate, rate);

	/* Switch back to non-idle frequency (if idle.) */
	pm_runtime_get_sync(&pdev->dev);

	/* Switch mux downstream of PLL away from PLL so we can change rate */
	ret = clk_set_parent(td->trip_switch_pll_mux_clk,
				td->trip_switch_mux_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to switch mux from PLL\n");
		goto out_resume;
	}

	change_ret = dev_pm_opp_set_rate(&pdev->dev, rate);
	if (change_ret)
		dev_err(&pdev->dev, "failed to set PLL rate\n");

	/* And switch the mux downstream of PLL back to PLL. */
	clk_set_parent(td->trip_switch_pll_mux_clk, td->trip_pll_clk);
	if (ret)
		dev_err(&pdev->dev, "failed to switch mux to PLL\n");

	td->clk_rate = clk_get_rate(td->trip_pll);

	if (change_ret)
		ret = change_ret;

out_resume:
	pm_runtime_put(&pdev->dev);
	return ret;
}

unsigned long trip_get_clk_rate(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	return clk_get_rate(td->trip_pll);
}

static int trip_devfreq_target(struct device *dev, unsigned long *freq,
				u32 flags)
{
	struct tripdev *td = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long rate;
	int ret;

	pr_debug("%s: freq %lu flags %x\n", __func__,
			*freq, flags);

	/* Switch back to non-idle frequency (if idle.) */
	pm_runtime_get_sync(dev);

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		pm_runtime_put(dev);
		return PTR_ERR(opp);
	}
	rate = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);
	pm_runtime_put(dev);

	ret = trip_set_clk_rate(td->pdev, rate);

	return ret;
}

static int trip_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct tripdev *td = dev_get_drvdata(dev);

	*freq = td->clk_rate;
	return 0;
}

int trip_clk_init(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	char asv_name[sizeof("asv0")];
	const char *regulator_name = "trip";
	unsigned long rate;
	int ret;

	td->trip_pll = of_clk_get_by_name(pdev->dev.of_node,
						"fout_pll_trip");
	if (IS_ERR(td->trip_pll)) {
		dev_err(&pdev->dev, "failed to get pll\n");
		return PTR_ERR(td->trip_pll);
	}

	td->trip_pll_clk = of_clk_get_by_name(pdev->dev.of_node,
						"mout_trip_pll");
	if (IS_ERR(td->trip_pll_clk)) {
		dev_err(&pdev->dev, "failed to get pll clk\n");
		return PTR_ERR(td->trip_pll_clk);
	}

	td->trip_switch_mux_clk = of_clk_get_by_name(pdev->dev.of_node,
						"mout_trip_switch_mux");
	if (IS_ERR(td->trip_switch_mux_clk)) {
		dev_err(&pdev->dev, "failed to get switch mux\n");
		return PTR_ERR(td->trip_switch_mux_clk);
	}

	td->trip_switch_pll_mux_clk = of_clk_get_by_name(pdev->dev.of_node,
						"mout_trip_switch_pll_mux");
	if (IS_ERR(td->trip_switch_pll_mux_clk)) {
		dev_err(&pdev->dev, "failed to get switch pll mux\n");
		return PTR_ERR(td->trip_switch_pll_mux_clk);
	}

	td->opp_table = dev_pm_opp_set_regulators(&pdev->dev, &regulator_name,
						  1);
	if (IS_ERR(td->opp_table)) {
		dev_err(&pdev->dev, "failed to set regulators\n");
		return PTR_ERR(td->opp_table);
	}

	snprintf(asv_name, sizeof(asv_name), "asv%d", get_asv_group());
	td->opp_table = dev_pm_opp_set_prop_name(&pdev->dev, asv_name);
	if (IS_ERR(td->opp_table)) {
		dev_err(&pdev->dev, "failed to set asv prop name\n");
		return PTR_ERR(td->opp_table);
	}

	ret = dev_pm_opp_of_add_table(&pdev->dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to add opp table\n");
		return ret;
	}

	/* Set ASV selected voltage for boot frequency, if possible */
	rate = clk_get_rate(td->trip_pll);
	if (rate == 0)
		return 0;

	if (dev_pm_opp_set_rate(&pdev->dev, rate))
		dev_err(&pdev->dev, "failed to set asv voltage\n");

	td->devfreq_profile.initial_freq = rate;
	td->devfreq_profile.target = trip_devfreq_target;
	td->devfreq_profile.get_cur_freq = trip_devfreq_cur_freq;
	td->devfreq = devfreq_add_device(&pdev->dev, &td->devfreq_profile,
						"performance", NULL);
	if (IS_ERR(td->devfreq)) {
		dev_err(&pdev->dev, "failed to add devfreq\n");
		return PTR_ERR(td->devfreq);
	}

	ret = devfreq_register_opp_notifier(&pdev->dev, td->devfreq);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register devfreq opp notifier\n");
		return ret;
	}

	td->devfreq_cooling = of_devfreq_cooling_register(pdev->dev.of_node,
						td->devfreq);
	if (IS_ERR(td->devfreq_cooling)) {
		dev_err(&pdev->dev, "failed to add devfreq cooling\n");
		return PTR_ERR(td->devfreq_cooling);
	}

	return 0;
}

int trip_dma_init(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);
	struct resource *res;
	unsigned int seg_size;
	int i, irq, ret;

	if (trip_release_reset(pdev) != 0)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	td->td_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(td->td_regs))
		return PTR_ERR(td->td_regs);
	dev_dbg(&pdev->dev, "regs start: 0x%lx, len 0x%lx\n",
		(unsigned long) res->start,
		(unsigned long) (res->end - res->start + 1));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	td->td_sram = devm_ioremap_resource(&pdev->dev, res);
	td->sram_len = res->end - res->start + 1;
	if (IS_ERR(td->td_sram))
		return PTR_ERR(td->td_sram);
	dev_dbg(&pdev->dev, "sram start: 0x%lx, len 0x%lx\n",
		(unsigned long) res->start,
		(unsigned long) td->sram_len);

	/* Run trip clock init at non-idle freq */
	pm_runtime_get_sync(&td->pdev->dev);
	ret = trip_clk_init(pdev);
	if (ret != 0)
		return ret;
	pm_runtime_put(&td->pdev->dev);

	td->hw_ver = trip_readl(td, TRIP_R_VERSION);
	dev_info(&pdev->dev, "trip hwver %x\n", td->hw_ver);

	mutex_init(&td->submit_mutex);
	spin_lock_init(&td->serial_lock);
	INIT_LIST_HEAD(&td->started_reqs);
	INIT_LIST_HEAD(&td->waiting_reqs);
	init_waitqueue_head(&td->idle_wq);

	/* Init per-network state */
	for (i = 0; i < TRIP_NET_MAX; i++) {
		td->net[i].idx = i;
		td->net[i].ptd = td;
		spin_lock_init(&td->net[i].net_lock);
		INIT_LIST_HEAD(&td->net[i].pending_reqs);
		init_waitqueue_head(&td->net[i].idle_wq);

		irq = platform_get_irq(pdev, TRIP_IRQ_ENG_COMP_START + i);
		if (irq < 0) {
			dev_err(&pdev->dev, "failed to get irq %d\n",
					TRIP_IRQ_ENG_COMP_START + i);
			return -ENXIO;
		}
		td->net[i].done_irq = irq;

		snprintf(td->net[i].name, sizeof(td->net[i].name),
				"%s.net.%d", dev_name(&pdev->dev), i);

		ret = devm_request_irq(&pdev->dev, td->net[i].done_irq,
				 trip_net_comp_isr, 0x0, td->net[i].name,
				 &td->net[i]);
		if (ret) {
			dev_err(&pdev->dev, "failed to register irq %s\n",
				td->net[i].name);
			return ret;
		}
	}

	irq = platform_get_irq(pdev, TRIP_IRQ_ENG_GENERIC);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n",
				TRIP_IRQ_ENG_GENERIC);
		return -ENXIO;
	}
	td->generic_irq = irq;

	irq = platform_get_irq(pdev, TRIP_IRQ_ENG_ERROR);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n",
				TRIP_IRQ_ENG_ERROR);
		return -ENXIO;
	}
	td->error_irq = irq;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	if (ret)
		dev_err(&pdev->dev, "failed to set dma mask\n");

	seg_size = dma_get_max_seg_size(&pdev->dev);

	if (pdev->dev.dma_parms == NULL)
		pdev->dev.dma_parms = kzalloc(sizeof(*pdev->dev.dma_parms),
						GFP_KERNEL);
	if (pdev->dev.dma_parms == NULL)
		return -ENOMEM;
	dma_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	/* Run init at higher clock frequency */
	pm_runtime_get_sync(&td->pdev->dev);

	/* Some initial setup and (optional) tests */
	(void) trip_reg_setup(pdev);
	(void) trip_sram_test_fast(pdev);
	(void) trip_dma_test(pdev);

	/* Idle until firmware load */
	pm_runtime_put(&td->pdev->dev);

	return 0;
}

bool trip_is_idle_serialized(struct tripdev *td)
{
	bool is_idle;

	spin_lock_irq(&td->serial_lock);
	is_idle = (list_empty(&td->started_reqs) &&
			list_empty(&td->waiting_reqs));
	spin_unlock_irq(&td->serial_lock);

	return is_idle;
}

bool trip_is_idle_net(struct trip_net *tn)
{
	bool is_idle;

	spin_lock_irq(&tn->net_lock);
	is_idle = list_empty(&tn->pending_reqs);
	spin_unlock_irq(&tn->net_lock);

	return is_idle;
}


/* Wait for dma idle */
int trip_dma_suspend(struct platform_device *pdev)
{
	struct tripdev *td = platform_get_drvdata(pdev);

	if (td->serialized) {
		wait_event(td->idle_wq, trip_is_idle_serialized(td));
	} else {
		int i;

		for (i = 0; i < TRIP_NET_MAX; i++) {
			wait_event(td->net[i].idle_wq,
				   trip_is_idle_net(&td->net[i]));
		}
	}

	return 0;
}

/* Reinitialize Trip DMA after resume */
int trip_dma_resume(struct platform_device *pdev)
{
	if (trip_release_reset(pdev) != 0)
		return -ENOMEM;

	/* Reinitialize hardware */
	(void) trip_reg_setup(pdev);
	return trip_sram_test_fast(pdev);
}

