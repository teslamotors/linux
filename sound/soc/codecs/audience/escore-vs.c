/*
 * escore-vs.c  --  Audience Voice Sense component ALSA Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "escore.h"
#include "escore-vs.h"

#define VS_KCONTROL(method, type) \
	int escore_vs_##method##_control_##type(struct snd_kcontrol *kcontrol,\
			struct snd_ctl_elem_value *ucontrol)		\
{									\
	struct escore_priv *escore = &escore_priv;			\
	if (escore->mode != VOICESENSE) {				\
		dev_warn(escore->dev, "%s(): Not in VS mode\n",		\
				__func__);				\
		return 0;						\
	}								\
	return escore_##method##_control_##type(kcontrol, ucontrol);	\
}

VS_KCONTROL(get, value)
VS_KCONTROL(put, value)
VS_KCONTROL(get, enum)
VS_KCONTROL(put, enum)

static int escore_vs_sleep(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	u32 cmd, rsp;
	int skip_vs_seq = 0, skip_vs_load = 0;
	int rc;
#ifdef CONFIG_SND_SOC_ES_CVQ_TIME_MEASUREMENT
	struct timespec cvq_sleep_start;
	struct timespec cvq_sleep_end;
	struct timespec cvq_sleep_time;
	struct timespec vs_load_start;
	struct timespec vs_load_end;
	struct timespec vs_load_time;
	struct timespec wdb_start;
	struct timespec wdb_end;
	struct timespec wdb_time;
#endif

	dev_dbg(escore->dev, "%s()\n", __func__);

	es_cvq_profiling(&cvq_sleep_start);

	if (escore->escore_power_state == ES_SET_POWER_STATE_VS_OVERLAY &&
		escore->es_vs_route_preset == voice_sense->vs_route_preset &&
		!escore->voice_recognition) {
		dev_dbg(escore->dev,
			"%s() No route change. Skipping CVQ sequence\n",
			__func__);
		skip_vs_seq = 1;
		skip_vs_load = 1;
		goto vs_set_low_power;
	} else if (escore->escore_power_state == ES_SET_POWER_STATE_VS_OVERLAY) {
		skip_vs_load = 1;
		dev_dbg(escore->dev, "%s() already in VS overlay mode\n",
								__func__);
		goto vs_set_presets;
	}

#ifdef CONFIG_SND_SOC_ES_AVOID_REPEAT_FW_DOWNLOAD
	if (escore_get_vs_download_req(escore) == false) {
		/*
		 * VS binary already copied.
		 * So No need to re-download Binary.
		 * But send the Overlay Power State Command
		 */
		cmd = (ES_SET_POWER_STATE << 16) |
			ES_POWER_STATE_VS_OVERLAP;
	} else {
		cmd = (ES_SET_POWER_STATE << 16) |
			ES_SET_POWER_STATE_VS_OVERLAY;
	}
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set Power State cmd fail %d\n",
			__func__, rc);
		goto vs_sleep_err;
	}

	msleep(55); /* delay required for copying the VS firmware */
#else
	/* Set smooth mute to 0 */
	cmd = ES_SET_SMOOTH_MUTE << 16 | ES_SMOOTH_MUTE_ZERO;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set Smooth Mute cmd fail %d\n",
			__func__, rc);
		goto vs_sleep_err;
	}
	/* Its observed that an additional 30msec delay is needed for positive
	 * stress test results. vs_sleep found to be failing in rare instances
	 * without this.
	 */
	msleep(30);
	/* change power state to OVERLAY */
	cmd = (ES_SET_POWER_STATE << 16) | ES_SET_POWER_STATE_VS_OVERLAY;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set Power State cmd fail %d\n",
			__func__, rc);
		goto vs_sleep_err;
	}
	escore->mode = SBL;
	msleep(30);
#endif

	rc = escore_is_sleep_aborted(escore);
	if (rc == -EABORT)
		goto escore_sleep_aborted;

	es_cvq_profiling(&vs_load_start);

#ifdef CONFIG_SND_SOC_ES_AVOID_REPEAT_FW_DOWNLOAD
	if (escore_get_vs_download_req(escore) == true) {
		rc = escore_vs_load(&escore_priv);
		if (rc) {
			pr_err("%s(): VS fw download fail %d\n",
			       __func__, rc);
			goto vs_sleep_err;
		}
		escore_set_vs_download_req(escore, false);
	} else {
		pr_debug("%s() VS firmware is already downloaded", __func__);

		/* Setup the Event response */
		cmd = (ES_SET_EVENT_RESP << 16) |
						escore->pdata->gpio_b_irq_type;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc < 0) {
			pr_err("%s(): Error %d in setting event response\n",
					__func__, rc);
			goto vs_sleep_err;
		}
	}
