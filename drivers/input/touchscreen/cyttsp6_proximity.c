/*
 * cyttsp6_proximity.c
 * Cypress TrueTouch(TM) Standard Product V4 Proximity Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2013-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp6_regs.h"

#define CY_PROXIMITY_ON 0
#define CY_PROXIMITY_OFF 1

static inline struct cyttsp6_proximity_data *get_prox_data(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);

	return &cd->pd;
}

static void cyttsp6_report_proximity(struct cyttsp6_proximity_data *pd,
	bool on)
{
	int val = on ? CY_PROXIMITY_ON : CY_PROXIMITY_OFF;

	input_report_abs(pd->input, ABS_DISTANCE, val);
	input_sync(pd->input);
}

static void cyttsp6_get_proximity_touch(struct cyttsp6_proximity_data *pd,
		int num_cur_rec)
{
	struct cyttsp6_touch tch;
	int i;

	for (i = 0; i < num_cur_rec; i++) {
		cyttsp6_get_touch_record(pd->dev, i, tch.abs);

		/* Check for proximity event */
		if (tch.abs[CY_TCH_O] == CY_OBJ_PROXIMITY) {
			if (tch.abs[CY_TCH_E] == CY_EV_TOUCHDOWN)
				cyttsp6_report_proximity(pd, true);
			else if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF)
				cyttsp6_report_proximity(pd, false);
			break;
		}
	}
}

/* read xy_data for all current touches */
static int cyttsp6_xy_worker(struct cyttsp6_proximity_data *pd)
{
	struct device *dev = pd->dev;
	struct cyttsp6_sysinfo *si = pd->si;
	u8 num_cur_rec;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;

	/*
	 * Get event data from cyttsp6 device.
	 * The event data includes all data
	 * for all active touches.
	 * Event data also includes button data
	 */
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);

	if (rep_len == 0 && num_cur_rec > 0) {
		dev_err(dev, "%s: report length error rep_len=%d num_rec=%d\n",
			__func__, rep_len, num_cur_rec);
		return 0;
	}

	/* check any error conditions */
	if (IS_BAD_PKT(rep_stat)) {
		dev_dbg(dev, "%s: Invalid buffer detected\n", __func__);
		return 0;
	}

	if (IS_LARGE_AREA(tt_stat))
		dev_dbg(dev, "%s: Large area detected\n", __func__);

	if (num_cur_rec > si->si_ofs.max_tchs) {
		dev_err(dev, "%s: %s (n=%d c=%zu)\n", __func__,
			"too many tch; set to max tch",
			num_cur_rec, si->si_ofs.max_tchs);
		num_cur_rec = si->si_ofs.max_tchs;
	}

	/* extract xy_data for all currently reported touches */
	dev_vdbg(dev, "%s: extract data num_cur_rec=%d\n", __func__,
		num_cur_rec);
	if (num_cur_rec)
		cyttsp6_get_proximity_touch(pd, num_cur_rec);
	else
		cyttsp6_report_proximity(pd, false);

	return 0;
}

static int cyttsp6_proximity_attention(struct device *dev)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);
	int rc = 0;

	/* core handles handshake */
	mutex_lock(&pd->prox_lock);
	rc = cyttsp6_xy_worker(pd);
	mutex_unlock(&pd->prox_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp6_startup_attention(struct device *dev)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);

	mutex_lock(&pd->prox_lock);
	cyttsp6_report_proximity(pd, false);
	mutex_unlock(&pd->prox_lock);

	return 0;
}

static int _cyttsp6_proximity_enable(struct cyttsp6_proximity_data *pd)
{
	struct device *dev = pd->dev;
	int rc = 0;

	/* We use pm_runtime_get_sync to activate
	 * the core device until it is disabled back
	 */
	pm_runtime_get_sync(dev);

	rc = cyttsp6_request_exclusive(dev,
			CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	rc = cyttsp6_request_enable_scan_type_(dev, CY_ST_PROXIMITY);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request enable proximity scantype r=%d\n",
				__func__, rc);
		goto exit_release;
	}

	dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_IRQ, CY_MODULE_PROX,
		cyttsp6_proximity_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_PROX,
		cyttsp6_startup_attention, 0);

exit_release:
	cyttsp6_release_exclusive(dev);
exit:
	return rc;
}

static int _cyttsp6_proximity_disable(struct cyttsp6_proximity_data *pd,
		bool force)
{
	struct device *dev = pd->dev;
	int rc = 0;

	rc = cyttsp6_request_exclusive(dev,
			CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	rc = cyttsp6_request_disable_scan_type_(dev, CY_ST_PROXIMITY);
	if (rc < 0) {
		dev_err(dev, "%s: Error on request disable proximity scan r=%d\n",
				__func__, rc);
		goto exit_release;
	}

exit_release:
	cyttsp6_release_exclusive(dev);

exit:
	if (!rc || force) {
		cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_IRQ,
			CY_MODULE_PROX, cyttsp6_proximity_attention,
			CY_MODE_OPERATIONAL);

		cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_PROX, cyttsp6_startup_attention, 0);
	}

	pm_runtime_put(dev);

	return rc;
}

