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
#ifndef _hw_ltc_gp10b_h_
#define _hw_ltc_gp10b_h_

static inline u32 ltc_pltcg_base_v(void)
{
	return 0x00140000;
}
static inline u32 ltc_pltcg_extent_v(void)
{
	return 0x0017ffff;
}
static inline u32 ltc_ltc0_ltss_v(void)
{
	return 0x00140200;
}
static inline u32 ltc_ltc0_lts0_v(void)
{
	return 0x00140400;
}
static inline u32 ltc_ltcs_ltss_v(void)
{
	return 0x0017e200;
}
static inline u32 ltc_ltcs_lts0_cbc_ctrl1_r(void)
{
	return 0x0014046c;
}
static inline u32 ltc_ltc0_lts0_dstg_cfg0_r(void)
{
	return 0x00140518;
}
static inline u32 ltc_ltcs_ltss_dstg_cfg0_r(void)
{
	return 0x0017e318;
}
static inline u32 ltc_ltcs_ltss_dstg_cfg0_vdc_4to2_disable_m(void)
{
	return 0x1 << 15;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_r(void)
{
	return 0x00140494;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_active_ways_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_active_sets_v(u32 r)
{
	return (r >> 16) & 0x3;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_active_sets_all_v(void)
{
	return 0x00000000;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_active_sets_half_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc0_lts0_tstg_cfg1_active_sets_quarter_v(void)
{
	return 0x00000002;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_r(void)
{
	return 0x0017e26c;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_clean_active_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_invalidate_active_f(void)
{
	return 0x2;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_clear_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_clear_active_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl1_clear_active_f(void)
{
	return 0x4;
}
static inline u32 ltc_ltc0_lts0_cbc_ctrl1_r(void)
{
	return 0x0014046c;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl2_r(void)
{
	return 0x0017e270;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl2_clear_lower_bound_f(u32 v)
{
	return (v & 0x3ffff) << 0;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl3_r(void)
{
	return 0x0017e274;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_f(u32 v)
{
	return (v & 0x3ffff) << 0;
}
static inline u32 ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_init_v(void)
{
	return 0x0003ffff;
}
static inline u32 ltc_ltcs_ltss_cbc_base_r(void)
{
	return 0x0017e278;
}
static inline u32 ltc_ltcs_ltss_cbc_base_alignment_shift_v(void)
{
	return 0x0000000b;
}
static inline u32 ltc_ltcs_ltss_cbc_base_address_v(u32 r)
{
	return (r >> 0) & 0x3ffffff;
}
static inline u32 ltc_ltcs_ltss_cbc_num_active_ltcs_r(void)
{
	return 0x0017e27c;
}
static inline u32 ltc_ltcs_misc_ltc_num_active_ltcs_r(void)
{
	return 0x0017e000;
}
static inline u32 ltc_ltcs_ltss_cbc_param_r(void)
{
	return 0x0017e280;
}
static inline u32 ltc_ltcs_ltss_cbc_param_comptags_per_cache_line_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ltc_ltcs_ltss_cbc_param_cache_line_size_v(u32 r)
{
	return (r >> 24) & 0xf;
}
static inline u32 ltc_ltcs_ltss_cbc_param_slices_per_ltc_v(u32 r)
{
	return (r >> 28) & 0xf;
}
static inline u32 ltc_ltcs_ltss_cbc_param2_r(void)
{
	return 0x0017e3f4;
}
static inline u32 ltc_ltcs_ltss_cbc_param2_gobs_per_comptagline_per_slice_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ltc_ltcs_ltss_tstg_set_mgmt_r(void)
{
	return 0x0017e2ac;
}
static inline u32 ltc_ltcs_ltss_tstg_set_mgmt_max_ways_evict_last_f(u32 v)
{
	return (v & 0x1f) << 16;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_index_r(void)
{
	return 0x0017e338;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_index_address_f(u32 v)
{
	return (v & 0xf) << 0;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(u32 i)
{
	return 0x0017e33c + i*4;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(void)
{
	return 0x00000004;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(void)
{
	return 0x0017e34c;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_s(void)
{
	return 32;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_m(void)
{
	return 0xffffffff << 0;
}
static inline u32 ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_v(u32 r)
{
	return (r >> 0) & 0xffffffff;
}
static inline u32 ltc_ltcs_ltss_tstg_set_mgmt_2_r(void)
{
	return 0x0017e2b0;
}
static inline u32 ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f(void)
{
	return 0x10000000;
}
static inline u32 ltc_ltcs_ltss_g_elpg_r(void)
{
	return 0x0017e214;
}
static inline u32 ltc_ltcs_ltss_g_elpg_flush_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltcs_ltss_g_elpg_flush_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_g_elpg_flush_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc0_ltss_g_elpg_r(void)
{
	return 0x00140214;
}
static inline u32 ltc_ltc0_ltss_g_elpg_flush_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc0_ltss_g_elpg_flush_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc0_ltss_g_elpg_flush_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc1_ltss_g_elpg_r(void)
{
	return 0x00142214;
}
static inline u32 ltc_ltc1_ltss_g_elpg_flush_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc1_ltss_g_elpg_flush_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc1_ltss_g_elpg_flush_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltcs_ltss_intr_r(void)
{
	return 0x0017e20c;
}
static inline u32 ltc_ltcs_ltss_intr_ecc_sec_error_pending_f(void)
{
	return 0x100;
}
static inline u32 ltc_ltcs_ltss_intr_ecc_ded_error_pending_f(void)
{
	return 0x200;
}
static inline u32 ltc_ltcs_ltss_intr_en_evicted_cb_m(void)
{
	return 0x1 << 20;
}
static inline u32 ltc_ltcs_ltss_intr_en_illegal_compstat_access_m(void)
{
	return 0x1 << 30;
}
static inline u32 ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f(void)
{
	return 0x1000000;
}
static inline u32 ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f(void)
{
	return 0x2000000;
}
static inline u32 ltc_ltc0_lts0_intr_r(void)
{
	return 0x0014040c;
}
static inline u32 ltc_ltc0_lts0_dstg_ecc_report_r(void)
{
	return 0x0014051c;
}
static inline u32 ltc_ltc0_lts0_dstg_ecc_report_sec_count_m(void)
{
	return 0xff << 0;
}
static inline u32 ltc_ltc0_lts0_dstg_ecc_report_sec_count_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 ltc_ltc0_lts0_dstg_ecc_report_ded_count_m(void)
{
	return 0xff << 16;
}
static inline u32 ltc_ltc0_lts0_dstg_ecc_report_ded_count_v(u32 r)
{
	return (r >> 16) & 0xff;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_r(void)
{
	return 0x0017e2a0;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_v(u32 r)
{
	return (r >> 8) & 0xf;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_v(void)
{
	return 0x00000003;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_f(void)
{
	return 0x300;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_f(void)
{
	return 0x10000000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_v(u32 r)
{
	return (r >> 29) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_f(void)
{
	return 0x20000000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_f(void)
{
	return 0x40000000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_r(void)
{
	return 0x0017e2a4;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_v(u32 r)
{
	return (r >> 8) & 0xf;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_v(void)
{
	return 0x00000003;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_f(void)
{
	return 0x300;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_v(u32 r)
{
	return (r >> 16) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_f(void)
{
	return 0x10000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_f(void)
{
	return 0x10000000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_v(u32 r)
{
	return (r >> 29) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_f(void)
{
	return 0x20000000;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_f(void)
{
	return 0x40000000;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt0_r(void)
{
	return 0x001402a0;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt0_invalidate_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt1_r(void)
{
	return 0x001402a4;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt1_clean_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt0_r(void)
{
	return 0x001422a0;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt0_invalidate_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt0_invalidate_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt0_invalidate_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt1_r(void)
{
	return 0x001422a4;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt1_clean_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt1_clean_pending_v(void)
{
	return 0x00000001;
}
static inline u32 ltc_ltc1_ltss_tstg_cmgmt1_clean_pending_f(void)
{
	return 0x1;
}
static inline u32 ltc_ltc0_lts0_tstg_info_1_r(void)
{
	return 0x0014058c;
}
static inline u32 ltc_ltc0_lts0_tstg_info_1_slice_size_in_kb_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ltc_ltc0_lts0_tstg_info_1_slices_per_l2_v(u32 r)
{
	return (r >> 16) & 0x1f;
}
static inline u32 ltc_ltca_g_axi_pctrl_r(void)
{
	return 0x00160000;
}
static inline u32 ltc_ltca_g_axi_pctrl_user_sid_f(u32 v)
{
	return (v & 0xff) << 2;
}
#endif
