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
	int ret;

	memset(out, 0, sizeof(*out));

	if (crypto_has_ablkcipher(alg_name, 0, 0) != 0) {
		out->type = 2;
	} else {
		out->type = 1;
	}

	if (unlikely(keylen > CRYPTO_CIPHER_MAX_KEY_LEN)) {
		dprintk(1,KERN_DEBUG,"Setting key failed for %s-%zu.\n",
			alg_name, keylen*8);
		return -EINVAL;
	}
	/* Copy the key from user and set to TFM. */
	copy_from_user(keyp, key, keylen);

	if (out->type == 1) {
		struct blkcipher_alg* alg;
		
		out->u.blk.s = crypto_alloc_blkcipher(alg_name, 0, CRYPTO_ALG_ASYNC);
		if (IS_ERR(out->u.blk.s)) {
			dprintk(1,KERN_DEBUG,"%s: Failed to load cipher %s\n", __func__,
				   alg_name);
			return -EINVAL;
		}

		alg = crypto_blkcipher_alg(out->u.blk.s);
		
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

		ret = crypto_blkcipher_setkey(out->u.blk.s, keyp, keylen);
		if (ret) {
			dprintk(1,KERN_DEBUG,"Setting key failed for %s-%zu.\n",
				alg_name, keylen*8);
			ret = -EINVAL;
			goto error;
		}
		
		out->blocksize = crypto_blkcipher_blocksize(out->u.blk.s);
		out->ivsize = crypto_blkcipher_ivsize(out->u.blk.s);

		out->u.blk.desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
		out->u.blk.desc.tfm = out->u.blk.s;
	
	} else { /* async */
		struct ablkcipher_alg* alg;
		
		out->u.ablk.s = crypto_alloc_ablkcipher(alg_name, 0, 0);
		if (IS_ERR(out->u.ablk.s)) {
			dprintk(1,KERN_DEBUG,"%s: Failed to load cipher %s\n", __func__,
				   alg_name);
			return -EINVAL;
		}

		alg = crypto_ablkcipher_alg(out->u.ablk.s);
		
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

		ret = crypto_ablkcipher_setkey(out->u.ablk.s, keyp, keylen);
		if (ret) {
			dprintk(1,KERN_DEBUG,"Setting key failed for %s-%zu.\n",
				alg_name, keylen*8);
			ret = -EINVAL;
			goto error;
		}

		out->blocksize = crypto_ablkcipher_blocksize(out->u.ablk.s);
		out->ivsize = crypto_ablkcipher_ivsize(out->u.ablk.s);

		out->u.ablk.async_result = kmalloc(sizeof(*out->u.ablk.async_result), GFP_KERNEL);
		if (!out->u.ablk.async_result) {
			ret = -ENOMEM;
			goto error;
		}

		memset(out->u.ablk.async_result, 0, sizeof(*out->u.ablk.async_result));
		init_completion(&out->u.ablk.async_result->completion);

		out->u.ablk.async_request = ablkcipher_request_alloc(out->u.ablk.s, GFP_KERNEL);
		if (!out->u.ablk.async_request) {
			dprintk(1,KERN_ERR,"error allocating async crypto request\n");
			ret = -ENOMEM;
			goto error;
		}

		ablkcipher_request_set_callback(out->u.ablk.async_request, CRYPTO_TFM_REQ_MAY_BACKLOG,
						cryptodev_complete, out->u.ablk.async_result);
	}

	return 0;
error:
	if (out->type == 1)
		crypto_free_blkcipher(out->u.blk.s);
	else {
		crypto_free_ablkcipher(out->u.ablk.s);
		kfree(out->u.ablk.async_result);
		ablkcipher_request_free(out->u.ablk.async_request);
	}
	
	return ret;
}

void cryptodev_cipher_deinit(struct cipher_data* cdata)
{
	if (cdata->type == 1)
		crypto_free_blkcipher(cdata->u.blk.s);
	else if (cdata->type == 2) {
		crypto_free_ablkcipher(cdata->u.ablk.s);
		kfree(cdata->u.ablk.async_result);
		ablkcipher_request_free(cdata->u.ablk.async_request);
	}
	cdata->type = 0;
}

