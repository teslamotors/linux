/*
 * arch/arm/mach-tegra/board-panel.h
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __MACH_TEGRA_BOARD_PANEL_H
#define __MACH_TEGRA_BOARD_PANEL_H

#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <mach/dc.h>
#include <mach/tegra-board-id.h>

struct tegra_panel {
	void (*init_sd_settings)(struct tegra_dc_sd_settings *);
	void (*init_dc_out)(struct tegra_dc_out *);
	void (*init_fb_data)(struct tegra_fb_data *);
	void (*init_cmu_data)(struct tegra_dc_platform_data *);
	void (*set_disp_device)(struct platform_device *);
	int (*register_bl_dev)(void);
	int (*register_i2c_bridge)(void);
};

enum {
	TEGRA_GPIO_RESET,
	TEGRA_GPIO_PANEL_EN,
	TEGRA_GPIO_PANEL_EN_1,
	TEGRA_GPIO_BL_ENABLE,
	TEGRA_GPIO_PWM,
	TEGRA_GPIO_BRIDGE_EN_0,
	TEGRA_GPIO_BRIDGE_EN_1,
	TEGRA_GPIO_BRIDGE_REFCLK_EN,
	TEGRA_N_GPIO_PANEL, /* add new gpio above this entry */
};

/* tegra_panel_of will replace tegra_panel once we completely move to DT */
struct tegra_panel_of {
	int panel_gpio[TEGRA_N_GPIO_PANEL];
	bool panel_gpio_populated;
	bool en_vmm_vpp_i2c_config;
};
static struct tegra_panel_of __maybe_unused panel_of = {
	/* TEGRA_N_GPIO_PANEL counts of gpio should be
	 * initialized to TEGRA_GPIO_INVALID */
	.panel_gpio = {-1, -1, -1, -1, -1, -1, -1},
};
struct tegra_panel_ops {
	int (*enable)(struct device *);
	int (*postpoweron)(struct device *);
	int (*prepoweroff)(void);
	int (*disable)(struct device *);
	int (*hotplug_init)(struct device *);
	int (*postsuspend)(void);
	void (*hotplug_report)(bool);
	struct pwm_bl_data_dt_ops *pwm_bl_ops;
};

struct generic_bl_data_dt_ops {
	int (*notify)(struct device *dev, int brightness);
};

extern struct tegra_panel_ops dsi_p_wuxga_10_1_ops;
extern struct tegra_panel_ops dsi_lgd_wxga_7_0_ops;
extern struct tegra_panel_ops dsi_s_wqxga_10_1_ops;
extern struct tegra_panel_ops dsi_s_wuxga_7_0_ops;
extern struct tegra_panel_ops dsi_s_wuxga_8_0_ops;
extern struct tegra_panel_ops dsi_a_1200_1920_8_0_ops;
extern struct tegra_panel_ops dsi_a_1200_800_8_0_ops;
extern struct tegra_panel_ops edp_a_1080p_14_0_ops;
extern struct tegra_panel_ops edp_i_1080p_11_6_ops;
extern struct tegra_panel_ops lvds_c_1366_14_ops;
extern struct tegra_panel_ops dsi_a_1080p_14_0_ops;
extern struct tegra_panel_ops dsi_j_1440_810_5_8_ops;
extern struct tegra_panel_ops dsi_j_720p_5_ops;
extern struct tegra_panel_ops dsi_l_720p_5_loki_ops;
extern struct tegra_panel_ops edp_s_uhdtv_15_6_ops;
extern struct tegra_panel_ops dsi_o_720p_6_0_ops;
extern struct tegra_panel_ops dsi_n_wqxga_6_0_ops;

extern struct tegra_panel_ops *fixed_primary_panel_ops;
extern struct tegra_panel_ops *fixed_secondary_panel_ops;
extern const char *fixed_primary_panel_compatible;
extern const char *fixed_secondary_panel_compatible;

extern struct pwm_bl_data_dt_ops *fixed_pwm_bl_ops;

extern struct tegra_panel dsi_p_wuxga_10_1;
extern struct tegra_panel dsi_a_1080p_11_6;
extern struct tegra_panel dsi_s_wqxga_10_1;
extern struct tegra_panel dsi_lgd_wxga_7_0;
extern struct tegra_panel dsi_a_1080p_14_0;
extern struct tegra_panel edp_a_1080p_14_0;
extern struct tegra_panel edp_i_1080p_11_6;
extern struct tegra_panel edp_s_wqxgap_15_6;
extern struct tegra_panel edp_s_uhdtv_15_6;
extern struct tegra_panel lvds_c_1366_14;
extern struct tegra_panel dsi_l_720p_5_loki;
extern struct tegra_panel dsi_j_1440_810_5_8;
extern struct tegra_panel dsi_j_720p_5;
extern struct tegra_panel dsi_a_1200_1920_8_0;
extern struct tegra_panel dsi_a_1200_800_8_0;

void tegra_dsi_resources_init(u8 dsi_instance,
			struct resource *resources, int n_resources);

void tegra_dsi_update_init_cmd_gpio_rst(struct tegra_dc_out *dsi_disp1_out);

int tegra_panel_gpio_get_dt(const char *comp_str,
				struct tegra_panel_of *panel);

int tegra_panel_check_regulator_dt_support(const char *comp_str,
				struct tegra_panel_of *panel);

int tegra_panel_reset(struct tegra_panel_of *panel, unsigned int delay_ms);

void tegra_fb_copy_or_clear(void);

int tegra_disp_defer_vcore_override(void);

void tegra_set_fixed_panel_ops(bool is_primary,
			struct tegra_panel_ops *p_ops, char *panel_node);

void tegra_set_fixed_pwm_bl_ops(struct pwm_bl_data_dt_ops *p_ops);

void tegra_pwm_bl_ops_register(struct device *dev);

void ti_lp855x_bl_ops_register(struct device *dev);
#endif /* __MACH_TEGRA_BOARD_PANEL_H */
