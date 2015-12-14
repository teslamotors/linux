/*
 * Copyright (c) 2013--2015 Intel Corporation.
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

#ifndef INTEL_IPU4_BUTTRESS_H
#define INTEL_IPU4_BUTTRESS_H

#include <linux/interrupt.h>
#include "intel-ipu4.h"

#define INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS	3

struct intel_ipu4_buttress_ctrl {
	u32 freq_ctl, pwr_sts_shift, pwr_sts_mask, pwr_sts_on,
		pwr_sts_off;
	unsigned int divisor;
	unsigned int qos_floor;
	bool started;
};

struct intel_ipu4_buttress {
	struct mutex power_mutex, auth_mutex;
	struct clk *clk_sensor[INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS];
	struct clk *pll_sensor;
	struct completion cse_ipc_complete;
	struct completion ish_ipc_complete;
	bool force_suspend;
	bool ps_started;
};

struct firmware;

enum intel_ipu4_buttress_ipc_domain {
	INTEL_IPU4_BUTTRESS_IPC_CSE,
	INTEL_IPU4_BUTTRESS_IPC_ISH,
};

int intel_ipu4_buttress_map_fw_image(struct intel_ipu4_bus_device *sys,
				     const struct firmware *fw,
				     struct sg_table *sgt);
int intel_ipu4_buttress_unmap_fw_image(struct intel_ipu4_bus_device *sys,
				    struct sg_table *sgt);
int intel_ipu4_buttress_power(struct device *dev,
			      struct intel_ipu4_buttress_ctrl *ctrl, bool on);
void intel_ipu4_buttress_set_psys_ratio(struct intel_ipu4_device *isp,
					unsigned int psys_divisor,
					unsigned int psys_qos_floor);
int intel_ipu4_buttress_authenticate(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_start_tsc_sync(struct intel_ipu4_device *isp);

irqreturn_t intel_ipu4_buttress_isr(int irq, void *isp_ptr);
irqreturn_t intel_ipu4_buttress_isr_threaded(int irq, void *isp_ptr);
int intel_ipu4_buttress_ipc_validity_protocol(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_debugfs_init(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_init(struct intel_ipu4_device *isp);
void intel_ipu4_buttress_exit(struct intel_ipu4_device *isp);
void intel_ipu4_buttress_csi_port_config(struct intel_ipu4_device *isp,
					u32 legacy, u32 combo);


#endif /* INTEL_IPU4_BUTTRESS_H */
