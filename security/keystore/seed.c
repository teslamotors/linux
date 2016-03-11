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

#include <asm/page.h>
#include <linux/errno.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>

#include <security/abl_cmdline.h>
#include <security/keystore_api_user.h>
#include "keystore_debug.h"
#include "keystore_seed.h"

/* Device and User seed - derived from platform seed and nonces from PDR */
uint8_t sec_seed[MAX_SEED_TYPES][SEC_SEED_SIZE];

bool seed_available[MAX_SEED_TYPES];

static int keystore_copy_seeds(struct seed_offset *offset)
{
	int res = 0;

	/* Set default availability */
	seed_available[SEED_TYPE_DEVICE] = false;
	seed_available[SEED_TYPE_USER] = false;

	/* each seed should be atleast SEC_SEED_SIZE */
	if (!offset) {
		ks_err(KBUILD_MODNAME ": keystore_get_seed: ksoffset: %p\n",
				offset);
		return -EFAULT;
	}

	/* First zero the contents */
	memset(sec_seed, 0, sizeof(sec_seed));

#if !defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
	if (!offset->device_seed) {
		ks_err(KBUILD_MODNAME ": No device SEED address given\n");
		return -EINVAL;
	}

	res = memcpy_from_ph(sec_seed[SEED_TYPE_DEVICE],
			     (phys_addr_t)offset->device_seed,
			     (size_t)SEC_SEED_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": Error retrieving Device SEED\n");
		return res;
	}

	seed_available[SEED_TYPE_DEVICE] = true;
#endif

	memset_ph((phys_addr_t)offset->device_seed, 0, (size_t)SEC_SEED_SIZE);

	if (offset->user_seed) {
#if !defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
		res = memcpy_from_ph(sec_seed[SEED_TYPE_USER],
				     (phys_addr_t)offset->user_seed,
				     (size_t)SEC_SEED_SIZE);
		if (res) {
			ks_err(KBUILD_MODNAME ": Error retrieving User SEED\n");
			return res;
		}

		seed_available[SEED_TYPE_USER] = true;

#endif
		memset_ph((phys_addr_t)offset->user_seed, 0,
			  (size_t)SEC_SEED_SIZE);
	}

	return res;
}

int keystore_fill_seeds(void)
{
	int res = 0;
	unsigned int indx = 0;
	struct seed_offset s_off;

#ifdef CONFIG_KEYSTORE_HARD_CODED_SEED
	/* return constant SEED for testing */
	for (indx = 0; indx < MAX_SEED_TYPES; indx++) {
		memset(sec_seed[indx], ((indx + 0xa5) & 0xFF), SEC_SEED_SIZE);
		keystore_hexdump("Keystore SEED",
				 sec_seed[indx], SEC_SEED_SIZE);
		seed_available[indx] = true;
	}

#if defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
	seed_available[SEED_TYPE_DEVICE] = false;
#elif defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
	seed_available[SEED_TYPE_USER] = false;
#endif

#else
	/* Get keys and seeds offsets from cmdline
	 * - ikey, okey, keysize, dseed and useed.
	 */
	res = get_seed_offsets(&s_off);
	if (res) {
		ks_err(KBUILD_MODNAME
		       ": d/u-seed info missing in cmdline\n");
		return res;
	}

	res = keystore_copy_seeds(&s_off);

#endif
	return res;
}

const uint8_t *keystore_get_seed(enum keystore_seed_type type)
{
	if ((type != SEED_TYPE_DEVICE) ||
	    (type != SEED_TYPE_USER))
		return NULL;

	if (!seed_available[type])
		return NULL;

	return sec_seed[type];
}

/* end of file */
