/*
 * NVGPU Public Interface Header
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

/* This file is meant to extend nvgpu.h, not replace it
 * as such, be sure that nvgpu.h is actually the file performing the
 * inclusion, to the extent that's possible.
 */
#ifndef _UAPI__LINUX_NVGPU_IOCTL_H
#    error "This file is to be included within nvgpu.h only."
#endif

#ifndef _UAPI__LINUX_NVGPU_T18X_IOCTL_H_
#define _UAPI__LINUX_NVGPU_T18X_IOCTL_H_

#define NVGPU_GPU_ARCH_GP100 0x00000130
#define NVGPU_GPU_IMPL_GP104 0x00000004
#define NVGPU_GPU_IMPL_GP106 0x00000006
#define NVGPU_GPU_IMPL_GP10B 0x0000000B

/*
 * this flag is used in struct nvgpu_as_map_buffer_ex_args
 * to specify IO coherence
 */
#define NVGPU_AS_MAP_BUFFER_FLAGS_IO_COHERENT	    (1 << 4)

/*
 * this flag is used in struct nvgpu_alloc_gpfifo_args
 * to enable re-playable faults for that channel
 */
#define NVGPU_ALLOC_GPFIFO_FLAGS_REPLAYABLE_FAULTS_ENABLE   (1 << 2)

/* Flags in nvgpu_alloc_obj_ctx_args.flags */
#define NVGPU_ALLOC_OBJ_FLAGS_GFXP		(1 << 1)
#define NVGPU_ALLOC_OBJ_FLAGS_CILP		(1 << 2)

/* Flags in nvgpu_preemption_mode_args.graphics_preempt_flags */
#define NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP     (1 << 1)
/* Flags in nvgpu_preemption_mode_args.compute_preempt_flags */
#define NVGPU_COMPUTE_PREEMPTION_MODE_CILP      (1 << 2)

/* SM LRF ECC is enabled */
#define NVGPU_GPU_FLAGS_ECC_ENABLED_SM_LRF	(1ULL << 60)
/* SM SHM ECC is enabled */
#define NVGPU_GPU_FLAGS_ECC_ENABLED_SM_SHM	(1ULL << 61)
/* TEX ECC is enabled */
#define NVGPU_GPU_FLAGS_ECC_ENABLED_TEX		(1ULL << 62)
/* L2 ECC is enabled */
#define NVGPU_GPU_FLAGS_ECC_ENABLED_LTC		(1ULL << 63)
/* All types of ECC are enabled */
#define NVGPU_GPU_FLAGS_ALL_ECC_ENABLED	\
				(NVGPU_GPU_FLAGS_ECC_ENABLED_SM_LRF |	\
				NVGPU_GPU_FLAGS_ECC_ENABLED_SM_SHM |	\
				NVGPU_GPU_FLAGS_ECC_ENABLED_TEX    |	\
				NVGPU_GPU_FLAGS_ECC_ENABLED_LTC)

/* Channel event_id in nvgpu_channel_events_ctrl_ext_args */
#define NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_STARTED	3
#define NVGPU_IOCTL_CHANNEL_EVENT_ID_CILP_PREEMPTION_COMPLETE	4

#endif /* _UAPI__LINUX_NVGPU_T18X_IOCTL_H_ */