#else
	/* download VS firmware */
	rc = escore_vs_load(escore);
	if (rc) {
		dev_err(escore->dev, "%s() VS FW load failed rc = %d\n",
					__func__, rc);
		goto vs_sleep_err;
	}
#endif

	escore->escore_power_state = ES_SET_POWER_STATE_VS_OVERLAY;

	es_cvq_profiling(&vs_load_end);

	/* Write hotword before CVS / VS preset */
	if (escore->voice_recognition) {
		es_cvq_profiling(&wdb_start);

		rc = escore_vs_write_bkg_and_keywords(escore);
		if (rc) {
			dev_err(escore->dev,
				"%s(): datablock write fail rc = %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}

		es_cvq_profiling(&wdb_end);
	}

vs_set_presets:
	if (!escore->voice_recognition) {
		cmd = ES_SET_PRESET << 16 | escore->es_vs_route_preset;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev, "%s(): Set Preset cmd fail %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}
		voice_sense->vs_route_preset = escore->es_vs_route_preset;
	}
	msleep(20);

	if (escore->voice_recognition)
		cmd = ES_SET_CVS_PRESET << 16 | ES_VS_HOTWORD_PRESET;
	else
		cmd = ES_SET_CVS_PRESET << 16 | escore->es_cvs_preset;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set CVS Preset cmd fail %d\n",
			__func__, rc);
		goto vs_sleep_err;
	}
	voice_sense->cvs_preset = escore->es_cvs_preset;

	rc = escore_is_sleep_aborted(escore);
	if (rc == -EABORT)
		goto escore_sleep_aborted;

	if (voice_sense->es_vs_keyword_length) {
		cmd = ((ES_SET_VS_KW_LENGTH << 16) |
				voice_sense->es_vs_keyword_length);
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev, "%s(): kw length cmd fail %d\n",
					__func__, rc);
			goto vs_sleep_err;
		}
	}

	/* Write CVQ model files after CVS / VS preset for Normal CVQ */
	if (!escore->voice_recognition) {
		es_cvq_profiling(&wdb_start);

		/* Reconfig API Interrupt mode */
		rc = escore_reconfig_api_intr(escore);
		if (rc)
			goto vs_sleep_err;

		/* write background model and keywords files */
		rc = escore_vs_write_bkg_and_keywords(escore);
		if (rc) {
			dev_err(escore->dev,
				"%s(): datablock write fail rc = %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}

		es_cvq_profiling(&wdb_end);

		cmd = ES_SET_ALGO_PARAM_ID << 16 | ES_VS_PROCESSING_MOE;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev,
				"%s(): Set Algo Param ID cmd fail %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}

		cmd = ES_SET_ALGO_PARAM << 16 | ES_VS_DETECT_KEYWORD;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev,
				"%s(): Set Algo Param cmd fail %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}
	} else {
		/* Send route preset after keyword length is set
		 * for hotword feature */
		cmd = ES_SET_PRESET << 16 | escore->es_vs_route_preset;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc) {
			dev_err(escore->dev, "%s(): Set Preset cmd fail %d\n",
				__func__, rc);
			goto vs_sleep_err;
		}
		voice_sense->vs_route_preset = escore->es_vs_route_preset;

		msleep(20);
	}

	rc = escore_is_sleep_aborted(escore);
	if (rc == -EABORT)
		goto escore_sleep_aborted;

	rc  = escore_start_int_osc(escore);
	if (rc) {
		dev_err(escore->dev, "%s(): int osc fail %d\n", __func__, rc);
		goto vs_sleep_err;
	}

vs_set_low_power:
	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(escore);

	cmd = (ES_SET_POWER_STATE << 16) | ES_SET_POWER_STATE_VS_LOWPWR;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc) {
		dev_err(escore->dev, "%s(): Set Power State cmd fail %d\n",
			__func__, rc);
		goto vs_sleep_err;
	}

	/* Wait for API Interrupt to confirm that device is in low power */
	if (escore->pdata->gpioa_gpio != -1) {
		rc = escore_api_intr_wait_completion(escore);
		if (rc)
			goto vs_sleep_err;
	}

	escore->escore_power_state = ES_SET_POWER_STATE_VS_LOWPWR;

escore_sleep_aborted:
vs_sleep_err:

	es_cvq_profiling(&cvq_sleep_end);

