/*
 * cyttsp6_mt_common.c
 * Cypress TrueTouch(TM) Standard Product V4 Multi-Touch Reports Module.
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

#define MT_PARAM_SIGNAL(md, sig_ost) PARAM_SIGNAL(md->pdata->frmwrk, sig_ost)
#define MT_PARAM_MIN(md, sig_ost) PARAM_MIN(md->pdata->frmwrk, sig_ost)
#define MT_PARAM_MAX(md, sig_ost) PARAM_MAX(md->pdata->frmwrk, sig_ost)
#define MT_PARAM_FUZZ(md, sig_ost) PARAM_FUZZ(md->pdata->frmwrk, sig_ost)
#define MT_PARAM_FLAT(md, sig_ost) PARAM_FLAT(md->pdata->frmwrk, sig_ost)

static void cyttsp6_mt_lift_all(struct cyttsp6_mt_data *md)
{
	if (!md->si)
		return;

	if (md->num_prv_rec != 0) {
		if (md->mt_function.report_slot_liftoff)
			md->mt_function.report_slot_liftoff(md,
				md->si->si_ofs.tch_abs[CY_TCH_T].max);
		input_sync(md->input);
		md->num_prv_rec = 0;
	}
}

static void cyttsp6_report_gesture(struct cyttsp6_mt_data *md,int gest_id){
	input_event(md->input, EV_MSC, MSC_GESTURE, gest_id);
}

static void cyttsp6_report_noise(struct cyttsp6_mt_data *md,int noise){
	input_event(md->input, EV_MSC, MSC_RAW, noise);
}

static void cyttsp6_mt_process_touch(struct cyttsp6_mt_data *md,
	struct cyttsp6_touch *touch)
{
	struct device *dev = md->dev;
	int tmp;
	bool flipped;

	/* Orientation is signed */
	touch->abs[CY_TCH_OR] = (int8_t)touch->abs[CY_TCH_OR];

	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		tmp = touch->abs[CY_TCH_X];
		touch->abs[CY_TCH_X] = touch->abs[CY_TCH_Y];
		touch->abs[CY_TCH_Y] = tmp;
		if (IS_TTSP_VER_GE(md->si, 2, 3)) {
			if (touch->abs[CY_TCH_OR] > 0)
				touch->abs[CY_TCH_OR] =
					md->or_max - touch->abs[CY_TCH_OR];
			else
				touch->abs[CY_TCH_OR] =
					md->or_min - touch->abs[CY_TCH_OR];
		}
		flipped = true;
	} else
		flipped = false;

	if (md->pdata->flags & CY_MT_FLAG_INV_X) {
		if (flipped)
			touch->abs[CY_TCH_X] = md->si->si_ofs.max_y -
				touch->abs[CY_TCH_X];
		else
			touch->abs[CY_TCH_X] = md->si->si_ofs.max_x -
				touch->abs[CY_TCH_X];
		touch->abs[CY_TCH_OR] *= -1;
	}
	if (md->pdata->flags & CY_MT_FLAG_INV_Y) {
		if (flipped)
			touch->abs[CY_TCH_Y] = md->si->si_ofs.max_x -
				touch->abs[CY_TCH_Y];
		else
			touch->abs[CY_TCH_Y] = md->si->si_ofs.max_y -
				touch->abs[CY_TCH_Y];
		touch->abs[CY_TCH_OR] *= -1;
	}

	/* Convert MAJOR/MINOR from mm to resolution */
	tmp = touch->abs[CY_TCH_MAJ] * 100 * md->si->si_ofs.max_x;
	touch->abs[CY_TCH_MAJ] = tmp / md->si->si_ofs.len_x;
	tmp = touch->abs[CY_TCH_MIN] * 100 * md->si->si_ofs.max_x;
	touch->abs[CY_TCH_MIN] = tmp / md->si->si_ofs.len_x;

	dev_vdbg(dev, "%s: flip=%s inv-x=%s inv-y=%s x=%04X(%d) y=%04X(%d)\n",
		__func__, flipped ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_X ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_Y ? "true" : "false",
		touch->abs[CY_TCH_X], touch->abs[CY_TCH_X],
		touch->abs[CY_TCH_Y], touch->abs[CY_TCH_Y]);
}

