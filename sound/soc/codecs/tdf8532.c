/*
 * tdf8532.c  --  driver for NXP Semiconductors TDF8532
 *
 * Copyright (C) 2017 Intel Corp.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>
#include "tdf8532.h"

/* forward declarations */
static int tdf8532_check_dev_error(struct tdf8532_priv *dev_data);

static int __tdf8532_build_pkt(struct tdf8532_priv *dev_data, va_list valist,
					u8 *payload)
{
	const u8 cmd_offset = 3;
	u8 *cmd_payload;
	int param;
	u8 len;

	payload[HEADER_TYPE] = MSG_TYPE_STX;
	payload[HEADER_PKTID] = dev_data->pkt_id;

	cmd_payload = &(payload[cmd_offset]);

	param = va_arg(valist, int);
	len = 0;

	while (param != END) {
		cmd_payload[len] = param;
		len++;
		param = va_arg(valist, int);
	}

	payload[HEADER_LEN] = len;

	return len + cmd_offset;
}

/* Use macro instead */
static int __tdf8532_single_write(struct tdf8532_priv *dev_data, int check_err,
					int dummy, ...)
{
	struct device *dev = &(dev_data->i2c->dev);
	u8 payload[255];
	va_list valist;
	u8 len;
	int ret = 0;

	va_start(valist, dummy);

	len = __tdf8532_build_pkt(dev_data, valist, payload);

	va_end(valist);

	print_hex_dump_debug("tdf8532-codec: Tx:", DUMP_PREFIX_NONE, 32, 1,
				payload, len, false);
	ret = i2c_master_send(dev_data->i2c, payload, len);

	if (ret < 0)
		dev_err(dev, "i2c send packet returned: %d\n", ret);

	mdelay(2);

	if (check_err)
		ret = tdf8532_check_dev_error(dev_data);

	dev_data->pkt_id++;

	return ret;
}

static uint8_t tdf8532_read_wait_ack(struct tdf8532_priv *dev_data,
						unsigned long timeout_val)
{
	unsigned long timeout = jiffies + timeout_val;
	uint8_t ack_repl[HEADER_SIZE] = {0, 0, 0};
	int ret;

	do {
		ret = i2c_master_recv(dev_data->i2c, ack_repl, HEADER_SIZE);
		if (ret < 0)
			goto out;

	} while (time_before(jiffies, timeout) && ack_repl[0] != MSG_TYPE_ACK);

	if (ack_repl[0] != MSG_TYPE_ACK)
		return -ETIME;
	else
		return ack_repl[2];

out:
	return ret;
}

static uint8_t tdf8532_single_read(struct tdf8532_priv *dev_data,
					char **repl_buff)
{
	struct device *dev = &(dev_data->i2c->dev);
	uint8_t recv_len;
	int ret;

	ret = tdf8532_read_wait_ack(dev_data, msecs_to_jiffies(ACK_TIMEOUT));

	if (ret < 0) {
		dev_err(dev, "Error waiting for ACK reply: %d\n", ret);
		goto out;
	}

	recv_len = ret + HEADER_SIZE;
	*repl_buff = kzalloc(recv_len, GFP_KERNEL);

	ret = i2c_master_recv(dev_data->i2c, *repl_buff, recv_len);

	if (ret < 0)
		goto out_free;

	print_hex_dump_debug("tdf8532-codec: Rx:", DUMP_PREFIX_NONE, 32, 1,
				*repl_buff, recv_len, false);

	if (ret != recv_len) {
		ret = -EIO;
		dev_err(dev, "i2c recv packet size: %d (expected: %d)\n",
				ret, recv_len);
		goto out_free;
	}

	mdelay(1);
	return recv_len;

out_free:
	kfree(*repl_buff);
	repl_buff = NULL;
out:
	return ret;
}

static int tdf8532_get_dev_info(struct tdf8532_priv *dev_data)
{
	struct get_ident_repl *id;
	char *repl_buff = NULL;
	int ret;

	ret = tdf8532_amp_write(dev_data, GET_IDENT);
	if (ret < 0)
		return ret;

	ret = tdf8532_single_read(dev_data, &repl_buff);
	if (ret < 0)
		return ret;

	id = (struct get_ident_repl *)repl_buff;

	dev_data->sw_major = id->sw_major;
	kfree(repl_buff);

	return 0;
}

static int tdf8532_get_state(struct tdf8532_priv *dev_data, u8 *state)
{
	char *repl_buff = NULL;
	int ret = 0;

	ret = tdf8532_amp_write(dev_data, GET_DEV_STATUS);
	if (ret < 0)
		goto out;

	ret = tdf8532_single_read(dev_data, &repl_buff);
	if (ret < 0)
		goto out;

	*state = ((struct get_dev_status_repl *)repl_buff)->state;

	kfree(repl_buff);
out:
	return ret;
}

static int tdf8532_check_dev_error(struct tdf8532_priv *dev_data)
{
	struct device *dev = &(dev_data->i2c->dev);
	char *repl_buff = NULL;
	int ret = 0;
	u8 error;

	ret = tdf8532_amp_write(dev_data, GET_ERROR);
	if (ret < 0)
		goto out;

	ret = tdf8532_single_read(dev_data, &repl_buff);
	if (ret < 0)
		goto out;

	error = ((struct get_error_repl *)repl_buff)->error;

	if (error) {
		dev_err(dev, "%s: 0x%X\n", __func__, error);
		ret = -EIO;
	}

	kfree(repl_buff);
out:
	return ret;
}

