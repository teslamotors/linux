/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2004 Michal Ludvig <mludvig@logix.net.nz>, SuSE Labs
 * Copyright (c) 2009,2010 Nikos Mavrogiannopoulos <nmav@gnutls.org>
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
 * Device /dev/crypto provides an interface for
 * accessing kernel CryptoAPI algorithms (ciphers,
 * hashes) from userspace programs.
 *
 * /dev/crypto interface was originally introduced in
 * OpenBSD and this module attempts to keep the API.
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

/* This file contains the traditional operations of encryption
 * and hashing of /dev/crypto.
 */

/* make cop->src and cop->dst available in scatterlists */
static int get_userbuf(struct csession *ses, struct kernel_crypt_op *kcop,
                       struct scatterlist **src_sg, struct scatterlist **dst_sg,
                       int *tot_pages)
{
	int src_pagecount, dst_pagecount = 0, pagecount, write_src = 1;
	struct crypt_op *cop = &kcop->cop;
	int rc;

	if (cop->src == NULL)
		return -EINVAL;

	if (ses->alignmask && !IS_ALIGNED((unsigned long)cop->src, ses->alignmask)) {
		dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)cop->src, ses->alignmask + 1);
	}

	src_pagecount = PAGECOUNT(cop->src, cop->len);
	if (!ses->cdata.init) {		/* hashing only */
		write_src = 0;
	} else if (cop->src != cop->dst) {	/* non-in-situ transformation */
		if (cop->dst == NULL)
			return -EINVAL;

		dst_pagecount = PAGECOUNT(cop->dst, cop->len);
		write_src = 0;

		if (ses->alignmask && !IS_ALIGNED((unsigned long)cop->dst, ses->alignmask)) {
			dprintk(2, KERN_WARNING, "%s: careful - destination address %lx is not %d byte aligned\n",
					__func__, (unsigned long)cop->dst, ses->alignmask + 1);
		}

	}
	(*tot_pages) = pagecount = src_pagecount + dst_pagecount;

	if (pagecount > ses->array_size) {
		rc = adjust_sg_array(ses, pagecount);
		if (rc)
			return rc;
	}

	rc = __get_userbuf(cop->src, cop->len, write_src, src_pagecount,
	                   ses->pages, ses->sg, kcop->task, kcop->mm);
	if (unlikely(rc)) {
		dprintk(1, KERN_ERR,
			"failed to get user pages for data input\n");
		return -EINVAL;
	}
	(*src_sg) = (*dst_sg) = ses->sg;

	if (!dst_pagecount)
		return 0;

	(*dst_sg) = ses->sg + src_pagecount;

	rc = __get_userbuf(cop->dst, cop->len, 1, dst_pagecount,
	                   ses->pages + src_pagecount, *dst_sg,
			   kcop->task, kcop->mm);
	if (unlikely(rc)) {
		dprintk(1, KERN_ERR,
		        "failed to get user pages for data output\n");
		release_user_pages(ses->pages, src_pagecount);
		return -EINVAL;
	}
	return 0;
}

static int
hash_n_crypt(struct csession *ses_ptr, struct crypt_op *cop,
		struct scatterlist *src_sg, struct scatterlist *dst_sg,
		uint32_t len)
{
	int ret;

	/* Always hash before encryption and after decryption. Maybe
	 * we should introduce a flag to switch... TBD later on.
	 */
	if (cop->op == COP_ENCRYPT) {
		if (ses_ptr->hdata.init != 0) {
			ret = cryptodev_hash_update(&ses_ptr->hdata,
							src_sg, len);
			if (unlikely(ret))
				goto out_err;
		}
		if (ses_ptr->cdata.init != 0) {
			ret = cryptodev_cipher_encrypt(&ses_ptr->cdata,
							src_sg, dst_sg, len);

			if (unlikely(ret))
				goto out_err;
		}
	} else {
		if (ses_ptr->cdata.init != 0) {
			ret = cryptodev_cipher_decrypt(&ses_ptr->cdata,
							src_sg, dst_sg, len);

			if (unlikely(ret))
				goto out_err;
		}

		if (ses_ptr->hdata.init != 0) {
			ret = cryptodev_hash_update(&ses_ptr->hdata,
								dst_sg, len);
			if (unlikely(ret))
				goto out_err;
		}
	}
	return 0;
out_err:
	dprintk(0, KERN_ERR, "CryptoAPI failure: %d\n", ret);
	return ret;
}

/* This is the main crypto function - feed it with plaintext
   and get a ciphertext (or vice versa :-) */
