/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <asm/barrier.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/nvgpu.h>
#include <linux/hashtable.h>
#include <linux/debugfs.h>
#include <linux/log2.h>
#include <uapi/linux/nvgpu.h>
#include "ctxsw_trace_gk20a.h"
#include "fecs_trace_gk20a.h"
#include "gk20a.h"
#include "gr_gk20a.h"
#include "hw_ctxsw_prog_gk20a.h"
#include "hw_gr_gk20a.h"

/*
 * If HW circular buffer is getting too many "buffer full" conditions,
 * increasing this constant should help (it drives Linux' internal buffer size).
 */
#define GK20A_FECS_TRACE_NUM_RECORDS		(1 << 6)
#define GK20A_FECS_TRACE_HASH_BITS		8 /* 2^8 */
#define GK20A_FECS_TRACE_FRAME_PERIOD_NS	(1000000000ULL/60ULL)
#define GK20A_FECS_TRACE_PTIMER_SHIFT		5

struct gk20a_fecs_trace_record {
	u32 magic_lo;
	u32 magic_hi;
	u32 context_id;
	u32 context_ptr;
	u32 new_context_id;
	u32 new_context_ptr;
	u64 ts[];
};

struct gk20a_fecs_trace_hash_ent {
	u32 context_ptr;
	pid_t pid;
	struct hlist_node node;
};

struct gk20a_fecs_trace {

	struct mem_desc trace_buf;
	DECLARE_HASHTABLE(pid_hash_table, GK20A_FECS_TRACE_HASH_BITS);
	struct mutex hash_lock;
	struct mutex poll_lock;
	struct task_struct *poll_task;
};

#ifdef CONFIG_GK20A_CTXSW_TRACE
static inline u32 gk20a_fecs_trace_record_ts_tag_v(u64 ts)
{
	return ctxsw_prog_record_timestamp_timestamp_hi_tag_v((u32) (ts >> 32));
}

static inline u64 gk20a_fecs_trace_record_ts_timestamp_v(u64 ts)
{
	return ts & ~(((u64)ctxsw_prog_record_timestamp_timestamp_hi_tag_m()) << 32);
}


static u32 gk20a_fecs_trace_fecs_context_ptr(struct channel_gk20a *ch)
{
	return (u32) (sg_phys(ch->inst_block.sgt->sgl) >> 12LL);
}

static inline int gk20a_fecs_trace_num_ts(void)
{
	return (ctxsw_prog_record_timestamp_record_size_in_bytes_v()
		- sizeof(struct gk20a_fecs_trace_record)) / sizeof(u64);
}

struct gk20a_fecs_trace_record *gk20a_fecs_trace_get_record(
	struct gk20a_fecs_trace *trace, int idx)
{
	return (struct gk20a_fecs_trace_record *)
		((u8 *) trace->trace_buf.cpu_va
		+ (idx * ctxsw_prog_record_timestamp_record_size_in_bytes_v()));
}

static bool gk20a_fecs_trace_is_valid_record(struct gk20a_fecs_trace_record *r)
{
	/*
	 * testing magic_hi should suffice. magic_lo is sometimes used
	 * as a sequence number in experimental ucode.
	 */
	return (r->magic_hi
		== ctxsw_prog_record_timestamp_magic_value_hi_v_value_v());
}

static int gk20a_fecs_trace_get_read_index(struct gk20a *g)
{
	return gr_gk20a_elpg_protected_call(g,
			gk20a_readl(g, gr_fecs_mailbox1_r()));
}

static int gk20a_fecs_trace_get_write_index(struct gk20a *g)
{
	return gr_gk20a_elpg_protected_call(g,
			gk20a_readl(g, gr_fecs_mailbox0_r()));
}

static int gk20a_fecs_trace_set_read_index(struct gk20a *g, int index)
{
	gk20a_dbg(gpu_dbg_ctxsw, "set read=%d", index);
	return gr_gk20a_elpg_protected_call(g,
			(gk20a_writel(g, gr_fecs_mailbox1_r(), index), 0));
}

