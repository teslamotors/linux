/*
 * GP10B specific sysfs files
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _GP10B_SYSFS_H_
#define _GP10B_SYSFS_H_

/*ECC Fuse*/
#define FUSE_OPT_ECC_EN  0x358

void gp10b_create_sysfs(struct device *dev);
void gp10b_remove_sysfs(struct device *dev);

#endif /*_GP10B_SYSFS_H_*/