#ifdef CONFIG_SND_SOC_ES_CVQ_TIME_MEASUREMENT

	if (!skip_vs_load) {
		vs_load_time = (timespec_sub(vs_load_end, vs_load_start));
		dev_info(escore->dev, "VS firmware load time = %lu.%03lu sec\n",
				vs_load_time.tv_sec, (vs_load_time.tv_nsec)/1000000);
	}
	if (!skip_vs_seq) {
		wdb_time = (timespec_sub(wdb_end, wdb_start));
		dev_info(escore->dev, "BKG and KW write time = %lu.%03lu sec\n",
				wdb_time.tv_sec, (wdb_time.tv_nsec)/1000000);
	}
	cvq_sleep_time = (timespec_sub(cvq_sleep_end, cvq_sleep_start));
	dev_info(escore->dev, "Total CVQ sleep time = %lu.%03lu sec\n",
		cvq_sleep_time.tv_sec, (cvq_sleep_time.tv_nsec)/1000000);
#endif

	return rc;
}

int escore_vs_wakeup(struct escore_priv *escore)
{
	u32 cmd, rsp;
	int rc = 0;

	dev_dbg(escore->dev, "%s()\n", __func__);

	if (escore->escore_power_state == ES_SET_POWER_STATE_NORMAL) {
		dev_dbg(escore->dev, "%s() Already in normal mode\n", __func__);
		goto out;
	}

	rc = escore_wakeup(escore);
	if (rc) {
		dev_err(escore->dev, "%s() wakeup failed rc = %d\n",
				__func__, rc);
		goto vs_wakeup_err;
	}
	escore->escore_power_state = ES_SET_POWER_STATE_VS_OVERLAY;

	if (escore->intr_recvd) {
		/* Total 60 ms Delay is required for firmware to be ready
		 * after wakeup */
		msleep(60 - escore->delay.wakeup_to_vs);
		dev_dbg(escore->dev,
			"%s() Keeping chip in CVQ command mode\n", __func__);
		goto out;
	}

	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(escore);

	/* change power state to Normal*/
	cmd = (ES_SET_POWER_STATE << 16) | ES_SET_POWER_STATE_NORMAL;
	rc = escore_cmd_nopm(escore, cmd, &rsp);
	if (rc < 0) {
		dev_err(escore->dev, "%s() - failed sync cmd resume %d\n",
			__func__, rc);
		goto vs_wakeup_err;
	}

	if (escore->pdata->gpioa_gpio != -1) {
		/* Wait for API Interrupt to confirm
		 * that device is ready to accept commands */
		rc = escore_api_intr_wait_completion(escore);
		if (rc)
			goto vs_wakeup_err;

		/* Reconfig API Interrupt mode */
		rc = escore_reconfig_api_intr(escore);
		if (rc)
			goto vs_wakeup_err;
	} else
		msleep(escore->delay.vs_to_normal);

	escore->mode = STANDARD;
	escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;

out:
vs_wakeup_err:
	return rc;
}

static int escore_cvq_sleep_thread(void *ptr)
{
	struct escore_priv *escore = (struct escore_priv *)ptr;
	int rc;
	u32 cmd, rsp;

	rc = escore_vs_sleep(&escore_priv);
	if (rc != -EABORT)
		goto escore_cvq_sleep_thread_exit;

	dev_dbg(escore->dev, "%s() CVQ sleep aborted\n", __func__);

	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(escore);

	/* Change power state to Normal.
	 * As per BAS-2581,
	 *   - Host should use power state command with supress response when
	 *     firmware is running
	 *   - Host should use power state command without supress response when
	 *     chip is in SBL mode.
	 * This command response may not be received and needs to be ignored.
	 * Actual firmware transition is verified after checking power state.
	 */
	if (escore->mode != VOICESENSE_PENDING) {
		if (escore->mode == SBL)
			/* BAS-3232: Change power state to Normal, send
			 * command with SR bit disable on PRI interface,
			 * but device won't send any response back */
			cmd = ES_SET_POWER_STATE_CMD << 16 |
					ES_SET_POWER_STATE_NORMAL;
		else
			/* BAS-3232: Change power state to Normal, send
			 * command with SR bit enable on PRI interface */
			cmd = ES_SET_POWER_STATE << 16 |
					ES_SET_POWER_STATE_NORMAL;
		/* handle endianness */
		cmd = escore->bus.ops.cpu_to_bus(escore, cmd);
		rc = escore->bus.ops.write(escore, &cmd, sizeof(cmd));
		update_cmd_history(escore->bus.ops.bus_to_cpu(escore,
							cmd), 0);
		if (rc) {
			pr_err("%s() PRI INTF write is failed, rc = %d\n",
					__func__, rc);
			goto escore_cvq_sleep_thread_exit;
		}
	}

	if (escore->pdata->gpioa_gpio == -1) {
		if (escore->mode == VOICESENSE)
			/* BAS-3232: Delay required before reading power
			 * state in case of VOICESENSE mode:
			 * es75x: 90 ms
			 * es70x / es80x: 50 ms */
			msleep(escore->delay.vs_to_normal);
		else
			/* BAS-3232: Total delay required before reading power
			 * state is 60 ms in case of SBL and VOICESENSE_PENDING
			 * mode */
			msleep(60);
		cmd = ES_GET_POWER_STATE << 16;
		rc = escore_cmd_nopm(escore, cmd, &rsp);
		if (rc < 0) {
			pr_err("%s() Get Power State failed rc = %d\n",
							__func__, rc);
			goto escore_cvq_sleep_thread_exit;
		}
		if (rsp != ES_PS_NORMAL) {
			pr_err("%s() Power state is not normal, rsp = %x\n",
					__func__, rsp);
			rc = -EINVAL;
			goto escore_cvq_sleep_thread_exit;
		}
	} else {
		/* Wait for API Interrupt to confirm that
		 * device is in normal mode */
		rc = escore_api_intr_wait_completion(escore);
		if (rc)
			goto escore_cvq_sleep_thread_exit;
	}
	escore_priv.escore_power_state = ES_SET_POWER_STATE_NORMAL;

escore_cvq_sleep_thread_exit:
	return rc;
}