static ssize_t cyttsp6_proximity_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);
	int val = 0;

	mutex_lock(&pd->sysfs_lock);
	val = pd->enable_count;
	mutex_unlock(&pd->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", val);
}

static ssize_t cyttsp6_proximity_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0 || (value != 0 && value != 1)) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pd->sysfs_lock);
	if (value) {
		if (pd->enable_count++) {
			dev_vdbg(dev, "%s: '%s' already enabled\n", __func__,
				pd->input->name);
		} else {
			rc = _cyttsp6_proximity_enable(pd);
			if (rc)
				pd->enable_count--;
		}
	} else {
		if (--pd->enable_count) {
			if (pd->enable_count < 0) {
				dev_err(dev, "%s: '%s' unbalanced disable\n",
					__func__, pd->input->name);
				pd->enable_count = 0;
			}
		} else {
			rc = _cyttsp6_proximity_disable(pd, false);
			if (rc)
				pd->enable_count++;
		}
	}
	mutex_unlock(&pd->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(prox_enable, S_IRUSR | S_IWUSR,
		cyttsp6_proximity_enable_show,
		cyttsp6_proximity_enable_store);

static int cyttsp6_setup_input_device_and_sysfs(struct device *dev)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);
	int signal = CY_IGNORE_VALUE;
	int i;
	int rc;

	rc = device_create_file(dev, &dev_attr_prox_enable);
	if (rc) {
		dev_err(dev, "%s: Error, could not create enable\n",
				__func__);
		goto exit;
	}

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);

	__set_bit(EV_ABS, pd->input->evbit);

	for (i = 0; i < NUM_SIGNALS(pd->pdata->frmwrk); i++) {
		signal = PARAM_SIGNAL(pd->pdata->frmwrk, i);
		if (signal != CY_IGNORE_VALUE) {
			input_set_abs_params(pd->input, signal,
				PARAM_MIN(pd->pdata->frmwrk, i),
				PARAM_MAX(pd->pdata->frmwrk, i),
				PARAM_FUZZ(pd->pdata->frmwrk, i),
				PARAM_FLAT(pd->pdata->frmwrk, i));
		}
	}

	rc = input_register_device(pd->input);
	if (rc) {
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
		goto unregister_enable;
	}

	pd->input_device_registered = true;
	return rc;

unregister_enable:
	device_remove_file(dev, &dev_attr_prox_enable);
exit:
	return rc;
}

static int cyttsp6_setup_input_attention(struct device *dev)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);
	int rc;

	pd->si = cyttsp6_request_sysinfo_(dev);
	if (!pd->si)
		return -EINVAL;

	rc = cyttsp6_setup_input_device_and_sysfs(dev);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_PROX,
		cyttsp6_setup_input_attention, 0);

	return rc;
}

int cyttsp6_proximity_probe(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_proximity_data *pd = &cd->pd;
	struct cyttsp6_platform_data *pdata = dev_get_platdata(dev);
	struct cyttsp6_proximity_platform_data *prox_pdata;
	int rc = 0;

	if (!pdata ||  !pdata->prox_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	prox_pdata = pdata->prox_pdata;

	mutex_init(&pd->prox_lock);
	mutex_init(&pd->sysfs_lock);
	pd->dev = dev;
	pd->pdata = prox_pdata;

	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	pd->input = input_allocate_device();
	if (!pd->input) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	if (pd->pdata->inp_dev_name)
		pd->input->name = pd->pdata->inp_dev_name;
	else
		pd->input->name = CYTTSP6_PROXIMITY_NAME;
	scnprintf(pd->phys, sizeof(pd->phys), "%s/input%d", dev_name(dev),
			cd->phys_num++);
	pd->input->phys = pd->phys;
	pd->input->dev.parent = pd->dev;
	input_set_drvdata(pd->input, pd);

	/* get sysinfo */
	pd->si = cyttsp6_request_sysinfo_(dev);
	if (pd->si) {
		rc = cyttsp6_setup_input_device_and_sysfs(dev);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, pd->si);
		cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_PROX, cyttsp6_setup_input_attention, 0);
	}

	return 0;

error_init_input:
	input_free_device(pd->input);
error_alloc_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}

int cyttsp6_proximity_release(struct device *dev)
{
	struct cyttsp6_proximity_data *pd = get_prox_data(dev);

	if (pd->input_device_registered) {
		/* Disable proximity sensing */
		mutex_lock(&pd->sysfs_lock);
		if (pd->enable_count)
			_cyttsp6_proximity_disable(pd, true);
		mutex_unlock(&pd->sysfs_lock);
		device_remove_file(dev, &dev_attr_prox_enable);
		input_unregister_device(pd->input);
	} else {
		input_free_device(pd->input);
		cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_PROX, cyttsp6_setup_input_attention, 0);
	}

	return 0;
}
