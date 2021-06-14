/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/anon_inodes.h>
#include <uapi/linux/kfd_ioctl.h>
#include "kfd_debug_events.h"
#include "kfd_priv.h"
#include "kfd_topology.h"

/* poll and read functions */
static __poll_t kfd_dbg_ev_poll(struct file *, struct poll_table_struct *);
static ssize_t kfd_dbg_ev_read(struct file *, char __user *, size_t, loff_t *);
static int kfd_dbg_ev_release(struct inode *, struct file *);

/* fd name */
static const char kfd_dbg_name[] = "kfd_debug";

/* fops for polling, read and ioctl */
static const struct file_operations kfd_dbg_ev_fops = {
	.owner = THIS_MODULE,
	.poll = kfd_dbg_ev_poll,
	.read = kfd_dbg_ev_read,
	.release = kfd_dbg_ev_release
};

struct kfd_debug_event_priv {
	struct kfifo fifo;
	wait_queue_head_t wait_queue;
	int max_debug_events;
};

/* poll on wait queue of file */
static __poll_t kfd_dbg_ev_poll(struct file *filep,
				struct poll_table_struct *wait)
{
	struct kfd_debug_event_priv *dbg_ev_priv = filep->private_data;

	__poll_t mask = 0;

	/* pending event have been queue'd via interrupt */
	poll_wait(filep, &dbg_ev_priv->wait_queue, wait);
	mask |= !kfifo_is_empty(&dbg_ev_priv->fifo) ?
						POLLIN | POLLRDNORM : mask;

	return mask;
}

/* read based on wait entries and return types found */
static ssize_t kfd_dbg_ev_read(struct file *filep, char __user *user,
			       size_t size, loff_t *offset)
{
	int ret, copied;
	struct kfd_debug_event_priv *dbg_ev_priv = filep->private_data;

	ret = kfifo_to_user(&dbg_ev_priv->fifo, user, size, &copied);

	if (ret || !copied) {
		pr_debug("KFD DEBUG EVENT: Failed to read poll fd (%i) (%i)\n",
								ret, copied);
		return ret ? ret : -EAGAIN;
	}

	return copied;
}

static int kfd_dbg_ev_release(struct inode *inode, struct file *filep)
{
	struct kfd_debug_event_priv *dbg_ev_priv = filep->private_data;

	kfifo_free(&dbg_ev_priv->fifo);
	kfree(dbg_ev_priv);

	return 0;
}

/* query pending events and return queue_id, event_type and is_suspended */
#define KFD_DBG_EV_SET_SUSPEND_STATE(x, s)			\
	((x) = (s) ? (x) | KFD_DBG_EV_STATUS_SUSPENDED :	\
		(x) & ~KFD_DBG_EV_STATUS_SUSPENDED)

#define KFD_DBG_EV_SET_EVENT_TYPE(x, e)				\
	((x) = ((x) & ~(KFD_DBG_EV_STATUS_TRAP			\
		| KFD_DBG_EV_STATUS_VMFAULT)) | (e))

#define KFD_DBG_EV_SET_NEW_QUEUE_STATE(x, n)			\
	((x) = (n) ? (x) | KFD_DBG_EV_STATUS_NEW_QUEUE :	\
		(x) & ~KFD_DBG_EV_STATUS_NEW_QUEUE)

uint32_t kfd_dbg_get_queue_status_word(struct queue *q, int flags)
{
	uint32_t queue_status_word = 0;

	KFD_DBG_EV_SET_EVENT_TYPE(queue_status_word,
				  READ_ONCE(q->properties.debug_event_type));
	KFD_DBG_EV_SET_SUSPEND_STATE(queue_status_word,
				  q->properties.is_suspended);
	KFD_DBG_EV_SET_NEW_QUEUE_STATE(queue_status_word,
				  q->properties.is_new);

	if (flags & KFD_DBG_EV_FLAG_CLEAR_STATUS)
		WRITE_ONCE(q->properties.debug_event_type, 0);

	return queue_status_word;
}

int kfd_dbg_ev_query_debug_event(struct kfd_process_device *pdd,
		      unsigned int *queue_id,
		      unsigned int flags,
		      uint32_t *event_status)
{
	struct process_queue_manager *pqm;
	struct process_queue_node *pqn;
	struct queue *q;
	int ret = 0;

	if (!pdd || !pdd->process)
		return -ENODATA;

	/* lock process events to update event queues */
	mutex_lock(&pdd->process->event_mutex);
	pqm = &pdd->process->pqm;

	if (*queue_id != KFD_INVALID_QUEUEID) {
		q = pqm_get_user_queue(pqm, *queue_id);

		if (!q) {
			ret = -EINVAL;
			goto out;
		}

		*event_status = kfd_dbg_get_queue_status_word(q, flags);
		q->properties.is_new = false;
		goto out;
	}

	list_for_each_entry(pqn, &pqm->queues, process_queue_list) {
		unsigned int tmp_status;

		if (!pqn->q)
			continue;

		tmp_status = kfd_dbg_get_queue_status_word(pqn->q, flags);
		if (tmp_status & (KFD_DBG_EV_STATUS_TRAP |
						KFD_DBG_EV_STATUS_VMFAULT)) {
			*queue_id = pqn->q->properties.queue_id;
			*event_status = tmp_status;
			pqn->q->properties.is_new = false;
			goto out;
		}
	}

	ret = -EAGAIN;

out:
	mutex_unlock(&pdd->process->event_mutex);
	return ret;
}

