/*
 * Media device
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/export.h>
#include <linux/ioctl.h>
#include <linux/media.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <media/media-device.h>
#include <media/media-devnode.h>
#include <media/media-entity.h>

static char *__request_state[] = {
	"IDLE",
	"QUEUED",
	"DELETED",
	"COMPLETED",
};

#define request_state(i)			\
	((i) < ARRAY_SIZE(__request_state) ? __request_state[i] : "UNKNOWN")

struct media_device_fh {
	struct media_devnode_fh fh;
	struct list_head requests;
	struct {
		struct list_head head;
		wait_queue_head_t wait;
		atomic_t sequence;
	} kevents;
};

static inline struct media_device_fh *media_device_fh(struct file *filp)
{
	return container_of(filp->private_data, struct media_device_fh, fh);
}

/* -----------------------------------------------------------------------------
 * Requests
 */

/**
 * media_device_request_find - Find a request based from its ID
 * @mdev: The media device
 * @reqid: The request ID
 *
 * Find and return the request associated with the given ID, or NULL if no such
 * request exists.
 *
 * When the function returns a non-NULL request it increases its reference
 * count. The caller is responsible for releasing the reference by calling
 * media_device_request_put() on the request.
 */
struct media_device_request *
media_device_request_find(struct media_device *mdev, u16 reqid)
{
	struct media_device_request *req;
	unsigned long flags;
	bool found = false;

	spin_lock_irqsave(&mdev->req_lock, flags);
	list_for_each_entry(req, &mdev->requests, list) {
		if (req->id == reqid) {
			kref_get(&req->kref);
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&mdev->req_lock, flags);

	if (!found) {
		dev_dbg(mdev->dev,
			"request: can't find %u\n", reqid);
		return NULL;
	}

	return req;
}
EXPORT_SYMBOL_GPL(media_device_request_find);

void media_device_request_get(struct media_device_request *req)
{
	kref_get(&req->kref);
}
EXPORT_SYMBOL_GPL(media_device_request_get);

static void media_device_request_queue_event(struct media_device *mdev,
					     struct media_device_request *req,
					     struct media_device_fh *fh)
{
	struct media_kevent *kev = req->kev;
	struct media_event *ev = &kev->ev;

	lockdep_assert_held(&mdev->req_lock);

	ev->sequence = atomic_inc_return(&fh->kevents.sequence);
	ev->type = MEDIA_EVENT_TYPE_REQUEST_COMPLETE;
	ev->req_complete.id = req->id;

	list_add(&kev->list, &fh->kevents.head);
	req->kev = NULL;
	req->state = MEDIA_DEVICE_REQUEST_STATE_COMPLETE;
	wake_up(&fh->kevents.wait);
}

static void media_device_request_release(struct kref *kref)
{
	struct media_device_request *req =
		container_of(kref, struct media_device_request, kref);
	struct media_device *mdev = req->mdev;

	dev_dbg(mdev->dev, "release request %u\n", req->id);

	ida_simple_remove(&mdev->req_ids, req->id);

	kfree(req->kev);
	req->kev = NULL;

	mdev->ops->req_free(mdev, req);
}

void media_device_request_put(struct media_device_request *req)
{
	kref_put(&req->kref, media_device_request_release);
}
EXPORT_SYMBOL_GPL(media_device_request_put);

static int media_device_request_alloc(struct media_device *mdev,
				      struct file *filp,
				      struct media_request_cmd *cmd)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_device_request *req;
	struct media_kevent *kev;
	unsigned long flags;
	int id = ida_simple_get(&mdev->req_ids, 1, 0, GFP_KERNEL);
	int ret;

	if (id < 0) {
		dev_dbg(mdev->dev, "request: unable to obtain new id\n");
		return id;
	}

	kev = kzalloc(sizeof(*kev), GFP_KERNEL);
	if (!kev) {
		ret = -ENOMEM;
		goto out_ida_simple_remove;
	}

	req = mdev->ops->req_alloc(mdev);
	if (!req) {
		ret = -ENOMEM;
		goto out_kev_free;
	}

	req->mdev = mdev;
	req->id = id;
	req->filp = filp;
	req->state = MEDIA_DEVICE_REQUEST_STATE_IDLE;
	req->kev = kev;
	kref_init(&req->kref);

	spin_lock_irqsave(&mdev->req_lock, flags);
	list_add_tail(&req->list, &mdev->requests);
	list_add_tail(&req->fh_list, &fh->requests);
	spin_unlock_irqrestore(&mdev->req_lock, flags);

	cmd->request = req->id;

	dev_dbg(mdev->dev, "request: allocated id %u\n", req->id);

	return 0;

out_kev_free:
	kfree(kev);

out_ida_simple_remove:
	ida_simple_remove(&mdev->req_ids, id);

	return ret;
}

