/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Dongwon Kim <dongwon.kim@intel.com>
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/slab.h>
#include <xen/grant_table.h>
#include <asm/xen/page.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include "hyper_dmabuf_xen_drv.h"
#include "../hyper_dmabuf_drv.h"

#define REFS_PER_PAGE (PAGE_SIZE/sizeof(grant_ref_t))

/*
 * Creates 2 level page directory structure for referencing shared pages.
 * Top level page is a single page that contains up to 1024 refids that
 * point to 2nd level pages.
 *
 * Each 2nd level page contains up to 1024 refids that point to shared
 * data pages.
 *
 * There will always be one top level page and number of 2nd level pages
 * depends on number of shared data pages.
 *
 *      3rd level page                2nd level pages            Data pages
 * +-------------------------+   ┌>+--------------------+ ┌>+------------+
 * |2nd level page 0 refid   |---┘ |Data page 0 refid   |-┘ |Data page 0 |
 * |2nd level page 1 refid   |---┐ |Data page 1 refid   |-┐ +------------+
 * |           ...           |   | |     ....           | |
 * |2nd level page 1022 refid|-┐ | |Data page 1023 refid| └>+------------+
 * |lock                     | | | +--------------------+   |Data page 1 |
 * +-------------------------+ | |                          +------------+
 *                             | └>+--------------------+
 *                             |   |Data page 1024 refid|
 *                             |   |Data page 1025 refid|
 *                             |   |       ...          |
 *                             |   |Data page 2047 refid|
 *                             |   +--------------------+
 *                             |
 *                             |        .....
 *                             └-->+-----------------------+
 *                                 |Data page 1047552 refid|
 *                                 |Data page 1047553 refid|
 *                                 |       ...             |
 *                                 |Data page 1048575 refid|
 *                                 +-----------------------+
 *
 * Using such 2 level structure it is possible to reference up to 4GB of
 * shared data using single refid pointing to top level page.
 *
 * Last 4 bytes of 3rd level page will be used as lock to synchronize
 * buffers usage between domains.
 *
 * Returns refid of top level page.
 */
