/*
 * GK20A PMU (aka. gPMU outside gk20a context)
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>	/* for mdelay */
#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>

#include "gk20a.h"
#include "gr_gk20a.h"
#include "semaphore_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_pwr_gk20a.h"
#include "hw_top_gk20a.h"
#include "nvgpu_common.h"

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "nvgpu_gpuid_t18x.h"
#endif

#define GK20A_PMU_UCODE_IMAGE	"gpmu_ucode.bin"

#define PMU_MEM_SCRUBBING_TIMEOUT_MAX 1000
#define PMU_MEM_SCRUBBING_TIMEOUT_DEFAULT 10

#define gk20a_dbg_pmu(fmt, arg...) \
	gk20a_dbg(gpu_dbg_pmu, fmt, ##arg)

static int gk20a_pmu_get_pg_stats(struct gk20a *g,
		u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data);
static void ap_callback_init_and_enable_ctrl(
		struct gk20a *g, struct pmu_msg *msg,
		void *param, u32 seq_desc, u32 status);

static int pmu_init_powergating(struct gk20a *g);

static u32 pmu_perfmon_cntr_sz_v0(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_perfmon_counter_v0);
}

static u32 pmu_perfmon_cntr_sz_v2(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_perfmon_counter_v2);
}

static void *get_perfmon_cntr_ptr_v2(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->perfmon_counter_v2);
}

static void *get_perfmon_cntr_ptr_v0(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->perfmon_counter_v0);
}

static void set_perfmon_cntr_ut_v2(struct pmu_gk20a *pmu, u16 ut)
{
	pmu->perfmon_counter_v2.upper_threshold = ut;
}

static void set_perfmon_cntr_ut_v0(struct pmu_gk20a *pmu, u16 ut)
{
	pmu->perfmon_counter_v0.upper_threshold = ut;
}

static void set_perfmon_cntr_lt_v2(struct pmu_gk20a *pmu, u16 lt)
{
	pmu->perfmon_counter_v2.lower_threshold = lt;
}

static void set_perfmon_cntr_lt_v0(struct pmu_gk20a *pmu, u16 lt)
{
	pmu->perfmon_counter_v0.lower_threshold = lt;
}

static void set_perfmon_cntr_valid_v2(struct pmu_gk20a *pmu, u8 valid)
{
	pmu->perfmon_counter_v2.valid = valid;
}

static void set_perfmon_cntr_valid_v0(struct pmu_gk20a *pmu, u8 valid)
{
	pmu->perfmon_counter_v0.valid = valid;
}

static void set_perfmon_cntr_index_v2(struct pmu_gk20a *pmu, u8 index)
{
	pmu->perfmon_counter_v2.index = index;
}

static void set_perfmon_cntr_index_v0(struct pmu_gk20a *pmu, u8 index)
{
	pmu->perfmon_counter_v0.index = index;
}

static void set_perfmon_cntr_group_id_v2(struct pmu_gk20a *pmu, u8 gid)
{
	pmu->perfmon_counter_v2.group_id = gid;
}

static void set_perfmon_cntr_group_id_v0(struct pmu_gk20a *pmu, u8 gid)
{
	pmu->perfmon_counter_v0.group_id = gid;
}

static u32 pmu_cmdline_size_v0(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v0);
}

static u32 pmu_cmdline_size_v1(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v1);
}

static u32 pmu_cmdline_size_v2(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v2);
}

static void set_pmu_cmdline_args_cpufreq_v2(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v2.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v2(struct pmu_gk20a *pmu, u32 val)
{
	pmu->args_v2.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v2(
			struct pmu_gk20a *pmu, u32 size)
{
	pmu->args_v2.falc_trace_size = size;
}

static void set_pmu_cmdline_args_falctracedmabase_v2(struct pmu_gk20a *pmu)
{
	pmu->args_v2.falc_trace_dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100;
}

static void set_pmu_cmdline_args_falctracedmaidx_v2(
			struct pmu_gk20a *pmu, u32 idx)
{
	pmu->args_v2.falc_trace_dma_idx = idx;
}


static void set_pmu_cmdline_args_falctracedmabase_v4(struct pmu_gk20a *pmu)
{
	pmu->args_v4.dma_addr.dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100;
	pmu->args_v4.dma_addr.dma_base1 = 0;
	pmu->args_v4.dma_addr.dma_offset = 0;
}

static u32 pmu_cmdline_size_v4(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v4);
}

static void set_pmu_cmdline_args_cpufreq_v4(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v4.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v4(struct pmu_gk20a *pmu, u32 val)
{
	pmu->args_v4.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v4(
			struct pmu_gk20a *pmu, u32 size)
{
	pmu->args_v4.falc_trace_size = size;
}
static void set_pmu_cmdline_args_falctracedmaidx_v4(
			struct pmu_gk20a *pmu, u32 idx)
{
	pmu->args_v4.falc_trace_dma_idx = idx;
}

static u32 pmu_cmdline_size_v5(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v5);
}

static void set_pmu_cmdline_args_cpufreq_v5(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v5.cpu_freq_hz = 204000000;
}
static void set_pmu_cmdline_args_secure_mode_v5(struct pmu_gk20a *pmu, u32 val)
{
	pmu->args_v5.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v5(
			struct pmu_gk20a *pmu, u32 size)
{
	pmu->args_v5.trace_buf.params |= (size & 0x0FFF);
}

static void set_pmu_cmdline_args_falctracedmabase_v5(struct pmu_gk20a *pmu)
{
	pmu->args_v5.trace_buf.address.lo = ((u32)pmu->trace_buf.gpu_va)/0x100;
	pmu->args_v5.trace_buf.address.hi = 0;
}

static void set_pmu_cmdline_args_falctracedmaidx_v5(
			struct pmu_gk20a *pmu, u32 idx)
{
	pmu->args_v5.trace_buf.params |= (idx << 24);
}

static u32 pmu_cmdline_size_v3(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_cmdline_args_v3);
}

static void set_pmu_cmdline_args_cpufreq_v3(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v3.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v3(struct pmu_gk20a *pmu, u32 val)
{
	pmu->args_v3.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v3(
			struct pmu_gk20a *pmu, u32 size)
{
	pmu->args_v3.falc_trace_size = size;
}

static void set_pmu_cmdline_args_falctracedmabase_v3(struct pmu_gk20a *pmu)
{
	pmu->args_v3.falc_trace_dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100;
}

static void set_pmu_cmdline_args_falctracedmaidx_v3(
			struct pmu_gk20a *pmu, u32 idx)
{
	pmu->args_v3.falc_trace_dma_idx = idx;
}

static void set_pmu_cmdline_args_cpufreq_v1(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v1.cpu_freq_hz = freq;
}
static void set_pmu_cmdline_args_secure_mode_v1(struct pmu_gk20a *pmu, u32 val)
{
	pmu->args_v1.secure_mode = val;
}

static void set_pmu_cmdline_args_falctracesize_v1(
			struct pmu_gk20a *pmu, u32 size)
{
	pmu->args_v1.falc_trace_size = size;
}

static bool find_hex_in_string(char *strings, struct gk20a *g, u32 *hex_pos)
{
	u32 i = 0, j = strlen(strings);
	for (; i < j; i++) {
		if (strings[i] == '%')
			if (strings[i + 1] == 'x' || strings[i + 1] == 'X') {
				*hex_pos = i;
				return true;
			}
	}
	*hex_pos = -1;
	return false;
}

static void printtrace(struct pmu_gk20a *pmu)
{
	u32 i = 0, j = 0, k, l, m, count;
	char *trace = pmu->trace_buf.cpu_va;
	char part_str[40], buf[0x40];
	u32 *trace1 = pmu->trace_buf.cpu_va;
	struct gk20a *g = gk20a_from_pmu(pmu);
	gk20a_err(dev_from_gk20a(g), "Dump pmutrace");
	for (i = 0; i < GK20A_PMU_TRACE_BUFSIZE; i += 0x40) {
		for (j = 0; j < 0x40; j++)
			if (trace1[(i / 4) + j])
				break;
		if (j == 0x40)
			return;
		count = scnprintf(buf, 0x40, "Index %x: ", trace1[(i / 4)]);
		l = 0;
		m = 0;
		while (find_hex_in_string((trace+i+20+m), g, &k)) {
			if (k >= 40)
				break;
			strncpy(part_str, (trace+i+20+m), k);
			part_str[k] = 0;
			count += scnprintf((buf + count), 0x40, "%s0x%x",
					part_str, trace1[(i / 4) + 1 + l]);
			l++;
			m += k + 2;
		}
		count += scnprintf((buf + count), 0x40, "%s", (trace+i+20+m));
		gk20a_err(dev_from_gk20a(g), "%s", buf);
	}
}

static void set_pmu_cmdline_args_falctracedmabase_v1(struct pmu_gk20a *pmu)
{
	pmu->args_v1.falc_trace_dma_base = ((u32)pmu->trace_buf.gpu_va)/0x100;
}

static void set_pmu_cmdline_args_falctracedmaidx_v1(
			struct pmu_gk20a *pmu, u32 idx)
{
	pmu->args_v1.falc_trace_dma_idx = idx;
}

static void set_pmu_cmdline_args_cpufreq_v0(struct pmu_gk20a *pmu, u32 freq)
{
	pmu->args_v0.cpu_freq_hz = freq;
}

static void *get_pmu_cmdline_args_ptr_v4(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v4);
}

static void *get_pmu_cmdline_args_ptr_v3(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v3);
}

static void *get_pmu_cmdline_args_ptr_v2(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v2);
}

static void *get_pmu_cmdline_args_ptr_v5(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v5);
}
static void *get_pmu_cmdline_args_ptr_v1(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v1);
}

static void *get_pmu_cmdline_args_ptr_v0(struct pmu_gk20a *pmu)
{
	return (void *)(&pmu->args_v0);
}

static u32 get_pmu_allocation_size_v3(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_allocation_v3);
}

static u32 get_pmu_allocation_size_v2(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_allocation_v2);
}

static u32 get_pmu_allocation_size_v1(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_allocation_v1);
}

static u32 get_pmu_allocation_size_v0(struct pmu_gk20a *pmu)
{
	return sizeof(struct pmu_allocation_v0);
}

static void set_pmu_allocation_ptr_v3(struct pmu_gk20a *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v3 **pmu_a_ptr =
		(struct pmu_allocation_v3 **)pmu_alloc_ptr;
	*pmu_a_ptr = (struct pmu_allocation_v3 *)assign_ptr;
}

static void set_pmu_allocation_ptr_v2(struct pmu_gk20a *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v2 **pmu_a_ptr =
		(struct pmu_allocation_v2 **)pmu_alloc_ptr;
	*pmu_a_ptr = (struct pmu_allocation_v2 *)assign_ptr;
}

static void set_pmu_allocation_ptr_v1(struct pmu_gk20a *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v1 **pmu_a_ptr =
		(struct pmu_allocation_v1 **)pmu_alloc_ptr;
	*pmu_a_ptr = (struct pmu_allocation_v1 *)assign_ptr;
}

static void set_pmu_allocation_ptr_v0(struct pmu_gk20a *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v0 **pmu_a_ptr =
		(struct pmu_allocation_v0 **)pmu_alloc_ptr;
	*pmu_a_ptr = (struct pmu_allocation_v0 *)assign_ptr;
}

static void pmu_allocation_set_dmem_size_v3(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v2(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v1(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v0(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v0 *pmu_a_ptr =
		(struct pmu_allocation_v0 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.size = size;
}

static u16 pmu_allocation_get_dmem_size_v3(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v2(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v1(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v0(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v0 *pmu_a_ptr =
		(struct pmu_allocation_v0 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.size;
}

static u32 pmu_allocation_get_dmem_offset_v3(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v2(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v1(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v0(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v0 *pmu_a_ptr =
		(struct pmu_allocation_v0 *)pmu_alloc_ptr;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v3(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	return &pmu_a_ptr->alloc.dmem.offset;
}

void *pmu_allocation_get_fb_addr_v3(
				struct pmu_gk20a *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	return (void *)&pmu_a_ptr->alloc.fb;
}

u32 pmu_allocation_get_fb_size_v3(
				struct pmu_gk20a *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	return sizeof(pmu_a_ptr->alloc.fb);
}

static u32 *pmu_allocation_get_dmem_offset_addr_v2(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v1(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v0(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v0 *pmu_a_ptr =
		(struct pmu_allocation_v0 *)pmu_alloc_ptr;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static void pmu_allocation_set_dmem_offset_v3(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v2(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v1(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v0(struct pmu_gk20a *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v0 *pmu_a_ptr =
		(struct pmu_allocation_v0 *)pmu_alloc_ptr;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v3(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v3));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v3(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)(&init_msg->v3);

	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v3(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)(&init_msg->v3);

	return init->sw_managed_area_size;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v2(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v2));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v2(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v2 *init =
		(struct pmu_init_msg_pmu_v2 *)(&init_msg->v1);
	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v2(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v2 *init =
		(struct pmu_init_msg_pmu_v2 *)(&init_msg->v1);
	return init->sw_managed_area_size;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v1(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v1));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v1(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);
	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v1(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);
	return init->sw_managed_area_size;
}

static void *get_pmu_msg_pmu_init_msg_ptr_v0(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v0));
}

static u16 get_pmu_init_msg_pmu_sw_mg_off_v0(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v0 *init =
		(struct pmu_init_msg_pmu_v0 *)(&init_msg->v0);
	return init->sw_managed_area_offset;
}

static u16 get_pmu_init_msg_pmu_sw_mg_size_v0(union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v0 *init =
		(struct pmu_init_msg_pmu_v0 *)(&init_msg->v0);
	return init->sw_managed_area_size;
}

static u32 get_pmu_perfmon_cmd_start_size_v3(void)
{
	return sizeof(struct pmu_perfmon_cmd_start_v3);
}

static u32 get_pmu_perfmon_cmd_start_size_v2(void)
{
	return sizeof(struct pmu_perfmon_cmd_start_v2);
}

static u32 get_pmu_perfmon_cmd_start_size_v1(void)
{
	return sizeof(struct pmu_perfmon_cmd_start_v1);
}

static u32 get_pmu_perfmon_cmd_start_size_v0(void)
{
	return sizeof(struct pmu_perfmon_cmd_start_v0);
}

static int get_perfmon_cmd_start_offsetofvar_v3(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_start_v3,
		counter_alloc);
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_perfmon_cmd_start_offsetofvar_v2(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_start_v2,
		counter_alloc);
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_perfmon_cmd_start_offsetofvar_v1(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_start_v1,
		counter_alloc);
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_perfmon_cmd_start_offsetofvar_v0(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_start_v0,
		counter_alloc);
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static u32 get_pmu_perfmon_cmd_init_size_v3(void)
{
	return sizeof(struct pmu_perfmon_cmd_init_v3);
}

static u32 get_pmu_perfmon_cmd_init_size_v2(void)
{
	return sizeof(struct pmu_perfmon_cmd_init_v2);
}

static u32 get_pmu_perfmon_cmd_init_size_v1(void)
{
	return sizeof(struct pmu_perfmon_cmd_init_v1);
}

static u32 get_pmu_perfmon_cmd_init_size_v0(void)
{
	return sizeof(struct pmu_perfmon_cmd_init_v0);
}

static int get_perfmon_cmd_init_offsetofvar_v3(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_init_v3,
		counter_alloc);
	default:
		return -EINVAL;
	}
	return 0;
}

static int get_perfmon_cmd_init_offsetofvar_v2(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_init_v2,
		counter_alloc);
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static int get_perfmon_cmd_init_offsetofvar_v1(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_init_v1,
		counter_alloc);
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static int get_perfmon_cmd_init_offsetofvar_v0(
	enum pmu_perfmon_cmd_start_fields field)
{
	switch (field) {
	case COUNTER_ALLOC:
		return offsetof(struct pmu_perfmon_cmd_init_v0,
		counter_alloc);
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static void perfmon_start_set_cmd_type_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->cmd_type = value;
}

static void perfmon_start_set_cmd_type_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;
	start->cmd_type = value;
}

static void perfmon_start_set_cmd_type_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;
	start->cmd_type = value;
}

static void perfmon_start_set_cmd_type_v0(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v0 *start = &pc->start_v0;
	start->cmd_type = value;
}

static void perfmon_start_set_group_id_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->group_id = value;
}

static void perfmon_start_set_group_id_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;
	start->group_id = value;
}

static void perfmon_start_set_group_id_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;
	start->group_id = value;
}

static void perfmon_start_set_group_id_v0(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v0 *start = &pc->start_v0;
	start->group_id = value;
}

static void perfmon_start_set_state_id_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->state_id = value;
}

static void perfmon_start_set_state_id_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;
	start->state_id = value;
}

static void perfmon_start_set_state_id_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;
	start->state_id = value;
}

static void perfmon_start_set_state_id_v0(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v0 *start = &pc->start_v0;
	start->state_id = value;
}

static void perfmon_start_set_flags_v3(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->flags = value;
}

static void perfmon_start_set_flags_v2(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;
	start->flags = value;
}

static void perfmon_start_set_flags_v1(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;
	start->flags = value;
}

static void perfmon_start_set_flags_v0(struct pmu_perfmon_cmd *pc, u8 value)
{
	struct pmu_perfmon_cmd_start_v0 *start = &pc->start_v0;
	start->flags = value;
}

static u8 perfmon_start_get_flags_v3(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	return start->flags;
}

static u8 perfmon_start_get_flags_v2(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;
	return start->flags;
}

static u8 perfmon_start_get_flags_v1(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;
	return start->flags;
}

static u8 perfmon_start_get_flags_v0(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v0 *start = &pc->start_v0;
	return start->flags;
}

static void perfmon_cmd_init_set_sample_buffer_v3(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_buffer = value;
}

static void perfmon_cmd_init_set_sample_buffer_v2(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->sample_buffer = value;
}


static void perfmon_cmd_init_set_sample_buffer_v1(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->sample_buffer = value;
}

static void perfmon_cmd_init_set_sample_buffer_v0(struct pmu_perfmon_cmd *pc,
	u16 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->sample_buffer = value;
}

static void perfmon_cmd_init_set_dec_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_dec_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_dec_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_dec_cnt_v0(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->to_decrease_count = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_base_cnt_id_v0(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->base_counter_id = value;
}

static void perfmon_cmd_init_set_samp_period_us_v3(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_samp_period_us_v2(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_samp_period_us_v1(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_samp_period_us_v0(struct pmu_perfmon_cmd *pc,
	u32 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->sample_period_us = value;
}

static void perfmon_cmd_init_set_num_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->num_counters = value;
}

static void perfmon_cmd_init_set_num_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->num_counters = value;
}

static void perfmon_cmd_init_set_num_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->num_counters = value;
}

static void perfmon_cmd_init_set_num_cnt_v0(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->num_counters = value;
}

static void perfmon_cmd_init_set_mov_avg_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->samples_in_moving_avg = value;
}

static void perfmon_cmd_init_set_mov_avg_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;
	init->samples_in_moving_avg = value;
}

static void perfmon_cmd_init_set_mov_avg_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;
	init->samples_in_moving_avg = value;
}

static void perfmon_cmd_init_set_mov_avg_v0(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v0 *init = &pc->init_v0;
	init->samples_in_moving_avg = value;
}

static void get_pmu_init_msg_pmu_queue_params_v0(struct pmu_queue *queue,
	u32 id, void *pmu_init_msg)
{
	struct pmu_init_msg_pmu_v0 *init =
		(struct pmu_init_msg_pmu_v0 *)pmu_init_msg;
	queue->index    = init->queue_info[id].index;
	queue->offset   = init->queue_info[id].offset;
	queue->size = init->queue_info[id].size;
}

static void get_pmu_init_msg_pmu_queue_params_v1(struct pmu_queue *queue,
	u32 id, void *pmu_init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)pmu_init_msg;
	queue->index    = init->queue_info[id].index;
	queue->offset   = init->queue_info[id].offset;
	queue->size = init->queue_info[id].size;
}

static void get_pmu_init_msg_pmu_queue_params_v2(struct pmu_queue *queue,
	u32 id, void *pmu_init_msg)
{
	struct pmu_init_msg_pmu_v2 *init =
		(struct pmu_init_msg_pmu_v2 *)pmu_init_msg;
	queue->index    = init->queue_info[id].index;
	queue->offset   = init->queue_info[id].offset;
	queue->size = init->queue_info[id].size;
}

static void get_pmu_init_msg_pmu_queue_params_v3(struct pmu_queue *queue,
	u32 id, void *pmu_init_msg)
{
	struct pmu_init_msg_pmu_v3 *init =
		(struct pmu_init_msg_pmu_v3 *)pmu_init_msg;
	u32 current_ptr = 0;
	u8 i;
	u8 tmp_id = id;

	if (tmp_id == PMU_COMMAND_QUEUE_HPQ)
		tmp_id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	else if (tmp_id == PMU_COMMAND_QUEUE_LPQ)
		tmp_id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	else if (tmp_id == PMU_MESSAGE_QUEUE)
		tmp_id = PMU_QUEUE_MSG_IDX_FOR_V3;
	else
		return;
	queue->index    = init->queue_index[tmp_id];
	queue->size = init->queue_size[tmp_id];
	if (tmp_id != 0) {
		for (i = 0 ; i < tmp_id; i++)
			current_ptr += init->queue_size[i];
	}
	queue->offset   = init->queue_offset + current_ptr;
}

static void *get_pmu_sequence_in_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v3);
}

static void *get_pmu_sequence_in_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v1);
}

