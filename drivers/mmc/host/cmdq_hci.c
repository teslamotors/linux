/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/leds.h>
#include <linux/platform_device.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/mmc/cmdq_hci.h>

/* 1 sec FIXME: optimize it */
#define HALT_TIMEOUT_MS 1000
#define TASK_CLEAR_TIMEOUT_MS 10
/* Timeout vaue = 127 * 1024 * timer_clk_freq;
 * timer_clk_freq = base_clk_freq * internal_freq_mul;
 * base_clk_freq = 200MHz;
 * internal_freq_mul = 0.001;
 */
#define QUEUE_EMPTY_TIMEOUT_MS 650

static inline u64 *get_desc(struct cmdq_host *cq_host, u8 tag)
{
	return cq_host->desc_base + (tag * cq_host->slot_sz);
}

static inline u64 *get_link_desc(struct cmdq_host *cq_host, u8 tag)
{
	u64 *desc = get_desc(cq_host, tag);

	return desc + cq_host->task_desc_len;
}

static inline dma_addr_t get_trans_desc_dma(struct cmdq_host *cq_host, u8 tag)
{
	u8 mul = sizeof(u64)/sizeof(dma_addr_t);
	return cq_host->trans_desc_dma_base +
		(mul * cq_host->mmc->max_segs * tag * cq_host->trans_desc_len);
}

static inline u64 *get_trans_desc(struct cmdq_host *cq_host, u8 tag)
{
	u8 mul = sizeof(u64)/sizeof(dma_addr_t);

	return cq_host->trans_desc_base +
		(mul * cq_host->mmc->max_segs * tag *
		(cq_host->trans_desc_len / sizeof(*cq_host->trans_desc_base)));
}

static void setup_trans_desc(struct cmdq_host *cq_host, u8 tag)
{
	u64 *link_temp;
	dma_addr_t trans_temp;

	link_temp = get_link_desc(cq_host, tag);
	trans_temp = get_trans_desc_dma(cq_host, tag);

	memset(link_temp, 0, sizeof(*link_temp));
	if (cq_host->link_desc_len > 1)
		*(link_temp + 1) &= 0;

	*link_temp = VALID(1) | ACT(0x6) | END(0);

	*link_temp |= DAT_ADDR_LO((u64)lower_32_bits(trans_temp));
	if (cq_host->link_desc_len > 1)
		*(link_temp + 1) |= DAT_ADDR_HI(upper_32_bits(trans_temp));
}

static void cmdq_clear_set_irqs(struct cmdq_host *cq_host, u32 clear, u32 set)
{
	u32 ier;

	ier = cmdq_readl(cq_host, CQISTE);
	ier &= ~clear;
	ier |= set;
	cmdq_writel(cq_host, ier, CQISTE);
	cmdq_writel(cq_host, ier, CQISGE);
	/* ensure the writes are done */
	mb();
}


#define DRV_NAME "cmdq-host"

