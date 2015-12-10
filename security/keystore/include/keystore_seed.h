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
 * keystore_fill_seeds() - Retrieve SEEDs from the ABL
 *
 * Obtains the seed address from the kernel command line
 * and fills the sec_seed array with their values.
 *
 * Return: 0 if OK or negative error code (see errno).
 */
int keystore_fill_seeds(void);

extern uint8_t sec_seed[MAX_SEED_TYPES][SEC_SEED_SIZE];

#endif /* _KEYSTORE_SEED_H_ */