static int escore_voicesense_sleep(struct escore_priv *escore)
{

	return escore_cvq_sleep_thread((void *)escore);
}

int escore_put_vs_sleep(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	mutex_lock(&escore->access_lock);
	if (value) {

		rc = escore_pm_get_sync();
		if (rc < 0) {
			pr_err("%s(): pm_get_sync failed :%d\n", __func__, rc);
			mutex_unlock(&escore->access_lock);
			return rc;
		}

		rc = escore_cvq_sleep_thread(escore);
		escore_pm_put_autosuspend();

	} else
		rc = escore_vs_wakeup(&escore_priv);

	mutex_unlock(&escore->access_lock);
	return rc;
}

int escore_get_vs_sleep(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int escore_put_voice_recognition_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	unsigned int value;
	int rc = 0;
	u32 cmd, rsp;

	value = ucontrol->value.enumerated.item[0];

	mutex_lock(&escore->access_lock);

	rc = escore_pm_get_sync();
	if (rc < 0) {
		pr_err("%s(): pm_get_sync failed :%d\n", __func__, rc);
		mutex_unlock(&escore->access_lock);
		return rc;
	}

	escore->voice_recognition = value;

	if (escore->voice_recognition) {
		voice_sense->es_vs_keyword_length = ES_VS_HOTWORD_LENGTH;
		rc = escore_pm_put_sync_suspend();
	} else {
		if (escore->escore_power_state ==
			ES_POWER_STATE_VS_STREAMING) {
			/* change power state to OVERLAY */
			cmd = (ES_SET_POWER_STATE << 16) |
				ES_SET_POWER_STATE_VS_OVERLAY;
			rc = escore_cmd_nopm(escore, cmd, &rsp);
			if (rc) {
				dev_err(escore->dev,
					"%s(): Set Power State cmd fail %d\n",
					__func__, rc);
				goto voice_recognition_err;
			}
			msleep(escore->delay.vs_streaming_to_vs);
			escore->escore_power_state =
				ES_SET_POWER_STATE_VS_OVERLAY;
		}
		/* Reset Keyword length on reset */
		voice_sense->es_vs_keyword_length = 0;
		escore_pm_put_autosuspend();
	}

voice_recognition_err:
	mutex_unlock(&escore->access_lock);

	return rc;
}

int escore_get_voice_recognition_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	ucontrol->value.enumerated.item[0] = escore->voice_recognition;

	return 0;
}

int escore_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = escore_put_control_value(kcontrol, ucontrol);

	if (!rc)
		voice_sense->cvs_preset = value;

	return rc;
}

int escore_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	ucontrol->value.integer.value[0] = voice_sense->cvs_preset;

	return 0;
}

int escore_vs_sleep_enable(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	return voice_sense->vs_active_keywords;
}

int escore_put_vs_keyword_length(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	unsigned int value;
	int rc = 0;
	u32 cmd, rsp;

	value = ucontrol->value.integer.value[0];

	voice_sense->es_vs_keyword_length = value;

	if (!escore_vs_sleep_enable(&escore_priv)) {
		cmd = ((ES_SET_VS_KW_LENGTH << 16) |
				voice_sense->es_vs_keyword_length);
		rc = escore_cmd_locked(&escore_priv, cmd, &rsp);
		if (rc) {
			dev_err(escore_priv.dev, "%s(): kw length cmd fail %d\n",
					__func__, rc);
		}
	}

	return rc;
}

int escore_get_vs_keyword_length(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	ucontrol->value.integer.value[0] = voice_sense->es_vs_keyword_length;

	return 0;
}

