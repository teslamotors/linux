/*
 * Inter-VM Communication
 *
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <asm/compiler.h>

#ifdef CONFIG_SMP

static inline void ivc_rmb(void)
{
	smp_rmb();
}

static inline void ivc_wmb(void)
{
	smp_wmb();
}

static inline void ivc_mb(void)
{
	smp_mb();
}

#else

static inline void ivc_rmb(void)
{
	rmb();
}

static inline void ivc_wmb(void)
{
	wmb();
}

static inline void ivc_mb(void)
{
	mb();
}

#endif

/*
 * IVC channel reset protocol.
 *
 * Each end uses its tx_channel.state to indicate its synchronization state.
 */
enum ivc_state {
	/*
	 * This value is zero for backwards compatibility with services that
	 * assume channels to be initially zeroed. Such channels are in an
	 * initially valid state, but cannot be asynchronously reset, and must
	 * maintain a valid state at all times.
	 *
	 * The transmitting end can enter the established state from the sync or
	 * ack state when it observes the receiving endpoint in the ack or
	 * established state, indicating that has cleared the counters in our
	 * rx_channel.
	 */
	ivc_state_established = 0,

	/*
	 * If an endpoint is observed in the sync state, the remote endpoint is
	 * allowed to clear the counters it owns asynchronously with respect to
	 * the current endpoint. Therefore, the current endpoint is no longer
	 * allowed to communicate.
	 */
	ivc_state_sync,

	/*
	 * When the transmitting end observes the receiving end in the sync
	 * state, it can clear the w_count and r_count and transition to the ack
	 * state. If the remote endpoint observes us in the ack state, it can
	 * return to the established state once it has cleared its counters.
	 */
	ivc_state_ack
};

/*
 * This structure is divided into two-cache aligned parts, the first is only
 * written through the tx_channel pointer, while the second is only written
 * through the rx_channel pointer. This delineates ownership of the cache lines,
 * which is critical to performance and necessary in non-cache coherent
 * implementations.
 */
struct ivc_channel_header {
	union {
		struct {
			/* fields owned by the transmitting end */
			uint32_t w_count;
			uint32_t state;
		};
		uint8_t w_align[IVC_ALIGN];
	};
	union {
		/* fields owned by the receiving end */
		uint32_t r_count;
		uint8_t r_align[IVC_ALIGN];
	};
};

static inline void ivc_invalidate_counter(struct ivc *ivc,
		dma_addr_t handle)
{
	if (!ivc->peer_device)
		return;
	dma_sync_single_for_cpu(ivc->peer_device, handle, IVC_ALIGN,
			DMA_FROM_DEVICE);
}

static inline void ivc_flush_counter(struct ivc *ivc, dma_addr_t handle)
{
	if (!ivc->peer_device)
		return;
	dma_sync_single_for_device(ivc->peer_device, handle, IVC_ALIGN,
			DMA_TO_DEVICE);
}

static inline int ivc_channel_empty(struct ivc *ivc,
		struct ivc_channel_header *ch)
{
	/*
	 * This function performs multiple checks on the same values with
	 * security implications, so create snapshots with ACCESS_ONCE() to
	 * ensure that these checks use the same values.
	 */
	uint32_t w_count = ACCESS_ONCE(ch->w_count);
	uint32_t r_count = ACCESS_ONCE(ch->r_count);

	/*
	 * Perform an over-full check to prevent denial of service attacks where
	 * a server could be easily fooled into believing that there's an
	 * extremely large number of frames ready, since receivers are not
	 * expected to check for full or over-full conditions.
	 *
	 * Although the channel isn't empty, this is an invalid case caused by
	 * a potentially malicious peer, so returning empty is safer, because it
	 * gives the impression that the channel has gone silent.
	 */
	if (w_count - r_count > ivc->nframes)
		return 1;

	return w_count == r_count;
}

static inline int ivc_channel_full(struct ivc *ivc,
		struct ivc_channel_header *ch)
{
	/*
	 * Invalid cases where the counters indicate that the queue is over
	 * capacity also appear full.
	 */
	return ACCESS_ONCE(ch->w_count) - ACCESS_ONCE(ch->r_count)
		>= ivc->nframes;
}

static inline uint32_t ivc_channel_avail_count(struct ivc *ivc,
		struct ivc_channel_header *ch)
{
	/*
	 * This function isn't expected to be used in scenarios where an
	 * over-full situation can lead to denial of service attacks. See the
	 * comment in ivc_channel_empty() for an explanation about special
	 * over-full considerations.
	 */
	return ACCESS_ONCE(ch->w_count) - ACCESS_ONCE(ch->r_count);
}

