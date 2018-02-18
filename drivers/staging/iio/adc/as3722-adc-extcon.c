/*
 * as3722-adc-extcon.c -- AMS AS3722 ADC EXTCON.
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * Author: Mallikarjun Kasoju<mkasoju@nvidia.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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
#include <linux/extcon.h>
#include <linux/mfd/as3722.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>

#define AS3722_ADC_CHAN_IIO(chan)					\
{									\
	.datasheet_name = AS3722_DATASHEET_NAME(chan),			\
	.type = IIO_VOLTAGE,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.indexed = 1,							\
	.channel = AS3722_ADC_CH_##chan,				\
}

static const struct iio_chan_spec as3722_gpadc_iio_channel[] = {
	AS3722_ADC_CHAN_IIO(SD0),
	AS3722_ADC_CHAN_IIO(SD1),
	AS3722_ADC_CHAN_IIO(SD6),
	AS3722_ADC_CHAN_IIO(DTEMP),
	AS3722_ADC_CHAN_IIO(VSUP),
	AS3722_ADC_CHAN_IIO(GPIO1),
	AS3722_ADC_CHAN_IIO(GPIO2),
	AS3722_ADC_CHAN_IIO(GPIO3),
	AS3722_ADC_CHAN_IIO(GPIO4),
	AS3722_ADC_CHAN_IIO(GPIO6),
	AS3722_ADC_CHAN_IIO(GPIO7),
	AS3722_ADC_CHAN_IIO(VBAT),
	AS3722_ADC_CHAN_IIO(ADC1),
	AS3722_ADC_CHAN_IIO(ADC2),
	AS3722_ADC_CHAN_IIO(TBD1),
	AS3722_ADC_CHAN_IIO(TBD2),
	AS3722_ADC_CHAN_IIO(T1SD0),
	AS3722_ADC_CHAN_IIO(T2SD0),
	AS3722_ADC_CHAN_IIO(T3SD0),
	AS3722_ADC_CHAN_IIO(T4SD0),
	AS3722_ADC_CHAN_IIO(TSD1),
	AS3722_ADC_CHAN_IIO(T1SD6),
	AS3722_ADC_CHAN_IIO(T2SD6),
};

struct as3722_adc {
	struct device				*dev;
	struct as3722				*as3722;
	struct extcon_dev			edev;
	struct iio_map				*iio_maps;
	struct as3722_adc_auto_conv_property	auto_conv1_data;
	int					irq;
};

const char *as3722_adc_excon_cable[] = {
	[0] = "USB-Host",
	NULL,
};

static int as3722_adc_start_conversion(struct as3722_adc *adc, int chan)
{
	struct as3722 *as3722 = adc->as3722;
	unsigned int val;
	int ret;
	int timeout = 10;

	/* configure ADC0 */
	ret = as3722_update_bits(as3722, AS3722_ADC0_CONTROL_REG,
			AS3722_ADC_MASK_SOURCE_SELECT,
			chan);
	if (ret < 0) {
		dev_err(adc->dev, "ADC0_CONTROL source select fail %d\n", ret);
		return ret;
	}
	/* Start ADC */
	ret = as3722_update_bits(as3722, AS3722_ADC0_CONTROL_REG,
				AS3722_ADC_MASK_CONV_START,
				AS3722_ADC_MASK_CONV_START);
	if (ret < 0) {
		dev_err(adc->dev, "ADC0_CONTROL write failed %d\n", ret);
		return ret;
	}

	/* Wait for conversion */
	do {
		ret = as3722_read(as3722, AS3722_ADC0_MSB_RESULT_REG,
				&val);
		if (ret < 0) {
			dev_err(adc->dev,
					"Reg 0x%02x read failed: %d\n",
					AS3722_ADC0_MSB_RESULT_REG, ret);
			return ret;
		}
		if (!(val & AS3722_ADC_MASK_CONV_NOTREADY))
			return 0;  /* result available */
		udelay(100);
	} while (--timeout > 0);

	return -ETIMEDOUT;
}


