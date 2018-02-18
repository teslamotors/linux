/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra_ast.h>
#include <linux/tegra-ivc-bus.h>

#define NV(p) "nvidia," #p

struct tegra_ivc_bus {
	struct device dev;
	struct tegra_ast *ast;
	struct tegra_ivc_channel *chans;
	u32 sid;
	unsigned num_regions;
	struct tegra_ast_region *regions[];
};

static void tegra_hsp_ring(struct device *dev)
{
	const struct tegra_hsp_ops *ops = tegra_hsp_dev_ops(dev);

	BUG_ON(ops == NULL || ops->ring == NULL);
	ops->ring(dev);
}

static void tegra_ivc_channel_ring(struct ivc *ivc)
{
	struct tegra_ivc_channel *chan =
		container_of(ivc, struct tegra_ivc_channel, ivc);
	struct tegra_ivc_bus *bus =
		container_of(chan->dev.parent, struct tegra_ivc_bus, dev);

	tegra_hsp_ring(&bus->dev);
}

struct device_type tegra_ivc_channel_type = {
	.name = "tegra-ivc-channel",
};
EXPORT_SYMBOL(tegra_ivc_channel_type);

static void tegra_ivc_channel_release(struct device *dev)
{
	struct tegra_ivc_channel *chan =
		container_of(dev, struct tegra_ivc_channel, dev);

	of_node_put(dev->of_node);
	kfree(chan);
}

static int tegra_ivc_bus_check_overlap(struct device *dev,
					const struct tegra_ivc_channel *chan)
{
	const struct tegra_ivc_bus *bus =
		container_of(dev, struct tegra_ivc_bus, dev);
	const struct tegra_ivc_channel *chan2;
	uintptr_t tx = (uintptr_t)chan->ivc.tx_channel;
	uintptr_t rx = (uintptr_t)chan->ivc.rx_channel;
	unsigned s = tegra_ivc_total_queue_size(chan->ivc.nframes *
							chan->ivc.frame_size);

	for (chan2 = bus->chans; chan2 != NULL; chan2 = chan2->next) {
		uintptr_t tx2 = (uintptr_t)chan2->ivc.tx_channel;
		uintptr_t rx2 = (uintptr_t)chan2->ivc.rx_channel;
		unsigned s2 = tegra_ivc_total_queue_size(chan2->ivc.nframes *
							chan2->ivc.frame_size);

		if ((tx < tx2 ? tx + s > tx2 : tx2 + s2 > tx) ||
			(rx < tx2 ? rx + s > tx2 : tx2 + s2 > rx) ||
			(rx < rx2 ? rx + s > rx2 : rx2 + s2 > rx) ||
			(tx < rx2 ? tx + s > rx2 : rx2 + s2 > tx)) {
			dev_err(dev, "buffers for %s and %s overlap\n",
				dev_name(&chan->dev), dev_name(&chan2->dev));
			return -EINVAL;
		}
	}
	return 0;
}

static struct tegra_ivc_channel *tegra_ivc_channel_create(
		struct device *dev, struct device_node *ch_node,
		struct tegra_ast_region *region)
{
	void *base;
	dma_addr_t dma_handle;
	size_t size;
	union {
		u32 tab[2];
		struct {
			u32 rx;
			u32 tx;
		};
	} start, end;
	u32 nframes, frame_size;
	int ret;

	struct tegra_ivc_channel *chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (unlikely(chan == NULL))
		return ERR_PTR(-ENOMEM);

	chan->dev.parent = dev;
	chan->dev.type = &tegra_ivc_channel_type;
	chan->dev.bus = &tegra_ivc_bus_type;
	chan->dev.of_node = of_node_get(ch_node);
	chan->dev.release = tegra_ivc_channel_release;
	dev_set_name(&chan->dev, "%s:%s", dev_name(dev),
			kbasename(ch_node->full_name));
	device_initialize(&chan->dev);
	pm_runtime_no_callbacks(&chan->dev);
	pm_runtime_enable(&chan->dev);

	ret = of_property_read_u32_array(ch_node, "reg", start.tab,
						ARRAY_SIZE(start.tab));
	if (ret) {
		dev_err(&chan->dev, "missing <%s> property\n", "reg");
		goto error;
	}

	ret = of_property_read_u32(ch_node, NV(frame-count), &nframes);
	if (ret) {
		dev_err(&chan->dev, "missing <%s> property\n",
			NV(frame-count));
		goto error;
	}

	ret = of_property_read_u32(ch_node, NV(frame-size), &frame_size);
	if (ret) {
		dev_err(&chan->dev, "missing <%s> property\n", NV(frame-size));
		goto error;
	}

	base = tegra_ast_region_get_mapping(region, &size, &dma_handle);
	ret = -EINVAL;
	end.rx = start.rx + tegra_ivc_total_queue_size(nframes * frame_size);
	end.tx = start.tx + tegra_ivc_total_queue_size(nframes * frame_size);