static inline void ivc_advance_tx(struct ivc *ivc)
{
	ACCESS_ONCE(ivc->tx_channel->w_count) =
		ACCESS_ONCE(ivc->tx_channel->w_count) + 1;

	if (ivc->w_pos == ivc->nframes - 1)
		ivc->w_pos = 0;
	else
		ivc->w_pos++;
}

static inline void ivc_advance_rx(struct ivc *ivc)
{
	ACCESS_ONCE(ivc->rx_channel->r_count) =
		ACCESS_ONCE(ivc->rx_channel->r_count) + 1;

	if (ivc->r_pos == ivc->nframes - 1)
		ivc->r_pos = 0;
	else
		ivc->r_pos++;
}

static inline int ivc_check_read(struct ivc *ivc)
{
	/*
	 * tx_channel->state is set locally, so it is not synchronized with
	 * state from the remote peer. The remote peer cannot reset its
	 * transmit counters until we've acknowledged its synchronization
	 * request, so no additional synchronization is required because an
	 * asynchronous transition of rx_channel->state to ivc_state_ack is not
	 * allowed.
	 */
	if (ivc->tx_channel->state != ivc_state_established)
		return -ECONNRESET;

	/*
	* Avoid unnecessary invalidations when performing repeated accesses to
	* an IVC channel by checking the old queue pointers first.
	* Synchronization is only necessary when these pointers indicate empty
	* or full.
	*/
	if (!ivc_channel_empty(ivc, ivc->rx_channel))
		return 0;

	ivc_invalidate_counter(ivc, ivc->rx_handle +
			offsetof(struct ivc_channel_header, w_count));
	return ivc_channel_empty(ivc, ivc->rx_channel) ? -ENOMEM : 0;
}

static inline int ivc_check_write(struct ivc *ivc)
{
	if (ivc->tx_channel->state != ivc_state_established)
		return -ECONNRESET;

	if (!ivc_channel_full(ivc, ivc->tx_channel))
		return 0;

	ivc_invalidate_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, r_count));
	return ivc_channel_full(ivc, ivc->tx_channel) ? -ENOMEM : 0;
}

int tegra_ivc_can_read(struct ivc *ivc)
{
	return ivc_check_read(ivc) == 0;
}
EXPORT_SYMBOL(tegra_ivc_can_read);

int tegra_ivc_can_write(struct ivc *ivc)
{
	return ivc_check_write(ivc) == 0;
}
EXPORT_SYMBOL(tegra_ivc_can_write);

int tegra_ivc_tx_empty(struct ivc *ivc)
{
	ivc_invalidate_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, r_count));
	return ivc_channel_empty(ivc, ivc->tx_channel);
}
EXPORT_SYMBOL(tegra_ivc_tx_empty);

uint32_t tegra_ivc_tx_frames_available(struct ivc *ivc)
{
	ivc_invalidate_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, r_count));
	return ivc->nframes - (ACCESS_ONCE(ivc->tx_channel->w_count) -
			ACCESS_ONCE(ivc->tx_channel->r_count));
}
EXPORT_SYMBOL(tegra_ivc_tx_frames_available);

static void *ivc_frame_pointer(struct ivc *ivc, struct ivc_channel_header *ch,
		uint32_t frame)
{
	BUG_ON(frame >= ivc->nframes);
	return (void *)((uintptr_t)(ch + 1) + ivc->frame_size * frame);
}

static inline dma_addr_t ivc_frame_handle(struct ivc *ivc,
		dma_addr_t channel_handle, uint32_t frame)
{
	BUG_ON(!ivc->peer_device);
	BUG_ON(frame >= ivc->nframes);
	return channel_handle + sizeof(struct ivc_channel_header) +
		ivc->frame_size * frame;
}

static inline void ivc_invalidate_frame(struct ivc *ivc,
		dma_addr_t channel_handle, unsigned frame, int offset, int len)
{
	if (!ivc->peer_device)
		return;
	dma_sync_single_for_cpu(ivc->peer_device,
			ivc_frame_handle(ivc, channel_handle, frame) + offset,
			len, DMA_FROM_DEVICE);
}