/* create event queue struct associated with process per device */
static int kfd_create_event_queue(struct kfd_process_device *pdd,
				struct kfd_debug_event_priv *dbg_ev_priv)
{
	struct process_queue_manager *pqm;
	struct process_queue_node *pqn;
	struct kfd_topology_device *tdev;
	int ret;

	if (!pdd || !pdd->process)
		return -ESRCH;

	tdev = kfd_topology_device_by_id(pdd->dev->id);

	dbg_ev_priv->max_debug_events = tdev->node_props.simd_count
				* tdev->node_props.max_waves_per_simd;

	ret = kfifo_alloc(&dbg_ev_priv->fifo,
				dbg_ev_priv->max_debug_events, GFP_KERNEL);

	if (ret)
		return ret;

	init_waitqueue_head(&dbg_ev_priv->wait_queue);

	pqm = &pdd->process->pqm;

	/* to reset queue pending status - TBD need init in queue creation */
	list_for_each_entry(pqn, &pqm->queues, process_queue_list) {
		if (pqn->q->device == pdd->dev)
			WRITE_ONCE(pqn->q->properties.debug_event_type, 0);
	}

	return ret;
}

/* update process device, write to kfifo and wake up wait queue  */
static void kfd_dbg_ev_update_event_queue(struct kfd_process_device *pdd,
					  unsigned int doorbell_id,
					  bool is_vmfault)
{
	struct process_queue_manager *pqm;
	struct process_queue_node *pqn;
	char fifo_output;

	if (!pdd->debug_trap_enabled)
		return;

	pqm = &pdd->process->pqm;

	/* iterate through each queue */
	list_for_each_entry(pqn, &pqm->queues,
				process_queue_list) {
		long bit_to_set;
		struct kfd_debug_event_priv *dbg_ev_priv;

		if (!pqn->q)
			continue;

		if (pqn->q->device != pdd->dev)
			continue;

		if (pqn->q->doorbell_id != doorbell_id && !is_vmfault)
			continue;

		bit_to_set = is_vmfault ?
			KFD_DBG_EV_STATUS_VMFAULT_BIT :
			KFD_DBG_EV_STATUS_TRAP_BIT;

		set_bit(bit_to_set, &pqn->q->properties.debug_event_type);

		fifo_output = is_vmfault ? 'v' : 't';

		dbg_ev_priv = pdd->dbg_ev_file->private_data;

		kfifo_in(&dbg_ev_priv->fifo, &fifo_output, 1);

		wake_up_all(&dbg_ev_priv->wait_queue);

		if (!is_vmfault)
			break;
	}
}

/* set pending event queue entry from ring entry  */
void kfd_set_dbg_ev_from_interrupt(struct kfd_dev *dev,
				   unsigned int pasid,
				   uint32_t doorbell_id,
				   bool is_vmfault)
{
	struct kfd_process *p;
	struct kfd_process_device *pdd;

	p = kfd_lookup_process_by_pasid(pasid);

	if (!p)
		return;

	pdd = kfd_get_process_device_data(dev, p);

	if (!pdd) {
		kfd_unref_process(p);
		return;
	}

	mutex_lock(&p->event_mutex);

	kfd_dbg_ev_update_event_queue(pdd, doorbell_id, is_vmfault);

	mutex_unlock(&p->event_mutex);

	kfd_unref_process(p);
}

/* enable debug and return file pointer struct */
int kfd_dbg_ev_enable(struct kfd_process_device *pdd)
{
	struct kfd_debug_event_priv  *dbg_ev_priv;
	int ret;

	if (!pdd || !pdd->process)
		return -ESRCH;

	dbg_ev_priv = kzalloc(sizeof(struct kfd_debug_event_priv), GFP_KERNEL);

	if (!dbg_ev_priv)
		return -ENOMEM;

	mutex_lock(&pdd->process->event_mutex);

	ret = kfd_create_event_queue(pdd, dbg_ev_priv);

	mutex_unlock(&pdd->process->event_mutex);

	if (ret)
		return ret;

	ret = anon_inode_getfd(kfd_dbg_name, &kfd_dbg_ev_fops,
				(void *)dbg_ev_priv, 0);

	if (ret < 0) {
		kfree(dbg_ev_priv);
		return ret;
	}

	pdd->dbg_ev_file = fget(ret);

	return ret;
}
