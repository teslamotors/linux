/* Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* The NVS = NVidia Sensor framework */
/* This common NVS ALS module allows, along with the NVS IIO common module, an
 * ALS driver to offload the code interacting with IIO and ALS reporting, and
 * just have code that interacts with the HW.
 * The commonality between this module and the NVS ALS driver is the nvs_light
 * structure.  It is expected that the NVS ALS driver will:
 * - call nvs_light_enable when the device is enabled to initialize variables.
 * - read the HW and place the value in nvs_light.hw
 * - call nvs_light_read
 * - depending on the nvs_light_read return value:
 *     - -1 = poll HW using nvs_light.poll_delay_ms delay.
 *     - 0 = if interrupt driven, do nothing or resume regular polling
 *     - 1 = set new thresholds using the nvs_light.hw_thresh_lo/hi
 * Reporting the lux is handled within this module.
 * See nvs_light.h for nvs_light structure details.
 */
/* The NVS HAL will use the IIO scale and offset sysfs attributes to modify the
 * data using the following formula: (data * scale) + offset
 * A scale value of 0 disables scale.
 * A scale value of 1 puts the NVS HAL into calibration mode where the scale
 * and offset are read everytime the data is read to allow realtime calibration
 * of the scale and offset values to be used in the device tree parameters.
 * Keep in mind the data is buffered but the NVS HAL will display the data and
 * scale/offset parameters in the log.  See calibration steps below.
 */
/* The configuration threshold values are HW value based.  In other words, to
 * obtain the upper and lower HW thresholds, the configuration threshold is
 * simply added or subtracted from the HW data read, respectively.
 * A little history about this: this code originally expected the configuration
 * threshold values to be in lux.  It then converted the threshold lux value to
 * a HW value by reversing the calibrated value to uncalibrated and then the
 * scaling of the resolution.  The idea was to make configuration easy by just
 * setting how much lux needed to change before reporting.  However, there were
 * two issues with this method:
 * 1) that's a lot of overhead for each sample, and 2) the lux range isn't
 * exactly linear.  Lux values in a dark room will probably want to be reported
 * every +/- 100 or 10 if not less.  This is opposed to a bright room or even
 * outside on a sunny day where lux value changes can be reported every
 * +/- 1000 or even 10000.  Since many ALS's have dynamic resolution, changing
 * the range depending on the lux reading, it makes sense to use HW threshold
 * values that will automatically scale with the HW resolution used.
 */
