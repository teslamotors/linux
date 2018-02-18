/*
 * ADMA driver for Nvidia's Tegra ADMA controller.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/tegra_pm_domains.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk/tegra.h>
#include <linux/tegra_pm_domains.h>
#include <linux/tegra-soc.h>

#include "dmaengine.h"

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include  "tegra210-adma.h"
#else
#include <sound/tegra_adma.h>
#endif

struct tegra_adma;

/*
 * tegra_adma_chip_data Tegra chip specific ADMA data
 * @channel_reg_size: Channel register size.
 * @max_dma_count: Maximum ADMA transfer count supported by ADMA controller.
 */
struct tegra_adma_chip_data {
	int channel_reg_size;
	int max_dma_count;
};

/* ADMA channel registers */
struct tegra_adma_chan_regs {
	unsigned long	ctrl;
	unsigned long	config;
	unsigned long	src_ptr;
	unsigned long	tgt_ptr;
	unsigned long	ahub_fifo_ctrl;
	unsigned long	tc;
	unsigned long	tx_done;
};

/*
 * tegra_adma_sg_req: Dma request details to configure hardware. This
 * contains the details for one transfer to configure ADMA hw.
 * The client's request for data transfer can be broken into multiple
 * sub-transfer as per requester details and hw support.
 * This sub transfer get added in the list of transfer and point to Tegra
 * ADMA descriptor which manages the transfer details.
 */
struct tegra_adma_sg_req {
	struct tegra_adma_chan_regs	ch_regs;
	int				req_len;
	bool				configured;
	bool				last_sg;
	struct list_head		node;
	struct tegra_adma_desc		*dma_desc;
};

/*
 * tegra_adma_desc: Tegra ADMA descriptors which manages the client requests.
 * This descriptor keep track of transfer status, callbacks and request
 * counts etc.
 */
struct tegra_adma_desc {
	struct dma_async_tx_descriptor	txd;
	uint64_t				bytes_requested;
	uint64_t				bytes_transferred;
	enum dma_status			dma_status;
	struct list_head		node;
	struct list_head		tx_list;
	struct list_head		cb_node;
	int				cb_count;
};

struct tegra_adma_chan;

typedef void (*dma_isr_handler)(struct tegra_adma_chan *tdc,
				bool to_terminate);

/* tegra_adma_chan: Channel specific information */
struct tegra_adma_chan {
	struct dma_chan		dma_chan;
	char			name[30];
	bool			config_init;
	int			id;
	int			irq;
	unsigned long		chan_base_offset;
	spinlock_t		lock;
	bool			busy;
	struct tegra_adma	*tdma;
	bool			cyclic;

	/* Different lists for managing the requests */
	struct list_head	free_sg_req;
	struct list_head	pending_sg_req;
	struct list_head	free_dma_desc;
	struct list_head	cb_desc;

	/* ISR handler and tasklet for bottom half of isr handling */
	dma_isr_handler		isr_handler;
	struct tasklet_struct	tasklet;
	dma_async_tx_callback	callback;
	void			*callback_param;

	/* Channel-slave specific configuration */
	struct dma_slave_config dma_sconfig;
	struct tegra_adma_chan_regs	channel_reg;
	uint64_t total_tx_done;
};

/* tegra_adma: Tegra ADMA specific information */
struct tegra_adma {
	struct dma_device		dma_dev;
	struct device			*dev;
	struct clk			*dma_clk;
	struct clk			*ape_clk;
	struct clk			*apb2ape_clk;
	spinlock_t			global_lock;
	void __iomem			*adma_addr[ADMA_MAX_ADDR];
	void __iomem			*base_addr;
	const struct tegra_adma_chip_data *chip_data;
	/* number of channels available in the controller */
	unsigned int			nr_channels;

	/* Index of the first physical adma channel to be used.
	 * Index counting starts from zero
	 */
	unsigned int			dma_start_index;

	/* If "true", means running in hypervisor */
	bool				is_virt;

	/* global register need to be cache before suspend */
	u32				global_reg;

	/* Last member of the structure */
	struct tegra_adma_chan channels[0];
};

static inline void global_write(struct tegra_adma *tdma, u32 reg, u32 val)
{
	writel(val, tdma->adma_addr[GLOBAL_REG] + reg);
}

static inline void global_ch_write(struct tegra_adma *tdma, u32 reg, u32 val)
{
	writel(val, tdma->base_addr + reg);
}

static inline u32 global_read(struct tegra_adma *tdma, u32 reg)
{
	return readl(tdma->adma_addr[GLOBAL_REG] + reg);
}

static inline void channel_write(struct tegra_adma_chan *tdc,
		u32 reg, u32 val)
{
	writel(val, tdc->tdma->base_addr + tdc->chan_base_offset + reg);
}

static inline u32 channel_read(struct tegra_adma_chan *tdc, u32 reg)
{
	return readl(tdc->tdma->base_addr + tdc->chan_base_offset + reg);
}

static inline void channel_set_field(struct tegra_adma_chan *tdc,
		u32 reg, u32 shift, u32 mask, u32 val)
{
	u32 t;

	t = readl(tdc->tdma->base_addr + tdc->chan_base_offset + reg);
	writel((t & ~((mask) << (shift))) + (((val) &
		(mask)) << (shift)), tdc->tdma->base_addr +
		tdc->chan_base_offset + reg);
}

static inline struct tegra_adma_chan *to_tegra_adma_chan(struct dma_chan *dc)
{
	return container_of(dc, struct tegra_adma_chan, dma_chan);
}

static inline struct tegra_adma_desc *txd_to_tegra_adma_desc(
		struct dma_async_tx_descriptor *td)
{
	return container_of(td, struct tegra_adma_desc, txd);
}

static inline struct device *tdc2dev(struct tegra_adma_chan *tdc)
{
	return &tdc->dma_chan.dev->device;
}

static dma_cookie_t tegra_adma_tx_submit(struct dma_async_tx_descriptor *tx);
static int tegra_adma_runtime_suspend(struct device *dev);
static int tegra_adma_runtime_resume(struct device *dev);

/* Get ADMA desc from free list, if not there then allocate it.  */
static struct tegra_adma_desc *tegra_adma_desc_get(
		struct tegra_adma_chan *tdc)
{
	struct tegra_adma_desc *dma_desc;
	unsigned long flags;

	spin_lock_irqsave(&tdc->lock, flags);

	/* Do not allocate if desc are waiting for ack */
	list_for_each_entry(dma_desc, &tdc->free_dma_desc, node) {
		if (async_tx_test_ack(&dma_desc->txd)) {
			list_del(&dma_desc->node);
			spin_unlock_irqrestore(&tdc->lock, flags);
			dma_desc->txd.flags = 0;
			return dma_desc;
		}
	}

	spin_unlock_irqrestore(&tdc->lock, flags);

	/* Allocate ADMA desc */
	dma_desc = kzalloc(sizeof(*dma_desc), GFP_ATOMIC);
	if (!dma_desc) {
		dev_err(tdc2dev(tdc), "dma_desc alloc failed\n");
		return NULL;
	}

