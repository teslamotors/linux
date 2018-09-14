// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/version.h>

#include <media/ipu-isys.h>
#include <media/ipu4-acpi.h>

#include "ipu.h"
#include "ipu-bus.h"
#include "ipu-cpd.h"
#include "ipu-mmu.h"
#include "ipu-dma.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-trace.h"
#include "ipu-buttress.h"
#include "isysapi/interface/ia_css_isysapi.h"
#include "./ici/ici-isys.h"
#include "./ici/ici-isys-csi2.h"
#include "./ici/ici-isys-pipeline-device.h"

#ifdef ICI_ENABLED

#define ISYS_PM_QOS_VALUE	300

#define INTEL_IPU4_ISYS_OUTPUT_PINS 11
#define INTEL_IPU4_NUM_CAPTURE_DONE 2

/* Trace block definitions for isys */
struct ipu_trace_block isys_trace_blocks[] = {
	{
		.offset = TRACE_REG_IS_TRACE_UNIT_BASE,
		.type = IPU_TRACE_BLOCK_TUN,
	},
	{
		.offset = TRACE_REG_IS_SP_EVQ_BASE,
		.type = IPU_TRACE_BLOCK_TM,
	},
	{
		.offset = TRACE_REG_IS_SP_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_IS_ISL_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_IS_MMU_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		.offset = TRACE_REG_CSI2_TM_BASE,
		.type = IPU_TRACE_CSI2,
	},
	{
		.offset = TRACE_REG_CSI2_3PH_TM_BASE,
		.type = IPU_TRACE_CSI2_3PH,
	},
	{
		/* Note! this covers all 9 blocks */
		.offset = TRACE_REG_CSI2_SIG2SIO_GR_BASE(0),
		.type = IPU_TRACE_SIG2CIOS,
	},
	{
		/* Note! this covers all 9 blocks */
		.offset = TRACE_REG_CSI2_PH3_SIG2SIO_GR_BASE(0),
		.type = IPU_TRACE_SIG2CIOS,
	},
	{
		.offset = TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N,
		.type = IPU_TRACE_TIMER_RST,
	},
	{
		.type = IPU_TRACE_BLOCK_END,
	}
};


// Latest code structure doesnt do these functions.
// let it remain to gauge the impact and then remove.
#if 0
static int isys_determine_legacy_csi_lane_configuration(struct ici_isys *isys)
{
	const struct csi_lane_cfg {
		u32 reg_value;
		int port_lanes[IPU_ISYS_MAX_CSI2_LEGACY_PORTS];
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
		for (j = 0; j < IPU_ISYS_MAX_CSI2_LEGACY_PORTS; j++) {
			/* Port with no sensor can be handled as don't care */
			if (!isys->ici_csi2[j].nlanes)
				continue;
			if (csi_lanes_to_cfg[i].port_lanes[j] !=
			    isys->ici_csi2[j].nlanes)
				break;
		}

		if (j < IPU_ISYS_MAX_CSI2_LEGACY_PORTS)
			continue;

		isys->legacy_port_cfg = csi_lanes_to_cfg[i].reg_value;
		dev_dbg(&isys->adev->dev, "Lane configuration value 0x%x\n,",
			 isys->legacy_port_cfg);
		return 0;
	}
	dev_err(&isys->adev->dev, "Non supported CSI lane configuration\n,");
	return -EINVAL;
}

