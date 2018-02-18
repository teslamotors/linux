/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_CLK_MRQ_H
#define _TEGRA_CLK_MRQ_H

enum {
	MRQ_CLK_GET_RATE = 1,
	MRQ_CLK_SET_RATE = 2,
	MRQ_CLK_ROUND_RATE = 3,
	MRQ_CLK_GET_PARENT = 4,
	MRQ_CLK_SET_PARENT = 5,
	MRQ_CLK_IS_ENABLED = 6,
	MRQ_CLK_ENABLE = 7,
	MRQ_CLK_DISABLE = 8,
	MRQ_CLK_PROPERTIES = 9,
	MRQ_CLK_POSSIBLE_PARENTS = 10,
	MRQ_CLK_NUM_POSSIBLE_PARENTS = 11,
	MRQ_CLK_GET_POSSIBLE_PARENT = 12,
	MRQ_CLK_RESET_REFCOUNTS = 13,
	MRQ_CLK_MAX,
};

#endif
