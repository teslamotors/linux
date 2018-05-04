/* SPDX-License_Identifier: GPL-2.0
 * Copyright (C) 2016 - 2018 Intel Corporation
 * FIXME: update checkpatch script
 */

#include <linux/slab.h>
#include <media/media-entity.h>


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
int media_entity_enum_init(struct media_entity_enum *ent_enum,
			   struct media_device *mdev)
{
	int idx_max = IPU_COMPAT_MAX_ENTITIES;

	ent_enum->bmap = kcalloc(DIV_ROUND_UP(idx_max, BITS_PER_LONG),
				 sizeof(long), GFP_KERNEL);
	if (!ent_enum->bmap)
		return -ENOMEM;

	bitmap_zero(ent_enum->bmap, idx_max);

	ent_enum->idx_max = idx_max;
	return 0;
}
#endif
