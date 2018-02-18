/*
 * palmas-adc.c -- TI PALMAS GPADC.
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * Author: Pradeep Goudagunta <pgoudagunta@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mfd/palmas.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/mutex.h>

#define MOD_NAME		"palmas-gpadc"
#define ADC_CONVERTION_TIMEOUT	(msecs_to_jiffies(5000))
#define TO_BE_CALCULATED	0
#define PRECISION_MULTIPLIER	1000000LL

struct palmas_gpadc_info {
/* calibration codes and regs */
	int x1;
	int x2;
	u8 trim1_reg;
	u8 trim2_reg;
	s64 gain;
	s64 offset;
	bool is_correct_code;
};

#define PALMAS_ADC_INFO(_chan, _x1, _x2, _t1, _t2, _is_correct_code)	\
[PALMAS_ADC_CH_##_chan] = {						\
		.x1 = _x1,						\
		.x2 = _x2,						\
		.gain = TO_BE_CALCULATED,				\
		.offset = TO_BE_CALCULATED,				\
		.trim1_reg = PALMAS_GPADC_TRIM##_t1,			\
		.trim2_reg = PALMAS_GPADC_TRIM##_t2,			\
		.is_correct_code = _is_correct_code			\
	}

static struct palmas_gpadc_info palmas_gpadc_info[] = {
	PALMAS_ADC_INFO(IN0, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN1, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN2, 2064, 3112, 3, 4, false),
	PALMAS_ADC_INFO(IN3, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN4, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN5, 2064, 3112, 1, 2, false),
	PALMAS_ADC_INFO(IN6, 2064, 3112, 5, 6, false),
	PALMAS_ADC_INFO(IN7, 2064, 3112, 7, 8, false),
	PALMAS_ADC_INFO(IN8, 2064, 3112, 9, 10, false),
	PALMAS_ADC_INFO(IN9, 2064, 3112, 11, 12, false),
	PALMAS_ADC_INFO(IN10, 2064, 3112, 13, 14, false),
	PALMAS_ADC_INFO(IN11, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN12, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN13, 0, 0, INVALID, INVALID, true),
	PALMAS_ADC_INFO(IN14, 2064, 3112, 15, 16, false),
	PALMAS_ADC_INFO(IN15, 0, 0, INVALID, INVALID, true),
};

struct palmas_gpadc {
	struct device			*dev;
	struct palmas			*palmas;
	u8				ch0_current;
	u8				ch3_current;
	bool				ch3_dual_current;
	bool				extended_delay;
	int				irq;
	int				irq_auto_0;
	int				irq_auto_1;
	struct palmas_gpadc_info	*adc_info;
	struct completion		conv_completion;
	struct palmas_adc_auto_conv_property auto_conv0_data;
	struct palmas_adc_auto_conv_property auto_conv1_data;
	bool				auto_conv0_enable;
	bool				auto_conv1_enable;
	int				auto_conversion_period;

	struct dentry			*dentry;
	bool is_shutdown;
	struct mutex lock;
};

/*
 * GPADC lock issue in AUTO mode.
 * Impact: In AUTO mode, GPADC conversion can be locked after disabling AUTO
 *	   mode feature.
 * Details:
 *	When the AUTO mode is the only conversion mode enabled, if the AUTO
 *	mode feature is disabled with bit GPADC_AUTO_CTRL.AUTO_CONV1_EN = 0
 *	or bit GPADC_AUTO_CTRL.AUTO_CONV0_EN = 0 during a conversion, the
 *	conversion mechanism can be seen as locked meaning that all following
 *	conversion will give 0 as a result. Bit GPADC_STATUS.GPADC_AVAILABLE
 *	will stay at 0 meaning that GPADC is busy. An RT conversion can unlock
 *	the GPADC.
 *
 * Workaround(s):
 *	To avoid the lock mechanism, the workaround to follow before any stop
 *	conversion request is:
 *	Force the GPADC state machine to be ON by using the
 *		GPADC_CTRL1.GPADC_FORCE bit = 1
 *	Shutdown the GPADC AUTO conversion using
 *		GPADC_AUTO_CTRL.SHUTDOWN_CONV[01] = 0.
 *	After 100us, force the GPADC state machine to be OFF by using the
 *		GPADC_CTRL1.GPADC_FORCE bit = 0
 */
