/*
 * Copyright (c) 2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fiq_glue.h>

#include "ote_protocol.h"

static struct fiq_glue_handler *current_handler;

void tlk_fiq_handler(struct pt_regs *regs, void *svc_sp)
{
	current_handler->fiq(current_handler, regs, svc_sp);
}

int fiq_glue_register_handler(struct fiq_glue_handler *handler)
{
	int ret;

	if (!handler || !handler->fiq)
		return -EINVAL;

	if (current_handler)
		return -EBUSY;

	current_handler = handler;

	ret = send_smc(TE_SMC_REGISTER_FIQ_GLUE,
			(uintptr_t)tlk_fiq_glue_aarch64, 0);
	if (ret)
		pr_err("%s: failed to register FIQ glue\n", __func__);

	return ret;
}