static int as3722_adc_extcon_read_raw(struct iio_dev *iodev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct as3722_adc *adc = iio_priv(iodev);
	struct as3722 *as3722 = adc->as3722;
	int adc_chan = chan->channel;
	int result;
	int ret = 0;

	if (adc_chan > AS3722_ADC_CH_MAX)
		return -EINVAL;

	mutex_lock(&iodev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = as3722_adc_start_conversion(adc, adc_chan);
		if (ret < 0) {
			dev_err(adc->dev,
				"failed to start conversion %d\n", ret);
			break;
		}
		ret = as3722_read(as3722, AS3722_ADC0_MSB_RESULT_REG , &result);
		if (ret < 0) {
			dev_err(adc->dev,
				"ADC1_MSB_RESULT read failed %d\n", ret);
			break;
		}
		*val = ((result & AS3722_ADC_MASK_MSB_VAL) << 3);
		ret = as3722_read(as3722, AS3722_ADC0_LSB_RESULT_REG, &result);
		if (ret < 0) {
			dev_err(adc->dev,
				"ADC1_LSB_RESULT read failed %d\n", ret);
			break;
		}
		*val |= result & AS3722_ADC_MASK_LSB_VAL;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&iodev->mlock);
	return ret;
}


static int as3722_read_adc1_cable_update(struct as3722_adc *adc)
{
	struct as3722_adc_auto_conv_property *conv_prop =
					&adc->auto_conv1_data;
	struct as3722 *as3722 = adc->as3722;
	int result, val;
	int ret;

	ret = as3722_read(as3722, AS3722_ADC1_MSB_RESULT_REG , &val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_MSB_RESULT read failed %d\n", ret);
		return ret;
	}
	result = ((val & AS3722_ADC_MASK_MSB_VAL) << 3);

	ret = as3722_read(as3722, AS3722_ADC1_LSB_RESULT_REG, &val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_LSB_RESULT read failed %d\n", ret);
		return ret;
	}
	result |= val & AS3722_ADC_MASK_LSB_VAL;
	if (result >= conv_prop->hi_threshold) {
		extcon_set_cable_state(&adc->edev, "USB-Host", false);
		dev_info(adc->dev, "USB-Host is disconnected\n");
	} else if (result <= conv_prop->low_threshold) {
		extcon_set_cable_state(&adc->edev, "USB-Host", true);
		dev_info(adc->dev, "USB-Host is connected\n");
	}
	return ret;
}

static irqreturn_t as3722_adc_extcon_irq(int irq, void *data)
{
	as3722_read_adc1_cable_update(data);
	return IRQ_HANDLED;
}

static int as3722_gpadc_configure(struct as3722_adc *adc)
{
	struct as3722_adc_auto_conv_property *conv_prop =
					&adc->auto_conv1_data;
	struct as3722 *as3722 = adc->as3722;
	int ret;
	int val;

	/* Set ADC threshold values */
	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_HI_MSB_REG,
				(conv_prop->hi_threshold >> 3) & 0x7F);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_HI_MSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_HI_LSB_REG,
				(conv_prop->hi_threshold) & 0x7);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_HI_LSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_LO_MSB_REG,
				(conv_prop->low_threshold >> 3) & 0x7F);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_LO_MSB write failed %d\n",
				ret);
		return ret;
	}

	ret = as3722_write(as3722, AS3722_ADC1_THRESHOLD_LO_LSB_REG,
				(conv_prop->low_threshold) & 0x7);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_THRESHOLD_LO_LSB write failed %d\n",
				ret);
		return ret;
	}

	/* Configure adc1 */
	val = (conv_prop->adc_channel & AS3722_ADC_MASK_SOURCE_SELECT) |
			AS3722_ADC1_MASK_INTEVAL_SCAN;
	if (conv_prop->enable_low_voltage_range)
		val |= AS3722_ADC_MASK_LOW_VOLTAGE_RANGE;
	ret = as3722_write(as3722, AS3722_ADC1_CONTROL_REG, val);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_CONTROL write failed %d\n", ret);
		return ret;
	}
	return 0;

}

