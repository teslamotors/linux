/*
 * Tegra Host Module Class IDs
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_CLASS_IDS_H
#define __NVHOST_CLASS_IDS_H


enum {
	NV_HOST1X_CLASS_ID		= 0x1,
	NV_VIDEO_ENCODE_MPEG_CLASS_ID	= 0x20,
	NV_VIDEO_ENCODE_MSENC_CLASS_ID	= 0x21,
	NV_VIDEO_ENCODE_NVENC_CLASS_ID	= 0x21,
	NV_VIDEO_STREAMING_VI_CLASS_ID	= 0x30,
	NV_VIDEO_STREAMING_ISP_CLASS_ID	= 0x32,
	NV_VIDEO_STREAMING_ISPB_CLASS_ID	= 0x34,
	NV_VIDEO_STREAMING_VII2C_CLASS_ID	= 0x36,
	NV_GRAPHICS_3D_CLASS_ID		= 0x60,
	NV_GRAPHICS_GPU_CLASS_ID	= 0x61,
	NV_GRAPHICS_VIC_CLASS_ID	= 0x5D,
	NV_TSEC_CLASS_ID		= 0xE0,
	NV_TSECB_CLASS_ID		= 0xE1,
	NV_NVJPG_CLASS_ID		= 0xC0,
	NV_NVDEC_CLASS_ID		= 0xF0,
};

#endif /*__NVHOST_CLASS_IDS_H */
