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
#ifndef _hw_psec_gp106_h_
#define _hw_psec_gp106_h_

static inline u32 psec_falcon_irqsset_r(void)
{
	return 0x00087000;
}
static inline u32 psec_falcon_irqsset_swgen0_set_f(void)
{
	return 0x40;
}
static inline u32 psec_falcon_irqsclr_r(void)
{
	return 0x00087004;
}
static inline u32 psec_falcon_irqstat_r(void)
{
	return 0x00087008;
}
static inline u32 psec_falcon_irqstat_halt_true_f(void)
{
	return 0x10;
}
static inline u32 psec_falcon_irqstat_exterr_true_f(void)
{
	return 0x20;
}
static inline u32 psec_falcon_irqstat_swgen0_true_f(void)
{
	return 0x40;
}
static inline u32 psec_falcon_irqmode_r(void)
{
	return 0x0008700c;
}
static inline u32 psec_falcon_irqmset_r(void)
{
	return 0x00087010;
}
static inline u32 psec_falcon_irqmset_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 psec_falcon_irqmset_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 psec_falcon_irqmset_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 psec_falcon_irqmset_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 psec_falcon_irqmset_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 psec_falcon_irqmset_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 psec_falcon_irqmset_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 psec_falcon_irqmset_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 psec_falcon_irqmclr_r(void)
{
	return 0x00087014;
}
static inline u32 psec_falcon_irqmclr_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 psec_falcon_irqmclr_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 psec_falcon_irqmclr_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 psec_falcon_irqmclr_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 psec_falcon_irqmclr_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 psec_falcon_irqmclr_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 psec_falcon_irqmclr_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 psec_falcon_irqmclr_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 psec_falcon_irqmclr_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 psec_falcon_irqmask_r(void)
{
	return 0x00087018;
}
static inline u32 psec_falcon_irqdest_r(void)
{
	return 0x0008701c;
}
static inline u32 psec_falcon_irqdest_host_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 psec_falcon_irqdest_host_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 psec_falcon_irqdest_host_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 psec_falcon_irqdest_host_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 psec_falcon_irqdest_host_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 psec_falcon_irqdest_host_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 psec_falcon_irqdest_host_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 psec_falcon_irqdest_host_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 psec_falcon_irqdest_host_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 psec_falcon_irqdest_target_gptmr_f(u32 v)
{
	return (v & 0x1) << 16;
}
static inline u32 psec_falcon_irqdest_target_wdtmr_f(u32 v)
{
	return (v & 0x1) << 17;
}
static inline u32 psec_falcon_irqdest_target_mthd_f(u32 v)
{
	return (v & 0x1) << 18;
}
static inline u32 psec_falcon_irqdest_target_ctxsw_f(u32 v)
{
	return (v & 0x1) << 19;
}
static inline u32 psec_falcon_irqdest_target_halt_f(u32 v)
{
	return (v & 0x1) << 20;
}
static inline u32 psec_falcon_irqdest_target_exterr_f(u32 v)
{
	return (v & 0x1) << 21;
}
static inline u32 psec_falcon_irqdest_target_swgen0_f(u32 v)
{
	return (v & 0x1) << 22;
}
static inline u32 psec_falcon_irqdest_target_swgen1_f(u32 v)
{
	return (v & 0x1) << 23;
}
static inline u32 psec_falcon_irqdest_target_ext_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 psec_falcon_curctx_r(void)
{
	return 0x00087050;
}
static inline u32 psec_falcon_nxtctx_r(void)
{
	return 0x00087054;
}
static inline u32 psec_falcon_mailbox0_r(void)
{
	return 0x00087040;
}
static inline u32 psec_falcon_mailbox1_r(void)
{
	return 0x00087044;
}
static inline u32 psec_falcon_itfen_r(void)
{
	return 0x00087048;
}
static inline u32 psec_falcon_itfen_ctxen_enable_f(void)
{
	return 0x1;
}
static inline u32 psec_falcon_idlestate_r(void)
{
	return 0x0008704c;
}
static inline u32 psec_falcon_idlestate_falcon_busy_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 psec_falcon_idlestate_ext_busy_v(u32 r)
{
	return (r >> 1) & 0x7fff;
}
static inline u32 psec_falcon_os_r(void)
{
	return 0x00087080;
}
static inline u32 psec_falcon_engctl_r(void)
{
	return 0x000870a4;
}
static inline u32 psec_falcon_cpuctl_r(void)
{
	return 0x00087100;
}
static inline u32 psec_falcon_cpuctl_startcpu_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 psec_falcon_cpuctl_halt_intr_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 psec_falcon_cpuctl_halt_intr_m(void)
{
	return 0x1 << 4;
}
static inline u32 psec_falcon_cpuctl_halt_intr_v(u32 r)
{
	return (r >> 4) & 0x1;
}
static inline u32 psec_falcon_cpuctl_cpuctl_alias_en_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 psec_falcon_cpuctl_cpuctl_alias_en_m(void)
{
	return 0x1 << 6;
}
static inline u32 psec_falcon_cpuctl_cpuctl_alias_en_v(u32 r)
{
	return (r >> 6) & 0x1;
}
static inline u32 psec_falcon_cpuctl_alias_r(void)
{
	return 0x00087130;
}
static inline u32 psec_falcon_cpuctl_alias_startcpu_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 psec_falcon_imemc_r(u32 i)
{
	return 0x00087180 + i*16;
}
static inline u32 psec_falcon_imemc_offs_f(u32 v)
{
	return (v & 0x3f) << 2;
}
static inline u32 psec_falcon_imemc_blk_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 psec_falcon_imemc_aincw_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 psec_falcon_imemd_r(u32 i)
{
	return 0x00087184 + i*16;
}
static inline u32 psec_falcon_imemt_r(u32 i)
{
	return 0x00087188 + i*16;
}
static inline u32 psec_falcon_sctl_r(void)
{
	return 0x00087240;
}
static inline u32 psec_falcon_mmu_phys_sec_r(void)
{
	return 0x00100ce4;
}
static inline u32 psec_falcon_bootvec_r(void)
{
	return 0x00087104;
}
static inline u32 psec_falcon_bootvec_vec_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 psec_falcon_dmactl_r(void)
{
	return 0x0008710c;
}
static inline u32 psec_falcon_dmactl_dmem_scrubbing_m(void)
{
	return 0x1 << 1;
}
static inline u32 psec_falcon_dmactl_imem_scrubbing_m(void)
{
	return 0x1 << 2;
}
static inline u32 psec_falcon_dmactl_require_ctx_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 psec_falcon_hwcfg_r(void)
{
	return 0x00087108;
}
static inline u32 psec_falcon_hwcfg_imem_size_v(u32 r)
{
	return (r >> 0) & 0x1ff;
}
static inline u32 psec_falcon_hwcfg_dmem_size_v(u32 r)
{
	return (r >> 9) & 0x1ff;
}
static inline u32 psec_falcon_dmatrfbase_r(void)
{
	return 0x00087110;
}
static inline u32 psec_falcon_dmatrfbase1_r(void)
{
	return 0x00087128;
}
static inline u32 psec_falcon_dmatrfmoffs_r(void)
{
	return 0x00087114;
}
static inline u32 psec_falcon_dmatrfcmd_r(void)
{
	return 0x00087118;
}
static inline u32 psec_falcon_dmatrfcmd_imem_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 psec_falcon_dmatrfcmd_write_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 psec_falcon_dmatrfcmd_size_f(u32 v)
{
	return (v & 0x7) << 8;
}
static inline u32 psec_falcon_dmatrfcmd_ctxdma_f(u32 v)
{
	return (v & 0x7) << 12;
}
static inline u32 psec_falcon_dmatrffboffs_r(void)
{
	return 0x0008711c;
}
static inline u32 psec_falcon_exterraddr_r(void)
{
	return 0x00087168;
}
static inline u32 psec_falcon_exterrstat_r(void)
{
	return 0x0008716c;
}
static inline u32 psec_falcon_exterrstat_valid_m(void)
{
	return 0x1 << 31;
}
static inline u32 psec_falcon_exterrstat_valid_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 psec_falcon_exterrstat_valid_true_v(void)
{
	return 0x00000001;
}
static inline u32 psec_sec2_falcon_icd_cmd_r(void)
{
	return 0x00087200;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_s(void)
{
	return 4;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_f(u32 v)
{
	return (v & 0xf) << 0;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_m(void)
{
	return 0xf << 0;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_v(u32 r)
{
	return (r >> 0) & 0xf;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_rreg_f(void)
{
	return 0x8;
}
static inline u32 psec_sec2_falcon_icd_cmd_opc_rstat_f(void)
{
	return 0xe;
}
static inline u32 psec_sec2_falcon_icd_cmd_idx_f(u32 v)
{
	return (v & 0x1f) << 8;
}
static inline u32 psec_sec2_falcon_icd_rdata_r(void)
{
	return 0x0008720c;
}
static inline u32 psec_falcon_dmemc_r(u32 i)
{
	return 0x000871c0 + i*8;
}
static inline u32 psec_falcon_dmemc_offs_f(u32 v)
{
	return (v & 0x3f) << 2;
}
static inline u32 psec_falcon_dmemc_offs_m(void)
{
	return 0x3f << 2;
}
static inline u32 psec_falcon_dmemc_blk_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 psec_falcon_dmemc_blk_m(void)
{
	return 0xff << 8;
}
static inline u32 psec_falcon_dmemc_aincw_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 psec_falcon_dmemc_aincr_f(u32 v)
{
	return (v & 0x1) << 25;
}
static inline u32 psec_falcon_dmemd_r(u32 i)
{
	return 0x000871c4 + i*8;
}
static inline u32 psec_falcon_debug1_r(void)
{
	return 0x00087090;
}
static inline u32 psec_falcon_debug1_ctxsw_mode_s(void)
{
	return 1;
}
static inline u32 psec_falcon_debug1_ctxsw_mode_f(u32 v)
{
	return (v & 0x1) << 16;
}
static inline u32 psec_falcon_debug1_ctxsw_mode_m(void)
{
	return 0x1 << 16;
}
static inline u32 psec_falcon_debug1_ctxsw_mode_v(u32 r)
{
	return (r >> 16) & 0x1;
}
static inline u32 psec_falcon_debug1_ctxsw_mode_init_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_transcfg_r(u32 i)
{
	return 0x00087600 + i*4;
}
static inline u32 psec_fbif_transcfg_target_local_fb_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_transcfg_target_coherent_sysmem_f(void)
{
	return 0x1;
}
static inline u32 psec_fbif_transcfg_target_noncoherent_sysmem_f(void)
{
	return 0x2;
}
static inline u32 psec_fbif_transcfg_mem_type_s(void)
{
	return 1;
}
static inline u32 psec_fbif_transcfg_mem_type_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 psec_fbif_transcfg_mem_type_m(void)
{
	return 0x1 << 2;
}
static inline u32 psec_fbif_transcfg_mem_type_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 psec_fbif_transcfg_mem_type_virtual_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_transcfg_mem_type_physical_f(void)
{
	return 0x4;
}
static inline u32 psec_falcon_engine_r(void)
{
	return 0x000873c0;
}
static inline u32 psec_falcon_engine_reset_true_f(void)
{
	return 0x1;
}
static inline u32 psec_falcon_engine_reset_false_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_ctl_r(void)
{
	return 0x00087624;
}
static inline u32 psec_fbif_ctl_allow_phys_no_ctx_init_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_ctl_allow_phys_no_ctx_disallow_f(void)
{
	return 0x0;
}
static inline u32 psec_fbif_ctl_allow_phys_no_ctx_allow_f(void)
{
	return 0x80;
}
#endif