	dma_async_tx_descriptor_init(&dma_desc->txd, &tdc->dma_chan);
	dma_desc->txd.tx_submit = tegra_adma_tx_submit;
	dma_desc->txd.flags = 0;
	return dma_desc;
}

static void tegra_adma_desc_put(struct tegra_adma_chan *tdc,
		struct tegra_adma_desc *dma_desc)
{
	unsigned long flags;

	spin_lock_irqsave(&tdc->lock, flags);
	if (!list_empty(&dma_desc->tx_list))
		list_splice_init(&dma_desc->tx_list, &tdc->free_sg_req);
	list_add_tail(&dma_desc->node, &tdc->free_dma_desc);
	spin_unlock_irqrestore(&tdc->lock, flags);
}

static struct tegra_adma_sg_req *tegra_adma_sg_req_get(
		struct tegra_adma_chan *tdc)
{
	struct tegra_adma_sg_req *sg_req = NULL;
	unsigned long flags;

	spin_lock_irqsave(&tdc->lock, flags);
	if (!list_empty(&tdc->free_sg_req)) {
		sg_req = list_first_entry(&tdc->free_sg_req,
					typeof(*sg_req), node);
		list_del(&sg_req->node);
		spin_unlock_irqrestore(&tdc->lock, flags);
		return sg_req;
	}
	spin_unlock_irqrestore(&tdc->lock, flags);

	sg_req = kzalloc(sizeof(struct tegra_adma_sg_req), GFP_ATOMIC);
	if (!sg_req)
		dev_err(tdc2dev(tdc), "sg_req alloc failed\n");
	return sg_req;
}

static int tegra_adma_slave_config(struct dma_chan *dc,
		struct dma_slave_config *sconfig)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);

	if (!list_empty(&tdc->pending_sg_req)) {
		dev_err(tdc2dev(tdc), "Configuration not allowed\n");
		return -EBUSY;
	}

	memcpy(&tdc->dma_sconfig, sconfig, sizeof(*sconfig));
	tdc->config_init = true;
	return 0;
}

static int tegra_adma_is_paused(struct tegra_adma_chan *tdc)
{
	u32 csts;

	csts = channel_read(tdc, ADMA_CH_STATUS);
	csts &= ADMA_CH_STATUS_TRANSFER_PAUSED;
	return csts;
}

static void tegra_adma_pause(struct tegra_adma_chan *tdc,
	bool wait_for_burst_complete)
{
	u32 dcnt = 10;

	channel_set_field(tdc, ADMA_CH_CTRL, ADMA_CH_CTRL_TRANSFER_PAUSE_SHIFT,
			ADMA_CH_CTRL_TRANSFER_PAUSE_MASK, 1);

	while (tegra_adma_is_paused(tdc) &&
	       dcnt--)
		udelay(TEGRA_ADMA_BURST_COMPLETE_TIME);
}

static void tegra_adma_resume(struct tegra_adma_chan *tdc)
{
	channel_set_field(tdc, ADMA_CH_CTRL, ADMA_CH_CTRL_TRANSFER_PAUSE_SHIFT,
			ADMA_CH_CTRL_TRANSFER_PAUSE_MASK, 0);
}

static int tegra_adma_is_enabled(struct tegra_adma_chan *tdc)
{
	u32 csts;

	csts = channel_read(tdc, ADMA_CH_STATUS);
	csts &= ADMA_CH_STATUS_TRANSFER_ENABLED;
	return csts;
}

static void tegra_adma_stop(struct tegra_adma_chan *tdc)
{
	u32 status, dcnt = 10;

	/* Disable interrupts  */
	channel_write(tdc, ADMA_CH_INT_CLEAR, 1);

	/* Disable ADMA */
	channel_write(tdc, ADMA_CH_CMD, 0);

	/* Clear interrupt status if it is there */
	status = channel_read(tdc, ADMA_CH_INT_STATUS);
	if (status & ADMA_CH_INT_TD_STATUS) {
		dev_dbg(tdc2dev(tdc), "%s():clearing interrupt\n", __func__);
		channel_write(tdc, ADMA_CH_INT_CLEAR, 1);
	}

	while (tegra_adma_is_enabled(tdc) &&
	       dcnt--)
		udelay(TEGRA_ADMA_BURST_COMPLETE_TIME);

	if (tegra_adma_is_enabled(tdc))
		dev_warn(tdc2dev(tdc), "%s(): stop failed for channel %d\n",
			__func__, tdc->id);

	tdc->busy = false;
}

static void tegra_adma_start(struct tegra_adma_chan *tdc,
		struct tegra_adma_sg_req *sg_req)
{
	struct tegra_adma_chan_regs *ch_regs = &sg_req->ch_regs;

	/* Update transfer done count for position calculation */
	tdc->total_tx_done = 0;
	tdc->channel_reg.tx_done = 0;
	tdc->channel_reg.tc = ch_regs->tc;
	channel_write(tdc, ADMA_CH_TC, ch_regs->tc);
	channel_write(tdc, ADMA_CH_CTRL, ch_regs->ctrl);
	channel_write(tdc, ADMA_CH_LOWER_SOURCE_ADDR, ch_regs->src_ptr);
	channel_write(tdc, ADMA_CH_LOWER_TARGET_ADDR, ch_regs->tgt_ptr);
	channel_write(tdc, ADMA_CH_AHUB_FIFO_CTRL, ch_regs->ahub_fifo_ctrl);
	channel_write(tdc, ADMA_CH_CONFIG, ch_regs->config);
	/* Start ADMA */
	channel_write(tdc, ADMA_CH_CMD, 1);
}

static void tegra_adma_configure_for_next(struct tegra_adma_chan *tdc,
		struct tegra_adma_sg_req *nsg_req)
{
	unsigned long status;

	if (tdc->cyclic) {
		nsg_req->configured = true;
		return;
	}

	/*
	 * The ADMA controller reloads the new configuration for next transfer
	 * after last burst of current transfer completes.
	 * If there is no interrupt transfer done status then this makes sure
	 * that last burst has not be completed. There may be case that last
	 * burst is on flight and so it can complete but because ADMA is paused,
	 * it will not generates interrupt as well as not reload the new
	 * configuration.
	 * If there is already interrupt transfer done status then interrupt
	 * handler need to load new configuration.
	 */
	tegra_adma_pause(tdc, false);
	status  = channel_read(tdc, ADMA_CH_INT_STATUS);

	/*
	 * If interrupt is pending then do nothing as the ISR will handle
	 * the programing for new request.
	 */
	if (status & ADMA_CH_INT_TD_STATUS) {
		dev_err(tdc2dev(tdc),
			"Skipping new configuration as interrupt is pending\n");
		tegra_adma_resume(tdc);
		return;
	}

	/* Safe to program new configuration */
	channel_write(tdc, ADMA_CH_LOWER_SOURCE_ADDR, nsg_req->ch_regs.src_ptr);
	channel_write(tdc, ADMA_CH_LOWER_TARGET_ADDR, nsg_req->ch_regs.tgt_ptr);
	channel_write(tdc, ADMA_CH_TC, nsg_req->ch_regs.tc);
	channel_write(tdc, ADMA_CH_CTRL, nsg_req->ch_regs.ctrl);
	channel_write(tdc, ADMA_CH_AHUB_FIFO_CTRL,
			nsg_req->ch_regs.ahub_fifo_ctrl);
	channel_write(tdc, ADMA_CH_CONFIG, nsg_req->ch_regs.config);