static void cmdq_dumpregs(struct cmdq_host *cq_host)
{
	struct mmc_host *mmc = cq_host->mmc;

	pr_debug(DRV_NAME ": ========== CMDQ REGISTER DUMP (%s)==========\n",
		mmc_hostname(mmc));

	pr_debug(DRV_NAME ": Version: 0x%08x | Caps:  0x%08x\n",
		cmdq_readl(cq_host, CQVER),
		cmdq_readl(cq_host, CQCAP));
	pr_debug(DRV_NAME ": Queing config: 0x%08x | Queue Ctrl:  0x%08x\n",
		cmdq_readl(cq_host, CQCFG),
		cmdq_readl(cq_host, CQCTL));
	pr_debug(DRV_NAME ": Int stat: 0x%08x | Int enab:  0x%08x\n",
		cmdq_readl(cq_host, CQIS),
		cmdq_readl(cq_host, CQISTE));
	pr_debug(DRV_NAME ": Int sig: 0x%08x | Int Coal:  0x%08x\n",
		cmdq_readl(cq_host, CQISGE),
		cmdq_readl(cq_host, CQIC));
	pr_debug(DRV_NAME ": TDL base: 0x%08x | TDL up32:  0x%08x\n",
		cmdq_readl(cq_host, CQTDLBA),
		cmdq_readl(cq_host, CQTDLBAU));
	pr_debug(DRV_NAME ": Doorbell: 0x%08x | Comp Notif:  0x%08x\n",
		cmdq_readl(cq_host, CQTDBR),
		cmdq_readl(cq_host, CQTCN));
	pr_debug(DRV_NAME ": Dev queue: 0x%08x | Dev Pend:  0x%08x\n",
		cmdq_readl(cq_host, CQDQS),
		cmdq_readl(cq_host, CQDPT));
	pr_debug(DRV_NAME ": Task clr: 0x%08x | Send stat 1:  0x%08x\n",
		cmdq_readl(cq_host, CQTCLR),
		cmdq_readl(cq_host, CQSSC1));
	pr_debug(DRV_NAME ": Send stat 2: 0x%08x | DCMD resp:  0x%08x\n",
		cmdq_readl(cq_host, CQSSC2),
		cmdq_readl(cq_host, CQCRDCT));
	pr_debug(DRV_NAME ": Resp err mask: 0x%08x | Task err:  0x%08x\n",
		cmdq_readl(cq_host, CQRMEM),
		cmdq_readl(cq_host, CQTERRI));
	pr_debug(DRV_NAME ": Resp idx: 0x%08x | Resp arg:  0x%08x\n",
		cmdq_readl(cq_host, CQCRI),
		cmdq_readl(cq_host, CQCRA));
	pr_debug(DRV_NAME ": ===========================================\n");

	if (cq_host->ops->dump_vendor_regs)
		cq_host->ops->dump_vendor_regs(mmc);
}

static void cmdq_wait_cq_empty(struct mmc_host *mmc)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	int timeout = QUEUE_EMPTY_TIMEOUT_MS;

	if (cq_host->ops->runtime_pm_get)
		cq_host->ops->runtime_pm_get(mmc);
	while (timeout) {
		if (!cmdq_readl(cq_host, CQTDBR))
			break;
		mdelay(1);
		--timeout;
	}
	if (!timeout)
		pr_err("%s: Some tasks are pending to process by CQE\n",
				mmc_hostname(mmc));

	timeout = QUEUE_EMPTY_TIMEOUT_MS;
	while (timeout) {
		if (!cmdq_readl(cq_host, CQTCN))
			break;
		mdelay(1);
		--timeout;
	}
	if (!timeout)
		pr_err("%s: Some tasks are pending for post proccess by CQE\n",
				mmc_hostname(mmc));
	if (cq_host->ops->runtime_pm_put)
		cq_host->ops->runtime_pm_put(mmc);
}

/**
 * The allocated descriptor table for task, link & transfer descritors
 * looks like:
 * |----------|
 * |task desc |  |->|----------|
 * |----------|  |  |trans desc|
 * |link desc-|->|  |----------|
 * |----------|          .
 *      .                .
 *  no. of slots      max-segs
 *      .           |----------|
 * |----------|
 * The idea here is to create the [task+trans] table and mark & point the
 * link desc to the transfer desc table on a per slot basis.
 */
