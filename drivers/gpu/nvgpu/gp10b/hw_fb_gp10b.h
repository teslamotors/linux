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
#ifndef _hw_fb_gp10b_h_
#define _hw_fb_gp10b_h_

static inline u32 fb_fbhub_num_active_ltcs_r(void)
{
	return 0x00100800;
}
static inline u32 fb_mmu_ctrl_r(void)
{
	return 0x00100c80;
}
static inline u32 fb_mmu_ctrl_vm_pg_size_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 fb_mmu_ctrl_vm_pg_size_128kb_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_ctrl_vm_pg_size_64kb_f(void)
{
	return 0x1;
}
static inline u32 fb_mmu_ctrl_pri_fifo_empty_v(u32 r)
{
	return (r >> 15) & 0x1;
}
static inline u32 fb_mmu_ctrl_pri_fifo_empty_false_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_ctrl_pri_fifo_space_v(u32 r)
{
	return (r >> 16) & 0xff;
}
static inline u32 fb_mmu_ctrl_use_pdb_big_page_size_v(u32 r)
{
	return (r >> 11) & 0x1;
}
static inline u32 fb_mmu_ctrl_use_pdb_big_page_size_true_f(void)
{
	return 0x800;
}
static inline u32 fb_mmu_ctrl_use_pdb_big_page_size_false_f(void)
{
	return 0x0;
}
static inline u32 fb_priv_mmu_phy_secure_r(void)
{
	return 0x00100ce4;
}
static inline u32 fb_mmu_invalidate_pdb_r(void)
{
	return 0x00100cb8;
}
static inline u32 fb_mmu_invalidate_pdb_aperture_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_invalidate_pdb_aperture_sys_mem_f(void)
{
	return 0x2;
}
static inline u32 fb_mmu_invalidate_pdb_addr_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 fb_mmu_invalidate_r(void)
{
	return 0x00100cbc;
}
static inline u32 fb_mmu_invalidate_all_va_true_f(void)
{
	return 0x1;
}
static inline u32 fb_mmu_invalidate_all_pdb_true_f(void)
{
	return 0x2;
}
static inline u32 fb_mmu_invalidate_hubtlb_only_s(void)
{
	return 1;
}
static inline u32 fb_mmu_invalidate_hubtlb_only_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 fb_mmu_invalidate_hubtlb_only_m(void)
{
	return 0x1 << 2;
}
static inline u32 fb_mmu_invalidate_hubtlb_only_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 fb_mmu_invalidate_hubtlb_only_true_f(void)
{
	return 0x4;
}
static inline u32 fb_mmu_invalidate_replay_s(void)
{
	return 3;
}
static inline u32 fb_mmu_invalidate_replay_f(u32 v)
{
	return (v & 0x7) << 3;
}
static inline u32 fb_mmu_invalidate_replay_m(void)
{
	return 0x7 << 3;
}
static inline u32 fb_mmu_invalidate_replay_v(u32 r)
{
	return (r >> 3) & 0x7;
}
static inline u32 fb_mmu_invalidate_replay_none_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_invalidate_replay_start_f(void)
{
	return 0x8;
}
static inline u32 fb_mmu_invalidate_replay_start_ack_all_f(void)
{
	return 0x10;
}
static inline u32 fb_mmu_invalidate_replay_cancel_targeted_f(void)
{
	return 0x18;
}
static inline u32 fb_mmu_invalidate_replay_cancel_global_f(void)
{
	return 0x20;
}
static inline u32 fb_mmu_invalidate_replay_cancel_f(void)
{
	return 0x20;
}
static inline u32 fb_mmu_invalidate_sys_membar_s(void)
{
	return 1;
}
static inline u32 fb_mmu_invalidate_sys_membar_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 fb_mmu_invalidate_sys_membar_m(void)
{
	return 0x1 << 6;
}
static inline u32 fb_mmu_invalidate_sys_membar_v(u32 r)
{
	return (r >> 6) & 0x1;
}
static inline u32 fb_mmu_invalidate_sys_membar_true_f(void)
{
	return 0x40;
}
static inline u32 fb_mmu_invalidate_ack_s(void)
{
	return 2;
}
static inline u32 fb_mmu_invalidate_ack_f(u32 v)
{
	return (v & 0x3) << 7;
}
static inline u32 fb_mmu_invalidate_ack_m(void)
{
	return 0x3 << 7;
}
static inline u32 fb_mmu_invalidate_ack_v(u32 r)
{
	return (r >> 7) & 0x3;
}
static inline u32 fb_mmu_invalidate_ack_ack_none_required_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_invalidate_ack_ack_intranode_f(void)
{
	return 0x100;
}
static inline u32 fb_mmu_invalidate_ack_ack_globally_f(void)
{
	return 0x80;
}
static inline u32 fb_mmu_invalidate_cancel_client_id_s(void)
{
	return 6;
}
static inline u32 fb_mmu_invalidate_cancel_client_id_f(u32 v)
{
	return (v & 0x3f) << 9;
}
static inline u32 fb_mmu_invalidate_cancel_client_id_m(void)
{
	return 0x3f << 9;
}
static inline u32 fb_mmu_invalidate_cancel_client_id_v(u32 r)
{
	return (r >> 9) & 0x3f;
}
static inline u32 fb_mmu_invalidate_cancel_gpc_id_s(void)
{
	return 5;
}
static inline u32 fb_mmu_invalidate_cancel_gpc_id_f(u32 v)
{
	return (v & 0x1f) << 15;
}
static inline u32 fb_mmu_invalidate_cancel_gpc_id_m(void)
{
	return 0x1f << 15;
}
static inline u32 fb_mmu_invalidate_cancel_gpc_id_v(u32 r)
{
	return (r >> 15) & 0x1f;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_s(void)
{
	return 1;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_f(u32 v)
{
	return (v & 0x1) << 20;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_m(void)
{
	return 0x1 << 20;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_v(u32 r)
{
	return (r >> 20) & 0x1;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_gpc_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_invalidate_cancel_client_type_hub_f(void)
{
	return 0x100000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_s(void)
{
	return 3;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_f(u32 v)
{
	return (v & 0x7) << 24;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_m(void)
{
	return 0x7 << 24;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_v(u32 r)
{
	return (r >> 24) & 0x7;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_all_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_pte_only_f(void)
{
	return 0x1000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde0_f(void)
{
	return 0x2000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde1_f(void)
{
	return 0x3000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde2_f(void)
{
	return 0x4000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde3_f(void)
{
	return 0x5000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde4_f(void)
{
	return 0x6000000;
}
static inline u32 fb_mmu_invalidate_cancel_cache_level_up_to_pde5_f(void)
{
	return 0x7000000;
}
static inline u32 fb_mmu_invalidate_trigger_s(void)
{
	return 1;
}
static inline u32 fb_mmu_invalidate_trigger_f(u32 v)
{
	return (v & 0x1) << 31;
}
static inline u32 fb_mmu_invalidate_trigger_m(void)
{
	return 0x1 << 31;
}
static inline u32 fb_mmu_invalidate_trigger_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 fb_mmu_invalidate_trigger_true_f(void)
{
	return 0x80000000;
}
static inline u32 fb_mmu_debug_wr_r(void)
{
	return 0x00100cc8;
}
static inline u32 fb_mmu_debug_wr_aperture_s(void)
{
	return 2;
}
static inline u32 fb_mmu_debug_wr_aperture_f(u32 v)
{
	return (v & 0x3) << 0;
}
static inline u32 fb_mmu_debug_wr_aperture_m(void)
{
	return 0x3 << 0;
}
static inline u32 fb_mmu_debug_wr_aperture_v(u32 r)
{
	return (r >> 0) & 0x3;
}
static inline u32 fb_mmu_debug_wr_aperture_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_debug_wr_aperture_sys_mem_coh_f(void)
{
	return 0x2;
}
static inline u32 fb_mmu_debug_wr_aperture_sys_mem_ncoh_f(void)
{
	return 0x3;
}
static inline u32 fb_mmu_debug_wr_vol_false_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_debug_wr_vol_true_v(void)
{
	return 0x00000001;
}
static inline u32 fb_mmu_debug_wr_vol_true_f(void)
{
	return 0x4;
}
static inline u32 fb_mmu_debug_wr_addr_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 fb_mmu_debug_wr_addr_alignment_v(void)
{
	return 0x0000000c;
}
static inline u32 fb_mmu_debug_rd_r(void)
{
	return 0x00100ccc;
}
static inline u32 fb_mmu_debug_rd_aperture_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_debug_rd_aperture_sys_mem_coh_f(void)
{
	return 0x2;
}
static inline u32 fb_mmu_debug_rd_aperture_sys_mem_ncoh_f(void)
{
	return 0x3;
}
static inline u32 fb_mmu_debug_rd_vol_false_f(void)
{
	return 0x0;
}
static inline u32 fb_mmu_debug_rd_addr_f(u32 v)
{
	return (v & 0xfffffff) << 4;
}
static inline u32 fb_mmu_debug_rd_addr_alignment_v(void)
{
	return 0x0000000c;
}
static inline u32 fb_mmu_debug_ctrl_r(void)
{
	return 0x00100cc4;
}
static inline u32 fb_mmu_debug_ctrl_debug_v(u32 r)
{
	return (r >> 16) & 0x1;
}
static inline u32 fb_mmu_debug_ctrl_debug_m(void)
{
	return 0x1 << 16;
}
static inline u32 fb_mmu_debug_ctrl_debug_enabled_v(void)
{
	return 0x00000001;
}
static inline u32 fb_mmu_debug_ctrl_debug_disabled_v(void)
{
	return 0x00000000;
}
static inline u32 fb_mmu_vpr_info_r(void)
{
	return 0x00100cd0;
}
static inline u32 fb_mmu_vpr_info_fetch_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 fb_mmu_vpr_info_fetch_false_v(void)
{
	return 0x00000000;
}
static inline u32 fb_mmu_vpr_info_fetch_true_v(void)
{
	return 0x00000001;
}
static inline u32 fb_niso_flush_sysmem_addr_r(void)
{
	return 0x00100c10;
}
#endif
