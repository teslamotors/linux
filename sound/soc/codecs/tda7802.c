// SPDX-License-Identifier: GPL-2.0
/*
 * tda7802.c  --  codec driver for ST TDA7802
 *
 * Copyright (C) 2016-2019 Tesla Motors, Inc.
 */
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <sound/soc.h>

enum tda7802_type {
	tda7802_base,
	tda7802_tesla_model3_base,
	tda7802_tesla_modelsx_info2_master,
	tda7802_tesla_modelsx_info2_slave,
};

#define I2C_BUS				   4

#define CHANNELS_PER_AMP		   4
#define MAX_NUM_AMPS			   4

#define ENABLE_DELAY_MS			   10

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

static const u8 IB3_FORMAT[4][4] = {
	/* 1x amp, 4 channels */
	{ IB3_FORMAT_TDM4 },
	/* 2x amp, 8 channels */
	{ IB3_FORMAT_TDM8_DEV1,
		IB3_FORMAT_TDM8_DEV2 },
	/* 3x amp not supported */
	{ },
	/* 4x amp, 16 channels */
	{ IB3_FORMAT_TDM16_DEV1,
		IB3_FORMAT_TDM16_DEV2,
		IB3_FORMAT_TDM16_DEV3,
		IB3_FORMAT_TDM16_DEV4 },
};

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

#define DUMP_NUM_BLOCK			   6
#define DUMP_NUM_REGS			   (DUMP_NUM_BLOCK * 2)

struct tda7802_priv {
	struct mutex mutex;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct regulator *enable_reg;
	const char *diag_mode_ch13, *diag_mode_ch24;
	u8 gain_ch13, gain_ch24;
};

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
	struct tda7802_priv *tda7802 =
		snd_soc_component_get_drvdata(dai->component);
	struct device *dev = dai->dev;
	u8 data[6] = { 0 };
	int err;

	dev_dbg(dev, "%s\n", __func__);

	/* All channels out of tri-state mode, all channels in Standard Class
	 * AB mode (not High-efficiency)
	 */
	data[0] = IB0_CH4_CLASS_AB | IB0_CH3_CLASS_AB | IB0_CH2_CLASS_AB |
		IB0_CH1_CLASS_AB;

	/* Rear channel load impedance set to 2-Ohm, default diagnostic timing,
	 * GV1 gain on all channels (default), no digital gain increase
	 */
	data[1] = IB1_REAR_IMPEDANCE_2_OHM | IB1_LONG_DIAG_TIMING_x1;
	switch (tda7802->gain_ch13) {
	case 4:
		data[1] |= IB1_GAIN_CH13_GV4;
		break;
	case 3:
		data[1] |= IB1_GAIN_CH13_GV3;
		break;
	case 2:
		data[1] |= IB1_GAIN_CH13_GV2;
		break;
	case 1:
		data[1] |= IB1_GAIN_CH13_GV1;
		break;
	default:
		dev_err(dev, "Unknown gain for channel 1,3 %d, setting to 1\n",
				tda7802->gain_ch13);
		return -EINVAL;
	}

	switch (tda7802->gain_ch24) {
	case 4:
		data[1] |= IB1_GAIN_CH24_GV4;
		break;
	case 3:
		data[1] |= IB1_GAIN_CH24_GV3;
		break;
	case 2:
		data[1] |= IB1_GAIN_CH24_GV2;
		break;
	case 1:
		data[1] |= IB1_GAIN_CH24_GV1;
		break;
	default:
		dev_err(dev, "Unknown gain for channel 2,4 %d, setting to 1\n",
				tda7802->gain_ch24);
		return -EINVAL;
	}

	/* Mute timing 1.45ms, all channels un-muted, digital mute enabled,
	 * 5.3V undervoltage threshold, front-channel load impedance set to
	 * 2-Ohms
	 */
	data[2] = IB2_MUTE_TIME_1_MS | IB2_CH13_UNMUTED | IB2_CH24_UNMUTED |
		IB2_AUTOMUTE_THRESHOLD_5V3 | IB2_FRONT_IMPEDANCE_2_OHM;

	/* Don't set IB3 here, we should set it in set_tdm_slot
	 * data[3] = 0;
	 */

	/* Noise gating enabled, short and offset info on CD-Diag (fault) pin,
	 * diagnostics disabled. Default to speaker.
	 */
	if (strcmp(tda7802->diag_mode_ch13, "Booster") == 0)
		data[4] |= IB4_CH13_DIAG_MODE_BOOSTER;
	else
		data[4] |= IB4_CH13_DIAG_MODE_SPEAKER;

	if (strcmp(tda7802->diag_mode_ch24, "Booster") == 0)
		data[4] |= IB4_CH24_DIAG_MODE_BOOSTER;
	else
		data[4] |= IB4_CH24_DIAG_MODE_SPEAKER;

	/* Temperature warning on diag pin set to TW1 (highest setting), clip
	 * detection set to 1% on all channels
	 */
	data[5] = IB5_TEMP_WARNING_ON_CDDIAG_TW1 | IB5_CLIP_DETECT_FRONT_1PC |
		IB5_CLIP_DETECT_REAR_1PC;

	/* enable the device */
	err = regulator_enable(tda7802->enable_reg);
	if (err < 0) {
		dev_err(dev, "Could not enable (startup): %d\n", err);
		return err;
	}
	msleep(ENABLE_DELAY_MS);

	err = regmap_bulk_write(tda7802->regmap, TDA7802_IB0, data,
			ARRAY_SIZE(data));
	if (err < 0) {
		dev_err(dev, "Cannot configure amp: %d\n", err);
		return err;
	}

	return 0;
}