	channel_write(tdc, ADMA_CH_CMD, 1);
	nsg_req->configured = true;

	tegra_adma_resume(tdc);

}

static void tdc_start_head_req(struct tegra_adma_chan *tdc)
{
	struct tegra_adma_sg_req *sg_req;

	if (list_empty(&tdc->pending_sg_req))
		return;

	sg_req = list_first_entry(&tdc->pending_sg_req,
					typeof(*sg_req), node);
	tegra_adma_start(tdc, sg_req);
	sg_req->configured = true;
	tdc->busy = true;
}

static void tdc_configure_next_head_desc(struct tegra_adma_chan *tdc)
{
	struct tegra_adma_sg_req *hsgreq;
	struct tegra_adma_sg_req *hnsgreq;

	if (list_empty(&tdc->pending_sg_req))
		return;

	hsgreq = list_first_entry(&tdc->pending_sg_req, typeof(*hsgreq), node);
	if (!list_is_last(&hsgreq->node, &tdc->pending_sg_req)) {
		hnsgreq = list_first_entry(&hsgreq->node,
					typeof(*hnsgreq), node);
		tegra_adma_configure_for_next(tdc, hnsgreq);
	}
}

static inline int get_current_xferred_count(struct tegra_adma_chan *tdc,
	struct tegra_adma_sg_req *sg_req, unsigned long status)
{
	return sg_req->req_len - (status & TEGRA_ADMA_STATUS_COUNT_MASK);
}

static void tegra_adma_abort_all(struct tegra_adma_chan *tdc)
{
	struct tegra_adma_sg_req *sgreq;
	struct tegra_adma_desc *dma_desc;

	while (!list_empty(&tdc->pending_sg_req)) {
		sgreq = list_first_entry(&tdc->pending_sg_req,
						typeof(*sgreq), node);
		list_move_tail(&sgreq->node, &tdc->free_sg_req);
		if (sgreq->last_sg) {
			dma_desc = sgreq->dma_desc;
			dma_desc->dma_status = DMA_ERROR;
			list_add_tail(&dma_desc->node, &tdc->free_dma_desc);

			/* Add in cb list if it is not there. */
			if (!dma_desc->cb_count)
				list_add_tail(&dma_desc->cb_node,
							&tdc->cb_desc);
			dma_desc->cb_count++;
		}
	}
	tdc->isr_handler = NULL;
}

/* Returns bytes transferred with period size granularity */
static inline uint64_t tegra_adma_get_position(struct tegra_adma_chan *tdc)
{
	unsigned long cur_tc = 0;
	uint64_t tx_done_max = (ADMA_CH_TRANSFER_DONE_COUNT_MASK >>
		ADMA_CH_TRANSFER_DONE_COUNT_SHIFT) + 1;
	uint64_t tx_done = channel_read(tdc, ADMA_CH_TRANSFER_STATUS) &
		ADMA_CH_TRANSFER_DONE_COUNT_MASK;
	tx_done = tx_done >> ADMA_CH_TRANSFER_DONE_COUNT_SHIFT;

	/* Handle wrap around case */
	if (tx_done < tdc->channel_reg.tx_done)
		tdc->total_tx_done += tx_done +
			(tx_done_max - tdc->channel_reg.tx_done);
	else
		tdc->total_tx_done += (tx_done - tdc->channel_reg.tx_done);
	tdc->channel_reg.tx_done = tx_done;

	/* read TC_STATUS register to get current transfer status */
	cur_tc = channel_read(tdc, ADMA_CH_TC_STATUS);
	/* get transferred data count */
	cur_tc = tdc->channel_reg.tc - cur_tc;

	return (tdc->total_tx_done * tdc->channel_reg.tc) + cur_tc;
}

static bool handle_continuous_head_request(struct tegra_adma_chan *tdc,
		struct tegra_adma_sg_req *last_sg_req, bool to_terminate)
{
	struct tegra_adma_sg_req *hsgreq = NULL;

	if (list_empty(&tdc->pending_sg_req)) {
		dev_err(tdc2dev(tdc), "Dma is running without req\n");
		tegra_adma_stop(tdc);
		return false;
	}

	/*
	 * Check that head req on list should be in flight.
	 * If it is not in flight then abort transfer as
	 * looping of transfer can not continue.
	 */
	hsgreq = list_first_entry(&tdc->pending_sg_req, typeof(*hsgreq), node);
	if (!hsgreq->configured) {
		tegra_adma_stop(tdc);
		dev_err(tdc2dev(tdc), "Error in dma transfer, aborting dma\n");
		tegra_adma_abort_all(tdc);
		return false;
	}

	/* Configure next request */
	if (!to_terminate)
		tdc_configure_next_head_desc(tdc);
	return true;
}

static void handle_once_dma_done(struct tegra_adma_chan *tdc,
	bool to_terminate)
{
	struct tegra_adma_sg_req *sgreq;
	struct tegra_adma_desc *dma_desc;

	tdc->busy = false;
	sgreq = list_first_entry(&tdc->pending_sg_req, typeof(*sgreq), node);
	dma_desc = sgreq->dma_desc;
	dma_desc->bytes_transferred = tegra_adma_get_position(tdc);

	list_del(&sgreq->node);
	if (sgreq->last_sg) {
		dma_desc->dma_status = DMA_COMPLETE;
		dma_cookie_complete(&dma_desc->txd);
		if (!dma_desc->cb_count)
			list_add_tail(&dma_desc->cb_node, &tdc->cb_desc);
		dma_desc->cb_count++;
		list_add_tail(&dma_desc->node, &tdc->free_dma_desc);
	}
	list_add_tail(&sgreq->node, &tdc->free_sg_req);

	if (to_terminate || list_empty(&tdc->pending_sg_req))
		return;

	tdc_start_head_req(tdc);
	return;
}

static void handle_cont_sngl_cycle_dma_done(struct tegra_adma_chan *tdc,
		bool to_terminate)
{
	struct tegra_adma_sg_req *sgreq;
	struct tegra_adma_desc *dma_desc;
	bool st;

	sgreq = list_first_entry(&tdc->pending_sg_req, typeof(*sgreq), node);
	dma_desc = sgreq->dma_desc;
	dma_desc->bytes_transferred = tegra_adma_get_position(tdc);

	/* Callback need to be call */
	if (!dma_desc->cb_count)
		list_add_tail(&dma_desc->cb_node, &tdc->cb_desc);
	dma_desc->cb_count++;

	/* If not last req then put at end of pending list */
	if (!list_is_last(&sgreq->node, &tdc->pending_sg_req)) {
		list_move_tail(&sgreq->node, &tdc->pending_sg_req);
		sgreq->configured = false;
		st = handle_continuous_head_request(tdc, sgreq, to_terminate);
		if (!st)
			dma_desc->dma_status = DMA_ERROR;
	}
	return;
}