int xen_be_share_pages(struct page **pages, int domid, int nents,
		       void **refs_info, unsigned int **lock)
{
	grant_ref_t lvl3_gref;
	grant_ref_t *lvl2_table;
	grant_ref_t *lvl3_table;

	/*
	 * Calculate number of pages needed for 2nd level addresing:
	 */
	int n_lvl2_grefs = (nents/REFS_PER_PAGE +
			   ((nents % REFS_PER_PAGE) ? 1 : 0));

	struct xen_shared_pages_info *sh_pages_info;
	int i;

	lvl3_table = (grant_ref_t *)__get_free_pages(GFP_KERNEL, 1);
	lvl2_table = (grant_ref_t *)__get_free_pages(GFP_KERNEL, n_lvl2_grefs);

	sh_pages_info = kmalloc(sizeof(*sh_pages_info), GFP_KERNEL);

	if (!sh_pages_info)
		return -ENOMEM;

	*refs_info = (void *)sh_pages_info;

	/* share data pages in readonly mode for security */
	for (i = 0; i < nents; i++) {
		lvl2_table[i] = gnttab_grant_foreign_access(domid,
					pfn_to_mfn(page_to_pfn(pages[i])),
					true /* read only */);
		if (lvl2_table[i] == -ENOSPC) {
			dev_err(hy_drv_priv->dev,
				"No more space left in grant table\n");

			/* Unshare all already shared pages for lvl2 */
			while (i--) {
				gnttab_end_foreign_access_ref(lvl2_table[i], 0);
				gnttab_free_grant_reference(lvl2_table[i]);
			}
			goto err_cleanup;
		}
	}

	/* Share 2nd level addressing pages in readonly mode*/
	for (i = 0; i < n_lvl2_grefs; i++) {
		lvl3_table[i] = gnttab_grant_foreign_access(domid,
					virt_to_mfn(
					(unsigned long)lvl2_table+i*PAGE_SIZE),
					true);

		if (lvl3_table[i] == -ENOSPC) {
			dev_err(hy_drv_priv->dev,
				"No more space left in grant table\n");

			/* Unshare all already shared pages for lvl3 */
			while (i--) {
				gnttab_end_foreign_access_ref(lvl3_table[i], 1);
				gnttab_free_grant_reference(lvl3_table[i]);
			}

			/* Unshare all pages for lvl2 */
			while (nents--) {
				gnttab_end_foreign_access_ref(
							lvl2_table[nents], 0);
				gnttab_free_grant_reference(lvl2_table[nents]);
			}

			goto err_cleanup;
		}
	}

	/*
	 * Share lvl3_table in read/write mode, so that lock
	 * can be modified also by importer
	 */
	lvl3_gref = gnttab_grant_foreign_access(domid,
			virt_to_mfn((unsigned long)lvl3_table),
			false);

	if (lvl3_gref == -ENOSPC) {
		dev_err(hy_drv_priv->dev,
			"No more space left in grant table\n");

		/* Unshare all pages for lvl3 */
		while (i--) {
			gnttab_end_foreign_access_ref(lvl3_table[i], 1);
			gnttab_free_grant_reference(lvl3_table[i]);
		}

		/* Unshare all pages for lvl2 */
		while (nents--) {
			gnttab_end_foreign_access_ref(lvl2_table[nents], 0);
			gnttab_free_grant_reference(lvl2_table[nents]);
		}

		goto err_cleanup;
	}

	/* Store lvl3_table page to be freed later */
	sh_pages_info->lvl3_table = lvl3_table;

	/* Store lvl2_table pages to be freed later */
	sh_pages_info->lvl2_table = lvl2_table;


	/* Store exported pages refid to be unshared later */
	sh_pages_info->lvl3_gref = lvl3_gref;

	/* Use last 4 bytes of lvl3_page as lock */
	if (lock) {
		*lock = (unsigned int *) ((unsigned long)lvl3_table + PAGE_SIZE - sizeof(unsigned int));
		*(*lock) = 0;
	}

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return lvl3_gref;

err_cleanup:
	free_pages((unsigned long)lvl2_table, n_lvl2_grefs);
	free_pages((unsigned long)lvl3_table, 1);

	return -ENOSPC;
}

int xen_be_unshare_pages(void **refs_info, int nents)
{
	struct xen_shared_pages_info *sh_pages_info;
	int n_lvl2_grefs = (nents/REFS_PER_PAGE +
			    ((nents % REFS_PER_PAGE) ? 1 : 0));
	int i;

	dev_dbg(hy_drv_priv->dev, "%s entry\n", __func__);
	sh_pages_info = (struct xen_shared_pages_info *)(*refs_info);

	if (sh_pages_info->lvl3_table == NULL ||
	    sh_pages_info->lvl2_table ==  NULL ||
	    sh_pages_info->lvl3_gref == -1) {
		dev_warn(hy_drv_priv->dev,
			 "gref table for hyper_dmabuf already cleaned up\n");
		return 0;
	}

	/* End foreign access for data pages, but do not free them */
	for (i = 0; i < nents; i++) {
		if (gnttab_query_foreign_access(sh_pages_info->lvl2_table[i]))
			dev_warn(hy_drv_priv->dev, "refid not shared !!\n");

		gnttab_end_foreign_access_ref(sh_pages_info->lvl2_table[i], 0);
		gnttab_free_grant_reference(sh_pages_info->lvl2_table[i]);
	}

	/* End foreign access for 2nd level addressing pages */
	for (i = 0; i < n_lvl2_grefs; i++) {
		if (gnttab_query_foreign_access(sh_pages_info->lvl3_table[i]))
			dev_warn(hy_drv_priv->dev, "refid not shared !!\n");

		if (!gnttab_end_foreign_access_ref(
					sh_pages_info->lvl3_table[i], 1))
			dev_warn(hy_drv_priv->dev, "refid still in use!!!\n");

		gnttab_free_grant_reference(sh_pages_info->lvl3_table[i]);
	}

	/* End foreign access for top level addressing page */
	if (gnttab_query_foreign_access(sh_pages_info->lvl3_gref))
		dev_warn(hy_drv_priv->dev, "gref not shared !!\n");

	gnttab_end_foreign_access_ref(sh_pages_info->lvl3_gref, 1);
	gnttab_free_grant_reference(sh_pages_info->lvl3_gref);

	/* freeing all pages used for 2 level addressing */
	free_pages((unsigned long)sh_pages_info->lvl2_table, n_lvl2_grefs);
	free_pages((unsigned long)sh_pages_info->lvl3_table, 1);

	sh_pages_info->lvl3_gref = -1;
	sh_pages_info->lvl2_table = NULL;
	sh_pages_info->lvl3_table = NULL;
	kfree(sh_pages_info);
	sh_pages_info = NULL;

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return 0;
}