static void cyttsp6_report_event(struct cyttsp6_mt_data *md, int event,
		int value)
{
	int sig = MT_PARAM_SIGNAL(md, event);

	if (sig != CY_IGNORE_VALUE)
		input_report_abs(md->input, sig, value);
}

static void cyttsp6_get_mt_touches(struct cyttsp6_mt_data *md, int num_cur_rec)
{
	struct device *dev = md->dev;
	struct cyttsp6_sysinfo *si = md->si;
	struct cyttsp6_touch tch;
	int sig;
	int i, j, t = 0;
	int mt_sync_count = 0;
	DECLARE_BITMAP(ids, si->si_ofs.tch_abs[CY_TCH_T].max);

	bitmap_zero(ids, si->si_ofs.tch_abs[CY_TCH_T].max);

	for (i = 0; i < num_cur_rec; i++) {
		cyttsp6_get_touch_record(md->dev, i, tch.abs);

		/* Discard proximity event */
		if (tch.abs[CY_TCH_O] == CY_OBJ_PROXIMITY) {
			dev_dbg(dev, "%s: Discarding proximity event\n",
				__func__);
			continue;
		}

		/* Validate track_id */
		t = tch.abs[CY_TCH_T];
		if (t < md->t_min || t > md->t_max) {
			dev_err(dev, "%s: tch=%d -> bad trk_id=%d max_id=%d\n",
				__func__, i, t, md->t_max);
			if (md->mt_function.input_sync)
				md->mt_function.input_sync(md->input);
			mt_sync_count++;
			continue;
		}

		/* Lift-off */
		if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF) {
			dev_dbg(dev, "%s: t=%d e=%d lift-off\n",
				__func__, t, tch.abs[CY_TCH_E]);
			goto cyttsp6_get_mt_touches_pr_tch;
		}

		/* Process touch */
		cyttsp6_mt_process_touch(md, &tch);

		/* use 0 based track id's */
		t -= md->t_min;

		sig = MT_PARAM_SIGNAL(md, CY_ABS_ID_OST);
		if (sig != CY_IGNORE_VALUE) {
			if (md->mt_function.input_report)
				md->mt_function.input_report(md->input, sig,
					t, tch.abs[CY_TCH_O]);
			__set_bit(t, ids);
		}

		/* If touch type is hover, send P as distance, reset P */
		if (tch.abs[CY_TCH_O] == CY_OBJ_HOVER) {
			cyttsp6_report_event(md, CY_ABS_D_OST,
					tch.abs[CY_TCH_P]);
			tch.abs[CY_TCH_P] = 0;
		}

		/* all devices: position and pressure fields */
		for (j = 0; j <= CY_ABS_W_OST ; j++)
			cyttsp6_report_event(md, CY_ABS_X_OST + j,
					tch.abs[CY_TCH_X + j]);

		if (IS_TTSP_VER_GE(si, 2, 3)) {
			/*
			 * TMA400 size and orientation fields:
			 * if pressure is non-zero and major touch
			 * signal is zero, then set major and minor touch
			 * signals to minimum non-zero value
			 */
			if (tch.abs[CY_TCH_P] > 0 && tch.abs[CY_TCH_MAJ] == 0)
				tch.abs[CY_TCH_MAJ] = tch.abs[CY_TCH_MIN] = 1;

			/* Get the extended touch fields */
			for (j = 0; j < CY_NUM_EXT_TCH_FIELDS; j++)
				cyttsp6_report_event(md, CY_ABS_MAJ_OST + j,
						tch.abs[CY_TCH_MAJ + j]);
		}
		if (md->mt_function.input_sync)
			md->mt_function.input_sync(md->input);
		mt_sync_count++;

cyttsp6_get_mt_touches_pr_tch:
		if (IS_TTSP_VER_GE(si, 2, 3))
			dev_dbg(dev,
				"%s: t=%d x=%d y=%d z=%d M=%d m=%d o=%d e=%d obj=%d\n",
				__func__, t,
				tch.abs[CY_TCH_X],
				tch.abs[CY_TCH_Y],
				tch.abs[CY_TCH_P],
				tch.abs[CY_TCH_MAJ],
				tch.abs[CY_TCH_MIN],
				tch.abs[CY_TCH_OR],
				tch.abs[CY_TCH_E],
				tch.abs[CY_TCH_O]);
		else
			dev_dbg(dev,
				"%s: t=%d x=%d y=%d z=%d e=%d\n", __func__,
				t,
				tch.abs[CY_TCH_X],
				tch.abs[CY_TCH_Y],
				tch.abs[CY_TCH_P],
				tch.abs[CY_TCH_E]);
	}

	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input,
			si->si_ofs.tch_abs[CY_TCH_T].max, mt_sync_count, ids);

	md->num_prv_rec = num_cur_rec;
}