static void tda7802_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct tda7802_priv *tda7802 =
		snd_soc_component_get_drvdata(dai->component);
	struct device *dev = dai->dev;
	int err;

	dev_dbg(dev, "%s\n", __func__);

	err = regulator_disable(tda7802->enable_reg);
	if (err < 0)
		dev_err(dev, "Could not disable (shutdown): %d\n", err);
}

static int tda7802_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct device *dev = dai->dev;
	int err;

	dev_dbg(dev, "%s mute=%d\n", __func__, mute);

	if (mute) {
		/* digital mute */
		err = snd_soc_component_update_bits(dai->component, TDA7802_IB2,
				IB2_DIGITAL_MUTE_DISABLED, 0);
		if (err < 0) {
			dev_err(dev, "Cannot mute amp: %d\n", err);
			return err;
		}

		/* amp off */
		err = snd_soc_component_update_bits(dai->component, TDA7802_IB5,
				IB5_AMPLIFIER_ON, 0);
		if (err < 0) {
			dev_err(dev, "Cannot amp off: %d\n", err);
			return err;
		}

	} else {
		/* amp on */
		err = snd_soc_component_update_bits(dai->component, TDA7802_IB5,
				IB5_AMPLIFIER_ON, IB5_AMPLIFIER_ON);
		if (err < 0) {
			dev_err(dev, "Cannot amp on: %d\n", err);
			return err;
		}

		/* digital unmute */
		err = snd_soc_component_update_bits(dai->component, TDA7802_IB2,
				IB2_DIGITAL_MUTE_DISABLED,
				IB2_DIGITAL_MUTE_DISABLED);
		if (err < 0) {
			dev_err(dev, "Cannot unmute amp: %d\n", err);
			return err;
		}
	}

	return 0;
}