/* NVS light drivers have two calibration mechanisms.  Method 1 is required if
 * the driver is using dynamic resolution since the resolution cannot be read
 * by the NVS HAL on every data value read due to buffering.  So instead.a
 * mechanism allows floating point to be calculated here in the kernel by
 * shifting up to integer the floating point significant amount.  This allows
 * real-time resolution changes without the NVS HAL having to synchronize to
 * the actual resolution for each datum.  The scale.fval must be a 10 base
 * value, e.g. 0.1, 0.01, ... 0.000001, etc. as the significant amount.
 * The NVS HAL will then convert the value to float by multiplying the integer
 * float-data with scale.
 * Method 1:
 * This method uses interpolation and requires a low and high uncalibrated
 * value along with the corresponding low and high calibrated values.  The
 * uncalibrated values are what is read from the sensor in the steps below.
 * The corresponding calibrated values are what the correct value should be.
 * All values are programmed into the device tree settings.
 * 1. Read scale sysfs attribute.  This value will need to be written back.
 * 2. Disable device.  (Write 0 = buffer/enable sysfs attribute)
 * 3. Write 1 to the scale sysfs attribute (e.g. in_illuminance_scale).
 * 4. Enable device. (Write 1 = buffer/enable sysfs attribute)
 * 5. The NVS HAL will announce in the 'logcat -v time | grep -i "sensors"'
 *    that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 6. Write the scale value read in step 1 back to the scale sysfs attribute.
 * 7. Put the device into a state where the data read is a low value.
 *    (Generate light source < 100 lux.)
 * 8. Note the values displayed in the log.  Separately measure the actual
 *    value with a high accuracy light meter.  Make sure the light meter
 *    is calibrated to the same color temperature as your light source.
 *    The value from the sensor will be the uncalibrated value and the
 *    separately measured value will be the calibrated value for the current
 *    state (low or high values).
 *    Device Tree nodes related to light calibration:
 *    light_uncalibrated_lo <-- Value found in logcat.
 *    light_calibrated_lo <-- Value read from light meter.
 * 9. Put the device into a state where the data read is a high value.
 *    (Generate light source > 3000 lux.)
 * 10. Repeat step 8.
 *    Device Tree nodes related to light calibration:
 *    light_uncalibrated_hi <-- Value found in logcat.
 *    light_calibrated_hi <-- Value read from light meter.
 * 11. Enter the values in the device tree settings for the device.  Both
 *     calibrated and uncalibrated values will be the values before scale and
 *     offset are applied.
 *     The light sensor has the following device tree parameters for this:
 *     light_uncalibrated_lo
 *     light_calibrated_lo
 *     light_uncalibrated_hi
 *     light_calibrated_hi
 *
 * To test calibration values in realtime.
 * An NVS ALS driver may support a simplified version of method 1 that can be
 * used in realtime:
 * 1. At step 8, while the light source is still at this low value,
 *    write the light meter value to the in_illuminance_threshold_low
 *    attribute.  When in calibration mode this value will be written to the
 *    light_calibrated_lo and the current lux written to light_uncalibrated_lo
 *    internal to the driver.
 * 2. If this is after step 9, while the light source is still at this high
 *    value, write the light meter value to the in_illuminance_threshold_high
 *    attribute.
 * 3. To confirm the realtime values and see what the driver used for
 *    uncalibrated values, do the following at the adb prompt in the driver
 *    space:
 *    # echo 5 > nvs
 *    # cat nvs
 *    This will be a partial dump of the sensor's configuration structure that
 *    will show the calibrated and uncalibrated values.  For example:
 *    ...
 *    uncal_lo=52	<-- Program this value into DT: light_uncalibrated_lo
 *    uncal_hi=1627	<-- Program this value into DT: light_uncalibrated_hi
 *    cal_lo=8050	<-- Program this value into DT: light_calibrated_lo
 *    cal_hi=308000	<-- Program this value into DT: light_calibrated_hi
 *    thresh_lo=10
 *    thresh_hi=10
 *    ...
 * Note that the calibrated value must be the value before the scale and offset
 * is applied.  For example, if the calibrated lux reading is 123.4 lux, and
 * the in_illuminance_scale is normally 0.01, then the value entered is 12340
 * which will be 123.4 lux when the scale is applied at the HAL layer.
 *
 * If the thresholds have changed instead of the calibration settings, then
 * the driver doesn't support this feature.
 * In order to display raw values, interpolation, that uses the calibration
 * values, is not executed by the driver when in calibration mode, so to test,
 * disable and reenable the device to exit calibration mode and test the new
 * calibration values.
 *
 *
 * Method 2 can only be used if dynamic resolution is not used by the HW
 * driver.  The data passed up to the HAL is the HW value read so that the HAL
 * can multiply the HW value with the scale (resolution).
 * As a baseline, scale would be the same value as the static resolution.
 * Method 2:
 * 1. Disable device.
 * 2. Write 1 to the scale sysfs attribute.
 * 3. Enable device.
 * 4. The NVS HAL will announce in the log that calibration mode is enabled and
 *    display the data along with the scale and offset parameters applied.
 * 5. Write to scale and offset sysfs attributes as needed to get the data
 *    modified as desired.
 * 6. Disabling the device disables calibration mode.
 * 7. Set the new scale and offset parameters in the device tree:
 *    light_scale_ival = the integer value of the scale.
 *    light_scale_fval = the floating value of the scale.
 *    light_offset_ival = the integer value of the offset.
 *    light_offset_fval = the floating value of the offset.
 *    The values are in the NVS_FLOAT_SIGNIFICANCE_ format (see nvs.h).
 */


#include <linux/of.h>
#include <linux/nvs_light.h>

#define NVS_LIGHT_VERSION		(103)


