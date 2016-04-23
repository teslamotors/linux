/*
 * svfpga-i2c.c -- SVFPGA ALSA SoC audio driver
 *
 * Copyright 2015 Intel, Inc.
 *
 * Author: Hardik Shah <hardik.t.shah@intel.com>
 * Dummy ASoC Codec Driver based Cirrus Logic CS42L42 Codec
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/sdw_bus.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include "svfpga.h"

extern const struct regmap_config svfpga_sdw_regmap;
extern const struct dev_pm_ops svfpga_runtime_pm;

static int svfpga_register_sdw_capabilties(struct sdw_slave *sdw,
				 const struct sdw_slave_id *sdw_id)
{
	struct sdw_slv_capabilities cap;
	struct sdw_slv_dpn_capabilities *dpn_cap = NULL;
	struct port_audio_mode_properties *prop = NULL;
	int i, j;

	cap.wake_up_unavailable = true;
	cap.test_mode_supported = false;
	cap.clock_stop1_mode_supported = false;
	cap.simplified_clock_stop_prepare = false;
	cap.highphy_capable = true;
	cap.paging_supported = false;
	cap.bank_delay_support = false;
	cap.port_15_read_behavior = 0;
	cap.sdw_dp0_supported = false;
	cap.num_of_sdw_ports = 3;
	cap.sdw_dpn_cap = devm_kzalloc(&sdw->dev,
			((sizeof(struct sdw_slv_dpn_capabilities)) *
			cap.num_of_sdw_ports), GFP_KERNEL);
	for (i = 0; i < cap.num_of_sdw_ports; i++) {
		dpn_cap = &cap.sdw_dpn_cap[i];
		if (i == 0 || i == 2)
			dpn_cap->port_direction = SDW_PORT_SOURCE;
		else
			dpn_cap->port_direction = SDW_PORT_SINK;

		dpn_cap->port_number = i+1;
		dpn_cap->max_word_length =  24;
		dpn_cap->min_word_length = 16;
		dpn_cap->num_word_length = 0;
		dpn_cap->word_length_buffer = NULL;
		dpn_cap->dpn_type = SDW_FULL_DP;
		dpn_cap->dpn_grouping = SDW_BLOCKGROUPCOUNT_1;
		dpn_cap->prepare_ch = SDW_SIMPLIFIED_CP_SM;
		dpn_cap->imp_def_intr_mask = 0x0;
		dpn_cap->min_ch_num = 1;
		dpn_cap->max_ch_num = 2;
		dpn_cap->num_ch_supported = 0;
		dpn_cap->ch_supported = NULL;
		dpn_cap->port_flow_mode_mask = SDW_PORT_FLOW_MODE_ISOCHRONOUS;
		dpn_cap->block_packing_mode_mask =
				SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT_MASK |
				SDW_PORT_BLK_PKG_MODE_BLK_PER_CH_MASK;
		dpn_cap->port_encoding_type_mask =
				SDW_PORT_ENCODING_TYPE_TWOS_CMPLMNT |
				SDW_PORT_ENCODING_TYPE_SIGN_MAGNITUDE |
				SDW_PORT_ENCODING_TYPE_IEEE_32_FLOAT;
		dpn_cap->num_audio_modes = 1;

		dpn_cap->mode_properties = devm_kzalloc(&sdw->dev,
				((sizeof(struct port_audio_mode_properties)) *
				dpn_cap->num_audio_modes), GFP_KERNEL);
		for (j = 0; j < dpn_cap->num_audio_modes; j++) {
			prop = &dpn_cap->mode_properties[j];
			prop->max_frequency = 16000000;
			prop->min_frequency = 1000000;
			prop->num_freq_configs = 0;
			prop->freq_supported = NULL;
			prop->glitchless_transitions_mask = 0x1;
			prop->max_sampling_frequency = 192000;
			prop->min_sampling_frequency = 8000;
			prop->num_sampling_freq_configs = 0;
			prop->sampling_freq_config = NULL;
			prop->ch_prepare_behavior = SDW_CH_PREP_ANY_TIME;
		}
	}
	return sdw_register_slave_capabilities(sdw, &cap);

}
static int svfpga_sdw_probe(struct sdw_slave *sdw,
				 const struct sdw_slave_id *sdw_id)
{
	struct regmap *regmap;
	int ret;

	/* Wait for codec internal initialization for 2.5ms minimum */
	msleep(5);
	regmap = devm_regmap_init_sdw(sdw, &svfpga_sdw_regmap);
	if (!regmap)
		return -EINVAL;
	ret = svfpga_register_sdw_capabilties(sdw, sdw_id);
	if (ret)
		return ret;
	return svfpga_probe(&sdw->dev, regmap, sdw);
}

static int svfpga_sdw_remove(struct sdw_slave *sdw)
{
	return svfpga_remove(&sdw->dev);
}

static const struct sdw_slave_id svfpga_id[] = {
	{"00:01:fa:42:42:00", 0},
	{"14:13:20:05:86:80", 0},
	{}
};

MODULE_DEVICE_TABLE(sdw, svfpga_id);

static struct sdw_slave_driver svfpga_sdw_driver = {
	.driver_type = SDW_DRIVER_TYPE_SLAVE,
	.driver = {
		   .name = "svfpga-codec",
		   .pm = &svfpga_runtime_pm,
		   },
	.probe = svfpga_sdw_probe,
	.remove = svfpga_sdw_remove,
	.id_table = svfpga_id,
};

module_sdw_slave_driver(svfpga_sdw_driver);

MODULE_DESCRIPTION("ASoC SVFPGA driver SDW");
MODULE_AUTHOR("Hardik shah, Intel Inc, <hardik.t.shah@intel.com");
MODULE_LICENSE("GPL");
