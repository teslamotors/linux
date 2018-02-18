/*
 * es755.c  --  Audience eS755 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "es755.h"
#include "escore.h"
#include "escore-i2c.h"
#include "escore-i2s.h"
#include "escore-spi.h"
#include "escore-slim.h"
#include "escore-cdev.h"
#include "escore-uart.h"
#include "escore-vs.h"
#include "escore-uart-common.h"
#include "es755-access.h"
#include "es-d300.h"
#if defined(CONFIG_SND_SOC_ES854)
#include "es-a350-reg.h"
#elif defined(CONFIG_SND_SOC_ES857)
#include "es-a375-reg.h"
#else
#include "es-a300-reg.h"
#endif
#include "escore-version.h"
#include <linux/sort.h>
#ifdef CONFIG_QPNP_CLKDIV
#include <linux/qpnp/clkdiv.h>
#else
#include <linux/clk.h>
#endif

union es755_accdet_reg {
	u16 value;
	struct {
		u16 res1:1;
		u16 plug_det_fsm:1;
		u16 debounce_timer:2;
		u16 res2:1;
		u16 mic_det_fsm:1;
		u16 mg_sel_force:1;
		u16 mg_select:2;
	} fields;
};

static const u16 es755_vs_presets[] = {
	[ES755_DMIC0] = ES755_DMIC0_VS_ROUTE_PRESET,
	[ES755_MIC0]  = ES755_MIC0_VS_ROUTE_PRESET,
	[ES755_MICHS] = ES755_MICHS_VS_ROUTE_PRESET,
};

static const u16 es755_cvs_presets[] = {
	[ES755_DMIC0] = ES755_DMIC0_CVS_PRESET,
	[ES755_MIC0]  = ES755_MIC0_CVS_PRESET,
	[ES755_MICHS] = ES755_MICHS_CVS_PRESET,
};

/* codec private data TODO: move to runtime init */
struct escore_priv escore_priv = {
	.probe = es755_core_probe,
	.set_streaming = es755_set_streaming,
	.set_datalogging = es755_set_datalogging,
	.streamdev.no_more_bit = 0,
	.default_cvq_mic = ES755_DMIC0,
	.es_vs_route_preset = ES755_DMIC0_VS_ROUTE_PRESET,
	.es_cvs_preset = ES755_DMIC0_CVS_PRESET,
	.es_wdb_max_size = ES755_WDB_MAX_SIZE,
	.delay.wakeup_to_normal = ES755_DELAY_WAKEUP_TO_NORMAL,
	.delay.wakeup_to_vs = ES755_DELAY_WAKEUP_TO_VS,
	.delay.vs_to_normal = ES755_DELAY_VS_TO_NORMAL,
	.delay.mpsleep_to_normal = ES755_DELAY_MPSLEEP_TO_NORMAL,
	.delay.vs_streaming_to_vs = ES755_DELAY_VS_STREAMING_TO_VS,
};

struct snd_soc_dai_driver es755_dai[];

#ifdef CONFIG_ARCH_MSM8974
/*Slimbus channel map for APQ8074*/
static int es755_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
		152, 153, 154, 155, 134, 135
};
static int es755_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
		156, 157, 144, 145, 146, 147
};
#else
/*Slimbus channel map for APQ8060*/
static int es755_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};
static int es755_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};

#endif

#ifdef CONFIG_QPNP_CLKDIV
static struct q_clkdiv *codec_clk;
#else
static struct clk *codec_clk;
#endif

static struct escore_i2s_dai_data i2s_dai_data[ES_NUM_CODEC_I2S_DAIS];
static struct escore_slim_dai_data slim_dai_data[ES_NUM_CODEC_SLIM_DAIS];
static struct escore_slim_ch slim_rx[ES_SLIM_RX_PORTS];
static struct escore_slim_ch slim_tx[ES_SLIM_TX_PORTS];
static const u32 es755_streaming_cmds[] = {
	[ES_INVAL_INTF] = 0x00000000,	/* ES_NULL_INTF */
	[ES_SLIM_INTF]  = 0x90250200,	/* ES_SLIM_INTF */
	[ES_I2C_INTF]   = 0x90250000,	/* ES_I2C_INTF  */
	[ES_SPI_INTF]   = 0x90250300,	/* ES_SPI_INTF  */
	[ES_UART_INTF]  = 0x90250100,	/* ES_UART_INTF */
};

#ifdef CONFIG_ARCH_MSM
#if defined(CONFIG_SND_SOC_ES854)
const struct slim_device_id escore_slim_id[] = {
	{ "earSmart-codec", ESCORE_DEVICE_NONE }, /* for portability */
	{ "earSmart-codec-intf", ESCORE_INTERFACE_DEVICE },
	{ "earSmart-codec-gen0", ESCORE_GENERIC_DEVICE },
	{  }
};
#else
const struct slim_device_id escore_slim_id[] = {
	{ "earSmart-codec", ESCORE_DEVICE_NONE }, /* for portability */
	{ "eS755A0-codec-intf", ESCORE_INTERFACE_DEVICE },
	{ "eS755A0-codec-gen0", ESCORE_GENERIC_DEVICE },
	{ "eS755A1-codec-intf", ESCORE_INTERFACE_DEVICE },
	{ "eS755A1-codec-gen0", ESCORE_GENERIC_DEVICE },
	{ "eS755A2-codec-intf", ESCORE_INTERFACE_DEVICE },
	{ "eS755A2-codec-gen0", ESCORE_GENERIC_DEVICE },
	{  }
};
#endif
MODULE_DEVICE_TABLE(slim, escore_slim_id);
#endif

static int es755_clk_ctl(int enable)
{
	int ret = 0;
	bool clk_en;

	clk_en = enable ? true : false;

	if (clk_en == escore_priv.codec_clk_en)
		return 0;

	if (clk_en)
#ifdef CONFIG_QPNP_CLKDIV
		ret = qpnp_clkdiv_enable(codec_clk);
#else
		ret = clk_enable(codec_clk);
#endif
	else
#ifdef CONFIG_QPNP_CLKDIV
		ret = qpnp_clkdiv_disable(codec_clk);
#else
		clk_disable(codec_clk);
#endif

	if (-EINVAL != ret) {
		escore_priv.codec_clk_en = clk_en;
		ret = 0;
	}
	return ret;
}
static int es755_channel_dir(int dai_id)
{
	int dir = ES_SLIM_CH_UND;

	if (dai_id == ES_SLIM_1_PB ||
			dai_id == ES_SLIM_2_PB ||
			dai_id == ES_SLIM_3_PB) {
		dir = ES_SLIM_CH_RX;
	} else if (dai_id == ES_SLIM_1_CAP ||
			dai_id == ES_SLIM_2_CAP ||
			dai_id == ES_SLIM_3_CAP)  {
		dir = ES_SLIM_CH_TX;
	}

	return dir;
}

static int es755_get_route_status(struct escore_priv *escore, int algo_id)
{
	int ret = 0, value = 0;
	u32 api_word = 0;
	unsigned int msg_len;

	ret = escore_prepare_msg(escore, ES_CHANGE_STATUS, value,
			(char *)&api_word, &msg_len, ES_MSG_READ);
	if (ret) {
		pr_err("%s: Prepare message fail %d\n", __func__, ret);
		goto out;
	}

	/* Set the algo type in message */
	api_word |= algo_id;
	ret = escore_cmd_locked(escore, api_word, &value);
	if (ret < 0) {
		pr_err("%s(): escore_cmd() ret:%d", __func__, ret);
		goto out;
	}
	ret = (value & 0xFF);

out:
	return ret;
}

static ssize_t es755_route_status_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;
	int value = 0;
	char *status_name = "Route Status";
	struct escore_priv *escore = &escore_priv;
	const char *algo_text[ALGO_MAX] = {
		[VP] = "VP",
		[MM] = "MM",
		[AUDIOFOCUS] = "AudioFocus",
		[PASSTHRU] = "Passthru",
		[VP_MM] = "VP_MM",
		[PASSTHRU_VP] = "Passthru VP",
		[PASSTHRU_MM] = "Passthru MM",
		[PASSTHRU_VP_MM] = "Passthru VP MM",
		[PASSTHRU_AF] = "Passthru AF",
		[VOICEQ] = "VoiceQ",
		[KALA_OK] = "KalaOK",
	};

	const char *status_text[] = {
		"Active",
		"Muting",
		"Switching",
		"Unmuting",
		"Inactive",
	};

	const u8 algo_id[ALGO_MAX] = {
		[VP] = 0,
		[MM] = 1,
		[AUDIOFOCUS] = 2,
		[PASSTHRU] = 3,
		[VOICEQ] = 4,
		[KALA_OK] = 3, /* KalaOK is same as PASSTHRU */
	};

	if (escore->algo_type == ALGO_NONE) {
		pr_err("Algo type not set\n");
		ret = -EINVAL;
		goto out;
	}

	switch (escore->algo_type) {
	case VP:
	case MM:
	case AUDIOFOCUS:
	case PASSTHRU:
	case VOICEQ:
	case KALA_OK:
		value = es755_get_route_status(escore,
				algo_id[escore->algo_type]);
		if (value < 0) {
			pr_err("%s(): GetRouteChangeStatus Failed: %d\n",
					__func__, value);
			ret = value;
			goto out;
		}

		ret = snprintf(buf, PAGE_SIZE, "%s for %s Algo (%d) is %s\n",
				status_name, algo_text[escore->algo_type],
				escore->algo_type, status_text[value]);
		break;
	case PASSTHRU_VP:
	case BARGE_IN:
		{
		int ret1, ret2;
		value = es755_get_route_status(escore, algo_id[VP]);
		if (value < 0) {
			pr_err("%s(): GetRouteChangeStatus Failed: %d\n",
					__func__, value);
			ret = value;
			goto out;
		}

		ret1 = snprintf(buf, PAGE_SIZE, "%s for %s Algo (%d) is %s\n",
				status_name, algo_text[VP],
				VP, status_text[value]);

		value = es755_get_route_status(escore, algo_id[PASSTHRU]);
		if (value < 0) {
			pr_err("%s(): GetRouteChangeStatus Failed: %d\n",
					__func__, value);
			ret = value;
			goto out;
		}

		ret2 = snprintf(buf+ret1, PAGE_SIZE,
				"%s for %s Algo (%d) is %s\n",
				status_name, algo_text[PASSTHRU],
				PASSTHRU, status_text[value]);
		ret = ret1 + ret2;
		break;
		}
	}
out:
	return ret;
}

static DEVICE_ATTR(route_status, 0444, es755_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/route_status */

static ssize_t es755_get_pm_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", escore_priv.pm_status ?
			"on" : "off");
}
static ssize_t es755_set_pm_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	pr_info("%s(): requested - %s\n", __func__, buf);
	if (!strncmp(buf, "on", 2))
		escore_pm_status_update(ES_PM_ON);
	else if (!strncmp(buf, "off", 3))
		escore_pm_status_update(ES_PM_OFF);
	return count;

}
static DEVICE_ATTR(pm_enable, 0664, es755_get_pm_enable, es755_set_pm_enable);

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es755_read() instead of BUS ops */
static ssize_t es755_fw_version_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;
	struct escore_priv *escore = &escore_priv;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	mutex_lock(&escore->access_lock);
	value = escore_read(NULL, ES_FW_FIRST_CHAR);
	*verbuf++ = (value & 0x00ff);
	for (idx = 0; idx < (SIZE_OF_VERBUF-2); idx++) {
		value = escore_read(NULL, ES_FW_NEXT_CHAR);
		*verbuf++ = (value & 0x00ff);
		if (!value)
			break;
	}
	mutex_unlock(&escore->access_lock);
	/* Null terminate the string*/
	*verbuf = '\0';
	pr_debug("Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es755_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/fw_version */

static ssize_t es755_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es755_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/clock_on */

static ssize_t es755_reset_control_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	es_d300_reset_cmdcache();
	return 0;
}

static DEVICE_ATTR(reset_control, 0444, es755_reset_control_show, NULL);

static ssize_t es755_ping_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct escore_priv *es755 = &escore_priv;
	int rc = 0;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	char *status_name = "Ping";

	rc = escore_cmd_locked(es755, sync_cmd, &sync_ack);
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write %d\n",
		       __func__, rc);
		goto cmd_err;
	}
	pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE,
		       "%s=0x%08x\n",
		       status_name, sync_ack);
cmd_err:
	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es755_ping_status_show, NULL);

static ssize_t escore_version_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ESCORE_VERSION);
}
static DEVICE_ATTR(escore_version, 0444, escore_version_show, NULL);

static const char * const es755_power_state_texts[] = {
	"None", "Sleep", "MP_Sleep", "MP_Cmd", "Normal", "Overlay", "Low_Power",
	"Codec Deep Sleep"
};

static ssize_t es755_power_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct escore_priv *es755 = &escore_priv;

	dev_dbg(es755->dev, "%s(): power state = %s\n", __func__,
			es755_power_state_texts[es755->escore_power_state]);
	return snprintf(buf, PAGE_SIZE, "power state = %s\n",
			es755_power_state_texts[es755->escore_power_state]);
}

static DEVICE_ATTR(power_state, 0444, es755_power_state_show, NULL);


/* Display Data format of Codec Control Registers dump */
#define ES_CODEC_REG_DISP_FMT		"Reg = 0x%02X , Value = 0x%02X\n"

