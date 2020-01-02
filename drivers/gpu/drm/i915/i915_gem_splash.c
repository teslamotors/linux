/*
 * Copyright © 2016 Intel Corporation
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
 */

#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"


/*
 * The memory was allocated outside of the i915 driver and is non-pagable,
 * it can not be migrated by either the i915 driver or the OS.
 *
 * The scatter/gather table has already been initialized when the gem obj
 * was created, nothing more needs to be done here, return 0 to
 * indicate sg is ready.
 */
static struct sg_table *
i915_gem_object_get_pages_splash(struct drm_i915_gem_object *obj)
{
	return NULL;
}

/* The sg is going to be freed when the gem obj itself is released. */
static void i915_gem_object_put_pages_splash(struct drm_i915_gem_object *obj,
					     struct sg_table *pages)
{
}


	static void
i915_gem_object_release_splash(struct drm_i915_gem_object *obj)
{
	if (obj->mm.pages) {
		dma_unmap_sg(&obj->base.dev->pdev->dev,
			     obj->mm.pages->sgl, obj->mm.pages->nents,
			     DMA_TO_DEVICE);
		sg_free_table(obj->mm.pages);
		kfree(obj->mm.pages);
		obj->mm.pages = NULL;
	}
}

static const struct drm_i915_gem_object_ops i915_gem_object_splash_ops = {
	.get_pages = i915_gem_object_get_pages_splash,
	.put_pages = i915_gem_object_put_pages_splash,
	.release = i915_gem_object_release_splash,
};

/* create a gem obj from a list of pages */
struct drm_i915_gem_object *
i915_gem_object_create_splash_pages(struct drm_i915_private *dev_priv,
				    struct page **pages, u32 n_pages)
{
	struct drm_i915_gem_object *obj;
	struct sg_table *st;
	unsigned long size = n_pages << PAGE_SHIFT;

	if (n_pages == 0)
		return NULL;

	obj = i915_gem_object_alloc(dev_priv);
	if (obj == NULL)
		return NULL;

	st = kmalloc(sizeof(*st), GFP_KERNEL);
	if (st == NULL)
		goto cleanup;

	drm_gem_private_object_init(&dev_priv->drm, &obj->base, size);
	i915_gem_object_init(obj, &i915_gem_object_splash_ops);

	if (sg_alloc_table_from_pages(st, pages, n_pages,
				     0, size, GFP_KERNEL))
		goto cleanup_st;

	obj->mm.pages = st;
	obj->base.read_domains = I915_GEM_DOMAIN_GTT;
	obj->cache_level = HAS_LLC(dev_priv) ? I915_CACHE_LLC : I915_CACHE_NONE;

	if (!dma_map_sg(&obj->base.dev->pdev->dev,
		       obj->mm.pages->sgl, obj->mm.pages->nents,
		       DMA_TO_DEVICE)) {
		sg_free_table(obj->mm.pages);
		obj->mm.pages = NULL;
		goto cleanup_st;
	}
	return obj;

cleanup_st:
	kfree(st);
cleanup:
	i915_gem_object_free(obj);
	return NULL;
}

/* create a gem obj from a virtual address */
struct drm_i915_gem_object *
i915_gem_object_create_splash(struct drm_i915_private *dev_priv,
			      const u8 *ptr, u32 n_pages)
{
	struct page **pvec;
	u32 i;
	struct drm_i915_gem_object *obj = NULL;

	if (ptr == NULL || n_pages == 0)
		return NULL;

	WARN_ON (!PAGE_ALIGNED(ptr));

	pvec = kmalloc(n_pages * sizeof(struct page *),
		       GFP_TEMPORARY | __GFP_NOWARN | __GFP_NORETRY);
	if (pvec == NULL)
		return NULL;

	for (i = 0; i < n_pages; i++) {
		*(pvec+i) = vmalloc_to_page(ptr);
		ptr += PAGE_SIZE;
	}

	obj = i915_gem_object_create_splash_pages(dev_priv, pvec, n_pages);
	kfree(pvec);
	return obj;
}