void gk20a_fecs_trace_hash_dump(struct gk20a *g)
{
	u32 bkt;
	struct gk20a_fecs_trace_hash_ent *ent;
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_dbg(gpu_dbg_ctxsw, "dumping hash table");

	mutex_lock(&trace->hash_lock);
	hash_for_each(trace->pid_hash_table, bkt, ent, node)
	{
		gk20a_dbg(gpu_dbg_ctxsw, " ent=%p bkt=%x context_ptr=%x pid=%d",
			ent, bkt, ent->context_ptr, ent->pid);

	}
	mutex_unlock(&trace->hash_lock);
}

static int gk20a_fecs_trace_hash_add(struct gk20a *g, u32 context_ptr, pid_t pid)
{
	struct gk20a_fecs_trace_hash_ent *he;
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_ctxsw,
		"adding hash entry context_ptr=%x -> pid=%d", context_ptr, pid);

	he = kzalloc(sizeof(*he), GFP_KERNEL);
	if (unlikely(!he)) {
		gk20a_warn(dev_from_gk20a(g),
			"can't alloc new hash entry for context_ptr=%x pid=%d",
			context_ptr, pid);
		return -ENOMEM;
	}

	he->context_ptr = context_ptr;
	he->pid = pid;
	mutex_lock(&trace->hash_lock);
	hash_add(trace->pid_hash_table, &he->node, context_ptr);
	mutex_unlock(&trace->hash_lock);
	return 0;
}

static void gk20a_fecs_trace_hash_del(struct gk20a *g, u32 context_ptr)
{
	struct hlist_node *tmp;
	struct gk20a_fecs_trace_hash_ent *ent;
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_ctxsw,
		"freeing hash entry context_ptr=%x", context_ptr);

	mutex_lock(&trace->hash_lock);
	hash_for_each_possible_safe(trace->pid_hash_table, ent, tmp, node,
		context_ptr) {
		if (ent->context_ptr == context_ptr) {
			hash_del(&ent->node);
			gk20a_dbg(gpu_dbg_ctxsw,
				"freed hash entry=%p context_ptr=%x", ent,
				ent->context_ptr);
			kfree(ent);
			break;
		}
	}
	mutex_unlock(&trace->hash_lock);
}

static void gk20a_fecs_trace_free_hash_table(struct gk20a *g)
{
	u32 bkt;
	struct hlist_node *tmp;
	struct gk20a_fecs_trace_hash_ent *ent;
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_ctxsw, "trace=%p", trace);

	mutex_lock(&trace->hash_lock);
	hash_for_each_safe(trace->pid_hash_table, bkt, tmp, ent, node) {
		hash_del(&ent->node);
		kfree(ent);
	}
	mutex_unlock(&trace->hash_lock);

}

static pid_t gk20a_fecs_trace_find_pid(struct gk20a *g, u32 context_ptr)
{
	struct gk20a_fecs_trace_hash_ent *ent;
	struct gk20a_fecs_trace *trace = g->fecs_trace;
	pid_t pid = 0;

	mutex_lock(&trace->hash_lock);
	hash_for_each_possible(trace->pid_hash_table, ent, node, context_ptr) {
		if (ent->context_ptr == context_ptr) {
			gk20a_dbg(gpu_dbg_ctxsw,
				"found context_ptr=%x -> pid=%d",
				ent->context_ptr, ent->pid);
			pid = ent->pid;
			break;
		}
	}
	mutex_unlock(&trace->hash_lock);

	return pid;
}

/*
 * Converts HW entry format to userspace-facing format and pushes it to the
 * queue.
 */
