/*
 * arch/arm64/mach-tegra/include/mach/nvdumper-footprint.h
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_TEGRA_NVFOOTPRINT_H
#define __MACH_TEGRA_NVFOOTPRINT_H

/*
 * The below debug footprint structure is used to
 * save debug information to.
 */
#define DBG_FOOTPRINT_NAME_LEN	16
struct dbg_footprint_element_cpu {
	volatile char name[DBG_FOOTPRINT_NAME_LEN];
	volatile uint32_t data[NR_CPUS];/* NR_CPUS is defined in config file. */
};

struct dbg_footprint_element_default {
	volatile char name[DBG_FOOTPRINT_NAME_LEN];
	volatile uint32_t data[1];
};

#define DECLARE_DBG_FOOTPRINT_DATA_CPU(name) \
	struct dbg_footprint_element_cpu name
#define DECLARE_DBG_FOOTPRINT_DATA(name) \
	struct dbg_footprint_element_default name

struct dbg_footprint_data_cpu {
	/* CPU related */
	DECLARE_DBG_FOOTPRINT_DATA_CPU(kernel_footprint_cpu);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(exit_counter_from_cpu);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(reset_vector_for_cpu);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_reset_vector_address);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_reset_vector_address_value);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_frequency);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(acpuclk_set_rate_footprint_cpu);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_prev_frequency);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_new_frequency);
	DECLARE_DBG_FOOTPRINT_DATA_CPU(cpu_hotplug_on);
};

struct dbg_footprint_data_soc {
	/* SOC */
	DECLARE_DBG_FOOTPRINT_DATA(emc_frequency);
	DECLARE_DBG_FOOTPRINT_DATA(lp_state_current);
	DECLARE_DBG_FOOTPRINT_DATA(lp_state_prev);
	DECLARE_DBG_FOOTPRINT_DATA(lp_state_next);
	DECLARE_DBG_FOOTPRINT_DATA(core_voltage);
	DECLARE_DBG_FOOTPRINT_DATA(soc_voltage);
	DECLARE_DBG_FOOTPRINT_DATA(gpu_voltage);
	DECLARE_DBG_FOOTPRINT_DATA(mem_voltage);

	/* customized debug purpose */
	DECLARE_DBG_FOOTPRINT_DATA(debug_data);

	/* Please add more if necessary */
};

#define INIT_DBG_FOOTPRINT(var, element, stringname) do {\
		memset((void *) var.element.data, 0, \
			sizeof(var.element.data)); \
		if (strlen(stringname) < DBG_FOOTPRINT_NAME_LEN - 1) \
			strlcpy((char *) var.element.name, stringname, \
				DBG_FOOTPRINT_NAME_LEN - 1); \
		else \
			strlcpy((char *) var.element.name, "error in name", \
				DBG_FOOTPRINT_NAME_LEN - 1); \
	} while (0)

#define GET_DBG_FP_DATA_CPU(element, core) (dbp_fp_cpu.element.data[core])
#define SET_DBG_FP_DATA_CPU(element, core, value) \
	dbp_fp_cpu.element.data[core] = (uint32_t) value;
#define GET_DBG_FP_DATA_DEFAULT(element) (dbp_fp_soc.element.data[0])
#define SET_DBG_FP_DATA_DEFAULT(element, value) \
	dbp_fp_soc.element.data[0] = (uint32_t) value;

#define CPU_CORE_COUNT_LIMIT_CHECK(c) do { \
		if (unlikely(c >= num_possible_cpus())) { \
			pr_err("Only %d cores but try to access core %d\n", \
				num_possible_cpus(), c); \
			return; \
		} \
	} while (0)

void nvdumper_dbg_footprint_init(void);
void nvdumper_dbg_footprint_exit(void);
void dbg_set_kernel_footprint_cpu(int core, uint32_t value);
void dbg_set_exit_counter_from_cpu(int core, uint32_t value);
void dbg_set_reset_vector_for_cpu(int core, uint32_t value);
void dbg_set_cpu_reset_vector_address(int core, uint32_t value);
void dbg_set_cpu_frequency(int core, uint32_t value);
void dbg_set_acpuclk_set_rate_footprint_cpu(int core, uint32_t value);
void dbg_set_cpu_prev_frequency(int core, uint32_t value);
void dbg_set_cpu_new_frequency(int core, uint32_t value);
void dbg_set_cpu_hotplug_on(int core, uint32_t value);
void dbg_set_emc_frequency(uint32_t value);
void dbg_set_lp_state_current(uint32_t value);
void dbg_set_lp_state_prev(uint32_t value);
void dbg_set_lp_state_next(uint32_t value);
void dbg_set_core_voltage(uint32_t value);
void dbg_set_soc_voltage(uint32_t value);
void dbg_set_gpu_voltage(uint32_t value);
void dbg_set_mem_voltage(uint32_t value);
void dbg_set_debug_data(uint32_t value);

#endif
