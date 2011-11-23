/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2011 Nikos Mavrogiannopoulos <nmav@gnutls.org>
 *
 * This file is part of linux cryptodev.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * This file handles the AEAD part of /dev/crypto.
 *
 */

#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/ioctl.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <crypto/cryptodev.h>
#include <crypto/scatterwalk.h>
#include <linux/scatterlist.h>
#include "cryptodev_int.h"
#include "version.h"


/* make cop->src and cop->dst available in scatterlists */
static int get_userbuf_aead(struct csession *ses, struct kernel_crypt_auth_op *kcaop,
			struct scatterlist **auth_sg, struct scatterlist **dst_sg, 
			int *tot_pages)
{
	int dst_pagecount = 0, pagecount;
	int auth_pagecount = 0;
	struct crypt_auth_op *caop = &kcaop->caop;
	int rc;

	if (caop->dst == NULL && caop->auth_src == NULL)
		return -EINVAL;

	if (ses->alignmask) {
		if (!IS_ALIGNED((unsigned long)caop->dst, ses->alignmask))
			dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)caop->dst, ses->alignmask + 1);
		if (!IS_ALIGNED((unsigned long)caop->auth_src, ses->alignmask))
			dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)caop->auth_src, ses->alignmask + 1);
	}

	if (kcaop->dst_len == 0) {
		dprintk(1, KERN_WARNING, "Destination length cannot be zero\n");
		return -EINVAL;
	}

	if (caop->auth_len > 0)
		auth_pagecount = PAGECOUNT(caop->auth_src, caop->auth_len);

	dst_pagecount = PAGECOUNT(caop->dst, kcaop->dst_len);

	(*tot_pages) = pagecount = auth_pagecount + dst_pagecount;

	rc = adjust_sg_array(ses, pagecount);
	if (rc)
		return rc;

	if (auth_pagecount > 0) {
		rc = __get_userbuf(caop->auth_src, caop->auth_len, 0, auth_pagecount,
				   ses->pages, ses->sg, kcaop->task, kcaop->mm);
		if (unlikely(rc)) {
			dprintk(1, KERN_ERR,
				"failed to get user pages for data input\n");
			return -EINVAL;
		}
		(*auth_sg) = ses->sg;
		(*dst_sg) = ses->sg + auth_pagecount;
	} else {
		(*auth_sg) = NULL;
		(*dst_sg) = ses->sg;
	}

	rc = __get_userbuf(caop->dst, kcaop->dst_len, 1, dst_pagecount,
	                   ses->pages + auth_pagecount, *dst_sg, kcaop->task, kcaop->mm);
	if (unlikely(rc)) {
		release_user_pages(ses->pages, auth_pagecount);
		dprintk(1, KERN_ERR,
			"failed to get user pages for data input\n");
		return -EINVAL;
	}

	return 0;
}

static int get_userbuf_srtp(struct csession *ses, struct kernel_crypt_auth_op *kcaop,
			struct scatterlist **auth_sg, struct scatterlist **dst_sg, 
			int *tot_pages)
{
	int pagecount, diff;
	int auth_pagecount = 0;
	struct crypt_auth_op *caop = &kcaop->caop;
	int rc;

	if (caop->dst == NULL && caop->auth_src == NULL)
		return -EINVAL;

	if (ses->alignmask) {
		if (!IS_ALIGNED((unsigned long)caop->dst, ses->alignmask))
			dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)caop->dst, ses->alignmask + 1);
		if (!IS_ALIGNED((unsigned long)caop->auth_src, ses->alignmask))
			dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)caop->auth_src, ses->alignmask + 1);
	}

	if (unlikely(kcaop->dst_len == 0 || caop->auth_len == 0)) {
		dprintk(1, KERN_WARNING, "Destination length cannot be zero\n");
		return -EINVAL;
	}

	/* Note that in SRTP auth data overlap with data to be encrypted (dst)
         */

	auth_pagecount = PAGECOUNT(caop->auth_src, caop->auth_len);
	diff = (int)(caop->src - caop->auth_src);
	if (diff > 256 || diff < 0) {
		dprintk(1, KERN_WARNING, "auth_src must overlap with src (diff: %d).\n", diff);
		return -EINVAL;
	}

	(*tot_pages) = pagecount = auth_pagecount;

	rc = adjust_sg_array(ses, pagecount);
	if (rc)
		return rc;

	rc = __get_userbuf(caop->auth_src, caop->auth_len, 1, auth_pagecount,
			   ses->pages, ses->sg, kcaop->task, kcaop->mm);
	if (unlikely(rc)) {
		dprintk(1, KERN_ERR,
			"failed to get user pages for data input\n");
		return -EINVAL;
	}
	(*auth_sg) = ses->sg;

	memcpy(&ses->sg2, ses->sg, sizeof(ses->sg[0]));
	ses->sg2.offset += diff;
	(*dst_sg) = &ses->sg2;

	return 0;
}


