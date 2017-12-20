/*
 * drivers/video/tegra/host/nvhost_cdma.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "nvhost_cdma.h"
#include "dev.h"
#include <asm/cacheflush.h>

#include <linux/slab.h>
#include <trace/events/nvhost.h>
#include <linux/interrupt.h>

/*
 * TODO:
 *   stats
 *     - for figuring out what to optimize further
 *   resizable push buffer & sync queue
 *     - some channels hardly need any, some channels (3d) could use more
 */

/* Sync Queue
 *
 * The sync queue is a circular buffer of u32s interpreted as:
 *   0: SyncPointID
 *   1: SyncPointValue
 *   2: FirstDMAGet (start of submit in pushbuffer)
 *   3: Timeout (time to live for this submit)
 *   4: TimeoutContext (userctx that submitted buffer)
 *   5: NumSlots (how many pushbuffer slots to free)
 *   6: NumHandles
 *   7: nvmap client which pinned the handles
 *   8..: NumHandles * nvmemhandle to unpin
 *
 * There's always one word unused, so (accounting for wrap):
 *   - Write == Read => queue empty
 *   - Write + 1 == Read => queue full
 * The queue must not be left with less than SYNC_QUEUE_MIN_ENTRY words
 * of space at the end of the array.
 *
 * We want to pass contiguous arrays of handles to nvmap_unpin_handles,
 * so arrays that would wrap at the end of the buffer will be split into
 * two (or more) entries.
 */

/* Number of words needed to store an entry containing one handle */
#define SYNC_QUEUE_MIN_ENTRY (SQ_IDX_HANDLES + (sizeof(void *)/4))

/* Magic to use to fill freed handle slots */
#define BAD_MAGIC 0xdeadbeef

/**
 * Reset to empty queue.
 */
static void reset_sync_queue(struct sync_queue *queue)
{
	queue->read = 0;
	queue->write = 0;
}

/**
 *  Find the number of handles that can be stashed in the sync queue without
 *  waiting.
 *  0 -> queue is full, must update to wait for some entries to be freed.
 */
static unsigned int sync_queue_space(struct sync_queue *queue)
{
	struct nvhost_cdma *cdma;
	struct nvhost_master *host;

	unsigned int read = queue->read;
	unsigned int write = queue->write;
	u32 size;

	cdma = container_of(queue, struct nvhost_cdma, sync_queue);
	host = cdma_to_dev(cdma);

	BUG_ON(read  > (host->sync_queue_size - SYNC_QUEUE_MIN_ENTRY));
	BUG_ON(write > (host->sync_queue_size - SYNC_QUEUE_MIN_ENTRY));

	/*
	 * We can use all of the space up to the end of the buffer, unless the
	 * read position is within that space (the read position may advance
	 * asynchronously, but that can't take space away once we've seen it).
	 */
	if (read > write) {
		size = (read - 1) - write;
	} else {
		size = host->sync_queue_size - write;

		/*
		 * If the read position is zero, it gets complicated. We can't
		 * use the last word in the buffer, because that would leave
		 * the queue empty.
		 * But also if we use too much we would not leave enough space
		 * for a single handle packet, and would have to wrap in
		 * add_to_sync_queue - also leaving write == read == 0,
		 * an empty queue.
		 */
		if (read == 0)
			size -= SYNC_QUEUE_MIN_ENTRY;
	}

	/*
	 * There must be room for an entry header and at least one handle,
	 * otherwise we report a full queue.
	 */
	if (size < SYNC_QUEUE_MIN_ENTRY)
		return 0;
	/* Minimum entry stores one handle */
	return (size - SYNC_QUEUE_MIN_ENTRY) + 1;
}

/**
 * Debug routine used to dump sync_queue entries
 */
