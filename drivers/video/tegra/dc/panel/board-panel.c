/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mach/dc.h>

#include "board-panel.h"
#include "board.h"
#include "iomap.h"
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform_data/lp855x.h>

#define PRISM_THRESHOLD		50
#define HYST_VAL		25

atomic_t sd_brightness = ATOMIC_INIT(255);
EXPORT_SYMBOL(sd_brightness);

struct tegra_panel_ops *fixed_primary_panel_ops;
struct tegra_panel_ops *fixed_secondary_panel_ops;
const char *fixed_primary_panel_compatible;
const char *fixed_secondary_panel_compatible;
struct pwm_bl_data_dt_ops *fixed_pwm_bl_ops;

void tegra_dsi_resources_init(u8 dsi_instance,
			struct resource *resources, int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "dsi_regs")) {
			switch (dsi_instance) {
			case DSI_INSTANCE_0:
				r->start = TEGRA_DSI_BASE;
				r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
				break;
			case DSI_INSTANCE_1:
			default:
				r->start = TEGRA_DSIB_BASE;
				r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
				break;
			}
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsia_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsib_regs")) {
			r->start = TEGRA_DSIB_BASE;
			r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
		}
#if defined(CONFIG_TEGRA_NVDISPLAY)
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "split_dsia_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "split_dsib_regs")) {
			r->start = TEGRA_DSIB_BASE;
			r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "split_dsic_regs")) {
			r->start = TEGRA_DSIC_BASE;
			r->end = TEGRA_DSIC_BASE + TEGRA_DSIC_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "split_dsid_regs")) {
			r->start = TEGRA_DSID_BASE;
			r->end = TEGRA_DSID_BASE + TEGRA_DSID_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "dsi_pad_reg")) {
			r->start = TEGRA_DSI_PADCTL_BASE;
			r->end = TEGRA_DSI_PADCTL_BASE +
						TEGRA_DSI_PADCTL_SIZE - 1;
		}
#endif
	}
}

void tegra_dsi_update_init_cmd_gpio_rst(
	struct tegra_dc_out *dsi_disp1_out)
{
	int i;
	for (i = 0; i < dsi_disp1_out->dsi->n_init_cmd; i++) {
		if (dsi_disp1_out->dsi->dsi_init_cmd[i].cmd_type ==
					TEGRA_DSI_GPIO_SET)
			dsi_disp1_out->dsi->dsi_init_cmd[i].sp_len_dly.gpio
				= dsi_disp1_out->dsi->dsi_panel_rst_gpio;
	}
}

int tegra_panel_reset(struct tegra_panel_of *panel, unsigned int delay_ms)
{
	int gpio = panel->panel_gpio[TEGRA_GPIO_RESET];

	if (!gpio_is_valid(gpio))
		return -ENOENT;

	gpio_direction_output(gpio, 1);
	usleep_range(1000, 5000);
	gpio_set_value(gpio, 0);
	usleep_range(1000, 5000);
	gpio_set_value(gpio, 1);
	msleep(delay_ms);

	return 0;
}


static int tegra_bl_notify(struct device *dev, int brightness)
{
	int cur_sd_brightness;

	struct lp855x *lp = NULL;
	struct platform_device *pdev = NULL;
	struct device *dc_dev;
	u8 *bl_measured = NULL;
	u8 *bl_curve = NULL;

	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	dc_dev = &pdev->dev;

	if (dc_dev) {
		if (brightness <= PRISM_THRESHOLD)
			nvsd_enbl_dsbl_prism(dc_dev, false);
		else if (brightness > PRISM_THRESHOLD + HYST_VAL)
			nvsd_enbl_dsbl_prism(dc_dev, true);
	}

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else if (of_device_is_compatible(dev->of_node,
				"ti,lp8550") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8551") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8552") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8553") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8554") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8555") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8556") ||
		of_device_is_compatible(dev->of_node,
				"ti,lp8557")) {
		lp = (struct lp855x *)dev_get_drvdata(dev);
		if (lp && lp->pdata) {
			bl_measured = lp->pdata->bl_measured;
			bl_curve = lp->pdata->bl_curve;
		}
	}

	if (bl_curve)
		brightness = bl_curve[brightness];

	cur_sd_brightness = atomic_read(&sd_brightness);
	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	if (bl_measured)
		brightness = bl_measured[brightness];

	return brightness;
}