static int fill_kcaop_from_caop(struct kernel_crypt_auth_op *kcaop, struct fcrypt *fcr)
{
	struct crypt_auth_op *caop = &kcaop->caop;
	struct csession *ses_ptr;
	int rc;

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, caop->ses);
	if (unlikely(!ses_ptr)) {
		dprintk(1, KERN_ERR, "invalid session ID=0x%08X\n", caop->ses);
		return -EINVAL;
	}

	if (caop->src != caop->dst) {
		dprintk(2, KERN_ERR,
			"Non-inplace encryption and decryption is not efficient\n");
		if (caop->len > sizeof(ses_ptr->buffer)) {
			dprintk(1, KERN_ERR,
				"The provided buffer is too large\n");
			rc = -EINVAL;
			goto out_unlock;
		}

		if (unlikely(copy_from_user(ses_ptr->buffer, caop->src, caop->len))) {
			rc = -EFAULT;
			goto out_unlock;
		}

		if (unlikely(copy_to_user(caop->dst, ses_ptr->buffer, caop->len))) {
			rc = -EFAULT;
			goto out_unlock;
		}

	}

	if (caop->tag_len == 0)
		caop->tag_len = ses_ptr->hdata.digestsize;

	kcaop->ivlen = caop->iv ? ses_ptr->cdata.ivsize : 0;

	if (caop->flags & COP_FLAG_AEAD_TLS_TYPE)
		kcaop->dst_len = caop->len + ses_ptr->cdata.blocksize /* pad */ + caop->tag_len;
	else
		kcaop->dst_len = caop->len;

	kcaop->task = current;
	kcaop->mm = current->mm;

	if (caop->iv) {
		rc = copy_from_user(kcaop->iv, caop->iv, kcaop->ivlen);
		if (unlikely(rc)) {
			dprintk(1, KERN_ERR,
				"error copying IV (%d bytes), copy_from_user returned %d for address %lx\n",
				kcaop->ivlen, rc, (unsigned long)caop->iv);
			rc = -EFAULT;
			goto out_unlock;
		}
	}
	
	rc = 0;

out_unlock:
	crypto_put_session(ses_ptr);
	return rc;

}

static int fill_caop_from_kcaop(struct kernel_crypt_auth_op *kcaop, struct fcrypt *fcr)
{
	int ret;

	kcaop->caop.len = kcaop->dst_len;

	if (kcaop->ivlen && kcaop->caop.flags & COP_FLAG_WRITE_IV) {
		ret = copy_to_user(kcaop->caop.iv,
				kcaop->iv, kcaop->ivlen);
		if (unlikely(ret)) {
			dprintk(1, KERN_ERR, "Error in copying to userspace\n");
			return -EFAULT;
		}
	}
	return 0;
}


int kcaop_from_user(struct kernel_crypt_auth_op *kcaop,
			struct fcrypt *fcr, void __user *arg)
{
	if (unlikely(copy_from_user(&kcaop->caop, arg, sizeof(kcaop->caop)))) {
		dprintk(1, KERN_ERR, "Error in copying from userspace\n");
		return -EFAULT;
	}