static int media_device_request_delete(struct media_device *mdev,
				       struct media_device_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&mdev->req_lock, flags);

	if (req->state != MEDIA_DEVICE_REQUEST_STATE_IDLE) {
		spin_unlock_irqrestore(&mdev->req_lock, flags);
		dev_dbg(mdev->dev, "request: can't delete %u, state %s\n",
			req->id, request_state(req->state));
		return -EINVAL;
	}

	req->state = MEDIA_DEVICE_REQUEST_STATE_DELETED;

	if (req->filp) {
		/*
		 * If the file handle is gone by now the
		 * request has already been deleted from the
		 * two lists.
		 */
		list_del(&req->list);
		list_del(&req->fh_list);
		req->filp = NULL;
	}

	spin_unlock_irqrestore(&mdev->req_lock, flags);

	media_device_request_put(req);

	return 0;
}

void media_device_request_complete(struct media_device *mdev,
				   struct media_device_request *req)
{
	struct file *filp;
	unsigned long flags;

	spin_lock_irqsave(&mdev->req_lock, flags);

	if (req->state == MEDIA_DEVICE_REQUEST_STATE_IDLE) {
		dev_dbg(mdev->dev,
			"request: not completing an idle request %u\n",
			req->id);
		spin_unlock_irqrestore(&mdev->req_lock, flags);
		return;
	}

	if (WARN_ON(req->state != MEDIA_DEVICE_REQUEST_STATE_QUEUED)) {
		dev_dbg(mdev->dev, "request: can't delete %u, state %s\n",
			req->id, request_state(req->state));
		spin_unlock_irqrestore(&mdev->req_lock, flags);
		return;
	}

	req->state = MEDIA_DEVICE_REQUEST_STATE_COMPLETE;
	filp = req->filp;
	if (filp) {
		/*
		 * If the file handle is still around we remove if
		 * from the lists here. Otherwise it has been removed
		 * when the file handle closed.
		 */
		list_del(&req->list);
		list_del(&req->fh_list);
		/* If the user asked for an event, let's queue one. */
		if (req->flags & MEDIA_REQ_FL_COMPLETE_EVENT)
			media_device_request_queue_event(
				mdev, req, media_device_fh(filp));
		req->filp = NULL;
	}

	spin_unlock_irqrestore(&mdev->req_lock, flags);

	/*
	 * The driver holds a reference to a request if the filp
	 * pointer is non-NULL: the file handle associated to the
	 * request may have been released by now, i.e. filp is NULL.
	 */
	if (filp)
		media_device_request_put(req);
}
EXPORT_SYMBOL_GPL(media_device_request_complete);

static int media_device_request_queue_apply(
	struct media_device *mdev, struct media_device_request *req,
	u32 req_flags, int (*fn)(struct media_device *mdev,
				 struct media_device_request *req), bool queue)
{
	char *str = queue ? "queue" : "apply";
	unsigned long flags;
	int rval = 0;

	if (!fn)
		return -ENOSYS;

	spin_lock_irqsave(&mdev->req_lock, flags);
	if (req->state != MEDIA_DEVICE_REQUEST_STATE_IDLE) {
		rval = -EINVAL;
		dev_dbg(mdev->dev,
			"request: unable to %s %u, request in state %s\n",
			str, req->id, request_state(req->state));
	} else {
		req->state = MEDIA_DEVICE_REQUEST_STATE_QUEUED;
		req->flags = req_flags;
	}
	spin_unlock_irqrestore(&mdev->req_lock, flags);

	if (rval)
		return rval;

