/*
 * tda7802.c  --  codec driver for ST TDA7802
 *
 * Copyright (C) 2016 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#if IS_ENABLED(CONFIG_SND_SOC_INTEL_SKYLAKE_MODEL3)

#define MODEL3 1
#define MODELSX 0

#define AMP1_ADDR			   0x6e
#define AMP2_ADDR			   0x6f

#elif IS_ENABLED(CONFIG_SND_SOC_INTEL_SKYLAKE_MODELSX_INFO2)

#define MODEL3 0
#define MODELSX 1

#define AMP1_ADDR			   0x6c
#define AMP2_ADDR			   0x6d
#define AMP3_ADDR			   0x6e
#define AMP4_ADDR			   0x6f

#else

#error "tda7802.c must be built for the Model 3 or Model S/X Info2 Platform"

#endif

#define I2C_BUS				   4

#define CHANNELS_PER_AMP		   4
#define AMPS_PER_DAI			   2

#define TDA7802_IB0			   0x00
#define TDA7802_IB1			   0x01
#define TDA7802_IB2			   0x02
#define TDA7802_IB3			   0x03
#define TDA7802_IB4			   0x04
#define TDA7802_IB5			   0x05

#define TDA7802_DB0			   0x10
#define TDA7802_DB1			   0x11
#define TDA7802_DB2			   0x12
#define TDA7802_DB3			   0x13
#define TDA7802_DB4			   0x14
#define TDA7802_DB5			   0x15

#define IB0_CH4_TRISTATE_ON		   (1 << 7)
#define IB0_CH3_TRISTATE_ON		   (1 << 6)
#define IB0_CH2_TRISTATE_ON		   (1 << 5)
#define IB0_CH1_TRISTATE_ON		   (1 << 4)
#define IB0_CH4_HIGH_EFF		   (0 << 3)
#define IB0_CH4_CLASS_AB		   (1 << 3)
#define IB0_CH3_HIGH_EFF		   (0 << 2)
#define IB0_CH3_CLASS_AB		   (1 << 2)
#define IB0_CH2_HIGH_EFF		   (0 << 0)
#define IB0_CH2_CLASS_AB		   (1 << 1)
#define IB0_CH1_HIGH_EFF		   (0 << 0)
#define IB0_CH1_CLASS_AB		   (1 << 0)

#define IB1_REAR_IMPEDANCE_2_OHM	   (0 << 7)
#define IB1_REAR_IMPEDANCE_4_OHM	   (1 << 7)
#define IB1_LONG_DIAG_TIMING_x1		   (0 << 5)
#define IB1_LONG_DIAG_TIMING_x2		   (1 << 5)
#define IB1_LONG_DIAG_TIMING_x4		   (2 << 5)
#define IB1_LONG_DIAG_TIMING_x8		   (3 << 5)
#define IB1_GAIN_CH13_GV1		   (0 << 3)
#define IB1_GAIN_CH13_GV2		   (1 << 3)
#define IB1_GAIN_CH13_GV3		   (2 << 3)
#define IB1_GAIN_CH13_GV4		   (3 << 3)
#define IB1_GAIN_CH24_GV1		   (0 << 1)
#define IB1_GAIN_CH24_GV2		   (1 << 1)
#define IB1_GAIN_CH24_GV3		   (2 << 1)
#define IB1_GAIN_CH24_GV4		   (3 << 1)
#define IB1_DIGITAL_GAIN_BOOST		   (1 << 0)

#define IB2_MUTE_TIME_1_MS		   (0 << 5)
#define IB2_MUTE_TIME_5_MS		   (1 << 5)
#define IB2_MUTE_TIME_11_MS		   (2 << 5)
#define IB2_MUTE_TIME_23_MS		   (3 << 5)
#define IB2_MUTE_TIME_46_MS		   (4 << 5)
#define IB2_MUTE_TIME_92_MS		   (5 << 5)
#define IB2_MUTE_TIME_185_MS		   (6 << 5)
#define IB2_MUTE_TIME_371_MS		   (7 << 5)
#define IB2_CH13_UNMUTED		   (1 << 4)
#define IB2_CH24_UNMUTED		   (1 << 3)
#define IB2_DIGITAL_MUTE_DISABLED	   (1 << 2)
#define IB2_AUTOMUTE_THRESHOLD_5V3	   (0 << 1)
#define IB2_AUTOMUTE_THRESHOLD_7V3	   (1 << 1)
#define IB2_FRONT_IMPEDANCE_2_OHM	   (0 << 0)
#define IB2_FRONT_IMPEDANCE_4_OHM	   (1 << 0)

#define IB3_SAMPLE_RATE_44_KHZ		   (0 << 6)
#define IB3_SAMPLE_RATE_48_KHZ		   (1 << 6)
#define IB3_SAMPLE_RATE_96_KHZ		   (2 << 6)
#define IB3_SAMPLE_RATE_192_KHZ		   (3 << 6)
#define IB3_FORMAT_I2S			   (0 << 3)
#define IB3_FORMAT_TDM4			   (1 << 3)
#define IB3_FORMAT_TDM8_DEV1		   (2 << 3)
#define IB3_FORMAT_TDM8_DEV2		   (3 << 3)
#define IB3_FORMAT_TDM16_DEV1		   (4 << 3)
#define IB3_FORMAT_TDM16_DEV2		   (5 << 3)
#define IB3_FORMAT_TDM16_DEV3		   (6 << 3)
#define IB3_FORMAT_TDM16_DEV4		   (7 << 3)
#define IB3_I2S_FRAME_PERIOD_64		   (0 << 2)
#define IB3_I2S_FRAME_PERIOD_50		   (1 << 2)
#define IB3_PLL_CLOCK_DITHER_ENABLE	   (1 << 1)
#define IB3_HIGH_PASS_FILTER_ENABLE	   (1 << 0)

#define IB4_NOISE_GATE_DISABLE		   (1 << 7)
#define IB4_SHORT_FAULT_ON_CDDIAG_YES	   (0 << 6)
#define IB4_SHORT_FAULT_ON_CDDIAG_NO	   (1 << 6)
#define IB4_OFFSET_ON_CDDIAG_YES	   (0 << 5)
#define IB4_OFFSET_ON_CDDIAG_NO		   (1 << 5)
#define IB4_AC_DIAG_CURRENT_THRESHOLD_HIGH (0 << 4)
#define IB4_AC_DIAG_CURRENT_THRESHOLD_LOW  (1 << 4)
#define IB4_AC_DIAG_ENABLE		   (1 << 3)
#define IB4_CH13_DIAG_MODE_SPEAKER	   (0 << 2)
#define IB4_CH13_DIAG_MODE_BOOSTER	   (1 << 2)
#define IB4_CH24_DIAG_MODE_SPEAKER	   (0 << 1)
#define IB4_CH24_DIAG_MODE_BOOSTER	   (1 << 1)
#define IB4_DIAG_MODE_ENABLE		   (1 << 0)

#define IB5_TEMP_WARNING_ON_CDDIAG_TW1	   (0 << 5)
#define IB5_TEMP_WARNING_ON_CDDIAG_TW2	   (1 << 5)
#define IB5_TEMP_WARNING_ON_CDDIAG_TW3	   (2 << 5)
#define IB5_TEMP_WARNING_ON_CDDIAG_TW4	   (3 << 5)
#define IB5_TEMP_WARNING_ON_CDDIAG_NONE	   (4 << 5)
#define IB5_CLIP_DETECT_FRONT_1PC	   (0 << 3)
#define IB5_CLIP_DETECT_FRONT_5PC	   (1 << 3)
#define IB5_CLIP_DETECT_FRONT_10PC	   (2 << 3)
#define IB5_CLIP_DETECT_FRONT_NONE	   (3 << 3)
#define IB5_CLIP_DETECT_REAR_1PC	   (0 << 1)
#define IB5_CLIP_DETECT_REAR_5PC	   (1 << 1)
#define IB5_CLIP_DETECT_REAR_10PC	   (2 << 1)
#define IB5_CLIP_DETECT_REAR_NONE	   (3 << 1)
#define IB5_AMPLIFIER_ON		   (1 << 0)

#define DB0_STARTUP_DIAG_STATUS		   0x40

struct tda7802_priv {
	struct mutex mutex;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct kobject *kobj;
	unsigned int channel_status[CHANNELS_PER_AMP];
};

static inline int tda7802_is_primary_amp_addr(unsigned short addr)
{
	return (addr % 2) == 0;
}

static bool tda7802_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TDA7802_IB0 ... TDA7802_IB5:
	case TDA7802_DB0 ... TDA7802_DB5:
		return true;
	default:
		return false;
	}
}

static bool tda7802_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TDA7802_IB0 ... TDA7802_IB5:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config tda7802_regmap_config = {
	.val_bits      = 8,
	.reg_bits      = 8,
	.max_register  = TDA7802_DB5,
	.use_single_rw = 1,

	.readable_reg  = tda7802_readable_reg,
	.writeable_reg = tda7802_writeable_reg,
};

static int tda7802_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tda7802_priv *tda7802s = snd_soc_codec_get_drvdata(codec);
	int err;
	int i;

	/* TODO: Probably want to explicitly disable the amplifier
	   before trying to configure it here in case it's already
	   enabled. Otherwise we could get pops.  */

	/* The two amps are configured the same, except for the
	   register that determines which slots of the TDM8 stream
	   they listen to. */

	unsigned char data[] = {
		/* All channels out of tri-state mode, all channels in
		   Standard Class AB mode (not High-efficiency) */
		IB0_CH4_CLASS_AB | IB0_CH3_CLASS_AB |
		IB0_CH2_CLASS_AB | IB0_CH1_CLASS_AB,

		/* Rear channel load impedance set to 2-Ohm, default
		   diagnostic timing, GV1 gain on all channels
		   (default), no digital gain increase */
		IB1_REAR_IMPEDANCE_2_OHM | IB1_LONG_DIAG_TIMING_x1 |
			(MODELSX ? IB1_GAIN_CH13_GV4 : IB1_GAIN_CH13_GV1) |
			IB1_GAIN_CH24_GV1,

		/* Mute timing 1.45ms, all channels un-muted, digital
		   mute enabled, 5.3V undervoltage threshold,
		   front-channel load impedance set to 2-Ohms */
		IB2_MUTE_TIME_1_MS | IB2_CH13_UNMUTED | IB2_CH24_UNMUTED |
		IB2_AUTOMUTE_THRESHOLD_5V3 | IB2_FRONT_IMPEDANCE_2_OHM,

		/* 48kHz sample rate, TDM8 dev1 configuration, 64-bit
		   I2S frame period, PLL clock dither disabled,
		   high-pass filter enabled (blocks DC output) */
		IB3_SAMPLE_RATE_48_KHZ | IB3_FORMAT_TDM8_DEV1 |
		IB3_I2S_FRAME_PERIOD_64 | IB3_HIGH_PASS_FILTER_ENABLE,

		/* Noise gating enabled, short and offset info on
		   CD-Diag (fault) pin, diagnostics disabled
		   On Model S/X Info2, all four amps drive line-outs (or
		   nothing) from channels 1&3, and speakers from channels 2&4.
		   On Model 3, both amps drive speakers from all four channels.
		   */
		(MODELSX ? IB4_CH13_DIAG_MODE_BOOSTER :
			IB4_CH13_DIAG_MODE_SPEAKER) |
			IB4_CH24_DIAG_MODE_SPEAKER,

		/* Temperature warning on diag pin set to TW1 (highest
		   setting), clip detection set to 1% on all channels,
		   amplifier ON */
		IB5_TEMP_WARNING_ON_CDDIAG_TW1 | IB5_CLIP_DETECT_FRONT_1PC |
		IB5_CLIP_DETECT_REAR_1PC | IB5_AMPLIFIER_ON,
	};

	dev_dbg(codec->dev, "%s\n", __func__);

	for (i = 0; i < AMPS_PER_DAI; ++i) {
		/* 48kHz sample rate, TDM8 dev1 or dev2 configuration,
		 * 64-bit I2S frame period, PLL clock dither disabled,
		 * high-pass filter enabled (blocks DC output) */
		data[TDA7802_IB3] = IB3_SAMPLE_RATE_48_KHZ |
			(tda7802_is_primary_amp_addr(tda7802s[i].i2c->addr) ?
			 IB3_FORMAT_TDM8_DEV1 : IB3_FORMAT_TDM8_DEV2) |
			IB3_I2S_FRAME_PERIOD_64 | IB3_HIGH_PASS_FILTER_ENABLE;
		err = regmap_bulk_write(tda7802s[i].regmap, TDA7802_IB0, data,
				ARRAY_SIZE(data));
		if (err) {
			dev_err(codec->dev, "Cannot configure amp%d %d\n", i,
					err);
			return err;
		}
	}

	return 0;
}