/* read xy_data for all current touches */
static int cyttsp6_xy_worker(struct cyttsp6_mt_data *md)
{
	struct device *dev = md->dev;
	struct cyttsp6_sysinfo *si = md->si;
	u8 num_cur_rec;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	u8 num_btn_regs;
	u8 gest_cnt, gest_ofs, gest_id=0;
	u16 gest_pos_x=0, gest_pos_y=0;
	u8 noise_wideband, noise_level;
	int rc = 0;

	/*
	 * Get event data from cyttsp6 device.
	 * The event data includes all data
	 * for all active touches.
	 * Event data also includes button data
	 */
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];
	num_btn_regs = si->si_ofs.num_btn_regs;
	gest_ofs = si->si_ofs.rep_ofs + num_btn_regs + 2;

	noise_wideband = si->xy_mode[si->si_ofs.rep_ofs + num_btn_regs + 10];
	noise_level = si->xy_mode[si->si_ofs.rep_ofs + num_btn_regs + 12];

	gest_id = si->xy_mode[gest_ofs];
	gest_cnt = si->xy_mode[gest_ofs + 1];

	if(IS_GEST_EXTD(gest_id)){
		gest_id = gest_id & CY_GEST_EXT_ID_MASK;
		gest_pos_x = ((si->xy_mode[gest_ofs + 1]) << 8)
				| (si->xy_mode[gest_ofs + 2]);
		gest_pos_y = ((si->xy_mode[gest_ofs + 3]) << 8)
				| (si->xy_mode[gest_ofs + 4]);
	}

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);

	if (rep_len == 0 && num_cur_rec > 0) {
		dev_err(dev, "%s: report length error rep_len=%d num_tch=%d\n",
			__func__, rep_len, num_cur_rec);
		goto cyttsp6_xy_worker_exit;
	}

	/* check any error conditions */
	if (IS_BAD_PKT(rep_stat)) {
		dev_dbg(dev, "%s: Invalid buffer detected\n", __func__);
		rc = 0;
		goto cyttsp6_xy_worker_exit;
	}

	if (IS_LARGE_AREA(tt_stat)) {
		dev_dbg(dev, "%s: Large area detected\n", __func__);
		/* Do not report touch if configured so */
		if (md->pdata->flags & CY_MT_FLAG_NO_TOUCH_ON_LO)
			num_cur_rec = 0;
	}

	if (num_cur_rec == 0 && md->num_prv_rec == 0)
		goto cyttsp6_xy_worker_exit;

	if (num_cur_rec > si->si_ofs.max_tchs) {
		dev_err(dev, "%s: %s (n=%d c=%zu)\n", __func__,
			"too many tch; set to max tch",
			num_cur_rec, si->si_ofs.max_tchs);
		num_cur_rec = si->si_ofs.max_tchs;
	}

	/* generate events for all the detected values*/
	if (gest_id)
		cyttsp6_report_gesture(md,gest_id);
	if (gest_pos_x || gest_pos_y)
		cyttsp6_report_gesture(md, (gest_pos_x << 16) | gest_pos_y);
	if(noise_wideband || noise_level)
		cyttsp6_report_noise(md, (noise_wideband << 8) | (noise_level));

	/* extract xy_data for all currently reported touches */
	dev_vdbg(dev, "%s: extract data num_cur_rec=%d\n", __func__,
		num_cur_rec);
	if (num_cur_rec)
		cyttsp6_get_mt_touches(md, num_cur_rec);
	else
		cyttsp6_mt_lift_all(md);


	dev_vdbg(dev, "%s: done\n", __func__);
	rc = 0;