static void *get_pmu_sequence_in_alloc_ptr_v0(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v0);
}

static void *get_pmu_sequence_out_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v3);
}

static void *get_pmu_sequence_out_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v1);
}

static void *get_pmu_sequence_out_alloc_ptr_v0(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v0);
}

static u8 pg_cmd_eng_buf_load_size_v0(struct pmu_pg_cmd *pg)
{
	return sizeof(pg->eng_buf_load_v0);
}

static u8 pg_cmd_eng_buf_load_size_v1(struct pmu_pg_cmd *pg)
{
	return sizeof(pg->eng_buf_load_v1);
}

static u8 pg_cmd_eng_buf_load_size_v2(struct pmu_pg_cmd *pg)
{
	return sizeof(pg->eng_buf_load_v2);
}

static void pg_cmd_eng_buf_load_set_cmd_type_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_cmd_type_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_cmd_type_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.cmd_type = value;
}

static void pg_cmd_eng_buf_load_set_engine_id_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_engine_id_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_engine_id_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.engine_id = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.buf_idx = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.buf_idx = value;
}
static void pg_cmd_eng_buf_load_set_buf_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.buf_idx = value;
}

static void pg_cmd_eng_buf_load_set_pad_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.pad = value;
}
static void pg_cmd_eng_buf_load_set_pad_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.pad = value;
}
static void pg_cmd_eng_buf_load_set_pad_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.pad = value;
}

static void pg_cmd_eng_buf_load_set_buf_size_v0(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v0.buf_size = value;
}
static void pg_cmd_eng_buf_load_set_buf_size_v1(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_size = value;
}
static void pg_cmd_eng_buf_load_set_buf_size_v2(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v2.dma_desc.params = value;
}

static void pg_cmd_eng_buf_load_set_dma_base_v0(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v0.dma_base = (value >> 8);
}
static void pg_cmd_eng_buf_load_set_dma_base_v1(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= u64_lo32(value);
	pg->eng_buf_load_v1.dma_desc.dma_addr.hi |= u64_hi32(value);
}
static void pg_cmd_eng_buf_load_set_dma_base_v2(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo = u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi = u64_lo32(value);
}

static void pg_cmd_eng_buf_load_set_dma_offset_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_offset = value;
}
static void pg_cmd_eng_buf_load_set_dma_offset_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= value;
}
static void pg_cmd_eng_buf_load_set_dma_offset_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo |= u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi |= u64_lo32(value);
}

static void pg_cmd_eng_buf_load_set_dma_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_idx = value;
}
static void pg_cmd_eng_buf_load_set_dma_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_idx = value;
}
static void pg_cmd_eng_buf_load_set_dma_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.params |= (value << 24);
}


int gk20a_init_pmu(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_v *pv = &g->ops.pmu_ver;

	mutex_init(&pmu->elpg_mutex);
	mutex_init(&pmu->pg_mutex);
	mutex_init(&pmu->isr_mutex);
	mutex_init(&pmu->pmu_copy_lock);
	mutex_init(&pmu->pmu_seq_lock);

	pmu->remove_support = gk20a_remove_pmu_support;

	switch (pmu->desc->app_version) {
	case APP_VERSION_NC_2:
	case APP_VERSION_NC_1:
	case APP_VERSION_NC_0:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v1;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v1;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 16;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v4;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v4;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v4;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v2;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v2;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v2;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v2;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v2;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v2;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v2;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v2;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v2;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v2;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v2;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v2;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v2;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v2;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v2;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	case APP_VERSION_GM206:
	case APP_VERSION_NV_GPU:
	case APP_VERSION_NV_GPU_1:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v2;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v2;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 16;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v5;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v5;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v5;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v3;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v3;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_addr =
				pmu_allocation_get_fb_addr_v3;
		g->ops.pmu_ver.pmu_allocation_get_fb_size =
				pmu_allocation_get_fb_size_v3;
		if(pmu->desc->app_version != APP_VERSION_NV_GPU &&
			pmu->desc->app_version != APP_VERSION_NV_GPU_1) {
			g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
					get_pmu_init_msg_pmu_queue_params_v2;
			g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
					get_pmu_msg_pmu_init_msg_ptr_v2;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
					get_pmu_init_msg_pmu_sw_mg_off_v2;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
					get_pmu_init_msg_pmu_sw_mg_size_v2;
		}
		else
		{
			g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
				get_pmu_init_msg_pmu_queue_params_v3;
			g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
				get_pmu_msg_pmu_init_msg_ptr_v3;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
				get_pmu_init_msg_pmu_sw_mg_off_v3;
			g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
				get_pmu_init_msg_pmu_sw_mg_size_v3;
		}
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v3;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v3;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v3;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v3;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v3;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v3;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v3;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v3;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v3;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v3;
		break;
	case APP_VERSION_GM20B_5:
	case APP_VERSION_GM20B_4:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v0;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 16;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v3;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v3;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v3;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v1;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v1;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v1;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v1;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v1;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v1;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	case APP_VERSION_GM20B_3:
	case APP_VERSION_GM20B_2:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v0;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v2;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v2;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v2;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v2;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v2;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v2;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 16;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v2;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v2;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v2;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v2;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v2;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v2;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v2;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v1;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v1;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v1;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v1;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v1;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v1;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	case APP_VERSION_GM20B_1:
	case APP_VERSION_GM20B:
	case APP_VERSION_1:
	case APP_VERSION_2:
	case APP_VERSION_3:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v0;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 16;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v0;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v0;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v0;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v0;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v0;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v0;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v0;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v1;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v1;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			set_pmu_cmdline_args_secure_mode_v1;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_size =
			set_pmu_cmdline_args_falctracesize_v1;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base =
			set_pmu_cmdline_args_falctracedmabase_v1;
		g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx =
			set_pmu_cmdline_args_falctracedmaidx_v1;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v1;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v1;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v1;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v1;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v1;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v1;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v1;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v1;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v1;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v1;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v1;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v1;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v1;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v1;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v1;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v1;
		break;
	case APP_VERSION_0:
		g->ops.pmu_ver.pg_cmd_eng_buf_load_size =
				pg_cmd_eng_buf_load_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type =
				pg_cmd_eng_buf_load_set_cmd_type_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id =
				pg_cmd_eng_buf_load_set_engine_id_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx =
				pg_cmd_eng_buf_load_set_buf_idx_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_pad =
				pg_cmd_eng_buf_load_set_pad_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size =
				pg_cmd_eng_buf_load_set_buf_size_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base =
				pg_cmd_eng_buf_load_set_dma_base_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset =
				pg_cmd_eng_buf_load_set_dma_offset_v0;
		g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx =
				pg_cmd_eng_buf_load_set_dma_idx_v0;
		g->ops.pmu_ver.cmd_id_zbc_table_update = 14;
		g->ops.pmu_ver.get_perfmon_cntr_ptr = get_perfmon_cntr_ptr_v0;
		g->ops.pmu_ver.set_perfmon_cntr_ut = set_perfmon_cntr_ut_v0;
		g->ops.pmu_ver.set_perfmon_cntr_lt = set_perfmon_cntr_lt_v0;
		g->ops.pmu_ver.set_perfmon_cntr_valid =
			set_perfmon_cntr_valid_v0;
		g->ops.pmu_ver.set_perfmon_cntr_index =
			set_perfmon_cntr_index_v0;
		g->ops.pmu_ver.set_perfmon_cntr_group_id =
			set_perfmon_cntr_group_id_v0;
		g->ops.pmu_ver.get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v0;
		g->ops.pmu_ver.get_pmu_cmdline_args_size =
			pmu_cmdline_size_v0;
		g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq =
			set_pmu_cmdline_args_cpufreq_v0;
		g->ops.pmu_ver.set_pmu_cmdline_args_secure_mode =
			NULL;
		g->ops.pmu_ver.get_pmu_cmdline_args_ptr =
			get_pmu_cmdline_args_ptr_v0;
		g->ops.pmu_ver.get_pmu_allocation_struct_size =
			get_pmu_allocation_size_v0;
		g->ops.pmu_ver.set_pmu_allocation_ptr =
			set_pmu_allocation_ptr_v0;
		g->ops.pmu_ver.pmu_allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v0;
		g->ops.pmu_ver.pmu_allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v0;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v0;
		g->ops.pmu_ver.pmu_allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v0;
		g->ops.pmu_ver.pmu_allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v0;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params =
			get_pmu_init_msg_pmu_queue_params_v0;
		g->ops.pmu_ver.get_pmu_msg_pmu_init_msg_ptr =
			get_pmu_msg_pmu_init_msg_ptr_v0;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_off =
			get_pmu_init_msg_pmu_sw_mg_off_v0;
		g->ops.pmu_ver.get_pmu_init_msg_pmu_sw_mg_size =
			get_pmu_init_msg_pmu_sw_mg_size_v0;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_start_size =
			get_pmu_perfmon_cmd_start_size_v0;
		g->ops.pmu_ver.get_perfmon_cmd_start_offsetofvar =
			get_perfmon_cmd_start_offsetofvar_v0;
		g->ops.pmu_ver.perfmon_start_set_cmd_type =
			perfmon_start_set_cmd_type_v0;
		g->ops.pmu_ver.perfmon_start_set_group_id =
			perfmon_start_set_group_id_v0;
		g->ops.pmu_ver.perfmon_start_set_state_id =
			perfmon_start_set_state_id_v0;
		g->ops.pmu_ver.perfmon_start_set_flags =
			perfmon_start_set_flags_v0;
		g->ops.pmu_ver.perfmon_start_get_flags =
			perfmon_start_get_flags_v0;
		g->ops.pmu_ver.get_pmu_perfmon_cmd_init_size =
			get_pmu_perfmon_cmd_init_size_v0;
		g->ops.pmu_ver.get_perfmon_cmd_init_offsetofvar =
			get_perfmon_cmd_init_offsetofvar_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_sample_buffer =
			perfmon_cmd_init_set_sample_buffer_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_dec_cnt =
			perfmon_cmd_init_set_dec_cnt_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_base_cnt_id =
			perfmon_cmd_init_set_base_cnt_id_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_samp_period_us =
			perfmon_cmd_init_set_samp_period_us_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_num_cnt =
			perfmon_cmd_init_set_num_cnt_v0;
		g->ops.pmu_ver.perfmon_cmd_init_set_mov_avg =
			perfmon_cmd_init_set_mov_avg_v0;
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr =
			get_pmu_sequence_in_alloc_ptr_v0;
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr =
			get_pmu_sequence_out_alloc_ptr_v0;
		break;
	default:
		gk20a_err(dev_from_gk20a(gk20a_from_pmu(pmu)),
		"PMU code version not supported version: %d\n",
			pmu->desc->app_version);
		return -EINVAL;
		break;
	}
	pv->set_perfmon_cntr_index(pmu, 3); /* GR & CE2 */
	pv->set_perfmon_cntr_group_id(pmu, PMU_DOMAIN_GROUP_PSTATE);

	return 0;
}

void pmu_copy_from_dmem(struct pmu_gk20a *pmu,
		u32 src, u8 *dst, u32 size, u8 port)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *dst_u32 = (u32*)dst;

	if (size == 0) {
		gk20a_err(dev_from_gk20a(g),
			"size is zero");
		return;
	}

	if (src & 0x3) {
		gk20a_err(dev_from_gk20a(g),
			"src (0x%08x) not 4-byte aligned", src);
		return;
	}

	mutex_lock(&pmu->pmu_copy_lock);

	words = size >> 2;
	bytes = size & 0x3;

	addr_mask = pwr_falcon_dmemc_offs_m() |
		    pwr_falcon_dmemc_blk_m();

	src &= addr_mask;

	gk20a_writel(g, pwr_falcon_dmemc_r(port),
		src | pwr_falcon_dmemc_aincr_f(1));

	for (i = 0; i < words; i++)
		dst_u32[i] = gk20a_readl(g, pwr_falcon_dmemd_r(port));

	if (bytes > 0) {
		data = gk20a_readl(g, pwr_falcon_dmemd_r(port));
		for (i = 0; i < bytes; i++) {
			dst[(words << 2) + i] = ((u8 *)&data)[i];
		}
	}
	mutex_unlock(&pmu->pmu_copy_lock);
	return;
}

void pmu_copy_to_dmem(struct pmu_gk20a *pmu,
		u32 dst, u8 *src, u32 size, u8 port)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *src_u32 = (u32*)src;

	if (size == 0) {
		gk20a_err(dev_from_gk20a(g),
			"size is zero");
		return;
	}

	if (dst & 0x3) {
		gk20a_err(dev_from_gk20a(g),
			"dst (0x%08x) not 4-byte aligned", dst);
		return;
	}

	mutex_lock(&pmu->pmu_copy_lock);

	words = size >> 2;
	bytes = size & 0x3;

	addr_mask = pwr_falcon_dmemc_offs_m() |
		    pwr_falcon_dmemc_blk_m();

	dst &= addr_mask;

	gk20a_writel(g, pwr_falcon_dmemc_r(port),
		dst | pwr_falcon_dmemc_aincw_f(1));

	for (i = 0; i < words; i++)
		gk20a_writel(g, pwr_falcon_dmemd_r(port), src_u32[i]);

	if (bytes > 0) {
		data = 0;
		for (i = 0; i < bytes; i++)
			((u8 *)&data)[i] = src[(words << 2) + i];
		gk20a_writel(g, pwr_falcon_dmemd_r(port), data);
	}

	data = gk20a_readl(g, pwr_falcon_dmemc_r(port)) & addr_mask;
	size = ALIGN(size, 4);
	if (data != ((dst + size) & addr_mask)) {
		gk20a_err(dev_from_gk20a(g),
			"copy failed. bytes written %d, expected %d",
			data - dst, size);
	}
	mutex_unlock(&pmu->pmu_copy_lock);
	return;
}

int pmu_idle(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(2000);
	u32 idle_stat;

	/* wait for pmu idle */
	do {
		idle_stat = gk20a_readl(g, pwr_falcon_idlestate_r());

		if (pwr_falcon_idlestate_falcon_busy_v(idle_stat) == 0 &&
		    pwr_falcon_idlestate_ext_busy_v(idle_stat) == 0) {
			break;
		}

		if (time_after_eq(jiffies, end_jiffies)) {
			gk20a_err(dev_from_gk20a(g),
				"timeout waiting pmu idle : 0x%08x",
				idle_stat);
			return -EBUSY;
		}
		usleep_range(100, 200);
	} while (1);

	gk20a_dbg_fn("done");
	return 0;
}