	return fill_kcaop_from_caop(kcaop, fcr);
}

int kcaop_to_user(struct kernel_crypt_auth_op *kcaop,
		struct fcrypt *fcr, void __user *arg)
{
	int ret;

	ret = fill_caop_from_kcaop(kcaop, fcr);
	if (unlikely(ret)) {
		dprintk(1, KERN_ERR, "fill_caop_from_kcaop\n");
		return ret;
	}

	if (unlikely(copy_to_user(arg, &kcaop->caop, sizeof(kcaop->caop)))) {
		dprintk(1, KERN_ERR, "Error in copying to userspace\n");
		return -EFAULT;
	}
	return 0;
}

static void copy_hash( struct scatterlist *dst_sg, int len, void* hash, int hash_len)
{
	scatterwalk_map_and_copy(hash, dst_sg, len, hash_len, 1);
}

static void read_hash( struct scatterlist *dst_sg, int len, void* hash, int hash_len)
{
	scatterwalk_map_and_copy(hash, dst_sg, len-hash_len, hash_len, 0);
}

static int pad_record( struct scatterlist *dst_sg, int len, int block_size)
{
	uint8_t pad[block_size];
	int pad_size = block_size - (len % block_size);

	memset(pad, pad_size-1, pad_size);

	scatterwalk_map_and_copy(pad, dst_sg, len, pad_size, 1);

	return pad_size;
}

static int verify_record_pad( struct scatterlist *dst_sg, int len, int block_size)
{
	uint8_t pad[256]; /* the maximum allowed */
	uint8_t pad_size;
	int i;

	scatterwalk_map_and_copy(&pad_size, dst_sg, len-1, 1, 0);
	pad_size++;

	if (pad_size > len)
		return -ECANCELED;

	scatterwalk_map_and_copy(pad, dst_sg, len-pad_size, pad_size, 0);

	for (i=0;i<pad_size;i++)
		if (pad[i] != pad_size)
			return -ECANCELED;

	return 0;
}

static int
tls_auth_n_crypt(struct csession *ses_ptr, struct kernel_crypt_auth_op *kcaop,
		struct scatterlist *auth_sg, uint32_t auth_len,
		struct scatterlist *dst_sg, uint32_t len)
{
	int ret, fail = 0;
	struct crypt_auth_op *caop = &kcaop->caop;
	uint8_t vhash[AALG_MAX_RESULT_LEN];
	uint8_t hash_output[AALG_MAX_RESULT_LEN];

	/* TLS authenticates the plaintext except for the padding.
	 */
	if (caop->op == COP_ENCRYPT) {
		if (ses_ptr->hdata.init != 0) {
			if (auth_len > 0) {
				ret = cryptodev_hash_update(&ses_ptr->hdata,
								auth_sg, auth_len);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
					goto out_err;
				}
			}

			if (len > 0) {
				ret = cryptodev_hash_update(&ses_ptr->hdata,
								dst_sg, len);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
					goto out_err;
				}
			}