/* Store register addresses of 1 byte */
static u8 reg_addr[ES_MAX_CODEC_CONTROL_REG];
/* Store read count of registers to be displayed */
static u8 reg_read_cnt = ES_MAX_CODEC_CONTROL_REG;

/* Device Attribute to dump Codec Control Registers */
static ssize_t codec_reg_dump_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct escore_priv *es755 = &escore_priv;
	int rc = 0;
	int j = 0;
	u32 cmd, resp;
	u8 i, val;
	u8 addr;
	int reg_max = reg_read_cnt;
	u32 max_size_disp_op;

	/* Dump all the registers */
	if (reg_read_cnt == ES_MAX_CODEC_CONTROL_REG)
		reg_max += 1;

	pr_debug("%s(): reg_max = 0x%02x\n", __func__, reg_max);

	/* Error if Display Data exceeds PAGE_SIZE */
	max_size_disp_op  = reg_max * sizeof(ES_CODEC_REG_DISP_FMT);
	if (max_size_disp_op > PAGE_SIZE) {
		pr_err("%s(): Display Data exceeds PAGE_SIZE", __func__);
		return -EINVAL;
	}

	mutex_lock(&es755->access_lock);
	for (i = 0; i < reg_max; i++) {
		if (reg_read_cnt == ES_MAX_CODEC_CONTROL_REG)
			addr = i;
		else
			addr = reg_addr[i];

		cmd = (ES_GET_CODEC_VAL << 16) | (addr << 8);
		rc = escore_cmd(es755, cmd, &resp);
		if (rc < 0) {
			pr_err("%s(): escore_cmd() rc:%d", __func__, rc);
			goto cmd_err;
		}
		pr_debug("%s(): cmd = 0x%08x, resp = 0x%08x\n",
				__func__, cmd, resp);

		val = resp & ES_CODEC_VAL_MASK;

		j += snprintf(buf + j, PAGE_SIZE,
			       ES_CODEC_REG_DISP_FMT,
			       addr, val);
		rc = j;
	}

cmd_err:
	mutex_unlock(&es755->access_lock);
	/* Assign max value for next read operation */
	reg_read_cnt = ES_MAX_CODEC_CONTROL_REG;

	return rc;
}

/* Device Attribute to store address of Codec Control Registers into array */
static ssize_t codec_reg_dump_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int arg_read = 0;
	int pos = 0;
	int bytes_read = 0;
	int in_addr;

	pr_debug("%s(): requested - %s, count=%d\n", __func__, buf, count);

	/* Return Error for Invalid argument */
	if (strncmp(buf, "0x", 2) || !count) {
		pr_err("%s(): Invalid Argument\n", __func__);
		return -EINVAL;
	}

	/* Reset total for new operation */
	reg_read_cnt = 0;

	while ((bytes_read <= count) &&
			(reg_read_cnt <= ES_MAX_CODEC_CONTROL_REG)) {
		arg_read = sscanf(buf + bytes_read, "%x%n", &in_addr, &pos);
		bytes_read += pos;

		pr_debug("%s(): in_addr = %02x, arg_read=%d, bytes_read=%d\n",
				__func__, in_addr, arg_read, bytes_read);

		/* Skip Invalid characters and register address */
		if (!arg_read || (in_addr > ES_MAX_CODEC_CONTROL_REG))
			continue;

		reg_addr[reg_read_cnt] = in_addr;
		reg_read_cnt++;
	};

	return count;
}
static DEVICE_ATTR(codec_reg_dump, 0664, codec_reg_dump_show,
			codec_reg_dump_store);

static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_reset_control.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_ping_status.attr,
	&dev_attr_pm_enable.attr,
	&dev_attr_escore_version.attr,
	&dev_attr_power_state.attr,
	&dev_attr_codec_reg_dump.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

static int es755_fw_download(struct escore_priv *es755)
{
	int rc;

	pr_debug("%s()\n", __func__);

	/* Reset Mode to Polling */
	es755->cmd_compl_mode = ES_CMD_COMP_POLL;

	if (es755->bus.ops.high_bw_open) {
		rc = es755->bus.ops.high_bw_open(es755);
		if (rc) {
			dev_err(es755->dev, "%s(): high_bw_open failed %d\n",
			__func__, rc);
			goto es755_high_bw_open_failed;
		}
	}

	if (es755->boot_ops.setup) {
		pr_debug("%s(): calling bus specific boot setup\n", __func__);
		rc = es755->boot_ops.setup(es755);
		if (rc != 0) {
			pr_err("%s() bus specific boot setup error %d\n",
			       __func__, rc);
			goto es755_bootup_failed;
		}
	}
	es755->mode = SBL;

	/* Set flag to Wait for API Interrupt */
	escore_set_api_intr_wait(es755);

	rc = es755->bus.ops.high_bw_write(es755, (char *)es755->standard->data,
			      es755->standard->size);
	if (rc < 0) {
		pr_err("%s(): firmware download failed %d\n",
		       __func__, rc);
		rc = -EIO;
		goto es755_bootup_failed;
	}

	/* Wait for API Interrupt to confirm that firmware is
	 * ready to accept command */
	if (es755->pdata->gpioa_gpio != -1) {
		rc = escore_api_intr_wait_completion(es755);
		if (rc)
			goto es755_bootup_failed;
	} else if (es755->boot_ops.finish) {
		/* Give the chip some time to become ready after firmware
		 * download. */
		msleep(20);
		pr_debug("%s(): calling bus specific boot finish\n", __func__);
		rc = es755->boot_ops.finish(es755);
		if (rc != 0) {
			pr_err("%s() bus specific boot finish error %d\n",
			       __func__, rc);
			goto es755_bootup_failed;
		}
	}
	es755->mode = STANDARD;

	/* Reconfig API Interrupt mode */
	rc = escore_reconfig_api_intr(es755);
	if (rc)
		goto es755_bootup_failed;

es755_bootup_failed:
	if (es755->bus.ops.high_bw_close) {
		int ret = 0;
		ret = es755->bus.ops.high_bw_close(es755);
		if (ret) {
			dev_err(es755->dev, "%s(): high_bw_close failed %d\n",
				__func__, ret);
			rc = ret;
		}
	}

es755_high_bw_open_failed:
	return rc;
}

/* This function must be called with access_lock acquired */
int es755_bootup(struct escore_priv *es755)
{
	u8 retry = ES755_FW_DOWNLOAD_MAX_RETRY;
	int rc;

	pr_debug("%s()\n", __func__);

	BUG_ON(es755->standard->size == 0);

	do {
		if (retry < ES755_FW_DOWNLOAD_MAX_RETRY)
			escore_gpio_reset(es755);
		rc = es755_fw_download(es755);
	} while (rc && retry--);

	if (rc) {
		dev_err(es755->dev, "%s(): STANDARD fw download error %d\n",
								__func__, rc);
	}

	return rc;
}


static int es755_slim_set_channel_map(struct snd_soc_dai *dai,
				      unsigned int tx_num,
				      unsigned int *tx_slot,
				      unsigned int rx_num,
				      unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB ||
	    id == ES_SLIM_2_PB ||
	    id == ES_SLIM_3_PB) {
		escore->slim_dai_data[DAI_INDEX(id)].ch_tot = rx_num;
		escore->slim_dai_data[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			escore->slim_dai_data[DAI_INDEX(id)].ch_num[i] =
				rx_slot[i];
	} else if (id == ES_SLIM_1_CAP ||
		 id == ES_SLIM_2_CAP ||
		 id == ES_SLIM_3_CAP) {
		escore->slim_dai_data[DAI_INDEX(id)].ch_tot = tx_num;
		escore->slim_dai_data[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < tx_num; i++)
			escore->slim_dai_data[DAI_INDEX(id)].ch_num[i] =
				tx_slot[i];
	}

	return rc;
}

#if defined(CONFIG_ARCH_MSM)
static int es755_slim_get_channel_map(struct snd_soc_dai *dai,
				      unsigned int *tx_num,
				      unsigned int *tx_slot,
				      unsigned int *rx_num,
				      unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	struct escore_slim_ch *rx = escore->slim_rx;
	struct escore_slim_ch *tx = escore->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_1_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_2_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_3_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_1_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_1_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_2_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_3_CAP_OFFSET + i].ch_num;
	}

	return rc;
}
#endif

#if defined(CONFIG_SND_SOC_ES_I2S)
static void es755_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	int id = DAI_INDEX(dai->id);

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		escore->i2s_dai_data[id].rx_ch_tot = 0;
	else
		escore->i2s_dai_data[id].tx_ch_tot = 0;

	if (escore->can_mpsleep)
		escore->can_mpsleep = 0;
}

static int es755_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int id = DAI_INDEX(dai->id);

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d, fmt = %x\n",
		__func__, dai->name, dai->id, fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		/* es755 as master */
		i2s_dai_data[id].port_mode = ES_PCM_PORT_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		/* es755 as slave */
		i2s_dai_data[id].port_mode = ES_PCM_PORT_SLAVE;
		break;
	default:
		dev_err(codec->dev, "%s(): unsupported DAI clk mode\n",
			__func__);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s(): clk mode = %d\n", __func__,
			i2s_dai_data[id].port_mode);

	return 0;
}

static int es755_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct escore_api_access *api_access;
	int rc = 0;
	int channels;
	int bps = 0;
	int rate = 0;
	int id = DAI_INDEX(dai->id);
	u16 clock_control = 0;
	u16 codec_output_rate = 0;
	int port_map = 0;
	u8 pcm_port[] = { ES755_PCM_PORT_A,
		ES755_PCM_PORT_B,
		ES755_PCM_PORT_C };

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	mutex_lock(&escore->access_lock);
	rc =  es_change_power_state_normal(escore);
	mutex_unlock(&escore->access_lock);

	if (rc) {
		dev_err(codec->dev, "%s(): Power state transition failed %d\n",
				__func__, rc);
		return rc;
	}

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			escore->i2s_dai_data[id].rx_ch_tot = channels;
			escore->i2s_dai_data[id].rx_ch_act = 0;
		} else {
			escore->i2s_dai_data[id].tx_ch_tot = channels;
			escore->i2s_dai_data[id].tx_ch_act = 0;
		}
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported number of channels, %d\n",
			__func__, channels);
		return -EINVAL;
	}


	escore->pcm_format = params_format(params);
	/* ######## HACK : Set PCM format as 32bit ####### */
	if (escore->algo_type == DHWPT || escore->dhwpt_enabled)
		escore->pcm_format = SNDRV_PCM_FORMAT_S32_LE;

	switch (escore->pcm_format) {
	case SNDRV_PCM_FORMAT_A_LAW:
		bps = 0x0207;
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		bps = 0x0107;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		bps = 0xF;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		bps = 0x13;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		bps = 0x17;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
		bps = 0x1F;
		break;
	default:
		dev_err(codec->dev, "%s(): Unsupported format :%d\n",
				__func__, params_format(params));
		break;
	}

	if (bps) {
		api_access = &escore->api_access[ES_PORT_WORD_LEN];

		/* Update the Port info in write command */
		api_access->write_msg[0] |=  ES_API_WORD(ES_SET_DEV_PARAM_ID,
				(pcm_port[id] << 8));

		rc = escore_write_locked(codec, ES_PORT_WORD_LEN, bps);
		if (rc) {
			pr_err("%s(): Preparing write message failed %d\n",
			       __func__, rc);
			return rc;
		}
		/* Clear the Port info in write command */
		api_access->write_msg[0] &= ES_API_WORD(ES_SET_DEV_PARAM_ID,
				0x00ff);
	}

	switch (params_rate(params)) {
	case 8000:
		rate = 8;
		break;
	case 11025:
		rate = 11;
		break;
	case 12000:
		rate = 12;
		break;
	case 16000:
		rate = 16;
		break;
	case 22050:
		rate = 22;
		break;
	case 24000:
		rate = 24;
		break;
	case 32000:
		rate = 32;
		break;
	case 44100:
		rate = 44;
		break;
	case 48000:
		rate = 48;
		break;
	case 96000:
		rate = 96;
		break;
	case 192000:
		rate = 192;
		break;
	default:
		pr_err("%s: Unsupported sampling rate %d\n", __func__,
				params_rate(params));
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s(): params_rate(params) = %d\n",
			__func__, params_rate(params));

	api_access = &escore->api_access[ES_PORT_CLOCK_CONTROL];

	/* Update the Port info in write command */
	api_access->write_msg[0] |=  ES_API_WORD(ES_SET_DEV_PARAM_ID,
			(pcm_port[id] << 8));

	clock_control  = rate | (i2s_dai_data[id].port_mode << 8);

	rc = escore_write_locked(codec, ES_PORT_CLOCK_CONTROL, clock_control);
	if (rc) {
		pr_err("%s(): Preparing write message failed %d\n",
		       __func__, rc);
		return rc;
	}

	/* Clear the Port info in write command */
	api_access->write_msg[0] &= ES_API_WORD(ES_SET_DEV_PARAM_ID, 0x00ff);

	/* Set codec output rate  */
	if (escore->dhwpt_enabled || escore->algo_type == DHWPT) {
		switch (params_rate(params)) {
		case 44100:
			codec_output_rate = 0x7;
			break;
		case 48000:
			codec_output_rate = 0x6;
			break;
		case 96000:
		case 192000:
			codec_output_rate = 0x2;
			break;
		default:
			pr_err("%s: Unsupported sampling rate %d for DHWPT\n",
				__func__, params_rate(params));
			return -EINVAL;
		}
	}
	rc = escore_write_locked(codec, ES_CODEC_OUTPUT_RATE,
						codec_output_rate);
	if (rc) {
		pr_err("%s(): Preparing write message failed %d\n",
				__func__, rc);
		return rc;
	}

	/*
	 * To enter into MP_SLEEP mode during playback, a minimum 3Mhz clock
	 * is required. For that, minimum 48Khz sample rate, 32 bit word length
	 * and 2 channels are required.
	 */
	escore->can_mpsleep = (rate >= 48) && (bps >= 0x1F) && (channels >= 2);

	dev_dbg(codec->dev, "%s(): # Rate %d BPS %d channels %d #\n", __func__,
		rate, bps, channels);

	switch (dai->id) {
	case ES_I2S_PORTA:
		port_map = PORT_A_TO_D;
		break;
	case ES_I2S_PORTB:
		port_map = PORT_B_TO_D;
		break;
	case ES_I2S_PORTC:
		port_map = PORT_C_TO_D;
		break;
	}

	if (escore->algo_type == DHWPT || escore->dhwpt_enabled)
		escore->dhwpt_cmd = (ES_DHWPT_CMD << 16) | port_map;

	/* BAS-3205, if frame size is 64 bits and word length is 16 bits, set
	 * TDM Timeslots Per Frame of PortA to 3  */
	if (escore->algo_type != DHWPT && !escore->dhwpt_enabled) {
		if (32 == (bps + 1)*channels && 0xF == bps) {
			api_access =
			     &escore->api_access[ES_PORT_TDM_SLOTS_PER_FRAME];
			api_access->write_msg[0] |=
				ES_API_WORD(ES_SET_DEV_PARAM_ID,
				(ES755_PCM_PORT_A << 8));

			rc = escore_write_locked(escore,
					ES_PORT_TDM_SLOTS_PER_FRAME,
					3);
			if (rc) {
				pr_err("%s(): Set TDM slots per frame fail %d\n",
						__func__, rc);
				return rc;
			}

			api_access->write_msg[0] &=
				ES_API_WORD(ES_SET_DEV_PARAM_ID, 0x00ff);
		}
	}

	dev_dbg(codec->dev, "%s(): params_channels(params) = %d\n", __func__,
		channels);

	return rc;
}
#endif