ssize_t nvs_light_dbg(struct nvs_light *nl, char *buf)
{
	ssize_t t;
	unsigned int n;
	unsigned int i;

	t = sprintf(buf, "%s v.%u:\n", __func__, NVS_LIGHT_VERSION);
	t += sprintf(buf + t, "timestamp=%lld\n", nl->timestamp);
	t += sprintf(buf + t, "timestamp_report=%lld\n", nl->timestamp_report);
	t += sprintf(buf + t, "lux=%u\n", nl->lux);
	t += sprintf(buf + t, "lux_max=%u\n", nl->lux_max);
	t += sprintf(buf + t, "hw=%u\n", nl->hw);
	t += sprintf(buf + t, "hw_mask=%x\n", nl->hw_mask);
	t += sprintf(buf + t, "hw_thresh_lo=%u\n", nl->hw_thresh_lo);
	t += sprintf(buf + t, "hw_thresh_hi=%u\n", nl->hw_thresh_hi);
	t += sprintf(buf + t, "hw_limit_lo=%x\n", nl->hw_limit_lo);
	t += sprintf(buf + t, "hw_limit_hi=%x\n", nl->hw_limit_hi);
	t += sprintf(buf + t, "thresh_valid_lo=%x\n", nl->thresh_valid_lo);
	t += sprintf(buf + t, "thresh_valid_hi=%x\n", nl->thresh_valid_hi);
	t += sprintf(buf + t, "thresholds_valid=%x\n", nl->thresholds_valid);
	t += sprintf(buf + t, "nld_i_change=%x\n", nl->nld_i_change);
	t += sprintf(buf + t, "calibration_en=%x\n", nl->calibration_en);
	t += sprintf(buf + t, "poll_delay_ms=%u\n", nl->poll_delay_ms);
	t += sprintf(buf + t, "delay_us=%u\n", nl->delay_us);
	t += sprintf(buf + t, "report=%u\n", nl->report);
	t += sprintf(buf + t, "nld_i=%u\n", nl->nld_i);
	t += sprintf(buf + t, "nld_i_lo=%u\n", nl->nld_i_lo);
	t += sprintf(buf + t, "nld_i_hi=%u\n", nl->nld_i_hi);
	t += sprintf(buf + t, "nld_tbl_n=%u\n", nl->nld_tbl_n);
	if (nl->nld_tbl) {
		if (nl->nld_tbl_n) {
			i = 0;
			n = nl->nld_tbl_n;
		} else {
			i = nl->nld_i_lo;
			n = nl->nld_i_hi + 1;
		}
		for (; i < n; i++) {
			if (nl->nld_thr) {
				t += sprintf(buf + t, "nld_thr[%u].lo=%u\n",
					     i, nl->nld_thr[i].lo);
				t += sprintf(buf + t, "nld_thr[%u].hi=%u\n",
					     i, nl->nld_thr[i].hi);
				t += sprintf(buf + t, "nld_thr[%u].i_lo=%u\n",
					     i, nl->nld_thr[i].i_lo);
				t += sprintf(buf + t, "nld_thr[%u].i_hi=%u\n",
					     i, nl->nld_thr[i].i_hi);
			}
			if (nl->cfg->float_significance) {
				t += sprintf(buf + t,
					    "nld_tbl[%d].resolution=%d.%09u\n",
					     i, nl->nld_tbl[i].resolution.ival,
					     nl->nld_tbl[i].resolution.fval);
				t += sprintf(buf + t,
					     "nld_tbl[%d].max_range=%d.%09u\n",
					     i, nl->nld_tbl[i].max_range.ival,
					     nl->nld_tbl[i].max_range.fval);
				t += sprintf(buf + t,
					     "nld_tbl[%d].milliamp=%d.%09u\n",
					     i, nl->nld_tbl[i].milliamp.ival,
					     nl->nld_tbl[i].milliamp.fval);
			} else {
				t += sprintf(buf + t,
					    "nld_tbl[%d].resolution=%d.%06u\n",
					     i, nl->nld_tbl[i].resolution.ival,
					     nl->nld_tbl[i].resolution.fval);
				t += sprintf(buf + t,
					     "nld_tbl[%d].max_range=%d.%06u\n",
					     i, nl->nld_tbl[i].max_range.ival,
					     nl->nld_tbl[i].max_range.fval);
				t += sprintf(buf + t,
					     "nld_tbl[%d].milliamp=%d.%06u\n",
					     i, nl->nld_tbl[i].milliamp.ival,
					     nl->nld_tbl[i].milliamp.fval);
			}
			t += sprintf(buf + t, "nld_tbl[%d].delay_min_ms=%u\n",
				     i, nl->nld_tbl[i].delay_min_ms);
			t += sprintf(buf + t, "nld_tbl[%d].driver_data=%u\n",
				     i, nl->nld_tbl[i].driver_data);
		}
	}
	return t;
}

