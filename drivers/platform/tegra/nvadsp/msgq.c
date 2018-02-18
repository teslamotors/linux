/*
 * ADSP circular message queue
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
 */

#include <linux/tegra_nvadsp.h>

#define msgq_wmemcpy(dest, src, words) \
	memcpy(dest, src, (words) * sizeof(int32_t))


/**
 * msgq_init - Initialize message queue
 * @msgq:           pointer to the client message queue
 * @size:           size of message queue in words
 *                  size will be capped to MSGQ_MAX_WSIZE
 *
 * This function returns 0 if no error has occurred.
 *
 * The message queue requires space for the queue to be
 * preallocated and should only be initialized once. The queue
 * space immediately follows the queue header and begins at
 * msgq_t::message_queue.  All messages are queued directly with
 * no pointer address space translation.
 *
 *
 */
void msgq_init(msgq_t *msgq, int32_t size)
{
	if (MSGQ_MAX_QUEUE_WSIZE < size) {
		/* cap the maximum size */
		pr_info("msgq_init: %d size capped to MSGQ_MAX_QUEUE_WSIZE\n",
			size);
		size = MSGQ_MAX_QUEUE_WSIZE;
	}

	msgq->size = size;
	msgq->read_index = 0;
	msgq->write_index = 0;
}
EXPORT_SYMBOL(msgq_init);
/**
 * msgq_queue_message - Queues a message in the queue
 * @msgq:           pointer to the client message queue
 * @message:        Message buffer to copy from
 *
 * This function returns 0 if no error has occurred. ERR_NO_MEMORY will
 * be returned if no space is available in the queue for the
 * entire message.  On ERR_NO_MEMORY, it may be possible the
 * queue size was capped at init time to MSGQ_MAX_WSIZE if an
 * unreasonable size was sepecified.
 *
 *
 */
int32_t msgq_queue_message(msgq_t *msgq, const msgq_message_t *message)
{
	int32_t ret = 0;
	if (msgq && message) {
		int32_t ri = msgq->read_index;
		int32_t wi = msgq->write_index;
		bool wrap = ri <= wi;
		int32_t *start = msgq->queue;
		int32_t *end = &msgq->queue[msgq->size];
		int32_t *first = &msgq->queue[wi];
		int32_t *last = &msgq->queue[ri];
		int32_t qremainder = wrap ? end - first : last - first;
		int32_t qsize = wrap ? qremainder + (last - start) : qremainder;
		int32_t msize = &message->payload[message->size] -
			(int32_t *)message;

		if (qsize <= msize) {
			/* don't allow read == write */
			ret = -ENOSPC;
		} else if (msize < qremainder) {
			msgq_wmemcpy(first, message, msize);
			msgq->write_index = wi + MSGQ_MESSAGE_HEADER_WSIZE +
				message->size;
		} else {
			/* message wrapped */
			msgq_wmemcpy(first, message, qremainder);
			msgq_wmemcpy(msgq->queue, (int32_t *)message +
				qremainder, msize - qremainder);
			msgq->write_index = wi + MSGQ_MESSAGE_HEADER_WSIZE +
				message->size - msgq->size;
		}
	} else {
		pr_err("NULL: msgq %p message %p\n", msgq, message);
		ret = -EFAULT; /* Bad Address */
	}

	return ret;
}
EXPORT_SYMBOL(msgq_queue_message);
/**
 * msgq_dequeue_message - Dequeues a message from the queue
 * @msgq:           pointer to the client message queue
 * @message:        Message buffer to copy to or
 *                  NULL to discard the current message
 *
 * This function returns 0 if no error has occurred.
 * msgq_message_t::size will be set to the size of the message
 * in words. ERR_NO_MEMORY will be returned if the buffer is too small
 * for the queued message. ERR_NO_MSG will be returned if there is no
 * message in the queue.
 *
 *
 */
int32_t msgq_dequeue_message(msgq_t *msgq, msgq_message_t *message)
{
	int32_t ret = 0;
	int32_t ri;
	int32_t wi;
	msgq_message_t *msg;

	if (!msgq) {
		pr_err("NULL: msgq %p\n", msgq);
		return -EFAULT; /* Bad Address */
	}

	ri = msgq->read_index;
	wi = msgq->write_index;
	msg = (msgq_message_t *)&msgq->queue[msgq->read_index];

	if (ri == wi) {
		/* empty queue */
		if (message)
			message->size = 0;
		ret = -ENOMSG;
	} else if (!message) {
		/* no input buffer, discard top message */
		ri += MSGQ_MESSAGE_HEADER_WSIZE + msg->size;
		msgq->read_index = ri < msgq->size ? ri : ri - msgq->size;
	} else if (message->size < msg->size) {
		/* return buffer too small */
		message->size = msg->size;
		ret = -ENOSPC;
	} else {
		/* copy message to the output buffer */
		int32_t msize = MSGQ_MESSAGE_HEADER_WSIZE + msg->size;
		int32_t *first = &msgq->queue[msgq->read_index];
		int32_t *end = &msgq->queue[msgq->size];
		int32_t qremainder = end - first;

		if (msize < qremainder) {
			msgq_wmemcpy(message, first, msize);
			msgq->read_index = ri + MSGQ_MESSAGE_HEADER_WSIZE +
				msg->size;
		} else {
			/* message wrapped */
			msgq_wmemcpy(message, first, qremainder);
			msgq_wmemcpy((int32_t *)message + qremainder,
				msgq->queue, msize - qremainder);
			msgq->read_index = ri + MSGQ_MESSAGE_HEADER_WSIZE +
				msg->size - msgq->size;
		}
	}

	return ret;
}
EXPORT_SYMBOL(msgq_dequeue_message);