struct snd_soc_dai_driver es755_dai[] = {
#if defined(CONFIG_SND_SOC_ES_I2S)
	{
		.name = "earSmart-porta",
		.id = ES_I2S_PORTA,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
	{
		.name = "earSmart-portb",
		.id = ES_I2S_PORTB,
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
	{
		.name = "earSmart-portc",
		.id = ES_I2S_PORTC,
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM)
	{
		.name = "es755-slim-rx1",
		.id = ES_SLIM_1_PB,
		.playback = {
			.stream_name = "SLIM_PORT-1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es755-slim-tx1",
		.id = ES_SLIM_1_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es755-slim-rx2",
		.id = ES_SLIM_2_PB,
		.playback = {
			.stream_name = "SLIM_PORT-2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es755-slim-tx2",
		.id = ES_SLIM_2_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es755-slim-rx3",
		.id = ES_SLIM_3_PB,
		.playback = {
			.stream_name = "SLIM_PORT-3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es755-slim-tx3",
		.id = ES_SLIM_3_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	}
#endif
};

#if defined(CONFIG_SND_SOC_ES_VS)
static const char * const es755_vs_event_texts[] = {
	"No Event", "Codec Event", "VS Keyword Event",
};
static const struct soc_enum es755_vs_event_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_EVENT, 0,
			ARRAY_SIZE(es755_vs_event_texts),
			es755_vs_event_texts);

static const char * const es755_vs_training_mode_texts[] = {
	"Detect Keyword", "N/A", "Train User-defined Keyword",
};

static const struct soc_enum es755_vs_training_mode_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_MODE, 0,
			ARRAY_SIZE(es755_vs_training_mode_texts),
			es755_vs_training_mode_texts);

static const char * const es755_vs_training_status_texts[] = {
	"busy", "Success", "Utterance Long", "Utterance Short",
	"Verification Failed", "Failed Bad length", "Noise", "Level too low",
};
static const struct soc_enum es755_vs_training_status_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_STATUS, 0,
			ARRAY_SIZE(es755_vs_training_status_texts),
			es755_vs_training_status_texts);

static const char * const es755_vs_training_record_texts[] = {
	"Start_0", "Start_1", "Start_2",
};
static const struct soc_enum es755_vs_training_record_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_RECORD, 0,
			ARRAY_SIZE(es755_vs_training_record_texts),
			es755_vs_training_record_texts);

static const char * const es755_vs_stored_keyword_texts[] = {
	"Put", "Get", "Clear"
};
static const struct soc_enum es755_vs_stored_keyword_enum =
	SOC_ENUM_SINGLE(ES_VS_STORED_KEYWORD, 0,
			ARRAY_SIZE(es755_vs_stored_keyword_texts),
			es755_vs_stored_keyword_texts);

static const char * const es755_default_cvq_mic_texts[] = {
	"MIC0", "DMIC0",
};

static const struct soc_enum es755_default_cvq_mic_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es755_default_cvq_mic_texts),
			es755_default_cvq_mic_texts);

static const char * const es755_voice_recognition_texts[] = {
	"Stop", "Start",
};

static const struct soc_enum es755_voice_recognition_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es755_voice_recognition_texts),
			es755_voice_recognition_texts);

/* Use for NULL "get" handler */
static int es755_get_null_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Do Nothing */
	return 0;
}

static int es755_get_power_saving_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = escore->power_saving_mode;

	return 0;
}

static int es755_set_power_saving_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&codec->mutex);
	/* Value 0 is used to disable Power Level support */
	escore->power_saving_mode = ucontrol->value.enumerated.item[0];
	mutex_unlock(&codec->mutex);

	return 0;
}

static int es755_get_default_cvq_mic_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	ucontrol->value.enumerated.item[0] = escore->default_cvq_mic;

	return 0;
}

static int es755_put_default_cvq_mic_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	escore->default_cvq_mic = ucontrol->value.enumerated.item[0];
	escore->es_vs_route_preset = es755_vs_presets[escore->default_cvq_mic];
	escore->es_cvs_preset = es755_cvs_presets[escore->default_cvq_mic];

	return 0;
}

static struct snd_kcontrol_new es755_voice_sense_snd_controls[] = {
	SOC_ENUM_EXT("Voice Sense Status",
		     es755_vs_event_enum,
		     escore_vs_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Mode",
			 es755_vs_training_mode_enum,
			 escore_vs_get_control_enum,
			 escore_vs_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Training Status",
		     es755_vs_training_status_enum,
		     escore_vs_get_control_enum, NULL),
	SOC_SINGLE_EXT("Voice Sense Training Model Length",
			ES_VOICE_SENSE_TRAINING_MODEL_LENGTH, 0, 75, 0,
			escore_vs_get_control_value,
			NULL),
	SOC_ENUM_EXT("Voice Sense Training Record",
		     es755_vs_training_record_enum,
		     es755_get_null_control_enum, escore_vs_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Stored Keyword",
		     es755_vs_stored_keyword_enum,
		     es755_get_null_control_enum, escore_vs_put_control_enum),
	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
			ES_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
			escore_vs_get_control_value,
			escore_vs_put_control_value),
	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
			ES_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
			escore_vs_get_control_value,
			escore_vs_put_control_value),
	SOC_SINGLE_EXT("Continuous Voice Sense Preset",
		       ES_CVS_PRESET, 0, 65535, 0,
		       escore_get_cvs_preset_value,
		       escore_put_cvs_preset_value),
	SOC_SINGLE_EXT("Enable/Disable Streaming PATH/Endpoint",
		       ES_FE_STREAMING, 0, 65535, 0,
		       es755_get_null_control_enum,
		       escore_put_control_value),
	SOC_SINGLE_EXT("CVQ Activate Keywords",
		       SND_SOC_NOPM, 0, 31, 0,
		       escore_get_vs_activate_keyword,
		       escore_put_vs_activate_keyword),
	SOC_SINGLE_EXT("CVQ Sleep",
		       SND_SOC_NOPM, 0, 1, 0,
		       escore_get_vs_sleep,
		       escore_put_vs_sleep),
	SOC_SINGLE_EXT("VS keyword length",
		       SND_SOC_NOPM, 0, 65535, 0,
		       escore_get_vs_keyword_length,
		       escore_put_vs_keyword_length),
	SOC_SINGLE_EXT("KW Overrun Error",
		       0, 0, 65535, 0, escore_get_keyword_overrun,
		       NULL),
	SOC_ENUM_EXT("CVQ Default MIC", es755_default_cvq_mic_enum,
			es755_get_default_cvq_mic_enum,
			es755_put_default_cvq_mic_enum),
	SOC_SINGLE_EXT("Power Saving Level",
			SND_SOC_NOPM, 0, 6, 0,
			es755_get_power_saving_mode,
			es755_set_power_saving_mode),
	SOC_SINGLE_EXT("Audio Focus Enable", SND_SOC_NOPM, 0, 1, 0,
			escore_get_af_status, escore_put_af_status),
	SOC_ENUM_EXT("Voice Recognition", es755_voice_recognition_enum,
			escore_get_voice_recognition_enum,
			escore_put_voice_recognition_enum),
};

/* This function must be called with access_lock acquired */
int es755_start_int_osc(void)
{
	int rc = 0;
	int retry = MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE;

	dev_info(escore_priv.dev, "%s()\n", __func__);

	/* Start internal Osc. */
	rc = escore_write(NULL, ES_VS_INT_OSC_MEASURE_START, 0);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): OSC Measure Start fail %d\n",
			__func__, rc);
		return rc;
	}

	/* Poll internal Osc. status */
	do {
		/*
		 * Wait 20ms each time before reading
		 * up to 100ms
		 */
		msleep(20);
		rc = escore_read(NULL, ES_VS_INT_OSC_MEASURE_STATUS);

		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): OSC Measure Read Status fail %d\n",
				__func__, rc);
			break;
		}
		dev_dbg(escore_priv.dev,
			"%s(): OSC Measure Status = 0x%04x\n",
			__func__, rc);
	} while (rc && --retry);

	if (rc > 0) {
		dev_err(escore_priv.dev,
			"%s(): Unexpected OSC Measure Status = 0x%04x\n",
			__func__, rc);
		dev_err(escore_priv.dev,
			"%s(): Can't switch to Low Power Mode\n",
			__func__);
	}

	return rc;
}

int es755_wakeup(void)
{
	int rc = 0;

	dev_dbg(escore_priv.dev, "%s()\n", __func__);

	rc = escore_wakeup(&escore_priv);

	return rc;
}

/* Power state transition:
 * This function must be called with access_lock acquired */
int es755_power_transition(int next_power_state,
				unsigned int set_power_state_cmd)
{
	int rc = 0;

	dev_info(escore_priv.dev, "%s()\n", __func__);

	/* Power state transition */
	while (next_power_state != escore_priv.escore_power_state) {
		switch (escore_priv.escore_power_state) {
		case ES_SET_POWER_STATE_SLEEP:
		case ES_SET_POWER_STATE_DEEP_SLEEP:
		case ES_SET_POWER_STATE_MP_SLEEP:
			/* Wakeup Chip */
			rc = es755_wakeup();
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): es755 wakeup failed %d\n",
					__func__, rc);
				goto es755_power_transition_exit;
			}
			escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;
			break;

		case ES_SET_POWER_STATE_NORMAL:
			/* Either switch to Sleep or VS Overlay mode */
			switch (next_power_state) {
			case ES_SET_POWER_STATE_MP_SLEEP:
				escore_priv.escore_power_state =
					next_power_state;
				break;
			case ES_SET_POWER_STATE_VS_OVERLAY:
			case ES_SET_POWER_STATE_VS_LOWPWR:
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_OVERLAY;
				break;
			default:
				escore_priv.escore_power_state =
					next_power_state;
				break;
			}
			/* Set flag to Wait for API Interrupt */
			escore_set_api_intr_wait(&escore_priv);

			rc = escore_write(NULL, set_power_state_cmd,
					escore_priv.escore_power_state);
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): Power state cmd write fail %d\n",
					__func__, rc);
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;
				goto es755_power_transition_exit;
			}

			/* Wait for API Interrupt to confirm
			 * the power transition */
			if (escore_priv.pdata->gpioa_gpio != -1) {
				rc =
				escore_api_intr_wait_completion(&escore_priv);
				if (rc)
					goto es755_power_transition_exit;
			}

			/* VS fw download */
			if (escore_priv.escore_power_state ==
				ES_SET_POWER_STATE_VS_OVERLAY) {
				/* wait es755 SBL mode for polling method */
				if (escore_priv.pdata->gpioa_gpio == -1)
					msleep(50);
				rc = escore_vs_load(&escore_priv);
				if (rc) {
					dev_err(escore_priv.dev,
						"%s(): vs fw download fail %d\n",
						__func__, rc);
					goto es755_power_transition_exit;
				}
			}

			if (escore_priv.escore_power_state ==
				ES_SET_POWER_STATE_MP_SLEEP) {
				/* Diable reference clock in MP_Sleep */
				if (escore_priv.pdata->esxxx_clk_cb)
					escore_priv.pdata->esxxx_clk_cb(0);
			}

			break;

		case ES_SET_POWER_STATE_VS_OVERLAY:
			/* Either switch to VS low power or Normal mode */
			if (next_power_state == ES_SET_POWER_STATE_VS_LOWPWR) {
				/* Start internal oscilator */
				rc = es755_start_int_osc();
				if (rc)
					goto es755_power_transition_exit;

				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_LOWPWR;

			} else {
				/* Set flag to Wait for API Interrupt */
				escore_set_api_intr_wait(&escore_priv);

				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;
				escore_priv.mode = STANDARD;
			}

			rc = escore_write(NULL, set_power_state_cmd,
					escore_priv.escore_power_state);
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): Power state cmd write fail %d\n",
					__func__, rc);
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_OVERLAY;
				goto es755_power_transition_exit;
			}

			if (escore_priv.escore_power_state ==
					ES_SET_POWER_STATE_VS_LOWPWR) {
				/* Disable the clocks */
				if (escore_priv.pdata->esxxx_clk_cb)
					escore_priv.pdata->esxxx_clk_cb(0);
				if (escore_priv.pdata->gpioa_gpio != -1)
					escore_priv.cmd_compl_mode =
							ES_CMD_COMP_POLL;
			} else if ((escore_priv.pdata->gpioa_gpio != -1) &&
				(escore_priv.escore_power_state ==
				ES_SET_POWER_STATE_NORMAL)) {
				/* Wait for API Interrupt to confirm
				 * the power transition */
				rc =
				escore_api_intr_wait_completion(&escore_priv);
				if (rc)
					goto es755_power_transition_exit;
				/* Reconfig API Interrupt mode */
				rc = escore_reconfig_api_intr(&escore_priv);
				if (rc)
					goto es755_power_transition_exit;
			} else if (escore_priv.escore_power_state ==
				ES_SET_POWER_STATE_NORMAL) {
				/* Add delay before power transition */
				msleep(escore_priv.delay.vs_to_normal);
			}
			break;

		case ES_SET_POWER_STATE_VS_LOWPWR:
			/* Wakeup Chip */
			rc = es755_wakeup();
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): es755 wakeup fail %d\n",
					__func__, rc);
				goto es755_power_transition_exit;
			}
			escore_priv.escore_power_state =
				ES_SET_POWER_STATE_VS_OVERLAY;
			break;
		default:
			dev_err(escore_priv.dev,
				"%s(): Unsupported state in es755\n",
				__func__);
			rc = -EINVAL;
			goto es755_power_transition_exit;
		}
		dev_dbg(escore_priv.dev,
			"%s(): Current state = %d, val=%d\n",
			__func__, escore_priv.escore_power_state,
			next_power_state);
	}

	dev_dbg(escore_priv.dev, "%s(): Power state change successful\n",
		__func__);
