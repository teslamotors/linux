/*
 * rt700-sdw.c -- rt700 ALSA SoC audio driver
 *
 * Copyright 2016 Realtek, Inc.
 *
 * Author: Bard Liao <bardliao@realtek.com>
 * ALC700 ASoC Codec Driver based Intel Dummy SdW codec driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#define DEBUG

#include <linux/delay.h>
#include <linux/sdw_bus.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include "rt700.h"
#include "rt700-sdw.h"

static bool rt700_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0020:
	case 0x0030:
	case 0x0060:
	case 0x0070:
	case 0x00e0:
	case 0x00f0:
	case 0x0000 ... 0x0005:
	case 0x0023 ... 0x0026:
	case 0x0032 ... 0x0036:
	case 0x0040 ... 0x0046:
	case 0x0050 ... 0x0055:
	case 0x0100 ... 0x0105:
	case 0x0120 ... 0x0127:
	case 0x0130 ... 0x0137:
	case 0x0200 ... 0x0205:
	case 0x0220 ... 0x0227:
	case 0x0230 ... 0x0237:
	case 0x0300 ... 0x0305:
	case 0x0320 ... 0x0327:
	case 0x0330 ... 0x0337:
	case 0x0400 ... 0x0405:
	case 0x0420 ... 0x0427:
	case 0x0430 ... 0x0437:
	case 0x0500 ... 0x0505:
	case 0x0520 ... 0x0527:
	case 0x0530 ... 0x0537:
	case 0x0600 ... 0x0605:
	case 0x0620 ... 0x0627:
	case 0x0630 ... 0x0637:
	case 0x0700 ... 0x0705:
	case 0x0720 ... 0x0727:
	case 0x0730 ... 0x0737:
	case 0x0800 ... 0x0805:
	case 0x0820 ... 0x0827:
	case 0x0830 ... 0x0837:
	case 0x0f00 ... 0x0f27:
	case 0x0f30 ... 0x0f37:
	case 0x2000 ... 0x200e:
	case 0x2012 ... 0x2016:
	case 0x201a ... 0x2027:
	case 0x2029 ... 0x202a:
	case 0x202d ... 0x2034:
	case 0x2100 ... 0x213f:
	case 0x2200 ... 0x2204:
	case 0x2206 ... 0x2212:
	case 0x2220 ... 0x2223:
	case 0x2230 ... 0x2231:
	case 0x3000 ... 0xffff:
		return true;
	default:
		return false;
	}
}

static bool rt700_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0000:
	case 0x0001:
	case 0x0004:
	case 0x0005:
	case 0x0040:
	case 0x0042:
	case 0x0043:
	case 0x0044:
	case 0x0045:
	case 0x0104:
	case 0x0204:
	case 0x0304:
	case 0x0404:
	case 0x0504:
	case 0x0604:
	case 0x0704:
	case 0x0804:
	case 0x0105:
	case 0x0205:
	case 0x0305:
	case 0x0405:
	case 0x0505:
	case 0x0605:
	case 0x0705:
	case 0x0805:
	case 0x0f00:
	case 0x0f04:
	case 0x0f05:
	case 0x2009:
	case 0x2016:
	case 0x201b:
	case 0x201c:
	case 0x201d:
	case 0x201f:
	case 0x2021:
	case 0x2023:
	case 0x2230:
	case 0x0050 ... 0x0055: /* device id */
	case 0x200b ... 0x200e: /* i2c read */
	case 0x2012 ... 0x2015: /* HD-A read */
	case 0x202d ... 0x202f: /* BRA */
	case 0x2201 ... 0x2212: /* i2c debug */
	case 0x2220 ... 0x2223: /* decoded HD-A */
		return true;
	case 0x3000 ... 0xffff: /* HD-A command */
		return false; /* should always read from cache */
	default:
		return true; /*for debug*/
	}
}