static u32 nvs_light_interpolate(int x1, s64 x2, int x3, int y1, int y3)
{
	s64 dividend;
	s64 divisor;

	/* y2 = ((x2 - x1)(y3 - y1)/(x3 - x1)) + y1 */
	divisor = (x3 - x1);
	if (!divisor)
		return (u32)x2;

	dividend = (x2 - x1) * (y3 - y1);
	if (dividend < 0) {
		dividend = abs64(dividend);
		do_div(dividend, divisor);
		dividend = 0 - dividend;
	} else {
		do_div(dividend, divisor);
	}
	dividend += y1;
	if (dividend < 0)
		dividend = 0;
	return (u32)dividend;
}

static int nvs_light_nld(struct nvs_light *nl, unsigned int nld_i)
{
	nl->nld_i = nld_i;
	nl->nld_i_change = true;
	nl->cfg->resolution.ival = nl->nld_tbl[nld_i].resolution.ival;
	nl->cfg->resolution.fval = nl->nld_tbl[nld_i].resolution.fval;
	nl->cfg->max_range.ival = nl->nld_tbl[nld_i].max_range.ival;
	nl->cfg->max_range.fval = nl->nld_tbl[nld_i].max_range.fval;
	nl->cfg->milliamp.ival = nl->nld_tbl[nld_i].milliamp.ival;
	nl->cfg->milliamp.fval = nl->nld_tbl[nld_i].milliamp.fval;
	nl->cfg->delay_us_min = nl->nld_tbl[nld_i].delay_min_ms * 1000;
	return RET_POLL_NEXT;
}

/**
 * nvs_light_read - called after HW is read and placed in nl.
 * @nl: the common structure between driver and common module.
 *
 * This will handle the conversion of HW to lux value,
 * reporting, calculation of thresholds and poll time.
 *
 * Returns: -1 = Error and/or polling is required for next
 *               sample regardless of being interrupt driven.
 *          0 = Do nothing.  Lux has not changed for reporting
 *              and same threshold values if interrupt driven.
 *              If not interrupt driven use poll_delay_ms.
 *          1 = New HW thresholds are needed.
 *              If not interrupt driven use poll_delay_ms.
 */
