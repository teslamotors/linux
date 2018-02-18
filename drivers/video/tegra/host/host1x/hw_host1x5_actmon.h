/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
/*
 * Function naming determines intended use:
 *
 *     <x>_r(void) : Returns the offset for register <x>.
 *
 *     <x>_o(void) : Returns the offset for element <x>.
 *
 *     <x>_w(void) : Returns the word offset for word (4 byte) element <x>.
 *
 *     <x>_<y>_s(void) : Returns size of field <y> of register <x> in bits.
 *
 *     <x>_<y>_f(u32 v) : Returns a value based on 'v' which has been shifted
 *         and masked to place it at field <y> of register <x>.  This value
 *         can be |'d with others to produce a full register value for
 *         register <x>.
 *
 *     <x>_<y>_m(void) : Returns a mask for field <y> of register <x>.  This
 *         value can be ~'d and then &'d to clear the value of field <y> for
 *         register <x>.
 *
 *     <x>_<y>_<z>_f(void) : Returns the constant value <z> after being shifted
 *         to place it at field <y> of register <x>.  This value can be |'d
 *         with others to produce a full register value for <x>.
 *
 *     <x>_<y>_v(u32 r) : Returns the value of field <y> from a full register
 *         <x> value 'r' after being shifted to place its LSB at bit 0.
 *         This value is suitable for direct comparison with other unshifted
 *         values appropriate for use in field <y> of register <x>.
 *
 *     <x>_<y>_<z>_v(void) : Returns the constant value for <z> defined for
 *         field <y> of register <x>.  This value is suitable for direct
 *         comparison with unshifted values appropriate for use in field <y>
 *         of register <x>.
 */
#ifndef _hw_host1x5_actmon_h_
#define _hw_host1x5_actmon_h_

static inline u32 actmon_glb_ctrl_r(void)
{
	return 0x0;
}
static inline u32 actmon_glb_ctrl_sample_tick_f(u32 v)
{
	return (v & 0x1) << 10;
}
static inline u32 actmon_glb_ctrl_sample_tick_m(void)
{
	return 0x1 << 10;
}
static inline u32 actmon_glb_ctrl_source_f(u32 v)
{
	return (v & 0x3) << 8;
}
static inline u32 actmon_glb_ctrl_source_m(void)
{
	return 0x3 << 8;
}
static inline u32 actmon_glb_ctrl_sample_period_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 actmon_glb_ctrl_sample_period_m(void)
{
	return 0xff << 0;
}
static inline u32 actmon_glb_intr_en_r(void)
{
	return 0x4;
}
static inline u32 actmon_glb_intr_status_r(void)
{
	return 0x8;
}
static inline u32 actmon_local_ctrl_r(void)
{
	return 0x80;
}
static inline u32 actmon_local_ctrl_actmon_enable_f(u32 v)
{
	return (v & 0x1) << 31;
}
static inline u32 actmon_local_ctrl_enb_cumulative_f(u32 v)
{
	return (v & 0x1) << 30;
}
static inline u32 actmon_local_ctrl_enb_periodic_f(u32 v)
{
	return (v & 0x1) << 13;
}
static inline u32 actmon_local_ctrl_k_val_f(u32 v)
{
	return (v & 0x7) << 10;
}
static inline u32 actmon_local_ctrl_k_val_m(void)
{
	return 0x7 << 10;
}
static inline u32 actmon_local_intr_en_r(void)
{
	return 0x84;
}
static inline u32 actmon_local_intr_en_avg_above_wmark_en_f(u32 v)
{
	return (v & 0x1) << 29;
}
static inline u32 actmon_local_intr_en_avg_below_wmark_en_f(u32 v)
{
	return (v & 0x1) << 28;
}
static inline u32 actmon_local_intr_status_r(void)
{
	return 0x88;
}
static inline u32 actmon_local_intr_status_avg_above_wmark_v(u32 r)
{
	return (r >> 29) & 0x1;
}
static inline u32 actmon_local_intr_status_avg_below_wmark_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 actmon_local_avg_upper_wmark_r(void)
{
	return 0x94;
}
static inline u32 actmon_local_avg_lower_wmark_r(void)
{
	return 0x98;
}
static inline u32 actmon_local_init_avg_r(void)
{
	return 0x9c;
}
static inline u32 actmon_local_count_r(void)
{
	return 0xa0;
}
static inline u32 actmon_local_avg_count_r(void)
{
	return 0xa4;
}
static inline u32 actmon_local_cumulative_r(void)
{
	return 0xac;
}
#endif
