/*
 * arch/arm/mach-tegra/tegra21_speedo.c
 *
 * Copyright (C) 2013-2015 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/bug.h>			/* For BUG_ON.  */

#include <linux/tegra-fuse.h>
#include <linux/tegra-soc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "iomap.h"
#include <linux/platform/tegra/common.h>

#define TEGRA21_SOC_SPEEDO 1900
#define TEGRA21_CPU_SPEEDO 2100
#define TEGRA21_GPU_SPEEDO 2100
#define TEGRA21_GPU_SPEEDO_OFFS 75


#define CPU_PROCESS_CORNERS_NUM		3
#define GPU_PROCESS_CORNERS_NUM		2
#define CORE_PROCESS_CORNERS_NUM	3

#define FUSE_CPU_SPEEDO_0	0x114
#define FUSE_CPU_SPEEDO_1	0x12c
#define FUSE_CPU_SPEEDO_2	0x130
#define FUSE_SOC_SPEEDO_0	0x134
#define FUSE_SOC_SPEEDO_1	0x138
#define FUSE_SOC_SPEEDO_2	0x13c
#define FUSE_CPU_IDDQ		0x118
#define FUSE_SOC_IDDQ		0x140
#define FUSE_GPU_IDDQ		0x228
#define FUSE_FT_REV		0x128

static int threshold_index;
static int cpu_process_id;
static int core_process_id;
static int gpu_process_id;
static int cpu_speedo_id;
static int soc_speedo_id;
static int gpu_speedo_id;
static int package_id;

static int core_min_mv;

static int cpu_iddq_value;
static int gpu_iddq_value;
static int soc_iddq_value;

static int cpu_speedo_0_value;
static int cpu_speedo_1_value;
static int cpu_speedo_2_value;
static int soc_speedo_0_value;
static int soc_speedo_1_value;
static int soc_speedo_2_value;

static int cpu_speedo_value;
static int gpu_speedo_value;
static int soc_speedo_value;

static int speedo_rev;

static int enable_app_profiles;