int nvs_light_read(struct nvs_light *nl)
{
	u64 calc_i;
	u64 calc_f;
	s64 calc;
	s64 timestamp_diff;
	s64 delay;
	bool report_delay_min = true;
	unsigned int poll_delay = 0;
	unsigned int thr_lo;
	unsigned int thr_hi;
	int ret;

	if (nl->calibration_en)
		/* always report without report_delay_min */
		nl->report = nl->cfg->report_n;
	if (nl->report < nl->cfg->report_n) { /* always report first sample */
		/* calculate elapsed time for allowed report rate */
		timestamp_diff = nl->timestamp - nl->timestamp_report;
		delay = nl->delay_us * 1000;
		if (timestamp_diff < delay) {
			/* data changes are happening faster than allowed to
			 * report so we poll for the next data at an allowed
			 * rate with interrupts disabled.
			 */
			delay -= timestamp_diff;
			do_div(delay, 1000); /* ns => us */
			poll_delay = delay;
			report_delay_min = false;
		}
	}
	/* threshold flags */
	if (nl->nld_thr) {
		thr_lo = nl->nld_thr[nl->nld_i].lo;
		thr_hi = nl->nld_thr[nl->nld_i].hi;
	} else {
		thr_lo = nl->cfg->thresh_lo;
		thr_hi = nl->cfg->thresh_hi;
	}
	if (thr_lo < nl->hw_mask) {
		nl->thresh_valid_lo = true;
	} else {
		nl->thresh_valid_lo = false;
		thr_lo = 0;
	}
	if (thr_hi < nl->hw_mask) {
		nl->thresh_valid_hi = true;
	} else {
		nl->thresh_valid_hi = false;
		thr_hi = 0;
	}
	if (nl->thresh_valid_lo && nl->thresh_valid_hi)
		nl->thresholds_valid = true;
	else
		nl->thresholds_valid = false;
	/* limit flags */
	if (nl->nld_thr) {
		/* using absolute values */
		if (nl->hw < nl->nld_thr[nl->nld_i].i_lo)
			nl->hw_limit_lo = true;
		else
			nl->hw_limit_lo = false;
		if (nl->hw > nl->nld_thr[nl->nld_i].i_hi)
			nl->hw_limit_hi = true;
		else
			nl->hw_limit_hi = false;
	} else {
		if (nl->hw < thr_lo)
			nl->hw_limit_lo = true;
		else
			nl->hw_limit_lo = false;
		if (nl->hw > (nl->hw_mask - thr_hi))
			nl->hw_limit_hi = true;
		else
			nl->hw_limit_hi = false;
	}
	if (nl->hw == 0)
		nl->hw_limit_lo = true;
	if (nl->hw >= nl->hw_mask)
		nl->hw_limit_hi = true;
	/* reporting and thresholds */
	if (nl->nld_i_change) {
		/* HW resolution just changed.  Need thresholds and reporting
		 * based on new settings.  Reporting may not be this cycle due
		 * to report_delay_min.
		 */
		nl->report = nl->cfg->report_n;
	} else {
		if (nl->thresholds_valid) {
			if (nl->hw < nl->hw_thresh_lo)
				nl->report = nl->cfg->report_n;
			else if (nl->hw > nl->hw_thresh_hi)
				nl->report = nl->cfg->report_n;
		} else {
			/* report everything if no thresholds */
			nl->report = nl->cfg->report_n;
		}
	}
	ret = RET_NO_CHANGE;
	/* lux reporting */
	if (nl->report && report_delay_min) {
		nl->report--;
		nl->timestamp_report = nl->timestamp;
		calc_f = 0;
		if (nl->cfg->scale.fval && !nl->dynamic_resolution_dis) {
			/* The mechanism below allows floating point to be
			 * calculated here in the kernel by shifting up to
			 * integer the floating point significant amount.
			 * The nl->cfg->scale.fval must be a 10 base value,
			 * e.g. 0.1, 0.01, ... 0.000001, etc.
			 * The significance is calculated as:
			 * s = (NVS_FLOAT_SIGNIFICANCE_* / scale.fval) so that
			 * lux = HW * resolution * s
			 * The NVS HAL will then convert the value to float
			 * by multiplying the data with scale.
			 */
			if (nl->cfg->resolution.fval) {
				calc_f = (u64)
					 (nl->hw * nl->cfg->resolution.fval);
				do_div(calc_f, nl->cfg->scale.fval);
			}
			if (nl->cfg->resolution.ival) {
				if (nl->cfg->float_significance)
					calc_i = NVS_FLOAT_SIGNIFICANCE_NANO;
				else
					calc_i = NVS_FLOAT_SIGNIFICANCE_MICRO;
				do_div(calc_i, nl->cfg->scale.fval);
				calc_i *= (u64)
					  (nl->hw * nl->cfg->resolution.ival);
			} else {
				calc_i = 0;
			}
		} else {
			calc_i = nl->hw;
		}
		calc = (s64)(calc_i + calc_f);
		if (nl->calibration_en)
			/* when in calibration mode just return lux value */
			nl->lux = (u32)calc;
		else
			/* get calibrated value if not in calibration mode */
			nl->lux = nvs_light_interpolate(nl->cfg->uncal_lo,
							calc,
							nl->cfg->uncal_hi,
							nl->cfg->cal_lo,
							nl->cfg->cal_hi);
		if (nl->lux_max) {
			if (nl->lux > nl->lux_max)
				nl->lux = nl->lux_max;
		}
		/* report lux */
		nl->handler(nl->nvs_st, &nl->lux, nl->timestamp_report);
		if (nl->thresholds_valid && !nl->report) {
			/* calculate low threshold */
			calc = (s64)nl->hw;
			calc -= thr_lo;
			if (calc < 0)
				/* low threshold is disabled */
				nl->hw_thresh_lo = 0;
			else
				nl->hw_thresh_lo = calc;
			/* calculate high threshold */
			calc = nl->hw + thr_hi;
			if (calc > nl->hw_mask)
				/* high threshold is disabled */
				nl->hw_thresh_hi = nl->hw_mask;
			else
				nl->hw_thresh_hi = calc;
			ret = RET_HW_UPDATE;
		}
	}
	/* dynamic resolution */
	nl->nld_i_change = false;
	if (nl->nld_tbl) { /* if dynamic resolution is enabled */
		/* adjust resolution if need to make room for thresholds */
		if (nl->hw_limit_hi && nl->nld_i < nl->nld_i_hi)
			/* too many photons - decrease integration time */
			ret = nvs_light_nld(nl, nl->nld_i + 1);
		else if (nl->hw_limit_lo && nl->nld_i > nl->nld_i_lo)
			/* not enough photons - increase integration time */
			ret = nvs_light_nld(nl, nl->nld_i - 1);
	}
	/* poll time */
	if (nl->nld_i_change) {
		nl->poll_delay_ms = nl->nld_tbl[nl->nld_i].delay_min_ms;
	} else {
		if (report_delay_min)
			poll_delay = nl->delay_us;
		if ((poll_delay < nl->cfg->delay_us_min) || nl->calibration_en)
			poll_delay = nl->cfg->delay_us_min;
		nl->poll_delay_ms = poll_delay / 1000;
	}
	if (nl->report || nl->calibration_en)
		ret = RET_POLL_NEXT; /* poll for next sample */
	return ret;
}