int escore_get_keyword_overrun(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	u32 es_get_keyword_overrun = ESCORE_GET_KEYWORD_OVERRUN_ERROR << 16;
	u32 rspn = 0;

	rc = escore_cmd_locked(&escore_priv, es_get_keyword_overrun,
				&rspn);
	if (rc < 0) {
		dev_err(escore_priv.dev, "Failed to set the keyword length %d()",
				rc);
		return rc;
	}

	ucontrol->value.enumerated.item[0] = rspn & 0x0000ffff;
	dev_dbg(escore_priv.dev, "%s: Response 0x%08X", __func__,
				rspn);

	return 0;
}

int escore_put_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc < 0) {
		dev_err(escore->dev, "%s(): pm_get_sync failed :%d\n",
				__func__, rc);
		goto exit;
	}

	rc = escore_vs_request_keywords(escore, value);

	escore_pm_put_autosuspend();
exit:
	mutex_unlock(&escore->access_lock);
	return rc;
}

int escore_get_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	ucontrol->value.integer.value[0] = voice_sense->vs_active_keywords;

	return 0;
}

static ssize_t escore_vs_status_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Voice Sense Status";
	/* Disable vs status read for interrupt to work */
	struct escore_priv *escore = &escore_priv;
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	mutex_lock(&voice_sense->vs_event_mutex);

	value = voice_sense->vs_get_event;
	/* Reset the detection status after read */
	voice_sense->vs_get_event = ES_NO_EVENT;

	mutex_unlock(&voice_sense->vs_event_mutex);

	ret = snprintf(buf, PAGE_SIZE, "%s=0x%04x\n", status_name, value);

	return ret;
}

static DEVICE_ATTR(vs_status, 0444, escore_vs_status_show, NULL);

static struct attribute *vscore_sysfs_attrs[] = {
	&dev_attr_vs_status.attr,
	NULL
};

static struct attribute_group vscore_sysfs = {
	.attrs = vscore_sysfs_attrs
};

int escore_vs_request_firmware(struct escore_priv *escore,
				const char *vs_filename)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	return request_firmware((const struct firmware **)&voice_sense->vs,
			      vs_filename, escore->dev);
}

void escore_vs_release_firmware(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	release_firmware(voice_sense->vs);
}

int escore_vs_request_bkg(struct escore_priv *escore, const char *bkg_filename)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *) escore->voice_sense;
	int rc = 0;

	rc = request_firmware((const struct firmware **)&voice_sense->bkg,
			bkg_filename, escore->dev);
	if (rc) {
		dev_err(escore->dev, "%s(): request_firmware(%s) failed %d\n",
				__func__, bkg_filename, rc);
		return  rc;
	}
	return  rc;
}

void escore_vs_release_bkg(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	release_firmware(voice_sense->bkg);
}

int escore_vs_request_keywords(struct escore_priv *escore, unsigned int value)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	int rc = 0;
	int i;
	char kw_filename[] = "audience-vs-kw-1.bin";
	int size = sizeof(kw_filename)/sizeof(char);

	/* Keyword number is based on bit positions. For an example, for keyword
	 * number 3, value is 100B i.e. 4 in decimal. Similarly for keywords 4
	 * and 5, 4th and 5th bit is set i.e. 11000 i.e. 18 (bit position is
	 * started from 1 here).
	 *
	 * Flow of function:
	 *	- Release already requested keyword
	 *	- Request new keyword if it is selected
	 *	- In case of error, release all keywords
	 */

	for (i = 0; i < MAX_NO_OF_VS_KW; i++) {

		/* Release keyword if it is requested before. This is required
		 * for kernel 3.10 in which if old keyword is not released,
		 * keyword data remains unchanged.
		 */
		if (voice_sense->vs_active_keywords & BIT(i)) {
			dev_dbg(escore->dev, "%s(): release kw = %d\n",
							__func__, i + 1);
			release_firmware(voice_sense->kw[i]);
			voice_sense->vs_active_keywords &= (~BIT(i));
		}

		/* If keyword is not set, do not request it. */
		if (!(value & BIT(i)))
			continue;

		/* Request selected keywords */
		snprintf(kw_filename, size, "audience-vs-kw-%d.bin", i + 1);
		dev_dbg(escore->dev, "%s(): kw filename = %s\n",
				__func__, kw_filename);
		rc = request_firmware(
				(const struct firmware **)&(voice_sense->kw[i]),
				kw_filename, escore->dev);
		if (rc) {
			dev_err(escore->dev, "%s(): request kw(%d) failed %d\n",
					__func__, i + 1, rc);
			goto request_firmware_kw_exit;
		}
		voice_sense->vs_active_keywords |= BIT(i);
	}

	return rc;