static void tegra_adma_tasklet(unsigned long data)
{
	struct tegra_adma_chan *tdc = (struct tegra_adma_chan *)data;
	dma_async_tx_callback callback = NULL;
	void *callback_param = NULL;
	struct tegra_adma_desc *dma_desc;
	unsigned long flags;
	int cb_count;

	spin_lock_irqsave(&tdc->lock, flags);
	while (!list_empty(&tdc->cb_desc)) {
		dma_desc  = list_first_entry(&tdc->cb_desc,
					typeof(*dma_desc), cb_node);
		list_del(&dma_desc->cb_node);
		callback = dma_desc->txd.callback;
		callback_param = dma_desc->txd.callback_param;
		cb_count = dma_desc->cb_count;
		dma_desc->cb_count = 0;
		spin_unlock_irqrestore(&tdc->lock, flags);
		while (cb_count-- && callback)
			callback(callback_param);
		spin_lock_irqsave(&tdc->lock, flags);
	}
	spin_unlock_irqrestore(&tdc->lock, flags);
}

static irqreturn_t tegra_adma_isr(int irq, void *dev_id)
{
	struct tegra_adma_chan *tdc = dev_id;
	unsigned long status;
	unsigned long flags;

	spin_lock_irqsave(&tdc->lock, flags);

	status = channel_read(tdc, ADMA_CH_INT_STATUS);
	if (status & ADMA_CH_INT_TD_STATUS) {
		channel_write(tdc, ADMA_CH_INT_CLEAR, 1);
		if (tdc->isr_handler)
			tdc->isr_handler(tdc, false);
		tasklet_schedule(&tdc->tasklet);
		spin_unlock_irqrestore(&tdc->lock, flags);
		return IRQ_HANDLED;
	}

	spin_unlock_irqrestore(&tdc->lock, flags);
	dev_dbg(tdc2dev(tdc),
		"Interrupt already served status 0x%08lx\n", status);
	return IRQ_NONE;
}

static dma_cookie_t tegra_adma_tx_submit(struct dma_async_tx_descriptor *txd)
{
	struct tegra_adma_desc *dma_desc = txd_to_tegra_adma_desc(txd);
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(txd->chan);
	unsigned long flags;
	dma_cookie_t cookie;

	spin_lock_irqsave(&tdc->lock, flags);
	dma_desc->dma_status = DMA_IN_PROGRESS;
	cookie = dma_cookie_assign(&dma_desc->txd);
	list_splice_tail_init(&dma_desc->tx_list, &tdc->pending_sg_req);
	spin_unlock_irqrestore(&tdc->lock, flags);
	return cookie;
}

static void tegra_adma_issue_pending(struct dma_chan *dc)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	unsigned long flags;

	spin_lock_irqsave(&tdc->lock, flags);
	if (list_empty(&tdc->pending_sg_req)) {
		dev_err(tdc2dev(tdc), "No ADMA request\n");
		goto end;
	}

	if (!tdc->busy) {
		tdc_start_head_req(tdc);

		/* Continuous single mode: Configure next req */
		if (tdc->cyclic) {
			/*
			 * Wait for 1 burst time for configure ADMA for
			 * next transfer.
			 */
			udelay(TEGRA_ADMA_BURST_COMPLETE_TIME);
			tdc_configure_next_head_desc(tdc);
		}
	}
end:
	spin_unlock_irqrestore(&tdc->lock, flags);
	return;
}

static void tegra_adma_terminate_all(struct dma_chan *dc)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	struct tegra_adma_sg_req *sgreq;
	struct tegra_adma_desc *dma_desc;
	unsigned long flags;
	unsigned long status;
	unsigned long tc = 0;
	bool was_busy;

	spin_lock_irqsave(&tdc->lock, flags);
	if (list_empty(&tdc->pending_sg_req)) {
		spin_unlock_irqrestore(&tdc->lock, flags);
		return;
	}

	if (!tdc->busy)
		goto skip_dma_stop;

	/* Pause ADMA before checking the queue status */
	tegra_adma_pause(tdc, true);

	status = channel_read(tdc, ADMA_CH_INT_STATUS);
	tc = channel_read(tdc, ADMA_CH_TC_STATUS);
	if (status & ADMA_CH_INT_TD_STATUS) {
		dev_dbg(tdc2dev(tdc), "%s():handling isr\n", __func__);
		tdc->isr_handler(tdc, true);
		status = channel_read(tdc, ADMA_CH_INT_STATUS);
		tc = channel_read(tdc, ADMA_CH_TC_STATUS);
	}

	was_busy = tdc->busy;
	tegra_adma_stop(tdc);

	if (!list_empty(&tdc->pending_sg_req) && was_busy) {
		sgreq = list_first_entry(&tdc->pending_sg_req,
					typeof(*sgreq), node);
		tc = status;
		sgreq->dma_desc->bytes_transferred +=
				get_current_xferred_count(tdc, sgreq, tc);
	}
	tegra_adma_resume(tdc);
skip_dma_stop:
	tegra_adma_abort_all(tdc);

	while (!list_empty(&tdc->cb_desc)) {
		dma_desc  = list_first_entry(&tdc->cb_desc,
					typeof(*dma_desc), cb_node);
		list_del(&dma_desc->cb_node);
		dma_desc->cb_count = 0;
	}
	spin_unlock_irqrestore(&tdc->lock, flags);
}

static enum dma_status tegra_adma_tx_status(struct dma_chan *dc,
	dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	struct tegra_adma_desc *dma_desc;
	struct tegra_adma_sg_req *sg_req;
	enum dma_status ret;
	unsigned long flags;
	unsigned int residual;

	spin_lock_irqsave(&tdc->lock, flags);

	ret = dma_cookie_status(dc, cookie, txstate);
	if (ret == DMA_COMPLETE) {
		spin_unlock_irqrestore(&tdc->lock, flags);
		return ret;
	}

	/* Check on wait_ack desc status */
	list_for_each_entry(dma_desc, &tdc->free_dma_desc, node) {
		if (dma_desc->txd.cookie == cookie) {
			residual =  dma_desc->bytes_requested -
					(tegra_adma_get_position(tdc) %
						dma_desc->bytes_requested);
			dma_set_residue(txstate, residual);
			ret = dma_desc->dma_status;
			spin_unlock_irqrestore(&tdc->lock, flags);
			return ret;
		}
	}

	/* Check in pending list */
	list_for_each_entry(sg_req, &tdc->pending_sg_req, node) {
		dma_desc = sg_req->dma_desc;
		if (dma_desc->txd.cookie == cookie) {
			residual =  dma_desc->bytes_requested -
					(tegra_adma_get_position(tdc) %
						dma_desc->bytes_requested);
			dma_set_residue(txstate, residual);
			ret = dma_desc->dma_status;
			spin_unlock_irqrestore(&tdc->lock, flags);
			return ret;
		}
	}

	dev_dbg(tdc2dev(tdc), "cookie %d does not found\n", cookie);
	spin_unlock_irqrestore(&tdc->lock, flags);
	return ret;
}

