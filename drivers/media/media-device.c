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
 */

/* We need to access legacy defines from linux/media.h */
#define __NEED_MEDIA_LEGACY_API

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/ioctl.h>
#include <linux/media.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/usb.h>
#include <linux/wait.h>

#include <media/media-device.h>
#include <media/media-devnode.h>
#include <media/media-entity.h>

#ifdef CONFIG_MEDIA_CONTROLLER

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

static inline void __user *media_get_uptr(__u64 arg)
{
	return (void __user *)(uintptr_t)arg;
}

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
	struct media_device *mdev = fh->fh.devnode->media_dev;

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

static long media_device_get_info(struct media_device *dev,
				 struct file *filp,
				 void *arg)
{
	struct media_device_info *info = arg;

	memset(info, 0, sizeof(*info));

	if (dev->driver_name[0])
		strlcpy(info->driver, dev->driver_name, sizeof(info->driver));
	else
		strlcpy(info->driver, dev->dev->driver->name,
			sizeof(info->driver));

	strlcpy(info->model, dev->model, sizeof(info->model));
	strlcpy(info->serial, dev->serial, sizeof(info->serial));
	strlcpy(info->bus_info, dev->bus_info, sizeof(info->bus_info));

	info->media_version = LINUX_VERSION_CODE;
	info->driver_version = info->media_version;
	info->hw_revision = dev->hw_revision;

	return 0;
}

static struct media_entity *find_entity(struct media_device *mdev, u32 id)
{
	struct media_entity *entity;
	int next = id & MEDIA_ENT_ID_FLAG_NEXT;

	id &= ~MEDIA_ENT_ID_FLAG_NEXT;

	media_device_for_each_entity(entity, mdev) {
		if (((media_entity_id(entity) == id) && !next) ||
		    ((media_entity_id(entity) > id) && next)) {
			return entity;
		}
	}

	return NULL;
}

static long media_device_enum_entities(struct media_device *mdev,
				       struct file *filp,
				       void *arg)
{
	struct media_entity_desc *entd = arg;
	struct media_entity *ent;

	ent = find_entity(mdev, entd->id);
	if (ent == NULL)
		return -EINVAL;

	memset(entd, 0, sizeof(*entd));

	entd->id = media_entity_id(ent);
	if (ent->name)
		strlcpy(entd->name, ent->name, sizeof(entd->name));
	entd->type = ent->function;
	entd->revision = 0;		/* Unused */
	entd->flags = ent->flags;
	entd->group_id = 0;		/* Unused */
	entd->pads = ent->num_pads;
	entd->links = ent->num_links - ent->num_backlinks;

	/*
	 * Workaround for a bug at media-ctl <= v1.10 that makes it to
	 * do the wrong thing if the entity function doesn't belong to
	 * either MEDIA_ENT_F_OLD_BASE or MEDIA_ENT_F_OLD_SUBDEV_BASE
	 * Ranges.
	 *
	 * Non-subdevices are expected to be at the MEDIA_ENT_F_OLD_BASE,
	 * or, otherwise, will be silently ignored by media-ctl when
	 * printing the graphviz diagram. So, map them into the devnode
	 * old range.
	 */
	if (ent->function < MEDIA_ENT_F_OLD_BASE ||
	    ent->function > MEDIA_ENT_F_TUNER) {
		if (is_media_entity_v4l2_subdev(ent))
			entd->type = MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN;
		else if (ent->function != MEDIA_ENT_F_IO_V4L)
			entd->type = MEDIA_ENT_T_DEVNODE_UNKNOWN;
	}

	memcpy(&entd->raw, &ent->info, sizeof(ent->info));

	return 0;
}

static void media_device_kpad_to_upad(const struct media_pad *kpad,
				      struct media_pad_desc *upad)
{
	upad->entity = media_entity_id(kpad->entity);
	upad->index = kpad->index;
	upad->flags = kpad->flags;
}