static void dump_sync_queue_entry(struct nvhost_cdma *cdma, u32 *entry)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);

	dev_dbg(&dev->pdev->dev, "sync_queue index 0x%x\n",
		(entry - cdma->sync_queue.buffer));
	dev_dbg(&dev->pdev->dev, "    SYNCPT_ID   %d\n",
		entry[SQ_IDX_SYNCPT_ID]);
	dev_dbg(&dev->pdev->dev, "    SYNCPT_VAL  %d\n",
		entry[SQ_IDX_SYNCPT_VAL]);
	dev_dbg(&dev->pdev->dev, "    FIRST_GET   0x%x\n",
		entry[SQ_IDX_FIRST_GET]);
	dev_dbg(&dev->pdev->dev, "    TIMEOUT     %d\n",
		entry[SQ_IDX_TIMEOUT]);
	dev_dbg(&dev->pdev->dev, "    TIMEOUT_CTX 0x%x\n",
		entry[SQ_IDX_TIMEOUT_CTX]);
	dev_dbg(&dev->pdev->dev, "    NUM_SLOTS   %d\n",
		entry[SQ_IDX_NUM_SLOTS]);
	dev_dbg(&dev->pdev->dev, "    NUM_HANDLES %d\n",
		entry[SQ_IDX_NUM_HANDLES]);
}

/**
 * Add an entry to the sync queue.
 */
#define entry_size(_cnt)	((_cnt)*sizeof(void *)/sizeof(u32))

static void add_to_sync_queue(struct sync_queue *queue,
			      u32 sync_point_id, u32 sync_point_value,
			      u32 nr_slots, struct nvmap_client *user_nvmap,
			      struct nvmap_handle **handles, u32 nr_handles,
			      u32 first_get,
			      struct nvhost_userctx_timeout *timeout)
{
	struct nvhost_cdma *cdma;
	struct nvhost_master *host;
	u32 size, write = queue->write;
	u32 *p = queue->buffer + write;

	cdma = container_of(queue, struct nvhost_cdma, sync_queue);
	host = cdma_to_dev(cdma);

	BUG_ON(sync_point_id == NVSYNCPT_INVALID);
	BUG_ON(sync_queue_space(queue) < nr_handles);

	size  = SQ_IDX_HANDLES;
	size += entry_size(nr_handles);

	write += size;
	BUG_ON(write > host->sync_queue_size);

	p[SQ_IDX_SYNCPT_ID] = sync_point_id;
	p[SQ_IDX_SYNCPT_VAL] = sync_point_value;
	p[SQ_IDX_FIRST_GET] = first_get;
	p[SQ_IDX_TIMEOUT] = timeout->timeout;
	p[SQ_IDX_NUM_SLOTS] = nr_slots;
	p[SQ_IDX_NUM_HANDLES] = nr_handles;

	*(void **)(&p[SQ_IDX_TIMEOUT_CTX]) = timeout;

	BUG_ON(!user_nvmap);
	*(struct nvmap_client **)(&p[SQ_IDX_NVMAP_CTX]) =
		nvmap_client_get(user_nvmap);

	if (nr_handles) {
		memcpy(&p[SQ_IDX_HANDLES], handles,
			(nr_handles * sizeof(struct nvmap_handle *)));
	}

	/* If there's not enough room for another entry, wrap to the start. */
	if ((write + SYNC_QUEUE_MIN_ENTRY) > host->sync_queue_size) {
		/*
		 * It's an error for the read position to be zero, as that
		 * would mean we emptied the queue while adding something.
		 */
		BUG_ON(queue->read == 0);
		write = 0;
	}
	queue->write = write;
}

/**
 * Get a pointer to the next entry in the queue, or NULL if the queue is empty.
 * Doesn't consume the entry.
 */
static u32 *sync_queue_head(struct sync_queue *queue)
{
	struct nvhost_cdma *cdma = container_of(queue,
						struct nvhost_cdma,
						sync_queue);
	struct nvhost_master *host = cdma_to_dev(cdma);
	u32 read = queue->read;
	u32 write = queue->write;

	BUG_ON(read  > (host->sync_queue_size - SYNC_QUEUE_MIN_ENTRY));
	BUG_ON(write > (host->sync_queue_size - SYNC_QUEUE_MIN_ENTRY));

	if (read == write)
		return NULL;
	return queue->buffer + read;
}

/**
 * Advances to the next queue entry, if you want to consume it.
 */
