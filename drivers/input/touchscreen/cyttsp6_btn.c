/*
 * cyttsp6_btn.c
 * Cypress TrueTouch(TM) Standard Product V4 CapSense Reports Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2012-2015 Cypress Semiconductor
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

static inline void cyttsp6_btn_key_action(struct cyttsp6_btn_data *bd,
	int btn_no, int btn_state)
{
	struct device *dev = bd->dev;
	struct cyttsp6_sysinfo *si = bd->si;

	if (!si->btn[btn_no].enabled ||
			si->btn[btn_no].state == btn_state)
		return;

	si->btn[btn_no].state = btn_state;
	input_report_key(bd->input, si->btn[btn_no].key_code, btn_state);
	input_sync(bd->input);

	dev_dbg(dev, "%s: btn=%d key_code=%d %s\n", __func__,
		btn_no, si->btn[btn_no].key_code,
		btn_state == CY_BTN_PRESSED ?
			"PRESSED" : "RELEASED");
}

static void cyttsp6_get_btn_touches(struct cyttsp6_btn_data *bd)
{
	struct cyttsp6_sysinfo *si = bd->si;
	int num_btn_regs = si->si_ofs.num_btn_regs;
	int num_btns = si->si_ofs.num_btns;
	int cur_reg;
	int cur_reg_val;
	int cur_btn;
	int cur_btn_state;
	int i;

	for (cur_btn = 0, cur_reg = 0; cur_reg < num_btn_regs; cur_reg++) {
		cur_reg_val = si->xy_mode[si->si_ofs.rep_ofs + 2 + cur_reg];

		for (i = 0; i < CY_NUM_BTN_PER_REG && cur_btn < num_btns;
				i++, cur_btn++) {
			/* Get current button state */
			cur_btn_state = cur_reg_val &
					((1 << CY_BITS_PER_BTN) - 1);
			/* Shift reg value for next iteration */
			cur_reg_val >>= CY_BITS_PER_BTN;

			cyttsp6_btn_key_action(bd, cur_btn, cur_btn_state);
		}
	}
}

static void cyttsp6_btn_lift_all(struct cyttsp6_btn_data *bd)
{
	struct cyttsp6_sysinfo *si = bd->si;
	int i;

	if (!si || si->si_ofs.num_btns == 0)
		return;

	for (i = 0; i < si->si_ofs.num_btns; i++)
		cyttsp6_btn_key_action(bd, i, CY_BTN_RELEASED);
}

#ifdef VERBOSE_DEBUG
static void cyttsp6_log_btn_data(struct cyttsp6_btn_data *bd)
{
	struct device *dev = bd->dev;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	u8 *pr_buf = cd->pr_buf;
	struct cyttsp6_sysinfo *si = bd->si;
	int cur;
	int t;

	for (cur = 0; cur < si->si_ofs.num_btns; cur++) {
		pr_buf[0] = 0;
		snprintf(pr_buf, CY_MAX_PRBUF_SIZE, "btn_rec[%d]=0x", cur);
		for (t = 0; t < si->si_ofs.btn_rec_size; t++)
			snprintf(pr_buf, CY_MAX_PRBUF_SIZE, "%s%02X",
				pr_buf, si->btn_rec_data
				[(cur * si->si_ofs.btn_rec_size) + t]);

		dev_vdbg(dev, "%s: %s\n", __func__, pr_buf);
	}
}
#endif

/* read xy_data for all current CapSense button touches */
static int cyttsp6_xy_worker(struct cyttsp6_btn_data *bd)
{
	struct cyttsp6_sysinfo *si = bd->si;
	u8 rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
#ifdef VERBOSE_DEBUG
	int rc;
#endif

	/* rep_data for bad packet check */
	if (IS_BAD_PKT(rep_stat)) {
		dev_dbg(bd->dev, "%s: Invalid buffer detected\n", __func__);
		return 0;
	}

	/* extract button press/release touch information */
	if (si->si_ofs.num_btns > 0) {
		cyttsp6_get_btn_touches(bd);
#ifdef VERBOSE_DEBUG
		/* read button diff data */
		rc = cyttsp6_read(bd->dev, CY_MODE_OPERATIONAL,
				si->si_ofs.tt_stat_ofs + 1 +
				si->si_ofs.max_tchs * si->si_ofs.tch_rec_size,
				si->btn_rec_data,
				si->si_ofs.num_btns * si->si_ofs.btn_rec_size);
		if (rc < 0) {
			dev_err(bd->dev, "%s: read fail on button regs r=%d\n",
					__func__, rc);
			return 0;
		}

		/* log button press/release touch information */
		cyttsp6_log_btn_data(bd);
#endif
	}

	return 0;
}