void pmu_enable_irq(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	gk20a_dbg_fn("");

	g->ops.mc.intr_unit_config(g, MC_INTR_UNIT_DISABLE, true,
			mc_intr_mask_0_pmu_enabled_f());
	g->ops.mc.intr_unit_config(g, MC_INTR_UNIT_DISABLE, false,
			mc_intr_mask_1_pmu_enabled_f());

	gk20a_writel(g, pwr_falcon_irqmclr_r(),
		pwr_falcon_irqmclr_gptmr_f(1)  |
		pwr_falcon_irqmclr_wdtmr_f(1)  |
		pwr_falcon_irqmclr_mthd_f(1)   |
		pwr_falcon_irqmclr_ctxsw_f(1)  |
		pwr_falcon_irqmclr_halt_f(1)   |
		pwr_falcon_irqmclr_exterr_f(1) |
		pwr_falcon_irqmclr_swgen0_f(1) |
		pwr_falcon_irqmclr_swgen1_f(1) |
		pwr_falcon_irqmclr_ext_f(0xff));

	if (enable) {
		/* dest 0=falcon, 1=host; level 0=irq0, 1=irq1 */
		gk20a_writel(g, pwr_falcon_irqdest_r(),
			pwr_falcon_irqdest_host_gptmr_f(0)    |
			pwr_falcon_irqdest_host_wdtmr_f(1)    |
			pwr_falcon_irqdest_host_mthd_f(0)     |
			pwr_falcon_irqdest_host_ctxsw_f(0)    |
			pwr_falcon_irqdest_host_halt_f(1)     |
			pwr_falcon_irqdest_host_exterr_f(0)   |
			pwr_falcon_irqdest_host_swgen0_f(1)   |
			pwr_falcon_irqdest_host_swgen1_f(0)   |
			pwr_falcon_irqdest_host_ext_f(0xff)   |
			pwr_falcon_irqdest_target_gptmr_f(1)  |
			pwr_falcon_irqdest_target_wdtmr_f(0)  |
			pwr_falcon_irqdest_target_mthd_f(0)   |
			pwr_falcon_irqdest_target_ctxsw_f(0)  |
			pwr_falcon_irqdest_target_halt_f(0)   |
			pwr_falcon_irqdest_target_exterr_f(0) |
			pwr_falcon_irqdest_target_swgen0_f(0) |
			pwr_falcon_irqdest_target_swgen1_f(0) |
			pwr_falcon_irqdest_target_ext_f(0xff));

		/* 0=disable, 1=enable */
		gk20a_writel(g, pwr_falcon_irqmset_r(),
			pwr_falcon_irqmset_gptmr_f(1)  |
			pwr_falcon_irqmset_wdtmr_f(1)  |
			pwr_falcon_irqmset_mthd_f(0)   |
			pwr_falcon_irqmset_ctxsw_f(0)  |
			pwr_falcon_irqmset_halt_f(1)   |
			pwr_falcon_irqmset_exterr_f(1) |
			pwr_falcon_irqmset_swgen0_f(1) |
			pwr_falcon_irqmset_swgen1_f(1));

		g->ops.mc.intr_unit_config(g, MC_INTR_UNIT_ENABLE, true,
				mc_intr_mask_0_pmu_enabled_f());
	}

	gk20a_dbg_fn("done");
}

int pmu_enable_hw(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	gk20a_dbg_fn("");

	if (enable) {
		int retries = PMU_MEM_SCRUBBING_TIMEOUT_MAX /
			      PMU_MEM_SCRUBBING_TIMEOUT_DEFAULT;
		gk20a_enable(g, mc_enable_pwr_enabled_f());

		if (g->ops.clock_gating.slcg_pmu_load_gating_prod)
			g->ops.clock_gating.slcg_pmu_load_gating_prod(g,
					g->slcg_enabled);
		if (g->ops.clock_gating.blcg_pmu_load_gating_prod)
			g->ops.clock_gating.blcg_pmu_load_gating_prod(g,
					g->blcg_enabled);

		do {
			u32 w = gk20a_readl(g, pwr_falcon_dmactl_r()) &
				(pwr_falcon_dmactl_dmem_scrubbing_m() |
				 pwr_falcon_dmactl_imem_scrubbing_m());

			if (!w) {
				gk20a_dbg_fn("done");
				return 0;
			}
			udelay(PMU_MEM_SCRUBBING_TIMEOUT_DEFAULT);
		} while (--retries || !tegra_platform_is_silicon());

		gk20a_disable(g, mc_enable_pwr_enabled_f());
		gk20a_err(dev_from_gk20a(g), "Falcon mem scrubbing timeout");

		return -ETIMEDOUT;
	} else {
		gk20a_disable(g, mc_enable_pwr_enabled_f());
		return 0;
	}
}

static int pmu_enable(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 pmc_enable;
	int err;

	gk20a_dbg_fn("");

	if (!enable) {
		pmc_enable = gk20a_readl(g, mc_enable_r());
		if (mc_enable_pwr_v(pmc_enable) !=
		    mc_enable_pwr_disabled_v()) {

			pmu_enable_irq(pmu, false);
			pmu_enable_hw(pmu, false);
		}
	} else {
		err = pmu_enable_hw(pmu, true);
		if (err)
			return err;

		/* TBD: post reset */

		err = pmu_idle(pmu);
		if (err)
			return err;

		pmu_enable_irq(pmu, true);
	}

	gk20a_dbg_fn("done");
	return 0;
}

int pmu_reset(struct pmu_gk20a *pmu)
{
	int err;

	err = pmu_idle(pmu);
	if (err)
		return err;

	/* TBD: release pmu hw mutex */

	err = pmu_enable(pmu, false);
	if (err)
		return err;

	/* TBD: cancel all sequences */
	/* TBD: init all sequences and state tables */
	/* TBD: restore pre-init message handler */

	err = pmu_enable(pmu, true);
	if (err)
		return err;

	return 0;
}

int pmu_bootstrap(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	struct mm_gk20a *mm = &g->mm;
	struct pmu_ucode_desc *desc = pmu->desc;
	u64 addr_code, addr_data, addr_load;
	u32 i, blocks, addr_args;

	gk20a_dbg_fn("");

	gk20a_writel(g, pwr_falcon_itfen_r(),
		gk20a_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());
	gk20a_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f(
			gk20a_mm_inst_block_addr(g, &mm->pmu.inst_block) >> 12) |
		pwr_pmu_new_instblk_valid_f(1) |
		pwr_pmu_new_instblk_target_sys_coh_f());

	/* TBD: load all other surfaces */
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_size(
		pmu, GK20A_PMU_TRACE_BUFSIZE);
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_base(pmu);
	g->ops.pmu_ver.set_pmu_cmdline_args_trace_dma_idx(
		pmu, GK20A_PMU_DMAIDX_VIRT);

	g->ops.pmu_ver.set_pmu_cmdline_args_cpu_freq(pmu,
		clk_get_rate(platform->clk[1]));

	addr_args = (pwr_falcon_hwcfg_dmem_size_v(
		gk20a_readl(g, pwr_falcon_hwcfg_r()))
			<< GK20A_PMU_DMEM_BLKSIZE2) -
		g->ops.pmu_ver.get_pmu_cmdline_args_size(pmu);

	pmu_copy_to_dmem(pmu, addr_args,
			(u8 *)(g->ops.pmu_ver.get_pmu_cmdline_args_ptr(pmu)),
			g->ops.pmu_ver.get_pmu_cmdline_args_size(pmu), 0);

	gk20a_writel(g, pwr_falcon_dmemc_r(0),
		pwr_falcon_dmemc_offs_f(0) |
		pwr_falcon_dmemc_blk_f(0)  |
		pwr_falcon_dmemc_aincw_f(1));

	addr_code = u64_lo32((pmu->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8) ;
	addr_data = u64_lo32((pmu->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8);
	addr_load = u64_lo32((pmu->ucode.gpu_va +
			desc->bootloader_start_offset) >> 8);

	gk20a_writel(g, pwr_falcon_dmemd_r(0), GK20A_PMU_DMAIDX_UCODE);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_imem_entry);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_data);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_data_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), 0x1);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_args);

	g->ops.pmu.write_dmatrfbase(g,
			addr_load - (desc->bootloader_imem_offset >> 8));

	blocks = ((desc->bootloader_size + 0xFF) & ~0xFF) >> 8;

	for (i = 0; i < blocks; i++) {
		gk20a_writel(g, pwr_falcon_dmatrfmoffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrffboffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrfcmd_r(),
			pwr_falcon_dmatrfcmd_imem_f(1)  |
			pwr_falcon_dmatrfcmd_write_f(0) |
			pwr_falcon_dmatrfcmd_size_f(6)  |
			pwr_falcon_dmatrfcmd_ctxdma_f(GK20A_PMU_DMAIDX_UCODE));
	}

	gk20a_writel(g, pwr_falcon_bootvec_r(),
		pwr_falcon_bootvec_vec_f(desc->bootloader_entry_point));

	gk20a_writel(g, pwr_falcon_cpuctl_r(),
		pwr_falcon_cpuctl_startcpu_f(1));

	gk20a_writel(g, pwr_falcon_os_r(), desc->app_version);

	return 0;
}

void pmu_seq_init(struct pmu_gk20a *pmu)
{
	u32 i;

	memset(pmu->seq, 0,
		sizeof(struct pmu_sequence) * PMU_MAX_NUM_SEQUENCES);
	memset(pmu->pmu_seq_tbl, 0,
		sizeof(pmu->pmu_seq_tbl));

	for (i = 0; i < PMU_MAX_NUM_SEQUENCES; i++)
		pmu->seq[i].id = i;
}

static int pmu_seq_acquire(struct pmu_gk20a *pmu,
			struct pmu_sequence **pseq)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_sequence *seq;
	u32 index;

	mutex_lock(&pmu->pmu_seq_lock);
	index = find_first_zero_bit(pmu->pmu_seq_tbl,
				sizeof(pmu->pmu_seq_tbl));
	if (index >= sizeof(pmu->pmu_seq_tbl)) {
		gk20a_err(dev_from_gk20a(g),
			"no free sequence available");
		mutex_unlock(&pmu->pmu_seq_lock);
		return -EAGAIN;
	}
	set_bit(index, pmu->pmu_seq_tbl);
	mutex_unlock(&pmu->pmu_seq_lock);

	seq = &pmu->seq[index];
	seq->state = PMU_SEQ_STATE_PENDING;

	*pseq = seq;
	return 0;
}

static void pmu_seq_release(struct pmu_gk20a *pmu,
			struct pmu_sequence *seq)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	seq->state	= PMU_SEQ_STATE_FREE;
	seq->desc	= PMU_INVALID_SEQ_DESC;
	seq->callback	= NULL;
	seq->cb_params	= NULL;
	seq->msg	= NULL;
	seq->out_payload = NULL;
	g->ops.pmu_ver.pmu_allocation_set_dmem_size(pmu,
		g->ops.pmu_ver.get_pmu_seq_in_a_ptr(seq), 0);
	g->ops.pmu_ver.pmu_allocation_set_dmem_size(pmu,
		g->ops.pmu_ver.get_pmu_seq_out_a_ptr(seq), 0);

	clear_bit(seq->id, pmu->pmu_seq_tbl);
}

static int pmu_queue_init(struct pmu_gk20a *pmu,
		u32 id, union pmu_init_msg_pmu *init)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_queue *queue = &pmu->queue[id];
	queue->id	= id;
	g->ops.pmu_ver.get_pmu_init_msg_pmu_queue_params(queue, id, init);
	queue->mutex_id = id;
	mutex_init(&queue->mutex);

	gk20a_dbg_pmu("queue %d: index %d, offset 0x%08x, size 0x%08x",
		id, queue->index, queue->offset, queue->size);

	return 0;
}

static int pmu_queue_head(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			u32 *head, bool set)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	BUG_ON(!head);

	if (PMU_IS_COMMAND_QUEUE(queue->id)) {

		if (queue->index >= pwr_pmu_queue_head__size_1_v())
			return -EINVAL;

		if (!set)
			*head = pwr_pmu_queue_head_address_v(
				gk20a_readl(g,
					pwr_pmu_queue_head_r(queue->index)));
		else
			gk20a_writel(g,
				pwr_pmu_queue_head_r(queue->index),
				pwr_pmu_queue_head_address_f(*head));
	} else {
		if (!set)
			*head = pwr_pmu_msgq_head_val_v(
				gk20a_readl(g, pwr_pmu_msgq_head_r()));
		else
			gk20a_writel(g,
				pwr_pmu_msgq_head_r(),
				pwr_pmu_msgq_head_val_f(*head));
	}

	return 0;
}

static int pmu_queue_tail(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			u32 *tail, bool set)
{
	struct gk20a *g = gk20a_from_pmu(pmu);

	BUG_ON(!tail);

	if (PMU_IS_COMMAND_QUEUE(queue->id)) {

		if (queue->index >= pwr_pmu_queue_tail__size_1_v())
			return -EINVAL;

		if (!set)
			*tail = pwr_pmu_queue_tail_address_v(
				gk20a_readl(g,
					pwr_pmu_queue_tail_r(queue->index)));
		else
			gk20a_writel(g,
				pwr_pmu_queue_tail_r(queue->index),
				pwr_pmu_queue_tail_address_f(*tail));

	} else {
		if (!set)
			*tail = pwr_pmu_msgq_tail_val_v(
				gk20a_readl(g, pwr_pmu_msgq_tail_r()));
		else
			gk20a_writel(g,
				pwr_pmu_msgq_tail_r(),
				pwr_pmu_msgq_tail_val_f(*tail));
	}

	return 0;
}

static inline void pmu_queue_read(struct pmu_gk20a *pmu,
			u32 offset, u8 *dst, u32 size)
{
	pmu_copy_from_dmem(pmu, offset, dst, size, 0);
}

static inline void pmu_queue_write(struct pmu_gk20a *pmu,
			u32 offset, u8 *src, u32 size)
{
	pmu_copy_to_dmem(pmu, offset, src, size, 0);
}

int pmu_mutex_acquire(struct pmu_gk20a *pmu, u32 id, u32 *token)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_mutex *mutex;
	u32 data, owner, max_retry;

	if (!pmu->initialized)
		return -EINVAL;

	BUG_ON(!token);
	BUG_ON(!PMU_MUTEX_ID_IS_VALID(id));
	BUG_ON(id > pmu->mutex_cnt);

	mutex = &pmu->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	if (*token != PMU_INVALID_MUTEX_OWNER_ID && *token == owner) {
		BUG_ON(mutex->ref_cnt == 0);
		gk20a_dbg_pmu("already acquired by owner : 0x%08x", *token);
		mutex->ref_cnt++;
		return 0;
	}

	max_retry = 40;
	do {
		data = pwr_pmu_mutex_id_value_v(
			gk20a_readl(g, pwr_pmu_mutex_id_r()));
		if (data == pwr_pmu_mutex_id_value_init_v() ||
		    data == pwr_pmu_mutex_id_value_not_avail_v()) {
			gk20a_warn(dev_from_gk20a(g),
				"fail to generate mutex token: val 0x%08x",
				owner);
			usleep_range(20, 40);
			continue;
		}

		owner = data;
		gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
			pwr_pmu_mutex_value_f(owner));

		data = pwr_pmu_mutex_value_v(
			gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

		if (owner == data) {
			mutex->ref_cnt = 1;
			gk20a_dbg_pmu("mutex acquired: id=%d, token=0x%x",
				mutex->index, *token);
			*token = owner;
			return 0;
		} else {
			gk20a_dbg_info("fail to acquire mutex idx=0x%08x",
				mutex->index);

			data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
			data = set_field(data,
				pwr_pmu_mutex_id_release_value_m(),
				pwr_pmu_mutex_id_release_value_f(owner));
			gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

			usleep_range(20, 40);
			continue;
		}
	} while (max_retry-- > 0);

	return -EBUSY;
}

int pmu_mutex_release(struct pmu_gk20a *pmu, u32 id, u32 *token)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_mutex *mutex;
	u32 owner, data;

	if (!pmu->initialized)
		return -EINVAL;

	BUG_ON(!token);
	BUG_ON(!PMU_MUTEX_ID_IS_VALID(id));
	BUG_ON(id > pmu->mutex_cnt);

	mutex = &pmu->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	if (*token != owner) {
		gk20a_err(dev_from_gk20a(g),
			"requester 0x%08x NOT match owner 0x%08x",
			*token, owner);
		return -EINVAL;
	}

	if (--mutex->ref_cnt > 0)
		return -EBUSY;

	gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
		pwr_pmu_mutex_value_initial_lock_f());

	data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
	data = set_field(data, pwr_pmu_mutex_id_release_value_m(),
		pwr_pmu_mutex_id_release_value_f(owner));
	gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

	gk20a_dbg_pmu("mutex released: id=%d, token=0x%x",
		mutex->index, *token);

	return 0;
}

static int pmu_queue_lock(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	if (PMU_IS_MESSAGE_QUEUE(queue->id))
		return 0;

	if (PMU_IS_SW_COMMAND_QUEUE(queue->id)) {
		mutex_lock(&queue->mutex);
		return 0;
	}

	err = pmu_mutex_acquire(pmu, queue->mutex_id, &queue->mutex_lock);
	return err;
}

static int pmu_queue_unlock(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	if (PMU_IS_MESSAGE_QUEUE(queue->id))
		return 0;

	if (PMU_IS_SW_COMMAND_QUEUE(queue->id)) {
		mutex_unlock(&queue->mutex);
		return 0;
	}

	err = pmu_mutex_release(pmu, queue->mutex_id, &queue->mutex_lock);
	return err;
}

/* called by pmu_read_message, no lock */
static bool pmu_queue_is_empty(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	u32 head, tail;

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	if (queue->opened && queue->oflag == OFLAG_READ)
		tail = queue->position;
	else
		pmu_queue_tail(pmu, queue, &tail, QUEUE_GET);

	return head == tail;
}

static bool pmu_queue_has_room(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, u32 size, bool *need_rewind)
{
	u32 head, tail;
	bool rewind = false;
	int free;

	size = ALIGN(size, QUEUE_ALIGNMENT);

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	pmu_queue_tail(pmu, queue, &tail, QUEUE_GET);
	if (head >= tail) {
		free = queue->offset + queue->size - head;
		free -= PMU_CMD_HDR_SIZE;

		if (size > free) {
			rewind = true;
			head = queue->offset;
		}
	}

	if (head < tail)
		free = tail - head - 1;

	if (need_rewind)
		*need_rewind = rewind;

	return size <= free;
}

static int pmu_queue_push(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, void *data, u32 size)
{

	gk20a_dbg_fn("");

	if (!queue->opened && queue->oflag == OFLAG_WRITE){
		gk20a_err(dev_from_gk20a(gk20a_from_pmu(pmu)),
			"queue not opened for write");
		return -EINVAL;
	}

	pmu_queue_write(pmu, queue->position, data, size);
	queue->position += ALIGN(size, QUEUE_ALIGNMENT);
	return 0;
}