request_firmware_kw_exit:

	/* In case of failure, release all keywords */
	for (i = 0; i < MAX_NO_OF_VS_KW; i++) {
		if (voice_sense->vs_active_keywords & BIT(i)) {
			dev_dbg(escore->dev, "%s(): release kw %d\n",
							__func__, i + 1);
			release_firmware(voice_sense->kw[i]);
		}
	}
	voice_sense->vs_active_keywords = 0;

	return  rc;
}

int escore_vs_write_bkg_and_keywords(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	int rc;
	int i;

	rc = escore_datablock_open(escore);
	if (rc) {
		dev_err(escore->dev, "%s(): can't open datablock device = %d\n",
			__func__, rc);
		goto escore_vs_write_bkg_keywords_exit;
	}

	/* Write Hotword datablock received from user space */
	if (escore->voice_recognition && escore->hotword_buf) {
		dev_dbg(escore->dev, "%s(): Write hotword, size = %d\n",
				__func__, escore->hotword_buf_size);
		rc = escore_datablock_write(escore, escore->hotword_buf,
						escore->hotword_buf_size);
		if ((rc < 0) || (rc < escore->hotword_buf_size)) {
			dev_err(escore->dev,
				"%s(): hotword write failed rc = %d\n",
				__func__, rc);
			goto escore_vs_write_bkg_keywords_exit;
		}
	} else if (voice_sense->vs_active_keywords) {
		rc = escore_datablock_write(escore, voice_sense->bkg->data,
				voice_sense->bkg->size);
		if ((rc < 0) || (rc < voice_sense->bkg->size)) {
			dev_err(escore->dev, "%s(): bkg write failed rc = %d\n",
							__func__, rc);
			goto escore_vs_write_bkg_keywords_exit;
		}

		for (i = 0; i < MAX_NO_OF_VS_KW; i++) {
			if (!(voice_sense->vs_active_keywords & (1 << i)))
				continue;
			dev_dbg(escore->dev, "%s(): Write kw = %d\n",
							__func__, i + 1);
			rc = escore_datablock_write(escore,
						voice_sense->kw[i]->data,
						voice_sense->kw[i]->size);
			if ((rc < 0) || (rc < voice_sense->kw[i]->size)) {
				dev_err(escore->dev,
					"%s(): kw %d write failed rc = %d\n",
					__func__, i + 1, rc);
				goto escore_vs_write_bkg_keywords_exit;
			}
		}
	} else {
		dev_err(escore->dev,
			"%s(): Invalid condition, shouldn't occur\n",
			__func__);
		rc = -EINVAL;
		goto escore_vs_write_bkg_keywords_exit;
	}

	escore_datablock_close(escore);

	return 0;

escore_vs_write_bkg_keywords_exit:
	escore_datablock_close(escore);
	return rc;
}

static int escore_vs_isr(struct notifier_block *self, unsigned long action,
		void *dev)
{
	struct escore_priv *escore = (struct escore_priv *)dev;
	char *def_event[] = { "ACTION=ADNC_KW_DETECT", NULL };
	char *hotword_event[] = { "ACTION=HOTWORD", NULL };
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *) escore->voice_sense;
	u32 es_set_power_level = ES_SET_POWER_LEVEL << 16 | ES_POWER_LEVEL_6;
	u32 resp;
	int rc = 0;

	dev_dbg(escore->dev, "%s(): Event: 0x%04x\n", __func__, (u32)action);

	if (!(action & ES_VS_INTR_EVENT)) {
		dev_dbg(escore->dev, "%s(): Invalid event callback 0x%04x\n",
				__func__, (u32) action);
		return NOTIFY_DONE;
	}
	dev_info(escore->dev, "%s(): VS event detected 0x%04x\n",
				__func__, (u32) action);

	if (voice_sense->cvs_preset != 0xFFFF && voice_sense->cvs_preset != 0) {
		if (escore->voice_recognition)
			/* In hotword feature device wakes
			 * up in VS Streaming mode */
			escore->escore_power_state =
				ES_POWER_STATE_VS_STREAMING;
		else {
			escore->escore_power_state =
				ES_SET_POWER_STATE_NORMAL;
			escore->mode = STANDARD;
		}
	}

	mutex_lock(&voice_sense->vs_event_mutex);
	voice_sense->vs_get_event = action;
	mutex_unlock(&voice_sense->vs_event_mutex);

	/* If CVS preset is set (other than 0xFFFF), earSmart chip is
	 * in CVS mode. To make it switch from internal to external
	 * oscillator, send power level command with highest power
	 * level
	 */
	if (voice_sense->cvs_preset != 0xFFFF &&
			voice_sense->cvs_preset != 0) {
		if (!escore->voice_recognition) {
			/* Following command will set the power level to 6,
			   any subsequent preset will switch the oscillator
			   to external */
			rc = escore_cmd_locked(escore,
						es_set_power_level,
						&resp);
			if (rc < 0) {
				pr_err("%s(): Error setting power level %d\n",
				       __func__, rc);
				goto voiceq_isr_exit;
			}
			usleep_range(2000, 2005);
		}
		/* Enable the clock before switching to external oscillator */
		if (escore->pdata->esxxx_clk_cb)
			escore->pdata->esxxx_clk_cb(1);

		/* Required only for the UART interface */
		if (escore->bus.ops.high_bw_calibrate) {
			mutex_lock(&escore->access_lock);
			rc = escore->bus.ops.high_bw_calibrate(escore);
			mutex_unlock(&escore->access_lock);
			if (rc) {
				dev_err(escore->dev,
					"%s() error calibrating the interface rc = %d\n",
					__func__, rc);
				goto voiceq_isr_exit;
			}
		}
		/* Each time earSmart chip comes in BOSKO mode after
		 * VS detect, CVS mode will be disabled */
		voice_sense->cvs_preset = 0;
	}

	if (escore->voice_recognition) {
		pr_debug("%s(): Hotword detected", __func__);
		kobject_uevent_env(&escore->dev->kobj, KOBJ_CHANGE,
				hotword_event);
	} else
		kobject_uevent_env(&escore->dev->kobj, KOBJ_CHANGE, def_event);

	return NOTIFY_OK;

