/*
 * Copyright (C) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#ifndef __TEGRA_HV_H__
#define __TEGRA_HV_H__

#include "syscalls.h"

const struct ivc_info_page *tegra_hv_get_ivc_info(void);
int tegra_hv_get_vmid(void);

#endif /* __TEGRA_HV_H__ */
