/*
 * gk20a allocator
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include "gk20a.h"
#include "mm_gk20a.h"
#include "platform_gk20a.h"
#include "gk20a_allocator.h"

u32 gk20a_alloc_tracing_on;

u64 gk20a_alloc_length(struct gk20a_allocator *a)
{
	if (a->ops->length)
		return a->ops->length(a);

	return 0;
}

u64 gk20a_alloc_base(struct gk20a_allocator *a)
{
	if (a->ops->base)
		return a->ops->base(a);

	return 0;
}

u64 gk20a_alloc_initialized(struct gk20a_allocator *a)
{
	if (!a->ops || !a->ops->inited)
		return 0;

	return a->ops->inited(a);
}

u64 gk20a_alloc_end(struct gk20a_allocator *a)
{
	if (a->ops->end)
		return a->ops->end(a);

	return 0;
}

u64 gk20a_alloc_space(struct gk20a_allocator *a)
{
	if (a->ops->space)
		return a->ops->space(a);

	return 0;
}

u64 gk20a_alloc(struct gk20a_allocator *a, u64 len)
{
	return a->ops->alloc(a, len);
}

void gk20a_free(struct gk20a_allocator *a, u64 addr)
{
	a->ops->free(a, addr);
}

u64 gk20a_alloc_fixed(struct gk20a_allocator *a, u64 base, u64 len)
{
	if (a->ops->alloc_fixed)
		return a->ops->alloc_fixed(a, base, len);

	return 0;
}

void gk20a_free_fixed(struct gk20a_allocator *a, u64 base, u64 len)
{
	/*
	 * If this operation is not defined for the allocator then just do
	 * nothing. The alternative would be to fall back on the regular
	 * free but that may be harmful in unexpected ways.
	 */
	if (a->ops->free_fixed)
		a->ops->free_fixed(a, base, len);
}

int gk20a_alloc_reserve_carveout(struct gk20a_allocator *a,
				 struct gk20a_alloc_carveout *co)
{
	if (a->ops->reserve_carveout)
		return a->ops->reserve_carveout(a, co);

	return -ENODEV;
}

void gk20a_alloc_release_carveout(struct gk20a_allocator *a,
				  struct gk20a_alloc_carveout *co)
{
	if (a->ops->release_carveout)
		a->ops->release_carveout(a, co);
}

void gk20a_alloc_destroy(struct gk20a_allocator *a)
{
	a->ops->fini(a);
	memset(a, 0, sizeof(*a));
}

/*
 * Handle the common init stuff for a gk20a_allocator.
 */
int __gk20a_alloc_common_init(struct gk20a_allocator *a,
			      const char *name, void *priv, bool dbg,
			      const struct gk20a_allocator_ops *ops)
{
	if (!ops)
		return -EINVAL;

	/*
	 * This is the bare minimum operations required for a sensible
	 * allocator.
	 */
	if (!ops->alloc || !ops->free || !ops->fini)
		return -EINVAL;

	a->ops = ops;
	a->priv = priv;
	a->debug = dbg;

	mutex_init(&a->lock);

	strlcpy(a->name, name, sizeof(a->name));

	return 0;
}

void gk20a_alloc_print_stats(struct gk20a_allocator *__a,
			     struct seq_file *s, int lock)
{
	__a->ops->print_stats(__a, s, lock);
}

#ifdef CONFIG_DEBUG_FS
static int __alloc_show(struct seq_file *s, void *unused)
{
	struct gk20a_allocator *a = s->private;

	gk20a_alloc_print_stats(a, s, 1);

	return 0;
}

static int __alloc_open(struct inode *inode, struct file *file)
{
	return single_open(file, __alloc_show, inode->i_private);
}

static const struct file_operations __alloc_fops = {
	.open = __alloc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

void gk20a_init_alloc_debug(struct gk20a *g, struct gk20a_allocator *a)
{
#ifdef CONFIG_DEBUG_FS
	if (!g->debugfs_allocators)
		return;

	a->debugfs_entry = debugfs_create_file(a->name, S_IRUGO,
					       g->debugfs_allocators,
					       a, &__alloc_fops);
#endif
}

void gk20a_fini_alloc_debug(struct gk20a_allocator *a)
{
#ifdef CONFIG_DEBUG_FS
	if (!IS_ERR_OR_NULL(a->debugfs_entry))
		debugfs_remove(a->debugfs_entry);
#endif
}

void gk20a_alloc_debugfs_init(struct device *dev)
{
#ifdef CONFIG_DEBUG_FS
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct dentry *gpu_root = platform->debugfs;
	struct gk20a *g = get_gk20a(dev);

	g->debugfs_allocators = debugfs_create_dir("allocators", gpu_root);
	if (IS_ERR_OR_NULL(g->debugfs_allocators))
		return;

	debugfs_create_u32("tracing", 0664, g->debugfs_allocators,
			   &gk20a_alloc_tracing_on);
#endif
}