static int isys_determine_csi_combo_lane_configuration(struct ici_isys *isys)
{
	const struct csi_lane_cfg {
		u32 reg_value;
		int port_lanes[IPU_ISYS_MAX_CSI2_COMBO_PORTS];
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
		for (j = 0; j <  IPU_ISYS_MAX_CSI2_COMBO_PORTS; j++) {
			/* Port with no sensor can be handled as don't care */
			if (!isys->ici_csi2[j + IPU_ISYS_MAX_CSI2_LEGACY_PORTS].nlanes)
				continue;
			if (csi_lanes_to_cfg[i].port_lanes[j] !=
			    isys->ici_csi2[j + IPU_ISYS_MAX_CSI2_LEGACY_PORTS].nlanes)
				break;
		}

		if (j < IPU_ISYS_MAX_CSI2_COMBO_PORTS)
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

#endif
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

static struct ici_ext_subdev *register_acpi_i2c_subdev(
	struct ipu_isys_subdev_info *sd_info, struct i2c_client *client)
{
	struct i2c_board_info *info = &sd_info->i2c.board_info;
	struct ici_ext_subdev *sd;

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

	module_put(client->dev.driver->owner);

	return sd;
}

static int ext_device_setup_node(void* ipu_data,
	struct ici_ext_subdev *sd,
	const char* name)
{
	int rval;
	struct ici_isys *isys = ipu_data;
	sd->node.sd = sd;
	sd->node.external = true;
	rval = ici_isys_pipeline_node_init(
		isys, &sd->node, name, sd->num_pads, sd->pads);
	if (rval)
		return rval;
	sd->num_pads = sd->node.nr_pads;
	return 0;
}

static int isys_complete_ext_device_registration(
	struct ici_isys *isys,
	struct ici_ext_subdev *sd,
	struct ipu_isys_csi2_config *csi2)
{
	int rval;
	struct ici_ext_subdev_register sd_register = {0};
	unsigned int i;

	sd_register.ipu_data = isys;
	sd_register.sd = sd;
	sd_register.setup_node = ext_device_setup_node;
	sd_register.create_link = node_pad_create_link;
	rval = sd->do_register(&sd_register);
	if (rval) {
		dev_err(&isys->adev->dev,
			"Failed to regsiter external subdev\n");
		return rval;
	}
	if (csi2) {
        for (i = 0; i < NR_OF_CSI2_VC; i++) {
            rval = node_pad_create_link(&sd->node, sd->src_pad,
                    &isys->ici_csi2[csi2->port].asd[i].node,
                    CSI2_ICI_PAD_SINK, 0);
            if (rval) {
                dev_warn(&isys->adev->dev,
                        "can't create link from external node\n");
                return rval;
            }

            isys->ici_csi2[csi2->port].nlanes = csi2->nlanes;
            isys->ici_csi2[csi2->port].ext_sd = sd;
        }
    }
	return 0;
}

static int isys_register_ext_subdev(struct ici_isys *isys,
				    struct ipu_isys_subdev_info *sd_info,
				    bool acpi_only)
{
	struct i2c_adapter *adapter =
		i2c_get_adapter(sd_info->i2c.i2c_adapter_id);
	struct ici_ext_subdev *sd;
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
		if (sd_info->csi2->port >= IPU_ISYS_MAX_CSI2_PORTS ||
		    !isys->ici_csi2[sd_info->csi2->port].isys) {
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
		rval = 0;
		goto skip_put_adapter;
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
	} 
	else if (sd_info->acpiname) {
		dev_dbg(&isys->adev->dev, "ACPI name don't match: %s\n",
			sd_info->acpiname);
		rval = 0;
		goto skip_put_adapter;
	}
	if (!client) {
		dev_info(&isys->adev->dev,
			 "i2c device not found in ACPI table\n");
		client = i2c_new_device(adapter,
			&sd_info->i2c.board_info);
		sd = i2c_get_clientdata(client);
	} else {
		dev_info(&isys->adev->dev, "i2c device found in ACPI table\n");
		sd = register_acpi_i2c_subdev(sd_info, client);
	}

	if (!sd) {
		dev_warn(&isys->adev->dev, "can't create new i2c subdev\n");
		rval = -EINVAL;
		goto skip_put_adapter;
	}

	return isys_complete_ext_device_registration(isys, sd, sd_info->csi2);

skip_put_adapter:
	i2c_put_adapter(adapter);

	return rval;
}

static int isys_acpi_add_device(struct device *dev, void *priv,
				struct ipu_isys_csi2_config *csi2,
				bool reprobe)
{
	struct ici_isys *isys = priv;
	struct i2c_client *client = i2c_verify_client(dev);
	struct ici_ext_subdev *sd;

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
        module_put(client->dev.driver->owner);
	if (!sd) {
		dev_warn(&isys->adev->dev, "can't create new i2c subdev\n");
		return -ENODEV;
	}

	return isys_complete_ext_device_registration(isys, sd, csi2);
}

static void isys_register_ext_subdevs(struct ici_isys *isys)
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

static void isys_unregister_subdevices(struct ici_isys *isys)
{
	const struct ipu_isys_internal_tpg_pdata *tpg =
		&isys->pdata->ipdata->tpg;
	const struct ipu_isys_internal_csi2_pdata *csi2 =
		&isys->pdata->ipdata->csi2;
	unsigned int i;

    for (i = 0; i < NR_OF_CSI2_BE_SOC_STREAMS; i++) {
		ici_isys_csi2_be_cleanup(&isys->ici_csi2_be[i]);
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		ici_isys_tpg_cleanup(&isys->ici_tpg[i]);
	}

	for (i = 0; i < csi2->nports; i++) {
		ici_isys_csi2_cleanup(&isys->ici_csi2[i]);
	}
}

static int isys_register_subdevices(struct ici_isys *isys)
{
	const struct ipu_isys_internal_tpg_pdata *tpg =
		&isys->pdata->ipdata->tpg;
	const struct ipu_isys_internal_csi2_pdata *csi2 =
		&isys->pdata->ipdata->csi2;

	unsigned int i, j, k;
	int rval;

	BUG_ON(csi2->nports > IPU_ISYS_MAX_CSI2_PORTS);
	BUG_ON(tpg->ntpgs > 2);

	for (i = 0; i < csi2->nports; i++) {
		rval = ici_isys_csi2_init(
			&isys->ici_csi2[i], isys,
			isys->pdata->base + csi2->offsets[i], i);
		if (rval)
			goto fail;

		isys->isr_csi2_bits |=
				IPU_ISYS_UNISPART_IRQ_CSI2(i);
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = ici_isys_tpg_init(&isys->ici_tpg[i], isys,
				isys->pdata->base + tpg->offsets[i],
				isys->pdata->base + tpg->sels[i], i);
		if(rval)
			goto fail;
	}

	for (i = 0; i < NR_OF_CSI2_BE_SOC_STREAMS; i++) {
		rval = ici_isys_csi2_be_init(&isys->ici_csi2_be[i],
				isys, i);
		if (rval) {
			goto fail;
		}
	}

    for (i = 0; i < csi2->nports; i++) {
        for (j = 0; j < NR_OF_CSI2_VC; j++ ) {
            rval = node_pad_create_link(
                    &isys->ici_csi2[i].asd[j].node, CSI2_ICI_PAD_SOURCE,
                    &isys->ici_csi2_be[ICI_BE_RAW].asd.node,
                    CSI2_BE_ICI_PAD_SINK, 0);
            if (rval) {
                dev_info(&isys->adev->dev,
                        "can't create link between csi2 and csi2_be\n");
                goto fail;
            }

            for (k = 1; k < NR_OF_CSI2_BE_SOC_STREAMS; k++ ) {
				rval = node_pad_create_link(
					&isys->ici_csi2[i].asd[j].node, CSI2_ICI_PAD_SOURCE,
					&isys->ici_csi2_be[k].asd.node,
					CSI2_BE_ICI_PAD_SINK, 0);
				if (rval) {
					dev_info(&isys->adev->dev,
						"can't create link between csi2 and csi2_be soc\n");
					goto fail;
				}
       		 	}
		}
	}

	for (i = 0; i < tpg->ntpgs; i++) {
		rval = node_pad_create_link(
			&isys->ici_tpg[i].asd.node, TPG_PAD_SOURCE,
			&isys->ici_csi2_be[ICI_BE_RAW].asd.node,
			CSI2_BE_ICI_PAD_SINK, 0);
		if (rval) {
			dev_info(&isys->adev->dev,
				"can't create link between tpg and csi2_be\n");
			goto fail;
		}

        for (j = 1; j < NR_OF_CSI2_BE_SOC_STREAMS; j++) {
		    rval = node_pad_create_link(
			    &isys->ici_tpg[i].asd.node, TPG_PAD_SOURCE,
			    &isys->ici_csi2_be[j].asd.node,
			    CSI2_BE_ICI_PAD_SINK, 0);
		    if (rval) {
			    dev_info(&isys->adev->dev,
			    	"can't create link between tpg and csi2_be soc\n");
			    goto fail;
		    }
        }
    }

	return 0;

fail:
	isys_unregister_subdevices(isys);
	return rval;
}

static int isys_register_devices(struct ici_isys *isys)
{
	int rval;

/* Pipeline device registration */
	DEBUGK("Pipeline device registering...\n");
	rval = pipeline_device_register(&isys->pipeline_dev, isys);
	if (rval < 0) {
		dev_info(&isys->pipeline_dev.dev, "can't register pipeline device\n");
		return rval;
	}

	rval = isys_register_subdevices(isys);
	if (rval)
		goto out_pipeline_device_unregister;

	isys_register_ext_subdevs(isys);

// Latest code structure doesnt do these functions.
// let it remain to gaugae impact and then remove.
#if 0
	rval = isys_determine_legacy_csi_lane_configuration(isys);
	if (rval)
		goto out_isys_unregister_subdevices;

	rval = isys_determine_csi_combo_lane_configuration(isys);
	if (rval)
		goto out_isys_unregister_subdevices;

#ifndef CONFIG_PM
	ipu_buttress_csi_port_config(isys->adev->isp,
					    isys->legacy_port_cfg,
					    isys->combo_port_cfg);
#endif
#endif
	return 0;

#if 0
out_isys_unregister_subdevices:
	isys_unregister_subdevices(isys);
#endif
out_pipeline_device_unregister:
	pipeline_device_unregister(&isys->pipeline_dev);

	return rval;
}

static void isys_unregister_devices(struct ici_isys *isys)
{
	pipeline_device_unregister(&isys->pipeline_dev);
	DEBUGK("Pipeline device unregistered\n");
	isys_unregister_subdevices(isys);
}

static void isys_setup_hw(struct ici_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	u32 irqs;
	unsigned int i;

	/* Enable irqs for all MIPI busses */
	irqs = IPU_ISYS_UNISPART_IRQ_CSI2(0) |
	       IPU_ISYS_UNISPART_IRQ_CSI2(1) |
	       IPU_ISYS_UNISPART_IRQ_CSI2(2) |
	       IPU_ISYS_UNISPART_IRQ_CSI2(3) |
	       IPU_ISYS_UNISPART_IRQ_CSI2(4) |
	       IPU_ISYS_UNISPART_IRQ_CSI2(5);

	irqs |= IPU_ISYS_UNISPART_IRQ_SW;

	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_EDGE);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_MASK);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_ENABLE);

	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);

	/* Write CDC FIFO threshold values for isys */
	for (i = 0; i < isys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(isys->pdata->ipdata->hw_variant.cdc_fifo_threshold[i],
		       base + IPU_REG_ISYS_CDC_THRESHOLD(i));
}

