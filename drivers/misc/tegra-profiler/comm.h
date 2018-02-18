/*
 * drivers/misc/tegra-profiler/comm.h
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
 */

#ifndef __QUADD_COMM_H__
#define __QUADD_COMM_H__

#include <linux/types.h>

struct quadd_record_data;
struct quadd_comm_cap;
struct quadd_module_state;
struct miscdevice;
struct quadd_parameters;
struct quadd_sections;
struct quadd_unwind_ctx;
struct quadd_ring_buffer;

struct quadd_iovec {
	void *base;
	size_t len;
};

enum {
	QUADD_MMAP_TYPE_NONE = 1,
	QUADD_MMAP_TYPE_EXTABS,
	QUADD_MMAP_TYPE_RB,
};

struct quadd_mmap_area {
	int type;

	struct vm_area_struct *mmap_vma;
	void *data;

	struct list_head list;
	struct list_head ex_entries;

	struct quadd_ring_buffer *rb;
};

struct quadd_comm_control_interface {
	int (*start)(void);
	void (*stop)(void);
	int (*set_parameters)(struct quadd_parameters *param);
	int (*set_parameters_for_cpu)(struct quadd_pmu_setup_for_cpu *param);
	void (*get_capabilities)(struct quadd_comm_cap *cap);
	void (*get_capabilities_for_cpu)(int cpuid,
					 struct quadd_comm_cap_for_cpu *cap);
	void (*get_state)(struct quadd_module_state *state);
	int (*set_extab)(struct quadd_sections *extabs,
			 struct quadd_mmap_area *mmap);
	void (*delete_mmap)(struct quadd_mmap_area *mmap);
};

struct quadd_comm_data_interface {
	ssize_t (*put_sample)(struct quadd_record_data *data,
			      struct quadd_iovec *vec,
			      int vec_count, int cpu_id);
	void (*reset)(void);
	int (*is_active)(void);
};

struct quadd_comm_data_interface *
quadd_comm_events_init(struct quadd_comm_control_interface *control);
void quadd_comm_events_exit(void);

#endif	/* __QUADD_COMM_H__ */