static int
__crypto_run_std(struct csession *ses_ptr, struct crypt_op *cop)
{
	char *data;
	char __user *src, *dst;
	struct scatterlist sg;
	size_t nbytes, bufsize;
	int ret = 0;

	nbytes = cop->len;
	data = (char *)__get_free_page(GFP_KERNEL);

	if (unlikely(!data))
		return -ENOMEM;

	bufsize = PAGE_SIZE < nbytes ? PAGE_SIZE : nbytes;

	src = cop->src;
	dst = cop->dst;

	while (nbytes > 0) {
		size_t current_len = nbytes > bufsize ? bufsize : nbytes;

		if (unlikely(copy_from_user(data, src, current_len))) {
			ret = -EFAULT;
			break;
		}

		sg_init_one(&sg, data, current_len);

		ret = hash_n_crypt(ses_ptr, cop, &sg, &sg, current_len);

		if (unlikely(ret))
			break;

		if (ses_ptr->cdata.init != 0) {
			if (unlikely(copy_to_user(dst, data, current_len))) {
				ret = -EFAULT;
				break;
			}
		}

		dst += current_len;
		nbytes -= current_len;
		src += current_len;
	}

	free_page((unsigned long)data);
	return ret;
}

/* This is the main crypto function - zero-copy edition */
static int
__crypto_run_zc(struct csession *ses_ptr, struct kernel_crypt_op *kcop)
{
	struct scatterlist *src_sg, *dst_sg;
	struct crypt_op *cop = &kcop->cop;
	int ret = 0, pagecount;

	ret = get_userbuf(ses_ptr, kcop, &src_sg, &dst_sg, &pagecount);
	if (unlikely(ret)) {
		dprintk(1, KERN_ERR, "Error getting user pages. "
					"Falling back to non zero copy.\n");
		return __crypto_run_std(ses_ptr, cop);
	}

	ret = hash_n_crypt(ses_ptr, cop, src_sg, dst_sg, cop->len);

	release_user_pages(ses_ptr->pages, pagecount);
	return ret;
}

int crypto_run(struct fcrypt *fcr, struct kernel_crypt_op *kcop)
{
	struct csession *ses_ptr;
	struct crypt_op *cop = &kcop->cop;
	int ret;

	if (unlikely(cop->op != COP_ENCRYPT && cop->op != COP_DECRYPT)) {
		dprintk(1, KERN_DEBUG, "invalid operation op=%u\n", cop->op);
		return -EINVAL;
	}

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dprintk(1, KERN_ERR, "invalid session ID=0x%08X\n", cop->ses);
		return -EINVAL;
	}

	if (ses_ptr->hdata.init != 0 && !(cop->flags & (COP_FLAG_UPDATE | COP_FLAG_FINAL))) {
		ret = cryptodev_hash_reset(&ses_ptr->hdata);
		if (unlikely(ret)) {
			dprintk(1, KERN_ERR,
				"error in cryptodev_hash_reset()\n");
			goto out_unlock;
		}
	}

	if (ses_ptr->cdata.init != 0) {
		int blocksize = ses_ptr->cdata.blocksize;

		if (unlikely(cop->len % blocksize)) {
			dprintk(1, KERN_ERR,
				"data size (%u) isn't a multiple "
				"of block size (%u)\n",
				cop->len, blocksize);
			ret = -EINVAL;
			goto out_unlock;
		}

		cryptodev_cipher_set_iv(&ses_ptr->cdata, kcop->iv,
				min(ses_ptr->cdata.ivsize, kcop->ivlen));
	}

	if (likely(cop->len)) {
		if (cop->flags & COP_FLAG_NO_ZC)
			ret = __crypto_run_std(ses_ptr, &kcop->cop);
		else
			ret = __crypto_run_zc(ses_ptr, kcop);
		if (unlikely(ret))
			goto out_unlock;
	}

	if (ses_ptr->cdata.init != 0) {
		cryptodev_cipher_get_iv(&ses_ptr->cdata, kcop->iv,
				min(ses_ptr->cdata.ivsize, kcop->ivlen));
	}

	if (ses_ptr->hdata.init != 0 &&
		((cop->flags & COP_FLAG_FINAL) ||
		   (!(cop->flags & COP_FLAG_UPDATE) || cop->len == 0))) {

		ret = cryptodev_hash_final(&ses_ptr->hdata, kcop->hash_output);
		if (unlikely(ret)) {
			dprintk(0, KERN_ERR, "CryptoAPI failure: %d\n", ret);
			goto out_unlock;
		}
		kcop->digestsize = ses_ptr->hdata.digestsize;
	}

out_unlock:
	crypto_put_session(ses_ptr);
	return ret;
}
