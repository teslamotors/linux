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
 * ANSI X9.63 KDF
 *
 * Key derivation and expansion based on a hash function
 *
 * Currently only supports SHA2-256
 *
 */

/* TODO hash should be configurable by caller */
#define USE_CRYPTOAPI
/*#define X963_SHA1*/
/*#define X963_SHA256 */

int kdf_x963(uint8_t *key, uint32_t keylen, uint8_t *rand, uint32_t randlen,
	     uint8_t *shared, uint32_t sharedlen);
