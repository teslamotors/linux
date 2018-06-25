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

int keystore_calc_mac(const char *alg_name, const char *key, size_t klen,
		      const char *data_in, size_t dlen,
		      char *hash_out, size_t outlen)
{
	struct crypto_ahash *tfm;
	struct scatterlist sg;
	struct ahash_request *req;
	void *hash_buf;
	int rc;

	if (!alg_name || !key || !data_in || !hash_out)
		return -EFAULT;

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
	struct crypto_shash *tfm;
	struct shash_desc *sdesc;
	int shash_desc_size;

	if (!data || !result)
		return -EFAULT;

	tfm = crypto_alloc_shash("sha256",
				CRYPTO_ALG_TYPE_HASH,
				CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		int res = PTR_ERR(tfm);
		return (res < 0) ? res : -res;
	}

	shash_desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
	sdesc = kmalloc(shash_desc_size, GFP_KERNEL);
	if (!sdesc) {
		crypto_free_shash(tfm);
		ks_err("ecc: memory allocation error\n");
		return -ENOMEM;
	}

	if (result_size < crypto_shash_digestsize(tfm)) {
		crypto_free_shash(tfm);
		return -EINVAL;
	}

	sdesc->tfm = tfm;
	sdesc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	crypto_shash_init(sdesc);
	crypto_shash_update(sdesc, data, size);
	crypto_shash_final(sdesc, result);

	crypto_free_shash(tfm);

	return 0;
}

/* end of file */
