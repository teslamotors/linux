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

#include <linux/sort.h>

#include "gk20a/gk20a.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlvolt.h"
#include "gk20a/pmu_gk20a.h"

#include "pmuif/gpmuifperfvfe.h"
#include "include/bios.h"
#include "volt.h"

#define VOLT_DEV_PWM_VOLTAGE_STEPS_INVALID	0
#define VOLT_DEV_PWM_VOLTAGE_STEPS_DEFAULT	1

u32 volt_device_pmu_data_init_super(struct gk20a *g,
	struct boardobj *pboard_obj, struct nv_pmu_boardobj *ppmudata)
{
	u32 status;
	struct voltage_device *pdev;
	struct nv_pmu_volt_volt_device_boardobj_set *pset;

	status = boardobj_pmudatainit_super(g, pboard_obj, ppmudata);
	if (status)
		return status;

	pdev = (struct voltage_device *)pboard_obj;
	pset = (struct nv_pmu_volt_volt_device_boardobj_set *)ppmudata;

	pset->switch_delay_us = pdev->switch_delay_us;
	pset->voltage_min_uv = pdev->voltage_min_uv;
	pset->voltage_max_uv = pdev->voltage_max_uv;
	pset->volt_step_uv = pdev->volt_step_uv;

	return status;
}

static u32 volt_device_pmu_data_init_pwm(struct gk20a *g,
		struct boardobj *pboard_obj, struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct voltage_device_pwm *pdev;
	struct nv_pmu_volt_volt_device_pwm_boardobj_set *pset;

	status = volt_device_pmu_data_init_super(g, pboard_obj, ppmudata);
	if (status)
		return  status;

	pdev = (struct voltage_device_pwm *)pboard_obj;
	pset = (struct nv_pmu_volt_volt_device_pwm_boardobj_set *)ppmudata;

	pset->raw_period = pdev->raw_period;
	pset->voltage_base_uv = pdev->voltage_base_uv;
	pset->voltage_offset_scale_uv = pdev->voltage_offset_scale_uv;
	pset->pwm_source = pdev->source;

	return status;
}

u32 construct_volt_device(struct gk20a *g,
	struct boardobj **ppboardobj, u16 size, void *pargs)
{
	struct voltage_device *ptmp_dev = (struct voltage_device *)pargs;
	struct voltage_device *pvolt_dev = NULL;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj, size, pargs);
	if (status)
		return status;

	pvolt_dev = (struct voltage_device *)*ppboardobj;

	pvolt_dev->volt_domain = ptmp_dev->volt_domain;
	pvolt_dev->i2c_dev_idx = ptmp_dev->i2c_dev_idx;
	pvolt_dev->switch_delay_us = ptmp_dev->switch_delay_us;
	pvolt_dev->rsvd_0 = VOLTAGE_DESCRIPTOR_TABLE_ENTRY_INVALID;
	pvolt_dev->rsvd_1 =
			VOLTAGE_DESCRIPTOR_TABLE_ENTRY_INVALID;
	pvolt_dev->operation_type = ptmp_dev->operation_type;
	pvolt_dev->voltage_min_uv = ptmp_dev->voltage_min_uv;
	pvolt_dev->voltage_max_uv = ptmp_dev->voltage_max_uv;

	pvolt_dev->super.pmudatainit = volt_device_pmu_data_init_super;

	return status;
}

u32 construct_pwm_volt_device(struct gk20a *g, struct boardobj **ppboardobj,
		u16 size, void *pargs)
{
	struct boardobj *pboard_obj = NULL;
	struct voltage_device_pwm *ptmp_dev =
			(struct voltage_device_pwm *)pargs;
	struct voltage_device_pwm *pdev = NULL;
	u32 status = 0;

	status = construct_volt_device(g, ppboardobj, size, pargs);
	if (status)
		return status;

	pboard_obj = (*ppboardobj);
	pdev  = (struct voltage_device_pwm *)*ppboardobj;

	pboard_obj->pmudatainit  = volt_device_pmu_data_init_pwm;

	/* Set VOLTAGE_DEVICE_PWM-specific parameters */
	pdev->voltage_base_uv = ptmp_dev->voltage_base_uv;
	pdev->voltage_offset_scale_uv = ptmp_dev->voltage_offset_scale_uv;
	pdev->source = ptmp_dev->source;
	pdev->raw_period = ptmp_dev->raw_period;

	return status;
}