static int as3722_gpadc_get_adc_dt_data(struct platform_device *pdev,
	struct as3722_adc *adc)
{
	struct device_node *node;
	struct as3722_adc_auto_conv_property *conv_prop = &adc->auto_conv1_data;

	node = of_get_child_by_name(pdev->dev.parent->of_node, "adc_extcon");
	if (!node) {
		dev_err(&pdev->dev, "Device is not having adc_extcon node\n");
		return -ENODEV;
	}
	pdev->dev.of_node = node;
	of_property_read_string(node, "ams,extcon-name",
			&conv_prop->connection_name);
	conv_prop->enable_adc1_continuous_mode =
		of_property_read_bool(node,
				"ams,enable-adc1-continuous-mode");
	conv_prop->enable_low_voltage_range =
		of_property_read_bool(node,
				"ams,enable-low-voltage-range");
	of_property_read_u32(node, "ams,adc-channel",
			&conv_prop->adc_channel);
	of_property_read_u32(node, "ams,hi-threshold",
			&conv_prop->hi_threshold);
	of_property_read_u32(node, "ams,low-threshold",
			&conv_prop->low_threshold);

	return 0;
}

static const struct iio_info as3722_gpadc_iio_info = {
	.read_raw = as3722_adc_extcon_read_raw,
	.driver_module = THIS_MODULE,
};

static int as3722_adc_extcon_probe(struct platform_device *pdev)
{
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct as3722_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct as3722_adc *adc;
	struct iio_dev *iodev;
	unsigned int try_counter = 0;
	int val;
	int ret = 0;

	iodev = iio_device_alloc(sizeof(*adc));
	if (!iodev) {
		dev_err(&pdev->dev, "iio_device_alloc failed\n");
		return -ENOMEM;
	}
	adc = iio_priv(iodev);
	ret = as3722_gpadc_get_adc_dt_data(pdev, adc);
	if (ret  < 0) {
		if (pdata && pdata->extcon_pdata)
			adc->auto_conv1_data =
				pdata->extcon_pdata->adc1_auto_conv_data;
		else {
			dev_err(&pdev->dev, "no platform data available\n");
			return -ENODEV;
		}
	}

	adc->dev = &pdev->dev;
	adc->as3722 = as3722;
	dev_set_drvdata(&pdev->dev, iodev);
	adc->irq = platform_get_irq(pdev, 0);
	if (adc->iio_maps) {
		ret = iio_map_array_register(iodev, adc->iio_maps);
		if (ret < 0) {
			dev_err(&pdev->dev, "iio_map_array_register failed\n");
			goto out;
		}
	}

	if (!adc->auto_conv1_data.enable_adc1_continuous_mode)
		goto skip_adc_config;

	ret = as3722_gpadc_configure(adc);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"adc1 extcon property setting failed %d\n", ret);
		goto out;
	}

	/* Start ADC */
	ret = as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
					AS3722_ADC_MASK_CONV_START,
					AS3722_ADC_MASK_CONV_START);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1_CONTROL write failed %d\n", ret);
		return ret;
	}
	/* Wait for 1 conversion */
	do {
		ret = as3722_read(as3722, AS3722_ADC1_MSB_RESULT_REG ,
				&val);
		if (ret < 0) {
			dev_err(adc->dev, "ADC1_MSB_RESULT read failed %d\n",
				ret);
			return ret;
		}
		if (!(val & AS3722_ADC_MASK_CONV_NOTREADY))
			break;
		udelay(500);
	} while (try_counter++ < 10);
	adc->edev.name = (adc->auto_conv1_data.connection_name) ?
			adc->auto_conv1_data.connection_name : pdev->name;
	adc->edev.dev.parent = &pdev->dev;
	adc->edev.supported_cable = as3722_adc_excon_cable;
	adc->edev.node = pdev->dev.of_node;
	ret = extcon_dev_register(&adc->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "extcon dev register failed %d\n", ret);
		goto out;
	}

	/* Read ADC result */
	ret = as3722_read_adc1_cable_update(adc);
	if (ret < 0) {
		dev_err(&pdev->dev, "ADC read failed %d\n", ret);
		goto scrub_edev;
	}

	ret = as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
		AS3722_ADC1_MASK_INTEVAL_SCAN, AS3722_ADC1_MASK_INTEVAL_SCAN);
	if (ret < 0) {
		dev_err(adc->dev, "ADC1 INTEVAL_SCAN set failed: %d\n", ret);
		goto scrub_edev;
	}

	iodev->name = "as3722-adc-extcon";
	iodev->dev.parent = &pdev->dev;
	iodev->info = &as3722_gpadc_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;
	iodev->channels = as3722_gpadc_iio_channel;
	iodev->num_channels = ARRAY_SIZE(as3722_gpadc_iio_channel);

	ret = iio_device_register(iodev);
	if (ret < 0) {
		dev_err(adc->dev, "iio_device_register() failed: %d\n", ret);
		goto scrub_edev;
	}
	ret = request_threaded_irq(adc->irq, NULL, as3722_adc_extcon_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(adc->dev),
		adc);
	if (ret < 0) {
		dev_err(adc->dev, "request irq %d failed: %dn", adc->irq, ret);
		goto stop_adc1;
	}

