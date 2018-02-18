/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _hw_xve_gp106_h_
#define _hw_xve_gp106_h_

static inline u32 xve_rom_ctrl_r(void)
{
	return 0x00000050;
}
static inline u32 xve_rom_ctrl_rom_shadow_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 xve_rom_ctrl_rom_shadow_disabled_f(void)
{
	return 0x0;
}
static inline u32 xve_rom_ctrl_rom_shadow_enabled_f(void)
{
	return 0x1;
}
static inline u32 xve_link_control_status_r(void)
{
	return 0x00000088;
}
static inline u32 xve_link_control_status_link_speed_m(void)
{
	return 0xf << 16;
}
static inline u32 xve_link_control_status_link_speed_v(u32 r)
{
	return (r >> 16) & 0xf;
}
static inline u32 xve_link_control_status_link_speed_link_speed_2p5_v(void)
{
	return 0x00000001;
}
static inline u32 xve_link_control_status_link_speed_link_speed_5p0_v(void)
{
	return 0x00000002;
}
static inline u32 xve_link_control_status_link_speed_link_speed_8p0_v(void)
{
	return 0x00000003;
}
static inline u32 xve_link_control_status_link_width_m(void)
{
	return 0x3f << 20;
}
static inline u32 xve_link_control_status_link_width_v(u32 r)
{
	return (r >> 20) & 0x3f;
}
static inline u32 xve_link_control_status_link_width_x1_v(void)
{
	return 0x00000001;
}
static inline u32 xve_link_control_status_link_width_x2_v(void)
{
	return 0x00000002;
}
static inline u32 xve_link_control_status_link_width_x4_v(void)
{
	return 0x00000004;
}
static inline u32 xve_link_control_status_link_width_x8_v(void)
{
	return 0x00000008;
}
static inline u32 xve_link_control_status_link_width_x16_v(void)
{
	return 0x00000010;
}
static inline u32 xve_priv_xv_r(void)
{
	return 0x00000150;
}
static inline u32 xve_priv_xv_cya_l0s_enable_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 xve_priv_xv_cya_l0s_enable_m(void)
{
	return 0x1 << 7;
}
static inline u32 xve_priv_xv_cya_l0s_enable_v(u32 r)
{
	return (r >> 7) & 0x1;
}
static inline u32 xve_priv_xv_cya_l1_enable_f(u32 v)
{
	return (v & 0x1) << 8;
}
static inline u32 xve_priv_xv_cya_l1_enable_m(void)
{
	return 0x1 << 8;
}
static inline u32 xve_priv_xv_cya_l1_enable_v(u32 r)
{
	return (r >> 8) & 0x1;
}
static inline u32 xve_reset_r(void)
{
	return 0x00000718;
}
static inline u32 xve_reset_reset_m(void)
{
	return 0x1 << 0;
}
static inline u32 xve_reset_gpu_on_sw_reset_m(void)
{
	return 0x1 << 1;
}
static inline u32 xve_reset_counter_en_m(void)
{
	return 0x1 << 2;
}
static inline u32 xve_reset_counter_val_f(u32 v)
{
	return (v & 0x7ff) << 4;
}
static inline u32 xve_reset_counter_val_m(void)
{
	return 0x7ff << 4;
}
static inline u32 xve_reset_counter_val_v(u32 r)
{
	return (r >> 4) & 0x7ff;
}
static inline u32 xve_reset_clock_on_sw_reset_m(void)
{
	return 0x1 << 15;
}
static inline u32 xve_reset_clock_counter_en_m(void)
{
	return 0x1 << 16;
}
static inline u32 xve_reset_clock_counter_val_f(u32 v)
{
	return (v & 0x7ff) << 17;
}
static inline u32 xve_reset_clock_counter_val_m(void)
{
	return 0x7ff << 17;
}
static inline u32 xve_reset_clock_counter_val_v(u32 r)
{
	return (r >> 17) & 0x7ff;
}
#endif
