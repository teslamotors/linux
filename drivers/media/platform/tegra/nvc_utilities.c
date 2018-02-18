/*
 * nvc_utilities.c - nvc utility functions
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation. All Rights Reserved.
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

#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <linux/export.h>

#include "nvc_utilities.h"

#ifdef NVC_IMAGER_DUMP
static void nvc_imager_dump(
	struct nvc_imager_cap *cap,
	struct nvc_imager_static_nvc *stc)
{
	pr_info("%s imager_capability:\n", __func__);
	pr_info("    cap_version:            %d\n", cap->cap_version);
	pr_info("    identifier:             %s\n", cap->identifier);
	pr_info("    sensor_nvc_interface:   %d\n", cap->sensor_nvc_interface);
	pr_info("    pixel_type:             %x\n", cap->pixel_types[0]);
	pr_info("    orientation:            %d\n", cap->orientation);
	pr_info("    direction:              %d\n", cap->direction);
	pr_info("    initial_clock_rate_khz: %d\n",
			cap->initial_clock_rate_khz);
	pr_info("    external_clock:         %d\n",
			cap->clock_profiles[0].external_clock_khz);
	pr_info("    clock_multipiler:       %d\n",
			cap->clock_profiles[0].clock_multiplier);
	pr_info("    csi_port:               %d\n", cap->csi_port);
	pr_info("    data_lanes:             %d\n", cap->data_lanes);
	pr_info("    virtual_channel_id:     %d\n", cap->virtual_channel_id);
	pr_info("    discontinuous_clk_mode: %d\n",
			cap->discontinuous_clk_mode);
	pr_info("    cil_threshold_settle:   %d\n", cap->cil_threshold_settle);
	pr_info("    preferred_mode_index:   %d\n", cap->preferred_mode_index);
	pr_info("    flash_control_enabled:  %s\n",
			cap->flash_control_enabled ? "yes" : "no");
	pr_info("    adjustable_flash_timing:   %d\n",
			cap->adjustable_flash_timing);
	pr_info("    is_hdr:                 %s\n", cap->is_hdr ? "yes" : "no");
	pr_info("    h_sync_edge:            %d\n", cap->h_sync_edge);
	pr_info("    v_sync_edge:            %d\n", cap->v_sync_edge);
	pr_info("    mclk_on_vgp0:           %d\n", cap->mclk_on_vgp0);
	pr_info("    min_blank_time_width:   %d\n", cap->min_blank_time_width);
	pr_info("    min_blank_time_height:  %d\n", cap->min_blank_time_height);
	pr_info("    focuser_guid:           %llx\n", cap->focuser_guid);
	pr_info("    torch_guid:             %llx\n", cap->torch_guid);

	if (stc == NULL)
		return;
	pr_info("%s imager_static:\n", __func__);
	pr_info("    api_version:            %d\n", stc->api_version);
	pr_info("    sensor_type:            %d\n", stc->sensor_type);
	pr_info("    bits_per_pixel:         %d\n", stc->bits_per_pixel);
	pr_info("    sensor_id:              %d\n", stc->sensor_id);
	pr_info("    sensor_id_minor:        %d\n", stc->sensor_id_minor);
	pr_info("    focal_len:              %d\n", stc->focal_len);
	pr_info("    max_aperture:           %d\n", stc->max_aperture);
	pr_info("    fnumber:                %d\n", stc->fnumber);
	pr_info("    view_angle_h:           %d\n", stc->view_angle_h);
	pr_info("    view_angle_v:           %d\n", stc->view_angle_v);

}

static void nvc_focuser_dump(
	struct nvc_focus_cap *cap,
	struct nvc_focus_nvc *stc)
{
	pr_info("%s focuser_capability:\n", __func__);
	pr_info("    version:            %d\n", cap->version);
	pr_info("    slew_rate:          0x%x\n", cap->slew_rate);
	pr_info("    settle_time:        %d\n", cap->settle_time);
	pr_info("    actuator_range:     %d\n", cap->actuator_range);
	pr_info("    focus_macro:        %d\n", cap->focus_macro);
	pr_info("    focus_hyper:        %d\n", cap->focus_hyper);
	pr_info("    focus_infinity:     %d\n", cap->focus_infinity);
	pr_info("    position_translate: %d\n", cap->position_translate);

	if (stc == NULL)
		return;
	pr_info("%s focuser_static:\n", __func__);
	pr_info("    focal_len:              %d\n", stc->focal_length);
	pr_info("    max_aperture:           %d\n", stc->max_aperture);
	pr_info("    fnumber:                %d\n", stc->fnumber);
}
#else
#define nvc_imager_dump(...)
#define nvc_focuser_dump(...)
#endif

int nvc_imager_parse_caps(struct device_node *np,
		struct nvc_imager_cap *imager_cap,
		struct nvc_imager_static_nvc *imager_static)
{
	const char *cap_identifier = NULL;
	u32 temp_prop_read = 0;

	if (imager_cap == NULL) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EFAULT;
	}

	of_property_read_string(np, "cap-identifier", &cap_identifier);
	if (!cap_identifier)
		return -EFAULT;

	strcpy(imager_cap->identifier, cap_identifier);
	of_property_read_u32(np, "cap-version",
				&imager_cap->cap_version);
	of_property_read_u32(np, "cap-sensor_nvc_interface",
				&imager_cap->sensor_nvc_interface);
	of_property_read_u32(np, "cap-pixel_types",
				&imager_cap->pixel_types[0]);
	of_property_read_u32(np, "cap-orientation",
				&imager_cap->orientation);
	of_property_read_u32(np, "cap-direction",
				&imager_cap->direction);
	of_property_read_u32(np, "cap-initial_clock_rate_khz",
				&imager_cap->initial_clock_rate_khz);
	of_property_read_u32(np, "cap-h_sync_edge",
				&imager_cap->h_sync_edge);
	of_property_read_u32(np, "cap-v_sync_edge",
				&imager_cap->v_sync_edge);
	of_property_read_u32(np, "cap-mclk_on_vgp0",
				&imager_cap->mclk_on_vgp0);
	of_property_read_u32(np, "cap-csi_port",
				&temp_prop_read);
	imager_cap->csi_port = (u8)temp_prop_read;
	of_property_read_u32(np, "cap-data_lanes",
				&temp_prop_read);
	imager_cap->data_lanes = (u8)temp_prop_read;
	of_property_read_u32(np, "cap-virtual_channel_id",
				&temp_prop_read);
	imager_cap->virtual_channel_id = (u8)temp_prop_read;
	of_property_read_u32(np, "cap-discontinuous_clk_mode",
				&temp_prop_read);
	imager_cap->discontinuous_clk_mode = (u8)temp_prop_read;
	of_property_read_u32(np, "cap-cil_threshold_settle",
				&temp_prop_read);
	imager_cap->cil_threshold_settle = (u8)temp_prop_read;
	of_property_read_u32(np, "cap-min_blank_time_width",
				&temp_prop_read);
	imager_cap->min_blank_time_width = (s32)temp_prop_read;
	of_property_read_u32(np, "cap-min_blank_time_height",
				&temp_prop_read);
	imager_cap->min_blank_time_height = (s32)temp_prop_read;
	of_property_read_u32(np, "cap-preferred_mode_index",
				&imager_cap->preferred_mode_index);
	of_property_read_u32(np, "cap-external_clock_khz_0",
		&imager_cap->clock_profiles[0].external_clock_khz);
	of_property_read_u32(np, "cap-clock_multiplier_0",
		&imager_cap->clock_profiles[0].clock_multiplier);
	of_property_read_u32(np, "cap-external_clock_khz_1",
		&imager_cap->clock_profiles[1].external_clock_khz);
	of_property_read_u32(np, "cap-clock_multiplier_1",
		&imager_cap->clock_profiles[1].clock_multiplier);
	if (of_property_read_bool(np, "cap-flash-control")) {
		imager_cap->flash_control_enabled = true;
		of_property_read_u32(np, "cap-flash-timing", &temp_prop_read);
		imager_cap->adjustable_flash_timing = (u8)temp_prop_read;
	}
	imager_cap->is_hdr = of_property_read_bool(np, "cap-hdr-enabled");
	of_property_read_u64(np, "cap-focuser_guid",
				&imager_cap->focuser_guid);
	of_property_read_u64(np, "cap-torch_guid",
				&imager_cap->torch_guid);

	if (imager_static == NULL)
		goto imager_parse_caps_done;

	of_property_read_u32(np, "static-version",
		&imager_static->api_version);
	of_property_read_u32(np, "static-sensor-type",
		&imager_static->sensor_type);
	of_property_read_u32(np, "static-bits-per-pixel",
		&imager_static->bits_per_pixel);
	of_property_read_u32(np, "static-sensor-id",
		&imager_static->sensor_id);
	of_property_read_u32(np, "static-sensor-id-minor",
		&imager_static->sensor_id_minor);
	of_property_read_u32(np, "static-focal-length",
		&imager_static->focal_len);
	of_property_read_u32(np, "static-max-aperture",
		&imager_static->max_aperture);
	of_property_read_u32(np, "static-f-number",
		&imager_static->fnumber);
	of_property_read_u32(np, "static-view-angle-h",
		&imager_static->view_angle_h);
	of_property_read_u32(np, "static-view-angle-v",
		&imager_static->view_angle_v);

imager_parse_caps_done:
	nvc_imager_dump(imager_cap, imager_static);
	return 0;
}
EXPORT_SYMBOL(nvc_imager_parse_caps);

int nvc_focuser_parse_caps(struct device_node *np,
		struct nvc_focus_cap *focuser_cap,
		struct nvc_focus_nvc *static_info)
{
	if (focuser_cap == NULL) {
		pr_err("%s: NULL pointer\n", __func__);
		return -EFAULT;
	}

	of_property_read_u32(np, "version", &focuser_cap->version);
	of_property_read_u32(np, "slew-rate", &focuser_cap->slew_rate);
	of_property_read_u32(np, "settle-time", &focuser_cap->settle_time);
	focuser_cap->settle_time /= 1000; /* converts to mS */
	of_property_read_u32(np, "range", &focuser_cap->actuator_range);
	of_property_read_u32(np, "macro-position", &focuser_cap->focus_macro);
	of_property_read_u32(np, "hyper-value", &focuser_cap->focus_hyper);
	of_property_read_u32(np, "infinity-position",
			&focuser_cap->focus_infinity);
	of_property_read_u32(np, "translate", &focuser_cap->position_translate);

	if (static_info == NULL)
		goto focuser_parse_caps_done;

	of_property_read_u32(np, "focal-length", &static_info->focal_length);
	of_property_read_u32(np, "f-number", &static_info->fnumber);
	of_property_read_u32(np, "max-aperture", &static_info->max_aperture);

focuser_parse_caps_done:
	nvc_focuser_dump(focuser_cap, static_info);
	return 0;
}
EXPORT_SYMBOL(nvc_focuser_parse_caps);

unsigned long nvc_imager_get_mclk(const struct nvc_imager_cap *cap,
				  const struct nvc_imager_cap *cap_default,
				  int profile)
{
	unsigned long mclk_freq_khz = 0;

	if (profile < 0) { /* initial rate */
		if (cap)
			mclk_freq_khz = cap->initial_clock_rate_khz;

		if (mclk_freq_khz == 0 && cap_default)
			mclk_freq_khz = cap_default->initial_clock_rate_khz;

		if (mclk_freq_khz == 0)
			mclk_freq_khz = 6000;

	} else { /* rate from clock profile N */
		if (cap)
			mclk_freq_khz = cap->clock_profiles[profile]
					.external_clock_khz;

		if (mclk_freq_khz == 0 && cap_default)
			mclk_freq_khz = cap_default->clock_profiles[profile]
					.external_clock_khz;

		if (mclk_freq_khz == 0)
			mclk_freq_khz = 24000;
	}

	return mclk_freq_khz * 1000;
}
EXPORT_SYMBOL(nvc_imager_get_mclk);
