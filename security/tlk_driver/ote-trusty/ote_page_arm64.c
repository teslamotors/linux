/*
 * Copyright (C) 2014 Google, Inc.
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

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/pagemap.h>

#include "ote_protocol.h"

int te_fill_page_info(struct te_oper_param_page_info *pg_inf,
		      unsigned long start, struct page *page,
		      struct vm_area_struct *vma)
{
	uint64_t mair;
	uint64_t pte;
	uint8_t memory_type;
	uint64_t attr_index;

	if (!page || !vma)
		return -1;

	/* Only support normal memory types. */
	if ((vma->vm_page_prot & PTE_ATTRINDX_MASK) !=
	    PTE_ATTRINDX(MT_NORMAL)) {
		pr_err("%s: unsupported memory type: %llx\n", __func__,
		       vma->vm_page_prot & PTE_ATTRINDX_MASK);
		return -1;
	}

	/*
	 * Recreate the pte of the page - we can't access it
	 * safely here race free.
	 */
	pte = page_to_phys(page);
#ifdef CONFIG_TEGRA_VIRTUALIZATION
	hyp_ipa_translate(&pte);
#endif
	pte |= pgprot_val(vma->vm_page_prot);
	if (vma->vm_flags & VM_WRITE)
		pte &= ~PTE_RDONLY;

	/* Pull the mt index out of the pte */
	attr_index = (pte & PTE_ATTRINDX_MASK) >> 2;

	/* Pull the memory attributes out of the mair register. */
	asm ("mrs %0, mair_el1\n" : "=&r" (mair));
	memory_type = (mair >> (attr_index * 8)) & 0xff;

	pg_inf->attr = (pte & 0x0000FFFFFFFFFFFFull) |
		       ((uint64_t)memory_type << 48);

	return 0;
}

