// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/version.h>

#include <media/ipu-isys.h>
#include <media/ipu4-acpi.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#include <media/v4l2-mc.h>
#endif
#include <media/v4l2-subdev.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-cpd.h"
#include "ipu-mmu.h"
#include "ipu-dma.h"
#include "ipu-isys.h"
#include "ipu-isys-csi2.h"
#include "ipu-isys-tpg.h"
#include "ipu-isys-video.h"
#include "ipu-platform-regs.h"
#include "ipu-buttress.h"
#include "ipu-platform.h"
#include "ipu-platform-buttress-regs.h"

#define ISYS_PM_QOS_VALUE	300

/*
 * The param was passed from module to indicate if port
 * could be optimized.
 */
static bool csi2_port_optimized;
module_param(csi2_port_optimized, bool, 0660);
MODULE_PARM_DESC(csi2_port_optimized, "IPU CSI2 port optimization");

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/*
 * BEGIN adapted code from drivers/media/platform/omap3isp/isp.c.
 * FIXME: This (in terms of functionality if not code) should be most
 * likely generalised in the framework, and use made optional for
 * drivers.
 */
/*
 * ipu_pipeline_pm_use_count - Count the number of users of a pipeline
 * @entity: The entity
 *
 * Return the total number of users of all video device nodes in the pipeline.
 */
