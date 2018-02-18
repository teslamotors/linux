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
#ifndef _hw_trim_gp106_h_
#define _hw_trim_gp106_h_
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_r(void)
{
	return 0x00132924;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_s(void)
{
	return 16;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_m(void)
{
	return 0xffff << 0;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_s(void)
{
	return 1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_f(u32 v)
{
	return (v & 0x1) << 16;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_m(void)
{
	return 0x1 << 16;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_v(u32 r)
{
	return (r >> 16) & 0x1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_deasserted_f(void)
{
	return 0;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_asserted_f(void)
{
	return 0x10000;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_s(void)
{
	return 1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_f(u32 v)
{
	return (v & 0x1) << 20;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_m(void)
{
	return 0x1 << 20;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_v(u32 r)
{
	return (r >> 20) & 0x1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_deasserted_f(void)
{
	return 0;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_asserted_f(void)
{
	return 0x100000;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_s(void)
{
	return 1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_m(void)
{
	return 0x1 << 24;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_v(u32 r)
{
	return (r >> 24) & 0x1;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_deasserted_f(void)
{
	return 0;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_asserted_f(void)
{
	return 0x1000000;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_source_gpc2clk_f(void)
{
	return 0x70000000;
}
static inline u32 trim_gpc_bcast_clk_cntr_ncgpcclk_cnt_r(void)
{
	return 0x00132928;
}
static inline u32 trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_r(void)
{
	return 0x00132128;
}
static inline u32 trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_source_dramdiv4_rec_clk1_f(void)
{
	return 0x20000000;
}
static inline u32 trim_fbpa_bcast_clk_cntr_ncltcclk_cnt_r(void)
{
	return 0x0013212c;
}
static inline u32 trim_sys_clk_cntr_ncltcpll_cfg_r(void)
{
	return 0x001373c0;
}
static inline u32 trim_sys_clk_cntr_ncltcpll_cfg_source_xbar2clk_f(void)
{
	return 0x20000000;
}
static inline u32 trim_sys_clk_cntr_ncltcpll_cnt_r(void)
{
	return 0x001373c4;
}
static inline u32 trim_sys_clk_cntr_ncsyspll_cfg_r(void)
{
	return 0x001373b0;
}
static inline u32 trim_sys_clk_cntr_ncsyspll_cfg_source_sys2clk_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_clk_cntr_ncsyspll_cnt_r(void)
{
	return 0x001373b4;
}

#endif
