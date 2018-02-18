/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef TEGRA_TSEC_T186_H
#define TEGRA_TSEC_T186_H

#include "tsec.h"

#define TSEC_THI_STREAMID0	0x30
#define TSEC_THI_STREAMID1	0x34

extern const struct tsec_config tsec_t186_config;
extern const struct tsec_config tsecb_t186_config;

#endif