static int tegra_adma_device_control(struct dma_chan *dc, enum dma_ctrl_cmd cmd,
			unsigned long arg)
{
	switch (cmd) {
	case DMA_SLAVE_CONFIG:
		return tegra_adma_slave_config(dc,
				(struct dma_slave_config *)arg);

	case DMA_TERMINATE_ALL:
		tegra_adma_terminate_all(dc);
		return 0;

	default:
		break;
	}

	return -ENXIO;
}


static int get_transfer_param(struct tegra_adma_chan *tdc,
	enum dma_transfer_direction direction, unsigned long *ahub_fifo_ctrl,
	unsigned long *ctrl, unsigned long *config)
{
	u32 burst_size;
	*ctrl = channel_read(tdc, ADMA_CH_CTRL);
	*ahub_fifo_ctrl = channel_read(tdc, ADMA_CH_AHUB_FIFO_CTRL);
	*config = channel_read(tdc, ADMA_CH_CONFIG);

	*ctrl  &= ~ADMA_CH_CTRL_TRANSFER_DIRECTION_MASK;
	*config &= ~ADMA_CH_CONFIG_BURST_SIZE_MASK;
	*ahub_fifo_ctrl |= BURST_BASED <<
			ADMA_CH_AHUB_FIFO_CTRL_FETCHING_POLICY_SHIFT;
	switch (direction) {
	case DMA_MEM_TO_DEV:
		*ahub_fifo_ctrl &= ~ADMA_CH_AHUB_FIFO_CTRL_TX_FIFO_SIZE_MASK;
		burst_size = fls(tdc->dma_sconfig.dst_maxburst);
		if (!burst_size || burst_size > WORDS_16)
			burst_size = WORDS_16;
		*config |= burst_size << ADMA_CH_CONFIG_BURST_SIZE_SHIFT;
		*ctrl |= MEMORY_TO_AHUB <<
				ADMA_CH_CTRL_TRANSFER_DIRECTION_SHIFT;
		*ctrl &= ~ADMA_CH_CTRL_TX_REQUEST_SELECT_MASK;
		*ctrl |= tdc->dma_sconfig.slave_id <<
				ADMA_CH_CTRL_TX_REQUEST_SELECT_SHIFT;
		*ahub_fifo_ctrl |= 3 <<
				ADMA_CH_AHUB_FIFO_CTRL_TX_FIFO_SIZE_SHIFT;
		return 0;
	case DMA_DEV_TO_MEM:
		*ahub_fifo_ctrl &= ~ADMA_CH_AHUB_FIFO_CTRL_RX_FIFO_SIZE_MASK;
		burst_size = fls(tdc->dma_sconfig.src_maxburst);
		if (!burst_size || burst_size > WORDS_16)
			burst_size = WORDS_16;
		*config |= burst_size << ADMA_CH_CONFIG_BURST_SIZE_SHIFT;
		*ctrl |= AHUB_TO_MEMORY <<
				ADMA_CH_CTRL_TRANSFER_DIRECTION_SHIFT;
		*ctrl &= ~ADMA_CH_CTRL_RX_REQUEST_SELECT_MASK;
		*ctrl |= tdc->dma_sconfig.slave_id  <<
				ADMA_CH_CTRL_RX_REQUEST_SELECT_SHIFT;
		*ahub_fifo_ctrl |= 3 <<
				ADMA_CH_AHUB_FIFO_CTRL_RX_FIFO_SIZE_SHIFT;
		return 0;
	default:
		dev_err(tdc2dev(tdc), "Dma direction is not supported\n");
		return -EINVAL;
	}
	return -EINVAL;
}

static struct dma_async_tx_descriptor *tegra_adma_prep_slave_sg(
	struct dma_chan *dc, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	struct tegra_adma_desc *dma_desc;
	unsigned int	    i;
	struct scatterlist      *sg;
	unsigned long ctrl, config, ahub_fifo_ctrl;
	struct list_head req_list;
	struct tegra_adma_sg_req  *sg_req = NULL;
	int ret;

	if (!tdc->config_init) {
		dev_err(tdc2dev(tdc), "dma channel is not configured\n");
		return NULL;
	}
	if (sg_len < 1) {
		dev_err(tdc2dev(tdc), "Invalid segment length %d\n", sg_len);
		return NULL;
	}

	ret = get_transfer_param(tdc, direction, &ahub_fifo_ctrl,
			&ctrl, &config);
	if (ret < 0)
		return NULL;

	INIT_LIST_HEAD(&req_list);

	ctrl &= ~ADMA_CH_CTRL_TRANSFER_MODE_MASK;
	ctrl |= ADMA_MODE_ONESHOT << ADMA_CH_CTRL_TRANSFER_MODE_SHIFT;

	dma_desc = tegra_adma_desc_get(tdc);
	if (!dma_desc) {
		dev_err(tdc2dev(tdc), "Dma descriptors not available\n");
		return NULL;
	}
	INIT_LIST_HEAD(&dma_desc->tx_list);
	INIT_LIST_HEAD(&dma_desc->cb_node);
	dma_desc->cb_count = 0;
	dma_desc->bytes_requested = 0;
	dma_desc->bytes_transferred = 0;
	dma_desc->dma_status = DMA_IN_PROGRESS;

	/* Make transfer requests */
	for_each_sg(sgl, sg, sg_len, i) {
		u32 len, mem;

		mem = sg_dma_address(sg);
		len = sg_dma_len(sg);

		if ((len & 3) || (mem & 3) ||
				(len > tdc->tdma->chip_data->max_dma_count)) {
			dev_err(tdc2dev(tdc),
				"Dma length/memory address is not supported\n");
			tegra_adma_desc_put(tdc, dma_desc);
			return NULL;
		}

		sg_req = tegra_adma_sg_req_get(tdc);
		if (!sg_req) {
			dev_err(tdc2dev(tdc), "Dma sg-req not available\n");
			tegra_adma_desc_put(tdc, dma_desc);
			return NULL;
		}

		dma_desc->bytes_requested += len;

		if (direction == DMA_MEM_TO_DEV)
			sg_req->ch_regs.src_ptr = mem;
		else
			sg_req->ch_regs.tgt_ptr = mem;

		sg_req->ch_regs.tc = len & 0x3FFFFFFC;
		sg_req->ch_regs.ctrl = ctrl;

		sg_req->ch_regs.ahub_fifo_ctrl = ahub_fifo_ctrl;
		sg_req->ch_regs.config = config;
		sg_req->configured = false;
		sg_req->last_sg = false;
		sg_req->dma_desc = dma_desc;
		sg_req->req_len = len;

		list_add_tail(&sg_req->node, &dma_desc->tx_list);
	}
	sg_req->last_sg = true;
	if (flags & DMA_CTRL_ACK)
		dma_desc->txd.flags = DMA_CTRL_ACK;

	/*
	 * Make sure that mode should not be conflicting with currently
	 * configured mode.
	 */
	if (!tdc->isr_handler) {
		tdc->isr_handler = handle_once_dma_done;
		tdc->cyclic = false;
	} else {
		if (tdc->cyclic) {
			dev_err(tdc2dev(tdc),
				"ADMA configured in cyclic mode\n");
			tegra_adma_desc_put(tdc, dma_desc);
			return NULL;
		}
	}

	return &dma_desc->txd;
}

