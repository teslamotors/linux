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
	pg_inf->attr = page_to_phys(page);
#ifdef CONFIG_TEGRA_VIRTUALIZATION
	hyp_ipa_translate(&pg_inf->attr);
#endif
	return 0;
}

