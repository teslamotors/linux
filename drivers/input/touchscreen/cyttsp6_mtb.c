/*
 * cyttsp6_mtb.c
 * Cypress TrueTouch(TM) Standard Product V4 Multi-Touch Protocol B Module.
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
#include <linux/input/mt.h>
#include <linux/version.h>

static void cyttsp6_final_sync(struct input_dev *input, int max_slots,
		int mt_sync_count, unsigned long *ids)
{
	int t;

	for (t = 0; t < max_slots; t++) {
		if (test_bit(t, ids))
			continue;
		input_mt_slot(input, t);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
	}

	input_sync(input);
}

static void cyttsp6_input_report(struct input_dev *input, int sig,
		int t, int type)
{
	input_mt_slot(input, t);

	if (type == CY_OBJ_STANDARD_FINGER || type == CY_OBJ_GLOVE
			|| type == CY_OBJ_HOVER)
		input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
	else if (type == CY_OBJ_STYLUS)
		input_mt_report_slot_state(input, MT_TOOL_PEN, true);
}

static void cyttsp6_report_slot_liftoff(struct cyttsp6_mt_data *md,
		int max_slots)
{
	int t;

	if (md->num_prv_rec == 0)
		return;

	for (t = 0; t < max_slots; t++) {
		input_mt_slot(md->input, t);
		input_mt_report_slot_state(md->input,
			MT_TOOL_FINGER, false);
	}
}

static int cyttsp6_input_register_device(struct input_dev *input, int max_slots)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
	input_mt_init_slots(input, max_slots, 0);
#else
	input_mt_init_slots(input, max_slots);
#endif

	return input_register_device(input);
}

void cyttsp6_init_function_ptrs(struct cyttsp6_mt_data *md)
{
	md->mt_function.report_slot_liftoff = cyttsp6_report_slot_liftoff;
	md->mt_function.final_sync = cyttsp6_final_sync;
	md->mt_function.input_sync = NULL;
	md->mt_function.input_report = cyttsp6_input_report;
	md->mt_function.input_register_device = cyttsp6_input_register_device;
}