es755_power_transition_exit:
	return rc;
}

static int es755_get_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = escore_priv.escore_power_state;

	return 0;
}

static int es755_put_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;
	struct escore_priv *escore = &escore_priv;

	value = ucontrol->value.enumerated.item[0];

	/* Not supported */
	if (!value) {
		dev_err(escore_priv.dev, "%s(): Unsupported state\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc < 0) {
		dev_err(escore_priv.dev, "%s(): pm_get_sync failed :%d\n",
								__func__, rc);
		goto exit;
	}

	if (ES_SET_POWER_STATE_MP_SLEEP == value) {
		/* If already in DHWPT mode, directly switch to MP_Sleep,
		 * otherwise buffer the MP_Sleep switching to
		 * _es_set_final_route() */

		escore->mp_sleep_requested = 1;

		if (escore->mp_sleep_requested && escore->dhwpt_mode &&
					!escore->dhwpt_capture_on) {
			rc = es755_enter_mp_sleep();
			if (rc) {
				dev_dbg(escore_priv.dev,
					"%s() es755_enter_mp_sleep fail :%d\n",
					__func__, rc);
			}
		} else {
			dev_dbg(escore_priv.dev, "%s(): %s, %s\n", __func__,
				"Seem capture is present or no playback started yet",
				"Skip putting in MP_sleep");
		}
	} else {

		escore->mp_sleep_requested = 0;
		rc = cancel_delayed_work_sync(&escore->mp_sleep_work);
		dev_dbg(escore_priv.dev,
			"%s() cancel_delayed_work_sync() rc = %d\n",
			__func__, rc);

		rc = es755_power_transition(value, reg);
		if (rc) {
			dev_err(escore_priv.dev,
				"%s(): es755_power_transition() fail %d\n",
				__func__, rc);
		}
	}

	escore_pm_put_autosuspend();
exit:
	mutex_unlock(&escore->access_lock);

	return rc;
}

static int es755_put_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = escore_write_locked(NULL, reg, value);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set Preset fail %d\n",
			__func__, rc);
		return rc;
	}

	escore_priv.preset = value;

	return rc;
}

static int es755_get_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.preset;

	return 0;
}

static int es755_put_factory_intr_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	unsigned int value;
	int rc = 0;
	u32 cmd;
	u32 resp;
	value = ucontrol->value.integer.value[0];

	rc = escore_pm_get_sync();
	if (rc > -1) {
		cmd = (ES_GET_FACTORY_INT << 16) | (value & 0xFFFF);
		rc = escore_cmd_nopm(escore, cmd, &resp);
		if (rc < 0) {
			pr_err("%s(): Error %d in trigger dummy interrupt\n",
				__func__, rc);
			escore_pm_put_autosuspend();
			goto out;
		}
	}

out:
	return rc;
}

static int es755_get_factory_intr_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es755_get_rdb_size(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] =
				escore_priv.datablock_dev.rdb_read_count;

	return 0;
}

static int es755_get_event_status(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&escore_priv.escore_event_type_mutex);

	ucontrol->value.enumerated.item[0] = escore_priv.escore_event_type;

	/* Reset the event status after read */
	escore_priv.escore_event_type = ES_NO_EVENT;

	mutex_unlock(&escore_priv.escore_event_type_mutex);

	return 0;
}

static int es755_put_hs_delay_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	escore_priv.hs_delay = ucontrol->value.integer.value[0];

	if (escore_priv.hs_delay < MIN_HS_REDETECTION_DELAY)
		escore_priv.hs_delay = MIN_HS_REDETECTION_DELAY;

	return rc;
}

static int es755_get_hs_delay_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.hs_delay;

	return 0;
}

static int es755_get_select_endpoint(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	ucontrol->value.integer.value[0] = escore->selected_endpoint;
	return 0;
}

static int es755_put_select_endpoint(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	u32 value = ucontrol->value.enumerated.item[0];

	pr_debug("%s: Select path ID %d.\n", __func__, value);

	/* Only support path ID, endpoint ID is not supported */
	if (value > ES300_PASSOUT4) {
		dev_err(escore_priv.dev, "%s(): Incorrect Path ID.\n",
			__func__);
		return -EINVAL;
	}

	escore->selected_endpoint = value;

	return 0;
}

static int es755_get_digital_gain(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;

	ucontrol->value.integer.value[0] = escore->digital_gain[0];
	return 0;
}

static int es755_put_digital_gain(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *escore = &escore_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val   = e->values[ucontrol->value.enumerated.item[0]];
	unsigned int mask  = e->mask;

	pr_debug("%s: Endpoint %d Set digital gain 0x%x.\n", __func__,
		escore->selected_endpoint, val);

	if ( ucontrol->value.enumerated.item[0] > (e->items - 1) ) {
		dev_err(escore_priv.dev, "%s(): Enum exceed the maximum!\n",
			__func__);
		return -EINVAL;
	}

	if (escore->selected_endpoint < ES300_PRI ||
	    escore->selected_endpoint > ES300_PASSOUT4) {
		dev_err(escore_priv.dev, "%s(): Invalid selected endpoint/pathID!\n",
			__func__);
		return -EINVAL;
	}

	escore->digital_gain[escore->selected_endpoint] = (u32)val&mask;
	escore->selected_endpoint = -1;

	return 0;
}


static const char * const es755_path_id_texts[] = {
	[ES300_PRI] = "PRI", "SEC", "TER", "FEIN", "AECREF", "AUDIN1",
	"AUDIN2", "RESERVED", "RESERVED",
	[ES300_UITONE1] = "UITONE1", "UITONE2", "UITONE3", "UITONE4", "PASSIN1",
	"PASSIN2", "PASSIN3", "PASSIN4", "FEIN2", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED",
	[ES300_CSOUT1] = "CSOUT1", "CSOUT2", "FEOUT1", "FEOUT2", "AUDOUT1",
	"AUDOUT2", "RESERVED", "RESERVED",
	[ES300_MONOUT1] = "MONOUT1", "MONOUT2", "MONOUT3", "MONOUT4",
	"PASSOUT1", "PASSOUT2", "PASSOUT3", "PASSOUT4",
};

static const struct soc_enum es755_path_id_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es755_path_id_texts),
			es755_path_id_texts);

static const char * const digital_gain_text[] = {
	"-90dB", "-89dB", "-88dB", "-87dB", "-86dB",
	"-85dB", "-84dB", "-83dB", "-82dB", "-81dB",
	"-80dB", "-79dB", "-78dB", "-77dB", "-76dB",
	"-75dB", "-74dB", "-73dB", "-72dB", "-71dB",
	"-70dB", "-69dB", "-68dB", "-67dB", "-66dB",
	"-65dB", "-64dB", "-63dB", "-62dB", "-61dB",
	"-60dB", "-59dB", "-58dB", "-57dB", "-56dB",
	"-55dB", "-54dB", "-53dB", "-52dB", "-51dB",
	"-50dB", "-49dB", "-48dB", "-47dB", "-46dB",
	"-45dB", "-44dB", "-43dB", "-42dB", "-41dB",
	"-40dB", "-39dB", "-38dB", "-37dB", "-36dB",
	"-35dB", "-34dB", "-33dB", "-32dB", "-31dB",
	"-30dB", "-29dB", "-28dB", "-27dB", "-26dB",
	"-25dB", "-24dB", "-23dB", "-22dB", "-21dB",
	"-20dB", "-19dB", "-18dB", "-17dB", "-16dB",
	"-15dB", "-14dB", "-13dB", "-12dB", "-11dB",
	"-10dB", "-9dB", "-8dB", "-7dB", "-6dB",
	"-5dB", "-4dB", "-3dB", "-2dB", "-1dB",
	"0dB", "1dB", "2dB", "3dB", "4dB",
	"5dB", "6dB", "7dB", "8dB", "9dB",
	"10dB", "11dB", "12dB", "13dB", "14dB",
	"15dB", "16dB", "17dB", "18dB", "19dB",
	"20dB", "21dB", "22dB", "23dB", "24dB",
	"25dB", "26dB", "27dB", "28dB", "29dB",
	"30dB",
};

static const unsigned int digital_gain_value[] = {
	0xA6, 0xA7, 0xA8, 0xA9, 0xAA,
	0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
	0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
	0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
	0xBF, 0xC0, 0xC1, 0xC2, 0xC3,
	0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
	0xC9, 0xCA, 0xCB, 0xCC, 0xCD,
	0xCE, 0xCF, 0xD0, 0xD1, 0xD2,
	0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
	0xD8, 0xD9, 0xDA, 0xDB, 0xDC,
	0xDD, 0xDE, 0xDF, 0xE0, 0xE1,
	0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
	0xE7, 0xE8, 0xE9, 0xEA, 0xEB,
	0xEC, 0xED, 0xEE, 0xEF, 0xF0,
	0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
	0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
	0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D,
	0x1E,
};

static const struct soc_enum digital_gain_enum =
	SOC_VALUE_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ES_DIGITAL_GAIN_MASK, ARRAY_SIZE(digital_gain_text),
		digital_gain_text,
		digital_gain_value);

static const struct soc_enum es755_power_state_enum =
	SOC_ENUM_SINGLE(ES_POWER_STATE, 0,
			ARRAY_SIZE(es755_power_state_texts),
			es755_power_state_texts);

static const char * const es755_runtime_pm_texts[] = {
	"Disable", "Enable"
};

static const struct soc_enum es755_runtime_pm_enum =
	SOC_ENUM_SINGLE(ES_RUNTIME_PM, 0,
			ARRAY_SIZE(es755_runtime_pm_texts),
			es755_runtime_pm_texts);

static const char * const es755_streaming_mode_texts[] = {
	"CVQ", "Non-CVQ",
};
static const struct soc_enum es755_streaming_mode_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es755_streaming_mode_texts),
			es755_streaming_mode_texts);

static struct snd_kcontrol_new es755_snd_controls[] = {
	SOC_ENUM_EXT("ES755 Power State", es755_power_state_enum,
		     es755_get_power_control_enum,
		     es755_put_power_control_enum),
	SOC_ENUM_EXT("Runtime PM", es755_runtime_pm_enum,
		     escore_get_runtime_pm_enum,
		     escore_put_runtime_pm_enum),
	SOC_SINGLE_EXT("Preset",
		       ES_PRESET, 0, 65535, 0, es755_get_preset_value,
		       es755_put_preset_value),
	SOC_SINGLE_EXT("Trigger factory interrupt",
		       SND_SOC_NOPM, 0, 65535, 0,
		       es755_get_factory_intr_value,
		       es755_put_factory_intr_value),
	SOC_SINGLE_EXT("ES755 Get Event Status",
		       SND_SOC_NOPM, 0, 65535, 0, es755_get_event_status, NULL),
	SOC_SINGLE_EXT("Get RDB data size",
		       SND_SOC_NOPM, 0, 65535, 0,
		       es755_get_rdb_size, NULL),
	SOC_ENUM_EXT("Streaming Mode", es755_streaming_mode_enum,
			   escore_get_streaming_mode,
			   escore_put_streaming_mode),
	SOC_ENUM_EXT("Select Endpoint", es755_path_id_enum,
			   es755_get_select_endpoint,
			   es755_put_select_endpoint),
	SOC_ENUM_EXT("Set Digital Gain", digital_gain_enum,
			   es755_get_digital_gain,
			   es755_put_digital_gain),
	SOC_SINGLE_EXT("HS Delay",
		       SND_SOC_NOPM, 0, 2000, 0, es755_get_hs_delay_value,
		       es755_put_hs_delay_value),
};