struct voltage_device_entry *volt_dev_construct_dev_entry_pwm(struct gk20a *g,
		u32 voltage_uv, void *pargs)
{
	struct voltage_device_pwm_entry *pentry = NULL;
	struct voltage_device_pwm_entry *ptmp_entry =
			(struct voltage_device_pwm_entry *)pargs;

	pentry = kzalloc(sizeof(struct voltage_device_pwm_entry), GFP_KERNEL);
	if (pentry == NULL)
		return NULL;

	memset(pentry, 0, sizeof(struct voltage_device_pwm_entry));

	pentry->super.voltage_uv = voltage_uv;
	pentry->duty_cycle = ptmp_entry->duty_cycle;

	return (struct voltage_device_entry *)pentry;
}

static u8 volt_dev_operation_type_convert(u8 vbios_type)
{
	switch (vbios_type) {
	case NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_DEFAULT:
		return CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT;

	case NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_LPWR_STEADY_STATE:
		return CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_STEADY_STATE;

	case NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_LPWR_SLEEP_STATE:
		return CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_SLEEP_STATE;
	}

	return CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID;
}

struct voltage_device *volt_volt_device_construct(struct gk20a *g,
		void *pargs)
{
	struct boardobj *pboard_obj = NULL;

	if (BOARDOBJ_GET_TYPE(pargs) == CTRL_VOLT_DEVICE_TYPE_PWM) {
		u32 status = construct_pwm_volt_device(g, &pboard_obj,
				sizeof(struct voltage_device_pwm), pargs);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				" Could not allocate memory for VOLTAGE_DEVICE type (%x).",
				BOARDOBJ_GET_TYPE(pargs));
			pboard_obj = NULL;
		}
	}

	return (struct voltage_device *)pboard_obj;
}