/* Maps provided top level ref id and then return array of pages
 * containing data refs.
 */
struct page **xen_be_map_shared_pages(unsigned long lvl3_gref, int domid,
				      int nents, void **refs_info,
				      unsigned int **lock)
{
	struct page **lvl3_table_page;
	struct page **lvl2_table_pages;
	struct page **data_pages;
	struct xen_shared_pages_info *sh_pages_info;

	grant_ref_t *lvl3_table;
	grant_ref_t *lvl2_table;

	struct gnttab_map_grant_ref lvl3_map_ops;
	struct gnttab_unmap_grant_ref *lvl3_unmap_ops;

	struct gnttab_map_grant_ref lvl3_pte_map_ops;
	struct gnttab_unmap_grant_ref *lvl3_pte_unmap_ops = NULL;

	struct gnttab_map_grant_ref *lvl2_map_ops;
	struct gnttab_unmap_grant_ref *lvl2_unmap_ops;

	struct gnttab_map_grant_ref *data_map_ops;
	struct gnttab_unmap_grant_ref *data_unmap_ops;

	/* Needed for mapping lvl3 page in a way that will allow to write to it by importer */
	pte_t **lvl3_ptes = NULL;
	struct vm_struct *lvl3_vm_area = NULL;

	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		lvl3_ptes = kcalloc(1, sizeof(pte_t *), GFP_KERNEL);
		lvl3_vm_area = alloc_vm_area(PAGE_SIZE, lvl3_ptes);
	}

	/* # of grefs in the last page of lvl2 table */
	int nents_last = (nents - 1) % REFS_PER_PAGE + 1;
	int n_lvl2_grefs = (nents / REFS_PER_PAGE) +
			   ((nents_last > 0) ? 1 : 0) -
			   (nents_last == REFS_PER_PAGE);
	int i, j, k;

	dev_dbg(hy_drv_priv->dev, "%s entry\n", __func__);

	sh_pages_info = kmalloc(sizeof(*sh_pages_info), GFP_KERNEL);
	*refs_info = (void *) sh_pages_info;

	lvl3_table_page = kcalloc(sizeof(struct page*), 1, GFP_KERNEL);

	lvl2_table_pages = kcalloc(n_lvl2_grefs, sizeof(struct page *),
				   GFP_KERNEL);

	lvl3_unmap_ops = kcalloc(sizeof(*lvl3_unmap_ops), 1,
				 GFP_KERNEL);
	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		lvl3_pte_unmap_ops = kcalloc(sizeof(*lvl3_unmap_ops), 1,
					     GFP_KERNEL);
	}

	data_pages = kcalloc(nents, sizeof(struct page *), GFP_KERNEL);

	lvl2_map_ops = kcalloc(n_lvl2_grefs, sizeof(*lvl2_map_ops),
			       GFP_KERNEL);

	lvl2_unmap_ops = kcalloc(n_lvl2_grefs, sizeof(*lvl2_unmap_ops),
				 GFP_KERNEL);

	data_map_ops = kcalloc(nents, sizeof(*data_map_ops), GFP_KERNEL);
	data_unmap_ops = kcalloc(nents, sizeof(*data_unmap_ops), GFP_KERNEL);

	/* Map top level addressing page */
	if (gnttab_alloc_pages(1, lvl3_table_page)) {
		dev_err(hy_drv_priv->dev, "Cannot allocate pages\n");
		return NULL;
	}

	lvl3_table = (grant_ref_t *)pfn_to_kaddr(page_to_pfn(*lvl3_table_page));

	if (!xen_feature(XENFEAT_auto_translated_physmap)) {

		gnttab_set_map_op(&lvl3_pte_map_ops, arbitrary_virt_to_machine(lvl3_ptes[0]).maddr,
				  GNTMAP_host_map | GNTMAP_contains_pte,
				  (grant_ref_t)lvl3_gref, domid);

		gnttab_set_unmap_op(lvl3_pte_unmap_ops, arbitrary_virt_to_machine(lvl3_ptes[0]).maddr,
				    GNTMAP_host_map | GNTMAP_contains_pte, -1);

		gnttab_set_map_op(&lvl3_map_ops, (unsigned long)lvl3_table,
				  GNTMAP_host_map | GNTMAP_readonly,
				  (grant_ref_t)lvl3_gref, domid);

		gnttab_set_unmap_op(lvl3_unmap_ops, (unsigned long)lvl3_table,
				    GNTMAP_host_map | GNTMAP_readonly, -1);

		if (gnttab_map_refs(&lvl3_pte_map_ops, &lvl3_map_ops, lvl3_table_page, 1)) {
			dev_err(hy_drv_priv->dev,
				"HYPERVISOR map grant ref failed");
			return NULL;
		}
	} else {
		gnttab_set_map_op(&lvl3_map_ops, (unsigned long)lvl3_table,
			  GNTMAP_host_map,
			  (grant_ref_t)lvl3_gref, domid);

		gnttab_set_unmap_op(lvl3_unmap_ops, (unsigned long)lvl3_table,
			    GNTMAP_host_map, -1);

		if (gnttab_map_refs(&lvl3_map_ops, NULL, lvl3_table_page, 1)) {
			dev_err(hy_drv_priv->dev,
				"HYPERVISOR map grant ref failed");
			return NULL;
		}
	}

	if (lvl3_map_ops.status) {
		dev_err(hy_drv_priv->dev,
			"HYPERVISOR map grant ref failed status = %d",
			lvl3_map_ops.status);

		goto error_cleanup_lvl3;
	} else {
		lvl3_unmap_ops->handle = lvl3_map_ops.handle;
		if (!xen_feature(XENFEAT_auto_translated_physmap))
			lvl3_pte_unmap_ops->handle = lvl3_pte_map_ops.handle;
	}

	/* Map all second level pages */
	if (gnttab_alloc_pages(n_lvl2_grefs, lvl2_table_pages)) {
		dev_err(hy_drv_priv->dev, "Cannot allocate pages\n");
		goto error_cleanup_lvl3;
	}

	for (i = 0; i < n_lvl2_grefs; i++) {
		lvl2_table = (grant_ref_t *)pfn_to_kaddr(
					page_to_pfn(lvl2_table_pages[i]));
		gnttab_set_map_op(&lvl2_map_ops[i],
				  (unsigned long)lvl2_table, GNTMAP_host_map |
				  GNTMAP_readonly,
				  lvl3_table[i], domid);
		gnttab_set_unmap_op(&lvl2_unmap_ops[i],
				    (unsigned long)lvl2_table, GNTMAP_host_map |
				    GNTMAP_readonly, -1);
	}

	if (gnttab_map_refs(lvl2_map_ops, NULL,
			    lvl2_table_pages, n_lvl2_grefs)) {
		dev_err(hy_drv_priv->dev,
			"HYPERVISOR map grant ref failed");
		return NULL;
	}

	/* Checks if pages were mapped correctly */
	for (i = 0; i < n_lvl2_grefs; i++) {
		if (lvl2_map_ops[i].status) {
			dev_err(hy_drv_priv->dev,
				"HYPERVISOR map grant ref failed status = %d",
				lvl2_map_ops[i].status);
			goto error_cleanup_lvl2;
		} else {
			lvl2_unmap_ops[i].handle = lvl2_map_ops[i].handle;
		}
	}

	if (gnttab_alloc_pages(nents, data_pages)) {
		dev_err(hy_drv_priv->dev,
			"Cannot allocate pages\n");
		goto error_cleanup_lvl2;
	}

	k = 0;

	for (i = 0; i < n_lvl2_grefs - 1; i++) {
		lvl2_table = pfn_to_kaddr(page_to_pfn(lvl2_table_pages[i]));
		for (j = 0; j < REFS_PER_PAGE; j++) {
			gnttab_set_map_op(&data_map_ops[k],
				(unsigned long)pfn_to_kaddr(
						page_to_pfn(data_pages[k])),
				GNTMAP_host_map | GNTMAP_readonly,
				lvl2_table[j], domid);

			gnttab_set_unmap_op(&data_unmap_ops[k],
				(unsigned long)pfn_to_kaddr(
						page_to_pfn(data_pages[k])),
				GNTMAP_host_map | GNTMAP_readonly, -1);
			k++;
		}
	}

	/* for grefs in the last lvl2 table page */
	lvl2_table = pfn_to_kaddr(page_to_pfn(
				lvl2_table_pages[n_lvl2_grefs - 1]));

	for (j = 0; j < nents_last; j++) {
		gnttab_set_map_op(&data_map_ops[k],
			(unsigned long)pfn_to_kaddr(page_to_pfn(data_pages[k])),
			GNTMAP_host_map | GNTMAP_readonly,
			lvl2_table[j], domid);

		gnttab_set_unmap_op(&data_unmap_ops[k],
			(unsigned long)pfn_to_kaddr(page_to_pfn(data_pages[k])),
			GNTMAP_host_map | GNTMAP_readonly, -1);
		k++;
	}

	if (gnttab_map_refs(data_map_ops, NULL,
			    data_pages, nents)) {
		dev_err(hy_drv_priv->dev,
			"HYPERVISOR map grant ref failed\n");
		return NULL;
	}

	/* unmapping lvl2 table pages */
	if (gnttab_unmap_refs(lvl2_unmap_ops,
			      NULL, lvl2_table_pages,
			      n_lvl2_grefs)) {
		dev_err(hy_drv_priv->dev,
			"Cannot unmap 2nd level refs\n");
		return NULL;
	}

	/* Mark that pages were unmapped */
	for (i = 0; i < n_lvl2_grefs; i++)
		lvl2_unmap_ops[i].handle = -1;

	for (i = 0; i < nents; i++) {
		if (data_map_ops[i].status) {
			dev_err(hy_drv_priv->dev,
				"HYPERVISOR map grant ref failed status = %d\n",
				data_map_ops[i].status);
			goto error_cleanup_data;
		} else {
			data_unmap_ops[i].handle = data_map_ops[i].handle;
		}
	}

	/* store these references for unmapping in the future */
	sh_pages_info->unmap_ops = data_unmap_ops;
	sh_pages_info->data_pages = data_pages;
	sh_pages_info->lvl3_page = lvl3_table_page;
	sh_pages_info->lvl3_unmap_ops = lvl3_unmap_ops;
	sh_pages_info->lvl3_pte_unmap_ops = lvl3_pte_unmap_ops;
	sh_pages_info->lvl3_vm_area = lvl3_vm_area;
	sh_pages_info->lvl3_ptes = lvl3_ptes;

	/* Use last 4 bytest of lvl3_page as lock */
	if (lock) {
		char *tmp;
		if (!xen_feature(XENFEAT_auto_translated_physmap)) {
			tmp = lvl3_vm_area->addr;
		} else {
			tmp = pfn_to_kaddr(page_to_pfn(*lvl3_table_page));
		}
		tmp += PAGE_SIZE - (sizeof(unsigned int));
		*lock = (unsigned int*) tmp;
	}

	gnttab_free_pages(n_lvl2_grefs, lvl2_table_pages);
	kfree(lvl2_table_pages);
	kfree(lvl2_map_ops);
	kfree(lvl2_unmap_ops);
	kfree(data_map_ops);

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return data_pages;

