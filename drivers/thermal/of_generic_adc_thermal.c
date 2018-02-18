/*
 *
 * OF Generic ADC thermal driver
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation. All rights reserved.
 *
 * Based on thermal/generic-adc-thermal.c by
 *	Jinyoung Park <jinyoungp@nvidia.com>
 *
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
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/thermal.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>

enum gpadc_temp_table_type {
	ADC_TEMP_UNKNOWN,
	ADC_TEMP_LINEAR_SINGLE,
	ADC_TEMP_LINEAR_MIN_MAX_AVG,
	ADC_TEMP_TDIODE_EQN,
	ADC_TEMP_LINEAR_SINGLE_REF,
};

struct gpadc_thermal_info {
	struct device *dev;
	struct gpadc_thermal_platform_data *pdata;
	struct thermal_zone_device *tz_dev;
	struct iio_channel *channel;
	struct iio_channel *ref_channel;
	int temp_offset;
	bool dual_mode;
	enum gpadc_temp_table_type table_type;
};

struct gpadc_thermal_platform_data {
	int lower_temp;
	int upper_temp;
	int step_temp;
	int n_temp;
	u32 *lookup_table;
	s64 tdiode_m1;
	s64 tdiode_b1;
	s64 tdiode_m2;
	s64 tdiode_b2;
	int tdiode_k;
};

static int gpadc_thermal_read_linear_lookup_table(struct device *dev,
		enum gpadc_temp_table_type table_type,
		struct gpadc_thermal_platform_data **plat_data)
{
	struct gpadc_thermal_platform_data *pdata;
	s32 l_temp;
	s32 u_temp;
	s32 st_temp;
	s32 range_temp;
	int n_temp;
	u32 *out_buff;
	int ret;
	int entry_size;

	ret = of_property_read_s32(dev->of_node, "lower-temperature", &l_temp);
	if (ret < 0) {
		dev_err(dev, "Lower temp not found\n");
		return ret;
	}

	ret = of_property_read_s32(dev->of_node, "upper-temperature", &u_temp);
	if (ret < 0) {
		dev_err(dev, "Upper temp not found\n");
		return ret;
	}

	ret = of_property_read_s32(dev->of_node, "step-temperature", &st_temp);
	if (ret < 0) {
		dev_err(dev, "Steps temp not found\n");
		return ret;
	}

	range_temp = abs(u_temp - l_temp);

	if (range_temp % st_temp) {
		dev_err(dev, "Steps does not meet with lower/upper\n");
		return -EINVAL;
	}

	n_temp = range_temp / st_temp;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	switch (table_type) {
	case ADC_TEMP_LINEAR_SINGLE:
	case ADC_TEMP_LINEAR_SINGLE_REF:
		entry_size = 1;
		break;
	case ADC_TEMP_LINEAR_MIN_MAX_AVG:
		entry_size = 3;
		break;
	default:
		return -EINVAL;
	}

	out_buff = devm_kzalloc(dev, entry_size * sizeof(*out_buff) * n_temp,
			GFP_KERNEL);
	if (!out_buff)
		return -ENOMEM;


	ret = of_property_read_u32_array(dev->of_node, "adc-temperature-map",
			out_buff, n_temp * entry_size);
	if (ret < 0) {
		dev_err(dev, "ADC to temp map read failed: %d\n", ret);
		return ret;
	}

	pdata->lower_temp = l_temp;
	pdata->upper_temp = u_temp;
	pdata->step_temp = st_temp;
	pdata->lookup_table = out_buff;
	pdata->n_temp = n_temp;
	*plat_data = pdata;
	return 0;
}

static int gpadc_thermal_read_tdidoe_eqn_coeff(struct device *dev,
		enum gpadc_temp_table_type table_type,
		struct gpadc_thermal_platform_data **plat_data)
{
	struct gpadc_thermal_platform_data *pdata;
	s32 pvals32;
	u64 pval64;
	u32 pval32;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_property_read_s32(dev->of_node, "lower-temperature", &pvals32);
	if (ret < 0) {
		dev_err(dev, "Lower temp not found: %d\n", ret);
		return ret;
	}
	pdata->lower_temp = pvals32;

	ret = of_property_read_s32(dev->of_node, "upper-temperature", &pvals32);
	if (ret < 0) {
		dev_err(dev, "Upper temp not found: %d\n", ret);
		return ret;
	}
	pdata->upper_temp = pvals32;

	ret = of_property_read_u64(dev->of_node, "tdiode-m1", &pval64);
	if (ret < 0) {
		dev_err(dev, "tdidoe-m1 not found: %d\n", ret);
		return ret;
	}
	pdata->tdiode_m1 = (s64)pval64;

	ret = of_property_read_u64(dev->of_node, "tdiode-m2", &pval64);
	if (ret < 0) {
		dev_err(dev, "tdidoe-m2 not found: %d\n", ret);
		return ret;
	}
	pdata->tdiode_m2 = (s64)pval64;

	ret = of_property_read_u64(dev->of_node, "tdiode-b1", &pval64);
	if (ret < 0) {
		dev_err(dev, "tdidoe-b1 not found: %d\n", ret);
		return ret;
	}
	pdata->tdiode_b1 = (s64)pval64;

	ret = of_property_read_u64(dev->of_node, "tdiode-b2", &pval64);
	if (ret < 0) {
		dev_err(dev, "tdidoe-b2 not found: %d\n", ret);
		return ret;
	}
	pdata->tdiode_b2 = (s64)pval64;

	ret = of_property_read_u32(dev->of_node, "tdiode-k", &pval32);
	if (ret < 0) {
		dev_err(dev, "tdidoe-k not found: %d\n", ret);
		return ret;
	}
	pdata->tdiode_k = (int)pval32;

	dev_info(dev, "TDiode Parameters:\n");
	dev_info(dev, "m1: %llx, m2: %llx, b1: %llx, b2: %llx, k: %d\n",
		pdata->tdiode_m1, pdata->tdiode_m2, pdata->tdiode_b1,
		pdata->tdiode_b2, pdata->tdiode_k);

	*plat_data = pdata;
	return 0;
}

static int gpadc_thermal_parse_dt(struct platform_device *pdev,
	struct gpadc_thermal_info *gti)
{
	s32 temp_offset = 0;
	char const *pstr;
	int ret;

	ret = of_property_read_s32(pdev->dev.of_node, "temperature-offset",
				&temp_offset);
	if (!ret)
		gti->temp_offset = temp_offset;

	gti->dual_mode = of_property_read_bool(pdev->dev.of_node,
					"dual-mode-adc-read");
	gti->table_type = ADC_TEMP_UNKNOWN;
	ret = of_property_read_string(pdev->dev.of_node,
			"adc-temp-lookup-table", &pstr);
	if (!ret) {
		if (!strcmp(pstr, "linear-table"))
			gti->table_type = ADC_TEMP_LINEAR_SINGLE;
		else if (!strcmp(pstr, "linear-table-ref-channel"))
			gti->table_type = ADC_TEMP_LINEAR_SINGLE_REF;
		else if (!strcmp(pstr, "linear-table-min-max-avg"))
			gti->table_type = ADC_TEMP_LINEAR_MIN_MAX_AVG;
		else if (!strcmp(pstr, "tdiode-equation"))
			gti->table_type = ADC_TEMP_TDIODE_EQN;
	}

	switch (gti->table_type) {
	case ADC_TEMP_LINEAR_SINGLE:
	case ADC_TEMP_LINEAR_SINGLE_REF:
	case ADC_TEMP_LINEAR_MIN_MAX_AVG:
		ret = gpadc_thermal_read_linear_lookup_table(&pdev->dev,
				gti->table_type, &gti->pdata);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"ADC_Temp lookuptable not proper: %d\n", ret);
			return ret;
		}
		break;
	case ADC_TEMP_TDIODE_EQN:
		ret = gpadc_thermal_read_tdidoe_eqn_coeff(&pdev->dev,
				gti->table_type, &gti->pdata);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"TDiode  params are not proper: %d\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(&pdev->dev, "Non supported table type\n");
		return -EINVAL;
	}

	return 0;
}

static int gpadc_thermal_read_channel(struct gpadc_thermal_info *gti,
				int *val, int *val2)
{
	int ret;

	if (gti->dual_mode) {
		ret = iio_read_channel_processed_dual(gti->channel, val,
							val2);
		if (ret < 0)
			ret = iio_read_channel_raw_dual(gti->channel, val,
							val2);
	} else {
		ret = iio_read_channel_processed(gti->channel, val);
		if (ret < 0)
			ret = iio_read_channel_raw(gti->channel, val);
		if (ret < 0)
			return ret;

		if (gti->ref_channel) {
			ret = iio_read_channel_processed(gti->ref_channel,
							val2);
			if (ret < 0)
				ret = iio_read_channel_raw(gti->ref_channel,
							val2);
		}
	}
	return ret;
}

static int gpadc_thermal_adc_to_temp_linear_range(
		struct gpadc_thermal_info *gti, int val)
{
	int temp, i, f_part = 0;

	for (i = 0; i < gti->pdata->n_temp; i++) {
		if (val >= gti->pdata->lookup_table[i * 3 + 1])
			break;
	}

	if (i == 0) {
		temp = gti->pdata->lower_temp;
	} else if (i >= (gti->pdata->n_temp - 1)) {
		temp = gti->pdata->upper_temp;
	} else {
		/* Find floating part with interpolate computing */
		int min, max, avg;

		min = gti->pdata->lookup_table[i * 3];
		max = gti->pdata->lookup_table[i * 3 + 2];
		avg = gti->pdata->lookup_table[i * 3 + 1];
		f_part = ((val - min) * 10) / (max - min);
		temp = gti->pdata->lower_temp + i * gti->pdata->step_temp;
		temp = (temp * 10) + (f_part * 1000);
	}

	return temp / 10;
}

