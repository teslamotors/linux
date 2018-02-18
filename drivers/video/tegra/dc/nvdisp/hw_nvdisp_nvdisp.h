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
#ifndef _hw_nvdisp_nvdisp_h_
#define _hw_nvdisp_nvdisp_h_

static inline u32 nvdisp_display_cmd_option_r(void)
{
	return 0x00000031;
}
static inline u32 nvdisp_display_command_r(void)
{
	return 0x00000032;
}
static inline u32 nvdisp_display_command_control_mode_stop_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_display_command_control_mode_c_display_f(void)
{
	return 0x20;
}
static inline u32 nvdisp_display_command_control_mode_nc_display_f(void)
{
	return 0x40;
}
static inline u32 nvdisp_cmd_int_status_r(void)
{
	return 0x00000037;
}
static inline u32 nvdisp_cmd_int_status_frame_end_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 nvdisp_cmd_int_status_v_blank_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 nvdisp_cmd_int_status_region_crc_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 nvdisp_cmd_int_status_msf_f(u32 v)
{
	return (v & 0x1) << 12;
}
static inline u32 nvdisp_cmd_int_status_uf_f(u32 v)
{
	return (v & 0x1) << 23;
}
static inline u32 nvdisp_cmd_int_status_sd3_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 nvdisp_cmd_int_status_obuf_uf_f(u32 v)
{
	return (v & 0x1) << 26;
}
static inline u32 nvdisp_cmd_int_status_rbuf_uf_f(u32 v)
{
	return (v & 0x1) << 27;
}
static inline u32 nvdisp_cmd_int_status_bbuf_uf_f(u32 v)
{
	return (v & 0x1) << 28;
}
static inline u32 nvdisp_cmd_int_status_dsc_uf_f(u32 v)
{
	return (v & 0x1) << 29;
}
static inline u32 nvdisp_cmd_int_mask_r(void)
{
	return 0x00000038;
}
static inline u32 nvdisp_cmd_int_enable_r(void)
{
	return 0x00000039;
}
static inline u32 nvdisp_int_type_r(void)
{
	return 0x0000003a;
}
static inline u32 nvdisp_int_polarity_r(void)
{
	return 0x0000003b;
}
static inline u32 nvdisp_state_access_r(void)
{
	return 0x00000040;
}
static inline u32 nvdisp_state_access_read_mux_assembly_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_state_access_read_mux_active_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_state_access_write_mux_assembly_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_state_access_write_mux_active_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_cmd_state_ctrl_r(void)
{
	return 0x00000041;
}
static inline u32 nvdisp_cmd_state_ctrl_general_act_req_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_cmd_state_ctrl_a_act_req_enable_f(void)
{
	return 0x2;
}
static inline u32 nvdisp_cmd_state_ctrl_b_act_req_enable_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_cmd_state_ctrl_c_act_req_enable_f(void)
{
	return 0x8;
}
static inline u32 nvdisp_cmd_state_ctrl_d_act_req_enable_f(void)
{
	return 0x10;
}
static inline u32 nvdisp_cmd_state_ctrl_e_act_req_enable_f(void)
{
	return 0x20;
}
static inline u32 nvdisp_cmd_state_ctrl_f_act_req_enable_f(void)
{
	return 0x40;
}
static inline u32 nvdisp_cmd_state_ctrl_cursor_act_req_enable_f(void)
{
	return 0x80;
}
static inline u32 nvdisp_cmd_state_ctrl_general_update_enable_f(void)
{
	return 0x100;
}
static inline u32 nvdisp_cmd_state_ctrl_win_a_update_enable_f(void)
{
	return 0x200;
}
static inline u32 nvdisp_cmd_state_ctrl_win_b_update_enable_f(void)
{
	return 0x400;
}
static inline u32 nvdisp_cmd_state_ctrl_win_c_update_enable_f(void)
{
	return 0x800;
}
static inline u32 nvdisp_cmd_state_ctrl_win_d_update_enable_f(void)
{
	return 0x1000;
}
static inline u32 nvdisp_cmd_state_ctrl_win_e_update_enable_f(void)
{
	return 0x2000;
}
static inline u32 nvdisp_cmd_state_ctrl_win_f_update_enable_f(void)
{
	return 0x4000;
}
static inline u32 nvdisp_cmd_state_ctrl_cursor_update_enable_f(void)
{
	return 0x8000;
}
static inline u32 nvdisp_cmd_state_ctrl_common_act_req_enable_f(void)
{
	return 0x10000;
}
static inline u32 nvdisp_cmd_state_ctrl_common_act_update_enable_f(void)
{
	return 0x20000;
}
static inline u32 nvdisp_cmd_state_ctrl_host_trig_enable_f(void)
{
	return 0x1000000;
}
static inline u32 nvdisp_cmd_state_ctrl_host_trig_secure_v(void)
{
	return 0x00000000;
}
static inline u32 nvdisp_cmd_state_ctrl_gen_act_req_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_cmd_state_ctrl_win_act_req_range_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 nvdisp_cmd_state_ctrl_win_act_req_range_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 nvdisp_cmd_disp_win_hdr_r(void)
{
	return 0x00000042;
}
static inline u32 nvdisp_win_t_state_ctrl_r(void)
{
	return 0x00000044;
}
static inline u32 nvdisp_secure_ctrl_r(void)
{
	return 0x00000045;
}
static inline u32 nvdisp_reg_access_ctrl_r(void)
{
	return 0x00000046;
}
static inline u32 nvdisp_postcomp_capa_r(void)
{
	return 0x00000050;
}
static inline u32 nvdisp_postcomp_capa_is_tz_enable_v(u32 r)
{
	return (r >> 8) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_is_lut_early_v(u32 r)
{
	return (r >> 7) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_lut_type_v(u32 r)
{
	return (r >> 5) & 0x3;
}
static inline u32 nvdisp_postcomp_capa_lut_type_none_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_postcomp_capa_lut_type_s257_f(void)
{
	return 0x20;
}
static inline u32 nvdisp_postcomp_capa_lut_type_s1025_f(void)
{
	return 0x40;
}
static inline u32 nvdisp_postcomp_capa_is_ocsc_enable_v(u32 r)
{
	return (r >> 3) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_is_hsat_enable_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_is_yuv422_enable_v(u32 r)
{
	return (r >> 4) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_is_scaler_enable_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 nvdisp_postcomp_capa_is_scaler_has_yuv422_enable_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 nvdisp_ihub_capa_r(void)
{
	return 0x00000060;
}
static inline u32 nvdisp_ihub_capa_mempool_entries_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 nvdisp_ihub_capa_mempool_width_v(u32 r)
{
	return (r >> 16) & 0x3;
}
static inline u32 nvdisp_ihub_capa_rotation_support_v(u32 r)
{
	return (r >> 18) & 0x1;
}
static inline u32 nvdisp_ihub_capa_planar_support_v(u32 r)
{
	return (r >> 19) & 0x1;
}
static inline u32 nvdisp_ihub_capa_vga_support_v(u32 r)
{
	return (r >> 20) & 0x1;
}
static inline u32 nvdisp_ihub_capa_mempool_compression_v(u32 r)
{
	return (r >> 21) & 0x1;
}
static inline u32 nvdisp_ihub_capa_mspg_support_v(u32 r)
{
	return (r >> 22) & 0x1;
}
static inline u32 nvdisp_ihub_capa_mclk_switch_support_v(u32 r)
{
	return (r >> 23) & 0x1;
}
static inline u32 nvdisp_ihub_capa_asr_support_v(u32 r)
{
	return (r >> 24) & 0x1;
}
static inline u32 nvdisp_ihub_capa_latency_support_v(u32 r)
{
	return (r >> 26) & 0x1;
}
static inline u32 nvdisp_ihub_capa_tz_window_support_v(u32 r)
{
	return (r >> 27) & 0x1;
}
static inline u32 nvdisp_ihub_capa_size_per_line_rot_v(u32 r)
{
	return (r >> 28) & 0x3;
}
static inline u32 nvdisp_ihub_capa_size_per_line_nonrot_v(u32 r)
{
	return (r >> 30) & 0x3;
}
static inline u32 nvdisp_ihub_capb_r(void)
{
	return 0x00000061;
}
static inline u32 nvdisp_ihub_capb_max_planar_threadgrps_v(u32 r)
{
	return (r >> 0) & 0x3f;
}
static inline u32 nvdisp_ihub_capb_max_semi_planar_threadgrps_v(u32 r)
{
	return (r >> 6) & 0x3f;
}
static inline u32 nvdisp_ihub_capb_max_packed_2bpp_threadgrps_v(u32 r)
{
	return (r >> 12) & 0x3f;
}
static inline u32 nvdisp_ihub_capb_max_packed_1bpp_threadgrps_v(u32 r)
{
	return (r >> 18) & 0x3f;
}
static inline u32 nvdisp_ihub_capb_max_packed_422_threadgrps_v(u32 r)
{
	return (r >> 24) & 0x3f;
}
static inline u32 nvdisp_ihub_capc_r(void)
{
	return 0x00000062;
}
static inline u32 nvdisp_ihub_capc_clear_rect_v(u32 r)
{
	return (r >> 8) & 0x7;
}
static inline u32 nvdisp_ihub_capc_max_lines_buffered_v(u32 r)
{
	return (r >> 4) & 0x7;
}
static inline u32 nvdisp_ihub_capc_pitch_size_v(u32 r)
{
	return (r >> 0) & 0x3;
}
static inline u32 nvdisp_ihub_capd_r(void)
{
	return 0x00000063;
}
static inline u32 nvdisp_ihub_capd_rdout_bufsize_v(u32 r)
{
	return (r >> 16) & 0xffff;
}
static inline u32 nvdisp_ihub_capd_reorder_buf_depth_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 nvdisp_ihub_config_r(void)
{
	return 0x00000067;
}
static inline u32 nvdisp_ihub_config_batch_size_v(u32 r)
{
	return (r >> 0) & 0x7;
}
static inline u32 nvdisp_ihub_misc_ctl_r(void)
{
	return 0x00000068;
}
static inline u32 nvdisp_ihub_misc_ctl_fetch_meter_enable_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_ihub_misc_ctl_req_limit_enable_f(void)
{
	return 0x40000000;
}
static inline u32 nvdisp_ihub_misc_ctl_no_compress_enable_f(void)
{
	return 0x10000;
}
static inline u32 nvdisp_ihub_misc_ctl_latency_event_enable_f(void)
{
	return 0x8;
}
static inline u32 nvdisp_ihub_misc_ctl_critical_enable_f(void)
{
	return 0x20;
}
static inline u32 nvdisp_ihub_misc_ctl_mspg_enable_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_ihub_misc_ctl_switch_enable_f(void)
{
	return 0x2;
}
static inline u32 nvdisp_ihub_misc_ctl_asr_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_ihub_misc_emergency_r(void)
{
	return 0x00000069;
}
static inline u32 nvdisp_ihub_misc_emergency_is_stopped_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 nvdisp_ihub_misc_emergency_stop_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_streamid_r(void)
{
	return 0x000000a0;
}
static inline u32 nvdisp_crc_control_r(void)
{
	return 0x00000300;
}
static inline u32 nvdisp_crc_control_enable_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_crc_control_enable_diasble_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_crc_control_input_data_full_frame_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_crc_control_input_data_active_data_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_head_crc_control_r(void)
{
	return 0x00000301;
}
static inline u32 nvdisp_comp_crca_r(void)
{
	return 0x0000032a;
}
static inline u32 nvdisp_comp_crcb_r(void)
{
	return 0x0000032b;
}
static inline u32 nvdisp_rg_crca_r(void)
{
	return 0x0000032c;
}
static inline u32 nvdisp_rg_crca_valid_false_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_rg_crca_valid_true_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_rg_crca_error_false_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_rg_crca_error_true_f(void)
{
	return 0x2;
}
static inline u32 nvdisp_rg_crcb_r(void)
{
	return 0x0000032d;
}
static inline u32 nvdisp_head_loadv_cntr_r(void)
{
	return 0x0000035f;
}
static inline u32 nvdisp_rg_status_r(void)
{
	return 0x00000362;
}
static inline u32 nvdisp_rg_dclk_r(void)
{
	return 0x00000364;
}
static inline u32 nvdisp_rg_underflow_r(void)
{
	return 0x00000365;
}
static inline u32 nvdisp_rg_underflow_enable_enable_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_rg_underflow_enable_disable_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_rg_underflow_uflowed_no_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_rg_underflow_uflowed_yes_f(void)
{
	return 0x10;
}
static inline u32 nvdisp_rg_underflow_uflowed_clr_f(void)
{
	return 0x10;
}
static inline u32 nvdisp_rg_underflow_mode_repeat_f(void)
{
	return 0x100;
}
static inline u32 nvdisp_rg_underflow_mode_red_f(void)
{
	return 0x100;
}
static inline u32 nvdisp_rg_underflow_frames_uflowed_v(u32 r)
{
	return (r >> 16) & 0xff;
}
static inline u32 nvdisp_rg_underflow_is_frames_uflowed_rst_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 nvdisp_rg_underflow_frames_uflowed_rst_done_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_rg_underflow_frames_uflowed_rst_pending_f(void)
{
	return 0x1000000;
}
static inline u32 nvdisp_rg_underflow_frames_uflowed_rst_trigger_f(void)
{
	return 0x1000000;
}
static inline u32 nvdisp_rg_dpca_r(void)
{
	return 0x00000366;
}
static inline u32 nvdisp_rg_underflow_pixel_r(void)
{
	return 0x0000036d;
}
static inline u32 nvdisp_rg_region_crc_r(void)
{
	return 0x000003a2;
}
static inline u32 nvdisp_rg_region_crc_ctl_r(void)
{
	return 0x000003a3;
}
static inline u32 nvdisp_disp_signal_option_r(void)
{
	return 0x00000400;
}
static inline u32 nvdisp_win_options_r(void)
{
	return 0x00000402;
}
static inline u32 nvdisp_win_options_cursor_is_enable_v(u32 r)
{
	return (r >> 16) & 0x1;
}
static inline u32 nvdisp_win_options_cursor_set_cursor_enable_f(void)
{
	return 0x10000;
}
static inline u32 nvdisp_win_options_sor_is_enable_v(u32 r)
{
	return (r >> 25) & 0x1;
}
static inline u32 nvdisp_win_options_sor_set_sor_enable_f(void)
{
	return 0x2000000;
}
static inline u32 nvdisp_win_options_sor1_is_enable_v(u32 r)
{
	return (r >> 26) & 0x1;
}
static inline u32 nvdisp_win_options_sor1_set_sor1_enable_f(void)
{
	return 0x4000000;
}
static inline u32 nvdisp_win_options_dsi_is_enable_v(u32 r)
{
	return (r >> 29) & 0x1;
}
static inline u32 nvdisp_win_options_dsi_set_dsi_enable_f(void)
{
	return 0x20000000;
}
static inline u32 nvdisp_sor_control_r(void)
{
	return 0x00000403;
}
static inline u32 nvdisp_sor_control_protocol_dpa_f(void)
{
	return 0x600;
}
static inline u32 nvdisp_sor_control_protocol_dpb_f(void)
{
	return 0x700;
}
static inline u32 nvdisp_sor_control_protocol_custom_f(void)
{
	return 0xf00;
}
static inline u32 nvdisp_sor1_control_r(void)
{
	return 0x00000404;
}
static inline u32 nvdisp_sor1_control_protocol_dpa_f(void)
{
	return 0x600;
}
static inline u32 nvdisp_sor1_control_protocol_tmdsa_f(void)
{
	return 0x100;
}
static inline u32 nvdisp_dsi_control_r(void)
{
	return 0x00000405;
}
static inline u32 nvdisp_dsi_control_protocol_dsia_f(void)
{
	return 0x800;
}
static inline u32 nvdisp_sync_width_r(void)
{
	return 0x00000407;
}
static inline u32 nvdisp_sync_width_h_f(u32 v)
{
	return (v & 0x7fff) << 0;
}
static inline u32 nvdisp_sync_width_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_back_porch_r(void)
{
	return 0x00000408;
}
static inline u32 nvdisp_back_porch_h_f(u32 v)
{
	return (v & 0x7fff) << 0;
}
static inline u32 nvdisp_back_porch_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_active_r(void)
{
	return 0x00000409;
}
static inline u32 nvdisp_active_h_f(u32 v)
{
	return (v & 0x7fff) << 0;
}
static inline u32 nvdisp_active_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_front_porch_r(void)
{
	return 0x0000040a;
}
static inline u32 nvdisp_front_porch_h_f(u32 v)
{
	return (v & 0x7fff) << 0;
}
static inline u32 nvdisp_front_porch_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_rg_ext_back_porch_r(void)
{
	return 0x00000410;
}
static inline u32 nvdisp_rg_ext_front_porch_r(void)
{
	return 0x00000411;
}
static inline u32 nvdisp_rg_ext_r(void)
{
	return 0x00000412;
}
static inline u32 nvdisp_color_ctl_r(void)
{
	return 0x00000430;
}
static inline u32 nvdisp_color_ctl_cmu_enable_f(void)
{
	return 0x100000;
}
static inline u32 nvdisp_color_ctl_cmu_disable_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_color_ctl_dither_disable_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_color_ctl_dither_enable_f(void)
{
	return 0x80000;
}
static inline u32 nvdisp_color_ctl_ord_dither_rot_f(u32 v)
{
	return (v & 0x3) << 12;
}
static inline u32 nvdisp_color_ctl_dither_ctl_disable_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_color_ctl_dither_ctl_err_acc_f(void)
{
	return 0x100;
}
static inline u32 nvdisp_color_ctl_dither_ctl_ordered_f(void)
{
	return 0x200;
}
static inline u32 nvdisp_color_ctl_dither_ctl_temporal_f(void)
{
	return 0x300;
}
static inline u32 nvdisp_color_ctl_dither_phase_f(u32 v)
{
	return (v & 0x3) << 6;
}
static inline u32 nvdisp_color_ctl_base_color_size_18bits_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_color_ctl_base_color_size_24bits_f(void)
{
	return 0x8;
}
static inline u32 nvdisp_color_ctl_base_color_size_30bits_f(void)
{
	return 0xa;
}
static inline u32 nvdisp_color_ctl_base_color_size_36bits_f(void)
{
	return 0xc;
}
static inline u32 nvdisp_output_lut_ctl_r(void)
{
	return 0x00000431;
}
static inline u32 nvdisp_output_lut_ctl_mode_f(u32 v)
{
	return (v & 0x3) << 5;
}
static inline u32 nvdisp_output_lut_ctl_range_f(u32 v)
{
	return (v & 0x3) << 3;
}
static inline u32 nvdisp_output_lut_ctl_size_v(u32 r)
{
	return (r >> 1) & 0x3;
}
static inline u32 nvdisp_output_lut_ctl_size_257_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_output_lut_ctl_size_1025_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_output_lut_base_r(void)
{
	return 0x00000432;
}
static inline u32 nvdisp_output_lut_base_hi_r(void)
{
	return 0x00000433;
}
static inline u32 nvdisp_procamp_r(void)
{
	return 0x00000434;
}
static inline u32 nvdisp_procamp_sat_sine_f(u32 v)
{
	return (v & 0xfff) << 15;
}
static inline u32 nvdisp_procamp_sat_cos_f(u32 v)
{
	return (v & 0xfff) << 3;
}
static inline u32 nvdisp_procamp_chroma_lpf_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 nvdisp_procamp_chroma_lpf_enable_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_cursor_ctrl_r(void)
{
	return 0x0000043b;
}
static inline u32 nvdisp_display_rate_r(void)
{
	return 0x00000415;
}
static inline u32 nvdisp_display_rate_min_refresh_enable_f(u32 v)
{
	return (v & 0x1) << 23;
}
static inline u32 nvdisp_display_rate_min_refresh_interval_f(u32 v)
{
	return (v & 0x3fffff) << 1;
}
static inline u32 nvdisp_cursor_startaddr_r(void)
{
	return 0x0000043e;
}
static inline u32 nvdisp_cursor_startaddr_range_v(u32 r)
{
	return (r >> 0) & 0x3fffff;
}
static inline u32 nvdisp_cursor_startaddr_get_size_v(u32 r)
{
	return (r >> 24) & 0x3;
}
static inline u32 nvdisp_cursor_startaddr_get_size_size_32x32_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_cursor_startaddr_get_size_size_64x64_f(void)
{
	return 0x1000000;
}
static inline u32 nvdisp_cursor_startaddr_get_size_size_128x128_f(void)
{
	return 0x2000000;
}
static inline u32 nvdisp_cursor_startaddr_get_size_size_256x256_f(void)
{
	return 0x3000000;
}
static inline u32 nvdisp_cursor_position_r(void)
{
	return 0x00000440;
}
static inline u32 nvdisp_cursor_position_h_range_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 nvdisp_cursor_position_v_range_v(u32 r)
{
	return (r >> 16) & 0xffff;
}
static inline u32 nvdisp_cursor_cropped_point_in_r(void)
{
	return 0x00000442;
}
static inline u32 nvdisp_cursor_cropped_point_in_x_range_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 nvdisp_cursor_cropped_point_in_y_range_v(u32 r)
{
	return (r >> 16) & 0xffff;
}
static inline u32 nvdisp_cursor_cropped_size_in_r(void)
{
	return 0x00000446;
}
static inline u32 nvdisp_cursor_cropped_size_in_width_range_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 nvdisp_cursor_cropped_size_in_height_range_v(u32 r)
{
	return (r >> 16) & 0xffff;
}
static inline u32 nvdisp_cursor_pipe_meter_r(void)
{
	return 0x00000450;
}
static inline u32 nvdisp_cursor_pipe_meter_status_f(u32 v)
{
	return (v & 0x3) << 30;
}
static inline u32 nvdisp_cursor_pipe_meter_status_active_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_cursor_pipe_meter_status_armed_f(void)
{
	return 0x40000000;
}
static inline u32 nvdisp_cursor_pipe_meter_status_assembly_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_cursor_pipe_meter_val_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_ihub_common_fetch_meter_r(void)
{
	return 0x00000451;
}
static inline u32 nvdisp_ihub_common_fetch_meter_cursor_slots_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 nvdisp_ihub_common_fetch_meter_wgrp_slots_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 nvdisp_ihub_cursor_pool_config_r(void)
{
	return 0x00000452;
}
static inline u32 nvdisp_ihub_cursor_pool_config_status_pending_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 nvdisp_ihub_cursor_pool_config_status_pending_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_ihub_cursor_pool_config_entries_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_ihub_cursor_fetch_meter_r(void)
{
	return 0x00000453;
}
static inline u32 nvdisp_ihub_cursor_fetch_meter_slots_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 nvdisp_sd_hist_ctrl_r(void)
{
	return 0x000004c2;
}
static inline u32 nvdisp_sd_hist_ctrl_is_enable_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 nvdisp_sd_hist_ctrl_set_enable_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_sd_hist_ctrl_is_reset_busy_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 nvdisp_sd_hist_ctrl_reset_start_f(void)
{
	return 0x40000000;
}
static inline u32 nvdisp_sd_hist_ctrl_is_ram_busy_v(u32 r)
{
	return (r >> 29) & 0x1;
}
static inline u32 nvdisp_sd_hist_ctrl_is_vid_luma_enable_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 nvdisp_sd_hist_ctrl_set_vid_luma_enable_f(void)
{
	return 0x10000000;
}
static inline u32 nvdisp_sd_hist_ctrl_is_win_enable_v(u32 r)
{
	return (r >> 27) & 0x1;
}
static inline u32 nvdisp_sd_hist_ctrl_set_window_enable_f(void)
{
	return 0x8000000;
}
static inline u32 nvdisp_sd_hist_ctrl_max_pixel_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
static inline u32 nvdisp_sd_hist_luma_r(void)
{
	return 0x000004c3;
}
static inline u32 nvdisp_sd_hist_luma_is_valid_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 nvdisp_sd_hist_luma_num_pixels_v(u32 r)
{
	return (r >> 0) & 0x3ffffff;
}
static inline u32 nvdisp_sd_hist_over_sat_r(void)
{
	return 0x000004c4;
}
static inline u32 nvdisp_sd_hist_over_sat_bin_num_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 nvdisp_sd_hist_int_bounds_r(void)
{
	return 0x000004c5;
}
static inline u32 nvdisp_sd_hist_int_bounds_is_enable_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 nvdisp_sd_hist_int_bounds_set_enable_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_sd_hist_int_bounds_upper_f(u32 v)
{
	return (v & 0xff) << 16;
}
static inline u32 nvdisp_sd_hist_int_bounds_lower_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 nvdisp_sd_hist_vid_luma_r(void)
{
	return 0x000004c6;
}
static inline u32 nvdisp_sd_hist_vid_luma_b_coeff_f(u32 v)
{
	return (v & 0xf) << 8;
}
static inline u32 nvdisp_sd_hist_vid_luma_g_coeff_f(u32 v)
{
	return (v & 0xf) << 4;
}
static inline u32 nvdisp_sd_hist_vid_luma_r_coeff_f(u32 v)
{
	return (v & 0xf) << 0;
}
static inline u32 nvdisp_sd_gain_ctrl_r(void)
{
	return 0x000004c7;
}
static inline u32 nvdisp_sd_gain_ctrl_is_enable_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 nvdisp_sd_gain_ctrl_set_enable_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_sd_gain_ctrl_update_busy_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 nvdisp_sd_gain_ctrl_update_start_f(void)
{
	return 0x40000000;
}
static inline u32 nvdisp_sd_gain_ctrl_timing_f(u32 v)
{
	return (v & 0x1) << 29;
}
static inline u32 nvdisp_sd_gain_ctrl_reset_busy_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 nvdisp_sd_gain_ctrl_reset_start_f(void)
{
	return 0x10000000;
}
static inline u32 nvdisp_sd_gain_rg_r(void)
{
	return 0x000004c8;
}
static inline u32 nvdisp_sd_gain_rg_gint_f(u32 v)
{
	return (v & 0x3) << 30;
}
static inline u32 nvdisp_sd_gain_rg_gfrac_f(u32 v)
{
	return (v & 0xfff) << 18;
}
static inline u32 nvdisp_sd_gain_rg_gval_f(u32 v)
{
	return (v & 0xffff) << 16;
}
static inline u32 nvdisp_sd_gain_rg_rint_f(u32 v)
{
	return (v & 0x3) << 14;
}
static inline u32 nvdisp_sd_gain_rg_rfrac_f(u32 v)
{
	return (v & 0xfff) << 2;
}
static inline u32 nvdisp_sd_gain_rg_rval_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_sd_gain_b_r(void)
{
	return 0x000004c9;
}
static inline u32 nvdisp_sd_gain_b_int_f(u32 v)
{
	return (v & 0x3) << 14;
}
static inline u32 nvdisp_sd_gain_b_frac_f(u32 v)
{
	return (v & 0xfff) << 2;
}
static inline u32 nvdisp_sd_gain_b_val_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_sd_hist_win_pos_r(void)
{
	return 0x000004cb;
}
static inline u32 nvdisp_sd_hist_win_pos_v_f(u32 v)
{
	return (v & 0xffff) << 16;
}
static inline u32 nvdisp_sd_hist_win_pos_h_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_sd_hist_win_size_r(void)
{
	return 0x000004cc;
}
static inline u32 nvdisp_sd_hist_win_size_height_f(u32 v)
{
	return (v & 0xffff) << 16;
}
static inline u32 nvdisp_sd_hist_win_size_width_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 nvdisp_incr_syncpt_cntrl_r(void)
{
	return 0x00000001;
}
static inline u32 nvdisp_incr_syncpt_cntrl_no_stall_f(u32 v)
{
	return (v & 0x1) << 8;
}
static inline u32 nvdisp_cont_syncpt_vsync_r(void)
{
	return 0x00000028;
}
static inline u32 nvdisp_cont_syncpt_vsync_indx_f(u32 v)
{
	return (v & 0x3ff) << 0;
}
static inline u32 nvdisp_cont_syncpt_vsync_en_enable_f(void)
{
	return 0x80000000;
}
static inline u32 nvdisp_background_color_r(void)
{
	return 0x000004e4;
}
static inline u32 nvdisp_interlace_ctl_r(void)
{
	return 0x000004e5;
}
static inline u32 nvdisp_interlace_fld2_width_r(void)
{
	return 0x000004e7;
}
static inline u32 nvdisp_interlace_fld2_width_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_interlace_fld2_bporch_r(void)
{
	return 0x000004e8;
}
static inline u32 nvdisp_interlace_fld2_bporch_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_interlace_fld2_fporch_r(void)
{
	return 0x000004e9;
}
static inline u32 nvdisp_interlace_fld2_fporch_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_interlace_fld2_active_r(void)
{
	return 0x000004ea;
}
static inline u32 nvdisp_interlace_fld2_active_v_f(u32 v)
{
	return (v & 0x7fff) << 16;
}
static inline u32 nvdisp_cursor_startaddr_hi_r(void)
{
	return 0x000004ec;
}
static inline u32 nvdisp_cursor_startaddr_hi_range_sw_default_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_csc2_control_r(void)
{
	return 0x000004ef;
}
static inline u32 nvdisp_csc2_control_limit_rgb_enable_f(void)
{
	return 0x4;
}
static inline u32 nvdisp_csc2_control_limit_rgb_disable_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_csc2_control_output_color_sel_rgb_f(void)
{
	return 0x0;
}
static inline u32 nvdisp_csc2_control_output_color_sel_y709_f(void)
{
	return 0x1;
}
static inline u32 nvdisp_csc2_control_output_color_sel_y601_f(void)
{
	return 0x2;
}
static inline u32 nvdisp_csc2_control_output_color_sel_y2020_f(void)
{
	return 0x3;
}
static inline u32 nvdisp_blend_cursor_ctrl_r(void)
{
	return 0x000004f1;
}
#endif