static int gk20a_fecs_trace_ring_read(struct gk20a *g, int index)
{
	int i;
	struct nvgpu_ctxsw_trace_entry entry = { };
	struct gk20a_fecs_trace *trace = g->fecs_trace;
	pid_t cur_pid;
	pid_t new_pid;

	/* for now, only one VM */
	const int vmid = 0;

	struct gk20a_fecs_trace_record *r = gk20a_fecs_trace_get_record(
		trace, index);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_ctxsw,
		"consuming record trace=%p read=%d record=%p", trace, index, r);

	if (unlikely(!gk20a_fecs_trace_is_valid_record(r))) {
		gk20a_warn(dev_from_gk20a(g),
			"trace=%p read=%d record=%p magic_lo=%08x magic_hi=%08x (invalid)",
			trace, index, r, r->magic_lo, r->magic_hi);
		return -EINVAL;
	}

	cur_pid = gk20a_fecs_trace_find_pid(g, r->context_ptr);
	new_pid = gk20a_fecs_trace_find_pid(g, r->new_context_ptr);

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_ctxsw,
		"context_ptr=%x (pid=%d) new_context_ptr=%x (pid=%d)",
		r->context_ptr, cur_pid, r->new_context_ptr, new_pid);

	entry.context_id = r->context_id;
	entry.vmid = vmid;

	/* break out FECS record into trace events */
	for (i = 0; i < gk20a_fecs_trace_num_ts(); i++) {

		entry.tag = gk20a_fecs_trace_record_ts_tag_v(r->ts[i]);
		entry.timestamp = gk20a_fecs_trace_record_ts_timestamp_v(r->ts[i]);
		entry.timestamp <<= GK20A_FECS_TRACE_PTIMER_SHIFT;

		gk20a_dbg(gpu_dbg_ctxsw,
			"tag=%x timestamp=%llx context_id=%08x new_context_id=%08x",
			entry.tag, entry.timestamp, r->context_id,
			r->new_context_id);

		switch (entry.tag) {
		case NVGPU_CTXSW_TAG_RESTORE_START:
		case NVGPU_CTXSW_TAG_CONTEXT_START:
			entry.context_id = r->new_context_id;
			entry.pid = new_pid;
			break;

		case NVGPU_CTXSW_TAG_CTXSW_REQ_BY_HOST:
		case NVGPU_CTXSW_TAG_FE_ACK:
		case NVGPU_CTXSW_TAG_FE_ACK_WFI:
		case NVGPU_CTXSW_TAG_FE_ACK_GFXP:
		case NVGPU_CTXSW_TAG_FE_ACK_CTAP:
		case NVGPU_CTXSW_TAG_FE_ACK_CILP:
		case NVGPU_CTXSW_TAG_SAVE_END:
			entry.context_id = r->context_id;
			entry.pid = cur_pid;
			break;

		default:
			/* tags are not guaranteed to start at the beginning */
			WARN_ON(entry.tag && (entry.tag != NVGPU_CTXSW_TAG_INVALID_TIMESTAMP));
			continue;
		}

		gk20a_dbg(gpu_dbg_ctxsw, "tag=%x context_id=%x pid=%lld",
			entry.tag, entry.context_id, entry.pid);

		if (!entry.context_id)
			continue;

		gk20a_ctxsw_trace_write(g, &entry);
	}

	gk20a_ctxsw_trace_wake_up(g, vmid);
	return 0;
}

static int gk20a_fecs_trace_poll(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	int read = 0;
	int write = 0;
	int cnt;
	int err;

	err = gk20a_busy(g->dev);
	if (unlikely(err))
		return err;

	mutex_lock(&trace->poll_lock);
	write = gk20a_fecs_trace_get_write_index(g);
	if (unlikely((write < 0) || (write >= GK20A_FECS_TRACE_NUM_RECORDS))) {
		gk20a_err(dev_from_gk20a(g),
			"failed to acquire write index, write=%d", write);
		err = write;
		goto done;
	}

	read = gk20a_fecs_trace_get_read_index(g);

	cnt = CIRC_CNT(write, read, GK20A_FECS_TRACE_NUM_RECORDS);
	if (!cnt)
		goto done;

	gk20a_dbg(gpu_dbg_ctxsw,
		"circular buffer: read=%d (mailbox=%d) write=%d cnt=%d",
		read, gk20a_fecs_trace_get_read_index(g), write, cnt);

	/* consume all records */
	while (read != write) {
		gk20a_fecs_trace_ring_read(g, read);

		/* Get to next record. */
		read = (read + 1) & (GK20A_FECS_TRACE_NUM_RECORDS - 1);
		gk20a_fecs_trace_set_read_index(g, read);
	}

done:
	mutex_unlock(&trace->poll_lock);
	gk20a_idle(g->dev);
	return err;
}