static int cmdq_host_alloc_tdl(struct cmdq_host *cq_host)
{

	int i = 0;

	/* task descriptor can be 64/128 bit irrespective of arch */
	if (cq_host->caps & CMDQ_TASK_DESC_SZ_128) {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCFG) |
			       CQ_TASK_DESC_SZ, CQCFG);
		cq_host->task_desc_len = 2;
	} else {
		cq_host->task_desc_len = 1;
	}

	/* transfer desc. is 64 bit for 32 bit arch and 128 bit for 64 bit */
	if (cq_host->dma64)
		cq_host->link_desc_len = 2;
	else
		cq_host->link_desc_len = 1;

	/* total size of a slot: 1 task & 1 transfer (link) */
	cq_host->slot_sz = cq_host->task_desc_len + cq_host->link_desc_len;

	/*
	 * 96 bits length of transfer desc instead of 128 bits which means
	 * ADMA would expect next valid descriptor at the 96th bit
	 * or 128th bit
	 */
	if (cq_host->dma64) {
		if (cq_host->quirks & CMDQ_QUIRK_SHORT_TXFR_DESC_SZ)
			cq_host->trans_desc_len = 12;
		else
			cq_host->trans_desc_len = 16;
	}

	cq_host->desc_size = (sizeof(*cq_host->desc_base)) *
		cq_host->slot_sz * cq_host->num_slots;

	/* FIXME: consider allocating smaller chunks iteratively */
	cq_host->data_size = cq_host->trans_desc_len * cq_host->mmc->max_segs *
		(cq_host->num_slots - 1);

	/*
	 * allocate a dma-mapped chunk of memory for the descriptors
	 * allocate a dma-mapped chunk of memory for link descriptors
	 * setup each link-desc memory offset per slot-number to
	 * the descriptor table.
	 */
	cq_host->desc_base = dma_zalloc_coherent(mmc_dev(cq_host->mmc),
						 cq_host->desc_size,
						 &cq_host->desc_dma_base,
						 GFP_KERNEL);
	cq_host->trans_desc_base = dma_zalloc_coherent(mmc_dev(cq_host->mmc),
					      cq_host->data_size,
					      &cq_host->trans_desc_dma_base,
					      GFP_KERNEL);

	if (!cq_host->desc_base || !cq_host->trans_desc_base)
		return -ENOMEM;

	for (; i < (cq_host->num_slots - 1); i++)
		setup_trans_desc(cq_host, i);

	return 0;
}

static int cmdq_enable(struct mmc_host *mmc)
{
	int err = 0;
	u32 cqcfg;
	bool dcmd_enable;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);

	if (cq_host->ops->runtime_pm_get)
		cq_host->ops->runtime_pm_get(mmc);

	if (!cq_host || !mmc->card || !mmc_card_mmc(mmc->card)) {
		err = -EINVAL;
		goto out;
	}

	if (cq_host->enabled)
		goto out;

	/* TODO: if the legacy MMC host controller is in idle state */

	cqcfg = cmdq_readl(cq_host, CQCFG);
	if (cqcfg & 0x1) {
		pr_info("%s: %s: cq_host is already enabled\n",
				mmc_hostname(mmc), __func__);
		WARN_ON(1);
		cq_host->enabled = true;
		goto out;
	}

	if (cq_host->quirks & CMDQ_QUIRK_NO_DCMD)
		dcmd_enable = false;
	else
		dcmd_enable = true;

	cqcfg = ((cq_host->dma64 ? CQ_TASK_DESC_SZ : 0) |
			(dcmd_enable ? CQ_DCMD : 0));

	cmdq_writel(cq_host, cqcfg, CQCFG);

	if (!cq_host->desc_base ||
			!cq_host->trans_desc_base) {
		err = cmdq_host_alloc_tdl(cq_host);
		if (err)
			goto out;
	}

	cmdq_writel(cq_host, lower_32_bits(cq_host->desc_dma_base),
			CQTDLBA);
	cmdq_writel(cq_host, upper_32_bits(cq_host->desc_dma_base),
			CQTDLBAU);

	/* Enable Interrupt Coalescing feature */
	if (cq_host->quirks & CMDQ_QUIRK_CQIC_SUPPORT)
		cmdq_writel(cq_host, (CQIC_ENABLE | CQIC_ICTOVALWEN |
				CQIC_DEFAULT_ICTOVAL), CQIC);

	/* leave send queue status timer configs to reset values */

	/* configure interrupt coalescing */
	/* mmc_cq_host_intr_aggr(host->cq_host, CQIC_DEFAULT_ICCTH,
	   CQIC_DEFAULT_ICTOVAL); */

	/* leave CQRMEM to reset value */

	/*
	 * disable all vendor interrupts
	 * enable CMDQ interrupts
	 * enable the vendor error interrupts
	 */
	if (cq_host->ops->clear_set_irqs)
		cq_host->ops->clear_set_irqs(mmc, CQ_INT_ALL, CQ_INT_EN);

	cmdq_clear_set_irqs(cq_host, 0x0, CQ_INT_ALL);

	/* cq_host would use this rca to address the card */
	cmdq_writel(cq_host, mmc->card->rca, CQSSC2);

	/* ensure the writes are done before enabling CQE */
	mb();

	/* enable CQ_HOST */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQCFG) | CQ_ENABLE,
		    CQCFG);
	cq_host->enabled = true;
