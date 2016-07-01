/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <media/media-entity.h>

#include "intel-ipu4-isys-compat-defs.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
int media_entity_enum_init(struct media_entity_enum *ent_enum,
			   struct media_device *mdev)
{
	int idx_max = INTEL_IPU4_COMPAT_MAX_ENTITIES;

	ent_enum->bmap = kcalloc(DIV_ROUND_UP(idx_max, BITS_PER_LONG),
				 sizeof(long), GFP_KERNEL);
	if (!ent_enum->bmap)
		return -ENOMEM;

	bitmap_zero(ent_enum->bmap, idx_max);

	ent_enum->idx_max = idx_max;
	return 0;
}
#endif
