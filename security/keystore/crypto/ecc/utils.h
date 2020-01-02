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

/*
 * Common utility functions
 */

void string2native(uint32_t *p_native, uint8_t *p_bytes, uint32_t len);
void native2string(uint8_t *p_bytes, uint32_t *p_native, uint32_t len);

/* RNG abstraction */
int rbg_init(void);
void getRandomBytes(void *p_dest, unsigned p_size);
