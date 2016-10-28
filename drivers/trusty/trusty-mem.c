/*
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/smcall.h>
#include <linux/version.h>

/* Normal memory */
#define NS_MAIR_NORMAL_CACHED_WB_RWA       0xFF /* inner and outer write back read/write allocate */
#define NS_MAIR_NORMAL_CACHED_WT_RA        0xAA /* inner and outer write through read allocate */
#define NS_MAIR_NORMAL_CACHED_WB_RA        0xEE /* inner and outer wriet back, read allocate */
#define NS_MAIR_NORMAL_UNCACHED            0x44 /* uncached */

static int get_mem_attr(struct page *page, pgprot_t pgprot)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	/* The porting to CHT kernel (3.14.55) is in the #else clause.
	** For BXT kernel (4.1.0), the function get_page_memtype() is static.
	**
	** The orignal google code (for arm) getst the cache states and page
	** flags from input parameter "pgprot", which is not prefered in x86.
	** In x86, both cache states and page flags should be got from input
	** parameter "page". But, since current caller of trusty_call32_mem_buf()
	** always allocate memory in kernel heap, it is also ok to use hardcode
	** here.
	**
	** The memory allocated in kernel heap should be CACHED. The reason to
	** return UNCACHED here is to pass the check in LK sm_decode_ns_memory_attr()
	** with SMP, which only allow UNCACHED.
	*/
	return NS_MAIR_NORMAL_UNCACHED;
#else
	unsigned long type;
	int ret_mem_attr = 0;

	type = get_page_memtype(page);
	/*
	* -1 from get_page_memtype() implies RAM page is in its
	* default state and not reserved, and hence of type WB
	*/
	if (type == -1) {
		type = _PAGE_CACHE_MODE_WB;
	}
	switch (type) {
	case _PAGE_CACHE_MODE_UC_MINUS:
		/* uncacheable */
		ret_mem_attr = NS_MAIR_NORMAL_UNCACHED;
		break;
	case _PAGE_CACHE_MODE_WB:
		/*  writeback */
		ret_mem_attr = NS_MAIR_NORMAL_CACHED_WB_RWA;
		break;
	case _PAGE_CACHE_MODE_WC:
		/* write combined */
		ret_mem_attr = NS_MAIR_NORMAL_UNCACHED;
		break;

	default:
		printk(KERN_ERR "%s(): invalid type: 0x%x\n", __func__, type);
		ret_mem_attr = -EINVAL;
	}
	return ret_mem_attr;
#endif
}

int trusty_encode_page_info(struct ns_mem_page_info *inf,
			    struct page *page, pgprot_t pgprot)
{
	int mem_attr;
	uint64_t pte;

	if (!inf || !page)
		return -EINVAL;

	/* get physical address */
	pte = (uint64_t) page_to_phys(page);

	/* get memory attributes */
	mem_attr = get_mem_attr(page, pgprot);
	if (mem_attr < 0)
		return mem_attr;
	if (pgprot_val(pgprot) & _PAGE_USER)
		pte |= (1 << 6);
	if (!(pgprot_val(pgprot) & _PAGE_RW))
		pte |= (1 << 7);

	inf->attr = (pte & 0x0000FFFFFFFFFFFFull) | ((uint64_t)mem_attr << 48);
	return 0;
}

int trusty_call32_mem_buf(struct device *dev, u32 smcnr,
			  struct page *page,  u32 size,
			  pgprot_t pgprot)
{
	int ret;
	struct ns_mem_page_info pg_inf;

	if (!dev || !page)
		return -EINVAL;

	ret = trusty_encode_page_info(&pg_inf, page, pgprot);
	if (ret)
		return ret;

	if (SMC_IS_FASTCALL(smcnr)) {
		return trusty_fast_call32(dev, smcnr,
					  (u32)pg_inf.attr,
					  (u32)(pg_inf.attr >> 32), size);
	} else {
		return trusty_std_call32(dev, smcnr,
					 (u32)pg_inf.attr,
					 (u32)(pg_inf.attr >> 32), size);
	}
}
EXPORT_SYMBOL(trusty_call32_mem_buf);
MODULE_LICENSE("GPL");