out:
	if (cq_host->ops->runtime_pm_put)
		cq_host->ops->runtime_pm_put(mmc);
	return err;
}

int cmdq_reenable(struct mmc_host *mmc)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	int err = 0;

	cq_host->enabled = false;
	err = cmdq_enable(mmc);
	if (err)
		pr_err("%s: error %d in re-enabling cmdq engine\n",
			mmc_hostname(mmc), err);

	return err;
}
EXPORT_SYMBOL(cmdq_reenable);

static void cmdq_disable(struct mmc_host *mmc, bool soft)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	if (soft) {
		cmdq_writel(cq_host, cmdq_readl(
				    cq_host, CQCFG) & ~(CQ_ENABLE),
			    CQCFG);
	} else {
		if (cq_host->desc_base)
			dma_free_coherent(mmc_dev(cq_host->mmc),
				cq_host->desc_size, cq_host->desc_base,
				cq_host->desc_dma_base);
		if (cq_host->trans_desc_base)
			dma_free_coherent(mmc_dev(cq_host->mmc),
				cq_host->data_size, cq_host->desc_base,
				cq_host->trans_desc_dma_base);
	}
	cq_host->enabled = false;
}

static void cmdq_prep_task_desc(struct mmc_request *mrq,
					u64 *data, bool intr, bool qbr)
{
	struct mmc_cmdq_req *cmdq_req = mrq->cmdq_req;
	u32 req_flags = cmdq_req->cmdq_req_flags;

	pr_debug("%s: %s: data-tag: 0x%08x - dir: %d - prio: %d - cnt: 0x%08x - addr: 0x%llx, intr: %d\n",
		 mmc_hostname(mrq->host), __func__,
		 !!(req_flags & DAT_TAG), !!(req_flags & DIR),
		 !!(req_flags & PRIO), cmdq_req->data.blocks,
		 (u64)mrq->cmdq_req->blk_addr, intr);

	*data = VALID(1) |
		END(1) |
		INT(intr) |
		ACT(0x5) |
		FORCED_PROG(!!(req_flags & FORCED_PRG)) |
		CONTEXT(mrq->cmdq_req->ctx_id) |
		DATA_TAG(!!(req_flags & DAT_TAG)) |
		DATA_DIR(!!(req_flags & DIR)) |
		PRIORITY(!!(req_flags & PRIO)) |
		QBAR(qbr) |
		REL_WRITE(!!(req_flags & REL_WR)) |
		BLK_COUNT(mrq->cmdq_req->data.blocks) |
		BLK_ADDR((u64)mrq->cmdq_req->blk_addr);
}

static int cmdq_dma_map(struct mmc_host *host, struct mmc_request *mrq)
{
	int sg_count;
	struct mmc_data *data = mrq->data;

	if (!data)
		return -EINVAL;

	sg_count = dma_map_sg(mmc_dev(host), data->sg,
			      data->sg_len,
			      (data->flags & MMC_DATA_WRITE) ?
			      DMA_TO_DEVICE : DMA_FROM_DEVICE);
	if (!sg_count)
		return -ENOMEM;

	return sg_count;
}

