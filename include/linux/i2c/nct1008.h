/*
 * linux/include/i2c/nct1008.h
 *
 * Temperature monitoring device from On Semiconductors
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_NCT1008_H
#define _LINUX_NCT1008_H

/* config params */
#define NCT1008_CONFIG_ALERT_DISABLE	0x80
#define NCT1008_CONFIG_RUN_STANDBY	0x40
#define NCT1008_CONFIG_ALERT_THERM2	0x20
#define NCT1008_CONFIG_ENABLE_EXTENDED	0x04

struct nct1008_platform_data {
	/* temperature conversion rate */
	int conv_rate;

	/* configuration parameters */
	int config;

	/* cpu shut down threshold */
	int thermal_threshold;

	/* temperature offset */
	int offset;
};

#endif /* _LINUX_NCT1008_H */