	rval = fn(mdev, req);
	if (rval) {
		spin_lock_irqsave(&mdev->req_lock, flags);
		req->state = MEDIA_DEVICE_REQUEST_STATE_IDLE;
		spin_unlock_irqrestore(&mdev->req_lock, flags);
		dev_dbg(mdev->dev,
			"request: can't %s %u\n", str, req->id);
	} else {
		dev_dbg(mdev->dev,
			"request: %s %u\n", str, req->id);
	}

	return rval;
}

static long media_device_request_cmd(struct media_device *mdev,
				     struct file *filp,
				     struct media_request_cmd *cmd)
{
	struct media_device_request *req = NULL;
	int ret;

	if (!mdev->ops || !mdev->ops->req_alloc || !mdev->ops->req_free)
		return -ENOTTY;

	if (cmd->cmd != MEDIA_REQ_CMD_ALLOC) {
		req = media_device_request_find(mdev, cmd->request);
		if (!req)
			return -EINVAL;
	}

	switch (cmd->cmd) {
	case MEDIA_REQ_CMD_ALLOC:
		ret = media_device_request_alloc(mdev, filp, cmd);
		break;

	case MEDIA_REQ_CMD_DELETE:
		ret = media_device_request_delete(mdev, req);
		break;

	case MEDIA_REQ_CMD_APPLY:
		ret = media_device_request_queue_apply(mdev, req, cmd->flags,
						       mdev->ops->req_apply,
						       false);
		break;

	case MEDIA_REQ_CMD_QUEUE:
		ret = media_device_request_queue_apply(mdev, req, cmd->flags,
						       mdev->ops->req_queue,
						       true);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (req)
		media_device_request_put(req);

	if (ret < 0)
		return ret;

	return 0;
}

/* -----------------------------------------------------------------------------
 * Userspace API
 */

static int media_device_open(struct file *filp)
{
	struct media_device_fh *fh;

	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh)
		return -ENOMEM;

	INIT_LIST_HEAD(&fh->requests);
	INIT_LIST_HEAD(&fh->kevents.head);
	init_waitqueue_head(&fh->kevents.wait);
	atomic_set(&fh->kevents.sequence, -1);
	filp->private_data = &fh->fh;

	return 0;
}

static int media_device_close(struct file *filp)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_device *mdev = to_media_device(fh->fh.devnode);

	spin_lock_irq(&mdev->req_lock);
	while (!list_empty(&fh->requests)) {
		struct media_device_request *req =
			list_first_entry(&fh->requests, typeof(*req), fh_list);

		list_del(&req->list);
		list_del(&req->fh_list);
		req->filp = NULL;
		spin_unlock_irq(&mdev->req_lock);
		media_device_request_put(req);
		spin_lock_irq(&mdev->req_lock);
	}

	while (!list_empty(&fh->kevents.head)) {
		struct media_kevent *kev =
			list_first_entry(&fh->kevents.head, typeof(*kev), list);

		list_del(&kev->list);
		spin_unlock_irq(&mdev->req_lock);
		kfree(kev);
		spin_lock_irq(&mdev->req_lock);
	}
	spin_unlock_irq(&mdev->req_lock);

	kfree(fh);

	return 0;
}

static int media_device_get_info(struct media_device *dev,
				 struct file *filp,
				 struct media_device_info *info)
{
	memset(info, 0, sizeof(*info));

	strlcpy(info->driver, dev->dev->driver->name, sizeof(info->driver));
	strlcpy(info->model, dev->model, sizeof(info->model));
	strlcpy(info->serial, dev->serial, sizeof(info->serial));
	strlcpy(info->bus_info, dev->bus_info, sizeof(info->bus_info));

	info->media_version = MEDIA_API_VERSION;
	info->hw_revision = dev->hw_revision;
	info->driver_version = dev->driver_version;

	return 0;
}

static struct media_entity *find_entity(struct media_device *mdev, u32 id)
{
	struct media_entity *entity;
	int next = id & MEDIA_ENT_ID_FLAG_NEXT;

	id &= ~MEDIA_ENT_ID_FLAG_NEXT;

	spin_lock(&mdev->lock);

	media_device_for_each_entity(entity, mdev) {
		if ((entity->id == id && !next) ||
		    (entity->id > id && next)) {
			spin_unlock(&mdev->lock);
			return entity;
		}
	}