void cryptodev_cipher_set_iv(struct cipher_data* cdata, void __user* iv, size_t iv_size)
{
	if (cdata->type == 1) {
		uint8_t _iv[EALG_MAX_BLOCK_LEN];
		
		copy_from_user(_iv, iv, min(iv_size,sizeof(_iv)));
		crypto_blkcipher_set_iv(cdata->u.blk.s, _iv, iv_size);
	} else {
		copy_from_user(cdata->u.ablk.iv, iv, min(iv_size,sizeof(cdata->u.ablk.iv)));
	}
}

static inline int waitfor (struct cryptodev_result* cr, ssize_t ret)
{
	switch (ret) {
	case 0:
		break;
	case -EINPROGRESS:
	case -EBUSY:
		ret = wait_for_completion_interruptible(
			&cr->completion);
		/* no error from wait_for_completion */
		if (ret) {
			dprintk(0,KERN_ERR,"error waiting for async request completion: %zd\n", ret);
			return ret;
		}

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
	if (cdata->type == 1)
		return crypto_blkcipher_encrypt(&cdata->u.blk.desc, sg1, sg2, len);
	else {
		int ret;
		
		INIT_COMPLETION(cdata->u.ablk.async_result->completion);
		ablkcipher_request_set_crypt(cdata->u.ablk.async_request, sg1, sg2,
				len, cdata->u.ablk.iv);
		ret = crypto_ablkcipher_encrypt(cdata->u.ablk.async_request);
		
		return waitfor(cdata->u.ablk.async_result,ret);
	}
}

ssize_t cryptodev_cipher_decrypt( struct cipher_data* cdata, struct scatterlist *sg1, struct scatterlist *sg2, size_t len)
{
	if (cdata->type == 1)
		return crypto_blkcipher_encrypt(&cdata->u.blk.desc, sg1, sg2, len);
	else {
		int ret;
		
		INIT_COMPLETION(cdata->u.ablk.async_result->completion);
		ablkcipher_request_set_crypt(cdata->u.ablk.async_request, sg1, sg2,
				len, cdata->u.ablk.iv);
		ret = crypto_ablkcipher_decrypt(cdata->u.ablk.async_request);

		return waitfor(cdata->u.ablk.async_result, ret);
	}
}

/* Hash functions */

static int _crypto_has_ahash(const char* alg_name)
{
uint32_t type = 0, mask = 0;
        type &= ~CRYPTO_ALG_TYPE_MASK;
        mask &= ~CRYPTO_ALG_TYPE_MASK;
        type |= CRYPTO_ALG_TYPE_AHASH;
        mask |= CRYPTO_ALG_TYPE_AHASH_MASK;

        return crypto_has_alg(alg_name, type, mask);
}

int cryptodev_hash_init( struct hash_data* hdata, const char* alg_name, int hmac_mode, void __user* mackey, size_t mackeylen)
{
int ret;
uint8_t hkeyp[CRYPTO_HMAC_MAX_KEY_LEN];

	if (_crypto_has_ahash(alg_name) != 0) {
		hdata->type = 2;
	} else {
		hdata->type = 1;
	}

	if (mackeylen > CRYPTO_HMAC_MAX_KEY_LEN) {
		dprintk(1,KERN_DEBUG,"Setting hmac key failed for %s-%zu.\n",
			alg_name, mackeylen*8);
		return -EINVAL;
	}
	copy_from_user(hkeyp, mackey, mackeylen);
	
	if (hdata->type == 1) {
		hdata->u.sync.s = crypto_alloc_hash(alg_name, 0, CRYPTO_ALG_ASYNC);
		if (IS_ERR(hdata->u.sync.s)) {
			dprintk(1,KERN_DEBUG,"%s: Failed to load transform for %s\n", __func__,
					   alg_name);
			return -EINVAL;
		}

		/* Copy the key from user and set to TFM. */
		if (hmac_mode != 0) {

			ret = crypto_hash_setkey(hdata->u.sync.s, hkeyp, mackeylen);
			if (ret) {
				dprintk(1,KERN_DEBUG,"Setting hmac key failed for %s-%zu.\n",
					alg_name, mackeylen*8);
				ret = -EINVAL;
				goto error;
			}
		}

		hdata->u.sync.desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
		hdata->u.sync.desc.tfm = hdata->u.sync.s;
		
		hdata->digestsize = crypto_hash_digestsize(hdata->u.sync.s);

	} else {
		hdata->u.async.s = crypto_alloc_ahash(alg_name, 0, 0);
		if (IS_ERR(hdata->u.async.s)) {
			dprintk(1,KERN_DEBUG,"%s: Failed to load transform for %s\n", __func__,
					   alg_name);
			return -EINVAL;
		}

		/* Copy the key from user and set to TFM. */
		if (hmac_mode != 0) {

			ret = crypto_ahash_setkey(hdata->u.async.s, hkeyp, mackeylen);
			if (ret) {
				dprintk(1,KERN_DEBUG,"Setting hmac key failed for %s-%zu.\n",
					alg_name, mackeylen*8);
				ret = -EINVAL;
				goto error;
			}
		}

		hdata->digestsize = crypto_ahash_digestsize(hdata->u.async.s);

		hdata->u.async.async_result = kmalloc(sizeof(*hdata->u.async.async_result), GFP_KERNEL);
		if (!hdata->u.async.async_result) {
			ret = -ENOMEM;
			goto error;
		}

		memset(hdata->u.async.async_result, 0, sizeof(*hdata->u.async.async_result));
		init_completion(&hdata->u.async.async_result->completion);

		hdata->u.async.async_request = ahash_request_alloc(hdata->u.async.s, GFP_KERNEL);
		if (!hdata->u.async.async_request) {
			dprintk(0,KERN_ERR,"error allocating async crypto request\n");
			ret = -ENOMEM;
			goto error;
		}

		ahash_request_set_callback(hdata->u.async.async_request, CRYPTO_TFM_REQ_MAY_BACKLOG,
						cryptodev_complete, hdata->u.async.async_result);

	
	}
	
	return 0;

error:
	if (hdata->type == 1)
		crypto_free_hash(hdata->u.sync.s);
	else
		crypto_free_ahash(hdata->u.async.s);
	return ret;
}

void cryptodev_hash_deinit(struct hash_data* hdata)
{
	if (hdata->type == 1)
		crypto_free_hash(hdata->u.sync.s);
	else if (hdata->type == 2)
		crypto_free_ahash(hdata->u.async.s);
	hdata->type = 0;
}

int cryptodev_hash_reset( struct hash_data* hdata)
{
int ret;
	if (hdata->type == 1) {
		ret = crypto_hash_init(&hdata->u.sync.desc);
		if (unlikely(ret)) {
			dprintk(0,KERN_ERR,
				"error in crypto_hash_init()\n");
			return ret;
		}
	} else {
		ret = crypto_ahash_init(hdata->u.async.async_request);
		if (unlikely(ret)) {
			dprintk(0,KERN_ERR,
				"error in crypto_hash_init()\n");
			return ret;
		}
	}
	
	return 0;

}

ssize_t cryptodev_hash_update( struct hash_data* hdata, struct scatterlist *sg, size_t len)
{
	if (hdata->type == 1)
		return crypto_hash_update(&hdata->u.sync.desc, sg, len);
	else {
		int ret;

		INIT_COMPLETION(hdata->u.async.async_result->completion);
		ahash_request_set_crypt(hdata->u.async.async_request, sg, NULL,
				len);
		
		ret = crypto_ahash_update(hdata->u.async.async_request);
		
		return waitfor(hdata->u.async.async_result,ret);
	}
}


int cryptodev_hash_final( struct hash_data* hdata, void* output)
{
	if (hdata->type == 1)
		return crypto_hash_final(&hdata->u.sync.desc, output);
	else {
		int ret;
		
		INIT_COMPLETION(hdata->u.async.async_result->completion);
		ahash_request_set_crypt(hdata->u.async.async_request, NULL, output, 0);
		
		ret = crypto_ahash_final(hdata->u.async.async_request);
		
		return waitfor(hdata->u.async.async_result,ret);
	}
}
