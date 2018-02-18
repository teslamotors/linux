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
#ifndef _hw_fuse_gm206_h_
#define _hw_fuse_gm206_h_

static inline u32 fuse_status_opt_tpc_gpc_r(u32 i)
{
	return 0x00021c38 + i*4;
}
static inline u32 fuse_ctrl_opt_tpc_gpc_r(u32 i)
{
	return 0x00021838 + i*4;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_r(void)
{
	return 0x00021944;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_data_f(u32 v)
{
	return (v & 0x3) << 0;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_data_m(void)
{
	return 0x3 << 0;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_data_v(u32 r)
{
	return (r >> 0) & 0x3;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_r(void)
{
	return 0x00021948;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_data_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_data_m(void)
{
	return 0x1 << 0;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_data_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_data_yes_f(void)
{
	return 0x1;
}
static inline u32 fuse_ctrl_opt_ram_svop_pdp_override_data_no_f(void)
{
	return 0x0;
}
static inline u32 fuse_status_opt_fbio_r(void)
{
	return 0x00021c14;
}
static inline u32 fuse_status_opt_fbio_data_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 fuse_status_opt_fbio_data_m(void)
{
	return 0xffff << 0;
}
static inline u32 fuse_status_opt_fbio_data_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 fuse_status_opt_rop_l2_fbp_r(u32 i)
{
	return 0x00021d70 + i*4;
}
static inline u32 fuse_status_opt_fbp_r(void)
{
	return 0x00021d38;
}
static inline u32 fuse_status_opt_fbp_idx_v(u32 r, u32 i)
{
	return (r >> (0 + i*0)) & 0x1;
}
#endif