/**
 * nvs_light_enable - called when the light sensor is enabled.
 * @nl: the common structure between driver and common module.
 *
 * This inititializes the nl NVS variables.
 *
 * Returns 0 on success or a negative error code.
 */
int nvs_light_enable(struct nvs_light *nl)
{
	if (!nl->cfg->report_n)
		nl->cfg->report_n = 1;
	nl->report = nl->cfg->report_n;
	nl->timestamp_report = 0;
	nl->hw_thresh_hi = 0;
	nl->hw_thresh_lo = -1;
	if (nl->nld_tbl)
		nvs_light_nld(nl, nl->nld_i_hi);
	nl->poll_delay_ms = nl->cfg->delay_us_min / 1000;
	if (nl->cfg->scale.ival == 1 && !nl->cfg->scale.fval)
		nl->calibration_en = true;
	else
		nl->calibration_en = false;
	return 0;
}

/**
 * nvs_light_of_dt - called during system boot to acquire
 * dynamic resolution table index limits.
 * @nl: the common structure between driver and common module.
 * @np: device node pointer.
 * @dev_name: device name string.  Typically a string to "light"
 *            or NULL.
 *
 * Returns 0 on success or a negative error code.
 *
 * Driver must initialize variables if no success.
 * NOTE: DT must have both indexes for a success.
 */
