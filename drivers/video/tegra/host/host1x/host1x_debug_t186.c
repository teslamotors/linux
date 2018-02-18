/*
 * Tegra Graphics Host Hardware Debug Functions
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "chip_support.h"

#define NVHOST_DEBUG_MAX_PAGE_OFFSET 102400

enum {
	NVHOST_DBG_STATE_CMD = 0,
	NVHOST_DBG_STATE_DATA = 1,
	NVHOST_DBG_STATE_GATHER = 2
};

static void do_show_channel_gather(struct output *o,
		phys_addr_t phys_addr,
		u32 words, struct nvhost_cdma *cdma,
		phys_addr_t pin_addr, u32 *map_addr)
{
	/* Map dmaget cursor to corresponding nvmap_handle */
	u32 offset;
	int state, i;

	offset = phys_addr - pin_addr;
	/*
	 * Sometimes we're given different hardware address to the same
	 * page - in these cases the offset will get an invalid number and
	 * we just have to bail out.
	 */
	if (offset > NVHOST_DEBUG_MAX_PAGE_OFFSET) {
		nvhost_debug_output(o, "[address mismatch]\n");
	} else {
		/* GATHER buffer starts always with commands */
		state = NVHOST_DBG_STATE_CMD;
		for (i = 0; i < words; i++)
			nvhost_debug_output(o,
					"%08x ", *(map_addr + offset/4 + i));
		nvhost_debug_output(o, "\n");
	}
}

static void show_channel_gathers(struct output *o, struct nvhost_cdma *cdma)
{
	struct nvhost_job *job;
	int i;

	if (list_empty(&cdma->sync_queue)) {
		nvhost_debug_output(o, "The CDMA sync queue is empty.\n");
		return;
	}

	job = list_first_entry(&cdma->sync_queue, struct nvhost_job, list);

	nvhost_debug_output(o, "\n%p: JOB, syncpt_id=%d, syncpt_val=%d,"
			" first_get=%08x, timeout=%d,"
			" num_slots=%d\n",
			job,
			job->sp ? job->sp->id : -1,
			job->sp ? job->sp->fence : -1,
			job->first_get,
			job->timeout,
			job->num_slots);

	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];
		u32 *mapped = dma_buf_vmap(g->buf);
		if (!mapped) {
			nvhost_debug_output(o, "[could not mmap]\n");
			continue;
		}

		nvhost_debug_output(o,
			"    GATHER at %08llx+%04x, %u words\n",
			(u64)g->mem_base, g->offset, g->words);

		do_show_channel_gather(o, g->mem_base + g->offset,
				g->words, cdma, g->mem_base, mapped);
		dma_buf_vunmap(g->buf, mapped);
	}
}

static void show_channel_regs(struct output *o, struct nvhost_channel *ch,
	struct nvhost_cdma *cdma)
{
	u32 val;

	nvhost_debug_output(o, "NvHost basic channel registers:\n");
	val = host1x_channel_readl(ch, host1x_channel_fifostat_r());
	nvhost_debug_output(o, "CMDFIFO_STAT_0:  %08x\n", val);
	val = host1x_channel_readl(ch, host1x_channel_rdata_r());
	nvhost_debug_output(o, "CMDFIFO_RDATA_0: %08x\n", val);
	if (!tegra_platform_is_linsim()) {
		val = host1x_channel_readl(ch, host1x_channel_cmdp_offset_r());
		nvhost_debug_output(o, "CMDP_OFFSET_0:   %08x\n", val);
		val = host1x_channel_readl(ch, host1x_channel_cmdp_class_r());
		nvhost_debug_output(o, "CMDP_CLASS_0:    %08x\n", val);
	}
	val = host1x_channel_readl(ch, host1x_channel_cmdp_channelstat_r());
	nvhost_debug_output(o, "CHANNELSTAT_0:   %08x\n", val);

}

