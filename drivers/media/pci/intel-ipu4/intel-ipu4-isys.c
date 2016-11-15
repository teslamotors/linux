/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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

#include <media/intel-ipu4-isys.h>
#include <media/intel-ipu4-acpi.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#include <media/v4l2-mc.h>
#endif
#include <media/v4l2-subdev.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-cpd.h"
#include "intel-ipu4-mmu.h"
#include "intel-ipu4-dma.h"
#include "intel-ipu4-isys.h"
#include "intel-ipu-isys-csi2-common.h"
#include "intel-ipu4-isys-tpg.h"
#include "intel-ipu4-isys-video.h"
#include "intel-ipu4-regs.h"
#include "intel-ipu5-regs.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-buttress-regs.h"
#include "intel-ipu4-wrapper.h"
#include "intel-ipu5-devel.h"

#define ISYS_PM_QOS_VALUE	300

/*
 * The param was passed from module to indicate if port
 * could be optimized.
 */
static bool csi2_port_optimized;
module_param(csi2_port_optimized, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(csi2_port_optimized, "IPU4 CSI2 port optimization");

static const struct intel_ipu4_isys_fw_ctrl *api_ops;

/* Trace block definitions for isys */
static struct intel_ipu4_trace_block isys_trace_blocks_ipu4[] = {
	{
		.offset = TRACE_REG_IS_TRACE_UNIT_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TUN,
	},
	{
		.offset = TRACE_REG_IS_SP_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_IS_SP_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_IS_ISL_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_IS_MMU_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_CSI2_TM_BASE,
		.type = INTEL_IPU4_TRACE_CSI2,
	},
	{
		.offset = TRACE_REG_CSI2_3PH_TM_BASE,
		.type = INTEL_IPU4_TRACE_CSI2_3PH,
	},
	{
		/* Note! this covers all 9 blocks */
		.offset = TRACE_REG_CSI2_SIG2SIO_GRn_BASE(0),
		.type = INTEL_IPU4_TRACE_SIG2CIOS,
	},
	{
		/* Note! this covers all 9 blocks */
		.offset = TRACE_REG_CSI2_PH3_SIG2SIO_GRn_BASE(0),
		.type = INTEL_IPU4_TRACE_SIG2CIOS,
	},
	{
		.offset = TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N,
		.type = INTEL_IPU4_TRACE_TIMER_RST,
	},
	{
		.type = INTEL_IPU4_TRACE_BLOCK_END,
	}
};

static struct intel_ipu4_trace_block isys_trace_blocks_ipu5A0[] = {
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_TRACE_UNIT_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TUN,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_SP_EVQ_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_TM,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_SP_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_ISL_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_MMU_GPC_BASE,
		.type = INTEL_IPU4_TRACE_BLOCK_GPC,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_CSI2_TM_BASE,
		.type = INTEL_IPU4_TRACE_CSI2,
	},
	{
		/* Note! this covers all 11 blocks */
		.offset = INTEL_IPU5_TRACE_REG_CSI2_SIG2SIO_GRn_BASE(0),
		.type = INTEL_IPU4_TRACE_SIG2CIOS,
	},
	{
		.offset = INTEL_IPU5_TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N,
		.type = INTEL_IPU4_TRACE_TIMER_RST,
	},
	{
		.type = INTEL_IPU4_TRACE_BLOCK_END,
	}
};

void intel_ipu4_isys_register_ext_library(
	const struct intel_ipu4_isys_fw_ctrl *ops)
{
	api_ops = ops;
}
EXPORT_SYMBOL_GPL(intel_ipu4_isys_register_ext_library);

void intel_ipu4_isys_unregister_ext_library(void)
{
	api_ops = NULL;
}
EXPORT_SYMBOL_GPL(intel_ipu4_isys_unregister_ext_library);