skip_adc_config:
	device_init_wakeup(&pdev->dev, 1);
	return 0;

stop_adc1:
	as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
			AS3722_ADC_MASK_CONV_START, 0);
	iio_device_unregister(iodev);
scrub_edev:
	extcon_dev_unregister(&adc->edev);

out:
	iio_device_free(iodev);
	return ret;
}

static int as3722_adc_extcon_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = dev_get_drvdata(&pdev->dev);
	struct as3722_adc *adc = iio_priv(iodev);
	struct as3722 *as3722 = adc->as3722;

	as3722_update_bits(as3722, AS3722_ADC1_CONTROL_REG,
			AS3722_ADC_MASK_CONV_START, 0);
	extcon_dev_unregister(&adc->edev);
	if (adc->iio_maps)
		iio_map_array_unregister(iodev);
	iio_device_unregister(iodev);
	free_irq(adc->irq, adc);
	iio_device_free(iodev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int as3722_adc_extcon_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct as3722_adc *adc = iio_priv(iodev);

	if (device_may_wakeup(dev))
		enable_irq_wake(adc->irq);

	return 0;
}

static int as3722_adc_extcon_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_get_drvdata(dev);
	struct as3722_adc *adc = iio_priv(iodev);

	if (device_may_wakeup(dev))
		disable_irq_wake(adc->irq);

	return 0;
};
#endif

static const struct dev_pm_ops as3722_adc_extcon_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(as3722_adc_extcon_suspend,
				as3722_adc_extcon_resume)
};

static struct platform_driver as3722_adc_extcon_driver = {
	.probe = as3722_adc_extcon_probe,
	.remove = as3722_adc_extcon_remove,
	.driver = {
		.name = "as3722-adc-extcon",
		.owner = THIS_MODULE,
		.pm = &as3722_adc_extcon_pm_ops,
	},
};

static int __init as3722_adc_extcon_init(void)
{
	return platform_driver_register(&as3722_adc_extcon_driver);
}

subsys_initcall_sync(as3722_adc_extcon_init);

static void __exit as3722_adc_extcon_exit(void)
{
	platform_driver_unregister(&as3722_adc_extcon_driver);
}
module_exit(as3722_adc_extcon_exit);

MODULE_DESCRIPTION("as3722 ADC extcon driver");
MODULE_AUTHOR("Mallikarjun Kasoju <mkasoju@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_AUTHOR("Bibek Basu <bbasu@nvidia.com>");
MODULE_ALIAS("platform:as3722-adc-extcon");
MODULE_LICENSE("GPL v2");
