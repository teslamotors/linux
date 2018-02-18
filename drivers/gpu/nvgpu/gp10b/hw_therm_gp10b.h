/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _hw_therm_gp10b_h_
#define _hw_therm_gp10b_h_

static inline u32 therm_use_a_r(void)
{
	return 0x00020798;
}
static inline u32 therm_use_a_ext_therm_0_enable_f(void)
{
	return 0x1;
}
static inline u32 therm_use_a_ext_therm_1_enable_f(void)
{
	return 0x2;
}
static inline u32 therm_use_a_ext_therm_2_enable_f(void)
{
	return 0x4;
}
static inline u32 therm_evt_ext_therm_0_r(void)
{
	return 0x00020700;
}
static inline u32 therm_evt_ext_therm_0_slow_factor_f(u32 v)
{
	return (v & 0x3f) << 24;
}
static inline u32 therm_evt_ext_therm_0_slow_factor_init_v(void)
{
	return 0x00000001;
}
static inline u32 therm_evt_ext_therm_0_mode_f(u32 v)
{
	return (v & 0x3) << 30;
}
static inline u32 therm_evt_ext_therm_0_mode_normal_v(void)
{
	return 0x00000000;
}
static inline u32 therm_evt_ext_therm_0_mode_inverted_v(void)
{
	return 0x00000001;
}
static inline u32 therm_evt_ext_therm_0_mode_forced_v(void)
{
	return 0x00000002;
}
static inline u32 therm_evt_ext_therm_0_mode_cleared_v(void)
{
	return 0x00000003;
}
static inline u32 therm_evt_ext_therm_1_r(void)
{
	return 0x00020704;
}
static inline u32 therm_evt_ext_therm_1_slow_factor_f(u32 v)
{
	return (v & 0x3f) << 24;
}
static inline u32 therm_evt_ext_therm_1_slow_factor_init_v(void)
{
	return 0x00000002;
}
static inline u32 therm_evt_ext_therm_1_mode_f(u32 v)
{
	return (v & 0x3) << 30;
}
static inline u32 therm_evt_ext_therm_1_mode_normal_v(void)
{
	return 0x00000000;
}
static inline u32 therm_evt_ext_therm_1_mode_inverted_v(void)
{
	return 0x00000001;
}
static inline u32 therm_evt_ext_therm_1_mode_forced_v(void)
{
	return 0x00000002;
}
static inline u32 therm_evt_ext_therm_1_mode_cleared_v(void)
{
	return 0x00000003;
}
static inline u32 therm_evt_ext_therm_2_r(void)
{
	return 0x00020708;
}
static inline u32 therm_evt_ext_therm_2_slow_factor_f(u32 v)
{
	return (v & 0x3f) << 24;
}
static inline u32 therm_evt_ext_therm_2_slow_factor_init_v(void)
{
	return 0x00000003;
}
static inline u32 therm_evt_ext_therm_2_mode_f(u32 v)
{
	return (v & 0x3) << 30;
}
static inline u32 therm_evt_ext_therm_2_mode_normal_v(void)
{
	return 0x00000000;
}
static inline u32 therm_evt_ext_therm_2_mode_inverted_v(void)
{
	return 0x00000001;
}
static inline u32 therm_evt_ext_therm_2_mode_forced_v(void)
{
	return 0x00000002;
}
static inline u32 therm_evt_ext_therm_2_mode_cleared_v(void)
{
	return 0x00000003;
}
static inline u32 therm_weight_1_r(void)
{
	return 0x00020024;
}
static inline u32 therm_config1_r(void)
{
	return 0x00020050;
}
static inline u32 therm_config2_r(void)
{
	return 0x00020130;
}
static inline u32 therm_config2_slowdown_factor_extended_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 therm_config2_grad_enable_f(u32 v)
{
	return (v & 0x1) << 31;
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
static inline u32 therm_gate_ctrl_eng_pwr_m(void)
{
	return 0x3 << 4;
}
static inline u32 therm_gate_ctrl_eng_pwr_auto_f(void)
{
	return 0x10;
}
static inline u32 therm_gate_ctrl_eng_pwr_off_v(void)
{
	return 0x00000002;
}
static inline u32 therm_gate_ctrl_eng_pwr_off_f(void)
{
	return 0x20;
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
static inline u32 therm_clk_slowdown_r(u32 i)
{
	return 0x00020160 + i*4;
}
static inline u32 therm_clk_slowdown_idle_factor_f(u32 v)
{
	return (v & 0x3f) << 16;
}
static inline u32 therm_clk_slowdown_idle_factor_m(void)
{
	return 0x3f << 16;
}
static inline u32 therm_clk_slowdown_idle_factor_v(u32 r)
{
	return (r >> 16) & 0x3f;
}
static inline u32 therm_clk_slowdown_idle_factor_disabled_f(void)
{
	return 0x0;
}
static inline u32 therm_grad_stepping_table_r(u32 i)
{
	return 0x000202c8 + i*4;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_f(u32 v)
{
	return (v & 0x3f) << 0;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_m(void)
{
	return 0x3f << 0;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_fpdiv_by1p5_f(void)
{
	return 0x1;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_fpdiv_by2_f(void)
{
	return 0x2;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_fpdiv_by4_f(void)
{
	return 0x6;
}
static inline u32 therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f(void)
{
	return 0xe;
}
static inline u32 therm_grad_stepping_table_slowdown_factor1_f(u32 v)
{
	return (v & 0x3f) << 6;
}
static inline u32 therm_grad_stepping_table_slowdown_factor1_m(void)
{
	return 0x3f << 6;
}
static inline u32 therm_grad_stepping_table_slowdown_factor2_f(u32 v)
{
	return (v & 0x3f) << 12;
}
static inline u32 therm_grad_stepping_table_slowdown_factor2_m(void)
{
	return 0x3f << 12;
}
static inline u32 therm_grad_stepping_table_slowdown_factor3_f(u32 v)
{
	return (v & 0x3f) << 18;
}
static inline u32 therm_grad_stepping_table_slowdown_factor3_m(void)
{
	return 0x3f << 18;
}
static inline u32 therm_grad_stepping_table_slowdown_factor4_f(u32 v)
{
	return (v & 0x3f) << 24;
}
static inline u32 therm_grad_stepping_table_slowdown_factor4_m(void)
{
	return 0x3f << 24;
}
static inline u32 therm_grad_stepping0_r(void)
{
	return 0x000202c0;
}
static inline u32 therm_grad_stepping0_feature_s(void)
{
	return 1;
}
static inline u32 therm_grad_stepping0_feature_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 therm_grad_stepping0_feature_m(void)
{
	return 0x1 << 0;
}
static inline u32 therm_grad_stepping0_feature_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 therm_grad_stepping0_feature_enable_f(void)
{
	return 0x1;
}
static inline u32 therm_grad_stepping1_r(void)
{
	return 0x000202c4;
}
static inline u32 therm_grad_stepping1_pdiv_duration_f(u32 v)
{
	return (v & 0x1ffff) << 0;
}
static inline u32 therm_clk_timing_r(u32 i)
{
	return 0x000203c0 + i*4;
}
static inline u32 therm_clk_timing_grad_slowdown_f(u32 v)
{
	return (v & 0x1) << 16;
}
static inline u32 therm_clk_timing_grad_slowdown_m(void)
{
	return 0x1 << 16;
}
static inline u32 therm_clk_timing_grad_slowdown_enabled_f(void)
{
	return 0x10000;
}
#endif