static void cmdq_set_tran_desc(u8 *desc, dma_addr_t addr, int len,
		bool end, bool adma2_26bit)
{
	__le32 *link = (__le32 __force *)desc;
	__le64 *dataddr = (__le64 __force *)(desc + 4);

	*link = (VALID(1) |
		 END(end ? 1 : 0) |
		 INT(0) |
		 ACT(0x4));
	if (adma2_26bit)
		*link = *link | DAT_LENGTH_26BIT(len);
	else
		*link = *link | DAT_LENGTH(len);

	dataddr[0] = cpu_to_le64(addr);
}

static int cmdq_prep_tran_desc(struct mmc_request *mrq,
			       struct cmdq_host *cq_host, int tag)
{
	struct mmc_data *data = mrq->data;
	int i, sg_count, len;
	bool end = false;
	u64 *trans_desc = NULL;
	dma_addr_t addr;
	u8 *desc;
	struct scatterlist *sg;

	sg_count = cmdq_dma_map(mrq->host, mrq);
	if (sg_count < 0) {
		pr_err("%s: %s: unable to map sg lists, %d\n",
				mmc_hostname(mrq->host), __func__, sg_count);
		return sg_count;
	}

	trans_desc = get_trans_desc(cq_host, tag);
	desc = (u8 *)trans_desc;

	memset(trans_desc, 0, sizeof(*trans_desc));

	for_each_sg(data->sg, sg, sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		if ((i+1) == sg_count)
			end = true;
		cmdq_set_tran_desc(desc, addr, len, end,
				cq_host->enable_adma2_26bit);
		if (cq_host->dma64)
			desc += cq_host->trans_desc_len;
		else
			desc += 8;
	}

	pr_debug("%s: req: 0x%p tag: %d calc-link_desc: 0x%p, sg-cnt: %d\n",
		__func__, mrq->req, tag, trans_desc, sg_count);

	return 0;
}

static void cmdq_prep_dcmd_desc(struct mmc_host *mmc,
				   struct mmc_request *mrq)
{
	u64 *task_desc = NULL;
	u64 data = 0;
	u8 resp_type;
	u8 *desc;
	__le64 *dataddr;
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	int r1b = 0;

	if (!(mrq->cmd->flags & MMC_RSP_PRESENT)) {
		resp_type = 0;
	} else if (mrq->cmd->flags & MMC_RSP_R1B) {
		resp_type = 3;
		r1b = 1;
	} else if (mrq->cmd->flags & (MMC_RSP_R1 | MMC_RSP_R4 | MMC_RSP_R5)) {
		resp_type = 2;
	} else {
		pr_err("%s: weird response: 0x%x\n", mmc_hostname(mmc),
			mrq->cmd->flags);
		BUG_ON(1);
	}

	task_desc = get_desc(cq_host, cq_host->dcmd_slot);
	memset(task_desc, 0, sizeof(*task_desc));
	data |= (VALID(1) |
		 END(1) |
		 INT(1) |
		 QBAR(r1b) |
		 ACT(0x5) |
		 CMD_INDEX(mrq->cmd->opcode) |
		 CMD_TIMING(r1b) | RESP_TYPE(resp_type));
	*task_desc |= data;
	desc = (u8 *)task_desc;

	dataddr = (__le64 __force *)(desc + 4);
	dataddr[0] = cpu_to_le64((u64)mrq->cmd->arg);

	if (cq_host->ops->set_data_timeout)
		cq_host->ops->set_data_timeout(mmc, 0xe);
}