static void tda7802_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tda7802_priv *tda7802s = snd_soc_codec_get_drvdata(codec);
	int err;
	int i;

	dev_dbg(codec->dev, "%s\n", __func__);

	/* Turn the amplifiers off. */

	for (i = 0; i < AMPS_PER_DAI; ++i) {
		err = regmap_write(tda7802s[i].regmap, TDA7802_IB5, 0x00);
		if (err) {
			dev_err(codec->dev, "Cannot disable amp%d %d\n", i,
					err);
		}
	}
}

static int tda7802_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tda7802_priv *tda7802s = snd_soc_codec_get_drvdata(codec);
	int val = mute ? 0 : IB2_DIGITAL_MUTE_DISABLED;
	int err;
	int i;

	dev_dbg(codec->dev, "%s mute=%d\n", __func__, mute);

	for (i = 0; i < AMPS_PER_DAI; ++i) {
		err = regmap_update_bits(tda7802s[i].regmap, TDA7802_IB2,
				IB2_DIGITAL_MUTE_DISABLED, val);
		if (err) {
			dev_err(codec->dev, "Cannot %smute amp%d %d\n",
					mute ? "" : "un", i, err);
			return err;
		}
	}

	return 0;
}

static const struct snd_soc_dai_ops tda7802_dai_ops = {
	.startup      = tda7802_dai_startup,
	.shutdown     = tda7802_dai_shutdown,
	.digital_mute = tda7802_digital_mute,
};