#ifdef CONFIG_PM
static int isys_runtime_pm_resume(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_device *isp = adev->isp;
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);
	unsigned long flags;
	int ret;

	if (!isys) {
		WARN(1, "%s called before probing. skipping.\n", __func__);
		return 0;
	}

	ipu_trace_restore(dev);

	pm_qos_update_request(&isys->pm_qos, ISYS_PM_QOS_VALUE);
#if 0
	ipu_buttress_csi_port_config(isp,
					    isys->legacy_port_cfg,
					    isys->combo_port_cfg);
#endif
	ret = ipu_buttress_start_tsc_sync(isp);
	if (ret)
		return ret;

	spin_lock_irqsave(&isys->power_lock, flags);
	isys->power = 1;
	spin_unlock_irqrestore(&isys->power_lock, flags);

	isys_setup_hw(isys);

	return 0;
}

static int isys_runtime_pm_suspend(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);
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
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);

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
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);
	struct ipu_device *isp = adev->isp;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	struct dma_attrs attrs;
#else
	unsigned long attrs;
#endif

	dev_info(&adev->dev, "removed\n");
	debugfs_remove_recursive(isys->debugfsdir);

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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
	dma_free_attrs(&adev->dev,
		IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
		isys->short_packet_trace_buffer,
		isys->short_packet_trace_buffer_dma_addr, &attrs);
