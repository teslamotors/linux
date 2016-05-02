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

#ifndef _MEDIA_DEVICE_H
#define _MEDIA_DEVICE_H

#include <linux/kref.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#include <media/media-devnode.h>
#include <media/media-entity.h>

#include <uapi/linux/media.h>

struct device;
struct file;
struct media_device;
struct media_device_fh;

enum media_device_request_state {
	MEDIA_DEVICE_REQUEST_STATE_IDLE,
	MEDIA_DEVICE_REQUEST_STATE_QUEUED,
	MEDIA_DEVICE_REQUEST_STATE_DELETED,
	MEDIA_DEVICE_REQUEST_STATE_COMPLETE,
};

/**
 * struct media_device_request - Media device request
 * @id: Request ID
 * @mdev: Media device this request belongs to
 * @kref: Reference count
 * @list: List entry in the media device requests list
 * @fh_list: List entry in the media file handle requests list
 * @state: The state of the request, MEDIA_DEVICE_REQUEST_STATE_*,
 *	   access to state serialised by mdev->req_lock
 * @flags: Request specific flags, MEDIA_REQ_FL_*
 */
struct media_device_request {
	u32 id;
	struct media_device *mdev;
	struct file *filp;
	struct media_kevent *kev;
	struct kref kref;
	struct list_head list;
	struct list_head fh_list;
	enum media_device_request_state state;
	u32 flags;
};

/**
 * struct media_device_ops - Media device operations
 * @link_notify: Link state change notification callback. This callback is
 *		 called with the graph_mutex held.
 * @req_alloc: Allocate a request
 * @req_free: Free a request
 * @req_apply: Apply a request
 * @req_queue: Queue a request
 */
struct media_device_ops {
	int (*link_notify)(struct media_link *link, u32 flags,
			   unsigned int notification);
	struct media_device_request *(*req_alloc)(struct media_device *mdev);
	void (*req_free)(struct media_device *mdev,
			 struct media_device_request *req);
	int (*req_apply)(struct media_device *mdev,
			 struct media_device_request *req);
	int (*req_queue)(struct media_device *mdev,
			 struct media_device_request *req);
};

struct media_kevent {
	struct list_head list;
	struct media_event ev;
};

/**
 * struct media_device - Media device
 * @dev:	Parent device
 * @devnode:	Media device node
 * @model:	Device model name
 * @serial:	Device serial number (optional)
 * @bus_info:	Unique and stable device location identifier
 * @hw_revision: Hardware device revision
 * @driver_version: Device driver version
 * @entity_id:	ID of the next entity to be registered
 * @entities:	List of registered entities
 * @lock:	Entities list lock
 * @graph_mutex: Entities graph operation lock
 * @link_notify: Link state change notification callback
 * @req_ids:	Allocated request IDs
 * @req_lock:	Serialise access to requests list
 * @requests:	List of allocated requests
 *
 * This structure represents an abstract high-level media device. It allows easy
 * access to entities and provides basic media device-level support. The
 * structure can be allocated directly or embedded in a larger structure.
 *
 * The parent @dev is a physical device. It must be set before registering the
 * media device.
 *
 * @model is a descriptive model name exported through sysfs. It doesn't have to
 * be unique.
 */
struct media_device {
	/* dev->driver_data points to this struct. */
	struct device *dev;
	struct media_devnode devnode;

	char model[32];
	char serial[40];
	char bus_info[32];
	u32 hw_revision;
	u32 driver_version;

	u32 entity_id;
	struct list_head entities;

	/* Protects the entities list */
	spinlock_t lock;
	/* Serializes graph operations. */
	struct mutex graph_mutex;

	const struct media_device_ops *ops;

	struct ida req_ids;
	spinlock_t req_lock;
	struct list_head requests;
};

/* Supported link_notify @notification values. */
#define MEDIA_DEV_NOTIFY_PRE_LINK_CH	0
#define MEDIA_DEV_NOTIFY_POST_LINK_CH	1

/* media_devnode to media_device */
#define to_media_device(node) container_of(node, struct media_device, devnode)

int __must_check __media_device_register(struct media_device *mdev,
					 struct module *owner);
#define media_device_register(mdev) __media_device_register(mdev, THIS_MODULE)
void media_device_unregister(struct media_device *mdev);

int __must_check media_device_register_entity(struct media_device *mdev,
					      struct media_entity *entity);
void media_device_unregister_entity(struct media_entity *entity);

/* Iterate over all entities. */
#define media_device_for_each_entity(entity, mdev)			\
	list_for_each_entry(entity, &(mdev)->entities, list)

struct media_device_request *
media_device_request_find(struct media_device *mdev, u16 reqid);
void media_device_request_get(struct media_device_request *req);
void media_device_request_put(struct media_device_request *req);
void media_device_request_complete(struct media_device *mdev,
				   struct media_device_request *req);

#endif
