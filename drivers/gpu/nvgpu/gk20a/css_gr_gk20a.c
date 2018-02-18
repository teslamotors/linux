/*
 * GK20A Cycle stats snapshots support (subsystem for gr_gk20a).
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

#include "gk20a.h"
#include "hw_perf_gk20a.h"
#include "hw_mc_gk20a.h"



/* cycle stats fifo header (must match NvSnapshotBufferFifo) */
struct gk20a_cs_snapshot_fifo {
	/* layout description of the buffer */
	u32	start;
	u32	end;

	/* snafu bits */
	u32	hw_overflow_events_occured;
	u32	sw_overflow_events_occured;

	/* the kernel copies new entries to put and
	 * increment the put++. if put == get then
	 * overflowEventsOccured++
	 */
	u32	put;
	u32	_reserved10;
	u32	_reserved11;
	u32	_reserved12;

	/* the driver/client reads from get until
	 * put==get, get++ */
	u32	get;
	u32	_reserved20;
	u32	_reserved21;
	u32	_reserved22;

	/* unused */
	u32	_reserved30;
	u32	_reserved31;
	u32	_reserved32;
	u32	_reserved33;
};

/* cycle stats fifo entry (must match NvSnapshotBufferFifoEntry) */
struct gk20a_cs_snapshot_fifo_entry {
	/* global 48 timestamp */
	u32	timestamp31_00:32;
	u32	timestamp39_32:8;

	/* id of perfmon, should correlate with CSS_MAX_PERFMON_IDS */
	u32	perfmon_id:8;

	/* typically samples_counter is wired to #pmtrigger count */
	u32	samples_counter:12;

	/* DS=Delay Sample, SZ=Size (0=32B, 1=16B) */
	u32	ds:1;
	u32	sz:1;
	u32	zero0:1;
	u32	zero1:1;

	/* counter results */
	u32	event_cnt:32;
	u32	trigger0_cnt:32;
	u32	trigger1_cnt:32;
	u32	sample_cnt:32;

	/* Local PmTrigger results for Maxwell+ or padding otherwise */
	u16	local_trigger_b_count:16;
	u16	book_mark_b:16;
	u16	local_trigger_a_count:16;
	u16	book_mark_a:16;
};


/* cycle stats snapshot client data (e.g. associated with channel) */
struct gk20a_cs_snapshot_client {
	struct list_head	list;
	u32			dmabuf_fd;
	struct dma_buf		*dma_handler;
	struct gk20a_cs_snapshot_fifo	*snapshot;
	u32			snapshot_size;
	u32			perfmon_start;
	u32			perfmon_count;
};

/* check client for pointed perfmon ownership */
#define CONTAINS_PERFMON(cl, pm)				\
		((cl)->perfmon_start <= (pm) &&			\
		((pm) - (cl)->perfmon_start) < (cl)->perfmon_count)

/* the minimal size of HW buffer - should be enough to avoid HW overflows */
#define CSS_MIN_HW_SNAPSHOT_SIZE	(8 * 1024 * 1024)

/* the minimal size of client buffer */
#define CSS_MIN_CLIENT_SNAPSHOT_SIZE				\
		(sizeof(struct gk20a_cs_snapshot_fifo) +	\
		sizeof(struct gk20a_cs_snapshot_fifo_entry) * 256)

/* address of fifo entry by offset */
#define CSS_FIFO_ENTRY(fifo, offs)				\
	((struct gk20a_cs_snapshot_fifo_entry *)(((char *)(fifo)) + (offs)))

/* calculate area capacity in number of fifo entries */
#define CSS_FIFO_ENTRY_CAPACITY(s)				\
	(((s) - sizeof(struct gk20a_cs_snapshot_fifo))		\
		/ sizeof(struct gk20a_cs_snapshot_fifo_entry))

/* reserved to indicate failures with data */
#define CSS_FIRST_PERFMON_ID	32
/* should correlate with size of gk20a_cs_snapshot_fifo_entry::perfmon_id */
#define CSS_MAX_PERFMON_IDS	256

