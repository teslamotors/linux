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
		unsigned int pgcount, struct page **pg, struct scatterlist *sg,
		struct task_struct *task, struct mm_struct *mm)
{
	int ret, pglen, i = 0;
	struct scatterlist *sgp;

	if (unlikely(!pgcount || !len || !addr)) {
		sg_mark_end(sg);
	}
	else {
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
	}
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
	dprintk(0, KERN_DEBUG, "reallocating from %d to %d pages\n",
			ses->array_size, array_size);
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

void release_user_pages(struct csession *ses)
{
	unsigned int i;

	for (i=0;i<ses->used_pages;i++) {
		if (!PageReserved(ses->pages[i]))
			SetPageDirty(ses->pages[i]);

		if (ses->readonly_pages == 0)
			flush_dcache_page(ses->pages[i]);
		else
			ses->readonly_pages--;

		page_cache_release(ses->pages[i]);
	}
	ses->used_pages = 0;
}

/* make src and dst available in scatterlists.
 * dst might be the same as src.
 */
int get_userbuf(struct csession *ses,
                void* __user src, unsigned int src_len,
                void* __user dst, unsigned int dst_len,
                struct task_struct *task, struct mm_struct *mm,
                struct scatterlist **src_sg,
                struct scatterlist **dst_sg)
{
	int src_pagecount, dst_pagecount;
	int rc;

	/* Empty input is a valid option to many algorithms & is tested by NIST/FIPS */
	/* Make sure NULL input has 0 length */
	if (!src && src_len) { src_len = 0; }

	/* I don't know that null output is ever useful, but we can handle it gracefully */
	/* Make sure NULL output has 0 length */
	if (!dst && dst_len) { dst_len = 0; }

	if (ses->alignmask && !IS_ALIGNED((unsigned long)src, ses->alignmask)) {
		dprintk(2, KERN_WARNING, "careful - source address %lx is not %d byte aligned\n",
				(unsigned long)src, ses->alignmask + 1);
	}

	if (ses->alignmask && !IS_ALIGNED((unsigned long)dst, ses->alignmask)) {
		dprintk(2, KERN_WARNING, "careful - destination address %lx is not %d byte aligned\n",
				(unsigned long)dst, ses->alignmask + 1);
	}

	src_pagecount = PAGECOUNT(src, src_len);
	dst_pagecount = PAGECOUNT(dst, dst_len);

	ses->used_pages = (src == dst) ? max(src_pagecount, dst_pagecount)
	                               : src_pagecount + dst_pagecount;

	ses->readonly_pages = (src == dst) ? 0 : src_pagecount;

	if (ses->used_pages > ses->array_size) {
		rc = adjust_sg_array(ses, ses->used_pages);
		if (rc)
			return rc;
	}

	if (src == dst) {
		rc = __get_userbuf(src, src_len, 1, ses->used_pages,
			               ses->pages, ses->sg, task, mm);
		if (unlikely(rc)) {
			dprintk(1, KERN_ERR,
				"failed to get user pages for data IO\n");
			return rc;
		}
		(*src_sg) = (*dst_sg) = ses->sg;
	}
	else {
		const unsigned int readonly_pages = ses->readonly_pages;
		const unsigned int writable_pages = ses->used_pages - readonly_pages;

		if(likely(src)) {
			rc = __get_userbuf(src, src_len, 0, readonly_pages,
					           ses->pages, ses->sg, task, mm);
			if (unlikely(rc)) {
				dprintk(1, KERN_ERR,
					"failed to get user pages for data input\n");
				return rc;
			}
			*src_sg = ses->sg;
		}
		else {
			*src_sg = NULL; // no input
		}

		if(likely(dst)) {
			struct page **dst_pages = ses->pages + readonly_pages;
			*dst_sg = ses->sg + readonly_pages;

			rc = __get_userbuf(dst, dst_len, 1, writable_pages,
					           dst_pages, *dst_sg, task, mm);
			if (unlikely(rc)) {
				dprintk(1, KERN_ERR,
						"failed to get user pages for data output\n");
				release_user_pages(ses);  /* FIXME: use __release_userbuf(src, ...) */
				return rc;
			}
		}
		else {
			*dst_sg = NULL; // ignore output
		}
	}
	return 0;
}

