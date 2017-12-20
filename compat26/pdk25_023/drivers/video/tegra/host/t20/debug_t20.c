/*
 * drivers/video/tegra/host/t20/debug_t20.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include <asm/io.h>

#include "../dev.h"
#include "../debug.h"
#include "../nvhost_cdma.h"
#include "../../nvmap/nvmap.h"

#include "hardware_t20.h"

#define NVHOST_DEBUG_MAX_PAGE_OFFSET 102400

enum {
	NVHOST_DBG_STATE_CMD = 0,
	NVHOST_DBG_STATE_DATA = 1,
	NVHOST_DBG_STATE_GATHER = 2
};

static int show_channel_command(struct output *o, u32 addr, u32 val, int *count)
{
	unsigned mask;
	unsigned subop;

	switch (val >> 28) {
	case 0x0:
		mask = val & 0x3f;
		if (mask) {
			nvhost_debug_output(o, "SETCL(class=%03x, offset=%03x, mask=%02x, [",
				   val >> 6 & 0x3ff, val >> 16 & 0xfff, mask);
			*count = hweight8(mask);
			return NVHOST_DBG_STATE_DATA;
		} else {
			nvhost_debug_output(o, "SETCL(class=%03x)\n", val >> 6 & 0x3ff);
			return NVHOST_DBG_STATE_CMD;
		}

	case 0x1:
		nvhost_debug_output(o, "INCR(offset=%03x, [", val >> 16 & 0xfff);
		*count = val & 0xffff;
		return NVHOST_DBG_STATE_DATA;

	case 0x2:
		nvhost_debug_output(o, "NONINCR(offset=%03x, [", val >> 16 & 0xfff);
		*count = val & 0xffff;
		return NVHOST_DBG_STATE_DATA;

	case 0x3:
		mask = val & 0xffff;
		nvhost_debug_output(o, "MASK(offset=%03x, mask=%03x, [",
			   val >> 16 & 0xfff, mask);
		*count = hweight16(mask);
		return NVHOST_DBG_STATE_DATA;

	case 0x4:
		nvhost_debug_output(o, "IMM(offset=%03x, data=%03x)\n",
			   val >> 16 & 0xfff, val & 0xffff);
		return NVHOST_DBG_STATE_CMD;

	case 0x5:
		nvhost_debug_output(o, "RESTART(offset=%08x)\n", val << 4);
		return NVHOST_DBG_STATE_CMD;

	case 0x6:
		nvhost_debug_output(o, "GATHER(offset=%03x, insert=%d, type=%d, count=%04x, addr=[",
			   val >> 16 & 0xfff, val >> 15 & 0x1, val >> 14 & 0x1,
			   val & 0x3fff);
		*count = val & 0x3fff; // TODO: insert
		return NVHOST_DBG_STATE_GATHER;

	case 0xe:
		subop = val >> 24 & 0xf;
		if (subop == 0)
			nvhost_debug_output(o, "ACQUIRE_MLOCK(index=%d)\n", val & 0xff);
		else if (subop == 1)
			nvhost_debug_output(o, "RELEASE_MLOCK(index=%d)\n", val & 0xff);
		else
			nvhost_debug_output(o, "EXTEND_UNKNOWN(%08x)\n", val);
		return NVHOST_DBG_STATE_CMD;

	default:
		return NVHOST_DBG_STATE_CMD;
	}
}

static void show_channel_gather(struct output *o, u32 addr,
		phys_addr_t phys_addr,
		u32 words, struct nvhost_cdma *cdma);

static void show_channel_word(struct output *o, int *state, int *count,
		u32 addr, u32 val, struct nvhost_cdma *cdma)
{
	static int start_count, dont_print;

	switch (*state) {
	case NVHOST_DBG_STATE_CMD:
		if (addr)
			nvhost_debug_output(o, "%08x: %08x:", addr, val);
		else
			nvhost_debug_output(o, "%08x:", val);

		*state = show_channel_command(o, addr, val, count);
		dont_print = 0;
		start_count = *count;
		if (*state == NVHOST_DBG_STATE_DATA && *count == 0) {
			*state = NVHOST_DBG_STATE_CMD;
			nvhost_debug_output(o, "])\n");
		}
		break;

	case NVHOST_DBG_STATE_DATA:
		(*count)--;
		if (start_count - *count < 64)
			nvhost_debug_output(o, "%08x%s",
				val, *count > 0 ? ", " : "])\n");
		else if (!dont_print && (*count > 0)) {
			nvhost_debug_output(o, "[truncated; %d more words]\n",
				*count);
			dont_print = 1;
		}
		if (*count == 0)
			*state = NVHOST_DBG_STATE_CMD;
		break;

	case NVHOST_DBG_STATE_GATHER:
		*state = NVHOST_DBG_STATE_CMD;
		nvhost_debug_output(o, "%08x]):\n", val);
		if (cdma) {
			show_channel_gather(o, addr, val,
					*count, cdma);
		}
		break;
	}
}

static void show_channel_gather(struct output *o, u32 addr,
		phys_addr_t phys_addr,
		u32 words, struct nvhost_cdma *cdma)
{
	/* Map dmaget cursor to corresponding nvmap_handle */
	struct push_buffer *pb = &cdma->push_buffer;
	u32 cur = addr - pb->phys;
	struct nvmap_client_handle *nvmap = &pb->nvmap[cur/8];
	struct nvmap_handle_ref ref;
	u32 *map_addr, offset;
	phys_addr_t pin_addr;
	int state, count, i;

	if ((u32)nvmap->handle == NVHOST_CDMA_PUSH_GATHER_CTXSAVE) {
		nvhost_debug_output(o, "[context save]\n");
		return;
	}

	if (!nvmap->handle || !nvmap->client
			|| atomic_read(&nvmap->handle->ref) < 1) {
		nvhost_debug_output(o, "[already deallocated]\n");
		return;
	}

	/* Create a fake nvmap_handle_ref - nvmap requires it
	 * but accesses only the first field - nvmap_handle */
	ref.handle = nvmap->handle;

	map_addr = nvmap_mmap(&ref);
	if (!map_addr) {
		nvhost_debug_output(o, "[could not mmap]\n");
		return;
	}

	/* Get base address from nvmap */
	pin_addr = nvmap_pin(nvmap->client, &ref);
	if (IS_ERR_VALUE(pin_addr)) {
		nvhost_debug_output(o, "[couldn't pin]\n");
		nvmap_munmap(&ref, map_addr);
		return;
	}

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
			show_channel_word(o, &state, &count,
					phys_addr + i * 4,
					*(map_addr + offset/4 + i),
					cdma);
	}
	nvmap_unpin(nvmap->client, &ref);
	nvmap_munmap(&ref, map_addr);
}