/* local definitions to avoid hardcodes sizes and shifts */
#define PM_BITMAP_SIZE	DIV_ROUND_UP(CSS_MAX_PERFMON_IDS, BITS_PER_LONG)

/* cycle stats snapshot control structure for one HW entry and many clients */
struct gk20a_cs_snapshot {
	unsigned long perfmon_ids[PM_BITMAP_SIZE];
	struct list_head	clients;
	struct mem_desc		hw_memdesc;
	/* pointer to allocated cpu_va memory where GPU place data */
	struct gk20a_cs_snapshot_fifo_entry	*hw_snapshot;
	struct gk20a_cs_snapshot_fifo_entry	*hw_end;
	struct gk20a_cs_snapshot_fifo_entry	*hw_get;
};

/* reports whether the hw queue overflowed */
static inline bool css_hw_get_overflow_status(struct gk20a *g)
{
	const u32 st = perf_pmasys_control_membuf_status_overflowed_f();
	return st == (gk20a_readl(g, perf_pmasys_control_r()) & st);
}

/* returns how many pending snapshot entries are pending */
static inline u32 css_hw_get_pending_snapshots(struct gk20a *g)
{
	return gk20a_readl(g, perf_pmasys_mem_bytes_r()) /
			sizeof(struct gk20a_cs_snapshot_fifo_entry);
}

/* informs hw how many snapshots have been processed (frees up fifo space) */
static inline void css_hw_set_handled_snapshots(struct gk20a *g, u32 done)
{
	if (done > 0) {
		gk20a_writel(g, perf_pmasys_mem_bump_r(),
		     done * sizeof(struct gk20a_cs_snapshot_fifo_entry));
	}
}

/* disable streaming to memory */
static void css_hw_reset_streaming(struct gk20a *g)
{
	u32 engine_status;
	u32 old_pmc = gk20a_readl(g, mc_enable_r());

	/* reset the perfmon */
	gk20a_writel(g, mc_enable_r(),
				old_pmc & ~mc_enable_perfmon_enabled_f());
	gk20a_writel(g, mc_enable_r(), old_pmc);

	/* RBUFEMPTY must be set -- otherwise we'll pick up */
	/* snapshot that have been queued up from earlier   */
	engine_status = gk20a_readl(g, perf_pmasys_enginestatus_r());
	WARN_ON(0 == (engine_status
			& perf_pmasys_enginestatus_rbufempty_empty_f()));

	/* turn off writes */
	gk20a_writel(g, perf_pmasys_control_r(),
			perf_pmasys_control_membuf_clear_status_doit_f());

	/* pointing all pending snapshots as handled */
	css_hw_set_handled_snapshots(g, css_hw_get_pending_snapshots(g));
}

/*
 * WARNING: all css_gr_XXX functions are local and expected to be called
 * from locked context (protected by cs_lock)
 */

static int css_gr_create_shared_data(struct gr_gk20a *gr)
{
	struct gk20a_cs_snapshot *data;

	if (gr->cs_data)
		return 0;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_LIST_HEAD(&data->clients);
	gr->cs_data = data;

	return 0;
}

