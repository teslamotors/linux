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
 *
 */

 /*
  * Function naming determines intended use:
  *
  *     <x>_r(void) : Returns the offset for register <x>.
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

#ifndef HOST1X_HW_HOST1X05_SYNC_H
#define HOST1X_HW_HOST1X05_SYNC_H

#define REGISTER_STRIDE	4

static inline u32 host1x_sync_intstatus_r(void)
{
	return 0x1c;
}
#define HOST1X_SYNC_INTSTATUS \
	host1x_sync_intstatus_r()
static inline u32 host1x_sync_intmask_r(void)
{
	return 0x30;
}
#define HOST1X_SYNC_INTMASK \
	host1x_sync_intmask_r()
static inline u32 host1x_sync_intc0mask_r(void)
{
	return 0x4;
}
#define HOST1X_SYNC_INTC0MASK \
	host1x_sync_intc0mask_r()
static inline u32 host1x_sync_intgmask_r(void)
{
	return 0x44;
}
#define HOST1X_SYNC_INTGMASK \
	host1x_sync_intgmask_r()
static inline u32 host1x_sync_syncpt_intgmask_r(void)
{
	return 0x50;
}
#define HOST1X_SYNC_SYNCPT_INTGMASK \
	host1x_sync_syncpt_intgmask_r()
static inline u32 host1x_sync_intstatus_ip_read_int_v(u32 r)
{
	return (r >> 0) & 0x1;
}
#define HOST1X_SYNC_INTSTATUS_IP_READ_INT_V(r) \
	host1x_sync_intstatus_ip_read_int_v(r)
static inline u32 host1x_sync_intstatus_ip_write_int_v(u32 r)
{
	return (r >> 1) & 0x1;
}
#define HOST1X_SYNC_INTSTATUS_IP_WRITE_INT_V(r) \
	host1x_sync_intstatus_ip_write_int_v(r)
static inline u32 host1x_sync_intstatus_illegal_pb_access_v(u32 r)
{
	return (r >> 28) & 0x1;
}
#define HOST1X_SYNC_INTSTATUS_ILLEGAL_PB_ACCESS_V(r) \
	host1x_sync_intstatus_illegal_pb_access_v(r)
static inline u32 host1x_sync_illegal_syncpt_access_frm_pb_r(void)
{
	return 0x2270;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_PB \
	host1x_sync_illegal_syncpt_access_frm_pb_r()
static inline u32 host1x_sync_illegal_syncpt_access_frm_pb_syncpt_v(u32 r)
{
	return (r >> 16) & 0x3ff;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_PB_SYNCPT_V(r) \
	host1x_sync_illegal_syncpt_access_frm_pb_syncpt_v(r)
static inline u32 host1x_sync_illegal_syncpt_access_frm_pb_ch_v(u32 r)
{
	return (r >> 10) & 0x3f;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_PB_CH_V(r) \
	host1x_sync_illegal_syncpt_access_frm_pb_ch_v(r)
static inline u32 host1x_sync_intstatus_illegal_client_access_v(u32 r)
{
	return (r >> 30) & 0x1;
}
#define HOST1X_SYNC_INTSTATUS_ILLEGAL_CLIENT_ACCESS_V(r) \
	host1x_sync_intstatus_illegal_client_access_v(r)
static inline u32 host1x_sync_illegal_syncpt_access_frm_client_r(void)
{
	return 0x2268;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_CLIENT \
	host1x_sync_illegal_syncpt_access_frm_client_r()
static inline u32 host1x_sync_illegal_syncpt_access_frm_client_syncpt_v(u32 r)
{
	return (r >> 16) & 0x3ff;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_CLIENT_SYNCPT_V(r) \
	host1x_sync_illegal_syncpt_access_frm_client_syncpt_v(r)
static inline u32 host1x_sync_illegal_syncpt_access_frm_client_ch_v(u32 r)
{
	return (r >> 10) & 0x3f;
}
#define HOST1X_SYNC_ILLEGAL_SYNCPT_ACCESS_FRM_CLIENT_CH_V(r) \
	host1x_sync_illegal_syncpt_access_frm_client_ch_v(r)
static inline u32 host1x_sync_syncpt_r(unsigned int id)
{
	return 0x18080 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT(id) \
	host1x_sync_syncpt_r(id)
static inline u32 host1x_sync_syncpt_thresh_cpu0_int_status_r(unsigned int id)
{
	return 0x16464 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_THRESH_CPU0_INT_STATUS(id) \
	host1x_sync_syncpt_thresh_cpu0_int_status_r(id)
static inline u32 host1x_sync_syncpt_thresh_int_disable_r(unsigned int id)
{
	return 0x16590 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_THRESH_INT_DISABLE(id) \
	host1x_sync_syncpt_thresh_int_disable_r(id)
static inline u32 host1x_sync_syncpt_thresh_int_enable_cpu0_r(unsigned int id)
{
	return 0x1652c + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_THRESH_INT_ENABLE_CPU0(id) \
	host1x_sync_syncpt_thresh_int_enable_cpu0_r(id)
static inline u32 host1x_sync_cf_setup_r(unsigned int channel)
{
	return 0x2588 + channel * REGISTER_STRIDE;
}
#define HOST1X_SYNC_CF_SETUP(channel) \
	host1x_sync_cf_setup_r(channel)
static inline u32 host1x_sync_cf_setup_base_v(u32 r)
{
	return (r >> 0) & 0xfff;
}
#define HOST1X_SYNC_CF_SETUP_BASE_V(r) \
	host1x_sync_cf_setup_base_v(r)
static inline u32 host1x_sync_cf_setup_limit_v(u32 r)
{
	return (r >> 16) & 0xfff;
}
#define HOST1X_SYNC_CF_SETUP_LIMIT_V(r) \
	host1x_sync_cf_setup_limit_v(r)
static inline u32 host1x_sync_cmdproc_stop_r(void)
{
	return 0x48;
}
#define HOST1X_SYNC_CMDPROC_STOP \
	host1x_sync_cmdproc_stop_r()
static inline u32 host1x_sync_ch_teardown_r(void)
{
	return 0x4c;
}
#define HOST1X_SYNC_CH_TEARDOWN \
	host1x_sync_ch_teardown_r()
static inline u32 host1x_sync_usec_clk_r(void)
{
	return 0x2244;
}
#define HOST1X_SYNC_USEC_CLK \
	host1x_sync_usec_clk_r()
static inline u32 host1x_sync_ctxsw_timeout_cfg_r(void)
{
	return 0x2248;
}
#define HOST1X_SYNC_CTXSW_TIMEOUT_CFG \
	host1x_sync_ctxsw_timeout_cfg_r()
static inline u32 host1x_sync_ip_busy_timeout_r(void)
{
	return 0x2250;
}
#define HOST1X_SYNC_IP_BUSY_TIMEOUT \
	host1x_sync_ip_busy_timeout_r()
static inline u32 host1x_sync_ip_read_timeout_addr_r(void)
{
	return 0x2254;
}
#define HOST1X_SYNC_IP_READ_TIMEOUT_ADDR \
	host1x_sync_ip_read_timeout_addr_r()
static inline u32 host1x_sync_ip_write_timeout_addr_r(void)
{
	return 0x225c;
}
#define HOST1X_SYNC_IP_WRITE_TIMEOUT_ADDR \
	host1x_sync_ip_write_timeout_addr_r()
static inline u32 host1x_sync_syncpt_int_thresh_r(unsigned int id)
{
	return 0x18a00 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_INT_THRESH(id) \
	host1x_sync_syncpt_int_thresh_r(id)
static inline u32 host1x_sync_syncpt_base_r(unsigned int id)
{
	return 0x18000 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_BASE(id) \
	host1x_sync_syncpt_base_r(id)
static inline u32 host1x_sync_syncpt_cpu_incr_r(unsigned int id)
{
	return 0x16400 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_SYNCPT_CPU_INCR(id) \
	host1x_sync_syncpt_cpu_incr_r(id)
static inline u32 host1x_sync_cfpeek_ctrl_r(void)
{
	return 0x233c;
}
#define HOST1X_SYNC_CFPEEK_CTRL \
	host1x_sync_cfpeek_ctrl_r()
static inline u32 host1x_sync_cfpeek_ctrl_addr_f(u32 v)
{
	return (v & 0xfff) << 0;
}
#define HOST1X_SYNC_CFPEEK_CTRL_ADDR_F(v) \
	host1x_sync_cfpeek_ctrl_addr_f(v)
static inline u32 host1x_sync_cfpeek_ctrl_channr_f(u32 v)
{
	return (v & 0x3f) << 16;
}
#define HOST1X_SYNC_CFPEEK_CTRL_CHANNR_F(v) \
	host1x_sync_cfpeek_ctrl_channr_f(v)
static inline u32 host1x_sync_cfpeek_ctrl_ena_f(u32 v)
{
	return (v & 0x1) << 31;
}
#define HOST1X_SYNC_CFPEEK_CTRL_ENA_F(v) \
	host1x_sync_cfpeek_ctrl_ena_f(v)
static inline u32 host1x_sync_cfpeek_read_r(void)
{
	return 0x2340;
}
#define HOST1X_SYNC_CFPEEK_READ \
	host1x_sync_cfpeek_read_r()
static inline u32 host1x_sync_cfpeek_ptrs_r(void)
{
	return 0x2344;
}
#define HOST1X_SYNC_CFPEEK_PTRS \
	host1x_sync_cfpeek_ptrs_r()
static inline u32 host1x_sync_cfpeek_ptrs_cf_rd_ptr_v(u32 r)
{
	return (r >> 0) & 0xfff;
}
#define HOST1X_SYNC_CFPEEK_PTRS_CF_RD_PTR_V(r) \
	host1x_sync_cfpeek_ptrs_cf_rd_ptr_v(r)
static inline u32 host1x_sync_cfpeek_ptrs_cf_wr_ptr_v(u32 r)
{
	return (r >> 16) & 0xfff;
}
#define HOST1X_SYNC_CFPEEK_PTRS_CF_WR_PTR_V(r) \
	host1x_sync_cfpeek_ptrs_cf_wr_ptr_v(r)
static inline u32 host1x_sync_common_mlock_r(unsigned long id)
{
	return 0x2030 + id * REGISTER_STRIDE;
}
#define HOST1X_SYNC_COMMON_MLOCK \
	host1x_sync_common_mlock_r()
static inline u32 host1x_sync_common_mlock_ch_v(u32 r)
{
	return (r >> 2) & 0x3f;
}
#define HOST1X_SYNC_COMMON_MLOCK_CH_V(r) \
	host1x_sync_common_mlock_ch_v(r)
static inline u32 host1x_sync_common_mlock_locked_v(u32 r)
{
	return (r >> 0) & 0x1;
}
#define HOST1X_SYNC_COMMON_MLOCK_LOCKED_V(r) \
	host1x_sync_common_mlock_locked_v(r)
static inline u32 host1x_thost_common_icg_en_override_0_r(void)
{
	return 0x2aa8;
}
#define HOST1X_THOST_COMMON_ICG_EN_OVERRIDE_0 \
	host1x_thost_common_icg_en_override_0_r()

#endif