	spin_unlock(&mdev->lock);

	return NULL;
}

static long media_device_enum_entities(struct media_device *mdev,
				       struct file *filp,
				       struct media_entity_desc *entd)
{
	struct media_entity *ent;

	ent = find_entity(mdev, entd->id);
	if (ent == NULL)
		return -EINVAL;

	memset(entd, 0, sizeof(*entd));

	entd->id = ent->id;
	if (ent->name)
		strlcpy(entd->name, ent->name, sizeof(entd->name));
	entd->type = ent->type;
	entd->revision = 0;
	entd->flags = ent->flags;
	entd->group_id = 0;
	entd->pads = ent->num_pads;
	entd->links = ent->num_links - ent->num_backlinks;

	memcpy(&entd->raw, &ent->info, sizeof(ent->info));

	return 0;
}

static void media_device_kpad_to_upad(const struct media_pad *kpad,
				      struct media_pad_desc *upad)
{
	upad->entity = kpad->entity->id;
	upad->index = kpad->index;
	upad->flags = kpad->flags;
}

static long media_device_enum_links(struct media_device *mdev,
				    struct file *filp,
				    struct media_links_enum *links)
{
	struct media_entity *entity;

	entity = find_entity(mdev, links->entity);
	if (entity == NULL)
		return -EINVAL;

	if (links->pads) {
		unsigned int p;

		for (p = 0; p < entity->num_pads; p++) {
			struct media_pad_desc pad;

			memset(&pad, 0, sizeof(pad));
			media_device_kpad_to_upad(&entity->pads[p], &pad);
			if (copy_to_user(&links->pads[p], &pad, sizeof(pad)))
				return -EFAULT;
		}
	}

	if (links->links) {
		struct media_link_desc __user *ulink;
		unsigned int l;

		for (l = 0, ulink = links->links; l < entity->num_links; l++) {
			struct media_link_desc link;

			/* Ignore backlinks. */
			if (entity->links[l].source->entity != entity)
				continue;

			memset(&link, 0, sizeof(link));
			media_device_kpad_to_upad(entity->links[l].source,
						  &link.source);
			media_device_kpad_to_upad(entity->links[l].sink,
						  &link.sink);
			link.flags = entity->links[l].flags;
			if (copy_to_user(ulink, &link, sizeof(*ulink)))
				return -EFAULT;
			ulink++;
		}
	}

	return 0;
}

static long media_device_setup_link(struct media_device *mdev,
				    struct file *filp,
				    struct media_link_desc *linkd)
{
	struct media_link *link = NULL;
	struct media_entity *source;
	struct media_entity *sink;

	/* Find the source and sink entities and link.
	 */
	source = find_entity(mdev, linkd->source.entity);
	sink = find_entity(mdev, linkd->sink.entity);

	if (source == NULL || sink == NULL)
		return -EINVAL;

	if (linkd->source.index >= source->num_pads ||
	    linkd->sink.index >= sink->num_pads)
		return -EINVAL;

	link = media_entity_find_link(&source->pads[linkd->source.index],
				      &sink->pads[linkd->sink.index]);
	if (link == NULL)
		return -EINVAL;

	/* Setup the link on both entities. */
	return __media_entity_setup_link(link, linkd->flags);
}

static struct media_kevent *opportunistic_dqevent(struct media_device *mdev,
						  struct file *filp)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_kevent *kev = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mdev->req_lock, flags);
	if (!list_empty(&fh->kevents.head)) {
		kev = list_last_entry(&fh->kevents.head,
				      struct media_kevent, list);
		list_del(&kev->list);
	}
	spin_unlock_irqrestore(&mdev->req_lock, flags);

	return kev;
}

static int media_device_dqevent(struct media_device *mdev,
				struct file *filp,
				struct media_event *ev)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_kevent *kev;

	if (filp->f_flags & O_NONBLOCK) {
		kev = opportunistic_dqevent(mdev, filp);
		if (!kev)
			return -ENODATA;
	} else {
		int ret = wait_event_interruptible(
			fh->kevents.wait,
			(kev = opportunistic_dqevent(mdev, filp)));
		if (ret == -ERESTARTSYS)
			return ret;
	}

	*ev = kev->ev;
	kfree(kev);

	return 0;
}

