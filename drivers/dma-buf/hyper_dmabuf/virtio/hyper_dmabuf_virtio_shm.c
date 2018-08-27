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
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
#include <linux/vhm/acrn_vhm_mm.h>
#endif
#include "../hyper_dmabuf_drv.h"
#include "hyper_dmabuf_virtio_shm.h"
#include "hyper_dmabuf_virtio_common.h"

#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
struct virtio_shared_pages_info {
	u64 *lvl3_table;
	u64 **lvl2_table;
	u64 lvl3_gref;
	struct page **data_pages;
	int n_lvl2_refs;
	int nents_last;
	int vmid;
};
#else
struct virtio_shared_pages_info {
	u64 *lvl3_table;
	u64 *lvl2_table;
	u64 lvl3_gref;
};
#endif

#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
static long virtio_be_share_pages(struct page **pages,
				 int vmid,
				 int nents,
				 void **refs_info)
{
	dev_err(hy_drv_priv->dev,
		"Pages sharing not available with ACRN backend in SOS\n");

	return -EINVAL;
}

static int virtio_be_unshare_pages(void **refs_info,
				   int nents)
{
	dev_err(hy_drv_priv->dev,
		"Pages sharing not available with ACRN backend in SOS\n");

	return -EINVAL;
}

static struct page **virtio_be_map_shared_pages(unsigned long lvl3_gref,
						int vmid, int nents,
						void **refs_info)
{
	u64 *lvl3_table = NULL;
	u64 **lvl2_table = NULL;
	struct page **data_pages = NULL;
	struct virtio_shared_pages_info *sh_pages_info = NULL;
	void *pageaddr;

	int nents_last = (nents - 1) % REFS_PER_PAGE + 1;
	int n_lvl2_refs = (nents / REFS_PER_PAGE) + ((nents_last > 0) ? 1 : 0) -
			  (nents_last == REFS_PER_PAGE);
	int i, j, k;

	sh_pages_info = kmalloc(sizeof(*sh_pages_info), GFP_KERNEL);
	if (sh_pages_info == NULL)
		goto map_failed;

	*refs_info = (void *) sh_pages_info;

	data_pages = kcalloc(nents, sizeof(struct page *), GFP_KERNEL);
	if (data_pages == NULL)
		goto map_failed;

	lvl2_table = kcalloc(n_lvl2_refs, sizeof(u64 *), GFP_KERNEL);
	if (lvl2_table == NULL)
		goto map_failed;

	lvl3_table = (u64 *)map_guest_phys(vmid, lvl3_gref, PAGE_SIZE);
	if (lvl3_table == NULL)
		goto map_failed;

	for (i = 0; i < n_lvl2_refs; i++) {
		lvl2_table[i] = (u64 *)map_guest_phys(vmid,
						      lvl3_table[i],
						      PAGE_SIZE);
		if (lvl2_table[i] == NULL)
			goto map_failed;
	}

	k = 0;
	for (i = 0; i < n_lvl2_refs - 1; i++) {
		for (j = 0; j < REFS_PER_PAGE; j++) {
			pageaddr = map_guest_phys(vmid,
						  lvl2_table[i][j],
						  PAGE_SIZE);
			if (pageaddr == NULL)
				goto map_failed;

			data_pages[k] = virt_to_page(pageaddr);
			k++;
		}
	}

	for (j = 0; j < nents_last; j++) {
		pageaddr = map_guest_phys(vmid,
					  lvl2_table[i][j],
					  PAGE_SIZE);
		if (pageaddr == NULL)
			goto map_failed;

		data_pages[k] = virt_to_page(pageaddr);
		k++;
	}

	sh_pages_info->lvl2_table = lvl2_table;
	sh_pages_info->lvl3_table = lvl3_table;
	sh_pages_info->lvl3_gref = lvl3_gref;
	sh_pages_info->n_lvl2_refs = n_lvl2_refs;
	sh_pages_info->nents_last = nents_last;
	sh_pages_info->data_pages = data_pages;
	sh_pages_info->vmid = vmid;

	return data_pages;

map_failed:
	dev_err(hy_drv_priv->dev,
		"Cannot map guest memory\n");

	kfree(lvl2_table);
	kfree(data_pages);
	kfree(sh_pages_info);

	return NULL;
}

/*
 * TODO: In theory pages don't need to be unmaped,
 * as ACRN is just translating memory addresses,
 * but not sure if that will work the same way in future
 */
