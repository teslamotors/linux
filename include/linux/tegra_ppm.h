/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All Rights Reserved.
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

#ifndef __TEGRA_PPM_H
#define __TEGRA_PPM_H

#include <linux/clk.h>
#include <linux/fs.h>

struct fv_relation;
struct fv_relation *fv_relation_create(
	struct clk *, int, ssize_t, int (*)(struct clk *, long unsigned int));
void fv_relation_destroy(struct fv_relation *);

#define TEGRA_PPM_MAX_CORES 4
struct tegra_ppm_params {
	int n_cores;

	/* activity factor in nA/MHz (aka fF, femtofarads) */
	u32 dyn_consts_n[TEGRA_PPM_MAX_CORES];

	/* leakage scaling factor based on number of cores. Expressed permill */
	u32 leakage_consts_n[TEGRA_PPM_MAX_CORES];

	/* Coefficients to a tricubic model of leakage. Model inputs are:
	 *   Voltage in Volts
	 *   IDDQ in Amps
	 *   Temperature in *deciCelcius*
	 *
	 * This unusual choice of units helps with the accuracy of fixed point
	 * calculations by keeping all of the coefficients near the same order
	 * of magnitude.
	 *
	 * All of the coefficients are expressed as ijk_scaled times their true
	 * value so that they fit nicely in 32-bit integers with minimal
	 * quantization error.
	 */
	s32 leakage_consts_ijk[4][4][4];
	u32 ijk_scaled;

	u32 leakage_min; /* minimum leakage current */
};

struct tegra_ppm;
struct dt;

struct tegra_ppm_params *of_read_tegra_ppm_params(struct device_node *np);

struct tegra_ppm *tegra_ppm_create(const char *name,
				   struct fv_relation *fv,
				   struct tegra_ppm_params *params,
				   int iddq_ma,
				   struct dentry *debugfs_dir);
void tegra_ppm_destroy(struct tegra_ppm *doomed,
		       struct fv_relation **fv,
		       struct tegra_ppm_params **params);

#define TEGRA_PPM_UNITS_MILLIAMPS 0
#define TEGRA_PPM_UNITS_MILLIWATTS 1


unsigned tegra_ppm_get_maxf(struct tegra_ppm *ctx,
			    unsigned int limit, int units,
			    int temp_c, int cores);

void tegra_ppm_drop_cache(struct tegra_ppm *ctx);

#endif