static void
dequeue_sync_queue_head(struct sync_queue *queue)
{
	struct nvhost_cdma *cdma = container_of(queue,
						struct nvhost_cdma,
						sync_queue);
	struct nvhost_master *host = cdma_to_dev(cdma);
	u32 read = queue->read;
	u32 size;

	BUG_ON(read == queue->write);

	size  = SQ_IDX_HANDLES;
	size += entry_size(queue->buffer[read + SQ_IDX_NUM_HANDLES]);

	read += size;
	BUG_ON(read > host->sync_queue_size);

	/* If there's not enough room for another entry, wrap to the start. */
	if ((read + SYNC_QUEUE_MIN_ENTRY) > host->sync_queue_size)
		read = 0;
	queue->read = read;
}

/**
 * Return the status of the cdma's sync queue or push buffer for the given event
 *  - sq empty: returns 1 for empty, 0 for not empty (as in "1 empty queue" :-)
 *  - sq space: returns the number of handles that can be stored in the queue
 *  - pb space: returns the number of free slots in the channel's push buffer
 * Must be called with the cdma lock held.
 */
static unsigned int cdma_status_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	switch (event) {
	case CDMA_EVENT_SYNC_QUEUE_EMPTY:
		return sync_queue_head(&cdma->sync_queue) ? 0 : 1;
	case CDMA_EVENT_SYNC_QUEUE_SPACE:
		return sync_queue_space(&cdma->sync_queue);
	case CDMA_EVENT_PUSH_BUFFER_SPACE: {
		struct push_buffer *pb = &cdma->push_buffer;
		BUG_ON(!cdma_pb_op(cdma).space);
		return cdma_pb_op(cdma).space(pb);
	}
	default:
		return 0;
	}
}

/**
 * Sleep (if necessary) until the requested event happens
 *   - CDMA_EVENT_SYNC_QUEUE_EMPTY : sync queue is completely empty.
 *     - Returns 1
 *   - CDMA_EVENT_SYNC_QUEUE_SPACE : there is space in the sync queue.
 *   - CDMA_EVENT_PUSH_BUFFER_SPACE : there is space in the push buffer
 *     - Return the amount of space (> 0)
 * Must be called with the cdma lock held.
 */
unsigned int nvhost_cdma_wait_locked(struct nvhost_cdma *cdma,
		enum cdma_event event)
{
	for (;;) {
		unsigned int space = cdma_status_locked(cdma, event);
		if (space)
			return space;

		trace_nvhost_wait_cdma(cdma_to_channel(cdma)->desc->name,
				event);

		BUG_ON(cdma->event != CDMA_EVENT_NONE);
		cdma->event = event;

		mutex_unlock(&cdma->lock);
		down(&cdma->sem);
		mutex_lock(&cdma->lock);
	}
	return 0;
}

/**
 * Start timer for a buffer submition that has completed yet.
 * Must be called with the cdma lock held.
 */
static void cdma_start_timer_locked(struct nvhost_cdma *cdma, u32 syncpt_id,
				u32 syncpt_val,
				struct nvhost_userctx_timeout *timeout)
{
	BUG_ON(!timeout);
	if (cdma->timeout.ctx_timeout) {
		/* timer already started */
		return;
	}

	cdma->timeout.ctx_timeout = timeout;
	cdma->timeout.syncpt_id = syncpt_id;
	cdma->timeout.syncpt_val = syncpt_val;
	cdma->timeout.start_ktime = ktime_get();

	schedule_delayed_work(&cdma->timeout.wq,
			msecs_to_jiffies(timeout->timeout));
}

/**
 * Stop timer when a buffer submition completes.
 * Must be called with the cdma lock held.
 */
static void stop_cdma_timer_locked(struct nvhost_cdma *cdma)
{
	cancel_delayed_work(&cdma->timeout.wq);
	cdma->timeout.ctx_timeout = NULL;
}

/**
 * For all sync queue entries that have already finished according to the
 * current sync point registers:
 *  - unpin & unref their mems
 *  - pop their push buffer slots
 *  - remove them from the sync queue
 * This is normally called from the host code's worker thread, but can be
 * called manually if necessary.
 * Must be called with the cdma lock held.
 */