#else
	attrs = DMA_ATTR_NON_CONSISTENT;
	dma_free_attrs(&adev->dev,
		IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
		isys->short_packet_trace_buffer,
		isys->short_packet_trace_buffer_dma_addr, attrs);
#endif
}

static int intel_ipu4_isys_icache_prefetch_get(void *data, u64 *val)
{
	struct ici_isys *isys = data;

	*val = isys->icache_prefetch;
	return 0;
}

static int intel_ipu4_isys_icache_prefetch_set(void *data, u64 val)
{
	struct ici_isys *isys = data;

	if (val != !!val)
		return -EINVAL;

	isys->icache_prefetch = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(isys_icache_prefetch_fops,
			intel_ipu4_isys_icache_prefetch_get,
			intel_ipu4_isys_icache_prefetch_set,
			"%llu\n");

static int intel_ipu4_isys_init_debugfs(struct ici_isys *isys)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir("isys", isys->adev->isp->ipu_dir);
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

static int alloc_fw_msg_buffers(struct ici_isys *isys, int amount)
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

static int isys_probe(struct ipu_bus_device *adev)
{
	struct ipu_mmu *mmu = dev_get_drvdata(adev->iommu);
	struct ici_isys *isys;
	struct ipu_device *isp = adev->isp;
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
	if (!isys) {
		trace_printk("E|TMWK\n");
		return -ENOMEM;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	init_dma_attrs(&attrs);
	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);
	isys->short_packet_trace_buffer = dma_alloc_attrs(&adev->dev,
		IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
		&isys->short_packet_trace_buffer_dma_addr, GFP_KERNEL, &attrs);
#else
	attrs = DMA_ATTR_NON_CONSISTENT;
	isys->short_packet_trace_buffer = dma_alloc_attrs(&adev->dev,
		IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
		&isys->short_packet_trace_buffer_dma_addr, GFP_KERNEL, attrs);
#endif
	if (!isys->short_packet_trace_buffer)
		return -ENOMEM;

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
	isys->icache_prefetch = is_ipu_hw_bxtp_e0(isp);

#ifndef CONFIG_PM
	isys_setup_hw(isys);
#endif

	if (!isp->secure_mode) {
		fw = isp->cpd_fw;

		rval = ipu_buttress_map_fw_image(
			adev, fw, &isys->fw_sgt);
		if (rval)
			goto release_firmware;

		isys->pkg_dir = ipu_cpd_create_pkg_dir(
			adev, isp->cpd_fw->data,
			sg_dma_address(isys->fw_sgt.sgl),
			&isys->pkg_dir_dma_addr,
			&isys->pkg_dir_size);
		if (isys->pkg_dir == NULL) {
			rval = -ENOMEM;
			goto  remove_shared_buffer;
		}
	}

	/* Debug fs failure is not fatal. */
	intel_ipu4_isys_init_debugfs(isys);

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
		ipu_buttress_unmap_fw_image(
			adev, &isys->fw_sgt);
release_firmware:
	if (!isp->secure_mode)
		release_firmware(isys->fw);
	ipu_trace_uninit(&adev->dev);

	trace_printk("E|TMWK\n");

	mutex_destroy(&isys->mutex);
	mutex_destroy(&isys->stream_mutex);
	mutex_destroy(&isys->lib_mutex);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	dma_free_attrs(&adev->dev,
		IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
		isys->short_packet_trace_buffer,
		isys->short_packet_trace_buffer_dma_addr, &attrs);
#else
        dma_free_attrs(&adev->dev,
                IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
                isys->short_packet_trace_buffer,
                isys->short_packet_trace_buffer_dma_addr, attrs);
#endif
	return rval;
}

struct fwmsg {
	int type;
	char *msg;
	bool valid_ts;
};

static const struct fwmsg fw_msg[] = {
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE,    "STREAM_OPEN_DONE", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_CLOSE_ACK,    "STREAM_CLOSE_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_START_ACK,    "STREAM_START_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK,
	  "STREAM_START_AND_CAPTURE_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_STOP_ACK,     "STREAM_STOP_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_FLUSH_ACK,    "STREAM_FLUSH_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY,      "PIN_DATA_READY", 1 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK,  "STREAM_CAPTURE_ACK", 0 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE,
	  "STREAM_START_AND_CAPTURE_DONE", 1 },
	{ IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE, "STREAM_CAPTURE_DONE", 1 },
	{ IA_CSS_ISYS_RESP_TYPE_FRAME_SOF,           "FRAME_SOF", 1 },
	{ IA_CSS_ISYS_RESP_TYPE_FRAME_EOF,           "FRAME_EOF", 1 },
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


static u64 extract_time_from_short_packet_msg(
			struct ici_isys_csi2_monitor_message *msg)

{
	u64 time_h = msg->timestamp_h << 14;
	u64 time_l = msg->timestamp_l;
	u64 time_h_ovl = time_h & 0xc000;
	u64 time_h_h = time_h & (~0xffff);

	/* Fix possible roll overs. */
	if (time_h_ovl >= (time_l & 0xc000))
		return time_h_h | time_l;
	else
		return (time_h_h - 0x10000) | time_l;
}
static u64 tunit_time_to_us(struct ici_isys *isys, u64 time)
{
	struct ipu_bus_device *adev =
			to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 1000000;
	return time / isys_clk;
}

static u64 tsc_time_to_tunit_time(struct ici_isys *isys,
			u64 tsc_base, u64 tunit_base, u64 tsc_time)
{
	struct ipu_bus_device *adev =
			to_ipu_bus_device(isys->adev->iommu);
	u64 isys_clk = IS_FREQ_SOURCE / adev->ctrl->divisor / 100000;
	u64 tsc_clk = IPU_BUTTRESS_TSC_CLK / 100000;

	return (tsc_time - tsc_base) * isys_clk / tsc_clk + tunit_base;
}

static int isys_isr_one_ici(struct ipu_bus_device *adev)
{
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);
	struct ia_css_isys_resp_info resp;
	struct ici_isys_pipeline *pipe;
	u64 ts;
	int rval;
	unsigned int i;

	if (!isys->fwcom)
		return 0;

	rval = ipu_lib_call_notrace_unlocked(stream_handle_response,
						    isys, &resp);
	if (rval < 0)
		return rval;

	ts = (u64)resp.timestamp[1] << 32 | resp.timestamp[0];


	if (resp.error == IA_CSS_ISYS_ERROR_STREAM_IN_SUSPENSION)
		/* Suspension is kind of special case: not enough buffers */
		dev_dbg(&adev->dev,
			"hostlib: error resp %02d %s, stream %u, error SUSPENSION, details %d, timestamp 0x%16.16llx, pin %d\n",
			resp.type,
			fw_msg[resp_type_to_index(resp.type)].msg,
			resp.stream_handle,
			resp.error_details,
			fw_msg[resp_type_to_index(resp.type)].valid_ts ?
			ts : 0, resp.pin_id);
	else if (resp.error)
		dev_dbg(&adev->dev,
			"hostlib: error resp %02d %s, stream %u, error %d, details %d, timestamp 0x%16.16llx, pin %d\n",
			resp.type,
			fw_msg[resp_type_to_index(resp.type)].msg,
			resp.stream_handle,
			resp.error, resp.error_details,
			fw_msg[resp_type_to_index(resp.type)].valid_ts ?
			ts : 0, resp.pin_id);
	else
		dev_dbg(&adev->dev,
			"hostlib: resp %02d %s, stream %u, timestamp 0x%16.16llx, pin %d\n",
			resp.type,
			fw_msg[resp_type_to_index(resp.type)].msg,
			resp.stream_handle,
			fw_msg[resp_type_to_index(resp.type)].valid_ts ?
			ts : 0, resp.pin_id);

	if (resp.stream_handle >= INTEL_IPU4_ISYS_MAX_STREAMS) {
		dev_err(&adev->dev, "bad stream handle %u\n",
			resp.stream_handle);
		return 0;
	}

	pipe = isys->ici_pipes[resp.stream_handle];
	if (!pipe) {
		dev_err(&adev->dev, "no pipeline for stream %u\n",
			resp.stream_handle);
		return 0;
	}
	pipe->error = resp.error;

	switch (resp.type) {
	case IA_CSS_ISYS_RESP_TYPE_STREAM_OPEN_DONE:
		complete(&pipe->stream_open_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CLOSE_ACK:
		complete(&pipe->stream_close_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_ACK:
		complete(&pipe->stream_start_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_ACK:
		complete(&pipe->stream_start_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_STOP_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_FLUSH_ACK:
		complete(&pipe->stream_stop_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_PIN_DATA_READY:
		if (resp.pin_id <  IPU_ISYS_OUTPUT_PINS &&
		    pipe->output_pins[resp.pin_id].pin_ready)
			pipe->output_pins[resp.pin_id].pin_ready(pipe, &resp);
		else
			dev_err(&adev->dev,
				"%d:No data pin ready handler for pin id %d\n",
				resp.stream_handle, resp.pin_id);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_ACK:
		complete(&pipe->capture_ack_completion);
		break;
	case IA_CSS_ISYS_RESP_TYPE_STREAM_START_AND_CAPTURE_DONE:
	case IA_CSS_ISYS_RESP_TYPE_STREAM_CAPTURE_DONE:

		if(pipe->interlaced && pipe->short_packet_source ==
            IPU_ISYS_SHORT_PACKET_FROM_TUNIT) {
			unsigned int i = pipe->short_packet_trace_index;
			bool msg_matched = false;
			unsigned int monitor_id;

			if(pipe->csi2->index>=	IPU_ISYS_MAX_CSI2_LEGACY_PORTS)
				monitor_id = TRACE_REG_CSI2_3PH_TM_MONITOR_ID;
			else
				monitor_id = TRACE_REG_CSI2_TM_MONITOR_ID;

			dma_sync_single_for_cpu(&isys->adev->dev,
				isys->short_packet_trace_buffer_dma_addr,
				IPU_ISYS_SHORT_PACKET_TRACE_BUFFER_SIZE,
				DMA_BIDIRECTIONAL);

			do {
				struct ici_isys_csi2_monitor_message msg =	isys->short_packet_trace_buffer[i];
				u64 sof_time = tsc_time_to_tunit_time(isys,
					isys->tsc_timer_base, isys->tunit_timer_base,
					(u64) resp.timestamp[1] << 32 | resp.timestamp[0]);
				u64 trace_time = extract_time_from_short_packet_msg(&msg);
				u64 delta_time_us = tunit_time_to_us(isys,
					(sof_time > trace_time) ?
					sof_time - trace_time :
					trace_time - sof_time);

				i = (i + 1) % IPU_ISYS_SHORT_PACKET_TRACE_MSG_NUMBER;
				if (msg.cmd == TRACE_REG_CMD_TYPE_D64MTS &&
				    msg.monitor_id == monitor_id &&
				    msg.fs == 1 &&
				    msg.port == pipe->csi2->index &&
				    msg.vc == pipe->vc &&
				    delta_time_us < IPU_ISYS_SHORT_PACKET_TRACE_MAX_TIMESHIFT) {
					    pipe->cur_field = (msg.sequence % 2) ?
                            ICI_FIELD_TOP : ICI_FIELD_BOTTOM;
					    pipe->short_packet_trace_index = i;
					    msg_matched = true;
					    dev_dbg(&isys->adev->dev,"Interlaced field ready. field = %d\n",
                            pipe->cur_field);
					break;
				}
			} while (i != pipe->short_packet_trace_index);

			if (!msg_matched)
			/* We have walked through the whole buffer. */
				dev_dbg(&isys->adev->dev,"No matched trace message found.\n");
		}

		for (i = 0; i < INTEL_IPU4_NUM_CAPTURE_DONE; i++)
			if (pipe->capture_done[i])
				pipe->capture_done[i](pipe, &resp);
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_SOF:
		break;
	case IA_CSS_ISYS_RESP_TYPE_FRAME_EOF:
		break;
	default:
		dev_err(&adev->dev, "%d:unknown response type %u\n",
			resp.stream_handle, resp.type);
		break;
	}

	return 0;
}

static irqreturn_t isys_isr(struct ipu_bus_device *adev)
{
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status;

	spin_lock(&isys->power_lock);
	if (!isys->power) {
		spin_unlock(&isys->power_lock);
		return IRQ_NONE;
	}

	status = readl(isys->pdata->base +
		       IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	do {
		writel(status, isys->pdata->base +
		       IPU_REG_ISYS_UNISPART_IRQ_CLEAR);

		if (isys->isr_csi2_bits & status) {
			unsigned int i;

			for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
				if (status &
				 IPU_ISYS_UNISPART_IRQ_CSI2(i)){

				    ici_isys_csi2_isr(
						&isys->ici_csi2[i]);
				}
			}
		}

		writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);

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
		if (status & IPU_ISYS_UNISPART_IRQ_SW &&
		    !isys_isr_one_ici(adev))
			status = IPU_ISYS_UNISPART_IRQ_SW;
		else
			status = 0;

		status |= readl(isys->pdata->base +
				IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (status & (isys->isr_csi2_bits
			   | IPU_ISYS_UNISPART_IRQ_SW));

	spin_unlock(&isys->power_lock);
	return IRQ_HANDLED;
}

static void isys_isr_poll_ici(struct ipu_bus_device *adev)
{
	struct ici_isys *isys = ipu_bus_get_drvdata(adev);

	if (!isys->fwcom) {
		dev_dbg(&isys->adev->dev,
			"got interrupt but device not configured yet\n");
		return;
	}

	while (!isys_isr_one_ici(adev));
}

int intel_ipu4_isys_isr_run_ici(void *ptr)
{
	struct ici_isys *isys = ptr;

	while (!kthread_should_stop()) {
		usleep_range(500, 1000);
		if (isys->ici_stream_opened)
			isys_isr_poll_ici(isys->adev);
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

MODULE_AUTHOR("Scott Kennedy <scottx.m.kennedy@intel.com>");
MODULE_AUTHOR("Marcin Mozejko <marcinx.mozejko@intel.com>");
MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Jouni Ukkonen <jouni.ukkonen@intel.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 ici input system driver");

#endif /* ICI_ENABLED */

