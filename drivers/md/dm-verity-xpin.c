// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/dm-verity-xpin.h>

#include "dm.h"
#include "dm-verity.h"

#define DM_MSG_PREFIX	"verity-xpin"

static bool is_verity_target(struct dm_target *ti)
{
	int verity_mode;

	if (!dm_is_verity_target(ti))
		return false;

	verity_mode = dm_verity_get_mode(ti);

	if (
		verity_mode != DM_VERITY_MODE_EIO
		&& verity_mode != DM_VERITY_MODE_RESTART
	) {
		return false;
	}

	return true;
}

/*
 * Determines whether the file system of a superblock is located on
 * a verity device.
 */
bool dm_verity_xpin_is_verity_bdev(struct block_device *bdev)
{
	struct mapped_device *md;
	struct dm_table *table;
	struct dm_target *ti;
	int srcu_idx;
	bool is_verity = false;

	md = dm_get_md(bdev->bd_dev);
	if (!md)
		return false;

	table = dm_get_live_table(md, &srcu_idx);

	if (dm_table_get_num_targets(table) != 1)
		goto out;

	ti = dm_table_get_target(table, 0);

	if (is_verity_target(ti))
		is_verity = true;

out:
	dm_put_live_table(md, srcu_idx);
	dm_put(md);

	return is_verity;
}