static void debug_show_channel_cdma(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	struct nvhost_cdma *cdma = &ch->cdma;

	show_channel_regs(o, ch, cdma);
	show_channel_gathers(o, cdma);
	nvhost_debug_output(o, "\n");
}

static void debug_show_channel_fifo(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	u32 val, temp, rd_ptr, wr_ptr, start, end, max = 64;
	struct platform_device *pdev = to_platform_device(ch->dev->dev.parent);

	nvhost_debug_output(o, "%d: fifo:\n", chid);

	temp = host1x_hypervisor_readl(pdev,
			host1x_thost_common_icg_en_override_0_r());
	host1x_hypervisor_writel(pdev,
			host1x_thost_common_icg_en_override_0_r(), 0x1);

	host1x_hypervisor_writel(pdev, host1x_sync_cfpeek_ctrl_r(),
			host1x_sync_cfpeek_ctrl_cfpeek_ena_f(1)
			| host1x_sync_cfpeek_ctrl_cfpeek_channr_f(chid));
	wmb();

	val = host1x_channel_readl(ch, host1x_channel_fifostat_r());
	if (host1x_channel_fifostat_cfempty_v(val)) {
		host1x_hypervisor_writel(pdev, host1x_sync_cfpeek_ctrl_r(), 0x0);
		nvhost_debug_output(o, "FIFOSTAT %08x\n[empty]\n",
				val);
		return;
	}

	val = host1x_hypervisor_readl(pdev, host1x_sync_cfpeek_ptrs_r());
	rd_ptr = host1x_sync_cfpeek_ptrs_cf_rd_ptr_v(val);
	wr_ptr = host1x_sync_cfpeek_ptrs_cf_wr_ptr_v(val);

	val = host1x_hypervisor_readl(pdev,
			host1x_sync_cf0_setup_r() + 4 * chid);
	start = host1x_sync_cf0_setup_cf0_base_v(val);
	end = host1x_sync_cf0_setup_cf0_limit_v(val);

	nvhost_debug_output(o, "FIFOSTAT %08x, %03x - %03x, RD %03x, WR %03x\n",
			val, start, end, rd_ptr, wr_ptr);
	do {
		host1x_hypervisor_writel(pdev, host1x_sync_cfpeek_ctrl_r(),
			host1x_sync_cfpeek_ctrl_cfpeek_ena_f(1)
			       | host1x_sync_cfpeek_ctrl_cfpeek_channr_f(chid)
			       | host1x_sync_cfpeek_ctrl_cfpeek_addr_f(rd_ptr));
		wmb();
		val = host1x_hypervisor_readl(pdev,
				host1x_sync_cfpeek_read_r());
		rmb();

		nvhost_debug_output(o, "%08x ", val);

		if (rd_ptr == end)
			rd_ptr = start;
		else
			rd_ptr++;

		max--;
	} while (max && rd_ptr != wr_ptr);

	host1x_hypervisor_writel(pdev,
			host1x_thost_common_icg_en_override_0_r(), temp);

	nvhost_debug_output(o, "\n");
}

static void debug_show_mlocks(struct nvhost_master *m, struct output *o)
{
	struct nvhost_syncpt *sp = &m->syncpt;
	unsigned int idx;
	bool cpu, ch;
	unsigned int chid;

	nvhost_debug_output(o, "---- mlocks ----\n");
	for (idx = 0; idx < NV_HOST1X_NB_MLOCKS; idx++) {
		syncpt_op().mutex_owner(sp, idx, &cpu, &ch, &chid);
		if (ch)
			nvhost_debug_output(o, "%d: locked by channel %d\n",
					    idx, chid);
	}
	nvhost_debug_output(o, "\n");
}

static const struct nvhost_debug_ops host1x_debug_ops = {
	.show_channel_cdma = debug_show_channel_cdma,
	.show_channel_fifo = debug_show_channel_fifo,
	.show_mlocks = debug_show_mlocks,
};
