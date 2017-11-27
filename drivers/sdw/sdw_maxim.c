/*
 * sdw_maxim.c -- Maxim SoundWire slave device driver. Dummy driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/sdw_bus.h>
#include <linux/module.h>


static int maxim_register_sdw_capabilties(struct sdw_slave *sdw,
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
		dpn_cap->prepare_ch = SDW_CP_SM;
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
static int maxim_sdw_probe(struct sdw_slave *sdw,
				 const struct sdw_slave_id *sdw_id)
{
	dev_info(&sdw->dev, "Maxim SoundWire Slave Registered %lx\n", sdw_id->driver_data);
	return maxim_register_sdw_capabilties(sdw, sdw_id);
}

static int maxim_sdw_remove(struct sdw_slave *sdw)
{
	dev_info(&sdw->dev, "Maxim SoundWire Slave un-Registered\n");
	return 0;
}

static const struct sdw_slave_id maxim_id[] = {
	{"03:01:9f:79:00:00", 0},
	{"09:01:9f:79:00:00", 1},
	{"04:01:9f:79:00:00", 2},
	{"0a:01:9f:79:00:00", 3},
	{"04:01:9f:79:00:00", 4},
	{"0a:01:9f:79:00:00", 5},
	{"05:01:9f:79:00:00", 6},
	{"06:01:9f:79:00:00", 7},
	{"05:01:9f:79:00:00", 8},
	{"00:01:9f:79:00:00", 9},
	{"06:01:9f:79:00:00", 10},
	{"07:01:9f:79:00:00", 11},
	{"00:01:9f:79:00:00", 12},
	{"06:01:9f:79:00:00", 13},
	{"01:01:9f:79:00:00", 14},
	{"07:01:9f:79:00:00", 15},
	{"08:01:9f:79:00:00", 16},
	{"01:01:9f:79:00:00", 17},
	{"07:01:9f:79:00:00", 18},
	{"02:01:9f:79:00:00", 19},
	{"08:01:9f:79:00:00", 20},
	{"09:01:9f:79:00:00", 21},
	{"02:01:9f:79:00:00", 22},
	{"08:01:9f:79:00:00", 23},
	{"03:01:9f:79:00:00", 24},
	{"09:01:9f:79:00:00", 25},
	{"0a:01:9f:79:00:00", 26},
	{},
};

MODULE_DEVICE_TABLE(sdw, maxim_id);

static struct sdw_slave_driver maxim_sdw_driver = {
	.driver_type = SDW_DRIVER_TYPE_SLAVE,
	.driver = {
		   .name = "maxim",
		   },
	.probe = maxim_sdw_probe,
	.remove = maxim_sdw_remove,
	.id_table = maxim_id,
};

module_sdw_slave_driver(maxim_sdw_driver);

MODULE_DESCRIPTION("SoundWire Maxim Slave Driver");
MODULE_AUTHOR("Hardik Shah, <hardik.t.shah@intel.com>");
MODULE_LICENSE("GPL");