	if (end.rx > size) {
		dev_err(&chan->dev, "%s buffer exceeds IVC size\n", "RX");
		goto error;
	}

	if (end.tx > size) {
		dev_err(&chan->dev, "%s buffer exceeds IVC size\n", "TX");
		goto error;
	}

	if (start.tx < start.rx ? end.tx > start.rx : end.rx > start.tx) {
		dev_err(&chan->dev, "RX and TX buffers overlap\n");
		goto error;
	}

	ret = tegra_ivc_bus_check_overlap(dev, chan);
	if (ret)
		goto error;

	/* Init IVC */
	ret = tegra_ivc_init_with_dma_handle(&chan->ivc,
			(uintptr_t)base + start.rx, dma_handle + start.rx,
			(uintptr_t)base + start.tx, dma_handle + start.tx,
			nframes, frame_size, dev->parent,
			tegra_ivc_channel_ring);
	if (ret) {
		dev_err(&chan->dev, "IVC initialization error: %d\n", ret);
		goto error;
	}

	dev_dbg(&chan->dev, "%s: RX: 0x%x-0x%x TX: 0x%x-0x%x\n",
		ch_node->name, start.rx, end.rx, start.tx, end.tx);

	ret = device_add(&chan->dev);
	if (ret) {
		dev_err(&chan->dev, "channel device error: %d\n", ret);
		goto error;
	}

	return chan;
error:
	put_device(&chan->dev);
	return ERR_PTR(ret);
}

static void tegra_ivc_channel_notify(struct tegra_ivc_channel *chan)
{
	const struct tegra_ivc_channel_ops *ops;

	rcu_read_lock();
	ops = rcu_dereference(chan->ops);

	if (ops != NULL && ops->notify != NULL)
		ops->notify(chan);
	rcu_read_unlock();
}

void tegra_hsp_notify(struct device *dev)
{
	struct tegra_ivc_bus *bus =
		container_of(dev, struct tegra_ivc_bus, dev);
	struct tegra_ivc_channel *chan;

	for (chan = bus->chans; chan != NULL; chan = chan->next)
		tegra_ivc_channel_notify(chan);
}
EXPORT_SYMBOL(tegra_hsp_notify);

struct device_type tegra_hsp_type = {
	.name = "tegra-hsp",
};
EXPORT_SYMBOL(tegra_hsp_type);

static void tegra_ivc_bus_release(struct device *dev)
{
	struct tegra_ivc_bus *bus =
		container_of(dev, struct tegra_ivc_bus, dev);

	of_node_put(dev->of_node);
	kfree(bus);
}

static int tegra_ivc_bus_match(struct device *dev, struct device_driver *drv)
{
	struct tegra_ivc_driver *ivcdrv = to_tegra_ivc_driver(drv);

	if (dev->type != ivcdrv->dev_type)
		return 0;
	return of_driver_match_device(dev, drv);
}

static void tegra_ivc_bus_stop(struct device *dev)
{
	struct tegra_ivc_bus *bus =
		container_of(dev, struct tegra_ivc_bus, dev);

	while (bus->chans != NULL) {
		struct tegra_ivc_channel *chan = bus->chans;

		bus->chans = chan->next;
		pm_runtime_disable(&chan->dev);
		device_unregister(&chan->dev);
	}
}

static int tegra_ivc_bus_start(struct device *dev)
{
	struct tegra_ivc_bus *bus =
		container_of(dev, struct tegra_ivc_bus, dev);
	struct device_node *dn = bus->dev.parent->of_node;
	struct of_phandle_args reg_spec;
	int i, ret;

	for (i = 0;
		of_parse_phandle_with_fixed_args(dn, NV(ivc-channels), 3,
							i, &reg_spec) == 0;
		i++) {
		struct device_node *ch_node;

		for_each_child_of_node(reg_spec.np, ch_node) {
			struct tegra_ivc_channel *chan;

			chan = tegra_ivc_channel_create(&bus->dev, ch_node,
							bus->regions[i]);
			if (IS_ERR(chan)) {
				ret = PTR_ERR(chan);
				of_node_put(ch_node);
				goto error;
			}

			chan->next = bus->chans;
			bus->chans = chan;
		}
	}

	return 0;
error:
	tegra_ivc_bus_stop(dev);
	return ret;
}

static int tegra_ivc_bus_probe(struct device *dev)
{
	struct tegra_ivc_driver *drv = to_tegra_ivc_driver(dev->driver);
	int ret = -ENXIO;

	if (dev->type == &tegra_ivc_channel_type) {
		struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
		const struct tegra_ivc_channel_ops *ops = drv->ops.channel;

		BUG_ON(ops == NULL);
		if (ops->probe != NULL) {
			ret = ops->probe(chan);
			if (ret)
				return ret;
		}

		rcu_assign_pointer(chan->ops, ops);
		ret = 0;

	} else if (dev->type == &tegra_hsp_type) {
		const struct tegra_hsp_ops *ops = drv->ops.hsp;

		BUG_ON(ops == NULL || ops->probe == NULL);
		ret = ops->probe(dev);
		if (ret)
			return ret;

		ret = tegra_ivc_bus_start(dev);
		if (ret && ops->remove != NULL)
			ops->remove(dev);
	}

	return ret;
}

