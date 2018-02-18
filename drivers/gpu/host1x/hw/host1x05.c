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

/* include hw specification */
#include "host1x05.h"
#include "host1x05_hardware.h"

/* include code */
#include "cdma_hw_t186.c"
#include "channel_hw_t186.c"
#include "debug_hw_t186.c"
#include "intr_hw_t186.c"
#include "syncpt_hw_t186.c"
#include "stremid_hw_t186.c"

#include "dev.h"

int host1x05_init(struct host1x *host)
{
	host->dev_op = &host1x_dev_t186_ops;
	host->channel_op = &host1x_channel_t186_ops;
	host->cdma_op = &host1x_cdma_t186_ops;
	host->cdma_pb_op = &host1x_pushbuffer_t186_ops;
	host->syncpt_op = &host1x_syncpt_t186_ops;
	host->intr_op = &host1x_intr_t186_ops;
	host->debug_op = &host1x_debug_t186_ops;

	return 0;
}