static int cyttsp6_btn_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;
	int rc = 0;

	/* core handles handshake */
	mutex_lock(&bd->btn_lock);
	rc = cyttsp6_xy_worker(bd);
	mutex_unlock(&bd->btn_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp6_startup_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	mutex_lock(&bd->btn_lock);
	cyttsp6_btn_lift_all(bd);
	mutex_unlock(&bd->btn_lock);

	return 0;
}

static int cyttsp6_btn_suspend_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	mutex_lock(&bd->btn_lock);
	cyttsp6_btn_lift_all(bd);
	bd->is_suspended = true;
	mutex_unlock(&bd->btn_lock);

	pm_runtime_put(dev);

	return 0;
}

static int cyttsp6_btn_resume_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	pm_runtime_get(dev);

	mutex_lock(&bd->btn_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->btn_lock);

	return 0;
}

static int cyttsp6_btn_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	pm_runtime_get_sync(dev);

	mutex_lock(&bd->btn_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->btn_lock);

	dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_IRQ, CY_MODULE_BTN,
		cyttsp6_btn_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_BTN,
		cyttsp6_startup_attention, 0);

	/* set up suspend call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_SUSPEND, CY_MODULE_BTN,
		cyttsp6_btn_suspend_attention, 0);

	/* set up resume call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_RESUME, CY_MODULE_BTN,
		cyttsp6_btn_resume_attention, 0);

	return 0;
}

static void cyttsp6_btn_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_IRQ, CY_MODULE_BTN,
		cyttsp6_btn_attention, CY_MODE_OPERATIONAL);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_BTN,
		cyttsp6_startup_attention, 0);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_SUSPEND, CY_MODULE_BTN,
		cyttsp6_btn_suspend_attention, 0);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_RESUME, CY_MODULE_BTN,
		cyttsp6_btn_resume_attention, 0);

	mutex_lock(&bd->btn_lock);
	if (!bd->is_suspended) {
		pm_runtime_put(dev);
		bd->is_suspended = true;
	}
	mutex_unlock(&bd->btn_lock);
}

static int cyttsp6_setup_input_device(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;
	int i;
	int rc;

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_KEY, bd->input->evbit);
	for (i = 0; i < bd->si->si_ofs.num_btns; i++)
		__set_bit(bd->si->btn[i].key_code, bd->input->keybit);

	rc = input_register_device(bd->input);
	if (rc < 0)
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		bd->input_device_registered = true;

	return rc;
}

static int cyttsp6_setup_input_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;
	int rc;

	bd->si = cyttsp6_request_sysinfo_(dev);
	if (!bd->si)
		return -1;

	rc = cyttsp6_setup_input_device(dev);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_BTN,
		cyttsp6_setup_input_attention, 0);

	return rc;
}

int cyttsp6_btn_probe(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;
	struct cyttsp6_platform_data *pdata = dev_get_platdata(dev);
	struct cyttsp6_btn_platform_data *btn_pdata;
	int rc = 0;

	if (!pdata || !pdata->btn_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	btn_pdata = pdata->btn_pdata;

	mutex_init(&bd->btn_lock);
	bd->dev = dev;
	bd->pdata = btn_pdata;

	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	bd->input = input_allocate_device();
	if (!bd->input) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	if (bd->pdata->inp_dev_name)
		bd->input->name = bd->pdata->inp_dev_name;
	else
		bd->input->name = CYTTSP6_BTN_NAME;
	scnprintf(bd->phys, sizeof(bd->phys), "%s/input%d", dev_name(dev),
			cd->phys_num++);
	bd->input->phys = bd->phys;
	bd->input->dev.parent = bd->dev;
	bd->input->open = cyttsp6_btn_open;
	bd->input->close = cyttsp6_btn_close;
	input_set_drvdata(bd->input, bd);

	/* get sysinfo */
	bd->si = cyttsp6_request_sysinfo_(dev);
	if (bd->si) {
		rc = cyttsp6_setup_input_device(dev);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, bd->si);
		cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_BTN, cyttsp6_setup_input_attention, 0);
	}

	return 0;

error_init_input:
	input_free_device(bd->input);
error_alloc_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}

int cyttsp6_btn_release(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_btn_data *bd = &cd->bd;

	if (bd->input_device_registered) {
		input_unregister_device(bd->input);
	} else {
		input_free_device(bd->input);
		cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_BTN, cyttsp6_setup_input_attention, 0);
	}

	return 0;
}