static int pmu_queue_pop(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, void *data, u32 size,
			u32 *bytes_read)
{
	u32 head, tail, used;

	*bytes_read = 0;

	if (!queue->opened && queue->oflag == OFLAG_READ){
		gk20a_err(dev_from_gk20a(gk20a_from_pmu(pmu)),
			"queue not opened for read");
		return -EINVAL;
	}

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	tail = queue->position;

	if (head == tail)
		return 0;

	if (head > tail)
		used = head - tail;
	else
		used = queue->offset + queue->size - tail;

	if (size > used) {
		gk20a_warn(dev_from_gk20a(gk20a_from_pmu(pmu)),
			"queue size smaller than request read");
		size = used;
	}

	pmu_queue_read(pmu, tail, data, size);
	queue->position += ALIGN(size, QUEUE_ALIGNMENT);
	*bytes_read = size;
	return 0;
}

static void pmu_queue_rewind(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	struct pmu_cmd cmd;

	gk20a_dbg_fn("");

	if (!queue->opened) {
		gk20a_err(dev_from_gk20a(gk20a_from_pmu(pmu)),
			"queue not opened");
		return;
	}

	if (queue->oflag == OFLAG_WRITE) {
		cmd.hdr.unit_id = PMU_UNIT_REWIND;
		cmd.hdr.size = PMU_CMD_HDR_SIZE;
		pmu_queue_push(pmu, queue, &cmd, cmd.hdr.size);
		gk20a_dbg_pmu("queue %d rewinded", queue->id);
	}

	queue->position = queue->offset;
	return;
}

/* open for read and lock the queue */
static int pmu_queue_open_read(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	err = pmu_queue_lock(pmu, queue);
	if (err)
		return err;

	if (queue->opened)
		BUG();

	pmu_queue_tail(pmu, queue, &queue->position, QUEUE_GET);
	queue->oflag = OFLAG_READ;
	queue->opened = true;

	return 0;
}

/* open for write and lock the queue
   make sure there's enough free space for the write */
static int pmu_queue_open_write(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, u32 size)
{
	bool rewind = false;
	int err;

	err = pmu_queue_lock(pmu, queue);
	if (err)
		return err;

	if (queue->opened)
		BUG();

	if (!pmu_queue_has_room(pmu, queue, size, &rewind)) {
		gk20a_dbg_pmu("queue full: queue-id %d: index %d",
				queue->id, queue->index);
		pmu_queue_unlock(pmu, queue);
		return -EAGAIN;
	}

	pmu_queue_head(pmu, queue, &queue->position, QUEUE_GET);
	queue->oflag = OFLAG_WRITE;
	queue->opened = true;

	if (rewind)
		pmu_queue_rewind(pmu, queue);

	return 0;
}

/* close and unlock the queue */
static int pmu_queue_close(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, bool commit)
{
	if (!queue->opened)
		return 0;

	if (commit) {
		if (queue->oflag == OFLAG_READ) {
			pmu_queue_tail(pmu, queue,
				&queue->position, QUEUE_SET);
		}
		else {
			pmu_queue_head(pmu, queue,
				&queue->position, QUEUE_SET);
		}
	}

	queue->opened = false;

	pmu_queue_unlock(pmu, queue);

	return 0;
}

void gk20a_remove_pmu_support(struct pmu_gk20a *pmu)
{
	gk20a_dbg_fn("");

	if (gk20a_alloc_initialized(&pmu->dmem))
		gk20a_alloc_destroy(&pmu->dmem);

	release_firmware(pmu->fw);
}

static int gk20a_init_pmu_reset_enable_hw(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;

	gk20a_dbg_fn("");

	pmu_enable_hw(pmu, true);

	return 0;
}

static int gk20a_prepare_ucode(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err = 0;
	struct device *d = dev_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;

	if (pmu->fw)
		return gk20a_init_pmu(pmu);

	pmu->fw = nvgpu_request_firmware(g, GK20A_PMU_UCODE_IMAGE, 0);
	if (!pmu->fw) {
		gk20a_err(d, "failed to load pmu ucode!!");
		return err;
	}

	gk20a_dbg_fn("firmware loaded");

	pmu->desc = (struct pmu_ucode_desc *)pmu->fw->data;
	pmu->ucode_image = (u32 *)((u8 *)pmu->desc +
			pmu->desc->descriptor_size);

	err = gk20a_gmmu_alloc_map_sys(vm, GK20A_PMU_UCODE_SIZE_MAX,
			&pmu->ucode);
	if (err)
		goto err_release_fw;

	gk20a_mem_wr_n(g, &pmu->ucode, 0, pmu->ucode_image,
			pmu->desc->app_start_offset + pmu->desc->app_size);

	return gk20a_init_pmu(pmu);

 err_release_fw:
	release_firmware(pmu->fw);
	pmu->fw = NULL;

	return err;
}

static int gk20a_init_pmu_setup_sw(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	int i, err = 0;
	u8 *ptr;

	gk20a_dbg_fn("");

	/* start with elpg disabled until first enable call */
	pmu->elpg_refcnt = 0;

	if (pmu->sw_ready) {
		for (i = 0; i < pmu->mutex_cnt; i++) {
			pmu->mutex[i].id    = i;
			pmu->mutex[i].index = i;
		}
		pmu_seq_init(pmu);

		gk20a_dbg_fn("skip init");
		goto skip_init;
	}

	/* no infoRom script from vbios? */

	/* TBD: sysmon subtask */

	if (IS_ENABLED(CONFIG_TEGRA_GK20A_PERFMON))
		pmu->perfmon_sampling_enabled = true;

	pmu->mutex_cnt = pwr_pmu_mutex__size_1_v();
	pmu->mutex = kzalloc(pmu->mutex_cnt *
		sizeof(struct pmu_mutex), GFP_KERNEL);
	if (!pmu->mutex) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < pmu->mutex_cnt; i++) {
		pmu->mutex[i].id    = i;
		pmu->mutex[i].index = i;
	}

	pmu->seq = kzalloc(PMU_MAX_NUM_SEQUENCES *
		sizeof(struct pmu_sequence), GFP_KERNEL);
	if (!pmu->seq) {
		err = -ENOMEM;
		goto err_free_mutex;
	}

	pmu_seq_init(pmu);

	INIT_WORK(&pmu->pg_init, pmu_setup_hw);

	err = gk20a_gmmu_alloc_map_sys(vm, GK20A_PMU_SEQ_BUF_SIZE,
			&pmu->seq_buf);
	if (err) {
		gk20a_err(d, "failed to allocate memory\n");
		goto err_free_seq;
	}

	err = gk20a_gmmu_alloc_map_sys(vm, GK20A_PMU_TRACE_BUFSIZE,
			&pmu->trace_buf);
	if (err) {
		gk20a_err(d, "failed to allocate trace memory\n");
		goto err_free_seq_buf;
	}

	ptr = (u8 *)pmu->seq_buf.cpu_va;

	/* TBD: remove this if ZBC save/restore is handled by PMU
	 * end an empty ZBC sequence for now */
	ptr[0] = 0x16; /* opcode EXIT */
	ptr[1] = 0; ptr[2] = 1; ptr[3] = 0;
	ptr[4] = 0; ptr[5] = 0; ptr[6] = 0; ptr[7] = 0;

	pmu->seq_buf.size = GK20A_PMU_SEQ_BUF_SIZE;

	pmu->sw_ready = true;

skip_init:
	gk20a_dbg_fn("done");
	return 0;
 err_free_seq_buf:
	gk20a_gmmu_unmap_free(vm, &pmu->seq_buf);
 err_free_seq:
	kfree(pmu->seq);
 err_free_mutex:
	kfree(pmu->mutex);
 err:
	gk20a_dbg_fn("fail");
	return err;
}

static void pmu_handle_pg_buf_config_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	struct pmu_pg_msg_eng_buf_stat *eng_buf_stat = &msg->msg.pg.eng_buf_stat;

	gk20a_dbg_fn("");

	gk20a_dbg_pmu("reply PMU_PG_CMD_ID_ENG_BUF_LOAD PMU_PGENG_GR_BUFFER_IDX_FECS");
	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "PGENG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	pmu->buf_loaded = (eng_buf_stat->status == PMU_PG_MSG_ENG_BUF_LOADED);
	if ((!pmu->buf_loaded) &&
	    (pmu->pmu_state == PMU_STATE_LOADING_PG_BUF))
		gk20a_err(dev_from_gk20a(g), "failed to load PGENG buffer");
	else {
	  schedule_work(&pmu->pg_init);
	}
}

static int gk20a_init_pmu_setup_hw1(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err = 0;

	gk20a_dbg_fn("");

	mutex_lock(&pmu->isr_mutex);
	pmu_reset(pmu);
	pmu->isr_enabled = true;
	mutex_unlock(&pmu->isr_mutex);

	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
		pwr_fbif_transcfg_mem_type_virtual_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
		pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_noncoherent_sysmem_f());

	err = g->ops.pmu.pmu_nsbootstrap(pmu);

	return err;

}

static void pmu_setup_hw_load_zbc(struct gk20a *g);
static void pmu_setup_hw_enable_elpg(struct gk20a *g);

void pmu_setup_hw(struct work_struct *work)
{
	struct pmu_gk20a *pmu = container_of(work, struct pmu_gk20a, pg_init);
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	switch (pmu->pmu_state) {
	case PMU_STATE_INIT_RECEIVED:
		gk20a_dbg_pmu("pmu starting");
		if (platform->can_elpg)
			pmu_init_powergating(g);
		break;
	case PMU_STATE_ELPG_BOOTED:
		gk20a_dbg_pmu("elpg booted");
		gk20a_init_pmu_bind_fecs(g);
		break;
	case PMU_STATE_LOADING_PG_BUF:
		gk20a_dbg_pmu("loaded pg buf");
		pmu_setup_hw_load_zbc(g);
		break;
	case PMU_STATE_LOADING_ZBC:
		gk20a_dbg_pmu("loaded zbc");
		pmu_setup_hw_enable_elpg(g);
		break;
	case PMU_STATE_STARTED:
		gk20a_dbg_pmu("PMU booted");
		break;
	default:
		gk20a_dbg_pmu("invalid state");
		break;
	}
}

int gk20a_init_pmu_bind_fecs(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 desc;
	int err = 0;
	u32 gr_engine_id;

	gk20a_dbg_fn("");

	gr_engine_id = gk20a_fifo_get_gr_engine_id(g);

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE +
			g->ops.pmu_ver.pg_cmd_eng_buf_load_size(&cmd.cmd.pg);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type(&cmd.cmd.pg,
			PMU_PG_CMD_ID_ENG_BUF_LOAD);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id(&cmd.cmd.pg,
			gr_engine_id);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx(&cmd.cmd.pg,
			PMU_PGENG_GR_BUFFER_IDX_FECS);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size(&cmd.cmd.pg,
			pmu->pg_buf.size);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base(&cmd.cmd.pg,
			u64_lo32(pmu->pg_buf.gpu_va));
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset(&cmd.cmd.pg,
			(u8)(pmu->pg_buf.gpu_va & 0xFF));
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx(&cmd.cmd.pg,
			PMU_DMAIDX_VIRT);

	pmu->buf_loaded = false;
	gk20a_dbg_pmu("cmd post PMU_PG_CMD_ID_ENG_BUF_LOAD PMU_PGENG_GR_BUFFER_IDX_FECS");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu, &desc, ~0);
	pmu->pmu_state = PMU_STATE_LOADING_PG_BUF;
	return err;
}

static void pmu_setup_hw_load_zbc(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 desc;
	u32 gr_engine_id;

	gr_engine_id = gk20a_fifo_get_gr_engine_id(g);

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE +
			g->ops.pmu_ver.pg_cmd_eng_buf_load_size(&cmd.cmd.pg);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_cmd_type(&cmd.cmd.pg,
			PMU_PG_CMD_ID_ENG_BUF_LOAD);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_engine_id(&cmd.cmd.pg,
			gr_engine_id);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_idx(&cmd.cmd.pg,
			PMU_PGENG_GR_BUFFER_IDX_ZBC);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_buf_size(&cmd.cmd.pg,
			pmu->seq_buf.size);
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_base(&cmd.cmd.pg,
			u64_lo32(pmu->seq_buf.gpu_va));
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_offset(&cmd.cmd.pg,
			(u8)(pmu->seq_buf.gpu_va & 0xFF));
	g->ops.pmu_ver.pg_cmd_eng_buf_load_set_dma_idx(&cmd.cmd.pg,
			PMU_DMAIDX_VIRT);

	pmu->buf_loaded = false;
	gk20a_dbg_pmu("cmd post PMU_PG_CMD_ID_ENG_BUF_LOAD PMU_PGENG_GR_BUFFER_IDX_ZBC");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu, &desc, ~0);
	pmu->pmu_state = PMU_STATE_LOADING_ZBC;
}

static void pmu_setup_hw_enable_elpg(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;

	/*
	 * FIXME: To enable ELPG, we increase the PMU ext2priv timeout unit to
	 * 7. This prevents PMU stalling on Host register accesses. Once the
	 * cause for this hang is discovered and fixed, this WAR should be
	 * removed.
	 */
	gk20a_writel(g, 0x10a164, 0x109ff);

	pmu->initialized = true;
	pmu->pmu_state = PMU_STATE_STARTED;

	pmu->zbc_ready = true;
	/* Save zbc table after PMU is initialized. */
	gk20a_pmu_save_zbc(g, 0xf);

	if (g->elpg_enabled) {
		/* Init reg with prod values*/
		if (g->ops.pmu.pmu_setup_elpg)
			g->ops.pmu.pmu_setup_elpg(g);
		gk20a_pmu_enable_elpg(g);
	}

	udelay(50);

	/* Enable AELPG */
	if (g->aelpg_enabled) {
		gk20a_aelpg_init(g);
		gk20a_aelpg_init_and_enable(g, PMU_AP_CTRL_ID_GRAPHICS);
	}
}

static void gk20a_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	gk20a_writel(g, pwr_falcon_dmatrfbase_r(), addr);
}

int gk20a_pmu_reset(struct gk20a *g)
{
	int err;
	struct pmu_gk20a *pmu = &g->pmu;

	err = pmu_reset(pmu);

	return err;
}

u32 gk20a_pmu_pg_engines_list(struct gk20a *g)
{
	return BIT(PMU_PG_ELPG_ENGINE_ID_GRAPHICS);
}

u32 gk20a_pmu_pg_feature_list(struct gk20a *g, u32 pg_engine_id)
{
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
		return PMU_PG_FEATURE_GR_POWER_GATING_ENABLED;

	return 0;
}

void gk20a_init_pmu_ops(struct gpu_ops *gops)
{
	gops->pmu.prepare_ucode = gk20a_prepare_ucode;
	gops->pmu.pmu_setup_hw_and_bootstrap = gk20a_init_pmu_setup_hw1;
	gops->pmu.pmu_nsbootstrap = pmu_bootstrap;
	gops->pmu.pmu_setup_elpg = NULL;
	gops->pmu.init_wpr_region = NULL;
	gops->pmu.load_lsfalcon_ucode = NULL;
	gops->pmu.write_dmatrfbase = gk20a_write_dmatrfbase;
	gops->pmu.pmu_elpg_statistics = gk20a_pmu_elpg_statistics;
	gops->pmu.pmu_pg_init_param = NULL;
	gops->pmu.pmu_pg_supported_engines_list = gk20a_pmu_pg_engines_list;
	gops->pmu.pmu_pg_engines_feature_list = gk20a_pmu_pg_feature_list;
	gops->pmu.pmu_pg_param_post_init = NULL;
	gops->pmu.send_lrf_tex_ltc_dram_overide_en_dis_cmd = NULL;
	gops->pmu.dump_secure_fuses = NULL;
	gops->pmu.is_lazy_bootstrap = NULL;
	gops->pmu.is_priv_load = NULL;
	gops->pmu.get_wpr = NULL;
	gops->pmu.alloc_blob_space = NULL;
	gops->pmu.pmu_populate_loader_cfg = NULL;
	gops->pmu.flcn_populate_bl_dmem_desc = NULL;
	gops->pmu.reset = gk20a_pmu_reset;
}

int gk20a_init_pmu_support(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32 err;

	gk20a_dbg_fn("");

	if (pmu->initialized)
		return 0;

	err = gk20a_init_pmu_reset_enable_hw(g);
	if (err)
		return err;

	if (support_gk20a_pmu(g->dev)) {
		err = gk20a_init_pmu_setup_sw(g);
		if (err)
			return err;
		err = g->ops.pmu.pmu_setup_hw_and_bootstrap(g);
		if (err)
			return err;

		pmu->pmu_state = PMU_STATE_STARTING;
	}

	return err;
}

static void pmu_handle_pg_elpg_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	struct pmu_pg_msg_elpg_msg *elpg_msg = &msg->msg.pg.elpg_msg;

	gk20a_dbg_fn("");

	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "ELPG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	switch (elpg_msg->msg) {
	case PMU_PG_ELPG_MSG_INIT_ACK:
		gk20a_dbg_pmu("INIT_PG is ack from PMU, eng - %d",
			elpg_msg->engine_id);
		break;
	case PMU_PG_ELPG_MSG_ALLOW_ACK:
		gk20a_dbg_pmu("ALLOW is ack from PMU, eng - %d",
			elpg_msg->engine_id);
		if (elpg_msg->engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
			pmu->elpg_stat = PMU_ELPG_STAT_ON;
		else if (elpg_msg->engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
			pmu->mscg_transition_state = PMU_ELPG_STAT_ON;
		break;
	case PMU_PG_ELPG_MSG_DISALLOW_ACK:
		gk20a_dbg_pmu("DISALLOW is ack from PMU, eng - %d",
			elpg_msg->engine_id);

		if (elpg_msg->engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
			pmu->elpg_stat = PMU_ELPG_STAT_OFF;
		else if (elpg_msg->engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
			pmu->mscg_transition_state = PMU_ELPG_STAT_OFF;

		if (pmu->pmu_state == PMU_STATE_ELPG_BOOTING) {
			if (g->ops.pmu.pmu_pg_engines_feature_list &&
				g->ops.pmu.pmu_pg_engines_feature_list(g,
				PMU_PG_ELPG_ENGINE_ID_GRAPHICS) !=
				PMU_PG_FEATURE_GR_POWER_GATING_ENABLED) {
				pmu->initialized = true;
				pmu->pmu_state = PMU_STATE_STARTED;
				WRITE_ONCE(pmu->mscg_stat, PMU_MSCG_DISABLED);
				/* make status visible */
				smp_mb();
			} else {
				pmu->pmu_state = PMU_STATE_ELPG_BOOTED;
				schedule_work(&pmu->pg_init);
			}
		}
		break;
	default:
		gk20a_err(dev_from_gk20a(g),
			"unsupported ELPG message : 0x%04x", elpg_msg->msg);
	}

	return;
}

static void pmu_handle_pg_stat_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;

	gk20a_dbg_fn("");

	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "ELPG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	switch (msg->msg.pg.stat.sub_msg_id) {
	case PMU_PG_STAT_MSG_RESP_DMEM_OFFSET:
		gk20a_dbg_pmu("ALLOC_DMEM_OFFSET is acknowledged from PMU");
		pmu->stat_dmem_offset[msg->msg.pg.stat.engine_id] =
			msg->msg.pg.stat.data;
		break;
	default:
		break;
	}
}

