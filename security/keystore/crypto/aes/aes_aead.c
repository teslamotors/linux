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

#include <crypto/aead.h>
#include <crypto/algapi.h>

#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "keystore_debug.h"
#include "aes_utils.h"

#define MAX_IVLEN 32

/**
 * Encrypt or decrypt a block of data using AES CCM.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param iv Pointer to the Initialization Vector.
 * @param ivlen Initialization Vector size in bytes.
 * @param data_in Pointer to the input data block.
 * @param ilen Input data block size in bytes.
 * @param alen Associated data block size in bytes.
 * @param data_out Pointer to the output buffer.
 * @param outlen Output buffer size in bytes.
 *
 * See aead_request_set_crypt() for input and output data formatting.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_aes_aead_crypt(int enc, const char *algo,
				   const char *key, size_t klen,
				   const char *iv, size_t ivlen,
				   const char *data_in, size_t ilen,
				   size_t alen,
				   char *data_out, size_t outlen)
{
	struct crypto_aead *tfm;
	struct scatterlist src, dst;
	struct aead_request *req;
	void *data_in_buf = NULL, *data_out_buf = NULL;
	unsigned int authsize = abs(outlen - ilen);
	char ivbuf[MAX_IVLEN];
	int rc;

	ks_debug(KBUILD_MODNAME ": keystore_aes_aead_crypt enc=%d klen=%zu ivlen=%zu ilen=%zu alen=%zu outlen=%zu authsize=%u\n",
		 enc, klen, ivlen, ilen, alen, outlen, authsize);

	tfm = crypto_alloc_aead(algo, 0, 0);
	if (IS_ERR(tfm)) {
		ks_err(KBUILD_MODNAME ": failed to load transform for %s: %ld\n",
		       algo,
		       PTR_ERR(tfm));
		rc = PTR_ERR(tfm);
		goto err_tfm;
	}
	req = aead_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		ks_err(KBUILD_MODNAME ": failed to allocate request for %s)\n",
			algo);
		rc = -ENOMEM;
		goto err_req;
	}

	data_in_buf = alloc_and_init_sg(&src,
					data_in,
					ilen,
					ilen);
	if (!data_in_buf) {
		ks_err(KBUILD_MODNAME ": failed to alloc data_in_buf");
		rc = -ENOMEM;
		goto err_in_buf;
	}

	data_out_buf = alloc_and_init_sg(&dst,
					 NULL,
					 ilen + (enc ? authsize : 0),
					 0);
	if (!data_out_buf) {
		ks_err(KBUILD_MODNAME ": failed to alloc data_out_buf");
		rc = -ENOMEM;
		goto err_out_buf;
	}

	crypto_aead_clear_flags(tfm, ~0);
	rc = crypto_aead_setkey(tfm, key, klen);
	if (rc) {
		ks_err(KBUILD_MODNAME ": crypto_aead_setkey failed, flags=%x\n",
				crypto_aead_get_flags(tfm));
		goto err_setkey;
	}
	rc = crypto_aead_setauthsize(tfm, authsize);
	if (rc) {
		ks_err(KBUILD_MODNAME ": failed to set authsize to %u\n",
				authsize);
		goto err_setkey;
	}

	memcpy(ivbuf, iv, ivlen);
	memset(ivbuf + ivlen, 0, sizeof(ivbuf) - ivlen);

	aead_request_set_crypt(req, &src, &dst, ilen, ivbuf);

	aead_request_set_ad(req, alen);

	rc = enc ? crypto_aead_encrypt(req) : crypto_aead_decrypt(req);
	if (rc)	{
		ks_err(KBUILD_MODNAME ": %s failed, error %d\n",
		       enc ? "crypto_aead_encrypt" : "crypto_aead_decrypt",
		       rc);
	} else {
		/* OK */
		memcpy(data_out, data_out_buf, outlen);
	}

err_setkey:
	kzfree(data_out_buf);
err_out_buf:
	kzfree(data_in_buf);
err_in_buf:
	aead_request_free(req);
err_req:
	crypto_free_aead(tfm);
err_tfm:
	return rc;
}

int keystore_aes_ccm_crypt(int enc, const char *key, size_t klen,
			   const char *iv, size_t ivlen,
			   const char *data_in, size_t ilen,
			   size_t alen,
			   char *data_out, size_t outlen)
{
	return keystore_aes_aead_crypt(enc, "ccm(aes)", key, klen,
				       iv, ivlen, data_in, ilen, alen,
				       data_out, outlen);
}

int keystore_aes_gcm_crypt(int enc, const char *key, size_t klen,
			   const char *iv, size_t ivlen,
			   const char *data_in, size_t ilen,
			   size_t alen,
			   char *data_out, size_t outlen)
{
	return keystore_aes_aead_crypt(enc, "gcm(aes)", key, klen,
				       iv, ivlen, data_in, ilen, alen,
				       data_out, outlen);
}

/* end of file */