static struct dma_async_tx_descriptor *tegra_adma_prep_dma_cyclic(
	struct dma_chan *dc, dma_addr_t buf_addr, size_t buf_len,
	size_t period_len, enum dma_transfer_direction direction,
	unsigned long flags)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	struct tegra_adma_desc *dma_desc = NULL;
	struct tegra_adma_sg_req  *sg_req = NULL;
	unsigned long ctrl, config, ahub_fifo_ctrl, config_mem_buffs;
	int len;
	size_t remain_len;
	dma_addr_t mem = buf_addr;
	int ret;

	if (!buf_len || !period_len) {
		dev_err(tdc2dev(tdc), "Invalid buffer/period len\n");
		return NULL;
	}

	if (!tdc->config_init) {
		dev_err(tdc2dev(tdc), "ADMA slave is not configured\n");
		return NULL;
	}

	/*
	 * We allow to take more number of requests till ADMA is
	 * not started. The driver will loop over all requests.
	 * Once ADMA is started then new requests can be queued only after
	 * terminating the ADMA.
	 */
	if (tdc->busy) {
		dev_err(tdc2dev(tdc), "Request not allowed when dma running\n");
		return NULL;
	}

	/*
	 * We only support cycle transfer when buf_len is multiple of
	 * period_len.
	 */
	if (buf_len % period_len) {
		dev_err(tdc2dev(tdc),
			"buf_len is not multiple of period_len\n");
		return NULL;
	}

	len = period_len;
	if ((len & 3) || (buf_addr & 3) ||
			(len > tdc->tdma->chip_data->max_dma_count)) {
		dev_err(tdc2dev(tdc), "Req len/mem address is not correct\n");
		return NULL;
	}

	ret = get_transfer_param(tdc, direction, &ahub_fifo_ctrl,
				&ctrl, &config);
	if (ret < 0)
		return NULL;


	ctrl &= ~ADMA_CH_CTRL_TRANSFER_MODE_MASK;
	ctrl |= ADMA_MODE_CONTINUOUS << ADMA_CH_CTRL_TRANSFER_MODE_SHIFT;

	config_mem_buffs = buf_len/period_len;

	if (config_mem_buffs <= ADMA_CH_CONFIG_MAX_MEM_BUFFERS) {
		if (direction == DMA_MEM_TO_DEV) {
			config &= ~(ADMA_CH_CONFIG_SOURCE_MEMORY_BUFFER_MASK);
			config |= ((config_mem_buffs - 1)
				<< ADMA_CH_CONFIG_SOURCE_MEMORY_BUFFER_SHIFT);
		} else {
			config &= ~(ADMA_CH_CONFIG_TARGET_MEMORY_BUFFER_MASK);
			config |= ((config_mem_buffs - 1)
				<< ADMA_CH_CONFIG_TARGET_MEMORY_BUFFER_SHIFT);
		}
	}

	dma_desc = tegra_adma_desc_get(tdc);
	if (!dma_desc) {
		dev_err(tdc2dev(tdc), "not enough descriptors available\n");
		return NULL;
	}

	INIT_LIST_HEAD(&dma_desc->tx_list);
	INIT_LIST_HEAD(&dma_desc->cb_node);
	dma_desc->cb_count = 0;

	dma_desc->bytes_transferred = 0;
	dma_desc->bytes_requested = buf_len;
	remain_len = buf_len;

	/* Split transfer equal to period size */
	while (remain_len) {
		sg_req = tegra_adma_sg_req_get(tdc);
		if (!sg_req) {
			dev_err(tdc2dev(tdc), "Dma sg-req not available\n");
			tegra_adma_desc_put(tdc, dma_desc);
			return NULL;
		}

		if (direction == DMA_MEM_TO_DEV)
			sg_req->ch_regs.src_ptr = mem;
		else
			sg_req->ch_regs.tgt_ptr = mem;

		sg_req->ch_regs.tc = len & 0x3FFFFFFC;
		sg_req->ch_regs.ctrl = ctrl;
		sg_req->ch_regs.config = config;
		sg_req->ch_regs.ahub_fifo_ctrl = ahub_fifo_ctrl;
		sg_req->configured = false;
		sg_req->last_sg = false;
		sg_req->dma_desc = dma_desc;
		sg_req->req_len = len;

		list_add_tail(&sg_req->node, &dma_desc->tx_list);
		remain_len -= len;
		mem += len;
	}
	sg_req->last_sg = true;
	if (flags & DMA_CTRL_ACK)
		dma_desc->txd.flags = DMA_CTRL_ACK;

	/*
	 * Make sure that mode should not be conflicting with currently
	 * configured mode.
	 */
	if (!tdc->isr_handler) {
		tdc->isr_handler = handle_cont_sngl_cycle_dma_done;
		tdc->cyclic = true;
	} else {
		if (!tdc->cyclic) {
			dev_err(tdc2dev(tdc), "ADMA configuration conflict\n");
			tegra_adma_desc_put(tdc, dma_desc);
			return NULL;
		}
	}

	return &dma_desc->txd;
}

static int tegra_adma_alloc_chan_resources(struct dma_chan *dc)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);

	pm_runtime_get_sync(tdc->tdma->dev);
	dma_cookie_init(&tdc->dma_chan);
	tdc->config_init = false;
	return 0;
}

static void tegra_adma_free_chan_resources(struct dma_chan *dc)
{
	struct tegra_adma_chan *tdc = to_tegra_adma_chan(dc);
	struct tegra_adma_desc *dma_desc;
	struct tegra_adma_sg_req *sg_req;
	struct list_head dma_desc_list;
	struct list_head sg_req_list;
	unsigned long flags;

	INIT_LIST_HEAD(&dma_desc_list);
	INIT_LIST_HEAD(&sg_req_list);

	dev_dbg(tdc2dev(tdc), "Freeing channel %d\n", tdc->id);

	if (tdc->busy)
		tegra_adma_terminate_all(dc);
	pm_runtime_put(tdc->tdma->dev);
	spin_lock_irqsave(&tdc->lock, flags);
	list_splice_init(&tdc->pending_sg_req, &sg_req_list);
	list_splice_init(&tdc->free_sg_req, &sg_req_list);
	list_splice_init(&tdc->free_dma_desc, &dma_desc_list);
	INIT_LIST_HEAD(&tdc->cb_desc);
	tdc->config_init = false;
	tdc->isr_handler = NULL;
	spin_unlock_irqrestore(&tdc->lock, flags);

	while (!list_empty(&dma_desc_list)) {
		dma_desc = list_first_entry(&dma_desc_list,
					typeof(*dma_desc), node);
		list_del(&dma_desc->node);
		kfree(dma_desc);
	}

	while (!list_empty(&sg_req_list)) {
		sg_req = list_first_entry(&sg_req_list, typeof(*sg_req), node);
		list_del(&sg_req->node);
		kfree(sg_req);
	}
}

static struct dma_chan *tegra_dma_of_xlate(struct of_phandle_args *dma_spec,
					   struct of_dma *ofdma)
{
	struct tegra_adma *tdma = ofdma->of_dma_data;
	struct dma_chan *chan;