static int __tdf8532_wait_state(struct tdf8532_priv *dev_data, u8 req_state,
					unsigned long timeout_val, u8 or_higher)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_val);
	struct device *dev = &(dev_data->i2c->dev);
	u8 state;
	int ret;

	do {
		ret = tdf8532_get_state(dev_data, &state);
		if (ret < 0)
			goto out;

	} while (time_before(jiffies, timeout) && state != req_state);

	if (or_higher) {
		if (state < req_state)
			goto out_timeout;
	} else {
		if (state != req_state)
			goto out_timeout;
	}

out:
	return ret;
out_timeout:
	ret = -ETIME;
	dev_err(dev, "State: %u, req_state: %u, ret: %d\n", state,
			req_state, ret);
	return ret;
}

static int tdf8532_start_play(struct tdf8532_priv *tdf8532)
{
	int ret;

	ret = tdf8532_amp_write_check_err(tdf8532, SET_CHNL_ENABLE,
			CHNL_MASK(tdf8532->channels));

	if (ret < 0)
		return ret;

	ret = tdf8532_amp_write_check_err(tdf8532, SET_CLK_STATE, CLK_CONNECT);
	if (ret < 0)
		return ret;

	ret = tdf8532_wait_state_or_higher(tdf8532, STATE_PLAY,
				ACK_TIMEOUT);

	return ret;
}


static int tdf8532_stop_play(struct tdf8532_priv *tdf8532)
{
	int ret;

	ret = tdf8532_amp_write_check_err(tdf8532, SET_CHNL_DISABLE,
			CHNL_MASK(tdf8532->channels));
	if (ret < 0)
		goto out;

	ret = tdf8532_wait_state(tdf8532, STATE_STBY, ACK_TIMEOUT);
	if (ret < 0)
		goto out;

	ret = tdf8532_amp_write_check_err(tdf8532, SET_CLK_STATE,
						CLK_DISCONNECT);
	if (ret < 0)
		goto out;

	ret = tdf8532_wait_state(tdf8532, STATE_IDLE, ACK_TIMEOUT);

out:
	return ret;
}


static int tdf8532_dai_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tdf8532_priv *tdf8532;
	int ret = 0;

	tdf8532 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = tdf8532_start_play(tdf8532);
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = tdf8532_stop_play(tdf8532);
		break;
	}

	return ret;
}

static int tdf8532_mute(struct snd_soc_dai *dai, int mute)
{
	struct tdf8532_priv *tdf8532 = snd_soc_codec_get_drvdata(dai->codec);
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	if (mute)
		return tdf8532_amp_write(tdf8532, SET_CHNL_MUTE,
						CHNL_MASK(CHNL_MAX));
	else
		return tdf8532_amp_write(tdf8532, SET_CHNL_UNMUTE,
						CHNL_MASK(CHNL_MAX));
}

static const struct snd_soc_dai_ops tdf8532_dai_ops = {
	.trigger  = tdf8532_dai_trigger,
	.digital_mute = tdf8532_mute,
};

static struct snd_soc_codec_driver soc_codec_tdf8532;

static struct snd_soc_dai_driver tdf8532_dai[] = {
	{
		.name = "tdf8532-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 4,
			.channels_max = 4,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tdf8532_dai_ops,
	}
};

static int tdf8532_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct device *dev = &(i2c->dev);
	struct tdf8532_priv *dev_data;
	int ret;

	dev_dbg(&i2c->dev, "%s\n", __func__);

	dev_data = devm_kzalloc(dev, sizeof(struct tdf8532_priv), GFP_KERNEL);

	if (!dev_data) {
		ret = -ENOMEM;
		goto out;
	}

	dev_data->i2c = i2c;
	dev_data->pkt_id = 0;
	dev_data->channels = 4;

	i2c_set_clientdata(i2c, dev_data);

	ret = tdf8532_get_dev_info(dev_data);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device info: %d\n", ret);
		goto out;
	}

	dev_dbg(&i2c->dev, "%s: sw_major: %u\n", __func__, dev_data->sw_major);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_tdf8532,
					tdf8532_dai, ARRAY_SIZE(tdf8532_dai));
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static int tdf8532_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);

	return 0;
}

static const struct i2c_device_id tdf8532_i2c_id[] = {
	{ "tdf8532", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tdf8532_i2c_id);

#if CONFIG_ACPI
static const struct acpi_device_id tdf8532_acpi_match[] = {
	{"INT34C3", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, tdf8532_acpi_match);
#endif

static struct i2c_driver tdf8532_i2c_driver = {
	.driver = {
		.name = "tdf8532-codec",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(tdf8532_acpi_match),
	},
	.probe =    tdf8532_i2c_probe,
	.remove =   tdf8532_i2c_remove,
	.id_table = tdf8532_i2c_id,
};

module_i2c_driver(tdf8532_i2c_driver);

MODULE_DESCRIPTION("ASoC NXP Semiconductors TDF8532 driver");
MODULE_AUTHOR("Steffen Wagner <steffen.wagner@intel.com>");
MODULE_AUTHOR("Craig Kewley <craigx.kewley@intel.com>");
MODULE_LICENSE("GPL");