static u32 volt_get_voltage_device_table_1x_psv(struct gk20a *g,
		struct vbios_voltage_device_table_1x_entry *p_bios_entry,
		struct voltage_device_metadata *p_Volt_Device_Meta_Data,
		u8 entry_Idx)
{
	u32 status = 0;
	u32 entry_cnt = 0;
	struct voltage_device *pvolt_dev = NULL;
	struct voltage_device_pwm *pvolt_dev_pwm = NULL;
	struct voltage_device_pwm *ptmp_dev = NULL;
	u32 duty_cycle;
	u32 frequency_hz;
	u32 voltage_uv;
	u8 ext_dev_idx;
	u8 steps;
	u8 volt_domain = 0;
	struct voltage_device_pwm_entry pwm_entry = { { 0 } };

	ptmp_dev = kzalloc(sizeof(struct voltage_device_pwm), GFP_KERNEL);
	if (ptmp_dev == NULL)
		return -ENOMEM;

	frequency_hz = (u32)BIOS_GET_FIELD(p_bios_entry->param0,
		NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_INPUT_FREQUENCY);

	ext_dev_idx = (u8)BIOS_GET_FIELD(p_bios_entry->param0,
		NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_EXT_DEVICE_INDEX);

	ptmp_dev->super.operation_type = volt_dev_operation_type_convert(
			(u8)BIOS_GET_FIELD(p_bios_entry->param1,
			NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE));

	if (ptmp_dev->super.operation_type ==
			CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID) {
		gk20a_err(dev_from_gk20a(g),
			" Invalid Voltage Device Operation Type.");

		status = -EINVAL;
		goto done;
	}

	ptmp_dev->super.voltage_min_uv =
		(u32)BIOS_GET_FIELD(p_bios_entry->param1,
			NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_VOLTAGE_MINIMUM);

	ptmp_dev->super.voltage_max_uv =
		(u32)BIOS_GET_FIELD(p_bios_entry->param2,
			NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_VOLTAGE_MAXIMUM);

	ptmp_dev->voltage_base_uv = BIOS_GET_FIELD(p_bios_entry->param3,
		NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_BASE);

	steps = (u8)BIOS_GET_FIELD(p_bios_entry->param3,
		NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_STEPS);
	if (steps == VOLT_DEV_PWM_VOLTAGE_STEPS_INVALID)
		steps = VOLT_DEV_PWM_VOLTAGE_STEPS_DEFAULT;

	ptmp_dev->voltage_offset_scale_uv =
			BIOS_GET_FIELD(p_bios_entry->param4,
				NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_OFFSET_SCALE);

	volt_domain = volt_rail_vbios_volt_domain_convert_to_internal(g,
		(u8)p_bios_entry->volt_domain);
	if (volt_domain == CTRL_VOLT_DOMAIN_INVALID) {
		gk20a_err(dev_from_gk20a(g),
			"invalid voltage domain = %d",
			(u8)p_bios_entry->volt_domain);
		status = -EINVAL;
		goto done;
	}

	if (ptmp_dev->super.operation_type ==
			CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT) {
		if (volt_domain == CTRL_VOLT_DOMAIN_LOGIC)
			ptmp_dev->source =
				NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0;
		if (volt_domain == CTRL_VOLT_DOMAIN_SRAM)
			ptmp_dev->source =
				NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_1;
		ptmp_dev->raw_period =
			g->ops.clk.get_crystal_clk_hz(g) / frequency_hz;
	} else if (ptmp_dev->super.operation_type ==
		CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_STEADY_STATE) {
		ptmp_dev->source = NV_PMU_PMGR_PWM_SOURCE_RSVD_0;
		ptmp_dev->raw_period = 0;
	} else if (ptmp_dev->super.operation_type ==
		CTRL_VOLT_DEVICE_OPERATION_TYPE_LPWR_SLEEP_STATE) {
		ptmp_dev->source = NV_PMU_PMGR_PWM_SOURCE_RSVD_1;
		ptmp_dev->raw_period = 0;
	}

	/* Initialize data for parent class. */
	ptmp_dev->super.super.type = CTRL_VOLT_DEVICE_TYPE_PWM;
	ptmp_dev->super.volt_domain = volt_domain;
	ptmp_dev->super.i2c_dev_idx = ext_dev_idx;
	ptmp_dev->super.switch_delay_us = (u16)p_bios_entry->settle_time_us;

	pvolt_dev = volt_volt_device_construct(g, ptmp_dev);
	if (pvolt_dev == NULL) {
		gk20a_err(dev_from_gk20a(g),
			" Failure to construct VOLTAGE_DEVICE object.");

		status = -EINVAL;
		goto done;
	}

	status = boardobjgrp_objinsert(
				&p_Volt_Device_Meta_Data->volt_devices.super,
				(struct boardobj *)pvolt_dev, entry_Idx);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"could not add VOLTAGE_DEVICE for entry %d into boardobjgrp ",
			entry_Idx);
		goto done;
	}

	pvolt_dev_pwm = (struct voltage_device_pwm *)pvolt_dev;

	duty_cycle = 0;
	do {
		voltage_uv = (u32)(pvolt_dev_pwm->voltage_base_uv +
			(s32)((((s64)((s32)duty_cycle)) *
			pvolt_dev_pwm->voltage_offset_scale_uv)
			/ ((s64)((s32) pvolt_dev_pwm->raw_period))));

		/* Skip creating entry for invalid voltage. */
		if ((voltage_uv >= pvolt_dev_pwm->super.voltage_min_uv) &&
			(voltage_uv <= pvolt_dev_pwm->super.voltage_max_uv)) {
			if (pvolt_dev_pwm->voltage_offset_scale_uv < 0)
				pwm_entry.duty_cycle =
					pvolt_dev_pwm->raw_period - duty_cycle;
			else
				pwm_entry.duty_cycle = duty_cycle;

			/* Check if there is room left in the voltage table. */
			if (entry_cnt == VOLTAGE_TABLE_MAX_ENTRIES) {
				gk20a_err(dev_from_gk20a(g), "Voltage table is full");
				status = -EINVAL;
				goto done;
			}

			pvolt_dev->pentry[entry_cnt] =
				volt_dev_construct_dev_entry_pwm(g,
					voltage_uv, &pwm_entry);
			if (pvolt_dev->pentry[entry_cnt] == NULL) {
				gk20a_err(dev_from_gk20a(g),
					" Error creating voltage_device_pwm_entry!");
				status = -EINVAL;
				goto done;
			}

			entry_cnt++;
		}

		/* Obtain next value after the specified steps. */
		duty_cycle = duty_cycle + (u32)steps;

		/* Cap duty cycle to PWM period. */
		if (duty_cycle > pvolt_dev_pwm->raw_period)
			duty_cycle = pvolt_dev_pwm->raw_period;

	} while (duty_cycle < pvolt_dev_pwm->raw_period);