static int cmdq_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int err;
	u64 data = 0;
	u64 *task_desc = NULL;
	u32 tag = mrq->cmdq_req->tag;
	bool intr = true;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned long flags;

	if (!cq_host->enabled) {
		err = -EINVAL;
		goto out;
	}

	if (cq_host->ops->runtime_pm_get)
		cq_host->ops->runtime_pm_get(mmc);

	spin_lock_irqsave(&cq_host->cmdq_lock, flags);

	if (mrq->cmdq_req->cmdq_req_flags & DCMD) {
		cmdq_prep_dcmd_desc(mmc, mrq);
		cq_host->mrq_slot[31] = mrq;
		cmdq_writel(cq_host, 1 << 31, CQTDBR);
		spin_unlock_irqrestore(&cq_host->cmdq_lock, flags);
		return 0;
	}

	cq_host->data = mrq->data;

	task_desc = get_desc(cq_host, tag);

	if (cq_host->quirks & CMDQ_QUIRK_CQIC_SUPPORT)
		intr = false;

	cmdq_prep_task_desc(mrq, &data, intr,
			    (mrq->cmdq_req->cmdq_req_flags & QBR));
	*task_desc = cpu_to_le64(data);

	err = cmdq_prep_tran_desc(mrq, cq_host, tag);
	if (err) {
		pr_err("%s: %s: failed to setup tx desc: %d\n",
		       mmc_hostname(mmc), __func__, err);
		BUG_ON(1);
	}

	BUG_ON(cmdq_readl(cq_host, CQTDBR) & (1 << tag));

	cq_host->mrq_slot[tag] = mrq;
	if (cq_host->ops->set_tranfer_params)
		cq_host->ops->set_tranfer_params(mmc);

	if (cq_host->ops->set_data_timeout)
		cq_host->ops->set_data_timeout(mmc, 0xe);

	cmdq_writel(cq_host, 1 << tag, CQTDBR);
	spin_unlock_irqrestore(&cq_host->cmdq_lock, flags);

out:
	return err;
}

static void cmdq_finish_data(struct mmc_host *mmc, unsigned int tag)
{
	struct mmc_request *mrq;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	mrq = cq_host->mrq_slot[tag];
	if (mrq->cmdq_req->cmdq_req_flags & DCMD)
		mrq->cmd->resp[0] = cmdq_readl(cq_host, CQCRDCT);
	mrq->done(mrq);

	if (cq_host->ops->runtime_pm_put)
		cq_host->ops->runtime_pm_put(mmc);
}

irqreturn_t cmdq_irq(struct mmc_host *mmc, u32 intmask)
{
	u32 status, cqtdbr, cqtcn;
	unsigned long tag = 0, comp_status;
	unsigned long cqic;
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);

	spin_lock(&cq_host->cmdq_lock);

	status = cmdq_readl(cq_host, CQIS);
	cmdq_writel(cq_host, status, CQIS);

	cqtcn = cmdq_readl(cq_host, CQTCN);
	cqtdbr = cmdq_readl(cq_host, CQTDBR);

	if (cq_host->quirks & CMDQ_QUIRK_CQIC_SUPPORT) {
		cqic = cmdq_readl(cq_host, CQIC);
		cmdq_writel(cq_host, cqic | CQIC_RESET, CQIC);
	}

	pr_debug("%s: %s: CQIS: %x, intmask: %x\n", mmc_hostname(mmc),
			__func__, status, intmask);

	if (mmc->ops->clear_cqe_intr)
		mmc->ops->clear_cqe_intr(mmc, intmask);

	if (status & CQIS_HAC) {
		/* halt is completed, wakeup waiting thread */
		complete(&cq_host->halt_comp);
	} else if (status & CQIS_TCC) {
		/* read QCTCN and complete the request */
		comp_status = cqtcn & ~cqtdbr;
		if (!comp_status)
			pr_debug("%s: bogus comp-stat\n", __func__);

		for_each_set_bit(tag, &comp_status, cq_host->num_slots) {
			/* complete the corresponding mrq */
			cmdq_finish_data(mmc, tag);
			/* complete DCMD on tag 31 */
		}
		cmdq_writel(cq_host, comp_status, CQTCN);
	} else if (status & CQIS_RED) {
		/* task response has an error */
		pr_err("%s: RED error %d !!!\n", mmc_hostname(mmc), status);
		cmdq_dumpregs(cq_host);
		BUG_ON(1);
	} else if (status & CQIS_TCL) {
		/* task is cleared, wakeup waiting thread */
		;
	}
	spin_unlock(&cq_host->cmdq_lock);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(cmdq_irq);

