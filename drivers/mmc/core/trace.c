/*
 * MMC trace points
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>

#ifdef MODULE

#define CREATE_TRACE_POINTS
#include <trace/events/mmc.h>

EXPORT_TRACEPOINT_SYMBOL(mmc_blk_erase_start);
EXPORT_TRACEPOINT_SYMBOL(mmc_blk_erase_end);
EXPORT_TRACEPOINT_SYMBOL(mmc_blk_rw_start);
EXPORT_TRACEPOINT_SYMBOL(mmc_blk_rw_end);

#endif /* MODULE */