cyttsp6_xy_worker_exit:
	return rc;
}

static void cyttsp6_mt_send_dummy_event(struct cyttsp6_mt_data *md)
{
	unsigned long ids = 0;

	/* for easy wakeup */
	if (md->mt_function.input_report)
		md->mt_function.input_report(md->input, ABS_MT_TRACKING_ID,
			0, CY_OBJ_STANDARD_FINGER);
	if (md->mt_function.input_sync)
		md->mt_function.input_sync(md->input);
	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input, 0, 1, &ids);
	if (md->mt_function.report_slot_liftoff)
		md->mt_function.report_slot_liftoff(md, 1);
	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input, 1, 1, &ids);
}

static int cyttsp6_mt_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;
	int rc = 0;

	/* core handles handshake */
	mutex_lock(&md->mt_lock);
	rc = cyttsp6_xy_worker(md);
	mutex_unlock(&md->mt_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp6_mt_wake_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	mutex_lock(&md->mt_lock);
	cyttsp6_mt_send_dummy_event(md);
	mutex_unlock(&md->mt_lock);
	return 0;
}

static int cyttsp6_startup_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;
	int rc = 0;

	mutex_lock(&md->mt_lock);
	cyttsp6_mt_lift_all(md);
	mutex_unlock(&md->mt_lock);
	return rc;
}

static int cyttsp6_mt_suspend_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	mutex_lock(&md->mt_lock);
	cyttsp6_mt_lift_all(md);
	md->is_suspended = true;
	mutex_unlock(&md->mt_lock);

	pm_runtime_put(dev);

	return 0;
}

static int cyttsp6_mt_resume_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	pm_runtime_get(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = false;
	mutex_unlock(&md->mt_lock);

	return 0;
}

static int cyttsp6_mt_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	pm_runtime_get_sync(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = false;
	mutex_unlock(&md->mt_lock);

	dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp6_mt_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp6_startup_attention, 0);

	/* set up wakeup call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_WAKE, CY_MODULE_MT,
		cyttsp6_mt_wake_attention, 0);

	/* set up suspend call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_SUSPEND, CY_MODULE_MT,
		cyttsp6_mt_suspend_attention, 0);

	/* set up resume call back */
	cyttsp6_subscribe_attention_(dev, CY_ATTEN_RESUME, CY_MODULE_MT,
		cyttsp6_mt_resume_attention, 0);

	return 0;
}

static void cyttsp6_mt_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp6_mt_attention, CY_MODE_OPERATIONAL);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp6_startup_attention, 0);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_WAKE, CY_MODULE_MT,
		cyttsp6_mt_wake_attention, 0);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_SUSPEND, CY_MODULE_MT,
		cyttsp6_mt_suspend_attention, 0);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_RESUME, CY_MODULE_MT,
		cyttsp6_mt_resume_attention, 0);

	mutex_lock(&md->mt_lock);
	if (!md->is_suspended) {
		pm_runtime_put(dev);
		md->is_suspended = true;
	}
	mutex_unlock(&md->mt_lock);
}

static int cyttsp6_setup_input_device(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;
	int signal = CY_IGNORE_VALUE;
	int max_x, max_y, max_p, min, max;
	int max_x_tmp, max_y_tmp;
	int i;
	int rc;

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, md->input->evbit);
	__set_bit(EV_REL, md->input->evbit);
	__set_bit(EV_KEY, md->input->evbit);
	__set_bit(EV_MSC, md->input->evbit);
	__set_bit(MSC_GESTURE, md->input->mscbit);
	__set_bit(MSC_RAW, md->input->mscbit);