static unsigned int cmdq_poll_clear(struct cmdq_host *cq_host, u32 reg, u32 bit,
		unsigned timeout)
{
	do {
		if (!(cmdq_readl(cq_host, reg) & bit))
			break;
		udelay(1);
		--timeout;
	} while (timeout);

	return timeout;
}

static int cmdq_discard_task(struct mmc_host *mmc, u32 tag, bool all)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	u32 task = 1 << tag;
	int err = 0;

	if (all) {
		/* Discard entire queue in the device */
		if (mmc->ops->discard_cqe_task)
			mmc->ops->discard_cqe_task(mmc, tag, true);

		/* Write '1' to 8th bit of CQCTL to clear all tasks sent
		 * to the device.
		 */
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) |
				CLEAR_ALL_TASKS, CQCTL);

		/* Poll on CQCTL until 8th bit is set to 0 */
		err = cmdq_poll_clear(cq_host, CQCTL, CLEAR_ALL_TASKS,
				TASK_CLEAR_TIMEOUT_MS);
		if (!err) {
			pr_err("%s: timeout(%dms) reached to clear all cqe task\n",
				mmc_hostname(mmc), TASK_CLEAR_TIMEOUT_MS);
			cmdq_dumpregs(cq_host);
			return -ETIMEDOUT;
		} else {
			pr_debug("%s: all cmdq task are cleared from cqe\n",
				mmc_hostname(mmc));
			goto cqe_resume;
		}
	} else if (cmdq_readl(cq_host, CQDPT) & task) {
		/* CQDPT[tag] == 1, Task queued in the device.
		 * Send CMD48 to discard the task.
		 * If CQDPT[tag] == 0 then clear the task from CQTCLR only
		 */
		if (mmc->ops->discard_cqe_task)
			mmc->ops->discard_cqe_task(mmc, tag, false);
	}

	/* Write '1' to CQTCLR[tag] to clear task in CQE */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQTCLR) | task, CQTCLR);

	/* Poll on CQTCLR[tag] until it is '0' */
	err = cmdq_poll_clear(cq_host, CQTCLR, task, TASK_CLEAR_TIMEOUT_MS);

	if (!err) {
		pr_err("%s: timeout(%dms) reached to clear the cqe task: %u\n",
			mmc_hostname(mmc), TASK_CLEAR_TIMEOUT_MS, tag);
		cmdq_dumpregs(cq_host);
		return -ETIMEDOUT;
	} else {
		pr_debug("%s: cmdq task %d is cleared from cqe\n",
			mmc_hostname(mmc), tag);
	}

cqe_resume:
	/* Resume CQE operations by writing '0' to 0th bit in CQCTL */
	cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT, CQCTL);
	return 0;
}

