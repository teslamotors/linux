/*
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

#include "therm_gp106.h"
#include <linux/debugfs.h>
#include "hw_therm_gp106.h"
#include "therm/thrmpmu.h"

static void gp106_get_internal_sensor_limits(s32 *max_24_8, s32 *min_24_8)
{
	*max_24_8 = (0x87 << 8);
	*min_24_8 = ((-216) << 8);
}

static int gp106_get_internal_sensor_curr_temp(struct gk20a *g, u32 *temp_f24_8)
{
	int err = 0;
	u32 readval;

	readval = gk20a_readl(g, therm_temp_sensor_tsense_r());

	if (!(therm_temp_sensor_tsense_state_v(readval) &
		therm_temp_sensor_tsense_state_valid_v())) {
		gk20a_err(dev_from_gk20a(g),
			"Attempt to read temperature while sensor is OFF!\n");
		err = -EINVAL;
	} else if (therm_temp_sensor_tsense_state_v(readval) &
		therm_temp_sensor_tsense_state_shadow_v()) {
		gk20a_err(dev_from_gk20a(g),
			"Reading temperature from SHADOWed sensor!\n");
	}

	// Convert from F9.5 -> F27.5 -> F24.8.
	readval &= therm_temp_sensor_tsense_fixed_point_m();

	*temp_f24_8 = readval;

	return err;
}

#ifdef CONFIG_DEBUG_FS
static int therm_get_internal_sensor_curr_temp(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	u32 readval;
	int err;

	err = gp106_get_internal_sensor_curr_temp(g, &readval);
	if (!err)
		*val = readval;

	return err;
}
DEFINE_SIMPLE_ATTRIBUTE(therm_ctrl_fops, therm_get_internal_sensor_curr_temp, NULL, "%llu\n");

static void gp106_therm_debugfs_init(struct gk20a *g) {
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	struct dentry *dbgentry;

	dbgentry = debugfs_create_file(
		"temp", S_IRUGO, platform->debugfs, g, &therm_ctrl_fops);
	if (!dbgentry)
		gk20a_err(dev_from_gk20a(g), "debugfs entry create failed for therm_curr_temp");
}
#endif

static int gp106_elcg_init_idle_filters(struct gk20a *g)
{
	u32 gate_ctrl, idle_filter;
	u32 engine_id;
	u32 active_engine_id = 0;
	struct fifo_gk20a *f = &g->fifo;

	gk20a_dbg_fn("");

	for (engine_id = 0; engine_id < f->num_engines; engine_id++) {
		active_engine_id = f->active_engines_list[engine_id];
		gate_ctrl = gk20a_readl(g, therm_gate_ctrl_r(active_engine_id));

		if (tegra_platform_is_linsim()) {
			gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_delay_after_m(),
				therm_gate_ctrl_eng_delay_after_f(4));
		}

		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_exp_m(),
			therm_gate_ctrl_eng_idle_filt_exp_f(2));
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_mant_m(),
			therm_gate_ctrl_eng_idle_filt_mant_f(1));
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_delay_before_m(),
			therm_gate_ctrl_eng_delay_before_f(0));
		gk20a_writel(g, therm_gate_ctrl_r(active_engine_id), gate_ctrl);
	}

	/* default fecs_idle_filter to 0 */
	idle_filter = gk20a_readl(g, therm_fecs_idle_filter_r());
	idle_filter &= ~therm_fecs_idle_filter_value_m();
	gk20a_writel(g, therm_fecs_idle_filter_r(), idle_filter);
	/* default hubmmu_idle_filter to 0 */
	idle_filter = gk20a_readl(g, therm_hubmmu_idle_filter_r());
	idle_filter &= ~therm_hubmmu_idle_filter_value_m();
	gk20a_writel(g, therm_hubmmu_idle_filter_r(), idle_filter);

	gk20a_dbg_fn("done");
	return 0;
}

static u32 gp106_configure_therm_alert(struct gk20a *g, s32 curr_warn_temp)
{
	u32 err = 0;

	if (g->curr_warn_temp != curr_warn_temp) {
		g->curr_warn_temp = curr_warn_temp;
		err = therm_configure_therm_alert(g);
	}

	return err;
}

void gp106_init_therm_ops(struct gpu_ops *gops) {
#ifdef CONFIG_DEBUG_FS
	gops->therm.therm_debugfs_init = gp106_therm_debugfs_init;
#endif
	gops->therm.elcg_init_idle_filters = gp106_elcg_init_idle_filters;
	gops->therm.get_internal_sensor_curr_temp = gp106_get_internal_sensor_curr_temp;
	gops->therm.get_internal_sensor_limits =
			gp106_get_internal_sensor_limits;
	gops->therm.configure_therm_alert = gp106_configure_therm_alert;
}
