/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2010 Nikos Mavrogiannopoulos <nmav@gnutls.org>
 *
 * This file is part of linux cryptodev.
 *
 * cryptodev is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cryptodev is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/random.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <linux/scatterlist.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include "cryptodev.h"
#include "cryptodev_int.h"


struct cryptodev_result {
	struct completion completion;
	int err;
};

static void cryptodev_complete(struct crypto_async_request *req, int err)
{
	struct cryptodev_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

int cryptodev_cipher_init(struct cipher_data* out, const char* alg_name, uint8_t __user * key, size_t keylen)
{
	uint8_t keyp[CRYPTO_CIPHER_MAX_KEY_LEN];
	struct ablkcipher_alg* alg;
	int ret;

	memset(out, 0, sizeof(*out));

	out->init = 1;

	if (unlikely(keylen > CRYPTO_CIPHER_MAX_KEY_LEN)) {
		dprintk(1,KERN_DEBUG,"Setting key failed for %s-%zu.\n",
			alg_name, keylen*8);
		return -EINVAL;
	}
	/* Copy the key from user and set to TFM. */
	copy_from_user(keyp, key, keylen);

	out->async.s = crypto_alloc_ablkcipher(alg_name, 0, 0);
	if (IS_ERR(out->async.s)) {
		dprintk(1,KERN_DEBUG,"%s: Failed to load cipher %s\n", __func__,
			   alg_name);
		return -EINVAL;
	}

	alg = crypto_ablkcipher_alg(out->async.s);
	
	if (alg != NULL) {
		/* Was correct key length supplied? */
		if ((keylen < alg->min_keysize) ||
			(keylen > alg->max_keysize)) {
			dprintk(1,KERN_DEBUG,"Wrong keylen '%zu' for algorithm '%s'. Use %u to %u.\n",
				   keylen, alg_name, alg->min_keysize, 
				   alg->max_keysize);
			ret = -EINVAL;
			goto error;
		}
	}

	ret = crypto_ablkcipher_setkey(out->async.s, keyp, keylen);
	if (ret) {
		dprintk(1,KERN_DEBUG,"Setting key failed for %s-%zu.\n",
			alg_name, keylen*8);
		ret = -EINVAL;
		goto error;
	}

	out->blocksize = crypto_ablkcipher_blocksize(out->async.s);
	out->ivsize = crypto_ablkcipher_ivsize(out->async.s);

	out->async.result = kmalloc(sizeof(*out->async.result), GFP_KERNEL);
	if (!out->async.result) {
		ret = -ENOMEM;
		goto error;
	}

	memset(out->async.result, 0, sizeof(*out->async.result));
	init_completion(&out->async.result->completion);

	out->async.request = ablkcipher_request_alloc(out->async.s, GFP_KERNEL);
	if (!out->async.request) {
		dprintk(1,KERN_ERR,"error allocating async crypto request\n");
		ret = -ENOMEM;
		goto error;
	}

	ablkcipher_request_set_callback(out->async.request, CRYPTO_TFM_REQ_MAY_BACKLOG,
					cryptodev_complete, out->async.result);

	return 0;
error:
	crypto_free_ablkcipher(out->async.s);
	kfree(out->async.result);
	ablkcipher_request_free(out->async.request);
	
	return ret;
}

void cryptodev_cipher_deinit(struct cipher_data* cdata)
{
	crypto_free_ablkcipher(cdata->async.s);
	kfree(cdata->async.result);
	ablkcipher_request_free(cdata->async.request);

	cdata->init = 0;
}

void cryptodev_cipher_set_iv(struct cipher_data* cdata, void __user* iv, size_t iv_size)
{
	copy_from_user(cdata->async.iv, iv, min(iv_size,sizeof(cdata->async.iv)));
}

static inline int waitfor (struct cryptodev_result* cr, ssize_t ret)
{
	switch (ret) {
	case 0:
		break;
	case -EINPROGRESS:
	case -EBUSY:
		wait_for_completion(&cr->completion);
		/* no error from wait_for_completion */

		if (cr->err) {
			dprintk(0,KERN_ERR,"error from async request: %zd \n", ret);
                        return cr->err; 
		}

		break;
	default:
		return ret;
	}
	
	return 0;
}

ssize_t cryptodev_cipher_encrypt( struct cipher_data* cdata, struct scatterlist *sg1, struct scatterlist *sg2, size_t len)
{
	int ret;
	
	INIT_COMPLETION(cdata->async.result->completion);
	ablkcipher_request_set_crypt(cdata->async.request, sg1, sg2,
			len, cdata->async.iv);
	ret = crypto_ablkcipher_encrypt(cdata->async.request);
	
	return waitfor(cdata->async.result,ret);
}

ssize_t cryptodev_cipher_decrypt( struct cipher_data* cdata, struct scatterlist *sg1, struct scatterlist *sg2, size_t len)
{
	int ret;
	
	INIT_COMPLETION(cdata->async.result->completion);
	ablkcipher_request_set_crypt(cdata->async.request, sg1, sg2,
			len, cdata->async.iv);
	ret = crypto_ablkcipher_decrypt(cdata->async.request);

	return waitfor(cdata->async.result, ret);
}

/* Hash functions */

int cryptodev_hash_init( struct hash_data* hdata, const char* alg_name, int hmac_mode, void __user* mackey, size_t mackeylen)
{
int ret;
uint8_t hkeyp[CRYPTO_HMAC_MAX_KEY_LEN];

	hdata->init = 1;

	if (mackeylen > CRYPTO_HMAC_MAX_KEY_LEN) {
		dprintk(1,KERN_DEBUG,"Setting hmac key failed for %s-%zu.\n",
			alg_name, mackeylen*8);
		return -EINVAL;
	}
	copy_from_user(hkeyp, mackey, mackeylen);
	
	hdata->async.s = crypto_alloc_ahash(alg_name, 0, 0);
	if (IS_ERR(hdata->async.s)) {
		dprintk(1,KERN_DEBUG,"%s: Failed to load transform for %s\n", __func__,
				   alg_name);
		return -EINVAL;
	}

	/* Copy the key from user and set to TFM. */
	if (hmac_mode != 0) {

		ret = crypto_ahash_setkey(hdata->async.s, hkeyp, mackeylen);
		if (ret) {
			dprintk(1,KERN_DEBUG,"Setting hmac key failed for %s-%zu.\n",
				alg_name, mackeylen*8);
			ret = -EINVAL;
			goto error;
		}
	}

	hdata->digestsize = crypto_ahash_digestsize(hdata->async.s);

	hdata->async.result = kmalloc(sizeof(*hdata->async.result), GFP_KERNEL);
	if (!hdata->async.result) {
		ret = -ENOMEM;
		goto error;
	}

	memset(hdata->async.result, 0, sizeof(*hdata->async.result));
	init_completion(&hdata->async.result->completion);

	hdata->async.request = ahash_request_alloc(hdata->async.s, GFP_KERNEL);
	if (!hdata->async.request) {
		dprintk(0,KERN_ERR,"error allocating async crypto request\n");
		ret = -ENOMEM;
		goto error;
	}

	ahash_request_set_callback(hdata->async.request, CRYPTO_TFM_REQ_MAY_BACKLOG,
					cryptodev_complete, hdata->async.result);


	return 0;

error:
	crypto_free_ahash(hdata->async.s);
	return ret;
}

void cryptodev_hash_deinit(struct hash_data* hdata)
{
	crypto_free_ahash(hdata->async.s);
	hdata->init = 0;
}

int cryptodev_hash_reset( struct hash_data* hdata)
{
int ret;
	ret = crypto_ahash_init(hdata->async.request);
	if (unlikely(ret)) {
		dprintk(0,KERN_ERR,
			"error in crypto_hash_init()\n");
		return ret;
	}
	
	return 0;

}

ssize_t cryptodev_hash_update( struct hash_data* hdata, struct scatterlist *sg, size_t len)
{
	int ret;

	INIT_COMPLETION(hdata->async.result->completion);
	ahash_request_set_crypt(hdata->async.request, sg, NULL,
			len);
	
	ret = crypto_ahash_update(hdata->async.request);
	
	return waitfor(hdata->async.result,ret);
}


int cryptodev_hash_final( struct hash_data* hdata, void* output)
{
	int ret;
	
	INIT_COMPLETION(hdata->async.result->completion);
	ahash_request_set_crypt(hdata->async.request, NULL, output, 0);
	
	ret = crypto_ahash_final(hdata->async.request);
	
	return waitfor(hdata->async.result,ret);
}