const struct intel_ipu4_isys_fw_ctrl *intel_ipu4_isys_get_api_ops(void)
{
	return api_ops;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/*
 * BEGIN adapted code from drivers/media/platform/omap3isp/isp.c.
 * FIXME: This (in terms of functionality if not code) should be most
 * likely generalised in the framework, and use made optional for
 * drivers.
 */
/*
 * intel_ipu4_pipeline_pm_use_count - Count the number of users of a pipeline
 * @entity: The entity
 *
 * Return the total number of users of all video device nodes in the pipeline.
 */
static int intel_ipu4_pipeline_pm_use_count(struct media_pad *pad)
{
	struct media_entity_graph graph;
	struct media_entity *entity = pad->entity;
	int use = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_entity_graph_walk_start(&graph, pad);

	while ((entity = media_entity_graph_walk_next(&graph))) {
		if (is_media_entity_v4l2_io(entity))
			use += entity->use_count;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_cleanup(&graph);
#endif
	return use;
}

/*
 * intel_ipu4_pipeline_pm_power_one - Apply power change to an entity
 * @entity: The entity
 * @change: Use count change
 *
 * Change the entity use count by @change. If the entity is a subdev update its
 * power state by calling the core::s_power operation when the use count goes
 * from 0 to != 0 or from != 0 to 0.
 *
 * Return 0 on success or a negative error code on failure.
 */
static int intel_ipu4_pipeline_pm_power_one(struct media_entity *entity,
					int change)
{
	struct v4l2_subdev *subdev;
	int ret;

	subdev = is_media_entity_v4l2_subdev(entity)
	       ? media_entity_to_v4l2_subdev(entity) : NULL;

	if (entity->use_count == 0 && change > 0 && subdev != NULL) {
		ret = v4l2_subdev_call(subdev, core, s_power, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;
	}

	entity->use_count += change;
	WARN_ON(entity->use_count < 0);

	if (entity->use_count == 0 && change < 0 && subdev != NULL)
		v4l2_subdev_call(subdev, core, s_power, 0);

	return 0;
}

/*
 * intel_ipu4_pipeline_pm_power - Apply power change to all entities
 * in a pipeline
 * @entity: The entity
 * @change: Use count change
 *
 * Walk the pipeline to update the use count and the power state of
 * all non-node
 * entities.
 *
 * Return 0 on success or a negative error code on failure.
 */
static int intel_ipu4_pipeline_pm_power(struct media_entity *entity,
					int change)
{
	struct media_entity_graph graph;
	struct media_entity *first = entity;
	int ret = 0;

	if (!change)
		return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_entity_graph_walk_start(&graph, &entity->pads[0]);

	while (!ret && (entity = media_entity_graph_walk_next(&graph)))
		if (!is_media_entity_v4l2_io(entity))
			ret = intel_ipu4_pipeline_pm_power_one(entity, change);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_cleanup(&graph);
#endif
	if (!ret)
		return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_init(&graph, entity->graph_obj.mdev);
#endif
	media_entity_graph_walk_start(&graph, &first->pads[0]);

	while ((first = media_entity_graph_walk_next(&graph))
	       && first != entity)
		if (!is_media_entity_v4l2_io(first))
			intel_ipu4_pipeline_pm_power_one(first, -change);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_entity_graph_walk_cleanup(&graph);
#endif
	return ret;
}

/*
 * intel_ipu4_pipeline_pm_use - Update the use count of an entity
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
int intel_ipu4_pipeline_pm_use(struct media_entity *entity, int use)
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
	ret = intel_ipu4_pipeline_pm_power(entity, change);
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
 * intel_ipu4_pipeline_link_notify - Link management notification callback
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
static int intel_ipu4_pipeline_link_notify(struct media_link *link, u32 flags,
					unsigned int notification)
{
	struct media_entity *source = link->source->entity;
	struct media_entity *sink = link->sink->entity;
	int source_use = intel_ipu4_pipeline_pm_use_count(link->source);
	int sink_use = intel_ipu4_pipeline_pm_use_count(link->sink);
	int ret;

	if (notification == MEDIA_DEV_NOTIFY_POST_LINK_CH &&
	    !(flags & MEDIA_LNK_FL_ENABLED)) {
		/* Powering off entities is assumed to never fail. */
		intel_ipu4_pipeline_pm_power(source, -sink_use);
		intel_ipu4_pipeline_pm_power(sink, -source_use);
		return 0;
	}

	if (notification == MEDIA_DEV_NOTIFY_PRE_LINK_CH &&
		(flags & MEDIA_LNK_FL_ENABLED)) {

		ret = intel_ipu4_pipeline_pm_power(source, sink_use);
		if (ret < 0)
			return ret;

		ret = intel_ipu4_pipeline_pm_power(sink, source_use);
		if (ret < 0)
			intel_ipu4_pipeline_pm_power(source, -sink_use);

		return ret;
	}

	return 0;
}
/* END adapted code from drivers/media/platform/omap3isp/isp.c */
#endif /* < v4.6 */

static int isys_determine_legacy_csi_lane_configuration(
					struct intel_ipu4_isys *isys)
{
	const struct csi_lane_cfg {
		u32 reg_value;
		int port_lanes[INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS];
	} csi_lanes_to_cfg[] = {
		{ 0x0, { 4, 2, 0, 0 } }, /* no sensor defaults here */
		{ 0x1, { 3, 2, 0, 0 } },
		{ 0x2, { 2, 2, 0, 0 } },
		{ 0x3, { 1, 2, 0, 0 } },
		{ 0x4, { 4, 1, 0, 0 } },
		{ 0x5, { 3, 1, 0, 0 } },
		{ 0x6, { 2, 1, 0, 0 } },
		{ 0x7, { 1, 1, 0, 0 } },
		{ 0x8, { 4, 1, 0, 1 } },
		{ 0x9, { 3, 1, 0, 1 } },
		{ 0xa, { 2, 1, 0, 1 } },
		{ 0xb, { 1, 1, 0, 1 } },
		{ 0x10, { 2, 2, 2, 0 } },
		{ 0x11, { 2, 2, 1, 0 } },
		{ 0x18, { 2, 1, 2, 1 } },
		{ 0x19, { 1, 1, 1, 1 } },
	};
	int i, j;

	for (i = 0; i < ARRAY_SIZE(csi_lanes_to_cfg); i++) {
		for (j = 0; j < INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS; j++) {
			/* Port with no sensor can be handled as don't care */
			if (!isys->csi2[j].nlanes)
				continue;
			if (csi_lanes_to_cfg[i].port_lanes[j] !=
			    isys->csi2[j].nlanes)
				break;
		}

		if (j < INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS)
			continue;

		isys->legacy_port_cfg = csi_lanes_to_cfg[i].reg_value;
		dev_dbg(&isys->adev->dev, "Lane configuration value 0x%x\n,",
			 isys->legacy_port_cfg);
		return 0;
	}
	dev_err(&isys->adev->dev, "Non supported CSI lane configuration\n,");
	return -EINVAL;
}


static int isys_determine_csi_combo_lane_configuration(
					struct intel_ipu4_isys *isys)
{
	const struct csi_lane_cfg {
		u32 reg_value;
		int port_lanes[INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS];
	} csi_lanes_to_cfg[] = {
		{ 0x1f, { 0, 0 } }, /* no sensor defaults here - disable all */
		{ 0x10, { 4, 0 } },
		{ 0x11, { 3, 0 } },
		{ 0x12, { 2, 0 } },
		{ 0x13, { 1, 0 } },
		{ 0x14, { 3, 1 } },
		{ 0x15, { 2, 1 } },
		{ 0x16, { 1, 1 } },
		{ 0x18, { 2, 2 } },
		{ 0x19, { 1, 2 } },
	};
	int i, j;

	for (i = 0; i < ARRAY_SIZE(csi_lanes_to_cfg); i++) {
		for (j = 0; j <  INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS; j++) {
			/* Port with no sensor can be handled as don't care */
			if (!isys->csi2[j +
				INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS].nlanes)
				continue;
			if (csi_lanes_to_cfg[i].port_lanes[j] !=
				isys->csi2[j +
				INTEL_IPU4_ISYS_MAX_CSI2_LEGACY_PORTS].nlanes)
				break;
		}

		if (j < INTEL_IPU4_ISYS_MAX_CSI2_COMBO_PORTS)
			continue;

		isys->combo_port_cfg = csi_lanes_to_cfg[i].reg_value;
		dev_dbg(&isys->adev->dev,
			"Combo port lane configuration value 0x%x\n",
			isys->combo_port_cfg);

		return 0;
	}
	dev_err(&isys->adev->dev,
		"Unsupported CSI2-combo lane configuration\n");
	return 0;
}

static int isys_determine_csi_combo_lane_configuration_ipu5(
					struct intel_ipu4_isys *isys)
{
	const struct csi_lane_cfg {
		u8 reg_value;
		u8 port_lanes[INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS];
	} csi_lanes_to_cfg[] = {
		{ 0x1f, { 0, 0, 0, 0 } }, /* no sensor defaults here*/
		{ 0x00, { 0, 0, 4, 0 } }, /* Dphy0, Dphy1, Cphy0, Cphy1*/
		{ 0x01, { 0, 0, 3, 0 } },
		{ 0x02, { 0, 0, 2, 0 } },
		{ 0x03, { 0, 0, 1, 0 } },
		{ 0x04, { 0, 0, 3, 1 } },
		{ 0x05, { 0, 0, 2, 1 } },
		{ 0x06, { 0, 0, 1, 1 } },
		{ 0x08, { 0, 0, 2, 2 } },
		{ 0x09, { 0, 0, 1, 2 } },
		{ 0x10, { 4, 0, 0, 0 } },
		{ 0x11, { 3, 0, 0, 0 } },
		{ 0x12, { 2, 0, 0, 0 } },
		{ 0x13, { 1, 0, 0, 0 } },
		{ 0x14, { 3, 1, 0, 0 } },
		{ 0x15, { 2, 1, 0, 0 } },
		{ 0x16, { 1, 1, 0, 0 } },
		{ 0x18, { 2, 2, 0, 0 } },
		{ 0x19, { 1, 2, 0, 0 } },
	};
	u8 i, j, top_num;

	for (top_num = 0; top_num < INTEL_IPU5_ISYS_COMBO_PHY_NUM; top_num++) {
		for (i = 0; i < ARRAY_SIZE(csi_lanes_to_cfg); i++) {
			for (j = 0; j <
				INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS; j++) {
				if (!isys->csi2[j + top_num *
				INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS].nlanes)
					continue;
				/*
				* if current lanes number of port can not
				* match within csi_lanes_to_cfg[i], switch
				* to csi_lanes_to_cfg[i+1]
				*/
				if (csi_lanes_to_cfg[i].port_lanes[j] !=
					isys->csi2[j + top_num *
				INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS].nlanes)
					break;
			}

			if (j < INTEL_IPU5_ISYS_MAX_CSI2_COMBO_PORTS)
				continue;

			isys->combo_port_cfg |=
				csi_lanes_to_cfg[i].reg_value <<
				(top_num *
				BUTTRESS_CSI2_PORT_CONFIG_SHIFT_IPU5A0);
			dev_dbg(&isys->adev->dev,
				"Combo port lane configuration value 0x%x\n",
				isys->combo_port_cfg);
			break;
		}
		if (i == ARRAY_SIZE(csi_lanes_to_cfg))
			dev_err(&isys->adev->dev,
				"Unsupported CSI2-combo lane configuration on top %d\n",
						top_num);
	}
	return 0;
}

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

	if (i2c_adapter_id(client->adapter) != test->bus_nr
	    || client->addr != test->addr)
		return 0;

	test->client = client;

	return 0;
}

