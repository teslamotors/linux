/*
 * Copyright (c) 2016, NVIDIA Corporation.
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


#include "dev_t186.h"

struct host1x_streamid_mapping {
	u32 host1x_offset;
	u32 client_offset;
	u32 client_limit;
};

static struct host1x_streamid_mapping __attribute__((__unused__))
        t18x_host1x_streamid_mapping[] = {
	/* HOST1X_THOST_COMMON_SE1_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001ac8, 0x00000090, 0x00000090},
	/* HOST1X_THOST_COMMON_SE2_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001ad0, 0x00000090, 0x00000090},
	/* HOST1X_THOST_COMMON_SE3_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001ad8, 0x00000090, 0x00000090},
	/* HOST1X_THOST_COMMON_SE4_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001ae0, 0x00000090, 0x00000090},
	/* HOST1X_THOST_COMMON_ISP_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001ae8, 0x00000050, 0x00000050},
	/* HOST1X_THOST_COMMON_VIC_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001af0, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_NVENC_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001af8, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_NVDEC_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001b00, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_NVJPG_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001b08, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_TSEC_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001b10, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_TSECB_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001b18, 0x00000030, 0x00000034},
	/* HOST1X_THOST_COMMON_VI_STRMID_0_OFFSET_BASE_0 */
	{ 0x00001b80, 0x00010000, 0x00010000},
	/* HOST1X_THOST_COMMON_VI_STRMID_1_OFFSET_BASE_0 */
	{ 0x00001b88, 0x00020000, 0x00020000},
	/* HOST1X_THOST_COMMON_VI_STRMID_2_OFFSET_BASE_0 */
	{ 0x00001b90, 0x00030000, 0x00030000},
	/* HOST1X_THOST_COMMON_VI_STRMID_3_OFFSET_BASE_0 */
	{ 0x00001b98, 0x00040000, 0x00040000},
	/* HOST1X_THOST_COMMON_VI_STRMID_4_OFFSET_BASE_0 */
	{ 0x00001ba0, 0x00050000, 0x00050000},
	/* HOST1X_THOST_COMMON_VI_STRMID_5_OFFSET_BASE_0 */
	{ 0x00001ba8, 0x00060000, 0x00060000},
	/* HOST1X_THOST_COMMON_VI_STRMID_6_OFFSET_BASE_0 */
	{ 0x00001bb0, 0x00070000, 0x00070000},
	/* HOST1X_THOST_COMMON_VI_STRMID_7_OFFSET_BASE_0 */
	{ 0x00001bb8, 0x00080000, 0x00080000},
	/* HOST1X_THOST_COMMON_VI_STRMID_8_OFFSET_BASE_0 */
	{ 0x00001bc0, 0x00090000, 0x00090000},
	/* HOST1X_THOST_COMMON_VI_STRMID_9_OFFSET_BASE_0 */
	{ 0x00001bc8, 0x000a0000, 0x000a0000},
	/* HOST1X_THOST_COMMON_VI_STRMID_10_OFFSET_BASE_0 */
	{ 0x00001bd0, 0x000b0000, 0x000b0000},
	/* HOST1X_THOST_COMMON_VI_STRMID_11_OFFSET_BASE_0 */
	{ 0x00001bd8, 0x000c0000, 0x000c0000},
	{},
};

static int load_streamid_regs(struct host1x *host)
{
	struct host1x_streamid_mapping *map_regs = t18x_host1x_streamid_mapping;

	while (map_regs->host1x_offset) {
		host1x_writel(host, map_regs->client_offset,
				    map_regs->host1x_offset);
		host1x_writel(host, map_regs->client_limit,
				    map_regs->host1x_offset + sizeof(u32));

		map_regs++;
	}

	return 0;
}

static const struct host1x_dev_ops host1x_dev_t186_ops = {
	.load_regs = load_streamid_regs,
};