done:
	if (pvolt_dev != NULL)
		pvolt_dev->num_entries = entry_cnt;

	kfree(ptmp_dev);
	return status;
}

static u32 volt_get_volt_devices_table(struct gk20a *g,
		struct voltage_device_metadata *pvolt_device_metadata)
{
	u32 status = 0;
	u8 *volt_device_table_ptr = NULL;
	struct vbios_voltage_device_table_1x_header header = { 0 };
	struct vbios_voltage_device_table_1x_entry entry  = { 0 };
	u8 entry_idx;
	u8 *entry_offset;

	if (g->ops.bios.get_perf_table_ptrs) {
		volt_device_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, VOLTAGE_DEVICE_TABLE);
		if (volt_device_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	} else {
		status = -EINVAL;
		goto done;
	}

	memcpy(&header, volt_device_table_ptr,
			sizeof(struct vbios_voltage_device_table_1x_header));

	/* Read in the entries. */
	for (entry_idx = 0; entry_idx < header.num_table_entries; entry_idx++) {
		entry_offset = (volt_device_table_ptr + header.header_size +
					(entry_idx * header.table_entry_size));

		memcpy(&entry, entry_offset,
			sizeof(struct vbios_voltage_device_table_1x_entry));

		if (entry.type == NV_VBIOS_VOLTAGE_DEVICE_1X_ENTRY_TYPE_PSV)
			status = volt_get_voltage_device_table_1x_psv(g,
					&entry, pvolt_device_metadata,
					entry_idx);
	}

done:
	return status;
}

static u32 _volt_device_devgrp_pmudata_instget(struct gk20a *g,
	struct nv_pmu_boardobjgrp *pmuboardobjgrp,
	struct nv_pmu_boardobj **ppboardobjpmudata, u8 idx)
{
	struct nv_pmu_volt_volt_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_volt_volt_device_boardobj_grp_set *)
		pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.board_obj;
	gk20a_dbg_info("Done");
	return 0;
}

static u32 _volt_device_devgrp_pmustatus_instget(struct gk20a *g,
	void *pboardobjgrppmu,
	struct nv_pmu_boardobj_query **ppboardobjpmustatus, u8 idx)
{
	struct nv_pmu_volt_volt_device_boardobj_grp_get_status *pgrp_get_status
		= (struct nv_pmu_volt_volt_device_boardobj_grp_get_status *)
			pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_get_status->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmustatus = (struct nv_pmu_boardobj_query *)
			&pgrp_get_status->objects[idx].data.board_obj;
	return 0;
}