static struct generic_bl_data_dt_ops generic_bl_ops = {
	.notify = tegra_bl_notify,
};

int tegra_panel_gpio_get_dt(const char *comp_str,
				struct tegra_panel_of *panel)
{
	int cnt = 0;
	char *label = NULL;
	int err = 0;
	struct device_node *node =
		of_find_compatible_node(NULL, NULL, comp_str);

	/*
	 * If gpios are already populated, just return.
	 */
	if (panel->panel_gpio_populated)
		return 0;

	if (!node) {
		pr_info("%s panel dt support not available\n", comp_str);
		err = -ENOENT;
		goto fail;
	}

	panel->panel_gpio[TEGRA_GPIO_RESET] =
		of_get_named_gpio(node, "nvidia,panel-rst-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_PANEL_EN] =
		of_get_named_gpio(node, "nvidia,panel-en-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_PANEL_EN_1] =
		of_get_named_gpio(node, "nvidia,panel-en-1-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_BL_ENABLE] =
		of_get_named_gpio(node, "nvidia,panel-bl-en-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_PWM] =
		of_get_named_gpio(node, "nvidia,panel-bl-pwm-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_BRIDGE_EN_0] =
		of_get_named_gpio(node, "nvidia,panel-bridge-en-0-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_BRIDGE_EN_1] =
		of_get_named_gpio(node, "nvidia,panel-bridge-en-1-gpio", 0);

	panel->panel_gpio[TEGRA_GPIO_BRIDGE_REFCLK_EN] =
		of_get_named_gpio(node,
			"nvidia,panel-bridge-refclk-en-gpio", 0);

	for (cnt = 0; cnt < TEGRA_N_GPIO_PANEL; cnt++) {
		if (gpio_is_valid(panel->panel_gpio[cnt])) {
			switch (cnt) {
			case TEGRA_GPIO_RESET:
				label = "tegra-panel-reset";
				break;
			case TEGRA_GPIO_PANEL_EN:
				label = "tegra-panel-en";
				break;
			case TEGRA_GPIO_PANEL_EN_1:
				label = "tegra-panel-en-1";
				break;
			case TEGRA_GPIO_BL_ENABLE:
				label = "tegra-panel-bl-enable";
				break;
			case TEGRA_GPIO_PWM:
				label = "tegra-panel-pwm";
				break;
			case TEGRA_GPIO_BRIDGE_EN_0:
				label = "tegra-panel-bridge-en-0";
				break;
			case TEGRA_GPIO_BRIDGE_EN_1:
				label = "tegra-panel-bridge-en-1";
				break;
			case TEGRA_GPIO_BRIDGE_REFCLK_EN:
				label = "tegra-panel-bridge-refclk-en";
				break;
			default:
				pr_err("tegra panel no gpio entry\n");
			}
			if (label) {
				gpio_request(panel->panel_gpio[cnt],
					label);
				label = NULL;
			}
		}
	}
	if (gpio_is_valid(panel->panel_gpio[TEGRA_GPIO_PWM]))
		gpio_free(panel->panel_gpio[TEGRA_GPIO_PWM]);
	panel->panel_gpio_populated = true;
fail:
	of_node_put(node);
	return err;
}

int tegra_panel_check_regulator_dt_support(const char *comp_str,
				struct tegra_panel_of *panel)
{
	int err = 0;
	struct device_node *node =
		of_find_compatible_node(NULL, NULL, comp_str);

	if (!node) {
		pr_info("%s panel dt support not available\n", comp_str);
		err = -ENOENT;
	}

	panel->en_vmm_vpp_i2c_config =
		of_property_read_bool(node, "nvidia,en-vmm-vpp-with-i2c-config");

	return err;
}

void tegra_set_fixed_panel_ops(bool is_primary,
	struct tegra_panel_ops *p_ops, char *panel_compatible)
{
	if (is_primary) {
		fixed_primary_panel_ops = p_ops;
		fixed_primary_panel_compatible =
			(const char *)panel_compatible;
	} else {
		fixed_secondary_panel_ops = p_ops;
		fixed_secondary_panel_compatible =
			(const char *)panel_compatible;
	}
}

void tegra_set_fixed_pwm_bl_ops(struct pwm_bl_data_dt_ops *p_ops)
{
	fixed_pwm_bl_ops = p_ops;
}

static bool tegra_available_pwm_bl_ops_register(struct device *dev)
{
	struct device_node *np_bl = NULL;
	struct device_node *np_parent = NULL;
	const char *pn_compat = NULL;
	bool ret = false;

	np_parent = of_find_node_by_path("/backlight");
	if (np_parent) {
		for_each_available_child_of_node(np_parent, np_bl) {
			if (np_bl)
				break;
		}
	}

	if (!np_bl) {
		pr_info("no avaiable target backlight node\n");
		goto end;
	}

	pn_compat = of_get_property(np_bl, "compatible", NULL);
	if (!pn_compat) {
		WARN(1, "No compatible prop in backlight node\n");
		goto end;
	}

	if (of_device_is_compatible(np_bl, "p,wuxga-10-1-bl")) {
		dev_set_drvdata(dev, dsi_p_wuxga_10_1_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "lg,wxga-7-bl")) {
		dev_set_drvdata(dev, dsi_lgd_wxga_7_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "s,wqxga-10-1-bl")) {
		dev_set_drvdata(dev, dsi_s_wqxga_10_1_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "c,wxga-14-0-bl")) {
		dev_set_drvdata(dev, lvds_c_1366_14_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "a,1080p-14-0-bl")) {
		dev_set_drvdata(dev, dsi_a_1080p_14_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "j,1440-810-5-8-bl")) {
		dev_set_drvdata(dev, dsi_j_1440_810_5_8_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "s,wuxga-7-0-bl")) {
		dev_set_drvdata(dev, dsi_s_wuxga_7_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "s,wuxga-8-0-bl")) {
		dev_set_drvdata(dev, dsi_s_wuxga_8_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "a,wuxga-8-0-bl")) {
		dev_set_drvdata(dev, dsi_a_1200_1920_8_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "a,wxga-8-0-bl")) {
		dev_set_drvdata(dev, dsi_a_1200_800_8_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "i-edp,1080p-11-6-bl")) {
		dev_set_drvdata(dev, edp_i_1080p_11_6_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "a-edp,1080p-14-0-bl")) {
		dev_set_drvdata(dev, edp_a_1080p_14_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "j,720p-5-0-bl")) {
		dev_set_drvdata(dev, dsi_j_720p_5_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "l,720p-5-0-bl")) {
		dev_set_drvdata(dev, dsi_l_720p_5_loki_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "s-edp,uhdtv-15-6-bl")) {
		dev_set_drvdata(dev, edp_s_uhdtv_15_6_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "o,720-1280-6-0-bl")) {
		dev_set_drvdata(dev, dsi_o_720p_6_0_ops.pwm_bl_ops);
	} else if (of_device_is_compatible(np_bl, "dsi,1080p-bl")) {
	} else if (of_device_is_compatible(np_bl, "dsi,2820x720-bl")) {
	} else if (of_device_is_compatible(np_bl, "dsi,25x16-bl")) {
	} else {
		pr_info("invalid compatible for backlight node\n");
		goto end;
	}

	ret = true;
end:
	of_node_put(np_parent);
	of_node_put(np_bl);
	return ret;
}
static void tegra_pwm_bl_ops_reg_based_on_disp_board_id(struct device *dev)
{
	struct board_info display_board;

	bool is_dsi_a_1200_1920_8_0 = false;
	bool is_dsi_a_1200_800_8_0 = false;
	bool is_edp_i_1080p_11_6 = false;
	bool is_edp_a_1080p_14_0 = false;
	bool is_edp_s_2160p_15_6 = false;

	tegra_get_display_board_info(&display_board);

	switch (display_board.board_id) {
	case BOARD_E1627:
	case BOARD_E1797:
		dev_set_drvdata(dev, dsi_p_wuxga_10_1_ops.pwm_bl_ops);
		break;
	case BOARD_E1549:
		dev_set_drvdata(dev, dsi_lgd_wxga_7_0_ops.pwm_bl_ops);
		break;
	case BOARD_E1639:
	case BOARD_E1813:
	case BOARD_E2145:
		dev_set_drvdata(dev, dsi_s_wqxga_10_1_ops.pwm_bl_ops);
		break;
	case BOARD_PM366:
		dev_set_drvdata(dev, lvds_c_1366_14_ops.pwm_bl_ops);
		break;
	case BOARD_PM354:
		dev_set_drvdata(dev, dsi_a_1080p_14_0_ops.pwm_bl_ops);
		break;
	case BOARD_E2129:
		dev_set_drvdata(dev, dsi_j_1440_810_5_8_ops.pwm_bl_ops);
		break;
	case BOARD_E2534:
		if (display_board.fab == 0x2)
			dev_set_drvdata(dev,
				dsi_j_720p_5_ops.pwm_bl_ops);
		else if (display_board.fab == 0x1)
			dev_set_drvdata(dev,
				dsi_j_1440_810_5_8_ops.pwm_bl_ops);
		else
			dev_set_drvdata(dev,
				dsi_l_720p_5_loki_ops.pwm_bl_ops);
		break;
	case BOARD_E1937:
	case BOARD_E2149:
		is_dsi_a_1200_1920_8_0 = true;
		break;
	case BOARD_E1807:
		is_dsi_a_1200_800_8_0 = true;
		break;
	case BOARD_P1761:
		if (tegra_get_board_panel_id())
			is_dsi_a_1200_1920_8_0 = true;
		else
			is_dsi_a_1200_800_8_0 = true;
		break;
	case BOARD_PM363:
	case BOARD_E1824:
		if (of_machine_is_compatible("nvidia,jetson-cv"))
			is_edp_s_2160p_15_6 = true;
		else if (display_board.sku == 1200)
			is_edp_i_1080p_11_6 = true;
		else
			is_edp_a_1080p_14_0 = true;
		break;
	default:
		/* TODO
		 * Finally, check if there's fixed_pwm_bl_ops
		 */
		if (fixed_pwm_bl_ops)
			dev_set_drvdata(dev, fixed_pwm_bl_ops);
		else
			pr_info("pwm_bl_ops are not required\n");
	};

	if (is_dsi_a_1200_1920_8_0)
		dev_set_drvdata(dev, dsi_a_1200_1920_8_0_ops.pwm_bl_ops);
	if (is_dsi_a_1200_800_8_0)
		dev_set_drvdata(dev, dsi_a_1200_800_8_0_ops.pwm_bl_ops);

	if (is_edp_i_1080p_11_6)
		dev_set_drvdata(dev, edp_i_1080p_11_6_ops.pwm_bl_ops);
	if (is_edp_a_1080p_14_0)
		dev_set_drvdata(dev, edp_a_1080p_14_0_ops.pwm_bl_ops);
	if (is_edp_s_2160p_15_6)
		dev_set_drvdata(dev, edp_s_uhdtv_15_6_ops.pwm_bl_ops);
}

void tegra_pwm_bl_ops_register(struct device *dev)
{
	bool ret = 0;
	ret = tegra_available_pwm_bl_ops_register(dev);
	if (!ret)
		tegra_pwm_bl_ops_reg_based_on_disp_board_id(dev);
}

void ti_lp855x_bl_ops_register(struct device *dev)
{
	dev_set_drvdata(dev, &generic_bl_ops);
}

static void tegra_panel_register_ops(struct tegra_dc_out *dc_out,
				struct tegra_panel_ops *p_ops)
{
	BUG_ON(!dc_out);

	if (!p_ops) {
		/* TODO: register default ops */
	}

	dc_out->enable = p_ops->enable;
	dc_out->postpoweron = p_ops->postpoweron;
	dc_out->prepoweroff = p_ops->prepoweroff;
	dc_out->disable = p_ops->disable;
	dc_out->hotplug_init = p_ops->hotplug_init;
	dc_out->postsuspend = p_ops->postsuspend;
	dc_out->hotplug_report = p_ops->hotplug_report;
}
static struct device_node *available_internal_panel_select(
			struct tegra_dc_platform_data *pdata)
{
	struct device_node *np_panel = NULL;
	struct device_node *np_dsi = NULL, *np_sor = NULL;
	struct tegra_dc_out *dc_out = NULL;
	const char *pn_compat = NULL;
	struct device_node *default_out = NULL;

	/*
	 * for internal panel node, search
	 * child node from DSI_NODE at first.
	 * If child node is not found, search
	 * child node from SOR_NODE.
	 */

	np_dsi = of_find_node_by_path(DSI_NODE);
	if (np_dsi) {
		for_each_available_child_of_node(np_dsi, np_panel) {
			if (np_panel)
				break;
		}
	}

	if (!np_panel) {
		np_sor = of_find_node_by_path(SOR_NODE);
		if (np_sor) {
			for_each_available_child_of_node(np_sor, np_panel) {
				if (np_panel) {
					default_out = of_get_child_by_name
						(np_panel, "disp-default-out");
					if (default_out)
						break;
				}
			}
			if (!default_out) {
				for_each_available_child_of_node(np_sor,
						np_panel) {
					if (np_panel)
						break;
				}
			}
		}
	}

	if (!np_panel) {
		pr_info("panel_select fail by _node_status\n");
		goto end;
	}

	if (!pdata)
		goto end;

	dc_out = pdata->default_out;
	if (!dc_out) {
		WARN(1, "dc_out is not valid\n");
		of_node_put(np_panel);
		np_panel = NULL;
		goto end;
	}

	pn_compat = of_get_property(np_panel, "compatible", NULL);
	if (!pn_compat) {
		WARN(1, "panel node do not have compatible prop\n");
		of_node_put(np_panel);
		np_panel = NULL;
		goto end;
	}

	if (of_device_is_compatible(np_panel, "p,wuxga-10-1")) {
		tegra_panel_register_ops(dc_out,
			&dsi_p_wuxga_10_1_ops);
	} else if (of_device_is_compatible(np_panel, "lg,wxga-7")) {
		tegra_panel_register_ops(dc_out,
			&dsi_lgd_wxga_7_0_ops);
	} else if (of_device_is_compatible(np_panel, "s,wqxga-10-1")) {
		tegra_panel_register_ops(dc_out,
			&dsi_s_wqxga_10_1_ops);
	} else if (of_device_is_compatible(np_panel, "c,wxga-14-0")) {
		tegra_panel_register_ops(dc_out,
			&lvds_c_1366_14_ops);
	} else if (of_device_is_compatible(np_panel, "a,1080p-14-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_a_1080p_14_0_ops);
	} else if (of_device_is_compatible(np_panel, "j,1440-810-5-8")) {
		tegra_panel_register_ops(dc_out,
			&dsi_j_1440_810_5_8_ops);
	} else if (of_device_is_compatible(np_panel, "s,wuxga-7-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_s_wuxga_7_0_ops);
	} else if (of_device_is_compatible(np_panel, "s,wuxga-8-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_s_wuxga_8_0_ops);
	} else if (of_device_is_compatible(np_panel, "a,wuxga-8-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_a_1200_1920_8_0_ops);
	} else if (of_device_is_compatible(np_panel, "a,wxga-8-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_a_1200_800_8_0_ops);
	} else if (of_device_is_compatible(np_panel, "i-edp,1080p-11-6")) {
		tegra_panel_register_ops(dc_out,
			&edp_i_1080p_11_6_ops);
	} else if (of_device_is_compatible(np_panel, "a-edp,1080p-14-0")) {
		tegra_panel_register_ops(dc_out,
			&edp_a_1080p_14_0_ops);
	} else if (of_device_is_compatible(np_panel, "j,720p-5-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_j_720p_5_ops);
	} else if (of_device_is_compatible(np_panel, "l,720p-5-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_l_720p_5_loki_ops);
	} else if (of_device_is_compatible(np_panel, "s-edp,uhdtv-15-6")) {
		tegra_panel_register_ops(dc_out,
			&edp_s_uhdtv_15_6_ops);
	} else if (of_device_is_compatible(np_panel, "o,720-1280-6-0")) {
		tegra_panel_register_ops(dc_out,
			&dsi_o_720p_6_0_ops);
	} else if (of_device_is_compatible(np_panel, "dsi,1080p")) {
	} else if (of_device_is_compatible(np_panel, "dsi,2820x720")) {
	} else if (of_device_is_compatible(np_panel, "dsi,25x16")) {
	} else {
		pr_info("invalid panel compatible\n");
		of_node_put(np_panel);
		np_panel = NULL;
	}

end:
	of_node_put(np_dsi);
	of_node_put(np_sor);
	return np_panel;
}

static struct device_node
	*internal_panel_select_by_disp_board_id(
		struct tegra_dc_platform_data *pdata)
{
	struct device_node *np_panel = NULL;
	struct tegra_dc_out *dc_out = NULL;
	struct board_info display_board;

	bool is_dsi_a_1200_1920_8_0 = false;
	bool is_dsi_a_1200_800_8_0 = false;
	bool is_edp_i_1080p_11_6 = false;
	bool is_edp_a_1080p_14_0 = false;
	bool is_edp_s_2160p_15_6 = false;

	tegra_get_display_board_info(&display_board);
	pr_info("display board info: id 0x%x, fab 0x%x\n",
		display_board.board_id, display_board.fab);

	if (pdata)
		dc_out = pdata->default_out;

	switch (display_board.board_id) {
	case BOARD_E1627:
	case BOARD_E1797:
		np_panel = of_find_compatible_node(NULL, NULL, "p,wuxga-10-1");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_p_wuxga_10_1_ops);
		break;
	case BOARD_E1549:
		np_panel = of_find_compatible_node(NULL, NULL, "lg,wxga-7");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_lgd_wxga_7_0_ops);
		break;
	case BOARD_E1639:
	case BOARD_E1813:
	case BOARD_E2145:
		np_panel = of_find_compatible_node(NULL, NULL, "s,wqxga-10-1");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_s_wqxga_10_1_ops);
		break;
	case BOARD_PM366:
		np_panel = of_find_compatible_node(NULL, NULL, "c,wxga-14-0");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&lvds_c_1366_14_ops);
		break;
	case BOARD_PM354:
		np_panel = of_find_compatible_node(NULL, NULL, "a,1080p-14-0");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_a_1080p_14_0_ops);
		break;
	case BOARD_E2129:
		np_panel = of_find_compatible_node(NULL,
			NULL, "j,1440-810-5-8");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_j_1440_810_5_8_ops);
		break;
	case BOARD_E2534:
		if (display_board.fab == 0x2) {
			np_panel = of_find_compatible_node(NULL, NULL,
				"j,720p-5-0");
			if (np_panel && pdata && dc_out)
				tegra_panel_register_ops(dc_out,
					&dsi_j_720p_5_ops);
		} else if (display_board.fab == 0x1) {
			np_panel = of_find_compatible_node(NULL, NULL,
				"j,1440-810-5-8");
			if (np_panel && pdata && dc_out)
				tegra_panel_register_ops(dc_out,
					&dsi_j_1440_810_5_8_ops);
		} else {
			np_panel = of_find_compatible_node(NULL, NULL,
				"l,720p-5-0");
			if (np_panel && pdata && dc_out)
				tegra_panel_register_ops(dc_out,
					&dsi_l_720p_5_loki_ops);
		}
		break;
	case BOARD_E1937:
	case BOARD_E2149:
		is_dsi_a_1200_1920_8_0 = true;
		break;
	case BOARD_E1807:
		is_dsi_a_1200_800_8_0 = true;
		break;
	case BOARD_P1761:
		if (tegra_get_board_panel_id())
			is_dsi_a_1200_1920_8_0 = true;
		else
			is_dsi_a_1200_800_8_0 = true;
		break;
	case BOARD_PM363:
	case BOARD_E1824:
		if (of_machine_is_compatible("nvidia,jetson-cv"))
			is_edp_s_2160p_15_6 = true;
		else if (display_board.sku == 1200)
			is_edp_i_1080p_11_6 = true;
		else
			is_edp_a_1080p_14_0 = true;
		break;
	default:
		/* If display panel is not searched by display board id,
		 * check if there's fixed primary panel.
		 */
		if (fixed_primary_panel_ops &&
			fixed_primary_panel_compatible) {
			np_panel = of_find_compatible_node(NULL, NULL,
				fixed_primary_panel_compatible);
			if (np_panel && pdata && dc_out)
				tegra_panel_register_ops(dc_out,
					fixed_primary_panel_ops);
		}
	};

	if (is_dsi_a_1200_1920_8_0) {
		np_panel = of_find_compatible_node(NULL, NULL,
				"a,wuxga-8-0");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_a_1200_1920_8_0_ops);
	}
	if (is_dsi_a_1200_800_8_0) {
		np_panel = of_find_compatible_node(NULL, NULL,
				"a,wxga-8-0");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&dsi_a_1200_800_8_0_ops);
	}

	if (is_edp_i_1080p_11_6) {
		np_panel = of_find_compatible_node(NULL, NULL,
				"i-edp,1080p-11-6");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&edp_i_1080p_11_6_ops);
	}
	if (is_edp_a_1080p_14_0) {
		np_panel = of_find_compatible_node(NULL, NULL,
				"a-edp,1080p-14-0");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&edp_a_1080p_14_0_ops);
	}
	if (is_edp_s_2160p_15_6) {
		np_panel = of_find_compatible_node(NULL, NULL,
				"s-edp,uhdtv-15-6");
		if (np_panel && pdata && dc_out)
			tegra_panel_register_ops(dc_out,
				&edp_s_uhdtv_15_6_ops);
	}
	if (np_panel)
		return np_panel;
	else
		return NULL;
}

struct device_node *tegra_primary_panel_get_dt_node(
			struct tegra_dc_platform_data *pdata)
{
	struct device_node *np_panel = NULL;
	struct tegra_dc_out *dc_out = NULL;
	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);

	if (pdata)
		dc_out = pdata->default_out;

	np_panel =
		internal_panel_select_by_disp_board_id(pdata);
	if (np_panel) {
		/*
		 * legacy method to select internal panel
		 * based on disp board id.
		 */
		of_node_put(np_hdmi);
		return np_panel;
	};

	np_panel =
		available_internal_panel_select(pdata);

	if (np_panel) {
		/*
		 * search internal panel node by
		 * status property.
		 */
		of_node_put(np_hdmi);
		return np_panel;
	};

	/*
	 * Check hdmi primary, if there's neither
	 * valid internal panel nor fixed panel.
	 */
	np_panel =
		of_get_child_by_name(np_hdmi, "hdmi-display");

	of_node_put(np_hdmi);
	return of_device_is_available(np_panel) ? np_panel : NULL;
}

struct device_node *tegra_secondary_panel_get_dt_node(
			struct tegra_dc_platform_data *pdata)
{
	struct device_node *np_panel = NULL, *np_display = NULL;
	struct tegra_dc_out *dc_out = NULL;
	const char *sor1_output_type;

	if (pdata)
		dc_out = pdata->default_out;

	if (fixed_secondary_panel_ops &&
			fixed_secondary_panel_compatible) {
			np_panel = of_find_compatible_node(NULL, NULL,
				fixed_secondary_panel_compatible);
			if (np_panel && pdata && dc_out)
				tegra_panel_register_ops(dc_out,
					fixed_secondary_panel_ops);
	} else {
		np_display = of_find_node_by_path(SOR1_NODE);
		if (!of_property_read_string(np_display,
			"nvidia,sor1-output-type", &sor1_output_type)) {
			if (strcmp(sor1_output_type, "dp") == 0) {
				np_panel = of_get_child_by_name(np_display,
						"dp-display");
				goto success;
			}
		}
	}

	of_node_put(np_display);
	np_display = of_find_node_by_path(HDMI_NODE);
	np_panel = of_get_child_by_name(np_display, "hdmi-display");

success:
	of_node_put(np_display);
	return of_device_is_available(np_panel) ? np_panel : NULL;
}

void tegra_fb_copy_or_clear(void)
{
	bool fb_existed = (tegra_fb_start && tegra_fb_size) ?
		true : false;
	bool fb2_existed = (tegra_fb2_start && tegra_fb2_size) ?
		true : false;

	/* Copy the bootloader fb to the fb. */
	if (fb_existed) {
		if (tegra_bootloader_fb_size)
			__tegra_move_framebuffer(NULL,
				tegra_fb_start, tegra_bootloader_fb_start,
				min(tegra_fb_size, tegra_bootloader_fb_size));
		else
			__tegra_clear_framebuffer(NULL,
				tegra_fb_start, tegra_fb_size);
	}

	/* Copy the bootloader fb2 to the fb2. */
	if (fb2_existed) {
		if (tegra_bootloader_fb2_size)
			__tegra_move_framebuffer(NULL,
				tegra_fb2_start, tegra_bootloader_fb2_start,
				min(tegra_fb2_size, tegra_bootloader_fb2_size));
		else
			__tegra_clear_framebuffer(NULL,
				tegra_fb2_start, tegra_fb2_size);
	}
}

int tegra_disp_defer_vcore_override(void)
{
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_13x_SOC)
	struct clk *disp1_clk = clk_get_sys("tegradc.0", NULL);
	struct clk *disp2_clk = clk_get_sys("tegradc.1", NULL);
	long disp1_rate = 0;
	long disp2_rate = 0;
	struct device_node *np_target_disp1 = NULL;
	struct device_node *np_target_disp2 = NULL;
	struct device_node *np_disp1_timings = NULL;
	struct device_node *np_disp1_def_out = NULL;
	struct device_node *np_disp2_def_out = NULL;
	struct device_node *entry = NULL;
	u32 temp = 0;
	bool is_hdmi_primary = false;

	if (WARN_ON(IS_ERR(disp1_clk))) {
		if (disp2_clk && !IS_ERR(disp2_clk))
			clk_put(disp2_clk);
		return PTR_ERR(disp1_clk);
	}

	if (WARN_ON(IS_ERR(disp2_clk))) {
		clk_put(disp1_clk);
		return PTR_ERR(disp1_clk);
	}

	np_target_disp1 = tegra_primary_panel_get_dt_node(NULL);
	np_target_disp2 = tegra_secondary_panel_get_dt_node(NULL);

	if (np_target_disp1)
		if (of_device_is_compatible(np_target_disp1, "hdmi,display"))
			is_hdmi_primary = true;

	if (!is_hdmi_primary) {
		/*
		 * internal panel is mapped to dc0,
		 * hdmi disp is mapped to dc1.
		 */
		if (np_target_disp1) {
			np_disp1_timings =
				of_get_child_by_name(np_target_disp1,
				"display-timings");
		}
		if (np_disp1_timings) {
			for_each_child_of_node(np_disp1_timings, entry) {
				if (!of_property_read_u32(entry,
					"clock-frequency", &temp)) {
					disp1_rate = (long) temp;
					break;
				}
			}
		}
		if (np_target_disp2) {
			np_disp2_def_out =
				of_get_child_by_name(np_target_disp2,
				"disp-default-out");
		}

		if (np_disp2_def_out) {
			if (!of_property_read_u32(np_disp2_def_out,
				"nvidia,out-max-pixclk", &temp)) {
				disp2_rate = PICOS2KHZ(temp) * 1000;
			}
		} else
			disp2_rate = 297000000; /* HDMI 4K */
		if (disp1_rate)
			tegra_dvfs_resolve_override(disp1_clk, disp1_rate);
		if (disp2_rate)
			tegra_dvfs_resolve_override(disp2_clk, disp2_rate);
	} else {
		/*
		 * this is hdmi primary
		 */
		if (np_target_disp1) {
			np_disp1_def_out =
				of_get_child_by_name(np_target_disp1,
				"disp-default-out");
		}
		if (np_disp1_def_out) {
			if (!of_property_read_u32(np_disp1_def_out,
				"nvidia,out-max-pixclk", &temp)) {
				disp1_rate = PICOS2KHZ(temp) * 1000;
			}
		} else
			disp1_rate = 297000000; /* HDMI 4K */

		if (disp1_rate)
			tegra_dvfs_resolve_override(disp1_clk, disp1_rate);
	}

	clk_put(disp1_clk);
	clk_put(disp2_clk);
#endif
	return 0;
}
