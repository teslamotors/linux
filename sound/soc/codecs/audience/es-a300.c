
/*
 * es-a300.c  --  Audience eS755 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Rajat Aggarwal <raggarwal@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "escore.h"
#include "es-a300-reg.h"

static DECLARE_TLV_DB_SCALE(spkr_tlv, -3600, 200, 1);
static DECLARE_TLV_DB_SCALE(ep_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(aux_tlv, 0, 150, 0);
static DECLARE_TLV_DB_SCALE(mic_tlv, 0, 150, 0);

static const unsigned int hp_tlv[] = {
	TLV_DB_RANGE_HEAD(6),
	0, 0, TLV_DB_SCALE_ITEM(-6000, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(-5100, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(-4300, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(-3700, 500, 0),
	6, 11, TLV_DB_SCALE_ITEM(-2300, 300, 0),
	12, 15, TLV_DB_SCALE_ITEM(-600, 200, 0),
};

static const char * const michs_sel_mux_text[] = {
	"MIC0_PGA", "MIC1_PGA",
};

static const struct soc_enum michs_sel_mux_enum =
	SOC_ENUM_SINGLE(ES_MICHS_IN_SEL, ES_MICHS_IN_SEL_SHIFT,
		ARRAY_SIZE(michs_sel_mux_text), michs_sel_mux_text);

static const char * const micx_bias_output_voltage_text[] = {
	"1.6V", "1.8V", "2.0V", "2.2V", "2.4V", "2.6V", "2.8V", "3.0V",
};

static const unsigned int micx_bias_output_voltage_value[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static const char * const lo_gain_text[] = {
	"mute", "-6db", "-4db", "-2db", "0db", "2db",
};

static const unsigned int lo_gain_value[] = {
	0, 9, 10, 11, 12, 13,
};

static const struct soc_enum lol_gain_enum =
	SOC_VALUE_ENUM_SINGLE(ES_LO_L_GAIN, ES_LO_L_GAIN_SHIFT,
		ES_LO_L_GAIN_MASK, ARRAY_SIZE(lo_gain_text),
			lo_gain_text,
			lo_gain_value);

static const struct soc_enum lor_gain_enum =
	SOC_VALUE_ENUM_SINGLE(ES_LO_R_GAIN, ES_LO_R_GAIN_SHIFT,
		ES_LO_R_GAIN_MASK, ARRAY_SIZE(lo_gain_text),
			lo_gain_text,
			lo_gain_value);

static const struct soc_enum mic0_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB0_TRIM, ES_MB0_TRIM_SHIFT,
		ES_MB0_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);


static const struct soc_enum mic1_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB1_TRIM, ES_MB1_TRIM_SHIFT,
		ES_MB1_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const struct soc_enum mic2_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB2_TRIM, ES_MB2_TRIM_SHIFT,
		ES_MB2_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const struct soc_enum michs_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MBHS_TRIM, ES_MBHS_TRIM_SHIFT,
		ES_MBHS_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const char * const micx_zin_mode_text[] = {
	"100kohm", "50kohm", "25kohm", "Attenuate by 3dB",
};

static const char * const squelch_mode_texts[] = {
	"Off", "On"
};
static const unsigned int squelch_mode_value[] = {
	0, 3,
};

static const struct soc_enum mic0_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC0_ZIN_MODE, ES_MIC0_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum mic1_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC1_ZIN_MODE, ES_MIC1_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum mic2_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC2_ZIN_MODE, ES_MIC2_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum michs_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MICHS_ZIN_MODE, ES_MICHS_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum squelch_mode_enum =
	SOC_VALUE_ENUM_SINGLE(ES_SQUELCH_THRESHOLD, ES_SQUELCH_THRESHOLD_SHIFT,
		ES_SQUELCH_THRESHOLD_MASK, ARRAY_SIZE(squelch_mode_texts),
		squelch_mode_texts, squelch_mode_value);

static const char * const bps_text[] = {
	"0", "16", "20", "24", "32",
};

static const unsigned int bps_value_text[] = {
	0, 15, 19, 23, 31,
};

const struct snd_kcontrol_new es_codec_snd_controls[] = {

	SOC_DOUBLE_R_TLV("SPKR Gain", ES_SPKRL_GAIN, ES_SPKRR_GAIN,
			0, 0x1F, 0, spkr_tlv),
	SOC_DOUBLE_R_TLV("HP Gain", ES_HPL_GAIN, ES_HPR_GAIN,
			0, 0x0F, 0, hp_tlv),
	SOC_DOUBLE_R_TLV("AUXIN Gain", ES_AUXL_ON, ES_AUXR_ON,
			1, 0x14, 0, aux_tlv),


	SOC_SINGLE_TLV("SPKRL Gain", ES_SPKRL_GAIN, ES_SPKRL_GAIN_SHIFT,
			ES_SPKR_L_GAIN_MAX, 0, spkr_tlv),
	SOC_SINGLE_TLV("SPKRR Gain", ES_SPKRR_GAIN, ES_SPKRR_GAIN_SHIFT,
			ES_SPKR_R_GAIN_MAX, 0, spkr_tlv),
	SOC_ENUM("LOL Gain", lol_gain_enum),
	SOC_ENUM("LOR Gain", lor_gain_enum),
	SOC_SINGLE_TLV("HPL Gain", ES_HPL_GAIN, ES_HPL_GAIN_SHIFT,
			ES_HPL_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("HPR Gain", ES_HPR_GAIN, ES_HPR_GAIN_SHIFT,
			ES_HPR_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("EP Gain", ES_EP_GAIN, ES_EP_GAIN_SHIFT,
			ES_EP_GAIN_MAX, 0, ep_tlv),
	SOC_SINGLE_TLV("AUXINL Gain", ES_AUXL_GAIN, ES_AUXL_GAIN_SHIFT,
			ES_AUXL_GAIN_MAX, 0, aux_tlv),
	SOC_SINGLE_TLV("AUXINR Gain", ES_AUXR_GAIN, ES_AUXR_GAIN_SHIFT,
			ES_AUXR_GAIN_MAX, 0, aux_tlv),


	SOC_SINGLE_TLV("MIC0 Gain", ES_MIC0_GAIN, ES_MIC0_GAIN_SHIFT,
			ES_MIC0_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MIC1 Gain", ES_MIC1_GAIN, ES_MIC1_GAIN_SHIFT,
			ES_MIC1_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MIC2 Gain", ES_MIC2_GAIN, ES_MIC2_GAIN_SHIFT,
			ES_MIC2_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MICHS Gain", ES_MICHS_GAIN, ES_MICHS_GAIN_SHIFT,
			ES_MICHS_GAIN_MAX, 0, mic_tlv),


	SOC_SINGLE("ADC Mute", ES_ADC_MUTE, ES_ADC_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("EP Mute", ES_EP_MUTE, ES_EP_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPL Mute", ES_HPL_MUTE, ES_HPL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPR Mute", ES_HPR_MUTE, ES_HPR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("SPKRL Mute", ES_SPKRL_MUTE, ES_SPKRL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("SPKRR Mute", ES_SPKRR_MUTE, ES_SPKRR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOL Mute", ES_LO_L_MUTE, ES_LO_L_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOR Mute", ES_LO_R_MUTE, ES_LO_R_MUTE_SHIFT, 1, 0),

	SOC_ENUM("MICHS SEL MUX", michs_sel_mux_enum),

	SOC_ENUM("MIC0 Bias Output Voltage",
			mic0_bias_output_voltage_enum),
	SOC_ENUM("MIC1 Bias Output Voltage",
			mic1_bias_output_voltage_enum),
	SOC_ENUM("MIC2 Bias Output Voltage",
			mic2_bias_output_voltage_enum),
	SOC_ENUM("MICHS Bias Output Voltage",
			michs_bias_output_voltage_enum),

	SOC_ENUM("MIC0 Input Impedance Mode", mic0_zin_mode_enum),
	SOC_ENUM("MIC1 Input Impedance Mode", mic1_zin_mode_enum),
	SOC_ENUM("MIC2 Input Impedance Mode", mic2_zin_mode_enum),
	SOC_ENUM("MICHS Input Impedance Mode", michs_zin_mode_enum),

	SOC_ENUM("Squelch", squelch_mode_enum),
	SOC_SINGLE("Squelch Count Threshold", ES_SQUELCH_TERM_CNT,
			ES_SQUELCH_TERM_CNT_SHIFT, 15, 0),
};

static const char * const adc1_mux_text[] = {
	"MIC1_PGA", "MIC2_PGA",
};

static const struct soc_enum adc1_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC1_IN_SEL, ES_ADC1_IN_SEL_SHIFT,
		ARRAY_SIZE(adc1_mux_text), adc1_mux_text);

static const struct snd_kcontrol_new adc1_mux =
		SOC_DAPM_ENUM("ADC1 MUX Mux", adc1_mux_enum);

static const char * const adc2_mux_text[] = {
	"MIC2_PGA", "AUXR_PGA",
};

static const struct soc_enum adc2_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC2_IN_SEL, ES_ADC2_IN_SEL_SHIFT,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

static const struct snd_kcontrol_new adc2_mux =
		SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_mux_enum);


static const struct snd_kcontrol_new ep_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_DAC0L_TO_EP, ES_DAC0L_TO_EP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpl_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_DAC0L_TO_HPL, ES_DAC0L_TO_HPL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpr_mix[] = {
	SOC_DAPM_SINGLE("DAC0R", ES_DAC0R_TO_HPR, ES_DAC0R_TO_HPR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new spkrl_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_DAC0L_TO_SPKRL, ES_DAC0L_TO_SPKRL_SHIFT,
								1, 0),
	SOC_DAPM_SINGLE("DAC1L", ES_DAC1L_TO_SPKRL, ES_DAC1L_TO_SPKRL_SHIFT,
								 1, 0),
};

static const struct snd_kcontrol_new spkrr_mix[] = {
	SOC_DAPM_SINGLE("DAC0R", ES_DAC0R_TO_SPKRR, ES_DAC0R_TO_SPKRR_SHIFT,
								 1, 0),
	SOC_DAPM_SINGLE("DAC1R", ES_DAC1R_TO_SPKRR, ES_DAC1R_TO_SPKRR_SHIFT,
								 1, 0),
};

static const struct snd_kcontrol_new lo_l_mix[] = {
	SOC_DAPM_SINGLE("DAC1L", ES_DAC1L_TO_LO_L, ES_DAC1L_TO_LO_L_SHIFT,
								1, 0),
};

static const struct snd_kcontrol_new lo_r_mix[] = {
	SOC_DAPM_SINGLE("DAC1R", ES_DAC1R_TO_LO_R, ES_DAC1R_TO_LO_R_SHIFT,
								1, 0),
};

static const char *const mic0_pga_mux_names[] = {
	"NONE", "MIC-0", "MIC-HS"
};

static const struct soc_enum mic0_pga_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mic0_pga_mux_names),
			mic0_pga_mux_names);

static const struct snd_kcontrol_new mic0_pga_mux_controls =
	SOC_DAPM_ENUM("Switch", mic0_pga_mux_enum);

static const char *const mic1_pga_mux_names[] = {
	"NONE", "MIC-1", "MIC-HS"
};

static const struct soc_enum mic1_pga_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mic1_pga_mux_names),
			mic1_pga_mux_names);

static const struct snd_kcontrol_new mic1_pga_mux_controls =
	SOC_DAPM_ENUM("Switch", mic1_pga_mux_enum);

static int mic_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = w->codec;
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "MIC0", 4))
			snd_soc_write(codec, ES_MIC0_ON, 1);
		else if (!strncmp(w->name, "MIC1", 4))
			snd_soc_write(codec, ES_MIC1_ON, 1);
		else if (!strncmp(w->name, "MIC2", 4))
			snd_soc_write(codec, ES_MIC2_ON, 1);
		else if (!strncmp(w->name, "MICHS", 5))
			snd_soc_write(codec, ES_MICHS_ON, 1);
		else {
			pr_err("%s: Invalid Mic Widget ON = %s\n",
			       __func__, w->name);
			return -EINVAL;
		}

	} else {
		if (!strncmp(w->name, "MIC0", 4))
			snd_soc_write(codec, ES_MIC0_ON, 0);
		else if (!strncmp(w->name, "MIC1", 4))
			snd_soc_write(codec, ES_MIC1_ON, 0);
		else if (!strncmp(w->name, "MIC2", 4))
			snd_soc_write(codec, ES_MIC2_ON, 0);
		else if (!strncmp(w->name, "MICHS", 5))
			snd_soc_write(codec, ES_MICHS_ON, 0);
		else {
			pr_err("%s: Invalid Mic Widget OFF = %s\n",
			       __func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
}

const struct snd_soc_dapm_widget es_codec_dapm_widgets[] = {

	/* Inputs */
	SND_SOC_DAPM_MIC("MIC0", mic_event),
	SND_SOC_DAPM_MIC("MIC1", mic_event),
	SND_SOC_DAPM_MIC("MIC2", mic_event),
	SND_SOC_DAPM_MIC("MICHS", mic_event),
	SND_SOC_DAPM_MIC("AUXINM", NULL),
	SND_SOC_DAPM_MIC("AUXINP", NULL),

	/* Outputs */
	SND_SOC_DAPM_HP("HPL", NULL),
	SND_SOC_DAPM_HP("HPR", NULL),
	SND_SOC_DAPM_SPK("SPKRL", NULL),
	SND_SOC_DAPM_SPK("SPKRR", NULL),
	SND_SOC_DAPM_OUTPUT("EP"),
	SND_SOC_DAPM_LINE("AUXOUTL", NULL),
	SND_SOC_DAPM_LINE("AUXOUTR", NULL),

	/* Microphone bias */
	SND_SOC_DAPM_SUPPLY("MIC0 Bias", ES_MBIAS0_MODE,
		ES_MBIAS0_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC1 Bias", ES_MBIAS1_MODE,
		ES_MBIAS1_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC2 Bias", ES_MBIAS2_MODE,
		ES_MBIAS2_MODE_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_MUX("MIC0 PGA Mux", SND_SOC_NOPM, 0, 0,
		&mic0_pga_mux_controls),

	SND_SOC_DAPM_MUX("MIC1 PGA Mux", SND_SOC_NOPM, 0, 0,
		&mic1_pga_mux_controls),

	/* ADC */
	SND_SOC_DAPM_ADC("ADC0", NULL, ES_ADC0_ON, ES_ADC0_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC1 MUX", SND_SOC_NOPM, 0, 0, &adc1_mux),
	SND_SOC_DAPM_ADC("ADC1", NULL, ES_ADC1_ON, ES_ADC1_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &adc2_mux),
	SND_SOC_DAPM_ADC("ADC2", NULL, ES_ADC2_ON, ES_ADC2_ON_SHIFT, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL, ES_ADC3_ON, ES_ADC3_ON_SHIFT, 0),

	/* DAC */
	SND_SOC_DAPM_DAC("DAC0L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC0R", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC1L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC1R", NULL, SND_SOC_NOPM, 0, 0),

	/* Earphone Mixer */
	SND_SOC_DAPM_MIXER("EP MIXER", SND_SOC_NOPM, 0, 0,
		ep_mix, ARRAY_SIZE(ep_mix)),
	/* Headphone Mixer */
	SND_SOC_DAPM_MIXER("HPL MIXER", SND_SOC_NOPM, 0, 0,
		hpl_mix, ARRAY_SIZE(hpl_mix)),
	SND_SOC_DAPM_MIXER("HPR MIXER", SND_SOC_NOPM, 0, 0,
		hpr_mix, ARRAY_SIZE(hpr_mix)),

	/* Handsfree Mixer */
	SND_SOC_DAPM_MIXER("SPKRL MIXER", SND_SOC_NOPM, 0, 0,
		spkrl_mix, ARRAY_SIZE(spkrl_mix)),
	SND_SOC_DAPM_MIXER("SPKRR MIXER", SND_SOC_NOPM, 0, 0,
		spkrr_mix, ARRAY_SIZE(spkrr_mix)),

	/* LineOut Mixer */
	SND_SOC_DAPM_MIXER("LOL MIXER", SND_SOC_NOPM, 0, 0,
		lo_l_mix, ARRAY_SIZE(lo_l_mix)),
	SND_SOC_DAPM_MIXER("LOR MIXER", SND_SOC_NOPM, 0, 0,
		lo_r_mix, ARRAY_SIZE(lo_r_mix)),

	/* Output PGAs */
	SND_SOC_DAPM_PGA("SPKRL PGA", ES_SPKRL_ON, ES_SPKRL_ON_SHIFT,
		0, NULL, 0),
	SND_SOC_DAPM_PGA("SPKRR PGA", ES_SPKRR_ON, ES_SPKRR_ON_SHIFT,
		0, NULL, 0),
	SND_SOC_DAPM_PGA("HPL PGA", ES_HPL_ON, ES_HPL_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPR PGA", ES_HPR_ON, ES_HPR_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LOL PGA", ES_LO_L_ON, ES_LO_L_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LOR PGA", ES_LO_R_ON, ES_LO_R_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("EP PGA", ES_EP_ON, ES_EP_ON_SHIFT, 0, NULL, 0),

	/* Input PGAs */
	SND_SOC_DAPM_PGA("MIC0 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC1 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC2 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AUXINL PGA", ES_AUXL_ON, ES_AUXL_ON_SHIFT, 0,
		NULL, 0),
	SND_SOC_DAPM_PGA("AUXINR PGA", ES_AUXR_ON, ES_AUXR_ON_SHIFT,	0,
		NULL, 0),

};

/* TODO */
static const struct snd_soc_dapm_route intercon[] = {

	/* Capture path */

	{"MIC0", NULL, "MIC0 Bias"},
	{"MIC1", NULL, "MIC1 Bias"},
	{"MIC2", NULL, "MIC2 Bias"},

	{"MIC0 PGA Mux", "MIC-0", "MIC0"},
	{"MIC0 PGA Mux", "MIC-HS", "MICHS"},
	{"MIC0 PGA", NULL, "MIC0 PGA Mux"},

	{"MIC1 PGA Mux", "MIC-1", "MIC1"},
	{"MIC1 PGA Mux", "MIC-HS", "MICHS"},
	{"MIC1 PGA", NULL, "MIC1 PGA Mux"},

	{"MIC2 PGA", NULL, "MIC2"},

	{"AUXINL PGA", NULL, "AUXINP"},
	{"AUXINR PGA", NULL, "AUXINM"},

	{"ADC0", NULL, "MIC0 PGA"},
	{"ADC1 MUX", "MIC1_PGA", "MIC1 PGA"},
	{"ADC1 MUX", "MIC2_PGA", "MIC2 PGA"},
	{"ADC2 MUX", "MIC2_PGA", "MIC2 PGA"},
	{"ADC2 MUX", "AUXR_PGA", "AUXINR PGA"},
	{"ADC3", NULL, "AUXINL PGA"},

	{"ADC1", NULL, "ADC1 MUX"},
	{"ADC2", NULL, "ADC2 MUX"},

	/* Playback path */

	{"SPKRL MIXER", "DAC0L", "DAC0L"},
	{"SPKRL MIXER", "DAC1L", "DAC1L"},

	{"SPKRR MIXER", "DAC0R", "DAC0R"},
	{"SPKRR MIXER", "DAC1R", "DAC1R"},

	{"HPL MIXER", "DAC0L", "DAC0L"},

	{"HPR MIXER", "DAC0R", "DAC0R"},

	{"EP MIXER", "DAC0L", "DAC0L"},

	{"LOL MIXER", "DAC1L", "DAC1L"},

	{"LOR MIXER", "DAC1R", "DAC1R"},

	{"LOL PGA", NULL, "LOL MIXER"},
	{"LOR PGA", NULL, "LOR MIXER"},
	{"HPL PGA", NULL, "HPL MIXER"},
	{"HPR PGA", NULL, "HPR MIXER"},
	{"SPKRL PGA", NULL, "SPKRL MIXER"},
	{"SPKRR PGA", NULL, "SPKRR MIXER"},
	{"EP PGA", NULL, "EP MIXER"},

	{"AUXOUTL", NULL, "LOL PGA"},
	{"AUXOUTR", NULL, "LOR PGA"},
	{"HPL", NULL, "HPL PGA"},
	{"HPR", NULL, "HPR PGA"},
	{"SPKRL", NULL, "SPKRL PGA"},
	{"SPKRR", NULL, "SPKRR PGA"},
	{"EP", NULL, "EP PGA"},

};

int es_analog_add_snd_soc_controls(struct snd_soc_codec *codec)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es_codec_snd_controls,
			ARRAY_SIZE(es_codec_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es_codec_snd_controls,
			ARRAY_SIZE(es_codec_snd_controls));
#endif

	return rc;
}
int es_analog_add_snd_soc_dapm_controls(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_new_controls(&codec->dapm, es_codec_dapm_widgets,
					ARRAY_SIZE(es_codec_dapm_widgets));

	return rc;
}
int es_analog_add_snd_soc_route_map(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_add_routes(&codec->dapm, intercon,
					ARRAY_SIZE(intercon));

	return rc;
}