static int ipu_pipeline_pm_use_count(struct media_pad *pad)
{
	struct media_entity_graph graph;
	struct media_entity *entity = pad->entity;
	int use = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_graph_walk_start(&graph, pad);

	while ((entity = media_graph_walk_next(&graph))) {
		if (is_media_entity_v4l2_io(entity))
			use += entity->use_count;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_cleanup(&graph);
#endif
	return use;
}

/*
 * ipu_pipeline_pm_power_one - Apply power change to an entity
 * @entity: The entity
 * @change: Use count change
 *
 * Change the entity use count by @change. If the entity is a subdev update its
 * power state by calling the core::s_power operation when the use count goes
 * from 0 to != 0 or from != 0 to 0.
 *
 * Return 0 on success or a negative error code on failure.
 */
static int ipu_pipeline_pm_power_one(struct media_entity *entity, int change)
{
	struct v4l2_subdev *subdev;
	int ret;

	subdev = is_media_entity_v4l2_subdev(entity)
	    ? media_entity_to_v4l2_subdev(entity) : NULL;

	if (entity->use_count == 0 && change > 0 && subdev) {
		ret = v4l2_subdev_call(subdev, core, s_power, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
	}

	entity->use_count += change;
	WARN_ON(entity->use_count < 0);

	if (entity->use_count == 0 && change < 0 && subdev)
		v4l2_subdev_call(subdev, core, s_power, 0);

	return 0;
}

/*
 * ipu_get_linked_pad - Find internally connected pad for a given pad
 * @entity: The entity
 * @pad: Initial pad
 *
 * Return index of the linked pad.
 */
static int ipu_get_linked_pad(struct media_entity *entity,
			      struct media_pad *pad)
{
	int i;

	for (i = 0; i < entity->num_pads; i++) {
		struct media_pad *opposite_pad = &entity->pads[i];

		if (opposite_pad == pad)
			continue;

		if (media_entity_has_route(entity, pad->index,
					   opposite_pad->index))
			return opposite_pad->index;
	}

	return 0;
}

/*
 * ipu_pipeline_pm_power - Apply power change to all entities
 * in a pipeline
 * @entity: The entity
 * @change: Use count change
 * @from_pad: Starting pad
 *
 * Walk the pipeline to update the use count and the power state of
 * all non-node
 * entities.
 *
 * Return 0 on success or a negative error code on failure.
 */
static int ipu_pipeline_pm_power(struct media_entity *entity,
				 int change, int from_pad)
{
	struct media_entity_graph graph;
	struct media_entity *first = entity;
	int ret = 0;

	if (!change)
		return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_graph_walk_start(&graph, &entity->pads[from_pad]);

	while (!ret && (entity = media_graph_walk_next(&graph)))
		if (!is_media_entity_v4l2_io(entity))
			ret = ipu_pipeline_pm_power_one(entity, change);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_cleanup(&graph);
#endif
	if (!ret)
		return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_graph_walk_start(&graph, &first->pads[from_pad]);

	while ((first = media_graph_walk_next(&graph)) &&
	       first != entity)
		if (!is_media_entity_v4l2_io(first))
			ipu_pipeline_pm_power_one(first, -change);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_graph_walk_cleanup(&graph);
#endif
	return ret;
}

/*
 * ipu_pipeline_pm_use - Update the use count of an entity
 * @entity: The entity
 * @use: Use (1) or stop using (0) the entity
 *
 * Update the use count of all entities in the pipeline and power entities
 * on or off accordingly.
 *
 * Return 0 on success or a negative error code on failure. Powering entities
 * off is assumed to never fail. No failure can occur when the use parameter is
 * set to 0.
 */
int ipu_pipeline_pm_use(struct media_entity *entity, int use)
{
	int change = use ? 1 : -1;
	int ret;

	mutex_lock(&entity->
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		   parent
#else
		   graph_obj.mdev
#endif
		   ->graph_mutex);

	/* Apply use count to node. */
	entity->use_count += change;
	WARN_ON(entity->use_count < 0);

	/* Apply power change to connected non-nodes. */
	ret = ipu_pipeline_pm_power(entity, change, 0);
	if (ret < 0)
		entity->use_count -= change;

	mutex_unlock(&entity->
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		     parent
#else
		     graph_obj.mdev
#endif
		     ->graph_mutex);

	return ret;
}

/*
 * ipu_pipeline_link_notify - Link management notification callback
 * @link: The link
 * @flags: New link flags that will be applied
 * @notification: The link's state change notification type
 * (MEDIA_DEV_NOTIFY_*)
 *
 * React to link management on powered pipelines by updating the use count of
 * all entities in the source and sink sides of the link. Entities are powered
 * on or off accordingly.
 *
 * Return 0 on success or a negative error code on failure. Powering entities
 * off is assumed to never fail. This function will not fail for disconnection
 * events.
 */
static int ipu_pipeline_link_notify(struct media_link *link, u32 flags,
				    unsigned int notification)
{
	struct media_entity *source = link->source->entity;
	struct media_entity *sink = link->sink->entity;
	int source_use = ipu_pipeline_pm_use_count(link->source);
	int sink_use = ipu_pipeline_pm_use_count(link->sink);
	int ret;

	if (notification == MEDIA_DEV_NOTIFY_POST_LINK_CH &&
	    !(flags & MEDIA_LNK_FL_ENABLED)) {
		/* Powering off entities is assumed to never fail. */
		ipu_pipeline_pm_power(source, -sink_use, 0);
		ipu_pipeline_pm_power(sink, -source_use, 0);
		return 0;
	}

	if (notification == MEDIA_DEV_NOTIFY_PRE_LINK_CH &&
	    (flags & MEDIA_LNK_FL_ENABLED)) {
		int from_pad = ipu_get_linked_pad(source, link->source);

		ret = ipu_pipeline_pm_power(source, sink_use, from_pad);
		if (ret < 0)
			return ret;

		ret = ipu_pipeline_pm_power(sink, source_use, 0);
		if (ret < 0)
			ipu_pipeline_pm_power(source, -sink_use, 0);

		return ret;
	}

	return 0;
}

/* END adapted code from drivers/media/platform/omap3isp/isp.c */
#endif /* < v4.6 */

struct isys_i2c_test {
	u8 bus_nr;
	u16 addr;
	struct i2c_client *client;
};

static int isys_i2c_test(struct device *dev, void *priv)
{
	struct i2c_client *client = i2c_verify_client(dev);
	struct isys_i2c_test *test = priv;

	if (!client)
		return 0;

	if (i2c_adapter_id(client->adapter) != test->bus_nr ||
	    client->addr != test->addr)
		return 0;

	test->client = client;

	return 0;
}

static struct
i2c_client *isys_find_i2c_subdev(struct i2c_adapter *adapter,
				 struct ipu_isys_subdev_info *sd_info)
{
	struct i2c_board_info *info = &sd_info->i2c.board_info;
	struct isys_i2c_test test = {
		.bus_nr = i2c_adapter_id(adapter),
		.addr = info->addr,
	};
	int rval;

	rval = i2c_for_each_dev(&test, isys_i2c_test);
	if (rval || !test.client)
		return NULL;
	return test.client;
}

static struct v4l2_subdev *
register_acpi_i2c_subdev(struct v4l2_device *v4l2_dev,
                        struct ipu_isys_subdev_info *sd_info,
                        struct i2c_client *client)
{
       struct i2c_board_info *info = &sd_info->i2c.board_info;
       struct v4l2_subdev *sd;

       request_module(I2C_MODULE_PREFIX "%s", info->type);

       /* ACPI overwrite with platform data */
       client->dev.platform_data = info->platform_data;
       /* Change I2C client name to one in temporary platform data */
       strlcpy(client->name, info->type, sizeof(client->name));

       if (device_reprobe(&client->dev))
               return NULL;

       if (!client->dev.driver)
               return NULL;

       if (!try_module_get(client->dev.driver->owner))
               return NULL;

       sd = i2c_get_clientdata(client);

       if (v4l2_device_register_subdev(v4l2_dev, sd))
               sd = NULL;

       module_put(client->dev.driver->owner);

       return sd;
}

static int
isys_complete_ext_device_registration(struct ipu_isys *isys,
				      struct v4l2_subdev *sd,
				      struct ipu_isys_csi2_config *csi2)
{
	unsigned int i;
	int rval;

	v4l2_set_subdev_hostdata(sd, csi2);

	for (i = 0; i < sd->entity.num_pads; i++) {
		if (sd->entity.pads[i].flags & MEDIA_PAD_FL_SOURCE)
			break;
	}

	if (i == sd->entity.num_pads) {
		dev_warn(&isys->adev->dev,
			 "no source pad in external entity\n");
		rval = -ENOENT;
		goto skip_unregister_subdev;
	}

	rval = media_create_pad_link(&sd->entity, i,
				     &isys->csi2[csi2->port].asd.sd.entity,
				     0, 0);
	if (rval) {
		dev_warn(&isys->adev->dev, "can't create link\n");
		goto skip_unregister_subdev;
	}

	isys->csi2[csi2->port].nlanes = csi2->nlanes;
	return 0;

skip_unregister_subdev:
	v4l2_device_unregister_subdev(sd);
	return rval;
}

static int isys_register_ext_subdev(struct ipu_isys *isys,
				    struct ipu_isys_subdev_info *sd_info,
				    bool acpi_only)
{
	struct i2c_adapter *adapter =
	    i2c_get_adapter(sd_info->i2c.i2c_adapter_id);
	struct v4l2_subdev *sd;
	struct i2c_client *client;
	int rval;

	dev_info(&isys->adev->dev,
		 "creating new i2c subdev for %s (address %2.2x, bus %d)",
		 sd_info->i2c.board_info.type, sd_info->i2c.board_info.addr,
		 sd_info->i2c.i2c_adapter_id);

	if (!adapter) {
		dev_warn(&isys->adev->dev, "can't find adapter\n");
		return -ENOENT;
	}
	if (sd_info->csi2) {
		dev_info(&isys->adev->dev, "sensor device on CSI port: %d\n",
			 sd_info->csi2->port);
		if (sd_info->csi2->port >= isys->pdata->ipdata->csi2.nports ||
		    !isys->csi2[sd_info->csi2->port].isys) {
			dev_warn(&isys->adev->dev, "invalid csi2 port %u\n",
				 sd_info->csi2->port);
			rval = -EINVAL;
			goto skip_put_adapter;
		}
	} else {
		dev_info(&isys->adev->dev, "non camera subdevice\n");
	}

	client = isys_find_i2c_subdev(adapter, sd_info);

	if (acpi_only) {
		if (!client) {
			dev_dbg(&isys->adev->dev, "Matching ACPI device not found - postpone\n");
			rval = 0;
			goto skip_put_adapter;
		}
		if (!sd_info->acpiname) {
			dev_dbg(&isys->adev->dev, "No name in platform data\n");
			rval = 0;
			goto skip_put_adapter;
		}
		if (strcmp(dev_name(&client->dev), sd_info->acpiname)) {
			dev_dbg(&isys->adev->dev, "Names don't match: %s != %s",
                               dev_name(&client->dev), sd_info->acpiname);
			rval = 0;
			goto skip_put_adapter;
		}
               /* Acpi match found. Continue to reprobe */
	} else if (client) {
		dev_dbg(&isys->adev->dev, "Device exists\n");
		rval = 0;
		goto skip_put_adapter;
	} else if (sd_info->acpiname) {
		dev_dbg(&isys->adev->dev, "ACPI name don't match: %s\n",
                       sd_info->acpiname);
		rval = 0;
		goto skip_put_adapter;
	}

        if (!client) {
                dev_info(&isys->adev->dev,
                         "i2c device not found in ACPI table\n");
                sd = v4l2_i2c_new_subdev_board(&isys->v4l2_dev, adapter,
                                               &sd_info->i2c.board_info, 0);
        } else {
                dev_info(&isys->adev->dev, "i2c device found in ACPI table\n");
                sd = register_acpi_i2c_subdev(&isys->v4l2_dev,
                                              sd_info, client);
        }

	if (!sd) {
		dev_warn(&isys->adev->dev, "can't create new i2c subdev\n");
		rval = -EINVAL;
		goto skip_put_adapter;
	}
	if (!sd_info->csi2)
		return 0;

	return isys_complete_ext_device_registration(isys, sd, sd_info->csi2);

skip_put_adapter:
	i2c_put_adapter(adapter);

	return rval;
}

static int isys_acpi_add_device(struct device *dev, void *priv,
                               struct ipu_isys_csi2_config *csi2,
                               bool reprobe)
{
       struct ipu_isys *isys = priv;
       struct i2c_client *client = i2c_verify_client(dev);
       struct v4l2_subdev *sd;

       if (!client)
               return -ENODEV;

       if (reprobe)
               if (device_reprobe(&client->dev))
                       return -ENODEV;

       if (!client->dev.driver)
               return -ENODEV;

       /* Lock the module so we can safely get the v4l2_subdev pointer */
       if (!try_module_get(client->dev.driver->owner))
               return -ENODEV;

       sd = i2c_get_clientdata(client);

       if (v4l2_device_register_subdev(&isys->v4l2_dev, sd)) {
               dev_warn(&isys->adev->dev, "can't create new i2c subdev\n");
               goto leave_module_put;
       }
       module_put(client->dev.driver->owner);

       if (!csi2)
               return 0;

       return isys_complete_ext_device_registration(isys, sd, csi2);

leave_module_put:
       module_put(client->dev.driver->owner);
       return -ENODEV;
}

static void isys_register_ext_subdevs(struct ipu_isys *isys)
{
	struct ipu_isys_subdev_pdata *spdata = isys->pdata->spdata;
	struct ipu_isys_subdev_info **sd_info;

        if (spdata) {
                /* Scan spdata first to possibly override ACPI data */
                /* ACPI created devices */
                for (sd_info = spdata->subdevs; *sd_info; sd_info++)
                        isys_register_ext_subdev(isys, *sd_info, true);
 
                /* Scan non-acpi devices */
                for (sd_info = spdata->subdevs; *sd_info; sd_info++)
                        isys_register_ext_subdev(isys, *sd_info, false);
        } else {
		dev_info(&isys->adev->dev, "no subdevice info provided\n");
	}

        /* Handle real ACPI stuff */
        request_module("ipu4-acpi");
        ipu_get_acpi_devices(isys, &isys->adev->dev,
                                    isys_acpi_add_device);

}

static void isys_unregister_subdevices(struct ipu_isys *isys)
{
	const struct ipu_isys_internal_tpg_pdata *tpg =
	    &isys->pdata->ipdata->tpg;
	const struct ipu_isys_internal_csi2_pdata *csi2 =
	    &isys->pdata->ipdata->csi2;
	unsigned int i;

	ipu_isys_csi2_be_cleanup(&isys->csi2_be);
	ipu_isys_csi2_be_soc_cleanup(&isys->csi2_be_soc);

	ipu_isys_isa_cleanup(&isys->isa);

	for (i = 0; i < tpg->ntpgs; i++)
		ipu_isys_tpg_cleanup(&isys->tpg[i]);

	for (i = 0; i < csi2->nports; i++)
		ipu_isys_csi2_cleanup(&isys->csi2[i]);
}

static int isys_register_subdevices(struct ipu_isys *isys)
{
	const struct ipu_isys_internal_tpg_pdata *tpg =
	    &isys->pdata->ipdata->tpg;
	const struct ipu_isys_internal_csi2_pdata *csi2 =
	    &isys->pdata->ipdata->csi2;
	struct ipu_isys_subdev_pdata *spdata = isys->pdata->spdata;
	struct ipu_isys_subdev_info **sd_info;
	DECLARE_BITMAP(csi2_enable, 32);
	unsigned int i, j, k;
	int rval;

	/*
	 * Here is somewhat a workaround, let each platform decide
	 * if csi2 port can be optimized, which means only registered
	 * port from pdata would be enabled.
	 */
	if (csi2_port_optimized && spdata) {
		bitmap_zero(csi2_enable, 32);
		for (sd_info = spdata->subdevs; *sd_info; sd_info++) {
			if ((*sd_info)->csi2) {
				i = (*sd_info)->csi2->port;
				if (i >= csi2->nports) {
					dev_warn(&isys->adev->dev,
						 "invalid csi2 port %u\n", i);
					continue;
				}
				bitmap_set(csi2_enable, i, 1);
			}
		}
	} else {
		bitmap_fill(csi2_enable, 32);
	}

	isys->csi2 = devm_kcalloc(&isys->adev->dev, csi2->nports,
				  sizeof(*isys->csi2), GFP_KERNEL);
	if (!isys->csi2) {
		rval = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < csi2->nports; i++) {
		if (!test_bit(i, csi2_enable))
			continue;

		rval = ipu_isys_csi2_init(&isys->csi2[i], isys,
					  isys->pdata->base +
					  csi2->offsets[i], i);
		if (rval)
			goto fail;

		isys->isr_csi2_bits |= IPU_ISYS_UNISPART_IRQ_CSI2(i);
	}

	isys->tpg = devm_kcalloc(&isys->adev->dev, tpg->ntpgs,
				 sizeof(*isys->tpg), GFP_KERNEL);
	if (!isys->tpg) {
		rval = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = ipu_isys_tpg_init(&isys->tpg[i], isys,
					 isys->pdata->base +
					 tpg->offsets[i],
					 tpg->sels ? (isys->pdata->base +
						      tpg->sels[i]) : NULL, i);
		if (rval)
			goto fail;
	}

	rval = ipu_isys_csi2_be_soc_init(&isys->csi2_be_soc, isys);
	if (rval) {
		dev_info(&isys->adev->dev,
			 "can't register soc csi2 be device\n");
		goto fail;
	}

	rval = ipu_isys_csi2_be_init(&isys->csi2_be, isys);
	if (rval) {
		dev_info(&isys->adev->dev,
			 "can't register raw csi2 be device\n");
		goto fail;
	}
	rval = ipu_isys_isa_init(&isys->isa, isys, NULL);
	if (rval) {
		dev_info(&isys->adev->dev, "can't register isa device\n");
		goto fail;
	}

	for (i = 0; i < csi2->nports; i++) {
		if (!test_bit(i, csi2_enable))
			continue;

		for (j = CSI2_PAD_SOURCE(0);
		     j < (NR_OF_CSI2_SOURCE_PADS + CSI2_PAD_SOURCE(0)); j++) {
			rval =
			    media_create_pad_link(&isys->csi2[i].asd.sd.entity,
						  j,
						  &isys->csi2_be.asd.sd.entity,
						  CSI2_BE_PAD_SINK, 0);
			if (rval) {
				dev_info(&isys->adev->dev,
					 "can't create link csi2 <=> csi2_be\n");
				goto fail;
			}

			for (k = CSI2_BE_SOC_PAD_SINK(0);
			     k < NR_OF_CSI2_BE_SOC_SINK_PADS; k++) {
				rval =
				    media_create_pad_link(&isys->csi2[i].asd.sd.
							  entity, j,
							  &isys->csi2_be_soc.
							  asd.sd.entity, k,
							  MEDIA_LNK_FL_DYNAMIC);
				if (rval) {
					dev_info(&isys->adev->dev,
						 "can't create link csi2->be_soc\n");
					goto fail;
				}
			}
		}
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = media_create_pad_link(&isys->tpg[i].asd.sd.entity,
					     TPG_PAD_SOURCE,
					     &isys->csi2_be.asd.sd.entity,
					     CSI2_BE_PAD_SINK, 0);
		if (rval) {
			dev_info(&isys->adev->dev,
				 "can't create link between tpg and csi2_be\n");
			goto fail;
		}

		for (k = CSI2_BE_SOC_PAD_SINK(0);
		     k < NR_OF_CSI2_BE_SOC_SINK_PADS; k++) {
			rval =
			    media_create_pad_link(&isys->tpg[i].asd.sd.entity,
						  TPG_PAD_SOURCE,
						  &isys->csi2_be_soc.asd.sd.
						  entity, k, 0);
			if (rval) {
				dev_info(&isys->adev->dev,
					 "can't create link tpg->be_soc\n");
				goto fail;
			}
		}
	}

	rval = media_create_pad_link(&isys->csi2_be.asd.sd.entity,
				     CSI2_BE_PAD_SOURCE,
				     &isys->isa.asd.sd.entity, ISA_PAD_SINK, 0);
	if (rval) {
		dev_info(&isys->adev->dev,
			 "can't create link between CSI2 raw be and ISA\n");
		goto fail;
	}
	return 0;

fail:
	isys_unregister_subdevices(isys);
	return rval;
}

static struct media_device_ops isys_mdev_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	.link_notify = ipu_pipeline_link_notify,
#else
	.link_notify = v4l2_pipeline_link_notify,
#endif
	.req_alloc = ipu_isys_req_alloc,
	.req_free = ipu_isys_req_free,
	.req_queue = ipu_isys_req_queue,
};

static int isys_register_devices(struct ipu_isys *isys)
{
	int rval;

	isys->media_dev.dev = &isys->adev->dev;
	isys->media_dev.ops = &isys_mdev_ops;
	    strlcpy(isys->media_dev.model,
		    IPU_MEDIA_DEV_MODEL_NAME, sizeof(isys->media_dev.model));
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	isys->media_dev.driver_version = LINUX_VERSION_CODE;
#endif
	snprintf(isys->media_dev.bus_info, sizeof(isys->media_dev.bus_info),
		 "pci:%s", dev_name(isys->adev->dev.parent->parent));
	strlcpy(isys->v4l2_dev.name, isys->media_dev.model,
		sizeof(isys->v4l2_dev.name));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_device_init(&isys->media_dev);
#endif

	rval = media_device_register(&isys->media_dev);
	if (rval < 0) {
		dev_info(&isys->adev->dev, "can't register media device\n");
		goto out_media_device_unregister;
	}

	isys->v4l2_dev.mdev = &isys->media_dev;

	rval = v4l2_device_register(&isys->adev->dev, &isys->v4l2_dev);
	if (rval < 0) {
		dev_info(&isys->adev->dev, "can't register v4l2 device\n");
		goto out_media_device_unregister;
	}

	rval = isys_register_subdevices(isys);
	if (rval)
		goto out_v4l2_device_unregister;

	isys_register_ext_subdevs(isys);

	rval = v4l2_device_register_subdev_nodes(&isys->v4l2_dev);
	if (rval)
		goto out_isys_unregister_subdevices;

	return 0;

out_isys_unregister_subdevices:
	isys_unregister_subdevices(isys);

out_v4l2_device_unregister:
	v4l2_device_unregister(&isys->v4l2_dev);

out_media_device_unregister:
	media_device_unregister(&isys->media_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_device_cleanup(&isys->media_dev);
#endif

	return rval;
}

static void isys_unregister_devices(struct ipu_isys *isys)
{
	isys_unregister_subdevices(isys);
	v4l2_device_unregister(&isys->v4l2_dev);
	media_device_unregister(&isys->media_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_device_cleanup(&isys->media_dev);
#endif
}

#ifdef CONFIG_PM
static int isys_runtime_pm_resume(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_device *isp = adev->isp;
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	unsigned long flags;
	int ret;

	if (!isys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	ipu_trace_restore(dev);

	pm_qos_update_request(&isys->pm_qos, ISYS_PM_QOS_VALUE);

	ret = ipu_buttress_start_tsc_sync(isp);
	if (ret)
		return ret;

	spin_lock_irqsave(&isys->power_lock, flags);
	isys->power = 1;
	spin_unlock_irqrestore(&isys->power_lock, flags);

	if (isys->short_packet_source == IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		mutex_lock(&isys->short_packet_tracing_mutex);
		isys->short_packet_tracing_count = 0;
		mutex_unlock(&isys->short_packet_tracing_mutex);
	}
	isys_setup_hw(isys);

	return 0;
}

static int isys_runtime_pm_suspend(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	unsigned long flags;

	if (!isys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&isys->power_lock, flags);
	isys->power = 0;
	spin_unlock_irqrestore(&isys->power_lock, flags);

	ipu_trace_stop(dev);
	mutex_lock(&isys->mutex);
	isys->reset_needed = false;
	mutex_unlock(&isys->mutex);

	pm_qos_update_request(&isys->pm_qos, PM_QOS_DEFAULT_VALUE);

	return 0;
}

static int isys_suspend(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);

	/* If stream is open, refuse to suspend */
	if (isys->stream_opened)
		return -EBUSY;

	return 0;
}

static int isys_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops isys_pm_ops = {
	.runtime_suspend = isys_runtime_pm_suspend,
	.runtime_resume = isys_runtime_pm_resume,
	.suspend = isys_suspend,
	.resume = isys_resume,
};

#define ISYS_PM_OPS (&isys_pm_ops)
#else
#define ISYS_PM_OPS NULL
#endif

static void isys_remove(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	struct ipu_device *isp = adev->isp;
	struct isys_fw_msgs *fwmsg, *safe;

	dev_info(&adev->dev, "removed\n");
	if (isp->ipu_dir)
		debugfs_remove_recursive(isys->debugfsdir);

	list_for_each_entry_safe(fwmsg, safe, &isys->framebuflist, head) {
		dma_free_attrs(&adev->dev, sizeof(struct isys_fw_msgs),
			       fwmsg, fwmsg->dma_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       NULL
#else
			       0
#endif
		    );
	}

	list_for_each_entry_safe(fwmsg, safe, &isys->framebuflist_fw, head) {
		dma_free_attrs(&adev->dev, sizeof(struct isys_fw_msgs),
			       fwmsg, fwmsg->dma_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       NULL
#else
			       0
#endif
		    );
	}

	ipu_trace_uninit(&adev->dev);
	isys_unregister_devices(isys);
	pm_qos_remove_request(&isys->pm_qos);

	if (!isp->secure_mode) {
		ipu_cpd_free_pkg_dir(adev, isys->pkg_dir,
				     isys->pkg_dir_dma_addr,
				     isys->pkg_dir_size);
		ipu_buttress_unmap_fw_image(adev, &isys->fw_sgt);
		release_firmware(isys->fw);
	}

	mutex_destroy(&isys->stream_mutex);
	mutex_destroy(&isys->mutex);

	if (isys->short_packet_source == IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		u32 trace_size = IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
		struct dma_attrs attrs;

		init_dma_attrs(&attrs);
		dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
		dma_free_attrs(&adev->dev, trace_size,
			       isys->short_packet_trace_buffer,
			       isys->short_packet_trace_buffer_dma_addr,
			       &attrs);
#else
		unsigned long attrs;

		attrs = DMA_ATTR_NON_CONSISTENT;
		dma_free_attrs(&adev->dev, trace_size,
			       isys->short_packet_trace_buffer,
			       isys->short_packet_trace_buffer_dma_addr, attrs);
#endif
	}
}

static int ipu_isys_icache_prefetch_get(void *data, u64 *val)
{
	struct ipu_isys *isys = data;

	*val = isys->icache_prefetch;
	return 0;
}

static int ipu_isys_icache_prefetch_set(void *data, u64 val)
{
	struct ipu_isys *isys = data;

	if (val != !!val)
		return -EINVAL;

	isys->icache_prefetch = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(isys_icache_prefetch_fops,
			ipu_isys_icache_prefetch_get,
			ipu_isys_icache_prefetch_set, "%llu\n");

static int ipu_isys_init_debugfs(struct ipu_isys *isys)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir("isys", isys->adev->isp->ipu_dir);
	if (IS_ERR(dir))
		return -ENOMEM;

	file = debugfs_create_file("icache_prefetch", 0600,
				   dir, isys, &isys_icache_prefetch_fops);
	if (IS_ERR(file))
		goto err;

	isys->debugfsdir = dir;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

static int alloc_fw_msg_buffers(struct ipu_isys *isys, int amount)
{
	dma_addr_t dma_addr;
	struct isys_fw_msgs *addr;
	unsigned int i;
	unsigned long flags;

	for (i = 0; i < amount; i++) {
		addr = dma_alloc_attrs(&isys->adev->dev,
				       sizeof(struct isys_fw_msgs),
				       &dma_addr, GFP_KERNEL,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
				       NULL
#else
				       0
#endif
		    );
		if (!addr)
			break;
		addr->dma_addr = dma_addr;

		spin_lock_irqsave(&isys->listlock, flags);
		list_add(&addr->head, &isys->framebuflist);
		spin_unlock_irqrestore(&isys->listlock, flags);
	}
	if (i == amount)
		return 0;
	spin_lock_irqsave(&isys->listlock, flags);
	while (!list_empty(&isys->framebuflist)) {
		addr = list_first_entry(&isys->framebuflist,
					struct isys_fw_msgs, head);
		list_del(&addr->head);
		spin_unlock_irqrestore(&isys->listlock, flags);
		dma_free_attrs(&isys->adev->dev,
			       sizeof(struct isys_fw_msgs),
			       addr, addr->dma_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       NULL
#else
			       0
#endif
		    );
		spin_lock_irqsave(&isys->listlock, flags);
	}
	spin_unlock_irqrestore(&isys->listlock, flags);
	return -ENOMEM;
}

struct isys_fw_msgs *ipu_get_fw_msg_buf(struct ipu_isys_pipeline *ip)
{
	struct ipu_isys_video *pipe_av =
	    container_of(ip, struct ipu_isys_video, ip);
	struct ipu_isys *isys;
	struct isys_fw_msgs *msg;
	unsigned long flags;

	isys = pipe_av->isys;

	spin_lock_irqsave(&isys->listlock, flags);
	if (list_empty(&isys->framebuflist)) {
		spin_unlock_irqrestore(&isys->listlock, flags);
		dev_dbg(&isys->adev->dev, "Frame list empty - Allocate more");

		alloc_fw_msg_buffers(isys, 5);

		spin_lock_irqsave(&isys->listlock, flags);
		if (list_empty(&isys->framebuflist)) {
			dev_err(&isys->adev->dev, "Frame list empty");
			spin_unlock_irqrestore(&isys->listlock, flags);
			return NULL;
		}
	}
	msg = list_last_entry(&isys->framebuflist, struct isys_fw_msgs, head);
	list_move(&msg->head, &isys->framebuflist_fw);
	spin_unlock_irqrestore(&isys->listlock, flags);
	memset(&msg->fw_msg, 0, sizeof(msg->fw_msg));

	return msg;
}

void ipu_cleanup_fw_msg_bufs(struct ipu_isys *isys)
{
	struct isys_fw_msgs *fwmsg, *fwmsg0;
	unsigned long flags;

	spin_lock_irqsave(&isys->listlock, flags);
	list_for_each_entry_safe(fwmsg, fwmsg0, &isys->framebuflist_fw, head)
		list_move(&fwmsg->head, &isys->framebuflist);
	spin_unlock_irqrestore(&isys->listlock, flags);
}

void ipu_put_fw_mgs_buffer(struct ipu_isys *isys, u64 data)
{
	struct isys_fw_msgs *msg;
	u64 *ptr = (u64 *)(unsigned long)data;

	if (!ptr)
		return;

	spin_lock(&isys->listlock);
	msg = container_of(ptr, struct isys_fw_msgs, fw_msg.dummy);
	list_move(&msg->head, &isys->framebuflist);
	spin_unlock(&isys->listlock);
}
EXPORT_SYMBOL_GPL(ipu_put_fw_mgs_buffer);

static int isys_probe(struct ipu_bus_device *adev)
{
	struct ipu_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct ipu_isys *isys;
	struct ipu_device *isp = adev->isp;
	const u32 trace_size = IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE;
	dma_addr_t *trace_dma_addr;

	const struct firmware *uninitialized_var(fw);
	int rval = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	struct dma_attrs attrs;
#else
	unsigned long attrs;
#endif

	trace_printk("B|%d|TMWK\n", current->pid);

	/* Has the domain been attached? */
	if (!mmu || !isp->pkg_dir_dma_addr) {
		trace_printk("E|TMWK\n");
		return -EPROBE_DEFER;
	}

	isys = devm_kzalloc(&adev->dev, sizeof(*isys), GFP_KERNEL);
	if (!isys)
		return -ENOMEM;

	/* By default, short packet is captured from T-Unit. */
#if defined(CONFIG_VIDEO_INTEL_IPU4) || defined(CONFIG_VIDEO_INTEL_IPU4P)
	isys->short_packet_source = IPU_ISYS_SHORT_PACKET_FROM_TUNIT;
	trace_dma_addr = &isys->short_packet_trace_buffer_dma_addr;
	mutex_init(&isys->short_packet_tracing_mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
	isys->short_packet_trace_buffer =
	    dma_alloc_attrs(&adev->dev, trace_size, trace_dma_addr,
			    GFP_KERNEL, &attrs);
#else
	attrs = DMA_ATTR_NON_CONSISTENT;
	isys->short_packet_trace_buffer =
	    dma_alloc_attrs(&adev->dev, trace_size, trace_dma_addr,
			    GFP_KERNEL, attrs);
#endif
	if (!isys->short_packet_trace_buffer)
		return -ENOMEM;
#else
	isys->short_packet_source = IPU_ISYS_SHORT_PACKET_FROM_RECEIVER;
#endif
	isys->adev = adev;
	isys->pdata = adev->pdata;

	INIT_LIST_HEAD(&isys->requests);

	spin_lock_init(&isys->lock);
	spin_lock_init(&isys->power_lock);
	isys->power = 0;

	mutex_init(&isys->mutex);
	mutex_init(&isys->stream_mutex);
	mutex_init(&isys->lib_mutex);

	spin_lock_init(&isys->listlock);
	INIT_LIST_HEAD(&isys->framebuflist);
	INIT_LIST_HEAD(&isys->framebuflist_fw);

	dev_info(&adev->dev, "isys probe %p %p\n", adev, &adev->dev);
	ipu_bus_set_drvdata(adev, isys);

	isys->line_align = IPU_ISYS_2600_MEM_LINE_ALIGN;
#ifdef CONFIG_VIDEO_INTEL_IPU4
	isys->icache_prefetch = is_ipu_hw_bxtp_e0(isp);
#else
	isys->icache_prefetch = 0;
#endif

#ifndef CONFIG_PM
	isys_setup_hw(isys);
#endif

	if (!isp->secure_mode) {
		fw = isp->cpd_fw;
		rval = ipu_buttress_map_fw_image(adev, fw, &isys->fw_sgt);
		if (rval)
			goto release_firmware;

		isys->pkg_dir = ipu_cpd_create_pkg_dir(adev, isp->cpd_fw->data,
						       sg_dma_address(isys->
								      fw_sgt.
								      sgl),
						       &isys->pkg_dir_dma_addr,
						       &isys->pkg_dir_size);
		if (!isys->pkg_dir) {
			rval = -ENOMEM;
			goto remove_shared_buffer;
		}
	}

	/* Debug fs failure is not fatal. */
	ipu_isys_init_debugfs(isys);

	ipu_trace_init(adev->isp, isys->pdata->base, &adev->dev,
		       isys_trace_blocks);

	pm_qos_add_request(&isys->pm_qos, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	alloc_fw_msg_buffers(isys, 20);

	pm_runtime_allow(&adev->dev);
	pm_runtime_enable(&adev->dev);

	rval = isys_register_devices(isys);
	if (rval)
		goto out_remove_pkg_dir_shared_buffer;

	trace_printk("E|TMWK\n");
	return 0;

out_remove_pkg_dir_shared_buffer:
	if (!isp->secure_mode)
		ipu_cpd_free_pkg_dir(adev, isys->pkg_dir,
				     isys->pkg_dir_dma_addr,
				     isys->pkg_dir_size);
remove_shared_buffer:
	if (!isp->secure_mode)
		ipu_buttress_unmap_fw_image(adev, &isys->fw_sgt);
release_firmware:
	if (!isp->secure_mode)
		release_firmware(isys->fw);
	ipu_trace_uninit(&adev->dev);

	trace_printk("E|TMWK\n");

	mutex_destroy(&isys->mutex);
	mutex_destroy(&isys->stream_mutex);

	if (isys->short_packet_source == IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		mutex_destroy(&isys->short_packet_tracing_mutex);
		dma_free_attrs(&adev->dev, trace_size,
			       isys->short_packet_trace_buffer,
			       isys->short_packet_trace_buffer_dma_addr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
			       &attrs);
#else
			       attrs);
#endif
	}

	return rval;
}

struct fwmsg {
	int type;
	char *msg;
	bool valid_ts;
};

static const struct fwmsg fw_msg[] = {
	{IPU_FW_ISYS_RESP_TYPE_STREAM_OPEN_DONE, "STREAM_OPEN_DONE", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_CLOSE_ACK, "STREAM_CLOSE_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_START_ACK, "STREAM_START_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK,
	 "STREAM_START_AND_CAPTURE_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_STOP_ACK, "STREAM_STOP_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_FLUSH_ACK, "STREAM_FLUSH_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_PIN_DATA_READY, "PIN_DATA_READY", 1},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK, "STREAM_CAPTURE_ACK", 0},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE,
	 "STREAM_START_AND_CAPTURE_DONE", 1},
	{IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE, "STREAM_CAPTURE_DONE", 1},
	{IPU_FW_ISYS_RESP_TYPE_FRAME_SOF, "FRAME_SOF", 1},
	{IPU_FW_ISYS_RESP_TYPE_FRAME_EOF, "FRAME_EOF", 1},
	{IPU_FW_ISYS_RESP_TYPE_STATS_DATA_READY, "STATS_READY", 1},
	{-1, "UNKNOWN MESSAGE", 0},
};

static int resp_type_to_index(int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fw_msg); i++)
		if (fw_msg[i].type == type)
			return i;

	return i - 1;
}

int isys_isr_one(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	struct ipu_fw_isys_resp_info_abi resp_data;
	struct ipu_fw_isys_resp_info_abi *resp;
	struct ipu_isys_pipeline *pipe;
	u64 ts;
	unsigned int i;

	if (!isys->fwcom)
		return 0;

	resp = ipu_fw_isys_get_resp(isys->fwcom, IPU_BASE_MSG_RECV_QUEUES,
				    &resp_data);
	if (!resp)
		return 1;

	ts = (u64) resp->timestamp[1] << 32 | resp->timestamp[0];

	if (resp->error_info.error == IPU_FW_ISYS_ERROR_STREAM_IN_SUSPENSION)
		/* Suspension is kind of special case: not enough buffers */
		dev_dbg(&adev->dev,
			"hostlib: error resp %02d %s, stream %u, error SUSPENSION, details %d, timestamp 0x%16.16llx, pin %d\n",
			resp->type,
			fw_msg[resp_type_to_index(resp->type)].msg,
			resp->stream_handle,
			resp->error_info.error_details,
			fw_msg[resp_type_to_index(resp->type)].valid_ts ?
			ts : 0, resp->pin_id);
	else if (resp->error_info.error)
		dev_dbg(&adev->dev,
			"hostlib: error resp %02d %s, stream %u, error %d, details %d, timestamp 0x%16.16llx, pin %d\n",
			resp->type,
			fw_msg[resp_type_to_index(resp->type)].msg,
			resp->stream_handle,
			resp->error_info.error, resp->error_info.error_details,
			fw_msg[resp_type_to_index(resp->type)].valid_ts ?
			ts : 0, resp->pin_id);
	else
		dev_dbg(&adev->dev,
			"hostlib: resp %02d %s, stream %u, timestamp 0x%16.16llx, pin %d\n",
			resp->type,
			fw_msg[resp_type_to_index(resp->type)].msg,
			resp->stream_handle,
			fw_msg[resp_type_to_index(resp->type)].valid_ts ?
			ts : 0, resp->pin_id);

	if (resp->stream_handle >= IPU_ISYS_MAX_STREAMS) {
		dev_err(&adev->dev, "bad stream handle %u\n",
			resp->stream_handle);
		goto leave;
	}

	pipe = isys->pipes[resp->stream_handle];
	if (!pipe) {
		dev_err(&adev->dev, "no pipeline for stream %u\n",
			resp->stream_handle);
		goto leave;
	}
	pipe->error = resp->error_info.error;

	switch (resp->type) {
	case IPU_FW_ISYS_RESP_TYPE_STREAM_OPEN_DONE:
		ipu_put_fw_mgs_buffer(ipu_bus_get_drvdata(adev), resp->buf_id);
		complete(&pipe->stream_open_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CLOSE_ACK:
		complete(&pipe->stream_close_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_ACK:
		complete(&pipe->stream_start_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
		ipu_put_fw_mgs_buffer(ipu_bus_get_drvdata(adev), resp->buf_id);
		complete(&pipe->stream_start_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_STOP_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_FLUSH_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_PIN_DATA_READY:
		if (resp->pin_id < IPU_ISYS_OUTPUT_PINS &&
		    pipe->output_pins[resp->pin_id].pin_ready)
			pipe->output_pins[resp->pin_id].pin_ready(pipe, resp);
		else
			dev_err(&adev->dev,
				"%d:No data pin ready handler for pin id %d\n",
				resp->stream_handle, resp->pin_id);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		ipu_put_fw_mgs_buffer(ipu_bus_get_drvdata(adev), resp->buf_id);
		complete(&pipe->capture_ack_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE:
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE:
		if (pipe->interlaced) {
			struct ipu_isys_buffer *ib, *ib_safe;
			struct list_head list;
			unsigned long flags;

			if (pipe->isys->short_packet_source ==
			    IPU_ISYS_SHORT_PACKET_FROM_TUNIT)
				pipe->cur_field =
				    ipu_isys_csi2_get_current_field(pipe,
								    resp->
								    timestamp);
			/*
			 * Move the pending buffers to a local temp list.
			 * Then we do not need to handle the lock during
			 * the loop.
			 */
			spin_lock_irqsave(&pipe->short_packet_queue_lock,
					  flags);
			list_cut_position(&list,
					  &pipe->pending_interlaced_bufs,
					  pipe->pending_interlaced_bufs.prev);
			spin_unlock_irqrestore(&pipe->short_packet_queue_lock,
					       flags);

			list_for_each_entry_safe(ib, ib_safe, &list, head) {
				struct vb2_buffer *vb;

				vb = ipu_isys_buffer_to_vb2_buffer(ib);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				vb->v4l2_buf.field = pipe->cur_field;
#else
				to_vb2_v4l2_buffer(vb)->field = pipe->cur_field;
#endif
				list_del(&ib->head);

				ipu_isys_queue_buf_done(ib);
			}
		}
		for (i = 0; i < IPU_NUM_CAPTURE_DONE; i++)
			if (pipe->capture_done[i])
				pipe->capture_done[i] (pipe, resp);

		break;
	case IPU_FW_ISYS_RESP_TYPE_FRAME_SOF:
#ifdef IPU_TPG_SOF
		if (pipe->tpg)
			ipu_isys_tpg_sof_event(pipe->tpg);
#endif
		pipe->seq[pipe->seq_index].sequence =
		    atomic_read(&pipe->sequence) - 1;
		pipe->seq[pipe->seq_index].timestamp = ts;
		dev_dbg(&adev->dev,
			"sof: handle %d: (index %u), timestamp 0x%16.16llx\n",
			resp->stream_handle,
			pipe->seq[pipe->seq_index].sequence, ts);
		pipe->seq_index = (pipe->seq_index + 1)
		    % IPU_ISYS_MAX_PARALLEL_SOF;
		break;
	case IPU_FW_ISYS_RESP_TYPE_FRAME_EOF:
		dev_dbg(&adev->dev,
			"eof: handle %d: (index %u), timestamp 0x%16.16llx\n",
			resp->stream_handle,
			pipe->seq[pipe->seq_index].sequence, ts);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STATS_DATA_READY:
		break;
	default:
		dev_err(&adev->dev, "%d:unknown response type %u\n",
			resp->stream_handle, resp->type);
		break;
	}

leave:
	ipu_fw_isys_put_resp(isys->fwcom, IPU_BASE_MSG_RECV_QUEUES);
	return 0;
}

static void isys_isr_poll(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);

	if (!isys->fwcom) {
		dev_dbg(&isys->adev->dev,
			"got interrupt but device not configured yet\n");
		return;
	}

	mutex_lock(&isys->mutex);
	isys_isr(adev);
	mutex_unlock(&isys->mutex);
}

int ipu_isys_isr_run(void *ptr)
{
	struct ipu_isys *isys = ptr;

	while (!kthread_should_stop()) {
		usleep_range(500, 1000);
		if (isys->stream_opened)
			isys_isr_poll(isys->adev);
	}

	return 0;
}

static struct ipu_bus_driver isys_driver = {
	.probe = isys_probe,
	.remove = isys_remove,
	.isr = isys_isr,
	.wanted = IPU_ISYS_NAME,
	.drv = {
		.name = IPU_ISYS_NAME,
		.owner = THIS_MODULE,
		.pm = ISYS_PM_OPS,
	},
};

module_ipu_bus_driver(isys_driver);

static const struct pci_device_id ipu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU_PCI_ID)},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ipu_pci_tbl);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_AUTHOR("Jianxu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Renwei Wu <renwei.wu@intel.com>");
MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_AUTHOR("Yunliang Ding <yunliang.ding@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Leifu Zhao <leifu.zhao@intel.com>");
MODULE_AUTHOR("Xia Wu <xia.wu@intel.com>");
MODULE_AUTHOR("Kun Jiang <kun.jiang@intel.com>");
MODULE_AUTHOR("Yu Xia <yu.y.xia@intel.com>");
MODULE_AUTHOR("Jerry Hu <jerry.w.hu@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu input system driver");
