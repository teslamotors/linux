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

#ifndef __HOST1X_HOST1X05_HARDWARE_H
#define __HOST1X_HOST1X05_HARDWARE_H

#include <linux/types.h>
#include <linux/bitops.h>

#include "hw_host1x05_channel.h"
#include "hw_host1x05_sync.h"
#include "hw_host1x05_uclass.h"

static inline u32 host1x_class_host_wait_syncpt(
	unsigned indx, unsigned threshold)
{
	return host1x_uclass_wait_syncpt_indx_f(indx)
		| host1x_uclass_wait_syncpt_thresh_f(threshold);
}

static inline u32 host1x_class_host_load_syncpt_base(
	unsigned indx, unsigned threshold)
{
	return host1x_uclass_load_syncpt_base_base_indx_f(indx)
		| host1x_uclass_load_syncpt_base_value_f(threshold);
}

static inline u32 host1x_class_host_wait_syncpt_base(
	unsigned indx, unsigned base_indx, unsigned offset)
{
	return host1x_uclass_wait_syncpt_base_indx_f(indx)
		| host1x_uclass_wait_syncpt_base_base_indx_f(base_indx)
		| host1x_uclass_wait_syncpt_base_offset_f(offset);
}

static inline u32 host1x_class_host_incr_syncpt_base(
	unsigned base_indx, unsigned offset)
{
	return host1x_uclass_incr_syncpt_base_base_indx_f(base_indx)
		| host1x_uclass_incr_syncpt_base_offset_f(offset);
}

static inline u32 host1x_class_host_incr_syncpt(
	unsigned cond, unsigned indx)
{
	return host1x_uclass_incr_syncpt_cond_f(cond)
		| host1x_uclass_incr_syncpt_indx_f(indx);
}

/* cdma opcodes */
static inline u32 host1x_opcode_setclass(
	unsigned class_id, unsigned offset, unsigned mask)
{
	return (0 << 28) | (offset << 16) | (class_id << 6) | mask;
}

static inline u32 host1x_opcode_incr(unsigned offset, unsigned count)
{
	return (1 << 28) | (offset << 16) | count;
}

static inline u32 host1x_opcode_nonincr(unsigned offset, unsigned count)
{
	return (2 << 28) | (offset << 16) | count;
}

static inline u32 host1x_opcode_mask(unsigned offset, unsigned mask)
{
	return (3 << 28) | (offset << 16) | mask;
}

static inline u32 host1x_opcode_imm(unsigned offset, unsigned value)
{
	return (4 << 28) | (offset << 16) | value;
}

static inline u32 host1x_opcode_imm_incr_syncpt(unsigned cond, unsigned indx)
{
	return host1x_opcode_imm(host1x_uclass_incr_syncpt_r(),
		host1x_class_host_incr_syncpt(cond, indx));
}

static inline u32 host1x_opcode_restart(unsigned address)
{
	return (5 << 28) | (address >> 4);
}

static inline u32 host1x_opcode_gather(unsigned count)
{
	return (6 << 28) | count;
}

static inline u32 host1x_opcode_gather_nonincr(unsigned offset,	unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | count;
}

static inline u32 host1x_opcode_gather_incr(unsigned offset, unsigned count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | BIT(14) | count;
}

static inline u32 host1x_opcode_acquire_mlock(unsigned id)
{
	return (14 << 28) | id;
}

static inline u32 host1x_opcode_release_mlock(unsigned id)
{
	return (14 << 28) | (1 << 24) | id;
}

#define HOST1X_OPCODE_NOP host1x_opcode_nonincr(0, 0)

#endif