voiceq_isr_exit:
	return NOTIFY_DONE;
}

static struct notifier_block escore_vs_intr_cb = {
	.notifier_call = escore_vs_isr,
	.priority = ES_NORMAL,
};

void escore_vs_init_intr(struct escore_priv *escore)
{
	escore_register_notify(escore_priv.irq_notifier_list,
			&escore_vs_intr_cb);
	((struct escore_voice_sense *)escore->voice_sense)->vs_irq = true;
}

int escore_vs_load(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	u32 cmd, resp;
	int rc = 0;

	BUG_ON(voice_sense->vs->size == 0);
	escore->mode = VOICESENSE_PENDING;

	/* Reset Mode to Polling */
	escore->cmd_compl_mode = ES_CMD_COMP_POLL;

	if (!escore->boot_ops.setup || !escore->boot_ops.finish) {
		dev_err(escore->dev,
			"%s(): boot setup or finish function undefined\n",
			__func__);
		rc = -EIO;
		goto escore_vs_uart_open_failed;
	}

	if (escore->bus.ops.high_bw_open) {
		rc = escore->bus.ops.high_bw_open(escore);
		if (rc) {
			dev_err(escore->dev, "%s(): high_bw_open failed %d\n",
				__func__, rc);
			goto escore_vs_uart_open_failed;
		}
	}

	rc = escore->boot_ops.setup(escore);
	if (rc) {
		dev_err(escore->dev, "%s(): fw download start error %d\n",
			__func__, rc);
		goto escore_vs_fw_download_failed;
	}

	rc = escore_is_sleep_aborted(escore);
	if (rc == -EABORT) {
		/* after boot setup, firmware download is initiated and can be
		 * aborted by sending abort keyword. This abort word is located
		 * in the firmware binary image, comprised of the 4 bytes
		 * starting at binary offset 0x000C. So host has to write
		 * at least 16 byte so that abort code is known to chip.
		 * Firmware abort sequence is mentioned in BUG #20899
		 */
		rc = escore->bus.ops.high_bw_write(escore,
				((char *)voice_sense->vs->data) , 20);
		if (rc) {
			dev_err(escore->dev,
				"%s(): vs firmware data write error %d\n",
				__func__, rc);
			rc = -EIO;
			goto escore_vs_fw_download_failed;
		}

		/* Write abort keyword */
		cmd = *(u32 *)(voice_sense->vs->data + 0x000c);
		rc = escore->bus.ops.high_bw_write(escore, &cmd , 4);
		if (rc) {
			dev_err(escore->dev,
				"%s(): abort word write error %d\n",
				__func__, rc);
			rc = -EIO;
			goto escore_vs_fw_download_failed;
		}
		/* BAS-3232: 5ms delay is required after abort command write */
		usleep_range(5000, 5050);

		if (escore->high_bw_intf == ES_UART_INTF ||
					escore->high_bw_intf == ES_SPI_INTF) {
			if (escore->boot_ops.escore_abort_config)
				escore->boot_ops.escore_abort_config(escore);
		}

		/* BAS-3232: Change power state to Normal, send command
		 * with SR bit disable on HIGH BW interface, but device won't
		 * send any response back */
		cmd = ES_SET_POWER_STATE_CMD << 16 | ES_SET_POWER_STATE_NORMAL;
		cmd = escore->bus.ops.cpu_to_high_bw_bus(escore, cmd);
		rc = escore->bus.ops.high_bw_write(escore, &cmd, sizeof(cmd));
		update_cmd_history(escore->bus.ops.high_bw_bus_to_cpu(escore,
								cmd), 0);
		if (rc) {
			pr_err("%s() HIGH BW INTF write is failed, rc = %d\n",
					__func__, rc);
			goto escore_vs_fw_download_failed;
		}

		rc = -EABORT;
		goto escore_sleep_aborted;
	}

	dev_dbg(escore->dev, "%s(): write vs firmware image\n", __func__);

	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(escore);

	rc = escore->bus.ops.high_bw_write(escore,
		((char *)voice_sense->vs->data) , voice_sense->vs->size);
	if (rc) {
		dev_err(escore->dev, "%s(): vs firmware image write error %d\n",
			__func__, rc);
		rc = -EIO;
		goto escore_vs_fw_download_failed;
	}

	escore->mode = VOICESENSE;

	if (((struct escore_voice_sense *)escore->voice_sense)->vs_irq != true)
		escore_vs_init_intr(escore);

	/* Wait for API Interrupt to confirm
	 * that firmware is ready to accept command */
	if (escore->pdata->gpioa_gpio != -1) {
		rc = escore_api_intr_wait_completion(escore);
		if (rc)
			goto escore_vs_fw_download_failed;
	} else {
		/* boot_ops.finish is required only in the case of POLL mode
		 * command completion*/
		rc = escore->boot_ops.finish(escore);
		if (rc) {
			dev_err(escore->dev,
				"%s() vs fw download finish error %d\n",
				__func__, rc);
			goto escore_vs_fw_download_failed;
		}
	}

	rc = escore_is_sleep_aborted(escore);
	if (rc == -EABORT)
		goto escore_sleep_aborted;

	/* Reconfig API Interrupt mode */
	rc = escore_reconfig_api_intr(escore);
	if (rc)
		goto escore_vs_fw_download_failed;

	/* Setup the Event response */
	cmd = (ES_SET_EVENT_RESP << 16) | escore->pdata->gpio_b_irq_type;
	rc = escore_cmd_nopm(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): Error %d in setting event response\n",
				__func__, rc);
		goto escore_vs_fw_download_failed;
	}

	dev_dbg(escore->dev, "%s(): fw download done\n", __func__);