static struct i2c_client *isys_find_i2c_subdev(struct i2c_adapter *adapter,
				struct intel_ipu4_isys_subdev_info *sd_info)
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

static struct v4l2_subdev *register_acpi_i2c_subdev(
	struct v4l2_device *v4l2_dev,
	struct intel_ipu4_isys_subdev_info *sd_info,
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

static int isys_complete_ext_device_registration(
	struct intel_ipu4_isys *isys,
	struct v4l2_subdev *sd,
	struct intel_ipu4_isys_csi2_config *csi2)
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

	rval = media_create_pad_link(
		&sd->entity, i,
		&isys->csi2[csi2->port].asd.sd.entity, 0, 0);
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

static int isys_register_ext_subdev(struct intel_ipu4_isys *isys,
				struct intel_ipu4_isys_subdev_info *sd_info,
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
			dev_dbg(&isys->adev->dev,
				 "Matching ACPI device not found - postpone\n");
			rval = 0;
			goto skip_put_adapter;
		}
		if (!sd_info->acpiname) {
			dev_dbg(&isys->adev->dev,
				 "No name in platform data\n");
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
				struct intel_ipu4_isys_csi2_config *csi2,
				bool reprobe)
{
	struct intel_ipu4_isys *isys = priv;
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

static void isys_register_ext_subdevs(struct intel_ipu4_isys *isys)
{
	struct intel_ipu4_isys_subdev_pdata *spdata = isys->pdata->spdata;
	struct intel_ipu4_isys_subdev_info **sd_info;

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
	request_module("intel-ipu4-acpi");
	intel_ipu4_get_acpi_devices(isys, &isys->adev->dev,
				    isys_acpi_add_device);
}

static void isys_unregister_subdevices(struct intel_ipu4_isys *isys)
{
	const struct intel_ipu4_isys_internal_tpg_pdata *tpg =
		&isys->pdata->ipdata->tpg;
	const struct intel_ipu4_isys_internal_csi2_pdata *csi2 =
		&isys->pdata->ipdata->csi2;
	unsigned int i;

	intel_ipu4_isys_csi2_be_cleanup(&isys->csi2_be);
	intel_ipu4_isys_csi2_be_soc_cleanup(&isys->csi2_be_soc);

	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp))
		intel_ipu4_isys_isa_cleanup(&isys->isa);

	for (i = 0; i < tpg->ntpgs; i++)
		intel_ipu4_isys_tpg_cleanup(&isys->tpg[i]);

	for (i = 0; i < csi2->nports; i++)
		intel_ipu_isys_csi2_cleanup(&isys->csi2[i]);
}