const struct regmap_config rt700_sdw_regmap = {
	.reg_bits = 32, /* Total register space for SDW */
	.val_bits = 8, /* Total number of bits in register */

	.readable_reg = rt700_readable_register, /* Readable registers */
	.volatile_reg = rt700_volatile_register, /* volatile register */

	.max_register = 0xffff, /* Maximum number of register */
	.reg_defaults = rt700_reg_defaults, /* Defaults */
	.num_reg_defaults = ARRAY_SIZE(rt700_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

int hda_to_sdw(unsigned int nid, unsigned int verb, unsigned int payload,
		unsigned int *sdw_addr_h, unsigned int *sdw_data_h,
		unsigned int *sdw_addr_l, unsigned int *sdw_data_l) {
	unsigned int offset_h, offset_l, e_verb;

	if (((verb & 0xff) != 0) || verb == 0xf00) { /* 12 bits command */
		if (verb == 0x7ff) /* special case */
			offset_h = 0;
		else
			offset_h = 0x3000;

		if (verb & 0x800) /* get command */
			e_verb = (verb - 0xf00) | 0x80;
		else /* set command */
			e_verb = (verb - 0x700);

		*sdw_data_h = payload; /* 7 bits payload */
		*sdw_addr_l = *sdw_data_l = 0;
	} else { /* 4 bits command */
		if ((verb & 0x800) == 0x800) { /* read */
			offset_h = 0x9000;
			offset_l = 0xa000;
		} else { /* write */
			offset_h = 0x7000;
			offset_l = 0x8000;
		}
		e_verb = verb >> 8;
		*sdw_data_h = (payload >> 8); /* 16 bits payload [15:8] */
		*sdw_addr_l = (e_verb << 8) | nid | 0x80; /* 0x80: valid bit */
		*sdw_addr_l += offset_l;
		*sdw_data_l = payload & 0xff;
	}

	*sdw_addr_h = (e_verb << 8) | nid;
	*sdw_addr_h += offset_h;

	return 0;
}
EXPORT_SYMBOL(hda_to_sdw);

static int rt700_register_sdw_capabilties(struct sdw_slave *sdw,
				 const struct sdw_slave_id *sdw_id)
{
	struct sdw_slv_capabilities cap;
	struct sdw_slv_dpn_capabilities *dpn_cap = NULL;
	struct port_audio_mode_properties *prop = NULL;
	unsigned int bus_config_frequencies[] = {
		2400000, 4800000, 9600000, 3000000, 6000000, 12000000, 19200000};
	unsigned int freq_supported[] = {
		19200000, 2400000, 4800000, 9600000, 12000000, 12288000};
	int i, j;

	cap.wake_up_unavailable = false;
	cap.test_mode_supported = true;
	cap.clock_stop1_mode_supported = true;
	cap.simplified_clock_stop_prepare = true;
	cap.highphy_capable = false;
	cap.paging_supported = false;
	cap.bank_delay_support = false;
	cap.port_15_read_behavior = 1;
	cap.sdw_dp0_supported = false;
	cap.num_of_sdw_ports = 8; /* Inport 4, Outport 4 */
	cap.scp_impl_def_intr_mask = 0x4;
	cap.sdw_dp0_cap = devm_kzalloc(&sdw->dev,
			sizeof(struct sdw_slv_dp0_capabilities), GFP_KERNEL);

	cap.sdw_dp0_cap->max_word_length = 64;
	cap.sdw_dp0_cap->min_word_length = 1;
	cap.sdw_dp0_cap->num_word_length = 0;
	cap.sdw_dp0_cap->word_length_buffer = NULL;
	cap.sdw_dp0_cap->bra_use_flow_control = 0;
	cap.sdw_dp0_cap->bra_initiator_supported = 0;
	cap.sdw_dp0_cap->ch_prepare_mode = SDW_SIMPLIFIED_CP_SM;
	cap.sdw_dp0_cap->impl_def_response_supported = false;
	/* Address 0x0001, bit 0: port ready, bit 2: BRA failure*/
	cap.sdw_dp0_cap->imp_def_intr_mask = 0;
	cap.sdw_dp0_cap->impl_def_bpt_supported = true;
	cap.sdw_dp0_cap->slave_bra_cap.max_bus_frequency = 24000000;
	cap.sdw_dp0_cap->slave_bra_cap.min_bus_frequency = 2400000;
	cap.sdw_dp0_cap->slave_bra_cap.num_bus_config_frequency = 7;
	cap.sdw_dp0_cap->slave_bra_cap.bus_config_frequencies =
						bus_config_frequencies;
	cap.sdw_dp0_cap->slave_bra_cap.max_data_per_frame = 470;
	cap.sdw_dp0_cap->slave_bra_cap.min_us_between_transactions = 0;
	cap.sdw_dp0_cap->slave_bra_cap.max_bandwidth = (500 - 48) * 48000;
	cap.sdw_dp0_cap->slave_bra_cap.mode_block_alignment = 1;

	cap.sdw_dpn_cap = devm_kzalloc(&sdw->dev,
			((sizeof(struct sdw_slv_dpn_capabilities)) *
			cap.num_of_sdw_ports), GFP_KERNEL);
	for (i = 1; i <= cap.num_of_sdw_ports; i++) {
		dpn_cap = &cap.sdw_dpn_cap[i-1];
		if ((i % 2) == 0)
			dpn_cap->port_direction = SDW_PORT_SINK;
		else
			dpn_cap->port_direction = SDW_PORT_SOURCE;

		dpn_cap->port_number = i;
		dpn_cap->max_word_length =  64;
		dpn_cap->min_word_length = 1;
		dpn_cap->num_word_length = 0;
		dpn_cap->word_length_buffer = NULL;
		dpn_cap->dpn_type = SDW_FULL_DP;
		dpn_cap->dpn_grouping = SDW_BLOCKGROUPCOUNT_1;
		dpn_cap->prepare_ch = SDW_SIMPLIFIED_CP_SM;
		dpn_cap->imp_def_intr_mask = 0; /* bit 0: Test Fail */
#ifdef CONFIG_SND_SOC_SDW_AGGM1M2
		dpn_cap->min_ch_num = 1;
#else
		dpn_cap->min_ch_num = 2;
#endif
		dpn_cap->max_ch_num = 2;
		dpn_cap->num_ch_supported = 0;
		dpn_cap->ch_supported = NULL;
		dpn_cap->port_flow_mode_mask = SDW_PORT_FLOW_MODE_ISOCHRONOUS;
		dpn_cap->block_packing_mode_mask =
				SDW_PORT_BLK_PKG_MODE_BLK_PER_PORT_MASK |
				SDW_PORT_BLK_PKG_MODE_BLK_PER_CH_MASK;
		dpn_cap->port_encoding_type_mask =
				SDW_PORT_ENCODING_TYPE_TWOS_CMPLMNT;
		dpn_cap->num_audio_modes = 1;

		dpn_cap->mode_properties = devm_kzalloc(&sdw->dev,
				((sizeof(struct port_audio_mode_properties)) *
				dpn_cap->num_audio_modes), GFP_KERNEL);
		for (j = 0; j < dpn_cap->num_audio_modes; j++) {
			prop = &dpn_cap->mode_properties[j];
			prop->max_frequency = 24000000;
			prop->min_frequency = 2400000;
			prop->num_freq_configs = 0;
			prop->freq_supported = devm_kzalloc(&sdw->dev,
						    prop->num_freq_configs *
						    (sizeof(unsigned int)),
						    GFP_KERNEL);
			if (!prop->freq_supported)
				return -ENOMEM;

			memcpy(prop->freq_supported, freq_supported,
						    prop->num_freq_configs *
						    (sizeof(unsigned int)));
			prop->glitchless_transitions_mask = 0x0;
			prop->max_sampling_frequency = 192000;
			prop->min_sampling_frequency = 8000;
			prop->num_sampling_freq_configs = 0;
			prop->sampling_freq_config = NULL;
			prop->ch_prepare_behavior = SDW_CH_PREP_ANY_TIME;
		}
	}
	return sdw_register_slave_capabilities(sdw, &cap);

}
static int rt700_sdw_probe(struct sdw_slave *sdw,
				 const struct sdw_slave_id *sdw_id)
{
	struct alc700 *alc700_priv;
	struct regmap *regmap;
	int ret;

	alc700_priv = kzalloc(sizeof(struct alc700), GFP_KERNEL);
	if(!alc700_priv) {
		return -ENOMEM;
	}
	dev_set_drvdata(&sdw->dev, alc700_priv);
	alc700_priv->sdw = sdw;
	alc700_priv->params = kzalloc(sizeof(struct sdw_bus_params), GFP_KERNEL);
	regmap = devm_regmap_init_sdw(sdw, &rt700_sdw_regmap);
	if (!regmap)
		return -EINVAL;
	ret = rt700_register_sdw_capabilties(sdw, sdw_id);
	if (ret)
		return ret;
	ret = sdw_slave_get_bus_params(sdw, alc700_priv->params);
	if (ret)
		return -EFAULT;
	return rt700_probe(&sdw->dev, regmap, sdw, sdw_id->driver_data);
}

static int rt700_sdw_remove(struct sdw_slave *sdw)
{
	return rt700_remove(&sdw->dev);
}

static const struct sdw_slave_id rt700_id[] = {
	{"10:02:5d:07:00:00", 0},
	{"11:02:5d:07:00:00", 0},
	{"12:02:5d:07:00:00", 0},
	{"13:02:5d:07:00:00", 0},
	{"14:02:5d:07:00:00", 0},
	{"15:02:5d:07:00:00", 0},
	{"16:02:5d:07:00:00", 0},
	{"17:02:5d:07:00:00", 0},
	{"10:02:5d:07:01:00", 0},
	{"11:02:5d:07:01:00", 0},
	{"12:02:5d:07:01:00", 0},
	{"13:02:5d:07:01:00", 0},
	{"14:02:5d:07:01:00", 0},
	{"15:02:5d:07:01:00", 0},
	{"16:02:5d:07:01:00", 0},
	{"17:02:5d:07:01:00", 0},
#ifndef CONFIG_SND_SOC_INTEL_CNL_FPGA
#ifndef CONFIG_SND_SOC_SDW_AGGM1M2
	{"10:02:5d:07:00:01", 0},
#else
	{"10:02:5d:07:00:01", 1},
	{"10:02:5d:07:01:02", 2},
	{"10:02:5d:07:01:03", 3},
#endif
#endif
	{}
};

MODULE_DEVICE_TABLE(sdw, rt700_id);


static int rt700_sdw_handle_impl_def_interrupts(struct sdw_slave *swdev,
		struct sdw_impl_def_intr_stat *intr_status)
{
	struct rt700_priv *rt700 = dev_get_drvdata(&swdev->dev);
	bool hp, mic;

	pr_debug("%s control_port_stat=%x port0_stat=%x\n", __func__,
		intr_status->control_port_stat, intr_status->port0_stat);
	if (intr_status->control_port_stat & 0x4) {
		rt700_jack_detect(rt700, &hp, &mic);
		pr_info("%s hp=%d mic=%d\n", __func__, hp, mic);
	}

	return 0;
}

static struct sdw_slave_driver rt700_sdw_driver = {
	.driver_type = SDW_DRIVER_TYPE_SLAVE,
	.driver = {
		   .name = "rt700",
		   .pm = &rt700_runtime_pm,
		   },
	.probe = rt700_sdw_probe,
	.remove = rt700_sdw_remove,
	.handle_impl_def_interrupts = rt700_sdw_handle_impl_def_interrupts,

	.id_table = rt700_id,
};

module_sdw_slave_driver(rt700_sdw_driver);

MODULE_DESCRIPTION("ASoC RT700 driver SDW");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
