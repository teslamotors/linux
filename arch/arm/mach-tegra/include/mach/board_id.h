/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 *
 */

#ifndef __INCLUDE_BOARD_INFO_H
#define __INCLUDE_BOARD_INFO_H

#define MAX_BUFFER 8

struct tegra_vcm_board_info {
	int valid;
	char bom[MAX_BUFFER];
	char project[MAX_BUFFER];
	char sku[MAX_BUFFER];
	char revision[MAX_BUFFER];
};

bool tegra_is_vcm_board_of_type(const char *bom, const char *project,
		const char *sku, const char *revision);
u32 tegra_get_vcm_sku_rev(void);
#endif
