/*
 * drivers/video/tegra/nvmap/nvmap_tag.c
 *
 * Allocation tag routines for nvmap
 *
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/moduleparam.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"

struct nvmap_tag_entry *nvmap_search_tag_entry(struct rb_root *root, u32 tag)
{
	struct rb_node *node = root->rb_node;  /* top of the tree */
	struct nvmap_tag_entry *entry;

	while (node) {
		entry = rb_entry(node, struct nvmap_tag_entry, node);

		if (entry->tag > tag)
			node = node->rb_left;
		else if (entry->tag < tag)
			node = node->rb_right;
		else
			return entry;  /* Found it */
	}
	return NULL;
}


static void nvmap_insert_tag_entry(struct rb_root *root,
		struct nvmap_tag_entry *new)
{
	struct rb_node **link = &root->rb_node;
	struct rb_node *parent = NULL;
	struct nvmap_tag_entry *entry;
	u32 tag = new->tag;

	/* Go to the bottom of the tree */
	while (*link) {
	    parent = *link;
	    entry = rb_entry(parent, struct nvmap_tag_entry, node);

	    if (entry->tag > tag)
		link = &parent->rb_left;
	    else
		link = &parent->rb_right;
	}

	/* Put the new node there */
	rb_link_node(&new->node, parent, link);
	rb_insert_color(&new->node, root);
}


int nvmap_define_tag(struct nvmap_device *dev, u32 tag,
		const char __user *name, u32 len)
{
	struct nvmap_tag_entry *new;
	struct nvmap_tag_entry *old;

	new = kzalloc(sizeof(struct nvmap_tag_entry) + len + 1, GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	if (copy_from_user(new + 1, name, len)) {
		kfree(new);
		return -EFAULT;
	}

	new->tag = tag;

	mutex_lock(&dev->tags_lock);
	old = nvmap_search_tag_entry(&dev->tags, tag);
	if (old) {
		rb_replace_node(&old->node, &new->node, &dev->tags);
		kfree(old);
	} else {
		nvmap_insert_tag_entry(&dev->tags, new);
	}
	mutex_unlock(&dev->tags_lock);

	return 0;
}

int nvmap_remove_tag(struct nvmap_device *dev, u32 tag)
{
	struct nvmap_tag_entry *old;

	mutex_lock(&dev->tags_lock);
	old = nvmap_search_tag_entry(&dev->tags, tag);
	if (old)
		rb_erase(&old->node, &dev->tags);
	mutex_unlock(&dev->tags_lock);

	return 0;
}