static int es_voice_sense_add_snd_soc_controls(struct snd_soc_codec *codec)
{
	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	ret = snd_soc_add_codec_controls(codec, es755_voice_sense_snd_controls,
			ARRAY_SIZE(es755_voice_sense_snd_controls));
#else
	ret = snd_soc_add_controls(codec, es755_voice_sense_snd_controls,
			ARRAY_SIZE(es755_voice_sense_snd_controls));
#endif
	return ret;
}

#endif /* CONFIG_SND_SOC_ES_VS */

static void es755_mp_sleep_work(struct work_struct *work)
{
	int rc;

	rc = es755_power_transition(ES_SET_POWER_STATE_MP_SLEEP,
				    ES_POWER_STATE);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): es755_power_transition() failed %d\n",
			__func__, rc);
	}

}

int es755_enter_mp_sleep()
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;
	int rc  = 0;

	ret = queue_delayed_work(escore->power_transition_wq,
				 &(escore->mp_sleep_work),
				 msecs_to_jiffies(ES755_MP_SLEEP_DELAY));
		dev_dbg(escore->dev, "%s() queue_delayed_work() ret = %d\n",
			__func__, ret);

	dev_dbg(escore->dev, "%s() queue_delayed_work() ret = %d\n",
		__func__, ret);
	return rc;
}

int es755_exit_mp_sleep()
{
	struct escore_priv *escore = &escore_priv;
	int rc = 0;
	int ret = 0;

	rc = cancel_delayed_work_sync(&escore->mp_sleep_work);
	dev_dbg(escore->dev, "%s() cancel_delayed_work_sync() rc = %d\n",
		__func__, rc);

	if (escore->escore_power_state == ES_SET_POWER_STATE_MP_SLEEP) {

		dev_dbg(escore->dev,
			"%s(): Bring chip from MP Sleep to Normal mode\n",
			__func__);

		ret = es755_power_transition(ES_SET_POWER_STATE_NORMAL,
					     ES_POWER_STATE);
		if (ret) {
			dev_err(escore->dev,
				"%s(): es755_power_transition() failed %d\n",
				__func__, ret);

			return ret;
		}
	}

	return ret;
}

static int es755_set_bias_level(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
	pr_debug("%s(): Setting bias level to :%d\n", __func__, level);
	codec->dapm.bias_level = level;
	return 0;
}


static void es755_init_regs(struct escore_priv *es755)
{
	int reg_index;

	for (reg_index = 0; reg_index < ES_MAX_REGISTER; reg_index++) {
		/* Initialize values with invalid values */
		es755->reg_cache[reg_index].value = ES_INVALID_REG_VALUE;
	}
}

static int es755_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct escore_priv *es755 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es755->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es755_init_regs(es755);

	ret = es_d300_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_snd_controls fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = es_analog_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_snd_controls fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = es_d300_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_dapm_widgets fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = es_analog_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_dapm_widgets fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = es_d300_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_add_routes fail %d\n",
			__func__, ret);
		return ret;
	}
	ret = es_analog_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_add_routes fail %d\n",
			__func__, ret);
		return ret;
	}

#if defined(CONFIG_SND_SOC_ES_VS)
	ret = es_voice_sense_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es755 VS snd control fail %d\n",
			__func__, ret);
		return ret;
	}

	device_set_wakeup_capable(codec->dev, true);

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	ret = snd_soc_add_codec_controls(codec, es755_snd_controls,
			ARRAY_SIZE(es755_snd_controls));
#else
	ret = snd_soc_add_controls(codec, es755_snd_controls,
			ARRAY_SIZE(es755_snd_controls));
#endif

	ret = es_d300_fill_cmdcache(escore_priv.codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): Cache initialization fail %d\n",
			__func__, ret);
		return ret;
	}

	es755_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return ret;
}

static int  es755_codec_remove(struct snd_soc_codec *codec)
{

	pr_debug("%s(): Codec removed\n", __func__);
	return 0;
}

static unsigned int es755_codec_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct escore_priv *escore = &escore_priv;
	u32 value = 0;
	int rc;
	int ret = 0;

	if (reg > ES_MAX_REGISTER) {
		/*dev_err(codec->dev, "read out of range reg %d", reg);*/
		return 0;
	}

	if (escore->reg_cache[reg].value != ES_INVALID_REG_VALUE) {
		value = escore->reg_cache[reg].value;
		return value;
	}

	mutex_lock(&escore->access_lock);

	if (escore->dhwpt_mode) {
		/* Bring the chip from MP Sleep to normal state before
		 * sending any analog commands*/
		rc = es755_exit_mp_sleep();
		if (rc) {
			dev_err(codec->dev,
			"%s(): es755_exit_mp_sleep() fail %d\n",
			__func__, rc);
			goto out;
		}
	}

	ret = escore_write(codec, ES_CODEC_ADDR, reg);
	if (ret < 0) {
		dev_err(codec->dev, "codec reg %x write err %d\n",
			reg, ret);
		goto out;
	}

	value = escore_read(NULL, ES_CODEC_VALUE);
	rc = value;
	if (value < 0) {
		pr_err("%s(): ES_CODEC_VALUE read fail %d\n", __func__, rc);
		goto out;
	}
	escore->reg_cache[reg].value = value;

	if (escore->mp_sleep_requested && escore->dhwpt_mode &&
			!escore->dhwpt_capture_on
			&& escore->acc_det_status != ES_ACC_BUTTON_PRESSED) {
		pr_debug("%s(): Put chip back into mp_sleep\n", __func__);
		es755_enter_mp_sleep();
	}
out:
	mutex_unlock(&escore->access_lock);
	return (rc < 0) ? rc : value;

}

static int es755_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct escore_priv *escore = &escore_priv;
	int ret = 0;

	if (reg > ES_MAX_REGISTER) {
		/*dev_err(codec->dev, "write out of range reg %d", reg);*/
		return 0;
	}

	mutex_lock(&escore->access_lock);

	if (escore->dhwpt_mode) {
		/* Bring the chip from MP Sleep to normal state before
		 * sending any analog commands*/
		ret = es755_exit_mp_sleep();
		if (ret) {
			dev_err(codec->dev,
				"%s(): es755_exit_mp_sleep() fail %d\n",
				__func__, ret);
			goto out;
		}
	}

	if (atomic_read(&escore->active_streams) > 0)
		usleep_range(1000, 1005);

	ret = escore_write(codec, ES_CODEC_ADDR, reg);
	if (ret < 0) {
		dev_err(codec->dev, "codec ES_CODEC_ADDR, %x write err %d\n",
			reg, ret);
		goto out;
	}

	ret = escore_write(codec, ES_CODEC_VALUE, value);
	if (ret < 0) {
		dev_err(codec->dev, "codec ES_CODEC_VALUE, %x write err %d\n",
			reg, ret);
		goto out;
	}
	escore->reg_cache[reg].value = value;

	if (escore->mp_sleep_requested && escore->dhwpt_mode &&
			!escore->dhwpt_capture_on &&
			escore->acc_det_status != ES_ACC_BUTTON_PRESSED) {
		pr_debug("%s(): Put chip back into mp_sleep\n", __func__);
		es755_enter_mp_sleep();
	}
out:
	mutex_unlock(&escore->access_lock);
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
struct snd_soc_codec_driver soc_codec_dev_es755 = {
	.probe =	es755_codec_probe,
	.remove =	es755_codec_remove,
	.read =		es755_codec_read,
	.write =	es755_codec_write,
	.set_bias_level =	es755_set_bias_level,
};
#else
struct snd_soc_codec_driver soc_codec_dev_es755 = {
	.probe =	es755_codec_probe,
	.remove =	es755_codec_remove,
	.read =		es755_codec_read,
	.write =	es755_codec_write,
	.set_bias_level =	es755_set_bias_level,
};
#endif

int es755_set_streaming(struct escore_priv *escore, int value)
{
	u32 resp;
	return escore_cmd_locked(escore,
		es755_streaming_cmds[escore->streamdev.intf] | value, &resp);
}

int es755_set_datalogging(struct escore_priv *escore, int value)
{
	u32 resp;
	return escore_cmd_locked(escore, value, &resp);
}

void es755_slim_setup(struct escore_priv *escore_priv)
{
	int i;
	int ch_cnt;

	escore_priv->init_slim_slave(escore_priv);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es755_dai); i++) {
		switch (es755_dai[i].id) {
		case ES_SLIM_1_PB:
		case ES_SLIM_2_PB:
		case ES_SLIM_3_PB:
			ch_cnt = es755_dai[i].playback.channels_max;
			break;
		case ES_SLIM_1_CAP:
		case ES_SLIM_2_CAP:
		case ES_SLIM_3_CAP:
			ch_cnt = es755_dai[i].capture.channels_max;
			break;
		default:
				continue;
		}
		escore_priv->slim_dai_data[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}
	/* front end for RX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[0] = 152;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[1] = 153;
	/* back end for RX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[0] = 138;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[1] = 139;
	/* front end for TX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[0] = 156;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[1] = 157;
	/* back end for TX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[0] = 134;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[1] = 135;
	/* front end for RX2 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[0] = 154;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[1] = 155;
	/* back end for RX2 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[0] = 143;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[1] = 144;
}

static int es755_config_jack(struct escore_priv *escore)
{
	struct esxxx_accdet_config *accdet_cfg = &escore->pdata->accdet_cfg;
	union es755_accdet_reg accdet_reg;
	u32 cmd;
	u32 resp;
	int rc;

	/* Setup the Event response */
	cmd = (ES_SET_EVENT_RESP << 16) | escore->pdata->gpio_b_irq_type;
	rc = escore_cmd_nopm(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): Error %d in setting event response\n",
				__func__, rc);
		goto out;
	}

	accdet_reg.value = 0;
	accdet_reg.fields.plug_det_fsm = accdet_cfg->plug_det_enabled & (0x1);
	accdet_reg.fields.debounce_timer = accdet_cfg->debounce_timer & (0x3);

	/* Setup the debounce timer for plug event */
	cmd = (ES_ACCDET_CONFIG_CMD << 16) | (accdet_reg.value);
	rc = escore_cmd_nopm(escore, cmd, &resp);
	if (rc < 0)
		pr_err("%s(): Error %d in setting debounce timer\n",
				__func__, rc);

out:
	return rc;
}

int es755_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	int rc;

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc > -1) {
		rc = es755_config_jack(escore);
		escore_pm_put_autosuspend();
	}
	mutex_unlock(&escore->access_lock);

	if (rc >= 0)
		escore->jack = jack;

	return rc;
}
EXPORT_SYMBOL_GPL(es755_detect);


static struct esxxx_platform_data *es755_populate_dt_pdata(struct device *dev)
{
	struct esxxx_platform_data *pdata;
	struct property *prop;
	u8 *temp;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}

	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
			"adnc,reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,reset-gpio", dev->of_node->full_name,
				pdata->reset_gpio);
		pdata->reset_gpio = -1;
	}
	dev_dbg(dev, "%s: reset gpio %d", __func__, pdata->reset_gpio);

	pdata->wakeup_gpio = of_get_named_gpio(dev->of_node,
						"adnc,wakeup-gpio", 0);
	if (pdata->wakeup_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,wakeup-gpio", dev->of_node->full_name,
				pdata->wakeup_gpio);
		pdata->wakeup_gpio = -1;
	}
	dev_dbg(dev, "%s: wakeup gpio %d", __func__, pdata->wakeup_gpio);
	pdata->uart_gpio = of_get_named_gpio(dev->of_node,
			"adnc,int-gpio", 0);
	if (pdata->uart_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,uart-gpio", dev->of_node->full_name,
				pdata->uart_gpio);
		pdata->uart_gpio = -1;
	}
	dev_dbg(dev, "%s(): int gpio %d",
			__func__, pdata->uart_gpio);

/* API Interrupt registration */
#ifdef CONFIG_SND_SOC_ES_GPIO_A
	dev_dbg(dev, "%s(): gpioa configured\n", __func__);
	pdata->gpioa_gpio = of_get_named_gpio(dev->of_node,
			"adnc,gpioa-gpio", 0);
	if (pdata->gpioa_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpioa-gpio", dev->of_node->full_name,
				pdata->gpioa_gpio);
		pdata->gpioa_gpio = -1;
	}
#endif
	dev_dbg(dev, "%s: gpioa_gpio %d", __func__, pdata->gpioa_gpio);

	pdata->gpiob_gpio = of_get_named_gpio(dev->of_node,
						"adnc,gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpiob-gpio", dev->of_node->full_name,
				pdata->gpiob_gpio);
		pdata->gpiob_gpio = -1;
	}
	dev_dbg(dev, "%s: gpiob_gpio %d", __func__, pdata->gpiob_gpio);

	prop = of_find_property(dev->of_node, "adnc,enable_hs_uart_intf", NULL);
	if (prop != NULL) {
		temp =	(u8 *)prop->value;
		if (temp[3] == 1)
			pdata->enable_hs_uart_intf = true;
		else
			pdata->enable_hs_uart_intf = false;
	}

	prop = of_find_property(dev->of_node, "adnc,ext_clk_rate", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->ext_clk_rate = temp[3];
	}

	prop = of_find_property(dev->of_node, "adnc,gpio_b_irq_type", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->gpio_b_irq_type = temp[3];
	}

	prop = of_find_property(dev->of_node, "adnc,debounce_timer", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->accdet_cfg.debounce_timer = temp[3];
	}

	prop = of_find_property(dev->of_node, "adnc,plug_det_enabled", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->accdet_cfg.plug_det_enabled = temp[3];
	}

	prop = of_find_property(dev->of_node, "adnc,mic_det_enabled", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->accdet_cfg.mic_det_enabled = temp[3];
	}