static int pmu_pg_init_send(struct gk20a *g, u32 pg_engine_id)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	gk20a_dbg_fn("");

	if (tegra_cpu_is_asim()) {
		/* TBD: calculate threshold for silicon */
		gk20a_writel(g, pwr_pmu_pg_idlefilth_r(pg_engine_id),
			PMU_PG_IDLE_THRESHOLD_SIM);
		gk20a_writel(g, pwr_pmu_pg_ppuidlefilth_r(pg_engine_id),
			PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM);
	} else {
		/* TBD: calculate threshold for silicon */
		gk20a_writel(g, pwr_pmu_pg_idlefilth_r(pg_engine_id),
			PMU_PG_IDLE_THRESHOLD);
		gk20a_writel(g, pwr_pmu_pg_ppuidlefilth_r(pg_engine_id),
			PMU_PG_POST_POWERUP_IDLE_THRESHOLD);
	}

	if (g->ops.pmu.pmu_pg_init_param)
		g->ops.pmu.pmu_pg_init_param(g, pg_engine_id);

	/* init ELPG */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_INIT;

	gk20a_dbg_pmu("cmd post PMU_PG_ELPG_CMD_INIT");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	/* alloc dmem for powergating state log */
	pmu->stat_dmem_offset[pg_engine_id] = 0;
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_stat);
	cmd.cmd.pg.stat.cmd_type = PMU_PG_CMD_ID_PG_STAT;
	cmd.cmd.pg.stat.engine_id = pg_engine_id;
	cmd.cmd.pg.stat.sub_cmd_id = PMU_PG_STAT_CMD_ALLOC_DMEM;
	cmd.cmd.pg.stat.data = 0;

	gk20a_dbg_pmu("cmd post PMU_PG_STAT_CMD_ALLOC_DMEM");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_stat_msg, pmu, &seq, ~0);

	/* disallow ELPG initially
	   PMU ucode requires a disallow cmd before allow cmd */
	/* set for wait_event PMU_ELPG_STAT_OFF */
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
		pmu->elpg_stat = PMU_ELPG_STAT_OFF;
	else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
		pmu->mscg_transition_state = PMU_ELPG_STAT_OFF;
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_DISALLOW;

	gk20a_dbg_pmu("cmd post PMU_PG_ELPG_CMD_DISALLOW");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
		pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	return 0;
}
static int pmu_init_powergating(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32 pg_engine_id;
	u32 pg_engine_id_list = 0;

	gk20a_dbg_fn("");

	if (g->ops.pmu.pmu_pg_supported_engines_list)
		pg_engine_id_list = g->ops.pmu.pmu_pg_supported_engines_list(g);

	gk20a_gr_wait_initialized(g);

	for (pg_engine_id = PMU_PG_ELPG_ENGINE_ID_GRAPHICS;
		pg_engine_id < PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE;
			pg_engine_id++) {

		if (BIT(pg_engine_id) & pg_engine_id_list) {
			pmu_pg_init_send(g, pg_engine_id);
			if (pmu->pmu_state == PMU_STATE_INIT_RECEIVED)
				pmu->pmu_state = PMU_STATE_ELPG_BOOTING;
		}
	}

	if (g->ops.pmu.pmu_pg_param_post_init)
		g->ops.pmu.pmu_pg_param_post_init(g);

	return 0;
}

static u8 get_perfmon_id(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 ver = g->gpu_characteristics.arch + g->gpu_characteristics.impl;
	u8 unit_id;

	switch (ver) {
	case GK20A_GPUID_GK20A:
	case GK20A_GPUID_GM20B:
		unit_id = PMU_UNIT_PERFMON;
		break;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	case TEGRA_18x_GPUID:
	case TEGRA_18x_GPUID2:
	case TEGRA_18x_GPUID3:
		unit_id = PMU_UNIT_PERFMON_T18X;
		break;
#endif
	case GK20A_GPUID_GM206:
	case GK20A_GPUID_GM204:
		unit_id = PMU_UNIT_PERFMON_T18X;
		break;
	default:
		gk20a_err(g->dev, "no support for %x", ver);
		BUG();
	}

	return unit_id;
}

static int pmu_init_perfmon(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_v *pv = &g->ops.pmu_ver;
	struct pmu_cmd cmd;
	struct pmu_payload payload;
	u32 seq;
	u32 data;

	gk20a_dbg_fn("");

	pmu->perfmon_ready = 0;

	/* use counter #3 for GR && CE2 busy cycles */
	gk20a_writel(g, pwr_pmu_idle_mask_r(3),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	/* disable idle filtering for counters 3 and 6 */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(3));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(3), data);

	/* use counter #6 for total cycles */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(6));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(6), data);

	/*
	 * We don't want to disturb counters #3 and #6, which are used by
	 * perfmon, so we add wiring also to counters #1 and #2 for
	 * exposing raw counter readings.
	 */
	gk20a_writel(g, pwr_pmu_idle_mask_r(1),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(1));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(1), data);

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(2));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(2), data);

	if (!pmu->sample_buffer)
		pmu->sample_buffer = gk20a_alloc(&pmu->dmem,
						  2 * sizeof(u16));
	if (!pmu->sample_buffer) {
		gk20a_err(dev_from_gk20a(g),
			  "failed to allocate perfmon sample buffer");
		return -ENOMEM;
	}

	/* init PERFMON */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = get_perfmon_id(pmu);
	cmd.hdr.size = PMU_CMD_HDR_SIZE + pv->get_pmu_perfmon_cmd_init_size();
	cmd.cmd.perfmon.cmd_type = PMU_PERFMON_CMD_ID_INIT;
	/* buffer to save counter values for pmu perfmon */
	pv->perfmon_cmd_init_set_sample_buffer(&cmd.cmd.perfmon,
	(u16)pmu->sample_buffer);
	/* number of sample periods below lower threshold
	   before pmu triggers perfmon decrease event
	   TBD: = 15 */
	pv->perfmon_cmd_init_set_dec_cnt(&cmd.cmd.perfmon, 15);
	/* index of base counter, aka. always ticking counter */
	pv->perfmon_cmd_init_set_base_cnt_id(&cmd.cmd.perfmon, 6);
	/* microseconds interval between pmu polls perf counters */
	pv->perfmon_cmd_init_set_samp_period_us(&cmd.cmd.perfmon, 16700);
	/* number of perfmon counters
	   counter #3 (GR and CE2) for gk20a */
	pv->perfmon_cmd_init_set_num_cnt(&cmd.cmd.perfmon, 1);
	/* moving average window for sample periods
	   TBD: = 3000000 / sample_period_us = 17 */
	pv->perfmon_cmd_init_set_mov_avg(&cmd.cmd.perfmon, 17);

	memset(&payload, 0, sizeof(struct pmu_payload));
	payload.in.buf = pv->get_perfmon_cntr_ptr(pmu);
	payload.in.size = pv->get_perfmon_cntr_sz(pmu);
	payload.in.offset = pv->get_perfmon_cmd_init_offsetofvar(COUNTER_ALLOC);

	gk20a_dbg_pmu("cmd post PMU_PERFMON_CMD_ID_INIT");
	gk20a_pmu_cmd_post(g, &cmd, NULL, &payload, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);

	return 0;
}

static int pmu_process_init_msg(struct pmu_gk20a *pmu,
			struct pmu_msg *msg)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_v *pv = &g->ops.pmu_ver;
	union pmu_init_msg_pmu *init;
	struct pmu_sha1_gid_data gid_data;
	u32 i, tail = 0;
	gk20a_dbg_pmu("init received\n");

	tail = pwr_pmu_msgq_tail_val_v(
		gk20a_readl(g, pwr_pmu_msgq_tail_r()));

	pmu_copy_from_dmem(pmu, tail,
		(u8 *)&msg->hdr, PMU_MSG_HDR_SIZE, 0);
	if (msg->hdr.unit_id != PMU_UNIT_INIT) {
		gk20a_err(dev_from_gk20a(g),
			"expecting init msg");
		return -EINVAL;
	}

	pmu_copy_from_dmem(pmu, tail + PMU_MSG_HDR_SIZE,
		(u8 *)&msg->msg, msg->hdr.size - PMU_MSG_HDR_SIZE, 0);

	if (msg->msg.init.msg_type != PMU_INIT_MSG_TYPE_PMU_INIT) {
		gk20a_err(dev_from_gk20a(g),
			"expecting init msg");
		return -EINVAL;
	}

	tail += ALIGN(msg->hdr.size, PMU_DMEM_ALIGNMENT);
	gk20a_writel(g, pwr_pmu_msgq_tail_r(),
		pwr_pmu_msgq_tail_val_f(tail));

	init = pv->get_pmu_msg_pmu_init_msg_ptr(&(msg->msg.init));
	if (!pmu->gid_info.valid) {

		pmu_copy_from_dmem(pmu,
			pv->get_pmu_init_msg_pmu_sw_mg_off(init),
			(u8 *)&gid_data,
			sizeof(struct pmu_sha1_gid_data), 0);

		pmu->gid_info.valid =
			(*(u32 *)gid_data.signature == PMU_SHA1_GID_SIGNATURE);

		if (pmu->gid_info.valid) {

			BUG_ON(sizeof(pmu->gid_info.gid) !=
				sizeof(gid_data.gid));

			memcpy(pmu->gid_info.gid, gid_data.gid,
				sizeof(pmu->gid_info.gid));
		}
	}

	for (i = 0; i < PMU_QUEUE_COUNT; i++)
		pmu_queue_init(pmu, i, init);

	if (!gk20a_alloc_initialized(&pmu->dmem)) {
		/* Align start and end addresses */
		u32 start = ALIGN(pv->get_pmu_init_msg_pmu_sw_mg_off(init),
				  PMU_DMEM_ALLOC_ALIGNMENT);
		u32 end = (pv->get_pmu_init_msg_pmu_sw_mg_off(init) +
			   pv->get_pmu_init_msg_pmu_sw_mg_size(init)) &
			~(PMU_DMEM_ALLOC_ALIGNMENT - 1);
		u32 size = end - start;
		gk20a_bitmap_allocator_init(g, &pmu->dmem, "gk20a_pmu_dmem",
					   start, size,
					   PMU_DMEM_ALLOC_ALIGNMENT, 0);
	}

	pmu->pmu_ready = true;
	pmu->pmu_state = PMU_STATE_INIT_RECEIVED;
	schedule_work(&pmu->pg_init);
	gk20a_dbg_pmu("init received end\n");

	return 0;
}

static bool pmu_read_message(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			struct pmu_msg *msg, int *status)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	u32 read_size, bytes_read;
	int err;

	*status = 0;

	if (pmu_queue_is_empty(pmu, queue))
		return false;

	err = pmu_queue_open_read(pmu, queue);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to open queue %d for read", queue->id);
		*status = err;
		return false;
	}

	err = pmu_queue_pop(pmu, queue, &msg->hdr,
			PMU_MSG_HDR_SIZE, &bytes_read);
	if (err || bytes_read != PMU_MSG_HDR_SIZE) {
		gk20a_err(dev_from_gk20a(g),
			"fail to read msg from queue %d", queue->id);
		*status = err | -EINVAL;
		goto clean_up;
	}

	if (msg->hdr.unit_id == PMU_UNIT_REWIND) {
		pmu_queue_rewind(pmu, queue);
		/* read again after rewind */
		err = pmu_queue_pop(pmu, queue, &msg->hdr,
				PMU_MSG_HDR_SIZE, &bytes_read);
		if (err || bytes_read != PMU_MSG_HDR_SIZE) {
			gk20a_err(dev_from_gk20a(g),
				"fail to read msg from queue %d", queue->id);
			*status = err | -EINVAL;
			goto clean_up;
		}
	}

	if (!PMU_UNIT_ID_IS_VALID(msg->hdr.unit_id)) {
		gk20a_err(dev_from_gk20a(g),
			"read invalid unit_id %d from queue %d",
			msg->hdr.unit_id, queue->id);
			*status = -EINVAL;
			goto clean_up;
	}

	if (msg->hdr.size > PMU_MSG_HDR_SIZE) {
		read_size = msg->hdr.size - PMU_MSG_HDR_SIZE;
		err = pmu_queue_pop(pmu, queue, &msg->msg,
			read_size, &bytes_read);
		if (err || bytes_read != read_size) {
			gk20a_err(dev_from_gk20a(g),
				"fail to read msg from queue %d", queue->id);
			*status = err;
			goto clean_up;
		}
	}

	err = pmu_queue_close(pmu, queue, true);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"fail to close queue %d", queue->id);
		*status = err;
		return false;
	}

	return true;

clean_up:
	err = pmu_queue_close(pmu, queue, false);
	if (err)
		gk20a_err(dev_from_gk20a(g),
			"fail to close queue %d", queue->id);
	return false;
}

static int pmu_response_handle(struct pmu_gk20a *pmu,
			struct pmu_msg *msg)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_sequence *seq;
	struct pmu_v *pv = &g->ops.pmu_ver;
	int ret = 0;

	gk20a_dbg_fn("");

	seq = &pmu->seq[msg->hdr.seq_id];
	if (seq->state != PMU_SEQ_STATE_USED &&
	    seq->state != PMU_SEQ_STATE_CANCELLED) {
		gk20a_err(dev_from_gk20a(g),
			"msg for an unknown sequence %d", seq->id);
		return -EINVAL;
	}

	if (msg->hdr.unit_id == PMU_UNIT_RC &&
	    msg->msg.rc.msg_type == PMU_RC_MSG_TYPE_UNHANDLED_CMD) {
		gk20a_err(dev_from_gk20a(g),
			"unhandled cmd: seq %d", seq->id);
	}
	else if (seq->state != PMU_SEQ_STATE_CANCELLED) {
		if (seq->msg) {
			if (seq->msg->hdr.size >= msg->hdr.size) {
				memcpy(seq->msg, msg, msg->hdr.size);
			}  else {
				gk20a_err(dev_from_gk20a(g),
					"sequence %d msg buffer too small",
					seq->id);
			}
		}
		if (pv->pmu_allocation_get_dmem_size(pmu,
		pv->get_pmu_seq_out_a_ptr(seq)) != 0) {
			pmu_copy_from_dmem(pmu,
			pv->pmu_allocation_get_dmem_offset(pmu,
			pv->get_pmu_seq_out_a_ptr(seq)),
			seq->out_payload,
			pv->pmu_allocation_get_dmem_size(pmu,
			pv->get_pmu_seq_out_a_ptr(seq)), 0);
		}
	} else
		seq->callback = NULL;
	if (pv->pmu_allocation_get_dmem_size(pmu,
			pv->get_pmu_seq_in_a_ptr(seq)) != 0)
		gk20a_free(&pmu->dmem,
			pv->pmu_allocation_get_dmem_offset(pmu,
			pv->get_pmu_seq_in_a_ptr(seq)));
	if (pv->pmu_allocation_get_dmem_size(pmu,
			pv->get_pmu_seq_out_a_ptr(seq)) != 0)
		gk20a_free(&pmu->dmem,
			pv->pmu_allocation_get_dmem_offset(pmu,
			pv->get_pmu_seq_out_a_ptr(seq)));

	if (seq->out_mem != NULL) {
		memset(pv->pmu_allocation_get_fb_addr(pmu,
			pv->get_pmu_seq_out_a_ptr(seq)), 0x0,
			pv->pmu_allocation_get_fb_size(pmu,
				pv->get_pmu_seq_out_a_ptr(seq)));

		gk20a_pmu_surface_free(g, seq->out_mem);
		if (seq->out_mem != seq->in_mem)
			kfree(seq->out_mem);
		else
			seq->out_mem = NULL;
	}

	if (seq->in_mem != NULL) {
		memset(pv->pmu_allocation_get_fb_addr(pmu,
			pv->get_pmu_seq_in_a_ptr(seq)), 0x0,
			pv->pmu_allocation_get_fb_size(pmu,
				pv->get_pmu_seq_in_a_ptr(seq)));

		gk20a_pmu_surface_free(g, seq->in_mem);
		kfree(seq->in_mem);
		seq->in_mem = NULL;
	}

	if (seq->callback)
		seq->callback(g, msg, seq->cb_params, seq->desc, ret);

	pmu_seq_release(pmu, seq);

	/* TBD: notify client waiting for available dmem */

	gk20a_dbg_fn("done");

	return 0;
}

int pmu_wait_message_cond(struct pmu_gk20a *pmu, u32 timeout,
				 u32 *var, u32 val);

static void pmu_handle_zbc_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	gk20a_dbg_pmu("reply ZBC_TABLE_UPDATE");
	pmu->zbc_save_done = 1;
}

void gk20a_pmu_save_zbc(struct gk20a *g, u32 entries)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	if (!pmu->pmu_ready || !entries || !pmu->zbc_ready)
		return;

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_zbc_cmd);
	cmd.cmd.zbc.cmd_type = g->ops.pmu_ver.cmd_id_zbc_table_update;
	cmd.cmd.zbc.entry_mask = ZBC_MASK(entries);

	pmu->zbc_save_done = 0;

	gk20a_dbg_pmu("cmd post ZBC_TABLE_UPDATE");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			   pmu_handle_zbc_msg, pmu, &seq, ~0);
	pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
			      &pmu->zbc_save_done, 1);
	if (!pmu->zbc_save_done)
		gk20a_err(dev_from_gk20a(g), "ZBC save timeout");
}