static int virtio_be_unmap_shared_pages(void **refs_info, int nents)
{
	struct virtio_shared_pages_info *sh_pages_info;
	int vmid;
	int i, j;

	sh_pages_info = (struct virtio_shared_pages_info *)(*refs_info);

	if (sh_pages_info->data_pages == NULL) {
		dev_warn(hy_drv_priv->dev,
			 "Imported pages already cleaned up");
		dev_warn(hy_drv_priv->dev,
			 "or buffer was not imported yet\n");
		return 0;
	}
	vmid = sh_pages_info->vmid;

	for (i = 0; i < sh_pages_info->n_lvl2_refs - 1; i++) {
		for (j = 0; j < REFS_PER_PAGE; j++)
			unmap_guest_phys(vmid,
					 sh_pages_info->lvl2_table[i][j]);
	}

	for (j = 0; j < sh_pages_info->nents_last; j++)
		unmap_guest_phys(vmid, sh_pages_info->lvl2_table[i][j]);

	for (i = 0; i < sh_pages_info->n_lvl2_refs; i++)
		unmap_guest_phys(vmid, sh_pages_info->lvl3_table[i]);

	unmap_guest_phys(vmid, sh_pages_info->lvl3_gref);

	kfree(sh_pages_info->lvl2_table);
	kfree(sh_pages_info->data_pages);
	sh_pages_info->data_pages = NULL;
	kfree(sh_pages_info);
	sh_pages_info = NULL;

	return 0;
}
#else
static long virtio_fe_share_pages(struct page **pages,
			  int domid, int nents,
			  void **refs_info)
{
	struct virtio_shared_pages_info *sh_pages_info;
	u64 lvl3_gref;
	u64 *lvl2_table;
	u64 *lvl3_table;
	int i;

	/*
	 * Calculate number of pages needed for 2nd level addresing:
	 */
	int n_lvl2_grefs = (nents/REFS_PER_PAGE +
			    ((nents % REFS_PER_PAGE) ? 1 : 0));

	lvl3_table = (u64 *)__get_free_pages(GFP_KERNEL, 1);
	lvl2_table = (u64 *)__get_free_pages(GFP_KERNEL, n_lvl2_grefs);

	sh_pages_info = kmalloc(sizeof(*sh_pages_info), GFP_KERNEL);

	if (sh_pages_info == NULL)
		return -ENOMEM;

	*refs_info = (void *)sh_pages_info;

	/* Share physical address of pages */
	for (i = 0; i < nents; i++)
		lvl2_table[i] = page_to_phys(pages[i]);

	for (i = 0; i < n_lvl2_grefs; i++)
		lvl3_table[i] =
			virt_to_phys((void *)lvl2_table + i * PAGE_SIZE);

	lvl3_gref = virt_to_phys(lvl3_table);

	sh_pages_info->lvl3_table = lvl3_table;
	sh_pages_info->lvl2_table = lvl2_table;
	sh_pages_info->lvl3_gref = lvl3_gref;

	return lvl3_gref;
}

static int virtio_fe_unshare_pages(void **refs_info,
				   int nents)
{
	struct virtio_shared_pages_info *sh_pages_info;
	int n_lvl2_grefs = (nents/REFS_PER_PAGE +
			    ((nents % REFS_PER_PAGE) ? 1 : 0));

	sh_pages_info = (struct virtio_shared_pages_info *)(*refs_info);

	if (sh_pages_info == NULL) {
		dev_err(hy_drv_priv->dev,
			"No pages info\n");
		return -EINVAL;
	}

	free_pages((unsigned long)sh_pages_info->lvl2_table, n_lvl2_grefs);
	free_pages((unsigned long)sh_pages_info->lvl3_table, 1);

	kfree(sh_pages_info);

	return 0;
}

static struct page **virtio_fe_map_shared_pages(unsigned long lvl3_gref,
						int vmid, int nents,
						void **refs_info)
{
	dev_dbg(hy_drv_priv->dev,
		"Virtio frontend not supporting currently page mapping\n");
	return NULL;
}

static int virtio_fe_unmap_shared_pages(void **refs_info, int nents)
{
	dev_dbg(hy_drv_priv->dev,
		"Virtio frontend not supporting currently page mapping\n");
	return -EINVAL;
}

#endif

long virtio_share_pages(struct page **pages,
		       int domid, int nents,
		       void **refs_info)
{
	long ret;
#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
	ret = virtio_be_share_pages(pages, domid, nents, refs_info);
#else
	ret = virtio_fe_share_pages(pages, domid, nents, refs_info);
#endif
	return ret;
}

int virtio_unshare_pages(void **refs_info, int nents)
{
	int ret;
#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
	ret = virtio_be_unshare_pages(refs_info, nents);
#else
	ret = virtio_fe_unshare_pages(refs_info, nents);
#endif
	return ret;
}

struct page **virtio_map_shared_pages(unsigned long lvl3_gref,
				      int vmid, int nents,
				      void **refs_info)
{
	struct page **ret;
#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
	ret = virtio_be_map_shared_pages(lvl3_gref, vmid,
					 nents, refs_info);
#else
	ret = virtio_fe_map_shared_pages(lvl3_gref, vmid,
					 nents, refs_info);
#endif
	return ret;
}

int virtio_unmap_shared_pages(void **refs_info, int nents)
{
	int ret;
#ifdef CONFIG_HYPER_DMABUF_VIRTIO_BE
	ret = virtio_be_unmap_shared_pages(refs_info, nents);
#else
	ret = virtio_fe_unmap_shared_pages(refs_info, nents);
#endif
	return ret;
}
