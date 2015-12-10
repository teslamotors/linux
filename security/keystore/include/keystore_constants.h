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

#ifndef _KEYSTORE_CONSTANTS_H_
#define _KEYSTORE_CONSTANTS_H_

#include <linux/hardirq.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

#define ROOTKEY_NAME				"KendrickCanyon Root Key Name"
#define ROOTKEY_NAME_SIZE			(strlen(ROOTKEY_NAME))
#define AES_BLOCK_SIZE				16
#define AES128_KEY_SIZE				16
#define SHA256_HMAC_SIZE	                32
#define AES256_KEY_SIZE				32
#define ECC_PUB_KEY_SIZE			136
#define ECC_PRIV_KEY_SIZE			68
#define ECC_OPENSSL_PUB_KEY_SIZE		132
#define ECC_OPENSSL_PRIV_KEY_SIZE		66
#define ECC_PRIV_AND_PUBLIC_KEY_ASN1_DER_SIZE	223
#define ECC_PUBLIC_KEY_DER_SIZE			158

#define KEYSTORE_CCM_AUTH_SIZE			8
#define KEYSTORE_GCM_AUTH_SIZE			8
#define KEYSTORE_EKEY_SIZE			SHA256_HMAC_SIZE
#define KEYSTORE_AES_IV_SIZE			AES_BLOCK_SIZE
#define KEYSTORE_CLIENTS_MAX			256
#define KEYSTORE_SLOTS_MAX			256
#define KEYSTORE_BACKUP_EXTRA			185
#define KERNEL_CLIENTS_ID			"+(!$(%@#%$$)*"

#endif /* _KEYSTORE_CONSTANTS_H_ */