static int css_hw_enable_snapshot(struct gr_gk20a *gr, u32 snapshot_size)
{
	struct gk20a *g = gr->g;
	struct gk20a_cs_snapshot *data = gr->cs_data;
	int ret;

	u32 virt_addr_lo;
	u32 virt_addr_hi;
	u32 inst_pa_page;

	if (data->hw_snapshot)
		return 0;

	if (snapshot_size < CSS_MIN_HW_SNAPSHOT_SIZE)
		snapshot_size = CSS_MIN_HW_SNAPSHOT_SIZE;

	ret = gk20a_gmmu_alloc_map_sys(&g->mm.pmu.vm, snapshot_size,
							&data->hw_memdesc);
	if (ret)
		return ret;

	/* perf output buffer may not cross a 4GB boundary - with a separate */
	/* va smaller than that, it won't but check anyway */
	if (!data->hw_memdesc.cpu_va ||
		data->hw_memdesc.size < snapshot_size ||
		data->hw_memdesc.gpu_va + u64_lo32(snapshot_size) > SZ_4G) {
		ret = -EFAULT;
		goto failed_allocation;
	}

	data->hw_snapshot =
		(struct gk20a_cs_snapshot_fifo_entry *)data->hw_memdesc.cpu_va;
	data->hw_end = data->hw_snapshot +
		snapshot_size / sizeof(struct gk20a_cs_snapshot_fifo_entry);
	data->hw_get = data->hw_snapshot;
	memset(data->hw_snapshot, 0xff, snapshot_size);

	/* address and size are aligned to 32 bytes, the lowest bits read back
	 * as zeros */
	virt_addr_lo = u64_lo32(data->hw_memdesc.gpu_va);
	virt_addr_hi = u64_hi32(data->hw_memdesc.gpu_va);

	css_hw_reset_streaming(g);

	gk20a_writel(g, perf_pmasys_outbase_r(), virt_addr_lo);
	gk20a_writel(g, perf_pmasys_outbaseupper_r(),
			perf_pmasys_outbaseupper_ptr_f(virt_addr_hi));
	gk20a_writel(g, perf_pmasys_outsize_r(), snapshot_size);

	/* this field is aligned to 4K */
	inst_pa_page = gk20a_mm_inst_block_addr(g, &g->mm.hwpm.inst_block) >> 12;

	/* A write to MEM_BLOCK triggers the block bind operation. MEM_BLOCK
	 * should be written last */
	gk20a_writel(g, perf_pmasys_mem_block_r(),
			perf_pmasys_mem_block_base_f(inst_pa_page) |
			perf_pmasys_mem_block_valid_true_f() |
			perf_pmasys_mem_block_target_lfb_f());

	gk20a_dbg_info("cyclestats: buffer for hardware snapshots enabled\n");

	return 0;

failed_allocation:
	if (data->hw_memdesc.size) {
		gk20a_gmmu_unmap_free(&g->mm.pmu.vm, &data->hw_memdesc);
		memset(&data->hw_memdesc, 0, sizeof(data->hw_memdesc));
	}
	data->hw_snapshot = NULL;

	return ret;
}

static void css_hw_disable_snapshot(struct gr_gk20a *gr)
{
	struct gk20a *g = gr->g;
	struct gk20a_cs_snapshot *data = gr->cs_data;

	if (!data->hw_snapshot)
		return;

	css_hw_reset_streaming(g);

	gk20a_writel(g, perf_pmasys_outbase_r(), 0);
	gk20a_writel(g, perf_pmasys_outbaseupper_r(),
			perf_pmasys_outbaseupper_ptr_f(0));
	gk20a_writel(g, perf_pmasys_outsize_r(), 0);

	gk20a_writel(g, perf_pmasys_mem_block_r(),
			perf_pmasys_mem_block_base_f(0) |
			perf_pmasys_mem_block_valid_false_f() |
			perf_pmasys_mem_block_target_f(0));

	gk20a_gmmu_unmap_free(&g->mm.pmu.vm, &data->hw_memdesc);
	memset(&data->hw_memdesc, 0, sizeof(data->hw_memdesc));
	data->hw_snapshot = NULL;

	gk20a_dbg_info("cyclestats: buffer for hardware snapshots disabled\n");
}

static void css_gr_free_shared_data(struct gr_gk20a *gr)
{
	if (gr->cs_data) {
		/* the clients list is expected to be empty */
		css_hw_disable_snapshot(gr);

		/* release the objects */
		kfree(gr->cs_data);
		gr->cs_data = NULL;
	}
}