static int tegra_ivc_bus_remove(struct device *dev)
{
	struct tegra_ivc_driver *drv = to_tegra_ivc_driver(dev->driver);

	if (dev->type == &tegra_ivc_channel_type) {
		struct tegra_ivc_channel *chan = to_tegra_ivc_channel(dev);
		const struct tegra_ivc_channel_ops *ops = drv->ops.channel;

		WARN_ON(rcu_access_pointer(chan->ops) != ops);
		RCU_INIT_POINTER(chan->ops, NULL);
		synchronize_rcu();

		if (ops->remove != NULL)
			ops->remove(chan);

	} else if (dev->type == &tegra_hsp_type) {
		const struct tegra_hsp_ops *ops = drv->ops.hsp;

		tegra_ivc_bus_stop(dev);

		if (ops->remove != NULL)
			ops->remove(dev);
	}

	return 0;
}

struct bus_type tegra_ivc_bus_type = {
	.name	= "tegra-ivc",
	.match	= tegra_ivc_bus_match,
	.probe	= tegra_ivc_bus_probe,
	.remove	= tegra_ivc_bus_remove,
};
EXPORT_SYMBOL(tegra_ivc_bus_type);

int tegra_ivc_driver_register(struct tegra_ivc_driver *drv)
{
	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(tegra_ivc_driver_register);

void tegra_ivc_driver_unregister(struct tegra_ivc_driver *drv)
{
	return driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(tegra_ivc_driver_unregister);

static int tegra_ivc_bus_parse_regions(struct tegra_ivc_bus *bus,
					struct device_node *dev_node)
{
	struct of_phandle_args reg_spec;
	int i;

	/* Parse out all regions in a node */
	for (i = 0;
		of_parse_phandle_with_fixed_args(dev_node, NV(ivc-channels), 3,
							i, &reg_spec) == 0;
		i++) {
		struct tegra_ast_region *region;

		/* Allocate RAM for IVC */
		region = of_tegra_ast_region_map(bus->ast, &reg_spec,
							bus->sid);
		of_node_put(reg_spec.np);

		if (IS_ERR(region))
			return PTR_ERR(region);

		bus->regions[i] = region;
	}

	return 0;
}

static unsigned tegra_ivc_bus_count_regions(const struct device_node *dev_node)
{
	unsigned i;

	for (i = 0; of_parse_phandle_with_fixed_args(dev_node,
			NV(ivc-channels), 3, i, NULL) == 0; i++)
		;

	return i;
}

struct tegra_ivc_bus *tegra_ivc_bus_create(struct device *dev,
						struct tegra_ast *ast, u32 sid)
{
	struct tegra_ivc_bus *bus;
	unsigned num;
	int ret;

	num = tegra_ivc_bus_count_regions(dev->of_node);

	bus = kzalloc(sizeof(*bus) + num * sizeof(*bus->regions), GFP_KERNEL);
	if (unlikely(bus == NULL))
		return ERR_PTR(-ENOMEM);

	bus->ast = ast;
	bus->sid = sid;
	bus->num_regions = num;
	bus->dev.parent = dev;
	bus->dev.type = &tegra_hsp_type;
	bus->dev.bus = &tegra_ivc_bus_type;
	bus->dev.of_node = of_get_child_by_name(dev->of_node, "hsp");
	bus->dev.release = tegra_ivc_bus_release;
	dev_set_name(&bus->dev, "ivc-%s", dev_name(dev));
	device_initialize(&bus->dev);
	pm_runtime_no_callbacks(&bus->dev);
	pm_runtime_enable(&bus->dev);

	ret = tegra_ivc_bus_parse_regions(bus, dev->of_node);
	if (ret) {
		dev_err(&bus->dev, "IVC regions setup failed: %d\n", ret);
		goto error;
	}

	ret = device_add(&bus->dev);
	if (ret) {
		dev_err(&bus->dev, "IVC instance error: %d\n", ret);
		goto error;
	}

	return bus;

error:
	put_device(&bus->dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(tegra_ivc_bus_create);

void tegra_ivc_bus_destroy(struct tegra_ivc_bus *bus)
{
	if (IS_ERR_OR_NULL(bus))
		return;

	pm_runtime_disable(&bus->dev);
	device_unregister(&bus->dev);
}
EXPORT_SYMBOL(tegra_ivc_bus_destroy);

static __init int tegra_ivc_bus_init(void)
{
	return bus_register(&tegra_ivc_bus_type);
}

static __exit void tegra_ivc_bus_exit(void)
{
	bus_unregister(&tegra_ivc_bus_type);
}

subsys_initcall(tegra_ivc_bus_init);
module_exit(tegra_ivc_bus_exit);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra IVC generic bus driver");
MODULE_LICENSE("GPL");
