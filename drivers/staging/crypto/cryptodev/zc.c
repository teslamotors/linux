/*
 * Driver for /dev/crypto device (aka CryptoDev)
 *
 * Copyright (c) 2009-2011 Nikos Mavrogiannopoulos <nmav@gnutls.org>
 * Copyright (c) 2010 Phil Sutter
 * Copyright (c) 2011, 2012 OpenSSL Software Foundation, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301, USA.
 */

#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/ioctl.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/uaccess.h>
#include <crypto/scatterwalk.h>
#include <linux/scatterlist.h>
#include "cryptodev_int.h"
#include "zc.h"
#include "version.h"

/* Helper functions to assist zero copy. 
 * This needs to be redesigned and moved out of the session. --nmav
 */

/* offset of buf in it's first page */
#define PAGEOFFSET(buf) ((unsigned long)buf & ~PAGE_MASK)

/* fetch the pages addr resides in into pg and initialise sg with them */
int __get_userbuf(uint8_t __user *addr, uint32_t len, int write,
		int pgcount, struct page **pg, struct scatterlist *sg,
		struct task_struct *task, struct mm_struct *mm)
{
	int ret, pglen, i = 0;
	struct scatterlist *sgp;

	down_write(&mm->mmap_sem);
	ret = get_user_pages(task, mm,
			(unsigned long)addr, pgcount, write, 0, pg, NULL);
	up_write(&mm->mmap_sem);
	if (ret != pgcount)
		return -EINVAL;

	sg_init_table(sg, pgcount);

	pglen = min((ptrdiff_t)(PAGE_SIZE - PAGEOFFSET(addr)), (ptrdiff_t)len);
	sg_set_page(sg, pg[i++], pglen, PAGEOFFSET(addr));

	len -= pglen;
	for (sgp = sg_next(sg); len; sgp = sg_next(sgp)) {
		pglen = min((uint32_t)PAGE_SIZE, len);
		sg_set_page(sgp, pg[i++], pglen, 0);
		len -= pglen;
	}
	sg_mark_end(sg_last(sg, pgcount));
	return 0;
}

int adjust_sg_array(struct csession * ses, int pagecount)
{
struct scatterlist *sg;
struct page **pages;
int array_size;

	for (array_size = ses->array_size; array_size < pagecount;
	     array_size *= 2)
		;

	dprintk(2, KERN_DEBUG, "%s: reallocating to %d elements\n",
			__func__, array_size);
	pages = krealloc(ses->pages, array_size * sizeof(struct page *),
			 GFP_KERNEL);
	if (unlikely(!pages))
		return -ENOMEM;
	ses->pages = pages;
	sg = krealloc(ses->sg, array_size * sizeof(struct scatterlist),
		      GFP_KERNEL);
	if (unlikely(!sg))
		return -ENOMEM;
	ses->sg = sg;
	ses->array_size = array_size;

	return 0;
}

void release_user_pages(struct page **pg, int pagecount)
{
	while (pagecount--) {
		if (!PageReserved(pg[pagecount]))
			SetPageDirty(pg[pagecount]);
		page_cache_release(pg[pagecount]);
	}
}

/* make src and dst available in scatterlists.
 * dst might be the same as src.
 */
int get_userbuf(struct csession *ses, void* __user src, int src_len,
                void* __user dst, int dst_len,
		struct task_struct *task, struct mm_struct *mm,
                struct scatterlist **src_sg, 
                struct scatterlist **dst_sg,
                int *tot_pages)
{
	int src_pagecount, dst_pagecount = 0, pagecount, write_src = 1;
	int rc;

	if (src == NULL)
		return -EINVAL;

	if (ses->alignmask && !IS_ALIGNED((unsigned long)src, ses->alignmask)) {
		dprintk(2, KERN_WARNING, "%s: careful - source address %lx is not %d byte aligned\n",
				__func__, (unsigned long)src, ses->alignmask + 1);
	}

	if (src == dst) {
                /* dst == src */
	        src_len = max(src_len, dst_len);
	        dst_len = src_len;
        }

	src_pagecount = PAGECOUNT(src, src_len);
	if (!ses->cdata.init) {		/* hashing only */
		write_src = 0;
	} else if (src != dst) {	/* non-in-situ transformation */
		if (dst == NULL)
			return -EINVAL;

		dst_pagecount = PAGECOUNT(dst, dst_len);
		write_src = 0;

		if (ses->alignmask && !IS_ALIGNED((unsigned long)dst, ses->alignmask)) {
			dprintk(2, KERN_WARNING, "%s: careful - destination address %lx is not %d byte aligned\n",
					__func__, (unsigned long)dst, ses->alignmask + 1);
		}
        }
	(*tot_pages) = pagecount = src_pagecount + dst_pagecount;

	if (pagecount > ses->array_size) {
		rc = adjust_sg_array(ses, pagecount);
		if (rc)
			return rc;
	}

	rc = __get_userbuf(src, src_len, write_src, src_pagecount,
	                   ses->pages, ses->sg, task, mm);
	if (unlikely(rc)) {
		dprintk(1, KERN_ERR,
			"failed to get user pages for data input\n");
		return -EINVAL;
	}
	(*src_sg) = (*dst_sg) = ses->sg;

	if (!dst_pagecount)
		return 0;

	(*dst_sg) = ses->sg + src_pagecount;

	rc = __get_userbuf(dst, dst_len, 1, dst_pagecount,
	                   ses->pages + src_pagecount, *dst_sg,
			   task, mm);
	if (unlikely(rc)) {
		dprintk(1, KERN_ERR,
		        "failed to get user pages for data output\n");
		release_user_pages(ses->pages, src_pagecount);
		return -EINVAL;
	}
	return 0;
}