			ret = cryptodev_hash_final(&ses_ptr->hdata, hash_output);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_hash_final: %d\n", ret);
				goto out_err;
			}

			copy_hash( dst_sg, len, hash_output, caop->tag_len);
			len += caop->tag_len;
		}

		if (ses_ptr->cdata.init != 0) {
			if (ses_ptr->cdata.blocksize > 1) {
				ret = pad_record(dst_sg, len, ses_ptr->cdata.blocksize);
				len += ret;
			}

			ret = cryptodev_cipher_encrypt(&ses_ptr->cdata,
							dst_sg, dst_sg, len);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_cipher_encrypt: %d\n", ret);
				goto out_err;
			}
		}
	} else {
		if (ses_ptr->cdata.init != 0) {
			ret = cryptodev_cipher_decrypt(&ses_ptr->cdata,
							dst_sg, dst_sg, len);

			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_cipher_decrypt: %d\n", ret);
				goto out_err;
			}

			if (ses_ptr->cdata.blocksize > 1) {
				ret = verify_record_pad(dst_sg, len, ses_ptr->cdata.blocksize);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "verify_record_pad: %d\n", ret);
					fail = 1;
				} else
					len -= ret;
			}
		}

		if (ses_ptr->hdata.init != 0) {
			if (unlikely(caop->tag_len > sizeof(vhash) || caop->tag_len > len)) {
				dprintk(1, KERN_ERR, "Illegal tag len size\n");
				ret = -EINVAL;
				goto out_err;
			}

			read_hash( dst_sg, len, vhash, caop->tag_len);
			len -= caop->tag_len;

			if (auth_len > 0) {
				ret = cryptodev_hash_update(&ses_ptr->hdata,
								auth_sg, auth_len);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
					goto out_err;
				}
			}

			if (len > 0) {
				ret = cryptodev_hash_update(&ses_ptr->hdata,
									dst_sg, len);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
					goto out_err;
				}
			}

			ret = cryptodev_hash_final(&ses_ptr->hdata, hash_output);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_hash_final: %d\n", ret);
				goto out_err;
			}

			if (memcmp(vhash, hash_output, caop->tag_len) != 0 || fail != 0) {
				dprintk(1, KERN_ERR, "MAC verification failed\n");
				ret = -ECANCELED;
				goto out_err;
			}
		}
	}
	kcaop->dst_len = len;
	return 0;
out_err:
	return ret;
}

static int
srtp_auth_n_crypt(struct csession *ses_ptr, struct kernel_crypt_auth_op *kcaop,
		  struct scatterlist *auth_sg, uint32_t auth_len,
		  struct scatterlist *dst_sg, uint32_t len)
{
	int ret, fail = 0;
	struct crypt_auth_op *caop = &kcaop->caop;
	uint8_t vhash[AALG_MAX_RESULT_LEN];
	uint8_t hash_output[AALG_MAX_RESULT_LEN];

	/* SRTP authenticates the encrypted data.
	 */
	if (caop->op == COP_ENCRYPT) {
		if (ses_ptr->cdata.init != 0) {
			if (ses_ptr->cdata.stream == 0) {
				dprintk(0, KERN_ERR, "Only stream modes are allowed in SRTP mode\n");
				ret = -EINVAL;
				goto out_err;
			}

			ret = cryptodev_cipher_encrypt(&ses_ptr->cdata,
							dst_sg, dst_sg, len);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_cipher_encrypt: %d\n", ret);
				goto out_err;
			}
		}

		if (ses_ptr->hdata.init != 0) {
			if (auth_len > 0) {
				ret = cryptodev_hash_update(&ses_ptr->hdata,
								auth_sg, auth_len);
				if (unlikely(ret)) {
					dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
					goto out_err;
				}
			}

			ret = cryptodev_hash_final(&ses_ptr->hdata, hash_output);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_hash_final: %d\n", ret);
				goto out_err;
			}

			if (unlikely(copy_to_user(caop->tag, hash_output, caop->tag_len))) {
				ret = -EFAULT;
				goto out_err;
			}
		}

	} else {
		if (ses_ptr->hdata.init != 0) {
			if (unlikely(caop->tag_len > sizeof(vhash) || caop->tag_len > len)) {
				dprintk(1, KERN_ERR, "Illegal tag len size\n");
				ret = -EINVAL;
				goto out_err;
			}

			if (unlikely(copy_from_user(vhash, caop->tag, caop->tag_len))) {
				ret = -EFAULT;
				goto out_err;
			}

			ret = cryptodev_hash_update(&ses_ptr->hdata,
							auth_sg, auth_len);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_hash_update: %d\n", ret);
				goto out_err;
			}

			ret = cryptodev_hash_final(&ses_ptr->hdata, hash_output);
			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_hash_final: %d\n", ret);
				goto out_err;
			}

			if (memcmp(vhash, hash_output, caop->tag_len) != 0 || fail != 0) {
				dprintk(1, KERN_ERR, "MAC verification failed\n");
				ret = -ECANCELED;
				goto out_err;
			}
		}

		if (ses_ptr->cdata.init != 0) {
			if (ses_ptr->cdata.stream == 0) {
				dprintk(0, KERN_ERR, "Only stream modes are allowed in SRTP mode\n");
				ret = -EINVAL;
				goto out_err;
			}

			ret = cryptodev_cipher_decrypt(&ses_ptr->cdata,
							dst_sg, dst_sg, len);

			if (unlikely(ret)) {
				dprintk(0, KERN_ERR, "cryptodev_cipher_decrypt: %d\n", ret);
				goto out_err;
			}
		}

	}
	kcaop->dst_len = len;
	return 0;
