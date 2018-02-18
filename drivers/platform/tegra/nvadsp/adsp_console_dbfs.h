/*
* adsp_console_dbfs.h
*
* A header file for adsp console driver
*
* Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef ADSP_CNSL_DBFS_H
#define ADSP_CNSL_DBFS_H

struct nvadsp_cnsl {
	struct device *dev;
	struct nvadsp_mbox shl_snd_mbox;
	struct nvadsp_mbox app_mbox;
};

int
adsp_create_cnsl(struct dentry *adsp_debugfs_root, struct nvadsp_cnsl *cnsl);

#endif /* ADSP_CNSL_DBFS_H */