static long copy_arg_from_user(void *karg, void __user *uarg, unsigned int cmd)
{
	/* All media IOCTLs are _IOWR() */
	if (copy_from_user(karg, uarg, _IOC_SIZE(cmd)))
		return -EFAULT;

	return 0;
}

static long copy_arg_to_user(void __user *uarg, void *karg, unsigned int cmd)
{
	/* All media IOCTLs are _IOWR() */
	if (copy_to_user(uarg, karg, _IOC_SIZE(cmd)))
		return -EFAULT;

	return 0;
}

#ifdef CONFIG_COMPAT
/* Only compat IOCTLs need this right now. */
static long copy_arg_to_user_nop(void __user *uarg, void *karg,
				 unsigned int cmd)
{
	return 0;
}
#endif

/* Do acquire the graph mutex */
#define MEDIA_IOC_FL_GRAPH_MUTEX	BIT(0)

#define MEDIA_IOC_SZ_ARG(__cmd, func, fl, alt_sz, from_user, to_user)	\
	[_IOC_NR(MEDIA_IOC_##__cmd)] = {				\
		.cmd = MEDIA_IOC_##__cmd,				\
		.fn = (long (*)(struct media_device *,			\
				struct file *, void *))func,		\
		.flags = fl,						\
		.alt_arg_sizes = alt_sz,				\
		.arg_from_user = from_user,				\
		.arg_to_user = to_user,					\
	}

#define MEDIA_IOC_ARG(__cmd, func, fl, from_user, to_user)		\
	MEDIA_IOC_SZ_ARG(__cmd, func, fl, NULL, from_user, to_user)

#define MEDIA_IOC_SZ(__cmd, func, fl, alt_sz)			\
	MEDIA_IOC_SZ_ARG(__cmd, func, fl, alt_sz,		\
			 copy_arg_from_user, copy_arg_to_user)

#define MEDIA_IOC(__cmd, func, fl)				\
	MEDIA_IOC_ARG(__cmd, func, fl,				\
		      copy_arg_from_user, copy_arg_to_user)

/* the table is indexed by _IOC_NR(cmd) */
struct media_ioctl_info {
	unsigned int cmd;
	long (*fn)(struct media_device *dev, struct file *file, void *arg);
	unsigned short flags;
	const unsigned short *alt_arg_sizes;
	long (*arg_from_user)(void *karg, void __user *uarg, unsigned int cmd);
	long (*arg_to_user)(void __user *uarg, void *karg, unsigned int cmd);
};

#define MASK_IOC_SIZE(cmd) \
	((cmd) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

static inline long is_valid_ioctl(const struct media_ioctl_info *info,
				  unsigned int len, unsigned int cmd)
{
	const unsigned short *alt_arg_sizes;

	if (unlikely(_IOC_NR(cmd) >= len))
		return -ENOIOCTLCMD;

	info += _IOC_NR(cmd);

	if (info->cmd == cmd)
		return 0;

	/*
	 * Verify that the size-dependent patch of the IOCTL command
	 * matches and that the size does not exceed the principal
	 * argument size.
	 */
	if (unlikely(MASK_IOC_SIZE(info->cmd) != MASK_IOC_SIZE(cmd)
		     || _IOC_SIZE(info->cmd) < _IOC_SIZE(cmd)))
		return -ENOIOCTLCMD;

	alt_arg_sizes = info->alt_arg_sizes;
	if (unlikely(!alt_arg_sizes))
		return -ENOIOCTLCMD;

	for (; *alt_arg_sizes; alt_arg_sizes++)
		if (_IOC_SIZE(cmd) == *alt_arg_sizes)
			return 0;

	return -ENOIOCTLCMD;
}

static long __media_device_ioctl(
	struct file *filp, unsigned int cmd, void __user *arg,
	const struct media_ioctl_info *info_array, unsigned int info_array_len)
{
	struct media_devnode *devnode = media_devnode_data(filp);
	struct media_device *dev = to_media_device(devnode);
	const struct media_ioctl_info *info;
	char __karg[256], *karg = __karg;
	long ret;

	ret = is_valid_ioctl(info_array, info_array_len, cmd);
	if (ret)
		return ret;

	info = &info_array[_IOC_NR(cmd)];

	if (_IOC_SIZE(info->cmd) > sizeof(__karg)) {
		karg = kmalloc(_IOC_SIZE(info->cmd), GFP_KERNEL);
		if (!karg)
			return -ENOMEM;
	}

	ret = info->arg_from_user(karg, arg, cmd);
	if (ret)
		goto out_free;

	/* Set the rest of the argument struct to zero */
	memset(karg + _IOC_SIZE(cmd), 0, _IOC_SIZE(info->cmd) - _IOC_SIZE(cmd));

	if (info->flags & MEDIA_IOC_FL_GRAPH_MUTEX)
		mutex_lock(&dev->graph_mutex);

	ret = info->fn(dev, filp, karg);

	if (info->flags & MEDIA_IOC_FL_GRAPH_MUTEX)
		mutex_unlock(&dev->graph_mutex);

	if (!ret)
		ret = info->arg_to_user(arg, karg, cmd);

out_free:
	if (karg != __karg)
		kfree(karg);

	return ret;
}

static const struct media_ioctl_info ioctl_info[] = {
	MEDIA_IOC(DEVICE_INFO, media_device_get_info, 0),
	MEDIA_IOC(ENUM_ENTITIES, media_device_enum_entities, 0),
	MEDIA_IOC(ENUM_LINKS, media_device_enum_links, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(SETUP_LINK, media_device_setup_link, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(REQUEST_CMD, media_device_request_cmd, 0),
	MEDIA_IOC(DQEVENT, media_device_dqevent, 0),
};

static long media_device_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	return __media_device_ioctl(
		filp, cmd, (void __user *)arg,
		ioctl_info, ARRAY_SIZE(ioctl_info));
}

static unsigned int media_device_poll(struct file *filp,
				      struct poll_table_struct *wait)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_device *mdev = to_media_device(fh->fh.devnode);
	unsigned int poll_events = poll_requested_events(wait);
	int ret = 0;

	if (poll_events & (POLLIN | POLLOUT))
		return POLLERR;

	if (poll_events & POLLPRI) {
		unsigned long flags;
		bool empty;

		spin_lock_irqsave(&mdev->req_lock, flags);
		empty = list_empty(&fh->kevents.head);
		spin_unlock_irqrestore(&mdev->req_lock, flags);

		if (empty)
			poll_wait(filp, &fh->kevents.wait, wait);
		else
			ret |= POLLPRI;
	}

	return ret;
}

#ifdef CONFIG_COMPAT

struct media_links_enum32 {
	__u32 entity;
	compat_uptr_t pads; /* struct media_pad_desc * */
	compat_uptr_t links; /* struct media_link_desc * */
	__u32 reserved[4];
};

static long from_user_enum_links32(void *karg, void __user *uarg,
				   unsigned int cmd)
{
	struct media_links_enum *links = karg;
	struct media_links_enum32 __user *ulinks = uarg;
	compat_uptr_t pads_ptr, links_ptr;

	memset(links, 0, sizeof(*links));

	if (get_user(links->entity, &ulinks->entity)
	    || get_user(pads_ptr, &ulinks->pads)
	    || get_user(links_ptr, &ulinks->links))
		return -EFAULT;

	links->pads = compat_ptr(pads_ptr);
	links->links = compat_ptr(links_ptr);

	return 0;
}

#define MEDIA_IOC_ENUM_LINKS32		_IOWR('|', 0x02, struct media_links_enum32)

static const struct media_ioctl_info compat_ioctl_info[] = {
	MEDIA_IOC(DEVICE_INFO, media_device_get_info, 0),
	MEDIA_IOC(ENUM_ENTITIES, media_device_enum_entities, 0),
	MEDIA_IOC_ARG(ENUM_LINKS32, media_device_enum_links, MEDIA_IOC_FL_GRAPH_MUTEX, from_user_enum_links32, copy_arg_to_user_nop),
	MEDIA_IOC(SETUP_LINK, media_device_setup_link, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(REQUEST_CMD, media_device_request_cmd, 0),
	MEDIA_IOC(DQEVENT, media_device_dqevent, 0),
};

static long media_device_compat_ioctl(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	return __media_device_ioctl(
		filp, cmd, (void __user *)arg,
		compat_ioctl_info, ARRAY_SIZE(compat_ioctl_info));
}
#endif /* CONFIG_COMPAT */

static const struct media_file_operations media_device_fops = {
	.owner = THIS_MODULE,
	.open = media_device_open,
	.ioctl = media_device_ioctl,
	.poll = media_device_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = media_device_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.release = media_device_close,
};

/* -----------------------------------------------------------------------------
 * sysfs
 */

static ssize_t show_model(struct device *cd,
			  struct device_attribute *attr, char *buf)
{
	struct media_device *mdev = to_media_device(to_media_devnode(cd));

	return sprintf(buf, "%.*s\n", (int)sizeof(mdev->model), mdev->model);
}

static DEVICE_ATTR(model, S_IRUGO, show_model, NULL);

/* -----------------------------------------------------------------------------
 * Registration/unregistration
 */

static void media_device_release(struct media_devnode *mdev)
{
}

/**
 * media_device_register - register a media device
 * @mdev:	The media device
 *
 * The caller is responsible for initializing the media device before
 * registration. The following fields must be set:
 *
 * - dev must point to the parent device
 * - model must be filled with the device model name
 */
int __must_check __media_device_register(struct media_device *mdev,
					 struct module *owner)
{
	int ret;

	if (WARN_ON(mdev->dev == NULL || mdev->model[0] == 0))
		return -EINVAL;

	mdev->entity_id = 1;
	INIT_LIST_HEAD(&mdev->entities);
	spin_lock_init(&mdev->lock);
	mutex_init(&mdev->graph_mutex);
	ida_init(&mdev->req_ids);
	spin_lock_init(&mdev->req_lock);
	INIT_LIST_HEAD(&mdev->requests);

	/* Register the device node. */
	mdev->devnode.fops = &media_device_fops;
	mdev->devnode.parent = mdev->dev;
	mdev->devnode.release = media_device_release;
	ret = media_devnode_register(&mdev->devnode, owner);
	if (ret < 0)
		return ret;

	ret = device_create_file(&mdev->devnode.dev, &dev_attr_model);
	if (ret < 0) {
		media_devnode_unregister(&mdev->devnode);
		ida_destroy(&mdev->req_ids);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(__media_device_register);

/**
 * media_device_unregister - unregister a media device
 * @mdev:	The media device
 *
 */
void media_device_unregister(struct media_device *mdev)
{
	struct media_entity *entity;
	struct media_entity *next;

	list_for_each_entry_safe(entity, next, &mdev->entities, list)
		media_device_unregister_entity(entity);

	device_remove_file(&mdev->devnode.dev, &dev_attr_model);
	media_devnode_unregister(&mdev->devnode);
	ida_destroy(&mdev->req_ids);
}
EXPORT_SYMBOL_GPL(media_device_unregister);

/**
 * media_device_register_entity - Register an entity with a media device
 * @mdev:	The media device
 * @entity:	The entity
 */
int __must_check media_device_register_entity(struct media_device *mdev,
					      struct media_entity *entity)
{
	/* Warn if we apparently re-register an entity */
	WARN_ON(entity->parent != NULL);
	entity->parent = mdev;

	spin_lock(&mdev->lock);
	if (entity->id == 0)
		entity->id = mdev->entity_id++;
	else
		mdev->entity_id = max(entity->id + 1, mdev->entity_id);
	list_add_tail(&entity->list, &mdev->entities);
	spin_unlock(&mdev->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(media_device_register_entity);

/**
 * media_device_unregister_entity - Unregister an entity
 * @entity:	The entity
 *
 * If the entity has never been registered this function will return
 * immediately.
 */
void media_device_unregister_entity(struct media_entity *entity)
{
	struct media_device *mdev = entity->parent;

	if (mdev == NULL)
		return;

	spin_lock(&mdev->lock);
	list_del(&entity->list);
	spin_unlock(&mdev->lock);
	entity->parent = NULL;
}
EXPORT_SYMBOL_GPL(media_device_unregister_entity);