out_err:
	return ret;
}

/* This is the main crypto function - zero-copy edition */
static int
__crypto_auth_run_zc(struct csession *ses_ptr, struct kernel_crypt_auth_op *kcaop)
{
	struct scatterlist *dst_sg, *auth_sg;
	struct crypt_auth_op *caop = &kcaop->caop;
	int ret = 0, pagecount;

	if (caop->flags & COP_FLAG_AEAD_TLS_TYPE) {
		ret = get_userbuf_aead(ses_ptr, kcaop, &auth_sg, &dst_sg, &pagecount);
		if (unlikely(ret)) {
			dprintk(1, KERN_ERR, "Error getting user pages.\n");
			return ret;
		}

		ret = tls_auth_n_crypt(ses_ptr, kcaop, auth_sg, caop->auth_len, 
			   dst_sg, caop->len);
	} else if (caop->flags & COP_FLAG_AEAD_SRTP_TYPE) {
		ret = get_userbuf_srtp(ses_ptr, kcaop, &auth_sg, &dst_sg, &pagecount);
		if (unlikely(ret)) {
			dprintk(1, KERN_ERR, "Error getting user pages.\n");
			return ret;
		}

		ret = srtp_auth_n_crypt(ses_ptr, kcaop, auth_sg, caop->auth_len, 
			   dst_sg, caop->len);
	} else {
		dprintk(1, KERN_ERR, "Unsupported flag for authenc\n");
		return -EINVAL;
	}

	release_user_pages(ses_ptr->pages, pagecount);
	return ret;
}


int crypto_auth_run(struct fcrypt *fcr, struct kernel_crypt_auth_op *kcaop)
{
	struct csession *ses_ptr;
	struct crypt_auth_op *caop = &kcaop->caop;
	int ret;

	if (unlikely(caop->op != COP_ENCRYPT && caop->op != COP_DECRYPT)) {
		dprintk(1, KERN_DEBUG, "invalid operation op=%u\n", caop->op);
		return -EINVAL;
	}

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, caop->ses);
	if (unlikely(!ses_ptr)) {
		dprintk(1, KERN_ERR, "invalid session ID=0x%08X\n", caop->ses);
		return -EINVAL;
	}

	if (unlikely(ses_ptr->cdata.init == 0)) {
		dprintk(1, KERN_ERR, "cipher context not initialized\n");
		return -EINVAL;
	}

	if (ses_ptr->hdata.init != 0) {
		ret = cryptodev_hash_reset(&ses_ptr->hdata);
		if (unlikely(ret)) {
			dprintk(1, KERN_ERR,
				"error in cryptodev_hash_reset()\n");
			goto out_unlock;
		}
	}

	cryptodev_cipher_set_iv(&ses_ptr->cdata, kcaop->iv,
				min(ses_ptr->cdata.ivsize, kcaop->ivlen));

	if (likely(caop->len || caop->auth_len)) {
		ret = __crypto_auth_run_zc(ses_ptr, kcaop);
		if (unlikely(ret))
			goto out_unlock;
	}

	cryptodev_cipher_get_iv(&ses_ptr->cdata, kcaop->iv,
				min(ses_ptr->cdata.ivsize, kcaop->ivlen));

out_unlock:
	mutex_unlock(&ses_ptr->sem);
	return ret;
}