static void show_channel_pair(struct output *o, u32 addr,
		u32 w0, u32 w1, struct nvhost_cdma *cdma)
{
	int state = NVHOST_DBG_STATE_CMD;
	int count;

	show_channel_word(o, &state, &count, addr, w0, cdma);
	show_channel_word(o, &state, &count, addr+4, w1, cdma);
}

/**
 * Retrieve the op pair at a slot offset from a DMA address
 */
static void cdma_peek(struct nvhost_cdma *cdma,
		      u32 dmaget, int slot, u32 *out)
{
	u32 offset = dmaget - cdma->push_buffer.phys;
	u32 *p = cdma->push_buffer.mapped;

	offset = ((offset + slot * 8) & (PUSH_BUFFER_SIZE - 1)) >> 2;
	out[0] = p[offset];
	out[1] = p[offset + 1];
}

u32 previous_oppair(struct nvhost_cdma *cdma, u32 cur)
{
	u32 pb = cdma->push_buffer.phys;
	u32 prev = cur-8;
	if (prev < pb)
		prev += PUSH_BUFFER_SIZE;
	return prev;
}

static void t20_debug_show_channel_cdma(struct nvhost_master *m,
					struct output *o, int chid)
{
	struct nvhost_channel *channel = m->channels + chid;
	struct nvhost_cdma *cdma = &channel->cdma;
	u32 dmaput, dmaget, dmactrl;
	u32 cbstat, cbread;
	u32 val, base, baseval;
	u32 pbw[2];

	dmaput = readl(channel->aperture + HOST1X_CHANNEL_DMAPUT);
	dmaget = readl(channel->aperture + HOST1X_CHANNEL_DMAGET);
	dmactrl = readl(channel->aperture + HOST1X_CHANNEL_DMACTRL);
	cbread = readl(m->aperture + HOST1X_SYNC_CBREAD(chid));
	cbstat = readl(m->aperture + HOST1X_SYNC_CBSTAT(chid));

	nvhost_debug_output(o, "%d-%s (%d): ", chid,
			    channel->mod.name,
			    atomic_read(&channel->mod.refcount));

	if ((dmactrl & 1) || !channel->cdma.push_buffer.mapped) {
		nvhost_debug_output(o, "inactive\n\n");
		return;
	}

