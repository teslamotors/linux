/*
 * bq25890_charger.h: platform data structure for bq25890 driver
 *
 * (C) Copyright 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __BQ25890_CHARGER_H__
#define __BQ25890_CHARGER_H__

struct bq25890_platform_data {
	int def_cc;	/* in uA */
	int def_cv;	/* in uV */
	int iterm;	/* in uA */
	int iprechg;	/* in uA */
	int sysvmin;	/* in uA */
	int boosti;	/* in uA */
	int boostv;	/* in uV */
	int max_cv;	/* in uV */
	int max_cc;	/* in uA */
	int min_temp;	/* in DegC */
	int max_temp;	/* in DegC */
	bool boostf_low;
	bool en_thermal_reg;
	bool en_ilim_pin;
};

#endif