static int pmu_perfmon_start_sampling(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_v *pv = &g->ops.pmu_ver;
	struct pmu_cmd cmd;
	struct pmu_payload payload;
	u32 seq;

	/* PERFMON Start */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = get_perfmon_id(pmu);
	cmd.hdr.size = PMU_CMD_HDR_SIZE + pv->get_pmu_perfmon_cmd_start_size();
	pv->perfmon_start_set_cmd_type(&cmd.cmd.perfmon,
		PMU_PERFMON_CMD_ID_START);
	pv->perfmon_start_set_group_id(&cmd.cmd.perfmon,
		PMU_DOMAIN_GROUP_PSTATE);
	pv->perfmon_start_set_state_id(&cmd.cmd.perfmon,
		pmu->perfmon_state_id[PMU_DOMAIN_GROUP_PSTATE]);

	pv->perfmon_start_set_flags(&cmd.cmd.perfmon,
		PMU_PERFMON_FLAG_ENABLE_INCREASE |
		PMU_PERFMON_FLAG_ENABLE_DECREASE |
		PMU_PERFMON_FLAG_CLEAR_PREV);

	memset(&payload, 0, sizeof(struct pmu_payload));

	/* TBD: PMU_PERFMON_PCT_TO_INC * 100 */
	pv->set_perfmon_cntr_ut(pmu, 3000); /* 30% */
	/* TBD: PMU_PERFMON_PCT_TO_DEC * 100 */
	pv->set_perfmon_cntr_lt(pmu, 1000); /* 10% */
	pv->set_perfmon_cntr_valid(pmu, true);

	payload.in.buf = pv->get_perfmon_cntr_ptr(pmu);
	payload.in.size = pv->get_perfmon_cntr_sz(pmu);
	payload.in.offset =
		pv->get_perfmon_cmd_start_offsetofvar(COUNTER_ALLOC);

	gk20a_dbg_pmu("cmd post PMU_PERFMON_CMD_ID_START");
	gk20a_pmu_cmd_post(g, &cmd, NULL, &payload, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);

	return 0;
}

static int pmu_perfmon_stop_sampling(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_cmd cmd;
	u32 seq;

	/* PERFMON Stop */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = get_perfmon_id(pmu);
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_perfmon_cmd_stop);
	cmd.cmd.perfmon.stop.cmd_type = PMU_PERFMON_CMD_ID_STOP;

	gk20a_dbg_pmu("cmd post PMU_PERFMON_CMD_ID_STOP");
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);
	return 0;
}

static int pmu_handle_perfmon_event(struct pmu_gk20a *pmu,
			struct pmu_perfmon_msg *msg)
{
	gk20a_dbg_fn("");

	switch (msg->msg_type) {
	case PMU_PERFMON_MSG_ID_INCREASE_EVENT:
		gk20a_dbg_pmu("perfmon increase event: "
			"state_id %d, ground_id %d, pct %d",
			msg->gen.state_id, msg->gen.group_id, msg->gen.data);
		(pmu->perfmon_events_cnt)++;
		break;
	case PMU_PERFMON_MSG_ID_DECREASE_EVENT:
		gk20a_dbg_pmu("perfmon decrease event: "
			"state_id %d, ground_id %d, pct %d",
			msg->gen.state_id, msg->gen.group_id, msg->gen.data);
		(pmu->perfmon_events_cnt)++;
		break;
	case PMU_PERFMON_MSG_ID_INIT_EVENT:
		pmu->perfmon_ready = 1;
		gk20a_dbg_pmu("perfmon init event");
		break;
	default:
		break;
	}

	/* restart sampling */
	if (pmu->perfmon_sampling_enabled)
		return pmu_perfmon_start_sampling(pmu);
	return 0;
}


static int pmu_handle_therm_event(struct pmu_gk20a *pmu,
			struct nv_pmu_therm_msg *msg)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	gk20a_dbg_fn("");

	switch (msg->msg_type) {
	case NV_PMU_THERM_MSG_ID_EVENT_HW_SLOWDOWN_NOTIFICATION:
		if (msg->hw_slct_msg.mask == BIT(NV_PMU_THERM_EVENT_THERMAL_1))
			nvgpu_clk_arb_schedule_alarm(g,
				(0x1UL << NVGPU_GPU_EVENT_ALARM_THERMAL_ABOVE_THRESHOLD));
		else
			gk20a_dbg_pmu("Unwanted/Unregistered thermal event received %d",
				msg->hw_slct_msg.mask);
		break;
	default:
		gk20a_dbg_pmu("unkown therm event received %d", msg->msg_type);
		break;
	}

	return 0;
}

static int pmu_handle_event(struct pmu_gk20a *pmu, struct pmu_msg *msg)
{
	int err = 0;
	struct gk20a *g = gk20a_from_pmu(pmu);

	gk20a_dbg_fn("");
	switch (msg->hdr.unit_id) {
	case PMU_UNIT_PERFMON:
	case PMU_UNIT_PERFMON_T18X:
		err = pmu_handle_perfmon_event(pmu, &msg->msg.perfmon);
		break;
	case PMU_UNIT_PERF:
		if (g->ops.perf.handle_pmu_perf_event != NULL) {
			err = g->ops.perf.handle_pmu_perf_event(g,
				(void *)&msg->msg.perf);
		} else {
			WARN_ON(1);
		}
		break;
	case PMU_UNIT_THERM:
		err = pmu_handle_therm_event(pmu, &msg->msg.therm);
		break;
	default:
		break;
	}

	return err;
}

static int pmu_process_message(struct pmu_gk20a *pmu)
{
	struct pmu_msg msg;
	int status;
	struct gk20a *g = gk20a_from_pmu(pmu);

	if (unlikely(!pmu->pmu_ready)) {
		pmu_process_init_msg(pmu, &msg);
		if (g->ops.pmu.init_wpr_region != NULL)
			g->ops.pmu.init_wpr_region(g);
		pmu_init_perfmon(pmu);

		return 0;
	}

	while (pmu_read_message(pmu,
		&pmu->queue[PMU_MESSAGE_QUEUE], &msg, &status)) {

		gk20a_dbg_pmu("read msg hdr: "
				"unit_id = 0x%08x, size = 0x%08x, "
				"ctrl_flags = 0x%08x, seq_id = 0x%08x",
				msg.hdr.unit_id, msg.hdr.size,
				msg.hdr.ctrl_flags, msg.hdr.seq_id);

		msg.hdr.ctrl_flags &= ~PMU_CMD_FLAGS_PMU_MASK;

		if (msg.hdr.ctrl_flags == PMU_CMD_FLAGS_EVENT) {
			pmu_handle_event(pmu, &msg);
		} else {
			pmu_response_handle(pmu, &msg);
		}
	}

	return 0;
}

int pmu_wait_message_cond(struct pmu_gk20a *pmu, u32 timeout,
				 u32 *var, u32 val)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);
	unsigned long delay = GR_IDLE_CHECK_DEFAULT;
	u32 servicedpmuint;

	servicedpmuint = pwr_falcon_irqstat_halt_true_f() |
				pwr_falcon_irqstat_exterr_true_f() |
				pwr_falcon_irqstat_swgen0_true_f();
	do {
		if (*var == val)
			return 0;

		if (gk20a_readl(g, pwr_falcon_irqstat_r()) & servicedpmuint)
			gk20a_pmu_isr(g);

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	} while (time_before(jiffies, end_jiffies) ||
			!tegra_platform_is_silicon());

	return -ETIMEDOUT;
}

static void pmu_dump_elpg_stats(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_pg_stats stats;

	pmu_copy_from_dmem(pmu,
		pmu->stat_dmem_offset[PMU_PG_ELPG_ENGINE_ID_GRAPHICS],
		(u8 *)&stats, sizeof(struct pmu_pg_stats), 0);

	gk20a_dbg_pmu("pg_entry_start_timestamp : 0x%016llx",
		stats.pg_entry_start_timestamp);
	gk20a_dbg_pmu("pg_exit_start_timestamp : 0x%016llx",
		stats.pg_exit_start_timestamp);
	gk20a_dbg_pmu("pg_ingating_start_timestamp : 0x%016llx",
		stats.pg_ingating_start_timestamp);
	gk20a_dbg_pmu("pg_ungating_start_timestamp : 0x%016llx",
		stats.pg_ungating_start_timestamp);
	gk20a_dbg_pmu("pg_avg_entry_time_us : 0x%08x",
		stats.pg_avg_entry_time_us);
	gk20a_dbg_pmu("pg_avg_exit_time_us : 0x%08x",
		stats.pg_avg_exit_time_us);
	gk20a_dbg_pmu("pg_ingating_cnt : 0x%08x",
		stats.pg_ingating_cnt);
	gk20a_dbg_pmu("pg_ingating_time_us : 0x%08x",
		stats.pg_ingating_time_us);
	gk20a_dbg_pmu("pg_ungating_count : 0x%08x",
		stats.pg_ungating_count);
	gk20a_dbg_pmu("pg_ungating_time_us 0x%08x: ",
		stats.pg_ungating_time_us);
	gk20a_dbg_pmu("pg_gating_cnt : 0x%08x",
		stats.pg_gating_cnt);
	gk20a_dbg_pmu("pg_gating_deny_cnt : 0x%08x",
		stats.pg_gating_deny_cnt);

	/*
	   Turn on PG_DEBUG in ucode and locate symbol "ElpgLog" offset
	   in .nm file, e.g. 0x1000066c. use 0x66c.
	u32 i, val[20];
	pmu_copy_from_dmem(pmu, 0x66c,
		(u8 *)val, sizeof(val), 0);
	gk20a_dbg_pmu("elpg log begin");
	for (i = 0; i < 20; i++)
		gk20a_dbg_pmu("0x%08x", val[i]);
	gk20a_dbg_pmu("elpg log end");
	*/

	gk20a_dbg_pmu("pwr_pmu_idle_mask_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_supp_r(3)));
	gk20a_dbg_pmu("pwr_pmu_idle_mask_1_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_1_supp_r(3)));
	gk20a_dbg_pmu("pwr_pmu_idle_ctrl_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_ctrl_supp_r(3)));
	gk20a_dbg_pmu("pwr_pmu_pg_idle_cnt_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_idle_cnt_r(0)));
	gk20a_dbg_pmu("pwr_pmu_pg_intren_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_intren_r(0)));

	gk20a_dbg_pmu("pwr_pmu_idle_count_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(3)));
	gk20a_dbg_pmu("pwr_pmu_idle_count_r(4): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(4)));
	gk20a_dbg_pmu("pwr_pmu_idle_count_r(7): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(7)));

	/*
	 TBD: script can't generate those registers correctly
	gk20a_dbg_pmu("pwr_pmu_idle_status_r(): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_status_r()));
	gk20a_dbg_pmu("pwr_pmu_pg_ctrl_r(): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_ctrl_r()));
	*/
}

void pmu_dump_falcon_stats(struct pmu_gk20a *pmu)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	int i;

	gk20a_err(dev_from_gk20a(g), "pwr_falcon_os_r : %d",
		gk20a_readl(g, pwr_falcon_os_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_cpuctl_r : 0x%x",
		gk20a_readl(g, pwr_falcon_cpuctl_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_idlestate_r : 0x%x",
		gk20a_readl(g, pwr_falcon_idlestate_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_mailbox0_r : 0x%x",
		gk20a_readl(g, pwr_falcon_mailbox0_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_mailbox1_r : 0x%x",
		gk20a_readl(g, pwr_falcon_mailbox1_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_irqstat_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqstat_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_irqmode_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqmode_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_irqmask_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqmask_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_irqdest_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqdest_r()));

	for (i = 0; i < pwr_pmu_mailbox__size_1_v(); i++)
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_mailbox_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_mailbox_r(i)));

	for (i = 0; i < pwr_pmu_debug__size_1_v(); i++)
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_debug_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_debug_r(i)));

	for (i = 0; i < 6/*NV_PPWR_FALCON_ICD_IDX_RSTAT__SIZE_1*/; i++) {
		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rstat_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(i));
		gk20a_err(dev_from_gk20a(g), "pmu_rstat (%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_error_status_r());
	gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_error_status_r : 0x%x", i);
	if (i != 0) {
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_addr_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_addr_r()));
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_data_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_data_r()));
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_timeout_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_timeout_r()));
		gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_ctl_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_ctl_r()));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_fecs_error_r());
	gk20a_err(dev_from_gk20a(g), "pwr_pmu_bar0_fecs_error_r : 0x%x", i);

	i = gk20a_readl(g, pwr_falcon_exterrstat_r());
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_exterrstat_r : 0x%x", i);
	if (pwr_falcon_exterrstat_valid_v(i) ==
			pwr_falcon_exterrstat_valid_true_v()) {
		gk20a_err(dev_from_gk20a(g), "pwr_falcon_exterraddr_r : 0x%x",
			gk20a_readl(g, pwr_falcon_exterraddr_r()));
		gk20a_err(dev_from_gk20a(g), "pmc_enable : 0x%x",
			gk20a_readl(g, mc_enable_r()));
	}

	gk20a_err(dev_from_gk20a(g), "pwr_falcon_engctl_r : 0x%x",
		gk20a_readl(g, pwr_falcon_engctl_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_curctx_r : 0x%x",
		gk20a_readl(g, pwr_falcon_curctx_r()));
	gk20a_err(dev_from_gk20a(g), "pwr_falcon_nxtctx_r : 0x%x",
		gk20a_readl(g, pwr_falcon_nxtctx_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_IMB));
	gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_IMB : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_DMB));
	gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_DMB : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_CSW));
	gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_CSW : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_CTX));
	gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_CTX : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_EXCI));
	gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_EXCI : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	for (i = 0; i < 4; i++) {
		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_PC));
		gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_PC : 0x%x",
			gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_SP));
		gk20a_err(dev_from_gk20a(g), "PMU_FALCON_REG_SP : 0x%x",
			gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));
	}
	gk20a_err(dev_from_gk20a(g), "elpg stat: %d\n",
			pmu->elpg_stat);

	/* PMU may crash due to FECS crash. Dump FECS status */
	gk20a_fecs_dump_falcon_stats(g);
	printtrace(pmu);
}

void gk20a_pmu_isr(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_queue *queue;
	u32 intr, mask;
	bool recheck = false;

	gk20a_dbg_fn("");

	mutex_lock(&pmu->isr_mutex);
	if (!pmu->isr_enabled) {
		mutex_unlock(&pmu->isr_mutex);
		return;
	}

	mask = gk20a_readl(g, pwr_falcon_irqmask_r()) &
		gk20a_readl(g, pwr_falcon_irqdest_r());

	intr = gk20a_readl(g, pwr_falcon_irqstat_r());

	gk20a_dbg_pmu("received falcon interrupt: 0x%08x", intr);

	intr = gk20a_readl(g, pwr_falcon_irqstat_r()) & mask;
	if (!intr || pmu->pmu_state == PMU_STATE_OFF) {
		gk20a_writel(g, pwr_falcon_irqsclr_r(), intr);
		mutex_unlock(&pmu->isr_mutex);
		return;
	}

	if (intr & pwr_falcon_irqstat_halt_true_f()) {
		gk20a_err(dev_from_gk20a(g),
			"pmu halt intr not implemented");
		pmu_dump_falcon_stats(pmu);
		if (gk20a_readl(g, pwr_pmu_mailbox_r
				(PMU_MODE_MISMATCH_STATUS_MAILBOX_R)) ==
				PMU_MODE_MISMATCH_STATUS_VAL)
			if (g->ops.pmu.dump_secure_fuses)
				g->ops.pmu.dump_secure_fuses(g);
	}
	if (intr & pwr_falcon_irqstat_exterr_true_f()) {
		gk20a_err(dev_from_gk20a(g),
			"pmu exterr intr not implemented. Clearing interrupt.");
		pmu_dump_falcon_stats(pmu);

		gk20a_writel(g, pwr_falcon_exterrstat_r(),
			gk20a_readl(g, pwr_falcon_exterrstat_r()) &
				~pwr_falcon_exterrstat_valid_m());
	}
	if (intr & pwr_falcon_irqstat_swgen0_true_f()) {
		pmu_process_message(pmu);
		recheck = true;
	}

	gk20a_writel(g, pwr_falcon_irqsclr_r(), intr);

	if (recheck) {
		queue = &pmu->queue[PMU_MESSAGE_QUEUE];
		if (!pmu_queue_is_empty(pmu, queue))
			gk20a_writel(g, pwr_falcon_irqsset_r(),
				pwr_falcon_irqsset_swgen0_set_f());
	}

	mutex_unlock(&pmu->isr_mutex);
}

static bool pmu_validate_cmd(struct pmu_gk20a *pmu, struct pmu_cmd *cmd,
			struct pmu_msg *msg, struct pmu_payload *payload,
			u32 queue_id)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_queue *queue;
	u32 in_size, out_size;

	if (!PMU_IS_SW_COMMAND_QUEUE(queue_id))
		goto invalid_cmd;

	queue = &pmu->queue[queue_id];
	if (cmd->hdr.size < PMU_CMD_HDR_SIZE)
		goto invalid_cmd;

	if (cmd->hdr.size > (queue->size >> 1))
		goto invalid_cmd;

	if (msg != NULL && msg->hdr.size < PMU_MSG_HDR_SIZE)
		goto invalid_cmd;

	if (!PMU_UNIT_ID_IS_VALID(cmd->hdr.unit_id))
		goto invalid_cmd;

	if (payload == NULL)
		return true;

	if (payload->in.buf == NULL && payload->out.buf == NULL)
		goto invalid_cmd;

	if ((payload->in.buf != NULL && payload->in.size == 0) ||
	    (payload->out.buf != NULL && payload->out.size == 0))
		goto invalid_cmd;

	in_size = PMU_CMD_HDR_SIZE;
	if (payload->in.buf) {
		in_size += payload->in.offset;
		in_size += g->ops.pmu_ver.get_pmu_allocation_struct_size(pmu);
	}

	out_size = PMU_CMD_HDR_SIZE;
	if (payload->out.buf) {
		out_size += payload->out.offset;
		out_size += g->ops.pmu_ver.get_pmu_allocation_struct_size(pmu);
	}

	if (in_size > cmd->hdr.size || out_size > cmd->hdr.size)
		goto invalid_cmd;


	if ((payload->in.offset != 0 && payload->in.buf == NULL) ||
	    (payload->out.offset != 0 && payload->out.buf == NULL))
		goto invalid_cmd;

	return true;

