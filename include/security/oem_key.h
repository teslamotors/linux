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

#ifndef _OEM_KEY_H_
#define _OEM_KEY_H_

#define rsa_key_N_BITS     2048
#define rsa_key_E_BITS     32

#define rsa_key_N_BYTES    ((rsa_key_N_BITS + 7) / 8)
#define rsa_key_E_BYTES    ((rsa_key_E_BITS + 7) / 8)
#define rsa_key_SIZE       (rsa_key_N_BYTES + rsa_key_E_BYTES)

#define RSA_SIGNATURE_SIZE  rsa_key_N_BYTES

#define KERNEL_PUBLIC_RSA_KEY_SIZE		260
#define GPG_PUBLIC_RSA_KEY_SIZE			264

/**
 * oem_key_verify_digest() - Check signature of message digest
 * and verify the signature against the RSA Root Key.
 *
 * @digest: SHA256 digest of data to be verified.
 * @digest_size: Size of the digest
 * @signature: Public key signature pointer (raw data, MSB first).
 * @signature_size: Signature size in bytes.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int oem_key_verify_digest(void *digest, unsigned int digest_size,
			  const void *signature, unsigned int signature_size);

#endif /* _KEYSTORE_DEV_H_ */