/* May sleep */
static int cmdq_halt(struct mmc_host *mmc, bool halt)
{
	struct cmdq_host *cq_host = (struct cmdq_host *)mmc_cmdq_private(mmc);
	unsigned timeout = HALT_TIMEOUT_MS;
	int err = 0;

	if (cq_host->ops->runtime_pm_get)
		cq_host->ops->runtime_pm_get(mmc);
	if (halt) {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) | HALT,
			    CQCTL);
		/* Poll for 1000ms until the Halt is set in CQCTL */
		do {
			if (cmdq_readl(cq_host, CQCTL) & HALT)
				break;
			mdelay(1);
			timeout--;
		} while (timeout);

		if (!timeout) {
			pr_err("%s: Setting HALT is failed\n",
					mmc_hostname(mmc));
			cmdq_dumpregs(cq_host);
			cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & 0x1FE,
			    CQCTL);
			if (cq_host->ops->runtime_pm_put)
				cq_host->ops->runtime_pm_put(mmc);
			err = -ETIMEDOUT;
		}
	} else {
		cmdq_writel(cq_host, cmdq_readl(cq_host, CQCTL) & ~HALT,
			    CQCTL);
	}

	if (cq_host->ops->runtime_pm_put)
		cq_host->ops->runtime_pm_put(mmc);
	return err;
}

static void cmdq_post_req(struct mmc_host *host, struct mmc_request *mrq,
			  int err)
{
	struct mmc_data *data = mrq->data;

	if (data) {
		if (data->error)
			data->bytes_xfered = 0;
		else
			data->bytes_xfered = data->blksz * data->blocks;

		data->error = 0;
		dma_unmap_sg(mmc_dev(host), data->sg, data->sg_len,
			     (data->flags & MMC_DATA_READ) ?
			     DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}
}

static const struct mmc_cmdq_host_ops cmdq_host_ops = {
	.enable = cmdq_enable,
	.disable = cmdq_disable,
	.request = cmdq_request,
	.halt = cmdq_halt,
	.post_req = cmdq_post_req,
	.discard_task = cmdq_discard_task,
	.wait_cq_empty = cmdq_wait_cq_empty,
};

struct cmdq_host *cmdq_pltfm_init(struct platform_device *pdev)
{
	struct cmdq_host *cq_host;
	struct resource *cmdq_memres = NULL;

	/* check and setup CMDQ interface */
	cmdq_memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!cmdq_memres) {
		dev_err(&pdev->dev, "CMDQ: CMDQ is not supported\n");
		return ERR_PTR(-EINVAL);
	}

	cq_host = kzalloc(sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		dev_err(&pdev->dev, "CMDQ: failed to allocate memory for CMDQ\n");
		return ERR_PTR(-ENOMEM);
	}

	cq_host->mmio = devm_ioremap(&pdev->dev, cmdq_memres->start + CQE_BASE,
			CQE_RES_SZ);

	if (!cq_host->mmio) {
		dev_err(&pdev->dev, "CMDQ: failed to remap cmdq regs\n");
		kfree(cq_host);
		return ERR_PTR(-EBUSY);
	}
	dev_dbg(&pdev->dev, "CMDQ ioremap: done\n");

	return cq_host;
}
EXPORT_SYMBOL(cmdq_pltfm_init);

int cmdq_init(struct cmdq_host *cq_host, struct mmc_host *mmc,
	      bool dma64)
{
	if (cq_host == NULL)
		return -EINVAL;

	cq_host->dma64 = dma64;
	cq_host->mmc = mmc;

	/* should be parsed */
	cq_host->num_slots = 32;
	cq_host->dcmd_slot = 31;

	if (!cq_host->dma64)
		cq_host->quirks |= CMDQ_QUIRK_SHORT_TXFR_DESC_SZ;

	cq_host->caps |= CMDQ_TASK_DESC_SZ_128;

	/* Set mmc caps */
	/*
	 * Fix me: Add option to disable QBR support through platform data or
	 * quirks.
	 */
	mmc->caps2 |= MMC_CAP2_CMDQ_QBR;

	mmc->cmdq_ops = &cmdq_host_ops;

	cq_host->mrq_slot = kzalloc(sizeof(cq_host->mrq_slot) *
				    cq_host->num_slots, GFP_KERNEL);
	if (!cq_host->mrq_slot)
		return -ENOMEM;

	spin_lock_init(&cq_host->cmdq_lock);
	init_completion(&cq_host->halt_comp);
	return 0;
}
EXPORT_SYMBOL(cmdq_init);