invalid_cmd:
	gk20a_err(dev_from_gk20a(g), "invalid pmu cmd :\n"
		"queue_id=%d,\n"
		"cmd_size=%d, cmd_unit_id=%d, msg=%p, msg_size=%d,\n"
		"payload in=%p, in_size=%d, in_offset=%d,\n"
		"payload out=%p, out_size=%d, out_offset=%d",
		queue_id, cmd->hdr.size, cmd->hdr.unit_id,
		msg, msg?msg->hdr.unit_id:~0,
		&payload->in, payload->in.size, payload->in.offset,
		&payload->out, payload->out.size, payload->out.offset);

	return false;
}

static int pmu_write_cmd(struct pmu_gk20a *pmu, struct pmu_cmd *cmd,
			u32 queue_id, unsigned long timeout)
{
	struct gk20a *g = gk20a_from_pmu(pmu);
	struct pmu_queue *queue;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(timeout);
	int err;

	gk20a_dbg_fn("");

	queue = &pmu->queue[queue_id];


	do {
		err = pmu_queue_open_write(pmu, queue, cmd->hdr.size);
		if (err == -EAGAIN && time_before(jiffies, end_jiffies))
			usleep_range(1000, 2000);
		else
			break;
	} while (1);

	if (err)
		goto clean_up;

	pmu_queue_push(pmu, queue, cmd, cmd->hdr.size);


	err = pmu_queue_close(pmu, queue, true);

clean_up:
	if (err)
		gk20a_err(dev_from_gk20a(g),
			"fail to write cmd to queue %d", queue_id);
	else
		gk20a_dbg_fn("done");

	return err;
}

void gk20a_pmu_surface_describe(struct gk20a *g, struct mem_desc *mem,
		struct flcn_mem_desc_v0 *fb)
{
	fb->address.lo = u64_lo32(mem->gpu_va);
	fb->address.hi = u64_hi32(mem->gpu_va);
	fb->params = ((u32)mem->size & 0xFFFFFF);
	fb->params |= (GK20A_PMU_DMAIDX_VIRT << 24);
}

int gk20a_pmu_vidmem_surface_alloc(struct gk20a *g, struct mem_desc *mem,
		u32 size)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	int err;

	err = gk20a_gmmu_alloc_map_vid(vm, size, mem);
	if (err) {
		gk20a_err(g->dev, "memory allocation failed");
		return -ENOMEM;
	}

	return 0;
}

int gk20a_pmu_sysmem_surface_alloc(struct gk20a *g, struct mem_desc *mem,
		u32 size)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	int err;

	err = gk20a_gmmu_alloc_map_sys(vm, size, mem);
	if (err) {
		gk20a_err(g->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	return 0;
}

void gk20a_pmu_surface_free(struct gk20a *g, struct mem_desc *mem)
{
	gk20a_gmmu_free_attr(g, DMA_ATTR_NO_KERNEL_MAPPING, mem);
	memset(mem, 0, sizeof(struct mem_desc));
}

int gk20a_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd,
		struct pmu_msg *msg, struct pmu_payload *payload,
		u32 queue_id, pmu_callback callback, void* cb_param,
		u32 *seq_desc, unsigned long timeout)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_v *pv = &g->ops.pmu_ver;
	struct pmu_sequence *seq;
	void *in = NULL, *out = NULL;
	int err;

	gk20a_dbg_fn("");

	if ((!cmd) || (!seq_desc) || (!pmu->pmu_ready)) {
		if (!cmd)
			gk20a_warn(dev_from_gk20a(g),
			"%s(): PMU cmd buffer is NULL", __func__);
		else if (!seq_desc)
			gk20a_warn(dev_from_gk20a(g),
			"%s(): Seq descriptor is NULL", __func__);
		else
			gk20a_warn(dev_from_gk20a(g),
			"%s(): PMU is not ready", __func__);

		WARN_ON(1);
		return -EINVAL;
	}

	if (!pmu_validate_cmd(pmu, cmd, msg, payload, queue_id))
		return -EINVAL;

	err = pmu_seq_acquire(pmu, &seq);
	if (err)
		return err;

	cmd->hdr.seq_id = seq->id;

	cmd->hdr.ctrl_flags = 0;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_STATUS;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_INTR;

	seq->callback = callback;
	seq->cb_params = cb_param;
	seq->msg = msg;
	seq->out_payload = NULL;
	seq->desc = pmu->next_seq_desc++;

	if (payload)
		seq->out_payload = payload->out.buf;

	*seq_desc = seq->desc;

	if (payload && payload->in.offset != 0) {
		pv->set_pmu_allocation_ptr(pmu, &in,
		((u8 *)&cmd->cmd + payload->in.offset));

		if (payload->in.buf != payload->out.buf)
			pv->pmu_allocation_set_dmem_size(pmu, in,
			(u16)payload->in.size);
		else
			pv->pmu_allocation_set_dmem_size(pmu, in,
			(u16)max(payload->in.size, payload->out.size));

		*(pv->pmu_allocation_get_dmem_offset_addr(pmu, in)) =
			gk20a_alloc(&pmu->dmem,
				     pv->pmu_allocation_get_dmem_size(pmu, in));
		if (!*(pv->pmu_allocation_get_dmem_offset_addr(pmu, in)))
			goto clean_up;

		if (payload->in.fb_size != 0x0) {
			seq->in_mem = kzalloc(sizeof(struct mem_desc),
				GFP_KERNEL);
			if (!seq->in_mem) {
				err = -ENOMEM;
				goto clean_up;
			}

			gk20a_pmu_vidmem_surface_alloc(g, seq->in_mem,
					payload->in.fb_size);
			gk20a_pmu_surface_describe(g, seq->in_mem,
					(struct flcn_mem_desc_v0 *)
					pv->pmu_allocation_get_fb_addr(pmu, in));

			gk20a_mem_wr_n(g, seq->in_mem, 0,
					payload->in.buf, payload->in.fb_size);

		} else {
			pmu_copy_to_dmem(pmu,
				(pv->pmu_allocation_get_dmem_offset(pmu, in)),
				payload->in.buf, payload->in.size, 0);
		}
		pv->pmu_allocation_set_dmem_size(pmu,
		pv->get_pmu_seq_in_a_ptr(seq),
		pv->pmu_allocation_get_dmem_size(pmu, in));
		pv->pmu_allocation_set_dmem_offset(pmu,
		pv->get_pmu_seq_in_a_ptr(seq),
		pv->pmu_allocation_get_dmem_offset(pmu, in));
	}

	if (payload && payload->out.offset != 0) {
		pv->set_pmu_allocation_ptr(pmu, &out,
		((u8 *)&cmd->cmd + payload->out.offset));
		pv->pmu_allocation_set_dmem_size(pmu, out,
		(u16)payload->out.size);

		if (payload->in.buf != payload->out.buf) {
			*(pv->pmu_allocation_get_dmem_offset_addr(pmu, out)) =
				gk20a_alloc(&pmu->dmem,
				    pv->pmu_allocation_get_dmem_size(pmu, out));
			if (!*(pv->pmu_allocation_get_dmem_offset_addr(pmu,
					out)))
				goto clean_up;

			if (payload->out.fb_size != 0x0) {
				seq->out_mem = kzalloc(sizeof(struct mem_desc),
					GFP_KERNEL);
				if (!seq->out_mem) {
					err = -ENOMEM;
					goto clean_up;
				}
				gk20a_pmu_vidmem_surface_alloc(g, seq->out_mem,
					payload->out.fb_size);
				gk20a_pmu_surface_describe(g, seq->out_mem,
					(struct flcn_mem_desc_v0 *)
					pv->pmu_allocation_get_fb_addr(pmu,
					out));
			}
		} else {
			BUG_ON(in == NULL);
			seq->out_mem = seq->in_mem;
			pv->pmu_allocation_set_dmem_offset(pmu, out,
			pv->pmu_allocation_get_dmem_offset(pmu, in));
		}
		pv->pmu_allocation_set_dmem_size(pmu,
		pv->get_pmu_seq_out_a_ptr(seq),
		pv->pmu_allocation_get_dmem_size(pmu, out));
		pv->pmu_allocation_set_dmem_offset(pmu,
		pv->get_pmu_seq_out_a_ptr(seq),
		pv->pmu_allocation_get_dmem_offset(pmu, out));

	}



	seq->state = PMU_SEQ_STATE_USED;

	err = pmu_write_cmd(pmu, cmd, queue_id, timeout);
	if (err)
		seq->state = PMU_SEQ_STATE_PENDING;

	gk20a_dbg_fn("done");

	return 0;

clean_up:
	gk20a_dbg_fn("fail");
	if (in)
		gk20a_free(&pmu->dmem,
			pv->pmu_allocation_get_dmem_offset(pmu, in));
	if (out)
		gk20a_free(&pmu->dmem,
			pv->pmu_allocation_get_dmem_offset(pmu, out));

	pmu_seq_release(pmu, seq);
	return err;
}

int gk20a_pmu_pg_global_enable(struct gk20a *g, u32 enable_pg)
{
	u32 status = 0;

	if (enable_pg == true) {
		if (g->ops.pmu.pmu_pg_engines_feature_list &&
			g->ops.pmu.pmu_pg_engines_feature_list(g,
			PMU_PG_ELPG_ENGINE_ID_GRAPHICS) !=
			PMU_PG_FEATURE_GR_POWER_GATING_ENABLED) {
			status = nvgpu_lpwr_enable_pg(g, true);
		} else if (support_gk20a_pmu(g->dev))
			status = gk20a_pmu_enable_elpg(g);
	} else if (enable_pg == false) {
		if (g->ops.pmu.pmu_pg_engines_feature_list &&
			g->ops.pmu.pmu_pg_engines_feature_list(g,
			PMU_PG_ELPG_ENGINE_ID_GRAPHICS) !=
			PMU_PG_FEATURE_GR_POWER_GATING_ENABLED) {
			status = nvgpu_lpwr_disable_pg(g, true);
		} else if (support_gk20a_pmu(g->dev))
			status = gk20a_pmu_disable_elpg(g);
	}

	return status;
}

static int gk20a_pmu_enable_elpg_locked(struct gk20a *g, u32 pg_engine_id)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq, status;

	gk20a_dbg_fn("");

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE +
		sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_ALLOW;

   /* no need to wait ack for ELPG enable but set
	* pending to sync with follow up ELPG disable
	*/
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
		pmu->elpg_stat = PMU_ELPG_STAT_ON_PENDING;

	else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
		pmu->mscg_transition_state = PMU_ELPG_STAT_ON_PENDING;

	gk20a_dbg_pmu("cmd post PMU_PG_ELPG_CMD_ALLOW");
	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL,
		PMU_COMMAND_QUEUE_HPQ, pmu_handle_pg_elpg_msg,
		pmu, &seq, ~0);
	WARN_ON(status != 0);

	gk20a_dbg_fn("done");
	return 0;
}

int gk20a_pmu_enable_elpg(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct gr_gk20a *gr = &g->gr;
	u32 pg_engine_id;
	u32 pg_engine_id_list = 0;

	int ret = 0;

	gk20a_dbg_fn("");

	if (!support_gk20a_pmu(g->dev))
		return ret;

	mutex_lock(&pmu->elpg_mutex);

	pmu->elpg_refcnt++;
	if (pmu->elpg_refcnt <= 0)
		goto exit_unlock;

	/* something is not right if we end up in following code path */
	if (unlikely(pmu->elpg_refcnt > 1)) {
		gk20a_warn(dev_from_gk20a(g),
		"%s(): possible elpg refcnt mismatch. elpg refcnt=%d",
		__func__, pmu->elpg_refcnt);
		WARN_ON(1);
	}

	/* do NOT enable elpg until golden ctx is created,
	   which is related with the ctx that ELPG save and restore. */
	if (unlikely(!gr->ctx_vars.golden_image_initialized))
		goto exit_unlock;

	/* return if ELPG is already on or on_pending or off_on_pending */
	if (pmu->elpg_stat != PMU_ELPG_STAT_OFF)
		goto exit_unlock;

	if (g->ops.pmu.pmu_pg_supported_engines_list)
		pg_engine_id_list = g->ops.pmu.pmu_pg_supported_engines_list(g);

	for (pg_engine_id = PMU_PG_ELPG_ENGINE_ID_GRAPHICS;
		pg_engine_id < PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE;
		pg_engine_id++) {

		if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS &&
			ACCESS_ONCE(pmu->mscg_stat) == PMU_MSCG_DISABLED)
			continue;

		if (BIT(pg_engine_id) & pg_engine_id_list)
			ret = gk20a_pmu_enable_elpg_locked(g, pg_engine_id);
	}

exit_unlock:
	mutex_unlock(&pmu->elpg_mutex);
	gk20a_dbg_fn("done");
	return ret;
}

int gk20a_pmu_disable_elpg(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;
	int ret = 0;
	u32 pg_engine_id;
	u32 pg_engine_id_list = 0;
	u32 *ptr = NULL;

	gk20a_dbg_fn("");

	if (g->ops.pmu.pmu_pg_supported_engines_list)
		pg_engine_id_list = g->ops.pmu.pmu_pg_supported_engines_list(g);

	if (!support_gk20a_pmu(g->dev))
		return ret;

	mutex_lock(&pmu->elpg_mutex);

	pmu->elpg_refcnt--;
	if (pmu->elpg_refcnt > 0) {
		gk20a_warn(dev_from_gk20a(g),
		"%s(): possible elpg refcnt mismatch. elpg refcnt=%d",
		__func__, pmu->elpg_refcnt);
		WARN_ON(1);
		ret = 0;
		goto exit_unlock;
	}

	/* cancel off_on_pending and return */
	if (pmu->elpg_stat == PMU_ELPG_STAT_OFF_ON_PENDING) {
		pmu->elpg_stat = PMU_ELPG_STAT_OFF;
		ret = 0;
		goto exit_reschedule;
	}
	/* wait if on_pending */
	else if (pmu->elpg_stat == PMU_ELPG_STAT_ON_PENDING) {

		pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
				      &pmu->elpg_stat, PMU_ELPG_STAT_ON);

		if (pmu->elpg_stat != PMU_ELPG_STAT_ON) {
			gk20a_err(dev_from_gk20a(g),
				"ELPG_ALLOW_ACK failed, elpg_stat=%d",
				pmu->elpg_stat);
			pmu_dump_elpg_stats(pmu);
			pmu_dump_falcon_stats(pmu);
			ret = -EBUSY;
			goto exit_unlock;
		}
	}
	/* return if ELPG is already off */
	else if (pmu->elpg_stat != PMU_ELPG_STAT_ON) {
		ret = 0;
		goto exit_reschedule;
	}

	for (pg_engine_id = PMU_PG_ELPG_ENGINE_ID_GRAPHICS;
		pg_engine_id < PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE;
		pg_engine_id++) {

		if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS &&
			ACCESS_ONCE(pmu->mscg_stat) == PMU_MSCG_DISABLED)
			continue;

		if (BIT(pg_engine_id) & pg_engine_id_list) {
			memset(&cmd, 0, sizeof(struct pmu_cmd));
			cmd.hdr.unit_id = PMU_UNIT_PG;
			cmd.hdr.size = PMU_CMD_HDR_SIZE +
				sizeof(struct pmu_pg_cmd_elpg_cmd);
			cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
			cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
			cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_DISALLOW;

			if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
				pmu->elpg_stat = PMU_ELPG_STAT_OFF_PENDING;
			else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
				pmu->mscg_transition_state =
					PMU_ELPG_STAT_OFF_PENDING;

			if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS)
				ptr = &pmu->elpg_stat;
			else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS)
				ptr = &pmu->mscg_transition_state;

			gk20a_dbg_pmu("cmd post PMU_PG_ELPG_CMD_DISALLOW");
			gk20a_pmu_cmd_post(g, &cmd, NULL, NULL,
				PMU_COMMAND_QUEUE_HPQ, pmu_handle_pg_elpg_msg,
				pmu, &seq, ~0);

			pmu_wait_message_cond(pmu,
				gk20a_get_gr_idle_timeout(g),
				ptr, PMU_ELPG_STAT_OFF);
			if (*ptr != PMU_ELPG_STAT_OFF) {
				gk20a_err(dev_from_gk20a(g),
					"ELPG_DISALLOW_ACK failed");
					pmu_dump_elpg_stats(pmu);
					pmu_dump_falcon_stats(pmu);
				ret = -EBUSY;
				goto exit_unlock;
			}
		}
	}

exit_reschedule:
exit_unlock:
	mutex_unlock(&pmu->elpg_mutex);
	gk20a_dbg_fn("done");
	return ret;
}

int gk20a_pmu_perfmon_enable(struct gk20a *g, bool enable)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err;

	gk20a_dbg_fn("");

	if (enable)
		err = pmu_perfmon_start_sampling(pmu);
	else
		err = pmu_perfmon_stop_sampling(pmu);

	return err;
}

int gk20a_pmu_destroy(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_pg_stats_data pg_stat_data = { 0 };

	gk20a_dbg_fn("");

	if (!support_gk20a_pmu(g->dev))
		return 0;

	/* make sure the pending operations are finished before we continue */
	cancel_work_sync(&pmu->pg_init);

	gk20a_pmu_get_pg_stats(g,
		PMU_PG_ELPG_ENGINE_ID_GRAPHICS,	&pg_stat_data);

	gk20a_pmu_disable_elpg(g);
	pmu->initialized = false;

	/* update the s/w ELPG residency counters */
	g->pg_ingating_time_us += (u64)pg_stat_data.ingating_time;
	g->pg_ungating_time_us += (u64)pg_stat_data.ungating_time;
	g->pg_gating_cnt += pg_stat_data.gating_cnt;

	mutex_lock(&pmu->isr_mutex);
	pmu->isr_enabled = false;
	mutex_unlock(&pmu->isr_mutex);

	pmu->pmu_state = PMU_STATE_OFF;
	pmu->pmu_ready = false;
	pmu->perfmon_ready = false;
	pmu->zbc_ready = false;
	g->ops.pmu.lspmuwprinitdone = false;
	g->ops.pmu.fecsbootstrapdone = false;

	gk20a_dbg_fn("done");
	return 0;
}

