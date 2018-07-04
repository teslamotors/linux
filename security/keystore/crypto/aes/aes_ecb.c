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

#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "keystore_debug.h"
#include "aes_utils.h"

/**
 * Encrypt or decrypt a block of data using AES ECB.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param data_in Pointer to the input data block.
 * @param data_out Pointer to the output buffer.
 * @param len Input/output data size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_aes_crypt(int enc, const char *key, size_t klen,
		const char *data_in, char *data_out, size_t len)
{
	struct crypto_blkcipher *tfm;
	struct blkcipher_desc desc;
	struct scatterlist src, dst;
	void *data_in_buf = NULL, *data_out_buf = NULL;
	int rc;

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ": keystore_aes_crypt enc=%d klen=%zu len=%zu\n"
			, enc, klen, len);
#endif

	tfm = crypto_alloc_blkcipher("ecb(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		ks_err(KBUILD_MODNAME ": failed to load transform for ecb(aes): %ld\n",
		       PTR_ERR(tfm));
		rc = PTR_ERR(tfm);
		goto err_tfm;
	}

	desc.tfm = tfm;
	desc.flags = 0;

	data_in_buf = alloc_and_init_sg(&src, data_in, len, len);
	if (!data_in_buf) {
		ks_err(KBUILD_MODNAME ": failed to alloc data_in_buf");
		rc = -ENOMEM;
		goto err_in_buf;
	}

	data_out_buf = alloc_and_init_sg(&dst, NULL, len, 0);
	if (!data_out_buf) {
		ks_err(KBUILD_MODNAME ": failed to alloc data_out_buf");
		rc = -ENOMEM;
		goto err_out_buf;
	}

	rc = crypto_blkcipher_setkey(tfm, key, klen);
	if (rc) {
		ks_err(KBUILD_MODNAME ": crypto_blkcipher_setkey failed, flags=%x\n",
				crypto_blkcipher_get_flags(tfm));
		goto err_setkey;
	}

	rc = enc ?
		crypto_blkcipher_encrypt(&desc, &dst, &src, len) :
		crypto_blkcipher_decrypt(&desc, &dst, &src, len);
	if (rc) {
		ks_err(KBUILD_MODNAME ": %s failed, error %d\n",
		       enc ?
		       "crypto_blkcipher_encrypt" : "crypto_blkcipher_decrypt",
		       rc);
	} else {
		/* OK */
		memcpy(data_out, data_out_buf, len);
	}

err_setkey:
	kzfree(data_out_buf);

err_out_buf:
	kzfree(data_in_buf);

err_in_buf:
	crypto_free_blkcipher(tfm);

err_tfm:
	return rc;
}

/* end of file */