int nvs_light_of_dt(struct nvs_light *nl, const struct device_node *np,
		    const char *dev_name)
{
	bool nld_thr_disable;
	char str[256];
	unsigned int i;
	int ret;
	int ret_t;

	if (nl->cfg)
		nl->cfg->flags |= SENSOR_FLAG_ON_CHANGE_MODE;
	if (np == NULL)
		return -EINVAL;

	if (dev_name == NULL)
		dev_name = NVS_LIGHT_STRING;
	if (nl->nld_thr) {
		/* nl->nld_tbl_n == 0 is allowed in case HW driver provides
		 * hardcoded values in nl->nld_thr
		 */
		if (nl->nld_tbl_n)
			nld_thr_disable = true;
		else
			nld_thr_disable = false;
		for (i = 0; i < nl->nld_tbl_n; i++) {
			nl->nld_thr[i].lo = nl->cfg->thresh_lo;
			nl->nld_thr[i].i_lo = nl->cfg->thresh_lo;
			ret = sprintf(str, "%s_nld_thr_lo_%u", dev_name, i);
			if (ret > 0) {
				ret = of_property_read_u32(np, str,
							   &nl->nld_thr[i].lo);
				if (!ret)
					nld_thr_disable = false;
			}
			ret = sprintf(str, "%s_nld_thr_i_lo_%u", dev_name, i);
			if (ret > 0) {
				ret = of_property_read_u32(np, str,
							 &nl->nld_thr[i].i_lo);
				if (!ret)
					nld_thr_disable = false;
			}
			nl->nld_thr[i].hi = nl->cfg->thresh_hi;
			nl->nld_thr[i].i_hi = nl->hw_mask - nl->cfg->thresh_hi;
			ret = sprintf(str, "%s_nld_thr_hi_%u", dev_name, i);
			if (ret > 0) {
				ret = of_property_read_u32(np, str,
							   &nl->nld_thr[i].hi);
				if (!ret)
					nld_thr_disable = false;
			}
			ret = sprintf(str, "%s_nld_thr_i_hi_%u", dev_name, i);
			if (ret > 0) {
				ret = of_property_read_u32(np, str,
							 &nl->nld_thr[i].i_hi);
				if (!ret)
					nld_thr_disable = false;
			}
		}

		if (nld_thr_disable)
			/* there isn't a DT entry so disable this feature */
			nl->nld_thr = NULL;
	}

	ret = sprintf(str, "%s_lux_maximum", dev_name);
	if (ret > 0)
		of_property_read_u32(np, str, &nl->lux_max);
	ret_t = -EINVAL;
	ret = sprintf(str, "%s_dynamic_resolution_index_limit_low", dev_name);
	if (ret > 0)
		ret_t = of_property_read_u32(np, str, &nl->nld_i_lo);
	ret = sprintf(str, "%s_dynamic_resolution_index_limit_high", dev_name);
	if (ret > 0)
		ret_t |= of_property_read_u32(np, str, &nl->nld_i_hi);
	if (nl->nld_i_hi < nl->nld_i_lo)
		return -EINVAL;

	return ret_t;
}

/**
 * nvs_light_resolution - runtime mechanism to modify nld_i_lo.
 * @nl: the common structure between driver and common module.
 * @resolution: an integer value that will be nld_i_lo.
 *
 * Returns 0 on success or a negative error code if a dynamic
 * resolution table exists.  Otherwise a 1 is returned.
 *
 * NOTE: A returned 1 allows the NVS layer above this one to
 *       simply store the new resolution value.
 * NOTE: Caller can check nld_i_change to update the HW with the
 *       new indexed values.
 */
int nvs_light_resolution(struct nvs_light *nl, int resolution)
{
	if (nl->nld_tbl == NULL)
		return 1;

	if (!nl->nld_tbl_n) {
		pr_err("%s ERR: feature not supported\n", __func__);
		return -EINVAL;
	}

	if (resolution < 0 || resolution >= nl->nld_tbl_n) {
		pr_err("%s ERR: nld_i_lo (%d) out of range (0-%u)\n",
		       __func__, resolution, nl->nld_tbl_n - 1);
		return -EINVAL;
	}

	if (resolution > nl->nld_i_hi) {
		pr_err("%s ERR: nld_i_lo (%d) > nld_i_hi (%u)\n",
		       __func__, resolution, nl->nld_i_hi);
		return -EINVAL;
	}

	nl->nld_i_lo = resolution;
	if (nl->nld_i < nl->nld_i_lo)
		nvs_light_nld(nl, nl->nld_i_lo);
	return 0;
}

/**
 * nvs_light_max_range - runtime mechanism to modify nld_i_hi.
 * @nl: the common structure between driver and common module.
 * @max_range: an integer value that will be nld_i_hi.
 *
 * Returns 0 on success or a negative error code if a dynamic
 * resolution table exists.  Otherwise a 1 is returned.
 *
 * NOTE: A returned 1 allows the NVS layer above this one to
 *       simply store the new max_range value.
 * NOTE: Caller can check nld_i_change to update the HW with the
 *       new indexed values.
 */