/*
	prop = of_find_property(dev->of_node, "adnc,hs_mg_select", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->accdet_cfg.hs_mg_select = temp[3];
	}
*/
	pdata->accdet_cfg.hs_mg_select = 2;


	prop = of_find_property(dev->of_node, "adnc,cmd_comp_mode", NULL);
	if (prop != NULL) {
		temp = (u8 *)prop->value;
		if (temp[3] != 0)
			pdata->cmd_comp_mode = temp[3];
	}

/*TODO:Acce detect GPIO*/
	/*
pdata->gpiob_gpio = of_get_named_gpio(dev->of_node,
						"adnc,gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpiob-gpio", dev->of_node->full_name,
				pdata->gpiob_gpio);
		pdata->gpiob_gpio = -1;
	}
	dev_dbg(dev, "%s: gpiob_gpio %d", __func__, pdata->gpiob_gpio);
*/
#ifdef CONFIG_QPNP_CLKDIV
	codec_clk = qpnp_clkdiv_get(dev, "es755-mclk");
#else
	codec_clk = clk_get(dev, "es755-mclk");
#endif
	if (IS_ERR(codec_clk)) {
		dev_err(dev, "%s: Failed to request es755 mclk from pmic %ld\n",
				__func__, PTR_ERR(codec_clk));
		pdata->esxxx_clk_cb = NULL;
	} else {
		pdata->esxxx_clk_cb = es755_clk_ctl;
		pdata->esxxx_clk_cb(1);
	}

	return pdata;
}


static int es755_mic_config(struct escore_priv *escore)
{
	struct snd_soc_codec *codec = escore->codec;
	union es755_accdet_reg accdet_reg;
	struct esxxx_accdet_config accdet_cfg = escore->pdata->accdet_cfg;

	accdet_reg.value = 0;

	accdet_reg.fields.mic_det_fsm = 1;
	accdet_reg.fields.plug_det_fsm = accdet_cfg.plug_det_enabled & (0x1);
	accdet_reg.fields.debounce_timer = accdet_cfg.debounce_timer & (0x3);
	/*
	 * BAS-3584: As per updated accessory detection logic, always set
	 * mg_sel_force bit to 1 and mg_select bits as per required
	 * configuration - LRGM: 0, LRMG: 1, Both: 2
	 * */
	accdet_reg.fields.mg_sel_force = 1;
	accdet_reg.fields.mg_select = accdet_cfg.hs_mg_select;

	pr_debug("%s()\n", __func__);

	return escore_write_locked(codec, ES_ACCDET_CONFIG, accdet_reg.value);
}

static int es755_button_config(struct escore_priv *escore)
{
	union es755_btn_ctl1 btn_ctl1;
	int rc = 0;

	btn_ctl1.value = 0;
	/* Enable Button Press Detection */
	btn_ctl1.fields.btn_press_det = ES_BTN_DET_EN;
	mutex_lock(&escore->access_lock);

	/* Read default value */
	rc = escore_read(NULL, ES_BUTTON_CTRL1);
	if (rc < 0) {
		dev_err(escore_priv.dev,
			"%s(): ES_BUTTON_CTRL Read fail %d\n",
			__func__, rc);
		goto btn_cfg_exit;
	}

	/* Enable btn_press_det bit */
	btn_ctl1.value |= rc;

	/* Write updated value with Button Press Detection enabled */
	rc = escore_write(NULL, ES_BUTTON_CTRL1, btn_ctl1.value);
	if (rc < 0)
		pr_err("%s(): Error setting button control 1 %d\n",
		       __func__, rc);
btn_cfg_exit:
	mutex_unlock(&escore->access_lock);
	return rc;

}

static int chk_btn_press_event(
	struct escore_priv *escore, int sys_intr_resp)
{
	int rc = 0, value;
	union es755_accdet_status_reg accdet_status_reg;

	if (ES_BUTTON_PRESS_EVENT(sys_intr_resp)) {
		pr_debug("%s(): Button press event\n", __func__);

		/*
		 * Due to FW constraint, wait for 65ms before Button
		 * Detection
		 */
		msleep(65);

		value = escore_read_locked(NULL, ES_GET_ACCDET_STATUS);
		if (value < 0) {
			pr_err("%s(): Accessory detect status fail %d\n",
			       __func__, value);
			rc = value;
			goto err_exit;
		}

		accdet_status_reg.value = value;

		pr_debug("Impd:%d\n", accdet_status_reg.fields.impd_level);

		switch (accdet_status_reg.fields.impd_level) {
#if defined(CONFIG_SND_SOC_ES854) || defined(CONFIG_SND_SOC_ES857)
		case 0:
			snd_soc_jack_report(escore->jack,
					    SND_JACK_BTN_0, SND_JACK_BTN_0);
			escore->acc_btn_pressed = SND_JACK_BTN_0;
			pr_info("%s(): Button-0 Press event\n", __func__);
			break;

		case 1:
			/* BTN_3 is mapped with IMPD_LEVEL 1 to support
			 * Google Voice Search trigger functionality */
			snd_soc_jack_report(escore->jack,
					    SND_JACK_BTN_3, SND_JACK_BTN_3);
			escore->acc_btn_pressed = SND_JACK_BTN_3;
			pr_info("%s(): Button-3 Press event\n", __func__);
			break;
#else
		case 0:
		case 1:
			snd_soc_jack_report(escore->jack,
					    SND_JACK_BTN_0, SND_JACK_BTN_0);
			escore->acc_btn_pressed = SND_JACK_BTN_0;
			pr_info("%s(): Button-0 Press event\n", __func__);
			break;
#endif

		case 2:
			snd_soc_jack_report(escore->jack,
					    SND_JACK_BTN_1, SND_JACK_BTN_1);
			escore->acc_btn_pressed = SND_JACK_BTN_1;
			pr_info("%s(): Button-1 Press event\n", __func__);
			break;

		case 3:
			snd_soc_jack_report(escore->jack,
					    SND_JACK_BTN_2, SND_JACK_BTN_2);
			escore->acc_btn_pressed = SND_JACK_BTN_2;
			pr_info("%s(): Button-2 Press event\n", __func__);
			break;

		default:
			escore->acc_btn_pressed = ES_ACC_BTN_INVALID;
			pr_err("Invalid button type : %d\n",
			       accdet_status_reg.fields.impd_level);
			break;
		}

		/* Avoid chip suspend when button is still pressed */
		if (escore->acc_det_status != ES_ACC_BUTTON_PRESSED) {
			rc = escore_pm_get_sync();
			if (rc < 0) {
				pr_err("%s(): pm_get_sync failed :%d\n",
				       __func__, rc);
				goto err_exit;
			}
			escore->acc_det_status = ES_ACC_BUTTON_PRESSED;
		}
	}

err_exit:
	return rc;
}

static int chk_btn_release_event(
	struct escore_priv *escore, int sys_intr_resp)
{
	int rc = 0;

	if (ES_BUTTON_RELEASE_EVENT(sys_intr_resp)) {
		pr_debug("%s(): Button release event\n", __func__);

		/*
		 * If user presses button for more than autosuspend
		 * time, firmware should not be put into suspend mode
		 * until button is released OR it is unplugged.
		 * Put the chip back to autosuspend when user releases
		 * the button.
		 */
		if (escore->acc_det_status == ES_ACC_BUTTON_PRESSED) {
			escore_pm_put_autosuspend();
			if (escore->mp_sleep_requested && escore->dhwpt_mode
						&& !escore->dhwpt_capture_on) {
				pr_debug("%s(): Put chip back into mp_sleep\n",
							__func__);
				es755_enter_mp_sleep();
			}
		}

		pr_debug("button type : %d\n", escore->acc_btn_pressed);

		switch (escore->acc_btn_pressed) {
		case SND_JACK_BTN_0:
			/* Button release event must be sent */
			snd_soc_jack_report(escore->jack, 0, SND_JACK_BTN_0);

			pr_info("%s(): Button-0 release event\n", __func__);
			break;

		case SND_JACK_BTN_1:
			/* Button release event must be sent */
			snd_soc_jack_report(escore->jack, 0, SND_JACK_BTN_1);
			pr_info("%s(): Button-1 release event\n", __func__);
			break;

		case SND_JACK_BTN_2:
			/* Button release event must be sent */
			snd_soc_jack_report(escore->jack, 0, SND_JACK_BTN_2);
			pr_info("%s(): Button-2 release event\n", __func__);
			break;

		case SND_JACK_BTN_3:
			/* Button release event must be sent */
			snd_soc_jack_report(escore->jack, 0, SND_JACK_BTN_3);
			pr_info("%s(): Button-3 release event\n", __func__);
			break;

		case ES_ACC_BTN_INVALID:
		default:
			pr_err("Invalid button type : %d\n",
			       escore->acc_btn_pressed);
			break;
		}

		escore->acc_det_status = ES_ACC_BUTTON_RELEASED;

	}

	return rc;
};

