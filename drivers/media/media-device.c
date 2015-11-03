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

#include <linux/compat.h>
#include <linux/export.h>
#include <linux/ioctl.h>
#include <linux/media.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <media/media-device.h>
#include <media/media-devnode.h>
#include <media/media-entity.h>

struct media_device_fh {
	struct media_devnode_fh fh;
};

static inline struct media_device_fh *media_device_fh(struct file *filp)
{
	return container_of(filp->private_data, struct media_device_fh, fh);
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

	filp->private_data = &fh->fh;

	return 0;
}

static int media_device_close(struct file *filp)
{
	struct media_device_fh *fh = media_device_fh(filp);

	kfree(fh);

	return 0;
}

static int media_device_get_info(struct media_device *dev,
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
		.fn = (long (*)(struct media_device *, void *))func,	\
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
	long (*fn)(struct media_device *dev, void *arg);
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

	ret = info->fn(dev, karg);

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
};

static long media_device_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	return __media_device_ioctl(
		filp, cmd, (void __user *)arg,
		ioctl_info, ARRAY_SIZE(ioctl_info));
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
