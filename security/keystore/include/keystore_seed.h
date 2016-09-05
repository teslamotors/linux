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

#ifndef _KEYSTORE_SEED_H_
#define _KEYSTORE_SEED_H_

#include <linux/types.h>
#include <security/keystore_api_user.h>
#include <security/abl_cmdline.h>

#if   defined(CONFIG_KEYSTORE_SEED_SIZE_64)
#define SEC_SEED_SIZE				64
#elif defined(CONFIG_KEYSTORE_SEED_SIZE_32)
#define SEC_SEED_SIZE				32
#else
#error "SEED size not set! Speficy KEYSTORE_SEED_SIZE_XX (XX=32,64) in Kconfig."
#endif

#define MAX_SEED_TYPES 2

/**
 * DOC: Seed
 *
 * The seed interface provisions and provides access to the secret seed
 * values which Keystore uses to derive all client and internal keys.
 *
 * The seeds are first filled using the keystore_fill_seeds() functions,
 * after which they can be accessed via the sec_seed variable.
 *
 * There are two types of seed: device and user seed. The user seed
 * is used to wrap keys which encrypt data associated with a user
 * of the keystore device, such as personal data. Device seeds
 * are used to encrypt any other data.
 */

/**
 * keystore_fill_seeds() - Retrieve SEEDs from the ABL
 *
 * Obtains the seed address from the kernel command line
 * and fills the sec_seed array with their values.
 *
 * The function will look for at least the device SEED address
 * on the command line. If this is not present, the function
 * will fail.
 *
 * If the user seed is not present, the function fill fail but
 * the seed will not be available for wrapping.
 *
 * Return: 0 if OK or negative error code (see errno).
 */
int keystore_fill_seeds(void);

/**
 * keystore_get_seed() - Get a pointer to the seed
 *
 * @type: The type of seed to return.
 *
 * Returns: A pointer to the SEED, which is an
 * array of size %SEC_SEED_SIZE. If the seed type is not
 * valid, or the seed is not available, the function
 * will return %NULL.
 *
 */
const uint8_t *keystore_get_seed(enum keystore_seed_type type);



#endif /* _KEYSTORE_SEED_H_ */
