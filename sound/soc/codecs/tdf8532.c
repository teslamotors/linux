/*
 * tdf8532.c  --  driver for NXP Semiconductors TDF8532
 *
 * Copyright (C) 2016 Intel Corp.
 * Author: Steffen Wagner <steffen.wagner@intel.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

struct tdf8532_priv {
	struct i2c_client *i2c;

	/* Fine to wrap back to 0 */
	u8 packet_id;
};

static int tdf8532_cmd_send(struct tdf8532_priv *dev_data, char *cmd_packet,
		u16 len)
{
	int ret;
	u8 packet_id;

	packet_id = (len > 1) ? cmd_packet[1] : 0;

	ret = i2c_master_send(dev_data->i2c, cmd_packet, len);

	if (ret < 0) {
		dev_err(&(dev_data->i2c->dev),
			"i2c send packet(%u) returned: %d\n", packet_id, ret);
		return ret;
	}


	dev_dbg(&(dev_data->i2c->dev),
			"i2c send packet(%u) returned: %d\n", packet_id, ret);

	return 0;
}

static void tdf8532_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tdf8532_priv *tdf8532 = snd_soc_codec_get_drvdata(codec);

	/*disconnect clock*/
	unsigned char data[] = {0x02, (tdf8532->packet_id)++, 0x03,
					0x80, 0x1A, 0x00};

	dev_dbg(codec->dev, "%s\n", __func__);

	tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
}

static int tdf8532_dai_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct tdf8532_priv *tdf8532 = snd_soc_codec_get_drvdata(codec);

	/*enable or disable 4 channels*/
	unsigned char data[] = {0x02, (tdf8532->packet_id)++, 0x03,
					0x80, 0x00, 0x0F};

	dev_dbg(codec->dev, "%s: cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		data[4] = 0x26;
		ret = tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		/* WA on unexpected codec down during S3
		 SNDRV_PCM_TRIGGER_STOP fails so skip set ret */
		tdf8532_stop_play(tdf8532);
		data[4] = 0x27;
		ret = tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));

		/*delay 300ms to allow state change to occur*/
		/*TODO: add state check to wait for state change*/
		mdelay(300);
		break;
	}

	return tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
}

static int tdf8532_dai_prepare(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tdf8532_priv *tdf8532 = snd_soc_codec_get_drvdata(codec);

	/*attach clock*/
	unsigned char data[] = {0x02, (tdf8532->packet_id)++, 0x03,
					0x80, 0x1A, 0x01};

	dev_dbg(codec->dev, "%s\n", __func__);

	return tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
}


#define MUTE 0x42
#define UNMUTE 0x43

static int tdf8532_mute(struct snd_soc_dai *dai, int mute)
{
	struct tdf8532_priv *tdf8532 = snd_soc_codec_get_drvdata(dai->codec);
	unsigned char data[] = {0x02, (tdf8532->packet_id)++, 0x03,
					0x80, MUTE, 0x1F};

	if (!mute)
		data[4] = UNMUTE;
	else
		data[4] = MUTE;

	dev_dbg(&(tdf8532->i2c->dev), "%s\n", __func__);

	return tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
}

static int tdf8532_set_fast_mute(struct tdf8532_priv *tdf8532)
{
	unsigned char data[] = {
		0x02, (tdf8532->packet_id)++, 0x06,
			0x80, 0x18, 0x03, 0x01, 0x02, 0x00};

	return tdf8532_cmd_send(tdf8532, data, ARRAY_SIZE(data));
}

static const struct snd_soc_dai_ops tdf8532_dai_ops = {
	.shutdown = tdf8532_dai_shutdown,
	.trigger  = tdf8532_dai_trigger,
	.prepare  = tdf8532_dai_prepare,
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
	int ret;
	struct tdf8532_priv *tdf8532;

	dev_dbg(&i2c->dev, "%s\n", __func__);

	tdf8532 = devm_kzalloc(&i2c->dev, sizeof(*tdf8532),
			GFP_KERNEL);

	if (NULL == tdf8532)
		return -ENOMEM;

	tdf8532->i2c = i2c;
	tdf8532->packet_id = 0;

	i2c_set_clientdata(i2c, tdf8532);

	ret = tdf8532_set_fast_mute(tdf8532);

	if (ret < 0)
		dev_err(&i2c->dev, "Failed to set fast mute option: %d\n", ret);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_tdf8532,
			tdf8532_dai, ARRAY_SIZE(tdf8532_dai));
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);

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
MODULE_LICENSE("GPL");
