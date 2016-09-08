/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/types.h>
#include <linux/random.h>

#include "utils.h"

/*
 * Big endian/little endian conversion, adapted from ecc.c
 */
void string2native(uint32_t *p_native, uint8_t *p_bytes, uint32_t len)
{
	uint32_t i;

	if (!p_native || !p_bytes)
		return;

	for (i = 0; i < len; ++i) {
		uint8_t *p_digit = p_bytes + 4 * (len - 1 - i);

		p_native[i] = ((uint32_t) p_digit[0] << 24) |
			((uint32_t) p_digit[1] << 16) |
			((uint32_t) p_digit[2] << 8)
			| (uint32_t) p_digit[3];
	}
}

void native2string(uint8_t *p_bytes, uint32_t *p_native, uint32_t len)
{
	uint32_t i;

	if (!p_native || !p_bytes)
		return;

	for (i = 0; i < len; ++i) {
		uint8_t *p_digit = p_bytes + 4 * (len - 1 - i);

		p_digit[0] = p_native[i] >> 24;
		p_digit[1] = p_native[i] >> 16;
		p_digit[2] = p_native[i] >> 8;
		p_digit[3] = p_native[i];
	}
}

/*
 * RNG abstraction. Please use a proper/secure RNG.
 */

int rbg_init(void)
{
	return 0;
}

void getRandomBytes(void *p_dest, unsigned p_size)
{
	get_random_bytes(p_dest, p_size);
}