static int gpadc_thermal_adc_to_temp_linear_single(
		struct gpadc_thermal_info *gti, int val)
{
	int temp = 0, adc_hi, adc_lo;
	int i;

	for (i = 0; i < gti->pdata->n_temp; i++)
		if (val >= gti->pdata->lookup_table[i])
			break;

	if (i == 0) {
		temp = gti->pdata->lower_temp;
	} else if (i >= (gti->pdata->n_temp - 1)) {
		temp = gti->pdata->upper_temp;
	} else {
		adc_hi = gti->pdata->lookup_table[i - 1];
		adc_lo = gti->pdata->lookup_table[i];
		temp = (gti->pdata->lower_temp + i * gti->pdata->step_temp);
		temp -= ((val - adc_lo) * 1000 / (adc_hi - adc_lo));
	}
	return temp;
}

#define TDIODE_PRECISION_MULTIPLIER (1000000000LL)
static long gpadc_thermal_adc_to_temp_tdidoe_eqn(struct gpadc_thermal_info *gti,
		int val1, int val2)
{
	s64 m1 = gti->pdata->tdiode_m1;
	s64 m2 = gti->pdata->tdiode_m2;
	s64 b1 = gti->pdata->tdiode_b1;
	s64 b2 = gti->pdata->tdiode_b2;
	int k = gti->pdata->tdiode_k;
	s64 temp = TDIODE_PRECISION_MULTIPLIER;
	s32 temp_l;
	int val;

	/* diode temp = ((adc2 - k * adc1) - (b2 - k * b1)) / (m2 - k * m1) */
	val = val2 - k * val1;
	temp *= val;
	temp -= (b2 - k * b1);
	temp = div64_s64(temp, (m2 - k * m1));

	temp_l = (s32) temp;
	temp_l = max_t(s32, temp_l, gti->pdata->lower_temp);
	temp_l = min_t(s32, temp_l, gti->pdata->upper_temp);
	return temp_l;
}

