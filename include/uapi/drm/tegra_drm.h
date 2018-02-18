/*
 * Copyright (C) 2012-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _UAPI_TEGRA_DRM_H_
#define _UAPI_TEGRA_DRM_H_

#include <drm/drm.h>

#define DRM_TEGRA_GEM_CREATE_TILED     (1 << 0)
#define DRM_TEGRA_GEM_CREATE_BOTTOM_UP (1 << 1)

struct drm_tegra_gem_create {
	__u64 size;
	__u32 flags;
	__u32 handle;
};

struct drm_tegra_gem_mmap {
	__u32 handle;
	__u32 offset;
};

struct drm_tegra_syncpt_read {
	__u32 id;
	__u32 value;
};

struct drm_tegra_syncpt_incr {
	__u32 id;
	__u32 pad;
};

struct drm_tegra_syncpt_wait {
	__u32 id;
	__u32 thresh;
	__u32 timeout;
	__u32 value;
};

struct drm_tegra_fence_info {
	__u32 id;
	__u32 thresh;
};

struct drm_tegra_fence_create {
	__u32 num_pts;
	__s32 fence_fd; /* fd of new fence */
	__u64 pts; /* struct drm_tegra_fence_info* */
	__u64 name; /* const char* */
};

struct drm_tegra_fence_set_name {
	__u64 name; /* const char* for name */
	__s32 fence_fd; /* fd of fence */
};

#define DRM_TEGRA_ERROR_SUBMIT_TIMEOUT		1

struct drm_tegra_notification {
	struct {
		__u32 nanoseconds[2];   /* nanoseconds since Jan. 1, 1970 */
	} time_stamp;
	__u32 error32;
	__u16 error16;
	__u16 status;
};

struct drm_tegra_set_error_notifier {
	__u64 context;
	__u64 offset;
	__u32 handle;
	__u32 pad;
};

#define DRM_TEGRA_NO_TIMEOUT	(0xffffffff)

struct drm_tegra_open_channel {
	__u32 client;
	__u32 pad;
	__u64 context;
};

struct drm_tegra_close_channel {
	__u64 context;
};

struct drm_tegra_get_syncpt {
	__u64 context;
	__u32 index;
	__u32 id;
};

struct drm_tegra_get_syncpt_base {
	__u64 context;
	__u32 syncpt;
	__u32 id;
};

struct drm_tegra_syncpt {
	__u32 id;
	__u32 incrs;
};

struct drm_tegra_cmdbuf {
	__u32 handle;
	__u32 offset;
	__u32 words;
	__s32 pre_fence;
};

struct drm_tegra_reloc {
	struct {
		__u32 handle;
		__u32 offset;
	} cmdbuf;
	struct {
		__u32 handle;
		__u32 offset;
	} target;
	__u32 shift;
	__u32 pad;
};

struct drm_tegra_waitchk {
	__u32 handle;
	__u32 offset;
	__u32 syncpt;
	__u32 thresh;
};

#define DRM_TEGRA_SUBMIT_FLAGS_SYNC_FD	(1 << 0)

struct drm_tegra_submit {
	__u64 context;
	__u32 num_syncpts;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 num_waitchks;
	__u32 flags;
	__u32 timeout;
	__u64 syncpts;
	__u64 cmdbufs;
	__u64 relocs;
	__u64 waitchks;
	__u32 fence;		/* Return value */
	__u32 reserved0;
	__u64 fences;
	__u64 class_ids;
};

#define DRM_TEGRA_GEM_TILING_MODE_PITCH 0
#define DRM_TEGRA_GEM_TILING_MODE_TILED 1
#define DRM_TEGRA_GEM_TILING_MODE_BLOCK 2

struct drm_tegra_gem_set_tiling {
	/* input */
	__u32 handle;
	__u32 mode;
	__u32 value;
	__u32 pad;
};

struct drm_tegra_gem_get_tiling {
	/* input */
	__u32 handle;
	/* output */
	__u32 mode;
	__u32 value;
	__u32 pad;
};

#define DRM_TEGRA_GEM_BOTTOM_UP		(1 << 0)
#define DRM_TEGRA_GEM_FLAGS		(DRM_TEGRA_GEM_BOTTOM_UP)

struct drm_tegra_gem_set_flags {
	/* input */
	__u32 handle;
	/* output */
	__u32 flags;
};

struct drm_tegra_gem_get_flags {
	/* input */
	__u32 handle;
	/* output */
	__u32 flags;
};

struct drm_tegra_characteristics {
#define DRM_TEGRA_CHARA_GFILTER (1 << 0)
	__u64 flags;

	__u32 num_syncpts;
	__u32 pad;
};

struct drm_tegra_get_characteristics {
	__u64 drm_tegra_chara_buf_size;
	__u64 drm_tegra_chara_buf_addr;
};