static inline void ivc_flush_frame(struct ivc *ivc, dma_addr_t channel_handle,
		unsigned frame, int offset, int len)
{
	if (!ivc->peer_device)
		return;
	dma_sync_single_for_device(ivc->peer_device,
			ivc_frame_handle(ivc, channel_handle, frame) + offset,
			len, DMA_TO_DEVICE);
}

static int ivc_read_frame(struct ivc *ivc, void *buf, void __user *user_buf,
		size_t max_read)
{
	const void *src;
	int result;

	BUG_ON(buf && user_buf);

	if (max_read > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_read(ivc);
	if (result)
		return result;

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_handle, ivc->r_pos, 0, max_read);
	src = ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);

	/*
	 * When compiled with optimizations, different versions of this
	 * function should be inlined into tegra_ivc_read_frame() or
	 * tegra_ivc_read_frame_user(). This should ensure that the user
	 * version does not add overhead to the kernel version.
	 */
	if (buf) {
		memcpy(buf, src, max_read);
	} else if (user_buf) {
		if (copy_to_user(user_buf, src, max_read))
			return -EFAULT;
	} else
		BUG();

	ivc_advance_rx(ivc);
	ivc_flush_counter(ivc, ivc->rx_handle +
			offsetof(struct ivc_channel_header, r_count));

	/*
	 * Ensure our write to r_pos occurs before our read from w_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from full to non-full.
	 * The available count can only asynchronously increase, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_invalidate_counter(ivc, ivc->rx_handle +
		offsetof(struct ivc_channel_header, w_count));

	if (ivc_channel_avail_count(ivc, ivc->rx_channel) == ivc->nframes - 1)
		ivc->notify(ivc);

	return (int)max_read;
}

int tegra_ivc_read(struct ivc *ivc, void *buf, size_t max_read)
{
	return ivc_read_frame(ivc, buf, NULL, max_read);
}
EXPORT_SYMBOL(tegra_ivc_read);

int tegra_ivc_read_user(struct ivc *ivc, void __user *buf, size_t max_read)
{
	return ivc_read_frame(ivc, NULL, buf, max_read);
}
EXPORT_SYMBOL(tegra_ivc_read_user);

/* peek in the next rx buffer at offset off, the count bytes */
int tegra_ivc_read_peek(struct ivc *ivc, void *buf, size_t off, size_t count)
{
	const void *src;
	int result;

	if (off > ivc->frame_size || off + count > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_read(ivc);
	if (result)
		return result;

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_handle, ivc->r_pos, off, count);
	src = ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);

	memcpy(buf, (void *)((uintptr_t)src + off), count);

	/* note, no interrupt is generated */

	return (int)count;
}
EXPORT_SYMBOL(tegra_ivc_read_peek);

/* directly peek at the next frame rx'ed */
void *tegra_ivc_read_get_next_frame(struct ivc *ivc)
{
	int result = ivc_check_read(ivc);
	if (result)
		return ERR_PTR(result);

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_handle, ivc->r_pos, 0,
			ivc->frame_size);
	return ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);
}
EXPORT_SYMBOL(tegra_ivc_read_get_next_frame);

