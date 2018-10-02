/*
 * Copyright © 2012 - 2015 Intel Corporation
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

#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include <linux/swap.h>

#include "gvt/fb_decoder.h"

static struct sg_table *
i915_gem_gvtbuffer_get_pages(struct drm_i915_gem_object *obj)
{
	BUG();
	return ERR_PTR(-EINVAL);
}

static void i915_gem_gvtbuffer_put_pages(struct drm_i915_gem_object *obj,
					 struct sg_table *pages)
{
	/* like stolen memory, this should only be called during free
	 * after clearing pin count.
	 */
	sg_free_table(pages);
	kfree(pages);
}

static void
i915_gem_gvtbuffer_release(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin_pages(obj);
}

static const struct drm_i915_gem_object_ops i915_gem_gvtbuffer_ops = {
	.get_pages = i915_gem_gvtbuffer_get_pages,
	.put_pages = i915_gem_gvtbuffer_put_pages,
	.release = i915_gem_gvtbuffer_release,
};

#define GEN8_DECODE_PTE(pte) \
	((dma_addr_t)(((((u64)pte) >> 12) & 0x7ffffffULL) << 12))

#define GEN7_DECODE_PTE(pte) \
	((dma_addr_t)(((((u64)pte) & 0x7f0) << 28) | (u64)(pte & 0xfffff000)))

static struct sg_table *
i915_create_sg_pages_for_gvtbuffer(struct drm_device *dev,
			     u32 start, u32 num_pages)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct sg_table *st;
	struct scatterlist *sg;
	int i;

	st = kmalloc(sizeof(*st), GFP_KERNEL);
	if (st == NULL)
		return NULL;

	if (sg_alloc_table(st, num_pages, GFP_KERNEL)) {
		kfree(st);
		return NULL;
	}

	if (INTEL_INFO(dev_priv)->gen >= 8) {
		gen8_pte_t __iomem *gtt_entries =
			(gen8_pte_t __iomem *)dev_priv->ggtt.gsm +
			(start >> PAGE_SHIFT);
		for_each_sg(st->sgl, sg, num_pages, i) {
			sg->offset = 0;
			sg->length = PAGE_SIZE;
			sg_dma_address(sg) =
				GEN8_DECODE_PTE(readq(&gtt_entries[i]));
			sg_dma_len(sg) = PAGE_SIZE;
		}
	} else {
		gen6_pte_t __iomem *gtt_entries =
			(gen6_pte_t __iomem *)dev_priv->ggtt.gsm +
			(start >> PAGE_SHIFT);
		for_each_sg(st->sgl, sg, num_pages, i) {
			sg->offset = 0;
			sg->length = PAGE_SIZE;
			sg_dma_address(sg) =
				GEN7_DECODE_PTE(readq(&gtt_entries[i]));
			sg_dma_len(sg) = PAGE_SIZE;
		}
	}

	return st;
}

struct drm_i915_gem_object *
i915_gem_object_create_gvtbuffer(struct drm_device *dev,
				 u32 start, u32 num_pages)
{
	struct drm_i915_gem_object *obj;
	obj = i915_gem_object_alloc(to_i915(dev));
	if (obj == NULL)
		return NULL;

	drm_gem_private_object_init(dev, &obj->base, num_pages << PAGE_SHIFT);
	i915_gem_object_init(obj, &i915_gem_gvtbuffer_ops);

	obj->mm.pages = i915_create_sg_pages_for_gvtbuffer(dev, start, num_pages);
	if (obj->mm.pages == NULL) {
		i915_gem_object_free(obj);
		return NULL;
	}

	if (i915_gem_object_pin_pages(obj))
		printk(KERN_ERR "%s:%d> Pin pages failed!\n", __func__, __LINE__);
	obj->cache_level = I915_CACHE_L3_LLC;

	DRM_DEBUG_DRIVER("GVT_GEM: backing store base = 0x%x pages = 0x%x\n",
			 start, num_pages);
	return obj;
}

static int gvt_decode_information(struct drm_device *dev,
				  struct drm_i915_gem_gvtbuffer *args)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct gvt_fb_format fb;
	struct gvt_primary_plane_format *p;
	struct gvt_cursor_plane_format *c;
	struct gvt_pipe_format *pipe;
#if IS_ENABLED(CONFIG_DRM_I915_GVT)
	u32 id = args->id;

	if (gvt_decode_fb_format(dev_priv->gvt, id, &fb))
		return -EINVAL;
