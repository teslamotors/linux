/*
 * escore-vs.h  --  voice sense interface for Audience earSmart chips
 *
 * Copyright 2011-2013 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_VS_H
#define _ESCORE_VS_H

#include <linux/firmware.h>

/* Maximum size of keyword parameter block in bytes. */
#define ES_VS_KEYWORD_PARAM_MAX 512
#define MAX_NO_OF_VS_KW		5

/* Preset ID used for Hotword (9999) */
#define ES_VS_HOTWORD_PRESET	0x270F
/* Keyword length of 2 seconds */
#define ES_VS_HOTWORD_LENGTH	2000

#ifdef CONFIG_SND_SOC_ES_CVQ_TIME_MEASUREMENT
#define es_cvq_profiling(x) getnstimeofday(x)
#else
#define es_cvq_profiling(x)
#endif
struct escore_voice_sense {
	int vs_wakeup_keyword;
	int vs_irq;
	struct firmware *vs;
	struct firmware *bkg;
	struct firmware *kw[MAX_NO_OF_VS_KW];
	u16 vs_active_keywords;
	u16 vs_get_event;
	struct mutex vs_event_mutex;
	u16 cvs_preset;
	u16 vs_route_preset;
	u16 es_vs_keyword_length;
#ifdef CONFIG_SND_SOC_ES_AVOID_REPEAT_FW_DOWNLOAD
	bool vs_download_req;
#endif
};

extern int escore_vs_init(struct escore_priv *escore);
void escore_vs_exit(struct escore_priv *escore);
extern int escore_vs_load(struct escore_priv *escore);
extern int escore_get_vs_sleep(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_sleep(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_get_voice_recognition_enum(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_voice_recognition_enum(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol);
int escore_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_put_vs_keyword_length(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_vs_keyword_length(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_keyword_overrun(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_vs_sleep_enable(struct escore_priv *escore);
int escore_put_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_vs_request_firmware(struct escore_priv *escore,
				const char *vs_filename);
void escore_vs_release_firmware(struct escore_priv *escore);
int escore_vs_request_bkg(struct escore_priv *escore, const char *vs_filename);
void escore_vs_release_bkg(struct escore_priv *escore);
int escore_vs_request_keywords(struct escore_priv *escore, unsigned int value);
void escore_vs_release_keywords(struct escore_priv *escore);
int escore_vs_write_bkg_and_keywords(struct escore_priv *escore);
int escore_vs_put_control_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
int escore_vs_get_control_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
int escore_vs_put_control_value(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int escore_vs_get_control_value(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);

#ifdef CONFIG_SND_SOC_ES_AVOID_REPEAT_FW_DOWNLOAD
static inline void escore_set_vs_download_req(struct escore_priv *escore,
								bool val)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore->voice_sense;
	voice_sense->vs_download_req = val;
}

static inline bool escore_get_vs_download_req(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore->voice_sense;
	return voice_sense->vs_download_req;
}

#endif
#endif /* _ESCORE_VS_H */
