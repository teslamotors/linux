/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _hw_xp_gp106_h_
#define _hw_xp_gp106_h_

static inline u32 xp_dl_mgr_r(u32 i)
{
	return 0x0008b8c0 + i*4;
}
static inline u32 xp_dl_mgr_safe_timing_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 xp_pl_link_config_r(u32 i)
{
	return 0x0008c040 + i*4;
}
static inline u32 xp_pl_link_config_ltssm_status_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 xp_pl_link_config_ltssm_status_idle_v(void)
{
	return 0x00000000;
}
static inline u32 xp_pl_link_config_ltssm_directive_f(u32 v)
{
	return (v & 0xf) << 0;
}
static inline u32 xp_pl_link_config_ltssm_directive_m(void)
{
	return 0xf << 0;
}
static inline u32 xp_pl_link_config_ltssm_directive_normal_operations_v(void)
{
	return 0x00000000;
}
static inline u32 xp_pl_link_config_ltssm_directive_change_speed_v(void)
{
	return 0x00000001;
}
static inline u32 xp_pl_link_config_max_link_rate_f(u32 v)
{
	return (v & 0x3) << 18;
}
static inline u32 xp_pl_link_config_max_link_rate_m(void)
{
	return 0x3 << 18;
}
static inline u32 xp_pl_link_config_max_link_rate_2500_mtps_v(void)
{
	return 0x00000002;
}
static inline u32 xp_pl_link_config_max_link_rate_5000_mtps_v(void)
{
	return 0x00000001;
}
static inline u32 xp_pl_link_config_max_link_rate_8000_mtps_v(void)
{
	return 0x00000000;
}
static inline u32 xp_pl_link_config_target_tx_width_f(u32 v)
{
	return (v & 0x7) << 20;
}
static inline u32 xp_pl_link_config_target_tx_width_m(void)
{
	return 0x7 << 20;
}
static inline u32 xp_pl_link_config_target_tx_width_x1_v(void)
{
	return 0x00000007;
}
static inline u32 xp_pl_link_config_target_tx_width_x2_v(void)
{
	return 0x00000006;
}
static inline u32 xp_pl_link_config_target_tx_width_x4_v(void)
{
	return 0x00000005;
}
static inline u32 xp_pl_link_config_target_tx_width_x8_v(void)
{
	return 0x00000004;
}
static inline u32 xp_pl_link_config_target_tx_width_x16_v(void)
{
	return 0x00000000;
}
#endif