#else
	return -EINVAL;
#endif

	pipe = ((args->pipe_id >= I915_MAX_PIPES) ?
		NULL : &fb.pipes[args->pipe_id]);

	if (!pipe || !pipe->primary.enabled) {
		DRM_DEBUG_DRIVER("GVT_GEM: Invalid pipe_id: %d\n",
				 args->pipe_id);
		return -EINVAL;
	}

	if ((args->plane_id) == I915_GVT_PLANE_PRIMARY) {
		p = &pipe->primary;
		args->enabled = p->enabled;
		args->x_offset = p->x_offset;
		args->y_offset = p->y_offset;
		args->start = p->base;
		args->width = p->width;
		args->height = p->height;
		args->stride = p->stride;
		args->bpp = p->bpp;
		args->hw_format = p->hw_format;
		args->drm_format = p->drm_format;
		args->tiled = p->tiled;
	} else if ((args->plane_id) == I915_GVT_PLANE_CURSOR) {
		c = &pipe->cursor;
		args->enabled = c->enabled;
		args->x_offset = c->x_hot;
		args->y_offset = c->y_hot;
		args->x_pos = c->x_pos;
		args->y_pos = c->y_pos;
		args->start = c->base;
		args->width = c->width;
		args->height = c->height;
		args->stride = c->width * (c->bpp / 8);
		args->bpp = c->bpp;
		args->tiled = 0;
	} else {
		DRM_DEBUG_DRIVER("GVT_GEM: Invalid plaine_id: %d\n",
				 args->plane_id);
		return -EINVAL;
	}

	args->size = ALIGN(args->stride * args->height, PAGE_SIZE) >> PAGE_SHIFT;

	if (args->start & (PAGE_SIZE - 1)) {
		DRM_DEBUG_DRIVER("GVT_GEM: Not aligned fb start address: "
				 "0x%x\n", args->start);
		return -EINVAL;
	}

	if (((args->start >> PAGE_SHIFT) + args->size) >
	    ggtt_total_entries(&dev_priv->ggtt)) {
		DRM_DEBUG_DRIVER("GVT: Invalid GTT offset or size\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * Creates a new mm object that wraps some user memory.
 */
int
i915_gem_gvtbuffer_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_i915_gem_gvtbuffer *args = data;
	struct drm_i915_gem_object *obj;
	u32 handle;
	int ret = 0;

	if (INTEL_INFO(dev_priv)->gen < 7)
		return -EPERM;

	if (args->flags & I915_GVTBUFFER_CHECK_CAPABILITY)
		return 0;
#if 0
	if (!gvt_check_host())
		return -EPERM;
#endif
	/* if args->start != 0 do not decode, but use it as ggtt offset*/
	if (args->start == 0) {
		ret = gvt_decode_information(dev, args);
		if (ret)
			return ret;
	}

	if (ret)
		return ret;

	if (args->flags & I915_GVTBUFFER_QUERY_ONLY)
		return 0;

	obj = i915_gem_object_create_gvtbuffer(dev, args->start, args->size);
	if (!obj) {
		DRM_DEBUG_DRIVER("GVT_GEM: Failed to create gem object"
					" for VM FB!\n");
		return -EINVAL;
	}

	if (IS_SKYLAKE(dev_priv) || IS_BROXTON(dev_priv) ||
	    IS_KABYLAKE(dev_priv)) {
		unsigned int tiling_mode = I915_TILING_NONE;
		unsigned int stride = 0;

		switch (args->tiled << 10) {
		case PLANE_CTL_TILED_LINEAR:
			/* Default valid value */
			break;
		case PLANE_CTL_TILED_X:
			tiling_mode = I915_TILING_X;
			stride = args->stride;
			break;
		case PLANE_CTL_TILED_Y:
			tiling_mode = I915_TILING_Y;
			stride = args->stride;
			break;
		default:
			DRM_ERROR("gvt: tiling mode %d not supported\n", args->tiled);
		}
		obj->tiling_and_stride = tiling_mode | stride;
	} else {
		obj->tiling_and_stride = (args->tiled ? I915_TILING_X : I915_TILING_NONE) |
			                 (args->tiled ? args->stride : 0);
	}

	ret = drm_gem_handle_create(file, &obj->base, &handle);

	/* drop reference from allocate - handle holds it now */
	i915_gem_object_put(obj);

	if (ret)
		return ret;

	args->handle = handle;
	return 0;
}