enum request_type {
	DRM_TEGRA_REQ_TYPE_CLK_KHZ = 0,
	DRM_TEGRA_REQ_TYPE_BW_KBPS,
};

struct drm_tegra_get_clk_rate {
	/* class ID*/
	__u32 id;
	/* request type: KBps or KHz */
	__u32 type;
	/* numeric value for type */
	__u64 data;
};

struct drm_tegra_set_clk_rate {
	/* class ID*/
	__u32 id;
	/* request type: KBps or KHz */
	__u32 type;
	/* numeric value for type */
	__u64 data;
};

#define DRM_TEGRA_GEM_CREATE		0x00
#define DRM_TEGRA_GEM_MMAP		0x01
#define DRM_TEGRA_SYNCPT_READ		0x02
#define DRM_TEGRA_SYNCPT_INCR		0x03
#define DRM_TEGRA_SYNCPT_WAIT		0x04
#define DRM_TEGRA_OPEN_CHANNEL		0x05
#define DRM_TEGRA_CLOSE_CHANNEL		0x06
#define DRM_TEGRA_GET_SYNCPT		0x07
#define DRM_TEGRA_SUBMIT		0x08
#define DRM_TEGRA_GET_SYNCPT_BASE	0x09
#define DRM_TEGRA_GEM_SET_TILING	0x0a
#define DRM_TEGRA_GEM_GET_TILING	0x0b
#define DRM_TEGRA_GEM_SET_FLAGS		0x0c
#define DRM_TEGRA_GEM_GET_FLAGS		0x0d
#define DRM_TEGRA_FENCE_CREATE		0x40
#define DRM_TEGRA_FENCE_SET_NAME	0x41
#define DRM_TEGRA_SET_ERROR_NOTIFIER	0x42
#define DRM_TEGRA_GET_CHARACTERISTICS   0x43
#define DRM_TEGRA_GET_CLK_RATE		0x44
#define DRM_TEGRA_SET_CLK_RATE		0x45

#define DRM_IOCTL_TEGRA_GEM_CREATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_CREATE, struct drm_tegra_gem_create)
#define DRM_IOCTL_TEGRA_GEM_MMAP DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_MMAP, struct drm_tegra_gem_mmap)
#define DRM_IOCTL_TEGRA_SYNCPT_READ DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SYNCPT_READ, struct drm_tegra_syncpt_read)
#define DRM_IOCTL_TEGRA_SYNCPT_INCR DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SYNCPT_INCR, struct drm_tegra_syncpt_incr)
#define DRM_IOCTL_TEGRA_SYNCPT_WAIT DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SYNCPT_WAIT, struct drm_tegra_syncpt_wait)
#define DRM_IOCTL_TEGRA_OPEN_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_OPEN_CHANNEL, struct drm_tegra_open_channel)
#define DRM_IOCTL_TEGRA_CLOSE_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_CLOSE_CHANNEL, struct drm_tegra_open_channel)
#define DRM_IOCTL_TEGRA_GET_SYNCPT DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GET_SYNCPT, struct drm_tegra_get_syncpt)
#define DRM_IOCTL_TEGRA_SUBMIT DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SUBMIT, struct drm_tegra_submit)
#define DRM_IOCTL_TEGRA_GET_SYNCPT_BASE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GET_SYNCPT_BASE, struct drm_tegra_get_syncpt_base)
#define DRM_IOCTL_TEGRA_GEM_SET_TILING DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_SET_TILING, struct drm_tegra_gem_set_tiling)
#define DRM_IOCTL_TEGRA_GEM_GET_TILING DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_GET_TILING, struct drm_tegra_gem_get_tiling)
#define DRM_IOCTL_TEGRA_GEM_SET_FLAGS DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_SET_FLAGS, struct drm_tegra_gem_set_flags)
#define DRM_IOCTL_TEGRA_GEM_GET_FLAGS DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_GET_FLAGS, struct drm_tegra_gem_get_flags)
#define DRM_IOCTL_TEGRA_FENCE_CREATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_FENCE_CREATE, struct drm_tegra_fence_create)
#define DRM_IOCTL_TEGRA_FENCE_SET_NAME DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_FENCE_SET_NAME, struct drm_tegra_fence_set_name)
#define DRM_IOCTL_TEGRA_SET_ERROR_NOTIFIER DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SET_ERROR_NOTIFIER, struct drm_tegra_set_error_notifier)
#define DRM_IOCTL_TEGRA_GET_CHARACTERISTICS DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GET_CHARACTERISTICS, struct drm_tegra_get_characteristics)
#define DRM_IOCTL_TEGRA_GET_CLK_RATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GET_CLK_RATE, struct drm_tegra_get_clk_rate)
#define DRM_IOCTL_TEGRA_SET_CLK_RATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SET_CLK_RATE, struct drm_tegra_set_clk_rate)

#endif