	switch (cbstat) {
	case 0x00010008:
		nvhost_debug_output(o, "waiting on syncpt %d val %d\n",
			cbread >> 24, cbread & 0xffffff);
		break;

	case 0x00010009:
		base = (cbread >> 16) & 0xff;
		val = readl(m->aperture + HOST1X_SYNC_SYNCPT_BASE(base));
		baseval = val & 0xffff;
		val = cbread & 0xffff;
		nvhost_debug_output(o, "waiting on syncpt %d val %d "
			  "(base %d = %d; offset = %d)\n",
			cbread >> 24, baseval + val,
			base, baseval, val);
		break;

	default:
		nvhost_debug_output(o,
				"active class %02x, offset %04x, val %08x\n",
			cbstat >> 16, cbstat & 0xffff, cbread);
		break;
	}

	nvhost_debug_output(o, "DMAPUT %08x, DMAGET %08x, DMACTL %08x\n",
		dmaput, dmaget, dmactrl);
	nvhost_debug_output(o, "CBREAD %08x, CBSTAT %08x\n", cbread, cbstat);

	cdma_peek(cdma, dmaget, -1, pbw);
	show_channel_pair(o, previous_oppair(cdma, dmaget), pbw[0], pbw[1], &channel->cdma);
	nvhost_debug_output(o, "\n");
}

void t20_debug_show_channel_fifo(struct nvhost_master *m,
				 struct output *o, int chid)
{
	u32 val, rd_ptr, wr_ptr, start, end;
	struct nvhost_channel *channel = m->channels + chid;
	int state, count;

	nvhost_debug_output(o, "%d: fifo:\n", chid);

	val = readl(channel->aperture + HOST1X_CHANNEL_FIFOSTAT);
	nvhost_debug_output(o, "FIFOSTAT %08x\n", val);
	if (val & (1 << 10)) {
		nvhost_debug_output(o, "[empty]\n");
		return;
	}

	writel(0x0, m->aperture + HOST1X_SYNC_CFPEEK_CTRL);
	writel((1 << 31) | (chid << 16),
		m->aperture + HOST1X_SYNC_CFPEEK_CTRL);

	val = readl(m->aperture + HOST1X_SYNC_CFPEEK_PTRS);
	rd_ptr = val & 0x1ff;
	wr_ptr = (val >> 16) & 0x1ff;

	val = readl(m->aperture + HOST1X_SYNC_CF_SETUP(chid));
	start = val & 0x1ff;
	end = (val >> 16) & 0x1ff;

	state = NVHOST_DBG_STATE_CMD;

	do {
		writel(0x0, m->aperture + HOST1X_SYNC_CFPEEK_CTRL);
		writel((1 << 31) | (chid << 16) | rd_ptr,
			m->aperture + HOST1X_SYNC_CFPEEK_CTRL);
		val = readl(m->aperture + HOST1X_SYNC_CFPEEK_READ);

		show_channel_word(o, &state, &count, 0, val, NULL);

		if (rd_ptr == end)
			rd_ptr = start;
		else
			rd_ptr++;
	} while (rd_ptr != wr_ptr);

	if (state == NVHOST_DBG_STATE_DATA)
		nvhost_debug_output(o, ", ...])\n");
	nvhost_debug_output(o, "\n");

	writel(0x0, m->aperture + HOST1X_SYNC_CFPEEK_CTRL);
}

static void t20_debug_show_mlocks(struct nvhost_master *m, struct output *o)
{
	u32 __iomem *mlo_regs = m->sync_aperture + HOST1X_SYNC_MLOCK_OWNER_0;
	int i;

	nvhost_debug_output(o, "---- mlocks ----\n");
	for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++) {
		u32 owner = readl(mlo_regs + i);
		if (owner & 0x1)
			nvhost_debug_output(o, "%d: locked by channel %d\n",
				i, (owner >> 8) & 0xf);
		else if (owner & 0x2)
			nvhost_debug_output(o, "%d: locked by cpu\n", i);
		else
			nvhost_debug_output(o, "%d: unlocked\n", i);
	}
	nvhost_debug_output(o, "\n");
}

int nvhost_init_t20_debug_support(struct nvhost_master *host)
{
	host->op.debug.show_channel_cdma = t20_debug_show_channel_cdma;
	host->op.debug.show_channel_fifo = t20_debug_show_channel_fifo;
	host->op.debug.show_mlocks = t20_debug_show_mlocks;

	return 0;
}