static void update_cdma_locked(struct nvhost_cdma *cdma)
{
	bool signal = false;
	struct nvhost_master *dev = cdma_to_dev(cdma);

	/* If CDMA is stopped, queue is cleared and we can return */
	if (!cdma->running)
		return;

	/*
	 * Walk the sync queue, reading the sync point registers as necessary,
	 * to consume as many sync queue entries as possible without blocking
	 */
	for (;;) {
		u32 syncpt_id, syncpt_val;
		u32 timeout;
		struct nvhost_userctx_timeout *timeout_ref = NULL;
		unsigned int nr_slots, nr_handles;
		struct nvhost_syncpt *sp = &dev->syncpt;
		struct nvmap_handle **handles;
		struct nvmap_client *nvmap;
		u32 *sync;

		sync = sync_queue_head(&cdma->sync_queue);
		if (!sync) {
			if (cdma->event == CDMA_EVENT_SYNC_QUEUE_EMPTY)
				signal = true;
			break;
		}

		syncpt_id = sync[SQ_IDX_SYNCPT_ID];
		syncpt_val = sync[SQ_IDX_SYNCPT_VAL];
		timeout = sync[SQ_IDX_TIMEOUT];
		timeout_ref = (struct nvhost_userctx_timeout *)
				sync[SQ_IDX_TIMEOUT_CTX];

		BUG_ON(syncpt_id == NVSYNCPT_INVALID);

		/* Check whether this syncpt has completed, and bail if not */
		if (!nvhost_syncpt_is_expired(sp, syncpt_id, syncpt_val)) {
			/* Start timer on next pending syncpt */
			if (timeout) {
				cdma_start_timer_locked(cdma, syncpt_id,
					syncpt_val, timeout_ref);
			}
			break;
		}

		/* Cancel timeout, when a buffer completes */
		if (cdma->timeout.ctx_timeout)
			stop_cdma_timer_locked(cdma);

		nr_slots = sync[SQ_IDX_NUM_SLOTS];
		nr_handles = sync[SQ_IDX_NUM_HANDLES];
		nvmap = (struct nvmap_client *)sync[SQ_IDX_NVMAP_CTX];
		handles = (struct nvmap_handle **)&sync[SQ_IDX_HANDLES];

		BUG_ON(!nvmap);

		/* Unpin the memory */
		nvmap_unpin_handles(nvmap, handles, nr_handles);
		memset(handles, BAD_MAGIC, nr_handles * sizeof(*handles));
		nvmap_client_put(nvmap);
		sync[SQ_IDX_NVMAP_CTX] = 0;

		/* Pop push buffer slots */
		if (nr_slots) {
			struct push_buffer *pb = &cdma->push_buffer;
			BUG_ON(!cdma_pb_op(cdma).pop_from);
			cdma_pb_op(cdma).pop_from(pb, nr_slots);
			if (cdma->event == CDMA_EVENT_PUSH_BUFFER_SPACE)
				signal = true;
		}

		dequeue_sync_queue_head(&cdma->sync_queue);
		if (cdma->event == CDMA_EVENT_SYNC_QUEUE_SPACE)
			signal = true;
	}

	/* Wake up CdmaWait() if the requested event happened */
	if (signal) {
		cdma->event = CDMA_EVENT_NONE;
		up(&cdma->sem);
	}
}

static u32 *advance_next_entry(struct nvhost_cdma *cdma, u32 *read)
{
	struct nvhost_master *host;
	u32 ridx;

	host = cdma_to_dev(cdma);

	/* move sync_queue read ptr to next entry */
	ridx = (read - cdma->sync_queue.buffer);
	ridx += (SQ_IDX_HANDLES + entry_size(read[SQ_IDX_NUM_HANDLES]));
	if ((ridx + SYNC_QUEUE_MIN_ENTRY) > host->sync_queue_size)
		ridx = 0;

	/* return sync_queue entry */
	return cdma->sync_queue.buffer + ridx;
}

