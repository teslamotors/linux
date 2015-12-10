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

#include <crypto/hash.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>

#include "keystore_mac.h"
#include "keystore_debug.h"

/**
 * Calculate SHA-256 HMAC or AES-128 CMAC.
 *
 * @param alg_name Algorithm name ("hmac(sha256)" or "cmac(aes)").
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param data_in Pointer to the data block.
 * @param dlen Data block size in bytes.
 * @param hash_out Pointer to the output buffer.
 * @param outlen Output buffer size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_calc_mac(const char *alg_name, const char *key, size_t klen,
		      const char *data_in, size_t dlen,
		      char *hash_out, size_t outlen)
{
	struct crypto_ahash *tfm;
	struct scatterlist sg;
	struct ahash_request *req;
	void *hash_buf;
	int rc;

	memset(hash_out, 0, outlen);

	tfm = crypto_alloc_ahash(alg_name, 0, 0);
	if (IS_ERR(tfm)) {
		ks_err(KBUILD_MODNAME": crypto_alloc_ahash failed.\n");
		rc = PTR_ERR(tfm);
		goto err_tfm;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		ks_err(KBUILD_MODNAME ": failed to allocate request for mac\n");
		rc = -ENOMEM;
		goto err_req;
	}

	if (crypto_ahash_digestsize(tfm) > outlen) {
		ks_err(KBUILD_MODNAME ": tfm size > result buffer.\n");
		rc = -EINVAL;
		goto err_hash_buf;
	}

	hash_buf = kzalloc(dlen, GFP_KERNEL);
	if (!hash_buf) {
		rc = -ENOMEM;
		goto err_hash_buf;
	}

	memcpy(hash_buf, data_in, dlen);
	sg_init_one(&sg, hash_buf, dlen);
	crypto_ahash_clear_flags(tfm, ~0);
	rc = crypto_ahash_setkey(tfm, key, klen);
	if (rc) {
		ks_err(KBUILD_MODNAME": crypto_ahash_setkey failed\n");
	} else {
		char hash_tmp[crypto_ahash_digestsize(tfm)];

		memset(hash_tmp, 0, sizeof(hash_tmp));

		ahash_request_set_crypt(req, &sg, hash_tmp, dlen);

		rc = crypto_ahash_digest(req);
		if (rc == 0) {
			/* OK */
			memcpy(hash_out, hash_tmp, sizeof(hash_tmp));
		} else {
			ks_err(KBUILD_MODNAME": crypto_ahash_digest failed\n");
		}
	}

	kfree(hash_buf);

err_hash_buf:
	ahash_request_free(req);

err_req:
	crypto_free_ahash(tfm);

err_tfm:
	return rc;
}

/**
 * Calculate SHA-256 digest of a data block.
 *
 * @param data The data block pointer.
 * @param size The size of data in bytes.
 * @param result The result digest pointer.
 * @param result_size The result digest block size.
 *
 * @return 0 if OK or negative error code.
 */
int keystore_sha256_block(const void *data, unsigned int size,
			  void *result, unsigned int result_size)
{
	struct crypto_hash *tfm;
	struct hash_desc desc;
	struct scatterlist sg;

	if (!data || !result)
		return -EFAULT;

	tfm = crypto_alloc_hash("sha256",
				CRYPTO_ALG_TYPE_HASH,
				CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		int res = PTR_ERR(tfm);

		return (res < 0) ? res : -res;
	}

	if (result_size < crypto_hash_digestsize(tfm)) {
		crypto_free_hash(tfm);
		return -EINVAL;
	}

	desc.tfm = tfm;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	sg_init_one(&sg, data, size);

	crypto_hash_init(&desc);
	crypto_hash_update(&desc, &sg, size);
	crypto_hash_final(&desc, result);

	crypto_free_hash(tfm);

	return 0;
}

/* end of file */