static int es755_codec_intr(struct notifier_block *self, unsigned long action,
		void *dev)
{
	struct escore_priv *escore = (struct escore_priv *)dev;
	union es755_accdet_status_reg accdet_status_reg;
	struct esxxx_accdet_config accdet_cfg = escore->pdata->accdet_cfg;
	int sys_intr_resp, value;
	int rc = 0;
	static int redetect_hs = 1;
	u8 impd_level;
	u8 mg_sel_force;
	u8 mg_select;
	u8 is_invalid_type;

	pr_debug("%s(): Event: 0x%04x\n", __func__, (u32)action);

	if (escore->dhwpt_mode) {
		/* Cancel delayed work of MP Sleep and bring chip to Normal
		   mode if it is in MP Sleep mode */
		pr_debug("%s(): Bring chip from MP Sleep to Normal mode\n",
				__func__);
		rc = es755_exit_mp_sleep();
		if (rc) {
			dev_err(escore_priv.dev,
					"%s(): es755_exit_mp_sleep() fail %d\n",
					__func__, rc);
			goto intr_exit;
		}
	}

	if (action & ES_CODEC_INTR_EVENT) {
		sys_intr_resp = escore_read_locked(NULL, ES_GET_SYS_INTERRUPT);
		if (sys_intr_resp < 0) {
			pr_err("%s(): Get System Event failed %d\n",
			       __func__, sys_intr_resp);
			goto intr_exit;
		}

		if (ES_FACTORY_INTR_EVENT(sys_intr_resp)) {
			pr_err("%s(): Factory mode interrupt reported !!\n",
						__func__);
			escore_pm_put_autosuspend();
			goto intr_exit;
		}

		if (ES_PLUG_EVENT(sys_intr_resp)) {

			/*
			 * Due to Quick insertion, FW might give more
			 * than 1 interrupt for single Plug event. if
			 * MIC Config is already sent, ignore
			 * subsequent Plug events.
			 */
			/*
			 * However, in case of Slow insertions, FW
			 * will send 2 Plug-in events which is valid and
			 * should not be ignored.
			 *
			 * Since the events are actually coming from the
			 * FW, HSW should display it as is. So, accept
			 * plug-in events even if it is a Duplicate.
			*/
			pr_info("%s(): Plug event\n", __func__);
			/* Enable MIC Detection */
			rc = es755_mic_config(escore);
			if (rc < 0) {
				pr_err("%s(): MIC config fail %d\n",
				       __func__, rc);
				goto intr_exit;
			}

			escore->acc_det_status = ES_ACC_PLUGGED;
		}

		if (ES_MICDET_EVENT(sys_intr_resp)) {

			value = escore_read_locked(NULL, ES_GET_ACCDET_STATUS);
			if (value < 0) {
				pr_err("%s(): Accessory detect status fail %d\n",
				       __func__, value);
				goto intr_exit;
			}

			accdet_status_reg.value = value;

			/* Ignore interrupt if
			 * plugdet_fsm2 = 1 and plugdet_fsm1 = 0
			 */
			if (accdet_status_reg.fields.plug_det_fsm2 &&
				!accdet_status_reg.fields.plug_det_fsm1) {
				pr_debug("%s(): Found PLUG_LOCK, ignore it\n",
				       __func__);
				goto intr_exit;
			}

			impd_level = accdet_status_reg.fields.impd_level;

			mg_select = accdet_status_reg.fields.mg_select;
			mg_sel_force = accdet_cfg.mic_det_enabled & (0x1);

			/* Updated Headphone Type Decode Table as per BAS_3584
			 *  ------------------------------
			 * | MG_SEL | IMP_LEVEL | HS_TYPE |
			 * |   X    |    0      |  LRG HP |
			 * |   0    |   1-5     |  Inval  |
			 * |   0    |    6      |  LRGM   |
			 * |   1    |   1-5     |  Inval  |
			 * |   1    |    6      |  LRMG   |
			 *  ------------------------------
			 */

			/* Open mic detected then according to es854 API guide
			 * HS_TYPE is assume to LRGG */
			if (ES_OPEN_MICDET_EVENT(sys_intr_resp)) {
				pr_info("%s(): LRGG Headset detected, Reporting as Headphone\n",
						__func__);
				snd_soc_jack_report(escore->jack,
					SND_JACK_HEADPHONE,
					SND_JACK_HEADPHONE);
				escore->acc_det_status = ES_ACC_DETECTED;

			} else if (impd_level) {
				pr_info("%s(): Headset detected\n", __func__);

				if (redetect_hs) {
					redetect_hs = 0;
					msleep(escore_priv.hs_delay);
					pr_debug("%s(): ***Redetect HS MIC with Delay %d\n",
						__func__, escore_priv.hs_delay);
					/* redetect_HS   */
					rc = es755_mic_config(escore);
					if (rc < 0) {
						pr_err("%s(): MIC config fail %d\n",
							   __func__, rc);
					}
					goto intr_exit;
				} else
					redetect_hs = 1;

				/* Mic Impedence - 6 */
				if (impd_level == MIC_IMPEDANCE_LEVEL) {
					if (mg_select)
						pr_info("LRMG Headset\n");
					else
						pr_info("LRGM Headset\n");
					is_invalid_type = false;
				} else {
					/* MIC Impedence - 1 to 5 or > 6 */
					is_invalid_type = true;
				}

				if (is_invalid_type) {
					pr_err("Invalid type:%d\n", impd_level);
					rc = -EINVAL;
					goto intr_exit;
				}
				escore->es_vs_route_preset =
						es755_vs_presets[ES755_MICHS];
				escore->es_cvs_preset =
						es755_cvs_presets[ES755_MICHS];
				escore->mic_source_change = 1;

				escore->button_config_required = 1;
				snd_soc_jack_report(escore->jack,
					SND_JACK_HEADSET, SND_JACK_HEADSET);

				escore->acc_det_status = ES_ACC_DETECTED;

				rc = es755_button_config(escore);
				if (rc < 0)
					goto intr_exit;

			} else {
				pr_info("%s(): Headphone detected\n",
						__func__);
				redetect_hs = 1;

				snd_soc_jack_report(escore->jack,
					SND_JACK_HEADPHONE,
					SND_JACK_HEADPHONE);

				escore->acc_det_status = ES_ACC_DETECTED;
			}

			pr_debug("%s(): AccDet status 0x%04x\n", __func__,
					accdet_status_reg.value);
		}

		/*
		 * Note: Un-Plug event should have higher priority compared to
		 * button press & release Event Checks.
		 */
		if (ES_UNPLUG_EVENT(sys_intr_resp)) {

			/*
			 * Due to Quick removal, FW might give more
			 * than 1 interrupt for single Unplug event.
			 * If Unplug Event is already detected, ignore
			 * subsequent Unplug events.
			 */
			if (escore->acc_det_status == ES_ACC_UNPLUGGED) {
				pr_debug("%s(): Ignoring Duplicate Unplug Event\n",
					 __func__);
				goto intr_exit;
			}

			value = escore_read_locked(NULL, ES_GET_ACCDET_STATUS);
			if (value < 0) {
				pr_err("%s(): Accessory detect status fail %d",
				       __func__, value);
				goto intr_exit;
			}
			accdet_status_reg.value = value;
			redetect_hs = 1; /* initialize the counter */

			/* Ignore interrupt if
			 * plugdet_fsm2 = 1 and plugdet_fsm1 = 1
			 */
			if (accdet_status_reg.fields.plug_det_fsm2 &&
			    accdet_status_reg.fields.plug_det_fsm1) {
				pr_info("%s(): Found UNPLUG_LOCK, ignore it\n",
					__func__);
				goto intr_exit;
			}

			pr_info("%s(): Unplug detected\n", __func__);
			escore->es_vs_route_preset =
				es755_vs_presets[escore->default_cvq_mic];
			escore->es_cvs_preset =
				es755_cvs_presets[escore->default_cvq_mic];
			/* Headset Unplug detected because
			 * button_config_required is enabled only when
			 * Headset is detected */
			if (escore->button_config_required) {
				escore->mic_source_change = 1;
				escore->button_config_required = 0;
			}
			snd_soc_jack_report(escore->jack, 0, JACK_DET_MASK);

			/*
			 * If user presses button for more than autosuspend
			 * time, firmware should not be put into suspend mode
			 * until button is released OR it is unplugged.
			 * Put the chip back to autosuspend when user unplugs.
			 */
			if (escore->acc_det_status == ES_ACC_BUTTON_PRESSED) {
				escore_pm_put_autosuspend();
				if (escore->mp_sleep_requested
						&& escore->dhwpt_mode
						&& !escore->dhwpt_capture_on) {
					pr_debug("%s(): %s\n",
						 "Put chip back into mp_sleep",
						__func__);
					es755_enter_mp_sleep();
				}
			}

			escore->acc_det_status = ES_ACC_UNPLUGGED;

			/*
			 * Since Headphone/set is removed, Mask the Button
			 * Press/Release event as well. This way, duplicate or
			 * invalid Button event is avoided.
			 */
			sys_intr_resp &= ~(ES_BUTTON_RELEASE_EVENT_MASK |
						ES_BUTTON_PRESS_EVENT_MASK);

		}

		if (ES_THERMAL_SHUTDOWN_EVENT(sys_intr_resp))
			pr_err("%s(): Thermal shut down detected\n",
				__func__);

		if (ES_LINEOUT_SHORT_CIRCUIT_EVENT(sys_intr_resp))
			pr_err("%s(): Lineout short circuit detected\n",
				__func__);

		if (ES_SPKR_LEFT_SHORT_CIRCUIT_EVENT(sys_intr_resp))
			pr_err("%s(): Speaker left short circuit detected\n",
				__func__);

		if (ES_SPKR_RIGHT_SHORT_CIRCUIT_EVENT(sys_intr_resp))
			pr_err("%s(): Speaker right short circuit detected\n",
				__func__);

		if (ES_HP_SHORT_CIRCUIT_EVENT(sys_intr_resp))
			pr_err("%s(): Headphone short circuit detected\n",
				__func__);

		if (ES_EP_SHORT_CIRCUIT_EVENT(sys_intr_resp))
			pr_err("%s(): Earphone short circuit detected\n",
				__func__);


		/*
		 * If previous Event is not Button Press, Test Button Press 1st
		 * & then Button Release.
		 */
		if (escore->acc_det_status != ES_ACC_BUTTON_PRESSED) {
			rc = chk_btn_press_event(escore, sys_intr_resp);
			if (rc < 0)
				goto intr_exit;

			chk_btn_release_event(escore, sys_intr_resp);

		}  else {

			/* Test Button Release 1st  & then Button Press */
			rc = chk_btn_release_event(escore, sys_intr_resp);
			if (rc < 0)
				goto intr_exit;

			chk_btn_press_event(escore, sys_intr_resp);
		}
	}

intr_exit:
	return NOTIFY_OK;
}

irqreturn_t es755_cmd_completion_isr(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;

	BUG_ON(!escore);

	pr_debug("%s(): API Interrupt received\n", __func__);
	/* Complete if expected */
	if (escore->wait_api_intr) {
		pr_debug("%s(): API Interrupt completion.\n", __func__);
		escore->wait_api_intr = 0;
		complete(&escore->cmd_compl);
	}
	return IRQ_HANDLED;
}

irqreturn_t es755_irq_work(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;
	int retry = ES_EVENT_STATUS_RETRY_COUNT;
	int rc, ret;
	u32 cmd = 0;
	u32 event_type = 0;

	pr_debug("%s() Interrupt received\n", __func__);
	if (!escore) {
		pr_err("%s(): Invalid IRQ data\n", __func__);
		return IRQ_HANDLED;
	}

	escore->intr_recvd = 1;

	/* Take wake lock to avoid system wide suspend */
	wake_lock_timeout(&escore_priv.wake_lock, 1000);
repeat:
	rc = wait_event_interruptible(escore->irq_waitq,
			(escore->dev->power.is_suspended != true));
	if (unlikely(rc == -ERESTARTSYS)) {
		pr_err("%s(): Signal received, try again\n", __func__);
		goto repeat;
	}

	mutex_lock(&escore->access_lock);
	rc = escore_pm_get_sync();
	if (rc < 0) {
		dev_err(escore_priv.dev, "%s(): pm_get_sync() failed :%d\n",
								__func__, rc);
		mutex_unlock(&escore->access_lock);
		goto err_pm_get_sync;
	}

	/* Enable the clocks. This is required for codec interrupts. If chip
	 * is in suspend mode, runtime PM is disabled. This ISR is executed
	 * before system resume and hence chip needs to be provided clock
	 * before doing any command transfer.
	 */
	if (escore_priv.pdata->esxxx_clk_cb) {
		escore_priv.pdata->esxxx_clk_cb(1);
		/* Setup for clock stablization delay */
		msleep(ES_PM_CLOCK_STABILIZATION);
	}

	if (escore->dhwpt_mode) {
		/* Cancel delayed work of MP Sleep, and bring chip to
		   Normal mode if it is in MP Sleep mode */
		pr_debug("%s(): Bring chip from MP Sleep to Normal mode\n",
				__func__);
		rc = es755_exit_mp_sleep();
		if (rc) {
			dev_err(escore_priv.dev,
			"%s(): es755_exit_mp_sleep() fail %d\n",
			__func__, rc);
			goto irq_exit;
		}
	}

	/* BAS-2402: When pm usage count is non-zero and device is in
	 * VS Low Power mode, following delay is required for firmware
	 * to be ready after keyword detection interrupt is received.
	 * Runtime PM status value is set to ES_PM_OFF only by CVQ
	 * from application and increments pm usage count, otherwise
	 * setting Runtime PM status value to ES_PM_OFF is not advisable
	 * for actual usecases.
	 */
	if (escore->escore_power_state == ES_SET_POWER_STATE_VS_LOWPWR)
		msleep(escore_priv.delay.vs_to_normal);
	else if (escore->escore_power_state == ES_SET_POWER_STATE_MP_SLEEP)
		msleep(escore_priv.delay.mpsleep_to_normal);

	/* Power state change:
	 * Interrupt will put the chip into Normal State
	 */

	cmd = ES_GET_EVENT << 16;

	while (retry) {
		rc = escore_cmd_nopm(escore, cmd, &event_type);
		if (!rc || !--retry)
			break;
		pr_info("%s(): wakeup and retry\n", __func__);
		ret = escore_wakeup(escore);
		if (ret) {
			dev_err(escore->dev, "%s() wakeup failed ret = %d\n",
								__func__, ret);
			break;
		}
	}
	if (rc < 0) {
		pr_err("%s(): Error reading IRQ event: %d\n", __func__, rc);
		mutex_unlock(&escore->access_lock);
		goto irq_exit;
	}

	mutex_unlock(&escore->access_lock);

	if (escore->escore_power_state == escore->non_vs_sleep_state ||
		escore->escore_power_state == ES_SET_POWER_STATE_MP_SLEEP) {
		escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;
	} else if (escore->escore_power_state == ES_SET_POWER_STATE_VS_LOWPWR) {
		escore->escore_power_state = ES_SET_POWER_STATE_VS_OVERLAY;

		/* If the chip is not awake, the reading of event will wakeup
		 * the chip which will result in interrupt reconfiguration
		 * from device resume. Reconfiguration is deferred till the
		 * time interrupt is processed from the notifier callback.
		 *
		 * The interrupt reconfiguration in this case will be taken
		 * care of by a low priority notifier callback
		 */
		escore->defer_intr_config = 1;
	}

	event_type &= ES_MASK_INTR_EVENT;
	mutex_lock(&escore->escore_event_type_mutex);
	escore->escore_event_type = event_type;
	mutex_unlock(&escore->escore_event_type_mutex);

	if (event_type != ES_NO_EVENT) {
		pr_debug("%s(): Notify subscribers about 0x%04x event",
				__func__, event_type);
		blocking_notifier_call_chain(escore->irq_notifier_list,
						event_type, escore);
	}

irq_exit:
	if (escore->mic_source_change && escore->voice_recognition)
		escore_pm_put_sync_suspend();
	else
		escore_pm_put_autosuspend();
err_pm_get_sync:
	escore->intr_recvd = 0;
	escore->mic_source_change = 0;
	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(es755_irq_notifier_list);

static struct notifier_block es755_codec_intr_cb = {
	.notifier_call = es755_codec_intr,
	.priority = ES_NORMAL,
};

int es755_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata;
	int rc = 0;
	unsigned long irq_flags = IRQF_DISABLED;
#if defined(CONFIG_SND_SOC_ES854)
	const char *fw_filename = "audience/es854/audience-es854-fw.bin";
	const char *fw_af_filename = "audience/es854/audience-es755-af.bin";
#if defined(CONFIG_SND_SOC_ES_VS)
	const char *vs_filename = "audience/es854/audience-es854-vs.bin";
	const char *bkg_filename =
		"audience/cvqmodels/adnc_cvq_detection_bkg_w_hdrs.bin";
#endif
#elif defined(CONFIG_SND_SOC_ES857)
	const char *fw_filename = "audience/es857/audience-es857-fw.bin";
	const char *fw_af_filename = "audience/es857/audience-es857-af.bin";
#if defined(CONFIG_SND_SOC_ES_VS)
	const char *vs_filename = "audience/es857/audience-es857-vs.bin";
	const char *bkg_filename =
		"audience/cvqmodels/adnc_cvq_detection_bkg_w_hdrs.bin";
#endif
#else
	const char *fw_filename = "audience/es755/audience-es755-fw.bin";
	const char *fw_af_filename = "audience/es755/audience-es755-af.bin";
#if defined(CONFIG_SND_SOC_ES_VS)
	const char *vs_filename = "audience/es755/audience-es755-vs.bin";
	const char *bkg_filename =
		"audience/cvqmodels/adnc_cvq_detection_bkg_w_hdrs.bin";
#endif
#endif

	/**/
	if (dev->of_node) {
		dev_info(dev, "Platform data from device tree\n");
		pdata = es755_populate_dt_pdata(dev);
		dev->platform_data = pdata;

		/* Retrieve FW names from DTS if provided */
		if (of_property_read_string(dev->of_node, "adnc,fw_filename",
					(const char **)&fw_filename)) {
			dev_err(escore_priv.dev, "%s, %s\n",
				"Property adnc,fw_filename missing or invalid",
				"Using default firmware name");
		}

		if (of_property_read_string(dev->of_node, "adnc,vs_filename",
					(const char **)&vs_filename)) {
			dev_err(escore_priv.dev, "%s, %s\n",
				"Property adnc,vs_filename missing or invalid",
				"Using default firmware name");
		}

	} else {
		dev_info(dev, "Platform data from board file\n");
		pdata = dev->platform_data;
	}

	escore_priv.pdata = pdata;

	mutex_init(&escore_priv.datablock_dev.datablock_read_mutex);
	mutex_init(&escore_priv.pm_mutex);
	mutex_init(&escore_priv.streaming_mutex);
	mutex_init(&escore_priv.msg_list_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_mutex);
	mutex_init(&escore_priv.escore_event_type_mutex);
	wake_lock_init(&escore_priv.wake_lock, WAKE_LOCK_SUSPEND, "es755_wakelock");

	init_completion(&escore_priv.cmd_compl);
	init_waitqueue_head(&escore_priv.stream_in_q);
	init_waitqueue_head(&escore_priv.irq_waitq);
	INIT_LIST_HEAD(&escore_priv.msg_list);
	escore_priv.irq_notifier_list = &es755_irq_notifier_list;
	escore_priv.cmd_compl_mode = ES_CMD_COMP_POLL;
	escore_priv.wait_api_intr = 0;
	escore_priv.fw_af_filename = fw_af_filename;
	escore_priv.mic_source_change = 0;

	escore_priv.selected_endpoint = -1;
	memset(escore_priv.digital_gain, 0, sizeof(escore_priv.digital_gain));

	escore_priv.power_transition_wq =
		create_singlethread_workqueue("escore_power_transition_wq");
	INIT_DELAYED_WORK(&escore_priv.mp_sleep_work, es755_mp_sleep_work);

#if defined(CONFIG_SND_SOC_ES_VS)
	rc = escore_vs_init(&escore_priv);
#endif

	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
		rc = devm_gpio_request(dev, pdata->wakeup_gpio, "es755_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_wakeup request fail %d\n",
				__func__, rc);
			goto exit;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_wakeup direction fail %d\n",
				__func__, rc);
			goto exit;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_wakeup undefined\n",
			 __func__);
	}

