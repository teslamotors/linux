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

#ifndef _KEYSTORE_TESTS_H_
#define _KEYSTORE_TESTS_H_

#ifdef CONFIG_KEYSTORE_TESTMODE

#include <linux/types.h>
#include <linux/list.h>

/**
 * Run all of the Tests
 *
 * @return 0 if OK or negative error code (see errno).
 *
 */
int keystore_run_tests(void);

/**
 * Test the AES_SIV.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes_siv(void);

/**
 * Test the AES_SIV helper functions.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes_siv_steps(void);

/**
 * Test the CMAC (AES-128).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_cmac(void);

/**
 * Test the SHA-256 HMAC.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_hmac(void);

/**
 * Test the AES encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes(void);

/**
 * Test ECC signing
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_ecc(void);

/**
 * Test GPG to RAW conversion
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_gpg_to_raw(void);

int keystore_test_genkey(void);

int keystore_test_wrap_unwrap(void);

int keystore_test_encrypt_decrypt_ccm(void);

int keystore_test_encrypt_decrypt_gcm(void);

int keystore_test_encrypt_for_host(void);

int _keystore_assert(bool x, const char *test,
			 const char *func, unsigned int line);

#define xstr(s) str(s)
#define str(s) #s
#define keystore_assert(x) _keystore_assert((x), xstr(x), __func__, __LINE__)

struct keystore_test {
	const char *name;
	int (*run)(void);
	int result;
};


/* Dummy function if testing is not configured */
/* Static inline so will be optimised away by compiler */
#else /* CONFIG_KEYSTORE_TESTMODE */
static inline int keystore_run_tests(void) { return -EPERM; }
#endif


#endif /* _KEYSTORE_TESTS_H_ */