escore_sleep_aborted:
escore_vs_fw_download_failed:
	if (escore->bus.ops.high_bw_close) {
		int ret = 0;
		ret = escore->bus.ops.high_bw_close(escore);
		if (ret) {
			dev_err(escore->dev, "%s(): high_bw_close failed %d\n",
				__func__, ret);
			rc = ret;
		}
	}
escore_vs_uart_open_failed:

	return rc;
}

int escore_vs_init_sysfs(struct escore_priv *escore)
{
	return sysfs_create_group(&escore->dev->kobj, &vscore_sysfs);
}

void escore_vs_exit(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	int i;
	for (i = 0; i < MAX_NO_OF_VS_KW; i++) {
		if (voice_sense->vs_active_keywords & BIT(i)) {
			dev_dbg(escore->dev, "%s(): release kw = %d\n",
							__func__, i + 1);
			release_firmware(voice_sense->kw[i]);
		}
	}
	kfree(voice_sense);
}

int escore_vs_init(struct escore_priv *escore)
{
	int rc = 0;

	struct escore_voice_sense *voice_sense;
	voice_sense = (struct escore_voice_sense *)
			kmalloc(sizeof(struct escore_voice_sense), GFP_KERNEL);
	if (!voice_sense) {
		rc = -ENOMEM;
		goto voice_sense_alloc_err;
	}

	escore->voice_sense = (void *)voice_sense;

	/* Initialize variables */
	voice_sense->cvs_preset = 0;
	voice_sense->vs_active_keywords = 0;
	voice_sense->es_vs_keyword_length = 0;

	mutex_init(&voice_sense->vs_event_mutex);

	rc = escore_vs_init_sysfs(escore);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
		goto sysfs_init_err;
	}

	escore->vs_ops.escore_is_voicesense_sleep_enable =
					escore_vs_sleep_enable;
	escore->vs_ops.escore_voicesense_sleep = escore_voicesense_sleep;
	escore->vs_ops.escore_voicesense_wakeup = escore_vs_wakeup;

	return rc;

sysfs_init_err:
	kfree(voice_sense);
voice_sense_alloc_err:
	return rc;
}