static struct gk20a_cs_snapshot_client*
css_gr_search_client(struct list_head *clients, u32 perfmon)
{
	struct list_head *pos;

	list_for_each(pos, clients) {
		struct gk20a_cs_snapshot_client *client =
			container_of(pos,
				struct gk20a_cs_snapshot_client, list);
		if (CONTAINS_PERFMON(client, perfmon))
			return client;
	}

	return NULL;
}

static int css_gr_flush_snapshots(struct gr_gk20a *gr)
{
	struct gk20a *g = gr->g;
	struct gk20a_cs_snapshot *css = gr->cs_data;
	struct gk20a_cs_snapshot_client *cur;
	u32 pending;

	/* variables for iterating over HW entries */
	u32 sid;
	struct gk20a_cs_snapshot_fifo_entry *src;

	/* due to data sharing with userspace we allowed update only */
	/* overflows and put field in the fifo header                */
	struct gk20a_cs_snapshot_fifo *dst;
	struct gk20a_cs_snapshot_fifo_entry *dst_get;
	struct gk20a_cs_snapshot_fifo_entry *dst_put;
	struct gk20a_cs_snapshot_fifo_entry *dst_head;
	struct gk20a_cs_snapshot_fifo_entry *dst_tail;

	if (!css)
		return -EINVAL;

	if (!css->hw_snapshot)
		return -EINVAL;

	if (list_empty(&css->clients))
		return -EBADF;

	/* check data available */
	pending = css_hw_get_pending_snapshots(g);
	if (!pending)
		return 0;

	if (css_hw_get_overflow_status(g)) {
		struct list_head *pos;

		list_for_each(pos, &css->clients) {
			cur = container_of(pos,
				struct gk20a_cs_snapshot_client, list);
			cur->snapshot->hw_overflow_events_occured++;
		}

		gk20a_warn(dev_from_gk20a(g),
			   "cyclestats: hardware overflow detected\n");
	}

	/* proceed all items in HW buffer */
	sid = 0;
	cur = NULL;
	dst = NULL;
	dst_put = NULL;
	src = css->hw_get;

	/* proceed all completed records */
	while (sid < pending && 0 == src->zero0) {
		/* we may have a new perfmon_id which required to */
		/* switch to a new client -> let's forget current */
		if (cur && !CONTAINS_PERFMON(cur, src->perfmon_id)) {
			dst->put = (char *)dst_put - (char *)dst;
			dst = NULL;
			cur = NULL;
		}

		/* now we have to select a new current client         */
		/* the client selection rate depends from experiment  */
		/* activity but on Android usually happened 1-2 times */
		if (!cur) {
			cur = css_gr_search_client(&css->clients,
							src->perfmon_id);
			if (cur) {
				/* found - setup all required data */
				dst = cur->snapshot;
				dst_get = CSS_FIFO_ENTRY(dst, dst->get);
				dst_put = CSS_FIFO_ENTRY(dst, dst->put);
				dst_head = CSS_FIFO_ENTRY(dst, dst->start);
				dst_tail = CSS_FIFO_ENTRY(dst, dst->end) - 1;
			} else {
				/* client not found - skipping this entry */
				gk20a_warn(dev_from_gk20a(g),
					   "cyclestats: orphaned perfmon %u\n",
							src->perfmon_id);
				goto next_hw_fifo_entry;
			}
		}

		/* check for software overflows */
		if (dst_put + 1 == dst_get ||
			(dst_put == dst_tail && dst_get == dst_head)) {
			/* no data copy, no pointer updates */
			dst->sw_overflow_events_occured++;
			gk20a_warn(dev_from_gk20a(g),
				   "cyclestats: perfmon %u soft overflow\n",
							src->perfmon_id);
		} else {
			*dst_put = *src;
			if (dst_put == dst_tail)
				dst_put = dst_head;
			else
				dst_put++;
		}

next_hw_fifo_entry:
		sid++;
		if (++src >= css->hw_end)
			src = css->hw_snapshot;
	}

	/* update client put pointer if necessary */
	if (cur && dst)
		dst->put = (char *)dst_put - (char *)dst;

	/* re-set HW buffer after processing taking wrapping into account */
	if (css->hw_get < src) {
		memset(css->hw_get, 0xff, (src - css->hw_get) * sizeof(*src));
	} else {
		memset(css->hw_snapshot, 0xff,
				(src - css->hw_snapshot) * sizeof(*src));
		memset(css->hw_get, 0xff,
				(css->hw_end - css->hw_get) * sizeof(*src));
	}
	gr->cs_data->hw_get = src;
	css_hw_set_handled_snapshots(g, sid);
	if (pending != sid) {
		/* not all entries proceed correctly. some of problems */
		/* reported as overflows, some as orphaned perfmons,   */
		/* but it will be better notify with summary about it  */
		gk20a_warn(dev_from_gk20a(g),
			   "cyclestats: done %u from %u entries\n",
							sid, pending);
	}

	return 0;
}

