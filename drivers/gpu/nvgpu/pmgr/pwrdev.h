/*
 * general power device structures & definitions
 *
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
#ifndef _PWRDEV_H_
#define _PWRDEV_H_

#include "boardobj/boardobj.h"
#include "pmuif/gpmuifpmgr.h"
#include "ctrl/ctrlpmgr.h"

#define  PWRDEV_I2CDEV_DEVICE_INDEX_NONE  (0xFF)

#define  PWR_DEVICE_PROV_NUM_DEFAULT                                           1

struct pwr_device {
	struct boardobj super;
	u8 power_rail;
	u8 i2c_dev_idx;
	bool bIs_inforom_config;
	u32 power_corr_factor;
};

struct pwr_devices {
	struct boardobjgrp_e32 super;
};

struct pwr_device_ina3221 {
	struct pwr_device super;
	struct ctrl_pmgr_pwr_device_info_rshunt
		r_shuntm_ohm[NV_PMU_PMGR_PWR_DEVICE_INA3221_CH_NUM];
	u16 configuration;
	u16 mask_enable;
	u8 gpio_function;
	u16 curr_correct_m;
	s16 curr_correct_b;
} ;

u32 pmgr_device_sw_setup(struct gk20a *g);

#endif