	chan = dma_get_any_slave_channel(&tdma->dma_dev);
	if (!chan)
		return NULL;

	return chan;
}

static const struct tegra_adma_chip_data tegra210_adma_chip_data = {
	.channel_reg_size       = CH_REG_SIZE,
	.max_dma_count          = 1024UL * 64,
};

static const struct of_device_id tegra_adma_of_match[] = {
	{
		.compatible = "nvidia,tegra210-adma",
		.data = &tegra210_adma_chip_data,
	}, {
		.compatible = "nvidia,tegra210-adma-hv",
		.data = &tegra210_adma_chip_data,
	}, {
	},
};
MODULE_DEVICE_TABLE(of, tegra_adma_of_match);

static struct platform_device_id tegra_adma_devtype[] = {
	{
		.name = "tegra210-adma",
		.driver_data = (unsigned long)&tegra210_adma_chip_data,
	},
};

static struct device *dma_device;

static int tegra_adma_probe(struct platform_device *pdev)
{
	struct resource	*res;
	struct tegra_adma *tdma;
	unsigned int nr_channels = 0;
	unsigned int dma_start_index = 0;
	unsigned int adma_page = 1;
	bool is_virt = false;
	int ret, i;

	const struct tegra_adma_chip_data *cdata = NULL;
	const struct of_device_id *match;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No device tree node for ADMA driver");
		return -ENODEV;
	}

	match = of_match_device(of_match_ptr(tegra_adma_of_match),
				&pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	cdata = match->data;

	ret = of_property_read_u32(pdev->dev.of_node, "dma-channels",
			&nr_channels);
	if (ret) {
		dev_err(&pdev->dev, "failed to read dma-channels from DT\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "dma-start-index",
			&dma_start_index);
	if (ret) {
		/* Optional DT property. Assume to be zero and continue */
		dma_start_index = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "adma-page",
			&adma_page);
	if (ret) {
		/* Optional DT property. Assume to be one and continue */
		adma_page = 1;
	}

	if (of_device_is_compatible(pdev->dev.of_node,
	    "nvidia,tegra210-adma-hv")) {
		is_virt = true;
	}

	tdma = devm_kzalloc(&pdev->dev, sizeof(*tdma) + nr_channels *
			sizeof(struct tegra_adma_chan), GFP_KERNEL);
	if (!tdma) {
		dev_err(&pdev->dev, "Error: memory allocation failed\n");
		return -ENOMEM;
	}

	tdma->dev = &pdev->dev;
	tdma->chip_data = cdata;
	tdma->nr_channels = nr_channels;
	tdma->dma_start_index = dma_start_index;
	tdma->is_virt = is_virt;
	platform_set_drvdata(pdev, tdma);

	for (i = 0; i < ADMA_MAX_ADDR; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev, "No mem resource for ADMA\n");
			return -EINVAL;
		}

		tdma->adma_addr[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(tdma->adma_addr[i]))
			return PTR_ERR(tdma->adma_addr[i]);
	}

	/* Select Appropriate Base address for T18x */
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
		tdma->base_addr = tdma->adma_addr[adma_page];
#else
		tdma->base_addr = tdma->adma_addr[ADDR1];
#endif

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		tdma->dma_clk = devm_clk_get(&pdev->dev, NULL);
		if (IS_ERR(tdma->dma_clk)) {
			dev_err(&pdev->dev, "Error: Missing controller clock\n");
			return PTR_ERR(tdma->dma_clk);
		}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		tdma->ape_clk = clk_get_sys(NULL, "adma.ape");
		if (IS_ERR(tdma->ape_clk)) {
			dev_err(&pdev->dev, "Error: Missing APE clock\n");
			return PTR_ERR(tdma->ape_clk);
		}
#else
		tdma->ape_clk = devm_clk_get(&pdev->dev, "adma.ape");
		if (IS_ERR(tdma->ape_clk)) {
			dev_err(&pdev->dev, "Error: Missing APE clock\n");
			return PTR_ERR(tdma->ape_clk);
		}

		tdma->apb2ape_clk = devm_clk_get(&pdev->dev, "apb2ape");
		if (IS_ERR(tdma->apb2ape_clk)) {
			dev_err(&pdev->dev, "Error: Missing APB2APE clock\n");
			return PTR_ERR(tdma->apb2ape_clk);
		}
#endif
	}

	spin_lock_init(&tdma->global_lock);

	dma_device = &pdev->dev;

	tegra_pd_add_device(&pdev->dev);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra_adma_runtime_resume(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "dma_runtime_resume failed %d\n",
				ret);
			goto err_pm_disable;
		}
	}

	INIT_LIST_HEAD(&tdma->dma_dev.channels);
	for (i = 0; i < tdma->nr_channels; i++) {
		struct tegra_adma_chan *tdc = &tdma->channels[i];

		tdc->chan_base_offset = cdata->channel_reg_size *
					(i + tdma->dma_start_index);

		res = platform_get_resource(pdev, IORESOURCE_IRQ,
						(i + dma_start_index));
		if (!res) {
			ret = -EINVAL;
			dev_err(&pdev->dev, "No irq resource for chan %d\n", i);
			goto err_irq;
		}

		tdc->irq = res->start;
		snprintf(tdc->name, sizeof(tdc->name), "adma.%d", i);
		ret = devm_request_irq(&pdev->dev, tdc->irq,
				tegra_adma_isr, 0, tdc->name, tdc);
		if (ret) {
			dev_err(&pdev->dev,
				"request_irq failed with err %d channel %d\n",
				i, ret);
			goto err_irq;
		}

		tdc->dma_chan.device = &tdma->dma_dev;
		dma_cookie_init(&tdc->dma_chan);
		list_add_tail(&tdc->dma_chan.device_node,
				&tdma->dma_dev.channels);
		tdc->tdma = tdma;
		tdc->id = i;

		tasklet_init(&tdc->tasklet, tegra_adma_tasklet,
				(unsigned long)tdc);
		spin_lock_init(&tdc->lock);

		INIT_LIST_HEAD(&tdc->pending_sg_req);
		INIT_LIST_HEAD(&tdc->free_sg_req);
		INIT_LIST_HEAD(&tdc->free_dma_desc);
		INIT_LIST_HEAD(&tdc->cb_desc);
	}

	dma_cap_set(DMA_SLAVE, tdma->dma_dev.cap_mask);
	dma_cap_set(DMA_PRIVATE, tdma->dma_dev.cap_mask);
	dma_cap_set(DMA_CYCLIC, tdma->dma_dev.cap_mask);

	tdma->dma_dev.dev = &pdev->dev;
	tdma->dma_dev.device_alloc_chan_resources =
					tegra_adma_alloc_chan_resources;
	tdma->dma_dev.device_free_chan_resources =
					tegra_adma_free_chan_resources;
	tdma->dma_dev.device_prep_slave_sg = tegra_adma_prep_slave_sg;
	tdma->dma_dev.device_prep_dma_cyclic = tegra_adma_prep_dma_cyclic;
	tdma->dma_dev.device_control = tegra_adma_device_control;
	tdma->dma_dev.device_tx_status = tegra_adma_tx_status;
	tdma->dma_dev.device_issue_pending = tegra_adma_issue_pending;

	/* Enable clock before accessing registers */
	pm_runtime_get_sync(&pdev->dev);

	global_ch_write(tdma, ADMA_GLOBAL_INT_CLEAR, 0x1);
	global_ch_write(tdma, ADMA_GLOBAL_INT_SET, 0x1);

	if (tdma->is_virt == false) {
		/* Reset ADMA controller */
		global_write(tdma, ADMA_GLOBAL_SOFT_RESET, 0x1);

		/* Enable global ADMA registers */
		global_write(tdma, ADMA_GLOBAL_CMD, 1);
	} else {
		/* Audio Server owns ADMA GLOBAL and set registers */
		tdma->global_reg = 1;
	}

	pm_runtime_put_sync(&pdev->dev);

	ret = dma_async_device_register(&tdma->dma_dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Tegra ADMA driver registration failed %d\n", ret);
		goto err_irq;
	}

	ret = of_dma_controller_register(pdev->dev.of_node,
					 tegra_dma_of_xlate, tdma);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Tegra ADMA OF registration failed %d\n", ret);
		goto err_unregister_dma_dev;
	}

	dev_info(&pdev->dev, "Tegra ADMA driver register %d channels\n",
			tdma->nr_channels);
	return 0;