static const u32 cpu_process_speedos[][CPU_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	       1          2 */
	{2119,  UINT_MAX,  UINT_MAX}, /* [0]: threshold_index 0 */
	{2119,  UINT_MAX,  UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 gpu_process_speedos[][GPU_PROCESS_CORNERS_NUM] = {
/* proc_id      0,        1 */
	{UINT_MAX, UINT_MAX}, /* [0]: threshold_index 0 */
	{UINT_MAX, UINT_MAX}, /* [1]: threshold_index 1 */
};

static const u32 core_process_speedos[][CORE_PROCESS_CORNERS_NUM] = {
/* proc_id  0,	       1,             2 */
	{1950,      2073,      UINT_MAX}, /* [0]: threshold_index 0 */
	{UINT_MAX,  UINT_MAX,  UINT_MAX}, /* [1]: threshold_index 1 */
};

static void rev_sku_to_speedo_ids(int rev, int sku, int speedo_rev)
{
	bool joint_xpu_rail = false;
	bool always_on = false;
	bool shield_sku = false;
	bool vcm31_sku = false;
	bool a02 = rev == TEGRA_REVISION_A02;

#ifdef CONFIG_OF
	joint_xpu_rail = of_property_read_bool(of_chosen,
					  "nvidia,tegra-joint_xpu_rail");
	always_on = of_property_read_bool(of_chosen,
					  "nvidia,tegra-always-on-personality");
	shield_sku = of_property_read_bool(of_chosen,
					   "nvidia,tegra-shield-sku");
	vcm31_sku = of_property_read_bool(of_chosen,
					       "nvidia,t210-vcm31-sku");
#endif
	switch (sku) {
	case 0x00: /* Engg sku */
	case 0x01: /* Engg sku */
	case 0x13:
		if (a02 && !shield_sku) {
			cpu_speedo_id = 5;
			soc_speedo_id = 0;
			gpu_speedo_id = 2;
			threshold_index = 0;
			core_min_mv = 800;
			break;
		}
		/* fall thru for a01 or shield sku */
	case 0x07:
	case 0x17:
		if (!vcm31_sku || (sku != 0x17)) {
			if (a02 && !shield_sku) {
				cpu_speedo_id = always_on ? 8 : 7;
				soc_speedo_id = 0;
				gpu_speedo_id = 2;
				threshold_index = 0;
				core_min_mv = 800;
			} else {
				cpu_speedo_id = shield_sku ? 2 : 0;
				soc_speedo_id = 0;
				gpu_speedo_id = 1;
				threshold_index = 0;
				core_min_mv = 825;
			}
			break;
		}
		/* fall thru for vcm31_sku 0x17 */
	case 0x57:
		if (tegra_get_sku_override()) {
			cpu_speedo_id = 0;
			gpu_speedo_id = 1;
		} else {
			cpu_speedo_id = 4;
			gpu_speedo_id = 4;
		}
		soc_speedo_id = 1;
		threshold_index = 1;
		core_min_mv = 1100;
		break;
	case 0x83:
		if (a02) {
			cpu_speedo_id = 3;
			soc_speedo_id = 0;
			gpu_speedo_id = 3;
			threshold_index = 0;
			core_min_mv = 800;
			break;
		}
		/* fall thru for a01 part and Darcy SKU*/
	case 0x87:
		if (a02) {
			cpu_speedo_id = joint_xpu_rail ? 6 : 2;
			soc_speedo_id = 0;
			gpu_speedo_id = 1;
			threshold_index = 0;
			core_min_mv = 825;
			break;
		}
		/* fall thru for a01 part */
	case 0x8F:
		if (a02 && always_on) {
			cpu_speedo_id = joint_xpu_rail ? 10 : 9;
			soc_speedo_id = 0;
			gpu_speedo_id = 2;
			threshold_index = 0;
			core_min_mv = 800;
			break;
		}
		/* fall thru for a01 part or not always on */
	default:
		pr_warn("Tegra21: Unknown SKU/mode:\n");
		pr_warn("sku = 0x%X a02 = %d shield_sku = %d vcm31_sku = %d\n",
			sku, a02, shield_sku, vcm31_sku);
		pr_warn("joint_xpu_rail = %d always_on = %d\n",
			joint_xpu_rail, always_on);
		cpu_speedo_id = 0;
		soc_speedo_id = 0;
		gpu_speedo_id = 0;
		threshold_index = 0;
		core_min_mv = 950;
		break;
	}

	/* Overwrite GPU speedo selection for speedo revision 0, 1 */
	if (speedo_rev < 2)
		gpu_speedo_id = 0;
}

static int get_speedo_rev(void)
{
	return (tegra_spare_fuse(4) << 2) |
		(tegra_spare_fuse(3) << 1) |
		(tegra_spare_fuse(2) << 0);
}

void tegra_init_speedo_data(void)
{
	int i;
	u32 tegra_sku_id;

	if (!tegra_platform_is_silicon()) {
		cpu_process_id  =  0;
		core_process_id =  0;
		gpu_process_id  = 0;
		cpu_speedo_id   = 0;
		soc_speedo_id   = 0;
		gpu_speedo_id   = 0;
		package_id = -1;
		cpu_speedo_value = TEGRA21_CPU_SPEEDO;
		gpu_speedo_value = TEGRA21_GPU_SPEEDO;
		soc_speedo_value = TEGRA21_SOC_SPEEDO;
		cpu_speedo_0_value = 0;
		cpu_speedo_1_value = 0;
		soc_speedo_0_value = 0;
		soc_speedo_1_value = 0;
		soc_speedo_2_value = 0;
		soc_iddq_value = 0;
		gpu_iddq_value = 0;
		pr_info("Tegra21: CPU Speedo value %d, Soc Speedo value %d, Gpu Speedo value %d\n",
			cpu_speedo_value, soc_speedo_value, gpu_speedo_value);
		pr_info("Tegra21: CPU Speedo ID %d, Soc Speedo ID %d, Gpu Speedo ID %d\n",
			cpu_speedo_id, soc_speedo_id, gpu_speedo_id);
		pr_info("Tegra21: CPU Process ID %d,Soc Process ID %d,Gpu Process ID %d\n",
			cpu_process_id, core_process_id, gpu_process_id);
		return;
	}

	/* Read speedo/iddq fuses */
	cpu_speedo_0_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_0);
	cpu_speedo_1_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_1);
	cpu_speedo_2_value = tegra_fuse_readl(FUSE_CPU_SPEEDO_2);

	soc_speedo_0_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_0);
	soc_speedo_1_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_1);
	soc_speedo_2_value = tegra_fuse_readl(FUSE_SOC_SPEEDO_2);

	cpu_iddq_value = tegra_fuse_readl(FUSE_CPU_IDDQ) * 4;
	soc_iddq_value = tegra_fuse_readl(FUSE_SOC_IDDQ) * 4;
	gpu_iddq_value = tegra_fuse_readl(FUSE_GPU_IDDQ) * 5;

	/*
	 * Determine CPU, GPU, SOC speedo values depending on speedo fusing
	 * revision. Note that GPU speedo value is fused in CPU_SPEEDO_2
	 */
	speedo_rev = get_speedo_rev();
	if (speedo_rev >= 3) {
		cpu_speedo_value = cpu_speedo_0_value;
		gpu_speedo_value = cpu_speedo_2_value;
		soc_speedo_value = soc_speedo_0_value;
	} else if (speedo_rev == 2) {
		cpu_speedo_value = (-1938 + (1095*cpu_speedo_0_value/100)) / 10;
		gpu_speedo_value = (-1662 + (1082*cpu_speedo_2_value/100)) / 10;
		soc_speedo_value = (-705 + (1037*soc_speedo_0_value/100)) / 10;
	} else {
		/* FIXME: do we need hard-coded IDDQ here? */
		cpu_speedo_value = TEGRA21_CPU_SPEEDO;
		gpu_speedo_value = cpu_speedo_2_value - TEGRA21_GPU_SPEEDO_OFFS;
		soc_speedo_value = TEGRA21_SOC_SPEEDO;
	}

	if (cpu_speedo_value <= 0) {
		cpu_speedo_value = TEGRA21_CPU_SPEEDO;
		pr_warn("Tegra21: Warning: CPU Speedo value not fused. PLEASE FIX!!!!!!!!!!!\n");
		pr_warn("Tegra21: Warning: PLEASE USE BOARD WITH FUSED SPEEDO VALUE !!!!\n");
	}

	if (gpu_speedo_value <= 0) {
		gpu_speedo_value = TEGRA21_GPU_SPEEDO;
		pr_warn("Tegra21: Warning: GPU Speedo value not fused. PLEASE FIX!!!!!!!!!!!\n");
		pr_warn("Tegra21: Warning: PLEASE USE BOARD WITH FUSED SPEEDO VALUE !!!!\n");
	}

	if (soc_speedo_value <= 0) {
		soc_speedo_value = TEGRA21_SOC_SPEEDO;
		pr_warn("Tegra21: Warning: SOC Speedo value not fused. PLEASE FIX!!!!!!!!!!!\n");
		pr_warn("Tegra21: Warning: PLEASE USE BOARD WITH FUSED SPEEDO VALUE !!!!\n");
	}

	/* Map chip sku, rev, speedo values into speedo and process IDs */
	tegra_sku_id = tegra_get_sku_id();
	rev_sku_to_speedo_ids(tegra_revision, tegra_sku_id, speedo_rev);

	for (i = 0; i < GPU_PROCESS_CORNERS_NUM; i++) {
		if (gpu_speedo_value <
			gpu_process_speedos[threshold_index][i]) {
			break;
		}
	}
	gpu_process_id = i;

	for (i = 0; i < CPU_PROCESS_CORNERS_NUM; i++) {
		if (cpu_speedo_value <
			cpu_process_speedos[threshold_index][i]) {
			break;
		}
	}
	cpu_process_id = i;

	for (i = 0; i < CORE_PROCESS_CORNERS_NUM; i++) {
		if (soc_speedo_value <
			core_process_speedos[threshold_index][i]) {
			break;
		}
	}
	core_process_id = i;

	pr_info("Tegra21: Speedo/IDDQ fuse revision %d\n", speedo_rev);
	pr_info("Tegra21: CPU Speedo ID %d, Soc Speedo ID %d, Gpu Speedo ID %d\n",
		cpu_speedo_id, soc_speedo_id, gpu_speedo_id);
	pr_info("Tegra21: CPU Process ID %d, Soc Process ID %d, Gpu Process ID %d\n",
		 cpu_process_id, core_process_id, gpu_process_id);
	pr_info("Tegra21: CPU Speedo value %d, Soc Speedo value %d, Gpu Speedo value %d\n",
		 cpu_speedo_value, soc_speedo_value, gpu_speedo_value);
	pr_info("Tegra21: CPU IDDQ %d, Soc IDDQ %d, Gpu IDDQ %d\n",
		cpu_iddq_value, soc_iddq_value, gpu_iddq_value);
}

