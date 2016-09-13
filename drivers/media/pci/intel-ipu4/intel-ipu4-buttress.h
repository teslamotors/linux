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

#ifndef INTEL_IPU4_BUTTRESS_H
#define INTEL_IPU4_BUTTRESS_H

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include "intel-ipu4.h"

#define INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS	3
#define INTEL_IPU4_BUTTRESS_NUM_OF_PLL_CKS	3
#define INTEL_IPU4_BUTTRESS_TSC_CLK		19200000

#define BUTTRESS_IRQS		(BUTTRESS_ISR_SAI_VIOLATION |		\
				 BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING |	\
				 BUTTRESS_ISR_IPC_FROM_ISH_IS_WAITING |	\
				 BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE |	\
				 BUTTRESS_ISR_IPC_EXEC_DONE_BY_ISH |	\
				 BUTTRESS_ISR_IS_IRQ |			\
				 BUTTRESS_ISR_PS_IRQ)

struct intel_ipu4_buttress_ctrl {
	u32 freq_ctl, pwr_sts_shift, pwr_sts_mask, pwr_sts_on,
		pwr_sts_off;
	unsigned int divisor;
	unsigned int qos_floor;
	bool started;
};

struct intel_ipu4_buttress_fused_freqs {
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int efficient_freq;
};

struct intel_ipu4_buttress_ipc {
	struct completion send_complete;
	struct completion recv_complete;
	u32 nack;
	u32 nack_mask;
	u32 recv_data;
	u32 csr_out;
	u32 csr_in;
	u32 db0_in;
	u32 db0_out;
	u32 data0_out;
	u32 data0_in;
};

struct intel_ipu4_buttress {
	struct mutex power_mutex, auth_mutex, cons_mutex, ipc_mutex;
	spinlock_t tsc_lock;
	struct clk *clk_sensor[INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS];
	struct clk *pll_sensor[INTEL_IPU4_BUTTRESS_NUM_OF_PLL_CKS];
	struct intel_ipu4_buttress_ipc cse;
	struct intel_ipu4_buttress_ipc ish;
	struct list_head constraints;
	struct intel_ipu4_buttress_fused_freqs psys_fused_freqs;
	unsigned int psys_min_freq;
	u32 wdt_cached_value;
	u8 psys_force_ratio;
	bool force_suspend;
	bool ps_started;
};

struct intel_ipu4_buttress_sensor_clk_freq {
	unsigned int rate;
	unsigned int val;
};

struct firmware;

enum intel_ipu4_buttress_ipc_domain {
	INTEL_IPU4_BUTTRESS_IPC_CSE,
	INTEL_IPU4_BUTTRESS_IPC_ISH,
};

struct intel_ipu4_buttress_constraint {
	struct list_head list;
	unsigned int min_freq;
};

struct intel_ipu4_ipc_buttress_bulk_msg {
	u32 cmd;
	u32 expected_resp;
	bool require_resp;
	u8 cmd_size;
};

int intel_ipu4_buttress_ipc_reset(struct intel_ipu4_device *isp,
				  struct intel_ipu4_buttress_ipc *ipc);
int intel_ipu4_buttress_map_fw_image(struct intel_ipu4_bus_device *sys,
				     const struct firmware *fw,
				     struct sg_table *sgt);
int intel_ipu4_buttress_unmap_fw_image(struct intel_ipu4_bus_device *sys,
				    struct sg_table *sgt);
int intel_ipu4_buttress_power(struct device *dev,
			      struct intel_ipu4_buttress_ctrl *ctrl, bool on);
void intel_ipu4_buttress_add_psys_constraint(
	struct intel_ipu4_device *isp,
	struct intel_ipu4_buttress_constraint *constraint);
void intel_ipu4_buttress_remove_psys_constraint(
	struct intel_ipu4_device *isp,
	struct intel_ipu4_buttress_constraint *constraint);
void intel_ipu4_buttress_set_secure_mode(struct intel_ipu4_device *isp);
bool intel_ipu4_buttress_get_secure_mode(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_authenticate(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_reset_authentication(struct intel_ipu4_device *isp);
bool intel_ipu4_buttress_auth_done(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_start_tsc_sync(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_tsc_read(struct intel_ipu4_device *isp, u64 *val);
u64 intel_ipu4_buttress_tsc_ticks_to_ns(u64 ticks);

irqreturn_t intel_ipu4_buttress_isr(int irq, void *isp_ptr);
irqreturn_t intel_ipu4_buttress_isr_threaded(int irq, void *isp_ptr);
int intel_ipu4_buttress_debugfs_init(struct intel_ipu4_device *isp);
int intel_ipu4_buttress_init(struct intel_ipu4_device *isp);
void intel_ipu4_buttress_exit(struct intel_ipu4_device *isp);
void intel_ipu4_buttress_csi_port_config(struct intel_ipu4_device *isp,
					u32 legacy, u32 combo);
int intel_ipu4_buttress_restore(struct intel_ipu4_device *isp);

int intel_ipu4_buttress_ipc_send_bulk(
	struct intel_ipu4_device *isp,
	enum intel_ipu4_buttress_ipc_domain ipc_domain,
	struct intel_ipu4_ipc_buttress_bulk_msg *msgs,
	u32 size);
#endif /* INTEL_IPU4_BUTTRESS_H */
