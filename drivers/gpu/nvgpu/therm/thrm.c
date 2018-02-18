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
#include "thrm.h"
#include "thrmpmu.h"

u32 therm_domain_sw_setup(struct gk20a *g)
{
	u32 status;

	status = therm_device_sw_setup(g);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for therm devices, status - 0x%x",
			status);
		goto exit;
	}

	status = therm_channel_sw_setup(g);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for therm channel, status - 0x%x",
			status);
		goto exit;
	}

exit:
	return status;
}

u32 therm_domain_pmu_setup(struct gk20a *g)
{
	return therm_send_pmgr_tables_to_pmu(g);
}