int tegra_ivc_read_advance(struct ivc *ivc)
{
	/*
	 * No read barriers or synchronization here: the caller is expected to
	 * have already observed the channel non-empty. This check is just to
	 * catch programming errors.
	 */
	int result = ivc_check_read(ivc);
	if (result)
		return result;

	ivc_advance_rx(ivc);
	ivc_flush_counter(ivc, ivc->rx_handle +
			offsetof(struct ivc_channel_header, r_count));

	/*
	 * Ensure our write to r_pos occurs before our read from w_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from full to non-full.
	 * The available count can only asynchronously increase, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_invalidate_counter(ivc, ivc->rx_handle +
		offsetof(struct ivc_channel_header, w_count));

	if (ivc_channel_avail_count(ivc, ivc->rx_channel) == ivc->nframes - 1)
		ivc->notify(ivc);

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_read_advance);

static int ivc_write_frame(struct ivc *ivc, const void *buf,
		const void __user *user_buf, size_t size)
{
	void *p;
	int result;

	BUG_ON(buf && user_buf);

	if (size > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_write(ivc);
	if (result)
		return result;

	p = ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);

	/*
	 * When compiled with optimizations, different versions of this
	 * function should be inlined into tegra_ivc_write_frame() or
	 * tegra_ivc_write_frame_user(). This should ensure that the user
	 * version does not add overhead to the kernel version.
	 */
	if (buf) {
		memcpy(p, buf, size);
	} else if (user_buf) {
		if (copy_from_user(p, user_buf, size))
			return -EFAULT;
	} else
		BUG();

	memset(p + size, 0, ivc->frame_size - size);
	ivc_flush_frame(ivc, ivc->tx_handle, ivc->w_pos, 0, size);

	/*
	 * Ensure that updated data is visible before the w_pos counter
	 * indicates that it is ready.
	 */
	ivc_wmb();

	ivc_advance_tx(ivc);
	ivc_flush_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, w_count));

	/*
	 * Ensure our write to w_pos occurs before our read from r_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from empty to non-empty.
	 * The available count can only asynchronously decrease, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_invalidate_counter(ivc, ivc->tx_handle +
		offsetof(struct ivc_channel_header, r_count));

	if (ivc_channel_avail_count(ivc, ivc->tx_channel) == 1)
		ivc->notify(ivc);

	return (int)size;
}

int tegra_ivc_write(struct ivc *ivc, const void *buf, size_t size)
{
	return ivc_write_frame(ivc, buf, NULL, size);
}
EXPORT_SYMBOL(tegra_ivc_write);

int tegra_ivc_write_user(struct ivc *ivc, const void __user *user_buf,
		size_t size)
{
	return ivc_write_frame(ivc, NULL, user_buf, size);
}
EXPORT_SYMBOL(tegra_ivc_write_user);

/* poke in the next tx buffer at offset off, the count bytes */
int tegra_ivc_write_poke(struct ivc *ivc, const void *buf, size_t off,
		size_t count)
{
	void *dest;
	int result;

	if (off > ivc->frame_size || off + count > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_write(ivc);
	if (result)
		return result;

	dest = ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);
	memcpy(dest + off, buf, count);

	return (int)count;
}
EXPORT_SYMBOL(tegra_ivc_write_poke);

/* directly poke at the next frame to be tx'ed */
void *tegra_ivc_write_get_next_frame(struct ivc *ivc)
{
	int result = ivc_check_write(ivc);
	if (result)
		return ERR_PTR(result);

	return ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);
}
EXPORT_SYMBOL(tegra_ivc_write_get_next_frame);

/* advance the tx buffer */
int tegra_ivc_write_advance(struct ivc *ivc)
{
	int result = ivc_check_write(ivc);
	if (result)
		return result;

	ivc_flush_frame(ivc, ivc->tx_handle, ivc->w_pos, 0, ivc->frame_size);

	/*
	 * Order any possible stores to the frame before update of w_pos.
	 */
	ivc_wmb();

	ivc_advance_tx(ivc);
	ivc_flush_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, w_count));

	/*
	 * Ensure our write to w_pos occurs before our read from r_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from empty to non-empty.
	 * The available count can only asynchronously decrease, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_invalidate_counter(ivc, ivc->tx_handle +
		offsetof(struct ivc_channel_header, r_count));

	if (ivc_channel_avail_count(ivc, ivc->tx_channel) == 1)
		ivc->notify(ivc);

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_write_advance);

void tegra_ivc_channel_reset(struct ivc *ivc)
{
	ivc->tx_channel->state = ivc_state_sync;
	ivc_flush_counter(ivc, ivc->tx_handle +
			offsetof(struct ivc_channel_header, w_count));
	ivc->notify(ivc);
}
EXPORT_SYMBOL(tegra_ivc_channel_reset);

/*
 * ===============================================================
 *  IVC State Transition Table - see tegra_ivc_channel_notified()
 * ===============================================================
 *
 *	local	remote	action
 *	-----	------	-----------------------------------
 *	SYNC	EST	<none>
 *	SYNC	ACK	reset counters; move to EST; notify
 *	SYNC	SYNC	reset counters; move to ACK; notify
 *	ACK	EST	move to EST; notify
 *	ACK	ACK	move to EST; notify
 *	ACK	SYNC	reset counters; move to ACK; notify
 *	EST	EST	<none>
 *	EST	ACK	<none>
 *	EST	SYNC	reset counters; move to ACK; notify
 *
 * ===============================================================
 */