static int isys_register_subdevices(struct intel_ipu4_isys *isys)
{
	const struct intel_ipu4_isys_internal_tpg_pdata *tpg =
		&isys->pdata->ipdata->tpg;
	const struct intel_ipu4_isys_internal_csi2_pdata *csi2 =
		&isys->pdata->ipdata->csi2;
	struct intel_ipu4_isys_subdev_pdata *spdata = isys->pdata->spdata;
	struct intel_ipu4_isys_subdev_info **sd_info;
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

		rval = intel_ipu_isys_csi2_init(
			&isys->csi2[i], isys,
			isys->pdata->base + csi2->offsets[i], i);
		if (rval)
			goto fail;

		if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp))
			isys->isr_csi2_bits |=
				INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(i);
		else
			isys->isr_csi2_bits |=
				INTEL_IPU5_ISYS_UNISPART_IRQ_CSI2_A0(i);
	}

	isys->tpg = devm_kcalloc(&isys->adev->dev, tpg->ntpgs,
		sizeof(*isys->tpg), GFP_KERNEL);
	if (!isys->tpg) {
		rval = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = intel_ipu4_isys_tpg_init(
			&isys->tpg[i], isys,
			isys->pdata->base + tpg->offsets[i],
			isys->pdata->base + tpg->sels[i], i);
		if (rval)
			goto fail;
	}

	rval = intel_ipu4_isys_csi2_be_soc_init(&isys->csi2_be_soc, isys);
	if (rval) {
		dev_info(&isys->adev->dev,
			 "can't register soc csi2 be device\n");
		goto fail;
	}

	rval = intel_ipu4_isys_csi2_be_init(&isys->csi2_be, isys);
	if (rval) {
		dev_info(&isys->adev->dev,
			 "can't register raw csi2 be device\n");
		goto fail;
	}

	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp)) {
		rval = intel_ipu4_isys_isa_init(&isys->isa, isys, NULL);
		if (rval) {
			dev_info(&isys->adev->dev,
				 "can't register isa device\n");
			goto fail;
		}
	}

	for (i = 0; i < csi2->nports; i++) {
		if (!test_bit(i, csi2_enable))
			continue;

		for (j = CSI2_PAD_SOURCE(0);
		     j < (NR_OF_CSI2_SOURCE_PADS + CSI2_PAD_SOURCE(0)); j++) {
			rval = media_entity_create_link(
				&isys->csi2[i].asd.sd.entity, j,
				&isys->csi2_be.asd.sd.entity,
				CSI2_BE_PAD_SINK, 0);
			if (rval) {
				dev_info(&isys->adev->dev,
					 "can't create link csi2 <=> csi2_be\n");
				goto fail;
			}

			for (k = CSI2_BE_SOC_PAD_SINK(0);
				k < NR_OF_CSI2_BE_SOC_SINK_PADS; k++) {
				rval = media_entity_create_link(
					&isys->csi2[i].asd.sd.entity, j,
				&isys->csi2_be_soc.asd.sd.entity,
					k, MEDIA_LNK_FL_DYNAMIC);
				if (rval) {
					dev_info(&isys->adev->dev,
					"can't create link csi2->be_soc\n");
					goto fail;
				}
			}
		}
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = media_create_pad_link(
			&isys->tpg[i].asd.sd.entity, TPG_PAD_SOURCE,
			&isys->csi2_be.asd.sd.entity,
			CSI2_BE_PAD_SINK, 0);
		if (rval) {
			dev_info(&isys->adev->dev,
				 "can't create link between tpg and csi2_be\n");
			goto fail;
		}

		for (k = CSI2_BE_SOC_PAD_SINK(0);
		     k < NR_OF_CSI2_BE_SOC_SINK_PADS; k++) {
			rval = media_entity_create_link(
				&isys->tpg[i].asd.sd.entity, TPG_PAD_SOURCE,
				&isys->csi2_be_soc.asd.sd.entity,
							k, 0);
			if (rval) {
				dev_info(&isys->adev->dev,
					 "can't create link tpg->be_soc\n");
				goto fail;
			}
		}
	}

	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp)) {
		rval = media_create_pad_link(
			&isys->csi2_be.asd.sd.entity,
			CSI2_BE_PAD_SOURCE,
			&isys->isa.asd.sd.entity, ISA_PAD_SINK,
			0);
		if (rval) {
			dev_info(&isys->adev->dev,
				 "can't create link between CSI2 raw be and ISA\n");
			goto fail;
		}
	}
	return 0;

fail:
	isys_unregister_subdevices(isys);
	return rval;
}

#ifdef MEDIA_IOC_REQUEST_CMD
struct media_device_ops isys_mdev_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	.link_notify = intel_ipu4_pipeline_link_notify,
#else
	.link_notify = v4l2_pipeline_link_notify,
#endif
	.req_alloc = intel_ipu4_isys_req_alloc,
	.req_free = intel_ipu4_isys_req_free,
	.req_queue = intel_ipu4_isys_req_queue,
};
#endif /* MEDIA_IOC_REQUEST_CMD */

static int isys_register_devices(struct intel_ipu4_isys *isys)
{
	int rval;

	isys->media_dev.dev = &isys->adev->dev;
#ifdef MEDIA_IOC_REQUEST_CMD
	isys->media_dev.ops = &isys_mdev_ops;
#else /* ! MEDIA_IOC_REQUEST_CMD */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
	isys->media_dev.link_notify = intel_ipu4_pipeline_link_notify,
#else
	isys->media_dev.link_notify = v4l2_pipeline_link_notify,
#endif
#endif /* ! MEDIA_IOC_REQUEST_CMD */
	strlcpy(isys->media_dev.model,
		intel_ipu4_media_ctl_dev_model(isys->adev->isp),
		sizeof(isys->media_dev.model));
	isys->media_dev.driver_version = LINUX_VERSION_CODE;
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

	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp)) {
		rval = isys_determine_legacy_csi_lane_configuration(isys);
		if (rval)
			goto out_isys_unregister_subdevices;

		rval = isys_determine_csi_combo_lane_configuration(isys);
		if (rval)
			goto out_isys_unregister_subdevices;
	} else {
		rval = isys_determine_csi_combo_lane_configuration_ipu5(isys);
		if (rval)
			goto out_isys_unregister_subdevices;
	}
#ifndef CONFIG_PM
	intel_ipu4_buttress_csi_port_config(isys->adev->isp,
					    isys->legacy_port_cfg,
					    isys->combo_port_cfg);
#endif

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

