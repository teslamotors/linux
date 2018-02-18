/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a/gk20a.h"
#include "pwrdev.h"
#include "pmgrpmu.h"
#include <linux/debugfs.h>

int pmgr_pwr_devices_get_power(struct gk20a *g, u32 *val)
{
	struct nv_pmu_pmgr_pwr_devices_query_payload payload;
	int status;

	status = pmgr_pmu_pwr_devices_query_blocking(g, 1, &payload);
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"pmgr_pwr_devices_get_current_power failed %x",
			status);

	*val = payload.devices[0].powerm_w;

	return status;
}

int pmgr_pwr_devices_get_current(struct gk20a *g, u32 *val)
{
	struct nv_pmu_pmgr_pwr_devices_query_payload payload;
	int status;

	status = pmgr_pmu_pwr_devices_query_blocking(g, 1, &payload);
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"pmgr_pwr_devices_get_current failed %x",
			status);

	*val = payload.devices[0].currentm_a;

	return status;
}

int pmgr_pwr_devices_get_voltage(struct gk20a *g, u32 *val)
{
	struct nv_pmu_pmgr_pwr_devices_query_payload payload;
	int status;

	status = pmgr_pmu_pwr_devices_query_blocking(g, 1, &payload);
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"pmgr_pwr_devices_get_current_voltage failed %x",
			status);

	*val = payload.devices[0].voltageu_v;

	return status;
}

#ifdef CONFIG_DEBUG_FS
int pmgr_pwr_devices_get_power_u64(void *data, u64 *p)
{
	struct gk20a *g = (struct gk20a *)data;
	int err;
	u32 val;

	err = pmgr_pwr_devices_get_power(g, &val);
	*p = val;

	return err;
}

int pmgr_pwr_devices_get_current_u64(void *data, u64 *p)
{
	struct gk20a *g = (struct gk20a *)data;
	int err;
	u32 val;

	err = pmgr_pwr_devices_get_current(g, &val);
	*p = val;

	return err;
}

int pmgr_pwr_devices_get_voltage_u64(void *data, u64 *p)
{
	struct gk20a *g = (struct gk20a *)data;
	int err;
	u32 val;

	err = pmgr_pwr_devices_get_voltage(g, &val);
	*p = val;

	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(
		pmgr_power_ctrl_fops, pmgr_pwr_devices_get_power_u64, NULL, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(
		pmgr_current_ctrl_fops, pmgr_pwr_devices_get_current_u64, NULL, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(
		pmgr_voltage_ctrl_fops, pmgr_pwr_devices_get_voltage_u64, NULL, "%llu\n");

static void pmgr_debugfs_init(struct gk20a *g) {
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	struct dentry *dbgentry;

	dbgentry = debugfs_create_file(
				"power", S_IRUGO, platform->debugfs, g, &pmgr_power_ctrl_fops);
	if (!dbgentry)
		gk20a_err(dev_from_gk20a(g),
				"debugfs entry create failed for power");

	dbgentry = debugfs_create_file(
				"current", S_IRUGO, platform->debugfs, g, &pmgr_current_ctrl_fops);
	if (!dbgentry)
		gk20a_err(dev_from_gk20a(g),
				"debugfs entry create failed for current");

	dbgentry = debugfs_create_file(
				"voltage", S_IRUGO, platform->debugfs, g, &pmgr_voltage_ctrl_fops);
	if (!dbgentry)
		gk20a_err(dev_from_gk20a(g),
				"debugfs entry create failed for voltage");
}
#endif

u32 pmgr_domain_sw_setup(struct gk20a *g)
{
	u32 status;

	status = pmgr_device_sw_setup(g);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for pmgr devices, status - 0x%x",
			status);
		goto exit;
	}

	status = pmgr_monitor_sw_setup(g);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for pmgr monitor, status - 0x%x",
			status);
		goto exit;
	}

	status = pmgr_policy_sw_setup(g);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for pmgr policy, status - 0x%x",
			status);
		goto exit;
	}

#ifdef CONFIG_DEBUG_FS
	pmgr_debugfs_init(g);
#endif

exit:
	return status;
}

u32 pmgr_domain_pmu_setup(struct gk20a *g)
{
	return pmgr_send_pmgr_tables_to_pmu(g);
}
