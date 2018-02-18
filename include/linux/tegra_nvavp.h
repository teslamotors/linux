/*
 * include/linux/tegra_nvavp.h
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __LINUX_TEGRA_NVAVP_H
#define __LINUX_TEGRA_NVAVP_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define NVAVP_MAX_RELOCATION_COUNT 64

/* avp submit flags */
#define NVAVP_FLAG_NONE		0x00000000
#define NVAVP_UCODE_EXT		0x00000001 /*use external ucode provided */

enum {
	NVAVP_MODULE_ID_CPU	= 1,
	NVAVP_MODULE_ID_AVP	= 2,
	NVAVP_MODULE_ID_VCP	= 3,
	NVAVP_MODULE_ID_BSEA	= 27,
	NVAVP_MODULE_ID_VDE	= 28,
	NVAVP_MODULE_ID_MPE	= 29,
	NVAVP_MODULE_ID_EMC	= 75,
};

struct nvavp_cmdbuf {
	__u32 mem;
	__u32 offset;
	__u32 words;
};

struct nvavp_reloc {
	__u32 cmdbuf_mem;
	__u32 cmdbuf_offset;
	__u32 target;
	__u32 target_offset;
};

struct nvavp_syncpt {
	__u32 id;
	__u32 value;
};

#ifdef CONFIG_COMPAT
struct nvavp_pushbuffer_submit_hdr_v32 {
	struct nvavp_cmdbuf	cmdbuf;
	__u32 relocs;
	__u32 num_relocs;
	__u32 syncpt;
	__u32 flags;
};
#endif

struct nvavp_pushbuffer_submit_hdr {
	struct nvavp_cmdbuf	cmdbuf;
	struct nvavp_reloc	*relocs;
	__u32			num_relocs;
	struct nvavp_syncpt	*syncpt;
	__u32			flags;
};

struct nvavp_set_nvmap_fd_args {
	__u32 fd;
};

struct nvavp_clock_args {
	__u32 id;
	__u32 rate;
};

enum nvavp_clock_stay_on_state {
	NVAVP_CLOCK_STAY_ON_DISABLED = 0,
	NVAVP_CLOCK_STAY_ON_ENABLED
};

struct nvavp_clock_stay_on_state_args {
	enum nvavp_clock_stay_on_state	state;
};

struct nvavp_num_cpus_args {
	__u32 min_online_cpus;
};

struct nvavp_map_args {
	__s32 fd;
	__u32 addr;
};

struct nvavp_channel_open_args {
	__u32 channel_fd;
};

#define NVAVP_IOCTL_MAGIC		'n'

#define NVAVP_IOCTL_SET_NVMAP_FD	_IOW(NVAVP_IOCTL_MAGIC, 0x60, \
					struct nvavp_set_nvmap_fd_args)
#define NVAVP_IOCTL_GET_SYNCPOINT_ID	_IOR(NVAVP_IOCTL_MAGIC, 0x61, \
					__u32)
#define NVAVP_IOCTL_PUSH_BUFFER_SUBMIT	_IOWR(NVAVP_IOCTL_MAGIC, 0x63, \
					struct nvavp_pushbuffer_submit_hdr)
#ifdef CONFIG_COMPAT
#define NVAVP_IOCTL_PUSH_BUFFER_SUBMIT32	_IOWR(NVAVP_IOCTL_MAGIC, 0x63, \
					struct nvavp_pushbuffer_submit_hdr_v32)
#endif
#define NVAVP_IOCTL_SET_CLOCK		_IOWR(NVAVP_IOCTL_MAGIC, 0x64, \
					struct nvavp_clock_args)
#define NVAVP_IOCTL_GET_CLOCK		_IOR(NVAVP_IOCTL_MAGIC, 0x65, \
					struct nvavp_clock_args)
#define NVAVP_IOCTL_WAKE_AVP		_IOR(NVAVP_IOCTL_MAGIC, 0x66, \
					__u32)
#define NVAVP_IOCTL_FORCE_CLOCK_STAY_ON	_IOW(NVAVP_IOCTL_MAGIC, 0x67, \
					struct nvavp_clock_stay_on_state_args)
#define NVAVP_IOCTL_ENABLE_AUDIO_CLOCKS	 _IOWR(NVAVP_IOCTL_MAGIC, 0x68, \
					struct nvavp_clock_args)
#define NVAVP_IOCTL_DISABLE_AUDIO_CLOCKS _IOWR(NVAVP_IOCTL_MAGIC, 0x69, \
					struct nvavp_clock_args)
#define NVAVP_IOCTL_SET_MIN_ONLINE_CPUS _IOWR(NVAVP_IOCTL_MAGIC, 0x70, \
					struct nvavp_num_cpus_args)
#define NVAVP_IOCTL_MAP_IOVA		_IOWR(NVAVP_IOCTL_MAGIC, 0x71, \
					struct nvavp_map_args)
#define NVAVP_IOCTL_UNMAP_IOVA		_IOW(NVAVP_IOCTL_MAGIC, 0x72, \
					struct nvavp_map_args)
#define NVAVP_IOCTL_CHANNEL_OPEN	_IOR(NVAVP_IOCTL_MAGIC, 0x73, \
					struct nvavp_channel_open_args)
#define NVAVP_IOCTL_VPR_FLOOR_SIZE	_IOW(NVAVP_IOCTL_MAGIC, 0x74, __u32)

#define NVAVP_IOCTL_MIN_NR		_IOC_NR(NVAVP_IOCTL_SET_NVMAP_FD)
#define NVAVP_IOCTL_MAX_NR		_IOC_NR(NVAVP_IOCTL_VPR_FLOOR_SIZE)

#define NVAVP_IOCTL_CHANNEL_MAX_ARG_SIZE	\
				sizeof(struct nvavp_pushbuffer_submit_hdr)
#endif /* __LINUX_TEGRA_NVAVP_H */