int tegra_cpu_process_id(void)
{
	return cpu_process_id;
}

int tegra_core_process_id(void)
{
	return core_process_id;
}

int tegra_gpu_process_id(void)
{
	return gpu_process_id;
}

int tegra_cpu_speedo_id(void)
{
	return cpu_speedo_id;
}

int tegra_soc_speedo_id(void)
{
	return soc_speedo_id;
}

int tegra_gpu_speedo_id(void)
{
	return gpu_speedo_id;
}

int tegra_package_id(void)
{
	return package_id;
}

int tegra_cpu_speedo_value(void)
{
	return cpu_speedo_value;
}

int tegra_cpu_speedo_0_value(void)
{
	return cpu_speedo_0_value;
}

int tegra_cpu_speedo_1_value(void)
{
	return cpu_speedo_1_value;
}

int tegra_gpu_speedo_value(void)
{
	return gpu_speedo_value;
}

int tegra_soc_speedo_0_value(void)
{
	return soc_speedo_value;
}
EXPORT_SYMBOL(tegra_soc_speedo_0_value);

int tegra_soc_speedo_1_value(void)
{
	return soc_speedo_1_value;
}

int tegra_soc_speedo_2_value(void)
{
	return soc_speedo_2_value;
}

/*
 * Core nominal and minimum voltage levels as determined by chip SKU and speedo
 * (not final - will be clipped to dvfs tables).
 */
int tegra_cpu_speedo_mv(void)
{
	/* Not applicable on Tegra21 */
	return -ENOSYS;
}

int tegra_core_speedo_mv(void)
{
	switch (core_process_id) {
	case 0:
		if (soc_speedo_id == 1)
			return 1100;
		if (speedo_rev <= 1)
			return 1000;
		return 1125;
	case 1:
		return 1075;
	case 2:
		return 1000;
	default:
		BUG();
	}
}

int tegra_core_speedo_min_mv(void)
{
	return core_min_mv;
}

int tegra_get_cpu_iddq_value(void)
{
	return cpu_iddq_value;
}

int tegra_get_soc_iddq_value(void)
{
	return soc_iddq_value;
}

int tegra_get_gpu_iddq_value(void)
{
	return gpu_iddq_value;
}

static int get_enable_app_profiles(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_profiles_ops = {
	.get = get_enable_app_profiles,
};

module_param_cb(tegra_enable_app_profiles,
	&tegra_profiles_ops, &enable_app_profiles, 0444);
