/*
 * arch/arm64/mach-tegra/nvdumper-footprint.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/string.h>
#include <linux/cpumask.h>
#include <asm/ptrace.h>
#include "nvdumper-footprint.h"

static struct dbg_footprint_data_cpu dbp_fp_cpu;
static struct dbg_footprint_data_soc dbp_fp_soc;

void nvdumper_dbg_footprint_init(void)
{
	/* Set names */
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, kernel_footprint_cpu, "KernelFPCpu");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, exit_counter_from_cpu,
		"CpuExitCounter");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, reset_vector_for_cpu, "ResetVector");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_reset_vector_address,
		"RstVectorAddr");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_reset_vector_address_value,
		"RstVectAddrVal");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_frequency, "CpuFreq");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, acpuclk_set_rate_footprint_cpu,
		"acpuclk");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_prev_frequency, "CpuPrevFreq");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_new_frequency, "CpuNewFreq");
	INIT_DBG_FOOTPRINT(dbp_fp_cpu, cpu_hotplug_on, "CpuHotplug");

	INIT_DBG_FOOTPRINT(dbp_fp_soc, emc_frequency, "EmcFreq");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, lp_state_current, "LPStateCur");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, lp_state_prev, "LPStatePrev");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, lp_state_next, "LPStateNext");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, core_voltage, "CoreVol");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, soc_voltage, "SocVol");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, gpu_voltage, "GpuVol");
	INIT_DBG_FOOTPRINT(dbp_fp_soc, mem_voltage, "MemVol");

	INIT_DBG_FOOTPRINT(dbp_fp_soc, debug_data, "DebugData");
}

void nvdumper_dbg_footprint_exit(void)
{

}

void dbg_set_kernel_footprint_cpu(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(kernel_footprint_cpu, core, value);
}

void dbg_set_exit_counter_from_cpu(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(exit_counter_from_cpu, core, value);
}

void dbg_set_reset_vector_for_cpu(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(reset_vector_for_cpu, core, value);
}

void dbg_set_cpu_reset_vector_address(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(cpu_reset_vector_address, core, value);
}

void dbg_set_cpu_frequency(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(cpu_frequency, core, value);
}

void dbg_set_acpuclk_set_rate_footprint_cpu(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(acpuclk_set_rate_footprint_cpu, core, value);
}

void dbg_set_cpu_prev_frequency(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(cpu_prev_frequency, core, value);
}

void dbg_set_cpu_new_frequency(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(cpu_new_frequency, core, value);
}

void dbg_set_cpu_hotplug_on(int core, uint32_t value)
{
	CPU_CORE_COUNT_LIMIT_CHECK(core);

	SET_DBG_FP_DATA_CPU(cpu_hotplug_on, core, value);
}

void dbg_set_emc_frequency(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(emc_frequency, value);
}

void dbg_set_lp_state_current(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(lp_state_current, value);
}

void dbg_set_lp_state_prev(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(lp_state_prev, value);
}

void dbg_set_lp_state_next(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(lp_state_next, value);
}

void dbg_set_core_voltage(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(core_voltage, value);
}

void dbg_set_soc_voltage(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(soc_voltage, value);
}

void dbg_set_gpu_voltage(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(gpu_voltage, value);
}

void dbg_set_mem_voltage(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(mem_voltage, value);
}

void dbg_set_debug_data(uint32_t value)
{
	SET_DBG_FP_DATA_DEFAULT(debug_data, value);
}