int nvs_light_max_range(struct nvs_light *nl, int max_range)
{
	if (nl->nld_tbl == NULL)
		return 1;

	if (!nl->nld_tbl_n) {
		pr_err("%s ERR: feature not supported\n", __func__);
		return -EINVAL;
	}

	if (max_range < 0 || max_range >= nl->nld_tbl_n) {
		pr_err("%s ERR: nld_i_hi (%d) out of range (0-%u)\n",
		       __func__, max_range, nl->nld_tbl_n - 1);
		return -EINVAL;
	}

	if (max_range < nl->nld_i_lo) {
		pr_err("%s ERR: nld_i_hi (%d) < nld_i_lo (%u)\n",
		       __func__, max_range, nl->nld_i_lo);
		return -EINVAL;
	}

	nl->nld_i_hi = max_range;
	if (nl->nld_i > nl->nld_i_hi)
		nvs_light_nld(nl, nl->nld_i_hi);
	return 0;
}

/**
 * nvs_light_threshold_calibrate_lo - runtime mechanism to
 * modify calibrated/uncalibrated low value.
 * @nl: the common structure between driver and common module.
 * @lo: either cal_lo or thresh_lo.
 *
 * Returns 0 on success or a negative error code.
 *
 * NOTE: If not in calibration mode then thresholds are modified
 * instead.
 */
int nvs_light_threshold_calibrate_lo(struct nvs_light *nl, int lo)
{
	bool i_thr;
	unsigned int i;

	if (nl->calibration_en) {
		nl->cfg->uncal_lo = nl->lux;
		nl->cfg->cal_lo = lo;
	} else {
		if (nl->nld_thr) {
			if (!nl->nld_tbl_n) {
				pr_err("%s ERR: feature not supported\n",
				       __func__);
				return -EINVAL;
			}

			i = lo >> NVS_LIGHT_THRESH_CMD_SHIFT;
			i_thr = (bool)(i & NVS_LIGHT_THRESH_CMD_SEL_MSK);
			i &= NVS_LIGHT_THRESH_CMD_NDX_MSK;
			if (i >= nl->nld_tbl_n) {
				pr_err("%s ERR: index %d > %u\n",
				       __func__, i, nl->nld_tbl_n - 1);
				return -EINVAL;
			}

			if (i_thr)
				nl->nld_thr[i].i_lo = lo & nl->hw_mask;
			else
				nl->nld_thr[i].lo = lo & nl->hw_mask;
		} else {
			nl->cfg->thresh_lo = lo;
		}
	}

	return 0;
}

/**
 * nvs_light_threshold_calibrate_hi - runtime mechanism to
 * modify calibrated/uncalibrated high value.
 * @nl: the common structure between driver and common module.
 * @hi: either cal_hi or thresh_hi.
 *
 * Returns 0 on success or a negative error code.
 *
 * NOTE: If not in calibration mode then thresholds are modified
 * instead.
 */
int nvs_light_threshold_calibrate_hi(struct nvs_light *nl, int hi)
{
	bool i_thr;
	unsigned int i;

	if (nl->calibration_en) {
		nl->cfg->uncal_hi = nl->lux;
		nl->cfg->cal_hi = hi;
	} else {
		if (nl->nld_thr) {
			if (!nl->nld_tbl_n) {
				pr_err("%s ERR: feature not supported\n",
				       __func__);
				return -EINVAL;
			}

			i = hi >> NVS_LIGHT_THRESH_CMD_SHIFT;
			i_thr = (bool)(i & NVS_LIGHT_THRESH_CMD_SEL_MSK);
			i &= NVS_LIGHT_THRESH_CMD_NDX_MSK;
			if (i >= nl->nld_tbl_n) {
				pr_err("%s ERR: index %u > %u\n",
				       __func__, i, nl->nld_tbl_n - 1);
				return -EINVAL;
			}

			if (i_thr)
				nl->nld_thr[i].i_hi = hi & nl->hw_mask;
			else
				nl->nld_thr[i].hi = hi & nl->hw_mask;
		} else {
			nl->cfg->thresh_hi = hi;
		}
	}

	return 0;
}

