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
#ifndef _hw_therm_gp106_h_
#define _hw_therm_gp106_h_

static inline u32 therm_temp_sensor_tsense_r(void)
{
	return 0x00020460;
}
static inline u32 therm_temp_sensor_tsense_fixed_point_f(u32 v)
{
	return (v & 0x3fff) << 3;
}
static inline u32 therm_temp_sensor_tsense_fixed_point_m(void)
{
	return 0x3fff << 3;
}
static inline u32 therm_temp_sensor_tsense_fixed_point_v(u32 r)
{
	return (r >> 3) & 0x3fff;
}
static inline u32 therm_temp_sensor_tsense_fixed_point_min_v(void)
{
	return 0x00003b00;
}
static inline u32 therm_temp_sensor_tsense_fixed_point_max_v(void)
{
	return 0x000010e0;
}
static inline u32 therm_temp_sensor_tsense_state_f(u32 v)
{
	return (v & 0x3) << 29;
}
static inline u32 therm_temp_sensor_tsense_state_m(void)
{
	return 0x3 << 29;
}
static inline u32 therm_temp_sensor_tsense_state_v(u32 r)
{
	return (r >> 29) & 0x3;
}
static inline u32 therm_temp_sensor_tsense_state_valid_v(void)
{
	return 0x00000001;
}
static inline u32 therm_temp_sensor_tsense_state_shadow_v(void)
{
	return 0x00000002;
}
static inline u32 therm_gate_ctrl_r(u32 i)
{
	return 0x00020200 + i*4;
}
static inline u32 therm_gate_ctrl_eng_clk_m(void)
{
	return 0x3 << 0;
}
static inline u32 therm_gate_ctrl_eng_clk_run_f(void)
{
	return 0x0;
}
static inline u32 therm_gate_ctrl_eng_clk_auto_f(void)
{
	return 0x1;
}
static inline u32 therm_gate_ctrl_eng_clk_stop_f(void)
{
	return 0x2;
}
static inline u32 therm_gate_ctrl_blk_clk_m(void)
{
	return 0x3 << 2;
}
static inline u32 therm_gate_ctrl_blk_clk_run_f(void)
{
	return 0x0;
}
static inline u32 therm_gate_ctrl_blk_clk_auto_f(void)
{
	return 0x4;
}
static inline u32 therm_gate_ctrl_eng_idle_filt_exp_f(u32 v)
{
	return (v & 0x1f) << 8;
}
static inline u32 therm_gate_ctrl_eng_idle_filt_exp_m(void)
{
	return 0x1f << 8;
}
static inline u32 therm_gate_ctrl_eng_idle_filt_mant_f(u32 v)
{
	return (v & 0x7) << 13;
}
static inline u32 therm_gate_ctrl_eng_idle_filt_mant_m(void)
{
	return 0x7 << 13;
}
static inline u32 therm_gate_ctrl_eng_delay_before_f(u32 v)
{
	return (v & 0xf) << 16;
}
static inline u32 therm_gate_ctrl_eng_delay_before_m(void)
{
	return 0xf << 16;
}
static inline u32 therm_gate_ctrl_eng_delay_after_f(u32 v)
{
	return (v & 0xf) << 20;
}
static inline u32 therm_gate_ctrl_eng_delay_after_m(void)
{
	return 0xf << 20;
}
static inline u32 therm_fecs_idle_filter_r(void)
{
	return 0x00020288;
}
static inline u32 therm_fecs_idle_filter_value_m(void)
{
	return 0xffffffff << 0;
}
static inline u32 therm_hubmmu_idle_filter_r(void)
{
	return 0x0002028c;
}
static inline u32 therm_hubmmu_idle_filter_value_m(void)
{
	return 0xffffffff << 0;
}
#endif