void nvhost_cdma_update_sync_queue(struct nvhost_cdma *cdma,
		struct nvhost_syncpt *syncpt, struct device *dev)
{
	u32 first_get, get_restart;
	u32 syncpt_incrs, nr_slots;
	bool exec_ctxsave;
	struct sync_queue *queue = &cdma->sync_queue;
	u32 *sync = sync_queue_head(queue);
	u32 syncpt_val = nvhost_syncpt_update_min(syncpt,
			cdma->timeout.syncpt_id);

	dev_dbg(dev,
		"%s: starting cleanup (thresh %d, queue rd 0x%x wr 0x%x)\n",
		__func__,
		syncpt_val, queue->read, queue->write);

	/*
	 * Move the sync_queue read pointer to the first entry that hasn't
	 * completed based on the current HW syncpt value. It's likely there
	 * won't be any (i.e. we're still at the head), but covers the case
	 * where a syncpt incr happens just prior/during the teardown.
	 */

	dev_dbg(dev,
		"%s: skip completed buffers still in sync_queue\n",
		__func__);

	while (sync != (queue->buffer + queue->write)) {
		/* move read ptr to first blocked entry */
		if (syncpt_val < sync[SQ_IDX_SYNCPT_VAL])
			break;	/* not completed */

		dump_sync_queue_entry(cdma, sync);
		sync = advance_next_entry(cdma, sync);
	}

	/*
	 * Walk the sync_queue, first incrementing with the CPU syncpts that
	 * are partially executed (the first buffer) or fully skipped while
	 * still in the current context (slots are also NOP-ed).
	 *
	 * At the point contexts are interleaved, syncpt increments must be
	 * done inline with the pushbuffer from a GATHER buffer to maintain
	 * the order (slots are modified to be a GATHER of syncpt incrs).
	 *
	 * Note: save in get_restart the location where the timed out buffer
	 * started in the PB, so we can start the refetch from there (with the
	 * modified NOP-ed PB slots). This lets things appear to have completed
	 * properly for this buffer and resources are freed.
	 */

	dev_dbg(dev,
		"%s: perform CPU incr on pending same ctx buffers\n",
		__func__);

	get_restart = cdma->last_put;
	if (sync != (queue->buffer + queue->write))
		get_restart = sync[SQ_IDX_FIRST_GET];

	/* do CPU increments */
	while (sync != (queue->buffer + queue->write)) {

		/* different context, gets us out of this loop */
		if ((void *)sync[SQ_IDX_TIMEOUT_CTX] !=
				cdma->timeout.ctx_timeout)
			break;

		syncpt_incrs = (sync[SQ_IDX_SYNCPT_VAL] - syncpt_val);
		first_get = sync[SQ_IDX_FIRST_GET];
		nr_slots = sync[SQ_IDX_NUM_SLOTS];

		/* won't need a timeout when replayed */
		sync[SQ_IDX_TIMEOUT] = 0;

		dev_dbg(dev,
			"%s: CPU incr (%d)\n", __func__, syncpt_incrs);

		dump_sync_queue_entry(cdma, sync);

		/* safe to use CPU to incr syncpts */
		cdma_op(cdma).timeout_cpu_incr(cdma, first_get,
			syncpt_incrs, sync[SQ_IDX_SYNCPT_VAL], nr_slots);
		syncpt_val += syncpt_incrs;
		sync = advance_next_entry(cdma, sync);
	}

	dev_dbg(dev,
		"%s: GPU incr blocked interleaved ctx buffers\n",
		__func__);

	exec_ctxsave = false;

	/* setup GPU increments */
	while (sync != (queue->buffer + queue->write)) {

		syncpt_incrs = (sync[SQ_IDX_SYNCPT_VAL] - syncpt_val);
		first_get = sync[SQ_IDX_FIRST_GET];
		nr_slots = sync[SQ_IDX_NUM_SLOTS];

		/* same context, increment in the pushbuffer */
		if ((void *)sync[SQ_IDX_TIMEOUT_CTX] ==
				cdma->timeout.ctx_timeout) {

			/* won't need a timeout when replayed */
			sync[SQ_IDX_TIMEOUT] = 0;

			/* update buffer's syncpts in the pushbuffer */
			cdma_op(cdma).timeout_pb_incr(cdma, first_get,
				syncpt_incrs, nr_slots, exec_ctxsave);

			exec_ctxsave = false;
		} else {
			dev_dbg(dev,
				"%s: switch to a different userctx\n",
				__func__);
			/*
			 * If previous context was the timed out context
			 * then clear its CTXSAVE in this slot.
			 */
			exec_ctxsave = true;
		}

		dump_sync_queue_entry(cdma, sync);

		syncpt_val = sync[SQ_IDX_SYNCPT_VAL];
		sync = advance_next_entry(cdma, sync);
	}