error_cleanup_data:
	gnttab_unmap_refs(data_unmap_ops, NULL, data_pages,
			  nents);

	gnttab_free_pages(nents, data_pages);

error_cleanup_lvl2:
	if (lvl2_unmap_ops[0].handle != -1)
		gnttab_unmap_refs(lvl2_unmap_ops, NULL,
				  lvl2_table_pages, n_lvl2_grefs);
	gnttab_free_pages(n_lvl2_grefs, lvl2_table_pages);

error_cleanup_lvl3:
	if (lvl3_unmap_ops->handle != -1)
		gnttab_unmap_refs(lvl3_unmap_ops, NULL,
				  lvl3_table_page, 1);
	gnttab_free_pages(1, lvl3_table_page);

	kfree(lvl2_table_pages);
	kfree(lvl2_map_ops);
	kfree(lvl2_unmap_ops);
	kfree(data_map_ops);

	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		kfree(lvl3_table_page);
		free_vm_area(sh_pages_info->lvl3_vm_area);
		kfree(lvl3_ptes);
	}

	return NULL;
}

int xen_be_unmap_shared_pages(void **refs_info, int nents)
{
	struct xen_shared_pages_info *sh_pages_info;

	dev_dbg(hy_drv_priv->dev, "%s entry\n", __func__);

	sh_pages_info = (struct xen_shared_pages_info *)(*refs_info);

	if (sh_pages_info->unmap_ops == NULL ||
	    sh_pages_info->data_pages == NULL) {
		dev_warn(hy_drv_priv->dev,
			 "pages already cleaned up or buffer not imported yet\n");
		return 0;
	}

	if (gnttab_unmap_refs(sh_pages_info->unmap_ops, NULL,
			      sh_pages_info->data_pages, nents)) {
		dev_err(hy_drv_priv->dev, "Cannot unmap data pages\n");
		return -EFAULT;
	}

	gnttab_free_pages(nents, sh_pages_info->data_pages);

	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		if (gnttab_unmap_refs(sh_pages_info->lvl3_pte_unmap_ops,
				      sh_pages_info->lvl3_unmap_ops,
				      sh_pages_info->lvl3_page, 1)) {
			dev_err(hy_drv_priv->dev,
				"xen: cannot unmap top level page\n");
			return -EFAULT;
		}
	} else {
		if (gnttab_unmap_refs(sh_pages_info->lvl3_unmap_ops,
				      NULL,
				      sh_pages_info->lvl3_page, 1)) {
			dev_err(hy_drv_priv->dev,
				"xen: cannot unmap top level page\n");
			return -EFAULT;
		}
	}

	gnttab_free_pages(1, sh_pages_info->lvl3_page);

	if (!xen_feature(XENFEAT_auto_translated_physmap))
		free_vm_area(sh_pages_info->lvl3_vm_area);

	kfree(sh_pages_info->lvl3_ptes);
	kfree(sh_pages_info->data_pages);
	kfree(sh_pages_info->lvl3_page);
	kfree(sh_pages_info->unmap_ops);
	kfree(sh_pages_info->lvl3_unmap_ops);
	kfree(sh_pages_info->lvl3_pte_unmap_ops);
	sh_pages_info->unmap_ops = NULL;
	sh_pages_info->data_pages = NULL;
	sh_pages_info->lvl3_unmap_ops = NULL;
	sh_pages_info->lvl3_page = NULL;
	sh_pages_info->lvl3_vm_area = NULL;
	sh_pages_info->lvl3_ptes = NULL;
	kfree(sh_pages_info);
	sh_pages_info = NULL;

	dev_dbg(hy_drv_priv->dev, "%s exit\n", __func__);
	return 0;
}