static struct snd_soc_codec_driver soc_codec_tda7802;

#if (MODEL3)

static struct snd_soc_dai_driver tda7802_base_amp_dai = {
	.name = "tda7802-oct",
	.playback = {
		.stream_name  = "Base Amp",
		.channels_min = 8,
		.channels_max = 8,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

#elif (MODELSX)

static struct snd_soc_dai_driver tda7802_amp_m_dai = {
	.name = "tda7802-oct-m",
	.playback = {
		.stream_name  = "AmpM Codec",
		.channels_min = 8,
		.channels_max = 8,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

static struct snd_soc_dai_driver tda7802_amp_s_dai = {
	.name = "tda7802-oct-s",
	.playback = {
		.stream_name  = "AmpS Codec",
		.channels_min = 8,
		.channels_max = 8,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

#endif

/* The speaker test is triggered by reading a sysfs attribute file attached to
 * the I2C device. The user space thread that's doing the reading is blocked
 * until the test completes (or times out). We only permit one test to be in
 * progress at a time.
 */

/* We poll the test completion bit, letting the current thread sleep
 * until we're done. These values are not critical. */
#define SPEAKER_TEST_DONE_POLL_PERIOD_MS 5
#define SPEAKER_TEST_DONE_TIMEOUT_MS	 5000

static int wait_reg(struct regmap *regmap, unsigned int reg,
		    uint8_t mask, uint8_t val)
{
	unsigned int data;
	int t = 0;

	while (1) {
		int err;

		err = regmap_read(regmap, reg, &data);
		if (err)
			return err;

		if ((data & mask) == val)
			return 0; /* Found what we're looking for. */

		msleep(SPEAKER_TEST_DONE_POLL_PERIOD_MS);
		t += SPEAKER_TEST_DONE_POLL_PERIOD_MS;
		if (t >= SPEAKER_TEST_DONE_TIMEOUT_MS)
			return -ENODATA;
	}
}

static int speaker_test_start(struct regmap *regmap)
{
	int err;

	err = regmap_update_bits(regmap, TDA7802_IB5, IB5_AMPLIFIER_ON, 0);
	if (err) {
		dev_err(regmap_get_device(regmap),
			"Cannot disable amp for speaker test (%d)\n", err);
		return err;
	}

	err = regmap_update_bits(regmap, TDA7802_IB4, IB4_DIAG_MODE_ENABLE, 0);
	if (err) {
		dev_err(regmap_get_device(regmap),
			"Cannot disable diag mode before speaker test (%d)\n",
			err);
		return err;
	}

	/* Seem to need at least a 15 ms delay here before the rising
	   edge. Otherwise the diagnostics never complete (perhaps
	   they never start). */
	msleep(20);

	err = regmap_update_bits(regmap, TDA7802_IB4,
				 IB4_DIAG_MODE_ENABLE, IB4_DIAG_MODE_ENABLE);
	if (err) {
		dev_err(regmap_get_device(regmap),
			"Cannot enable diag mode for speaker test (%d)\n", err);
		return err;
	}

	return 0;
}

static int speaker_test_stop(struct regmap *regmap)
{
	int err;

	err = regmap_update_bits(regmap, TDA7802_IB4, IB4_DIAG_MODE_ENABLE, 0);
	if (err) {
		dev_err(regmap_get_device(regmap),
			"Cannot disable diag mode after speaker test (%d)\n",
			err);
		return err;
	}

	err = regmap_update_bits(regmap, TDA7802_IB5,
				 IB5_AMPLIFIER_ON, IB5_AMPLIFIER_ON);
	if (err)
		dev_err(regmap_get_device(regmap),
			"Cannot re-enable amp after speaker test (%d)\n", err);

	return err;
}

static int speaker_test_check(struct regmap *regmap, unsigned int *status)
{
	int err;
	int i;

	for (i = 0; i < CHANNELS_PER_AMP; ++i)
		status[i] = 0xff;

	err = speaker_test_start(regmap);
	if (err)
		return err;

	err = wait_reg(regmap, TDA7802_DB0,
		       DB0_STARTUP_DIAG_STATUS, DB0_STARTUP_DIAG_STATUS);
	if (err) {
		int stop_err;

		if (err == -ENODATA)
			dev_err(regmap_get_device(regmap),
				"Speaker diagnostic test did not complete\n");
		stop_err = speaker_test_stop(regmap);
		if (stop_err)
			return stop_err;
		return err;
	}

	err = speaker_test_stop(regmap);
	if (err)
		return err;

	for (i = 0; i < CHANNELS_PER_AMP; ++i) {
		err = regmap_read(regmap, TDA7802_DB1 + i, status + i);
		if (err) {
			dev_err(regmap_get_device(regmap),
				"Cannot get speaker %d status (%d)\n", i, err);
			return err;
		}
	}

	return 0;
}

static ssize_t status_show(struct kobject *kobj,
			   struct kobj_attribute *attr,
			   char *buf)
{
	struct tda7802_priv *tda7802;
	ssize_t n = 0;
	int err;

	if (!kobj || !kobj->parent)
		return -ENODEV;

	tda7802 = i2c_get_clientdata(kobj_to_i2c_client(kobj->parent));

	mutex_lock(&tda7802->mutex);

	err = speaker_test_check(tda7802->regmap, tda7802->channel_status);

	if (!err) {
		n = sprintf(buf, "%02x %02x %02x %02x\n",
				tda7802->channel_status[0],
				tda7802->channel_status[1],
				tda7802->channel_status[2],
				tda7802->channel_status[3]);
	}

	mutex_unlock(&tda7802->mutex);

	if (err)
		return err;

	return n;
}

static struct kobj_attribute speaker_test_status_attribute = __ATTR_RO(status);

static void tda7802_cleanup(struct tda7802_priv *tda7802)
{
	if (tda7802->kobj) {
		sysfs_remove_file(tda7802->kobj,
				&speaker_test_status_attribute.attr);
		kobject_put(tda7802->kobj);
	}
	mutex_destroy(&tda7802->mutex);
}

static int tda7802_init(struct tda7802_priv *tda7802, struct i2c_client *i2c)
{
	int err;

	mutex_init(&tda7802->mutex);

	tda7802->i2c = i2c;
	tda7802->regmap = devm_regmap_init_i2c(tda7802->i2c,
			&tda7802_regmap_config);
	if (IS_ERR(tda7802->regmap)) {
		err = PTR_ERR(tda7802->regmap);
		goto cleanup;
	}

	/* TODO: Do an i2c read to check that the device actually exists, and
	 * return -ENODEV if not. */

	tda7802->kobj = kobject_create_and_add("speaker-test",
			&tda7802->i2c->dev.kobj);
	if (!tda7802->kobj) {
		err = -ENOMEM;
		goto cleanup;
	}

	err = sysfs_create_file(tda7802->kobj,
			&speaker_test_status_attribute.attr);
	if (err)
		goto cleanup;

	i2c_set_clientdata(i2c, tda7802);

	return 0;

cleanup:
	tda7802_cleanup(tda7802);
	return err;
}

static int tda7802_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct tda7802_priv *tda7802s = 0;
	struct i2c_client *dummy = 0;
	int err;
	int i;

	dev_dbg(&i2c->dev, "%s addr=0x%02hx\n", __func__, i2c->addr);

	if (!tda7802_is_primary_amp_addr(i2c->addr)) {
		dev_err(&i2c->dev,
				"Cannot probe secondary amp addr=0x%02hx\n",
				i2c->addr);
		err = -ENODEV;
		goto cleanup;
	}

	tda7802s = devm_kmalloc_array(&i2c->dev, AMPS_PER_DAI,
			sizeof(*tda7802s), GFP_KERNEL);

	if (!tda7802s) {
		err = -ENOMEM;
		goto cleanup;
	}

	err = tda7802_init(&tda7802s[0], i2c);
	if (err)
		goto cleanup;

	/* create a dummy i2c client for the other amps */
	dummy = i2c_new_dummy(i2c->adapter, i2c->addr + 1);
	if (!dummy) {
		err = -ENODEV;
		goto cleanup;
	}
	err = tda7802_init(&tda7802s[1], dummy);
	if (err)
		goto cleanup;

	switch (i2c->addr) {
#if (MODEL3)
	case AMP1_ADDR:
		err = snd_soc_register_codec(&i2c->dev, &soc_codec_tda7802,
				&tda7802_base_amp_dai, 1);
		break;
#elif (MODELSX)
	case AMP1_ADDR:
		err = snd_soc_register_codec(&i2c->dev, &soc_codec_tda7802,
				&tda7802_amp_m_dai, 1);
		break;
	case AMP3_ADDR:
		err = snd_soc_register_codec(&i2c->dev, &soc_codec_tda7802,
				&tda7802_amp_s_dai, 1);
		break;
#endif
	default:
		dev_err(&i2c->dev, "Amp not paired with a codec addr=0x%02hx\n",
				i2c->addr);
		err = -ENODEV;
		goto cleanup;
	}

	if (err) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", err);
		goto cleanup;
	}

	return 0;

cleanup:
	if (tda7802s)
		for (i = 0; i < AMPS_PER_DAI; ++i)
			tda7802_cleanup(&tda7802s[i]);
	if (dummy)
		i2c_unregister_device(dummy);
	return err;
}

static int tda7802_i2c_remove(struct i2c_client *i2c)
{
	struct tda7802_priv *tda7802s = i2c_get_clientdata(i2c);
	int i;

	dev_dbg(&i2c->dev, "%s addr=0x%02hx\n", __func__, i2c->addr);

	if (!tda7802s)
		return -ENODEV;

	snd_soc_unregister_codec(&i2c->dev);

	for (i = 0; i < AMPS_PER_DAI; ++i)
		tda7802_cleanup(&tda7802s[i]);

	/* Unregister the dummy device. */
	i2c_unregister_device(tda7802s[1].i2c);

	return 0;
}

static const struct i2c_device_id tda7802_i2c_id[] = {
	{"tda7802", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tda7802_i2c_id);

static struct i2c_driver tda7802_i2c_driver = {
	.driver = {
		.name  = "tda7802-codec",
		.owner = THIS_MODULE,
	},
	.probe	  = tda7802_i2c_probe,
	.remove	  = tda7802_i2c_remove,
	.id_table = tda7802_i2c_id,
};

#if 0
/* If/when this driver doesn't need to set up devices in module_init we can
   provide the i2c driver with this declaration. Instead, we add the driver
   with i2c_add_driver in module_init. */
module_i2c_driver(tda7802_i2c_driver);
#endif

static struct i2c_board_info i2c_board_info[] = {
	{
		I2C_BOARD_INFO("tda7802", AMP1_ADDR),
	},
#if (MODELSX)
	{
		I2C_BOARD_INFO("tda7802", AMP3_ADDR),
	},
#endif
};
#define NUM_DAIS ARRAY_SIZE(i2c_board_info)

static struct i2c_client *dai_i2c_clients[NUM_DAIS];

static void dai_i2c_clients_cleanup(void)
{
	int i;

	for (i = 0; i < NUM_DAIS; ++i)
		if (dai_i2c_clients[i])
			i2c_unregister_device(dai_i2c_clients[i]);
}

static int __init tda7802_module_init(void)
{
	struct i2c_adapter *adapter;
	int err, i;

	err = i2c_add_driver(&tda7802_i2c_driver);
	if (err)
		goto cleanup;

	adapter = i2c_get_adapter(I2C_BUS);
	if (!adapter) {
		err = -ENODEV;
		goto cleanup;
	}

	for (i = 0; i < NUM_DAIS; ++i) {
		dai_i2c_clients[i] = i2c_new_device(adapter,
				&i2c_board_info[i]);
		if (!dai_i2c_clients[i]) {
			err = -ENODEV;
			goto cleanup;
		}
	}

	i2c_put_adapter(adapter);

	return 0;

cleanup:
	if (adapter)
		i2c_put_adapter(adapter);
	dai_i2c_clients_cleanup();
	i2c_del_driver(&tda7802_i2c_driver);

	return err;
}

static void __exit tda7802_module_exit(void)
{
	dai_i2c_clients_cleanup();
	i2c_del_driver(&tda7802_i2c_driver);
}

module_init(tda7802_module_init);
module_exit(tda7802_module_exit);

MODULE_DESCRIPTION("ASoC ST TDA7802 driver");
MODULE_LICENSE("GPL");