#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, md->input->propbit);
#endif

	/* If virtualkeys enabled, don't use all screen */
	if (md->pdata->flags & CY_MT_FLAG_VKEYS) {
		max_x_tmp = md->pdata->vkeys_x;
		max_y_tmp = md->pdata->vkeys_y;
	} else {
		max_x_tmp = md->si->si_ofs.max_x;
		max_y_tmp = md->si->si_ofs.max_y;
	}

	/* get maximum values from the sysinfo data */
	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		max_x = max_y_tmp - 1;
		max_y = max_x_tmp - 1;
	} else {
		max_x = max_x_tmp - 1;
		max_y = max_y_tmp - 1;
	}
	max_p = md->si->si_ofs.max_p;

	/* set event signal capabilities */
	for (i = 0; i < NUM_SIGNALS(md->pdata->frmwrk); i++) {
		signal = MT_PARAM_SIGNAL(md, i);
		if (signal != CY_IGNORE_VALUE) {
			__set_bit(signal, md->input->absbit);

			min = MT_PARAM_MIN(md, i);
			max = MT_PARAM_MAX(md, i);
			if (i == CY_ABS_ID_OST) {
				/* shift track ids down to start at 0 */
				max = max - min;
				min = min - min;
			} else if (i == CY_ABS_X_OST)
				max = max_x;
			else if (i == CY_ABS_Y_OST)
				max = max_y;
			else if (i == CY_ABS_P_OST)
				max = max_p;

			input_set_abs_params(md->input, signal, min, max,
				MT_PARAM_FUZZ(md, i), MT_PARAM_FLAT(md, i));

			dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
				__func__, signal, min, max);
			if (i == CY_ABS_ID_OST && !IS_TTSP_VER_GE(md->si, 2, 3))
				break;
		}
	}

	if (IS_TTSP_VER_GE(md->si, 2, 3)) {
		for (i = 0; i < NUM_SIGNALS(md->pdata->frmwrk); i++) {
			if (MT_PARAM_SIGNAL(md, i) == ABS_MT_ORIENTATION) {
				md->or_min = MT_PARAM_MIN(md, i);
				md->or_max = MT_PARAM_MAX(md, i);
				break;
			}
		}
	}

	md->t_min = MT_PARAM_MIN(md, CY_ABS_ID_OST);
	md->t_max = MT_PARAM_MAX(md, CY_ABS_ID_OST);

	rc = md->mt_function.input_register_device(md->input,
			md->si->si_ofs.tch_abs[CY_TCH_T].max);
	if (rc < 0)
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		md->input_device_registered = true;

	return rc;
}

static int cyttsp6_setup_input_attention(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;
	int rc = 0;

	md->si = cyttsp6_request_sysinfo_(dev);
	if (!md->si)
		return -EINVAL;

	rc = cyttsp6_setup_input_device(dev);

	cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp6_setup_input_attention, 0);

	return rc;
}

int cyttsp6_mt_probe(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;
	struct cyttsp6_platform_data *pdata = dev_get_platdata(dev);
	struct cyttsp6_mt_platform_data *mt_pdata;
	int rc = 0;

	if (!pdata || !pdata->mt_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	mt_pdata = pdata->mt_pdata;

	cyttsp6_init_function_ptrs(md);

	mutex_init(&md->mt_lock);
	md->dev = dev;
	md->pdata = mt_pdata;

	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	md->input = input_allocate_device();
	if (!md->input) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	if (md->pdata->inp_dev_name)
		md->input->name = md->pdata->inp_dev_name;
	else
		md->input->name = CYTTSP6_MT_NAME;
	scnprintf(md->phys, sizeof(md->phys), "%s/input%d", dev_name(dev),
			cd->phys_num++);
	md->input->phys = md->phys;
	md->input->dev.parent = md->dev;
	md->input->open = cyttsp6_mt_open;
	md->input->close = cyttsp6_mt_close;
	input_set_drvdata(md->input, md);

	/* get sysinfo */
	md->si = cyttsp6_request_sysinfo_(dev);
	if (md->si) {
		rc = cyttsp6_setup_input_device(dev);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, md->si);
		cyttsp6_subscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_MT, cyttsp6_setup_input_attention, 0);
	}

	return 0;

error_init_input:
	input_free_device(md->input);
error_alloc_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);

	return rc;
}

int cyttsp6_mt_release(struct device *dev)
{
	struct cyttsp6_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp6_mt_data *md = &cd->md;

	if (md->input_device_registered) {
		input_unregister_device(md->input);
	} else {
		input_free_device(md->input);
		cyttsp6_unsubscribe_attention_(dev, CY_ATTEN_STARTUP,
			CY_MODULE_MT, cyttsp6_setup_input_attention, 0);
	}

	return 0;
}
