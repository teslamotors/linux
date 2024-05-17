/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_DM_VERITY_XPIN_H
#define __LINUX_DM_VERITY_XPIN_H

struct block_device;

#if IS_ENABLED(CONFIG_SECURITY_XPIN) && IS_BUILTIN(CONFIG_DM_VERITY)
bool dm_verity_xpin_is_verity_bdev(struct block_device *bdev);
#else
static inline bool dm_verity_xpin_is_verity_bdev(struct block_device *bdev)
{
	return false;
}
#endif

#endif /* __LINUX_DM_VERITY_XPIN_H */