	dev_dbg(dev,
		"%s: finished sync_queue modification\n", __func__);

	/* roll back DMAGET and start up channel again */
	cdma_op(cdma).timeout_teardown_end(cdma, get_restart);

	cdma->timeout.ctx_timeout->has_timedout = true;
	mutex_unlock(&cdma->lock);
}

/**
 * Create a cdma
 */
int nvhost_cdma_init(struct nvhost_cdma *cdma)
{
	int err;
	struct push_buffer *pb = &cdma->push_buffer;
	BUG_ON(!cdma_pb_op(cdma).init);
	mutex_init(&cdma->lock);
	sema_init(&cdma->sem, 0);
	cdma->event = CDMA_EVENT_NONE;
	cdma->running = false;
	cdma->torndown = false;

	/* allocate sync queue memory */
	cdma->sync_queue.buffer = kzalloc(cdma_to_dev(cdma)->sync_queue_size
					  * sizeof(u32), GFP_KERNEL);
	if (!cdma->sync_queue.buffer)
		return -ENOMEM;

	err = cdma_pb_op(cdma).init(pb);
	if (err)
		return err;
	reset_sync_queue(&cdma->sync_queue);
	return 0;
}

/**
 * Destroy a cdma
 */
void nvhost_cdma_deinit(struct nvhost_cdma *cdma)
{
	struct push_buffer *pb = &cdma->push_buffer;

	BUG_ON(!cdma_pb_op(cdma).destroy);
	BUG_ON(cdma->running);
	kfree(cdma->sync_queue.buffer);
	cdma->sync_queue.buffer = NULL;
	cdma_pb_op(cdma).destroy(pb);
	cdma_op(cdma).timeout_destroy(cdma);
}

/**
 * Begin a cdma submit
 */
int nvhost_cdma_begin(struct nvhost_cdma *cdma,
		       struct nvhost_userctx_timeout *timeout)
{
	mutex_lock(&cdma->lock);

