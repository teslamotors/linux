/*
 *
 *  ffu.h
 *
 * Copyright © 2013 SanDisk Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program was created by SanDisk Corp
 * The ffu.h file is obtained under the GPL v2.0 license that is
 * available via http://www.gnu.org/licenses/,
 * or http://www.opensource.org/licenses/gpl-2.0.php
*/

#if !defined(_FFU_H_)
#define _FFU_H_

#include <linux/mmc/card.h>

#define CARD_BLOCK_SIZE 512

/*
 * eMMC5.0 Field Firmware Update (FFU) opcodes
*/
#define MMC_FFU_DOWNLOAD_OP 302
#define MMC_FFU_INSTALL_OP 303

#define MMC_FFU_MODE_SET 0x1
#define MMC_FFU_MODE_NORMAL 0x0
#define MMC_FFU_INSTALL_SET 0x1

#ifdef CONFIG_MMC_FFU
#define MMC_FFU_FEATURES 0x1
#define FFU_FEATURES(ffu_features) (ffu_features & MMC_FFU_FEATURES)

int mmc_ffu_download(struct mmc_card *card, const char *name);
int mmc_ffu_install(struct mmc_card *card);
#else
static inline int mmc_ffu_download(struct mmc_card *card, const char *name)
{
	return -ENOSYS;
}
static inline int mmc_ffu_install(struct mmc_card *card)
{
	return -ENOSYS;
}
#endif
#endif /* FFU_H_ */