static u32 css_gr_allocate_perfmon_ids(struct gk20a_cs_snapshot *data,
				       u32 count)
{
	unsigned long *pids = data->perfmon_ids;
	unsigned int f;

	f = bitmap_find_next_zero_area(pids, CSS_MAX_PERFMON_IDS,
				       CSS_FIRST_PERFMON_ID, count, 0);
	if (f > CSS_MAX_PERFMON_IDS)
		f = 0;
	else
		bitmap_set(pids, f, count);

	return f;
}

static u32 css_gr_release_perfmon_ids(struct gk20a_cs_snapshot *data,
				      u32 start,
				      u32 count)
{
	unsigned long *pids = data->perfmon_ids;
	u32  end = start + count;
	u32  cnt = 0;

	if (start >= CSS_FIRST_PERFMON_ID && end <= CSS_MAX_PERFMON_IDS) {
		bitmap_clear(pids, start, count);
		cnt = count;
	}

	return cnt;
}


static int css_gr_free_client_data(struct gk20a_cs_snapshot *data,
				struct gk20a_cs_snapshot_client *client)
{
	int ret = 0;

	if (client->list.next && client->list.prev)
		list_del(&client->list);

	if (client->perfmon_start && client->perfmon_count) {
		if (client->perfmon_count != css_gr_release_perfmon_ids(data,
				client->perfmon_start, client->perfmon_count))
			ret = -EINVAL;
	}

	if (client->dma_handler) {
		if (client->snapshot)
			dma_buf_vunmap(client->dma_handler, client->snapshot);
		dma_buf_put(client->dma_handler);
	}

	kfree(client);

	return ret;
}

static int css_gr_create_client_data(struct gk20a_cs_snapshot *data,
			u32 dmabuf_fd, u32 perfmon_count,
			struct gk20a_cs_snapshot_client **client)
{
	struct gk20a_cs_snapshot_client *cur;
	int ret = 0;

	cur = kzalloc(sizeof(*cur), GFP_KERNEL);
	if (!cur) {
		ret = -ENOMEM;
		goto failed;
	}

	cur->dmabuf_fd   = dmabuf_fd;
	cur->dma_handler = dma_buf_get(cur->dmabuf_fd);
	if (IS_ERR(cur->dma_handler)) {
		ret = PTR_ERR(cur->dma_handler);
		cur->dma_handler = NULL;
		goto failed;
	}

	cur->snapshot = (struct gk20a_cs_snapshot_fifo *)
					dma_buf_vmap(cur->dma_handler);
	if (!cur->snapshot) {
		ret = -ENOMEM;
		goto failed;
	}

	cur->snapshot_size = cur->dma_handler->size;
	if (cur->snapshot_size < CSS_MIN_CLIENT_SNAPSHOT_SIZE) {
		ret = -ENOMEM;
		goto failed;
	}