	if (timeout && timeout->has_timedout) {
		struct nvhost_master *dev = cdma_to_dev(cdma);
		u32 min, max;

		min = nvhost_syncpt_update_min(&dev->syncpt,
			cdma->timeout.syncpt_id);
		max = nvhost_syncpt_read_min(&dev->syncpt,
			cdma->timeout.syncpt_id);

		dev_dbg(&dev->pdev->dev,
			"%s: skip timed out ctx submit (min = %d, max = %d)\n",
			__func__, min, max);
		mutex_unlock(&cdma->lock);
		return -ETIMEDOUT;
	}
	if (timeout && timeout->timeout) {
		/* init state on first submit with timeout value */
		if (!cdma->timeout.initialized) {
			int err;
			BUG_ON(!cdma_op(cdma).timeout_init);
			err = cdma_op(cdma).timeout_init(cdma,
				timeout->syncpt_id);
			if (err) {
				mutex_unlock(&cdma->lock);
				return err;
			}
		}
	}
	if (!cdma->running) {
		BUG_ON(!cdma_op(cdma).start);
		cdma_op(cdma).start(cdma);
	}
	cdma->slots_free = 0;
	cdma->slots_used = 0;
	cdma->first_get = cdma_pb_op(cdma).putptr(&cdma->push_buffer);
	return 0;
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push(struct nvhost_cdma *cdma, u32 op1, u32 op2)
{
	nvhost_cdma_push_gather(cdma, NULL, NULL, op1, op2);
}

/**
 * Push two words into a push buffer slot
 * Blocks as necessary if the push buffer is full.
 */
void nvhost_cdma_push_gather(struct nvhost_cdma *cdma,
		struct nvmap_client *client,
		struct nvmap_handle *handle, u32 op1, u32 op2)
{
	u32 slots_free = cdma->slots_free;
	struct push_buffer *pb = &cdma->push_buffer;
	BUG_ON(!cdma_pb_op(cdma).push_to);
	BUG_ON(!cdma_op(cdma).kick);
	if (slots_free == 0) {
		cdma_op(cdma).kick(cdma);
		slots_free = nvhost_cdma_wait_locked(cdma,
				CDMA_EVENT_PUSH_BUFFER_SPACE);
	}
	cdma->slots_free = slots_free - 1;
	cdma->slots_used++;
	cdma_pb_op(cdma).push_to(pb, client, handle, op1, op2);
}

/**
 * End a cdma submit
 * Kick off DMA, add a contiguous block of memory handles to the sync queue,
 * and a number of slots to be freed from the pushbuffer.
 * Blocks as necessary if the sync queue is full.
 * The handles for a submit must all be pinned at the same time, but they
 * can be unpinned in smaller chunks.
 */
void nvhost_cdma_end(struct nvhost_cdma *cdma,
		struct nvmap_client *user_nvmap,
		u32 sync_point_id, u32 sync_point_value,
		struct nvmap_handle **handles, unsigned int nr_handles,
		struct nvhost_userctx_timeout *timeout)
{
	bool was_idle = (cdma->sync_queue.read == cdma->sync_queue.write);

	BUG_ON(!cdma_op(cdma).kick);
	cdma_op(cdma).kick(cdma);

	while (nr_handles || cdma->slots_used) {
		unsigned int count;
		/*
		 * Wait until there's enough room in the
		 * sync queue to write something.
		 */
		count = nvhost_cdma_wait_locked(cdma,
				CDMA_EVENT_SYNC_QUEUE_SPACE);

		/* Add reloc entries to sync queue (as many as will fit) */
		if (count > nr_handles)
			count = nr_handles;

		add_to_sync_queue(&cdma->sync_queue, sync_point_id,
				  sync_point_value, cdma->slots_used,
				  user_nvmap, handles, count, cdma->first_get,
				  timeout);

		/* NumSlots only goes in the first packet */
		cdma->slots_used = 0;
		handles += count;
		nr_handles -= count;
	}

	/* start timer on idle -> active transitions */
	if (timeout->timeout && was_idle) {
		cdma_start_timer_locked(cdma, sync_point_id, sync_point_value,
			timeout);
	}

	mutex_unlock(&cdma->lock);
}

/**
 * Update cdma state according to current sync point values
 */
void nvhost_cdma_update(struct nvhost_cdma *cdma)
{
	mutex_lock(&cdma->lock);
	update_cdma_locked(cdma);
	mutex_unlock(&cdma->lock);
}

/**
 * Wait for push buffer to be empty.
 * @cdma pointer to channel cdma
 * @timeout timeout in ms
 * Returns -ETIME if timeout was reached, zero if push buffer is empty.
 */
int nvhost_cdma_flush(struct nvhost_cdma *cdma, int timeout)
{
	unsigned int space, err = 0;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);

	/*
	 * Wait for at most timeout ms. Recalculate timeout at each iteration
	 * to better keep within given timeout.
	 */
	while(!err && time_before(jiffies, end_jiffies)) {
		int timeout_jiffies = end_jiffies - jiffies;

		mutex_lock(&cdma->lock);
		space = cdma_status_locked(cdma,
				CDMA_EVENT_SYNC_QUEUE_EMPTY);
		if (space) {
			mutex_unlock(&cdma->lock);
			return 0;
		}

		BUG_ON(cdma->event != CDMA_EVENT_NONE);
		cdma->event = CDMA_EVENT_SYNC_QUEUE_EMPTY;

		mutex_unlock(&cdma->lock);
		err = down_timeout(&cdma->sem,
				jiffies_to_msecs(timeout_jiffies));
	}
	return err;
}
