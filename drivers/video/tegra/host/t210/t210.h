/*
 * Tegra Graphics Chip support for T210
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
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
#ifndef _NVHOST_T210_H_
#define _NVHOST_T210_H_

#include "chip_support.h"

#define T124_NVHOST_NUMCHANNELS 12

extern struct nvhost_device_data t21_host1x_info;
extern struct nvhost_device_data t21_isp_info;
extern struct nvhost_device_data t21_ispb_info;
extern struct nvhost_device_data t21_vi_info;
extern struct nvhost_device_data t21_vi_i2c_info;
extern struct nvhost_device_data t21_msenc_info;
extern struct nvhost_device_data t21_nvdec_info;
extern struct nvhost_device_data t21_nvjpg_info;
extern struct nvhost_device_data t21_tsec_info;
extern struct nvhost_device_data t21_tsecb_info;
extern struct nvhost_device_data t21_vic_info;
extern struct nvhost_device_data t21_vii2c_info;

int nvhost_init_t210_support(struct nvhost_master *host,
       struct nvhost_chip_support *op);

#endif /* _NVHOST_T210_H_ */