	memset(cur->snapshot, 0, sizeof(*cur->snapshot));
	cur->snapshot->start = sizeof(*cur->snapshot);
	/* we should be ensure that can fit all fifo entries here */
	cur->snapshot->end =
		CSS_FIFO_ENTRY_CAPACITY(cur->snapshot_size)
			* sizeof(struct gk20a_cs_snapshot_fifo_entry)
			+ sizeof(struct gk20a_cs_snapshot_fifo);
	cur->snapshot->get = cur->snapshot->start;
	cur->snapshot->put = cur->snapshot->start;

	cur->perfmon_count = perfmon_count;
	if (cur->perfmon_count) {
		cur->perfmon_start = css_gr_allocate_perfmon_ids(data,
							cur->perfmon_count);
		if (!cur->perfmon_start) {
			ret = -ENOENT;
			goto failed;
		}
	}

	list_add_tail(&cur->list, &data->clients);
	*client = cur;

	return 0;

failed:
	*client = NULL;
	if (cur)
		css_gr_free_client_data(data, cur);

	return ret;
}


int gr_gk20a_css_attach(struct gk20a *g,
			u32 dmabuf_fd,
			u32 perfmon_count,
			u32 *perfmon_start,
			struct gk20a_cs_snapshot_client **cs_client)
{
	int ret = 0;
	struct gr_gk20a *gr;

	/* we must have a placeholder to store pointer to client structure */
	if (!cs_client)
		return -EINVAL;

	if (!perfmon_count ||
	    perfmon_count > CSS_MAX_PERFMON_IDS - CSS_FIRST_PERFMON_ID)
		return -EINVAL;

	gr = &g->gr;
	*cs_client = NULL;

	mutex_lock(&gr->cs_lock);

	ret = css_gr_create_shared_data(gr);
	if (ret)
		goto failed;

	ret = css_gr_create_client_data(gr->cs_data,
				     dmabuf_fd,
				     perfmon_count,
				     cs_client);
	if (ret)
		goto failed;

	ret = css_hw_enable_snapshot(gr, (*cs_client)->snapshot_size);
	if (ret)
		goto failed;

	if (perfmon_start)
		*perfmon_start = (*cs_client)->perfmon_start;

	mutex_unlock(&gr->cs_lock);

	return 0;

failed:
	if (gr->cs_data) {
		if (*cs_client) {
			css_gr_free_client_data(gr->cs_data, *cs_client);
			*cs_client = NULL;
		}

		if (list_empty(&gr->cs_data->clients))
			css_gr_free_shared_data(gr);
	}
	mutex_unlock(&gr->cs_lock);

	if (perfmon_start)
		*perfmon_start = 0;

	return ret;
}

int gr_gk20a_css_detach(struct gk20a *g,
				struct gk20a_cs_snapshot_client *cs_client)
{
	int ret = 0;
	struct gr_gk20a *gr;

	if (!cs_client)
		return -EINVAL;

	gr = &g->gr;
	mutex_lock(&gr->cs_lock);
	if (gr->cs_data) {
		struct gk20a_cs_snapshot *data = gr->cs_data;

		ret = css_gr_free_client_data(data, cs_client);
		if (list_empty(&data->clients))
			css_gr_free_shared_data(gr);
	} else {
		ret = -EBADF;
	}
	mutex_unlock(&gr->cs_lock);

	return ret;
}

int gr_gk20a_css_flush(struct gk20a *g,
				struct gk20a_cs_snapshot_client *cs_client)
{
	int ret = 0;
	struct gr_gk20a *gr;

	if (!cs_client)
		return -EINVAL;

	gr = &g->gr;
	mutex_lock(&gr->cs_lock);
	ret = css_gr_flush_snapshots(gr);
	mutex_unlock(&gr->cs_lock);

	return ret;
}

/* helper function with locking to cleanup snapshot code code in gr_gk20a.c */
void gr_gk20a_free_cyclestats_snapshot_data(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;

	mutex_lock(&gr->cs_lock);
	css_gr_free_shared_data(gr);
	mutex_unlock(&gr->cs_lock);
	mutex_destroy(&gr->cs_lock);
}