static int tda7802_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
		unsigned int rx_mask, int slots, int slot_width)
{
	int width_index, slot_index, ret;
	struct device *dev = dai->dev;
	u8 data;

	if (!(slots == 4 || slots == 8 || slots == 16)) {
		dev_err(dev, "Failed to set %d slots, supported: 4, 8, 16\n",
				slots);
		return -ENOTSUPP;
	}
	width_index = (slots / 4) - 1;

	switch (tx_mask) {
	case 0x000f:
		slot_index = 0;
		break;
	case 0x00f0:
		slot_index = 1;
		break;
	case 0x0f00:
		slot_index = 2;
		break;
	case 0xf000:
		slot_index = 3;
		break;
	default:
		/* must be contigious nibble */
		dev_err(dev, "Failed to set tx_mask %08x\n", tx_mask);
		return -ENOTSUPP;
	}

	/* 48kHz sample rate, TDM configuration, 64-bit I2S frame period, PLL
	 * clock dither disabled, high-pass filter enabled (blocks DC output)
	 */
	data = IB3_SAMPLE_RATE_48_KHZ | IB3_FORMAT[width_index][slot_index] |
		IB3_I2S_FRAME_PERIOD_64 | IB3_HIGH_PASS_FILTER_ENABLE;
	ret = snd_soc_component_write(dai->component, TDA7802_IB3, data);
	if (ret < 0) {
		dev_err(dev, "Failed to write IB3 config: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_dai_ops tda7802_dai_ops = {
	.startup      = tda7802_dai_startup,
	.shutdown     = tda7802_dai_shutdown,
	.digital_mute = tda7802_digital_mute,
	.set_tdm_slot = tda7802_set_tdm_slot,
};

static struct snd_soc_dai_driver tda7802_dai_driver = {
	.name = "tda7802",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 4,
		.channels_max = 4,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

/* Tesla Model 3 DAIs */
static struct snd_soc_dai_driver tda7802_base_amp_dai = {
	.name = "tda7802-oct",
	.playback = {
		.stream_name  = "Base Amp",
		.channels_min = 4,
		.channels_max = 4,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

/* Tesla Info 2 DAIs */
static struct snd_soc_dai_driver tda7802_amp_m_dai = {
	.name = "tda7802-oct-m",
	.playback = {
		.stream_name  = "AmpM Codec",
		.channels_min = 4,
		.channels_max = 4,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

static struct snd_soc_dai_driver tda7802_amp_s_dai = {
	.name = "tda7802-oct-s",
	.playback = {
		.stream_name  = "AmpS Codec",
		.channels_min = 4,
		.channels_max = 4,
		.rates	      = SNDRV_PCM_RATE_48000,
		.formats      = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &tda7802_dai_ops,
};

/* The speaker test is triggered by reading a sysfs attribute file attached to
 * the I2C device. The user space thread that's doing the reading is blocked
 * until the test completes (or times out). We only permit one test to be in
 * progress at a time.
 */

static int speaker_test_start(struct regmap *regmap)
{
	int err;

	err = regmap_update_bits(regmap, TDA7802_IB5, IB5_AMPLIFIER_ON, 0);
	if (err < 0) {
		dev_err(regmap_get_device(regmap),
			"Cannot disable amp for speaker test: %d\n", err);
		return err;
	}

	err = regmap_update_bits(regmap, TDA7802_IB4, IB4_DIAG_MODE_ENABLE, 0);
	if (err < 0) {
		dev_err(regmap_get_device(regmap),
			"Cannot disable diag mode before speaker test: %d\n",
			err);
		return err;
	}

	/* Seem to need at least a 15 ms delay here before the rising
	 * edge. Otherwise the diagnostics never complete (perhaps
	 * they never start).
	 */
	msleep(20);

	err = regmap_update_bits(regmap, TDA7802_IB4,
				 IB4_DIAG_MODE_ENABLE, IB4_DIAG_MODE_ENABLE);
	if (err < 0)
		dev_err(regmap_get_device(regmap),
			"Cannot enable diag mode for speaker test: %d\n", err);
	return err;
}

static int speaker_test_stop(struct regmap *regmap)
{
	int err;

	err = regmap_update_bits(regmap, TDA7802_IB4, IB4_DIAG_MODE_ENABLE, 0);
	if (err < 0)
		dev_err(regmap_get_device(regmap),
			"Cannot disable diag mode after speaker test: %d\n",
			err);
	return err;
}

/* We poll the test completion bit, letting the current thread sleep
 * until we're done. These values are not critical.
 */
#define SPEAKER_TEST_DONE_POLL_PERIOD_US 5000
#define SPEAKER_TEST_DONE_TIMEOUT_US	 5000000

static int speaker_test_check(struct tda7802_priv *tda7802,
		unsigned int *status)
{
	struct regmap *regmap = tda7802->regmap;
	struct device *dev = &tda7802->i2c->dev;
	int reg_err, err, i, amp_on;
	unsigned int val;

	reg_err = regulator_enable(tda7802->enable_reg);
	if (reg_err < 0) {
		dev_err(dev, "Could not enable (speaker-test): %d\n", err);
		return reg_err;
	}
	msleep(ENABLE_DELAY_MS);

	/* we should return amp on state when speaker-test is done */
	err = regmap_read(regmap, TDA7802_IB5, &amp_on);
	if (err < 0) {
		dev_err(dev, "Could not read amp on state: %d\n", err);
		goto speaker_test_disable;
	}

	for (i = 0; i < CHANNELS_PER_AMP; ++i)
		status[i] = 0xff;

	err = speaker_test_start(regmap);
	if (err < 0)
		goto speaker_test_restore;

	/* Wait until DB0_STARTUP_DIAG_STATUS is set */
	err = regmap_read_poll_timeout(regmap, TDA7802_DB0, val,
			val & DB0_STARTUP_DIAG_STATUS,
			SPEAKER_TEST_DONE_POLL_PERIOD_US,
			SPEAKER_TEST_DONE_TIMEOUT_US);
	if (err < 0) {
		if (err == -ENODATA)
			dev_err(dev,
				"Speaker diagnostic test did not complete\n");
		speaker_test_stop(regmap);
		goto speaker_test_restore;
	}

	err = speaker_test_stop(regmap);
	if (err < 0)
		goto speaker_test_restore;

	for (i = 0; i < CHANNELS_PER_AMP; ++i) {
		err = regmap_read(regmap, TDA7802_DB1 + i, status + i);
		if (err < 0) {
			dev_err(dev,
				"Cannot get speaker %d status: %d\n", i, err);
			goto speaker_test_restore;
		}
	}

speaker_test_restore:
	err = regmap_update_bits(regmap, TDA7802_IB5, IB5_AMPLIFIER_ON,
			amp_on);
	if (err < 0)
		dev_err(dev, "Could not restore amp on state (speaker-test): %d\n", err);

speaker_test_disable:
	reg_err = regulator_disable(tda7802->enable_reg);
	if (reg_err < 0) {
		dev_err(dev, "Could not disable (speaker-test): %d\n", err);
		return reg_err;
	}
	return err;
}

static ssize_t speaker_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tda7802_priv *tda7802 = dev_get_drvdata(dev);
	unsigned int channel_status[CHANNELS_PER_AMP];
	char *str = buf;
	int ret, i;

	mutex_lock(&tda7802->mutex);
	ret = speaker_test_check(tda7802, channel_status);
	mutex_unlock(&tda7802->mutex);
	if (ret < 0)
		return ret;

	for (i = 0; i < CHANNELS_PER_AMP; i++)
		str += sprintf(str, "%02x ", channel_status[i]);
	str += sprintf(str, "\n");

	return str - buf;
}

static DEVICE_ATTR_RO(speaker_test);

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct tda7802_priv *tda7802 = dev_get_drvdata(dev);
	int enabled = regulator_is_enabled(tda7802->enable_reg) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", enabled);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tda7802_priv *tda7802 = dev_get_drvdata(dev);
	int err;

	if (sysfs_streq(buf, "1")) {
		err = regulator_enable(tda7802->enable_reg);
		if (err < 0)
			dev_err(dev, "Could not enable (sysfs): %d\n", err);
	} else if (sysfs_streq(buf, "0")) {
		err = regulator_disable(tda7802->enable_reg);
		if (err < 0)
			dev_err(dev, "Could not disable (sysfs): %d\n", err);
	} else if (sysfs_streq(buf, "-1")) {
		err = regulator_force_disable(tda7802->enable_reg);
		if (err < 0)
			dev_err(dev, "Could not force disable (sysfs): %d\n", err);
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(enable);

/* read device tree or ACPI properties from device */
static int tda7802_read_properties(struct tda7802_priv *tda7802)
{
	struct device *dev = &tda7802->i2c->dev;
	int err = 0;

	tda7802->enable_reg = devm_regulator_get(dev, "enable");
	if (IS_ERR(tda7802->enable_reg)) {
		dev_err(dev, "Failed to get enable regulator\n");
		return PTR_ERR(tda7802->enable_reg);
	}

	err = device_property_read_u8(dev, "st,gain-ch13", &tda7802->gain_ch13);
	if (err < 0)
		dev_err(dev, "Failed to read gain, channel 1,3: %d\n", err);

	err = device_property_read_u8(dev, "st,gain-ch24", &tda7802->gain_ch24);
	if (err < 0)
		dev_err(dev, "Failed to read gain, channel 2,4: %d\n", err);

	err = device_property_read_string(dev, "st,diagnostic-mode-ch13",
			&tda7802->diag_mode_ch13);
	if (err < 0)
		dev_err(dev, "Failed to read diagnostic mode, channel 1,3: %d\n",
				err);

	err = device_property_read_string(dev, "st,diagnostic-mode-ch24",
			&tda7802->diag_mode_ch24);
	if (err < 0)
		dev_err(dev, "Failed to read diagnostic mode, channel 2,4: %d\n",
				err);

	return err;
}

static int tda7802_probe(struct snd_soc_component *component)
{
	struct tda7802_priv *tda7802 = snd_soc_component_get_drvdata(component);
	struct device *dev = &tda7802->i2c->dev;
	int err;

	dev_dbg(dev, "%s\n", __func__);

	/* Device is ready, now we should create sysfs attributes.
	 * Rememer to cleanup
	 */
	err = device_create_file(dev, &dev_attr_enable);
	if (err < 0) {
		dev_err(dev, "Could not create enable attr: %d\n", err);
		return err;
	}

	mutex_init(&tda7802->mutex);
	err = device_create_file(dev, &dev_attr_speaker_test);
	if (err < 0) {
		dev_err(dev, "Could not create speaker_test attr: %d\n", err);
		goto cleanup_speaker_test;
	}

	return 0;

cleanup_speaker_test:
	mutex_destroy(&tda7802->mutex);
	device_remove_file(&tda7802->i2c->dev, &dev_attr_enable);

	return err;
}

static void tda7802_remove(struct snd_soc_component *component)
{
	struct tda7802_priv *tda7802 = snd_soc_component_get_drvdata(component);

	device_remove_file(&tda7802->i2c->dev, &dev_attr_speaker_test);
	mutex_destroy(&tda7802->mutex);
	device_remove_file(&tda7802->i2c->dev, &dev_attr_enable);
}

static const struct snd_soc_component_driver tda7802_component_driver = {
	.probe = tda7802_probe,
	.remove = tda7802_remove,
};

static int tda7802_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct snd_soc_dai_driver *dai;
	struct tda7802_priv *tda7802;
	enum tda7802_type codec_type;
	int err;

	dev_dbg(dev, "%s addr=0x%02hx, id %p\n", __func__, i2c->addr, id);

	if (id) {
		codec_type = id->driver_data;
		dev_dbg(dev, "codec type %d\n", codec_type);
	} else {
		dev_err(dev, "Could not find matched device.\n");
		return -EINVAL;
	}

	switch (codec_type) {
	case tda7802_base:
		dai = &tda7802_dai_driver;
		break;
	case tda7802_tesla_model3_base:
		dai = &tda7802_base_amp_dai;
		break;
	case tda7802_tesla_modelsx_info2_master:
		dai = &tda7802_amp_m_dai;
		break;
	case tda7802_tesla_modelsx_info2_slave:
		dai = &tda7802_amp_s_dai;
		break;
	default:
		dev_err(dev, "No DAI for codec_type %d\n", codec_type);
		return -ENODEV;
	}

	tda7802 = devm_kmalloc(dev, sizeof(*tda7802), GFP_KERNEL);
	if (!tda7802)
		return -ENOMEM;

	i2c_set_clientdata(i2c, tda7802);
	tda7802->i2c = i2c;

	err = tda7802_read_properties(tda7802);
	if (err < 0)
		return err;

	tda7802->regmap = devm_regmap_init_i2c(tda7802->i2c,
			&tda7802_regmap_config);
	if (IS_ERR(tda7802->regmap))
		return PTR_ERR(tda7802->regmap);

	err = devm_snd_soc_register_component(dev, &tda7802_component_driver,
			dai, 1);
	if (err < 0)
		dev_err(dev, "Failed to register codec: %d\n", err);
	return err;
}

#ifdef CONFIG_OF
static const struct of_device_id tda7802_of_match[] = {
	{ .compatible = "st,tda7802" },
	{ .compatible = "st,tda7802-tm3b" },
	{ .compatible = "st,tda7802-ti2m" },
	{ .compatible = "st,tda7802-ti2s" },
	{ },
};
MODULE_DEVICE_TABLE(of, tda7802_of_match);
#endif

static const struct i2c_device_id tda7802_i2c_id[] = {
	{ "tda7802", tda7802_base },
	{ "tda7802-tm3b", tda7802_tesla_model3_base },
	{ "tda7802-ti2m", tda7802_tesla_modelsx_info2_master },
	{ "tda7802-ti2s", tda7802_tesla_modelsx_info2_slave },
	{},
};
MODULE_DEVICE_TABLE(i2c, tda7802_i2c_id);

static struct i2c_driver tda7802_i2c_driver = {
	.driver = {
		.name  = "tda7802-codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tda7802_of_match),
	},
	.probe	  = tda7802_i2c_probe,
	.id_table = tda7802_i2c_id,
};
module_i2c_driver(tda7802_i2c_driver);

MODULE_DESCRIPTION("ASoC ST TDA7802 driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Preston <thomas.preston@codethink.co.uk>");