static long media_device_enum_links(struct media_device *mdev,
				    struct file *filp,
				    void *arg)
{
	struct media_links_enum *links = arg;
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
		struct media_link *link;
		struct media_link_desc __user *ulink_desc = links->links;

		list_for_each_entry(link, &entity->links, list) {
			struct media_link_desc klink_desc;

			/* Ignore backlinks. */
			if (link->source->entity != entity)
				continue;
			memset(&klink_desc, 0, sizeof(klink_desc));
			media_device_kpad_to_upad(link->source,
						  &klink_desc.source);
			media_device_kpad_to_upad(link->sink,
						  &klink_desc.sink);
			klink_desc.flags = link->flags;
			if (copy_to_user(ulink_desc, &klink_desc,
					 sizeof(*ulink_desc)))
				return -EFAULT;
			ulink_desc++;
		}
	}

	return 0;
}

static long media_device_setup_link(struct media_device *mdev,
				    struct file *filp,
				    void *arg)
{
	struct media_link_desc *linkd = arg;
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

static long media_device_get_topology(struct media_device *mdev, void *arg)
{
	struct media_v2_topology *topo = arg;
	struct media_entity *entity;
	struct media_interface *intf;
	struct media_pad *pad;
	struct media_link *link;
	struct media_v2_entity kentity, __user *uentity;
	struct media_v2_interface kintf, __user *uintf;
	struct media_v2_pad kpad, __user *upad;
	struct media_v2_link klink, __user *ulink;
	unsigned int i;
	int ret = 0;

	topo->topology_version = mdev->topology_version;

	/* Get entities and number of entities */
	i = 0;
	uentity = media_get_uptr(topo->ptr_entities);
	media_device_for_each_entity(entity, mdev) {
		i++;
		if (ret || !uentity)
			continue;

		if (i > topo->num_entities) {
			ret = -ENOSPC;
			continue;
		}

		/* Copy fields to userspace struct if not error */
		memset(&kentity, 0, sizeof(kentity));
		kentity.id = entity->graph_obj.id;
		kentity.function = entity->function;
		strncpy(kentity.name, entity->name,
			sizeof(kentity.name));

		if (copy_to_user(uentity, &kentity, sizeof(kentity)))
			ret = -EFAULT;
		uentity++;
	}
	topo->num_entities = i;

	/* Get interfaces and number of interfaces */
	i = 0;
	uintf = media_get_uptr(topo->ptr_interfaces);
	media_device_for_each_intf(intf, mdev) {
		i++;
		if (ret || !uintf)
			continue;

		if (i > topo->num_interfaces) {
			ret = -ENOSPC;
			continue;
		}

		memset(&kintf, 0, sizeof(kintf));

		/* Copy intf fields to userspace struct */
		kintf.id = intf->graph_obj.id;
		kintf.intf_type = intf->type;
		kintf.flags = intf->flags;

		if (media_type(&intf->graph_obj) == MEDIA_GRAPH_INTF_DEVNODE) {
			struct media_intf_devnode *devnode;

			devnode = intf_to_devnode(intf);

			kintf.devnode.major = devnode->major;
			kintf.devnode.minor = devnode->minor;
		}

		if (copy_to_user(uintf, &kintf, sizeof(kintf)))
			ret = -EFAULT;
		uintf++;
	}
	topo->num_interfaces = i;

	/* Get pads and number of pads */
	i = 0;
	upad = media_get_uptr(topo->ptr_pads);
	media_device_for_each_pad(pad, mdev) {
		i++;
		if (ret || !upad)
			continue;

		if (i > topo->num_pads) {
			ret = -ENOSPC;
			continue;
		}

		memset(&kpad, 0, sizeof(kpad));

		/* Copy pad fields to userspace struct */
		kpad.id = pad->graph_obj.id;
		kpad.entity_id = pad->entity->graph_obj.id;
		kpad.flags = pad->flags;

		if (copy_to_user(upad, &kpad, sizeof(kpad)))
			ret = -EFAULT;
		upad++;
	}
	topo->num_pads = i;

	/* Get links and number of links */
	i = 0;
	ulink = media_get_uptr(topo->ptr_links);
	media_device_for_each_link(link, mdev) {
		if (link->is_backlink)
			continue;

		i++;

		if (ret || !ulink)
			continue;

		if (i > topo->num_links) {
			ret = -ENOSPC;
			continue;
		}

		memset(&klink, 0, sizeof(klink));

		/* Copy link fields to userspace struct */
		klink.id = link->graph_obj.id;
		klink.source_id = link->gobj0->id;
		klink.sink_id = link->gobj1->id;
		klink.flags = link->flags;

		if (copy_to_user(ulink, &klink, sizeof(klink)))
			ret = -EFAULT;
		ulink++;
	}
	topo->num_links = i;

	return ret;
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

/* Do acquire the graph mutex */
#define MEDIA_IOC_FL_GRAPH_MUTEX	BIT(0)

#define MEDIA_IOC_ARG(__cmd, func, fl, from_user, to_user)		\
	[_IOC_NR(MEDIA_IOC_##__cmd)] = {				\
		.cmd = MEDIA_IOC_##__cmd,				\
		.fn = (long (*)(struct media_device *,			\
				struct file *, void *))func,		\
		.flags = fl,						\
		.arg_from_user = from_user,				\
		.arg_to_user = to_user,					\
	}

#define MEDIA_IOC(__cmd, func, fl)					\
	MEDIA_IOC_ARG(__cmd, func, fl, copy_arg_from_user, copy_arg_to_user)

/* the table is indexed by _IOC_NR(cmd) */
struct media_ioctl_info {
	unsigned int cmd;
	long (*fn)(struct media_device *dev, struct file *file, void *arg);
	unsigned short flags;
	long (*arg_from_user)(void *karg, void __user *uarg, unsigned int cmd);
	long (*arg_to_user)(void __user *uarg, void *karg, unsigned int cmd);
};

static const struct media_ioctl_info ioctl_info[] = {
	MEDIA_IOC(DEVICE_INFO, media_device_get_info, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(ENUM_ENTITIES, media_device_enum_entities, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(ENUM_LINKS, media_device_enum_links, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(SETUP_LINK, media_device_setup_link, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(G_TOPOLOGY, media_device_get_topology, MEDIA_IOC_FL_GRAPH_MUTEX),
	MEDIA_IOC(REQUEST_CMD, media_device_request_cmd, 0),
	MEDIA_IOC(DQEVENT, media_device_dqevent, 0),
};

static long media_device_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long __arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);
	struct media_device *dev = devnode->media_dev;
	const struct media_ioctl_info *info;
	void __user *arg = (void __user *)__arg;
	char __karg[256], *karg = __karg;
	long ret;

	if (_IOC_NR(cmd) >= ARRAY_SIZE(ioctl_info)
	    || ioctl_info[_IOC_NR(cmd)].cmd != cmd)
		return -ENOIOCTLCMD;

	info = &ioctl_info[_IOC_NR(cmd)];

	if (_IOC_SIZE(info->cmd) > sizeof(__karg)) {
		karg = kmalloc(_IOC_SIZE(info->cmd), GFP_KERNEL);
		if (!karg)
			return -ENOMEM;
	}

	if (info->arg_from_user) {
		ret = info->arg_from_user(karg, arg, cmd);
		if (ret)
			goto out_free;
	}

	if (info->flags & MEDIA_IOC_FL_GRAPH_MUTEX)
		mutex_lock(&dev->graph_mutex);

	ret = info->fn(dev, filp, karg);

	if (info->flags & MEDIA_IOC_FL_GRAPH_MUTEX)
		mutex_unlock(&dev->graph_mutex);

	if (!ret && info->arg_to_user)
		ret = info->arg_to_user(arg, karg, cmd);

out_free:
	if (karg != __karg)
		kfree(karg);

	return ret;
}

static unsigned int media_device_poll(struct file *filp,
				      struct poll_table_struct *wait)
{
	struct media_device_fh *fh = media_device_fh(filp);
	struct media_device *mdev = fh->fh.devnode->media_dev;
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
static long media_device_enum_links32(struct media_device *mdev,
						struct file *filp,
						struct media_links_enum32 __user *ulinks)
{
	struct media_links_enum links;
	compat_uptr_t pads_ptr, links_ptr;

	memset(&links, 0, sizeof(links));

	if (get_user(links.entity, &ulinks->entity)
	    || get_user(pads_ptr, &ulinks->pads)
	    || get_user(links_ptr, &ulinks->links))
		return -EFAULT;

	links.pads = compat_ptr(pads_ptr);
	links.links = compat_ptr(links_ptr);

	return media_device_enum_links(mdev, filp, &links);
}

#define MEDIA_IOC_ENUM_LINKS32		_IOWR('|', 0x02, struct media_links_enum32)
static long media_device_compat_ioctl(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);
	struct media_device *dev = devnode->media_dev;
	long ret;

	switch (cmd) {
	case MEDIA_IOC_ENUM_LINKS32:
		mutex_lock(&dev->graph_mutex);
		ret = media_device_enum_links32(dev,
				filp,
				(struct media_links_enum32 __user *)arg);
		mutex_unlock(&dev->graph_mutex);
		break;

	default:
		return media_device_ioctl(filp, cmd, arg);
	}

	return ret;
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
	struct media_devnode *devnode = to_media_devnode(cd);
	struct media_device *mdev = devnode->media_dev;

	return sprintf(buf, "%.*s\n", (int)sizeof(mdev->model), mdev->model);
}

static DEVICE_ATTR(model, S_IRUGO, show_model, NULL);

/* -----------------------------------------------------------------------------
 * Registration/unregistration
 */

static void media_device_release(struct media_devnode *devnode)
{
	dev_dbg(devnode->parent, "Media device released\n");
}

/**
 * media_device_register_entity - Register an entity with a media device
 * @mdev:	The media device
 * @entity:	The entity
 */
int __must_check media_device_register_entity(struct media_device *mdev,
					      struct media_entity *entity)
{
	struct media_entity_notify *notify, *next;
	unsigned int i;
	int ret;

	if (entity->function == MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN ||
	    entity->function == MEDIA_ENT_F_UNKNOWN)
		dev_warn(mdev->dev,
			 "Entity type for entity %s was not initialized!\n",
			 entity->name);

	/* Warn if we apparently re-register an entity */
	WARN_ON(entity->graph_obj.mdev != NULL);
	entity->graph_obj.mdev = mdev;
	INIT_LIST_HEAD(&entity->links);
	entity->num_links = 0;
	entity->num_backlinks = 0;

	if (!ida_pre_get(&mdev->entity_internal_idx, GFP_KERNEL))
		return -ENOMEM;

	mutex_lock(&mdev->graph_mutex);

	ret = ida_get_new_above(&mdev->entity_internal_idx, 1,
				&entity->internal_idx);
	if (ret < 0) {
		mutex_unlock(&mdev->graph_mutex);
		return ret;
	}

	mdev->entity_internal_idx_max =
		max(mdev->entity_internal_idx_max, entity->internal_idx);

	/* Initialize media_gobj embedded at the entity */
	media_gobj_create(mdev, MEDIA_GRAPH_ENTITY, &entity->graph_obj);

	/* Initialize objects at the pads */
	for (i = 0; i < entity->num_pads; i++)
		media_gobj_create(mdev, MEDIA_GRAPH_PAD,
			       &entity->pads[i].graph_obj);

	/* invoke entity_notify callbacks */
	list_for_each_entry_safe(notify, next, &mdev->entity_notify, list)
		notify->notify(entity, notify->notify_data);

	if (mdev->entity_internal_idx_max
	    >= mdev->pm_count_walk.ent_enum.idx_max) {
		struct media_graph new = { .top = 0 };

		/*
		 * Initialise the new graph walk before cleaning up
		 * the old one in order not to spoil the graph walk
		 * object of the media device if graph walk init fails.
		 */
		ret = media_graph_walk_init(&new, mdev);
		if (ret) {
			mutex_unlock(&mdev->graph_mutex);
			return ret;
		}
		media_graph_walk_cleanup(&mdev->pm_count_walk);
		mdev->pm_count_walk = new;
	}
	mutex_unlock(&mdev->graph_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(media_device_register_entity);

static void __media_device_unregister_entity(struct media_entity *entity)
{
	struct media_device *mdev = entity->graph_obj.mdev;
	struct media_link *link, *tmp;
	struct media_interface *intf;
	unsigned int i;

	ida_simple_remove(&mdev->entity_internal_idx, entity->internal_idx);

	/* Remove all interface links pointing to this entity */
	list_for_each_entry(intf, &mdev->interfaces, graph_obj.list) {
		list_for_each_entry_safe(link, tmp, &intf->links, list) {
			if (link->entity == entity)
				__media_remove_intf_link(link);
		}
	}

	/* Remove all data links that belong to this entity */
	__media_entity_remove_links(entity);

	/* Remove all pads that belong to this entity */
	for (i = 0; i < entity->num_pads; i++)
		media_gobj_destroy(&entity->pads[i].graph_obj);

	/* Remove the entity */
	media_gobj_destroy(&entity->graph_obj);

	/* invoke entity_notify callbacks to handle entity removal?? */

	entity->graph_obj.mdev = NULL;
}

void media_device_unregister_entity(struct media_entity *entity)
{
	struct media_device *mdev = entity->graph_obj.mdev;

	if (mdev == NULL)
		return;

	mutex_lock(&mdev->graph_mutex);
	__media_device_unregister_entity(entity);
	mutex_unlock(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_device_unregister_entity);

/**
 * media_device_init() - initialize a media device
 * @mdev:	The media device
 *
 * The caller is responsible for initializing the media device before
 * registration. The following fields must be set:
 *
 * - dev must point to the parent device
 * - model must be filled with the device model name
 */
void media_device_init(struct media_device *mdev)
{
	INIT_LIST_HEAD(&mdev->entities);
	INIT_LIST_HEAD(&mdev->interfaces);
	INIT_LIST_HEAD(&mdev->pads);
	INIT_LIST_HEAD(&mdev->links);
	INIT_LIST_HEAD(&mdev->entity_notify);
	mutex_init(&mdev->graph_mutex);
	ida_init(&mdev->entity_internal_idx);

	dev_dbg(mdev->dev, "Media device initialized\n");
}
EXPORT_SYMBOL_GPL(media_device_init);

void media_device_cleanup(struct media_device *mdev)
{
	ida_destroy(&mdev->entity_internal_idx);
	mdev->entity_internal_idx_max = 0;
	media_graph_walk_cleanup(&mdev->pm_count_walk);
	mutex_destroy(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_device_cleanup);

int __must_check __media_device_register(struct media_device *mdev,
					 struct module *owner)
{
	struct media_devnode *devnode;
	int ret;

	devnode = kzalloc(sizeof(*devnode), GFP_KERNEL);
	if (!devnode)
		return -ENOMEM;

	ida_init(&mdev->req_ids);
	spin_lock_init(&mdev->req_lock);
	INIT_LIST_HEAD(&mdev->requests);

	/* Register the device node. */
	mdev->devnode = devnode;
	devnode->fops = &media_device_fops;
	devnode->parent = mdev->dev;
	devnode->release = media_device_release;

	/* Set version 0 to indicate user-space that the graph is static */
	mdev->topology_version = 0;

	ret = media_devnode_register(mdev, devnode, owner);
	if (ret < 0) {
		/* devnode free is handled in media_devnode_*() */
		mdev->devnode = NULL;
		return ret;
	}

	ret = device_create_file(&devnode->dev, &dev_attr_model);
	if (ret < 0) {
		/* devnode free is handled in media_devnode_*() */
		mdev->devnode = NULL;
		media_devnode_unregister_prepare(devnode);
		media_devnode_unregister(devnode);
		ida_destroy(&mdev->req_ids);
		return ret;
	}

	dev_dbg(mdev->dev, "Media device registered\n");

	return 0;
}
EXPORT_SYMBOL_GPL(__media_device_register);

int __must_check media_device_register_entity_notify(struct media_device *mdev,
					struct media_entity_notify *nptr)
{
	mutex_lock(&mdev->graph_mutex);
	list_add_tail(&nptr->list, &mdev->entity_notify);
	mutex_unlock(&mdev->graph_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(media_device_register_entity_notify);

/*
 * Note: Should be called with mdev->lock held.
 */
static void __media_device_unregister_entity_notify(struct media_device *mdev,
					struct media_entity_notify *nptr)
{
	list_del(&nptr->list);
}

void media_device_unregister_entity_notify(struct media_device *mdev,
					struct media_entity_notify *nptr)
{
	mutex_lock(&mdev->graph_mutex);
	__media_device_unregister_entity_notify(mdev, nptr);
	mutex_unlock(&mdev->graph_mutex);
}
EXPORT_SYMBOL_GPL(media_device_unregister_entity_notify);

void media_device_unregister(struct media_device *mdev)
{
	struct media_entity *entity;
	struct media_entity *next;
	struct media_interface *intf, *tmp_intf;
	struct media_entity_notify *notify, *nextp;

	if (mdev == NULL)
		return;

	mutex_lock(&mdev->graph_mutex);

	/* Check if mdev was ever registered at all */
	if (!media_devnode_is_registered(mdev->devnode)) {
		mutex_unlock(&mdev->graph_mutex);
		return;
	}

	/* Clear the devnode register bit to avoid races with media dev open */
	media_devnode_unregister_prepare(mdev->devnode);

	/* Remove all entities from the media device */
	list_for_each_entry_safe(entity, next, &mdev->entities, graph_obj.list)
		__media_device_unregister_entity(entity);

	/* Remove all entity_notify callbacks from the media device */
	list_for_each_entry_safe(notify, nextp, &mdev->entity_notify, list)
		__media_device_unregister_entity_notify(mdev, notify);

	/* Remove all interfaces from the media device */
	list_for_each_entry_safe(intf, tmp_intf, &mdev->interfaces,
				 graph_obj.list) {
		/*
		 * Unlink the interface, but don't free it here; the
		 * module which created it is responsible for freeing
		 * it
		 */
		__media_remove_intf_links(intf);
		media_gobj_destroy(&intf->graph_obj);
	}

	mutex_unlock(&mdev->graph_mutex);

	dev_dbg(mdev->dev, "Media device unregistered\n");

	device_remove_file(&mdev->devnode->dev, &dev_attr_model);
	media_devnode_unregister(mdev->devnode);
	ida_destroy(&mdev->req_ids);
	/* devnode free is handled in media_devnode_*() */
	mdev->devnode = NULL;
}
EXPORT_SYMBOL_GPL(media_device_unregister);

#if IS_ENABLED(CONFIG_PCI)
void media_device_pci_init(struct media_device *mdev,
			   struct pci_dev *pci_dev,
			   const char *name)
{
	mdev->dev = &pci_dev->dev;

	if (name)
		strlcpy(mdev->model, name, sizeof(mdev->model));
	else
		strlcpy(mdev->model, pci_name(pci_dev), sizeof(mdev->model));

	sprintf(mdev->bus_info, "PCI:%s", pci_name(pci_dev));

	mdev->hw_revision = (pci_dev->subsystem_vendor << 16)
			    | pci_dev->subsystem_device;

	media_device_init(mdev);
}
EXPORT_SYMBOL_GPL(media_device_pci_init);
#endif

#if IS_ENABLED(CONFIG_USB)
void __media_device_usb_init(struct media_device *mdev,
			     struct usb_device *udev,
			     const char *board_name,
			     const char *driver_name)
{
	mdev->dev = &udev->dev;

	if (driver_name)
		strlcpy(mdev->driver_name, driver_name,
			sizeof(mdev->driver_name));

	if (board_name)
		strlcpy(mdev->model, board_name, sizeof(mdev->model));
	else if (udev->product)
		strlcpy(mdev->model, udev->product, sizeof(mdev->model));
	else
		strlcpy(mdev->model, "unknown model", sizeof(mdev->model));
	if (udev->serial)
		strlcpy(mdev->serial, udev->serial, sizeof(mdev->serial));
	usb_make_path(udev, mdev->bus_info, sizeof(mdev->bus_info));
	mdev->hw_revision = le16_to_cpu(udev->descriptor.bcdDevice);

	media_device_init(mdev);
}
EXPORT_SYMBOL_GPL(__media_device_usb_init);
#endif


#endif /* CONFIG_MEDIA_CONTROLLER */