static int palmas_disable_auto_conversion(struct palmas_gpadc *adc)
{
	int ret;

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_CTRL1,
			PALMAS_GPADC_CTRL1_GPADC_FORCE,
			PALMAS_GPADC_CTRL1_GPADC_FORCE);
	if (ret < 0) {
		dev_err(adc->dev, "GPADC_CTRL1 update failed: %d\n", ret);
		return ret;
	}

	ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL, 0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL write failed: %d\n", ret);
		return ret;
	}

	udelay(100);

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_CTRL1,
			PALMAS_GPADC_CTRL1_GPADC_FORCE, 0);
	if (ret < 0) {
		dev_err(adc->dev, "GPADC_CTRL1 update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static irqreturn_t palmas_gpadc_irq(int irq, void *data)
{
	struct palmas_gpadc *adc = data;

	complete(&adc->conv_completion);
	return IRQ_HANDLED;
}

static irqreturn_t palmas_gpadc_irq_auto(int irq, void *data)
{
	struct palmas_gpadc *adc = data;
	unsigned int val = 0;
	int ret;

	ret = palmas_read(adc->palmas, PALMAS_INTERRUPT_BASE,
			PALMAS_INT3_LINE_STATE, &val);
	if (ret < 0)
		dev_err(adc->dev, "%s: Failed to read INT3_LINE_STATE, %d\n",
			__func__, ret);

	if (val & PALMAS_INT3_LINE_STATE_GPADC_AUTO_0)
		dev_info(adc->dev, "Auto0 threshold interrupt occurred\n");
	if (val & PALMAS_INT3_LINE_STATE_GPADC_AUTO_1)
		dev_info(adc->dev, "Auto1 threshold interrupt occurred\n");

	return IRQ_HANDLED;
}

static int palmas_gpadc_auto_conv_configure(struct palmas_gpadc *adc)
{
	int adc_period, conv;
	int i;
	int ch0 = 0, ch1 = 0;
	int thres;
	int ret;

	if (!adc->auto_conv0_enable && !adc->auto_conv1_enable)
		return 0;

	adc_period = adc->auto_conversion_period;
	for (i = 0; i < 16; ++i) {
		if (((1000 * (1 << i))/32) > adc_period)
			break;
	}
	if (i > 0)
		i--;
	adc_period = i;
	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL,
			PALMAS_GPADC_AUTO_CTRL_COUNTER_CONV_MASK,
			adc_period);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL write failed: %d\n", ret);
		return ret;
	}

	conv = 0;
	if (adc->auto_conv0_enable) {
		int is_high;

		ch0 = adc->auto_conv0_data.adc_channel_number;
		conv |= PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN;
		conv |= (adc->auto_conv0_data.adc_shutdown ?
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV0 : 0);
		if (adc->auto_conv0_data.adc_high_threshold > 0) {
			thres = adc->auto_conv0_data.adc_high_threshold;
			is_high = 0;
		} else {
			thres = adc->auto_conv0_data.adc_low_threshold;
			is_high = BIT(7);
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV0_LSB, thres & 0xFF);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV0_LSB write failed: %d\n", ret);
			return ret;
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV0_MSB,
				((thres >> 8) & 0xF) | is_high);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV0_MSB write failed: %d\n", ret);
			return ret;
		}
	}

	if (adc->auto_conv1_enable) {
		int is_high;

		ch1 = adc->auto_conv1_data.adc_channel_number;
		conv |= PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN;
		conv |= (adc->auto_conv1_data.adc_shutdown ?
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV1 : 0);
		if (adc->auto_conv1_data.adc_high_threshold > 0) {
			thres = adc->auto_conv1_data.adc_high_threshold;
			is_high = 0;
		} else {
			thres = adc->auto_conv1_data.adc_low_threshold;
			is_high = BIT(7);
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV1_LSB, thres & 0xFF);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV1_LSB write failed: %d\n", ret);
			return ret;
		}

		ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_THRES_CONV1_MSB,
				((thres >> 8) & 0xF) | is_high);
		if (ret < 0) {
			dev_err(adc->dev,
				"THRES_CONV1_MSB write failed: %d\n", ret);
			return ret;
		}
	}

	ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_SELECT, (ch1 << 4) | ch0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_SELECT write failed: %d\n", ret);
		return ret;
	}

	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_CTRL,
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV1 |
			PALMAS_GPADC_AUTO_CTRL_SHUTDOWN_CONV0 |
			PALMAS_GPADC_AUTO_CTRL_AUTO_CONV1_EN |
			PALMAS_GPADC_AUTO_CTRL_AUTO_CONV0_EN, conv);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_CTRL write failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int palmas_gpadc_auto_conv_reset(struct palmas_gpadc *adc)
{
	int ret;

	if (!adc->auto_conv0_enable && !adc->auto_conv1_enable)
		return 0;

	ret = palmas_disable_auto_conversion(adc);
	if (ret < 0) {
		dev_err(adc->dev, "Disable auto conversion failed: %d\n", ret);
		return ret;
	}

	ret = palmas_write(adc->palmas, PALMAS_GPADC_BASE,
			PALMAS_GPADC_AUTO_SELECT, 0);
	if (ret < 0) {
		dev_err(adc->dev, "AUTO_SELECT write failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static inline int palmas_gpadc_set_current_src(struct palmas_gpadc *adc,
					       u8 ch0_current, u8 ch3_current)
{
	unsigned int mask, val;

	mask = (PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_MASK |
		PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK);
	val = (ch0_current << PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_SHIFT) |
		(ch3_current << PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT);
	return palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				  PALMAS_GPADC_CTRL1, mask, val);
}

static int palmas_gpadc_check_status(struct palmas_gpadc *adc)
{
	int retry_cnt = 3;
	int check_cnt = 3;
	int loop_cnt = 3;
	unsigned int val = 0;
	int ret;

retry:
	do {
		ret = palmas_read(adc->palmas, PALMAS_GPADC_BASE,
				  PALMAS_GPADC_STATUS, &val);
		if (ret < 0) {
			dev_err(adc->dev, "%s: Failed to read STATUS, %d\n",
				__func__, ret);
			return ret;
		} else if (val & PALMAS_GPADC_STATUS_GPADC_AVAILABLE) {
			if (--check_cnt == 0)
				break;
		} else {
			dev_warn(adc->dev, "%s: GPADC is busy, STATUS 0x%02x\n",
				 __func__, val);
		}
		udelay(100);
	} while (loop_cnt-- > 0);

	if (check_cnt == 0) {
		if (retry_cnt < 3)
			dev_warn(adc->dev, "%s: GPADC is unlocked.\n",
				 __func__);
		return 0;
	}

	dev_warn(adc->dev, "%s: GPADC is locked.\n", __func__);
	dev_warn(adc->dev, "%s: Perform RT conversion to unlock GPADC.\n",
		__func__);
	palmas_disable_auto_conversion(adc);
	palmas_write(adc->palmas, PALMAS_GPADC_BASE, PALMAS_GPADC_RT_SELECT,
		     PALMAS_GPADC_RT_SELECT_RT_CONV_EN);
	palmas_write(adc->palmas, PALMAS_GPADC_BASE, PALMAS_GPADC_RT_CTRL,
		     PALMAS_GPADC_RT_CTRL_START_POLARITY);
	udelay(100);
	palmas_write(adc->palmas, PALMAS_GPADC_BASE, PALMAS_GPADC_RT_CTRL, 0);
	palmas_write(adc->palmas, PALMAS_GPADC_BASE, PALMAS_GPADC_RT_SELECT, 0);
	if (retry_cnt-- > 0) {
		goto retry;
	} else {
		dev_err(adc->dev, "%s: Failed to unlock GPADC.\n", __func__);
		return -EDEADLK;
	}

	return 0;
}

static int palmas_gpadc_start_mask_interrupt(struct palmas_gpadc *adc, int mask)
{
	int ret;

	if (!mask)
		ret = palmas_update_bits(adc->palmas, PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_MASK,
					PALMAS_INT3_MASK_GPADC_EOC_SW, 0);
	else
		ret = palmas_update_bits(adc->palmas, PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_MASK,
					PALMAS_INT3_MASK_GPADC_EOC_SW,
					PALMAS_INT3_MASK_GPADC_EOC_SW);
	if (ret < 0)
		dev_err(adc->dev, "GPADC INT MASK update failed: %d\n", ret);

	return ret;
}

static int palmas_gpadc_enable(struct palmas_gpadc *adc, int adc_chan,
			       int enable)
{
	unsigned int mask, val;
	int ret;

	if (enable) {
		val = (adc->extended_delay
			<< PALMAS_GPADC_RT_CTRL_EXTEND_DELAY_SHIFT);
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
					PALMAS_GPADC_RT_CTRL,
					PALMAS_GPADC_RT_CTRL_EXTEND_DELAY, val);
		if (ret < 0) {
			dev_err(adc->dev, "RT_CTRL update failed: %d\n", ret);
			return ret;
		}

		mask = (PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_MASK |
			PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK |
			PALMAS_GPADC_CTRL1_GPADC_FORCE);
		val = (adc->ch0_current
			<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH0_SHIFT);
		val |= (adc->ch3_current
			<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT);
		val |= PALMAS_GPADC_CTRL1_GPADC_FORCE;
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_CTRL1, mask, val);
		if (ret < 0) {
			dev_err(adc->dev,
				"Failed to update current setting: %d\n", ret);
			return ret;
		}

		mask = (PALMAS_GPADC_SW_SELECT_SW_CONV0_SEL_MASK |
			PALMAS_GPADC_SW_SELECT_SW_CONV_EN);
		val = (adc_chan | PALMAS_GPADC_SW_SELECT_SW_CONV_EN);
		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_SELECT, mask, val);
		if (ret < 0) {
			dev_err(adc->dev, "SW_SELECT update failed: %d\n", ret);
			return ret;
		}
	} else {
		mask = val = 0;
		mask |= PALMAS_GPADC_CTRL1_GPADC_FORCE;

		/* Restore CH3 current source if CH3 is dual current mode. */
		if ((adc_chan == PALMAS_ADC_CH_IN3) && adc->ch3_dual_current) {
			mask |= PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_MASK;
			val |= (adc->ch3_current
				<< PALMAS_GPADC_CTRL1_CURRENT_SRC_CH3_SHIFT);
		}

		ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
					 PALMAS_GPADC_CTRL1, mask, val);
		if (ret < 0) {
			dev_err(adc->dev, "CTRL1 update failed: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int palmas_gpadc_read_prepare(struct palmas_gpadc *adc, int adc_chan)
{
	int ret;

	ret = palmas_gpadc_enable(adc, adc_chan, true);
	if (ret < 0)
		return ret;

	return palmas_gpadc_start_mask_interrupt(adc, 0);
}

static void palmas_gpadc_read_done(struct palmas_gpadc *adc, int adc_chan)
{
	palmas_gpadc_start_mask_interrupt(adc, 1);
	palmas_gpadc_enable(adc, adc_chan, false);
}

static int palmas_gpadc_calibrate(struct palmas_gpadc *adc, int adc_chan)
{
	s64 k;
	int d1;
	int d2;
	int ret;
	int x1 =  adc->adc_info[adc_chan].x1;
	int x2 =  adc->adc_info[adc_chan].x2;

	ret = palmas_read(adc->palmas, PALMAS_TRIM_GPADC_BASE,
				adc->adc_info[adc_chan].trim1_reg, &d1);
	if (ret < 0) {
		dev_err(adc->dev, "TRIM read failed: %d\n", ret);
		goto scrub;
	}

	ret = palmas_read(adc->palmas, PALMAS_TRIM_GPADC_BASE,
				adc->adc_info[adc_chan].trim2_reg, &d2);
	if (ret < 0) {
		dev_err(adc->dev, "TRIM read failed: %d\n", ret);
		goto scrub;
	}

	/* Gain Calculation */
	k = PRECISION_MULTIPLIER;
	k += div64_s64(PRECISION_MULTIPLIER * (d2 - d1), x2 - x1);
	adc->adc_info[adc_chan].gain = k;

	/* Offset Calculation */
	adc->adc_info[adc_chan].offset = (d1 * PRECISION_MULTIPLIER);
	adc->adc_info[adc_chan].offset -= ((k - PRECISION_MULTIPLIER) * x1);

scrub:
	return ret;
}

static int palmas_gpadc_start_convertion(struct palmas_gpadc *adc, int adc_chan)
{
	unsigned int val;
	int ret;

	reinit_completion(&adc->conv_completion);
	ret = palmas_update_bits(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_SELECT,
				PALMAS_GPADC_SW_SELECT_SW_START_CONV0,
				PALMAS_GPADC_SW_SELECT_SW_START_CONV0);
	if (ret < 0) {
		dev_err(adc->dev, "ADC_SW_START write failed: %d\n", ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&adc->conv_completion,
				ADC_CONVERTION_TIMEOUT);
	if (ret == 0) {
		dev_err(adc->dev, "ADC conversion not completed\n");
		ret = -ETIMEDOUT;
		goto error;
	}

	ret = palmas_bulk_read(adc->palmas, PALMAS_GPADC_BASE,
				PALMAS_GPADC_SW_CONV0_LSB, &val, 2);
	if (ret < 0) {
		dev_err(adc->dev, "ADCDATA read failed: %d\n", ret);
		return ret;
	}

	ret = (val & 0xFFF);
	if (ret == 0) {
		ret = -EBUSY;
		goto error;
	}
	return ret;

error:
	palmas_gpadc_check_status(adc);
	palmas_gpadc_auto_conv_reset(adc);
	palmas_gpadc_auto_conv_configure(adc);
	return ret;
}

static int palmas_gpadc_get_calibrated_code(struct palmas_gpadc *adc,
						int adc_chan, int val)
{
	s64 code = val * PRECISION_MULTIPLIER;

	if ((code - adc->adc_info[adc_chan].offset) < 0) {
		dev_err(adc->dev, "No Input Connected\n");
		return 0;
	}

	if (!(adc->adc_info[adc_chan].is_correct_code)) {
		code -= adc->adc_info[adc_chan].offset;
		code = div_s64(code, adc->adc_info[adc_chan].gain);
		return code;
	}

	return val;
}

static int palmas_gpadc_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct  palmas_gpadc *adc = iio_priv(indio_dev);
	int adc_chan = chan->channel;
	int ret = 0;

	if (adc_chan > PALMAS_ADC_CH_MAX)
		return -EINVAL;

	mutex_lock(&adc->lock);
	if (adc->is_shutdown) {
		mutex_unlock(&adc->lock);
		return -EINVAL;
	}

	mutex_lock(&indio_dev->mlock);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	case IIO_CHAN_INFO_PROCESSED:
		ret = palmas_gpadc_read_prepare(adc, adc_chan);
		if (ret < 0)
			goto out;

		ret = palmas_gpadc_start_convertion(adc, adc_chan);
		if (ret < 0) {
			dev_err(adc->dev,
			"ADC start coversion failed\n");
			goto out;
		}

		if (mask == IIO_CHAN_INFO_PROCESSED)
			ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

		*val = ret;

		ret = IIO_VAL_INT;
		goto out;

	case IIO_CHAN_INFO_RAW_DUAL:
	case IIO_CHAN_INFO_PROCESSED_DUAL:
		ret = palmas_gpadc_read_prepare(adc, adc_chan);
		if (ret < 0)
			goto out;

		ret = palmas_gpadc_start_convertion(adc, adc_chan);
		if (ret < 0) {
			dev_err(adc->dev,
				"ADC start coversion failed\n");
			goto out;
		}

		if (mask == IIO_CHAN_INFO_PROCESSED_DUAL)
			ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

		*val = ret;

		if ((adc_chan == PALMAS_ADC_CH_IN3)
				&& adc->ch3_dual_current && val2) {
			ret = palmas_gpadc_set_current_src(adc,
					adc->ch0_current, adc->ch3_current + 1);
			if (ret < 0) {
				dev_err(adc->dev,
					"Failed to set current src: %d\n", ret);
				goto out;
			}

			ret = palmas_gpadc_start_convertion(adc, adc_chan);
			if (ret < 0) {
				dev_err(adc->dev,
					"ADC start coversion failed\n");
				goto out;
			}

			if (mask == IIO_CHAN_INFO_PROCESSED_DUAL)
				ret = palmas_gpadc_get_calibrated_code(
							adc, adc_chan, ret);

			*val2 = ret;
		}

		ret = IIO_VAL_INT;
		goto out;
	}

	mutex_unlock(&indio_dev->mlock);
	mutex_unlock(&adc->lock);
	return ret;

out:
	palmas_gpadc_read_done(adc, adc_chan);
	mutex_unlock(&indio_dev->mlock);
	mutex_unlock(&adc->lock);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int palams_gpadc_get_auto_conv_val(struct palmas_gpadc *adc,
					int auto_conv_ch)
{
	unsigned int reg;
	unsigned int val;
	int ret;

	if (auto_conv_ch == 0) {
		reg = PALMAS_GPADC_AUTO_CONV0_LSB;
	} else if (auto_conv_ch == 1) {
		reg = PALMAS_GPADC_AUTO_CONV1_LSB;
	} else {
		dev_err(adc->dev, "%s: Invalid auto conv channel %d\n\n",
			__func__, auto_conv_ch);
		return -EINVAL;
	}

	ret = palmas_bulk_read(adc->palmas, PALMAS_GPADC_BASE, reg, &val, 2);
	if (ret < 0) {
		dev_err(adc->dev, "%s: Auto conv%d data read failed: %d\n",
			__func__, auto_conv_ch, ret);
		return ret;
	}

	return (val & 0xFFF);
}

static ssize_t auto_conv_val_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct palmas_gpadc *adc = file->private_data;
	unsigned char *d_iname;
	char buf[64] = { 0, };
	ssize_t ret = 0;
	int auto_conv_ch = -1;

	d_iname = file->f_path.dentry->d_iname;

	if (!strcmp("auto_conv0_val", d_iname))
		auto_conv_ch = 0;
	else if (!strcmp("auto_conv1_val", d_iname))
		auto_conv_ch = 1;

	ret = palams_gpadc_get_auto_conv_val(adc, auto_conv_ch);
	if (ret < 0)
		return ret;

	ret = snprintf(buf, sizeof(buf), "%d\n", ret);
	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static const struct file_operations auto_conv_val_fops = {
	.open		= simple_open,
	.read		= auto_conv_val_read,
};

static int auto_conv_period_get(void *data, u64 *val)
{
	struct palmas_gpadc *adc = (struct palmas_gpadc *)data;

	*val = adc->auto_conversion_period;
	return 0;
}

static int auto_conv_period_set(void *data, u64 val)
{
	struct palmas_gpadc *adc = (struct palmas_gpadc *)data;
	struct iio_dev *iodev = dev_get_drvdata(adc->dev);

	adc->auto_conversion_period = val;

	mutex_lock(&iodev->mlock);
	palmas_gpadc_auto_conv_reset(adc);
	palmas_gpadc_auto_conv_configure(adc);
	mutex_unlock(&iodev->mlock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(auto_conv_period_fops, auto_conv_period_get,
			auto_conv_period_set, "%llu\n");

static ssize_t auto_conv_data_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct palmas_gpadc *adc = file->private_data;
	unsigned char *d_iname;
	char buf[64] = { 0, };
	ssize_t ret = 0;

	d_iname = file->f_path.dentry->d_iname;

	if (!strcmp("auto_conv0_channel", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv0_data.adc_channel_number);
	} else if (!strcmp("auto_conv1_channel", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv1_data.adc_channel_number);
	} else if (!strcmp("auto_conv0_high_threshold", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv0_data.adc_high_threshold);
	} else if  (!strcmp("auto_conv1_high_threshold", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv1_data.adc_high_threshold);
	} else if (!strcmp("auto_conv0_low_threshold", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv0_data.adc_low_threshold);
	} else if (!strcmp("auto_conv1_low_threshold", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv1_data.adc_low_threshold);
	} else if (!strcmp("auto_conv0_shutdown", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv0_data.adc_shutdown);
	} else if (!strcmp("auto_conv1_shutdown", d_iname)) {
		ret = snprintf(buf, sizeof(buf), "%d\n",
				adc->auto_conv1_data.adc_shutdown);
	}

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t auto_conv_data_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct palmas_gpadc *adc = file->private_data;
	struct iio_dev *iodev = dev_get_drvdata(adc->dev);
	unsigned char *d_iname;
	char buf[64] = { 0, };
	int val;
	ssize_t buf_size;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (!sscanf(buf, "%d\n", &val))
		return -EINVAL;

	d_iname = file->f_path.dentry->d_iname;

	if (!strcmp("auto_conv0_channel", d_iname)) {
		adc->auto_conv0_data.adc_channel_number = val;
	} else if (!strcmp("auto_conv1_channel", d_iname)) {
		adc->auto_conv1_data.adc_channel_number = val;
	} else if (!strcmp("auto_conv0_high_threshold", d_iname)) {
		adc->auto_conv0_data.adc_high_threshold = val;
		if (val > 0)
			adc->auto_conv0_data.adc_low_threshold = 0;
	} else if  (!strcmp("auto_conv1_high_threshold", d_iname)) {
		adc->auto_conv1_data.adc_high_threshold = val;
		if (val > 0)
			adc->auto_conv1_data.adc_low_threshold = 0;
	} else if (!strcmp("auto_conv0_low_threshold", d_iname)) {
		adc->auto_conv0_data.adc_low_threshold = val;
		if (val > 0)
			adc->auto_conv0_data.adc_high_threshold = 0;
	} else if (!strcmp("auto_conv1_low_threshold", d_iname)) {
		adc->auto_conv1_data.adc_low_threshold = val;
		if (val > 0)
			adc->auto_conv1_data.adc_high_threshold = 0;
	} else if (!strcmp("auto_conv0_shutdown", d_iname)) {
		adc->auto_conv0_data.adc_shutdown = val;
	} else if (!strcmp("auto_conv1_shutdown", d_iname)) {
		adc->auto_conv1_data.adc_shutdown = val;
	}

	mutex_lock(&iodev->mlock);
	palmas_gpadc_auto_conv_reset(adc);
	palmas_gpadc_auto_conv_configure(adc);
	mutex_unlock(&iodev->mlock);
	return buf_size;
}

static const struct file_operations auto_conv_data_fops = {
	.open		= simple_open,
	.write		= auto_conv_data_write,
	.read		= auto_conv_data_read,
};

static void palmas_gpadc_debugfs_init(struct palmas_gpadc *adc)
{
	adc->dentry = debugfs_create_dir(dev_name(adc->dev), NULL);
	if (!adc->dentry) {
		dev_err(adc->dev, "%s: failed to create debugfs dir\n",
			__func__);
		return;
	}

	if (adc->auto_conv0_enable || adc->auto_conv1_enable)
		debugfs_create_file("auto_conv_period", 0644,
				    adc->dentry, adc,
				    &auto_conv_period_fops);

	if (adc->auto_conv0_enable) {
		debugfs_create_file("auto_conv0_channel", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv0_high_threshold", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv0_low_threshold", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv0_shutdown", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv0_val", 0444,
				    adc->dentry, adc, &auto_conv_val_fops);
	}

	if (adc->auto_conv1_enable) {
		debugfs_create_file("auto_conv1_channel", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv1_high_threshold", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv1_low_threshold", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv1_shutdown", 0644,
				    adc->dentry, adc, &auto_conv_data_fops);
		debugfs_create_file("auto_conv1_val", 0444,
				    adc->dentry, adc, &auto_conv_val_fops);
	}
}
#else
static void palmas_gpadc_debugfs_init(struct palmas_gpadc *adc)
{
}
#endif /*  CONFIG_DEBUG_FS */

static const struct iio_info palmas_gpadc_iio_info = {
	.read_raw = palmas_gpadc_read_raw,
	.driver_module = THIS_MODULE,
};

#define PALMAS_ADC_CHAN_IIO(chan)					\
{									\
	.datasheet_name = PALMAS_DATASHEET_NAME(chan),			\
	.type = IIO_VOLTAGE, 						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_CALIBSCALE),			\
	.indexed = 1,							\
	.channel = PALMAS_ADC_CH_##chan,				\
}

#define PALMAS_ADC_CHAN_DUAL_IIO(chan)					\
{									\
	.datasheet_name = PALMAS_DATASHEET_NAME(chan),			\
	.type = IIO_VOLTAGE,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_PROCESSED) |			\
			BIT(IIO_CHAN_INFO_RAW_DUAL) |			\
			BIT(IIO_CHAN_INFO_PROCESSED_DUAL),		\
	.indexed = 1,							\
	.channel = PALMAS_ADC_CH_##chan,				\
}

static const struct iio_chan_spec palmas_gpadc_iio_channel[] = {
	PALMAS_ADC_CHAN_IIO(IN0),
	PALMAS_ADC_CHAN_IIO(IN1),
	PALMAS_ADC_CHAN_IIO(IN2),
	PALMAS_ADC_CHAN_DUAL_IIO(IN3),
	PALMAS_ADC_CHAN_IIO(IN4),
	PALMAS_ADC_CHAN_IIO(IN5),
	PALMAS_ADC_CHAN_IIO(IN6),
	PALMAS_ADC_CHAN_IIO(IN7),
	PALMAS_ADC_CHAN_IIO(IN8),
	PALMAS_ADC_CHAN_IIO(IN9),
	PALMAS_ADC_CHAN_IIO(IN10),
	PALMAS_ADC_CHAN_IIO(IN11),
	PALMAS_ADC_CHAN_IIO(IN12),
	PALMAS_ADC_CHAN_IIO(IN13),
	PALMAS_ADC_CHAN_IIO(IN14),
	PALMAS_ADC_CHAN_IIO(IN15),
};

static int palmas_gpadc_get_autoconv_prop(struct device *dev,
		struct device_node *np, const char *node_name,
		struct palmas_adc_auto_conv_property **conv_prop)
{
	struct device_node *conv_node;
	struct palmas_adc_auto_conv_property *cprop;
	int ret;
	u32 pval;
	s32 thres;

	conv_node = of_get_child_by_name(np, node_name);
	if (!conv_node)
		return -EINVAL;

	cprop = devm_kzalloc(dev, sizeof(*cprop), GFP_KERNEL);
	if (!cprop)
		return -ENOMEM;

	ret = of_property_read_u32(conv_node, "ti,adc-channel-number", &pval);
	if (ret < 0) {
		dev_err(dev, "Autoconversion channel is missing\n");
		return ret;
	}
	cprop->adc_channel_number = pval;

	ret = of_property_read_s32(conv_node, "ti,adc-high-threshold", &thres);
	if (!ret)
		cprop->adc_high_threshold = thres;

	ret = of_property_read_s32(conv_node, "ti,adc-low-threshold", &thres);
	if (!ret)
		cprop->adc_low_threshold = thres;

	cprop->adc_shutdown = of_property_read_bool(conv_node,
			"ti,enable-shutdown");
	*conv_prop = cprop;
	return 0;
}

static int palmas_gpadc_get_adc_dt_data(struct platform_device *pdev,
	struct palmas_gpadc_platform_data **gpadc_pdata)
{
	struct device_node *np = pdev->dev.of_node;
	struct palmas_gpadc_platform_data *gp_data;
	struct palmas_adc_auto_conv_property *conv_prop;
	int ret;
	u32 pval;

	gp_data = devm_kzalloc(&pdev->dev, sizeof(*gp_data), GFP_KERNEL);
	if (!gp_data)
		return -ENOMEM;

	ret = of_property_read_u32(np, "ti,channel0-current-microamp", &pval);
	if (!ret)
		gp_data->ch0_current = pval;

	ret = of_property_read_u32(np, "ti,channel3-current-microamp", &pval);
	if (!ret)
		gp_data->ch3_current = pval;

	gp_data->ch3_dual_current = of_property_read_bool(np,
					"ti,enable-channel3-dual-current");

	gp_data->extended_delay = of_property_read_bool(np,
					"ti,enable-extended-delay");

	ret = of_property_read_u32(np, "ti,auto-conversion-period-ms", &pval);
	if (!ret)
		gp_data->auto_conversion_period_ms = pval;

	ret = palmas_gpadc_get_autoconv_prop(&pdev->dev, np, "auto_conv0",
				&conv_prop);
	if (!ret)
		gp_data->adc_auto_conv0_data = conv_prop;

	ret = palmas_gpadc_get_autoconv_prop(&pdev->dev, np, "auto_conv1",
				&conv_prop);
	if (!ret)
		gp_data->adc_auto_conv1_data = conv_prop;

	*gpadc_pdata = gp_data;
	return 0;
}

static int palmas_gpadc_probe(struct platform_device *pdev)
{
	struct palmas_gpadc *adc;
	struct palmas_platform_data *pdata;
	struct palmas_gpadc_platform_data *gpadc_pdata = NULL;
	struct iio_dev *iodev;
	int ret, i;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata && pdata->gpadc_pdata)
		gpadc_pdata = pdata->gpadc_pdata;

	if (!gpadc_pdata && pdev->dev.of_node) {
		ret = palmas_gpadc_get_adc_dt_data(pdev, &gpadc_pdata);
		if (ret < 0)
			return ret;
	}
	if (!gpadc_pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	iodev = iio_device_alloc(sizeof(*adc));
	if (!iodev) {
		dev_err(&pdev->dev, "iio_device_alloc failed\n");
		return -ENOMEM;
	}

	if (gpadc_pdata->iio_maps) {
		ret = iio_map_array_register(iodev, gpadc_pdata->iio_maps);
		if (ret < 0) {
			dev_err(&pdev->dev, "iio_map_array_register failed\n");
			goto out;
		}
	}

	adc = iio_priv(iodev);
	adc->dev = &pdev->dev;
	adc->palmas = dev_get_drvdata(pdev->dev.parent);
	adc->adc_info = palmas_gpadc_info;
	init_completion(&adc->conv_completion);
	dev_set_drvdata(&pdev->dev, iodev);

	adc->is_shutdown = false;
	mutex_init(&adc->lock);

	adc->auto_conversion_period = gpadc_pdata->auto_conversion_period_ms;
	adc->irq = palmas_irq_get_virq(adc->palmas, PALMAS_GPADC_EOC_SW_IRQ);
	ret = request_threaded_irq(adc->irq, NULL,
		palmas_gpadc_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(adc->dev),
		adc);
	if (ret < 0) {
		dev_err(adc->dev,
			"request irq %d failed: %dn", adc->irq, ret);
		goto out_unregister_map;
	}

	if (gpadc_pdata->adc_auto_conv0_data) {
		memcpy(&adc->auto_conv0_data, gpadc_pdata->adc_auto_conv0_data,
			sizeof(adc->auto_conv0_data));
		adc->auto_conv0_enable = true;
		adc->irq_auto_0 = palmas_irq_get_virq(adc->palmas,
				PALMAS_GPADC_AUTO_0_IRQ);
		ret = request_threaded_irq(adc->irq_auto_0, NULL,
				palmas_gpadc_irq_auto,
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas-adc-auto-0", adc);
		if (ret < 0) {
			dev_err(adc->dev, "request auto0 irq %d failed: %dn",
				adc->irq_auto_0, ret);
			goto out_irq_free;
		}
	}

	if (gpadc_pdata->adc_auto_conv1_data) {
		memcpy(&adc->auto_conv1_data, gpadc_pdata->adc_auto_conv1_data,
				sizeof(adc->auto_conv1_data));
		adc->auto_conv1_enable = true;
		adc->irq_auto_1 = palmas_irq_get_virq(adc->palmas,
				PALMAS_GPADC_AUTO_1_IRQ);
		ret = request_threaded_irq(adc->irq_auto_1, NULL,
				palmas_gpadc_irq_auto,
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas-adc-auto-1", adc);
		if (ret < 0) {
			dev_err(adc->dev, "request auto1 irq %d failed: %dn",
				adc->irq_auto_1, ret);
			goto out_irq_auto0_free;
		}
	}

	if (gpadc_pdata->ch0_current == 0)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_0;
	else if (gpadc_pdata->ch0_current <= 5)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_5;
	else if (gpadc_pdata->ch0_current <= 15)
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_15;
	else
		adc->ch0_current = PALMAS_ADC_CH0_CURRENT_SRC_20;

	if (gpadc_pdata->ch3_current == 0)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_0;
	else if (gpadc_pdata->ch3_current <= 10)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_10;
	else if (gpadc_pdata->ch3_current <= 400)
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_400;
	else
		adc->ch3_current = PALMAS_ADC_CH3_CURRENT_SRC_800;

	/* Init current source for CH0 and CH3 */
	ret = palmas_gpadc_set_current_src(adc, adc->ch0_current,
					   adc->ch3_current);
	if (ret < 0) {
		dev_err(adc->dev, "Failed to set current src: %d\n", ret);
		goto out_irq_auto1_free;
	}

	/* If ch3_dual_current is true, it will measure ch3 input signal with
	 * ch3_current and the next current of ch3_current. */
	adc->ch3_dual_current = gpadc_pdata->ch3_dual_current;
	if (adc->ch3_dual_current &&
			(adc->ch3_current == PALMAS_ADC_CH3_CURRENT_SRC_800)) {
		dev_warn(adc->dev,
			"Disable ch3_dual_current by wrong current setting\n");
		adc->ch3_dual_current = false;
	}

	adc->extended_delay = gpadc_pdata->extended_delay;

	iodev->name = MOD_NAME;
	iodev->dev.parent = &pdev->dev;
	iodev->info = &palmas_gpadc_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;
	iodev->channels = palmas_gpadc_iio_channel;
	iodev->num_channels = ARRAY_SIZE(palmas_gpadc_iio_channel);

	ret = iio_device_register(iodev);
	if (ret < 0) {
		dev_err(adc->dev, "iio_device_register() failed: %d\n", ret);
		goto out_irq_auto1_free;
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	for (i = 0; i < PALMAS_ADC_CH_MAX; i++) {
		if (!(adc->adc_info[i].is_correct_code))
			palmas_gpadc_calibrate(adc, i);
	}

	if (adc->auto_conv0_enable || adc->auto_conv1_enable)
		device_wakeup_enable(&pdev->dev);

	ret = palmas_gpadc_check_status(adc);
	if (ret < 0)
		goto out_irq_auto1_free;

	palmas_gpadc_auto_conv_reset(adc);
	ret = palmas_gpadc_auto_conv_configure(adc);
	if (ret < 0) {
		dev_err(adc->dev, "auto_conv_configure() failed: %d\n", ret);
		goto out_irq_auto1_free;
	}

	palmas_gpadc_debugfs_init(adc);
	return 0;

out_irq_auto1_free:
	if (gpadc_pdata->adc_auto_conv1_data)
		free_irq(adc->irq_auto_1, adc);
out_irq_auto0_free:
	if (gpadc_pdata->adc_auto_conv0_data)
		free_irq(adc->irq_auto_0, adc);
out_irq_free:
	free_irq(adc->irq, adc);
out_unregister_map:
	if (gpadc_pdata->iio_maps)
		iio_map_array_unregister(iodev);
out:
	iio_device_free(iodev);
	return ret;
}

static int palmas_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = dev_get_drvdata(&pdev->dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	struct palmas_platform_data *pdata = dev_get_platdata(pdev->dev.parent);

	debugfs_remove_recursive(adc->dentry);
	if (pdata->gpadc_pdata->iio_maps)
		iio_map_array_unregister(iodev);
	iio_device_unregister(iodev);
	free_irq(adc->irq, adc);
	if (adc->auto_conv0_enable)
		free_irq(adc->irq_auto_0, adc);
	if (adc->auto_conv1_enable)
		free_irq(adc->irq_auto_1, adc);
	iio_device_free(iodev);
	return 0;
}

static void palmas_gpadc_shutdown(struct platform_device *pdev)
{
	struct iio_dev *iodev = dev_get_drvdata(&pdev->dev);
	struct palmas_gpadc *adc = iio_priv(iodev);

	mutex_lock(&adc->lock);
	adc->is_shutdown = true;
	if (adc->auto_conv0_enable || adc->auto_conv1_enable)
		palmas_gpadc_auto_conv_reset(adc);
	mutex_unlock(&adc->lock);
}

#ifdef CONFIG_PM_SLEEP
static int palmas_gpadc_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	int wakeup = adc->auto_conv0_enable || adc->auto_conv1_enable;

	if (!device_may_wakeup(dev) || !wakeup)
		return 0;

	if (adc->auto_conv0_enable)
		enable_irq_wake(adc->irq_auto_0);

	if (adc->auto_conv1_enable)
		enable_irq_wake(adc->irq_auto_1);

	return 0;
}

static int palmas_gpadc_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct palmas_gpadc *adc = iio_priv(iodev);
	int wakeup = adc->auto_conv0_enable || adc->auto_conv1_enable;

	if (!device_may_wakeup(dev) || !wakeup)
		return 0;

	if (adc->auto_conv0_enable)
		disable_irq_wake(adc->irq_auto_0);

	if (adc->auto_conv1_enable)
		disable_irq_wake(adc->irq_auto_1);

	return 0;
};
#endif

static const struct dev_pm_ops palmas_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_gpadc_suspend,
				palmas_gpadc_resume)
};

static struct of_device_id of_palmas_gpadc_match_tbl[] = {
	{ .compatible = "ti,palmas-gpadc", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_palmas_gpadc_match_tbl);

static struct platform_driver palmas_gpadc_driver = {
	.probe = palmas_gpadc_probe,
	.remove = palmas_gpadc_remove,
	.shutdown = palmas_gpadc_shutdown,
	.driver = {
		.name = MOD_NAME,
		.owner = THIS_MODULE,
		.pm = &palmas_pm_ops,
		.of_match_table = of_palmas_gpadc_match_tbl,
	},
};

static int __init palmas_gpadc_init(void)
{
	return platform_driver_register(&palmas_gpadc_driver);
}
module_init(palmas_gpadc_init);

static void __exit palmas_gpadc_exit(void)
{
	platform_driver_unregister(&palmas_gpadc_driver);
}
module_exit(palmas_gpadc_exit);

MODULE_DESCRIPTION("palmas GPADC driver");
MODULE_AUTHOR("Pradeep Goudagunta<pgoudagunta@nvidia.com>");
MODULE_ALIAS("platform:palmas-gpadc");
MODULE_LICENSE("GPL v2");
