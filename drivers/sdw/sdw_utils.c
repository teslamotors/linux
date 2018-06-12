/*
 *  sdw_bwcalc.c - SoundWire Bus BW calculation & CHN Enabling implementation
 *
 *  Copyright (C) 2015-2016 Intel Corp
 *  Author:  Sanyog Kale <sanyog.r.kale@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sdw_bus.h>
#include <linux/crc8.h>



/**
 * sdw_bus_compute_crc8: SoundWire bus helper function to compute crc8.
 *			This API uses crc8 helper functions internally.
 *
 * @values: Data buffer.
 * @num_bytes: Number of bytes.
 */
u8 sdw_bus_compute_crc8(u8 *values, u8 num_bytes)
{
	u8 table[256];
	u8 poly = 0x4D; /* polynomial = x^8 + x^6 + x^3 + x^2 + 1 */
	u8 crc = CRC8_INIT_VALUE; /* Initialize 8 bit to 11111111 */

	/* Populate MSB */
	crc8_populate_msb(table, poly);

	/* CRC computation */
	crc = crc8(table, values, num_bytes, crc);

	return crc;
}
EXPORT_SYMBOL(sdw_bus_compute_crc8);