err_unregister_dma_dev:
	dma_async_device_unregister(&tdma->dma_dev);
err_irq:
	while (--i >= 0) {
		struct tegra_adma_chan *tdc = &tdma->channels[i];
		tasklet_kill(&tdc->tasklet);
	}

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_adma_runtime_suspend(&pdev->dev);
	tegra_pd_remove_device(&pdev->dev);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	clk_put(tdma->ape_clk);
#else
	devm_clk_put(&pdev->dev, tdma->ape_clk);
	devm_clk_put(&pdev->dev, tdma->apb2ape_clk);
#endif
	return ret;
}

static int tegra_adma_remove(struct platform_device *pdev)
{
	struct tegra_adma *tdma = platform_get_drvdata(pdev);
	int i;
	struct tegra_adma_chan *tdc;

	dma_async_device_unregister(&tdma->dma_dev);

	for (i = 0; i < tdma->nr_channels; ++i) {
		tdc = &tdma->channels[i];
		tasklet_kill(&tdc->tasklet);
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	clk_put(tdma->ape_clk);
#else
	devm_clk_put(&pdev->dev, tdma->ape_clk);
	devm_clk_put(&pdev->dev, tdma->apb2ape_clk);
#endif
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_adma_runtime_suspend(&pdev->dev);

	tegra_pd_remove_device(&pdev->dev);
	return 0;
}

static int tegra_adma_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_adma *tdma = platform_get_drvdata(pdev);
	int i;

	if (tdma->is_virt == false)
		tdma->global_reg = global_read(tdma, ADMA_GLOBAL_CMD);

	if (tdma->global_reg) {
		for (i = 0; i < tdma->nr_channels; i++) {
			struct tegra_adma_chan *tdc = &tdma->channels[i];
			struct tegra_adma_chan_regs *ch_reg = &tdc->channel_reg;

			ch_reg->tc = channel_read(tdc, ADMA_CH_TC);
			ch_reg->src_ptr =
				channel_read(tdc, ADMA_CH_LOWER_SOURCE_ADDR);
			ch_reg->tgt_ptr =
				channel_read(tdc, ADMA_CH_LOWER_TARGET_ADDR);
			ch_reg->ctrl = channel_read(tdc, ADMA_CH_CTRL);
			ch_reg->ahub_fifo_ctrl =
				channel_read(tdc, ADMA_CH_AHUB_FIFO_CTRL);
			ch_reg->config = channel_read(tdc, ADMA_CH_CONFIG);
		}
	}

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		clk_disable_unprepare(tdma->dma_clk);
		clk_disable_unprepare(tdma->ape_clk);
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
		clk_disable_unprepare(tdma->apb2ape_clk);
#endif
	}

	return 0;
}

static int tegra_adma_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_adma *tdma = platform_get_drvdata(pdev);
	int i;
	int ret;

	if (!(tegra_platform_is_unit_fpga() || tegra_platform_is_fpga())) {
		ret = clk_prepare_enable(tdma->ape_clk);
		if (ret < 0) {
			dev_err(dev, "clk_enable failed: %d\n", ret);
			return ret;
		}
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
		ret = clk_prepare_enable(tdma->apb2ape_clk);
		if (ret < 0) {
			dev_err(dev, "clk_enable failed: %d\n", ret);
			return ret;
		}
#endif
		ret = clk_prepare_enable(tdma->dma_clk);
		if (ret < 0) {
			dev_err(dev, "clk_enable failed: %d\n", ret);
			return ret;
		}
	}

	if (tdma->is_virt == false) {
		ret = tegra_adma_init(pdev, tdma->adma_addr);
		if (ret) {
			dev_err(dev, "tegra_adma_init failed: %d\n", ret);
			return ret;
		}

		global_write(tdma, ADMA_GLOBAL_CMD, tdma->global_reg);
	}

	if (tdma->global_reg) {
		for (i = 0; i < tdma->nr_channels; i++) {
			struct tegra_adma_chan *tdc = &tdma->channels[i];
			struct tegra_adma_chan_regs *ch_reg = &tdc->channel_reg;

			channel_write(tdc, ADMA_CH_TC, ch_reg->tc);
			channel_write(tdc, ADMA_CH_LOWER_SOURCE_ADDR,
					ch_reg->src_ptr);
			channel_write(tdc, ADMA_CH_LOWER_TARGET_ADDR,
					ch_reg->tgt_ptr);
			channel_write(tdc, ADMA_CH_CTRL, ch_reg->ctrl);
			channel_write(tdc, ADMA_CH_AHUB_FIFO_CTRL,
					ch_reg->ahub_fifo_ctrl);
			channel_write(tdc, ADMA_CH_CONFIG, ch_reg->config);
		}
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_adma_pm_suspend(struct device *dev)
{
	return 0;
}

static int tegra_adma_pm_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops tegra_adma_dev_pm_ops = {
#ifdef CONFIG_PM
	.runtime_suspend = tegra_adma_runtime_suspend,
	.runtime_resume = tegra_adma_runtime_resume,
#endif
	SET_SYSTEM_SLEEP_PM_OPS(tegra_adma_pm_suspend, tegra_adma_pm_resume)
};

static struct platform_driver tegra_admac_driver = {
	.driver = {
		.name	= "tegra-adma",
		.owner = THIS_MODULE,
		.pm	= &tegra_adma_dev_pm_ops,
		.of_match_table = of_match_ptr(tegra_adma_of_match),
	},
	.probe		= tegra_adma_probe,
	.remove		= tegra_adma_remove,
	.id_table	= tegra_adma_devtype,
};

module_platform_driver(tegra_admac_driver);

MODULE_ALIAS("platform:tegra210-adma");
MODULE_DESCRIPTION("NVIDIA Tegra ADMA Controller driver");
MODULE_AUTHOR("Dara Ramesh <dramesh@nvidia.com>");
MODULE_LICENSE("GPL v2");