static int volt_device_volt_cmp(const void *a, const void *b)
{
	const struct voltage_device_entry *a_entry = *(const struct voltage_device_entry **)a;
	const struct voltage_device_entry *b_entry = *(const struct voltage_device_entry **)b;

	return (int)a_entry->voltage_uv - (int)b_entry->voltage_uv;
}

u32 volt_device_state_init(struct gk20a *g, struct voltage_device *pvolt_dev)
{
	u32 status = 0;
	struct voltage_rail *pRail = NULL;
	u8 rail_idx = 0;

	sort(pvolt_dev->pentry, pvolt_dev->num_entries,
	     sizeof(*pvolt_dev->pentry), volt_device_volt_cmp,
	     NULL);

	/* Initialize VOLT_DEVICE step size. */
	if (pvolt_dev->num_entries <= VOLTAGE_TABLE_MAX_ENTRIES_ONE)
		pvolt_dev->volt_step_uv = NV_PMU_VOLT_VALUE_0V_IN_UV;
	else
		pvolt_dev->volt_step_uv = (pvolt_dev->pentry[1]->voltage_uv -
				pvolt_dev->pentry[0]->voltage_uv);

	/* Build VOLT_RAIL SW state from VOLT_DEVICE SW state. */
	/* If VOLT_RAIL isn't supported, exit. */
	if (VOLT_RAIL_VOLT_3X_SUPPORTED(&g->perf_pmu.volt)) {
		rail_idx = volt_rail_volt_domain_convert_to_idx(g,
				pvolt_dev->volt_domain);
		if (rail_idx == CTRL_BOARDOBJ_IDX_INVALID) {
			gk20a_err(dev_from_gk20a(g),
				" could not convert voltage domain to rail index.");
			status = -EINVAL;
			goto done;
		}

		pRail = VOLT_GET_VOLT_RAIL(&g->perf_pmu.volt, rail_idx);
		if (pRail == NULL) {
			gk20a_err(dev_from_gk20a(g),
				"could not obtain ptr to rail object from rail index");
			status = -EINVAL;
			goto done;
		}

		status = volt_rail_volt_dev_register(g, pRail,
			BOARDOBJ_GET_IDX(pvolt_dev), pvolt_dev->operation_type);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"Failed to register the device with rail obj");
			goto done;
		}
	}

done:
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"Error in building rail sw state device sw");

	return status;
}

u32 volt_dev_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->perf_pmu.volt.volt_dev_metadata.volt_devices.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

u32 volt_dev_sw_setup(struct gk20a *g)
{
	u32 status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct voltage_device *pvolt_device;
	u8 i;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e32(&g->perf_pmu.volt.volt_dev_metadata.
			volt_devices);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for volt rail, status - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &g->perf_pmu.volt.volt_dev_metadata.volt_devices.super;

	pboardobjgrp->pmudatainstget  = _volt_device_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _volt_device_devgrp_pmustatus_instget;

	/* Obtain Voltage Rail Table from VBIOS */
	status = volt_get_volt_devices_table(g, &g->perf_pmu.volt.
			volt_dev_metadata);
	if (status)
		goto done;

	/* Populate data for the VOLT_RAIL PMU interface */
	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, VOLT, VOLT_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			volt, VOLT, volt_device, VOLT_DEVICE);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
			&g->perf_pmu.volt.volt_dev_metadata.volt_devices.super,
			volt, VOLT, volt_device, VOLT_DEVICE);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	/* update calibration to fuse */
	BOARDOBJGRP_FOR_EACH(&(g->perf_pmu.volt.volt_dev_metadata.volt_devices.
			       super),
			     struct voltage_device *, pvolt_device, i) {
		status = volt_device_state_init(g, pvolt_device);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"failure while executing devices's state init interface");
			gk20a_err(dev_from_gk20a(g),
				" railIdx = %d, status = 0x%x", i, status);
			goto done;
		}
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