static int gpadc_thermal_get_temp(void *data, long *temp)
{
	struct gpadc_thermal_info *gti = data;
	int val = 0, val2 = 0;
	int final_val;
	int ret;

	ret = gpadc_thermal_read_channel(gti, &val, &val2);
	if (ret < 0) {
		dev_err(gti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}

	switch (gti->table_type) {
	case ADC_TEMP_LINEAR_SINGLE:
		 *temp = gpadc_thermal_adc_to_temp_linear_single(gti, val);
		break;

	case ADC_TEMP_LINEAR_SINGLE_REF:
		if (!val2)
			return -EINVAL;
		final_val = DIV_ROUND_UP((val << 16), val2);
		 *temp = gpadc_thermal_adc_to_temp_linear_single(gti,
						final_val);
		break;

	case ADC_TEMP_LINEAR_MIN_MAX_AVG:
		*temp = gpadc_thermal_adc_to_temp_linear_range(gti, val);
		break;

	case ADC_TEMP_TDIODE_EQN:
		*temp = gpadc_thermal_adc_to_temp_tdidoe_eqn(gti, val, val2);
		break;

	default:
		return -EINVAL;
	}
	*temp += gti->temp_offset;
	return 0;
}

static int gpadc_thermal_probe(struct platform_device *pdev)
{
	struct gpadc_thermal_info *gti;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	gti = devm_kzalloc(&pdev->dev, sizeof(*gti), GFP_KERNEL);
	if (!gti)
		return -ENOMEM;

	platform_set_drvdata(pdev, gti);
	gti->dev = &pdev->dev;

	ret = gpadc_thermal_parse_dt(pdev, gti);
	if (ret < 0) {
		dev_err(&pdev->dev, "parsing of DT node  failed: %d\n", ret);
		return ret;
	}

	gti->channel = iio_channel_get(&pdev->dev, "sensor-channel");
	if (IS_ERR(gti->channel)) {
		ret = PTR_ERR(gti->channel);
		dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	gti->ref_channel = iio_channel_get(&pdev->dev, "ref-channel");
	if (IS_ERR(gti->ref_channel)) {
		ret = PTR_ERR(gti->ref_channel);
		if (ret == -EPROBE_DEFER) {
			dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
			goto scrub_ref;
		}
		gti->ref_channel = NULL;
	}

	gti->tz_dev = thermal_zone_of_sensor_register(&pdev->dev, 0,
				gti, gpadc_thermal_get_temp, NULL);
	if (IS_ERR(gti->tz_dev)) {
		ret = PTR_ERR(gti->tz_dev);
		dev_err(&pdev->dev,
			"Thermal zone of sensor register failed: %d\n", ret);
		goto scrub;
	}
	dev_info(&pdev->dev, "Thermal sensor registerd for thermal zone %s\n",
			gti->tz_dev->type);
	return 0;

scrub:
	if (gti->ref_channel)
		iio_channel_release(gti->ref_channel);
scrub_ref:
	iio_channel_release(gti->channel);
	return ret;
}

static int gpadc_thermal_remove(struct platform_device *pdev)
{
	struct gpadc_thermal_info *gti = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(&pdev->dev, gti->tz_dev);
	iio_channel_release(gti->channel);
	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "generic-adc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver gpadc_thermal_driver = {
	.driver = {
		.name = "of-generic-adc-thermal",
		.owner = THIS_MODULE,
		.of_match_table = of_adc_thermal_match,
	},
	.probe = gpadc_thermal_probe,
	.remove = gpadc_thermal_remove,
};

module_platform_driver(gpadc_thermal_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("Generic ADC thermal driver using IIO framework with DT");
MODULE_LICENSE("GPL v2");