#ifndef CONFIG_SND_SOC_ES_GPIO_A
	dev_dbg(dev, "%s(): gpioa not configured\n", __func__);
	pdata->gpioa_gpio = -1;
#endif
	dev_dbg(escore_priv.dev, "%s(): gpioa_gpio = %d\n", __func__,
		pdata->gpioa_gpio);
	if (pdata->gpioa_gpio != -1) {
		rc = devm_gpio_request(dev, pdata->gpioa_gpio, "es755_gpioa");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpioa request fail %d\n",
				__func__, rc);
			goto exit;
		}
		rc = gpio_direction_input(pdata->gpioa_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpioa direction fail %d\n",
				__func__, rc);
			goto exit;
		}
		/* Fix value for IRQ Type */
		pdata->gpio_a_irq_type = ES_FALLING_EDGE;
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_gpioa undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpiob_gpio = %d\n", __func__,
		pdata->gpiob_gpio);

	if (pdata->gpiob_gpio != -1) {
		rc = devm_gpio_request(dev, pdata->gpiob_gpio, "es755_gpiob");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpiob request fail %d\n",
				__func__, rc);
			goto exit;
		}
		rc = gpio_direction_input(pdata->gpiob_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpiob direction fail %d\n",
				__func__, rc);
			goto exit;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_gpiob undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
		rc = devm_gpio_request(dev, pdata->reset_gpio, "es755_reset");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_reset (%d) request failed :%d",
				__func__, pdata->reset_gpio, rc);
			goto exit;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_reset direction fail %d\n",
				__func__, rc);
			goto exit;
		}
		if (!escore_priv.flag.reset_done)
			escore_gpio_reset(&escore_priv);
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_reset undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}

#if defined(CONFIG_SND_SOC_ES_VS)
	rc = escore_vs_request_firmware(&escore_priv, vs_filename);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n",
			__func__, vs_filename, rc);
		goto request_vs_firmware_error;
	}

	rc = escore_vs_request_bkg(&escore_priv, bkg_filename);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware of bkg failed %d\n",
			__func__, rc);
		goto vs_request_bkg_err;
	}
#endif

	escore_priv.boot_ops.bootup = es755_bootup;
	escore_priv.soc_codec_dev_escore = &soc_codec_dev_es755;
	escore_priv.dai = es755_dai;
	escore_priv.dai_nr = ES_NUM_CODEC_DAIS;
	escore_priv.api_addr_max = ES_API_ADDR_MAX;
	escore_priv.api_access = es755_api_access;
	escore_priv.reg_cache = a300_reg_cache;
	escore_priv.flag.is_codec = 1;
	escore_priv.escore_power_state = ES_SET_POWER_STATE_NORMAL;
	escore_priv.escore_event_type = ES_NO_EVENT;
	escore_priv.i2s_dai_data = i2s_dai_data;
	escore_priv.config_jack = es755_config_jack;
	escore_priv.non_vs_sleep_state = ES_SET_POWER_STATE_DEEP_SLEEP;
	if (escore_priv.pri_intf == ES_SLIM_INTF) {

		escore_priv.slim_rx = slim_rx;
		escore_priv.slim_tx = slim_tx;
		escore_priv.slim_dai_data = slim_dai_data;
		escore_priv.slim_setup = es755_slim_setup;

		escore_priv.slim_rx_ports = ES_SLIM_RX_PORTS;
		escore_priv.slim_tx_ports = ES_SLIM_TX_PORTS;
		escore_priv.codec_slim_dais = ES_NUM_CODEC_SLIM_DAIS;

		escore_priv.slim_tx_port_to_ch_map = es755_slim_tx_port_to_ch;
		escore_priv.slim_rx_port_to_ch_map = es755_slim_rx_port_to_ch;

#if defined(CONFIG_ARCH_MSM)
		escore_priv.slim_dai_ops.get_channel_map =
				es755_slim_get_channel_map;
#endif
		escore_priv.slim_dai_ops.set_channel_map =
				es755_slim_set_channel_map;

		/* Initialization of be_id goes here if required */
		escore_priv.slim_be_id = NULL;

		/* Initialization of _remote_ routines goes here if required */
		escore_priv.remote_cfg_slim_rx = NULL;
		escore_priv.remote_cfg_slim_tx = NULL;
		escore_priv.remote_close_slim_rx = NULL;
		escore_priv.remote_close_slim_tx = NULL;

		escore_priv.flag.local_slim_ch_cfg = 1;
		escore_priv.channel_dir = es755_channel_dir;
		escore_priv.slim_setup(&escore_priv);
	}
#if defined(CONFIG_SND_SOC_ES_I2S)
	escore_priv.i2s_dai_ops.hw_params = es755_hw_params;
	escore_priv.i2s_dai_ops.set_fmt = es755_set_fmt;
	escore_priv.i2s_dai_ops.shutdown = es755_shutdown;
#endif

	/* Setup Default Accessory Detect Parameters */
	escore_priv.acc_det_status = ES_ACC_INVALID;
	escore_priv.acc_btn_pressed = ES_ACC_BTN_INVALID;
	escore_priv.hs_delay = DEFAULT_HS_REDETECTION_DELAY;

	/* API Interrupt registration */
	if (pdata->gpioa_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpioa_gpio), NULL,
				es755_cmd_completion_isr,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"es755-cmd-completion-isr", &escore_priv);
		if (rc < 0) {
			pr_err("%s() API interrupt registration failed :%d",
					__func__, rc);
			goto api_intr_error;
		}
	}

	/* Event Interrupt registration */
	if (pdata->gpiob_gpio != -1 && pdata->gpio_b_irq_type) {
		switch (pdata->gpio_b_irq_type) {
		case ES_ACTIVE_LOW:
			irq_flags = IRQF_TRIGGER_LOW;
			break;
		case ES_ACTIVE_HIGH:
			irq_flags = IRQF_TRIGGER_HIGH;
			break;
		case ES_FALLING_EDGE:
			irq_flags = IRQF_TRIGGER_FALLING;
			break;
		case ES_RISING_EDGE:
			irq_flags = IRQF_TRIGGER_RISING;
			break;
		}

		rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio), NULL,
				es755_irq_work, irq_flags | IRQF_ONESHOT,
				"escore-irq-work", &escore_priv);
		if (rc < 0) {
			pr_err("Error in registering interrupt :%d", rc);
			goto event_intr_error;
		} else {
			irq_set_irq_wake(gpio_to_irq(pdata->gpiob_gpio), 1);
		}

		/* Disable the interrupt till needed */
		if (escore_priv.pdata->gpiob_gpio != -1)
			disable_irq(gpio_to_irq(pdata->gpiob_gpio));

		escore_register_notify(escore_priv.irq_notifier_list,
				&es755_codec_intr_cb);
	}

#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
	escore_priv.streamdev = es_uart_streamdev;
#elif defined(CONFIG_SND_SOC_ES_SPI_STREAMDEV)
	escore_priv.streamdev = es_spi_streamdev;
#endif
	return rc;

event_intr_error:
	if (pdata->gpioa_gpio != -1)
		free_irq(gpio_to_irq(pdata->gpioa_gpio), NULL);
api_intr_error:
#if defined(CONFIG_SND_SOC_ES_VS)
	escore_vs_release_bkg(&escore_priv);
vs_request_bkg_err:
	escore_vs_release_firmware(&escore_priv);
request_vs_firmware_error:
#endif
	release_firmware(escore_priv.standard);
request_firmware_error:
exit:
	return rc;
}
EXPORT_SYMBOL_GPL(es755_core_probe);

static __init int es755_init(void)
{
	int rc = 0;

	pr_debug("%s()", __func__);

	escore_priv.codec_clk_en = false;
	/* Initially power level support is disabled */
	escore_priv.power_saving_mode = ES_POWER_LEVEL_DISABLE;
	escore_priv.device_name  = "elemental-addr";
	escore_priv.interface_device_name  = "slim-ifd";
	escore_priv.interface_device_elem_addr_name  =
					"slim-ifd-elemental-addr";
	mutex_init(&escore_priv.access_lock);
	mutex_init(&escore_priv.intf_probed_mutex);
	init_completion(&escore_priv.fw_download);
	escore_platform_init();
#if defined(CONFIG_SND_SOC_ES_I2C)
	escore_priv.pri_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_SLIM)
	escore_priv.pri_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_UART)
	escore_priv.pri_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_priv.pri_intf = ES_SPI_INTF;
#endif

#if defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	escore_priv.high_bw_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	escore_priv.high_bw_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	escore_priv.high_bw_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	escore_priv.high_bw_intf = ES_SPI_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_DEFAULT)
	escore_priv.high_bw_intf = escore_priv.pri_intf;
#endif

#if defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
	escore_priv.wakeup_intf = ES_UART_INTF;
#endif

#if defined(CONFIG_SND_SOC_ES_I2C) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	rc = escore_i2c_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	rc = escore_slimbus_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SPI) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	rc = escore_spi_init();
#endif
#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART) || \
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
	rc = escore_uart_bus_init(&escore_priv);
#endif
	if (rc) {
		pr_debug("Error registering Audience eS755 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if defined(CONFIG_SND_SOC_ES_CDEV)
	rc = escore_cdev_init(&escore_priv);
	if (rc) {
		pr_debug("Error enabling CDEV interface: %d\n", rc);
		goto INIT_ERR;
	}
#endif
INIT_ERR:
	return rc;
}
module_init(es755_init);

static __exit void es755_exit(void)
{
	pr_debug("%s()\n", __func__);
	if (escore_priv.flag.af)
		release_firmware(escore_priv.af);

	if (escore_priv.power_transition_wq)
		destroy_workqueue(escore_priv.power_transition_wq);

	wake_lock_destroy(&escore_priv.wake_lock);

#if defined(CONFIG_SND_SOC_ES_VS)
	escore_vs_release_firmware(&escore_priv);
	escore_vs_exit(&escore_priv);
#endif

#if defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&escore_i2c_driver);
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_spi_exit();
#endif
}
module_exit(es755_exit);


MODULE_DESCRIPTION("ASoC ES755 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es755-codec");
#if defined(CONFIG_SND_SOC_ES854)
MODULE_FIRMWARE("audience-es854-fw.bin");
#elif defined(CONFIG_SND_SOC_ES857)
MODULE_FIRMWARE("audience-es857-fw.bin");
#else
MODULE_FIRMWARE("audience-es755-fw.bin");
#endif
