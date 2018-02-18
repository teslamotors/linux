/*
 * GP10B Graphics
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gk20a/gk20a.h"
#include "hw_fuse_gp10b.h"
#include "hw_gr_gp10b.h"

static u64 gp10b_detect_ecc_enabled_units(struct gk20a *g)
{
	u64 ecc_enabled_units = 0;
	u32 opt_ecc_en = gk20a_readl(g, fuse_opt_ecc_en_r());
	u32 opt_feature_fuses_override_disable =
			gk20a_readl(g,
				fuse_opt_feature_fuses_override_disable_r());
	u32 fecs_feature_override_ecc =
				gk20a_readl(g,
					gr_fecs_feature_override_ecc_r());

	if (opt_feature_fuses_override_disable) {
		if (opt_ecc_en)
			ecc_enabled_units = NVGPU_GPU_FLAGS_ALL_ECC_ENABLED;
		else
			ecc_enabled_units = 0;
	} else {
		/* SM LRF */
		if (gr_fecs_feature_override_ecc_sm_lrf_override_v(
						fecs_feature_override_ecc)) {
			if (gr_fecs_feature_override_ecc_sm_lrf_v(
						fecs_feature_override_ecc)) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_SM_LRF;
			}
		} else {
			if (opt_ecc_en) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_SM_LRF;
			}
		}

		/* SM SHM */
		if (gr_fecs_feature_override_ecc_sm_shm_override_v(
						fecs_feature_override_ecc)) {
			if (gr_fecs_feature_override_ecc_sm_shm_v(
						fecs_feature_override_ecc)) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_SM_SHM;
			}
		} else {
			if (opt_ecc_en) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_SM_SHM;
			}
		}

		/* TEX */
		if (gr_fecs_feature_override_ecc_tex_override_v(
						fecs_feature_override_ecc)) {
			if (gr_fecs_feature_override_ecc_tex_v(
						fecs_feature_override_ecc)) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_TEX;
			}
		} else {
			if (opt_ecc_en) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_TEX;
			}
		}

		/* LTC */
		if (gr_fecs_feature_override_ecc_ltc_override_v(
						fecs_feature_override_ecc)) {
			if (gr_fecs_feature_override_ecc_ltc_v(
						fecs_feature_override_ecc)) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_LTC;
			}
		} else {
			if (opt_ecc_en) {
				ecc_enabled_units |=
					NVGPU_GPU_FLAGS_ECC_ENABLED_LTC;
			}
		}
	}

	return ecc_enabled_units;
}

int gp10b_init_gpu_characteristics(struct gk20a *g)
{
	gk20a_init_gpu_characteristics(g);
	g->gpu_characteristics.flags |= gp10b_detect_ecc_enabled_units(g);

	return 0;
}