int gk20a_pmu_load_norm(struct gk20a *g, u32 *load)
{
	*load = g->pmu.load_shadow;
	return 0;
}

int gk20a_pmu_load_update(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u16 _load = 0;

	if (!pmu->perfmon_ready) {
		pmu->load_shadow = 0;
		return 0;
	}

	pmu_copy_from_dmem(pmu, pmu->sample_buffer, (u8 *)&_load, 2, 0);
	pmu->load_shadow = _load / 10;
	pmu->load_avg = (((9*pmu->load_avg) + pmu->load_shadow) / 10);

	return 0;
}

void gk20a_pmu_get_load_counters(struct gk20a *g, u32 *busy_cycles,
				 u32 *total_cycles)
{
	if (!g->power_on || gk20a_busy(g->dev)) {
		*busy_cycles = 0;
		*total_cycles = 0;
		return;
	}

	*busy_cycles = pwr_pmu_idle_count_value_v(
		gk20a_readl(g, pwr_pmu_idle_count_r(1)));
	rmb();
	*total_cycles = pwr_pmu_idle_count_value_v(
		gk20a_readl(g, pwr_pmu_idle_count_r(2)));
	gk20a_idle(g->dev);
}

void gk20a_pmu_reset_load_counters(struct gk20a *g)
{
	u32 reg_val = pwr_pmu_idle_count_reset_f(1);

	if (!g->power_on || gk20a_busy(g->dev))
		return;

	gk20a_writel(g, pwr_pmu_idle_count_r(2), reg_val);
	wmb();
	gk20a_writel(g, pwr_pmu_idle_count_r(1), reg_val);
	gk20a_idle(g->dev);
}

void gk20a_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_pg_stats stats;

	pmu_copy_from_dmem(pmu,
		pmu->stat_dmem_offset[pg_engine_id],
		(u8 *)&stats, sizeof(struct pmu_pg_stats), 0);

	pg_stat_data->ingating_time = stats.pg_ingating_time_us;
	pg_stat_data->ungating_time = stats.pg_ungating_time_us;
	pg_stat_data->gating_cnt = stats.pg_gating_cnt;
	pg_stat_data->avg_entry_latency_us = stats.pg_avg_entry_time_us;
	pg_stat_data->avg_exit_latency_us = stats.pg_avg_exit_time_us;
}

static int gk20a_pmu_get_pg_stats(struct gk20a *g,
		u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32 pg_engine_id_list = 0;

	if (!pmu->initialized) {
		pg_stat_data->ingating_time = 0;
		pg_stat_data->ungating_time = 0;
		pg_stat_data->gating_cnt = 0;
		return 0;
	}

	if (g->ops.pmu.pmu_pg_supported_engines_list)
		pg_engine_id_list = g->ops.pmu.pmu_pg_supported_engines_list(g);

	if (BIT(pg_engine_id) & pg_engine_id_list)
		g->ops.pmu.pmu_elpg_statistics(g, pg_engine_id,
			pg_stat_data);

	return 0;
}

/* Send an Adaptive Power (AP) related command to PMU */
int gk20a_pmu_ap_send_command(struct gk20a *g,
			union pmu_ap_cmd *p_ap_cmd, bool b_block)
{
	struct pmu_gk20a *pmu = &g->pmu;
	/* FIXME: where is the PG structure defined?? */
	u32 status = 0;
	struct pmu_cmd cmd;
	u32 seq;
	pmu_callback p_callback = NULL;

	memset(&cmd, 0, sizeof(struct pmu_cmd));

	/* Copy common members */
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(union pmu_ap_cmd);

	cmd.cmd.pg.ap_cmd.cmn.cmd_type = PMU_PG_CMD_ID_AP;
	cmd.cmd.pg.ap_cmd.cmn.cmd_id = p_ap_cmd->cmn.cmd_id;

	/* Copy other members of command */
	switch (p_ap_cmd->cmn.cmd_id) {
	case PMU_AP_CMD_ID_INIT:
		gk20a_dbg_pmu("cmd post PMU_AP_CMD_ID_INIT");
		cmd.cmd.pg.ap_cmd.init.pg_sampling_period_us =
			p_ap_cmd->init.pg_sampling_period_us;
		break;

	case PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL:
		gk20a_dbg_pmu("cmd post PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL");
		cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.ctrl_id =
		p_ap_cmd->init_and_enable_ctrl.ctrl_id;
		memcpy(
		(void *)&(cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.params),
			(void *)&(p_ap_cmd->init_and_enable_ctrl.params),
			sizeof(struct pmu_ap_ctrl_init_params));

		p_callback = ap_callback_init_and_enable_ctrl;
		break;

	case PMU_AP_CMD_ID_ENABLE_CTRL:
		gk20a_dbg_pmu("cmd post PMU_AP_CMD_ID_ENABLE_CTRL");
		cmd.cmd.pg.ap_cmd.enable_ctrl.ctrl_id =
			p_ap_cmd->enable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_DISABLE_CTRL:
		gk20a_dbg_pmu("cmd post PMU_AP_CMD_ID_DISABLE_CTRL");
		cmd.cmd.pg.ap_cmd.disable_ctrl.ctrl_id =
			p_ap_cmd->disable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_KICK_CTRL:
		gk20a_dbg_pmu("cmd post PMU_AP_CMD_ID_KICK_CTRL");
		cmd.cmd.pg.ap_cmd.kick_ctrl.ctrl_id =
			p_ap_cmd->kick_ctrl.ctrl_id;
		cmd.cmd.pg.ap_cmd.kick_ctrl.skip_count =
			p_ap_cmd->kick_ctrl.skip_count;
		break;

	default:
		gk20a_dbg_pmu("%s: Invalid Adaptive Power command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		return 0x2f;
	}

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			p_callback, pmu, &seq, ~0);

	if (status) {
		gk20a_dbg_pmu(
			"%s: Unable to submit Adaptive Power Command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		goto err_return;
	}

	/* TODO: Implement blocking calls (b_block) */

err_return:
	return status;
}

static void ap_callback_init_and_enable_ctrl(
		struct gk20a *g, struct pmu_msg *msg,
		void *param, u32 seq_desc, u32 status)
{
	/* Define p_ap (i.e pointer to pmu_ap structure) */
	WARN_ON(!msg);

	if (!status) {
		switch (msg->msg.pg.ap_msg.cmn.msg_id) {
		case PMU_AP_MSG_ID_INIT_ACK:
			gk20a_dbg_pmu("reply PMU_AP_CMD_ID_INIT");
			break;

		default:
			gk20a_dbg_pmu(
			"%s: Invalid Adaptive Power Message: %x\n",
			__func__, msg->msg.pg.ap_msg.cmn.msg_id);
			break;
		}
	}
}

int gk20a_aelpg_init(struct gk20a *g)
{
	int status = 0;

	/* Remove reliance on app_ctrl field. */
	union pmu_ap_cmd ap_cmd;

	/* TODO: Check for elpg being ready? */
	ap_cmd.init.cmd_id = PMU_AP_CMD_ID_INIT;
	ap_cmd.init.pg_sampling_period_us = g->pmu.aelpg_param[0];

	status = gk20a_pmu_ap_send_command(g, &ap_cmd, false);
	return status;
}

int gk20a_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id)
{
	int status = 0;
	union pmu_ap_cmd ap_cmd;

	/* TODO: Probably check if ELPG is ready? */
	ap_cmd.init_and_enable_ctrl.cmd_id = PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL;
	ap_cmd.init_and_enable_ctrl.ctrl_id = ctrl_id;
	ap_cmd.init_and_enable_ctrl.params.min_idle_filter_us =
			g->pmu.aelpg_param[1];
	ap_cmd.init_and_enable_ctrl.params.min_target_saving_us =
			g->pmu.aelpg_param[2];
	ap_cmd.init_and_enable_ctrl.params.power_break_even_us =
			g->pmu.aelpg_param[3];
	ap_cmd.init_and_enable_ctrl.params.cycles_per_sample_max =
			g->pmu.aelpg_param[4];

	switch (ctrl_id) {
	case PMU_AP_CTRL_ID_GRAPHICS:
		break;
	default:
		break;
	}

	status = gk20a_pmu_ap_send_command(g, &ap_cmd, true);
	return status;
}

#ifdef CONFIG_DEBUG_FS
static int lpwr_debug_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;

	if (g->ops.pmu.pmu_pg_engines_feature_list &&
		g->ops.pmu.pmu_pg_engines_feature_list(g,
		PMU_PG_ELPG_ENGINE_ID_GRAPHICS) !=
		PMU_PG_FEATURE_GR_POWER_GATING_ENABLED) {
		seq_printf(s, "PSTATE: %u\n"
			"RPPG Enabled: %u\n"
			"RPPG ref count: %u\n"
			"RPPG state: %u\n"
			"MSCG Enabled: %u\n"
			"MSCG pstate state: %u\n"
			"MSCG transition state: %u\n",
			nvgpu_clk_arb_get_current_pstate(g),
			g->elpg_enabled, g->pmu.elpg_refcnt,
			g->pmu.elpg_stat, g->mscg_enabled,
			g->pmu.mscg_stat, g->pmu.mscg_transition_state);

	} else
		seq_printf(s, "ELPG Enabled: %u\n"
			"ELPG ref count: %u\n"
			"ELPG state: %u\n",
			g->elpg_enabled, g->pmu.elpg_refcnt,
			g->pmu.elpg_stat);

	return 0;

}

static int lpwr_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, lpwr_debug_show, inode->i_private);
}

static const struct file_operations lpwr_debug_fops = {
	.open		= lpwr_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mscg_stat_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u64 total_ingating, total_ungating, residency, divisor, dividend;
	struct pmu_pg_stats_data pg_stat_data = { 0 };
	int err;

	/* Don't unnecessarily power on the device */
	if (g->power_on) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_pmu_get_pg_stats(g,
			PMU_PG_ELPG_ENGINE_ID_MS, &pg_stat_data);
		gk20a_idle(g->dev);
	}
	total_ingating = g->pg_ingating_time_us +
			(u64)pg_stat_data.ingating_time;
	total_ungating = g->pg_ungating_time_us +
			(u64)pg_stat_data.ungating_time;

	divisor = total_ingating + total_ungating;

	/* We compute the residency on a scale of 1000 */
	dividend = total_ingating * 1000;

	if (divisor)
		residency = div64_u64(dividend, divisor);
	else
		residency = 0;

	seq_printf(s,
			"Time in MSCG: %llu us\n"
			"Time out of MSCG: %llu us\n"
			"MSCG residency ratio: %llu\n"
			"MSCG Entry Count: %u\n"
			"MSCG Avg Entry latency %u\n"
			"MSCG Avg Exit latency %u\n",
			total_ingating, total_ungating,
			residency, pg_stat_data.gating_cnt,
			pg_stat_data.avg_entry_latency_us,
			pg_stat_data.avg_exit_latency_us);
	return 0;

}

static int mscg_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, mscg_stat_show, inode->i_private);
}

static const struct file_operations mscg_stat_fops = {
	.open		= mscg_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mscg_transitions_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	struct pmu_pg_stats_data pg_stat_data = { 0 };
	u32 total_gating_cnt;
	int err;

	if (g->power_on) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_pmu_get_pg_stats(g,
			PMU_PG_ELPG_ENGINE_ID_MS, &pg_stat_data);
		gk20a_idle(g->dev);
	}
	total_gating_cnt = g->pg_gating_cnt + pg_stat_data.gating_cnt;

	seq_printf(s, "%u\n", total_gating_cnt);
	return 0;

}

static int mscg_transitions_open(struct inode *inode, struct file *file)
{
	return single_open(file, mscg_transitions_show, inode->i_private);
}

static const struct file_operations mscg_transitions_fops = {
	.open		= mscg_transitions_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int elpg_stat_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	struct pmu_pg_stats_data pg_stat_data = { 0 };
	u64 total_ingating, total_ungating, residency, divisor, dividend;
	int err;

	/* Don't unnecessarily power on the device */
	if (g->power_on) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_pmu_get_pg_stats(g,
			PMU_PG_ELPG_ENGINE_ID_GRAPHICS, &pg_stat_data);
		gk20a_idle(g->dev);
	}
	total_ingating = g->pg_ingating_time_us +
			(u64)pg_stat_data.ingating_time;
	total_ungating = g->pg_ungating_time_us +
			(u64)pg_stat_data.ungating_time;
	divisor = total_ingating + total_ungating;

	/* We compute the residency on a scale of 1000 */
	dividend = total_ingating * 1000;

	if (divisor)
		residency = div64_u64(dividend, divisor);
	else
		residency = 0;

	seq_printf(s,
			"Time in ELPG: %llu us\n"
			"Time out of ELPG: %llu us\n"
			"ELPG residency ratio: %llu\n"
			"ELPG Entry Count: %u\n"
			"ELPG Avg Entry latency %u us\n"
			"ELPG Avg Exit latency %u us\n",
			total_ingating, total_ungating,
			residency, pg_stat_data.gating_cnt,
			pg_stat_data.avg_entry_latency_us,
			pg_stat_data.avg_exit_latency_us);
	return 0;

}

static int elpg_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, elpg_stat_show, inode->i_private);
}

static const struct file_operations elpg_stat_fops = {
	.open		= elpg_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int elpg_transitions_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	struct pmu_pg_stats_data pg_stat_data = { 0 };
	u32 total_gating_cnt;
	int err;

	if (g->power_on) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		gk20a_pmu_get_pg_stats(g,
			PMU_PG_ELPG_ENGINE_ID_GRAPHICS, &pg_stat_data);
		gk20a_idle(g->dev);
	}
	total_gating_cnt = g->pg_gating_cnt + pg_stat_data.gating_cnt;

	seq_printf(s, "%u\n", total_gating_cnt);
	return 0;

}

static int elpg_transitions_open(struct inode *inode, struct file *file)
{
	return single_open(file, elpg_transitions_show, inode->i_private);
}

static const struct file_operations elpg_transitions_fops = {
	.open		= elpg_transitions_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int falc_trace_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	struct pmu_gk20a *pmu = &g->pmu;
	u32 i = 0, j = 0, k, l, m;
	char *trace = pmu->trace_buf.cpu_va;
	char part_str[40];
	u32 *trace1 = pmu->trace_buf.cpu_va;
	for (i = 0; i < GK20A_PMU_TRACE_BUFSIZE; i += 0x40) {
		for (j = 0; j < 0x40; j++)
			if (trace1[(i / 4) + j])
				break;
		if (j == 0x40)
			return 0;
		seq_printf(s, "Index %x: ", trace1[(i / 4)]);
		l = 0;
		m = 0;
		while (find_hex_in_string((trace+i+20+m), g, &k)) {
			if (k >= 40)
				break;
			strncpy(part_str, (trace+i+20+m), k);
			part_str[k] = 0;
			seq_printf(s, "%s0x%x", part_str,
					trace1[(i / 4) + 1 + l]);
			l++;
			m += k + 2;
		}
		seq_printf(s, "%s", (trace+i+20+m));
	}
	return 0;
}

static int falc_trace_open(struct inode *inode, struct file *file)
{
	return single_open(file, falc_trace_show, inode->i_private);
}

static const struct file_operations falc_trace_fops = {
	.open		= falc_trace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int perfmon_events_enable_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;

	seq_printf(s, "%u\n", g->pmu.perfmon_sampling_enabled ? 1 : 0);
	return 0;

}

static int perfmon_events_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmon_events_enable_show, inode->i_private);
}

static ssize_t perfmon_events_enable_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct gk20a *g = s->private;
	unsigned long val = 0;
	char buf[40];
	int buf_size;
	int err;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, (sizeof(buf)-1));

	if (copy_from_user(buf, userbuf, buf_size))
		return -EFAULT;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	/* Don't turn on gk20a unnecessarily */
	if (g->power_on) {
		err = gk20a_busy(g->dev);
		if (err)
			return err;

		if (val && !g->pmu.perfmon_sampling_enabled) {
			g->pmu.perfmon_sampling_enabled = true;
			pmu_perfmon_start_sampling(&(g->pmu));
		} else if (!val && g->pmu.perfmon_sampling_enabled) {
			g->pmu.perfmon_sampling_enabled = false;
			pmu_perfmon_stop_sampling(&(g->pmu));
		}
		gk20a_idle(g->dev);
	} else {
		g->pmu.perfmon_sampling_enabled = val ? true : false;
	}

	return count;
}

static const struct file_operations perfmon_events_enable_fops = {
	.open		= perfmon_events_enable_open,
	.read		= seq_read,
	.write		= perfmon_events_enable_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int perfmon_events_count_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;

	seq_printf(s, "%lu\n", g->pmu.perfmon_events_cnt);
	return 0;

}

static int perfmon_events_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, perfmon_events_count_show, inode->i_private);
}

static const struct file_operations perfmon_events_count_fops = {
	.open		= perfmon_events_count_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int security_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;

	seq_printf(s, "%d\n", g->pmu.pmu_mode);
	return 0;

}

static int security_open(struct inode *inode, struct file *file)
{
	return single_open(file, security_show, inode->i_private);
}

static const struct file_operations security_fops = {
	.open		= security_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int gk20a_pmu_debugfs_init(struct device *dev)
{
	struct dentry *d;
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);

	d = debugfs_create_file(
		"lpwr_debug", S_IRUGO|S_IWUSR, platform->debugfs, g,
						&lpwr_debug_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"mscg_residency", S_IRUGO|S_IWUSR, platform->debugfs, g,
						&mscg_stat_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"mscg_transitions", S_IRUGO, platform->debugfs, g,
						&mscg_transitions_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"elpg_residency", S_IRUGO|S_IWUSR, platform->debugfs, g,
						&elpg_stat_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"elpg_transitions", S_IRUGO, platform->debugfs, g,
						&elpg_transitions_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"falc_trace", S_IRUGO, platform->debugfs, g,
						&falc_trace_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"perfmon_events_enable", S_IRUGO, platform->debugfs, g,
						&perfmon_events_enable_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"perfmon_events_count", S_IRUGO, platform->debugfs, g,
						&perfmon_events_count_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"pmu_security", S_IRUGO, platform->debugfs, g,
						&security_fops);
	if (!d)
		goto err_out;
	return 0;
err_out:
	pr_err("%s: Failed to make debugfs node\n", __func__);
	debugfs_remove_recursive(platform->debugfs);
	return -ENOMEM;
}

#endif