static void isys_unregister_devices(struct intel_ipu4_isys *isys)
{
	isys_unregister_subdevices(isys);
	v4l2_device_unregister(&isys->v4l2_dev);
	media_device_unregister(&isys->media_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	media_device_cleanup(&isys->media_dev);
#endif
}

static void isys_setup_hw_ipu4(struct intel_ipu4_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	u32 irqs;
	unsigned int i;

	/* Enable irqs for all MIPI busses */
	irqs = INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(0) |
	       INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(1) |
	       INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(2) |
	       INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(3) |
	       INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(4) |
	       INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(5);

	irqs |= INTEL_IPU4_ISYS_UNISPART_IRQ_SW;

	writel(irqs, base + INTEL_IPU4_REG_ISYS_UNISPART_IRQ_EDGE);
	writel(irqs, base + INTEL_IPU4_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + INTEL_IPU4_REG_ISYS_UNISPART_IRQ_CLEAR);
	writel(irqs, base + INTEL_IPU4_REG_ISYS_UNISPART_IRQ_MASK);
	writel(irqs, base + INTEL_IPU4_REG_ISYS_UNISPART_IRQ_ENABLE);

	writel(0, base + INTEL_IPU4_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + INTEL_IPU4_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);

	/* Write CDC FIFO threshold values for isys */
	for (i = 0; i < isys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(isys->pdata->ipdata->hw_variant.cdc_fifo_threshold[i],
		       base + INTEL_IPU4_REG_ISYS_CDC_THRESHOLD(i));
}

static void isys_setup_hw_ipu5(struct intel_ipu4_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	u32 irqs = 0;
	unsigned int i, j, k;

	/*
	* TODO: set sw_irq bit to enable isr
	*/
	/* Enable irqs for all MIPI busses */
	for (i = 0; i < INTEL_IPU5_ISYS_COMBO_PHY_NUM; i++)
		for (j = 0; j < INTEL_IPU5_CSI_PIPE_NUM_PER_TOP; j++)
			for (k = 0; k < INTEL_IPU5_CSI_IRQ_NUM_PER_PIPE; k++)
				irqs |= INTEL_IPU5_ISYS_CSI_TOP_IRQ_A0(k +
				(i * INTEL_IPU5_CSI_PIPE_NUM_PER_TOP + j) *
				INTEL_IPU5_CSI_IRQ_NUM_PER_PIPE);

	writel(irqs, base + INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_EDGE);
	writel(irqs, base + INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_CLEAR);
	writel(irqs, base + INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_MASK);
	writel(irqs, base + INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_ENABLE);

	writel(0, base + INTEL_IPU5_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + INTEL_IPU5_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);

	/* Write CDC FIFO threshold values for isys */
	for (i = 0; i < isys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(isys->pdata->ipdata->hw_variant.cdc_fifo_threshold[i],
		base + INTEL_IPU5_REG_ISYS_CDC_THRESHOLD(i));
}

static void isys_setup_hw(struct intel_ipu4_isys *isys)
{
	if (is_intel_ipu4_hw_bxt_b0(isys->adev->isp))
		isys_setup_hw_ipu4(isys);
	else
		isys_setup_hw_ipu5(isys);
}

#ifdef CONFIG_PM
static int isys_runtime_pm_resume(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_device *isp = adev->isp;
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	unsigned long flags;
	int ret;

	if (!isys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	intel_ipu4_trace_restore(dev);

	pm_qos_update_request(&isys->pm_qos, ISYS_PM_QOS_VALUE);

	intel_ipu4_buttress_csi_port_config(isp,
					    isys->legacy_port_cfg,
					    isys->combo_port_cfg);

	ret = intel_ipu4_buttress_start_tsc_sync(isp);
	if (ret)
		return ret;

	spin_lock_irqsave(&isys->power_lock, flags);
	isys->power = 1;
	spin_unlock_irqrestore(&isys->power_lock, flags);

	if (isys->short_packet_source ==
		INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		mutex_lock(&isys->short_packet_tracing_mutex);
		isys->short_packet_tracing_count = 0;
		mutex_unlock(&isys->short_packet_tracing_mutex);
	}
	isys_setup_hw(isys);

	return 0;
}

static int isys_runtime_pm_suspend(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	unsigned long flags;

	if (!isys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&isys->power_lock, flags);
	isys->power = 0;
	spin_unlock_irqrestore(&isys->power_lock, flags);

	intel_ipu4_trace_stop(dev);
	mutex_lock(&isys->mutex);
	isys->reset_needed = false;
	mutex_unlock(&isys->mutex);

	pm_qos_update_request(&isys->pm_qos, PM_QOS_DEFAULT_VALUE);

	return 0;
}

static int isys_suspend(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);

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

static void isys_remove(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	struct intel_ipu4_device *isp = adev->isp;
	struct isys_fw_msgs *fwmsg, *safe;

	dev_info(&adev->dev, "removed\n");
	debugfs_remove_recursive(isys->debugfsdir);

	list_for_each_entry_safe(fwmsg, safe, &isys->framebuflist, head) {
		dma_free_attrs(&adev->dev, sizeof(struct isys_fw_msgs),
			       fwmsg, fwmsg->dma_addr, NULL);
	}

	list_for_each_entry_safe(fwmsg, safe, &isys->framebuflist_fw, head) {
		dma_free_attrs(&adev->dev, sizeof(struct isys_fw_msgs),
			       fwmsg, fwmsg->dma_addr, NULL);
	}

	intel_ipu4_trace_uninit(&adev->dev);
	isys_unregister_devices(isys);
	pm_qos_remove_request(&isys->pm_qos);

	if (!isp->secure_mode) {
		intel_ipu4_cpd_free_pkg_dir(adev, isys->pkg_dir,
					    isys->pkg_dir_dma_addr,
					    isys->pkg_dir_size);
		intel_ipu4_buttress_unmap_fw_image(adev, &isys->fw_sgt);
		release_firmware(isys->fw);
	}

	mutex_destroy(&isys->stream_mutex);
	mutex_destroy(&isys->mutex);

	if (isys->short_packet_source ==
	    INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		struct dma_attrs attrs;

		init_dma_attrs(&attrs);
		dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
		dma_free_attrs(&adev->dev,
			INTEL_IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
			isys->short_packet_trace_buffer,
			isys->short_packet_trace_buffer_dma_addr, &attrs);
	}
}

static int intel_ipu4_isys_icache_prefetch_get(void *data, u64 *val)
{
	struct intel_ipu4_isys *isys = data;

	*val = isys->icache_prefetch;
	return 0;
}

static int intel_ipu4_isys_icache_prefetch_set(void *data, u64 val)
{
	struct intel_ipu4_isys *isys = data;

	if (val != !!val)
		return -EINVAL;

	isys->icache_prefetch = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(isys_icache_prefetch_fops,
			intel_ipu4_isys_icache_prefetch_get,
			intel_ipu4_isys_icache_prefetch_set,
			"%llu\n");

static int intel_ipu4_isys_init_debugfs(struct intel_ipu4_isys *isys)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir("isys", isys->adev->isp->intel_ipu4_dir);
	if (IS_ERR(dir))
		return -ENOMEM;

	file = debugfs_create_file("icache_prefetch", S_IRUSR | S_IWUSR,
				   dir, isys,
				   &isys_icache_prefetch_fops);
	if (IS_ERR(file))
		goto err;

	isys->debugfsdir = dir;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

static int alloc_fw_msg_buffers(struct intel_ipu4_isys *isys, int amount)
{
	dma_addr_t dma_addr;
	struct isys_fw_msgs *addr;
	unsigned int i;
	unsigned long flags;

	for (i = 0; i < amount; i++) {
		addr = dma_alloc_attrs(&isys->adev->dev,
				       sizeof(struct isys_fw_msgs),
				       &dma_addr, GFP_KERNEL, NULL);
		addr->dma_addr = dma_addr;

		spin_lock_irqsave(&isys->listlock, flags);
		list_add(&addr->head, &isys->framebuflist);
		spin_unlock_irqrestore(&isys->listlock, flags);
	}
	return 0;
}

struct isys_fw_msgs *intel_ipu4_get_fw_msg_buf(
	struct intel_ipu4_isys_pipeline *ip)
{
	struct intel_ipu4_isys_video *pipe_av =
		container_of(ip, struct intel_ipu4_isys_video, ip);
	struct intel_ipu4_isys *isys;
	struct isys_fw_msgs  *msg;
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
	memset(&msg->fw_msg.dummy, 0, sizeof(msg->fw_msg));

	return msg;
}

void intel_ipu4_cleanup_fw_msg_bufs(struct intel_ipu4_isys *isys)
{
	struct isys_fw_msgs  *fwmsg, *fwmsg0;
	unsigned long flags;

	spin_lock_irqsave(&isys->listlock, flags);
	list_for_each_entry_safe(fwmsg, fwmsg0, &isys->framebuflist_fw, head)
		list_move(&fwmsg->head, &isys->framebuflist);
	spin_unlock_irqrestore(&isys->listlock, flags);
}

void intel_ipu4_put_fw_mgs_buffer(struct intel_ipu4_isys *isys,
		       u64 data)
{
	struct isys_fw_msgs *msg;
	u64 *ptr = (u64 *)data;

	if (!ptr)
		return;

	spin_lock(&isys->listlock);
	msg = container_of(ptr, struct isys_fw_msgs, fw_msg.dummy);
	list_move(&msg->head, &isys->framebuflist);
	spin_unlock(&isys->listlock);
}
EXPORT_SYMBOL_GPL(intel_ipu4_put_fw_mgs_buffer);

static int isys_probe(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct intel_ipu4_isys *isys;
	struct intel_ipu4_device *isp = adev->isp;

	const struct firmware *uninitialized_var(fw);
	int rval = 0;
	struct dma_attrs attrs;

	trace_printk("B|%d|TMWK\n", current->pid);

	if (!is_intel_ipu5_hw_a0(isp)) {
		/* Has the domain been attached? */
		if (!mmu || !isp->pkg_dir_dma_addr) {
			trace_printk("E|TMWK\n");
			return -EPROBE_DEFER;
		}
	}

	isys = devm_kzalloc(&adev->dev, sizeof(*isys), GFP_KERNEL);
	if (!isys) {
		trace_printk("E|TMWK\n");
		return -ENOMEM;
	}

	/* For BXT B0/C0 and BXT-P, short packet is captured from T-Unit. */
	if (is_intel_ipu4_hw_bxt_b0(isp)) {
		isys->short_packet_source =
			INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT;
		mutex_init(&isys->short_packet_tracing_mutex);
		init_dma_attrs(&attrs);
		dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
		isys->short_packet_trace_buffer = dma_alloc_attrs(&adev->dev,
			INTEL_IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
			&isys->short_packet_trace_buffer_dma_addr, GFP_KERNEL, &attrs);
		if (!isys->short_packet_trace_buffer)
			return -ENOMEM;
	} else {
		isys->short_packet_source =
			INTEL_IPU_ISYS_SHORT_PACKET_FROM_RECEIVER;
	}

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
	intel_ipu4_bus_set_drvdata(adev, isys);
	intel_ipu4_abi_init(isys);

	isys->line_align = INTEL_IPU4_ISYS_2600_MEM_LINE_ALIGN;
	isys->icache_prefetch = is_intel_ipu4_hw_bxt_c0(isp);

#ifndef CONFIG_PM
	isys_setup_hw(isys);
#endif

	if (!isp->secure_mode) {
		if (is_intel_ipu5_hw_a0(isp)) {
			int ret;

			ret = intel_ipu5_isys_load_pkg_dir(isys);
			if (ret < 0)
				goto release_firmware;
		} else {
			fw = isp->cpd_fw;

			rval = intel_ipu4_buttress_map_fw_image(
				adev, fw, &isys->fw_sgt);
			if (rval)
				goto release_firmware;

			isys->pkg_dir = intel_ipu4_cpd_create_pkg_dir(
				adev, isp->cpd_fw->data,
				sg_dma_address(isys->fw_sgt.sgl),
				&isys->pkg_dir_dma_addr,
				&isys->pkg_dir_size);
			if (isys->pkg_dir == NULL) {
				rval = -ENOMEM;
				goto  remove_shared_buffer;
			}
		}
	}

	/* Debug fs failure is not fatal. */
	intel_ipu4_isys_init_debugfs(isys);

	if (is_intel_ipu5_hw_a0(isp))
		intel_ipu4_trace_init(adev->isp, isys->pdata->base, &adev->dev,
				isys_trace_blocks_ipu5A0);
	else
		intel_ipu4_trace_init(adev->isp, isys->pdata->base, &adev->dev,
				isys_trace_blocks_ipu4);


	pm_qos_add_request(&isys->pm_qos, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	alloc_fw_msg_buffers(isys, 20);

	rval = isys_register_devices(isys);
	if (rval)
		goto out_remove_pkg_dir_shared_buffer;

	trace_printk("E|TMWK\n");
	return 0;

out_remove_pkg_dir_shared_buffer:
	if (!isp->secure_mode)
		intel_ipu4_cpd_free_pkg_dir(adev, isys->pkg_dir,
					    isys->pkg_dir_dma_addr,
					    isys->pkg_dir_size);
remove_shared_buffer:
	if (!isp->secure_mode)
		intel_ipu4_buttress_unmap_fw_image(
			adev, &isys->fw_sgt);
release_firmware:
	if (!isp->secure_mode)
		release_firmware(isys->fw);
	intel_ipu4_trace_uninit(&adev->dev);

	trace_printk("E|TMWK\n");

	mutex_destroy(&isys->mutex);
	mutex_destroy(&isys->stream_mutex);

	if (isys->short_packet_source ==
	    INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
		mutex_destroy(&isys->short_packet_tracing_mutex);
		dma_free_attrs(&adev->dev,
			INTEL_IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
			isys->short_packet_trace_buffer,
			isys->short_packet_trace_buffer_dma_addr, &attrs);
	}

	return rval;
}

struct fwmsg {
	int type;
	char *msg;
	bool valid_ts;
};

static const struct fwmsg fw_msg[] = {
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_OPEN_DONE,    "STREAM_OPEN_DONE", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_CLOSE_ACK,    "STREAM_CLOSE_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_START_ACK,    "STREAM_START_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK,
	  "STREAM_START_AND_CAPTURE_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_STOP_ACK,     "STREAM_STOP_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_FLUSH_ACK,    "STREAM_FLUSH_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_PIN_DATA_READY,      "PIN_DATA_READY", 1 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK,  "STREAM_CAPTURE_ACK", 0 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE,
	  "STREAM_START_AND_CAPTURE_DONE", 1 },
	{ IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE, "STREAM_CAPTURE_DONE", 1 },
	{ IPU_FW_ISYS_RESP_TYPE_FRAME_SOF,           "FRAME_SOF", 1 },
	{ IPU_FW_ISYS_RESP_TYPE_FRAME_EOF,           "FRAME_EOF", 1 },
	{ IPU_FW_ISYS_RESP_TYPE_STATS_DATA_READY,    "STATS_READY", 1 },
	{ -1, "UNKNOWN MESSAGE", 0 },
};

static int resp_type_to_index(int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fw_msg); i++)
		if (fw_msg[i].type == type)
			return i;

	return i - 1;
}

static int isys_isr_one(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	struct ipu_fw_isys_resp_info_abi resp_data;
	struct ipu_fw_isys_resp_info_abi *resp;
	struct intel_ipu4_isys_pipeline *pipe;
	u64 ts;
	unsigned int i;

	if (!isys->fwcom)
		return 0;

	resp = isys->fwctrl->get_response(isys->fwcom, ISYS_MSG_INDEX,
					  &resp_data);
	if (!resp)
		return 1;

	ts = (u64)resp->timestamp[1] << 32 | resp->timestamp[0];

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

	if (resp->stream_handle >= INTEL_IPU4_ISYS_MAX_STREAMS) {
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
		intel_ipu4_put_fw_mgs_buffer(intel_ipu4_bus_get_drvdata(adev),
				  resp->buf_id);
		complete(&pipe->stream_open_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CLOSE_ACK:
		complete(&pipe->stream_close_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_ACK:
		complete(&pipe->stream_start_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
		intel_ipu4_put_fw_mgs_buffer(intel_ipu4_bus_get_drvdata(adev),
				  resp->buf_id);
		complete(&pipe->stream_start_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_STOP_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_FLUSH_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_PIN_DATA_READY:
		if (resp->pin_id <  INTEL_IPU4_ISYS_OUTPUT_PINS &&
		    pipe->output_pins[resp->pin_id].pin_ready)
			pipe->output_pins[resp->pin_id].pin_ready(pipe, resp);
		else
			dev_err(&adev->dev,
				"%d:No data pin ready handler for pin id %d\n",
				resp->stream_handle, resp->pin_id);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		intel_ipu4_put_fw_mgs_buffer(intel_ipu4_bus_get_drvdata(adev),
				  resp->buf_id);
		complete(&pipe->capture_ack_completion);
		break;
	case IPU_FW_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE:
	case IPU_FW_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE:
		if (pipe->interlaced) {
			struct intel_ipu4_isys_buffer *ib, *ib_safe;
			struct list_head list;
			unsigned long flags;

			if (pipe->isys->short_packet_source ==
			    INTEL_IPU_ISYS_SHORT_PACKET_FROM_TUNIT)
				pipe->cur_field =
					intel_ipu_isys_csi2_get_current_field(
					pipe, resp);
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
				struct vb2_buffer *vb =
					intel_ipu4_isys_buffer_to_vb2_buffer(
						ib);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				vb->v4l2_buf.field = pipe->cur_field;
#else
				to_vb2_v4l2_buffer(vb)->field = pipe->cur_field;
#endif
				list_del(&ib->head);

				intel_ipu4_isys_queue_buf_done(ib);
			}
		}
		for (i = 0; i < INTEL_IPU4_NUM_CAPTURE_DONE; i++)
			if (pipe->capture_done[i])
				pipe->capture_done[i](pipe, resp);

		break;
	case IPU_FW_ISYS_RESP_TYPE_FRAME_SOF:
		pipe->seq[pipe->seq_index].sequence =
			atomic_read(&pipe->sequence) - 1;
		pipe->seq[pipe->seq_index].timestamp = ts;
		dev_dbg(&adev->dev,
			"sof: handle %d: (index %u), timestamp 0x%16.16llx\n",
			resp->stream_handle,
			pipe->seq[pipe->seq_index].sequence,
			ts);
		pipe->seq_index = (pipe->seq_index + 1)
			% INTEL_IPU4_ISYS_MAX_PARALLEL_SOF;
		break;
	case IPU_FW_ISYS_RESP_TYPE_FRAME_EOF:
		break;
	case IPU_FW_ISYS_RESP_TYPE_STATS_DATA_READY:
		break;
	default:
		dev_err(&adev->dev, "%d:unknown response type %u\n",
			resp->stream_handle, resp->type);
		break;
	}

leave:
	isys->fwctrl->put_response(isys->fwcom, ISYS_MSG_INDEX);
	return 0;
}

static void isys_isr_ipu4(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status;

	status = readl(isys->pdata->base +
		       INTEL_IPU4_REG_ISYS_UNISPART_IRQ_STATUS);
	do {
		writel(status, isys->pdata->base +
		       INTEL_IPU4_REG_ISYS_UNISPART_IRQ_CLEAR);

		if (isys->isr_csi2_bits & status) {
			unsigned int i;

			for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
				if (status &
				 INTEL_IPU4_ISYS_UNISPART_IRQ_CSI2_B0(i))
					intel_ipu_isys_csi2_isr(
						&isys->csi2[i]);
			}
		}

		writel(0, base + INTEL_IPU4_REG_ISYS_UNISPART_SW_IRQ_REG);

		/*
		 * Handle a single FW event per checking the CSI-2
		 * receiver SOF status. This is done in order to avoid
		 * the case where events arrive to the event queue and
		 * one of them is a SOF event which then could be
		 * handled before the SOF interrupt. This would pose
		 * issues in sequence numbering which is based on SOF
		 * interrupts, always assumed to arrive before FW SOF
		 * events.
		 */
		if (status & INTEL_IPU4_ISYS_UNISPART_IRQ_SW &&
		    !isys_isr_one(adev))
			status = INTEL_IPU4_ISYS_UNISPART_IRQ_SW;
		else
			status = 0;

		status |= readl(isys->pdata->base +
				INTEL_IPU4_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (status & (isys->isr_csi2_bits
			   | INTEL_IPU4_ISYS_UNISPART_IRQ_SW) &&
		 !isys->adev->isp->flr_done);
}

static void isys_isr_ipu5(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status_csi, status_sw;

	status_csi = readl(isys->pdata->base +
		       INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_STATUS);
	status_sw = readl(isys->pdata->base +
		       INTEL_IPU5_REG_ISYS_UNISPART_IRQ_STATUS);

	do {
		writel(status_csi, isys->pdata->base +
		       INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_CLEAR);
		writel(INTEL_IPU5_ISYS_UNISPART_IRQ_SW,
			isys->pdata->base +
			INTEL_IPU5_REG_ISYS_UNISPART_IRQ_CLEAR);

		if (isys->isr_csi2_bits & status_csi) {
			unsigned int i;

			for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
				if (status_csi &
					INTEL_IPU5_ISYS_UNISPART_IRQ_CSI2_A0(i))
					intel_ipu_isys_csi2_isr(
						&isys->csi2[i]);
			}
		}

		writel(0, base + INTEL_IPU5_REG_ISYS_UNISPART_SW_IRQ_REG);

		/*
		 * Handle a single FW event per checking the CSI-2
		 * receiver SOF status. This is done in order to avoid
		 * the case where events arrive to the event queue and
		 * one of them is a SOF event which then could be
		 * handled before the SOF interrupt. This would pose
		 * issues in sequence numbering which is based on SOF
		 * interrupts, always assumed to arrive before FW SOF
		 * events.
		 */
		/* TODO:check sw_irq bit then call isr_one() */
		if (!isys_isr_one(adev))
			status_sw = INTEL_IPU5_ISYS_UNISPART_IRQ_SW;
		else
			status_sw = 0;

		status_csi = readl(isys->pdata->base +
			INTEL_IPU5_REG_ISYS_CSI_TOP_IRQ_STATUS);
		status_sw |= readl(isys->pdata->base +
			INTEL_IPU5_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (((status_csi & isys->isr_csi2_bits) ||
		(status_sw & INTEL_IPU5_ISYS_UNISPART_IRQ_SW)) &&
		 !isys->adev->isp->flr_done);

}

static irqreturn_t isys_isr(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);

	spin_lock(&isys->power_lock);
	if (!isys->power) {
		spin_unlock(&isys->power_lock);
		return IRQ_NONE;
	}

	if (is_intel_ipu4_hw_bxt_b0(adev->isp))
		isys_isr_ipu4(adev);
	else
		isys_isr_ipu5(adev);

	spin_unlock(&isys->power_lock);
	return IRQ_HANDLED;
}

static void isys_isr_poll(struct intel_ipu4_bus_device *adev)
{
	struct intel_ipu4_isys *isys = intel_ipu4_bus_get_drvdata(adev);

	if (!isys->fwcom) {
		dev_dbg(&isys->adev->dev,
			"got interrupt but device not configured yet\n");
		return;
	}
	mutex_lock(&isys->mutex);
	if (is_intel_ipu5_hw_a0(adev->isp))
		isys_isr_ipu5(adev);
	else
		isys_isr_ipu4(adev);

	mutex_unlock(&isys->mutex);
}

int intel_ipu4_isys_isr_run(void *ptr)
{
	struct intel_ipu4_isys *isys = ptr;

	while (!kthread_should_stop()) {
		usleep_range(500, 1000);
		if (isys->stream_opened)
			isys_isr_poll(isys->adev);
	}

	return 0;
}

static struct intel_ipu4_bus_driver isys_driver = {
	.probe = isys_probe,
	.remove = isys_remove,
	.isr = isys_isr,
	.wanted = INTEL_IPU4_ISYS_NAME,
	.drv = {
		.name = INTEL_IPU4_ISYS_NAME,
		.owner = THIS_MODULE,
		.pm = ISYS_PM_OPS,
	},
};

module_intel_ipu4_bus_driver(isys_driver);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 input system driver");
