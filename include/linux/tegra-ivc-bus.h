/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_TEGRA_IVC_BUS_H
#define _LINUX_TEGRA_IVC_BUS_H

#include <linux/tegra-ivc-instance.h>

extern struct bus_type tegra_ivc_bus_type;
struct tegra_ivc_bus;
struct tegra_hsp_ops;
struct tegra_ast;

struct tegra_ivc_bus *tegra_ivc_bus_create(struct device *, struct tegra_ast *,
						u32 sid);
void tegra_ivc_bus_destroy(struct tegra_ivc_bus *ibus);

struct tegra_ivc_driver {
	struct device_driver driver;
	struct device_type *dev_type;
	union {
		const struct tegra_hsp_ops *hsp;
		const struct tegra_ivc_channel_ops *channel;
	} ops;
};

static inline struct tegra_ivc_driver *to_tegra_ivc_driver(
						struct device_driver *drv)
{
	if (drv == NULL)
		return NULL;
	return container_of(drv, struct tegra_ivc_driver, driver);
}

int tegra_ivc_driver_register(struct tegra_ivc_driver *drv);
void tegra_ivc_driver_unregister(struct tegra_ivc_driver *drv);
#define tegra_ivc_module_driver(drv) \
	module_driver(drv, tegra_ivc_driver_register, \
			tegra_ivc_driver_unregister)

/* Tegra HSP driver support */
extern struct device_type tegra_hsp_type;

struct tegra_hsp_ops {
	int (*probe)(struct device *);
	void (*remove)(struct device *);
	void (*ring)(struct device *);
};
void tegra_hsp_notify(struct device *);

static inline const struct tegra_hsp_ops *tegra_hsp_dev_ops(struct device *dev)
{
	struct tegra_ivc_driver *drv = to_tegra_ivc_driver(dev->driver);
	const struct tegra_hsp_ops *ops = NULL;

	if (drv != NULL && drv->dev_type == &tegra_hsp_type)
		ops = drv->ops.hsp;
	return ops;
}

/* IVC channel driver support */
extern struct device_type tegra_ivc_channel_type;

struct tegra_ivc_channel {
	struct ivc ivc;
	struct device dev;
	const struct tegra_ivc_channel_ops __rcu *ops;
	struct tegra_ivc_channel *next;
};

static inline void *tegra_ivc_channel_get_drvdata(
		struct tegra_ivc_channel *chan)
{
	return dev_get_drvdata(&chan->dev);
}

static inline void tegra_ivc_channel_set_drvdata(
		struct tegra_ivc_channel *chan, void *data)
{
	dev_set_drvdata(&chan->dev, data);
}

static inline struct tegra_ivc_channel *to_tegra_ivc_channel(
		struct device *dev)
{
	return container_of(dev, struct tegra_ivc_channel, dev);
}

static inline struct device *tegra_ivc_channel_to_camrtc_dev(
	struct tegra_ivc_channel *ch)
{
	if (unlikely(ch == NULL))
		return NULL;

	BUG_ON(ch->dev.parent == NULL);
	BUG_ON(ch->dev.parent->parent == NULL);

	return ch->dev.parent->parent;
}

struct tegra_ivc_channel_ops {
	int (*probe)(struct tegra_ivc_channel *);
	void (*remove)(struct tegra_ivc_channel *);
	void (*notify)(struct tegra_ivc_channel *);
};

/* Legacy mailbox support */
struct tegra_ivc_mbox_msg {
	int length;
	void *data;
};

#endif