static int gk20a_fecs_trace_periodic_polling(void *arg)
{
	struct gk20a *g = (struct gk20a *)arg;
	struct timespec ts = ns_to_timespec(GK20A_FECS_TRACE_FRAME_PERIOD_NS);

	pr_info("%s: running\n", __func__);

	while (!kthread_should_stop()) {

		hrtimer_nanosleep(&ts, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

		gk20a_fecs_trace_poll(g);
	}

	return 0;
}

static int gk20a_fecs_trace_alloc_ring(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	return gk20a_gmmu_alloc_sys(g, GK20A_FECS_TRACE_NUM_RECORDS
			* ctxsw_prog_record_timestamp_record_size_in_bytes_v(),
			&trace->trace_buf);
}

static void gk20a_fecs_trace_free_ring(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_gmmu_free(g, &trace->trace_buf);
}

#ifdef CONFIG_DEBUG_FS
/*
 * The sequence iterator functions.  We simply use the count of the
 * next line as our internal position.
 */
static void *gk20a_fecs_trace_debugfs_ring_seq_start(
		struct seq_file *s, loff_t *pos)
{
	if (*pos >= GK20A_FECS_TRACE_NUM_RECORDS)
		return NULL;

	return pos;
}

static void *gk20a_fecs_trace_debugfs_ring_seq_next(
		struct seq_file *s, void *v, loff_t *pos)
{
	++(*pos);
	if (*pos >= GK20A_FECS_TRACE_NUM_RECORDS)
		return NULL;
	return pos;
}

static void gk20a_fecs_trace_debugfs_ring_seq_stop(
		struct seq_file *s, void *v)
{
}

static int gk20a_fecs_trace_debugfs_ring_seq_show(
		struct seq_file *s, void *v)
{
	loff_t *pos = (loff_t *) v;
	struct gk20a *g = *(struct gk20a **)s->private;
	struct gk20a_fecs_trace *trace = g->fecs_trace;
	struct gk20a_fecs_trace_record *r = gk20a_fecs_trace_get_record(trace, *pos);
	int i;
	const u32 invalid_tag =
	    ctxsw_prog_record_timestamp_timestamp_hi_tag_invalid_timestamp_v();
	u32 tag;
	u64 timestamp;

	seq_printf(s, "record #%lld (%p)\n", *pos, r);
	seq_printf(s, "\tmagic_lo=%08x\n", r->magic_lo);
	seq_printf(s, "\tmagic_hi=%08x\n", r->magic_hi);
	if (gk20a_fecs_trace_is_valid_record(r)) {
		seq_printf(s, "\tcontext_ptr=%08x\n", r->context_ptr);
		seq_printf(s, "\tcontext_id=%08x\n", r->context_id);
		seq_printf(s, "\tnew_context_ptr=%08x\n", r->new_context_ptr);
		seq_printf(s, "\tnew_context_id=%08x\n", r->new_context_id);
		for (i = 0; i < gk20a_fecs_trace_num_ts(); i++) {
			tag = gk20a_fecs_trace_record_ts_tag_v(r->ts[i]);
			if (tag == invalid_tag)
				continue;
			timestamp = gk20a_fecs_trace_record_ts_timestamp_v(r->ts[i]);
			timestamp <<= GK20A_FECS_TRACE_PTIMER_SHIFT;
			seq_printf(s, "\ttag=%02x timestamp=%012llx\n", tag, timestamp);
		}
	}
	return 0;
}

/*
 * Tie them all together into a set of seq_operations.
 */
const struct seq_operations gk20a_fecs_trace_debugfs_ring_seq_ops = {
	.start = gk20a_fecs_trace_debugfs_ring_seq_start,
	.next = gk20a_fecs_trace_debugfs_ring_seq_next,
	.stop = gk20a_fecs_trace_debugfs_ring_seq_stop,
	.show = gk20a_fecs_trace_debugfs_ring_seq_show
};

/*
 * Time to set up the file operations for our /proc file.  In this case,
 * all we need is an open function which sets up the sequence ops.
 */

static int gk20a_ctxsw_debugfs_ring_open(struct inode *inode,
	struct file *file)
{
	struct gk20a **p;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	p = __seq_open_private(file, &gk20a_fecs_trace_debugfs_ring_seq_ops,
		sizeof(struct gk20a *));
	if (!p)
		return -ENOMEM;

	*p = (struct gk20a *)inode->i_private;
	return 0;
};

/*
 * The file operations structure contains our open function along with
 * set of the canned seq_ ops.
 */
const struct file_operations gk20a_fecs_trace_debugfs_ring_fops = {
	.owner = THIS_MODULE,
	.open = gk20a_ctxsw_debugfs_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private
};

static int gk20a_fecs_trace_debugfs_read(void *arg, u64 *val)
{
	*val = gk20a_fecs_trace_get_read_index((struct gk20a *)arg);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(gk20a_fecs_trace_debugfs_read_fops,
	gk20a_fecs_trace_debugfs_read, NULL, "%llu\n");

static int gk20a_fecs_trace_debugfs_write(void *arg, u64 *val)
{
	*val = gk20a_fecs_trace_get_write_index((struct gk20a *)arg);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(gk20a_fecs_trace_debugfs_write_fops,
	gk20a_fecs_trace_debugfs_write, NULL, "%llu\n");

static void gk20a_fecs_trace_debugfs_init(struct gk20a *g)
{
	struct gk20a_platform *plat = dev_get_drvdata(g->dev);

	debugfs_create_file("ctxsw_trace_read", 0600, plat->debugfs, g,
		&gk20a_fecs_trace_debugfs_read_fops);
	debugfs_create_file("ctxsw_trace_write", 0600, plat->debugfs, g,
		&gk20a_fecs_trace_debugfs_write_fops);
	debugfs_create_file("ctxsw_trace_ring", 0600, plat->debugfs, g,
		&gk20a_fecs_trace_debugfs_ring_fops);
}

static void gk20a_fecs_trace_debugfs_cleanup(struct gk20a *g)
{
	struct gk20a_platform *plat = dev_get_drvdata(g->dev);

	debugfs_remove_recursive(plat->debugfs);
}

#else

static void gk20a_fecs_trace_debugfs_init(struct gk20a *g)
{
}

static inline void gk20a_fecs_trace_debugfs_cleanup(struct gk20a *g)
{
}

#endif /* CONFIG_DEBUG_FS */

static int gk20a_fecs_trace_init(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace;
	int err;

	trace = kzalloc(sizeof(struct gk20a_fecs_trace), GFP_KERNEL);
	if (!trace) {
		gk20a_warn(dev_from_gk20a(g), "failed to allocate fecs_trace");
		return -ENOMEM;
	}
	g->fecs_trace = trace;

	BUG_ON(!is_power_of_2(GK20A_FECS_TRACE_NUM_RECORDS));
	err = gk20a_fecs_trace_alloc_ring(g);
	if (err) {
		gk20a_warn(dev_from_gk20a(g), "failed to allocate FECS ring");
		goto clean;
	}

	mutex_init(&trace->poll_lock);
	mutex_init(&trace->hash_lock);
	hash_init(trace->pid_hash_table);

	gk20a_fecs_trace_debugfs_init(g);
	return 0;

clean:
	kfree(trace);
	g->fecs_trace = NULL;
	return err;
}

static int gk20a_fecs_trace_bind_channel(struct gk20a *g,
		struct channel_gk20a *ch)
{
	/*
	 * map our circ_buf to the context space and store the GPU VA
	 * in the context header.
	 */

	u32 lo;
	u32 hi;
	phys_addr_t pa;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	struct gk20a_fecs_trace *trace = g->fecs_trace;
	struct mem_desc *mem = &ch_ctx->gr_ctx->mem;
	u32 context_ptr = gk20a_fecs_trace_fecs_context_ptr(ch);
	pid_t pid;

	gk20a_dbg(gpu_dbg_fn|gpu_dbg_ctxsw,
			"hw_chid=%d context_ptr=%x inst_block=%llx",
			ch->hw_chid, context_ptr,
			gk20a_mm_inst_block_addr(g, &ch->inst_block));

	if (!trace)
		return -ENOMEM;

	pa = gk20a_mm_inst_block_addr(g, &trace->trace_buf);
	if (!pa)
		return -ENOMEM;

	if (gk20a_mem_begin(g, mem))
		return -ENOMEM;

	lo = u64_lo32(pa);
	hi = u64_hi32(pa);

	gk20a_dbg(gpu_dbg_ctxsw, "addr_hi=%x addr_lo=%x count=%d", hi,
		lo, GK20A_FECS_TRACE_NUM_RECORDS);

	gk20a_mem_wr(g, mem,
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_o(),
		lo);
	gk20a_mem_wr(g, mem,
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_o(),
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_v_f(hi));
	gk20a_mem_wr(g, mem,
		ctxsw_prog_main_image_context_timestamp_buffer_control_o(),
		ctxsw_prog_main_image_context_timestamp_buffer_control_num_records_f(
			GK20A_FECS_TRACE_NUM_RECORDS));

	gk20a_mem_end(g, mem);

	/* pid (process identifier) in user space, corresponds to tgid (thread
	 * group id) in kernel space.
	 */
	if (gk20a_is_channel_marked_as_tsg(ch))
		pid = tsg_gk20a_from_ch(ch)->tgid;
	else
		pid = ch->tgid;
	gk20a_fecs_trace_hash_add(g, context_ptr, pid);

	return 0;
}

static int gk20a_fecs_trace_unbind_channel(struct gk20a *g, struct channel_gk20a *ch)
{
	u32 context_ptr = gk20a_fecs_trace_fecs_context_ptr(ch);

	gk20a_dbg(gpu_dbg_fn|gpu_dbg_ctxsw,
			"ch=%p context_ptr=%x", ch, context_ptr);

	if (g->ops.fecs_trace.is_enabled(g)) {
		if (g->ops.fecs_trace.flush)
			g->ops.fecs_trace.flush(g);
		gk20a_fecs_trace_poll(g);
	}
	gk20a_fecs_trace_hash_del(g, context_ptr);
	return 0;
}

static int gk20a_fecs_trace_reset(struct gk20a *g)
{
	gk20a_dbg(gpu_dbg_fn|gpu_dbg_ctxsw, "");

	if (!g->ops.fecs_trace.is_enabled(g))
		return 0;

	gk20a_fecs_trace_poll(g);
	return gk20a_fecs_trace_set_read_index(g, 0);
}

static int gk20a_fecs_trace_deinit(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	gk20a_fecs_trace_debugfs_cleanup(g);
	kthread_stop(trace->poll_task);
	gk20a_fecs_trace_free_ring(g);
	gk20a_fecs_trace_free_hash_table(g);

	kfree(g->fecs_trace);
	g->fecs_trace = NULL;
	return 0;
}

static int gk20a_gr_max_entries(struct gk20a *g,
		struct nvgpu_ctxsw_trace_filter *filter)
{
	int n;
	int tag;

	/* Compute number of entries per record, with given filter */
	for (n = 0, tag = 0; tag < gk20a_fecs_trace_num_ts(); tag++)
		n += (NVGPU_CTXSW_FILTER_ISSET(tag, filter) != 0);

	/* Return max number of entries generated for the whole ring */
	return n * GK20A_FECS_TRACE_NUM_RECORDS;
}

static int gk20a_fecs_trace_enable(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;
	struct task_struct *task;

	if (!trace->poll_task) {
		task = kthread_run(gk20a_fecs_trace_periodic_polling, g, __func__);
		if (unlikely(IS_ERR(task))) {
			gk20a_warn(dev_from_gk20a(g), "failed to create FECS polling task");
			return PTR_ERR(task);
		}
		trace->poll_task = task;
	}

	return 0;
}

static int gk20a_fecs_trace_disable(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	if (trace->poll_task) {
		kthread_stop(trace->poll_task);
		trace->poll_task = NULL;
	}

	return -EPERM;
}

static bool gk20a_fecs_trace_is_enabled(struct gk20a *g)
{
	struct gk20a_fecs_trace *trace = g->fecs_trace;

	return (trace && trace->poll_task);
}


void gk20a_init_fecs_trace_ops(struct gpu_ops *ops)
{
	gk20a_ctxsw_trace_init_ops(ops);
	ops->fecs_trace.init = gk20a_fecs_trace_init;
	ops->fecs_trace.deinit = gk20a_fecs_trace_deinit;
	ops->fecs_trace.enable = gk20a_fecs_trace_enable;
	ops->fecs_trace.disable = gk20a_fecs_trace_disable;
	ops->fecs_trace.is_enabled = gk20a_fecs_trace_is_enabled;
	ops->fecs_trace.reset = gk20a_fecs_trace_reset;
	ops->fecs_trace.flush = NULL;
	ops->fecs_trace.poll = gk20a_fecs_trace_poll;
	ops->fecs_trace.bind_channel = gk20a_fecs_trace_bind_channel;
	ops->fecs_trace.unbind_channel = gk20a_fecs_trace_unbind_channel;
	ops->fecs_trace.max_entries = gk20a_gr_max_entries;
}
#else
void gk20a_init_fecs_trace_ops(struct gpu_ops *ops)
{
}
#endif /* CONFIG_GK20A_CTXSW_TRACE */