int tegra_ivc_channel_notified(struct ivc *ivc)
{
	enum ivc_state peer_state;

	/* Copy the receiver's state out of shared memory. */
	ivc_invalidate_counter(ivc, ivc->rx_handle +
			offsetof(struct ivc_channel_header, w_count));
	peer_state = ACCESS_ONCE(ivc->rx_channel->state);

	if (peer_state == ivc_state_sync) {
		/*
		 * Order observation of ivc_state_sync before stores clearing
		 * tx_channel.
		 */
		ivc_rmb();

		/*
		 * Reset tx_channel counters. The remote end is in the SYNC
		 * state and won't make progress until we change our state,
		 * so the counters are not in use at this time.
		 */
		ivc->tx_channel->w_count = 0;
		ivc->rx_channel->r_count = 0;

		ivc->w_pos = 0;
		ivc->r_pos = 0;

		/*
		 * Ensure that counters appear cleared before new state can be
		 * observed.
		 */
		ivc_wmb();

		/*
		 * Move to ACK state. We have just cleared our counters, so it
		 * is now safe for the remote end to start using these values.
		 */
		ivc->tx_channel->state = ivc_state_ack;
		ivc_flush_counter(ivc, ivc->tx_handle +
				offsetof(struct ivc_channel_header, w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else if (ivc->tx_channel->state == ivc_state_sync &&
			peer_state == ivc_state_ack) {
		/*
		 * Order observation of ivc_state_sync before stores clearing
		 * tx_channel.
		 */
		ivc_rmb();

		/*
		 * Reset tx_channel counters. The remote end is in the ACK
		 * state and won't make progress until we change our state,
		 * so the counters are not in use at this time.
		 */
		ivc->tx_channel->w_count = 0;
		ivc->rx_channel->r_count = 0;

		ivc->w_pos = 0;
		ivc->r_pos = 0;

		/*
		 * Ensure that counters appear cleared before new state can be
		 * observed.
		 */
		ivc_wmb();

		/*
		 * Move to ESTABLISHED state. We know that the remote end has
		 * already cleared its counters, so it is safe to start
		 * writing/reading on this channel.
		 */
		ivc->tx_channel->state = ivc_state_established;
		ivc_flush_counter(ivc, ivc->tx_handle +
				offsetof(struct ivc_channel_header, w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else if (ivc->tx_channel->state == ivc_state_ack) {
		/*
		 * At this point, we have observed the peer to be in either
		 * the ACK or ESTABLISHED state. Next, order observation of
		 * peer state before storing to tx_channel.
		 */
		ivc_rmb();

		/*
		 * Move to ESTABLISHED state. We know that we have previously
		 * cleared our counters, and we know that the remote end has
		 * cleared its counters, so it is safe to start writing/reading
		 * on this channel.
		 */
		ivc->tx_channel->state = ivc_state_established;
		ivc_flush_counter(ivc, ivc->tx_handle +
				offsetof(struct ivc_channel_header, w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else {
		/*
		 * There is no need to handle any further action. Either the
		 * channel is already fully established, or we are waiting for
		 * the remote end to catch up with our current state. Refer
		 * to the diagram in "IVC State Transition Table" above.
		 */
	}

	return ivc->tx_channel->state == ivc_state_established ? 0 : -EAGAIN;
}
EXPORT_SYMBOL(tegra_ivc_channel_notified);

/*
 * Temporary routine for re-synchronizing the channel across a reboot.
 */
int tegra_ivc_channel_sync(struct ivc *ivc)
{
	if ((ivc == NULL) || (ivc->nframes == 0)) {
		return -EINVAL;
	} else {
		ivc->w_pos = ivc->tx_channel->w_count % ivc->nframes;
		ivc->r_pos = ivc->rx_channel->r_count % ivc->nframes;
	}
	return 0;
}
EXPORT_SYMBOL(tegra_ivc_channel_sync);

size_t tegra_ivc_align(size_t size)
{
	return (size + (IVC_ALIGN - 1)) & ~(IVC_ALIGN - 1);
}
EXPORT_SYMBOL(tegra_ivc_align);

unsigned tegra_ivc_total_queue_size(unsigned queue_size)
{
	if (queue_size & (IVC_ALIGN - 1)) {
		pr_err("%s: queue_size (%u) must be %u-byte aligned\n",
				__func__, queue_size, IVC_ALIGN);
		return 0;
	}
	return queue_size + sizeof(struct ivc_channel_header);
}
EXPORT_SYMBOL(tegra_ivc_total_queue_size);

static int check_ivc_params(uintptr_t queue_base1, uintptr_t queue_base2,
		unsigned nframes, unsigned frame_size)
{
	BUG_ON(offsetof(struct ivc_channel_header, w_count) & (IVC_ALIGN - 1));
	BUG_ON(offsetof(struct ivc_channel_header, r_count) & (IVC_ALIGN - 1));
	BUG_ON(sizeof(struct ivc_channel_header) & (IVC_ALIGN - 1));

	if ((uint64_t)nframes * (uint64_t)frame_size >= 0x100000000) {
		pr_err("nframes * frame_size overflows\n");
		return -EINVAL;
	}

	/*
	 * The headers must at least be aligned enough for counters
	 * to be accessed atomically.
	 */
	if (queue_base1 & (IVC_ALIGN - 1)) {
		pr_err("ivc channel start not aligned: %lx\n", queue_base1);
		return -EINVAL;
	}
	if (queue_base2 & (IVC_ALIGN - 1)) {
		pr_err("ivc channel start not aligned: %lx\n", queue_base2);
		return -EINVAL;
	}

	if (frame_size & (IVC_ALIGN - 1)) {
		pr_err("frame size not adequately aligned: %u\n", frame_size);
		return -EINVAL;
	}

	if (queue_base1 < queue_base2) {
		if (queue_base1 + frame_size * nframes > queue_base2) {
			pr_err("queue regions overlap: %lx + %x, %x\n",
					queue_base1, frame_size,
					frame_size * nframes);
			return -EINVAL;
		}
	} else {
		if (queue_base2 + frame_size * nframes > queue_base1) {
			pr_err("queue regions overlap: %lx + %x, %x\n",
					queue_base2, frame_size,
					frame_size * nframes);
			return -EINVAL;
		}
	}

	return 0;
}

static int tegra_ivc_init_body(struct ivc *ivc, uintptr_t rx_base,
		dma_addr_t rx_handle, uintptr_t tx_base, dma_addr_t tx_handle,
		unsigned nframes, unsigned frame_size,
		struct device *peer_device, void (*notify)(struct ivc *))
{
	size_t queue_size;

	int result = check_ivc_params(rx_base, tx_base, nframes, frame_size);
	if (result)
		return result;

	BUG_ON(!ivc);
	BUG_ON(!notify);

	queue_size = tegra_ivc_total_queue_size(nframes * frame_size);

	/*
	 * All sizes that can be returned by communication functions should
	 * fit in an int.
	 */
	if (frame_size > INT_MAX)
		return -E2BIG;

	ivc->rx_channel = (struct ivc_channel_header *)rx_base;
	ivc->tx_channel = (struct ivc_channel_header *)tx_base;

	if (peer_device) {
		if (rx_handle != DMA_ERROR_CODE) {
			ivc->rx_handle = rx_handle;
			ivc->tx_handle = tx_handle;
		} else {
			ivc->rx_handle = dma_map_single(peer_device,
				ivc->rx_channel, queue_size, DMA_BIDIRECTIONAL);
			if (ivc->rx_handle == DMA_ERROR_CODE)
				return -ENOMEM;

			ivc->tx_handle = dma_map_single(peer_device,
				ivc->tx_channel, queue_size, DMA_BIDIRECTIONAL);
			if (ivc->tx_handle == DMA_ERROR_CODE) {
				dma_unmap_single(peer_device, ivc->rx_handle,
					queue_size, DMA_BIDIRECTIONAL);
				return -ENOMEM;
			}
		}
	}

	ivc->notify = notify;
	ivc->frame_size = frame_size;
	ivc->nframes = nframes;
	ivc->peer_device = peer_device;

	/*
	 * These values aren't necessarily correct until the channel has been
	 * reset.
	 */
	ivc->w_pos = 0;
	ivc->r_pos = 0;

	return 0;
}

int tegra_ivc_init(struct ivc *ivc, uintptr_t rx_base, uintptr_t tx_base,
		unsigned nframes, unsigned frame_size,
		struct device *peer_device, void (*notify)(struct ivc *))
{
	return tegra_ivc_init_body(ivc, rx_base, DMA_ERROR_CODE, tx_base,
		DMA_ERROR_CODE, nframes, frame_size, peer_device, notify);
}
EXPORT_SYMBOL(tegra_ivc_init);

int tegra_ivc_init_with_dma_handle(struct ivc *ivc, uintptr_t rx_base,
		dma_addr_t rx_handle, uintptr_t tx_base, dma_addr_t tx_handle,
		unsigned nframes, unsigned frame_size,
		struct device *peer_device, void (*notify)(struct ivc *))
{
	return tegra_ivc_init_body(ivc, rx_base, rx_handle, tx_base,
		tx_handle, nframes, frame_size, peer_device, notify);
}
EXPORT_SYMBOL(tegra_ivc_init_with_dma_handle);
